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

#pragma once

#include "interfaces.h"

namespace toolkit {

    namespace graphics {

        std::shared_ptr<IDevice> WrapD3D11Device(ID3D11Device* device);
        std::shared_ptr<ITexture> WrapD3D11Texture(std::shared_ptr<IDevice> device,
                                                   const XrSwapchainCreateInfo& info,
                                                   ID3D11Texture2D* texture,
                                                   const std::optional<std::string>& debugName);

    } // namespace graphics

    namespace menu {

        std::shared_ptr<IMenuHandler> CreateMenuHandler(std::shared_ptr<toolkit::graphics::IDevice> device);

    } // namespace menu

} // namespace toolkit
