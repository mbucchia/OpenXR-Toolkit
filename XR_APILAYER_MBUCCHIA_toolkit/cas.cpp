// MIT License
//
// Copyright(c) 2022 Matthieu Bucchianeri
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
#include <ffx_cas.h>

namespace {

    using namespace toolkit;
    using namespace toolkit::config;
    using namespace toolkit::graphics;
    using namespace toolkit::log;

    struct CASConstants {
        uint32_t Const0[4];
        uint32_t Const1[4];
    };

    class CASSharpener : public IImageProcessor {
      public:
        CASSharpener(std::shared_ptr<IConfigManager> configManager,
                    std::shared_ptr<IDevice> graphicsDevice)
            : m_configManager(configManager), m_device(graphicsDevice) {
            initializeSharpener();
        }

        void reload() override {
            initializeSharpener();
        }

        void update() override {
        }

        void process(std::shared_ptr<ITexture> input,
                     std::shared_ptr<ITexture> output,
                     std::vector<std::shared_ptr<ITexture>>& textures,
                     std::array<uint8_t, 1024>& blob,
                     std::optional<utilities::Eye> eye = std::nullopt) override {
            // We need to use a per-instance blob.
            static_assert(sizeof(CASConstants) <= 1024);
            CASConstants* const config = reinterpret_cast<CASConstants*>(blob.data());

            // Update the scaler's configuration specifically for this image.
            const auto inputWidth = input->getInfo().width;
            const auto inputHeight = input->getInfo().height;
            const auto outputWidth = output->getInfo().width;
            const auto outputHeight = output->getInfo().height;
            const float sharpness = m_configManager->getValue(SettingSharpness) / 100.f;

            CasSetup(config->Const0,
                     config->Const1,
                     AClampF1(sharpness, 0, 1),
                     static_cast<AF1>(inputWidth),
                     static_cast<AF1>(inputHeight),
                     static_cast<AF1>(outputWidth),
                     static_cast<AF1>(outputHeight));

            // TODO: We can use an IShaderBuffer cache per swapchain and avoid this every frame.
            m_configBuffer->uploadData(config, sizeof(*config));

            // This value is the image region dimension that each thread group of the CAS shader operates on
            const auto threadGroupWorkRegionDim = 16u;
            const std::array<unsigned int, 3> threadGroups = {
                (outputWidth + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim,  // dispatchX
                (outputHeight + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim, // dispatchY
                1};

            m_shaderCAS->updateThreadGroups(threadGroups);
            m_device->setShader(m_shaderCAS, SamplerType::LinearClamp);
            m_device->setShaderInput(0, m_configBuffer);
            m_device->setShaderInput(0, input);
            m_device->setShaderOutput(0, output);
            m_device->dispatchShader();
        }

      private:
        void initializeSharpener() {
            const auto shadersDir = dllHome / "shaders";
            const auto shaderFile = shadersDir / "CAS.hlsl";

            utilities::shader::Defines defines;
            defines.add("CAS_THREAD_GROUP_SIZE", 64);
            defines.add("CAS_SAMPLE_FP16", 0);
            defines.add("CAS_SAMPLE_SHARPEN_ONLY", 1);
            m_shaderCAS = m_device->createComputeShader(shaderFile, "mainCS", "CAS CS", {}, defines.get());

            m_configBuffer = m_device->createBuffer(sizeof(CASConstants), "CAS Constants CB");
        }

        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_device;

        std::shared_ptr<IComputeShader> m_shaderCAS;
        std::shared_ptr<IShaderBuffer> m_configBuffer;
    };

} // namespace

namespace toolkit::graphics {

    std::shared_ptr<IImageProcessor> CreateCASSharpener(std::shared_ptr<IConfigManager> configManager,
                                                       std::shared_ptr<IDevice> graphicsDevice) {
        return std::make_shared<CASSharpener>(configManager, graphicsDevice);
    }

} // namespace toolkit::graphics
