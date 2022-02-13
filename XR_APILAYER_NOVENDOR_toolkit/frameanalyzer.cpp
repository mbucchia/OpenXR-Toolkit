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
                      uint32_t displayHeight)
            : m_configManager(configManager), m_device(graphicsDevice), m_displayWidth(displayWidth),
              m_displayHeight(displayHeight) {
        }

        void registerColorSwapchainImage(std::shared_ptr<ITexture> source, Eye eye) override {
            m_eyeSwapchainImages[(int)eye].insert(source->getNativePtr());
        }

        void resetForFrame() override {
            // Assumes left eye is first in case we won't be able to tell for sure.
            m_eyePrediction = Eye::Left;
            m_isPredictionValid = m_shouldPredictEye;
        }

        void prepareForEndFrame() override {
        }

        void onSetRenderTarget(std::shared_ptr<graphics::IContext> context,
                               std::shared_ptr<ITexture> renderTarget) override {
            if (renderTarget->getInfo().arraySize != 1) {
                return;
            }

            const void* const nativePtr = renderTarget->getNativePtr();

            // Handle when the application uses the swapchain image directly.
            if (m_eyeSwapchainImages[0].find(nativePtr) != m_eyeSwapchainImages[0].cend()) {
                DebugLog("Detected setting RTV to left eye\n");
                m_eyePrediction = Eye::Left;

                // We are confident our prediction is accurate.
                m_shouldPredictEye = true;
            } else if (m_eyeSwapchainImages[1].find(nativePtr) != m_eyeSwapchainImages[1].cend()) {
                DebugLog("Detected setting RTV to right eye\n");
                m_eyePrediction = Eye::Right;

                // We are confident our prediction is accurate.
                m_shouldPredictEye = true;
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

            // Handle when the application copies the texture to the swapchain image mid-pass. Assumes left eye is
            // always first (hence we only detect changes to switch to right eye). This is what FS2020 does.
            if (m_eyeSwapchainImages[0].find(nativePtr) != m_eyeSwapchainImages[0].cend()) {
                DebugLog("Detected copy-out to left eye\n");

                // Switch to right eye now.
                m_eyePrediction = Eye::Right;

                // We are confident our prediction is accurate.
                m_shouldPredictEye = true;
            }
#ifdef _DEBUG
            else if (m_eyeSwapchainImages[1].find(nativePtr) != m_eyeSwapchainImages[1].cend()) {
                DebugLog("Detected copy-out to right eye\n");
            }
#endif
        }

        std::optional<Eye> getEyeHint() const override {
            if (!m_isPredictionValid) {
                return std::nullopt;
            }
            return m_eyePrediction;
        }

      private:
        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_device;
        const uint32_t m_displayWidth;
        const uint32_t m_displayHeight;

        std::set<const void*> m_eyeSwapchainImages[ViewCount];

        bool m_shouldPredictEye{false};
        bool m_isPredictionValid{false};
        Eye m_eyePrediction;
    };

} // namespace

namespace toolkit::graphics {
    std::shared_ptr<IFrameAnalyzer> CreateFrameAnalyzer(std::shared_ptr<IConfigManager> configManager,
                                                        std::shared_ptr<IDevice> graphicsDevice,
                                                        uint32_t displayWidth,
                                                        uint32_t displayHeight) {
        return std::make_shared<FrameAnalyzer>(configManager, graphicsDevice, displayWidth, displayWidth);
    }

} // namespace toolkit::graphics
