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

    // The xrWaitFrame() loop might cause to have 2 frames in-flight, so we want to delay the GPU timer re-use by those
    // 2 frames.
    constexpr uint32_t GpuTimerLatency = 2;

    using namespace toolkit;
    using namespace toolkit::log;

    using namespace xr::math;

    struct SwapchainImages {
        std::vector<std::shared_ptr<graphics::ITexture>> chain;

        std::shared_ptr<graphics::IGpuTimer> upscalerGpuTimer[utilities::ViewCount];
        std::shared_ptr<graphics::IGpuTimer> preProcessorGpuTimer[utilities::ViewCount];
        std::shared_ptr<graphics::IGpuTimer> postProcessorGpuTimer[utilities::ViewCount];
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
            m_configManager->setEnumDefault(config::SettingMenuFontSize, config::MenuFontSize::Medium);
            m_configManager->setEnumDefault(config::SettingMenuTimeout, config::MenuTimeout::Medium);
            m_configManager->setDefault(config::SettingMenuExpert, 0);
            m_configManager->setEnumDefault(config::SettingOverlayType, config::OverlayType::None);

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
            m_configManager->setEnumDefault(config::SettingMipMapBias, config::MipMapBias::Anisotropic);

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

            // Appearance.
            m_configManager->setDefault(config::SettingBrightness, 500);
            m_configManager->setDefault(config::SettingContrast, 5000);
            m_configManager->setDefault(config::SettingSaturation, 500);
            m_configManager->setDefault(config::SettingSaturationRed, 500);
            m_configManager->setDefault(config::SettingSaturationGreen, 500);
            m_configManager->setDefault(config::SettingSaturationBlue, 500);

            // Misc features.
            m_configManager->setDefault(config::SettingICD, 1000);
            m_configManager->setDefault(config::SettingFOV, 100);
            m_configManager->setDefault(config::SettingPredictionDampen, 100);
            m_configManager->setEnumDefault(config::SettingMotionReprojectionRate, config::MotionReprojectionRate::Off);
            m_configManager->setEnumDefault(config::SettingScreenshotFileFormat, config::ScreenshotFileFormat::PNG);

            // Misc debug.
            m_configManager->setDefault("debug_layer",
#ifdef _DEBUG
                                        1
#else
                                        0
#endif
            );
            m_configManager->setDefault("disable_frame_analyzer", 0);
            m_configManager->setDefault("canting", 0);

            // Workaround: the first versions of the toolkit used a different representation for the world scale.
            // Migrate the value upon first run.
            m_configManager->setDefault("icd", 0);
            if (m_configManager->getValue("icd") != 0) {
                const int migratedValue = 1'000'000 / m_configManager->getValue("icd");
                m_configManager->setValue(config::SettingICD, migratedValue, true);
                m_configManager->deleteValue("icd");
            }

            // Workaround: the first versions of the toolkit used a different representation for the contrast.
            // Migrate the value upon first run.
            m_configManager->setDefault("contrast", 0);
            if (m_configManager->getValue("contrast") != 0) {
                const int migratedValue = m_configManager->getValue("contrast") * 10;
                m_configManager->setValue(config::SettingContrast, migratedValue, true);
                m_configManager->deleteValue("contrast");
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

            // TODO: This should be auto-generated in the call above, but today our generator only looks at core spec.
            // We may let this fail intentionally and check that the pointer is populated later.
            // Workaround: the implementation of this function on the Varjo runtime seems to be using a time base than
            // the timings returned by xrWaitFrame(). Do not use it.
            if (m_runtimeName.find("Varjo") == std::string::npos) {
                xrGetInstanceProcAddr(
                    GetXrInstance(),
                    "xrConvertWin32PerformanceCounterToTimeKHR",
                    reinterpret_cast<PFN_xrVoidFunction*>(&xrConvertWin32PerformanceCounterToTimeKHR));
            }

            m_configManager = config::CreateConfigManager(createInfo->applicationInfo.applicationName);
            setOptionsDefaults();

            // Hook to enable Direct3D 11 Debug layer on request.
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

            if (m_configManager->getValue(config::SettingEyeTrackingEnabled)) {
                m_eyeTracker = input::CreateEyeTracker(*this, m_configManager);
            }

            return XR_SUCCESS;
        }

        ~OpenXrLayer() override {
            // We cleanup after ourselves (again) to avoid leaving state registry entries.
            utilities::UpdateWindowsMixedRealityReprojection(config::MotionReprojectionRate::Off);

            graphics::UnhookForD3D11DebugLayer();
        }

        XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId) override {
            const XrResult result = OpenXrApi::xrGetSystem(instance, getInfo, systemId);
            if (XR_SUCCEEDED(result) && getInfo->formFactor == XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY &&
                m_vrSystemId == XR_NULL_SYSTEM_ID) {
                // Store the actual OpenXR resolution.
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
                m_displayHeight = views[0].recommendedImageRectHeight;

                // Check for hand and eye tracking support.
                XrSystemHandTrackingPropertiesEXT handTrackingSystemProperties{
                    XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT, nullptr};
                handTrackingSystemProperties.supportsHandTracking = false;

                XrSystemEyeGazeInteractionPropertiesEXT eyeTrackingSystemProperties{
                    XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT, &handTrackingSystemProperties};
                eyeTrackingSystemProperties.supportsEyeGazeInteraction = false;

                XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES, &eyeTrackingSystemProperties};
                CHECK_XRCMD(OpenXrApi::xrGetSystemProperties(instance, *systemId, &systemProperties));
                m_supportHandTracking = handTrackingSystemProperties.supportsHandTracking;

                m_supportEyeTracking = eyeTrackingSystemProperties.supportsEyeGazeInteraction ||
                                       m_configManager->getValue(config::SettingEyeDebugWithController);

                // Workaround: the WMR runtime supports mapping the VR controllers through XR_EXT_hand_tracking, which
                // will (falsely) advertise hand tracking support. Check for the Ultraleap layer in this case.
                if (m_supportHandTracking &&
                    !m_configManager->getValue(config::SettingBypassMsftHandInteractionCheck) &&
                    m_runtimeName.find("Windows Mixed Reality Runtime") != std::string::npos) {
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
                if (m_supportEyeTracking &&
                    !m_configManager->getValue(config::SettingBypassMsftEyeGazeInteractionCheck) &&
                    m_runtimeName.find("Windows Mixed Reality Runtime") != std::string::npos) {
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

                if (inputWidth != m_displayWidth || inputHeight != m_displayHeight) {
                    // Override the recommended image size to account for scaling.
                    for (uint32_t i = 0; i < *viewCountOutput; i++) {
                        views[i].recommendedImageRectWidth = inputWidth;
                        views[i].recommendedImageRectHeight = inputHeight;

                        if (i == 0) {
                            Log("Upscaling from %ux%u to %ux%u (%u%%)\n",
                                views[i].recommendedImageRectWidth,
                                views[i].recommendedImageRectHeight,
                                m_displayWidth,
                                m_displayHeight,
                                (unsigned int)((((float)m_displayWidth / views[i].recommendedImageRectWidth) + 0.001f) *
                                               100));
                        }
                    }
                } else {
                    Log("Using OpenXR resolution (no upscaling): %ux%u\n", m_displayWidth, m_displayHeight);
                }
            }

            return result;
        }

        XrResult xrCreateSession(XrInstance instance,
                                 const XrSessionCreateInfo* createInfo,
                                 XrSession* session) override {
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
                        m_upscaler = graphics::CreateFSRUpscaler(
                            m_configManager, m_graphicsDevice, m_displayWidth, m_displayHeight);
                        break;

                    case config::ScalingType::NIS:
                        m_upscaler = graphics::CreateNISUpscaler(
                            m_configManager, m_graphicsDevice, m_displayWidth, m_displayHeight);
                        break;

                    case config::ScalingType::None:
                        break;

                    default:
                        Log("Unknown upscaling type, falling back to no upscaling\n");
                        m_upscaleMode = config::ScalingType::None;
                        break;
                    }

                    uint32_t inputWidth = m_displayWidth;
                    uint32_t inputHeight = m_displayHeight;
                    if (m_upscaleMode != config::ScalingType::None) {
                        std::tie(inputWidth, inputHeight) =
                            config::GetScaledDimensions(m_configManager.get(), m_displayWidth, m_displayHeight, 2);
                        m_upscalingFactor = m_configManager->getValue(config::SettingScaling);
                    }

                    // Per FSR SDK documentation.
                    m_mipMapBiasForUpscaling =
                        -std::log2f(static_cast<float>(m_displayWidth * m_displayHeight) / (inputWidth * inputHeight));
                    Log("MipMap biasing for upscaling is: %.3f\n", m_mipMapBiasForUpscaling);

                    m_postProcessor =
                        graphics::CreateImageProcessor(m_configManager, m_graphicsDevice, "postprocess.hlsl");

                    if (!m_configManager->getValue("disable_frame_analyzer")) {
                        m_frameAnalyzer = graphics::CreateFrameAnalyzer(m_configManager, m_graphicsDevice);
                    }

                    m_variableRateShader = graphics::CreateVariableRateShader(
                        m_configManager, m_graphicsDevice, m_eyeTracker, inputWidth, inputHeight);

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
                    m_graphicsDevice->registerUnsetRenderTargetEvent([&](std::shared_ptr<graphics::IContext> context) {
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
                        const bool isPredictionDampeningSupported =
                            xrConvertWin32PerformanceCounterToTimeKHR != nullptr;
                        const bool isMotionReprojectionRateSupported =
                            m_runtimeName.find("Windows Mixed Reality Runtime") != std::string::npos;
                        const auto displayRefreshRate =
                            utilities::RegGetDword(
                                HKEY_LOCAL_MACHINE,
                                L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Holographic\\DisplayThrottling",
                                L"ThrottleFramerate")
                                    .value_or(0)
                                ? 60u
                                : 90u;
                        m_menuHandler = menu::CreateMenuHandler(
                            m_configManager,
                            m_graphicsDevice,
                            m_displayWidth,
                            m_displayHeight,
                            m_keyModifiers,
                            m_supportHandTracking,
                            isPredictionDampeningSupported,
                            isMotionReprojectionRateSupported,
                            displayRefreshRate,
                            m_variableRateShader ? m_variableRateShader->getMaxDownsamplePow2() : 0,
                            m_supportEyeTracking);
                    }

                    // Create a reference space to calculate projection views.
                    {
                        XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
                                                                            nullptr};
                        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
                        referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity();
                        CHECK_XRCMD(xrCreateReferenceSpace(*session, &referenceSpaceCreateInfo, &m_viewSpace));
                    }
                } else {
                    Log("Unsupported graphics runtime.\n");
                }

                if (m_handTracker) {
                    m_handTracker->beginSession(*session, m_graphicsDevice);
                }
                if (m_eyeTracker) {
                    m_eyeTracker->beginSession(*session);
                }

                // Make sure we perform calibration again. We pass these values to the menu and FFR, so in the case of
                // multi-session applications, we must push those values again.
                m_needCalibrateEyeProjections = true;

                // Remember the XrSession to use.
                m_vrSession = *session;
            }

            return result;
        }

        XrResult xrDestroySession(XrSession session) override {
            const XrResult result = OpenXrApi::xrDestroySession(session);
            if (XR_SUCCEEDED(result) && isVrSession(session)) {
                // We cleanup after ourselves as soon as possible to avoid leaving state registry entries.
                utilities::UpdateWindowsMixedRealityReprojection(config::MotionReprojectionRate::Off);

                // Wait for any pending operation to complete.
                if (m_graphicsDevice) {
                    m_graphicsDevice->blockCallbacks();
                    m_graphicsDevice->flushContext(true);
                }

                if (m_viewSpace != XR_NULL_HANDLE) {
                    xrDestroySpace(m_viewSpace);
                    m_viewSpace = XR_NULL_HANDLE;
                }
                if (m_handTracker) {
                    m_handTracker->endSession();
                }
                m_upscaler.reset();
                m_preProcessor.reset();
                m_postProcessor.reset();
                m_frameAnalyzer.reset();
                if (m_eyeTracker) {
                    m_eyeTracker->endSession();
                }
                m_variableRateShader.reset();
                for (unsigned int i = 0; i <= GpuTimerLatency; i++) {
                    m_performanceCounters.appGpuTimer[i].reset();
                    m_performanceCounters.overlayGpuTimer[i].reset();
                }
                m_performanceCounters.appCpuTimer.reset();
                m_performanceCounters.waitCpuTimer.reset();
                m_performanceCounters.endFrameCpuTimer.reset();
                m_performanceCounters.overlayCpuTimer.reset();
                m_swapchains.clear();
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
            const bool useSwapchain = createInfo->usageFlags & (XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
                                                                XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

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

                if (createInfo->usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT) {
                    if (m_preProcessor) {
                        // This is redundant (given the useSwapchain conditions) but we do this for correctness.
                        chainCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
                    }

                    if (m_upscaler) {
                        // When upscaling, be sure to request the full resolution with the runtime.
                        chainCreateInfo.width = m_displayWidth;
                        chainCreateInfo.height = m_displayHeight;

                        // The upscaler requires to use as an unordered access view.
                        chainCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;
                    }

                    if (m_postProcessor) {
                        // We no longer need the runtime swapchain to have this flag since we will use an intermediate
                        // texture.
                        chainCreateInfo.usageFlags &= ~XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;

                        // This is redundant (given the useSwapchain conditions) but we do this for correctness.
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
                        Log("Swapchain image descriptor:\n");
                        Log("  w=%u h=%u arraySize=%u format=%u\n",
                            desc.Width,
                            desc.Height,
                            desc.ArraySize,
                            desc.Format);
                        Log("  mipCount=%u sampleCount=%u\n", desc.MipLevels, desc.SampleDesc.Count);
                        Log("  usage=0x%x bindFlags=0x%x cpuFlags=0x%x misc=0x%x\n",
                            desc.Usage,
                            desc.BindFlags,
                            desc.CPUAccessFlags,
                            desc.MiscFlags);

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

                        swapchainState.images.push_back(images);
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
                        Log("Swapchain image descriptor:\n");
                        Log("  w=%u h=%u arraySize=%u format=%u\n",
                            desc.Width,
                            desc.Height,
                            desc.DepthOrArraySize,
                            desc.Format);
                        Log("  mipCount=%u sampleCount=%u\n", desc.MipLevels, desc.SampleDesc.Count);
                        Log("  flags=0x%x\n", desc.Flags);

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

                        swapchainState.images.push_back(images);
                    }
                } else {
                    throw std::runtime_error("Unsupported graphics runtime");
                }

                for (uint32_t i = 0; i < imageCount; i++) {
                    SwapchainImages& images = swapchainState.images[i];

                    // We do no processing to depth buffers.
                    if (!(createInfo->usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT)) {
                        continue;
                    }

                    // Create other entries in the chain based on the processing to do (scaling,
                    // post-processing...).

                    if (m_preProcessor) {
                        // Create an intermediate texture with the same resolution as the input.
                        XrSwapchainCreateInfo inputCreateInfo = *createInfo;
                        inputCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
                        if (m_upscaler) {
                            // The upscaler requires to use as a shader input.
                            inputCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
                        }

                        auto inputTexture = m_graphicsDevice->createTexture(
                            inputCreateInfo, fmt::format("Postprocess input swapchain {} TEX2D", i), overrideFormat);

                        // We place the texture at the very front (app texture).
                        images.chain.insert(images.chain.begin(), inputTexture);

                        images.preProcessorGpuTimer[0] = m_graphicsDevice->createTimer();
                        if (createInfo->arraySize > 1) {
                            images.preProcessorGpuTimer[1] = m_graphicsDevice->createTimer();
                        }
                    }

                    if (m_upscaler) {
                        // Create an app texture with the lower resolution.
                        XrSwapchainCreateInfo inputCreateInfo = *createInfo;
                        inputCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
                        auto inputTexture = m_graphicsDevice->createTexture(
                            inputCreateInfo, fmt::format("App swapchain {} TEX2D", i), overrideFormat);

                        // We place the texture before the runtime texture, which means at the very front (app
                        // texture) or after the pre-processor.
                        images.chain.insert(images.chain.end() - 1, inputTexture);

                        images.upscalerGpuTimer[0] = m_graphicsDevice->createTimer();
                        if (createInfo->arraySize > 1) {
                            images.upscalerGpuTimer[1] = m_graphicsDevice->createTimer();
                        }
                    }

                    if (m_postProcessor) {
                        // Create an intermediate texture with the same resolution as the output.
                        XrSwapchainCreateInfo intermediateCreateInfo = chainCreateInfo;
                        intermediateCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
                        if (m_upscaler) {
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

                        images.postProcessorGpuTimer[0] = m_graphicsDevice->createTimer();
                        if (createInfo->arraySize > 1) {
                            images.postProcessorGpuTimer[1] = m_graphicsDevice->createTimer();
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
                newActionSets.resize(chainAttachInfo.countActionSets + 1);
                memcpy(newActionSets.data(),
                       chainAttachInfo.actionSets,
                       chainAttachInfo.countActionSets * sizeof(XrActionSet));
                uint32_t nextActionSetSlot = chainAttachInfo.countActionSets;

                newActionSets[nextActionSetSlot++] = m_eyeTracker->getActionSet();

                chainAttachInfo.actionSets = newActionSets.data();
                chainAttachInfo.countActionSets++;
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
                    auto swapchainState = swapchainIt->second;

                    // Return the application texture (first entry in the processing chain).
                    if (m_graphicsDevice->getApi() == graphics::Api::D3D11) {
                        XrSwapchainImageD3D11KHR* d3dImages = reinterpret_cast<XrSwapchainImageD3D11KHR*>(images);
                        for (uint32_t i = 0; i < *imageCountOutput; i++) {
                            d3dImages[i].texture = swapchainState.images[i].chain[0]->getNative<graphics::D3D11>();
                        }
                    } else if (m_graphicsDevice->getApi() == graphics::Api::D3D12) {
                        XrSwapchainImageD3D12KHR* d3dImages = reinterpret_cast<XrSwapchainImageD3D12KHR*>(images);
                        for (uint32_t i = 0; i < *imageCountOutput; i++) {
                            d3dImages[i].texture = swapchainState.images[i].chain[0]->getNative<graphics::D3D12>();
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

                m_posesForFrame[0] = views[0];
                m_posesForFrame[1] = views[1];

                // Override the canting angle if requested.
                const int cantOverride = m_configManager->getValue("canting");
                if (cantOverride != 0) {
                    const float angle = (float)(cantOverride * (M_PI / 180));

                    StoreXrPose(
                        &views[0].pose,
                        DirectX::XMMatrixMultiply(LoadXrPose(views[0].pose),
                                                  DirectX::XMMatrixRotationRollPitchYaw(0.f, -angle / 2.f, 0.f)));
                    StoreXrPose(
                        &views[1].pose,
                        DirectX::XMMatrixMultiply(LoadXrPose(views[1].pose),
                                                  DirectX::XMMatrixRotationRollPitchYaw(0.f, angle / 2.f, 0.f)));
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
                        DirectX::XMFLOAT4X4 leftView, rightView;
                        {
                            const auto tmp = LoadXrPose(eyeInViewSpace[0].pose);
                            DirectX::XMStoreFloat4x4(&leftView, tmp);
                        }
                        {
                            const auto tmp = LoadXrPose(eyeInViewSpace[1].pose);
                            DirectX::XMStoreFloat4x4(&rightView, tmp);
                        }

                        // This code is based on vrperfkit by Frydrych Holger.
                        // https://github.com/fholger/vrperfkit/blob/master/src/openvr/openvr_manager.cpp
                        const float dotForward = leftView.m[2][0] * rightView.m[2][0] +
                                                 leftView.m[2][1] * rightView.m[2][1] +
                                                 leftView.m[2][2] * rightView.m[2][2];

                        // In normalized screen coordinates.
                        float projCenterX[utilities::ViewCount];
                        float projCenterY[utilities::ViewCount];
                        for (uint32_t eye = 0; eye < utilities::ViewCount; eye++) {
                            const auto& fov = eyeInViewSpace[eye].fov;
                            const float cantedAngle = std::abs(std::acosf(dotForward) / 2) * (eye ? -1 : 1);
                            const float canted = std::tanf(cantedAngle);
                            projCenterX[eye] = 0.5f * (1.f + (fov.angleRight + fov.angleLeft - 2 * canted) /
                                                                 (fov.angleLeft - fov.angleRight));
                            projCenterY[eye] =
                                0.5f * (1.f + (fov.angleDown + fov.angleUp) / (fov.angleUp - fov.angleDown));

                            m_eyeGazeX[eye] = projCenterX[eye];
                            m_eyeGazeY[eye] = projCenterY[eye];
                        }

                        Log("Projection calibration: %.5f, %.5f | %.5f, %.5f\n",
                            projCenterX[0],
                            projCenterY[0],
                            projCenterX[1],
                            projCenterY[1]);

                        if (m_menuHandler) {
                            m_menuHandler->setViewProjectionCenters(
                                projCenterX[0], projCenterY[0], projCenterX[1], projCenterY[1]);
                        }
                        if (m_variableRateShader) {
                            m_variableRateShader->setViewProjectionCenters(
                                projCenterX[0], projCenterY[0], projCenterX[1], projCenterY[1]);
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
                    m_stats.icd = icd;
                    const auto center = views[0].pose.position + vec / 2.0f;
                    const auto unit = Normalize(vec);

                    views[0].pose.position = center - unit * (icd / 2.0f);
                    views[1].pose.position = center + unit * (icd / 2.0f);
                } else {
                    m_stats.icd = ipd;
                }

                // Override the FOV if requested.
                const int fovOverride = m_configManager->getValue(config::SettingFOV);
                if (fovOverride != 100) {
                    const float multiplier = fovOverride / 100.0f;

                    views[0].fov.angleUp *= multiplier;
                    views[0].fov.angleDown *= multiplier;
                    views[0].fov.angleLeft *= multiplier;
                    views[0].fov.angleRight *= multiplier;
                    views[1].fov.angleUp *= multiplier;
                    views[1].fov.angleDown *= multiplier;
                    views[1].fov.angleLeft *= multiplier;
                    views[1].fov.angleRight *= multiplier;
                }

                m_stats.totalFov = -views[0].fov.angleLeft + views[1].fov.angleRight;
            }

            return result;
        }

        XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location) override {
            m_performanceCounters.handTrackingTimer->start();
            if (m_handTracker && location->type == XR_TYPE_SPACE_LOCATION &&
                m_handTracker->locate(space, baseSpace, time, getTimeNow(), *location)) {
                m_performanceCounters.handTrackingTimer->stop();
                m_stats.handTrackingCpuTimeUs += m_performanceCounters.handTrackingTimer->query();
                return XR_SUCCESS;
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
            if (m_handTracker && isVrSession(session) && getInfo->type == XR_TYPE_ACTION_STATE_GET_INFO) {
                const std::string fullPath = m_handTracker->getFullPath(getInfo->action, getInfo->subactionPath);
                if (state->type == XR_TYPE_ACTION_STATE_POSE) {
                    if (fullPath == "/user/hand/left/input/grip/pose" || fullPath == "/user/hand/left/input/aim/pose") {
                        state->isActive = m_handTracker->isTrackedRecently(input::Hand::Left);
                        return XR_SUCCESS;
                    } else if (fullPath == "/user/hand/right/input/grip/pose" ||
                               fullPath == "/user/hand/right/input/aim/pose") {
                        state->isActive = m_handTracker->isTrackedRecently(input::Hand::Right);
                        return XR_SUCCESS;
                    }
                }
            }

            return OpenXrApi::xrGetActionStatePose(session, getInfo, state);
        }

        XrResult xrWaitFrame(XrSession session,
                             const XrFrameWaitInfo* frameWaitInfo,
                             XrFrameState* frameState) override {
            m_performanceCounters.waitCpuTimer->start();
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

                // Push the last averaged statistics.
                m_stats.fps = static_cast<float>(numFrames);
                m_stats.appCpuTimeUs /= numFrames;
                m_stats.appGpuTimeUs /= numFrames;
                m_stats.waitCpuTimeUs /= numFrames;
                // When CPU-bound, do not bother giving a (false) GPU time for D3D12
                if (m_graphicsDevice->getApi() == graphics::Api::D3D12 &&
                    m_stats.appCpuTimeUs + 500 > m_stats.appGpuTimeUs) {
                    m_stats.appGpuTimeUs = 0;
                }
                m_stats.endFrameCpuTimeUs /= numFrames;
                m_stats.upscalerGpuTimeUs /= numFrames;
                m_stats.preProcessorGpuTimeUs /= numFrames;
                m_stats.postProcessorGpuTimeUs /= numFrames;
                m_stats.overlayCpuTimeUs /= numFrames;
                m_stats.overlayGpuTimeUs /= numFrames;
                m_stats.handTrackingCpuTimeUs /= numFrames;
                m_stats.predictionTimeUs /= numFrames;

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

            // Refresh the configuration.
            if (m_preProcessor) {
                m_preProcessor->update();
            }
            if (m_upscaler) {
                m_upscaler->update();
            }
            if (m_postProcessor) {
                m_postProcessor->update();
            }
            if (m_eyeTracker) {
                m_eyeTracker->update();
            }
            if (m_variableRateShader) {
                m_variableRateShader->update();
            }
            if (m_configManager->hasChanged(config::SettingMipMapBias) ||
                m_configManager->hasChanged(config::SettingScalingType)) {
                const auto biasing = m_configManager->getEnumValue<config::ScalingType>(config::SettingScalingType) !=
                                             config::ScalingType::None
                                         ? m_configManager->getEnumValue<config::MipMapBias>(config::SettingMipMapBias)
                                         : config::MipMapBias::Off;
                m_graphicsDevice->setMipMapBias(biasing, m_mipMapBiasForUpscaling);
            }
        }

        void takeScreenshot(std::shared_ptr<graphics::ITexture> texture) const {
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
                parameters << upscaleName << m_upscalingFactor << "_"
                           << m_configManager->getValue(config::SettingSharpness);
            }

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
#ifdef _DEBUG
                m_variableRateShader->stopCapture();
#endif
            }

            m_graphicsDevice->saveContext();

            // Handle inputs.
            if (m_menuHandler) {
                m_menuHandler->handleInput();
            }

            // Forward the motion reprojection locking values to WMR.
            if (m_configManager->hasChanged(config::SettingMotionReprojectionRate)) {
                utilities::UpdateWindowsMixedRealityReprojection(
                    m_configManager->getEnumValue<config::MotionReprojectionRate>(
                        config::SettingMotionReprojectionRate));
            }

            // Prepare the Shaders for rendering.
            updateConfiguration();

            // Unbind all textures from the render targets.
            m_graphicsDevice->unsetRenderTargets();

            std::shared_ptr<graphics::ITexture> textureForOverlay[utilities::ViewCount] = {};
            std::shared_ptr<graphics::ITexture> depthForOverlay[utilities::ViewCount] = {};
            graphics::View viewsForOverlay[utilities::ViewCount];
            XrSpace spaceForOverlay = XR_NULL_HANDLE;

            // Because the frame info is passed const, we are going to need to reconstruct a writable version of it
            // to patch the resolution.
            XrFrameEndInfo chainFrameEndInfo = *frameEndInfo;
            std::vector<const XrCompositionLayerBaseHeader*> correctedLayers;

            std::vector<XrCompositionLayerProjection> layerProjectionAllocator;
            std::vector<std::array<XrCompositionLayerProjectionView, 2>> layerProjectionViewsAllocator;

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
                                    auto depthSwapchainState = depthSwapchainIt->second;

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

                        // Perform pre-processing.
                        if (m_preProcessor) {
                            nextImage++;

                            m_stats.preProcessorGpuTimeUs +=
                                swapchainImages.preProcessorGpuTimer[gpuTimerIndex]->query();
                            swapchainImages.preProcessorGpuTimer[gpuTimerIndex]->start();

                            m_preProcessor->process(
                                swapchainImages.chain[lastImage], swapchainImages.chain[nextImage], useVPRT ? eye : -1);
                            swapchainImages.preProcessorGpuTimer[gpuTimerIndex]->stop();

                            lastImage++;
                        }

                        // Perform upscaling (if requested).
                        if (m_upscaler) {
                            nextImage++;

                            // We allow to bypass scaling when the menu option is turned off. This is only for quick
                            // comparison/testing, since we're still holding to all the underlying resources.
                            if (m_configManager->getEnumValue<config::ScalingType>(config::SettingScalingType) !=
                                config::ScalingType::None) {
                                m_stats.upscalerGpuTimeUs += swapchainImages.upscalerGpuTimer[gpuTimerIndex]->query();
                                swapchainImages.upscalerGpuTimer[gpuTimerIndex]->start();

                                m_upscaler->process(swapchainImages.chain[lastImage],
                                                    swapchainImages.chain[nextImage],
                                                    useVPRT ? eye : -1);
                                swapchainImages.upscalerGpuTimer[gpuTimerIndex]->stop();

                                lastImage++;
                            }
                        }

                        // Perform post-processing.
                        if (m_postProcessor) {
                            nextImage++;

                            m_stats.postProcessorGpuTimeUs +=
                                swapchainImages.postProcessorGpuTimer[gpuTimerIndex]->query();
                            swapchainImages.postProcessorGpuTimer[gpuTimerIndex]->start();

                            m_postProcessor->process(
                                swapchainImages.chain[lastImage], swapchainImages.chain[nextImage], useVPRT ? eye : -1);
                            swapchainImages.postProcessorGpuTimer[gpuTimerIndex]->stop();

                            lastImage++;
                        }

                        // Make sure the chain was completed.
                        if (nextImage != swapchainImages.chain.size() - 1) {
                            throw std::runtime_error("Processing chain incomplete!");
                        }

                        textureForOverlay[eye] = swapchainImages.chain.back();
                        depthForOverlay[eye] = depthBuffer;

                        // Patch the resolution.
                        correctedProjectionViews[eye].subImage.imageRect.extent.width = m_displayWidth;
                        correctedProjectionViews[eye].subImage.imageRect.extent.height = m_displayHeight;

                        // Patch the FOV when set above 100%.
                        const int fov = m_configManager->getValue(config::SettingFOV);
                        if (fov > 100) {
                            const float multiplier = 100.0f / fov;

                            correctedProjectionViews[eye].fov.angleUp *= multiplier;
                            correctedProjectionViews[eye].fov.angleDown *= multiplier;
                            correctedProjectionViews[eye].fov.angleLeft *= multiplier;
                            correctedProjectionViews[eye].fov.angleRight *= multiplier;
                        }

                        // Patch the eye poses.
                        const int cantOverride = m_configManager->getValue("canting");
                        if (cantOverride != 0) {
                            correctedProjectionViews[eye].pose = m_posesForFrame[eye].pose;
                        }

                        viewsForOverlay[eye].pose = correctedProjectionViews[eye].pose;
                        viewsForOverlay[eye].fov = correctedProjectionViews[eye].fov;
                        viewsForOverlay[eye].nearFar = nearFar;
                    }

                    spaceForOverlay = proj->space;

                    correctedProjectionLayer->views = correctedProjectionViews;
                    correctedLayers.push_back(
                        reinterpret_cast<const XrCompositionLayerBaseHeader*>(correctedProjectionLayer));
                } else {
                    correctedLayers.push_back(chainFrameEndInfo.layers[i]);
                }
            }

            chainFrameEndInfo.layers = correctedLayers.data();

            // We intentionally exclude the overlay from this timer, as it has its own separate timer.
            m_performanceCounters.endFrameCpuTimer->stop();

            // Render our overlays.
            if (textureForOverlay[0]) {
                const bool useVPRT = textureForOverlay[1] == textureForOverlay[0];

                if (m_menuHandler || m_handTracker) {
                    m_stats.overlayCpuTimeUs += m_performanceCounters.overlayCpuTimer->query();
                    m_stats.overlayGpuTimeUs +=
                        m_performanceCounters.overlayGpuTimer[m_performanceCounters.gpuTimerIndex]->query();

                    m_performanceCounters.overlayCpuTimer->start();
                    m_performanceCounters.overlayGpuTimer[m_performanceCounters.gpuTimerIndex]->start();
                }

                // Render the hands or eye gaze helper.
                if (m_handTracker || (m_eyeTracker && m_configManager->getValue(config::SettingEyeDebug))) {
                    bool isEyeGazeValid = false;
                    if (m_eyeTracker && m_eyeTracker->getProjectedGaze(m_eyeGazeX, m_eyeGazeY)) {
                        isEyeGazeValid = true;
                    }

                    for (uint32_t eye = 0; eye < utilities::ViewCount; eye++) {
                        if (!useVPRT) {
                            m_graphicsDevice->setRenderTargets({textureForOverlay[eye]}, depthForOverlay[eye]);
                        } else {
                            m_graphicsDevice->setRenderTargets({std::make_pair(textureForOverlay[eye], eye)},
                                                               {std::make_pair(depthForOverlay[eye], eye)});
                        }
                        m_graphicsDevice->setViewProjection(viewsForOverlay[eye]);

                        if (m_handTracker) {
                            m_handTracker->render(
                                viewsForOverlay[eye].pose, spaceForOverlay, getTimeNow(), textureForOverlay[eye]);
                        }

                        if (m_eyeTracker) {
                            XrColor4f color = isEyeGazeValid ? XrColor4f{0, 1, 0, 1} : XrColor4f{1, 0, 0, 1};
                            const auto centerX = m_displayWidth * m_eyeGazeX[eye];
                            const auto centerY = m_displayHeight * m_eyeGazeY[eye];

                            m_graphicsDevice->clearColor(centerY - 20, centerX - 20, centerY + 20, centerX + 20, color);
                        }
                    }
                }

                // Render the menu.
                // Ideally, we would not have to split this from the branch above, however with D3D12 we are forced
                // to flush the context, and we'd rather do it only once.
                // We omit the depth buffer for menu (2D) content.
                if (m_menuHandler) {
                    if (m_graphicsDevice->getApi() == graphics::Api::D3D12) {
                        m_graphicsDevice->flushContext();
                    }
                    for (uint32_t eye = 0; eye < utilities::ViewCount; eye++) {
                        if (!useVPRT) {
                            m_graphicsDevice->setRenderTargets({textureForOverlay[eye]});
                        } else {
                            m_graphicsDevice->setRenderTargets({std::make_pair(textureForOverlay[eye], eye)});
                        }

                        m_graphicsDevice->beginText();
                        m_menuHandler->render((utilities::Eye)eye, textureForOverlay[eye]);
                        m_graphicsDevice->flushText();
                    }
                }

                if (m_menuHandler || m_handTracker ||
                    (m_eyeTracker && m_configManager->getValue(config::SettingEyeDebug))) {
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
                takeScreenshot(textureForOverlay[0]);

#ifdef _DEBUG
                if (m_variableRateShader) {
                    m_variableRateShader->startCapture();
                }
#endif
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
        XrSystemId m_vrSystemId{XR_NULL_SYSTEM_ID};
        XrSession m_vrSession{XR_NULL_HANDLE};
        uint32_t m_displayWidth{0};
        uint32_t m_displayHeight{0};
        bool m_supportHandTracking{false};
        bool m_supportEyeTracking{false};

        XrTime m_waitedFrameTime;
        XrTime m_begunFrameTime;
        bool m_isInFrame{false};
        bool m_sendInterationProfileEvent{false};
        XrSpace m_viewSpace{XR_NULL_HANDLE};
        bool m_needCalibrateEyeProjections{true};
        float m_eyeGazeX[utilities::ViewCount];
        float m_eyeGazeY[utilities::ViewCount];
        XrView m_posesForFrame[utilities::ViewCount];

        std::shared_ptr<config::IConfigManager> m_configManager;

        std::shared_ptr<graphics::IDevice> m_graphicsDevice;
        std::map<XrSwapchain, SwapchainState> m_swapchains;

        std::shared_ptr<graphics::IImageProcessor> m_preProcessor;
        std::shared_ptr<graphics::IImageProcessor> m_postProcessor;
        std::shared_ptr<graphics::IImageProcessor> m_upscaler;
        config::ScalingType m_upscaleMode{config::ScalingType::None};
        uint32_t m_upscalingFactor{100};
        float m_mipMapBiasForUpscaling{0.f};

        std::shared_ptr<graphics::IFrameAnalyzer> m_frameAnalyzer;
        std::shared_ptr<input::IEyeTracker> m_eyeTracker;
        std::shared_ptr<graphics::IVariableRateShader> m_variableRateShader;

        std::shared_ptr<input::IHandTracker> m_handTracker;

        std::vector<int> m_keyModifiers;
        int m_keyScreenshot;
        std::shared_ptr<menu::IMenuHandler> m_menuHandler;
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
