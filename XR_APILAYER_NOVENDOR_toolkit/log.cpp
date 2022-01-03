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

namespace toolkit::log {
    extern std::ofstream logStream;

    namespace {

        // Utility logging function.
        void InternalLog(const char* fmt, va_list va) {
            const std::time_t now = std::time(nullptr);

            char buf[1024];
            size_t offset =
                std::strftime(buf, sizeof(buf), "[OXRTK] %Y-%m-%d %H:%M:%S %z: ", std::localtime(&now));
            _vsnprintf_s(buf + offset, sizeof(buf) - offset, sizeof(buf) - offset, fmt, va);
            OutputDebugStringA(buf);
            if (logStream.is_open()) {
                logStream << buf;
                logStream.flush();
            }
        }
    } // namespace

    void Log(const char* fmt, ...) {
        va_list va;
        va_start(va, fmt);
        InternalLog(fmt, va);
        va_end(va);
    }

    void DebugLog(const char* fmt, ...) {
#ifdef _DEBUG
        va_list va;
        va_start(va, fmt);
        InternalLog(fmt, va);
        va_end(va);
#endif
    }

} // namespace toolkit::log
