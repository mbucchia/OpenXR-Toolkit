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
#include "shader_utilities.h"
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

#define DECLARE_ENUM_TO_STRING_VIEW(E, ...)                                                                            \
    template <>                                                                                                        \
    std::string_view to_string_view(E e) {                                                                             \
        constexpr std::string_view labels[] = ##__VA_ARGS__;                                                           \
        static_assert(std::size(labels) == static_cast<std::underlying_type_t<E>>(E::MaxValue));                       \
        return labels[static_cast<std::underlying_type_t<E>>(e)];                                                      \
    }

    DECLARE_ENUM_TO_STRING_VIEW(OffOnType, {"Off", "On"})
    DECLARE_ENUM_TO_STRING_VIEW(NoYesType, {"No", "Yes"})
    DECLARE_ENUM_TO_STRING_VIEW(OverlayType, {"Off", "FPS", "Advanced", "Developer"})
    DECLARE_ENUM_TO_STRING_VIEW(MenuFontSize, {"Small", "Medium", "Large"})
    DECLARE_ENUM_TO_STRING_VIEW(MenuTimeout, {"Short", "Medium", "Long", "None"})
    DECLARE_ENUM_TO_STRING_VIEW(ScalingType, {"Off", "NIS", "FSR"})
    DECLARE_ENUM_TO_STRING_VIEW(MipMapBias, {"Off", "Conservative", "All"})
    DECLARE_ENUM_TO_STRING_VIEW(HandTrackingEnabled, {"Off", "Both", "Left", "Right"})
    DECLARE_ENUM_TO_STRING_VIEW(HandTrackingVisibility, {"Hidden", "Bright", "Medium", "Dark", "Darker"})
    DECLARE_ENUM_TO_STRING_VIEW(MotionReprojectionRate, {"", "Off", "R_45Hz", "R_30Hz", "R_22Hz"})
    DECLARE_ENUM_TO_STRING_VIEW(VariableShadingRateType, {"Off", "Preset", "Custom"})
    DECLARE_ENUM_TO_STRING_VIEW(VariableShadingRateQuality, {"Performance", "Quality"})
    DECLARE_ENUM_TO_STRING_VIEW(VariableShadingRatePattern, {"Wide", "Balanced", "Narrow"})
    DECLARE_ENUM_TO_STRING_VIEW(VariableShadingRateDir, {"Vertical", "Horizontal"})
    DECLARE_ENUM_TO_STRING_VIEW(VariableShadingRateVal, {"1x", "1/2", "1/4", "1/8", "1/16", "Cull"})
    DECLARE_ENUM_TO_STRING_VIEW(PostProcessType, {"Off", "On"})
    DECLARE_ENUM_TO_STRING_VIEW(PostSunGlassesType, {"Off", "Light", "Dark", "TruNite"})
    DECLARE_ENUM_TO_STRING_VIEW(FovModeType, {"Simple", "Advanced"})
    DECLARE_ENUM_TO_STRING_VIEW(ScreenshotFileFormat, {"DDS", "PNG", "JPG", "BMP"})

    // DECLARE_ENUM_TO_STRING_VIEW(SaturationModeType, {"Global", "Selective"})

#undef DECLARE_ENUM_TO_STRING_VIEW

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
        if (blockSize >= 2u)
            size = roundUp(size, blockSize);
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

    void ToggleWindowsMixedRealityReprojection(bool enable) {
        utilities::RegSetDword(
            HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\OpenXR", L"MotionVectorEnabled", enable ? 2 /* Always on */ : 0);
    }

    void UpdateWindowsMixedRealityReprojectionRate(config::MotionReprojectionRate rate) {
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

    void ClearWindowsMixedRealityReprojection() {
        utilities::RegDeleteValue(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\OpenXR", L"MotionVectorEnabled");
        utilities::RegDeleteValue(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\OpenXR", L"MinimumFrameInterval");
        utilities::RegDeleteValue(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\OpenXR", L"MaximumFrameInterval");
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

} // namespace toolkit::utilities

namespace toolkit::utilities::shader {

    void CompileShader(const std::filesystem::path& shaderFile,
                       const char* entryPoint,
                       ID3DBlob** blob,
                       const D3D_SHADER_MACRO* defines /*= nullptr*/,
                       ID3DInclude* includes /* = nullptr*/,
                       const char* target /* = "cs_5_0"*/) {
        DWORD flags =
            D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#ifdef _DEBUG
        flags |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
        if (!includes) {
            includes = D3D_COMPILE_STANDARD_FILE_INCLUDE;
        }

        ComPtr<ID3DBlob> cdErrorBlob;
        const HRESULT hr =
            D3DCompileFromFile(shaderFile.c_str(), defines, includes, entryPoint, target, flags, 0, blob, &cdErrorBlob);

        if (FAILED(hr)) {
            if (cdErrorBlob) {
                Log("%s", (char*)cdErrorBlob->GetBufferPointer());
            }
            CHECK_HRESULT(hr, "Failed to compile shader file");
        }
    }

    void CompileShader(const void* data,
                       size_t size,
                       const char* entryPoint,
                       ID3DBlob** blob,
                       const D3D_SHADER_MACRO* defines /*= nullptr*/,
                       ID3DInclude* includes /* = nullptr*/,
                       const char* target /* = "cs_5_0"*/) {
        DWORD flags =
            D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#ifdef _DEBUG
        flags |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
        if (!includes) {
            // TODO: pSourceName must be a file name to derive relative paths from.
            includes = D3D_COMPILE_STANDARD_FILE_INCLUDE;
        }

        ComPtr<ID3DBlob> cdErrorBlob;
        const HRESULT hr =
            D3DCompile(data, size, nullptr, defines, includes, entryPoint, target, flags, 0, blob, &cdErrorBlob);

        if (FAILED(hr)) {
            if (cdErrorBlob) {
                Log("%s", (char*)cdErrorBlob->GetBufferPointer());
            }
            CHECK_HRESULT(hr, "Failed to compile shader");
        }
    }

    HRESULT IncludeHeader::Open(
        D3D_INCLUDE_TYPE /*includeType*/, LPCSTR pFileName, LPCVOID /*pParentData*/, LPCVOID* ppData, UINT* pBytes) {
        for (auto& it : m_includePaths) {
            auto path = it / pFileName;
            auto file = std::ifstream(path, std::ios_base::binary);
            if (file.is_open()) {
                assert(ppData && pBytes);
                m_data.push_back({});
                auto& buf = m_data.back();
                buf.resize(static_cast<size_t>(std::filesystem::file_size(path)));
                file.read(buf.data(), static_cast<std::streamsize>(buf.size()));
                buf.erase(std::remove(buf.begin(), buf.end(), '\0'), buf.end());
                *ppData = buf.data();
                *pBytes = static_cast<UINT>(buf.size());
                return S_OK;
            }
        }
        throw std::runtime_error("Error opening shader file include header");
    }

    HRESULT IncludeHeader::Close(LPCVOID pData) {
        return S_OK;
    }

    const D3D_SHADER_MACRO* Defines::get() const {
        static const D3D_SHADER_MACRO kEmpty = {nullptr, nullptr};
        if (!m_definesVector.empty()) {
            m_defines = std::make_unique<D3D_SHADER_MACRO[]>(m_definesVector.size() + 1);
            for (size_t i = 0; i < m_definesVector.size(); ++i)
                m_defines[i] = {m_definesVector[i].first.c_str(), m_definesVector[i].second.c_str()};
            m_defines[m_definesVector.size()] = kEmpty;
            return m_defines.get();
        }
        m_defines = nullptr;
        return &kEmpty;
    }

} // namespace toolkit::utilities::shader
