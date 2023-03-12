// MIT License
//
// Copyright(c) 2022 Matthieu Bucchianeri
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

#include "factories.h"
#include "interfaces.h"
#include "log.h"

namespace {

    using namespace toolkit;
    using namespace toolkit::config;
    using namespace toolkit::log;
    using namespace toolkit::graphics;
    using namespace toolkit::utilities;

    class FrameAnalyzer : public IFrameAnalyzer {
      public:
        FrameAnalyzer(std::shared_ptr<IConfigManager> configManager,
                      std::shared_ptr<IDevice> graphicsDevice,
                      uint32_t displayWidth,
                      uint32_t displayHeight,
                      FrameAnalyzerHeuristic heuristic)
            : m_configManager(configManager), m_device(graphicsDevice), m_displayWidth(displayWidth),
              m_displayHeight(displayHeight), m_forceHeuristic(heuristic) {
        }

        void registerColorSwapchainImage(XrSwapchain swapchain, std::shared_ptr<ITexture> source, Eye eye) override {
            m_eyeSwapchain[(int)eye].insert(swapchain);
            m_eyeSwapchainImages[(int)eye].insert(source->getNativePtr());
        }

        void resetForFrame() override {
            m_hasSeenLeftEye = m_hasSeenRightEye = false;
            m_hasCopiedLeftEye = m_hasCopiedRightEye = false;

            m_eyePrediction = m_firstEye;
            m_isPredictionValid = m_shouldPredictEye;

            if (m_fallbackDelay) {
                m_fallbackDelay--;
            }
        }

        void prepareForEndFrame() override {
            if (m_heuristic == FrameAnalyzerHeuristic::Unknown) {
                if (m_hasSeenLeftEye && m_hasSeenRightEye &&
                    (m_forceHeuristic == FrameAnalyzerHeuristic::ForwardRender ||
                     m_forceHeuristic == FrameAnalyzerHeuristic::Unknown)) {
                    Log("Detected forward rendering\n");
                    m_heuristic = FrameAnalyzerHeuristic::ForwardRender;
                    m_firstEye = Eye::Left;
                } else if (m_hasCopiedLeftEye && m_hasCopiedRightEye &&
                           (m_forceHeuristic == FrameAnalyzerHeuristic::DeferredCopy ||
                            m_forceHeuristic == FrameAnalyzerHeuristic::Unknown)) {
                    Log("Detected deferred rendering with copy\n");
                    m_heuristic = FrameAnalyzerHeuristic::DeferredCopy;
                    m_firstEye = m_firstEyeCopy;
                } else if (!m_fallbackDelay && (m_forceHeuristic == FrameAnalyzerHeuristic::Fallback ||
                                                m_forceHeuristic == FrameAnalyzerHeuristic::Unknown)) {
                    Log("Fallback to swapchain acquisition\n");
                    m_heuristic = FrameAnalyzerHeuristic::Fallback;
                    m_firstEye = Eye::Left;
                }

                TraceLoggingWrite(
                    g_traceProvider, "FrameAnalyzer_TrySetHeuristic", TLArg((uint32_t)m_heuristic, "Heuristic"));

                m_shouldPredictEye = m_heuristic != FrameAnalyzerHeuristic::Unknown;
            }
        }

        void onSetRenderTarget(std::shared_ptr<graphics::IContext> context,
                               std::shared_ptr<ITexture> renderTarget) override {
            const auto& info = renderTarget->getInfo();
            if (info.arraySize != 1) {
                return;
            }

            const void* const nativePtr = renderTarget->getNativePtr();

            // Handle when the application uses the swapchain image directly.
            if (m_eyeSwapchainImages[0].find(nativePtr) != m_eyeSwapchainImages[0].cend()) {
                TraceLoggingWrite(g_traceProvider, "FrameAnalyzer_DetectedLeftEyeForwardRender");
                m_eyePrediction = Eye::Left;
                m_hasSeenLeftEye = true;
            } else if (m_eyeSwapchainImages[1].find(nativePtr) != m_eyeSwapchainImages[1].cend()) {
                TraceLoggingWrite(g_traceProvider, "FrameAnalyzer_DetectedRightEyeForwardRender");
                m_eyePrediction = Eye::Right;
                m_hasSeenRightEye = true;
            }
        }

        void onUnsetRenderTarget(std::shared_ptr<graphics::IContext> context) override {
        }

        void onCopyTexture(std::shared_ptr<ITexture> source,
                           std::shared_ptr<ITexture> destination,
                           int sourceSlice = -1,
                           int destinationSlice = -1) override {
            if (destination->getInfo().arraySize != 1) {
                return;
            }

            const void* const nativePtr = destination->getNativePtr();

            // Handle when the application copies the texture to the swapchain image mid-pass. This is what FS2020 does.
            if (m_eyeSwapchainImages[0].find(nativePtr) != m_eyeSwapchainImages[0].cend()) {
                TraceLoggingWrite(g_traceProvider, "FrameAnalyzer_DetectedLeftEyeCopyOut");

                if (!m_hasCopiedLeftEye && !m_hasCopiedRightEye) {
                    m_firstEyeCopy = Eye::Left;
                }

                // Switch to right eye now.
                m_eyePrediction = Eye::Right;
                m_hasCopiedLeftEye = true;
            } else if (m_eyeSwapchainImages[1].find(nativePtr) != m_eyeSwapchainImages[1].cend()) {
                TraceLoggingWrite(g_traceProvider, "FrameAnalyzer_DetectedRightEyeCopyOut");

                if (!m_hasCopiedLeftEye && !m_hasCopiedRightEye) {
                    m_firstEyeCopy = Eye::Right;
                }

                // Switch to left eye now.
                m_eyePrediction = Eye::Left;
                m_hasCopiedRightEye = true;
            }
        }

        void onAcquireSwapchain(XrSwapchain swapchain) override {
            // If we don't have a better heuristic, just use the swapchain acquisition order.
            if (m_eyeSwapchain[0].find(swapchain) != m_eyeSwapchain[0].cend()) {
                TraceLoggingWrite(g_traceProvider, "FrameAnalyzer_DetectedLeftEyeSwapchainAcquisition");
                if (m_heuristic == FrameAnalyzerHeuristic::Fallback) {
                    m_eyePrediction = Eye::Left;
                }
            } else if (m_eyeSwapchain[1].find(swapchain) != m_eyeSwapchain[1].cend()) {
                TraceLoggingWrite(g_traceProvider, "FrameAnalyzer_DetectedRightEyeSwapchainAcquisition");
                if (m_heuristic == FrameAnalyzerHeuristic::Fallback) {
                    m_eyePrediction = Eye::Right;
                }
            }
        }

        void onReleaseSwapchain(XrSwapchain swapchain) override {
            // If we don't have a better heuristic, just use the swapchain acquisition order.
            // Switch eye once a swapchain is released.
            if (m_eyeSwapchain[0].find(swapchain) != m_eyeSwapchain[0].cend()) {
                TraceLoggingWrite(g_traceProvider, "FrameAnalyzer_DetectedLeftEyeSwapchainRelease");
                if (m_heuristic == FrameAnalyzerHeuristic::Fallback) {
                    m_eyePrediction = Eye::Right;
                }
            } else if (m_eyeSwapchain[1].find(swapchain) != m_eyeSwapchain[1].cend()) {
                TraceLoggingWrite(g_traceProvider, "FrameAnalyzer_DetectedRightEyeSwapchainRelease");
                if (m_heuristic == FrameAnalyzerHeuristic::Fallback) {
                    m_eyePrediction = Eye::Left;
                }
            }
        }

        std::optional<Eye> getEyeHint() const override {
            if (!m_isPredictionValid) {
                return std::nullopt;
            }
            return m_eyePrediction;
        }

        FrameAnalyzerHeuristic getCurrentHeuristic() const override {
            return m_heuristic;
        }

      private:
        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_device;
        const uint32_t m_displayWidth;
        const uint32_t m_displayHeight;
        const FrameAnalyzerHeuristic m_forceHeuristic;

        std::set<const void*> m_eyeSwapchainImages[ViewCount];
        std::set<XrSwapchain> m_eyeSwapchain[ViewCount];

        bool m_hasSeenLeftEye{false};
        bool m_hasSeenRightEye{false};
        bool m_hasCopiedLeftEye{false};
        bool m_hasCopiedRightEye{false};
        Eye m_firstEyeCopy;
        FrameAnalyzerHeuristic m_heuristic{FrameAnalyzerHeuristic::Unknown};

        bool m_shouldPredictEye{false};
        bool m_isPredictionValid{false};
        Eye m_eyePrediction;
        Eye m_firstEye{Eye::Left};

        uint32_t m_fallbackDelay{100};
    };

} // namespace

namespace toolkit::graphics {
    std::shared_ptr<IFrameAnalyzer> CreateFrameAnalyzer(std::shared_ptr<IConfigManager> configManager,
                                                        std::shared_ptr<IDevice> graphicsDevice,
                                                        uint32_t displayWidth,
                                                        uint32_t displayHeight,
                                                        FrameAnalyzerHeuristic heuristic) {
        return std::make_shared<FrameAnalyzer>(configManager, graphicsDevice, displayWidth, displayHeight, heuristic);
    }

} // namespace toolkit::graphics
