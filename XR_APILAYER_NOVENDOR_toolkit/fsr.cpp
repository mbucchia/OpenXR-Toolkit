// MIT License
//
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

#define A_CPU
#include <ffx_a.h>
#include <ffx_fsr1.h>

namespace {

    using namespace toolkit;
    using namespace toolkit::config;
    using namespace toolkit::graphics;
    using namespace toolkit::log;

    struct FSRConstants {
        uint32_t Const0[4];
        uint32_t Const1[4];
        uint32_t Const2[4];
        uint32_t Const3[4];
        uint32_t Const4[4];
    };

    class FSRUpscaler : public IImageProcessor {
      public:
        FSRUpscaler(std::shared_ptr<IConfigManager> configManager,
                    std::shared_ptr<IDevice> graphicsDevice,
                    uint32_t outputWidth,
                    uint32_t outputHeight)
            : m_configManager(configManager), m_device(graphicsDevice), m_outputWidth(outputWidth),
              m_outputHeight(outputHeight) {
            // The upscaling factor is only read upon initialization of the session. It cannot be changed after.
            std::tie(m_inputWidth, m_inputHeight) =
                config::GetScaledDimensions(m_configManager.get(), m_outputWidth, m_outputHeight, 2);

            m_isSharpenOnly = m_inputWidth == m_outputWidth && m_inputHeight == m_outputHeight;
            initializeScaler();
        }

        void update() override {
            if (m_configManager->hasChanged(SettingSharpness)) {
                updateScaler(m_configManager->getValue(SettingSharpness) / 100.f);
            }
        }

        void process(std::shared_ptr<ITexture> input, std::shared_ptr<ITexture> output, int32_t slice = -1) override {
            if (!m_intermediary) {
                const auto& infos = output->getInfo();
                initializeIntermediary(infos.width, infos.height, 0 /* infos.format */);
            }

            if (!m_isSharpenOnly) {
                m_device->setShader(m_shaderEASU);
                m_device->setShaderInput(0, m_configBuffer);
                m_device->setShaderInput(0, input, slice);
                m_device->setShaderOutput(0, m_intermediary);
                m_device->dispatchShader();
            }

            m_device->setShader(m_shaderRCAS);
            m_device->setShaderInput(0, m_configBuffer);
            m_device->setShaderInput(0, m_isSharpenOnly ? input : m_intermediary);
            m_device->setShaderOutput(0, output, slice);
            m_device->dispatchShader();
        }

      private:
        void initializeScaler() {
            const auto shadersDir = dllHome / "shaders";
            const auto shaderPath = shadersDir / "FSR.hlsl";

            // This value is the image region dimension that each thread group of the FSR shader operates on
            const auto threadGroupWorkRegionDim = 16u;
            const std::array<unsigned int, 3> threadGroups = {
                (m_outputWidth + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim,  // dispatchX
                (m_outputHeight + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim, // dispatchY
                1};

            // EASU/RCAS common
            utilities::shader::Defines defines;
            defines.add("FSR_THREAD_GROUP_SIZE", 64);
            defines.add("SAMPLE_SLOW_FALLBACK", 1);
            defines.add("SAMPLE_BILINEAR", 0);
            defines.add("SAMPLE_HDR_OUTPUT", 0);
            defines.add("FSR_RCAS_DENOISE", 0);

            // EASU specific
            defines.add("SAMPLE_RCAS", 0);
            defines.add("SAMPLE_EASU", 1);
            m_shaderEASU = m_device->createComputeShader(
                shaderPath.string(), "mainCS", "FSR EASU CS", threadGroups, defines.get(), shadersDir.string());

            // RCAS specific
            defines.set("SAMPLE_EASU", 0);
            defines.set("SAMPLE_RCAS", 1);
            m_shaderRCAS = m_device->createComputeShader(
                shaderPath.string(), "mainCS", "FSR RCAS CS", threadGroups, defines.get(), shadersDir.string());

            // TODO: Consider making immutable and create a new buffer in update(). For now, our D3D12 implementation
            // does not do heap descriptor recycling.
            m_configBuffer = m_device->createBuffer(sizeof(FSRConstants), "FSR Constants CB");
            updateScaler(m_configManager->getValue(SettingSharpness) / 100.f);
        }

        void initializeIntermediary(uint32_t width, uint32_t height, int64_t format) {
            if (format == 0) {
                // good balance between visuals and perf
                format = m_device->getTextureFormat(TextureFormat::R16G16B16A16_UNORM);
            }

            DebugLog("FSRUpscaler initializeIntermediary with %u, %u, %u\n", width, height, format);

            if (m_device->isTextureFormatSRGB(format)) {
                format = DXGI_FORMAT_R8G8B8A8_UNORM;
                DebugLog("  sRGB output format changed to: %u\n", format);
            }

            // create the intermediary texture between upscale and sharpen pass
            XrSwapchainCreateInfo info;
            ZeroMemory(&info, sizeof(info));
            info.width = width;
            info.height = height;
            info.format = format;
            info.arraySize = 1;
            info.mipCount = 1;
            info.sampleCount = 1;
            info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;
            m_intermediary = m_device->createTexture(info, "FSR Intermediary TEX2D");
        }

        void updateScaler(float sharpness) {
            const auto attenuation = 1.f - AClampF1(sharpness, 0, 1);

            FSRConstants config = {};
            if (!m_isSharpenOnly) {
                FsrEasuCon(config.Const0,
                           config.Const1,
                           config.Const2,
                           config.Const3,
                           static_cast<AF1>(m_inputWidth),
                           static_cast<AF1>(m_inputHeight),
                           static_cast<AF1>(m_inputWidth),
                           static_cast<AF1>(m_inputHeight),
                           static_cast<AF1>(m_outputWidth),
                           static_cast<AF1>(m_outputHeight));
            }

            FsrRcasCon(config.Const4, static_cast<AF1>(attenuation));

            // TODO:
            // The AMD FSR sample is using a value in the constant buffer to correct the output color accordingly.
            // We're replacing the constant with a shader compilation define because the project code is not HDR
            // aware yet, When we'll be supporting HDR, we might need to change the implementation back to something
            // like:
            //
            // config.Const4[3] = hdr ? 1 : 0;

            m_configBuffer->uploadData(&config, sizeof(config));
        }

        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_device;
        const uint32_t m_outputWidth;
        const uint32_t m_outputHeight;

        uint32_t m_inputWidth;
        uint32_t m_inputHeight;
        bool m_isSharpenOnly{false};

        std::shared_ptr<IComputeShader> m_shaderEASU;
        std::shared_ptr<IComputeShader> m_shaderRCAS;
        std::shared_ptr<IShaderBuffer> m_configBuffer;
        std::shared_ptr<ITexture> m_intermediary;
    };

} // namespace

namespace toolkit::graphics {

    std::shared_ptr<IImageProcessor> CreateFSRUpscaler(std::shared_ptr<IConfigManager> configManager,
                                                       std::shared_ptr<IDevice> graphicsDevice,
                                                       uint32_t outputWidth,
                                                       uint32_t outputHeight) {
        return std::make_shared<FSRUpscaler>(configManager, graphicsDevice, outputWidth, outputHeight);
    }

} // namespace toolkit::graphics
