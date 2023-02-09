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

    struct SwapchainImages {
        std::shared_ptr<graphics::ITexture> appTexture;
        std::shared_ptr<graphics::ITexture> runtimeTexture;
        std::shared_ptr<graphics::IGpuTimer> upscalingTimers[utilities::ViewCount];
        std::shared_ptr<graphics::IGpuTimer> postProcessingTimers[utilities::ViewCount];
    };

    struct SwapchainState {
        std::vector<SwapchainImages> images;
        uint32_t acquiredImageIndex{0};
        bool delayedRelease{false};

        // Intermediate textures for processing.
        std::shared_ptr<graphics::ITexture> nonVPRTInputTexture;
        std::shared_ptr<graphics::ITexture> nonVPRTOutputTexture;
        std::shared_ptr<graphics::ITexture> upscaledTexture;

        // Intermediate textures than can be used for state in the image processors.
        std::vector<std::shared_ptr<graphics::ITexture>> upscalerTextures;
        std::vector<std::shared_ptr<graphics::ITexture>> postProcessorTextures;

        // Opaque blobs of memory that can be used for constant buffer bouncing in the image processors.
        std::array<uint8_t, 1024> upscalerBlob;
        std::array<uint8_t, 1024> postProcessorBlob;

        bool registeredWithFrameAnalyzer{false};
    };

    class OpenXrLayer : public toolkit::OpenXrApi {
      public:
        OpenXrLayer() = default;

        void setOptionsDefaults() {
            m_configManager->setDefault("key_menu_gen", 1);
            m_configManager->setDefault(config::SettingFirstRun, 0);
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
            m_configManager->setDefault(config::SettingMenuEyeOffset, 0);
            m_configManager->setDefault(config::SettingMenuDistance, 100); // 1m
            m_configManager->setDefault(config::SettingMenuOpacity, 85);
            m_configManager->setDefault(config::SettingMenuFontSize, 44); // pt
            m_configManager->setEnumDefault(config::SettingMenuTimeout, config::MenuTimeout::Medium);
            m_configManager->setDefault(config::SettingMenuExpert, m_configManager->isDeveloper());
            m_configManager->setDefault(config::SettingMenuLegacyMode, 0);
            m_configManager->setEnumDefault(config::SettingOverlayType, config::OverlayType::None);
            m_configManager->setDefault(config::SettingOverlayShowClock, 0);
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
            m_configManager->setDefault(config::SettingHandOcclusion, 0);
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
            m_configManager->setDefault(config::SettingVRSScaleFilter, 80);
            m_configManager->setDefault(config::SettingVRSCullHAM, 0);

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
            m_configManager->setDefault(config::SettingZoom, 10);
            m_configManager->setDefault(config::SettingDisableHAM, 0);
            m_configManager->setEnumDefault(config::SettingBlindEye, config::BlindEye::None);
            m_configManager->setDefault(config::SettingPredictionDampen, 100);
            m_configManager->setDefault(config::SettingResolutionOverride, 0);
            m_configManager->setEnumDefault(config::SettingMotionReprojection, config::MotionReprojection::Default);
            m_configManager->setEnumDefault(config::SettingMotionReprojectionRate, config::MotionReprojectionRate::Off);
            m_configManager->setEnumDefault(config::SettingScreenshotFileFormat, config::ScreenshotFileFormat::PNG);
            m_configManager->setDefault(config::SettingScreenshotEye, 0); // Both
            m_configManager->setDefault(config::SettingRecordStats, 0);
            m_configManager->setDefault(config::SettingHighRateStats, 0);
            m_configManager->setDefault(config::SettingFrameThrottling, config::MaxFrameRate); // Off
            m_configManager->setDefault(config::SettingTargetFrameRate, config::MaxFrameRate); // Off
            m_configManager->setDefault(config::SettingTargetFrameRate2, 0);

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
            m_configManager->setDefault(config::SettingDisableInterceptor,
                                        (m_applicationName == "re2" || m_applicationName == "OpenComposite_Il-2") ? 1
                                                                                                                  : 0);
            // We disable the frame analyzer when using OpenComposite, because the app does not see the OpenXR
            // textures anyways.
            m_configManager->setDefault("disable_frame_analyzer",
                                        m_isOpenComposite || m_applicationName == "DCS World");
            m_configManager->setDefault("canting", 0);
            m_configManager->setDefault("vrs_capture", 0);
            m_configManager->setDefault("force_vprt_path", 0);
            m_configManager->setDefault("droolon_port", 5347);

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
            if (createInfo->type != XR_TYPE_INSTANCE_CREATE_INFO) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrCreateInstance",
                              TLArg(xr::ToString(createInfo->applicationInfo.apiVersion).c_str(), "ApiVersion"),
                              TLArg(createInfo->applicationInfo.applicationName, "ApplicationName"),
                              TLArg(createInfo->applicationInfo.applicationVersion, "ApplicationVersion"),
                              TLArg(createInfo->applicationInfo.engineName, "EngineName"),
                              TLArg(createInfo->applicationInfo.engineVersion, "EngineVersion"),
                              TLArg(createInfo->createFlags, "CreateFlags"));

            // Needed to resolve the requested function pointers.
            OpenXrApi::xrCreateInstance(createInfo);

            {
                PFN_xrVoidFunction unused;
                m_hasPerformanceCounterKHR = XR_SUCCEEDED(
                    xrGetInstanceProcAddr(GetXrInstance(), "xrConvertWin32PerformanceCounterToTimeKHR", &unused));
            }
            {
                PFN_xrVoidFunction unused;
                m_hasVisibilityMaskKHR =
                    XR_SUCCEEDED(xrGetInstanceProcAddr(GetXrInstance(), "xrGetVisibilityMaskKHR", &unused));
            }
            m_applicationName = createInfo->applicationInfo.applicationName;
            Log("Application name: '%s', Engine name: '%s'\n",
                createInfo->applicationInfo.applicationName,
                createInfo->applicationInfo.engineName);
            m_isOpenComposite = m_applicationName.find("OpenComposite_") == 0;
            if (m_isOpenComposite) {
                Log("Detected OpenComposite\n");
            }

            // With OpenComposite, we cannot use the frame analyzer, which results in the eye-tracked foveated rendering
            // not being advertised. However for certain apps using double-wide rendering, we can still enable the
            // feature, based on the list of apps below.
            m_overrideFoveatedRenderingCapability =
                m_isOpenComposite && m_applicationName == "OpenComposite_AC2-Win64-Shipping";

            // Dump the OpenXR runtime information to help debugging customer issues.
            XrInstanceProperties instanceProperties = {XR_TYPE_INSTANCE_PROPERTIES, nullptr};
            CHECK_XRCMD(xrGetInstanceProperties(GetXrInstance(), &instanceProperties));
            TraceLoggingWrite(g_traceProvider,
                              "xrGetInstanceProperties",
                              TLArg(instanceProperties.runtimeName, "RuntimeName"),
                              TLArg(xr::ToString(instanceProperties.runtimeVersion).c_str(), "RuntimeVersion"));
            m_runtimeName = fmt::format("{} {}.{}.{}",
                                        instanceProperties.runtimeName,
                                        XR_VERSION_MAJOR(instanceProperties.runtimeVersion),
                                        XR_VERSION_MINOR(instanceProperties.runtimeVersion),
                                        XR_VERSION_PATCH(instanceProperties.runtimeVersion));
            Log("Using OpenXR runtime %s\n", m_runtimeName.c_str());

            m_configManager = config::CreateConfigManager(createInfo->applicationInfo.applicationName);
            setOptionsDefaults();

            if (m_configManager->isDeveloper()) {
                TraceLoggingWrite(g_traceProvider, "DeveloperMode");
                Log("DEVELOPER MODE IS ENABLED! WARRANTY IS VOID!\n");
            }
            if (m_configManager->isSafeMode()) {
                TraceLoggingWrite(g_traceProvider, "SafeMode");
                Log("SAFE MODE IS ENABLED! NO SETTINGS ARE LOADED!\n");
            }

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
                        param.ports[0] = m_configManager->getValue("droolon_port");
                        Log("--> aSeeVR_connect_server(%d)\n", param.ports[0]);
                        const auto code = aSeeVR_connect_server(&param);
                        m_hasPimaxEyeTracker = code == ASEEVR_RETURN_CODE::success;
                        Log("<-- aSeeVR_connect_server %d\n", code);
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

                    m_needVarjoPollEventWorkaround = m_runtimeName.find("Varjo") != std::string::npos;
                }
            }

            // Clear HAM-related events so they don't fire off unnecessarily.
            (void)m_configManager->getValue(config::SettingDisableHAM);
            (void)m_configManager->getEnumValue<config::BlindEye>(config::SettingBlindEye);

            // We want to log a warning if HAGS is on.
            const auto hwSchMode = utilities::RegGetDword(
                HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers", L"HwSchMode");
            if (hwSchMode && hwSchMode.value() == 2) {
                Log("HAGS is on\n");
            }

            return XR_SUCCESS;
        }

        ~OpenXrLayer() override {
            if (m_configManager) {
                m_configManager->setActiveSession("");
            }

            utilities::RestoreTimerPrecision();

            graphics::UnhookForD3D11DebugLayer();
        }

        XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId) override {
            if (getInfo->type != XR_TYPE_SYSTEM_GET_INFO) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrGetSystem",
                              TLPArg(instance, "Instance"),
                              TLArg(xr::ToCString(getInfo->formFactor), "FormFactor"));

            const XrResult result = OpenXrApi::xrGetSystem(instance, getInfo, systemId);
            if (XR_SUCCEEDED(result) && getInfo->formFactor == XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY &&
                m_vrSystemId == XR_NULL_SYSTEM_ID) {
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
                    TLArg(fmt::format("{}x{}", m_displayWidth, m_displayHeight).c_str(), "RecommendedResolution"),
                    TLArg(handTrackingSystemProperties.supportsHandTracking, "SupportsHandTracking"),
                    TLArg(eyeTrackingSystemProperties.supportsEyeGazeInteraction, "SupportsEyeGazeInteraction"));
                Log("Using OpenXR system %s\n", m_systemName.c_str());

                const auto isWMR = m_runtimeName.find("Windows Mixed Reality Runtime") != std::string::npos;
                const auto isVive = m_runtimeName.find("Vive Reality Runtime") != std::string::npos;
                const auto isVarjo = m_runtimeName.find("Varjo") != std::string::npos;
                const auto isSteamVR = m_runtimeName.find("SteamVR") != std::string::npos;

                m_supportMotionReprojectionLock = isWMR;

                // Workaround: the Vive runtime does not seem to properly convert timestamps. We disable any feature
                // depending on timestamps conversion.
                m_hasPerformanceCounterKHR = !isVive;

                // Workaround: the Varjo and SteamVR runtimes always advertises maxImageRect==recommendedImageRect.
                if (isVarjo || isSteamVR) {
                    m_maxDisplayWidth = 8192;
                }

                m_supportHandTracking = handTrackingSystemProperties.supportsHandTracking;
                m_supportEyeTracking = eyeTrackingSystemProperties.supportsEyeGazeInteraction || m_isOmniceptDetected ||
                                       m_hasPimaxEyeTracker ||
                                       m_configManager->getValue(config::SettingEyeDebugWithController);
                const bool isEyeTrackingThruRuntime =
                    m_supportEyeTracking && !(m_isOmniceptDetected || m_hasPimaxEyeTracker);

                // Workaround: the WMR runtime supports mapping the VR controllers through XR_EXT_hand_tracking, which
                // will (falsely) advertise hand tracking support. Check for the Ultraleap layer in this case.
                if (m_supportHandTracking &&
                    (!m_configManager->isDeveloper() &&
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
                    (!m_configManager->isDeveloper() &&
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

                TraceLoggingWrite(g_traceProvider, "xrGetSystem", TLArg((int)*systemId, "SystemId"));
            }

            return result;
        }

        XrResult xrEnumerateViewConfigurationViews(XrInstance instance,
                                                   XrSystemId systemId,
                                                   XrViewConfigurationType viewConfigurationType,
                                                   uint32_t viewCapacityInput,
                                                   uint32_t* viewCountOutput,
                                                   XrViewConfigurationView* views) override {
            TraceLoggingWrite(g_traceProvider,
                              "xrEnumerateViewConfigurationViews",
                              TLPArg(instance, "Instance"),
                              TLArg((int)systemId, "SystemId"),
                              TLArg(viewCapacityInput, "ViewCapacityInput"),
                              TLArg(xr::ToCString(viewConfigurationType), "ViewConfigurationType"));

            const XrResult result = OpenXrApi::xrEnumerateViewConfigurationViews(
                instance, systemId, viewConfigurationType, viewCapacityInput, viewCountOutput, views);
            if (XR_SUCCEEDED(result) && isVrSystem(systemId) && views) {
                // Determine the application resolution.
                // If a session is active, we use the values latched at session creation. Some applications like Unreal
                // or Unity seem to constantly poll xrEnumerateViewConfigurationViews() and we do not want to create a
                // tear.
                const auto upscaleMode =
                    m_vrSession != XR_NULL_HANDLE
                        ? m_upscaleMode
                        : m_configManager->peekEnumValue<config::ScalingType>(config::SettingScalingType);
                const auto settingScaling = m_vrSession != XR_NULL_HANDLE
                                                ? m_settingScaling
                                                : m_configManager->peekValue(config::SettingScaling);
                const auto settingAnamophic = m_vrSession != XR_NULL_HANDLE
                                                  ? m_settingAnamorphic
                                                  : m_configManager->peekValue(config::SettingAnamorphic);

                uint32_t inputWidth = m_displayWidth;
                uint32_t inputHeight = m_displayHeight;

                switch (upscaleMode) {
                case config::ScalingType::FSR:
                case config::ScalingType::NIS: {
                    std::tie(inputWidth, inputHeight) = config::GetScaledDimensions(
                        settingScaling, settingAnamophic, m_displayWidth, m_displayHeight, 2);
                } break;

                case config::ScalingType::CAS:
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

                static bool atLeastOnce = false;
                if (m_vrSession == XR_NULL_HANDLE || atLeastOnce) {
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
                    atLeastOnce = true;
                }

                TraceLoggingWrite(
                    g_traceProvider,
                    "xrEnumerateViewConfigurationViews",
                    TLArg(fmt::format("{}x{}", inputWidth, inputHeight).c_str(), "AppResolution"),
                    TLArg(fmt::format("{}x{}", m_displayWidth, m_displayHeight).c_str(), "SystemResolution"));
            }

            return result;
        }

        XrResult xrCreateSession(XrInstance instance,
                                 const XrSessionCreateInfo* createInfo,
                                 XrSession* session) override {
            if (createInfo->type != XR_TYPE_SESSION_CREATE_INFO) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrCreateSession",
                              TLPArg(instance, "Instance"),
                              TLArg((int)createInfo->systemId, "SystemId"),
                              TLArg(createInfo->createFlags, "CreateFlags"));

            // Force motion reprojection if requested.
            if (m_supportMotionReprojectionLock) {
                utilities::ToggleWindowsMixedRealityReprojection(
                    m_configManager->getEnumValue<config::MotionReprojection>(config::SettingMotionReprojection));
                m_isFrameThrottlingPossible = m_configManager->peekEnumValue<config::MotionReprojection>(
                                                  config::SettingMotionReprojection) != config::MotionReprojection::On;
            }

            const XrResult result = OpenXrApi::xrCreateSession(instance, createInfo, session);
            if (XR_SUCCEEDED(result) && isVrSystem(createInfo->systemId)) {
                // Get the graphics device.
                const XrBaseInStructure* entry = reinterpret_cast<const XrBaseInStructure*>(createInfo->next);
                while (entry) {
                    if (entry->type == XR_TYPE_GRAPHICS_BINDING_D3D11_KHR) {
                        TraceLoggingWrite(g_traceProvider, "UseD3D11");

                        const XrGraphicsBindingD3D11KHR* d3dBindings =
                            reinterpret_cast<const XrGraphicsBindingD3D11KHR*>(entry);
                        // Workaround: On Oculus, we must delay the initialization of Detour.
                        const bool enableOculusQuirk = m_runtimeName.find("Oculus") != std::string::npos;
                        m_graphicsDevice =
                            graphics::WrapD3D11Device(d3dBindings->device, m_configManager, enableOculusQuirk);
                        break;
                    } else if (entry->type == XR_TYPE_GRAPHICS_BINDING_D3D12_KHR) {
                        TraceLoggingWrite(g_traceProvider, "UseD3D12");

                        const XrGraphicsBindingD3D12KHR* d3dBindings =
                            reinterpret_cast<const XrGraphicsBindingD3D12KHR*>(entry);
                        // Workaround: On Varjo, we must use intermediate textures with D3D11on12.
                        const bool enableVarjoQuirk = m_runtimeName.find("Varjo") != std::string::npos;
                        m_graphicsDevice = graphics::WrapD3D12Device(
                            d3dBindings->device, d3dBindings->queue, m_configManager, enableVarjoQuirk);
                        break;
                    }

                    entry = entry->next;
                }

                if (m_graphicsDevice) {
                    // Initialize the other resources.

                    m_upscaleMode = m_configManager->getEnumValue<config::ScalingType>(config::SettingScalingType);
                    if (m_upscaleMode == config::ScalingType::NIS || m_upscaleMode == config::ScalingType::FSR) {
                        m_settingScaling = m_configManager->peekValue(config::SettingScaling);
                        m_settingAnamorphic = m_configManager->peekValue(config::SettingAnamorphic);
                    }

                    switch (m_upscaleMode) {
                    case config::ScalingType::NIS:
                        m_upscaler = graphics::CreateNISUpscaler(
                            m_configManager, m_graphicsDevice, m_settingScaling, m_settingAnamorphic);
                        break;

                    case config::ScalingType::FSR:
                        m_upscaler = graphics::CreateFSRUpscaler(
                            m_configManager, m_graphicsDevice, m_settingScaling, m_settingAnamorphic);
                        break;

                    case config::ScalingType::CAS:
                        m_upscaler = graphics::CreateCASSharpener(m_configManager, m_graphicsDevice);
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
                    if (m_upscaleMode == config::ScalingType::NIS || m_upscaleMode == config::ScalingType::FSR) {
                        std::tie(renderWidth, renderHeight) = config::GetScaledDimensions(
                            m_settingScaling, m_settingAnamorphic, m_displayWidth, m_displayHeight, 2);

                        // Per FSR SDK documentation.
                        m_mipMapBiasForUpscaling = -std::log2f(static_cast<float>(m_displayWidth * m_displayHeight) /
                                                               (renderWidth * renderHeight));
                        Log("MipMap biasing for upscaling is: %.3f\n", m_mipMapBiasForUpscaling);
                    }

                    m_postProcessor = graphics::CreateImageProcessor(m_configManager, m_graphicsDevice);

                    if (m_graphicsDevice->isEventsSupported()) {
                        if (!m_configManager->getValue("disable_frame_analyzer")) {
                            graphics::FrameAnalyzerHeuristic heuristic = graphics::FrameAnalyzerHeuristic::Unknown;

                            // TODO: Override heuristic per-app if needed.

                            m_frameAnalyzer = graphics::CreateFrameAnalyzer(
                                m_configManager, m_graphicsDevice, m_displayWidth, m_displayHeight, heuristic);
                        }

                        m_variableRateShader = graphics::CreateVariableRateShader(*this,
                                                                                  m_configManager,
                                                                                  m_graphicsDevice,
                                                                                  m_eyeTracker,
                                                                                  renderWidth,
                                                                                  renderHeight,
                                                                                  m_displayWidth,
                                                                                  m_displayHeight);

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

                    m_performanceCounters.appCpuTimer = utilities::CreateCpuTimer();
                    m_performanceCounters.renderCpuTimer = utilities::CreateCpuTimer();
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
                            2048; // Let's hope the menu doesn't get bigger than that.
                        swapchainInfo.arraySize = 1;
                        swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
                        swapchainInfo.format = formats[0];
                        swapchainInfo.sampleCount = 1;
                        swapchainInfo.faceCount = 1;
                        swapchainInfo.mipCount = 1;
                        CHECK_XRCMD(OpenXrApi::xrCreateSwapchain(*session, &swapchainInfo, &m_menuSwapchain));
                        TraceLoggingWrite(g_traceProvider, "MenuSwapchain", TLPArg(m_menuSwapchain, "Swapchain"));

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
                                                               D3D12_RESOURCE_STATE_RENDER_TARGET,
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
                        menuInfo.isPredictionDampeningSupported = m_hasPerformanceCounterKHR;
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
                        menuInfo.isVisibilityMaskSupported = m_hasVisibilityMaskKHR;
                        // Our HAM override does not seem to work with OpenComposite.
                        menuInfo.isVisibilityMaskOverrideSupported = !m_isOpenComposite && m_hasVisibilityMaskKHR;
                        menuInfo.runtimeName = m_runtimeName;

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

                    // Re-attach action set for the eye tracker if needed.
                    m_isActionSetAttached = false;

                    // Remember the XrSession to use.
                    m_vrSession = *session;
                } else {
                    Log("Unsupported graphics runtime.\n");
                }

                TraceLoggingWrite(g_traceProvider, "xrCreateSession", TLPArg(*session, "Session"));
            }

            return result;
        }

        XrResult xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo) override {
            if (beginInfo->type != XR_TYPE_SESSION_BEGIN_INFO) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(
                g_traceProvider,
                "xrBeginSession",
                TLPArg(session, "Session"),
                TLArg(xr::ToCString(beginInfo->primaryViewConfigurationType), "PrimaryViewConfigurationType"));

            const XrResult result = OpenXrApi::xrBeginSession(session, beginInfo);
            if (XR_SUCCEEDED(result) && isVrSession(session)) {
                m_configManager->setActiveSession(m_applicationName);

                // Bump up timer precision for this process.
                utilities::EnableHighPrecisionTimer();

                if (m_variableRateShader) {
                    m_variableRateShader->beginSession(session);
                }
            }

            return result;
        }

        XrResult xrEndSession(XrSession session) override {
            TraceLoggingWrite(g_traceProvider, "xrEndSession", TLPArg(session, "Session"));

            const XrResult result = OpenXrApi::xrEndSession(session);
            if (XR_SUCCEEDED(result) && isVrSession(session)) {
                if (m_variableRateShader) {
                    m_variableRateShader->endSession();
                }

                utilities::RestoreTimerPrecision();

                m_configManager->setActiveSession("");
            }

            return result;
        }

        XrResult xrDestroySession(XrSession session) override {
            TraceLoggingWrite(g_traceProvider, "xrDestroySession", TLPArg(session, "Session"));

            // Prepare for shutdown
            if (isVrSession(session)) {
                if (m_configManager) {
                    m_configManager->setActiveSession("");
                }

                // Wait for any pending operation to complete.
                if (m_graphicsDevice) {
                    if (m_asyncWaitPromise.valid()) {
                        m_asyncWaitPromise.wait_for(5s);
                        m_asyncWaitPromise = {};
                    }

                    m_graphicsDevice->blockCallbacks();
                    m_graphicsDevice->flushContext(true);
                }

                // Cleanup session resources.
                if (m_viewSpace != XR_NULL_HANDLE) {
                    xrDestroySpace(m_viewSpace);
                    m_viewSpace = XR_NULL_HANDLE;
                }
                if (m_handTracker) {
                    m_handTracker->endSession();
                }
                if (m_eyeTracker) {
                    m_eyeTracker->endSession();
                }
                if (m_menuSwapchain != XR_NULL_HANDLE) {
                    xrDestroySwapchain(m_menuSwapchain);
                    m_menuSwapchain = XR_NULL_HANDLE;
                }
            }

            const XrResult result = OpenXrApi::xrDestroySession(session);

            if (XR_SUCCEEDED(result) && isVrSession(session)) {
                // Cleanup our resources.
                m_upscaler.reset();
                m_postProcessor.reset();
                m_frameAnalyzer.reset();
                m_variableRateShader.reset();
                for (unsigned int i = 0; i <= GpuTimerLatency; i++) {
                    m_performanceCounters.appGpuTimer[i].reset();
                    m_performanceCounters.overlayGpuTimer[i].reset();
                }
                m_performanceCounters.appCpuTimer.reset();
                m_performanceCounters.renderCpuTimer.reset();
                m_performanceCounters.waitCpuTimer.reset();
                m_performanceCounters.endFrameCpuTimer.reset();
                m_performanceCounters.overlayCpuTimer.reset();
                m_swapchains.clear();
                m_menuSwapchainImages.clear();
                m_menuHandler.reset();
                if (m_graphicsDevice) {
                    m_graphicsDevice->shutdown();
                }
                m_graphicsDevice.reset();

                // We intentionally do not reset hand/eye trackers since they are tied to the instance, not session.

                m_vrSession = XR_NULL_HANDLE;

                // A good check to ensure there are no resources leak is to confirm that the graphics device is
                // destroyed _before_ we see this message.
                // eg:
                // 2022-01-01 17:15:35 -0800: D3D11Device destroyed
                // 2022-01-01 17:15:35 -0800: Session destroyed
                // If the order is reversed or the Device is destructed missing, then it means that we are not cleaning
                // up the resources properly.
                Log("Session destroyed\n");

                utilities::RestoreTimerPrecision();
            }

            return result;
        }

        XrResult xrCreateSwapchain(XrSession session,
                                   const XrSwapchainCreateInfo* createInfo,
                                   XrSwapchain* swapchain) override {
            if (createInfo->type != XR_TYPE_SWAPCHAIN_CREATE_INFO) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrCreateSwapchain",
                              TLPArg(session, "Session"),
                              TLArg(createInfo->arraySize, "ArraySize"),
                              TLArg(createInfo->width, "Width"),
                              TLArg(createInfo->height, "Height"),
                              TLArg(createInfo->createFlags, "CreateFlags"),
                              TLArg(createInfo->format, "Format"),
                              TLArg(createInfo->faceCount, "FaceCount"),
                              TLArg(createInfo->mipCount, "MipCount"),
                              TLArg(createInfo->sampleCount, "SampleCount"),
                              TLArg(createInfo->usageFlags, "UsageFlags"));

            if (!isVrSession(session) || !m_graphicsDevice) {
                return OpenXrApi::xrCreateSwapchain(session, createInfo, swapchain);
            }

            // Identify the swapchains of interest for our processing chain.
            const bool useSwapchain =
                createInfo->usageFlags &
                (XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                 XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT);

            // We do no do any processing to depth buffer, but we like to have them for other things like occlusion when
            // drawing.
            const bool isDepth = createInfo->usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

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
            if (useSwapchain && !isDepth) {
                // Modify the swapchain to handle our processing chain (eg: change resolution and/or usage.

                if (m_upscaleMode == config::ScalingType::NIS || m_upscaleMode == config::ScalingType::FSR) {
                    float horizontalScaleFactor;
                    float verticalScaleFactor;
                    std::tie(horizontalScaleFactor, verticalScaleFactor) =
                        config::GetScalingFactors(m_settingScaling, m_settingAnamorphic);

                    chainCreateInfo.width = roundUp((uint32_t)std::ceil(createInfo->width * horizontalScaleFactor), 2);
                    chainCreateInfo.height = roundUp((uint32_t)std::ceil(createInfo->height * verticalScaleFactor), 2);
                }

                // The post processor will draw a full-screen quad onto the final swapchain.
                chainCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
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
                                          "RuntimeSwapchain",
                                          TLArg(desc.Width, "Width"),
                                          TLArg(desc.Height, "Height"),
                                          TLArg(desc.ArraySize, "ArraySize"),
                                          TLArg(desc.MipLevels, "MipCount"),
                                          TLArg(desc.SampleDesc.Count, "SampleCount"),
                                          TLArg((int)desc.Format, "Format"),
                                          TLArg((int)desc.Usage, "Usage"),
                                          TLArg(desc.BindFlags, "BindFlags"),
                                          TLArg(desc.CPUAccessFlags, "CPUAccessFlags"),
                                          TLArg(desc.MiscFlags, "MiscFlags"));

                        // Make sure to create the app texture typeless.
                        overrideFormat = (int64_t)desc.Format;
                    }

                    for (uint32_t i = 0; i < imageCount; i++) {
                        SwapchainImages images;

                        // Store the runtime images into the state (last entry in the processing chain).
                        images.runtimeTexture =
                            graphics::WrapD3D11Texture(m_graphicsDevice,
                                                       chainCreateInfo,
                                                       d3dImages[i].texture,
                                                       fmt::format("Runtime swapchain {} TEX2D", i));

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
                                          "RuntimeSwapchain",
                                          TLArg(desc.Width, "Width"),
                                          TLArg(desc.Height, "Height"),
                                          TLArg(desc.DepthOrArraySize, "ArraySize"),
                                          TLArg(desc.MipLevels, "MipCount"),
                                          TLArg(desc.SampleDesc.Count, "SampleCount"),
                                          TLArg((int)desc.Format, "Format"),
                                          TLArg((int)desc.Flags, "Flags"));

                        // Make sure to create the app texture typeless.
                        overrideFormat = (int64_t)desc.Format;
                    }

                    for (uint32_t i = 0; i < imageCount; i++) {
                        SwapchainImages images;

                        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
                        if ((chainCreateInfo.usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT)) {
                            initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
                        } else if ((chainCreateInfo.usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
                            initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
                        }

                        // Store the runtime images into the state (last entry in the processing chain).
                        images.runtimeTexture =
                            graphics::WrapD3D12Texture(m_graphicsDevice,
                                                       chainCreateInfo,
                                                       d3dImages[i].texture,
                                                       initialState,
                                                       fmt::format("Runtime swapchain {} TEX2D", i));

                        swapchainState.images.push_back(std::move(images));
                    }
                } else {
                    throw std::runtime_error("Unsupported graphics runtime");
                }

                for (uint32_t i = 0; i < imageCount; i++) {
                    SwapchainImages& images = swapchainState.images[i];

                    if (!isDepth) {
                        // Create an app texture with the exact specification requested (lower resolution in case of
                        // upscaling).
                        XrSwapchainCreateInfo inputCreateInfo = *createInfo;

                        // Both post-processor and upscalers need to do sampling.
                        inputCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

                        images.appTexture = m_graphicsDevice->createTexture(
                            inputCreateInfo, fmt::format("App swapchain {} TEX2D", i), overrideFormat);

                        for (uint32_t i = 0; i < utilities::ViewCount; i++) {
                            images.upscalingTimers[i] = m_graphicsDevice->createTimer();
                            images.postProcessingTimers[i] = m_graphicsDevice->createTimer();
                        }
                    } else {
                        images.appTexture = images.runtimeTexture;
                    }
                }

                m_swapchains.insert_or_assign(*swapchain, swapchainState);

                TraceLoggingWrite(g_traceProvider, "xrCreateSwapchain", TLPArg(*swapchain, "Swapchain"));
            }

            return result;
        }

        XrResult xrDestroySwapchain(XrSwapchain swapchain) override {
            TraceLoggingWrite(g_traceProvider, "xrDestroySwapchain", TLPArg(swapchain, "Swapchain"));

            const XrResult result = OpenXrApi::xrDestroySwapchain(swapchain);
            if (XR_SUCCEEDED(result)) {
                m_swapchains.erase(swapchain);
            }

            return result;
        }

        XrResult xrSuggestInteractionProfileBindings(
            XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings) override {
            if (suggestedBindings->type != XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrSuggestInteractionProfileBindings",
                              TLPArg(instance, "Instance"),
                              TLArg(getPath(suggestedBindings->interactionProfile).c_str(), "InteractionProfile"));

            for (uint32_t i = 0; i < suggestedBindings->countSuggestedBindings; i++) {
                TraceLoggingWrite(g_traceProvider,
                                  "xrSuggestInteractionProfileBindings",
                                  TLPArg(suggestedBindings->suggestedBindings[i].action, "Action"),
                                  TLArg(getPath(suggestedBindings->suggestedBindings[i].binding).c_str(), "Path"));
            }

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
            if (attachInfo->type != XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider, "xrAttachSessionActionSets", TLPArg(session, "Session"));
            for (uint32_t i = 0; i < attachInfo->countActionSets; i++) {
                TraceLoggingWrite(
                    g_traceProvider, "xrAttachSessionActionSets", TLPArg(attachInfo->actionSets[i], "ActionSet"));
            }

            XrSessionActionSetsAttachInfo chainAttachInfo = *attachInfo;
            std::vector<XrActionSet> newActionSets;
            if (m_eyeTracker && isVrSession(session)) {
                const auto eyeTrackerActionSet = m_eyeTracker->getActionSet();
                if (eyeTrackerActionSet != XR_NULL_HANDLE) {
                    newActionSets.resize(chainAttachInfo.countActionSets + 1);
                    memcpy(newActionSets.data(),
                           chainAttachInfo.actionSets,
                           chainAttachInfo.countActionSets * sizeof(XrActionSet));
                    uint32_t nextActionSetSlot = chainAttachInfo.countActionSets;

                    newActionSets[nextActionSetSlot++] = eyeTrackerActionSet;

                    chainAttachInfo.actionSets = newActionSets.data();
                    chainAttachInfo.countActionSets = nextActionSetSlot;
                }
                m_isActionSetUsed = attachInfo->countActionSets > 0;
            }

            return OpenXrApi::xrAttachSessionActionSets(session, &chainAttachInfo);
        }

        XrResult xrCreateAction(XrActionSet actionSet,
                                const XrActionCreateInfo* createInfo,
                                XrAction* action) override {
            if (createInfo->type != XR_TYPE_ACTION_CREATE_INFO) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrCreateAction",
                              TLPArg(actionSet, "ActionSet"),
                              TLArg(createInfo->actionName, "Name"),
                              TLArg(createInfo->localizedActionName, "LocalizedName"),
                              TLArg(xr::ToCString(createInfo->actionType), "Type"));
            for (uint32_t i = 0; i < createInfo->countSubactionPaths; i++) {
                TraceLoggingWrite(g_traceProvider,
                                  "xrCreateAction",
                                  TLArg(getPath(createInfo->subactionPaths[i]).c_str(), "SubactionPath"));
            }

            const XrResult result = OpenXrApi::xrCreateAction(actionSet, createInfo, action);
            if (XR_SUCCEEDED(result)) {
                if (m_handTracker) {
                    m_handTracker->registerAction(*action, actionSet);
                }
                TraceLoggingWrite(g_traceProvider, "xrCreateAction", TLPArg(*action, "Action"));
            }

            return result;
        }

        XrResult xrDestroyAction(XrAction action) override {
            TraceLoggingWrite(g_traceProvider, "xrDestroyAction", TLPArg(action, "Action"));

            const XrResult result = OpenXrApi::xrDestroyAction(action);
            if (XR_SUCCEEDED(result) && m_handTracker) {
                m_handTracker->unregisterAction(action);
            }

            return result;
        }

        XrResult xrCreateActionSpace(XrSession session,
                                     const XrActionSpaceCreateInfo* createInfo,
                                     XrSpace* space) override {
            if (createInfo->type != XR_TYPE_ACTION_SPACE_CREATE_INFO) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrCreateActionSpace",
                              TLPArg(session, "Session"),
                              TLPArg(createInfo->action, "Action"),
                              TLArg(getPath(createInfo->subactionPath).c_str(), "SubactionPath"),
                              TLArg(xr::ToString(createInfo->poseInActionSpace).c_str(), "PoseInActionSpace"));

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

                TraceLoggingWrite(g_traceProvider, "xrCreateActionSpace", TLPArg(*space, "Space"));
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
            TraceLoggingWrite(g_traceProvider,
                              "xrEnumerateSwapchainImages",
                              TLPArg(swapchain, "Swapchain"),
                              TLArg(imageCapacityInput, "ImageCapacityInput"));

            const XrResult result =
                OpenXrApi::xrEnumerateSwapchainImages(swapchain, imageCapacityInput, imageCountOutput, images);
            if (XR_SUCCEEDED(result) && images) {
                auto swapchainIt = m_swapchains.find(swapchain);
                if (swapchainIt != m_swapchains.end()) {
                    auto& swapchainState = swapchainIt->second;

                    // Return the application texture.
                    if (m_graphicsDevice->getApi() == graphics::Api::D3D11) {
                        XrSwapchainImageD3D11KHR* d3dImages = reinterpret_cast<XrSwapchainImageD3D11KHR*>(images);
                        for (uint32_t i = 0; i < *imageCountOutput; i++) {
                            d3dImages[i].texture = swapchainState.images[i].appTexture->getAs<graphics::D3D11>();
                            TraceLoggingWrite(
                                g_traceProvider, "xrEnumerateSwapchainImages", TLPArg(d3dImages[i].texture, "Image"));
                        }
                    } else if (m_graphicsDevice->getApi() == graphics::Api::D3D12) {
                        XrSwapchainImageD3D12KHR* d3dImages = reinterpret_cast<XrSwapchainImageD3D12KHR*>(images);
                        for (uint32_t i = 0; i < *imageCountOutput; i++) {
                            d3dImages[i].texture = swapchainState.images[i].appTexture->getAs<graphics::D3D12>();
                            TraceLoggingWrite(
                                g_traceProvider, "xrEnumerateSwapchainImages", TLPArg(d3dImages[i].texture, "Image"));
                        }
                    } else {
                        throw std::runtime_error("Unsupported graphics runtime");
                    }
                }
            }

            TraceLoggingWrite(
                g_traceProvider, "xrEnumerateSwapchainImages", TLArg(*imageCountOutput, "ImageCountOutput"));

            return result;
        }

        XrResult xrWaitSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo) override {
            if (waitInfo->type != XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(
                g_traceProvider, "xrWaitSwapchainImage", TLPArg(swapchain, "Swapchain"), TLArg(waitInfo->timeout));

            // We remove the timeout causing issues with OpenComposite.
            XrSwapchainImageWaitInfo chainWaitInfo = *waitInfo;
            chainWaitInfo.timeout = XR_INFINITE_DURATION;
            return OpenXrApi::xrWaitSwapchainImage(swapchain, &chainWaitInfo);
        }

        XrResult xrAcquireSwapchainImage(XrSwapchain swapchain,
                                         const XrSwapchainImageAcquireInfo* acquireInfo,
                                         uint32_t* index) override {
            if (acquireInfo && acquireInfo->type != XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider, "xrAcquireSwapchainImage", TLPArg(swapchain, "Swapchain"));

            auto swapchainIt = m_swapchains.find(swapchain);
            if (swapchainIt != m_swapchains.end()) {
                if (m_frameAnalyzer) {
                    m_frameAnalyzer->onAcquireSwapchain(swapchain);
                }

                // Perform the release now in case it was delayed.
                if (swapchainIt->second.delayedRelease) {
                    TraceLoggingWrite(g_traceProvider, "ForcedSwapchainRelease", TLPArg(swapchain, "Swapchain"));

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

                TraceLoggingWrite(g_traceProvider, "xrAcquireSwapchainImage", TLArg(*index, "Index"));

                // Arbitrary location to simulate workload.
                m_graphicsDevice->executeDebugWorkload();
            }

            return result;
        }

        XrResult xrReleaseSwapchainImage(XrSwapchain swapchain,
                                         const XrSwapchainImageReleaseInfo* releaseInfo) override {
            if (releaseInfo && releaseInfo->type != XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider, "xrReleaseSwapchainImage", TLPArg(swapchain, "Swapchain"));

            auto swapchainIt = m_swapchains.find(swapchain);
            if (swapchainIt != m_swapchains.end()) {
                if (m_frameAnalyzer) {
                    m_frameAnalyzer->onReleaseSwapchain(swapchain);
                }

                // Perform a delayed release: we still need to write to the swapchain in our xrEndFrame()!
                swapchainIt->second.delayedRelease = true;
                return XR_SUCCESS;
            }

            return OpenXrApi::xrReleaseSwapchainImage(swapchain, releaseInfo);
        }

        XrResult xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData) override {
            TraceLoggingWrite(g_traceProvider, "xrPollEvent", TLPArg(instance, "Instance"));

            m_needVarjoPollEventWorkaround = false;

            if (m_sendInterationProfileEvent && m_vrSession != XR_NULL_HANDLE) {
                XrEventDataInteractionProfileChanged* const buffer =
                    reinterpret_cast<XrEventDataInteractionProfileChanged*>(eventData);
                buffer->type = XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED;
                buffer->next = nullptr;
                buffer->session = m_vrSession;

                TraceLoggingWrite(g_traceProvider, "InteractionProfileChanged", TLPArg(buffer->session, "Session"));

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

                TraceLoggingWrite(g_traceProvider,
                                  "VisibilityMaskChanged",
                                  TLPArg(buffer->session, "Session"),
                                  TLArg(xr::ToCString(buffer->viewConfigurationType), "ViewConfigurationType"),
                                  TLArg(buffer->viewIndex, "ViewIndex"));
                Log("Send XrEventDataVisibilityMaskChangedKHR event for view %u\n", buffer->viewIndex);

                return XR_SUCCESS;
            }

            return OpenXrApi::xrPollEvent(instance, eventData);
        }

        XrResult xrGetCurrentInteractionProfile(XrSession session,
                                                XrPath topLevelUserPath,
                                                XrInteractionProfileState* interactionProfile) override {
            if (interactionProfile->type != XR_TYPE_INTERACTION_PROFILE_STATE) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrGetCurrentInteractionProfile",
                              TLPArg(session, "Session"),
                              TLArg(getPath(topLevelUserPath).c_str(), "TopLevelUserPath"));

            std::string path = topLevelUserPath != XR_NULL_PATH ? getPath(topLevelUserPath) : "";
            XrResult result = XR_ERROR_RUNTIME_FAILURE;
            if (m_handTracker && isVrSession(session) &&
                (path.empty() || path == "/user/hand/left" || path == "/user/hand/right") &&
                interactionProfile->type == XR_TYPE_INTERACTION_PROFILE_STATE) {
                // Return our emulated interaction profile for the hands.
                interactionProfile->interactionProfile = m_handTracker->getInteractionProfile();
                result = XR_SUCCESS;
            } else {
                result = OpenXrApi::xrGetCurrentInteractionProfile(session, topLevelUserPath, interactionProfile);
            }

            if (XR_SUCCEEDED(result)) {
                TraceLoggingWrite(g_traceProvider,
                                  "xrGetCurrentInteractionProfile",
                                  TLArg(getPath(interactionProfile->interactionProfile).c_str(), "InteractionProfile"));
            }

            return result;
        }

        XrResult xrGetVisibilityMaskKHR(XrSession session,
                                        XrViewConfigurationType viewConfigurationType,
                                        uint32_t viewIndex,
                                        XrVisibilityMaskTypeKHR visibilityMaskType,
                                        XrVisibilityMaskKHR* visibilityMask) override {
            if (visibilityMask->type != XR_TYPE_VISIBILITY_MASK_KHR) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrGetVisibilityMaskKHR",
                              TLPArg(session, "Session"),
                              TLArg(xr::ToCString(viewConfigurationType), "ViewConfigurationType"),
                              TLArg(viewIndex, "ViewIndex"),
                              TLArg(xr::ToCString(visibilityMaskType), "VisibilityMaskType"),
                              TLArg(visibilityMask->vertexCapacityInput, "VertexCapacityInput"),
                              TLArg(visibilityMask->indexCapacityInput, "IndexCapacityInput"));

            bool useFullMask = false;
            bool useEmptyMask = false;

            XrResult result = XR_ERROR_RUNTIME_FAILURE;
            if (isVrSession(session)) {
                if (m_configManager->getValue(config::SettingDisableHAM)) {
                    useEmptyMask = visibilityMaskType == XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR;
                    useFullMask = visibilityMaskType == XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR;
                }
                switch (m_configManager->getEnumValue<config::BlindEye>(config::SettingBlindEye)) {
                case config::BlindEye::Left:
                    if (viewIndex == 0) {
                        useEmptyMask = visibilityMaskType == XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR;
                        useFullMask = visibilityMaskType == XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR;
                    }
                    break;
                case config::BlindEye::Right:
                    if (viewIndex == 1) {
                        useEmptyMask = visibilityMaskType == XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR;
                        useFullMask = visibilityMaskType == XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR;
                    }
                    break;

                case config::BlindEye::None:
                default:
                    break;
                }
            }

            if (useFullMask) {
                visibilityMask->vertexCountOutput = 4;
                visibilityMask->indexCountOutput = 6;
                if (visibilityMask->vertexCapacityInput >= visibilityMask->vertexCountOutput &&
                    visibilityMask->indexCapacityInput >= visibilityMask->indexCountOutput) {
                    visibilityMask->vertices[0] = {2, 2};
                    visibilityMask->vertices[1] = {-2, 2};
                    visibilityMask->vertices[2] = {-2, -2};
                    visibilityMask->vertices[3] = {2, -2};
                    visibilityMask->indices[0] = 0;
                    visibilityMask->indices[1] = 1;
                    visibilityMask->indices[2] = 2;
                    visibilityMask->indices[3] = 2;
                    visibilityMask->indices[4] = 3;
                    visibilityMask->indices[5] = 0;
                }
                result = XR_SUCCESS;
            } else if (useEmptyMask) {
                // TODO: Not great, but we must return something apparently. No vertices/indices does not seem to work
                // for all apps (bad two-call idiom)?
                visibilityMask->vertexCountOutput = 1;
                visibilityMask->indexCountOutput = 3;
                if (visibilityMask->vertexCapacityInput >= visibilityMask->vertexCountOutput &&
                    visibilityMask->indexCapacityInput >= visibilityMask->indexCountOutput) {
                    visibilityMask->vertices[0] = {0, 0};
                    visibilityMask->indices[0] = 0;
                    visibilityMask->indices[1] = 0;
                    visibilityMask->indices[2] = 0;
                }
                result = XR_SUCCESS;
            } else {
                result = OpenXrApi::xrGetVisibilityMaskKHR(
                    session, viewConfigurationType, viewIndex, visibilityMaskType, visibilityMask);
            }

            if (XR_SUCCEEDED(result)) {
                if (visibilityMask->type != XR_TYPE_VISIBILITY_MASK_KHR) {
                    return XR_ERROR_VALIDATION_FAILURE;
                }

                TraceLoggingWrite(g_traceProvider,
                                  "xrGetVisibilityMaskKHR",
                                  TLArg(visibilityMask->vertexCountOutput, "VertexCountOutput"),
                                  TLArg(visibilityMask->indexCountOutput, "IndexCountOutput"));
            }

            return result;
        }

        XrResult xrLocateViews(XrSession session,
                               const XrViewLocateInfo* viewLocateInfo,
                               XrViewState* viewState,
                               uint32_t viewCapacityInput,
                               uint32_t* viewCountOutput,
                               XrView* views) override {
            if (viewLocateInfo->type != XR_TYPE_VIEW_LOCATE_INFO || viewState->type != XR_TYPE_VIEW_STATE) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrLocateViews",
                              TLPArg(session, "Session"),
                              TLArg(xr::ToCString(viewLocateInfo->viewConfigurationType), "ViewConfigurationType"),
                              TLArg(viewLocateInfo->displayTime, "DisplayTime"),
                              TLPArg(viewLocateInfo->space, "Space"),
                              TLArg(viewCapacityInput, "ViewCapacityInput"));

            const XrResult result =
                OpenXrApi::xrLocateViews(session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views);
            if (XR_SUCCEEDED(result) && isVrSession(session) &&
                viewLocateInfo->viewConfigurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO &&
                viewCapacityInput) {
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
                    CHECK_XRCMD(OpenXrApi::xrLocateViews(
                        session, &info, &state, viewCapacityInput, viewCountOutput, eyeInViewSpace));

                    if (Pose::IsPoseValid(state.viewStateFlags)) {
                        utilities::GetProjectedGaze(eyeInViewSpace, XrVector3f{0, 0, -1.0f}, m_projCenters);

                        for (uint32_t eye = 0; eye < utilities::ViewCount; eye++) {
                            m_eyeGaze[eye] = m_projCenters[eye];
                        }

                        Log("Projection calibration: %.5f, %.5f | %.5f, %.5f\n",
                            m_projCenters[0].x,
                            m_projCenters[0].y,
                            m_projCenters[1].x,
                            m_projCenters[1].y);

                        if (m_menuHandler) {
                            m_menuHandler->setViewProjectionCenters(m_projCenters[0], m_projCenters[1]);
                        }

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

                TraceLoggingWrite(g_traceProvider,
                                  "xrLocateViews",
                                  TLArg(*viewCountOutput, "ViewCountOutput"),
                                  TLArg(viewState->viewStateFlags, "ViewStateFlags"),
                                  TLArg(xr::ToString(views[0].pose).c_str(), "LeftPose"),
                                  TLArg(xr::ToString(views[0].fov).c_str(), "LeftFov"),
                                  TLArg(xr::ToString(views[1].pose).c_str(), "RightPose"),
                                  TLArg(xr::ToString(views[1].fov).c_str(), "RightFov"));
            }

            return result;
        }

        XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location) override {
            if (location->type != XR_TYPE_SPACE_LOCATION) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrLocateSpace",
                              TLPArg(space, "Space"),
                              TLPArg(baseSpace, "BaseSpace"),
                              TLArg(time, "Time"));

            if (m_handTracker && m_vrSession != XR_NULL_HANDLE && location->type == XR_TYPE_SPACE_LOCATION) {
                m_performanceCounters.handTrackingTimer->start();
                if (m_handTracker->locate(space, baseSpace, time, getTimeNow(), *location)) {
                    m_performanceCounters.handTrackingTimer->stop();
                    m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer->query();

                    TraceLoggingWrite(g_traceProvider,
                                      "xrLocateSpace",
                                      TLArg(location->locationFlags, "LocationFlags"),
                                      TLArg(xr::ToString(location->pose).c_str(), "Pose"));

                    return XR_SUCCESS;
                }
            }

            return OpenXrApi::xrLocateSpace(space, baseSpace, time, location);
        }

        XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo) override {
            if (syncInfo->type != XR_TYPE_ACTIONS_SYNC_INFO) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider, "xrSyncActions", TLPArg(session, "Session"));
            for (uint32_t i = 0; i < syncInfo->countActiveActionSets; i++) {
                TraceLoggingWrite(g_traceProvider,
                                  "xrSyncActions",
                                  TLPArg(syncInfo->activeActionSets[i].actionSet, "ActionSet"),
                                  TLArg(getPath(syncInfo->activeActionSets[i].subactionPath).c_str(), "SubactionPath"));
            }

            XrActionsSyncInfo chainSyncInfo = *syncInfo;
            std::vector<XrActiveActionSet> newActiveActionSets;
            if (m_eyeTracker && isVrSession(session)) {
                const auto eyeTrackerActionSet = m_eyeTracker->getActionSet();
                if (eyeTrackerActionSet != XR_NULL_HANDLE) {
                    newActiveActionSets.resize(chainSyncInfo.countActiveActionSets + 1);
                    memcpy(newActiveActionSets.data(),
                           chainSyncInfo.activeActionSets,
                           chainSyncInfo.countActiveActionSets * sizeof(XrActiveActionSet));
                    uint32_t nextActionSetSlot = chainSyncInfo.countActiveActionSets;

                    newActiveActionSets[nextActionSetSlot].actionSet = eyeTrackerActionSet;
                    newActiveActionSets[nextActionSetSlot++].subactionPath = XR_NULL_PATH;

                    chainSyncInfo.activeActionSets = newActiveActionSets.data();
                    chainSyncInfo.countActiveActionSets = nextActionSetSlot;
                }
            }

            const XrResult result =
                chainSyncInfo.countActiveActionSets ? OpenXrApi::xrSyncActions(session, &chainSyncInfo) : XR_SUCCESS;
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
            if (getInfo->type != XR_TYPE_ACTION_STATE_GET_INFO || state->type != XR_TYPE_ACTION_STATE_BOOLEAN) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrGetActionStateBoolean",
                              TLPArg(session, "Session"),
                              TLPArg(getInfo->action, "Action"),
                              TLArg(getPath(getInfo->subactionPath).c_str(), "SubactionPath"));

            if (m_handTracker && isVrSession(session) && getInfo->type == XR_TYPE_ACTION_STATE_GET_INFO &&
                state->type == XR_TYPE_ACTION_STATE_BOOLEAN) {
                m_performanceCounters.handTrackingTimer->start();
                if (m_handTracker->getActionState(*getInfo, *state)) {
                    m_performanceCounters.handTrackingTimer->stop();
                    m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer->query();

                    TraceLoggingWrite(g_traceProvider,
                                      "xrGetActionStateBoolean",
                                      TLArg(!!state->isActive, "Active"),
                                      TLArg(!!state->currentState, "CurrentState"),
                                      TLArg(!!state->changedSinceLastSync, "ChangedSinceLastSync"),
                                      TLArg(state->lastChangeTime, "LastChangeTime"));

                    return XR_SUCCESS;
                }
            }

            return OpenXrApi::xrGetActionStateBoolean(session, getInfo, state);
        }

        XrResult xrGetActionStateFloat(XrSession session,
                                       const XrActionStateGetInfo* getInfo,
                                       XrActionStateFloat* state) override {
            if (getInfo->type != XR_TYPE_ACTION_STATE_GET_INFO || state->type != XR_TYPE_ACTION_STATE_FLOAT) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrGetActionStateFloat",
                              TLPArg(session, "Session"),
                              TLPArg(getInfo->action, "Action"),
                              TLArg(getPath(getInfo->subactionPath).c_str(), "SubactionPath"));

            if (m_handTracker && isVrSession(session) && getInfo->type == XR_TYPE_ACTION_STATE_GET_INFO &&
                state->type == XR_TYPE_ACTION_STATE_FLOAT) {
                m_performanceCounters.handTrackingTimer->start();
                if (m_handTracker->getActionState(*getInfo, *state)) {
                    m_performanceCounters.handTrackingTimer->stop();
                    m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer->query();

                    TraceLoggingWrite(g_traceProvider,
                                      "xrGetActionStateFloat",
                                      TLArg(!!state->isActive, "Active"),
                                      TLArg(state->currentState, "CurrentState"),
                                      TLArg(!!state->changedSinceLastSync, "ChangedSinceLastSync"),
                                      TLArg(state->lastChangeTime, "LastChangeTime"));

                    return XR_SUCCESS;
                }
            }

            return OpenXrApi::xrGetActionStateFloat(session, getInfo, state);
        }

        XrResult xrGetActionStatePose(XrSession session,
                                      const XrActionStateGetInfo* getInfo,
                                      XrActionStatePose* state) override {
            if (getInfo->type != XR_TYPE_ACTION_STATE_GET_INFO || state->type != XR_TYPE_ACTION_STATE_POSE) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrGetActionStatePose",
                              TLPArg(session, "Session"),
                              TLPArg(getInfo->action, "Action"),
                              TLArg(getPath(getInfo->subactionPath).c_str(), "SubactionPath"));

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
                if (supportedPath && m_handTracker->isHandEnabled(hand)) {
                    state->isActive = m_handTracker->isTrackedRecently(hand);
                    m_performanceCounters.handTrackingTimer->stop();
                    m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer->query();

                    TraceLoggingWrite(g_traceProvider, "xrGetActionStatePose", TLArg(!!state->isActive, "Active"));

                    return XR_SUCCESS;
                }
            }

            return OpenXrApi::xrGetActionStatePose(session, getInfo, state);
        }

        XrResult xrApplyHapticFeedback(XrSession session,
                                       const XrHapticActionInfo* hapticActionInfo,
                                       const XrHapticBaseHeader* hapticFeedback) override {
            if (hapticActionInfo->type != XR_TYPE_HAPTIC_ACTION_INFO) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrApplyHapticFeedback",
                              TLPArg(session, "Session"),
                              TLPArg(hapticActionInfo->action, "Action"),
                              TLArg(getPath(hapticActionInfo->subactionPath).c_str(), "SubactionPath"));

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
                    m_handTracker->handleOutput(hand, haptics->frequency ? haptics->frequency : 1, haptics->duration);
                    m_performanceCounters.handTrackingTimer->stop();
                    m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer->query();
                }
            }

            return OpenXrApi::xrApplyHapticFeedback(session, hapticActionInfo, hapticFeedback);
        }

        XrResult xrStopHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo) override {
            if (hapticActionInfo->type != XR_TYPE_HAPTIC_ACTION_INFO) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrStopHapticFeedback",
                              TLPArg(session, "Session"),
                              TLPArg(hapticActionInfo->action, "Action"),
                              TLArg(getPath(hapticActionInfo->subactionPath).c_str(), "SubactionPath"));

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
            if ((frameWaitInfo && frameWaitInfo->type != XR_TYPE_FRAME_WAIT_INFO) ||
                frameState->type != XR_TYPE_FRAME_STATE) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider, "xrWaitFrame", TLPArg(session, "Session"));

            const auto lastFrameWaitTimestamp = m_lastFrameWaitTimestamp;
            if (isVrSession(session)) {
                if (m_graphicsDevice) {
                    m_performanceCounters.appCpuTimer->stop();
                    m_stats.appCpuTimeUs += m_performanceCounters.appCpuTimer->query();
                }

                // Do throttling if needed.
                if (m_isFrameThrottlingPossible) {
                    const auto frameThrottling = m_configManager->getValue(config::SettingFrameThrottling);
                    if (frameThrottling < config::MaxFrameRate) {
                        // TODO: Try to reduce latency by slowing slewing to reduce the predictedDisplayTime.

                        const auto target =
                            m_lastFrameWaitTimestamp +
                            std::chrono::microseconds(1000000 / frameThrottling + m_frameThrottleSleepOffset) -
                            500us /* "running start" */;
                        std::this_thread::sleep_until(target);
                    }
                }
                m_lastFrameWaitTimestamp = std::chrono::steady_clock::now();

                m_performanceCounters.waitCpuTimer->start();
            }

            std::unique_lock lock(m_frameLock);

            XrResult result = XR_ERROR_RUNTIME_FAILURE;
            if (isVrSession(session) && m_asyncWaitPromise.valid()) {
                TraceLoggingWrite(g_traceProvider, "AsyncWaitMode");

                // In Turbo mode, we accept pipelining of exactly one frame.
                if (m_asyncWaitPolled) {
                    TraceLocalActivity(local);

                    // On second frame poll, we must wait.
                    TraceLoggingWriteStart(local, "AsyncWaitNow");
                    m_asyncWaitPromise.wait();
                    TraceLoggingWriteStop(local, "AsyncWaitNow");
                }
                m_asyncWaitPolled = true;

                // In Turbo mode, we don't actually wait, we make up a predicted time.
                std::unique_lock lock(m_asyncWaitLock);
                frameState->predictedDisplayTime =
                    m_asyncWaitCompleted
                        ? m_lastPredictedDisplayTime
                        : (m_lastPredictedDisplayTime + (m_lastFrameWaitTimestamp - lastFrameWaitTimestamp).count());
                frameState->predictedDisplayPeriod = m_lastPredictedDisplayPeriod;
                frameState->shouldRender = XR_TRUE;
                result = XR_SUCCESS;
            } else {
                lock.unlock();
                result = OpenXrApi::xrWaitFrame(session, frameWaitInfo, frameState);
                lock.lock();

                if (XR_SUCCEEDED(result)) {
                    // We must always store those values to properly handle transitions into Turbo Mode.
                    m_lastPredictedDisplayTime = frameState->predictedDisplayTime;
                    m_lastPredictedDisplayPeriod = frameState->predictedDisplayPeriod;
                }
            }
            if (XR_SUCCEEDED(result) && isVrSession(session)) {
                m_performanceCounters.waitCpuTimer->stop();
                m_stats.waitCpuTimeUs += m_performanceCounters.waitCpuTimer->query();

                m_savedFrameTime1 = frameState->predictedDisplayTime;

                // Apply prediction dampening if possible and if needed.
                if (m_hasPerformanceCounterKHR) {
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

                // Per OpenXR spec, the predicted display must increase monotonically.
                frameState->predictedDisplayTime = std::max(frameState->predictedDisplayTime, m_waitedFrameTime + 1);

                // Record the predicted display time.
                m_waitedFrameTime = frameState->predictedDisplayTime;

                if (m_graphicsDevice) {
                    m_performanceCounters.appCpuTimer->start();
                }

                m_stats.isFramePipeliningDetected = m_isInFrame;

                TraceLoggingWrite(g_traceProvider,
                                  "xrWaitFrame",
                                  TLArg(!!frameState->shouldRender, "ShouldRender"),
                                  TLArg(frameState->predictedDisplayTime, "PredictedDisplayTime"),
                                  TLArg(frameState->predictedDisplayPeriod, "PredictedDisplayPeriod"));
            }

            return result;
        }

        XrResult xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo) override {
            if (frameBeginInfo && frameBeginInfo->type != XR_TYPE_FRAME_BEGIN_INFO) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider, "xrBeginFrame", TLPArg(session, "Session"));

            std::unique_lock lock(m_frameLock);

            // Release the swapchain images. Some runtimes don't seem to look cross-frame releasing and this can happen
            // when a frame is discarded.
            for (auto& swapchain : m_swapchains) {
                if (swapchain.second.delayedRelease) {
                    TraceLoggingWrite(g_traceProvider, "ForcedSwapchainRelease", TLPArg(swapchain.first, "Swapchain"));

                    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                    swapchain.second.delayedRelease = false;
                    CHECK_XRCMD(OpenXrApi::xrReleaseSwapchainImage(swapchain.first, &releaseInfo));
                }
            }

            XrResult result = XR_ERROR_RUNTIME_FAILURE;
            if (isVrSession(session) && m_asyncWaitPromise.valid()) {
                // In turbo mode, we do nothing here.
                TraceLoggingWrite(g_traceProvider, "AsyncWaitMode");
                result = XR_SUCCESS;
            } else {
                result = OpenXrApi::xrBeginFrame(session, frameBeginInfo);
            }
            if (XR_SUCCEEDED(result) && isVrSession(session)) {
                // Record the predicted display time.
                m_begunFrameTime = m_waitedFrameTime;
                m_savedFrameTime2 = m_savedFrameTime1;
                m_isInFrame = true;

                if (m_graphicsDevice) {
                    m_performanceCounters.renderCpuTimer->start();
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

                if (m_eyeTracker || m_handTracker) {
                    // Force artifical syncing of actions if the app doesn't seem to use actions.
                    if (!m_isActionSetUsed) {
                        if (m_eyeTracker && m_eyeTracker->getActionSet() != XR_NULL_HANDLE) {
                            if (!m_isActionSetAttached) {
                                XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
                                CHECK_XRCMD(xrAttachSessionActionSets(m_vrSession, &attachInfo));
                                m_isActionSetAttached = true;
                            }

                            // The app does not implement controller support, we must sync actions ourselves.

                            // Workaround: the eye tracker on Varjo does not seem to connect unless the app calls
                            // xrPollEvent(). So we make a call here if the app does not call it. A disciplined app
                            // should have called xrPollEvent() by now just to begin the session.
                            if (m_needVarjoPollEventWorkaround) {
                                XrEventDataBuffer buf{XR_TYPE_EVENT_DATA_BUFFER};
                                OpenXrApi::xrPollEvent(GetXrInstance(), &buf);
                            }
                        }

                        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
                        CHECK_XRCMD(xrSyncActions(m_vrSession, &syncInfo));
                    }

                    if (m_eyeTracker) {
                        m_eyeTracker->beginFrame(m_begunFrameTime);
                    }
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

            if (m_variableRateShader) {
                m_stats.actualRenderWidth = m_variableRateShader->getActualRenderWidth();
            }

            if (m_frameAnalyzer) {
                m_stats.frameAnalyzerHeuristic = m_frameAnalyzer->getCurrentHeuristic();
            }

            if (m_configManager->hasChanged(config::SettingRecordStats)) {
                if (m_configManager->getValue(config::SettingRecordStats)) {
                    const std::time_t now = std::time(nullptr);
                    char buf[1024];
                    std::strftime(buf, sizeof(buf), "stats_%Y%m%d_%H%M%S", std::localtime(&now));
                    std::string logFile = (localAppData / "stats" / (std::string(buf) + ".csv")).string();
                    m_logStats.open(logFile, std::ios_base::ate);

                    // Write headers.
                    m_logStats << "time,FPS,appCPU (us),renderCPU (us),appGPU (us),VRAM (MB),VRAM (%)\n";
                } else {
                    m_logStats.close();
                }
            }

            const bool highRate = m_configManager->getValue(config::SettingHighRateStats);
            if ((now - m_performanceCounters.lastWindowStart) >= (highRate ? 100ms : 1s)) {
                const auto duration = now - m_performanceCounters.lastWindowStart;
                m_performanceCounters.numFrames = 0;
                m_performanceCounters.lastWindowStart = now;

                // TODO: no need to compute these if no menu handler
                // or if menu isn't displaying any stats.

                // Push the last averaged statistics.
                m_stats.appCpuTimeUs /= numFrames;
                m_stats.renderCpuTimeUs /= numFrames;
                m_stats.appGpuTimeUs /= numFrames;
                m_stats.waitCpuTimeUs /= numFrames;
                m_stats.endFrameCpuTimeUs /= numFrames;
                m_stats.processorGpuTimeUs[0] /= numFrames;
                m_stats.processorGpuTimeUs[1] /= numFrames;
                m_stats.overlayCpuTimeUs /= numFrames;
                m_stats.overlayGpuTimeUs /= numFrames;
                m_stats.handTrackingCpuTimeUs /= numFrames;
                m_stats.predictionTimeUs /= numFrames;
                if (highRate) {
                    // We must still do a rolling average for the FPS otherwise the values are all over the place.
                    m_performanceCounters.frameRates.push_front(std::make_pair(duration, numFrames));
                    m_performanceCounters.framesInPeriod += numFrames;
                    m_performanceCounters.timePeriod += duration;
                    while (m_performanceCounters.frameRates.size() > 10) {
                        m_performanceCounters.framesInPeriod -= m_performanceCounters.frameRates.back().second;
                        m_performanceCounters.timePeriod -= m_performanceCounters.frameRates.back().first;
                        m_performanceCounters.frameRates.pop_back();
                    }
                    m_stats.fps =
                        m_performanceCounters.framesInPeriod * (1e9f / m_performanceCounters.timePeriod.count());
                } else {
                    m_stats.fps = static_cast<float>(numFrames);
                    m_performanceCounters.frameRates.clear();
                    m_performanceCounters.framesInPeriod = 0;
                    m_performanceCounters.timePeriod = 0s;
                }

                m_graphicsDevice->getVRAMUsage(m_stats.vramUsedSize, m_stats.vramUsedPercent);

                // When CPU-bound, do not bother giving a (false) GPU time for D3D12
                if (m_graphicsDevice->getApi() == graphics::Api::D3D12 &&
                    m_stats.appCpuTimeUs + 500 > m_stats.appGpuTimeUs) {
                    m_stats.appGpuTimeUs = 0;
                }

                if (m_menuHandler) {
                    m_menuHandler->updateStatistics(m_stats);
                }

                if (m_logStats.is_open()) {
                    const std::time_t now = std::time(nullptr);

                    char buf[1024];
                    size_t offset = std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z", std::localtime(&now));
                    m_logStats << buf << "," << std::fixed << std::setprecision(1) << m_stats.fps << ","
                               << m_stats.appCpuTimeUs << "," << m_stats.renderCpuTimeUs << "," << m_stats.appGpuTimeUs
                               << "," << m_stats.vramUsedSize / (1024 * 1024) << "," << (int)m_stats.vramUsedPercent
                               << "\n";
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
            if ((m_upscaleMode == config::ScalingType::NIS || m_upscaleMode == config::ScalingType::FSR) &&
                m_configManager->hasChanged(config::SettingMipMapBias)) {
                m_graphicsDevice->setMipMapBias(
                    m_configManager->getEnumValue<config::MipMapBias>(config::SettingMipMapBias),
                    m_mipMapBiasForUpscaling);
            }

            // Update HAM.
            if (m_configManager->hasChanged(config::SettingDisableHAM) ||
                m_configManager->hasChanged(config::SettingBlindEye)) {
                // Kick off HAM event.
                m_visibilityMaskEventIndex = 0;

                // The app might ignore the HAM events. We acknowledge the config change regardless.
                (void)m_configManager->getValue(config::SettingDisableHAM);
                (void)m_configManager->getEnumValue<config::BlindEye>(config::SettingBlindEye);
            }

            // Check to reload shaders.
            bool reloadShaders = false;
            if (m_configManager->hasChanged(config::SettingReloadShaders)) {
                if (m_configManager->getValue(config::SettingReloadShaders)) {
                    m_configManager->setValue(config::SettingReloadShaders, 0, true);
                    reloadShaders = true;
                }
            }

            // Refresh the configuration.
            if (m_upscaler) {
                if (reloadShaders) {
                    m_upscaler->reload();
                }
                m_upscaler->update();
            }
            if (reloadShaders) {
                m_postProcessor->reload();
            }
            m_postProcessor->update();

            if (m_eyeTracker) {
                m_eyeTracker->update();
            }

            if (m_variableRateShader) {
                m_variableRateShader->update();
            }
        }

        void takeScreenshot(std::shared_ptr<graphics::ITexture> texture,
                            const std::string& suffix,
                            const XrRect2Di& viewport) const {
            // Stamp the overlay/menu if it's active.
            if (m_menuHandler) {
                m_graphicsDevice->setRenderTargets(1, &texture, nullptr, &viewport);
                m_graphicsDevice->beginText(true /* mustKeepOldContent */);
                m_menuHandler->render(texture);
                m_graphicsDevice->flushText();

                m_graphicsDevice->unsetRenderTargets();
            }

            SYSTEMTIME st;
            ::GetLocalTime(&st);

            std::stringstream parameters;
            parameters << '_' << ((st.wYear * 10000u) + (st.wMonth * 100u) + (st.wDay)) << '_'
                       << ((st.wHour * 10000u) + (st.wMinute * 100u) + (st.wSecond));

            if (m_upscaleMode != config::ScalingType::None) {
                // TODO: add a getUpscaleModeName() helper to keep enum and string in sync.
                const auto upscaleName = m_upscaleMode == config::ScalingType::NIS   ? "_NIS_"
                                         : m_upscaleMode == config::ScalingType::FSR ? "_FSR_"
                                         : m_upscaleMode == config::ScalingType::CAS ? "_CAS_"
                                                                                     : "_SCL_";
                parameters << upscaleName << m_settingScaling << "_"
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

            // Handle VPRT.
            auto info = texture->getInfo();
            if (info.arraySize > 1 || viewport.offset.x || viewport.offset.y || info.width != viewport.extent.width ||
                info.height != viewport.extent.height) {
                const auto srcSlice = (info.arraySize > 1 && suffix == "R") ? 1 : 0;
                info.arraySize = 1;
                info.width = viewport.extent.width;
                info.height = viewport.extent.height;
                auto cropped = m_graphicsDevice->createTexture(info, "Screenshot");
                texture->copyTo(viewport.offset.x, viewport.offset.y, srcSlice, cropped);
                texture = cropped;
            }

            texture->saveToFile(path);
        }

        XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) override {
            if (frameEndInfo->type != XR_TYPE_FRAME_END_INFO) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrEndFrame",
                              TLPArg(session, "Session"),
                              TLArg(frameEndInfo->displayTime, "DisplayTime"),
                              TLArg(xr::ToCString(frameEndInfo->environmentBlendMode), "EnvironmentBlendMode"));

            if (!isVrSession(session) || !m_graphicsDevice) {
                return OpenXrApi::xrEndFrame(session, frameEndInfo);
            }

            std::unique_lock lock(m_frameLock);

            m_isInFrame = false;

            updateStatisticsForFrame();

            m_performanceCounters.renderCpuTimer->stop();
            m_stats.renderCpuTimeUs += m_performanceCounters.renderCpuTimer->query();
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
            uint32_t sliceForOverlay[utilities::ViewCount];
            std::shared_ptr<graphics::ITexture> depthForOverlay[utilities::ViewCount] = {};
            xr::math::ViewProjection viewForOverlay[utilities::ViewCount];
            XrRect2Di viewportForOverlay[utilities::ViewCount];
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

                    TraceLoggingWrite(g_traceProvider,
                                      "xrEndFrame_Layer",
                                      TLArg("Proj", "Type"),
                                      TLArg(proj->layerFlags, "Flags"),
                                      TLPArg(proj->space, "Space"));

                    // To patch the resolution of the layer we need to recreate the whole projection & views
                    // data structures.
                    auto correctedProjectionLayer = &layerProjectionAllocator.emplace_back(*proj);
                    auto correctedProjectionViews = layerProjectionViewsAllocator
                                                        .emplace_back(std::array<XrCompositionLayerProjectionView, 2>(
                                                            {proj->views[0], proj->views[1]}))
                                                        .data();

                    // When using texture arrays, we assume both eyes are submitted from the same swapchain.
                    static_assert(utilities::ViewCount == 2);
                    const bool useTextureArrays =
                        proj->views[0].subImage.swapchain == proj->views[1].subImage.swapchain &&
                        proj->views[0].subImage.imageArrayIndex != proj->views[1].subImage.imageArrayIndex;
                    const bool useDoubleWide = proj->views[0].subImage.swapchain == proj->views[1].subImage.swapchain &&
                                               proj->views[1].subImage.imageRect.offset.x != 0;
                    // TODO: Here we assume that left is always "first" (either slice 0 or "left-most" viewport).

                    if (useTextureArrays || useDoubleWide || m_overrideFoveatedRenderingCapability) {
                        // Assume that we've properly distinguished left/right eyes.
                        m_stats.hasColorBuffer[(int)utilities::Eye::Left] =
                            m_stats.hasColorBuffer[(int)utilities::Eye::Right] = true;
                    }

                    assert(proj->viewCount == utilities::ViewCount);
                    for (uint32_t eye = 0; eye < utilities::ViewCount; eye++) {
                        const XrCompositionLayerProjectionView& view = proj->views[eye];

                        TraceLoggingWrite(g_traceProvider,
                                          "xrEndFrame_View",
                                          TLArg("Proj", "Type"),
                                          TLArg(eye, "Index"),
                                          TLPArg(proj->views[eye].subImage.swapchain, "Swapchain"),
                                          TLArg(proj->views[eye].subImage.imageArrayIndex, "ImageArrayIndex"),
                                          TLArg(xr::ToString(proj->views[eye].subImage.imageRect).c_str(), "ImageRect"),
                                          TLArg(xr::ToString(proj->views[eye].pose).c_str(), "Pose"),
                                          TLArg(xr::ToString(proj->views[eye].fov).c_str(), "Fov"));

                        auto swapchainIt = m_swapchains.find(view.subImage.swapchain);
                        if (swapchainIt == m_swapchains.end()) {
                            throw std::runtime_error("Swapchain is not registered");
                        }
                        auto& swapchainState = swapchainIt->second;
                        auto& swapchainImages = swapchainState.images[swapchainState.acquiredImageIndex];

                        // Look for the depth buffer.
                        std::shared_ptr<graphics::ITexture> depthBuffer;
                        NearFar nearFar{0.001f, 100.f};
                        const XrBaseInStructure* entry = reinterpret_cast<const XrBaseInStructure*>(view.next);
                        while (entry) {
                            if (entry->type == XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR) {
                                const XrCompositionLayerDepthInfoKHR* depth =
                                    reinterpret_cast<const XrCompositionLayerDepthInfoKHR*>(entry);

                                TraceLoggingWrite(g_traceProvider,
                                                  "xrEndFrame_View",
                                                  TLArg("Depth", "Type"),
                                                  TLArg(eye, "Index"),
                                                  TLPArg(depth->subImage.swapchain, "Swapchain"),
                                                  TLArg(depth->subImage.imageArrayIndex, "ImageArrayIndex"),
                                                  TLArg(xr::ToString(depth->subImage.imageRect).c_str(), "ImageRect"),
                                                  TLArg(depth->nearZ, "Near"),
                                                  TLArg(depth->farZ, "Far"),
                                                  TLArg(depth->minDepth, "MinDepth"),
                                                  TLArg(depth->maxDepth, "MaxDepth"));

                                // The order of color/depth textures must match.
                                if (depth->subImage.imageArrayIndex == view.subImage.imageArrayIndex) {
                                    auto depthSwapchainIt = m_swapchains.find(depth->subImage.swapchain);
                                    if (depthSwapchainIt == m_swapchains.end()) {
                                        throw std::runtime_error("Swapchain is not registered");
                                    }
                                    auto& depthSwapchainState = depthSwapchainIt->second;

                                    depthBuffer =
                                        depthSwapchainState.images[depthSwapchainState.acquiredImageIndex].appTexture;
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
                        // TODO: We always assume that if texture arrays are used, left eye is texture 0 and right eye
                        // is texture 1. I'm sure this holds in like 99% of the applications, but still not very clean
                        // to assume.
                        if (m_frameAnalyzer && !useTextureArrays && !swapchainState.registeredWithFrameAnalyzer) {
                            for (const auto& image : swapchainState.images) {
                                m_frameAnalyzer->registerColorSwapchainImage(
                                    view.subImage.swapchain, image.appTexture, (utilities::Eye)eye);
                            }
                            swapchainState.registeredWithFrameAnalyzer = true;
                        }

                        // Detect whether the input uses a viewport (VP) or a texture array render target (RT)
                        const bool isVPRT =
                            view.subImage.imageArrayIndex > 0 || view.subImage.imageRect.offset.x ||
                            view.subImage.imageRect.offset.y ||
                            view.subImage.imageRect.extent.width != swapchainImages.appTexture->getInfo().width ||
                            view.subImage.imageRect.extent.height != swapchainImages.appTexture->getInfo().height ||
                            m_configManager->getValue("force_vprt_path");

                        std::shared_ptr<graphics::ITexture> nextInput = swapchainImages.appTexture;
                        std::shared_ptr<graphics::ITexture> finalOutput = swapchainImages.runtimeTexture;

                        float horizontalScaleFactor = 1.f;
                        float verticalScaleFactor = 1.f;
                        uint32_t scaledOutputWidth = view.subImage.imageRect.extent.width;
                        uint32_t scaledOutputHeight = view.subImage.imageRect.extent.height;
                        if (m_upscaleMode == config::ScalingType::NIS || m_upscaleMode == config::ScalingType::FSR) {
                            std::tie(horizontalScaleFactor, verticalScaleFactor) =
                                config::GetScalingFactors(m_settingScaling, m_settingAnamorphic);

                            scaledOutputWidth = roundUp(
                                (uint32_t)std::ceil(view.subImage.imageRect.extent.width * horizontalScaleFactor), 2);
                            scaledOutputHeight = roundUp(
                                (uint32_t)std::ceil(view.subImage.imageRect.extent.height * verticalScaleFactor), 2);
                        }

                        // Copy the VPRT app input into an intermediate buffer if needed.
                        // TODO: This is a naive solution to uniformely support the same time of input/output for all
                        // upscalers and post-processor.
                        if (isVPRT) {
                            if (!swapchainState.nonVPRTInputTexture ||
                                view.subImage.imageRect.extent.width !=
                                    swapchainState.nonVPRTInputTexture->getInfo().width ||
                                view.subImage.imageRect.extent.height !=
                                    swapchainState.nonVPRTInputTexture->getInfo().height) {
                                auto createInfo = swapchainImages.appTexture->getInfo();

                                // Single-surface, full (input) screen.
                                createInfo.arraySize = 1;
                                createInfo.width = view.subImage.imageRect.extent.width;
                                createInfo.height = view.subImage.imageRect.extent.height;
                                createInfo.mipCount = 1;

                                // Will be copied to from the app swapchain. Then both upscaler or post-processor will
                                // sample.
                                createInfo.usageFlags =
                                    XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

                                swapchainState.nonVPRTInputTexture =
                                    m_graphicsDevice->createTexture(createInfo, "Non-VPRT Input TEX2D");
                            }

                            // Patch the top-left corner offset.
                            if (m_upscaleMode == config::ScalingType::NIS ||
                                m_upscaleMode == config::ScalingType::FSR) {
                                correctedProjectionViews[eye].subImage.imageRect.offset.x = (uint32_t)std::ceil(
                                    correctedProjectionViews[eye].subImage.imageRect.offset.x * horizontalScaleFactor);
                                correctedProjectionViews[eye].subImage.imageRect.offset.y = (uint32_t)std::ceil(
                                    correctedProjectionViews[eye].subImage.imageRect.offset.y * verticalScaleFactor);
                            }

                            // Small adjustments to avoid pixel off-texture due to rounding error.
                            if (correctedProjectionViews[eye].subImage.imageRect.offset.x + scaledOutputWidth >
                                swapchainImages.runtimeTexture->getInfo().width) {
                                scaledOutputWidth = swapchainImages.runtimeTexture->getInfo().width -
                                                    correctedProjectionViews[eye].subImage.imageRect.offset.x;
                            }
                            if (correctedProjectionViews[eye].subImage.imageRect.offset.y + scaledOutputHeight >
                                swapchainImages.runtimeTexture->getInfo().height) {
                                scaledOutputHeight = swapchainImages.runtimeTexture->getInfo().height -
                                                     correctedProjectionViews[eye].subImage.imageRect.offset.y;
                            }

                            if (!swapchainState.nonVPRTOutputTexture ||
                                scaledOutputWidth != swapchainState.nonVPRTOutputTexture->getInfo().width ||
                                scaledOutputHeight != swapchainState.nonVPRTOutputTexture->getInfo().height) {
                                auto createInfo = swapchainImages.appTexture->getInfo();

                                // Single-surface, full (output) screen.
                                createInfo.arraySize = 1;
                                createInfo.width = scaledOutputWidth;
                                createInfo.height = scaledOutputHeight;
                                createInfo.mipCount = 1;

                                // Post-processor will draw a full-screen quad. Then will be copied from into the
                                // runtime swapchain.
                                createInfo.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT |
                                                        XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                                                        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

                                swapchainState.nonVPRTOutputTexture =
                                    m_graphicsDevice->createTexture(createInfo, "Non-VPRT Output TEX2D");
                            }

                            swapchainImages.appTexture->copyTo(view.subImage.imageRect.offset.x,
                                                               view.subImage.imageRect.offset.y,
                                                               view.subImage.imageArrayIndex,
                                                               swapchainState.nonVPRTInputTexture);

                            nextInput = swapchainState.nonVPRTInputTexture;
                            finalOutput = swapchainState.nonVPRTOutputTexture;
                        }

                        // Perform upscaling.
                        if (m_upscaler) {
                            if (!swapchainState.upscaledTexture ||
                                scaledOutputWidth != swapchainState.upscaledTexture->getInfo().width ||
                                scaledOutputHeight != swapchainState.upscaledTexture->getInfo().height) {
                                auto createInfo = swapchainImages.appTexture->getInfo();

                                // Single-surface, full (output) screen.
                                createInfo.arraySize = 1;
                                createInfo.width = scaledOutputWidth;
                                createInfo.height = scaledOutputHeight;

                                // Upscaler will write to as UAV. Then the post-processor will sample.
                                createInfo.usageFlags =
                                    XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

                                if (m_graphicsDevice->isTextureFormatSRGB(createInfo.format)) {
                                    // Good balance between visuals and performance.
                                    createInfo.format =
                                        m_graphicsDevice->getTextureFormat(graphics::TextureFormat::R10G10B10A2_UNORM);
                                }

                                swapchainState.upscaledTexture =
                                    m_graphicsDevice->createTexture(createInfo, "Upscaled TEX2D");
                            }

                            auto timer = swapchainImages.upscalingTimers[eye].get();
                            m_stats.processorGpuTimeUs[0] += timer->query();

                            timer->start();
                            m_upscaler->process(nextInput,
                                                swapchainState.upscaledTexture,
                                                swapchainState.upscalerTextures,
                                                swapchainState.upscalerBlob);
                            timer->stop();

                            nextInput = swapchainState.upscaledTexture;
                        }

                        // Do post-processing and color conversion.
                        {
                            auto timer = swapchainImages.postProcessingTimers[eye].get();
                            m_stats.processorGpuTimeUs[1] += timer->query();

                            timer->start();
                            m_postProcessor->process(nextInput,
                                                     finalOutput,
                                                     swapchainState.postProcessorTextures,
                                                     swapchainState.postProcessorBlob);
                            timer->stop();
                        }

                        // Copy the output back into the VPRT runtime swapchain is needed.
                        if (finalOutput != swapchainImages.runtimeTexture) {
                            finalOutput->copyTo(swapchainImages.runtimeTexture,
                                                correctedProjectionViews[eye].subImage.imageRect.offset.x,
                                                correctedProjectionViews[eye].subImage.imageRect.offset.y,
                                                view.subImage.imageArrayIndex);
                        }

                        // Patch the resolution.
                        correctedProjectionViews[eye].subImage.imageRect.extent.width = scaledOutputWidth;
                        correctedProjectionViews[eye].subImage.imageRect.extent.height = scaledOutputHeight;

                        textureForOverlay[eye] = swapchainImages.runtimeTexture;
                        sliceForOverlay[eye] = view.subImage.imageArrayIndex;
                        depthForOverlay[eye] = depthBuffer;

                        // Patch the eye poses.
                        if (m_configManager->getValue("canting")) {
                            correctedProjectionViews[eye].pose = m_posesForFrame[eye].pose;
                        }

                        // Patch the FOV if it was overriden.
                        const auto fovOverrideMode = m_configManager->peekValue(config::SettingFOVType);
                        if ((fovOverrideMode == 0 && m_configManager->peekValue(config::SettingFOV) != 100) ||
                            fovOverrideMode == 1 || m_configManager->peekValue(config::SettingZoom) != 10) {
                            const bool yflip = correctedProjectionViews[eye].fov.angleDown > 0 &&
                                               correctedProjectionViews[eye].fov.angleUp < 0;

                            correctedProjectionViews[eye].fov = m_posesForFrame[eye].fov;

                            // Some apps might modify the FOV, which we don't support in this override case. We
                            // accomodate the most common case here, which is the Y-flip case. For other (uncommon)
                            // cases, we expect the user to not use the override.
                            if (yflip) {
                                std::swap(correctedProjectionViews[eye].fov.angleDown,
                                          correctedProjectionViews[eye].fov.angleUp);
                            }
                        }

                        viewForOverlay[eye].Pose = correctedProjectionViews[eye].pose;
                        viewForOverlay[eye].Fov = correctedProjectionViews[eye].fov;
                        viewForOverlay[eye].NearFar = nearFar;
                        viewportForOverlay[eye] = correctedProjectionViews[eye].subImage.imageRect;
                    }

                    const int icdOverride = m_configManager->getValue(config::SettingICD);
                    if (icdOverride != 1000) {
                        // Restore the original IPD to avoid reprojection being confused.
                        const auto vec =
                            correctedProjectionViews[1].pose.position - correctedProjectionViews[0].pose.position;
                        const auto ipd = Length(vec);
                        const float icd = (ipd * std::max(icdOverride, 1)) / 1000;

                        const auto center = correctedProjectionViews[0].pose.position + (vec * 0.5f);
                        const auto offset = Normalize(vec) * (icd * 0.5f);
                        correctedProjectionViews[0].pose.position = center - offset;
                        correctedProjectionViews[1].pose.position = center + offset;
                    }

                    spaceForOverlay = proj->space;

                    for (uint32_t eye = 0; eye < utilities::ViewCount; eye++) {
                        TraceLoggingWrite(
                            g_traceProvider,
                            "CorrectedView",
                            TLArg("Proj", "Type"),
                            TLArg(eye, "Index"),
                            TLPArg(correctedProjectionViews[eye].subImage.swapchain, "Swapchain"),
                            TLArg(correctedProjectionViews[eye].subImage.imageArrayIndex, "ImageArrayIndex"),
                            TLArg(xr::ToString(correctedProjectionViews[eye].subImage.imageRect).c_str(), "ImageRect"),
                            TLArg(xr::ToString(correctedProjectionViews[eye].pose).c_str(), "Pose"),
                            TLArg(xr::ToString(correctedProjectionViews[eye].fov).c_str(), "Fov"));
                    }

                    correctedProjectionLayer->views = correctedProjectionViews;
                    correctedLayers.push_back(
                        reinterpret_cast<const XrCompositionLayerBaseHeader*>(correctedProjectionLayer));
                } else if (chainFrameEndInfo.layers[i]->type == XR_TYPE_COMPOSITION_LAYER_QUAD) {
                    const XrCompositionLayerQuad* quad =
                        reinterpret_cast<const XrCompositionLayerQuad*>(chainFrameEndInfo.layers[i]);

                    TraceLoggingWrite(g_traceProvider,
                                      "xrEndFrame_Layer",
                                      TLArg("Quad", "Type"),
                                      TLArg(quad->layerFlags, "Flags"),
                                      TLPArg(quad->space, "Space"));
                    TraceLoggingWrite(g_traceProvider,
                                      "xrEndFrame_View",
                                      TLArg("Quad", "Type"),
                                      TLPArg(quad->subImage.swapchain, "Swapchain"),
                                      TLArg(quad->subImage.imageArrayIndex, "ImageArrayIndex"),
                                      TLArg(xr::ToString(quad->subImage.imageRect).c_str(), "ImageRect"),
                                      TLArg(xr::ToString(quad->pose).c_str(), "Pose"),
                                      TLArg(quad->size.width, "Width"),
                                      TLArg(quad->size.height, "Height"),
                                      TLArg(xr::ToCString(quad->eyeVisibility), "EyeVisibility"));

                    auto swapchainIt = m_swapchains.find(quad->subImage.swapchain);
                    if (swapchainIt == m_swapchains.end()) {
                        throw std::runtime_error("Swapchain is not registered");
                    }

                    auto& swapchainState = swapchainIt->second;
                    auto& swapchainImages = swapchainState.images[swapchainState.acquiredImageIndex];

                    if (swapchainImages.appTexture != swapchainImages.runtimeTexture) {
                        swapchainImages.appTexture->copyTo(swapchainImages.runtimeTexture);
                    }

                    correctedLayers.push_back(chainFrameEndInfo.layers[i]);
                } else {
                    correctedLayers.push_back(chainFrameEndInfo.layers[i]);
                }
            }

            // We intentionally exclude the overlay from this timer, as it has its own separate timer.
            m_performanceCounters.endFrameCpuTimer->stop();

            // Render our overlays.
            bool needMenuSwapchainDelayedRelease = false;
            {
                const bool drawHands = m_handTracker && m_configManager->peekEnumValue<config::HandTrackingVisibility>(
                                                            config::SettingHandVisibilityAndSkinTone) !=
                                                            config::HandTrackingVisibility::Hidden;
                const bool drawEyeGaze = m_eyeTracker && m_configManager->getValue(config::SettingEyeDebug);

                m_stats.overlayCpuTimeUs += m_performanceCounters.overlayCpuTimer->query();
                m_stats.overlayGpuTimeUs +=
                    m_performanceCounters.overlayGpuTimer[m_performanceCounters.gpuTimerIndex]->query();

                m_performanceCounters.overlayCpuTimer->start();
                m_performanceCounters.overlayGpuTimer[m_performanceCounters.gpuTimerIndex]->start();

                if (textureForOverlay[0]) {
                    const bool useTextureArrays =
                        textureForOverlay[1] == textureForOverlay[0] && sliceForOverlay[0] != sliceForOverlay[1];

                    // Render the hands or eye gaze helper.
                    if (drawHands || drawEyeGaze) {
                        TraceLoggingWrite(
                            g_traceProvider, "StampOverlays", TLArg(drawHands, "Hands"), TLArg(drawEyeGaze, "EyeGaze"));

                        auto isEyeGazeValid = m_eyeTracker && m_eyeTracker->getProjectedGaze(m_eyeGaze);
                        const bool doHandOcclusion = m_configManager->getValue(config::SettingHandOcclusion);

                        for (uint32_t eye = 0; eye < utilities::ViewCount; eye++) {
                            m_graphicsDevice->setRenderTargets(
                                1,
                                &textureForOverlay[eye],
                                useTextureArrays ? reinterpret_cast<int32_t*>(&sliceForOverlay[eye]) : nullptr,
                                &viewportForOverlay[eye],
                                doHandOcclusion ? depthForOverlay[eye] : nullptr,
                                (doHandOcclusion && useTextureArrays) ? eye : -1);

                            m_graphicsDevice->setViewProjection(viewForOverlay[eye]);

                            if (drawHands) {
                                m_handTracker->render(
                                    viewForOverlay[eye].Pose, spaceForOverlay, getTimeNow(), textureForOverlay[eye]);
                            }

                            if (drawEyeGaze) {
                                XrColor4f color = isEyeGazeValid ? XrColor4f{0, 1, 0, 1} : XrColor4f{1, 0, 0, 1};
                                auto pos = utilities::NdcToScreen(m_eyeGaze[eye]);
                                pos.x = viewportForOverlay[eye].offset.x + pos.x * viewportForOverlay[eye].extent.width;
                                pos.y =
                                    viewportForOverlay[eye].offset.y + pos.y * viewportForOverlay[eye].extent.height;
                                m_graphicsDevice->clearColor(pos.y - 20, pos.x - 20, pos.y + 20, pos.x + 20, color);
                            }
                        }

                        m_graphicsDevice->unsetRenderTargets();
                    }
                }

                // Render the menu.
                if (m_menuHandler) {
                    if (!m_configManager->getValue(config::SettingMenuLegacyMode) && !m_configManager->isSafeMode()) {
                        if (m_menuHandler->isVisible() || m_menuLingering) {
                            TraceLoggingWrite(g_traceProvider, "OverlayMenu");

                            // Workaround: there is a bug in the WMR runtime that causes a past quad layer content
                            // to linger on the next projection layer. We make sure to submit a completely blank
                            // quad layer for 3 frames after its disappearance. The number 3 comes from the number
                            // of depth buffers cached inside the precompositor of the WMR runtime.
                            m_menuLingering = m_menuHandler->isVisible() ? 3 : m_menuLingering - 1;

                            uint32_t menuImageIndex;
                            {
                                XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                                CHECK_XRCMD(
                                    OpenXrApi::xrAcquireSwapchainImage(m_menuSwapchain, &acquireInfo, &menuImageIndex));

                                XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                                waitInfo.timeout = XR_INFINITE_DURATION;
                                CHECK_XRCMD(OpenXrApi::xrWaitSwapchainImage(m_menuSwapchain, &waitInfo));
                            }

                            const auto& textureInfo = m_menuSwapchainImages[menuImageIndex]->getInfo();

                            m_graphicsDevice->setRenderTargets(1, &m_menuSwapchainImages[menuImageIndex]);
                            m_graphicsDevice->beginText();
                            m_graphicsDevice->clearColor(
                                0, 0, (float)textureInfo.height, (float)textureInfo.width, XrColor4f{0, 0, 0, 0});
                            m_menuHandler->render(m_menuSwapchainImages[menuImageIndex]);
                            m_graphicsDevice->flushText();

                            m_graphicsDevice->unsetRenderTargets();

                            needMenuSwapchainDelayedRelease = true;

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
                            layerQuadForMenu.eyeVisibility =
                                visibility[std::min(m_configManager->getValue(config::SettingMenuEyeVisibility),
                                                    (int)std::size(visibility))];
                            layerQuadForMenu.subImage.swapchain = m_menuSwapchain;
                            layerQuadForMenu.subImage.imageRect.extent.width = textureInfo.width;
                            layerQuadForMenu.subImage.imageRect.extent.height = textureInfo.height;
                            layerQuadForMenu.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;

                            TraceLoggingWrite(g_traceProvider,
                                              "xrEndFrame_Layer",
                                              TLArg("Quad", "Type"),
                                              TLArg(layerQuadForMenu.layerFlags, "Flags"),
                                              TLPArg(layerQuadForMenu.space, "Space"));
                            TraceLoggingWrite(
                                g_traceProvider,
                                "xrEndFrame_View",
                                TLArg("Quad", "Type"),
                                TLPArg(layerQuadForMenu.subImage.swapchain, "Swapchain"),
                                TLArg(layerQuadForMenu.subImage.imageArrayIndex, "ImageArrayIndex"),
                                TLArg(xr::ToString(layerQuadForMenu.subImage.imageRect).c_str(), "ImageRect"),
                                TLArg(xr::ToString(layerQuadForMenu.pose).c_str(), "Pose"),
                                TLArg(layerQuadForMenu.size.width, "Width"),
                                TLArg(layerQuadForMenu.size.height, "Height"),
                                TLArg(xr::ToCString(layerQuadForMenu.eyeVisibility), "EyeVisibility"));

                            correctedLayers.push_back(
                                reinterpret_cast<XrCompositionLayerBaseHeader*>(&layerQuadForMenu));
                        }
                    } else {
                        // Legacy menu mode, for people having problems.
                        if (textureForOverlay[0]) {
                            TraceLoggingWrite(g_traceProvider, "StampMenu");

                            const bool useTextureArrays = textureForOverlay[1] == textureForOverlay[0] &&
                                                          sliceForOverlay[0] != sliceForOverlay[1];

                            for (uint32_t eye = 0; eye < utilities::ViewCount; eye++) {
                                m_graphicsDevice->setRenderTargets(
                                    1,
                                    &textureForOverlay[eye],
                                    useTextureArrays ? reinterpret_cast<int32_t*>(&sliceForOverlay[eye]) : nullptr,
                                    &viewportForOverlay[eye]);
                                m_graphicsDevice->beginText(true /* mustKeepOldContent */);
                                m_menuHandler->render(textureForOverlay[eye], (utilities::Eye)eye);
                                m_graphicsDevice->flushText();
                            }

                            m_graphicsDevice->unsetRenderTargets();
                        }
                    }
                }

                m_performanceCounters.overlayCpuTimer->stop();
                m_performanceCounters.overlayGpuTimer[m_performanceCounters.gpuTimerIndex]->stop();
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
                    takeScreenshot(textureForOverlay[0], "L", viewportForOverlay[0]);
                }
                if (textureForOverlay[1] &&
                    m_configManager->getValue(config::SettingScreenshotEye) != 1 /* Left only */) {
                    takeScreenshot(textureForOverlay[1], "R", viewportForOverlay[1]);
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
                    TraceLoggingWrite(g_traceProvider, "DelayedSwapchainRelease", TLPArg(swapchain.first, "Swapchain"));

                    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                    swapchain.second.delayedRelease = false;
                    CHECK_XRCMD(OpenXrApi::xrReleaseSwapchainImage(swapchain.first, &releaseInfo));
                }
            }
            if (needMenuSwapchainDelayedRelease) {
                TraceLoggingWrite(g_traceProvider, "MenuSwapchainRelease", TLPArg(m_menuSwapchain, "Swapchain"));

                XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                CHECK_XRCMD(OpenXrApi::xrReleaseSwapchainImage(m_menuSwapchain, &releaseInfo));
            }

            chainFrameEndInfo.layers = correctedLayers.data();
            chainFrameEndInfo.layerCount = (uint32_t)correctedLayers.size();

            // When using prediction dampening, we want to restore the display time in order to avoid confusing motion
            // reprojection.
            const bool isMotionReprojectionOn =
                m_supportMotionReprojectionLock &&
                m_configManager->getEnumValue<config::MotionReprojection>(config::SettingMotionReprojection) ==
                    config::MotionReprojection::On;
            if (m_hasPerformanceCounterKHR && m_configManager->getValue(config::SettingPredictionDampen) != 100 &&
                !isMotionReprojectionOn) {
                chainFrameEndInfo.displayTime = m_savedFrameTime2;
            }

            {
                if (m_asyncWaitPromise.valid()) {
                    TraceLocalActivity(local);

                    // This is the latest point we must have fully waited a frame before proceeding.
                    //
                    // Note: we should not wait infinitely here, however certain patterns of engine calls may cause us
                    // to attempt a "double xrWaitFrame" when turning on Turbo. Use a timeout to detect that, and
                    // refrain from enqueing a second wait further down. This isn't a pretty solution, but it is simple
                    // and it seems to work effectively (minus the 1s freeze observed in-game).
                    TraceLoggingWriteStart(local, "AsyncWaitNow");
                    const auto ready = m_asyncWaitPromise.wait_for(1s) == std::future_status::ready;
                    TraceLoggingWriteStop(local, "AsyncWaitNow", TLArg(ready, "Ready"));
                    if (ready) {
                        m_asyncWaitPromise = {};
                    }

                    CHECK_XRCMD(OpenXrApi::xrBeginFrame(m_vrSession, nullptr));
                }

                const auto result = OpenXrApi::xrEndFrame(session, &chainFrameEndInfo);

                m_graphicsDevice->unblockCallbacks();

                if (m_configManager->getValue(config::SettingTurboMode) && !m_asyncWaitPromise.valid()) {
                    m_asyncWaitPolled = false;
                    m_asyncWaitCompleted = false;

                    // In Turbo mode, we kick off a wait thread immediately.
                    TraceLoggingWrite(g_traceProvider, "AsyncWaitStart");
                    m_asyncWaitPromise = std::async(std::launch::async, [&] {
                        TraceLocalActivity(local);

                        XrFrameState frameState{XR_TYPE_FRAME_STATE};
                        TraceLoggingWriteStart(local, "AsyncWaitFrame");
                        CHECK_XRCMD(OpenXrApi::xrWaitFrame(m_vrSession, nullptr, &frameState));
                        TraceLoggingWriteStop(local,
                                              "AsyncWaitFrame",
                                              TLArg(frameState.predictedDisplayTime, "PredictedDisplayTime"),
                                              TLArg(frameState.predictedDisplayPeriod, "PredictedDisplayPeriod"));
                        {
                            std::unique_lock lock(m_asyncWaitLock);
                            m_lastPredictedDisplayTime = frameState.predictedDisplayTime;
                            m_lastPredictedDisplayPeriod = frameState.predictedDisplayPeriod;

                            m_asyncWaitCompleted = true;
                        }
                    });
                }

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
            if (path == XR_NULL_PATH) {
                return "";
            }

            char buf[XR_MAX_PATH_LENGTH];
            uint32_t count;
            CHECK_XRCMD(xrPathToString(GetXrInstance(), path, sizeof(buf), &count, buf));
            std::string str;
            str.assign(buf, count - 1);
            return str;
        }

        // Find the current time. Fallback to the frame time if we cannot query the actual time.
        XrTime getTimeNow() {
            XrTime xrTimeNow = m_begunFrameTime;
            if (m_hasPerformanceCounterKHR) {
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
        bool m_supportMotionReprojectionLock{false};
        bool m_isOmniceptDetected{false};
        bool m_hasPimaxEyeTracker{false};
        bool m_isFrameThrottlingPossible{true};
        bool m_overrideFoveatedRenderingCapability{false};

        std::mutex m_frameLock;
        XrTime m_waitedFrameTime;
        XrTime m_begunFrameTime;
        XrTime m_savedFrameTime1;
        XrTime m_savedFrameTime2;
        bool m_isInFrame{false};
        bool m_sendInterationProfileEvent{false};
        uint32_t m_visibilityMaskEventIndex{utilities::ViewCount};
        XrSpace m_viewSpace{XR_NULL_HANDLE};
        bool m_needCalibrateEyeProjections{true};
        XrVector2f m_projCenters[utilities::ViewCount];
        XrVector2f m_eyeGaze[utilities::ViewCount];
        XrView m_posesForFrame[utilities::ViewCount];
        std::chrono::time_point<std::chrono::steady_clock> m_lastFrameWaitTimestamp{};
        uint32_t m_frameThrottleSleepOffset{0};

        std::mutex m_asyncWaitLock;
        std::future<void> m_asyncWaitPromise;
        XrTime m_lastPredictedDisplayTime{0};
        XrTime m_lastPredictedDisplayPeriod{0};
        bool m_asyncWaitPolled{false};
        bool m_asyncWaitCompleted{false};

        std::shared_ptr<config::IConfigManager> m_configManager;

        std::shared_ptr<graphics::IDevice> m_graphicsDevice;
        std::map<XrSwapchain, SwapchainState> m_swapchains;

        config::ScalingType m_upscaleMode{config::ScalingType::None};
        int m_settingScaling{100};
        int m_settingAnamorphic{-100};
        float m_mipMapBiasForUpscaling{0.f};

        std::shared_ptr<graphics::IFrameAnalyzer> m_frameAnalyzer;
        std::shared_ptr<input::IEyeTracker> m_eyeTracker;
        bool m_isActionSetUsed{false};
        bool m_isActionSetAttached{false};
        bool m_needVarjoPollEventWorkaround{false};
        std::shared_ptr<input::IHandTracker> m_handTracker;

        std::shared_ptr<graphics::IImageProcessor> m_upscaler;
        std::shared_ptr<graphics::IImageProcessor> m_postProcessor;
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
            std::shared_ptr<utilities::ICpuTimer> renderCpuTimer;
            std::shared_ptr<graphics::IGpuTimer> appGpuTimer[GpuTimerLatency + 1];
            std::shared_ptr<utilities::ICpuTimer> waitCpuTimer;
            std::shared_ptr<utilities::ICpuTimer> endFrameCpuTimer;
            std::shared_ptr<utilities::ICpuTimer> overlayCpuTimer;
            std::shared_ptr<graphics::IGpuTimer> overlayGpuTimer[GpuTimerLatency + 1];
            std::shared_ptr<utilities::ICpuTimer> handTrackingTimer;

            unsigned int gpuTimerIndex{0};
            std::chrono::steady_clock::time_point lastWindowStart;
            std::deque<std::pair<std::chrono::steady_clock::duration, uint32_t>> frameRates;
            uint32_t framesInPeriod{0};
            std::chrono::steady_clock::duration timePeriod{0s};
            uint32_t numFrames{0};
        } m_performanceCounters;

        menu::MenuStatistics m_stats{};
        std::ofstream m_logStats;
        bool m_hasPerformanceCounterKHR{false};
        bool m_hasVisibilityMaskKHR{false};
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
