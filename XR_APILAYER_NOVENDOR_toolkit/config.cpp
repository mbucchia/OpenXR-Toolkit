// MIT License
//
// Copyright(c) 2021 Matthieu Bucchianeri
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

    constexpr unsigned int WriteDelay = 90; // 1-2s in good VR :)

    // https://docs.microsoft.com/en-us/archive/msdn-magazine/2017/may/c-use-modern-c-to-access-the-windows-registry
    int RegGetDword(HKEY hKey, const std::wstring& subKey, const std::wstring& value, DWORD defaultValue) {
        DWORD data{};
        DWORD dataSize = sizeof(data);
        LONG retCode = ::RegGetValue(hKey, subKey.c_str(), value.c_str(), RRF_RT_REG_DWORD, nullptr, &data, &dataSize);
        if (retCode != ERROR_SUCCESS) {
            return defaultValue;
        }
        return data;
    }

    void RegSetDword(HKEY hKey, const std::wstring& subKey, const std::wstring& value, DWORD dwordValue) {
        DWORD dataSize = sizeof(dwordValue);
        LONG retCode = ::RegSetKeyValue(hKey, subKey.c_str(), value.c_str(), REG_DWORD, &dwordValue, dataSize);
        if (retCode != ERROR_SUCCESS) {
            Log("Failed to write value: %d\n", retCode);
        }
    }

    void RegDeleteKey(HKEY hKey, const std::wstring& subKey) {
        ::RegDeleteKey(hKey, subKey.c_str());
    }

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
            std::string baseKey = RegPrefix + "\\" + appName;
            m_baseKey = std::wstring(baseKey.begin(), baseKey.end());
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
            for (auto& value : m_values) {
                ConfigValue& entry = value.second;

                if (entry.writeCountdown > 0) {
                    entry.writeCountdown--;

                    if (entry.writeCountdown == 0) {
                        writeValue(value.first, entry);
                    }
                }
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

        void setValue(const std::string& name, int value) override {
            auto it = m_values.find(name);

            ConfigValue newEntry;
            ConfigValue& entry = it != m_values.end() ? it->second : newEntry;
            entry.value = value;
            entry.changedSinceLastQuery = true;
            entry.writeCountdown = WriteDelay;
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

        void resetToDefaults() override {
            for (auto& value : m_values) {
                // Make an exception for this special entry.
                if (value.first == "first_run") {
                    continue;
                }

                ConfigValue& entry = value.second;

                entry.value = entry.defaultValue;
                entry.changedSinceLastQuery = true;
                entry.writeCountdown = WriteDelay;
            }
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
        void readValue(const std::string& name, ConfigValue& entry) const {
            entry.value =
                RegGetDword(HKEY_CURRENT_USER, m_baseKey, std::wstring(name.begin(), name.end()), entry.defaultValue);
            entry.changedSinceLastQuery = true;
        }

        void writeValue(const std::string& name, ConfigValue& entry) const {
            RegSetDword(HKEY_CURRENT_USER, m_baseKey, std::wstring(name.begin(), name.end()), entry.value);
        }

        const std::string m_appName;
        std::wstring m_baseKey;

        mutable std::map<std::string, ConfigValue> m_values;
    };

} // namespace

namespace toolkit::config {

    std::shared_ptr<IConfigManager> CreateConfigManager(const std::string& appName) {
        return std::make_shared<ConfigManager>(appName);
    }

} // namespace toolkit::config
