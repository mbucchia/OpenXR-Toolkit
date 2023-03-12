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

#include "pch.h"

namespace xr {
    static inline std::string ToString(XrVersion version) {
        return fmt::format("{}.{}.{}", XR_VERSION_MAJOR(version), XR_VERSION_MINOR(version), XR_VERSION_PATCH(version));
    }

    static inline std::string ToString(XrPosef pose) {
        return fmt::format("p: ({:.3f}, {:.3f}, {:.3f}), o:({:.3f}, {:.3f}, {:.3f}, {:.3f})",
                           pose.position.x,
                           pose.position.y,
                           pose.position.z,
                           pose.orientation.x,
                           pose.orientation.y,
                           pose.orientation.z,
                           pose.orientation.w);
    }

    static inline std::string ToString(XrVector3f vec) {
        return fmt::format("({:.3f}, {:.3f}, {:.3f})", vec.x, vec.y, vec.z);
    }

    static inline std::string ToString(XrFovf fov) {
        return fmt::format(
            "(l:{:.3f}, r:{:.3f}, u:{:.3f}, d:{:.3f})", fov.angleLeft, fov.angleRight, fov.angleUp, fov.angleDown);
    }

    static inline std::string ToString(XrRect2Di rect) {
        return fmt::format("x:{}, y:{} w:{} h:{}", rect.offset.x, rect.offset.y, rect.extent.width, rect.extent.height);
    }
}

namespace toolkit::log {

    TRACELOGGING_DECLARE_PROVIDER(g_traceProvider);

    extern TraceLoggingActivity<g_traceProvider> g_traceGlobal;

#define IsTraceEnabled() TraceLoggingProviderEnabled(g_traceProvider, 0, 0)

#define TraceLocalActivity(activity) TraceLoggingActivity<g_traceProvider> activity;

#define TLArg(var, ...) TraceLoggingValue(var, ##__VA_ARGS__)
#define TLPArg(var, ...) TraceLoggingValue(fmt::format("0x{:08x}", (uintptr_t)(var)).c_str(), ##__VA_ARGS__)

    // General logging function.
    void Log(const char* fmt, ...);

    // Debug logging function. Can make things very slow (only enabled on Debug builds).
    void DebugLog(const char* fmt, ...);

} // namespace toolkit::log
