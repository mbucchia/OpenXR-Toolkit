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

    using namespace toolkit;
    using namespace toolkit::log;

    struct SwapchainImages {
        std::vector<std::shared_ptr<graphics::ITexture>> chain;
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
                XrViewConfigurationView views[2] = {{XR_TYPE_VIEW_CONFIGURATION_VIEW},
                                                    {XR_TYPE_VIEW_CONFIGURATION_VIEW}};
                uint32_t viewCount;
                CHECK_XRCMD(OpenXrApi::xrEnumerateViewConfigurationViews(
                    instance, *systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &viewCount, views));

                m_displayWidth = views[0].recommendedImageRectWidth;
                m_displayHeight = views[0].recommendedImageRectHeight;

                // Set the default upscaling settings.
                m_configManager->setEnumDefault(config::SettingScalingType, config::ScalingType::None);
                m_configManager->setDefault(config::SettingScaling, 100);
                m_configManager->setDefault(config::SettingSharpness, 20);

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
                    auto resolution =
                        graphics::GetNISScaledResolution(m_configManager, m_displayWidth, m_displayHeight);
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

                    m_menuHandler = menu::CreateMenuHandler(m_configManager, m_graphicsDevice);
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

        XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) override {
            if (!isVrSession(session) || !m_graphicsDevice) {
                return OpenXrApi::xrEndFrame(session, frameEndInfo);
            }

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

            // Update the FPS counter.
            const auto now = std::chrono::steady_clock::now();
            m_frameTimestamps.push_back(now);
            while (std::chrono::duration<double>(now - m_frameTimestamps.front()).count() > 1.0) {
                m_frameTimestamps.pop_front();
            }
            m_stats.fps = (float)m_frameTimestamps.size();

            // Handle menu stuff.
            if (m_menuHandler) {
                m_menuHandler->handleInput();
                m_menuHandler->updateStatistics(m_stats);
            }

            // Unbind all textures from the render targets.
            m_graphicsDevice->clearRenderTargets();

            std::shared_ptr<graphics::ITexture> topLayer[2] = {};

            // Apply the processing chain to all the (supported) layers.
            for (uint32_t i = 0; i < frameEndInfo->layerCount; i++) {
                if (frameEndInfo->layers[i]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
                    const XrCompositionLayerProjection* proj =
                        reinterpret_cast<const XrCompositionLayerProjection*>(frameEndInfo->layers[i]);

                    // For VPRT, we need to handle texture arrays.
                    const bool useVPRT = proj->views[0].subImage.swapchain == proj->views[1].subImage.swapchain;

                    assert(proj->viewCount == 2);
                    for (uint32_t eye = 0; eye < 2; eye++) {
                        const XrCompositionLayerProjectionView& view = proj->views[eye];

                        auto swapchainIt = m_swapchains.find(view.subImage.swapchain);
                        if (swapchainIt == m_swapchains.end()) {
                            throw new std::runtime_error("Swapchain is not registered");
                        }
                        auto swapchainState = swapchainIt->second;
                        auto swapchainImages = swapchainState.images[swapchainState.acquiredImageIndex];
                        uint32_t nextImage = 0;
                        uint32_t lastImage = 0;

                        // TODO: Insert processing below.

                        // Perform post-processing.
                        if (m_preProcessor) {
                            nextImage++;
                            m_preProcessor->process(
                                swapchainImages.chain[lastImage], swapchainImages.chain[nextImage], useVPRT ? eye : -1);
                            lastImage++;
                        }

                        // Perform upscaling (if requested).
                        if (m_upscaler) {
                            nextImage++;
                            m_upscaler->upscale(
                                swapchainImages.chain[lastImage], swapchainImages.chain[nextImage], useVPRT ? eye : -1);
                            lastImage++;
                        }

                        // Perform post-processing.
                        if (m_postProcessor) {
                            nextImage++;
                            m_postProcessor->process(
                                swapchainImages.chain[lastImage], swapchainImages.chain[nextImage], useVPRT ? eye : -1);
                            lastImage++;
                        }

                        // Make sure the chain was completed.
                        if (nextImage != swapchainImages.chain.size() - 1) {
                            throw new std::runtime_error("Processing chain incomplete!");
                        }

                        topLayer[eye] = swapchainImages.chain[nextImage];

                        // TODO: This is non-compliant AND dangerous. We cannot bypass the constness here and should
                        // make a copy instead.
                        ((XrCompositionLayerProjectionView*)&view)->subImage.imageRect.extent.width = m_displayWidth;
                        ((XrCompositionLayerProjectionView*)&view)->subImage.imageRect.extent.height = m_displayHeight;
                    }
                }
            }

            // Render the menu in the top-most layer.
            if (m_menuHandler && topLayer[0]) {
                // When using VPRT, we rely on the menu renderer to render both views at once. Otherwise we
                // render each view one at a time.
                m_menuHandler->render(topLayer[0]);
                if (topLayer[1] != topLayer[0]) {
                    m_menuHandler->render(topLayer[1]);
                }
            }

            return OpenXrApi::xrEndFrame(session, frameEndInfo);
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
