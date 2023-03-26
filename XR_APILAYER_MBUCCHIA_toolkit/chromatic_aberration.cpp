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
        XrVector4f Params1;
        // ChromaticCorrectionR, ChromaticCorrectionG, ChromaticCorrectionB (-1..+1 params), Eye (0 = left, 1 = right)
    };

    class ChromaticAberrationPostProcessor : public IPostProcessor {
    public:
        ChromaticAberrationPostProcessor(std::shared_ptr<IConfigManager> configManager,
                                         std::shared_ptr<IDevice> graphicsDevice)
            : m_configManager(configManager), m_device(graphicsDevice) {
            createRenderResources();
        }

        bool isEnabled() override {
            return m_configManager->getEnumValue<config::PostProcessCACorrectionType>(
                       config::SettingPostChromaticCorrection) != config::PostProcessCACorrectionType::Off;
        }

        void reload() override {
            createRenderResources();
        }

        void update() override {
            // Generic implementation to support more than just Off/On modes in the future.
            const auto mode =
                m_configManager->getEnumValue<PostProcessCACorrectionType>(config::SettingPostChromaticCorrection);
            const auto hasModeChanged = mode != m_mode;

            if (hasModeChanged) {
                m_mode = mode;
            }

            if (hasModeChanged || checkUpdateConfig()) {
                updateConfig();
            }
        }

        void process(std::shared_ptr<ITexture> input,
                     std::shared_ptr<ITexture> output,
                     std::vector<std::shared_ptr<ITexture>>& textures,
                     std::array<uint8_t, 1024>& blob,
                     std::optional<utilities::Eye> eye = std::nullopt) override {
            // We need to use a per-instance blob.
            static_assert(sizeof(ImageProcessorConfig) <= 1024);
            const auto config = reinterpret_cast<ImageProcessorConfig*>(blob.data());

            memcpy(config, &m_config, sizeof(m_config));

            // Patch the eye.
            config->Params1.w = static_cast<float>(eye.value_or(utilities::Eye::Both));

            // TODO: We can use an IShaderBuffer cache per swapchain and avoid this every frame.
            m_cbParams->uploadData(config, sizeof(*config));

            m_device->setShader(m_shaderCA, SamplerType::LinearClamp);
            m_device->setShaderInput(0, m_cbParams);
            m_device->setShaderInput(0, input);
            m_device->setShaderOutput(0, output);
            m_device->dispatchShader();
        }

    private:
        void createRenderResources() {
            const auto shadersDir = dllHome / "shaders";
            const auto shaderFile = shadersDir / "varjo_ca.hlsl";

            const utilities::shader::Defines defines;

            m_shaderCA = m_device->createQuadShader(
                shaderFile,
                "main",
                "CACorrection PS",
                defines.get());

            m_cbParams = m_device->createBuffer(sizeof(ImageProcessorConfig), "Post Process (Varjo CA) CB");

            updateConfig();
        }

        bool checkUpdateConfig() const {
            return m_configManager->hasChanged(SettingPostChromaticCorrectionR) ||
                   m_configManager->hasChanged(SettingPostChromaticCorrectionB);
        }

        void updateConfig() {
            using namespace DirectX;
            using namespace xr::math;
            using namespace utilities;

            m_config.Params1.x = m_configManager->getValue(SettingPostChromaticCorrectionR) / 100000.0f;
            m_config.Params1.y = 1.0f;
            m_config.Params1.z = m_configManager->getValue(SettingPostChromaticCorrectionB) / 100000.0f;
            // Params4.w is patched JIT in process().
        }

        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_device;

        std::shared_ptr<IQuadShader> m_shaderCA;
        std::shared_ptr<IShaderBuffer> m_cbParams;

        PostProcessCACorrectionType m_mode{PostProcessCACorrectionType::Off};
        ImageProcessorConfig m_config{};
    };
} // namespace

namespace toolkit::graphics {
    std::shared_ptr<IPostProcessor> CreateChromaticAberrationPostProcessor(
        std::shared_ptr<IConfigManager> configManager,
        std::shared_ptr<IDevice> graphicsDevice) {
        return std::make_shared<ChromaticAberrationPostProcessor>(configManager, graphicsDevice);
    }
} // namespace toolkit::graphics
