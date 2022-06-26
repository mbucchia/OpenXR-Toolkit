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

#include "pch.h"

#include "factories.h"
#include "interfaces.h"
#include "layer.h"
#include "log.h"

namespace {

    using namespace toolkit;
    using namespace toolkit::log;
    using namespace toolkit::math;
    using namespace xr::math;

    // We are going to need to reconstruct a writable version of the frame info.
    // We keep a global pool of resources to reduce fragmentation every frame.

    static std::vector<const XrCompositionLayerBaseHeader*> gLayerHeaders;
    static std::vector<XrCompositionLayerProjection> gLayerProjections;
    static std::vector<XrCompositionLayerProjectionView> gLayerProjectionsViews;
    static XrCompositionLayerQuad gLayerQuadForMenu;

    // Up to 3 image processing stages are supported.
    enum ImgProc { Pre, Scale, Post, MaxValue };

    struct SwapchainImages {
        std::vector<std::shared_ptr<graphics::ITexture>> chain;
        std::shared_ptr<graphics::IGpuTimer> gpuTimers[ImgProc::MaxValue][utilities::ViewCount];
    };

    struct SwapchainState {
        std::vector<SwapchainImages> images;
        uint32_t acquiredImageIndex{0};
        bool delayedRelease{false};
        bool registeredWithFrameAnalyzer{false};
    };

    class OpenXrLayer : public toolkit::OpenXrApi {
      public:
        OpenXrLayer() = default;

        ~OpenXrLayer() override {
            // We cleanup after ourselves (again) to avoid leaving state registry entries.
            utilities::ClearWindowsMixedRealityReprojection();

            if (m_configManager) {
                m_configManager->setActiveSession("");
            }

            graphics::UnhookForD3D11DebugLayer();
        }

        void setOptionsDefaults() {
            m_configManager->setDefault(config::SettingDeveloper, 0);

            // Input & menu options.
            m_configManager->setDefault(config::SettingKeyCtrlModifier, 1);
            m_configManager->setDefault(config::SettingKeyAltModifier, 0);
            m_configManager->setDefault(config::SettingMenuKeyLeft, VK_F1);
            m_configManager->setDefault(config::SettingMenuKeyRight, VK_F3);
            m_configManager->setDefault(config::SettingMenuKeyDown, VK_F2);
            m_configManager->setDefault(config::SettingMenuKeyUp, 0);
            m_configManager->setDefault(config::SettingScreenshotKey, VK_F12);
            m_configManager->setDefault(config::SettingMenuEyeVisibility, XR_EYE_VISIBILITY_BOTH); // Both
            m_configManager->setDefault(config::SettingMenuDistance, 100);                         // 1m
            m_configManager->setDefault(config::SettingMenuOpacity, 85);
            m_configManager->setDefault(config::SettingMenuFontSize, 44); // pt
            m_configManager->setEnumDefault(config::SettingMenuTimeout, config::MenuTimeout::Medium);
            m_configManager->setDefault(config::SettingMenuExpert, m_configManager->getValue(config::SettingDeveloper));
            m_configManager->setEnumDefault(config::SettingOverlayType, config::OverlayType::None);
            // Legacy setting is 1/3rd from top and 2/3rd from left.
            {
                const auto ndcOffset = utilities::ScreenToNdc({2 / 3.f, 1 / 3.f});
                m_configManager->setDefault(config::SettingOverlayXOffset, static_cast<int>(ndcOffset.x * 100));
                m_configManager->setDefault(config::SettingOverlayYOffset, static_cast<int>(ndcOffset.y * 100));
            }

            // Hand tracking feature.
            m_configManager->setEnumDefault(config::SettingHandTrackingEnabled, config::HandTrackingEnabled::Off);
            m_configManager->setDefault(config::SettingBypassMsftHandInteractionCheck, 0);
            m_configManager->setDefault(config::SettingHandVisibilityAndSkinTone, 2); // Visible - Medium
            m_configManager->setDefault(config::SettingHandTimeout, 1);

            // Eye tracking feature.
            m_configManager->setDefault(config::SettingEyeTrackingEnabled, 0);
            m_configManager->setDefault(config::SettingBypassMsftEyeGazeInteractionCheck, 0);
            m_configManager->setDefault(config::SettingEyeDebugWithController, 0);
            m_configManager->setDefault(config::SettingEyeProjectionDistance, 200); // 2m
            m_configManager->setDefault(config::SettingEyeDebug, 0);

            // Upscaling feature.
            m_configManager->setEnumDefault(config::SettingScalingType, config::ScalingType::None);
            m_configManager->setDefault(config::SettingScaling, 100);
            m_configManager->setDefault(config::SettingAnamorphic, -100);
            m_configManager->setDefault(config::SettingSharpness, 20);
            // We default mip-map biasing to Off with OpenComposite since it's causing issues with certain apps. Users
            // have the (Expert) option to turn it back on.
            m_configManager->setEnumDefault(config::SettingMipMapBias,
                                            m_isOpenComposite ? config::MipMapBias::Off
                                                              : config::MipMapBias::Anisotropic);

            // Foveated rendering.
            m_configManager->setEnumDefault(config::SettingVRS, config::VariableShadingRateType::None);
            m_configManager->setEnumDefault(config::SettingVRSQuality, config::VariableShadingRateQuality::Performance);
            m_configManager->setEnumDefault(config::SettingVRSPattern, config::VariableShadingRatePattern::Wide);
            m_configManager->setEnumDefault(config::SettingVRSShowRings, config::NoYesType::No);
            m_configManager->setDefault(config::SettingVRSInner, 0); // 1x
            m_configManager->setDefault(config::SettingVRSInnerRadius, 55);
            m_configManager->setDefault(config::SettingVRSMiddle, 2); // 1/4x
            m_configManager->setDefault(config::SettingVRSOuter, 4);  // 1/16x
            m_configManager->setDefault(config::SettingVRSOuterRadius, 80);
            m_configManager->setDefault(config::SettingVRSXOffset, 0);
            m_configManager->setDefault(config::SettingVRSXScale, 125);
            m_configManager->setDefault(config::SettingVRSYOffset, 0);
            m_configManager->setDefault(config::SettingVRSPreferHorizontal, 0);
            m_configManager->setDefault(config::SettingVRSLeftRightBias, 0);

            // Appearance.
            m_configManager->setDefault(config::SettingPostProcess, 0);
            m_configManager->setDefault(config::SettingPostSunGlasses, 0);
            m_configManager->setDefault(config::SettingPostContrast, 500);
            m_configManager->setDefault(config::SettingPostBrightness, 500);
            m_configManager->setDefault(config::SettingPostExposure, 500);
            m_configManager->setDefault(config::SettingPostSaturation, 500);
            m_configManager->setDefault(config::SettingPostColorGainR, 500);
            m_configManager->setDefault(config::SettingPostColorGainG, 500);
            m_configManager->setDefault(config::SettingPostColorGainB, 500);
            m_configManager->setDefault(config::SettingPostVibrance, 0);
            m_configManager->setDefault(config::SettingPostHighlights, 1000);
            m_configManager->setDefault(config::SettingPostShadows, 0);

            // TODO: Appearance (User)
#if 0
            m_configManager->setDefault(config::SettingPostContrast + "_u1", 500);
            m_configManager->setDefault(config::SettingPostBrightness + "_u1", 500);
            m_configManager->setDefault(config::SettingPostExposure + "_u1", 500);
            m_configManager->setDefault(config::SettingPostSaturation + "_u1", 500);
            m_configManager->setDefault(config::SettingPostColorGainR + "_u1", 500);
            m_configManager->setDefault(config::SettingPostColorGainG + "_u1", 500);
            m_configManager->setDefault(config::SettingPostColorGainB + "_u1", 500);
            m_configManager->setDefault(config::SettingPostVibrance + "_u1", 0);
            m_configManager->setDefault(config::SettingPostHighlights + "_u1", 1000);
            m_configManager->setDefault(config::SettingPostShadows + "_u1", 0);
#endif
            // Misc features.
            m_configManager->setEnumDefault(config::SettingFOVType, config::FovModeType::Simple);
            m_configManager->setDefault(config::SettingFOV, 100);
            m_configManager->setDefault(config::SettingFOVUp, 100);
            m_configManager->setDefault(config::SettingFOVDown, 100);
            m_configManager->setDefault(config::SettingFOVLeftLeft, 100);
            m_configManager->setDefault(config::SettingFOVLeftRight, 100);
            m_configManager->setDefault(config::SettingFOVRightLeft, 100);
            m_configManager->setDefault(config::SettingFOVRightRight, 100);
            m_configManager->setDefault(config::SettingPimaxFOVHack, 0);
            m_configManager->setDefault(config::SettingICD, 1000);
            m_configManager->setDefault(config::SettingZoom, 10);
            m_configManager->setDefault(config::SettingPredictionDampen, 100);
            m_configManager->setDefault(config::SettingResolutionOverride, 0);
            m_configManager->setEnumDefault(config::SettingMotionReprojection, config::MotionReprojection::Default);
            m_configManager->setEnumDefault(config::SettingMotionReprojectionRate, config::MotionReprojectionRate::Off);
            m_configManager->setEnumDefault(config::SettingScreenshotFileFormat, config::ScreenshotFileFormat::PNG);
            m_configManager->setDefault(config::SettingScreenshotEye, 0); // Both

            // Misc debug.
            m_configManager->setDefault("debug_layer",
#ifdef _DEBUG
                                        1
#else
                                        0
#endif
            );
            // We disable the API interceptor with certain games where it seems to cause issues. As a result, foveated
            // rendering will not be offered.
            m_configManager->setDefault(
                "disable_interceptor",
                (m_applicationName == "OpenComposite_AC2-Win64-Shipping" || m_applicationName == "OpenComposite_Il-2"));
            // We disable the frame analyzer when using OpenComposite, because the app does not see the OpenXR
            // textures anyways.
            m_configManager->setDefault("disable_frame_analyzer", m_isOpenComposite);
            m_configManager->setDefault("canting", 0);
            m_configManager->setDefault("vrs_capture", 0);

            // Workaround: the first versions of the toolkit used a different representation for the world scale.
            // Migrate the value upon first run.
            m_configManager->setDefault("icd", 0);
            if (auto icdValue = m_configManager->getValue("icd")) {
                m_configManager->setValue(config::SettingICD, 1'000'000 / icdValue, true);
                m_configManager->deleteValue("icd");
            }

            // Commit any update above. This is needed for apps that create an instance, destroy it right away
            // without submitting a frame, then create a new one.
            m_configManager->tick();
        }

        XrResult xrCreateInstance(const XrInstanceCreateInfo* createInfo) override {
            // Needed to resolve the requested function pointers.
            OpenXrApi::xrCreateInstance(createInfo);

            m_applicationName = createInfo->applicationInfo.applicationName;
            m_isOpenComposite = contains_string(m_applicationName, "OpenComposite_");

            Log("Application name: '%s', Engine name: '%s'%s\n",
                createInfo->applicationInfo.applicationName,
                createInfo->applicationInfo.engineName,
                m_isOpenComposite ? "\nDetected OpenComposite" : "");

            // Dump the OpenXR runtime information to help debugging customer issues.
            auto instanceProperties = XrInstanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
            CHECK_XRCMD(xrGetInstanceProperties(GetXrInstance(), &instanceProperties));
            m_runtimeName = fmt::format("{} {}.{}.{}",
                                        instanceProperties.runtimeName,
                                        XR_VERSION_MAJOR(instanceProperties.runtimeVersion),
                                        XR_VERSION_MINOR(instanceProperties.runtimeVersion),
                                        XR_VERSION_PATCH(instanceProperties.runtimeVersion));
            Log("Using OpenXR runtime %s\n", m_runtimeName.c_str());

            TraceLoggingWrite(g_traceProvider,
                              "xrCreateInstance",
                              TLArg(m_applicationName.c_str(), "Application"),
                              TLArg(createInfo->applicationInfo.engineName, "Engine"),
                              TLArg(m_runtimeName.c_str(), "Runtime"));

            // TODO: This should be auto-generated in the call above, but today our generator only looks at core spec.
            // We may let this fail intentionally and check that the pointer is populated later.
            // Workaround: the implementation of this function on the Varjo runtime seems to be using a time base
            // different than the timings returned by xrWaitFrame(). Do not use it.
            if (contains_string(m_runtimeName, "Varjo")) {
                xrGetInstanceProcAddr(
                    GetXrInstance(),
                    "xrConvertWin32PerformanceCounterToTimeKHR",
                    reinterpret_cast<PFN_xrVoidFunction*>(&xrConvertWin32PerformanceCounterToTimeKHR));
            }

            m_configManager = config::CreateConfigManager(createInfo->applicationInfo.applicationName);
            setOptionsDefaults();

            // Hook to enable Direct3D Debug layer on request.
            if (m_configManager->getValue("debug_layer")) {
                graphics::HookForD3D11DebugLayer();
                graphics::EnableD3D12DebugLayer();
            }

            // Check what keys to use.
            if (m_configManager->getValue(config::SettingKeyCtrlModifier)) {
                m_keyModifiers.push_back(VK_CONTROL);
            }
            if (m_configManager->getValue(config::SettingKeyAltModifier)) {
                m_keyModifiers.push_back(VK_MENU);
            }
            m_keyScreenshot = m_configManager->getValue(config::SettingScreenshotKey);

            // We must initialize hand and eye tracking early on, because the application can start creating actions etc
            // before creating the session.
            if (m_configManager->getEnumValue<config::HandTrackingEnabled>(config::SettingHandTrackingEnabled) !=
                config::HandTrackingEnabled::Off) {
                m_handTracker = input::CreateHandTracker(*this, m_configManager);
                m_sendInterationProfileEvent = true;
            }

            // TODO: If Foveated Rendering is disabled, maybe do not initialize the eye tracker?
            if (m_configManager->getValue(config::SettingEyeTrackingEnabled)) {
                m_eyeTracker = input::CreateEyeTracker(*this, m_configManager, input::EyeTrackerType::Any);
            }

            return XR_SUCCESS;
        }

        XrResult xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function) override {
            // TODO: This should be auto-generated by the dispatch layer, but today our generator only looks at core
            // spec. We may let this fail intentionally and check that the pointer is populated later.
            if (std::string_view(name) != "xrGetVisibilityMaskKHR")
                return OpenXrApi::xrGetInstanceProcAddr(instance, name, function);

            auto result = m_xrGetInstanceProcAddr(instance, name, function);
            m_xrGetVisibilityMaskKHR = reinterpret_cast<PFN_xrGetVisibilityMaskKHR>(*function);
            *function = reinterpret_cast<PFN_xrVoidFunction>(_xrGetVisibilityMaskKHR);
            return result;
        }

        XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId) override {
            const XrResult result = OpenXrApi::xrGetSystem(instance, getInfo, systemId);
            if (XR_SUCCEEDED(result) && m_vrSystemId == XR_NULL_SYSTEM_ID &&
                getInfo->formFactor == XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY) {
                const bool isDeveloper = m_configManager->getValue(config::SettingDeveloper);

                // Retrieve the actual OpenXR resolution.
                XrViewConfigurationView views[utilities::ViewCount] = {{XR_TYPE_VIEW_CONFIGURATION_VIEW, nullptr},
                                                                       {XR_TYPE_VIEW_CONFIGURATION_VIEW, nullptr}};

                uint32_t viewCount = utilities::ViewCount;
                CHECK_XRCMD(OpenXrApi::xrEnumerateViewConfigurationViews(
                    instance, *systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount, views));

                // Check for hand and eye tracking support.
                auto systemProperties = XrSystemProperties{XR_TYPE_SYSTEM_PROPERTIES};
                auto handTrackingProps = XrSystemHandTrackingPropertiesEXT{XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT};
                auto eyeTrackingProps =
                    XrSystemEyeGazeInteractionPropertiesEXT{XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT};

                xr::InsertExtensionStruct(systemProperties, handTrackingProps);
                xr::InsertExtensionStruct(systemProperties, eyeTrackingProps);

                CHECK_XRCMD(OpenXrApi::xrGetSystemProperties(instance, *systemId, &systemProperties));

                TraceLoggingWrite(g_traceProvider,
                                  "xrGetSystem",
                                  TLArg(systemProperties.systemName, "System"),
                                  TLArg(views[0].recommendedImageRectWidth, "RecommendedResolutionX"),
                                  TLArg(views[0].recommendedImageRectHeight, "RecommendedResolutionY"),
                                  TLArg(handTrackingProps.supportsHandTracking, "SupportsHandTracking"),
                                  TLArg(eyeTrackingProps.supportsEyeGazeInteraction, "SupportsEyeGazeInteraction"));

                // assert(viewCount);

                m_vrSystemId = *systemId; // Remember the XrSystemId to use.
                m_systemName = systemProperties.systemName;

                Log("Using OpenXR system %s\n", m_systemName.c_str());

                m_displayWidth = std::max(views[0].recommendedImageRectWidth, 1u);
                m_displayHeight = std::max(views[0].recommendedImageRectHeight, 1u);
                m_resolutionHeightRatio = static_cast<float>(m_displayHeight) / m_displayWidth;
                m_maxDisplayWidth =
                    std::min(views[0].maxImageRectWidth,
                             static_cast<uint32_t>(views[0].maxImageRectHeight / m_resolutionHeightRatio));

                // Apply override to the target resolution.
                m_configManager->setDefault(config::SettingResolutionWidth, m_displayWidth);
                if (m_configManager->getValue(config::SettingResolutionOverride)) {
                    m_displayWidth = m_configManager->getValue(config::SettingResolutionWidth);
                    m_displayHeight = (uint32_t)(m_displayWidth * m_resolutionHeightRatio);

                    Log("Overriding OpenXR resolution: %ux%u\n", m_displayWidth, m_displayHeight);
                }

                // Detect when the Pimax FOV hack is applicable.

                const auto isRuntimeWMR = contains_string(m_runtimeName, "Windows Mixed Reality Runtime");
                const auto isSystemPimax = contains_string(m_systemName, "aapvr");

                m_supportMotionReprojectionLock = isRuntimeWMR;
                m_supportFOVHack = isDeveloper || (m_applicationName == "FS2020" && isSystemPimax);

                m_supportHandTracking = handTrackingProps.supportsHandTracking;
                m_supportEyeTracking = eyeTrackingProps.supportsEyeGazeInteraction ||
                                       (m_eyeTracker && !m_eyeTracker->isTrackingThroughRuntime()) ||
                                       m_configManager->getValue(config::SettingEyeDebugWithController);

                // Workaround: the WMR runtime supports mapping the VR controllers through XR_EXT_hand_tracking, which
                // will (falsely) advertise hand tracking support. Check for the Ultraleap layer in this case.
                if (m_supportHandTracking && isRuntimeWMR && !isDeveloper) {
                    if (!m_configManager->getValue(config::SettingBypassMsftHandInteractionCheck)) {
                        if (!contains(GetUpstreamLayers(), "XR_APILAYER_ULTRALEAP_hand_tracking")) {
                            Log("Ignoring XR_MSFT_hand_interaction for %s\n", m_runtimeName.c_str());
                            m_supportHandTracking = false;
                        }
                    }
                }

                // Workaround: the WMR runtime supports emulating eye tracking for development through
                // XR_EXT_eye_gaze_interaction, which will (falsely) advertise eye tracking support. Disable it.
                if (m_supportEyeTracking && isRuntimeWMR && !isDeveloper) {
                    if (!m_configManager->getValue(config::SettingBypassMsftEyeGazeInteractionCheck)) {
                        if (m_eyeTracker && m_eyeTracker->isTrackingThroughRuntime()) {
                            Log("Ignoring XR_EXT_eye_gaze_interaction for %s\n", m_runtimeName.c_str());
                            m_supportEyeTracking = false;
                        }
                    }
                }

                // We had to initialize the hand and eye trackers early on. If we find out now that they are not
                // supported, then destroy them. This could happen if the option was set while a hand tracking device
                // was connected, but later the hand tracking device was disconnected.
                if (!m_supportHandTracking)
                    m_handTracker.reset();

                if (!m_supportEyeTracking)
                    m_eyeTracker.reset();
            }

            return result;
        }

        XrResult xrEnumerateViewConfigurationViews(XrInstance instance,
                                                   XrSystemId systemId,
                                                   XrViewConfigurationType viewConfigurationType,
                                                   uint32_t viewCapacityInput,
                                                   uint32_t* viewCountOutput,
                                                   XrViewConfigurationView* views) override {
            const XrResult result = OpenXrApi::xrEnumerateViewConfigurationViews(
                instance, systemId, viewConfigurationType, viewCapacityInput, viewCountOutput, views);

            if (XR_SUCCEEDED(result) && isVrSystem(systemId) && views) {
                using namespace toolkit::config;

                // Determine the application resolution.
                auto inputWidth = m_displayWidth;
                auto inputHeight = m_displayHeight;

                if (m_configManager->getEnumValue<ScalingType>(SettingScalingType) != ScalingType::None) {
                    std::tie(inputWidth, inputHeight) =
                        GetScaledDimensions(m_configManager.get(), m_displayWidth, m_displayHeight, 2);
                }

                // Override the recommended image size to account for scaling.
                std::for_each_n(views, *viewCountOutput, [inputWidth, inputHeight](auto& view) {
                    view.recommendedImageRectWidth = inputWidth;
                    view.recommendedImageRectHeight = inputHeight;
                });

                if (inputWidth != m_displayWidth || inputHeight != m_displayHeight) {
                    Log("Upscaling from %ux%u to %ux%u (%u%%)\n",
                        inputWidth,
                        inputHeight,
                        m_displayWidth,
                        m_displayHeight,
                        std::max(m_displayWidth * 100u / inputWidth, 1u));
                } else {
                    Log("Using OpenXR resolution (no upscaling): %ux%u\n", m_displayWidth, m_displayHeight);
                }

                TraceLoggingWrite(g_traceProvider,
                                  "xrEnumerateViewConfigurationViews",
                                  TLArg(inputWidth, "AppResolutionX"),
                                  TLArg(inputHeight, "AppResolutionY"),
                                  TLArg(m_displayWidth, "SystemResolutionX"),
                                  TLArg(m_displayHeight, "SystemResolutionY"));
            }

            return result;
        }

        XrResult xrCreateSession(XrInstance instance,
                                 const XrSessionCreateInfo* createInfo,
                                 XrSession* session) override {
            // Force motion reprojection if requested.
            if (m_supportMotionReprojectionLock) {
                utilities::ToggleWindowsMixedRealityReprojection(
                    m_configManager->getEnumValue<config::MotionReprojection>(config::SettingMotionReprojection));
            }

            const XrResult result = OpenXrApi::xrCreateSession(instance, createInfo, session);
            if (XR_SUCCEEDED(result) && isVrSystem(createInfo->systemId)) {
                // Get the graphics device.
                for (auto it = reinterpret_cast<const XrBaseInStructure*>(createInfo->next); it; it = it->next) {
                    if (it->type == XR_TYPE_GRAPHICS_BINDING_D3D11_KHR) {
                        // Workaround: Oculus OpenXR DX11 Runtime seems to hook some D3D calls and breaks our Detours.
                        const auto delayHook = contains_string(m_runtimeName, "Oculus");
                        auto graphicsBinding = reinterpret_cast<const XrGraphicsBindingD3D11KHR*>(it);
                        m_graphicsDevice =
                            graphics::WrapD3D11Device(graphicsBinding->device, m_configManager, delayHook);
                        break;
                    }
                    if (it->type == XR_TYPE_GRAPHICS_BINDING_D3D12_KHR) {
                        auto graphicsBinding = reinterpret_cast<const XrGraphicsBindingD3D12KHR*>(it);
                        m_graphicsDevice =
                            graphics::WrapD3D12Device(graphicsBinding->device, graphicsBinding->queue, m_configManager);
                        break;
                    }
                }

                if (m_graphicsDevice) {
                    using namespace toolkit::config;

                    // Initialize the other resources.
                    uint32_t renderWidth = m_displayWidth;
                    uint32_t renderHeight = m_displayHeight;

                    m_upscaleMode = m_configManager->getEnumValue<ScalingType>(SettingScalingType);

                    if (m_upscaleMode != ScalingType::None) {
                        std::tie(renderWidth, renderHeight) =
                            GetScaledDimensions(m_configManager.get(), m_displayWidth, m_displayHeight, 2);

                        if (m_upscaleMode == ScalingType::FSR) {
                            m_imageProcessors[ImgProc::Scale] = graphics::CreateFSRUpscaler(m_configManager,
                                                                                            m_graphicsDevice,
                                                                                            renderWidth,
                                                                                            renderHeight,
                                                                                            m_displayWidth,
                                                                                            m_displayHeight);
                        }
                        if (m_upscaleMode == ScalingType::NIS) {
                            m_imageProcessors[ImgProc::Scale] = graphics::CreateNISUpscaler(m_configManager,
                                                                                            m_graphicsDevice,
                                                                                            renderWidth,
                                                                                            renderHeight,
                                                                                            m_displayWidth,
                                                                                            m_displayHeight);
                        }

                        // Per FSR SDK documentation.
                        m_mipMapBiasForUpscaling = -std::log2f(static_cast<float>(m_displayWidth * m_displayHeight) /
                                                               (renderWidth * renderHeight));

                        Log("MipMap biasing for upscaling is: %.3f\n", m_mipMapBiasForUpscaling);
                    }

                    if (m_graphicsDevice->isEventsSupported()) {
                        if (!m_configManager->getValue("disable_frame_analyzer")) {
                            m_frameAnalyzer = graphics::CreateFrameAnalyzer(m_configManager, m_graphicsDevice);
                        }

                        m_variableRateShader = graphics::CreateVariableRateShader(m_configManager,
                                                                                  m_graphicsDevice,
                                                                                  m_eyeTracker,
                                                                                  renderWidth,
                                                                                  renderHeight,
                                                                                  m_displayWidth,
                                                                                  m_displayHeight,
                                                                                  m_supportFOVHack);

                        // Register intercepted events.
                        m_graphicsDevice->registerSetRenderTargetEvent(
                            [&](std::shared_ptr<graphics::IContext> context,
                                std::shared_ptr<graphics::ITexture> renderTarget) {
                                if (m_isInFrame) {
                                    auto eyeHint = utilities::Eye::Both;
                                    if (m_frameAnalyzer) {
                                        m_frameAnalyzer->onSetRenderTarget(context, renderTarget);
                                        eyeHint = m_frameAnalyzer->getEyeHint();
                                        m_stats.hasColorBuffer[to_integral(eyeHint)] = true;
                                    }
                                    if (m_variableRateShader) {
                                        if (m_variableRateShader->onSetRenderTarget(context, renderTarget, eyeHint))
                                            m_stats.numRenderTargetsWithVRS++;
                                    }
                                }
                            });

                        m_graphicsDevice->registerUnsetRenderTargetEvent(
                            [&](std::shared_ptr<graphics::IContext> context) {
                                if (m_isInFrame) {
                                    if (m_frameAnalyzer)
                                        m_frameAnalyzer->onUnsetRenderTarget(context);
                                    if (m_variableRateShader)
                                        m_variableRateShader->onUnsetRenderTarget(context);
                                }
                            });

                        m_graphicsDevice->registerCopyTextureEvent([&](std::shared_ptr<graphics::IContext>,
                                                                       std::shared_ptr<graphics::ITexture> src,
                                                                       std::shared_ptr<graphics::ITexture> dst,
                                                                       int srcSlice,
                                                                       int dstSlice) {
                            if (m_isInFrame) {
                                if (m_frameAnalyzer)
                                    m_frameAnalyzer->onCopyTexture(src, dst, srcSlice, dstSlice);
                            }
                        });
                    }

                    m_imageProcessors[ImgProc::Post] =
                        graphics::CreateImageProcessor(m_configManager, m_graphicsDevice, m_variableRateShader);

                    m_performanceCounters.createGpuTimers(m_graphicsDevice.get());
                    m_performanceCounters.updateTimer.start();

                    // Create the Menu swapchain images
                    {
                        uint32_t formatCount = 0;
                        CHECK_XRCMD(xrEnumerateSwapchainFormats(*session, 0, &formatCount, nullptr));
                        std::vector<int64_t> formats(formatCount);
                        CHECK_XRCMD(xrEnumerateSwapchainFormats(*session, formatCount, &formatCount, formats.data()));
                        // assert(!formats.empty()); // unlikely

                        auto swapchainInfo = XrSwapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
                        swapchainInfo.width = 2048; // Let's hope the menu doesn't get bigger than that.
                        swapchainInfo.height = 2048;
                        swapchainInfo.arraySize = 1;
                        swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
                        swapchainInfo.format = formats[0];
                        swapchainInfo.sampleCount = 1;
                        swapchainInfo.faceCount = 1;
                        swapchainInfo.mipCount = 1;
                        CHECK_XRCMD(OpenXrApi::xrCreateSwapchain(*session, &swapchainInfo, &m_menuSwapchain));

                        m_menuSwapchainImages = graphics::WrapXrSwapchainImages(
                            m_graphicsDevice, swapchainInfo, m_menuSwapchain, "Menu swapchain {} TEX2D");
                    }

                    // Create the Menu handler.
                    {
                        menu::MenuInfo menuInfo;
                        menuInfo.displayWidth = m_displayWidth;
                        menuInfo.displayHeight = m_displayHeight;
                        menuInfo.keyModifiers = m_keyModifiers;
                        menuInfo.isHandTrackingSupported = m_supportHandTracking;
                        menuInfo.isPredictionDampeningSupported = xrConvertWin32PerformanceCounterToTimeKHR != nullptr;
                        menuInfo.maxDisplayWidth = m_maxDisplayWidth;
                        menuInfo.resolutionHeightRatio = m_resolutionHeightRatio;
                        menuInfo.isMotionReprojectionRateSupported = m_supportMotionReprojectionLock;
                        menuInfo.displayRefreshRate =
                            utilities::RegGetDword(
                                HKEY_LOCAL_MACHINE,
                                L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Holographic\\DisplayThrottling",
                                L"ThrottleFramerate")
                                    .value_or(0)
                                ? 60u
                                : 90u;
                        menuInfo.variableRateShaderMaxRate =
                            m_variableRateShader ? m_variableRateShader->getMaxRate() : 0;
                        menuInfo.isEyeTrackingSupported = m_supportEyeTracking;
                        menuInfo.isEyeTrackingProjectionDistanceSupported =
                            m_eyeTracker ? m_eyeTracker->isProjectionDistanceSupported() : false;
                        menuInfo.isPimaxFovHackSupported = m_supportFOVHack;

                        m_menuHandler = menu::CreateMenuHandler(m_configManager, m_graphicsDevice, menuInfo);
                    }

                    // Create a reference space to calculate projection views.
                    {
                        const auto referenceSpaceCreateInfo =
                            XrReferenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
                                                       nullptr,
                                                       XR_REFERENCE_SPACE_TYPE_VIEW,
                                                       Pose::Identity()};
                        CHECK_XRCMD(xrCreateReferenceSpace(*session, &referenceSpaceCreateInfo, &m_viewSpace));
                    }

                    if (m_handTracker) {
                        m_handTracker->beginSession(*session, m_graphicsDevice);
                    }
                    if (m_eyeTracker) {
                        m_eyeTracker->beginSession(*session);
                    }

                    // Make sure we perform calibration again. We pass these values to the menu and FFR, so in the case
                    // of multi-session applications, we must push those values again.
                    m_needCalibrateEyeProjections = true;

                    // Remember the XrSession to use.
                    m_vrSession = *session;
                } else {
                    Log("Unsupported graphics runtime.\n");
                }
            }

            return result;
        }

        XrResult xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo) override {
            const XrResult result = OpenXrApi::xrBeginSession(session, beginInfo);
            if (XR_SUCCEEDED(result) && isVrSession(session)) {
                m_configManager->setActiveSession(m_applicationName);
            }

            return result;
        }

        XrResult xrEndSession(XrSession session) override {
            const XrResult result = OpenXrApi::xrEndSession(session);
            if (XR_SUCCEEDED(result) && isVrSession(session)) {
                m_configManager->setActiveSession("");
            }

            return result;
        }

        XrResult xrDestroySession(XrSession session) override {
            const XrResult result = OpenXrApi::xrDestroySession(session);
            if (XR_SUCCEEDED(result) && isVrSession(session)) {
                // We cleanup after ourselves as soon as possible to avoid leaving state registry entries.
                utilities::ClearWindowsMixedRealityReprojection();

                if (m_configManager) {
                    m_configManager->setActiveSession("");
                }

                // Wait for any pending operation to complete.
                if (m_graphicsDevice) {
                    m_graphicsDevice->blockCallbacks();
                    m_graphicsDevice->flushContext(true);
                }

                if (m_viewSpace != XR_NULL_HANDLE) {
                    xrDestroySpace(m_viewSpace);
                    m_viewSpace = XR_NULL_HANDLE;
                }

                // Destroy session instances in reverse order of their dependencies.
                m_imageProcessors.fill(nullptr);
                m_variableRateShader.reset();
                m_frameAnalyzer.reset();

                // End session of these global instances but don't destroy.
                if (m_handTracker)
                    m_handTracker->endSession();

                if (m_eyeTracker)
                    m_eyeTracker->endSession();

                m_performanceCounters.destroyGpuTimers();

                m_swapchains.clear();
                m_menuSwapchainImages.clear();
                if (m_menuSwapchain != XR_NULL_HANDLE) {
                    xrDestroySwapchain(m_menuSwapchain);
                    m_menuSwapchain = XR_NULL_HANDLE;
                }
                m_menuHandler.reset();
                if (m_graphicsDevice) {
                    m_graphicsDevice->shutdown();
                }
                m_graphicsDevice.reset();
                m_vrSession = XR_NULL_HANDLE;
                // A good check to ensure there are no resources leak is to confirm that the graphics device is
                // destroyed _before_ we see this message.
                // eg:
                // 2022-01-01 17:15:35 -0800: D3D11Device destroyed
                // 2022-01-01 17:15:35 -0800: Session destroyed
                // If the order is reversed or the Device is destructed missing, then it means that we are not cleaning
                // up the resources properly.
                Log("Session destroyed\n");
            }

            return result;
        }

        XrResult xrCreateSwapchain(XrSession session,
                                   const XrSwapchainCreateInfo* createInfo,
                                   XrSwapchain* swapchain) override {
            if (!isVrSession(session) || !m_graphicsDevice) {
                return OpenXrApi::xrCreateSwapchain(session, createInfo, swapchain);
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrCreateSwapchain_AppSwapchain",
                              TLArg(createInfo->width, "ResolutionX"),
                              TLArg(createInfo->height, "ResolutionY"),
                              TLArg(createInfo->arraySize, "ArraySize"),
                              TLArg(createInfo->mipCount, "MipCount"),
                              TLArg(createInfo->sampleCount, "SampleCount"),
                              TLArg(createInfo->format, "Format"),
                              TLArg(createInfo->usageFlags, "Usage"));
            Log("Creating swapchain(app) with dimensions=%ux%u, arraySize=%u, mipCount=%u, sampleCount=%u, format=%d, "
                "usage=0x%x\n",
                createInfo->width,
                createInfo->height,
                createInfo->arraySize,
                createInfo->mipCount,
                createInfo->sampleCount,
                createInfo->format,
                createInfo->usageFlags);

            // Modify the swapchain to handle our processing chain (eg: change resolution and/or select usage
            // XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT).

            auto chainCreateInfo = *createInfo;

            const auto validUsageFlags =
                chainCreateInfo.usageFlags &
                (XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                 XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT);

            // Identify the swapchains of interest for our processing chain.
            // We do no processing to depth buffers.
            if (validUsageFlags && !(validUsageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
                if (m_imageProcessors[ImgProc::Pre]) {
                    chainCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
                }
                if (m_imageProcessors[ImgProc::Scale]) {
                    // The upscaler requires to use as an unordered access view.
                    chainCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;
                    // When upscaling, be sure to request the full resolution with the runtime.
                    chainCreateInfo.width = std::max(m_displayWidth, chainCreateInfo.width);
                    chainCreateInfo.height = std::max(m_displayHeight, chainCreateInfo.height);
                }
                if (m_imageProcessors[ImgProc::Post]) {
                    // These are not needed for the runtime swapchain: we're using an intermediate texture.
                    chainCreateInfo.usageFlags &= ~XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;
                    chainCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
                }
            }

            const auto result = OpenXrApi::xrCreateSwapchain(session, &chainCreateInfo, swapchain);

            if (XR_SUCCEEDED(result) && validUsageFlags) {
                SwapchainState swapchainState;
                {
                    auto chainImages = graphics::WrapXrSwapchainImages(
                        m_graphicsDevice, chainCreateInfo, *swapchain, "Runtime swapchain {} TEX2D");

                    // Store the runtime images into the state (last entry in the processing chain).
                    for (size_t i = 0; i < chainImages.size(); i++) {
                        SwapchainImages images;
                        images.chain.push_back(std::move(chainImages[i]));
                        swapchainState.images.push_back(std::move(images));
                    }
                }

                // Dump the descriptor for the first texture returned by the runtime for debug purposes.
                if (auto texture = swapchainState.images[0].chain[0]->getAs<graphics::D3D11>()) {
                    D3D11_TEXTURE2D_DESC desc;
                    texture->GetDesc(&desc);
                    TraceLoggingWrite(g_traceProvider,
                                      "xrCreateSwapchain_RuntimeSwapchain",
                                      TLArg(desc.Width, "ResolutionX"),
                                      TLArg(desc.Height, "ResolutionY"),
                                      TLArg(desc.ArraySize, "ArraySize"),
                                      TLArg(desc.MipLevels, "MipCount"),
                                      TLArg(desc.SampleDesc.Count, "SampleCount"),
                                      TLArg((int)desc.Format, "Format"),
                                      TLArg((int)desc.Usage, "Usage"),
                                      TLArg(desc.BindFlags, "BindFlags"),
                                      TLArg(desc.CPUAccessFlags, "CPUAccessFlags"),
                                      TLArg(desc.MiscFlags, "MiscFlags"));
                    DebugLog("Creating swapchain(own) with dimensions=%ux%u, arraySize=%u, mipCount=%u, sampleCount=%u, "
                        "format=%d, "
                        "usage=0x%x\n",
                        desc.Width,
                        desc.Height,
                        desc.ArraySize,
                        desc.MipLevels,
                        desc.SampleDesc.Count,
                        desc.Format,
                        desc.Usage);
                } else if (auto texture = swapchainState.images[0].chain[0]->getAs<graphics::D3D12>()) {
                    const auto& desc = texture->GetDesc();
                    TraceLoggingWrite(g_traceProvider,
                                      "xrCreateSwapchain_RuntimeSwapchain",
                                      TLArg(desc.Width, "ResolutionX"),
                                      TLArg(desc.Height, "ResolutionY"),
                                      TLArg(desc.DepthOrArraySize, "ArraySize"),
                                      TLArg(desc.MipLevels, "MipCount"),
                                      TLArg(desc.SampleDesc.Count, "SampleCount"),
                                      TLArg((int)desc.Format, "Format"),
                                      TLArg((int)desc.Flags, "Flags"));
                    DebugLog(
                        "Creating swapchain(own) with dimensions=%ux%u, arraySize=%u, mipCount=%u, sampleCount=%u, "
                        "format=%d, "
                        "flags=0x%x\n",
                        desc.Width,
                        desc.Height,
                        desc.DepthOrArraySize,
                        desc.MipLevels,
                        desc.SampleDesc.Count,
                        desc.Format,
                        desc.Flags);
                } else {
                    throw std::runtime_error("Unsupported graphics runtime"); // unlikely
                }

                // Make sure to create the underlying texture typeless.
                auto overrideFormat = swapchainState.images[0].chain[0]->getNativeFormat();

                for (size_t i = 0; i < swapchainState.images.size(); i++) {
                    // TODO: this is invariant, why testing this inside the loop?
                    if (createInfo->usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                        continue; // We do no processing to depth buffers.
                    }

                    // Create other entries in the chain based on the processing to do (scaling,
                    // post-processing...).
                    auto& chain = swapchainState.images[i].chain;

                    if (m_imageProcessors[ImgProc::Pre]) {
                        // Create an intermediate texture with the same resolution as the input.
                        auto imageCreateInfo = *createInfo;
                        imageCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

                        auto texture = m_graphicsDevice->createTexture(
                            imageCreateInfo, fmt::format("Pre swapchain {} TEX2D", i), overrideFormat);

                        // We place the texture at the very front (app texture).
                        chain.insert(chain.begin(), std::move(texture));
                    }

                    if (m_imageProcessors[ImgProc::Scale]) {
                        // Create an app texture with the lower resolution.
                        auto imageCreateInfo = *createInfo;
                        imageCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

                        auto texture = m_graphicsDevice->createTexture(
                            imageCreateInfo, fmt::format("App swapchain {} TEX2D", i), overrideFormat);

                        // We place the texture before the runtime texture, which means at the very
                        // front (app texture) or after the pre-processor.
                        chain.insert(chain.end() - 1, std::move(texture));
                    }

                    if (m_imageProcessors[ImgProc::Post]) {
                        // Create an intermediate texture with the same resolution as the output.
                        auto imageCreateInfo = chainCreateInfo;
                        imageCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

                        if (m_imageProcessors[ImgProc::Scale]) {
                            // Don't override. This isn't the texture the app is going to see anyway.
                            overrideFormat = 0;
                            // The upscaler requires to use as an unordered access view and non-sRGB type.
                            imageCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;
                            if (m_graphicsDevice->isTextureFormatSRGB(imageCreateInfo.format)) {
                                imageCreateInfo.format = m_graphicsDevice->getTextureFormat(
                                    graphics::TextureFormat::R10G10B10A2_UNORM); // good perf/visual balance
                            }
                        }
                        auto texture = m_graphicsDevice->createTexture(
                            imageCreateInfo, fmt::format("Pst swapchain {} TEX2D", i), overrideFormat);

                        // We place the texture just before the runtime texture.
                        chain.insert(chain.end() - 1, std::move(texture));
                    }

                    for (size_t j = 0; j < std::size(m_imageProcessors); j++) {
                        if (m_imageProcessors[j]) {
                            for (size_t k = 0; k < std::min(createInfo->arraySize, 2u); k++)
                                swapchainState.images[i].gpuTimers[j][k] = m_graphicsDevice->createTimer();
                        }
                    }
                }

                m_swapchains.insert_or_assign(*swapchain, swapchainState);
            }

            return result;
        }

        XrResult xrDestroySwapchain(XrSwapchain swapchain) override {
            const XrResult result = OpenXrApi::xrDestroySwapchain(swapchain);
            if (XR_SUCCEEDED(result)) {
                m_swapchains.erase(swapchain);
            }

            return result;
        }

        XrResult xrSuggestInteractionProfileBindings(
            XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings) override {
            if (m_configManager->getValue(config::SettingEyeDebugWithController)) {
                // We must drop calls to allow the controller override for debugging.
                if (suggestedBindings->countSuggestedBindings > 1 ||
                    getXrPath(suggestedBindings->interactionProfile) !=
                        "/interaction_profiles/hp/mixed_reality_controller") {
                    return XR_SUCCESS;
                }
            }

            const XrResult result = OpenXrApi::xrSuggestInteractionProfileBindings(instance, suggestedBindings);
            if (XR_SUCCEEDED(result) && m_handTracker) {
                m_handTracker->registerBindings(*suggestedBindings);
            }

            return result;
        }

        XrResult xrAttachSessionActionSets(XrSession session,
                                           const XrSessionActionSetsAttachInfo* attachInfo) override {
            XrSessionActionSetsAttachInfo chainAttachInfo = *attachInfo;
            std::vector<XrActionSet> newActionSets;
            if (m_eyeTracker) {
                const auto eyeTrackerActionSet = m_eyeTracker->getActionSet();
                if (eyeTrackerActionSet != XR_NULL_HANDLE) {
                    newActionSets.resize(chainAttachInfo.countActionSets + 1);
                    memcpy(newActionSets.data(),
                           chainAttachInfo.actionSets,
                           chainAttachInfo.countActionSets * sizeof(XrActionSet));
                    uint32_t nextActionSetSlot = chainAttachInfo.countActionSets;

                    newActionSets[nextActionSetSlot++] = eyeTrackerActionSet;

                    chainAttachInfo.actionSets = newActionSets.data();
                    chainAttachInfo.countActionSets++;
                }
            }

            return OpenXrApi::xrAttachSessionActionSets(session, &chainAttachInfo);
        }

        XrResult xrCreateAction(XrActionSet actionSet,
                                const XrActionCreateInfo* createInfo,
                                XrAction* action) override {
            const XrResult result = OpenXrApi::xrCreateAction(actionSet, createInfo, action);
            if (XR_SUCCEEDED(result)) {
                if (m_handTracker)
                    m_handTracker->registerAction(*action, actionSet);
            }

            return result;
        }

        XrResult xrDestroyAction(XrAction action) override {
            const XrResult result = OpenXrApi::xrDestroyAction(action);
            if (XR_SUCCEEDED(result)) {
                if (m_handTracker)
                    m_handTracker->unregisterAction(action);
            }

            return result;
        }

        XrResult xrCreateActionSpace(XrSession session,
                                     const XrActionSpaceCreateInfo* createInfo,
                                     XrSpace* space) override {
            const XrResult result = OpenXrApi::xrCreateActionSpace(session, createInfo, space);
            if (XR_SUCCEEDED(result) && isVrSession(session)) {
                if (m_handTracker) {
                    // Keep track of the XrSpace for controllers, so we can override the behavior for them.
                    const std::string fullPath =
                        m_handTracker->getFullPath(createInfo->action, createInfo->subactionPath);
                    if (fullPath == "/user/hand/right/input/grip/pose" ||
                        fullPath == "/user/hand/right/input/aim/pose" ||
                        fullPath == "/user/hand/left/input/grip/pose" || fullPath == "/user/hand/left/input/aim/pose") {
                        m_handTracker->registerActionSpace(*space, fullPath, createInfo->poseInActionSpace);
                    }
                }
            }

            return result;
        }

        XrResult xrDestroySpace(XrSpace space) override {
            const XrResult result = OpenXrApi::xrDestroySpace(space);
            if (XR_SUCCEEDED(result)) {
                if (m_handTracker)
                    m_handTracker->unregisterActionSpace(space);
            }
            return result;
        }

        XrResult xrEnumerateSwapchainImages(XrSwapchain swapchain,
                                            uint32_t imageCapacityInput,
                                            uint32_t* imageCountOutput,
                                            XrSwapchainImageBaseHeader* images) override {
            const XrResult result =
                OpenXrApi::xrEnumerateSwapchainImages(swapchain, imageCapacityInput, imageCountOutput, images);
            if (XR_SUCCEEDED(result) && images) {
                const auto swapchainIt = m_swapchains.find(swapchain);
                if (swapchainIt != m_swapchains.end()) {
                    assert(swapchainIt->second.images.size() >= *imageCountOutput);

                    // Return the application texture (first entry in the processing chain).
                    auto src = swapchainIt->second.images.cbegin();
                    auto cnt = *imageCountOutput;

                    if (images->type == XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
                        for (auto dst = reinterpret_cast<XrSwapchainImageD3D11KHR*>(images); cnt--; src++, dst++) {
                            dst->texture = src->chain[0]->getAs<graphics::D3D11>();
                        }
                    } else if (images->type == XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR) {
                        for (auto dst = reinterpret_cast<XrSwapchainImageD3D12KHR*>(images); cnt--; src++, dst++) {
                            dst->texture = src->chain[0]->getAs<graphics::D3D12>();
                        }
                    } else {
                        throw std::runtime_error("Unsupported graphics runtime");
                    }
                }
            }

            return result;
        }

        XrResult xrAcquireSwapchainImage(XrSwapchain swapchain,
                                         const XrSwapchainImageAcquireInfo* acquireInfo,
                                         uint32_t* index) override {
            // Perform the release now in case it was delayed. This could happen for a discarded frame.
            auto swapchainIt = m_swapchains.find(swapchain);
            if (swapchainIt != m_swapchains.end() && swapchainIt->second.delayedRelease) {
                swapchainIt->second.delayedRelease = false;
                const auto releaseInfo = XrSwapchainImageReleaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                CHECK_XRCMD(OpenXrApi::xrReleaseSwapchainImage(swapchain, &releaseInfo));
            }

            const XrResult result = OpenXrApi::xrAcquireSwapchainImage(swapchain, acquireInfo, index);

            // Record the index so we know which texture to use in xrEndFrame().
            if (XR_SUCCEEDED(result) && swapchainIt != m_swapchains.end()) {
                swapchainIt->second.acquiredImageIndex = *index;
            }

            return result;
        }

        XrResult xrReleaseSwapchainImage(XrSwapchain swapchain,
                                         const XrSwapchainImageReleaseInfo* releaseInfo) override {
            // Perform a delayed release: we still need to write to the swapchain in our xrEndFrame()!
            auto swapchainIt = m_swapchains.find(swapchain);
            if (swapchainIt != m_swapchains.end()) {
                swapchainIt->second.delayedRelease = true;
                return XR_SUCCESS;
            }

            return OpenXrApi::xrReleaseSwapchainImage(swapchain, releaseInfo);
        }

        XrResult xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData) override {
            if (m_vrSession != XR_NULL_HANDLE) {
                if (m_sendInterationProfileEvent) {
                    auto buffer = reinterpret_cast<XrEventDataInteractionProfileChanged*>(eventData);
                    buffer->type = XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED;
                    buffer->next = nullptr;
                    buffer->session = m_vrSession;
                    m_sendInterationProfileEvent = false;
                    return XR_SUCCESS;
                }
                if (m_visibilityMaskEventIndex != utilities::ViewCount) {
                    auto buffer = reinterpret_cast<XrEventDataVisibilityMaskChangedKHR*>(eventData);
                    buffer->type = XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR;
                    buffer->next = nullptr;
                    buffer->session = m_vrSession;
                    buffer->viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    buffer->viewIndex = m_visibilityMaskEventIndex++;
                    Log("Send XrEventDataVisibilityMaskChangedKHR event for view %u\n", buffer->viewIndex);
                    return XR_SUCCESS;
                }
            }

            return OpenXrApi::xrPollEvent(instance, eventData);
        }

        XrResult xrGetCurrentInteractionProfile(XrSession session,
                                                XrPath topLevelUserPath,
                                                XrInteractionProfileState* interactionProfile) override {
            if (isVrSession(session)) {
                if (m_handTracker) {
                    // Return our emulated interaction profile for the hands.
                    const auto path = getXrPath(topLevelUserPath);
                    if ((path.empty() || path == "/user/hand/left" || path == "/user/hand/right") &&
                        interactionProfile->type == XR_TYPE_INTERACTION_PROFILE_STATE) {
                        interactionProfile->interactionProfile = m_handTracker->getInteractionProfile();
                        return XR_SUCCESS;
                    }
                }
            }
            return OpenXrApi::xrGetCurrentInteractionProfile(session, topLevelUserPath, interactionProfile);
        }

        XrResult xrGetVisibilityMaskKHR(XrSession session,
                                        XrViewConfigurationType viewConfigurationType,
                                        uint32_t viewIndex,
                                        XrVisibilityMaskTypeKHR visibilityMaskType,
                                        XrVisibilityMaskKHR* visibilityMask) {
            // When doing the Pimax FOV hack, we swap left and right eyes.
            if (isVrSession(session)) {
                if (m_supportFOVHack && m_configManager->peekValue(config::SettingPimaxFOVHack))
                    viewIndex ^= 1;
            }

            return m_xrGetVisibilityMaskKHR(
                session, viewConfigurationType, viewIndex, visibilityMaskType, visibilityMask);
        }

        XrResult xrLocateViews(XrSession session,
                               const XrViewLocateInfo* viewLocateInfo,
                               XrViewState* viewState,
                               uint32_t viewCapacityInput,
                               uint32_t* viewCountOutput,
                               XrView* views) override {
            const XrResult result =
                OpenXrApi::xrLocateViews(session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views);

            if (XR_SUCCEEDED(result) && isVrSession(session) &&
                viewLocateInfo->viewConfigurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
                using namespace DirectX;

                // Calibrate the projection center for each eye.
                if (m_needCalibrateEyeProjections) {
                    // TODO: since we call our own xrLocateViews, is this still necessary to assert this?
                    assert(*viewCountOutput == utilities::ViewCount);
                    if (calibrateEyeProjection(session, *viewLocateInfo))
                        m_needCalibrateEyeProjections = false;
                }

                // Save the original views poses and orientations for xrEndFrame.
                m_posesForFrame[0].pose = views[0].pose;
                m_posesForFrame[1].pose = views[1].pose;

                // Measure the real ICD
                const auto pos = LoadXrVector3(views[0].pose.position);
                const auto vec = LoadXrVector3(views[1].pose.position) - pos;
                m_stats.icd = XMVectorGetX(XMVector3Length(vec));

                // Override the ICD if requested.
                const auto icdOverride = m_configManager->getValue(config::SettingICD);
                if (icdOverride != 1000) {
                    // L = L + (v/2) - (vec*icd)/2, R = L + (v/2) + (vec*icd)/2
                    const auto icd = 1000.f / std::max(icdOverride, 1);
                    StoreXrVector3(&views[1].pose.position, pos + (vec * ((1 + icd) * 0.5f)));
                    StoreXrVector3(&views[0].pose.position, pos + (vec * ((1 - icd) * 0.5f)));
                    m_stats.icd *= icd;
                }

                // Override the canting angle if requested.
                if (const auto cantingOverride = m_configManager->getValue("canting")) {
                    // rotate each views around the vertical axis of the requested reference space.
                    // ideally we would rotate in view space, but this feature is for dev/debug only.
                    static constexpr XMVECTORF32 kLocalYawAxis = {{{0, 1, 0, 0}}};
                    const auto semiAngle = static_cast<float>(cantingOverride * (M_PI / 360));

                    StoreXrQuaternion(&views[0].pose.orientation,
                                      XMQuaternionMultiply(LoadXrQuaternion(views[0].pose.orientation),
                                                           XMQuaternionRotationNormal(kLocalYawAxis, -semiAngle)));
                    StoreXrQuaternion(&views[1].pose.orientation,
                                      XMQuaternionMultiply(LoadXrQuaternion(views[1].pose.orientation),
                                                           XMQuaternionRotationNormal(kLocalYawAxis, +semiAngle)));
                }

                // Override the FOV if requested.
                auto fov_l = LoadXrFov(views[0].fov);
                auto fov_r = LoadXrFov(views[1].fov);

                if (m_configManager->getEnumValue<config::FovModeType>(config::SettingFOVType) ==
                    config::FovModeType::Simple) {
                    const auto scale_lr = m_configManager->getValue(config::SettingFOV);
                    if (scale_lr != 100) {
                        fov_l *= (scale_lr * 0.01f);
                        fov_r *= (scale_lr * 0.01f);
                    }
                } else {
                    // XrFovF layout is: L,R,U,D
                    const auto scale_l = XMINT4(m_configManager->getValue(config::SettingFOVLeftLeft),
                                                m_configManager->getValue(config::SettingFOVLeftRight),
                                                m_configManager->getValue(config::SettingFOVUp),
                                                m_configManager->getValue(config::SettingFOVDown));
                    const auto scale_r = XMINT4(m_configManager->getValue(config::SettingFOVRightLeft),
                                                m_configManager->getValue(config::SettingFOVRightRight),
                                                scale_l.z,
                                                scale_l.w);
                    fov_l *= (XMLoadSInt4(&scale_l) * 0.01f);
                    fov_r *= (XMLoadSInt4(&scale_r) * 0.01f);
                }

                // Save new Render FOV for xrEndFrame.
                StoreXrFov(&m_posesForFrame[0].fov, fov_l);
                StoreXrFov(&m_posesForFrame[1].fov, fov_r);

                // Apply zoom if requested.
                const auto zoom = m_configManager->getValue(config::SettingZoom);
                if (zoom != 10) {
                    fov_l *= (10.f / zoom);
                    fov_r *= (10.f / zoom);
                }

                // And return new Render FOV to the application.
                StoreXrFov(&views[0].fov, fov_l);
                StoreXrFov(&views[1].fov, fov_r);

                // When doing the Pimax FOV hack, we swap left and right eyes.
                if (m_supportFOVHack) {
                    if (m_configManager->hasChanged(config::SettingPimaxFOVHack))
                        m_visibilityMaskEventIndex = 0; // Send the necessary events to the app.

                    if (m_configManager->getValue(config::SettingPimaxFOVHack))
                        std::swap(views[0], views[1]);
                }

            }

            return result;
        }

        XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location) override {
            if (location->type == XR_TYPE_SPACE_LOCATION) {
                if (m_handTracker) {
                    m_performanceCounters.handTrackingTimer.start();
                    if (m_handTracker->locate(space, baseSpace, time, getXrTimeNow(), *location)) {
                        m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer.stop();
                        return XR_SUCCESS;
                    }
                }
            }

            return OpenXrApi::xrLocateSpace(space, baseSpace, time, location);
        }

        XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo) override {
            const XrResult result = OpenXrApi::xrSyncActions(session, syncInfo);
            if (XR_SUCCEEDED(result) && isVrSession(session)) {
                if (m_handTracker) {
                    m_performanceCounters.handTrackingTimer.start();
                    m_handTracker->sync(m_begunFrameTime, getXrTimeNow(), *syncInfo);
                    m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer.stop();
                }
            }

            return result;
        }

        XrResult xrGetActionStateBoolean(XrSession session,
                                         const XrActionStateGetInfo* getInfo,
                                         XrActionStateBoolean* state) override {
            if (isVrSession(session)) {
                assert(getInfo->type == XR_TYPE_ACTION_STATE_GET_INFO &&
                       state->type == XR_TYPE_ACTION_STATE_BOOLEAN); // implicit
                if (m_handTracker) {
                    m_performanceCounters.handTrackingTimer.start();
                    if (m_handTracker->getActionState(*getInfo, *state)) {
                        m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer.stop();
                        return XR_SUCCESS;
                    }
                }
            }

            return OpenXrApi::xrGetActionStateBoolean(session, getInfo, state);
        }

        XrResult xrGetActionStateFloat(XrSession session,
                                       const XrActionStateGetInfo* getInfo,
                                       XrActionStateFloat* state) override {
            if (isVrSession(session)) {
                assert(getInfo->type == XR_TYPE_ACTION_STATE_GET_INFO &&
                       state->type == XR_TYPE_ACTION_STATE_FLOAT); // implicit
                if (m_handTracker) {
                    m_performanceCounters.handTrackingTimer.start();
                    if (m_handTracker->getActionState(*getInfo, *state)) {
                        m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer.stop();
                        return XR_SUCCESS;
                    }
                }
            }

            return OpenXrApi::xrGetActionStateFloat(session, getInfo, state);
        }

        XrResult xrGetActionStatePose(XrSession session,
                                      const XrActionStateGetInfo* getInfo,
                                      XrActionStatePose* state) override {
            if (isVrSession(session)) {
                assert(getInfo->type == XR_TYPE_ACTION_STATE_GET_INFO &&
                       state->type == XR_TYPE_ACTION_STATE_POSE); // implicit
                if (m_handTracker) {
                    m_performanceCounters.handTrackingTimer.start();
                    const std::string fullPath = m_handTracker->getFullPath(getInfo->action, getInfo->subactionPath);
                    bool supportedPath = false;
                    input::Hand hand = input::Hand::Left;
                    if (fullPath == "/user/hand/left/input/grip/pose" || fullPath == "/user/hand/left/input/aim/pose") {
                        supportedPath = true;
                        hand = input::Hand::Left;
                    } else if (fullPath == "/user/hand/right/input/grip/pose" ||
                               fullPath == "/user/hand/right/input/aim/pose") {
                        supportedPath = true;
                        hand = input::Hand::Right;
                    }
                    if (supportedPath) {
                        state->isActive = m_handTracker->isTrackedRecently(hand);
                        m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer.stop();
                        return XR_SUCCESS;
                    }
                }
            }

            return OpenXrApi::xrGetActionStatePose(session, getInfo, state);
        }

        XrResult xrApplyHapticFeedback(XrSession session,
                                       const XrHapticActionInfo* hapticActionInfo,
                                       const XrHapticBaseHeader* hapticFeedback) override {
            if (isVrSession(session)) {
                assert(hapticActionInfo->type == XR_TYPE_HAPTIC_ACTION_INFO); // implicit
                if (m_handTracker &&
                    hapticFeedback->type == XR_TYPE_HAPTIC_VIBRATION) { // explicit (if expanded in the future)
                    m_performanceCounters.handTrackingTimer.start();
                    const std::string fullPath =
                        m_handTracker->getFullPath(hapticActionInfo->action, hapticActionInfo->subactionPath);
                    bool supportedPath = false;
                    input::Hand hand = input::Hand::Left;
                    if (fullPath == "/user/hand/left/output/haptic") {
                        supportedPath = true;
                        hand = input::Hand::Left;
                    } else if (fullPath == "/user/hand/right/output/haptic") {
                        supportedPath = true;
                        hand = input::Hand::Right;
                    }
                    if (supportedPath) {
                        auto haptics = reinterpret_cast<const XrHapticVibration*>(hapticFeedback);
                        m_handTracker->handleOutput(hand, haptics->frequency, haptics->duration);
                        m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer.stop();
                    }
                }
            }

            return OpenXrApi::xrApplyHapticFeedback(session, hapticActionInfo, hapticFeedback);
        }

        XrResult xrStopHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo) override {
            if (isVrSession(session)) {
                assert(hapticActionInfo->type == XR_TYPE_HAPTIC_ACTION_INFO); // implicit
                if (m_handTracker) {
                    m_performanceCounters.handTrackingTimer.start();
                    const std::string fullPath =
                        m_handTracker->getFullPath(hapticActionInfo->action, hapticActionInfo->subactionPath);
                    bool supportedPath = false;
                    input::Hand hand = input::Hand::Left;
                    if (fullPath == "/user/hand/left/output/haptic") {
                        supportedPath = true;
                        hand = input::Hand::Left;
                    } else if (fullPath == "/user/hand/right/output/haptic") {
                        supportedPath = true;
                        hand = input::Hand::Right;
                    }
                    if (supportedPath) {
                        m_handTracker->handleOutput(hand, NAN, 0);
                        m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer.stop();
                    }
                }
            }

            return OpenXrApi::xrStopHapticFeedback(session, hapticActionInfo);
        }

        XrResult xrWaitFrame(XrSession session,
                             const XrFrameWaitInfo* frameWaitInfo,
                             XrFrameState* frameState) override {
            if (isVrSession(session)) {
                m_performanceCounters.waitCpuTimer.start();
            }
            const XrResult result = OpenXrApi::xrWaitFrame(session, frameWaitInfo, frameState);
            if (XR_SUCCEEDED(result) && isVrSession(session)) {
                m_stats.waitCpuTimeUs += m_performanceCounters.waitCpuTimer.stop();

                // Apply prediction dampening if possible and if needed.
                const int predictionDampen = m_configManager->getValue(config::SettingPredictionDampen);
                if (predictionDampen != 100) {
                    const auto predictionAmount = frameState->predictedDisplayTime - getXrTimeNow();
                    if (predictionAmount > 0) {
                        frameState->predictedDisplayTime += (predictionAmount * (predictionDampen - 100u)) / 100u;
                    }
                    m_stats.predictionTimeUs += predictionAmount;
                }

                // Record the predicted display time.
                m_waitedFrameTime = frameState->predictedDisplayTime;
            }

            return result;
        }

        XrResult xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo) override {
            const XrResult result = OpenXrApi::xrBeginFrame(session, frameBeginInfo);
            if (XR_SUCCEEDED(result) && isVrSession(session)) {
                // Record the predicted display time.
                m_begunFrameTime = m_waitedFrameTime;
                m_isInFrame = true;

                if (m_graphicsDevice) {
                    m_performanceCounters.appCpuTimer.start();
                    m_stats.appGpuTimeUs += m_performanceCounters.startGpuTimer(m_gpuTimerApp);

                    // With D3D12, we want to make sure the query is enqueued now.
                    if (m_graphicsDevice->getApi() == graphics::Api::D3D12) {
                        m_graphicsDevice->flushContext();
                    }

                    if (m_frameAnalyzer) {
                        m_frameAnalyzer->resetForFrame();
                    }
                }

                if (m_eyeTracker) {
                    m_eyeTracker->beginFrame(m_begunFrameTime);
                }

                if (m_variableRateShader) {
                    m_variableRateShader->beginFrame(m_begunFrameTime);
                }
            }

            return result;
        }

        XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) override {
            if (!isVrSession(session) || !m_graphicsDevice) {
                return OpenXrApi::xrEndFrame(session, frameEndInfo);
            }

            m_isInFrame = false;

            {
                // Because gpu timers are async, we must accumulate cpu timer after updating the statistics:
                //   frame beg(N  ): accumulate GPU N-1
                //   frame end(N  ): updateStats N-1
                //                   accumulate CPU N
                //   frame beg(N+1): accumulate GPU N
                //   frame end(N+1): updateStats N
                //                   accumulate CPU N+1
                //   etc...

                updateStatisticsForFrame();
                m_stats.appCpuTimeUs += m_performanceCounters.appCpuTimer.stop();
                m_performanceCounters.gpuTimers[m_gpuTimerApp]->stop();

                m_performanceCounters.endFrameCpuTimer.start();
                m_graphicsDevice->resolveQueries();

                if (m_frameAnalyzer) {
                    m_frameAnalyzer->prepareForEndFrame();
                }
                // TODO: Ensure restoreContext() even on error.
                m_graphicsDevice->blockCallbacks();

                if (m_eyeTracker) {
                    m_eyeTracker->endFrame();
                }

                if (m_variableRateShader) {
                    m_variableRateShader->endFrame();
                    m_variableRateShader->stopCapture();
                }

                // Unbind all textures from the render targets.
                m_graphicsDevice->saveContext();
                m_graphicsDevice->unsetRenderTargets();

                if (m_menuHandler) {
                    m_menuHandler->handleInput();
                }
                updateConfiguration();
            }

            // We must reserve the underlying storage to keep our pointers stable.

            gLayerHeaders.reserve(frameEndInfo->layerCount + 1); // reserve space for the menu
            gLayerHeaders.assign(frameEndInfo->layers, frameEndInfo->layers + frameEndInfo->layerCount);
            gLayerProjections.clear();
            gLayerProjections.reserve(gLayerHeaders.size());
            gLayerProjectionsViews.clear();
            gLayerProjectionsViews.reserve(gLayerHeaders.size() * utilities::ViewCount);

            struct {
                const std::shared_ptr<graphics::ITexture>* color{nullptr};
                const std::shared_ptr<graphics::ITexture>* depth{nullptr};
                ViewProjection vproj{};
                XrSpace space{XR_NULL_HANDLE};
            } overlayData[utilities::ViewCount];

            // Apply the processing chain to all the (supported) layers.
            for (auto& baselayer : gLayerHeaders) {
                if (baselayer->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
                    static_assert(utilities::ViewCount == 2);

                    const auto layer = reinterpret_cast<const XrCompositionLayerProjection*>(baselayer);

                    // For VPRT, we need to handle texture arrays.
                    const auto useVPRT = layer->views[0].subImage.swapchain == layer->views[1].subImage.swapchain;
                    if (useVPRT) {
                        // Assume that we've properly distinguished left/right eyes.
                        // TODO: We need to use subImage.imageArrayIndex instead of assuming 0/left and 1/right.
                        m_stats.hasColorBuffer[to_integral(utilities::Eye::Left)] =
                            m_stats.hasColorBuffer[to_integral(utilities::Eye::Right)] = true;
                    }

                    assert(layer->viewCount == utilities::ViewCount);

                    for (uint32_t eye = 0; eye < utilities::ViewCount; eye++) {
                        auto& view = gLayerProjectionsViews.emplace_back(layer->views[eye]);

                        auto swapchainIt = m_swapchains.find(view.subImage.swapchain);
                        if (swapchainIt == m_swapchains.end()) {
                            throw std::runtime_error("Swapchain is not registered");
                        }

                        // Patch the eye poses (works with canting too) and the FOV.
                        view.pose.orientation = m_posesForFrame[eye].pose.orientation;
                        view.fov = m_posesForFrame[eye].fov;

                        // Prepare our overlay data.
                        overlayData[eye].vproj.Pose = view.pose;
                        overlayData[eye].vproj.Fov = view.fov;
                        overlayData[eye].vproj.NearFar = {0.001f, 100.f};
                        overlayData[eye].space = layer->space;

                        // Look for the depth buffer.
                        for (auto it = reinterpret_cast<const XrBaseInStructure*>(view.next); it; it = it->next) {
                            if (it->type != XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR)
                                continue;

                            // The order of color/depth textures must match.
                            const auto depthInfo = reinterpret_cast<const XrCompositionLayerDepthInfoKHR*>(it);
                            if (depthInfo->subImage.imageArrayIndex != view.subImage.imageArrayIndex)
                                continue;

                            auto depthchainIt = m_swapchains.find(depthInfo->subImage.swapchain);
                            if (depthchainIt != m_swapchains.end()) {
                                const auto& swapchainState = depthchainIt->second;
                                const auto& swapchainImages = swapchainState.images[swapchainState.acquiredImageIndex];

                                assert(swapchainImages.chain.size() == 1u);

                                overlayData[eye].depth = std::addressof(swapchainImages.chain[0]);
                                overlayData[eye].vproj.NearFar = {depthInfo->nearZ, depthInfo->farZ};

                                m_stats.hasDepthBuffer[eye] = true;
                                break;

                            } else
                                throw std::runtime_error("Swapchain is not registered");
                        }

                        // const bool isDepthInverted =
                        //    overlayData[eye].viewProj.NearFar.Far < overlayData[eye].viewProj.NearFar.Near;

                        // Now that we know what eye the swapchain is used for, register it.
                        // TODO: We always assume that if VPRT is used, left eye is texture 0 and right eye is
                        // texture 1. I'm sure this holds in like 99% of the applications, but still not very clean
                        // to assume.
                        auto& swapchainState = swapchainIt->second;

                        if (m_frameAnalyzer && !useVPRT && !swapchainState.registeredWithFrameAnalyzer) {
                            for (const auto& image : swapchainState.images) {
                                m_frameAnalyzer->registerColorSwapchainImage(image.chain[0],
                                                                             static_cast<utilities::Eye>(eye));
                            }
                            swapchainState.registeredWithFrameAnalyzer = true;
                        }

                        // Insert processing below.
                        //
                        // The pattern typically follows these steps:
                        // - Advanced to the right source and/or destination image;
                        // - Pull the previously measured timer value;
                        // - Start the timer;
                        // - Invoke the processing;
                        // - Stop the timer;
                        // - Advanced to the right source and/or destination image;

                        auto& swapchainImages = swapchainState.images[swapchainState.acquiredImageIndex];
                        overlayData[eye].color = std::addressof(swapchainImages.chain.back());

                        uint32_t nextImage = 0;
                        uint32_t lastImage = 0;

                        for (size_t i = 0; i < std::size(m_imageProcessors); i++) {
                            if (m_imageProcessors[i]) {
                                auto timer = swapchainImages.gpuTimers[i][useVPRT ? eye : 0].get();
                                m_stats.processorGpuTimeUs[i] += timer->query();

                                nextImage++;
                                timer->start();
                                m_imageProcessors[i]->process(swapchainImages.chain[lastImage],
                                                              swapchainImages.chain[nextImage],
                                                              useVPRT ? eye : -int32_t(eye + 1));
                                timer->stop();
                                lastImage++;
                            }
                        }

                        // Make sure the chain was completed.
                        if (nextImage != swapchainImages.chain.size() - 1) {
                            throw std::runtime_error("Processing chain incomplete!");
                        }

                        // Patch the resolution.
                        if (m_imageProcessors[ImgProc::Scale]) {
                            view.subImage.imageRect.extent.width = m_displayWidth;
                            view.subImage.imageRect.extent.height = m_displayHeight;
                        }
                    }

                    auto lastView = std::addressof(gLayerProjectionsViews.back());

                    // When doing the Pimax FOV hack, we swap left and right eyes.
                    if (m_supportFOVHack && m_configManager->peekValue(config::SettingPimaxFOVHack)) {
                        std::swap(lastView[0], lastView[-1]);
                        std::swap(overlayData[0].color, overlayData[1].color);
                        std::swap(overlayData[0].depth, overlayData[1].depth);
                        std::swap(overlayData[0].vproj, overlayData[1].vproj);
                        std::swap(overlayData[0].space, overlayData[1].space);
                    }

                    // To patch the layer resolution we recreate the whole projection and views data structures.
                    gLayerProjections.emplace_back(*layer).views = std::addressof(lastView[-1]);

                    baselayer =
                        reinterpret_cast<const XrCompositionLayerBaseHeader*>(std::addressof(gLayerProjections.back()));

                } else if (baselayer->type == XR_TYPE_COMPOSITION_LAYER_QUAD) {
                    const auto layer = reinterpret_cast<const XrCompositionLayerQuad*>(baselayer);
                    auto swapchainIt = m_swapchains.find(layer->subImage.swapchain);
                    if (swapchainIt != m_swapchains.end()) {
                        auto& swapchainState = swapchainIt->second;
                        auto& swapchainImages = swapchainState.images[swapchainState.acquiredImageIndex];
                        if (swapchainImages.chain.size() > 1u)
                            swapchainImages.chain.front()->copyTo(swapchainImages.chain.back());
                    } else
                        throw std::runtime_error("Swapchain is not registered");
                }
            }

            // We intentionally exclude the overlay from this timer, as it has its own separate timer.
            m_stats.endFrameCpuTimeUs += m_performanceCounters.endFrameCpuTimer.stop();

            // Render our overlays.
            {
                const bool drawHands = m_handTracker && m_configManager->peekEnumValue<config::HandTrackingVisibility>(
                                                            config::SettingHandVisibilityAndSkinTone) !=
                                                            config::HandTrackingVisibility::Hidden;
                const bool drawEyeGaze = m_eyeTracker && m_configManager->getValue(config::SettingEyeDebug);
                const bool drawOverlays = m_menuHandler || drawHands || drawEyeGaze;

                if (drawOverlays) {
                    m_performanceCounters.overlayCpuTimer.start();
                    m_stats.overlayGpuTimeUs += m_performanceCounters.startGpuTimer(m_gpuTimerOvr);
                }

                // Render the hands or eye gaze helper.
                if (drawHands || drawEyeGaze) {
                    const auto useVPRT = overlayData[0].color && overlayData[1].color &&
                                         overlayData[0].color->get() == overlayData[1].color->get();

                    for (uint32_t eye = 0; eye < utilities::ViewCount; eye++) {
                        const auto& overlay = overlayData[eye];
                        if (overlay.color) {
                            m_graphicsDevice->setRenderTargets(1,
                                                               overlay.color,
                                                               useVPRT ? reinterpret_cast<int32_t*>(&eye) : nullptr,
                                                               overlay.depth ? *overlay.depth : nullptr,
                                                               useVPRT ? eye : -1);

                            m_graphicsDevice->setViewProjection(overlay.vproj);

                            if (drawHands) {
                                m_handTracker->render(overlay.vproj.Pose, overlay.space, getXrTimeNow());
                            }

                            if (drawEyeGaze) {
                                const auto isEyeGazeValid = m_eyeTracker && m_eyeTracker->getProjectedGaze(m_eyeGaze);
                                const XrColor4f color = isEyeGazeValid ? XrColor4f{0, 1, 0, 1} : XrColor4f{1, 0, 0, 1};
                                auto pos = utilities::NdcToScreen(m_eyeGaze[eye]);
                                pos.x *= (*overlay.color)->getInfo().width;
                                pos.y *= (*overlay.color)->getInfo().height;
                                m_graphicsDevice->clearColor(pos.y - 20, pos.x - 20, pos.y + 20, pos.x + 20, color);
                            }
                        }
                    }
                }

                // Render the menu.
                if (m_menuHandler && (m_menuHandler->isVisible() || m_menuLingering)) {
                    // Workaround: there is a bug in the WMR runtime that causes a past quad layer content to linger
                    // on the next projection layer. We make sure to submit a completely blank quad layer for 3
                    // frames after its disappearance. The number 3 comes from the number of depth buffers cached
                    // inside the precompositor of the WMR runtime.
                    m_menuLingering = m_menuHandler->isVisible() ? 3 : m_menuLingering - 1;

                    uint32_t menuImageIndex;
                    {
                        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                        CHECK_XRCMD(OpenXrApi::xrAcquireSwapchainImage(m_menuSwapchain, &acquireInfo, &menuImageIndex));

                        XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                        waitInfo.timeout = 100000000000; // 100ms
                        CHECK_XRCMD(OpenXrApi::xrWaitSwapchainImage(m_menuSwapchain, &waitInfo));
                    }

                    const auto& textureInfo = m_menuSwapchainImages[menuImageIndex]->getInfo();

                    m_graphicsDevice->setRenderTargets(1, &m_menuSwapchainImages[menuImageIndex]);
                    m_graphicsDevice->clearColor(
                        0, 0, (float)textureInfo.height, (float)textureInfo.width, XrColor4f{0, 0, 0, 0});
                    m_graphicsDevice->beginText();
                    m_menuHandler->render(m_menuSwapchainImages[menuImageIndex]);
                    m_graphicsDevice->flushText();
                    m_graphicsDevice->unsetRenderTargets();

                    {
                        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                        CHECK_XRCMD(OpenXrApi::xrReleaseSwapchainImage(m_menuSwapchain, &releaseInfo));
                    }

                    // Add the quad layer to the frame.
                    const auto menuEyeVisibility =
                        static_cast<XrEyeVisibility>(m_configManager->getValue(config::SettingMenuEyeVisibility));

                    gLayerQuadForMenu.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
                    gLayerQuadForMenu.next = nullptr;
                    gLayerQuadForMenu.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                    gLayerQuadForMenu.space = m_viewSpace;
                    gLayerQuadForMenu.eyeVisibility =
                        std::clamp(menuEyeVisibility, XR_EYE_VISIBILITY_BOTH, XR_EYE_VISIBILITY_RIGHT);
                    gLayerQuadForMenu.subImage.swapchain = m_menuSwapchain;
                    gLayerQuadForMenu.subImage.imageRect.extent.width = textureInfo.width;
                    gLayerQuadForMenu.subImage.imageRect.extent.height = textureInfo.height;
                    gLayerQuadForMenu.size = {1.f, 1.f}; // 1m x 1m
                    gLayerQuadForMenu.pose =
                        Pose::Translation({0, 0, m_configManager->getValue(config::SettingMenuDistance) * -0.01f});

                    gLayerHeaders.push_back(
                        reinterpret_cast<const XrCompositionLayerBaseHeader*>(std::addressof(gLayerQuadForMenu)));
                }

                if (drawOverlays) {
                    m_stats.overlayCpuTimeUs += m_performanceCounters.overlayCpuTimer.stop();
                    m_performanceCounters.gpuTimers[m_gpuTimerOvr]->stop();
                }
            }

            // Whether the menu is available or not, we can still use that top-most texture for screenshot.
            // TODO: The screenshot does not work with multi-layer applications.
            const bool requestScreenshot =
                utilities::UpdateKeyState(m_requestScreenShotKeyState, m_keyModifiers, m_keyScreenshot, false) &&
                m_configManager->getValue(config::SettingScreenshotEnabled);

            if (requestScreenshot) {
                // TODO: this is capturing frame N-3
                // review the command queues/lists and context flush
                const auto shotEye = m_configManager->getValue(config::SettingScreenshotEye);

                if (overlayData[0].color && shotEye != 2 /* Right only */)
                    takeScreenshot(overlayData[0].color->get(), "L");
                
                if (overlayData[1].color && shotEye != 1 /* Left only */)
                    takeScreenshot(overlayData[1].color->get(), "R");

                if (m_variableRateShader && m_configManager->getValue("vrs_capture"))
                    m_variableRateShader->startCapture();
            }

            m_graphicsDevice->restoreContext();
            m_graphicsDevice->flushContext(false, true);

            // Release the swapchain images now, as we are really done this time.
            for (auto& swapchain : m_swapchains) {
                if (swapchain.second.delayedRelease) {
                    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, nullptr};
                    swapchain.second.delayedRelease = false;
                    CHECK_XRCMD(OpenXrApi::xrReleaseSwapchainImage(swapchain.first, &releaseInfo));
                }
            }

            auto chainFrameEndInfo = *frameEndInfo;
            chainFrameEndInfo.layerCount = static_cast<uint32_t>(gLayerHeaders.size());
            chainFrameEndInfo.layers = gLayerHeaders.data();

            const auto result = OpenXrApi::xrEndFrame(session, &chainFrameEndInfo);
            m_graphicsDevice->unblockCallbacks();

            return result;
        }

      private:
        bool isVrSystem(XrSystemId systemId) const {
            return systemId == m_vrSystemId;
        }

        bool isVrSession(XrSession session) const {
            return session == m_vrSession;
        }

        std::string getXrPath(XrPath path) {
            std::string str;
            if (path != XR_NULL_PATH) {
                // TODO: I can't find in the spec if max path includes the trailing 0?
                str.resize(XR_MAX_PATH_LENGTH + 1);
                auto count = static_cast<uint32_t>(str.size());
                CHECK_XRCMD(xrPathToString(GetXrInstance(), path, count, &count, &*str.begin())); // safe idiom
                str.resize(size_t(count) - (count != 0));
            }
            return str;
        }

        // Find the current time. Fallback to the frame time if we cannot query the actual time.
        XrTime getXrTimeNow() const {
            XrTime xrTimeNow = m_begunFrameTime;
            if (xrConvertWin32PerformanceCounterToTimeKHR) {
                LARGE_INTEGER qpcTimeNow;
                QueryPerformanceCounter(&qpcTimeNow);
                CHECK_XRCMD(xrConvertWin32PerformanceCounterToTimeKHR(GetXrInstance(), &qpcTimeNow, &xrTimeNow));
            }
            return xrTimeNow;
        }

        bool calibrateEyeProjection(XrSession session, XrViewLocateInfo viewLocateInfo) {
            viewLocateInfo.space = m_viewSpace;
            auto viewsState = XrViewState{XR_TYPE_VIEW_STATE};
            XrView eyeInViewSpace[utilities::ViewCount] = {{XR_TYPE_VIEW, nullptr}, {XR_TYPE_VIEW, nullptr}};
            uint32_t viewsCount = utilities::ViewCount;

            CHECK_HRCMD(OpenXrApi::xrLocateViews(
                session, &viewLocateInfo, &viewsState, viewsCount, &viewsCount, eyeInViewSpace));

            if (Pose::IsPoseValid(viewsState)) {
                // This code is based on vrperfkit by Frydrych Holger.
                // https://github.com/fholger/vrperfkit/blob/master/src/openvr/openvr_manager.cpp

                using namespace DirectX;

                // get angle between the two view planes
                const auto viewsDotAngle = XMVectorGetX(
                    XMVector3Dot(LoadXrPose(eyeInViewSpace[0].pose).r[2], LoadXrPose(eyeInViewSpace[1].pose).r[2]));

                // get the diff angle tangent around the center
                auto canted = std::tanf(std::abs(std::acosf(viewsDotAngle)) / 2) * 2;

                // In normalized screen coordinates.
                for (uint32_t eye = 0; eye < utilities::ViewCount; eye++) {
                    const auto& fov = eyeInViewSpace[eye].fov;
                    m_projCenters[eye].x = (fov.angleLeft + fov.angleRight - canted) / (fov.angleLeft - fov.angleRight);
                    m_projCenters[eye].y = (fov.angleDown + fov.angleUp) / (fov.angleDown - fov.angleUp);
                    m_eyeGaze[eye] = m_projCenters[eye];
                    canted = -canted; // for next eye
                }

                // Example with G2:
                // [OXRTK] Projection calibration : 0.05228, 0.00091 | -0.05176, -0.00091

                Log("Projection calibration: %.5f, %.5f | %.5f, %.5f\n",
                    m_projCenters[0].x,
                    m_projCenters[0].y,
                    m_projCenters[1].x,
                    m_projCenters[1].y);

                if (m_variableRateShader) {
                    m_variableRateShader->setViewProjectionCenters(m_projCenters[0], m_projCenters[1]);
                }

                return true;
            }
            return false;
        }

        void updateStatisticsForFrame() {
            const auto now = std::chrono::steady_clock::now();
            const auto numFrames = ++m_performanceCounters.numFrames;

            if (m_graphicsDevice) {
                m_stats.numBiasedSamplers = m_graphicsDevice->getNumBiasedSamplersThisFrame();
            }

            if (m_performanceCounters.updateTimer.restart(std::chrono::seconds(1))) {
                m_performanceCounters.numFrames = 0;

                // TODO: no need to compute these if no menu handler
                // or if menu isn't displaying any stats.

                // Push the last averaged statistics.
                m_stats.appCpuTimeUs /= numFrames;
                m_stats.appGpuTimeUs /= numFrames;
                m_stats.waitCpuTimeUs /= numFrames;
                m_stats.endFrameCpuTimeUs /= numFrames;
                m_stats.processorGpuTimeUs[0] /= numFrames;
                m_stats.processorGpuTimeUs[1] /= numFrames;
                m_stats.processorGpuTimeUs[2] /= numFrames;
                m_stats.overlayCpuTimeUs /= numFrames;
                m_stats.overlayGpuTimeUs /= numFrames;
                m_stats.handTrackingCpuTimeUs /= numFrames;
                m_stats.predictionTimeUs /= numFrames;
                m_stats.fps = static_cast<float>(numFrames);

                // When CPU-bound, do not bother giving a (false) GPU time for D3D12
                if (m_graphicsDevice->getAs<graphics::D3D12>()) {
                    if (m_stats.appGpuTimeUs < (m_stats.appCpuTimeUs + 500))
                        m_stats.appGpuTimeUs = 0;
                }

                if (m_menuHandler) {
                    // convert to degrees for display (1Hz)
                    StoreXrFov(&m_stats.fov[0], ConvertToDegrees(m_posesForFrame[0].fov));
                    StoreXrFov(&m_stats.fov[1], ConvertToDegrees(m_posesForFrame[1].fov));
                    m_menuHandler->updateStatistics(m_stats);
                }

                // Start from fresh!
                memset(&m_stats, 0, sizeof(m_stats));
            }

            if (m_handTracker && m_menuHandler) {
                m_menuHandler->updateGesturesState(m_handTracker->getGesturesState());
            }
            if (m_eyeTracker && m_menuHandler) {
                m_menuHandler->updateEyeGazeState(m_eyeTracker->getEyeGazeState());
            }

            std::fill_n(m_stats.hasColorBuffer, std::size(m_stats.hasColorBuffer), false);
            std::fill_n(m_stats.hasDepthBuffer, std::size(m_stats.hasDepthBuffer), false);
            m_stats.numRenderTargetsWithVRS = 0;
        }

        void updateConfiguration() {
            // Make sure config gets written if needed.
            m_configManager->tick();

            // Forward the motion reprojection locking values to WMR.
            if (m_supportMotionReprojectionLock &&
                (m_configManager->hasChanged(config::SettingMotionReprojection) ||
                 m_configManager->hasChanged(config::SettingMotionReprojectionRate))) {
                const auto motionReprojection =
                    m_configManager->getEnumValue<config::MotionReprojection>(config::SettingMotionReprojection);
                const auto rate = m_configManager->getEnumValue<config::MotionReprojectionRate>(
                    config::SettingMotionReprojectionRate);

                // If motion reprojection is not controlled by us, then make sure the reprojection rate is left to
                // default.
                if (motionReprojection != config::MotionReprojection::On) {
                    utilities::UpdateWindowsMixedRealityReprojectionRate(config::MotionReprojectionRate::Off);
                } else {
                    utilities::UpdateWindowsMixedRealityReprojectionRate(rate);
                }
            }

            // Adjust mip map biasing.
            if (m_configManager->hasChanged(config::SettingMipMapBias) ||
                m_configManager->hasChanged(config::SettingScalingType)) {
                const auto biasing = m_configManager->getEnumValue<config::ScalingType>(config::SettingScalingType) !=
                                             config::ScalingType::None
                                         ? m_configManager->getEnumValue<config::MipMapBias>(config::SettingMipMapBias)
                                         : config::MipMapBias::Off;
                m_graphicsDevice->setMipMapBias(biasing, m_mipMapBiasForUpscaling);
            }

            // Check to reload shaders.
            bool reloadShaders = false;
            if (m_configManager->hasChanged(config::SettingReloadShaders)) {
                if (m_configManager->getValue(config::SettingReloadShaders)) {
                    m_configManager->setValue(config::SettingReloadShaders, 0, true);
                    reloadShaders = true;
                }
            }

            // Update eye tracking and vrs first
            if (m_eyeTracker)
                m_eyeTracker->update();

            if (m_variableRateShader)
                m_variableRateShader->update();

            // Update image processors and prepare the Shaders for rendering.
            for (auto& processor : m_imageProcessors) {
                if (processor) {
                    if (reloadShaders)
                        processor->reload();
                    processor->update();
                }
            }
        }

        void takeScreenshot(const graphics::ITexture* texture, std::string_view suffix) const {
            auto path = localAppData / "screenshots";
            {
                SYSTEMTIME st;
                ::GetLocalTime(&st);

                std::stringstream parameters;
                parameters << m_applicationName << '_' << ((st.wYear * 10000u) + (st.wMonth * 100u) + (st.wDay)) << '_'
                           << ((st.wHour * 10000u) + (st.wMinute * 100u) + (st.wSecond));

                if (m_upscaleMode != config::ScalingType::None) {
                    // TODO: add a getUpscaleModeName() helper to keep enum and string in sync.
                    const auto upscaleName = m_upscaleMode == config::ScalingType::NIS   ? "_NIS_"
                                             : m_upscaleMode == config::ScalingType::FSR ? "_FSR_"
                                                                                         : "_SCL_";
                    parameters << upscaleName << m_configManager->getValue(config::SettingScaling) << "_"
                               << m_configManager->getValue(config::SettingSharpness);
                }
                parameters << "_" << suffix;
                path /= parameters.str();
            }

            const auto fileFormat =
                m_configManager->getEnumValue<config::ScreenshotFileFormat>(config::SettingScreenshotFileFormat);

            const auto fileExtension = fileFormat == config::ScreenshotFileFormat::DDS   ? ".dds"
                                       : fileFormat == config::ScreenshotFileFormat::JPG ? ".jpg"
                                       : fileFormat == config::ScreenshotFileFormat::BMP ? ".bmp"
                                                                                         : ".png";
            // Using std::filesystem automatically filters out unwanted app name chars.
            texture->saveToFile(path.replace_extension(fileExtension));
        }

        std::string m_applicationName;
        bool m_isOpenComposite{false};
        std::string m_runtimeName;
        std::string m_systemName;
        XrSystemId m_vrSystemId{XR_NULL_SYSTEM_ID};
        XrSession m_vrSession{XR_NULL_HANDLE};
        uint32_t m_displayWidth{0};
        uint32_t m_displayHeight{0};
        float m_resolutionHeightRatio{1.f};
        uint32_t m_maxDisplayWidth{0};
        bool m_supportHandTracking{false};
        bool m_supportEyeTracking{false};
        bool m_supportFOVHack{false};
        bool m_supportMotionReprojectionLock{false};

        XrTime m_waitedFrameTime;
        XrTime m_begunFrameTime;
        bool m_isInFrame{false};
        bool m_sendInterationProfileEvent{false};
        uint32_t m_visibilityMaskEventIndex{utilities::ViewCount};
        XrSpace m_viewSpace{XR_NULL_HANDLE};
        bool m_needCalibrateEyeProjections{true};
        XrVector2f m_projCenters[utilities::ViewCount];
        XrVector2f m_eyeGaze[utilities::ViewCount];
        XrView m_posesForFrame[utilities::ViewCount];

        std::shared_ptr<config::IConfigManager> m_configManager;

        std::shared_ptr<graphics::IDevice> m_graphicsDevice;
        std::map<XrSwapchain, SwapchainState> m_swapchains;

        config::ScalingType m_upscaleMode{config::ScalingType::None};
        float m_mipMapBiasForUpscaling{0.f};

        std::shared_ptr<graphics::IFrameAnalyzer> m_frameAnalyzer;
        std::shared_ptr<input::IEyeTracker> m_eyeTracker;
        std::shared_ptr<input::IHandTracker> m_handTracker;

        std::array<std::shared_ptr<graphics::IImageProcessor>, ImgProc::MaxValue> m_imageProcessors;
        std::shared_ptr<graphics::IVariableRateShader> m_variableRateShader;

        std::vector<int> m_keyModifiers;
        int m_keyScreenshot;
        XrSwapchain m_menuSwapchain{XR_NULL_HANDLE};
        std::vector<std::shared_ptr<graphics::ITexture>> m_menuSwapchainImages;
        std::shared_ptr<menu::IMenuHandler> m_menuHandler;
        int m_menuLingering{0};
        bool m_requestScreenShotKeyState{false};

        uint8_t m_gpuTimerApp{0};
        uint8_t m_gpuTimerOvr{0};

        struct {
            std::shared_ptr<graphics::IGpuTimer> gpuTimers[8];

            utilities::CpuTimer updateTimer;
            utilities::CpuTimer appCpuTimer;
            utilities::CpuTimer waitCpuTimer;
            utilities::CpuTimer endFrameCpuTimer;
            utilities::CpuTimer overlayCpuTimer;
            utilities::CpuTimer handTrackingTimer;
            uint32_t numFrames{0};
            uint32_t gpuTimersId{0};

            uint64_t startGpuTimer(uint8_t& id) {
                const auto lap = gpuTimers[id]->query();
                id = getNextGpuTimerId();
                gpuTimers[id]->start();
                return lap;
            }
            void stopGpuTimer(uint8_t id) {
                gpuTimers[id]->stop();
            }

            uint8_t getNextGpuTimerId() {
                static_assert(isPow2(ARRAYSIZE(gpuTimers)));
                return static_cast<uint8_t>((++gpuTimersId) % std::size(gpuTimers));
            }

            void createGpuTimers(graphics::IDevice* device) {
                for (auto& it : gpuTimers)
                    it = device->createTimer();
            }

            void destroyGpuTimers() {
                std::fill_n(gpuTimers, std::size(gpuTimers), nullptr);
            }

        } m_performanceCounters;

        menu::MenuStatistics m_stats{};

        // TODO: These should be auto-generated and accessible via OpenXrApi.
        PFN_xrConvertWin32PerformanceCounterToTimeKHR xrConvertWin32PerformanceCounterToTimeKHR{nullptr};
        PFN_xrGetVisibilityMaskKHR m_xrGetVisibilityMaskKHR{nullptr};

        static XrResult _xrGetVisibilityMaskKHR(XrSession session,
                                                XrViewConfigurationType viewConfigurationType,
                                                uint32_t viewIndex,
                                                XrVisibilityMaskTypeKHR visibilityMaskType,
                                                XrVisibilityMaskKHR* visibilityMask) {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local,
                                   "xrGetVisibilityMaskKHR",
                                   TLPArg(session),
                                   TLArg((int)viewConfigurationType),
                                   TLArg(viewIndex),
                                   TLArg((int)visibilityMaskType));

            XrResult result;
            try {
                result = dynamic_cast<OpenXrLayer*>(GetInstance())
                             ->xrGetVisibilityMaskKHR(
                                 session, viewConfigurationType, viewIndex, visibilityMaskType, visibilityMask);
            } catch (std::exception& exc) {
                TraceLoggingWriteTagged(local, "xrGetVisibilityMaskKHR_Error", TLArg(exc.what(), "Error"));
                Log("%s\n", exc.what());
                result = XR_ERROR_RUNTIME_FAILURE;
            }

            TraceLoggingWriteStop(local, "xrGetVisibilityMaskKHR", TLArg((int)result));

            return result;
        }
    };

    std::unique_ptr<OpenXrLayer> g_instance = nullptr;

} // namespace

namespace toolkit {
    OpenXrApi* GetInstance() {
        if (!g_instance) {
            g_instance = std::make_unique<OpenXrLayer>();
        }
        return g_instance.get();
    }

    void ResetInstance() {
        g_instance.reset();
    }

} // namespace toolkit

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        TraceLoggingRegister(g_traceProvider);
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
