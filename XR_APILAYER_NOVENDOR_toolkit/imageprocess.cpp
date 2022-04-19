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
    using namespace toolkit::log;

    struct alignas(16) ImageProcessorConfig {
        // DirectX::XMFLOAT4X4 BrightnessContrastSaturationMatrix;
        XrVector4f Scale;  // Contrast, Brightness, Exposure, Vibrance (-1..+1 params)
        XrVector4f Amount; // Saturation, Highlights, Shadows (0..1 params)
    };

    class ImageProcessor : public IImageProcessor {
      public:
        ImageProcessor(std::shared_ptr<IConfigManager> configManager, std::shared_ptr<IDevice> graphicsDevice)
            : m_configManager(configManager), m_device(graphicsDevice),
              m_userParams(GetUserParams(configManager.get())) {
            createRenderResources();
        }

#if 0
        void update() override {
            const bool hasSaturationModeChanged = m_configManager->hasChanged(SettingSaturationMode);
            const bool saturationPerChannel = m_configManager->getValue(SettingSaturationMode);

            if (m_configManager->hasChanged(SettingBrightness) || m_configManager->hasChanged(SettingContrast) ||
                hasSaturationModeChanged || (!saturationPerChannel && m_configManager->hasChanged(SettingSaturation)) ||
                (saturationPerChannel && (m_configManager->hasChanged(SettingSaturationRed) ||
                                          m_configManager->hasChanged(SettingSaturationGreen) ||
                                          m_configManager->hasChanged(SettingSaturationBlue)))) {
                const int saturationGlobal = m_configManager->getValue(SettingSaturation);
                const int saturationRedValue =
                    saturationPerChannel ? m_configManager->getValue(SettingSaturationRed) : saturationGlobal;
                const int saturationGreenValue =
                    saturationPerChannel ? m_configManager->getValue(SettingSaturationGreen) : saturationGlobal;
                const int saturationBlueValue =
                    saturationPerChannel ? m_configManager->getValue(SettingSaturationBlue) : saturationGlobal;

                // 0 -> -1, 500 -> 0, 1000 -> 1
                const float brightness = 1.f + 2.f * (m_configManager->getValue(SettingBrightness) - 500) / 1000.f;
                const float saturationRed = 1.f + 2.f * (saturationRedValue - 500) / 1000.f;
                const float saturationGreen = 1.f + 2.f * (saturationGreenValue - 500) / 1000.f;
                const float saturationBlue = 1.f + 2.f * (saturationBlueValue - 500) / 1000.f;
                // Contrast is also inverted.
                const float contrast = 2.f * -(m_configManager->getValue(SettingContrast) - 5000) / 10000.f;

                // This code is based on the article from Paul Haeberli.
                // http://www.graficaobscura.com/matrix/
                DirectX::XMMATRIX brightnessMatrix;
                {
                    // clang-format off
                    brightnessMatrix = DirectX::XMMatrixSet(brightness, 0.f, 0.f, 0.f,
                                                            0.f, brightness, 0.f, 0.f,
                                                            0.f, 0.f, brightness, 0.f,
                                                            0.f, 0.f, 0.f, 1.f);
                    // clang-format on
                }
                DirectX::XMMATRIX contrastMatrix;
                {
                    // clang-format off
                    contrastMatrix = DirectX::XMMatrixSet(1.f, 0.f, 0.f, 0.f,
                                                          0.f, 1.f, 0.f, 0.f,
                                                          0.f, 0.f, 1.f, 0.f,
                                                          contrast, contrast, contrast, 1.f);
                    // clang-format on
                }
                DirectX::XMMATRIX saturationMatrix;
                {
                    float red[] = {saturationRed + 0.3086f * (1.f - saturationRed),
                                   0.3086f * (1.f - saturationRed),
                                   0.3086f * (1.f - saturationRed)};
                    float green[] = {0.6094f * (1.f - saturationGreen),
                                     saturationGreen + 0.6094f * (1.f - saturationGreen),
                                     0.6094f * (1.f - saturationGreen)};
                    float blue[] = {0.0820f * (1.f - saturationBlue),
                                    0.0820f * (1.f - saturationBlue),
                                    saturationBlue + 0.0820f * (1.f - saturationBlue)};

                    // clang-format off
                    saturationMatrix = DirectX::XMMatrixSet(red[0], red[1], red[2], 0.f,
                                                            green[0], green[1], green[2], 0.f,
                                                            blue[0], blue[1], blue[2], 0.f,
                                                            0.f, 0.f, 0.f, 1.f);
                    // clang-format on
                }

                ImageProcessorConfig staging;
                DirectX::XMStoreFloat4x4(
                    &staging.BrightnessContrastSaturationMatrix,
                    DirectX::XMMatrixMultiply(brightnessMatrix,
                                              DirectX::XMMatrixMultiply(contrastMatrix, saturationMatrix)));
                m_configBuffer->uploadData(&staging, sizeof(staging));
            }
        }
#endif

        void reload() override {
            createRenderResources();
        }

        void update() override {
            if (checkUpdateConfig()) {
                updateConfig();
            }
        }

        void process(std::shared_ptr<ITexture> input, std::shared_ptr<ITexture> output, int32_t slice) override {
            m_device->setShader(!input->isArray() ? m_shader : m_shaderVPRT, SamplerType::LinearClamp);
            m_device->setShaderInput(0, m_configBuffer);
            m_device->setShaderInput(0, input, slice);
            m_device->setShaderOutput(0, output, slice);

            m_device->dispatchShader();
        }

      private:
        void createRenderResources() {
            const auto shadersDir = dllHome / "shaders";
            const auto shaderFile = shadersDir / "postprocess.hlsl";

            utilities::shader::Defines defines;
            defines.add("POST_PROCESS_SAMPLE_LINEAR", true);
            // defines.add("POST_PROCESS_SRC_SRGB", true);
            // defines.add("POST_PROCESS_DST_SRGB", true);

            m_shader = m_device->createQuadShader(
                shaderFile, "mainPostProcess", "Image Processor PS", defines.get() /*,  shadersDir*/);

            defines.add("VPRT", true);
            m_shaderVPRT = m_device->createQuadShader(
                shaderFile, "mainPostProcess", "Image Processor VPRT PS", defines.get() /*,  shadersDir*/);

            // TODO: For now, we're going to require that all image processing shaders share the same configuration
            // structure.
            m_configBuffer = m_device->createBuffer(sizeof(ImageProcessorConfig), "Image Processor Configuration CB");

            updateConfig();
        }

        bool checkUpdateConfig() {
            return m_configManager->hasChanged(SettingPostBrightness) ||
                   m_configManager->hasChanged(SettingPostContrast) ||
                   m_configManager->hasChanged(SettingPostExposure) ||
                   m_configManager->hasChanged(SettingPostVibrance) ||
                   m_configManager->hasChanged(SettingPostSaturation) ||
                   m_configManager->hasChanged(SettingPostHighlights) ||
                   m_configManager->hasChanged(SettingPostShadows) ||
                   m_configManager->hasChanged(SettingPostSunGlasses);
        }

        void updateConfig() {
            using namespace DirectX;
            using namespace xr::math;
            using namespace utilities;

            // const auto saturationMode = m_configManager->getValue(SettingSaturationMode);
            // const auto saturationR = saturationMode ? m_configManager->getValue(SettingSaturationRed) : saturation;
            // const auto saturationG = saturationMode ? m_configManager->getValue(SettingSaturationGreen) : saturation;
            // const auto saturationB = saturationMode ? m_configManager->getValue(SettingSaturationBlue) : saturation;

            // standard gains:
            // - reduce contrast and brighness ranges
            // - increase exposure and vibrance effect
            // - limit shadows range
            static constexpr XMVECTORF32 kGain[1][2] = {
                {{{{0.10f, 0.80f, 4.0f, 3.0f}}}, {{{1.0f, 1.0f, 0.50f, 1.0f}}}},
            };

            // standard presets
            static constexpr XMVECTORF32 kBias[to_integral(PostSunGlassesType::MaxValue)][2] = {
                // none
                {{{{0.0f, 0.0f, 0.0f, 0.0f}}}, {{{0.0f, 0.0f, 0.0f, 0.0f}}}},
                
                // user
                {{{{0.0f, 0.0f, 0.0f, 0.0f}}}, {{{0.0f, 0.0f, 0.0f, 0.0f}}}},
                
                // sunglasses light: +2.5 contrast, -5 bright, -5 expo, -20 highlights
                {{{{0.025f, -0.05f, -0.05f, 0.0f}}}, {{{0.0f, -0.20f, 0.0f, 0.f}}}},
                
                // sunglasses dark: +2.5 contrast, -10 bright, -10 expo, -40 highlights, +5 shad
                {{{{0.025f, -0.10f, -0.10f, 0.0f}}}, {{{0.0f, -0.40f, 0.05f, 0.0f}}}},
                
                // deep night: +0.5 contrast, -40 bright, +20 expo, +2.5 sat, -75 high, +15 shad
                {{{{0.005f, -0.40f, 0.20f, 0.0f}}}, {{{0.25f, -0.75f, 0.15f, 0.0f}}}},
            };

            ImageProcessorConfig config;

            const auto sunglasses = m_configManager->getEnumValue<PostSunGlassesType>(SettingPostSunGlasses);
            const auto is_user = sunglasses == PostSunGlassesType::User;

            const auto params1 = is_user ? m_userParams[0]
                                         : XMINT4(m_configManager->getValue(SettingPostContrast),
                                                  m_configManager->getValue(SettingPostBrightness),
                                                  m_configManager->getValue(SettingPostExposure),
                                                  m_configManager->getValue(SettingPostVibrance));

            const auto params2 = is_user ? m_userParams[1]
                                         : XMINT4(m_configManager->getValue(SettingPostSaturation),
                                                  m_configManager->getValue(SettingPostHighlights),
                                                  m_configManager->getValue(SettingPostShadows),
                                                  0);

            // [0..1000] -> [-1..+1]
            const auto scale = XMVectorSaturate(XMLoadSInt4(&params1) * 0.001f + kBias[to_integral(sunglasses)][0]);
            StoreXrVector4(&config.Scale, (scale * 2.0 - XMVectorSplatOne()) * kGain[0][0]);

            // [0..1000] -> [0..1]
            const auto amount = XMVectorSaturate(XMLoadSInt4(&params2) * 0.001f + kBias[to_integral(sunglasses)][1]);
            StoreXrVector4(&config.Amount, amount * kGain[0][1]);

            m_configBuffer->uploadData(&config, sizeof(config));
        }

        static std::array<DirectX::XMINT4, 2> GetUserParams(const IConfigManager* configManager) {
            using namespace DirectX;
            if (configManager) {
                return {XMINT4(configManager->getValue(SettingPostContrast + "_u1"),
                               configManager->getValue(SettingPostBrightness + "_u1"),
                               configManager->getValue(SettingPostExposure + "_u1"),
                               configManager->getValue(SettingPostVibrance + "_u1")),
                        XMINT4(configManager->getValue(SettingPostSaturation + "_u1"),
                               configManager->getValue(SettingPostHighlights + "_u1"),
                               configManager->getValue(SettingPostShadows + "_u1"),
                               0)};
            }
            return {XMINT4(500, 500, 500, 1000), XMINT4(500, 1000, 0, 0)};
        }

        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_device;
        const std::array<DirectX::XMINT4, 2> m_userParams;

        std::shared_ptr<IQuadShader> m_shader;
        std::shared_ptr<IQuadShader> m_shaderVPRT;
        std::shared_ptr<IShaderBuffer> m_configBuffer;
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
                                                          std::shared_ptr<IDevice> graphicsDevice) {
        return std::make_shared<ImageProcessor>(configManager, graphicsDevice);
    }

} // namespace toolkit::graphics
