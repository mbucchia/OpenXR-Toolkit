// MIT License
//
// Copyright(c) 2021-2022 Matthieu Bucchianeri
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "pch.h"

#include "factories.h"
#include "interfaces.h"
#include "layer.h"
#include "log.h"

namespace {

    using namespace toolkit;
    using namespace toolkit::config;
    using namespace toolkit::log;
    using namespace toolkit::utilities;

    constexpr unsigned int WriteDelay = 22; // 1s in bad VR.

    struct ConfigValue {
        int value;
        int defaultValue{0};

        bool changedSinceLastQuery{false};
        unsigned int writeCountdown{0};
    };

    // A very simple registry/DWORD backed configuration manager.
    // Handles deferred writes (to only commit values after a few game loops completed).
    class ConfigManager : public IConfigManager {
      public:
        ConfigManager(const std::string& appName) : m_appName(appName) {
            // Check for safe mode.
            m_safeMode = RegGetDword(HKEY_LOCAL_MACHINE, std::wstring(RegPrefix.begin(), RegPrefix.end()), L"safe_mode")
                             .value_or(0);

            std::string baseKey = RegPrefix + "\\" + appName;
            m_baseKey = std::wstring(baseKey.begin(), baseKey.end());

            m_watcher = wil::make_registry_watcher(
                HKEY_CURRENT_USER, m_baseKey.c_str(), true, [&](wil::RegistryChangeKind changeType) {
                    // TODO: Every call to writeValue() will also cause us to invoke this (unnecessarily).
                    m_needRefresh = true;
                });
        }

        ~ConfigManager() override {
            // Log all unwritten values.
            for (auto& value : m_values) {
                ConfigValue& entry = value.second;

                if (entry.writeCountdown > 0) {
                    Log("Config value '%s' was discarded due to quickly exiting after changing its value\n",
                        value.first.c_str());
                }
            }
        }

        void tick() override {
            const bool m_wasNeedRefresh = m_needRefresh;

            for (auto& value : m_values) {
                const std::string& name = value.first;
                ConfigValue& entry = value.second;

                if (m_needRefresh && !m_ignoreRefresh.count(name)) {
                    refreshValue(name, entry);
                }

                if (entry.writeCountdown > 0) {
                    entry.writeCountdown--;

                    if (entry.writeCountdown == 0) {
                        writeValue(name, entry);

                        // Don't refresh this value, since we just wrote it!
                        m_ignoreRefresh.insert(name);
                    }
                }
            }

            // Only clear the need for refresh if the whole tick update saw the changes.
            m_needRefresh = m_wasNeedRefresh != m_needRefresh;
            if (!m_needRefresh) {
                m_ignoreRefresh.clear();
            }
        }

        void setDefault(const std::string& name, int value) override {
            auto it = m_values.find(name);

            if (it != m_values.end()) {
                Log("Config value '%s' is assigned a default after being used\n", name.c_str());
            }

            ConfigValue newEntry;
            ConfigValue& entry = it != m_values.end() ? it->second : newEntry;
            entry.defaultValue = value;
            if (it == m_values.end()) {
                readValue(name, entry);
                m_values.insert_or_assign(name, entry);
            }
        }

        int getValue(const std::string& name) const override {
            auto it = m_values.find(name);

            ConfigValue newEntry;
            ConfigValue& entry = it != m_values.end() ? it->second : newEntry;
            if (it != m_values.end()) {
                entry.changedSinceLastQuery = false;
            } else {
                readValue(name, entry);
                m_values.insert_or_assign(name, entry);
            }

            return entry.value;
        }

        int peekValue(const std::string& name) const override {
            auto it = m_values.find(name);

            ConfigValue newEntry;
            ConfigValue& entry = it != m_values.end() ? it->second : newEntry;
            if (it == m_values.end()) {
                readValue(name, entry);
                m_values.insert_or_assign(name, entry);
            }

            return entry.value;
        }

        void setValue(const std::string& name, int value, bool noCommitDelay) override {
            auto it = m_values.find(name);

            ConfigValue newEntry;
            ConfigValue& entry = it != m_values.end() ? it->second : newEntry;
            entry.value = value;
            entry.changedSinceLastQuery = true;
            entry.writeCountdown = noCommitDelay ? 1 : WriteDelay;
            if (it == m_values.end()) {
                m_values.insert_or_assign(name, entry);
            }
        }

        bool hasChanged(const std::string& name) const override {
            auto it = m_values.find(name);

            if (it != m_values.end()) {
                ConfigValue& entry = it->second;

                return entry.changedSinceLastQuery;
            }

            return false;
        }

        void deleteValue(const std::string& name) override {
            RegDeleteValue(HKEY_CURRENT_USER, m_baseKey, std::wstring(name.begin(), name.end()));
            m_values.erase(name);
        }

        void resetToDefaults() override {
            // TODO: For now we do the same as a hard reset. Ideally, we wish to exclude `first_run' from this.
            hardReset();
        }

        bool isSafeMode() const override {
            return m_safeMode;
        }

        void hardReset() override {
            RegDeleteKey(HKEY_CURRENT_USER, m_baseKey);
            for (auto& value : m_values) {
                ConfigValue& entry = value.second;

                entry.value = entry.defaultValue;
                entry.changedSinceLastQuery = true;
                entry.writeCountdown = 0;
            }
        }

      private:
        std::optional<int> readRegistry(const std::string& name) const {
            auto value = RegGetDword(HKEY_CURRENT_USER, m_baseKey, std::wstring(name.begin(), name.end()));
            if (!value) {
                // Fallback to HKLM for global options.
                value = RegGetDword(HKEY_LOCAL_MACHINE,
                                    std::wstring(RegPrefix.begin(), RegPrefix.end()),
                                    std::wstring(name.begin(), name.end()));
            }
            return value;
        }

        void readValue(const std::string& name, ConfigValue& entry) const {
            if (m_safeMode) {
                entry.value = entry.defaultValue;
                entry.changedSinceLastQuery = true;
                return;
            }

            const auto value = readRegistry(name);
            entry.value = value.value_or(entry.defaultValue);
            entry.changedSinceLastQuery = true;

            TraceLoggingWrite(g_traceProvider, "Config_ReadValue", TLArg(name.c_str(), "Name"), TLArg(entry.value, "Value"));            
        }

        void refreshValue(const std::string& name, ConfigValue& entry) const {
            if (m_safeMode) {
                return;
            }

            const auto value = readRegistry(name);
            if (entry.value != value.value_or(entry.defaultValue)) {
                entry.value = value.value_or(entry.defaultValue);
                entry.changedSinceLastQuery = true;

                // Cancel pending writes.
                entry.writeCountdown = 0;
            }
        }

        void writeValue(const std::string& name, ConfigValue& entry) const {
            TraceLoggingWrite(g_traceProvider, "Config_WriteValue", TLArg(name.c_str(), "Name"), TLArg(entry.value, "Value"));
            RegSetDword(HKEY_CURRENT_USER, m_baseKey, std::wstring(name.begin(), name.end()), entry.value);
        }

        const std::string m_appName;
        std::wstring m_baseKey;
        bool m_safeMode;
        wil::unique_registry_watcher m_watcher;
        bool m_needRefresh{false};
        std::set<std::string> m_ignoreRefresh;

        mutable std::map<std::string, ConfigValue> m_values;
    };

} // namespace

namespace toolkit::config {

    std::shared_ptr<IConfigManager> CreateConfigManager(const std::string& appName) {
        return std::make_shared<ConfigManager>(appName);
    }

} // namespace toolkit::config
