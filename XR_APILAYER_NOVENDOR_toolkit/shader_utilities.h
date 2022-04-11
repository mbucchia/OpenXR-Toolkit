// From https://github.com/NVIDIAGameWorks/NVIDIAImageScaling/blob/main/samples/DX11/include/DXUtilities.h

// The MIT License(MIT)
//
// Copyright(c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files(the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include "log.h"

namespace toolkit::utilities::shader {

    using namespace toolkit::log;

    void CompileShader(const std::filesystem::path& shaderFile,
                       const char* entryPoint,
                       ID3DBlob** blob,
                       const D3D_SHADER_MACRO* defines = nullptr,
                       ID3DInclude* includes = nullptr,
                       const char* target = "cs_5_0");

    void CompileShader(const void* data,
                       size_t size,
                       const char* entryPoint,
                       ID3DBlob** blob,
                       const D3D_SHADER_MACRO* defines = nullptr,
                       ID3DInclude* includes = nullptr,
                       const char* target = "cs_5_0");

    struct IncludeHeader : ID3DInclude {
        IncludeHeader(std::vector<std::filesystem::path> includePaths) : m_includePaths(std::move(includePaths)) {
        }

        HRESULT
        Open(D3D_INCLUDE_TYPE includeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes);
        HRESULT Close(LPCVOID pData);

        std::vector<std::vector<char>> m_data;
        std::vector<std::filesystem::path> m_includePaths;
    };

    namespace {
        template <typename T>
        inline std::string toStr(T value) {
            return std::to_string(value);
        }

        template <>
        inline std::string toStr<bool>(bool value) {
            return value ? "1" : "0";
        }

        template <>
        inline std::string toStr<std::string>(std::string value) {
            return value;
        }

        template <>
        inline std::string toStr<const char*>(const char* value) {
            return value;
        }
    } // namespace

    class Defines {
      public:
        template <typename T>
        void add(const std::string& define, const T& val) {
            m_definesVector.push_back({define, toStr(val)});
        }
        template <typename T>
        void set(const std::string& define, const T& val) {
            auto it = std::find_if(m_definesVector.begin(), m_definesVector.end(), [&](const auto& entry) {
                return entry.first == define;
            });
            if (it != m_definesVector.end())
                it->second = toStr(val);
        }
        D3D_SHADER_MACRO* get() {
            m_defines = std::make_unique<D3D_SHADER_MACRO[]>(m_definesVector.size() + 1);
            for (size_t i = 0; i < m_definesVector.size(); ++i)
                m_defines[i] = {m_definesVector[i].first.c_str(), m_definesVector[i].second.c_str()};
            m_defines[m_definesVector.size()] = {nullptr, nullptr};
            return m_defines.get();
        }

      private:
        std::vector<std::pair<std::string, std::string>> m_definesVector;
        std::unique_ptr<D3D_SHADER_MACRO[]> m_defines;
    };

} // namespace toolkit::utilities::shader