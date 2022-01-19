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
#include "log.h"

#pragma warning(push)
#pragma warning(disable : 4305)
#include <NIS_Config.h>
#pragma warning(pop)

namespace toolkit {

    extern std::string dllHome;

} // namespace toolkit

namespace {

    using namespace toolkit;
    using namespace toolkit::config;
    using namespace toolkit::graphics;
    using namespace toolkit::log;
    using namespace toolkit::utilities;

    class NISUpscaler : public IUpscaler {
      public:
        NISUpscaler(std::shared_ptr<IConfigManager> configManager,
                    std::shared_ptr<IDevice> graphicsDevice,
                    uint32_t outputWidth,
                    uint32_t outputHeight)
            : m_configManager(configManager), m_device(graphicsDevice), m_outputWidth(outputWidth),
              m_outputHeight(outputHeight) {
            // Identify the GPU architecture in order to infer the best settings for the shader.
            NISGPUArchitecture gpuArch = NISGPUArchitecture::NVIDIA_Generic;
            std::string lowercaseDeviceName = m_device->getDeviceName();
            std::transform(lowercaseDeviceName.begin(),
                           lowercaseDeviceName.end(),
                           lowercaseDeviceName.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (lowercaseDeviceName.find("intel") != std::string::npos) {
                gpuArch = NISGPUArchitecture::Intel_Generic;
            } else if (lowercaseDeviceName.find("amd") != std::string::npos) {
                gpuArch = NISGPUArchitecture::AMD_Generic;
            }

            NISOptimizer opt(true, gpuArch);
            m_blockWidth = opt.GetOptimalBlockWidth();
            m_blockHeight = opt.GetOptimalBlockHeight();
            m_threadGroupSize = opt.GetOptimalThreadGroupSize();

            // The upscaling factor is only read upon initialization of the session. It cannot be changed after.
            std::tie(m_inputWidth, m_inputHeight) = utilities::GetScaledDimensions(
                m_outputWidth, m_outputHeight, m_configManager->getValue(SettingScaling), 2);

            if (m_inputWidth != m_outputWidth || m_inputHeight != m_outputHeight) {
                initializeScaler();
            } else {
                initializeSharpen();
            }

            // TODO: Consider making immutable and create a new buffer in update(). For now, our D3D12 implementation
            // does not do heap descriptor recycling.
            m_configBuffer = m_device->createBuffer(sizeof(NISConfig), "NIS Configuration CB");
            update();
        }

        void update() override {
            if (m_configManager->hasChanged(SettingSharpness)) {
                const float sharpness = m_configManager->getValue(SettingSharpness) / 100.0f;

                NISConfig config;
                if (!m_isSharpenOnly) {
                    NVScalerUpdateConfig(config,
                                         sharpness,
                                         0,
                                         0,
                                         m_inputWidth,
                                         m_inputHeight,
                                         m_inputWidth,
                                         m_inputHeight,
                                         0,
                                         0,
                                         m_outputWidth,
                                         m_outputHeight,
                                         m_outputWidth,
                                         m_outputHeight,
                                         NISHDRMode::None);
                } else {
                    NVSharpenUpdateConfig(config,
                                          sharpness,
                                          0,
                                          0,
                                          m_inputWidth,
                                          m_inputHeight,
                                          m_inputWidth,
                                          m_inputHeight,
                                          0,
                                          0,
                                          NISHDRMode::None);
                }

                m_configBuffer->uploadData(&config, sizeof(config));
            }
        }

        void upscale(std::shared_ptr<ITexture> input, std::shared_ptr<ITexture> output, int32_t slice = -1) override {
            m_device->setShader(!input->isArray() ? m_shader : m_shaderVPRT);
            m_device->setShaderInput(0, m_configBuffer);
            m_device->setShaderInput(0, input, slice);
            m_device->setShaderOutput(0, output, slice);
            if (!m_isSharpenOnly) {
                m_device->setShaderInput(1, m_coefScale);
                m_device->setShaderInput(2, m_coefUSM);
            }

            m_device->dispatchShader();
        }

      private:
        void initializeScaler() {
            const auto shadersDir = std::filesystem::path(dllHome) / std::filesystem::path("shaders");
            const auto shaderPath = shadersDir / std::filesystem::path("NIS.hlsl");

            utilities::shader::Defines defines;
            defines.add("NIS_SCALER", true);
            defines.add("NIS_HDR_MODE", (uint32_t)NISHDRMode::None);
            defines.add("NIS_BLOCK_WIDTH", m_blockWidth);
            defines.add("NIS_BLOCK_HEIGHT", m_blockHeight);
            defines.add("NIS_THREAD_GROUP_SIZE", m_threadGroupSize);

            const std::array<unsigned int, 3> threadGroups = {
                (unsigned int)std::ceil(m_outputWidth / float(m_blockWidth)),
                (unsigned int)std::ceil(m_outputHeight / float(m_blockHeight)),
                1};

            m_shader = m_device->createComputeShader(
                shaderPath.string(), "main", "NISScaler CS", threadGroups, defines.get(), shadersDir.string());

            defines.add("VPRT", true);
            m_shaderVPRT = m_device->createComputeShader(
                shaderPath.string(), "main", "NISScaler VPRT CS", threadGroups, defines.get(), shadersDir.string());

            const int rowPitch = kFilterSize * 4;
            const int rowPitchAligned = Align(rowPitch, m_device->getTextureAlignmentConstraint());
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
                    info, "NIS Scale Coefficients TEX2D", rowPitchAligned, coefSize, (void*)m_coefAligned.data());
            }
            {
                std::vector<uint32_t> m_coefAligned;
                createAlignedCoefficients((uint32_t*)coef_usm, m_coefAligned, rowPitchAligned);
                m_coefUSM = m_device->createTexture(
                    info, "NIS USM Coefficients TEX2D", rowPitch, coefSize, (void*)m_coefAligned.data());
            }

            m_isSharpenOnly = false;
        }

        void initializeSharpen() {
            const auto shadersDir = std::filesystem::path(dllHome) / std::filesystem::path("shaders");
            const auto shaderPath = shadersDir / std::filesystem::path("NIS.hlsl");

            utilities::shader::Defines defines;
            defines.add("NIS_SCALER", false);
            defines.add("NIS_HDR_MODE", (uint32_t)NISHDRMode::None);
            defines.add("NIS_BLOCK_WIDTH", m_blockWidth);
            defines.add("NIS_BLOCK_HEIGHT", m_blockHeight);
            defines.add("NIS_THREAD_GROUP_SIZE", m_threadGroupSize);

            const std::array<unsigned int, 3> threadGroups = {
                (unsigned int)std::ceil(m_outputWidth / float(m_blockWidth)),
                (unsigned int)std::ceil(m_outputHeight / float(m_blockHeight)),
                1};

            m_shader = m_device->createComputeShader(
                shaderPath.string(), "main", "NISSharpen CS", threadGroups, defines.get(), shadersDir.string());

            defines.add("VPRT", true);
            m_shaderVPRT = m_device->createComputeShader(
                shaderPath.string(), "main", "NISSharpen VPRT CS", threadGroups, defines.get(), shadersDir.string());

            // Sharpen does not use the coefficient inputs.
            m_isSharpenOnly = true;
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
        const uint32_t m_outputWidth;
        const uint32_t m_outputHeight;

        uint32_t m_blockWidth;
        uint32_t m_blockHeight;
        uint32_t m_threadGroupSize;
        uint32_t m_inputWidth;
        uint32_t m_inputHeight;
        std::shared_ptr<IComputeShader> m_shader;
        std::shared_ptr<IComputeShader> m_shaderVPRT;
        bool m_isSharpenOnly{false};
        std::shared_ptr<IShaderBuffer> m_configBuffer;
        std::shared_ptr<ITexture> m_coefScale;
        std::shared_ptr<ITexture> m_coefUSM;
    };

} // namespace

namespace toolkit::graphics {

    std::shared_ptr<IUpscaler> CreateNISUpscaler(std::shared_ptr<IConfigManager> configManager,
                                                 std::shared_ptr<IDevice> graphicsDevice,
                                                 uint32_t outputWidth,
                                                 uint32_t outputHeight) {
        return std::make_shared<NISUpscaler>(configManager, graphicsDevice, outputWidth, outputHeight);
    }

} // namespace toolkit::graphics
