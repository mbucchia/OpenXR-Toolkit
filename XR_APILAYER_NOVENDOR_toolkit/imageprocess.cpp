// MIT License
//
// Copyright(c) 2021 Matthieu Bucchianeri
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
#include "postprocess.h"

namespace toolkit {

    extern std::string dllHome;

} // namespace toolkit

namespace {

    using namespace toolkit;
    using namespace toolkit::config;
    using namespace toolkit::graphics;
    using namespace toolkit::graphics;
    using namespace toolkit::log;

    class ImageProcessor : public IImageProcessor {
      public:
        ImageProcessor(std::shared_ptr<IConfigManager> configManager,
                       std::shared_ptr<IDevice> graphicsDevice,
                       const std::string& shaderFile)
            : m_configManager(configManager), m_device(graphicsDevice) {
            const auto shadersDir = std::filesystem::path(dllHome) / std::filesystem::path("shaders");
            const auto shaderPath = shadersDir / std::filesystem::path(shaderFile);
            m_shader = m_device->createQuadShader(
                shaderPath.string(), "main", "Post-process PS", nullptr, shadersDir.string());

            utilities::shader::Defines defines;
            defines.add("VPRT", true);
            m_shaderVPRT = m_device->createQuadShader(
                shaderPath.string(), "main", "Post-process VPRT PS", defines.get(), shadersDir.string());

            // TODO: For now, we're going to require that all image processing shaders share the same configuration
            // structure.
            m_configBuffer = m_device->createBuffer(sizeof(PostProcessConfig), "Post-process Configuration CB");
        }

        void update() override {
            // TODO: Future usage: check configManager, then upload new parameters to m_configBuffer.
        }

        void process(std::shared_ptr<ITexture> input, std::shared_ptr<ITexture> output, int32_t slice) override {
            m_device->setShader(!input->isArray() ? m_shader : m_shaderVPRT);
            m_device->setShaderInput(0, m_configBuffer);
            m_device->setShaderInput(0, input, slice);
            m_device->setShaderOutput(0, output, slice);

            m_device->dispatchShader();
        }

      private:
        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_device;

        std::shared_ptr<IQuadShader> m_shader;
        std::shared_ptr<IQuadShader> m_shaderVPRT;
        std::shared_ptr<IShaderBuffer> m_configBuffer;
    };

} // namespace

namespace toolkit::graphics {

    std::shared_ptr<IImageProcessor> CreateImageProcessor(std::shared_ptr<IConfigManager> configManager,
                                                          std::shared_ptr<IDevice> graphicsDevice,
                                                          const std::string& shaderFile) {
        return std::make_shared<ImageProcessor>(configManager, graphicsDevice, shaderFile);
    }

} // namespace toolkit::graphics
