// MIT License
//
// Copyright(c) 2021-2022 Matthieu Bucchianeri
// Copyright(c) 2021-2022 Jean-Luc Dupiot - Reality XP
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
#include "log.h"

namespace {

    using namespace toolkit::utilities;

    class CpuTimer : public ICpuTimer {
        using clock = std::chrono::high_resolution_clock;

      public:
        void start() override {
            m_timeStart = clock::now();
        }

        void stop() override {
            m_duration = clock::now() - m_timeStart;
        }

        uint64_t query(bool reset) const override {
            const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(m_duration);

            if (reset)
                m_duration = clock::duration::zero();

            return duration.count();
        }

      private:
        clock::time_point m_timeStart;
        mutable clock::duration m_duration{0};
    };

} // namespace

namespace toolkit::config {

    std::pair<uint32_t, uint32_t> GetScaledDimensions(const IConfigManager* configManager,
                                                      uint32_t outputWidth,
                                                      uint32_t outputHeight,
                                                      uint32_t blockSize) {
        const auto settingScaling = configManager->peekValue(SettingScaling);
        const auto settingAnamophic = configManager->peekValue(SettingAnamorphic);

        return std::make_pair(utilities::GetScaledInputSize(outputWidth, settingScaling, blockSize),
                              utilities::GetScaledInputSize(
                                  outputHeight, settingAnamophic > 0 ? settingAnamophic : settingScaling, blockSize));
    }

} // namespace toolkit::config

namespace toolkit::utilities {

    using namespace toolkit::config;
    using namespace toolkit::log;

    // https://docs.microsoft.com/en-us/archive/msdn-magazine/2017/may/c-use-modern-c-to-access-the-windows-registry
    std::optional<int> RegGetDword(HKEY hKey, const std::wstring& subKey, const std::wstring& value) {
        DWORD data{};
        DWORD dataSize = sizeof(data);
        LONG retCode = ::RegGetValue(hKey, subKey.c_str(), value.c_str(), RRF_RT_REG_DWORD, nullptr, &data, &dataSize);
        if (retCode != ERROR_SUCCESS) {
            return {};
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

    void
    RegSetString(HKEY hKey, const std::wstring& subKey, const std::wstring& value, const std::string& stringValue) {
        LONG retCode = ::RegSetKeyValue(hKey,
                                        subKey.c_str(),
                                        value.c_str(),
                                        REG_SZ,
                                        std::wstring(stringValue.begin(), stringValue.end()).c_str(),
                                        (DWORD)(2 * (stringValue.length() + 1)));
        if (retCode != ERROR_SUCCESS) {
            Log("Failed to write value: %d\n", retCode);
        }
    }

    void RegDeleteValue(HKEY hKey, const std::wstring& subKey, const std::wstring& value) {
        ::RegDeleteKeyValue(hKey, subKey.c_str(), value.c_str());
    }

    void RegDeleteKey(HKEY hKey, const std::wstring& subKey) {
        ::RegDeleteKey(hKey, subKey.c_str());
    }

    std::shared_ptr<ICpuTimer> CreateCpuTimer() {
        return std::make_shared<CpuTimer>();
    }

    uint32_t GetScaledInputSize(uint32_t outputSize, int scalePercent, uint32_t blockSize) {
        scalePercent = abs(scalePercent);
        auto size = scalePercent >= 100 ? (outputSize * 100u) / scalePercent : (outputSize * scalePercent) / 100u;

        // align size to blockSize
        if (blockSize >= 2u)
            size = ((size + blockSize - 1u) / blockSize) * blockSize;

        return size;
    }

    bool UpdateKeyState(bool& keyState, const std::vector<int>& vkModifiers, int vkKey, bool isRepeat) {
        // bail out early if any modifier is not depressed.
        const auto isPressed =
            std::all_of(vkModifiers.begin(), vkModifiers.end(), [](int vk) { return GetAsyncKeyState(vk) < 0; }) &&
            GetAsyncKeyState(vkKey) < 0;
        const bool wasPressed = std::exchange(keyState, isPressed);
        return isPressed && (!wasPressed || isRepeat);
    }

    void UpdateWindowsMixedRealityReprojection(config::MotionReprojectionRate rate) {
        if (rate != config::MotionReprojectionRate::Off) {
            utilities::RegSetDword(
                HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\OpenXR", L"MinimumFrameInterval", (DWORD)rate);
            utilities::RegSetDword(
                HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\OpenXR", L"MaximumFrameInterval", (DWORD)rate);
        } else {
            utilities::RegDeleteValue(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\OpenXR", L"MinimumFrameInterval");
            utilities::RegDeleteValue(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\OpenXR", L"MaximumFrameInterval");
        }
    }

    // https://stackoverflow.com/questions/7808085/how-to-get-the-status-of-a-service-programmatically-running-stopped
    bool IsServiceRunning(const std::string& name) {
        SC_HANDLE theService, scm;
        SERVICE_STATUS_PROCESS ssStatus;
        DWORD dwBytesNeeded;

        scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
        if (!scm) {
            return false;
        }

        theService = OpenServiceA(scm, name.c_str(), SERVICE_QUERY_STATUS);
        if (!theService) {
            CloseServiceHandle(scm);
            return false;
        }

        auto result = QueryServiceStatusEx(theService,
                                           SC_STATUS_PROCESS_INFO,
                                           reinterpret_cast<LPBYTE>(&ssStatus),
                                           sizeof(SERVICE_STATUS_PROCESS),
                                           &dwBytesNeeded);

        CloseServiceHandle(theService);
        CloseServiceHandle(scm);

        if (result == 0) {
            return false;
        }

        return ssStatus.dwCurrentState == SERVICE_RUNNING;
    }

    // [-1,+1] (+up) -> [0..1] (+dn)
    XrVector2f NdcToScreen(XrVector2f v) {
        return {(1.f + v.x) * 0.5f, (v.y - 1.f) * -0.5f};
    }

    // [0..1] (+dn) -> [-1,+1] (+up)
    XrVector2f ScreenToNdc(XrVector2f v) {
        return {(v.x * 2.f) - 1.f, (v.y * -2.f) + 1.f};
    }

} // namespace toolkit::utilities