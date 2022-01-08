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

#pragma once

#include "interfaces.h"

namespace toolkit {

    namespace utilities {

        std::shared_ptr<ICpuTimer> CreateCpuTimer();

        std::pair<uint32_t, uint32_t>
        GetScaledDimensions(uint32_t outputWidth, uint32_t outputHeight, uint32_t scalePercent, uint32_t blockSize);

        bool UpdateKeyState(bool& keyState, int vkModifier, int vkKey, bool isRepeat);

    } // namespace utilities

    namespace config {

        std::shared_ptr<IConfigManager> CreateConfigManager(const std::string& appName);

    } // namespace config

    namespace graphics {

        std::shared_ptr<IDevice> WrapD3D11Device(ID3D11Device* device);
        std::shared_ptr<ITexture> WrapD3D11Texture(std::shared_ptr<IDevice> device,
                                                   const XrSwapchainCreateInfo& info,
                                                   ID3D11Texture2D* texture,
                                                   const std::optional<std::string>& debugName);

        std::shared_ptr<IUpscaler> CreateNISUpscaler(std::shared_ptr<toolkit::config::IConfigManager> configManager,
                                                     std::shared_ptr<IDevice> graphicsDevice,
                                                     uint32_t outputWidth,
                                                     uint32_t outputHeight);

        std::shared_ptr<IUpscaler> CreateFSRUpscaler(std::shared_ptr<toolkit::config::IConfigManager> configManager,
                                                     std::shared_ptr<IDevice> graphicsDevice,
                                                     uint32_t outputWidth,
                                                     uint32_t outputHeight);

        std::shared_ptr<IImageProcessor>
        CreateImageProcessor(std::shared_ptr<toolkit::config::IConfigManager> configManager,
                             std::shared_ptr<IDevice> graphicsDevice,
                             const std::string& shaderFile);

    } // namespace graphics

    namespace menu {

        std::shared_ptr<IMenuHandler> CreateMenuHandler(std::shared_ptr<toolkit::config::IConfigManager> configManager,
                                                        std::shared_ptr<toolkit::graphics::IDevice> device,
                                                        uint32_t displayWidth,
                                                        uint32_t displayHeight);

    } // namespace menu

} // namespace toolkit
