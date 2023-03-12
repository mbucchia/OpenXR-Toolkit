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

#include "shader_utilities.h"
#include "factories.h"
#include "interfaces.h"
#include "layer.h"
#include "log.h"

#pragma warning(push)
#pragma warning(disable : 4305)
#include <NIS_Config.h>
#pragma warning(pop)

namespace {

    using namespace toolkit;
    using namespace toolkit::config;
    using namespace toolkit::graphics;
    using namespace toolkit::log;
    using namespace toolkit::utilities;

    class NISUpscaler : public IImageProcessor {
      public:
        NISUpscaler(std::shared_ptr<IConfigManager> configManager,
                    std::shared_ptr<IDevice> graphicsDevice,
                    int settingScaling,
                    int settingAnamorphic)
            : m_configManager(configManager), m_device(graphicsDevice),
              m_isSharpenOnly(settingScaling == 100 && settingAnamorphic <= 0) {
            initializeScaler();
        }

        void reload() override {
            initializeScaler();
        }

        void update() override {
        }

        void process(std::shared_ptr<ITexture> input,
                     std::shared_ptr<ITexture> output,
                     std::vector<std::shared_ptr<ITexture>>& textures,
                     std::array<uint8_t, 1024>& blob,
                     std::optional<utilities::Eye> eye = std::nullopt) override {
            // We need to use a per-instance blob.
            static_assert(sizeof(NISConfig) <= 1024);
            NISConfig* const config = reinterpret_cast<NISConfig*>(blob.data());

            // Update the scaler's configuration specifically for this image.
            const auto inputWidth = input->getInfo().width;
            const auto inputHeight = input->getInfo().height;
            const auto outputWidth = output->getInfo().width;
            const auto outputHeight = output->getInfo().height;
            const float sharpness = m_configManager->getValue(SettingSharpness) / 100.f;

            if (!m_isSharpenOnly) {
                NVScalerUpdateConfig(*config,
                                     sharpness,
                                     0,
                                     0,
                                     inputWidth,
                                     inputHeight,
                                     inputWidth,
                                     inputHeight,
                                     0,
                                     0,
                                     outputWidth,
                                     outputHeight,
                                     outputWidth,
                                     outputHeight,
                                     NISHDRMode::None);
            } else {
                NVSharpenUpdateConfig(
                    *config, sharpness, 0, 0, inputWidth, inputHeight, inputWidth, inputHeight, 0, 0, NISHDRMode::None);
            }

            // TODO: We can use an IShaderBuffer cache per swapchain and avoid this every frame.
            m_configBuffer->uploadData(config, sizeof(*config));

            const std::array<unsigned int, 3> threadGroups = {
                (unsigned int)std::ceil(outputWidth / float(m_optimalBlockWidth)),
                (unsigned int)std::ceil(outputHeight / float(m_optimalBlockHeight)),
                1};
            m_shader->updateThreadGroups(threadGroups);

            m_device->setShader(m_shader, SamplerType::LinearClamp);
            m_device->setShaderInput(0, m_configBuffer);
            m_device->setShaderInput(0, input);
            m_device->setShaderOutput(0, output);

            if (!m_isSharpenOnly) {
                m_device->setShaderInput(1, m_coefScale);
                m_device->setShaderInput(2, m_coefUSM);
            }

            m_device->dispatchShader();
        }

      private:
        void initializeScaler() {
            const auto shadersDir = dllHome / "shaders";
            const auto shaderFile = shadersDir / "NIS.hlsl";

            // Identify the GPU architecture in order to infer the best settings for the shader.
            const auto gpuArchitecture = m_device->GetGpuArchitecture();
            const auto nisArchitecture = gpuArchitecture == GpuArchitecture::AMD ? NISGPUArchitecture::AMD_Generic
                                         : gpuArchitecture == GpuArchitecture::Intel
                                             ? NISGPUArchitecture::Intel_Generic
                                             : NISGPUArchitecture::NVIDIA_Generic;
            NISOptimizer opt(true, nisArchitecture);

            m_optimalBlockWidth = opt.GetOptimalBlockWidth();
            m_optimalBlockHeight = opt.GetOptimalBlockHeight();

            // NISScaler/NISSharpen common
            utilities::shader::Defines defines;
            defines.add("NIS_SCALER", !m_isSharpenOnly);
            defines.add("NIS_HDR_MODE", (uint32_t)NISHDRMode::None);
            defines.add("NIS_BLOCK_WIDTH", m_optimalBlockWidth);
            defines.add("NIS_BLOCK_HEIGHT", m_optimalBlockHeight);
            defines.add("NIS_THREAD_GROUP_SIZE", opt.GetOptimalThreadGroupSize());

            if (!m_isSharpenOnly) {
                m_shader = m_device->createComputeShader(shaderFile, "main", "NISScaler CS", {}, defines.get());

                // create coefficient inputs for NISScaler only
                initializeCoefficients();

            } else {
                m_shader = m_device->createComputeShader(shaderFile, "main", "NISSharpen CS", {}, defines.get());
            }

            m_configBuffer = m_device->createBuffer(sizeof(NISConfig), "NIS Configuration CB");
        }

        void initializeCoefficients() {
            const int rowPitch = kFilterSize * 4;
            const int rowPitchAligned = alignTo(rowPitch, m_device->getTextureAlignmentConstraint());
            const int coefSize = rowPitchAligned * kPhaseCount;

            XrSwapchainCreateInfo info;
            ZeroMemory(&info, sizeof(info));
            info.width = kFilterSize / 4;
            info.height = kPhaseCount;
            info.format = m_device->getTextureFormat(TextureFormat::R32G32B32A32_FLOAT);
            info.arraySize = 1;
            info.mipCount = 1;
            info.sampleCount = 1;
            info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
            {
                std::vector<uint32_t> m_coefAligned;
                createAlignedCoefficients((uint32_t*)coef_scale, m_coefAligned, rowPitchAligned);
                m_coefScale = m_device->createTexture(
                    info, "NIS Scale Coefficients TEX2D", 0, rowPitchAligned, coefSize, (void*)m_coefAligned.data());
            }
            {
                std::vector<uint32_t> m_coefAligned;
                createAlignedCoefficients((uint32_t*)coef_usm, m_coefAligned, rowPitchAligned);
                m_coefUSM = m_device->createTexture(
                    info, "NIS USM Coefficients TEX2D", 0, rowPitchAligned, coefSize, (void*)m_coefAligned.data());
            }
        }

        // Taken directly from /NVIDIAImageScaling/samples/DX12/src/NVScaler.cpp.
        template <typename T>
        void createAlignedCoefficients(const T* data, std::vector<T>& coef, uint32_t rowPitchAligned) {
            const int rowElements = rowPitchAligned / sizeof(T);
            const int coefSize = rowElements * kPhaseCount;
            coef.resize(coefSize);
            for (uint32_t y = 0; y < kPhaseCount; ++y) {
                for (uint32_t x = 0; x < kFilterSize; ++x) {
                    coef[x + y * uint64_t(rowElements)] = data[x + y * kFilterSize];
                }
            }
        }

        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_device;
        const bool m_isSharpenOnly;

        std::shared_ptr<IComputeShader> m_shader;
        uint32_t m_optimalBlockWidth;
        uint32_t m_optimalBlockHeight;
        std::shared_ptr<IShaderBuffer> m_configBuffer;
        std::shared_ptr<ITexture> m_coefScale;
        std::shared_ptr<ITexture> m_coefUSM;
    };

} // namespace

namespace toolkit::graphics {

    std::shared_ptr<IImageProcessor> CreateNISUpscaler(std::shared_ptr<IConfigManager> configManager,
                                                       std::shared_ptr<IDevice> graphicsDevice,
                                                       int settingScaling,
                                                       int settingAnamorphic) {
        return std::make_shared<NISUpscaler>(configManager, graphicsDevice, settingScaling, settingAnamorphic);
    }

} // namespace toolkit::graphics
