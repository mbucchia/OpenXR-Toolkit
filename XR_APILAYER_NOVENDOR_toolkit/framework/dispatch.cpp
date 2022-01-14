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

#include <layer.h>

#include "dispatch.h"
#include "log.h"

#ifndef LAYER_NAMESPACE
#error Must define LAYER_NAMESPACE
#endif

using namespace LAYER_NAMESPACE::log;

namespace LAYER_NAMESPACE {

    // Entry point for creating the layer.
    XrResult xrCreateApiLayerInstance(const XrInstanceCreateInfo* const instanceCreateInfo,
                                      const struct XrApiLayerCreateInfo* const apiLayerInfo,
                                      XrInstance* const instance) {
        DebugLog("--> xrCreateApiLayerInstance\n");

        if (!apiLayerInfo || apiLayerInfo->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_CREATE_INFO ||
            apiLayerInfo->structVersion != XR_API_LAYER_CREATE_INFO_STRUCT_VERSION ||
            apiLayerInfo->structSize != sizeof(XrApiLayerCreateInfo) || !apiLayerInfo->nextInfo ||
            apiLayerInfo->nextInfo->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_NEXT_INFO ||
            apiLayerInfo->nextInfo->structVersion != XR_API_LAYER_NEXT_INFO_STRUCT_VERSION ||
            apiLayerInfo->nextInfo->structSize != sizeof(XrApiLayerNextInfo) ||
            apiLayerInfo->nextInfo->layerName != LayerName || !apiLayerInfo->nextInfo->nextGetInstanceProcAddr ||
            !apiLayerInfo->nextInfo->nextCreateApiLayerInstance) {
            Log("xrCreateApiLayerInstance validation failed\n");
            return XR_ERROR_INITIALIZATION_FAILED;
        }

        // Determine whether we are invoked from the OpenXR Developer Tools for Windows Mixed Reality.
        // If we are, we will skip dummy instance create to avoid he XR_LIMIT_REACHED error.
        const bool fastInitialization =
            std::string(instanceCreateInfo->applicationInfo.engineName) == "OpenXRDeveloperTools";

        // Check that the extensions we need are supported by the runtime and/or an upstream API layer.
        // But first, we need to create a dummy instance in order to be able to perform these checks.
        bool hasHandTrackingExt = false;
        bool hasConvertPerformanceCounterTimeExt = false;
        if (!fastInitialization) {
            XrInstance dummyInstance = XR_NULL_HANDLE;
            PFN_xrEnumerateInstanceExtensionProperties xrEnumerateInstanceExtensionProperties = nullptr;
            PFN_xrDestroyInstance xrDestroyInstance = nullptr;

            // We patch the application name for telemetry purposes.
            XrInstanceCreateInfo dummyCreateInfo = *instanceCreateInfo;
            strcpy_s(dummyCreateInfo.applicationInfo.applicationName, "OpenXR-Toolkit");
            strcpy_s(dummyCreateInfo.applicationInfo.engineName, "OpenXR-Toolkit");
            dummyCreateInfo.applicationInfo.applicationVersion = dummyCreateInfo.applicationInfo.engineVersion =
                (uint32_t)XR_MAKE_VERSION(VersionMajor, VersionMinor, VersionPatch);

            // Try to speed things up by requesting no extentions.
            dummyCreateInfo.enabledExtensionCount = dummyCreateInfo.enabledApiLayerCount = 0;

            // Call the chain to create the dummy instance.
            XrApiLayerCreateInfo chainApiLayerInfo = *apiLayerInfo;
            chainApiLayerInfo.nextInfo = apiLayerInfo->nextInfo->next;

            const XrResult result = apiLayerInfo->nextInfo->nextCreateApiLayerInstance(
                &dummyCreateInfo, &chainApiLayerInfo, &dummyInstance);
            if (result == XR_SUCCESS) {
                CHECK_XRCMD(apiLayerInfo->nextInfo->nextGetInstanceProcAddr(
                    dummyInstance,
                    "xrEnumerateInstanceExtensionProperties",
                    reinterpret_cast<PFN_xrVoidFunction*>(&xrEnumerateInstanceExtensionProperties)));
                CHECK_XRCMD(apiLayerInfo->nextInfo->nextGetInstanceProcAddr(
                    dummyInstance, "xrDestroyInstance", reinterpret_cast<PFN_xrVoidFunction*>(&xrDestroyInstance)));
            } else {
                Log("Failed to create bootstrap instance: %d\n", result);
            }

            if (xrEnumerateInstanceExtensionProperties) {
                uint32_t extensionsCount = 0;
                CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionsCount, nullptr));
                std::vector<XrExtensionProperties> extensions(extensionsCount, {XR_TYPE_EXTENSION_PROPERTIES});
                CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(
                    nullptr, extensionsCount, &extensionsCount, extensions.data()));
                for (auto extension : extensions) {
                    const std::string extensionName(extension.extensionName);

                    if (extensionName == "XR_EXT_hand_tracking") {
                        hasHandTrackingExt = true;
                    } else if (extensionName == "XR_KHR_win32_convert_performance_counter_time") {
                        hasConvertPerformanceCounterTimeExt = true;
                    }
                }
            }

            if (xrDestroyInstance) {
                xrDestroyInstance(dummyInstance);
            }
        }

        // Add the extra extensions to the list of requested extensions when available.
        XrInstanceCreateInfo chainInstanceCreateInfo = *instanceCreateInfo;
        std::vector<const char*> newEnabledExtensionNames;
        if (!fastInitialization) {
            if (hasHandTrackingExt || hasConvertPerformanceCounterTimeExt) {
                if (hasHandTrackingExt) {
                    chainInstanceCreateInfo.enabledExtensionCount++;
                }
                if (hasConvertPerformanceCounterTimeExt) {
                    chainInstanceCreateInfo.enabledExtensionCount++;
                }

                newEnabledExtensionNames.resize(chainInstanceCreateInfo.enabledExtensionCount);
                chainInstanceCreateInfo.enabledExtensionNames = newEnabledExtensionNames.data();
                memcpy(newEnabledExtensionNames.data(),
                       instanceCreateInfo->enabledExtensionNames,
                       instanceCreateInfo->enabledExtensionCount * sizeof(const char*));
                uint32_t nextExtensionSlot = instanceCreateInfo->enabledExtensionCount;

                if (hasHandTrackingExt) {
                    newEnabledExtensionNames[nextExtensionSlot++] = "XR_EXT_hand_tracking";
                } else {
                    Log("XR_EXT_hand_tracking is not available from the OpenXR runtime or any upsteam API "
                        "layer.\n");
                }
                if (hasConvertPerformanceCounterTimeExt) {
                    newEnabledExtensionNames[nextExtensionSlot++] = "XR_KHR_win32_convert_performance_counter_time";
                }
            }
        }

        // Call the chain to create the instance.
        XrApiLayerCreateInfo chainApiLayerInfo = *apiLayerInfo;
        chainApiLayerInfo.nextInfo = apiLayerInfo->nextInfo->next;
        XrResult result =
            apiLayerInfo->nextInfo->nextCreateApiLayerInstance(&chainInstanceCreateInfo, &chainApiLayerInfo, instance);
        if (result == XR_SUCCESS) {
            // Create our layer.
            LAYER_NAMESPACE::GetInstance()->SetGetInstanceProcAddr(apiLayerInfo->nextInfo->nextGetInstanceProcAddr,
                                                                   *instance);

            result = XR_ERROR_RUNTIME_FAILURE;

            // Forward the xrCreateInstance() call to the layer.
            try {
                result = LAYER_NAMESPACE::GetInstance()->xrCreateInstance(instanceCreateInfo);
            } catch (std::runtime_error exc) {
                Log("%s\n", exc.what());
            }

            // Cleanup attempt before returning an error.
            if (XR_FAILED(result)) {
                PFN_xrDestroyInstance xrDestroyInstance = nullptr;
                if (XR_SUCCEEDED(apiLayerInfo->nextInfo->nextGetInstanceProcAddr(
                        *instance, "xrDestroyInstance", reinterpret_cast<PFN_xrVoidFunction*>(&xrDestroyInstance)))) {
                    xrDestroyInstance(*instance);
                }
            }
        }

        DebugLog("<-- xrCreateApiLayerInstance %d\n", result);

        return result;
    }

    // Handle cleanup of the layer's singleton.
    XrResult xrDestroyInstance(XrInstance instance) {
        DebugLog("--> xrDestroyInstance\n");

        XrResult result;
        try {
            result = LAYER_NAMESPACE::GetInstance()->xrDestroyInstance(instance);
            if (XR_SUCCEEDED(result)) {
                LAYER_NAMESPACE::ResetInstance();
            }
        } catch (std::runtime_error exc) {
            Log("%s\n", exc.what());
            result = XR_ERROR_RUNTIME_FAILURE;
        }

        DebugLog("<-- xrDestroyInstance %d\n", result);

        return result;
    }

    // Forward the xrGetInstanceProcAddr() call to the dispatcher.
    XrResult xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function) {
        try {
            return LAYER_NAMESPACE::GetInstance()->xrGetInstanceProcAddr(instance, name, function);
        } catch (std::runtime_error exc) {
            Log("%s\n", exc.what());
            return XR_ERROR_RUNTIME_FAILURE;
        }
    }

} // namespace LAYER_NAMESPACE
