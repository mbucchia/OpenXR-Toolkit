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
                // Remember the XrSystemId to use.
                m_vrSystemId = *systemId;
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
            }

            const XrResult result = OpenXrApi::xrCreateSwapchain(session, &chainCreateInfo, swapchain);
            if (XR_SUCCEEDED(result) && useSwapchain) {
                // Store the runtime images into the state (last entry in the processing chain).
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
                        images.chain.push_back(graphics::WrapD3D11Texture(
                            m_graphicsDevice, chainCreateInfo, d3dImages[i].texture, "Runtime swapchain TEX2D"));

                        swapchainState.images.push_back(images);
                    }
                } else {
                    throw new std::runtime_error("Unsupported graphics runtime");
                }

                // TODO: Create other entries in the chain based on the processing to do (scaling, post-processing...).

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

            // Identify the last projection layer: this is the one we will draw onto.
            int32_t lastProjectionLayerIndex = -1;
            for (uint32_t i = 0; i < frameEndInfo->layerCount; i++) {
                if (frameEndInfo->layers[i]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
                    lastProjectionLayerIndex = i;
                }
            }

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

                        // TODO: Insert processing here.

                        // Render the menu in the last entry of the last projection layer.
                        if (m_menuHandler && i == lastProjectionLayerIndex) {
                            // When using VPRT, we rely on the menu renderer to render both views at once. Otherwise we
                            // render each view one at a time.
                            if (!useVPRT || eye == 0) {
                                m_menuHandler->render(swapchainImages.chain[nextImage]);
                            }
                        }
                    }
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

        std::shared_ptr<config::IConfigManager> m_configManager;

        std::shared_ptr<graphics::IDevice> m_graphicsDevice;
        std::map<XrSwapchain, SwapchainState> m_swapchains;

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
