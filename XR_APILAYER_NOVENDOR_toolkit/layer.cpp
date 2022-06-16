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
    using namespace xr::math;
    using namespace toolkit::math;

    // The xrWaitFrame() loop might cause to have 2 frames in-flight, so we want to delay the GPU timer re-use by those
    // 2 frames.
    constexpr uint32_t GpuTimerLatency = 2;

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
            m_configManager->setDefault(config::SettingMenuEyeVisibility, 0); // Both
            m_configManager->setDefault(config::SettingMenuDistance, 100);    // 1m
            m_configManager->setDefault(config::SettingMenuOpacity, 85);
            m_configManager->setDefault(config::SettingMenuFontSize, 44); // pt
            m_configManager->setEnumDefault(config::SettingMenuTimeout, config::MenuTimeout::Medium);
            m_configManager->setDefault(config::SettingMenuExpert, m_configManager->getValue(config::SettingDeveloper));
            m_configManager->setEnumDefault(config::SettingOverlayType, config::OverlayType::None);
            // Legacy setting is 1/3rd from top and 2/3rd from left.
            {
                const auto ndcOffset = utilities::ScreenToNdc({2 / 3.f, 1 / 3.f});
                m_configManager->setDefault(config::SettingOverlayXOffset, (int)(ndcOffset.x * 100.f));
                m_configManager->setDefault(config::SettingOverlayYOffset, (int)(ndcOffset.y * 100.f));
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
                                            !m_isOpenComposite ? config::MipMapBias::Anisotropic
                                                               : config::MipMapBias::Off);

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
            m_configManager->setDefault(config::SettingICD, 1000);
            m_configManager->setDefault(config::SettingFOVType, 0); // Simple
            m_configManager->setDefault(config::SettingFOV, 100);
            m_configManager->setDefault(config::SettingFOVUp, 100);
            m_configManager->setDefault(config::SettingFOVDown, 100);
            m_configManager->setDefault(config::SettingFOVLeftLeft, 100);
            m_configManager->setDefault(config::SettingFOVLeftRight, 100);
            m_configManager->setDefault(config::SettingFOVRightLeft, 100);
            m_configManager->setDefault(config::SettingFOVRightRight, 100);
            m_configManager->setDefault(config::SettingPimaxFOVHack, 0);
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
                !(m_applicationName == "OpenComposite_AC2-Win64-Shipping" || m_applicationName == "OpenComposite_Il-2")
                    ? 0
                    : 1);
            // We disable the frame analyzer when using OpenComposite, because the app does not see the OpenXR
            // textures anyways.
            m_configManager->setDefault("disable_frame_analyzer", !m_isOpenComposite ? 0 : 1);
            m_configManager->setDefault("canting", 0);
            m_configManager->setDefault("vrs_capture", 0);

            // Workaround: the first versions of the toolkit used a different representation for the world scale.
            // Migrate the value upon first run.
            m_configManager->setDefault("icd", 0);
            if (m_configManager->getValue("icd") != 0) {
                const int migratedValue = 1'000'000 / m_configManager->getValue("icd");
                m_configManager->setValue(config::SettingICD, migratedValue, true);
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
            Log("Application name: '%s', Engine name: '%s'\n",
                createInfo->applicationInfo.applicationName,
                createInfo->applicationInfo.engineName);
            m_isOpenComposite = m_applicationName.find("OpenComposite_") == 0;
            if (m_isOpenComposite) {
                Log("Detected OpenComposite\n");
            }

            // Dump the OpenXR runtime information to help debugging customer issues.
            XrInstanceProperties instanceProperties = {XR_TYPE_INSTANCE_PROPERTIES, nullptr};
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
            if (m_runtimeName.find("Varjo") == std::string::npos) {
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

            // For eye tracking, we try to use the Omnicept runtime if it's available.
            std::unique_ptr<HP::Omnicept::Client> omniceptClient;
            if (utilities::IsServiceRunning("HP Omnicept")) {
                try {
                    HP::Omnicept::Client::StateCallback_T stateCallback = [&](const HP::Omnicept::Client::State state) {
                        if (state == HP::Omnicept::Client::State::RUNNING ||
                            state == HP::Omnicept::Client::State::PAUSED) {
                            Log("Omnicept client connected\n");
                        } else if (state == HP::Omnicept::Client::State::DISCONNECTED) {
                            Log("Omnicept client disconnected\n");
                        }
                    };

                    std::unique_ptr<HP::Omnicept::Glia::AsyncClientBuilder> omniceptClientBuilder =
                        HP::Omnicept::Glia::StartBuildClient_Async(
                            "OpenXR-Toolkit",
                            std::move(std::make_unique<HP::Omnicept::Abi::SessionLicense>(
                                "", "", HP::Omnicept::Abi::LicensingModel::CORE, false)),
                            stateCallback);

                    omniceptClient = std::move(omniceptClientBuilder->getBuildClientResultOrThrow());
                    Log("Detected HP Omnicept support\n");
                    m_isOmniceptDetected = true;
                } catch (const HP::Omnicept::Abi::HandshakeError& e) {
                    Log("Could not connect to Omnicept runtime HandshakeError: %s\n", e.what());
                } catch (const HP::Omnicept::Abi::TransportError& e) {
                    Log("Could not connect to Omnicept runtime TransportError: %s\n", e.what());
                } catch (const HP::Omnicept::Abi::ProtocolError& e) {
                    Log("Could not connect to Omnicept runtime ProtocolError: %s\n", e.what());
                } catch (std::exception& e) {
                    Log("Could not connect to Omnicept runtime: %s\n", e.what());
                }
            }

            // ...and the Pimax eye tracker if available.
            {
                XrSystemGetInfo getInfo{XR_TYPE_SYSTEM_GET_INFO};
                getInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
                XrSystemId systemId;
                if (XR_SUCCEEDED(OpenXrApi::xrGetSystem(GetXrInstance(), &getInfo, &systemId))) {
                    XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
                    CHECK_XRCMD(OpenXrApi::xrGetSystemProperties(GetXrInstance(), systemId, &systemProperties));
                    if (std::string(systemProperties.systemName).find("aapvr") != std::string::npos) {
                        aSeeVRInitParam param;
                        param.ports[0] = 5777;
                        Log("--> aSeeVR_connect_server\n");
                        m_hasPimaxEyeTracker = aSeeVR_connect_server(&param) == ASEEVR_RETURN_CODE::success;
                        Log("<-- aSeeVR_connect_server\n");
                        if (m_hasPimaxEyeTracker) {
                            Log("Detected Pimax Droolon support\n");
                        }
                    }
                }
            }

            // ...otherwise, we will try to fallback to OpenXR.

            // TODO: If Foveated Rendering is disabled, maybe do not initialize the eye tracker?
            if (m_configManager->getValue(config::SettingEyeTrackingEnabled)) {
                if (omniceptClient) {
                    m_eyeTracker = input::CreateOmniceptEyeTracker(*this, m_configManager, std::move(omniceptClient));
                } else if (m_hasPimaxEyeTracker) {
                    m_eyeTracker = input::CreatePimaxEyeTracker(*this, m_configManager);
                } else {
                    m_eyeTracker = input::CreateEyeTracker(*this, m_configManager);
                }
            }

            return XR_SUCCESS;
        }

        ~OpenXrLayer() override {
            // We cleanup after ourselves (again) to avoid leaving state registry entries.
            utilities::ClearWindowsMixedRealityReprojection();

            if (m_configManager) {
                m_configManager->setActiveSession("");
            }

            graphics::UnhookForD3D11DebugLayer();
        }

        XrResult xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function) override {
            const std::string apiName(name);
            XrResult result;

            // TODO: This should be auto-generated by the dispatch layer, but today our generator only looks at core
            // spec. We may let this fail intentionally and check that the pointer is populated later.
            if (apiName == "xrGetVisibilityMaskKHR") {
                result = m_xrGetInstanceProcAddr(instance, name, function);
                m_xrGetVisibilityMaskKHR = reinterpret_cast<PFN_xrGetVisibilityMaskKHR>(*function);
                *function = reinterpret_cast<PFN_xrVoidFunction>(_xrGetVisibilityMaskKHR);
            } else {
                result = OpenXrApi::xrGetInstanceProcAddr(instance, name, function);
            }
            return result;
        }

        XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId) override {
            const XrResult result = OpenXrApi::xrGetSystem(instance, getInfo, systemId);
            if (XR_SUCCEEDED(result) && getInfo->formFactor == XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY &&
                m_vrSystemId == XR_NULL_SYSTEM_ID) {
                const bool isDeveloper = m_configManager->getValue(config::SettingDeveloper);

                // Retrieve the actual OpenXR resolution.
                XrViewConfigurationView views[utilities::ViewCount] = {{XR_TYPE_VIEW_CONFIGURATION_VIEW, nullptr},
                                                                       {XR_TYPE_VIEW_CONFIGURATION_VIEW, nullptr}};
                uint32_t viewCount;
                CHECK_XRCMD(OpenXrApi::xrEnumerateViewConfigurationViews(instance,
                                                                         *systemId,
                                                                         XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                                         utilities::ViewCount,
                                                                         &viewCount,
                                                                         views));

                m_displayWidth = views[0].recommendedImageRectWidth;
                m_configManager->setDefault(config::SettingResolutionWidth, m_displayWidth);
                m_displayHeight = views[0].recommendedImageRectHeight;

                m_resolutionHeightRatio = (float)m_displayHeight / m_displayWidth;
                m_maxDisplayWidth = std::min(views[0].maxImageRectWidth,
                                             (uint32_t)(views[0].maxImageRectHeight / m_resolutionHeightRatio));

                // Check for hand and eye tracking support.
                XrSystemHandTrackingPropertiesEXT handTrackingSystemProperties{
                    XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT, nullptr};
                handTrackingSystemProperties.supportsHandTracking = false;

                XrSystemEyeGazeInteractionPropertiesEXT eyeTrackingSystemProperties{
                    XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT, &handTrackingSystemProperties};
                eyeTrackingSystemProperties.supportsEyeGazeInteraction = false;

                XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES, &eyeTrackingSystemProperties};
                CHECK_XRCMD(OpenXrApi::xrGetSystemProperties(instance, *systemId, &systemProperties));

                m_systemName = systemProperties.systemName;
                TraceLoggingWrite(
                    g_traceProvider,
                    "xrGetSystem",
                    TLArg(m_systemName.c_str(), "System"),
                    TLArg(m_displayWidth, "RecommendedResolutionX"),
                    TLArg(m_displayHeight, "RecommendedResolutionY"),
                    TLArg(handTrackingSystemProperties.supportsHandTracking, "SupportsHandTracking"),
                    TLArg(eyeTrackingSystemProperties.supportsEyeGazeInteraction, "SupportsEyeGazeInteraction"));
                Log("Using OpenXR system %s\n", m_systemName.c_str());

                // Detect when the Pimax FOV hack is applicable.
                m_supportFOVHack =
                    isDeveloper || (m_applicationName == "FS2020" && m_systemName.find("aapvr") != std::string::npos);

                const auto isWMR = m_runtimeName.find("Windows Mixed Reality Runtime") != std::string::npos;
                m_supportMotionReprojectionLock = isWMR;

                m_supportHandTracking = handTrackingSystemProperties.supportsHandTracking;
                m_supportEyeTracking = eyeTrackingSystemProperties.supportsEyeGazeInteraction || m_isOmniceptDetected ||
                                       m_hasPimaxEyeTracker ||
                                       m_configManager->getValue(config::SettingEyeDebugWithController);
                const bool isEyeTrackingThruRuntime =
                    m_supportEyeTracking && !(m_isOmniceptDetected || m_hasPimaxEyeTracker);

                // Workaround: the WMR runtime supports mapping the VR controllers through XR_EXT_hand_tracking, which
                // will (falsely) advertise hand tracking support. Check for the Ultraleap layer in this case.
                if (m_supportHandTracking &&
                    (!isDeveloper &&
                     (!m_configManager->getValue(config::SettingBypassMsftHandInteractionCheck) && isWMR))) {
                    bool hasUltraleapLayer = false;
                    for (const auto& layer : GetUpstreamLayers()) {
                        if (layer == "XR_APILAYER_ULTRALEAP_hand_tracking") {
                            hasUltraleapLayer = true;
                        }
                    }
                    if (!hasUltraleapLayer) {
                        Log("Ignoring XR_MSFT_hand_interaction for %s\n", m_runtimeName.c_str());
                        m_supportHandTracking = false;
                    }
                }

                // Workaround: the WMR runtime supports emulating eye tracking for development through
                // XR_EXT_eye_gaze_interaction, which will (falsely) advertise eye tracking support. Disable it.
                if (isEyeTrackingThruRuntime &&
                    (!isDeveloper &&
                     (!m_configManager->getValue(config::SettingBypassMsftEyeGazeInteractionCheck) && isWMR))) {
                    Log("Ignoring XR_EXT_eye_gaze_interaction for %s\n", m_runtimeName.c_str());
                    m_supportEyeTracking = false;
                }

                // We had to initialize the hand and eye trackers early on. If we find out now that they are not
                // supported, then destroy them. This could happen if the option was set while a hand tracking device
                // was connected, but later the hand tracking device was disconnected.
                if (!m_supportHandTracking) {
                    m_handTracker.reset();
                }
                if (!m_supportEyeTracking) {
                    m_eyeTracker.reset();
                }

                // Apply override to the target resolution.
                if (m_configManager->getValue(config::SettingResolutionOverride)) {
                    m_displayWidth = m_configManager->getValue(config::SettingResolutionWidth);
                    m_displayHeight = (uint32_t)(m_displayWidth * m_resolutionHeightRatio);

                    Log("Overriding OpenXR resolution: %ux%u\n", m_displayWidth, m_displayHeight);
                }

                // Remember the XrSystemId to use.
                m_vrSystemId = *systemId;
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
                // Determine the application resolution.
                const auto upscaleMode = m_configManager->getEnumValue<config::ScalingType>(config::SettingScalingType);

                uint32_t inputWidth = m_displayWidth;
                uint32_t inputHeight = m_displayHeight;

                switch (upscaleMode) {
                case config::ScalingType::FSR:
                case config::ScalingType::NIS: {
                    std::tie(inputWidth, inputHeight) =
                        config::GetScaledDimensions(m_configManager.get(), m_displayWidth, m_displayHeight, 2);
                } break;

                case config::ScalingType::None:
                    break;

                default:
                    Log("Unknown upscaling type, falling back to no upscaling\n");
                    break;
                }

                // Override the recommended image size to account for scaling.
                for (uint32_t i = 0; i < *viewCountOutput; i++) {
                    views[i].recommendedImageRectWidth = inputWidth;
                    views[i].recommendedImageRectHeight = inputHeight;
                }

                if (inputWidth != m_displayWidth || inputHeight != m_displayHeight) {
                    Log("Upscaling from %ux%u to %ux%u (%u%%)\n",
                        inputWidth,
                        inputHeight,
                        m_displayWidth,
                        m_displayHeight,
                        (unsigned int)((((float)m_displayWidth / inputWidth) + 0.001f) * 100));
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
                const XrBaseInStructure* entry = reinterpret_cast<const XrBaseInStructure*>(createInfo->next);
                while (entry) {
                    if (entry->type == XR_TYPE_GRAPHICS_BINDING_D3D11_KHR) {
                        const XrGraphicsBindingD3D11KHR* d3dBindings =
                            reinterpret_cast<const XrGraphicsBindingD3D11KHR*>(entry);
                        // Workaround: On Oculus, we must delay the initialization of Detour.
                        const bool enableOculusQuirk = m_runtimeName.find("Oculus") != std::string::npos;
                        m_graphicsDevice =
                            graphics::WrapD3D11Device(d3dBindings->device, m_configManager, enableOculusQuirk);
                        break;
                    } else if (entry->type == XR_TYPE_GRAPHICS_BINDING_D3D12_KHR) {
                        const XrGraphicsBindingD3D12KHR* d3dBindings =
                            reinterpret_cast<const XrGraphicsBindingD3D12KHR*>(entry);
                        m_graphicsDevice =
                            graphics::WrapD3D12Device(d3dBindings->device, d3dBindings->queue, m_configManager);
                        break;
                    }

                    entry = entry->next;
                }

                if (m_graphicsDevice) {
                    // Initialize the other resources.

                    m_upscaleMode = m_configManager->getEnumValue<config::ScalingType>(config::SettingScalingType);

                    switch (m_upscaleMode) {
                    case config::ScalingType::FSR:
                        m_imageProcessors[ImgProc::Scale] = graphics::CreateFSRUpscaler(
                            m_configManager, m_graphicsDevice, m_displayWidth, m_displayHeight);
                        break;

                    case config::ScalingType::NIS:
                        m_imageProcessors[ImgProc::Scale] = graphics::CreateNISUpscaler(
                            m_configManager, m_graphicsDevice, m_displayWidth, m_displayHeight);
                        break;

                    case config::ScalingType::None:
                        break;

                    default:
                        Log("Unknown upscaling type, falling back to no upscaling\n");
                        m_upscaleMode = config::ScalingType::None;
                        break;
                    }

                    uint32_t renderWidth = m_displayWidth;
                    uint32_t renderHeight = m_displayHeight;
                    if (m_upscaleMode != config::ScalingType::None) {
                        
                        std::tie(renderWidth, renderHeight) =
                            config::GetScaledDimensions(m_configManager.get(), m_displayWidth, m_displayHeight, 2);

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
                                if (!m_isInFrame) {
                                    return;
                                }

                                if (m_frameAnalyzer) {
                                    m_frameAnalyzer->onSetRenderTarget(context, renderTarget);
                                    const auto& eyeHint = m_frameAnalyzer->getEyeHint();
                                    if (eyeHint.has_value()) {
                                        m_stats.hasColorBuffer[(int)eyeHint.value()] = true;
                                    }
                                }
                                if (m_variableRateShader) {
                                    if (m_variableRateShader->onSetRenderTarget(
                                            context,
                                            renderTarget,
                                            m_frameAnalyzer ? m_frameAnalyzer->getEyeHint() : std::nullopt)) {
                                        m_stats.numRenderTargetsWithVRS++;
                                    }
                                }
                            });
                        m_graphicsDevice->registerUnsetRenderTargetEvent(
                            [&](std::shared_ptr<graphics::IContext> context) {
                                if (!m_isInFrame) {
                                    return;
                                }

                                if (m_frameAnalyzer) {
                                    m_frameAnalyzer->onUnsetRenderTarget(context);
                                }
                                if (m_variableRateShader) {
                                    m_variableRateShader->onUnsetRenderTarget(context);
                                }
                            });
                        m_graphicsDevice->registerCopyTextureEvent([&](std::shared_ptr<graphics::IContext> context,
                                                                       std::shared_ptr<graphics::ITexture> source,
                                                                       std::shared_ptr<graphics::ITexture> destination,
                                                                       int sourceSlice,
                                                                       int destinationSlice) {
                            if (!m_isInFrame) {
                                return;
                            }

                            if (m_frameAnalyzer) {
                                m_frameAnalyzer->onCopyTexture(source, destination, sourceSlice, destinationSlice);
                            }
                        });
                    }

                    m_imageProcessors[ImgProc::Post] =
                        graphics::CreateImageProcessor(m_configManager, m_graphicsDevice, m_variableRateShader);

                    m_performanceCounters.appCpuTimer = utilities::CreateCpuTimer();
                    m_performanceCounters.waitCpuTimer = utilities::CreateCpuTimer();
                    m_performanceCounters.endFrameCpuTimer = utilities::CreateCpuTimer();
                    m_performanceCounters.overlayCpuTimer = utilities::CreateCpuTimer();
                    m_performanceCounters.handTrackingTimer = utilities::CreateCpuTimer();

                    for (unsigned int i = 0; i <= GpuTimerLatency; i++) {
                        m_performanceCounters.appGpuTimer[i] = m_graphicsDevice->createTimer();
                        m_performanceCounters.overlayGpuTimer[i] = m_graphicsDevice->createTimer();
                    }

                    m_performanceCounters.lastWindowStart = std::chrono::steady_clock::now();

                    {
                        uint32_t formatCount = 0;
                        CHECK_XRCMD(xrEnumerateSwapchainFormats(*session, 0, &formatCount, nullptr));
                        std::vector<int64_t> formats(formatCount);
                        CHECK_XRCMD(xrEnumerateSwapchainFormats(*session, formatCount, &formatCount, formats.data()));

                        XrSwapchainCreateInfo swapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
                        swapchainInfo.width = swapchainInfo.height =
                            2000; // Let's hope the menu doesn't get bigger than that.
                        swapchainInfo.arraySize = 1;
                        swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
                        swapchainInfo.format = formats[0];
                        swapchainInfo.sampleCount = 1;
                        swapchainInfo.faceCount = 1;
                        swapchainInfo.mipCount = 1;
                        CHECK_XRCMD(OpenXrApi::xrCreateSwapchain(*session, &swapchainInfo, &m_menuSwapchain));

                        uint32_t imageCount;
                        CHECK_XRCMD(OpenXrApi::xrEnumerateSwapchainImages(m_menuSwapchain, 0, &imageCount, nullptr));

                        SwapchainState swapchainState;
                        int64_t overrideFormat = 0;
                        if (m_graphicsDevice->getApi() == graphics::Api::D3D11) {
                            std::vector<XrSwapchainImageD3D11KHR> d3dImages(imageCount,
                                                                            {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
                            CHECK_XRCMD(OpenXrApi::xrEnumerateSwapchainImages(
                                m_menuSwapchain,
                                imageCount,
                                &imageCount,
                                reinterpret_cast<XrSwapchainImageBaseHeader*>(d3dImages.data())));

                            for (uint32_t i = 0; i < imageCount; i++) {
                                m_menuSwapchainImages.push_back(
                                    graphics::WrapD3D11Texture(m_graphicsDevice,
                                                               swapchainInfo,
                                                               d3dImages[i].texture,
                                                               fmt::format("Menu swapchain {} TEX2D", i)));
                            }
                        } else if (m_graphicsDevice->getApi() == graphics::Api::D3D12) {
                            std::vector<XrSwapchainImageD3D12KHR> d3dImages(imageCount,
                                                                            {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
                            CHECK_XRCMD(OpenXrApi::xrEnumerateSwapchainImages(
                                m_menuSwapchain,
                                imageCount,
                                &imageCount,
                                reinterpret_cast<XrSwapchainImageBaseHeader*>(d3dImages.data())));

                            for (uint32_t i = 0; i < imageCount; i++) {
                                m_menuSwapchainImages.push_back(
                                    graphics::WrapD3D12Texture(m_graphicsDevice,
                                                               swapchainInfo,
                                                               d3dImages[i].texture,
                                                               fmt::format("Menu swapchain {} TEX2D", i)));
                            }
                        } else {
                            throw std::runtime_error("Unsupported graphics runtime");
                        }
                    }

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
                        XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
                                                                            nullptr};
                        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
                        referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity();
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

                for (unsigned int i = 0; i <= GpuTimerLatency; i++) {
                    m_performanceCounters.appGpuTimer[i].reset();
                    m_performanceCounters.overlayGpuTimer[i].reset();
                }
                m_performanceCounters.appCpuTimer.reset();
                m_performanceCounters.waitCpuTimer.reset();
                m_performanceCounters.endFrameCpuTimer.reset();
                m_performanceCounters.overlayCpuTimer.reset();
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

            // Identify the swapchains of interest for our processing chain.
            const bool useSwapchain =
                createInfo->usageFlags &
                (XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                 XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT);

            TraceLoggingWrite(g_traceProvider,
                              "xrCreateSwapchain_AppSwapchain",
                              TLArg(createInfo->width, "ResolutionX"),
                              TLArg(createInfo->height, "ResolutionY"),
                              TLArg(createInfo->arraySize, "ArraySize"),
                              TLArg(createInfo->mipCount, "MipCount"),
                              TLArg(createInfo->sampleCount, "SampleCount"),
                              TLArg(createInfo->format, "Format"),
                              TLArg(createInfo->usageFlags, "Usage"));
            Log("Creating swapchain with dimensions=%ux%u, arraySize=%u, mipCount=%u, sampleCount=%u, format=%d, "
                "usage=0x%x\n",
                createInfo->width,
                createInfo->height,
                createInfo->arraySize,
                createInfo->mipCount,
                createInfo->sampleCount,
                createInfo->format,
                createInfo->usageFlags);

            XrSwapchainCreateInfo chainCreateInfo = *createInfo;
            if (useSwapchain) {
                // Modify the swapchain to handle our processing chain (eg: change resolution and/or select usage
                // XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT).

                // We do no processing to depth buffers.
                if (!(createInfo->usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
                    if (m_imageProcessors[ImgProc::Pre]) {
                        chainCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
                    }

                    if (m_imageProcessors[ImgProc::Scale]) {
                        // When upscaling, be sure to request the full resolution with the runtime.
                        chainCreateInfo.width = std::max(m_displayWidth, createInfo->width);
                        chainCreateInfo.height = std::max(m_displayHeight, createInfo->height);

                        // The upscaler requires to use as an unordered access view.
                        chainCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;
                    }

                    if (m_imageProcessors[ImgProc::Post]) {
                        // We no longer need the runtime swapchain to have this flag since we will use an intermediate
                        // texture.
                        chainCreateInfo.usageFlags &= ~XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;

                        chainCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
                    }
                }
            }

            const XrResult result = OpenXrApi::xrCreateSwapchain(session, &chainCreateInfo, swapchain);
            if (XR_SUCCEEDED(result) && useSwapchain) {
                uint32_t imageCount;
                CHECK_XRCMD(OpenXrApi::xrEnumerateSwapchainImages(*swapchain, 0, &imageCount, nullptr));

                SwapchainState swapchainState;
                int64_t overrideFormat = 0;
                if (m_graphicsDevice->getApi() == graphics::Api::D3D11) {
                    std::vector<XrSwapchainImageD3D11KHR> d3dImages(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
                    CHECK_XRCMD(OpenXrApi::xrEnumerateSwapchainImages(
                        *swapchain,
                        imageCount,
                        &imageCount,
                        reinterpret_cast<XrSwapchainImageBaseHeader*>(d3dImages.data())));

                    // Dump the descriptor for the first texture returned by the runtime for debug purposes.
                    {
                        D3D11_TEXTURE2D_DESC desc;
                        d3dImages[0].texture->GetDesc(&desc);
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

                        // Make sure to create the underlying texture typeless.
                        overrideFormat = (int64_t)desc.Format;
                    }

                    for (uint32_t i = 0; i < imageCount; i++) {
                        SwapchainImages images;

                        // Store the runtime images into the state (last entry in the processing chain).
                        images.chain.push_back(
                            graphics::WrapD3D11Texture(m_graphicsDevice,
                                                       chainCreateInfo,
                                                       d3dImages[i].texture,
                                                       fmt::format("Runtime swapchain {} TEX2D", i)));

                        swapchainState.images.push_back(std::move(images));
                    }
                } else if (m_graphicsDevice->getApi() == graphics::Api::D3D12) {
                    std::vector<XrSwapchainImageD3D12KHR> d3dImages(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
                    CHECK_XRCMD(OpenXrApi::xrEnumerateSwapchainImages(
                        *swapchain,
                        imageCount,
                        &imageCount,
                        reinterpret_cast<XrSwapchainImageBaseHeader*>(d3dImages.data())));

                    // Dump the descriptor for the first texture returned by the runtime for debug purposes.
                    {
                        const auto& desc = d3dImages[0].texture->GetDesc();
                        TraceLoggingWrite(g_traceProvider,
                                          "xrCreateSwapchain_RuntimeSwapchain",
                                          TLArg(desc.Width, "ResolutionX"),
                                          TLArg(desc.Height, "ResolutionY"),
                                          TLArg(desc.DepthOrArraySize, "ArraySize"),
                                          TLArg(desc.MipLevels, "MipCount"),
                                          TLArg(desc.SampleDesc.Count, "SampleCount"),
                                          TLArg((int)desc.Format, "Format"),
                                          TLArg((int)desc.Flags, "Flags"));

                        // Make sure to create the underlying texture typeless.
                        overrideFormat = (int64_t)desc.Format;
                    }

                    for (uint32_t i = 0; i < imageCount; i++) {
                        SwapchainImages images;

                        // Store the runtime images into the state (last entry in the processing chain).
                        images.chain.push_back(
                            graphics::WrapD3D12Texture(m_graphicsDevice,
                                                       chainCreateInfo,
                                                       d3dImages[i].texture,
                                                       fmt::format("Runtime swapchain {} TEX2D", i)));

                        swapchainState.images.push_back(std::move(images));
                    }
                } else {
                    throw std::runtime_error("Unsupported graphics runtime");
                }

                for (uint32_t i = 0; i < imageCount; i++) {
                    SwapchainImages& images = swapchainState.images[i];

                    // We do no processing to depth buffers.
                    if (createInfo->usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                        continue;
                    }

                    // Create other entries in the chain based on the processing to do (scaling,
                    // post-processing...).

                    if (m_imageProcessors[ImgProc::Pre]) {
                        // Create an intermediate texture with the same resolution as the input.
                        XrSwapchainCreateInfo inputCreateInfo = *createInfo;
                        inputCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
                        if (m_imageProcessors[ImgProc::Scale]) {
                            // The upscaler requires to use as a shader input.
                            inputCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
                        }

                        auto inputTexture = m_graphicsDevice->createTexture(
                            inputCreateInfo, fmt::format("Postprocess input swapchain {} TEX2D", i), overrideFormat);

                        // We place the texture at the very front (app texture).
                        images.chain.insert(images.chain.begin(), inputTexture);

                        images.gpuTimers[ImgProc::Pre][0] = m_graphicsDevice->createTimer();
                        if (createInfo->arraySize > 1) {
                            images.gpuTimers[ImgProc::Pre][1] = m_graphicsDevice->createTimer();
                        }
                    }

                    if (m_imageProcessors[ImgProc::Scale]) {
                        // Create an app texture with the lower resolution.
                        XrSwapchainCreateInfo inputCreateInfo = *createInfo;
                        inputCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
                        auto inputTexture = m_graphicsDevice->createTexture(
                            inputCreateInfo, fmt::format("App swapchain {} TEX2D", i), overrideFormat);

                        // We place the texture before the runtime texture, which means at the very front (app
                        // texture) or after the pre-processor.
                        images.chain.insert(images.chain.end() - 1, inputTexture);

                        images.gpuTimers[ImgProc::Scale][0] = m_graphicsDevice->createTimer();
                        if (createInfo->arraySize > 1) {
                            images.gpuTimers[ImgProc::Scale][1] = m_graphicsDevice->createTimer();
                        }
                    }

                    if (m_imageProcessors[ImgProc::Post]) {
                        // Create an intermediate texture with the same resolution as the output.
                        XrSwapchainCreateInfo intermediateCreateInfo = chainCreateInfo;
                        intermediateCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
                        if (m_imageProcessors[ImgProc::Scale]) {
                            // The upscaler requires to use as an unordered access view.
                            intermediateCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;

                            // This also means we need a non-sRGB type.
                            if (m_graphicsDevice->isTextureFormatSRGB(intermediateCreateInfo.format)) {
                                // good balance between visuals and perf
                                intermediateCreateInfo.format =
                                    m_graphicsDevice->getTextureFormat(graphics::TextureFormat::R10G10B10A2_UNORM);
                            }

                            // Don't override. This isn't the texture the app is going to see anyway.
                            overrideFormat = 0;
                        }
                        auto intermediateTexture =
                            m_graphicsDevice->createTexture(intermediateCreateInfo,
                                                            fmt::format("Postprocess input swapchain {} TEX2D", i),
                                                            overrideFormat);

                        // We place the texture just before the runtime texture.
                        images.chain.insert(images.chain.end() - 1, intermediateTexture);

                        images.gpuTimers[ImgProc::Post][0] = m_graphicsDevice->createTimer();
                        if (createInfo->arraySize > 1) {
                            images.gpuTimers[ImgProc::Post][1] = m_graphicsDevice->createTimer();
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
                    getPath(suggestedBindings->interactionProfile) !=
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
            if (XR_SUCCEEDED(result) && m_handTracker) {
                m_handTracker->registerAction(*action, actionSet);
            }

            return result;
        }

        XrResult xrDestroyAction(XrAction action) override {
            const XrResult result = OpenXrApi::xrDestroyAction(action);
            if (XR_SUCCEEDED(result) && m_handTracker) {
                m_handTracker->unregisterAction(action);
            }

            return result;
        }

        XrResult xrCreateActionSpace(XrSession session,
                                     const XrActionSpaceCreateInfo* createInfo,
                                     XrSpace* space) override {
            const XrResult result = OpenXrApi::xrCreateActionSpace(session, createInfo, space);
            if (XR_SUCCEEDED(result) && m_handTracker && isVrSession(session)) {
                // Keep track of the XrSpace for controllers, so we can override the behavior for them.
                const std::string fullPath = m_handTracker->getFullPath(createInfo->action, createInfo->subactionPath);
                if (fullPath == "/user/hand/right/input/grip/pose" || fullPath == "/user/hand/right/input/aim/pose" ||
                    fullPath == "/user/hand/left/input/grip/pose" || fullPath == "/user/hand/left/input/aim/pose") {
                    m_handTracker->registerActionSpace(*space, fullPath, createInfo->poseInActionSpace);
                }
            }

            return result;
        }

        XrResult xrDestroySpace(XrSpace space) override {
            const XrResult result = OpenXrApi::xrDestroySpace(space);
            if (XR_SUCCEEDED(result) && m_handTracker) {
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
                auto swapchainIt = m_swapchains.find(swapchain);
                if (swapchainIt != m_swapchains.end()) {
                    auto& swapchainState = swapchainIt->second;

                    // Return the application texture (first entry in the processing chain).
                    if (m_graphicsDevice->getApi() == graphics::Api::D3D11) {
                        XrSwapchainImageD3D11KHR* d3dImages = reinterpret_cast<XrSwapchainImageD3D11KHR*>(images);
                        for (uint32_t i = 0; i < *imageCountOutput; i++) {
                            d3dImages[i].texture = swapchainState.images[i].chain[0]->getAs<graphics::D3D11>();
                        }
                    } else if (m_graphicsDevice->getApi() == graphics::Api::D3D12) {
                        XrSwapchainImageD3D12KHR* d3dImages = reinterpret_cast<XrSwapchainImageD3D12KHR*>(images);
                        for (uint32_t i = 0; i < *imageCountOutput; i++) {
                            d3dImages[i].texture = swapchainState.images[i].chain[0]->getAs<graphics::D3D12>();
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
            auto swapchainIt = m_swapchains.find(swapchain);
            if (swapchainIt != m_swapchains.end()) {
                // Perform the release now in case it was delayed. This could happen for a discarded frame.
                if (swapchainIt->second.delayedRelease) {
                    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, nullptr};
                    swapchainIt->second.delayedRelease = false;
                    CHECK_XRCMD(OpenXrApi::xrReleaseSwapchainImage(swapchain, &releaseInfo));
                }
            }

            const XrResult result = OpenXrApi::xrAcquireSwapchainImage(swapchain, acquireInfo, index);
            if (XR_SUCCEEDED(result)) {
                // Record the index so we know which texture to use in xrEndFrame().
                if (swapchainIt != m_swapchains.end()) {
                    swapchainIt->second.acquiredImageIndex = *index;
                }
            }

            return result;
        }

        XrResult xrReleaseSwapchainImage(XrSwapchain swapchain,
                                         const XrSwapchainImageReleaseInfo* releaseInfo) override {
            auto swapchainIt = m_swapchains.find(swapchain);
            if (swapchainIt != m_swapchains.end()) {
                // Perform a delayed release: we still need to write to the swapchain in our xrEndFrame()!
                swapchainIt->second.delayedRelease = true;
                return XR_SUCCESS;
            }

            return OpenXrApi::xrReleaseSwapchainImage(swapchain, releaseInfo);
        }

        XrResult xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData) override {
            if (m_sendInterationProfileEvent && m_vrSession != XR_NULL_HANDLE) {
                XrEventDataInteractionProfileChanged* const buffer =
                    reinterpret_cast<XrEventDataInteractionProfileChanged*>(eventData);
                buffer->type = XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED;
                buffer->next = nullptr;
                buffer->session = m_vrSession;

                m_sendInterationProfileEvent = false;
                return XR_SUCCESS;
            }

            if (m_visibilityMaskEventIndex != utilities::ViewCount && m_vrSession != XR_NULL_HANDLE) {
                XrEventDataVisibilityMaskChangedKHR* const buffer =
                    reinterpret_cast<XrEventDataVisibilityMaskChangedKHR*>(eventData);
                buffer->type = XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR;
                buffer->next = nullptr;
                buffer->session = m_vrSession;
                buffer->viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                buffer->viewIndex = m_visibilityMaskEventIndex++;
                Log("Send XrEventDataVisibilityMaskChangedKHR event for view %u\n", buffer->viewIndex);
                return XR_SUCCESS;
            }

            return OpenXrApi::xrPollEvent(instance, eventData);
        }

        XrResult xrGetCurrentInteractionProfile(XrSession session,
                                                XrPath topLevelUserPath,
                                                XrInteractionProfileState* interactionProfile) override {
            std::string path = topLevelUserPath != XR_NULL_PATH ? getPath(topLevelUserPath) : "";
            if (m_handTracker && isVrSession(session) &&
                (path.empty() || path == "/user/hand/left" || path == "/user/hand/right") &&
                interactionProfile->type == XR_TYPE_INTERACTION_PROFILE_STATE) {
                // Return our emulated interaction profile for the hands.
                interactionProfile->interactionProfile = m_handTracker->getInteractionProfile();
                return XR_SUCCESS;
            }

            return OpenXrApi::xrGetCurrentInteractionProfile(session, topLevelUserPath, interactionProfile);
        }

        XrResult xrGetVisibilityMaskKHR(XrSession session,
                                        XrViewConfigurationType viewConfigurationType,
                                        uint32_t viewIndex,
                                        XrVisibilityMaskTypeKHR visibilityMaskType,
                                        XrVisibilityMaskKHR* visibilityMask) {
            // When doing the Pimax FOV hack, we swap left and right eyes.
            if (m_supportFOVHack && isVrSession(session) && m_configManager->peekValue(config::SettingPimaxFOVHack)) {
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
                assert(*viewCountOutput == utilities::ViewCount);
                using namespace DirectX;

                m_posesForFrame[0].pose = views[0].pose;
                m_posesForFrame[1].pose = views[1].pose;

                // Override the canting angle if requested.
                const int cantOverride = m_configManager->getValue("canting");
                if (cantOverride != 0) {
                    const float angle = (float)(cantOverride * (M_PI / 180));

                    StoreXrPose(&views[0].pose,
                                XMMatrixMultiply(LoadXrPose(views[0].pose),
                                                 XMMatrixRotationRollPitchYaw(0.f, -angle / 2.f, 0.f)));
                    StoreXrPose(&views[1].pose,
                                XMMatrixMultiply(LoadXrPose(views[1].pose),
                                                 XMMatrixRotationRollPitchYaw(0.f, angle / 2.f, 0.f)));
                }

                // Calibrate the projection center for each eye.
                if (m_needCalibrateEyeProjections) {
                    XrViewLocateInfo info = *viewLocateInfo;
                    info.space = m_viewSpace;

                    XrViewState state{XR_TYPE_VIEW_STATE, nullptr};
                    XrView eyeInViewSpace[2] = {{XR_TYPE_VIEW, nullptr}, {XR_TYPE_VIEW, nullptr}};
                    CHECK_HRCMD(OpenXrApi::xrLocateViews(
                        session, &info, &state, viewCapacityInput, viewCountOutput, eyeInViewSpace));

                    if (Pose::IsPoseValid(state.viewStateFlags)) {
                        XMFLOAT4X4 leftView, rightView;
                        XMStoreFloat4x4(&leftView, LoadXrPose(eyeInViewSpace[0].pose));
                        XMStoreFloat4x4(&rightView, LoadXrPose(eyeInViewSpace[1].pose));

                        // This code is based on vrperfkit by Frydrych Holger.
                        // https://github.com/fholger/vrperfkit/blob/master/src/openvr/openvr_manager.cpp
                        const float dotForward = leftView.m[2][0] * rightView.m[2][0] +
                                                 leftView.m[2][1] * rightView.m[2][1] +
                                                 leftView.m[2][2] * rightView.m[2][2];

                        // In normalized screen coordinates.
                        for (uint32_t eye = 0; eye < utilities::ViewCount; eye++) {
                            const auto& fov = eyeInViewSpace[eye].fov;
                            const float cantedAngle = std::abs(std::acosf(dotForward) / 2) * (eye ? -1 : 1);
                            const float canted = std::tanf(cantedAngle);
                            m_projCenters[eye].x =
                                (fov.angleRight + fov.angleLeft - 2 * canted) / (fov.angleLeft - fov.angleRight);
                            m_projCenters[eye].y = -(fov.angleDown + fov.angleUp) / (fov.angleUp - fov.angleDown);
                            m_eyeGaze[eye] = m_projCenters[eye];
                        }

                        Log("Projection calibration: %.5f, %.5f | %.5f, %.5f\n",
                            m_projCenters[0].x,
                            m_projCenters[0].y,
                            m_projCenters[1].x,
                            m_projCenters[1].y);

                        if (m_variableRateShader) {
                            m_variableRateShader->setViewProjectionCenters(m_projCenters[0], m_projCenters[1]);
                        }

                        m_needCalibrateEyeProjections = false;
                    }
                }

                const auto vec = views[1].pose.position - views[0].pose.position;
                const auto ipd = Length(vec);

                // Override the ICD if requested.
                const int icdOverride = m_configManager->getValue(config::SettingICD);
                if (icdOverride != 1000) {
                    const float icd = (ipd * 1000) / std::max(icdOverride, 1);
                    const auto center = views[0].pose.position + (vec * 0.5f);
                    const auto offset = Normalize(vec) * (icd * 0.5f);
                    views[0].pose.position = center - offset;
                    views[1].pose.position = center + offset;
                    m_stats.icd = icd;

                } else {
                    m_stats.icd = ipd;
                }

                // Override the FOV if requested.
                if (m_configManager->getValue(config::SettingFOVType) == 0) {
                    const auto fovOverride = m_configManager->getValue(config::SettingFOV);
                    if (fovOverride != 100) {
                        StoreXrFov(&views[0].fov, LoadXrFov(views[0].fov) * XMVectorReplicate(fovOverride * 0.01f));
                        StoreXrFov(&views[1].fov, LoadXrFov(views[1].fov) * XMVectorReplicate(fovOverride * 0.01f));
                    }
                } else {
                    // XrFovF layout is: L,R,U,D
                    const auto fov1 = XMINT4(m_configManager->getValue(config::SettingFOVLeftLeft),
                                             m_configManager->getValue(config::SettingFOVLeftRight),
                                             m_configManager->getValue(config::SettingFOVUp),
                                             m_configManager->getValue(config::SettingFOVDown));

                    const auto fov2 = XMINT4(m_configManager->getValue(config::SettingFOVRightLeft),
                                             m_configManager->getValue(config::SettingFOVRightRight),
                                             fov1.z,
                                             fov1.w);

                    StoreXrFov(&views[0].fov, LoadXrFov(views[0].fov) * XMLoadSInt4(&fov1) * XMVectorReplicate(0.01f));
                    StoreXrFov(&views[1].fov, LoadXrFov(views[1].fov) * XMLoadSInt4(&fov2) * XMVectorReplicate(0.01f));
                }

                StoreXrFov(&m_stats.fov[0], ConvertToDegrees(views[0].fov));
                StoreXrFov(&m_stats.fov[1], ConvertToDegrees(views[1].fov));

                m_posesForFrame[0].fov = views[0].fov;
                m_posesForFrame[1].fov = views[1].fov;

                // Apply zoom if requested.
                const auto zoom = m_configManager->getValue(config::SettingZoom);
                if (zoom != 10) {
                    StoreXrFov(&views[0].fov, LoadXrFov(views[0].fov) * XMVectorReplicate(1.f / (zoom * 0.1f)));
                    StoreXrFov(&views[1].fov, LoadXrFov(views[1].fov) * XMVectorReplicate(1.f / (zoom * 0.1f)));
                }

                // When doing the Pimax FOV hack, we swap left and right eyes.
                if (m_supportFOVHack && m_configManager->hasChanged(config::SettingPimaxFOVHack)) {
                    // Send the necessary events to the app.
                    m_visibilityMaskEventIndex = 0;
                }
                if (m_supportFOVHack && m_configManager->getValue(config::SettingPimaxFOVHack)) {
                    std::swap(views[0], views[1]);
                }
            }

            return result;
        }

        XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location) override {
            if (m_handTracker && location->type == XR_TYPE_SPACE_LOCATION) {
                m_performanceCounters.handTrackingTimer->start();
                if (m_handTracker->locate(space, baseSpace, time, getTimeNow(), *location)) {
                    m_performanceCounters.handTrackingTimer->stop();
                    m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer->query();
                    return XR_SUCCESS;
                }
            }

            return OpenXrApi::xrLocateSpace(space, baseSpace, time, location);
        }

        XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo) override {
            const XrResult result = OpenXrApi::xrSyncActions(session, syncInfo);
            if (XR_SUCCEEDED(result) && m_handTracker && isVrSession(session)) {
                m_performanceCounters.handTrackingTimer->start();

                m_handTracker->sync(m_begunFrameTime, getTimeNow(), *syncInfo);

                m_performanceCounters.handTrackingTimer->stop();
                m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer->query();
            }

            return result;
        }

        XrResult xrGetActionStateBoolean(XrSession session,
                                         const XrActionStateGetInfo* getInfo,
                                         XrActionStateBoolean* state) override {
            if (m_handTracker && isVrSession(session) && getInfo->type == XR_TYPE_ACTION_STATE_GET_INFO &&
                state->type == XR_TYPE_ACTION_STATE_BOOLEAN) {
                m_performanceCounters.handTrackingTimer->start();
                if (m_handTracker->getActionState(*getInfo, *state)) {
                    m_performanceCounters.handTrackingTimer->stop();
                    m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer->query();
                    return XR_SUCCESS;
                }
            }

            return OpenXrApi::xrGetActionStateBoolean(session, getInfo, state);
        }

        XrResult xrGetActionStateFloat(XrSession session,
                                       const XrActionStateGetInfo* getInfo,
                                       XrActionStateFloat* state) override {
            if (m_handTracker && isVrSession(session) && getInfo->type == XR_TYPE_ACTION_STATE_GET_INFO &&
                state->type == XR_TYPE_ACTION_STATE_FLOAT) {
                m_performanceCounters.handTrackingTimer->start();
                if (m_handTracker->getActionState(*getInfo, *state)) {
                    m_performanceCounters.handTrackingTimer->stop();
                    m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer->query();
                    return XR_SUCCESS;
                }
            }

            return OpenXrApi::xrGetActionStateFloat(session, getInfo, state);
        }

        XrResult xrGetActionStatePose(XrSession session,
                                      const XrActionStateGetInfo* getInfo,
                                      XrActionStatePose* state) override {
            if (m_handTracker && isVrSession(session) && getInfo->type == XR_TYPE_ACTION_STATE_GET_INFO &&
                state->type == XR_TYPE_ACTION_STATE_POSE) {
                m_performanceCounters.handTrackingTimer->start();
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
                    m_performanceCounters.handTrackingTimer->stop();
                    m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer->query();
                    return XR_SUCCESS;
                }
            }

            return OpenXrApi::xrGetActionStatePose(session, getInfo, state);
        }

        XrResult xrApplyHapticFeedback(XrSession session,
                                       const XrHapticActionInfo* hapticActionInfo,
                                       const XrHapticBaseHeader* hapticFeedback) override {
            if (m_handTracker && isVrSession(session) && hapticActionInfo->type == XR_TYPE_HAPTIC_ACTION_INFO &&
                hapticFeedback->type == XR_TYPE_HAPTIC_VIBRATION) {
                m_performanceCounters.handTrackingTimer->start();
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
                    m_performanceCounters.handTrackingTimer->stop();
                    m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer->query();
                }
            }

            return OpenXrApi::xrApplyHapticFeedback(session, hapticActionInfo, hapticFeedback);
        }

        XrResult xrStopHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo) override {
            if (m_handTracker && isVrSession(session) && hapticActionInfo->type == XR_TYPE_HAPTIC_ACTION_INFO) {
                m_performanceCounters.handTrackingTimer->start();
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
                    m_performanceCounters.handTrackingTimer->stop();
                    m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer->query();
                }
            }

            return OpenXrApi::xrStopHapticFeedback(session, hapticActionInfo);
        }

        XrResult xrWaitFrame(XrSession session,
                             const XrFrameWaitInfo* frameWaitInfo,
                             XrFrameState* frameState) override {
            if (isVrSession(session)) {
                m_performanceCounters.waitCpuTimer->start();
            }
            const XrResult result = OpenXrApi::xrWaitFrame(session, frameWaitInfo, frameState);
            if (XR_SUCCEEDED(result) && isVrSession(session)) {
                m_performanceCounters.waitCpuTimer->stop();
                m_stats.waitCpuTimeUs += m_performanceCounters.waitCpuTimer->query();

                // Apply prediction dampening if possible and if needed.
                if (xrConvertWin32PerformanceCounterToTimeKHR) {
                    const int predictionDampen = m_configManager->getValue(config::SettingPredictionDampen);
                    if (predictionDampen != 100) {
                        // Find the current time.
                        LARGE_INTEGER qpcTimeNow;
                        QueryPerformanceCounter(&qpcTimeNow);

                        XrTime xrTimeNow;
                        CHECK_XRCMD(
                            xrConvertWin32PerformanceCounterToTimeKHR(GetXrInstance(), &qpcTimeNow, &xrTimeNow));

                        XrTime predictionAmount = frameState->predictedDisplayTime - xrTimeNow;
                        if (predictionAmount > 0) {
                            frameState->predictedDisplayTime = xrTimeNow + (predictionDampen * predictionAmount) / 100;
                        }

                        m_stats.predictionTimeUs += predictionAmount;
                    }
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
                    m_performanceCounters.appCpuTimer->start();
                    m_stats.appGpuTimeUs +=
                        m_performanceCounters.appGpuTimer[m_performanceCounters.gpuTimerIndex]->query();
                    m_performanceCounters.appGpuTimer[m_performanceCounters.gpuTimerIndex]->start();

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

        void updateStatisticsForFrame() {
            const auto now = std::chrono::steady_clock::now();
            const auto numFrames = ++m_performanceCounters.numFrames;

            if (m_graphicsDevice) {
                m_stats.numBiasedSamplers = m_graphicsDevice->getNumBiasedSamplersThisFrame();
            }

            if ((now - m_performanceCounters.lastWindowStart) >= std::chrono::seconds(1)) {
                m_performanceCounters.numFrames = 0;
                m_performanceCounters.lastWindowStart = now;

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
                if (m_graphicsDevice->getApi() == graphics::Api::D3D12 &&
                    m_stats.appCpuTimeUs + 500 > m_stats.appGpuTimeUs) {
                    m_stats.appGpuTimeUs = 0;
                }

                if (m_menuHandler) {
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

            for (unsigned int eye = 0; eye < utilities::ViewCount; eye++) {
                m_stats.hasColorBuffer[eye] = m_stats.hasDepthBuffer[eye] = false;
            }
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

            // Update image processors second.
            for (auto& processor : m_imageProcessors) {
                if (processor) {
                    if (reloadShaders)
                        processor->reload();
                    processor->update();
                }
            }
        }

        void takeScreenshot(std::shared_ptr<graphics::ITexture> texture, const std::string& suffix) const {
            SYSTEMTIME st;
            ::GetLocalTime(&st);

            std::stringstream parameters;
            parameters << '_' << ((st.wYear * 10000u) + (st.wMonth * 100u) + (st.wDay)) << '_'
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

            const auto fileFormat =
                m_configManager->getEnumValue<config::ScreenshotFileFormat>(config::SettingScreenshotFileFormat);

            const auto fileExtension = fileFormat == config::ScreenshotFileFormat::DDS   ? ".dds"
                                       : fileFormat == config::ScreenshotFileFormat::JPG ? ".jpg"
                                       : fileFormat == config::ScreenshotFileFormat::BMP ? ".bmp"
                                                                                         : ".png";
            // Using std::filesystem automatically filters out unwanted app name chars.
            auto path = localAppData / "screenshots" / (m_applicationName + parameters.str());
            path.replace_extension(fileExtension);

            texture->saveToFile(path);
        }

        XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) override {
            if (!isVrSession(session) || !m_graphicsDevice) {
                return OpenXrApi::xrEndFrame(session, frameEndInfo);
            }

            m_isInFrame = false;

            updateStatisticsForFrame();

            m_performanceCounters.appCpuTimer->stop();
            m_stats.appCpuTimeUs += m_performanceCounters.appCpuTimer->query();
            m_performanceCounters.appGpuTimer[m_performanceCounters.gpuTimerIndex]->stop();

            m_stats.endFrameCpuTimeUs += m_performanceCounters.endFrameCpuTimer->query();
            m_performanceCounters.endFrameCpuTimer->start();

            // Toggle to the next set of GPU timers.
            m_performanceCounters.gpuTimerIndex = (m_performanceCounters.gpuTimerIndex + 1) % (GpuTimerLatency + 1);
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

            m_graphicsDevice->saveContext();

            // Handle inputs.
            if (m_menuHandler) {
                m_menuHandler->handleInput();
            }

            // Prepare the Shaders for rendering.
            updateConfiguration();

            // Unbind all textures from the render targets.
            m_graphicsDevice->unsetRenderTargets();

            std::shared_ptr<graphics::ITexture> textureForOverlay[utilities::ViewCount] = {};
            std::shared_ptr<graphics::ITexture> depthForOverlay[utilities::ViewCount] = {};
            xr::math::ViewProjection viewsForOverlay[utilities::ViewCount];
            XrSpace spaceForOverlay = XR_NULL_HANDLE;

            // Because the frame info is passed const, we are going to need to reconstruct a writable version of it
            // to patch the resolution.
            XrFrameEndInfo chainFrameEndInfo = *frameEndInfo;
            std::vector<const XrCompositionLayerBaseHeader*> correctedLayers;

            std::vector<XrCompositionLayerProjection> layerProjectionAllocator;
            std::vector<std::array<XrCompositionLayerProjectionView, 2>> layerProjectionViewsAllocator;
            XrCompositionLayerQuad layerQuadForMenu{XR_TYPE_COMPOSITION_LAYER_QUAD};

            // We must reserve the underlying storage to keep our pointers stable.
            layerProjectionAllocator.reserve(chainFrameEndInfo.layerCount);
            layerProjectionViewsAllocator.reserve(chainFrameEndInfo.layerCount);

            // Apply the processing chain to all the (supported) layers.
            for (uint32_t i = 0; i < chainFrameEndInfo.layerCount; i++) {
                if (chainFrameEndInfo.layers[i]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
                    const XrCompositionLayerProjection* proj =
                        reinterpret_cast<const XrCompositionLayerProjection*>(chainFrameEndInfo.layers[i]);

                    // To patch the resolution of the layer we need to recreate the whole projection & views
                    // data structures.
                    auto correctedProjectionLayer = &layerProjectionAllocator.emplace_back(*proj);
                    auto correctedProjectionViews = layerProjectionViewsAllocator
                                                        .emplace_back(std::array<XrCompositionLayerProjectionView, 2>(
                                                            {proj->views[0], proj->views[1]}))
                                                        .data();

                    // For VPRT, we need to handle texture arrays.
                    static_assert(utilities::ViewCount == 2);
                    const bool useVPRT = proj->views[0].subImage.swapchain == proj->views[1].subImage.swapchain;
                    // TODO: We need to use subImage.imageArrayIndex instead of assuming 0/left and 1/right.

                    if (useVPRT) {
                        // Assume that we've properly distinguished left/right eyes.
                        m_stats.hasColorBuffer[(int)utilities::Eye::Left] =
                            m_stats.hasColorBuffer[(int)utilities::Eye::Right] = true;
                    }

                    assert(proj->viewCount == utilities::ViewCount);
                    for (uint32_t eye = 0; eye < utilities::ViewCount; eye++) {
                        const XrCompositionLayerProjectionView& view = proj->views[eye];

                        auto swapchainIt = m_swapchains.find(view.subImage.swapchain);
                        if (swapchainIt == m_swapchains.end()) {
                            throw std::runtime_error("Swapchain is not registered");
                        }
                        auto& swapchainState = swapchainIt->second;
                        auto& swapchainImages = swapchainState.images[swapchainState.acquiredImageIndex];
                        uint32_t nextImage = 0;
                        uint32_t lastImage = 0;
                        uint32_t gpuTimerIndex = useVPRT ? eye : 0;

                        // Look for the depth buffer.
                        std::shared_ptr<graphics::ITexture> depthBuffer;
                        NearFar nearFar{0.001f, 100.f};
                        const XrBaseInStructure* entry = reinterpret_cast<const XrBaseInStructure*>(view.next);
                        while (entry) {
                            if (entry->type == XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR) {
                                const XrCompositionLayerDepthInfoKHR* depth =
                                    reinterpret_cast<const XrCompositionLayerDepthInfoKHR*>(entry);
                                // The order of color/depth textures must match.
                                if (depth->subImage.imageArrayIndex == view.subImage.imageArrayIndex) {
                                    auto depthSwapchainIt = m_swapchains.find(depth->subImage.swapchain);
                                    if (depthSwapchainIt == m_swapchains.end()) {
                                        throw std::runtime_error("Swapchain is not registered");
                                    }
                                    auto& depthSwapchainState = depthSwapchainIt->second;

                                    assert(depthSwapchainState.images[depthSwapchainState.acquiredImageIndex]
                                               .chain.size() == 1);
                                    depthBuffer =
                                        depthSwapchainState.images[depthSwapchainState.acquiredImageIndex].chain[0];
                                    nearFar.Near = depth->nearZ;
                                    nearFar.Far = depth->farZ;

                                    m_stats.hasDepthBuffer[eye] = true;
                                }
                                break;
                            }
                            entry = entry->next;
                        }

                        const bool isDepthInverted = nearFar.Far < nearFar.Near;

                        // Now that we know what eye the swapchain is used for, register it.
                        // TODO: We always assume that if VPRT is used, left eye is texture 0 and right eye is
                        // texture 1. I'm sure this holds in like 99% of the applications, but still not very clean to
                        // assume.
                        if (m_frameAnalyzer && !useVPRT && !swapchainState.registeredWithFrameAnalyzer) {
                            for (const auto& image : swapchainState.images) {
                                m_frameAnalyzer->registerColorSwapchainImage(image.chain[0], (utilities::Eye)eye);
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

                        for (size_t i = 0; i < std::size(m_imageProcessors); i++) {
                            if (m_imageProcessors[i]) {
                                auto timer = swapchainImages.gpuTimers[i][gpuTimerIndex].get();
                                m_stats.processorGpuTimeUs[i] += timer->query();

                                nextImage++;
                                timer->start();
                                m_imageProcessors[i]->process(swapchainImages.chain[lastImage],
                                                              swapchainImages.chain[nextImage],
                                                              useVPRT ? eye : -int32_t(eye+1));
                                timer->stop();
                                lastImage++;
                            }
                        }

                        // Make sure the chain was completed.
                        if (nextImage != swapchainImages.chain.size() - 1) {
                            throw std::runtime_error("Processing chain incomplete!");
                        }

                        textureForOverlay[eye] = swapchainImages.chain.back();
                        depthForOverlay[eye] = depthBuffer;

                        // Patch the resolution.
                        if (m_imageProcessors[ImgProc::Scale]) {
                            correctedProjectionViews[eye].subImage.imageRect.extent.width = m_displayWidth;
                            correctedProjectionViews[eye].subImage.imageRect.extent.height = m_displayHeight;
                        }

                        // Patch the eye poses.
                        const int cantOverride = m_configManager->getValue("canting");
                        if (cantOverride != 0) {
                            correctedProjectionViews[eye].pose = m_posesForFrame[eye].pose;
                        }

                        // Patch the FOV.
                        correctedProjectionViews[eye].fov = m_posesForFrame[eye].fov;

                        viewsForOverlay[eye].Pose = correctedProjectionViews[eye].pose;
                        viewsForOverlay[eye].Fov = correctedProjectionViews[eye].fov;
                        viewsForOverlay[eye].NearFar = nearFar;
                    }

                    spaceForOverlay = proj->space;

                    // When doing the Pimax FOV hack, we swap left and right eyes.
                    if (m_supportFOVHack && m_configManager->peekValue(config::SettingPimaxFOVHack)) {
                        std::swap(correctedProjectionViews[0], correctedProjectionViews[1]);
                        std::swap(viewsForOverlay[0], viewsForOverlay[1]);
                        std::swap(textureForOverlay[0], textureForOverlay[1]);
                        std::swap(depthForOverlay[0], depthForOverlay[1]);
                    }

                    correctedProjectionLayer->views = correctedProjectionViews;
                    correctedLayers.push_back(
                        reinterpret_cast<const XrCompositionLayerBaseHeader*>(correctedProjectionLayer));

                } else if (chainFrameEndInfo.layers[i]->type == XR_TYPE_COMPOSITION_LAYER_QUAD) {
                    const XrCompositionLayerQuad* quad =
                        reinterpret_cast<const XrCompositionLayerQuad*>(chainFrameEndInfo.layers[i]);

                    auto swapchainIt = m_swapchains.find(quad->subImage.swapchain);
                    if (swapchainIt == m_swapchains.end()) {
                        throw std::runtime_error("Swapchain is not registered");
                    }

                    auto& swapchainState = swapchainIt->second;
                    auto& swapchainImages = swapchainState.images[swapchainState.acquiredImageIndex];

                    size_t swapChainSize = std::size(swapchainImages.chain);
                    if (swapChainSize > 1)
                        swapchainImages.chain[0]->copyTo(swapchainImages.chain[swapChainSize - 1]);

                    correctedLayers.push_back(chainFrameEndInfo.layers[i]);
                } else {
                    correctedLayers.push_back(chainFrameEndInfo.layers[i]);
                }
            }

            // We intentionally exclude the overlay from this timer, as it has its own separate timer.
            m_performanceCounters.endFrameCpuTimer->stop();

            // Render our overlays.
            {
                const bool drawHands = m_handTracker && m_configManager->peekEnumValue<config::HandTrackingVisibility>(
                                                            config::SettingHandVisibilityAndSkinTone) !=
                                                            config::HandTrackingVisibility::Hidden;
                const bool drawEyeGaze = m_eyeTracker && m_configManager->getValue(config::SettingEyeDebug);
                const bool drawOverlays = m_menuHandler || drawHands || drawEyeGaze;

                if (drawOverlays) {
                    m_stats.overlayCpuTimeUs += m_performanceCounters.overlayCpuTimer->query();
                    m_stats.overlayGpuTimeUs +=
                        m_performanceCounters.overlayGpuTimer[m_performanceCounters.gpuTimerIndex]->query();

                    m_performanceCounters.overlayCpuTimer->start();
                    m_performanceCounters.overlayGpuTimer[m_performanceCounters.gpuTimerIndex]->start();
                }

                if (textureForOverlay[0]) {
                    const bool useVPRT = textureForOverlay[1] == textureForOverlay[0];

                    // Render the hands or eye gaze helper.
                    if (drawHands || drawEyeGaze) {
                        auto isEyeGazeValid = m_eyeTracker && m_eyeTracker->getProjectedGaze(m_eyeGaze);

                        for (uint32_t eye = 0; eye < utilities::ViewCount; eye++) {
                            m_graphicsDevice->setRenderTargets(1,
                                                               &textureForOverlay[eye],
                                                               useVPRT ? reinterpret_cast<int32_t*>(&eye) : nullptr,
                                                               depthForOverlay[eye],
                                                               useVPRT ? eye : -1);

                            m_graphicsDevice->setViewProjection(viewsForOverlay[eye]);

                            if (drawHands) {
                                m_handTracker->render(
                                    viewsForOverlay[eye].Pose, spaceForOverlay, getTimeNow(), textureForOverlay[eye]);
                            }

                            if (drawEyeGaze) {
                                XrColor4f color = isEyeGazeValid ? XrColor4f{0, 1, 0, 1} : XrColor4f{1, 0, 0, 1};
                                auto pos = utilities::NdcToScreen(m_eyeGaze[eye]);
                                pos.x *= textureForOverlay[eye]->getInfo().width;
                                pos.y *= textureForOverlay[eye]->getInfo().height;
                                m_graphicsDevice->clearColor(pos.y - 20, pos.x - 20, pos.y + 20, pos.x + 20, color);
                            }
                        }
                    }
                }

                // Render the menu.
                if (m_menuHandler && (m_menuHandler->isVisible() || m_menuLingering)) {
                    // Workaround: there is a bug in the WMR runtime that causes a past quad layer content to linger on
                    // the next projection layer. We make sure to submit a completely blank quad layer for 3 frames
                    // after its disappearance. The number 3 comes from the number of depth buffers cached inside the
                    // precompositor of the WMR runtime.
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
                    layerQuadForMenu.space = m_viewSpace;
                    StoreXrPose(&layerQuadForMenu.pose,
                                DirectX::XMMatrixMultiply(
                                    DirectX::XMMatrixTranslation(
                                        0, 0, -m_configManager->getValue(config::SettingMenuDistance) / 100.f),
                                    LoadXrPose(Pose::Identity())));
                    layerQuadForMenu.size.width = layerQuadForMenu.size.height = 1; // 1m x 1m
                    static const XrEyeVisibility visibility[] = {
                        XR_EYE_VISIBILITY_BOTH, XR_EYE_VISIBILITY_LEFT, XR_EYE_VISIBILITY_RIGHT};
                    layerQuadForMenu.eyeVisibility = visibility[std::min(
                        m_configManager->getValue(config::SettingMenuEyeVisibility), (int)std::size(visibility))];
                    layerQuadForMenu.subImage.swapchain = m_menuSwapchain;
                    layerQuadForMenu.subImage.imageRect.extent.width = textureInfo.width;
                    layerQuadForMenu.subImage.imageRect.extent.height = textureInfo.height;
                    layerQuadForMenu.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;

                    correctedLayers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layerQuadForMenu));
                }

                if (drawOverlays) {
                    m_performanceCounters.overlayCpuTimer->stop();
                    m_performanceCounters.overlayGpuTimer[m_performanceCounters.gpuTimerIndex]->stop();
                }
            }

            // Whether the menu is available or not, we can still use that top-most texture for screenshot.
            // TODO: The screenshot does not work with multi-layer applications.
            const bool requestScreenshot =
                utilities::UpdateKeyState(m_requestScreenShotKeyState, m_keyModifiers, m_keyScreenshot, false) &&
                m_configManager->getValue(config::SettingScreenshotEnabled);

            if (textureForOverlay[0] && requestScreenshot) {
                // TODO: this is capturing frame N-3
                // review the command queues/lists and context flush
                if (m_configManager->getValue(config::SettingScreenshotEye) != 2 /* Right only */) {
                    takeScreenshot(textureForOverlay[0], "L");
                }
                if (textureForOverlay[1] &&
                    m_configManager->getValue(config::SettingScreenshotEye) != 1 /* Left only */) {
                    takeScreenshot(textureForOverlay[1], "R");
                }

                if (m_variableRateShader && m_configManager->getValue("vrs_capture")) {
                    m_variableRateShader->startCapture();
                }
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

            chainFrameEndInfo.layers = correctedLayers.data();
            chainFrameEndInfo.layerCount = (uint32_t)correctedLayers.size();

            {
                const auto result = OpenXrApi::xrEndFrame(session, &chainFrameEndInfo);

                m_graphicsDevice->unblockCallbacks();

                return result;
            }
        }

      private:
        bool isVrSystem(XrSystemId systemId) const {
            return systemId == m_vrSystemId;
        }

        bool isVrSession(XrSession session) const {
            return session == m_vrSession;
        }

        const std::string getPath(XrPath path) {
            char buf[XR_MAX_PATH_LENGTH];
            uint32_t count;
            CHECK_XRCMD(xrPathToString(GetXrInstance(), path, sizeof(buf), &count, buf));
            std::string str;
            str.assign(buf, count - 1);
            return str;
        }

        // Find the current time. Fallback to the frame time if we cannot query the actual time.
        XrTime getTimeNow() const {
            XrTime xrTimeNow = m_begunFrameTime;
            if (xrConvertWin32PerformanceCounterToTimeKHR) {
                LARGE_INTEGER qpcTimeNow;
                QueryPerformanceCounter(&qpcTimeNow);

                CHECK_XRCMD(xrConvertWin32PerformanceCounterToTimeKHR(GetXrInstance(), &qpcTimeNow, &xrTimeNow));
            }

            return xrTimeNow;
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
        bool m_isOmniceptDetected{false};
        bool m_hasPimaxEyeTracker{false};

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

        struct {
            std::shared_ptr<utilities::ICpuTimer> appCpuTimer;
            std::shared_ptr<graphics::IGpuTimer> appGpuTimer[GpuTimerLatency + 1];
            std::shared_ptr<utilities::ICpuTimer> waitCpuTimer;
            std::shared_ptr<utilities::ICpuTimer> endFrameCpuTimer;
            std::shared_ptr<utilities::ICpuTimer> overlayCpuTimer;
            std::shared_ptr<graphics::IGpuTimer> overlayGpuTimer[GpuTimerLatency + 1];
            std::shared_ptr<utilities::ICpuTimer> handTrackingTimer;

            unsigned int gpuTimerIndex{0};
            std::chrono::steady_clock::time_point lastWindowStart;
            uint32_t numFrames{0};
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
