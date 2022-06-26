// MIT License
//
// Copyright(c) 2021 Matthieu Bucchianeri
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

#include "shader_utilities.h"
#include "factories.h"
#include "interfaces.h"
#include "layer.h"
#include "log.h"

namespace {

    using namespace toolkit;
    using namespace toolkit::config;
    using namespace toolkit::graphics;
    using namespace toolkit::utilities;
    using namespace toolkit::log;

    struct alignas(16) ImageProcessorConfig {
        XrVector4f Params1;  // Contrast, Brightness, Exposure, Saturation (-1..+1 params)
        XrVector4f Params2;  // ColorGainR, ColorGainG, ColorGainB (-1..+1 params)
        XrVector4f Params3;  // Highlights, Shadows, Vibrance (0..1 params)
        XrVector4f Params4;  // Gaze, Ring (anti-flicker)
        XrVector2f Rings[4]; // 1/(a1^2), 1/(b1^2)
    };

    class ImageProcessor : public IImageProcessor {
      public:
        ImageProcessor(std::shared_ptr<IConfigManager> configManager,
                       std::shared_ptr<IDevice> graphicsDevice,
                       std::shared_ptr<IVariableRateShader> variableRateShader)
            : m_configManager(configManager), m_device(graphicsDevice), m_vrs(variableRateShader),
              m_userParams(GetParams(configManager.get(), 1)) {
            createRenderResources();
        }

        void reload() override {
            createRenderResources();
        }

        void update() override {
            // Generic implementation to support more than just Off/On modes in the future.
            const auto mode = m_configManager->getEnumValue<PostProcessType>(config::SettingPostProcess);
            const auto hasModeChanged = mode != m_mode;

            if (hasModeChanged)
                m_mode = mode;

            if (hasModeChanged || checkUpdateConfig(mode)) {
                updateConfig();
            }

            if (m_vrs && checkUpdateConfigVrs()) {
                updateConfigVrs();
            }
        }

        void process(std::shared_ptr<ITexture> input, std::shared_ptr<ITexture> output, int32_t slice) override {
            // TODO: check whether we can use a single buffer instead.

            const auto gazeIdx = slice >= 0 ? slice : 2;
            if (m_configUpdated) {
                m_configUpdated = gazeIdx == 0; // re-arm for the 2nd view
                m_config.Params4.x = m_vrsState.gazeXY[gazeIdx].x;
                m_config.Params4.y = m_vrsState.gazeXY[gazeIdx].y;
                m_cbParams[gazeIdx]->uploadData(&m_config, sizeof(m_config));
            }

            const auto usePostProcess = m_mode != PostProcessType::Off;
            m_device->setShader(m_shaders[input->isArray()][usePostProcess], SamplerType::LinearClamp);
            m_device->setShaderInput(0, m_cbParams[gazeIdx]);
            m_device->setShaderInput(0, input, slice);
            m_device->setShaderOutput(0, output, slice);
            m_device->dispatchShader();
        }

      private:
        void createRenderResources() {
            const auto shadersDir = dllHome / "shaders";
            const auto shaderFile = shadersDir / "postprocess.hlsl";

            shader::Defines defines;
            // defines.add("POST_PROCESS_SRC_SRGB", true);
            // defines.add("POST_PROCESS_DST_SRGB", true);

            defines.add("PASS_THROUGH_USE_GAINS", true);
            defines.add("VRS_NUM_RATES", 3);

            m_shaders[0][0] = m_device->createQuadShader(
                shaderFile, "mainPassThrough", "Passthrough PS", defines.get() /*,  shadersDir*/);
            m_shaders[0][1] = m_device->createQuadShader(
                shaderFile, "mainPostProcess", "Postprocess PS", defines.get() /*,  shadersDir*/);

            defines.add("VPRT", true);
            m_shaders[1][0] = m_device->createQuadShader(
                shaderFile, "mainPassThrough", "Passthrough PS (VPRT)", defines.get() /*,  shadersDir*/);
            m_shaders[1][1] = m_device->createQuadShader(
                shaderFile, "mainPostProcess", "Postprocess PS (VPRT)", defines.get() /*,  shadersDir*/);

            for (auto& it : m_cbParams) {
                it = m_device->createBuffer(sizeof(ImageProcessorConfig), "Postprocess CB");
            }

            // Initialize gaze and ring large enough.
            m_config.Params4.x = m_config.Params4.y = 0;
            m_config.Params4.z = m_config.Params4.w = 0.00001f;
            std::fill_n(m_config.Rings, std ::size(m_config.Rings), XrVector2f{0.00001f, 0.00001f});

            updateConfig();
            updateConfigVrs();
        }

        bool checkUpdateConfig(PostProcessType mode) const {
            if (mode != PostProcessType::Off) {
                return m_configManager->hasChanged(SettingPostSunGlasses) ||
                       m_configManager->hasChanged(SettingPostContrast) ||
                       m_configManager->hasChanged(SettingPostBrightness) ||
                       m_configManager->hasChanged(SettingPostExposure) ||
                       m_configManager->hasChanged(SettingPostSaturation) ||
                       m_configManager->hasChanged(SettingPostVibrance) ||
                       m_configManager->hasChanged(SettingPostHighlights) ||
                       m_configManager->hasChanged(SettingPostShadows) ||
                       m_configManager->hasChanged(SettingPostColorGainR) ||
                       m_configManager->hasChanged(SettingPostColorGainG) ||
                       m_configManager->hasChanged(SettingPostColorGainB);
            } else {
                return m_configManager->hasChanged(SettingPostColorGainR) ||
                       m_configManager->hasChanged(SettingPostColorGainG) ||
                       m_configManager->hasChanged(SettingPostColorGainB);
            }
        }

        void updateConfig() {
            using namespace DirectX;
            using namespace xr::math;

            // Transform normalized user settings to shader values:
            // - reduce brighness range  (scale: 0.8)
            // - increase exposure range (scale: 3.0)
            // - limit shadows range     (scale: 0.5)
            // - inverse highlight range (scale:-1.0)

            static constexpr XMVECTORF32 kGainBias[3][2] = {
                {{{{+2.0f, 1.6f, 6.0f, 2.0f}}}, {{{+1.0f, 0.8f, 3.0f, 1.0f}}}}, // ((v * 2) - 1)  -> [-1..+1]
                {{{{+2.0f, 2.0f, 2.0f, 2.0f}}}, {{{+1.0f, 1.0f, 1.0f, 1.0f}}}}, // ((v * 2) - 1)  -> [-1..+1]
                {{{{-1.0f, 0.5f, 1.0f, 1.0f}}}, {{{-1.0f, 0.0f, 0.0f, 0.0f}}}}, // ((v * 1) - 0)  -> [ 0..+1]
            };

            const auto sunglasses = m_configManager->getEnumValue<PostSunGlassesType>(SettingPostSunGlasses);
            const auto preset = GetPreset(static_cast<size_t>(to_integral(sunglasses)));
            const auto params = GetParams(m_configManager.get(), 0);

            // [0..1000] -> [0..1] * Gain - Bias
            for (size_t i = 0; i < 3; i++) {
                const auto param = XMVectorSaturate((XMLoadSInt4(&params[i]) + XMLoadSInt4(&preset[i])) * 0.001f);
                StoreXrVector4(&m_config.Params1 + i, (param * kGainBias[i][0]) - kGainBias[i][1]);
            }

            // m_cbParams->uploadData(&m_config, sizeof(m_config));
            m_configUpdated = true;
        }

        bool checkUpdateConfigVrs() {
            const auto currentGen = m_vrs ? m_vrs->getCurrentGen() : 0;
            const auto showRings =
                currentGen && m_configManager->getEnumValue<NoYesType>(config::SettingVRSShowRings) != NoYesType::No;

            if (m_vrsCurrentGen != currentGen || m_vrsShowRings != showRings) {
                m_vrsCurrentGen = currentGen;
                m_vrsShowRings = showRings;
                return true;
            }
            return false;
        }

        void updateConfigVrs() {
#if 0
            VariableRateShaderState vrsState;
            if (m_vrs && m_vrsCurrentGen) {
                m_vrs->getShaderState(vrsState, Eye::Both);

                // Don't track gaze changes when using eye tracking.
                const auto hasGazeChanged =
                   vrsState.mode != 2 &&
                   !std::equal(std::begin(vrsState.gazeXY), std::end(vrsState.gazeXY),
                   std::begin(m_vrsState.gazeXY));

                // Track rings and rate for anti-flicker
                const auto hasRingChanged =
                   !std::equal(std::begin(vrsState.rings), std::end(vrsState.rings), std::begin(m_vrsState.rings));
                const auto hasRateChanged =
                   !std::equal(std::begin(vrsState.rates), std::end(vrsState.rates), std::begin(m_vrsState.rates));
                
                if (hasRingChanged || hasRateChanged) {
                }

                m_vrsState = vrsState;
            }
#endif
            m_config.Params4.z = m_config.Params4.w = 0;

            if (m_vrs && m_vrsCurrentGen) {
                // Get gazes and rings but ignore L/R rate bias.
                m_vrs->getShaderState(m_vrsState, Eye::Both);

                // Reduce flickering for rings with a rate above 2x2.
                constexpr auto kLowResRate = to_integral(VariableShadingRateVal::R_2x2);
                const auto ringIdx = m_vrsState.rates[1] > kLowResRate   ? 0
                                     : m_vrsState.rates[2] > kLowResRate ? 1
                                     : m_vrsState.rates[3] > kLowResRate ? 2
                                                                         : -1;
                if (ringIdx >= 0) {
                    m_config.Params4.z = m_vrsState.rings[ringIdx].x;
                    m_config.Params4.w = m_vrsState.rings[ringIdx].y;
                }
            }

            if (m_vrsShowRings) {
                std::copy_n(m_vrsState.rings, ARRAYSIZE(m_vrsState.rings), m_config.Rings);
            } else {
                std::fill_n(m_config.Rings, ARRAYSIZE(m_config.Rings), XrVector2f{0, 0});
            }

            m_configUpdated = true;
        }

        static std::array<DirectX::XMINT4, 3> GetParams(const IConfigManager* configManager, size_t index) {
            using namespace DirectX;
            if (configManager) {
                static const char* lut[] = {"", "_u1", "_u2", "_u3", "_u4"}; // placeholder up to 4
                const auto suffix = lut[std::min(index, std::size(lut))];

                return {XMINT4(configManager->getValue(SettingPostContrast + suffix),
                               configManager->getValue(SettingPostBrightness + suffix),
                               configManager->getValue(SettingPostExposure + suffix),
                               configManager->getValue(SettingPostSaturation + suffix)),

                        XMINT4(configManager->getValue(SettingPostColorGainR + suffix),
                               configManager->getValue(SettingPostColorGainG + suffix),
                               configManager->getValue(SettingPostColorGainB + suffix),
                               0),

                        XMINT4(configManager->getValue(SettingPostHighlights + suffix),
                               configManager->getValue(SettingPostShadows + suffix),
                               configManager->getValue(SettingPostVibrance + suffix),
                               0)};
            }
            return {XMINT4(500, 500, 500, 500), XMINT4(500, 500, 500, 0), XMINT4(1000, 0, 0, 0)};
        }

        static std::array<DirectX::XMINT4, 3> GetPreset(size_t index) {
            using namespace DirectX;

            // standard presets
            static constexpr std::array<XMINT4, 3> lut[to_integral(PostSunGlassesType::MaxValue)] = {
                // none
                {{{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}},

                // sunglasses light: +2.5 contrast, -5 bright, -5 expo, -20 high
                {{{25, -50, -50, 0}, {0, 0, 0, 0}, {-20, 0, 0, 0}}},

                // sunglasses dark: +2.5 contrast, -10 bright, -10 expo, -40 high, +5 shad
                {{{25, -100, -100, 0}, {0, 0, 0, 0}, {-400, 50, 0, 0}}},

                //// deep night (beta1): +0.5 contrast, -40 bright, +20 expo, -15 sat, -75 high, +15 shad, +5 vib
                //{{{5, -400, 200, -150}, {0, 0, 0, 0}, {-750, 150, 50, 0}}},

                // deep night (beta2): +5 contrast, -30 bright, +25 expo, +5 sat, -100 high, +10 shad
                {{{50, -300, 250, +50}, {0, 0, 0, 0}, {-1000, 100, 0, 0}}},
            };

            return lut[index < std::size(lut) ? index : 0];
        }

        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_device;
        const std::shared_ptr<IVariableRateShader> m_vrs;

        const std::array<DirectX::XMINT4, 3> m_userParams;

        std::shared_ptr<IQuadShader> m_shaders[2][2]; // off, on, vprt
        std::shared_ptr<IShaderBuffer> m_cbParams[ViewCount + 1];

        bool m_configUpdated{false};
        bool m_configVrsUpdated{false};
        bool m_vrsShowRings{false};

        uint64_t m_vrsCurrentGen{0};
        VariableRateShaderState m_vrsState{};

        PostProcessType m_mode{PostProcessType::Off};
        ImageProcessorConfig m_config{};
    };

} // namespace

namespace toolkit::graphics {

    bool IsDeviceSupportingFP16(std::shared_ptr<IDevice> device) {
        if (device) {
            if (auto device11 = device->getAs<D3D11>()) {
                D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT feature = {};
                device11->CheckFeatureSupport(D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT, &feature, sizeof(feature));
                return (feature.PixelShaderMinPrecision & D3D11_SHADER_MIN_PRECISION_16_BIT) != 0;
            }
            if (auto device12 = device->getAs<D3D12>()) {
                D3D12_FEATURE_DATA_D3D12_OPTIONS feature = {};
                device12->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &feature, sizeof(feature));
                return (feature.MinPrecisionSupport & D3D12_SHADER_MIN_PRECISION_SUPPORT_16_BIT) != 0;
            }
        }

        return false;
    }

    GpuArchitecture GetGpuArchitecture(UINT VendorId) {
        // Known PCI vendor IDs
        constexpr uint32_t kVendorID_AMD = 0x1002;
        constexpr uint32_t kVendorID_Intel = 0x8086;
        constexpr uint32_t kVendorID_NVIDIA = 0x10DE;

        return VendorId == kVendorID_AMD      ? GpuArchitecture::AMD
               : VendorId == kVendorID_Intel  ? GpuArchitecture::Intel
               : VendorId == kVendorID_NVIDIA ? GpuArchitecture::NVidia
                                              : GpuArchitecture::Unknown;
    }

    GpuArchitecture GetGpuArchitecture(std::shared_ptr<IDevice> device) {
        if (device) {
            std::string name = device->getDeviceName();
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });

            if (name.find("nvidia") != std::string::npos)
                return GpuArchitecture::NVidia;

            if (name.find("intel") != std::string::npos)
                return GpuArchitecture::Intel;

            // put last in case other vendor names have these 3 letters in their device name.
            if (name.find("amd") != std::string::npos)
                return GpuArchitecture::AMD;
        }

        return GpuArchitecture::Unknown;
    }

    std::shared_ptr<IImageProcessor> CreateImageProcessor(std::shared_ptr<IConfigManager> configManager,
                                                          std::shared_ptr<IDevice> graphicsDevice,
                                                          std::shared_ptr<IVariableRateShader> variableRateShader) {
        return std::make_shared<ImageProcessor>(configManager, graphicsDevice, variableRateShader);
    }

} // namespace toolkit::graphics
