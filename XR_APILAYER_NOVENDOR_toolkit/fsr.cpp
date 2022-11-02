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
                     std::array<uint8_t, 1024>& blob) override {
            // We need to use a per-instance blob.
            static_assert(sizeof(FSRConstants) <= 1024);
            FSRConstants* const config = reinterpret_cast<FSRConstants*>(blob.data());

            // Update the scaler's configuration specifically for this image.
            const auto inputWidth = input->getInfo().width;
            const auto inputHeight = input->getInfo().height;
            const auto outputWidth = output->getInfo().width;
            const auto outputHeight = output->getInfo().height;
            const float sharpness = m_configManager->getValue(SettingSharpness) / 100.f;

            if (!m_isSharpenOnly) {
                FsrEasuCon(config->Const0,
                           config->Const1,
                           config->Const2,
                           config->Const3,
                           static_cast<AF1>(inputWidth),
                           static_cast<AF1>(inputHeight),
                           static_cast<AF1>(inputWidth),
                           static_cast<AF1>(inputHeight),
                           static_cast<AF1>(outputWidth),
                           static_cast<AF1>(outputHeight));
            }

            const auto attenuation = 1.f - AClampF1(sharpness, 0, 1);
            FsrRcasCon(config->Const4, static_cast<AF1>(attenuation));

            // TODO:
            // The AMD FSR sample is using a value in the constant buffer to correct the output color accordingly.
            // We're replacing the constant with a shader compilation define because the project code is not HDR
            // aware yet, When we'll be supporting HDR, we might need to change the implementation back to something
            // like:
            //
            // config.Const4[3] = hdr ? 1 : 0;

            // TODO: We can use an IShaderBuffer cache per swapchain and avoid this every frame.
            m_configBuffer->uploadData(config, sizeof(*config));

            // This value is the image region dimension that each thread group of the FSR shader operates on
            const auto threadGroupWorkRegionDim = 16u;
            const std::array<unsigned int, 3> threadGroups = {
                (outputWidth + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim,  // dispatchX
                (outputHeight + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim, // dispatchY
                1};

            // Create the intermediate texture if needed.
            if (!m_isSharpenOnly) {
                const auto outputInfo = output->getInfo();
                if (textures.empty() || textures[0]->getInfo().width != outputInfo.width ||
                    textures[0]->getInfo().height != outputInfo.height) {
                    textures.clear();
                    auto createInfo = outputInfo;

                    // Good balance between visuals and performance.
                    createInfo.format = m_device->getTextureFormat(TextureFormat::R16G16B16A16_UNORM);

                    createInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;
                    textures.push_back(m_device->createTexture(createInfo, "FSR Intermediate TEX2D"));
                }

                m_shaderEASU->updateThreadGroups(threadGroups);
                m_device->setShader(m_shaderEASU, SamplerType::LinearClamp);
                m_device->setShaderInput(0, m_configBuffer);
                m_device->setShaderInput(0, input);
                m_device->setShaderOutput(0, textures[0]);
                m_device->dispatchShader();
            }

            m_shaderRCAS->updateThreadGroups(threadGroups);
            m_device->setShader(m_shaderRCAS, SamplerType::LinearClamp);
            m_device->setShaderInput(0, m_configBuffer);
            m_device->setShaderInput(0, m_isSharpenOnly ? input : textures[0]);
            m_device->setShaderOutput(0, output);
            m_device->dispatchShader();
        }

      private:
        void initializeScaler() {
            const auto shadersDir = dllHome / "shaders";
            const auto shaderFile = shadersDir / "FSR.hlsl";

            // EASU/RCAS common
            utilities::shader::Defines defines;
            defines.add("FSR_THREAD_GROUP_SIZE", 64);
            defines.add("SAMPLE_SLOW_FALLBACK", 1);
            defines.add("SAMPLE_BILINEAR", 0);

            // EASU specific
            defines.add("SAMPLE_RCAS", 0);
            defines.add("SAMPLE_EASU", 1);
            defines.add("SAMPLE_HDR_OUTPUT", 0);
            m_shaderEASU = m_device->createComputeShader(shaderFile, "mainCS", "FSR EASU CS", {}, defines.get());

            // RCAS specific
            defines.set("SAMPLE_EASU", 0);
            defines.set("SAMPLE_RCAS", 1);
            defines.add("SAMPLE_HDR_OUTPUT", 1);
            m_shaderRCAS = m_device->createComputeShader(shaderFile, "mainCS", "FSR RCAS CS", {}, defines.get());

            m_configBuffer = m_device->createBuffer(sizeof(FSRConstants), "FSR Constants CB");
        }

        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_device;
        const bool m_isSharpenOnly;

        std::shared_ptr<IComputeShader> m_shaderEASU;
        std::shared_ptr<IComputeShader> m_shaderRCAS;
        std::shared_ptr<IShaderBuffer> m_configBuffer;
    };

} // namespace

namespace toolkit::graphics {

    std::shared_ptr<IImageProcessor> CreateFSRUpscaler(std::shared_ptr<IConfigManager> configManager,
                                                       std::shared_ptr<IDevice> graphicsDevice,
                                                       int settingScaling,
                                                       int settingAnamorphic) {
        return std::make_shared<FSRUpscaler>(configManager, graphicsDevice, settingScaling, settingAnamorphic);
    }

} // namespace toolkit::graphics
