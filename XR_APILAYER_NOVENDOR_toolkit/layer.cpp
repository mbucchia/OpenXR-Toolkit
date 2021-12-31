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

#include "factories.h"
#include "interfaces.h"
#include "layer.h"
#include "log.h"

namespace {

    // 2 views to process, one per eye.
    constexpr uint32_t ViewCount = 2;

    // The xrWaitFrame() loop might cause to have 2 frames in-flight, so we want to delay the GPU timer re-use by those
    // 2 frames.
    constexpr uint32_t GpuTimerLatency = 2;

    using namespace toolkit;
    using namespace toolkit::log;

    using namespace xr::math;

    struct SwapchainImages {
        std::vector<std::shared_ptr<graphics::ITexture>> chain;

        std::shared_ptr<graphics::IGpuTimer> upscalerGpuTimer[ViewCount];
        std::shared_ptr<graphics::IGpuTimer> preProcessorGpuTimer[ViewCount];
        std::shared_ptr<graphics::IGpuTimer> postProcessorGpuTimer[ViewCount];
    };

    struct SwapchainState {
        std::vector<SwapchainImages> images;
        uint32_t acquiredImageIndex;
    };

    class OpenXrLayer : public toolkit::OpenXrApi {
      public:
        OpenXrLayer() = default;
        ~OpenXrLayer() override = default;

        XrResult xrCreateInstance(const XrInstanceCreateInfo* createInfo) override {
            // Needed to resolve the requested function pointers.
            OpenXrApi::xrCreateInstance(createInfo);

            // Dump the OpenXR runtime information to help debugging customer issues.
            XrInstanceProperties instanceProperties = {XR_TYPE_INSTANCE_PROPERTIES};
            CHECK_XRCMD(xrGetInstanceProperties(GetXrInstance(), &instanceProperties));
            const std::string runtimeName(instanceProperties.runtimeName);
            Log("Using OpenXR runtime %s, version %u.%u.%u\n",
                runtimeName.c_str(),
                XR_VERSION_MAJOR(instanceProperties.runtimeVersion),
                XR_VERSION_MINOR(instanceProperties.runtimeVersion),
                XR_VERSION_PATCH(instanceProperties.runtimeVersion));

            m_configManager = config::CreateConfigManager(createInfo->applicationInfo.applicationName);

            return XR_SUCCESS;
        }

        XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId) override {
            const XrResult result = OpenXrApi::xrGetSystem(instance, getInfo, systemId);
            if (XR_SUCCEEDED(result) && getInfo->formFactor == XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY) {
                // Store the actual OpenXR resolution.
                XrViewConfigurationView views[ViewCount] = {{XR_TYPE_VIEW_CONFIGURATION_VIEW},
                                                            {XR_TYPE_VIEW_CONFIGURATION_VIEW}};
                uint32_t viewCount;
                CHECK_XRCMD(OpenXrApi::xrEnumerateViewConfigurationViews(
                    instance, *systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, ViewCount, &viewCount, views));

                m_displayWidth = views[0].recommendedImageRectWidth;
                m_displayHeight = views[0].recommendedImageRectHeight;

                // Set the default upscaling settings.
                m_configManager->setEnumDefault(config::SettingScalingType, config::ScalingType::None);
                m_configManager->setDefault(config::SettingScaling, 100);
                m_configManager->setDefault(config::SettingSharpness, 20);

                m_configManager->setDefault(config::SettingFOV, 100);

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
                auto upscaleMode = m_configManager->getEnumValue<config::ScalingType>(config::SettingScalingType);
                uint32_t inputWidth = m_displayWidth;
                uint32_t inputHeight = m_displayHeight;

                switch (upscaleMode) {
                case config::ScalingType::NIS: {
                    auto resolution = utilities::GetScaledResolution(m_configManager, m_displayWidth, m_displayHeight);
                    inputWidth = resolution.first;
                    inputHeight = resolution.second;
                    break;
                }

                case config::ScalingType::None:
                    break;

                default:
                    throw new std::runtime_error("Unknown scaling type");
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
                        m_graphicsDevice = graphics::WrapD3D11Device(d3dBindings->device);
                        break;
                    }

                    entry = entry->next;
                }

                if (m_graphicsDevice) {
                    // Initialize the other resources.
                    auto upscaleMode = m_configManager->getEnumValue<config::ScalingType>(config::SettingScalingType);

                    switch (upscaleMode) {
                    case config::ScalingType::NIS:
                        m_upscaler = graphics::CreateNISUpscaler(
                            m_configManager, m_graphicsDevice, m_displayWidth, m_displayHeight);
                        break;

                    case config::ScalingType::None:
                        break;

                    default:
                        throw new std::runtime_error("Unknown scaling type");
                        break;
                    }

                    m_postProcessor =
                        graphics::CreateImageProcessor(m_configManager, m_graphicsDevice, "postprocess.hlsl");

                    m_performanceCounters.appCpuTimer = utilities::CreateCpuTimer();
                    m_performanceCounters.endFrameCpuTimer = utilities::CreateCpuTimer();
                    m_performanceCounters.overlayCpuTimer = utilities::CreateCpuTimer();

                    for (unsigned int i = 0; i <= GpuTimerLatency; i++) {
                        m_performanceCounters.appGpuTimer[i] = m_graphicsDevice->createTimer();
                        m_performanceCounters.overlayGpuTimer[i] = m_graphicsDevice->createTimer();
                    }

                    m_performanceCounters.lastWindowStart = std::chrono::steady_clock::now();

                    m_menuHandler =
                        menu::CreateMenuHandler(m_configManager, m_graphicsDevice, m_displayWidth, m_displayHeight);
                } else {
                    Log("Unsupported graphics runtime.\n");
                }

                // Remember the XrSession to use.
                m_vrSession = *session;
            }

            return result;
        }

        XrResult xrDestroySession(XrSession session) override {
            const XrResult result = OpenXrApi::xrDestroySession(session);
            if (XR_SUCCEEDED(result) && isVrSession(session)) {
                m_swapchains.clear();
                m_menuHandler.reset();
                m_graphicsDevice.reset();
                m_vrSession = XR_NULL_HANDLE;
            }

            return result;
        }

        XrResult xrCreateSwapchain(XrSession session,
                                   const XrSwapchainCreateInfo* createInfo,
                                   XrSwapchain* swapchain) override {
            if (!isVrSession(session) || !m_graphicsDevice) {
                return OpenXrApi::xrCreateSwapchain(session, createInfo, swapchain);
            }

            // TODO: Identify the swapchains of interest for our processing chain. For now, we only handle color
            // buffers.
            const bool useSwapchain = createInfo->usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

            XrSwapchainCreateInfo chainCreateInfo = *createInfo;
            if (useSwapchain) {
                // TODO: Modify the swapchain to handle our processing chain (eg: change resolution and/or select usage
                // XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT).

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

            const XrResult result = OpenXrApi::xrCreateSwapchain(session, &chainCreateInfo, swapchain);
            if (XR_SUCCEEDED(result) && useSwapchain) {
                uint32_t imageCount;
                CHECK_XRCMD(OpenXrApi::xrEnumerateSwapchainImages(*swapchain, 0, &imageCount, nullptr));

                SwapchainState swapchainState;
                if (m_graphicsDevice->getApi() == graphics::Api::D3D11) {
                    std::vector<XrSwapchainImageD3D11KHR> d3dImages(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
                    CHECK_XRCMD(OpenXrApi::xrEnumerateSwapchainImages(
                        *swapchain,
                        imageCount,
                        &imageCount,
                        reinterpret_cast<XrSwapchainImageBaseHeader*>(d3dImages.data())));
                    for (uint32_t i = 0; i < imageCount; i++) {
                        SwapchainImages images;

                        // Store the runtime images into the state (last entry in the processing chain).
                        images.chain.push_back(
                            graphics::WrapD3D11Texture(m_graphicsDevice,
                                                       chainCreateInfo,
                                                       d3dImages[i].texture,
                                                       fmt::format("Runtime swapchain {} TEX2D", i)));

                        // TODO: Create other entries in the chain based on the processing to do (scaling,
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
                                inputCreateInfo, fmt::format("Postprocess input swapchain {} TEX2D", i));

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
                                inputCreateInfo, fmt::format("App swapchain {} TEX2D", i));

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
                                    intermediateCreateInfo.format =
                                        m_graphicsDevice->getTextureFormat(graphics::TextureFormat::R16G16B16A16_UNORM);
                                }
                            }
                            auto intermediateTexture = m_graphicsDevice->createTexture(
                                intermediateCreateInfo, fmt::format("Postprocess input swapchain {} TEX2D", i));

                            // We place the texture just before the runtime texture.
                            images.chain.insert(images.chain.end() - 1, intermediateTexture);

                            images.postProcessorGpuTimer[0] = m_graphicsDevice->createTimer();
                            if (createInfo->arraySize > 1) {
                                images.postProcessorGpuTimer[1] = m_graphicsDevice->createTimer();
                            }
                        }

                        swapchainState.images.push_back(images);
                    }
                } else {
                    throw new std::runtime_error("Unsupported graphics runtime");
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
                    } else {
                        throw new std::runtime_error("Unsupported graphics runtime");
                    }
                }
            }

            return result;
        }

        XrResult xrAcquireSwapchainImage(XrSwapchain swapchain,
                                         const XrSwapchainImageAcquireInfo* acquireInfo,
                                         uint32_t* index) override {
            const XrResult result = OpenXrApi::xrAcquireSwapchainImage(swapchain, acquireInfo, index);
            if (XR_SUCCEEDED(result)) {
                // Record the index so we know which texture to use in xrEndFrame().
                auto swapchainIt = m_swapchains.find(swapchain);
                if (swapchainIt != m_swapchains.end()) {
                    swapchainIt->second.acquiredImageIndex = *index;
                }
            }

            return result;
        }

        XrResult xrLocateViews(XrSession session,
                               const XrViewLocateInfo* viewLocateInfo,
                               XrViewState* viewState,
                               uint32_t viewCapacityInput,
                               uint32_t* viewCountOutput,
                               XrView* views) override {
            const XrResult result =
                OpenXrApi::xrLocateViews(session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views);
            if (XR_SUCCEEDED(result) &&
                viewLocateInfo->viewConfigurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
                assert(*viewCountOutput == ViewCount);

                const auto vec = views[1].pose.position - views[0].pose.position;
                const auto ipd = Length(vec);

                // If it's the first time, initialize the ICD to be the same as IPD.
                int icdInTenthmm = m_configManager->getValue(config::SettingICD);
                if (icdInTenthmm == 0) {
                    icdInTenthmm = (int)(ipd * 10000.0f);
                    m_configManager->setValue(config::SettingICD, icdInTenthmm);
                }
                const float icd = icdInTenthmm / 10000.0f;

                // Override the ICD if requested. We can't do a real epsilon-compare since we use this weird tenth of mm
                // intermediate unit.
                if (std::abs(ipd - icd) > 0.00005f) {
                    const auto center = views[0].pose.position + vec / 2.0f;
                    const auto unit = Normalize(vec);

                    views[0].pose.position = center - unit * (icd / 2.0f);
                    views[1].pose.position = center + unit * (icd / 2.0f);
                }

                // Override the FOV if requested.
                const int fov = m_configManager->getValue(config::SettingFOV);
                if (fov != 100) {
                    const float multiplier = fov / 100.0f;

                    views[0].fov.angleUp *= multiplier;
                    views[0].fov.angleDown *= multiplier;
                    views[0].fov.angleLeft *= multiplier;
                    views[0].fov.angleRight *= multiplier;
                    views[1].fov.angleUp *= multiplier;
                    views[1].fov.angleDown *= multiplier;
                    views[1].fov.angleLeft *= multiplier;
                    views[1].fov.angleRight *= multiplier;
                }
            }

            return result;
        }

        XrResult xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo) override {
            const XrResult result = OpenXrApi::xrBeginFrame(session, frameBeginInfo);
            if (XR_SUCCEEDED(result) && m_graphicsDevice) {
                m_performanceCounters.appCpuTimer->start();
                m_stats.appGpuTimeUs += m_performanceCounters.appGpuTimer[m_performanceCounters.gpuTimerIndex]->query();
                m_performanceCounters.appGpuTimer[m_performanceCounters.gpuTimerIndex]->start();
            }

            return result;
        }

        void updateStatisticsForFrame() {
            const auto now = std::chrono::steady_clock::now();

            m_frameTimestamps.push_back(now);

            if (std::chrono::duration<double>(now - m_performanceCounters.lastWindowStart).count() > 1.0) {
                // Update the FPS counter.
                while (std::chrono::duration<double>(now - m_frameTimestamps.front()).count() > 1.0) {
                    m_frameTimestamps.pop_front();
                }
                m_stats.fps = (float)m_frameTimestamps.size();

                // Push the last averaged statistics.
                if (m_performanceCounters.numFrames) {
                    m_stats.appCpuTimeUs /= m_performanceCounters.numFrames;
                    m_stats.appGpuTimeUs /= m_performanceCounters.numFrames;
                    m_stats.endFrameCpuTimeUs /= m_performanceCounters.numFrames;
                    m_stats.upscalerGpuTimeUs /= m_performanceCounters.numFrames;
                    m_stats.preProcessorGpuTimeUs /= m_performanceCounters.numFrames;
                    m_stats.postProcessorGpuTimeUs /= m_performanceCounters.numFrames;
                    m_stats.overlayCpuTimeUs /= m_performanceCounters.numFrames;
                    m_stats.overlayGpuTimeUs /= m_performanceCounters.numFrames;
                }
                m_menuHandler->updateStatistics(m_stats);

                // Start from fresh!
                m_stats.appCpuTimeUs = m_stats.appGpuTimeUs = m_stats.endFrameCpuTimeUs = 0;
                m_stats.overlayCpuTimeUs = m_stats.overlayGpuTimeUs = 0;
                m_stats.upscalerGpuTimeUs = m_stats.preProcessorGpuTimeUs = m_stats.postProcessorGpuTimeUs = 0;

                m_performanceCounters.numFrames = 0;
                m_performanceCounters.lastWindowStart = now;
            }
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
        }

        XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) override {
            if (!isVrSession(session) || !m_graphicsDevice) {
                return OpenXrApi::xrEndFrame(session, frameEndInfo);
            }

            updateStatisticsForFrame();

            m_performanceCounters.appCpuTimer->stop();
            m_stats.appCpuTimeUs += m_performanceCounters.appCpuTimer->query();
            m_performanceCounters.appGpuTimer[m_performanceCounters.gpuTimerIndex]->stop();

            m_stats.endFrameCpuTimeUs += m_performanceCounters.endFrameCpuTimer->query();
            m_performanceCounters.endFrameCpuTimer->start();

            updateConfiguration();

            // Toggle to the next set of GPU timers.
            m_performanceCounters.gpuTimerIndex = (m_performanceCounters.gpuTimerIndex + 1) % (GpuTimerLatency + 1);

            // Handle menu stuff.
            if (m_menuHandler) {
                m_menuHandler->handleInput();
            }

            // Unbind all textures from the render targets.
            m_graphicsDevice->clearRenderTargets();

            std::shared_ptr<graphics::ITexture> topLayer[ViewCount] = {};

            // Because the frame info is passed const, we are going to need to reconstruct a writable version of it to
            // patch the resolution.
            XrFrameEndInfo chainFrameEndInfo = *frameEndInfo;
            std::vector<const XrCompositionLayerBaseHeader*> correctedLayers;

            std::vector<XrCompositionLayerProjection> layerProjectionAllocator;
            std::vector<std::array<XrCompositionLayerProjectionView, 2>> layerProjectionViewsAllocator;

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
                    static_assert(ViewCount == 2);
                    const bool useVPRT = proj->views[0].subImage.swapchain == proj->views[1].subImage.swapchain;

                    assert(proj->viewCount == ViewCount);
                    for (uint32_t eye = 0; eye < ViewCount; eye++) {
                        const XrCompositionLayerProjectionView& view = proj->views[eye];

                        auto swapchainIt = m_swapchains.find(view.subImage.swapchain);
                        if (swapchainIt == m_swapchains.end()) {
                            throw new std::runtime_error("Swapchain is not registered");
                        }
                        auto swapchainState = swapchainIt->second;
                        auto swapchainImages = swapchainState.images[swapchainState.acquiredImageIndex];
                        uint32_t nextImage = 0;
                        uint32_t lastImage = 0;
                        uint32_t gpuTimerIndex = useVPRT ? eye : 0;

                        // TODO: Insert processing below.
                        // The pattern typically follows these steps:
                        // - Advanced to the right source and/or destination image;
                        // - Pull the previously measured timer value;
                        // - Start the timer;
                        // - Invoke the processing;
                        // - Stop the timer;
                        // - Advanced to the right source and/or destination image;

                        // Perform post-processing.
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

                            m_stats.upscalerGpuTimeUs += swapchainImages.upscalerGpuTimer[gpuTimerIndex]->query();
                            swapchainImages.upscalerGpuTimer[gpuTimerIndex]->start();

                            m_upscaler->upscale(
                                swapchainImages.chain[lastImage], swapchainImages.chain[nextImage], useVPRT ? eye : -1);
                            swapchainImages.upscalerGpuTimer[gpuTimerIndex]->stop();

                            lastImage++;
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
                            throw new std::runtime_error("Processing chain incomplete!");
                        }

                        topLayer[eye] = swapchainImages.chain[nextImage];

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
                    }

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

            // Render the menu in the top-most layer.
            if (m_menuHandler) {
                // Update with the statistics from the last frame.
                m_stats.overlayCpuTimeUs += m_performanceCounters.overlayCpuTimer->query();
                m_stats.overlayGpuTimeUs +=
                    m_performanceCounters.overlayGpuTimer[m_performanceCounters.gpuTimerIndex]->query();

                if (topLayer[0]) {
                    // When using VPRT, we rely on the menu renderer to render both views at once. Otherwise we
                    // render each view one at a time.
                    m_performanceCounters.overlayCpuTimer->start();
                    m_performanceCounters.overlayGpuTimer[m_performanceCounters.gpuTimerIndex]->start();
                    m_menuHandler->render(topLayer[0], 0);
                    static_assert(ViewCount == 2);
                    if (topLayer[1] != topLayer[0]) {
                        m_menuHandler->render(topLayer[1], 1);
                    }
                    m_performanceCounters.overlayCpuTimer->stop();
                    m_performanceCounters.overlayGpuTimer[m_performanceCounters.gpuTimerIndex]->stop();
                }
            }

            m_performanceCounters.numFrames++;

            return OpenXrApi::xrEndFrame(session, &chainFrameEndInfo);
        }

      private:
        bool isVrSystem(XrSystemId systemId) const {
            return systemId == m_vrSystemId;
        }

        bool isVrSession(XrSession session) const {
            return session == m_vrSession;
        }

        XrSystemId m_vrSystemId{XR_NULL_SYSTEM_ID};
        XrSession m_vrSession{XR_NULL_HANDLE};
        uint32_t m_displayWidth;
        uint32_t m_displayHeight;

        std::shared_ptr<config::IConfigManager> m_configManager;

        std::shared_ptr<graphics::IDevice> m_graphicsDevice;
        std::map<XrSwapchain, SwapchainState> m_swapchains;

        std::shared_ptr<graphics::IUpscaler> m_upscaler;
        std::shared_ptr<graphics::IImageProcessor> m_preProcessor;
        std::shared_ptr<graphics::IImageProcessor> m_postProcessor;

        std::shared_ptr<menu::IMenuHandler> m_menuHandler;

        struct {
            std::shared_ptr<utilities::ICpuTimer> appCpuTimer;
            std::shared_ptr<graphics::IGpuTimer> appGpuTimer[GpuTimerLatency + 1];
            std::shared_ptr<utilities::ICpuTimer> endFrameCpuTimer;
            std::shared_ptr<utilities::ICpuTimer> overlayCpuTimer;
            std::shared_ptr<graphics::IGpuTimer> overlayGpuTimer[GpuTimerLatency + 1];

            unsigned int gpuTimerIndex{0};
            std::chrono::time_point<std::chrono::steady_clock> lastWindowStart;
            uint32_t numFrames{0};
        } m_performanceCounters;

        LayerStatistics m_stats{};
        std::deque<std::chrono::time_point<std::chrono::steady_clock>> m_frameTimestamps;
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
