// MIT License
//
// Copyright(c) 2021-2022 Matthieu Bucchianeri
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

#include <layer.h>

#include "dispatch.h"
#include "log.h"

#ifndef LAYER_NAMESPACE
#error Must define LAYER_NAMESPACE
#endif

namespace LAYER_NAMESPACE {
    // The path where the DLL loads config files and stores logs.
    std::filesystem::path dllHome;

    // The path to store logs & others.
    std::filesystem::path localAppData;

    namespace log {
        // The file logger.
        std::ofstream logStream;
    } // namespace log
} // namespace LAYER_NAMESPACE

using namespace LAYER_NAMESPACE;
using namespace LAYER_NAMESPACE::log;

extern "C" {

// Entry point for the loader.
XrResult __declspec(dllexport) XRAPI_CALL
    xrNegotiateLoaderApiLayerInterface(const XrNegotiateLoaderInfo* const loaderInfo,
                                       const char* const apiLayerName,
                                       XrNegotiateApiLayerRequest* const apiLayerRequest) {
    TraceLocalActivity(local);
    TraceLoggingWriteStart(local, "xrNegotiateLoaderApiLayerInterface");

    // Retrieve the path of the DLL.
    if (dllHome.empty()) {
        HMODULE module;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)&dllHome,
                               &module)) {
            char path[_MAX_PATH];
            GetModuleFileNameA(module, path, sizeof(path));
            dllHome = std::filesystem::path(path).parent_path();
        } else {
            // Falling back to loading config/writing logs to the current working directory.
            DebugLog("Failed to locate DLL\n");
        }
    }

    // Create the necessary subfolders in LocalAppData (if they don't exist).
    localAppData = std::filesystem::path(getenv("LOCALAPPDATA")) / LayerPrettyName;
    CreateDirectoryA(localAppData.string().c_str(), nullptr);
    CreateDirectoryA((localAppData / "logs").string().c_str(), nullptr);
    CreateDirectoryA((localAppData / "screenshots").string().c_str(), nullptr);
    CreateDirectoryA((localAppData / "configs").string().c_str(), nullptr);

    // Start logging to file.
    if (!logStream.is_open()) {
        std::string logFile = (localAppData / "logs" / (LayerName + ".log")).string();
        logStream.open(logFile, std::ios_base::ate);
    }

    Log("%s\n", LayerPrettyNameFull.c_str());
    Log("dllHome is \"%s\"\n", dllHome.string().c_str());

    // Initialize Detours early on.
    DetourRestoreAfterWith();

    if (apiLayerName && apiLayerName != LayerName) {
        Log("Invalid apiLayerName \"%s\"\n", apiLayerName);
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    if (!loaderInfo || !apiLayerRequest || loaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
        loaderInfo->structVersion != XR_LOADER_INFO_STRUCT_VERSION ||
        loaderInfo->structSize != sizeof(XrNegotiateLoaderInfo) ||
        apiLayerRequest->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST ||
        apiLayerRequest->structVersion != XR_API_LAYER_INFO_STRUCT_VERSION ||
        apiLayerRequest->structSize != sizeof(XrNegotiateApiLayerRequest) ||
        loaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxApiVersion < XR_CURRENT_API_VERSION || loaderInfo->minApiVersion > XR_CURRENT_API_VERSION) {
        Log("xrNegotiateLoaderApiLayerInterface validation failed\n");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Setup our layer to intercept OpenXR calls.
    apiLayerRequest->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
    apiLayerRequest->layerApiVersion = XR_CURRENT_API_VERSION;
    apiLayerRequest->getInstanceProcAddr = reinterpret_cast<PFN_xrGetInstanceProcAddr>(xrGetInstanceProcAddr);
    apiLayerRequest->createApiLayerInstance = reinterpret_cast<PFN_xrCreateApiLayerInstance>(xrCreateApiLayerInstance);

    Log("%s layer is active\n", LayerPrettyName.c_str());

    TraceLoggingWriteStop(local, "xrNegotiateLoaderApiLayerInterface");

    return XR_SUCCESS;
}

__declspec(dllexport) const char* WINAPI getVersionString() {
    return LayerPrettyNameFull.c_str();
}
}
