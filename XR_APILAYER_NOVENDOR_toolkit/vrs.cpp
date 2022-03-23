// MIT License
//
// Copyright(c) 2022 Matthieu Bucchianeri
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

#include "pch.h"

#include "factories.h"
#include "interfaces.h"
#include "layer.h"
#include "log.h"

#define CHECK_NVCMD(cmd) xr::detail::_CheckNVResult(cmd, #cmd, FILE_AND_LINE)

namespace xr::detail {

    [[noreturn]] inline void _ThrowNVResult(NvAPI_Status nvs,
                                            const char* originator = nullptr,
                                            const char* sourceLocation = nullptr) {
        xr::detail::_Throw(xr::detail::_Fmt("NvAPI_Status failure [%x]", nvs), originator, sourceLocation);
    }

    inline HRESULT _CheckNVResult(NvAPI_Status nvs,
                                  const char* originator = nullptr,
                                  const char* sourceLocation = nullptr) {
        if ((nvs) != NVAPI_OK) {
            xr::detail::_ThrowNVResult(nvs, originator, sourceLocation);
        }

        return nvs;
    }

} // namespace xr::detail

namespace {

    using namespace toolkit;
    using namespace toolkit::config;
    using namespace toolkit::log;
    using namespace toolkit::graphics;
    using namespace toolkit::utilities;

    template <typename T>
    constexpr T integer_log2(T n) noexcept {
        // _HAS_CXX20: std::bit_width(m_tileSize) - 1;
        return (n > 1u) ? 1u + integer_log2(n >> 1u) : 0;
    }

    // Standard shading rates
    enum ShadingRates {
        SHADING_RATE_CULL,
        SHADING_RATE_x16,
        SHADING_RATE_x8,
        SHADING_RATE_x4,
        SHADING_RATE_x2,
        SHADING_RATE_x1,
        SHADING_RATE_2x1,
        SHADING_RATE_1x2,
        SHADING_RATE_2x2,
        SHADING_RATE_4x2,
        SHADING_RATE_2x4,
        SHADING_RATE_4x4,
    };

    // Constant buffer
    struct alignas(16) ShadingConstants {
        float Gaze[4];    // u,v,1/w,1/h
        float Ring12[4];  // sx1,sy1,sx2,sy2
        float Ring34[4];  // sx3,sy3,sx4,sy4
        int Rates[4];     // r1,r2,r3,r4
    };

    class VariableRateShader : public IVariableRateShader {
      public:
        VariableRateShader(std::shared_ptr<IConfigManager> configManager,
                           std::shared_ptr<IDevice> graphicsDevice,
                           std::shared_ptr<input::IEyeTracker> eyeTracker,
                           uint32_t targetWidth,
                           uint32_t targetHeight,
                           uint32_t tileSize,
                           uint32_t tileRateMax)
            : m_configManager(configManager), m_device(graphicsDevice), m_eyeTracker(eyeTracker),
              m_targetWidth(targetWidth), m_targetHeight(targetHeight), m_tileSize(tileSize),
              m_tileRateMax(tileRateMax), m_targetAspectRatio((float)m_targetWidth / m_targetHeight) {
            // Make sure to unload NvAPI on destruction
            m_NvShadingRateResources.deferredUnloadNvAPI.needUnload = m_device->getApi() == Api::D3D11;

            // Create a texture with the shading rates.
            createRenderResources(m_targetWidth, m_targetHeight);

            // Set initial projection center
            updateGazeLocation(0, 0);
        }

        ~VariableRateShader() override {
            disable();

            // TODO: Releasing the NVAPI resources sometimes leads to a crash... We leak them for now.
            for (auto& it : m_NvShadingRateResources.views) {
                it.Detach();
            }
            m_NvShadingRateResources.viewsVPRT.Detach();
        }

        void beginFrame(XrTime frameTime) override {
            m_needUpdateEye = m_eyeTracker && m_configManager->getValue(SettingEyeTrackingEnabled);
        }

        void endFrame() override {
            disable();
        }

        void update() override {
            const auto mode = m_configManager->getEnumValue<VariableShadingRateType>(config::SettingVRS);
            const auto prev_mode = std::exchange(m_mode, mode);

            if (m_mode == VariableShadingRateType::None)
                return;

            // Update the shading rates.
            // Only update the texture when necessary.
            const bool hasQualityChanged =
                mode != prev_mode || m_device->getApi() == Api::D3D12 ||
                (mode == VariableShadingRateType::Preset && m_configManager->hasChanged(SettingVRSQuality)) ||
                (mode == VariableShadingRateType::Custom &&
                 (m_configManager->hasChanged(SettingVRSInner) || m_configManager->hasChanged(SettingVRSMiddle) ||
                  m_configManager->hasChanged(SettingVRSOuter) ||
                  m_configManager->hasChanged(SettingVRSPreferHorizontal)));

            if (hasQualityChanged) {
                updateShadingRates();
            }

            const bool hasInnerRadiusChanged = m_configManager->hasChanged(SettingVRSInnerRadius);
            const bool hasOuterRadiusChanged = m_configManager->hasChanged(SettingVRSOuterRadius);

            const bool hasPatternChanged =
                (mode == VariableShadingRateType::Preset && m_configManager->hasChanged(SettingVRSPattern)) ||
                (mode == VariableShadingRateType::Custom &&
                 (hasInnerRadiusChanged || hasOuterRadiusChanged || m_configManager->hasChanged(SettingVRSXScale) ||
                  m_configManager->hasChanged(SettingVRSXOffset) || m_configManager->hasChanged(SettingVRSYOffset)));

            // For D3D11, the texture does not contain the actual rates, so we only need to update it when the
            // pattern changes.
            bool needRegeneratePattern = hasQualityChanged || hasPatternChanged || m_hasProjCenterChanged ||
                                         m_configManager->hasChanged(SettingEyeTrackingEnabled);

            // Update the VRS texture if needed.
            if (needRegeneratePattern) {
                // Adjust inner/outer radius to make sure we have valid bands.
                int innerRadius = m_configManager->getValue(SettingVRSInnerRadius);
                int outerRadius = m_configManager->getValue(SettingVRSOuterRadius);
                if (innerRadius > outerRadius) {
                    if (hasInnerRadiusChanged) {
                        outerRadius = innerRadius;
                        m_configManager->setValue(SettingVRSOuterRadius, outerRadius);
                    } else if (hasOuterRadiusChanged) {
                        innerRadius = outerRadius;
                        m_configManager->setValue(SettingVRSInnerRadius, innerRadius);
                    }
                }

                // TODO:
                updateFoveationMasks(0 /*m_projCenterX*/, 0 /*m_projCenterY*/, innerRadius, outerRadius);
                m_hasProjCenterChanged = false;
            }
        }

        bool onSetRenderTarget(std::shared_ptr<graphics::IContext> context,
                               std::shared_ptr<ITexture> renderTarget,
                               std::optional<Eye> eyeHint) override {
            const auto& info = renderTarget->getInfo();

            if (m_mode == VariableShadingRateType::None || !isVariableRateShadingCandidate(info)) {
                disable(context);
                return false;
            }

            // Attempt to update the foveation mask based on eye gaze.
            if (m_needUpdateEye) {
                assert(m_eyeTracker);
                // In normalized screen coordinates.
                float gazeScreenX[ViewCount], gazeScreenY[ViewCount];
                if (m_eyeTracker->getProjectedGaze(gazeScreenX, gazeScreenY)) {
                    int gazeX[ViewCount];
                    int gazeY[ViewCount];
                    for (uint32_t i = 0; i < ViewCount; i++) {
                        gazeX[i] = (int)(gazeScreenX[i] * m_targetWidth);
                        gazeY[i] = (int)(gazeScreenY[i] * m_targetHeight);
                    }

                    updateFoveationMasks(
                        gazeX, gazeY, -1, -1, eyeHint.has_value() && info.arraySize == 1, info.arraySize > 1, false);
                    m_needUpdateEye = false;

                    // TODO: What do we do upon (permanent) loss of tracking?
                }
            }

            DebugLog("VRS: Enable\n");
            if (auto context11 = context->getAs<D3D11>()) {
                // We set VRS on 2 viewports in case the stereo view renders in parallel.
                NV_D3D11_VIEWPORT_SHADING_RATE_DESC viewports[2];
                viewports[1] = viewports[0] = m_NvShadingRateResources.rates;
                viewports[0].enableVariablePixelShadingRate = true;
                viewports[1].enableVariablePixelShadingRate = true;

                // viewports[0].shadingRateTable[0] = m_NvShadingRateResources.innerShadingRate;
                // viewports[0].shadingRateTable[1] = m_NvShadingRateResources.middleShadingRate;
                // viewports[0].shadingRateTable[2] = m_NvShadingRateResources.outerShadingRate;
                // viewports[1] = viewports[0];

                NV_D3D11_VIEWPORTS_SHADING_RATE_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.version = NV_D3D11_VIEWPORTS_SHADING_RATE_DESC_VER;
                desc.numViewports = 2;
                desc.pViewports = viewports;
                CHECK_NVCMD(NvAPI_D3D11_RSSetViewportsPixelShadingRates(context11, &desc));

                auto& mask = info.arraySize == 2 ? m_NvShadingRateResources.viewsVPRT
                                                 : m_NvShadingRateResources.views[(uint32_t)eyeHint.value_or(Eye::Both)];

                CHECK_NVCMD(NvAPI_D3D11_RSSetShadingRateResourceView(context11, get(mask)));

#ifdef _DEBUG
                doCapture(context /* post */);
                doCapture(context, renderTarget, eyeHint);
#endif
            } else if (auto context12 = context->getAs<D3D12>()) {
                ComPtr<ID3D12GraphicsCommandList5> vrsCommandList;
                if (FAILED(context12->QueryInterface(set(vrsCommandList)))) {
                    DebugLog("VRS: failed to query ID3D12GraphicsCommandList5\n");
                    return false;
                }

                const D3D12_SHADING_RATE_COMBINER combiner[] = {D3D12_SHADING_RATE_COMBINER_PASSTHROUGH,
                                                                D3D12_SHADING_RATE_COMBINER_OVERRIDE};
                vrsCommandList->RSSetShadingRate(D3D12_SHADING_RATE_1X1, combiner);

                // TODO: With DX12, the mask cannot be a texture array. For now we just use the generic mask.
                auto& mask = m_shadingRateMask[(uint32_t)eyeHint.value_or(Eye::Both)];

                vrsCommandList->RSSetShadingRateImage(mask->getAs<D3D12>());
            } else {
                throw std::runtime_error("Unsupported graphics runtime");
            }

            return true;
        }

        void onUnsetRenderTarget(std::shared_ptr<graphics::IContext> context) override {
            disable(context);
        }

        void updateGazeLocation(float ndc_x, float ndc_y) override {
            m_gazeLocation[0].x = m_targetWidth * ndc_x;
            m_gazeLocation[0].y = m_targetHeight * ndc_y;
            m_gazeLocation[1].x = m_targetWidth * ndc_x;
            m_gazeLocation[1].y = m_targetHeight * ndc_y;
            m_hasProjCenterChanged = true;
        }

        uint8_t getMaxRate() const override {
            return static_cast<uint8_t>(m_tileRateMax);
        }

#ifdef _DEBUG
        void startCapture() override {
            DebugLog("VRS: Start capture\n");
            m_captureID++;
            m_captureFileIndex = 0;
            m_isCapturing = true;
        }

        void stopCapture() override {
            if (m_isCapturing) {
                DebugLog("VRS: Stop capture\n");
                m_isCapturing = false;
            }
        }

        void doCapture(std::shared_ptr<graphics::IContext> context,
                       std::shared_ptr<ITexture> renderTarget = nullptr,
                       std::optional<Eye> eyeHint = std::nullopt) {
            if (m_isCapturing) {
                if (renderTarget) {
                    const auto& info = renderTarget->getInfo();

                    // At the beginning of the frame, capture all the masks too.
                    if (!m_captureFileIndex) {
                        m_shadingRateMask[0]->saveToFile(
                            (localAppData / "screenshots" / fmt::format("vrs_{}_mask_left.dds", m_captureID)).string());
                        m_shadingRateMask[1]->saveToFile(
                            (localAppData / "screenshots" / fmt::format("vrs_{}_mask_right.dds", m_captureID))
                                .string());
                        m_shadingRateMask[2]->saveToFile(
                            (localAppData / "screenshots" / fmt::format("vrs_{}_mask_generic.dds", m_captureID))
                                .string());
                        m_shadingRateMaskVPRT->saveToFile(
                            (localAppData / "screenshots" / fmt::format("vrs_{}_mask_vprt.dds", m_captureID)).string());
                    }

                    DebugLog("VRS: Capturing file ID: %d\n", m_captureFileIndex);

                    renderTarget->saveToFile(
                        (localAppData / "screenshots" /
                         fmt::format("vrs_{}_{}_{}_pre.dds",
                                     m_captureID,
                                     m_captureFileIndex,
                                     info.arraySize == 2   ? "dual"
                                     : eyeHint.has_value() ? eyeHint.value() == Eye::Left ? "left" : "right"
                                                           : "generic"))
                            .string());

                    m_currentRenderTarget = renderTarget;
                    m_currentEyeHint = eyeHint;
                } else if (m_currentRenderTarget) {
                    const auto& info = m_currentRenderTarget->getInfo();

                    m_currentRenderTarget->saveToFile(
                        (localAppData / "screenshots" /
                         fmt::format("vrs_{}_{}_{}_post.dds",
                                     m_captureID,
                                     m_captureFileIndex++,
                                     info.arraySize == 2 ? "dual"
                                     : m_currentEyeHint.has_value()
                                         ? m_currentEyeHint.value() == Eye::Left ? "left" : "right"
                                         : "generic"))
                            .string());

                    m_currentRenderTarget = nullptr;
                }
            }
        }
#endif

      private:
        void createRenderResources(uint32_t renderWidth, uint32_t renderHeigh) {
            auto texW = xr::math::DivideRoundingUp(renderWidth, m_tileSize);
            auto texH = xr::math::DivideRoundingUp(renderHeigh, m_tileSize);
            {
                XrSwapchainCreateInfo info;
                ZeroMemory(&info, sizeof(info));
                info.width = texW;
                info.height = texH;
                info.format = DXGI_FORMAT_R8_UINT;
                info.arraySize = 1;
                info.mipCount = 1;
                info.sampleCount = 1;
                info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;

                for (auto& it : m_shadingRateMask) {
                    it = m_device->createTexture(info, "VRS TEX2D");
                }
                info.arraySize = 2;
                m_shadingRateMaskVPRT = m_device->createTexture(info, "VRS VPRT TEX2D");
            }

            // Initialize API-specific resources.
            if (auto device11 = m_device->getAs<D3D11>()) {
                m_NvShadingRateResources.resetRates();

                NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.version = NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC_VER;
                desc.Format = DXGI_FORMAT_R8_UINT;
                desc.ViewDimension = NV_SRRV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MipSlice = 0;

                for (size_t i = 0; i < std::size(m_shadingRateMask); i++) {
                    CHECK_NVCMD(NvAPI_D3D11_CreateShadingRateResourceView(
                        device11, m_shadingRateMask[i]->getAs<D3D11>(), &desc, set(m_NvShadingRateResources.views[i])));
                }

                desc.ViewDimension = NV_SRRV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.ArraySize = 2;
                CHECK_NVCMD(NvAPI_D3D11_CreateShadingRateResourceView(
                    device11, m_shadingRateMaskVPRT->getAs<D3D11>(), &desc, set(m_NvShadingRateResources.viewsVPRT)));

            } else if (m_device->getAs<D3D12>()) {
                // Nothing extra to initialize.
            }

            // Initialize compute shader
            {
                const auto shadersDir = dllHome / "shaders";
                const auto shaderFile = shadersDir / "VRS.hlsl";

                const std::array<unsigned int, 3> threadGroups = {texW, texH, 1};

                m_csShadingRate = m_device->createComputeShader(shaderFile, "mainCS", "VRS CS", threadGroups);
                m_cbShadingRate = m_device->createBuffer(sizeof(ShadingConstants), "VRS CB");
            }

            DebugLog("VRS: targetWidth=%u targetHeight=%u tileSize=%u\n", m_targetWidth, m_targetHeight, m_tileSize);
        }

        void disable(std::shared_ptr<graphics::IContext> context = nullptr) {
            DebugLog("VRS: Disable\n");
            if (m_device->getApi() == Api::D3D11) {
                auto context11 = context ? context->getAs<D3D11>() : m_device->getContextAs<D3D11>();

                NV_D3D11_VIEWPORTS_SHADING_RATE_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.version = NV_D3D11_VIEWPORTS_SHADING_RATE_DESC_VER;
                CHECK_NVCMD(NvAPI_D3D11_RSSetViewportsPixelShadingRates(context11, &desc));
                CHECK_NVCMD(NvAPI_D3D11_RSSetShadingRateResourceView(context11, nullptr));

#ifdef _DEBUG
                doCapture(context /* post */);
#endif
            } else if (m_device->getApi() == Api::D3D12) {
                auto context12 = context ? context->getAs<D3D12>() : m_device->getContextAs<D3D12>();

                ComPtr<ID3D12GraphicsCommandList5> vrsCommandList;
                if (FAILED(context12->QueryInterface(set(vrsCommandList)))) {
                    DebugLog("VRS: failed to query ID3D12GraphicsCommandList5\n");
                    return;
                }

                vrsCommandList->RSSetShadingRateImage(nullptr);
            } else {
                throw std::runtime_error("Unsupported graphics runtime");
            }
        }

        void updateShadingRates() {
            int innerRate, middleRate, outerRate;
            if (m_mode == VariableShadingRateType::Preset) {
                const auto quality = m_configManager->getEnumValue<VariableShadingRateQuality>(SettingVRSQuality);
                std::tie(innerRate, middleRate, outerRate) = getShadingRateForQuality(quality);
            } else {
                innerRate = m_configManager->getValue(SettingVRSInner);
                middleRate = m_configManager->getValue(SettingVRSMiddle);
                outerRate = m_configManager->getValue(SettingVRSOuter);
            }

            // Cap to device's capabilities.
            innerRate = std::clamp(static_cast<int>(m_tileRateMax), 1, innerRate);
            middleRate = std::clamp(static_cast<int>(m_tileRateMax), 1, middleRate);
            outerRate = std::clamp(static_cast<int>(m_tileRateMax), 1, outerRate);

            const bool preferHorizontal =
                m_mode == VariableShadingRateType::Custom && m_configManager->getValue(SettingVRSPreferHorizontal);

            if (m_device->getApi() == Api::D3D11) {
                // These are handled through an indirection table.
                m_NvShadingRateResources.rates.shadingRateTable[0] = getNVAPIShadingRate(innerRate, preferHorizontal);
                m_NvShadingRateResources.rates.shadingRateTable[1] = getNVAPIShadingRate(middleRate, preferHorizontal);
                m_NvShadingRateResources.rates.shadingRateTable[2] = getNVAPIShadingRate(outerRate, preferHorizontal);
                m_innerValue = 0;
                m_middleValue = 1;
                m_outerValue = 2;
            } else if (m_device->getApi() == Api::D3D12) {
                m_innerValue = getD3D12ShadingRate(innerRate, preferHorizontal);
                m_middleValue = getD3D12ShadingRate(middleRate, preferHorizontal);
                m_outerValue = getD3D12ShadingRate(outerRate, preferHorizontal);
            }
        }

        void updateFoveationMasks(int projCenterX[ViewCount],
                                  int projCenterY[ViewCount],
                                  int innerRadius,
                                  int outerRadius,
                                  bool updateSingleRTV = true,
                                  bool updateArrayRTV = true,
                                  bool updateFallbackRTV = true) {
            if (m_mode == VariableShadingRateType::Preset) {
                std::tie(innerRadius, outerRadius) =
                    getRadiusForPattern(m_configManager->getEnumValue<VariableShadingRatePattern>(SettingVRSPattern));
            } else if (innerRadius < 0 || outerRadius < 0) {
                innerRadius = m_configManager->getValue(SettingVRSInnerRadius);
                outerRadius = m_configManager->getValue(SettingVRSOuterRadius);
            }

            const auto xOffset =
                static_cast<int>(m_configManager->getValue(SettingVRSXOffset) * (m_targetWidth / 200.f));
            const auto yOffset =
                static_cast<int>(m_configManager->getValue(SettingVRSYOffset) * (m_targetHeight / 200.f));
            const float semiMajorFactor = m_configManager->getValue(SettingVRSXScale) / 100.f;

            const int rowPitch = roundUp(m_targetWidth, m_tileSize) / m_tileSize;
            const int rowPitchAligned = roundUp(rowPitch, m_device->getTextureAlignmentConstraint());

            static_assert(ViewCount == 2);
            std::vector<uint8_t> leftPattern;
            generateFoveationPattern(leftPattern,
                                     rowPitchAligned,
                                     projCenterX[0] + xOffset,
                                     projCenterY[0] + yOffset,
                                     innerRadius / 100.f,
                                     outerRadius / 100.f,
                                     semiMajorFactor,
                                     m_innerValue,
                                     m_middleValue,
                                     m_outerValue);

            std::vector<uint8_t> rightPattern;
            generateFoveationPattern(rightPattern,
                                     rowPitchAligned,
                                     projCenterX[1] - xOffset,
                                     projCenterY[1] + yOffset,
                                     innerRadius / 100.f,
                                     outerRadius / 100.f,
                                     semiMajorFactor,
                                     m_innerValue,
                                     m_middleValue,
                                     m_outerValue);

            std::vector<uint8_t> genericPattern;
            if (updateFallbackRTV) {
                generateFoveationPattern(genericPattern,
                                         rowPitchAligned,
                                         m_targetWidth / 2, // Cannot apply xOffset.
                                         (m_targetHeight / 2) + yOffset,
                                         innerRadius / 100.f,
                                         outerRadius / 100.f,
                                         semiMajorFactor,
                                         m_innerValue,
                                         m_middleValue,
                                         m_outerValue);
            }

            if (updateSingleRTV) {
                m_shadingRateMask[0]->uploadData(leftPattern.data(), rowPitchAligned);
                m_shadingRateMask[1]->uploadData(rightPattern.data(), rowPitchAligned);
            }
            if (updateFallbackRTV) {
                m_shadingRateMask[2]->uploadData(genericPattern.data(), rowPitchAligned);
            }

            if (updateArrayRTV) {
                m_shadingRateMaskVPRT->uploadData(leftPattern.data(), rowPitchAligned, 0);
                m_shadingRateMaskVPRT->uploadData(rightPattern.data(), rowPitchAligned, 1);
            }

            if (m_device->getApi() == Api::D3D12) {
                // TODO: Need to use the immediate context instead! (same for uploadData above).
                auto context12 = m_device->getContextAs<D3D12>();
                {
                    const D3D12_RESOURCE_BARRIER barriers[] = {
                        CD3DX12_RESOURCE_BARRIER::Transition(m_shadingRateMask[0]->getAs<D3D12>(),
                                                             D3D12_RESOURCE_STATE_COMMON,
                                                             D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE),
                        CD3DX12_RESOURCE_BARRIER::Transition(m_shadingRateMask[1]->getAs<D3D12>(),
                                                             D3D12_RESOURCE_STATE_COMMON,
                                                             D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE),
                        CD3DX12_RESOURCE_BARRIER::Transition(m_shadingRateMask[2]->getAs<D3D12>(),
                                                             D3D12_RESOURCE_STATE_COMMON,
                                                             D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE),
                        CD3DX12_RESOURCE_BARRIER::Transition(m_shadingRateMaskVPRT->getAs<D3D12>(),
                                                             D3D12_RESOURCE_STATE_COMMON,
                                                             D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE)};
                    context12->ResourceBarrier(ARRAYSIZE(barriers), barriers);
                }
            }
        }

        void generateFoveationPattern(std::vector<uint8_t>& pattern,
                                      size_t rowPitch,
                                      int projCenterX,
                                      int projCenterY,
                                      float innerRadius,
                                      float outerRadius,
                                      float semiMajorFactor,
                                      uint8_t innerValue,
                                      uint8_t middleValue,
                                      uint8_t outerValue) {
            const auto width = roundUp(m_targetWidth, m_tileSize) / m_tileSize;
            const auto height = roundUp(m_targetHeight, m_tileSize) / m_tileSize;

            pattern.resize(rowPitch * height);
            const int centerX = std::clamp(projCenterX, 0, (int)m_targetWidth) / m_tileSize;
            const int centerY = std::clamp(projCenterY, 0, (int)m_targetHeight) / m_tileSize;

            const int innerSemiMinor = (int)(height * innerRadius / 2);
            const int innerSemiMajor = (int)(semiMajorFactor * innerSemiMinor);
            const int outerSemiMinor = (int)(height * outerRadius / 2);
            const int outerSemiMajor = (int)(semiMajorFactor * outerSemiMinor);

            // No shame in looking up basic maths :D
            // https://www.geeksforgeeks.org/check-if-a-point-is-inside-outside-or-on-the-ellipse/
            auto isInsideEllipsis = [](int h, int k, int x, int y, int a, int b) {
                return (pow((x - h), 2) / pow(a, 2)) + (pow((y - k), 2) / pow(b, 2));
            };

            for (uint32_t y = 0; y < height; y++) {
                for (uint32_t x = 0; x < width; x++) {
                    uint8_t rate = outerValue;
                    if (isInsideEllipsis(centerX, centerY, x, y, innerSemiMajor, innerSemiMinor) < 1) {
                        rate = innerValue;
                    } else if (isInsideEllipsis(centerX, centerY, x, y, outerSemiMajor, outerSemiMinor) < 1) {
                        rate = middleValue;
                    }

                    pattern[y * rowPitch + x] = rate;
                }
            }
        }

        bool isVariableRateShadingCandidate(const XrSwapchainCreateInfo& info) const {
            // Check for proportionality with the size of our render target.
            // Also check that the texture is not under 50% of the render scale. We expect that no one should use
            // in-app render scale that is so small.
            DebugLog("VRS: info.width=%u info.height=%u\n", info.width, info.height);
            if (info.width < (m_targetWidth / 2))
                return false;

            const float aspectRatio = (float)info.width / info.height;
            if (std::abs(aspectRatio - m_targetAspectRatio) > 0.01f)
                return false;

            DebugLog("VRS: info.arraySize=%u\n", info.arraySize);
            if (info.arraySize > 2)
                return false;

            DebugLog("VRS: info.format=%u\n", info.format);

            return true;
        }

        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_device;
        const std::shared_ptr<input::IEyeTracker> m_eyeTracker;

        const uint32_t m_targetWidth;
        const uint32_t m_targetHeight;
        const uint32_t m_tileSize;
        const uint32_t m_tileRateMax;

        const float m_targetAspectRatio;

        VariableShadingRateType m_mode{VariableShadingRateType::None};

        XrVector2f m_gazeLocation[ViewCount];

        // bool m_isEnabled{true};

        int m_innerValue{0};
        int m_middleValue{0};
        int m_outerValue{0};

        bool m_hasProjCenterChanged{false};
        bool m_needUpdateEye{false};

        struct NvShadingRateResouces {
            // Must appear first.
            struct DeferredNvAPI_Unload {
                ~DeferredNvAPI_Unload() {
                    if (needUnload) {
                        // TODO: Unloading leads to a crash...
                        // NvAPI_Unload();
                    }
                }

                bool needUnload{false};
            } deferredUnloadNvAPI;

            void resetRates() {
                rates.enableVariablePixelShadingRate = false;
                std::fill_n(rates.shadingRateTable, std::size(rates.shadingRateTable), NV_PIXEL_X1_PER_RASTER_PIXEL);

                // We use a constant table and a varying shading rate texture filled with a compute shader.
                rates.shadingRateTable[SHADING_RATE_CULL] = NV_PIXEL_X0_CULL_RASTER_PIXELS;
                rates.shadingRateTable[SHADING_RATE_x16] = NV_PIXEL_X16_PER_RASTER_PIXEL;
                rates.shadingRateTable[SHADING_RATE_x8] = NV_PIXEL_X8_PER_RASTER_PIXEL;
                rates.shadingRateTable[SHADING_RATE_x4] = NV_PIXEL_X4_PER_RASTER_PIXEL;
                rates.shadingRateTable[SHADING_RATE_x2] = NV_PIXEL_X2_PER_RASTER_PIXEL;
                rates.shadingRateTable[SHADING_RATE_x1] = NV_PIXEL_X1_PER_RASTER_PIXEL;
                rates.shadingRateTable[SHADING_RATE_2x1] = NV_PIXEL_X1_PER_2X1_RASTER_PIXELS;
                rates.shadingRateTable[SHADING_RATE_1x2] = NV_PIXEL_X1_PER_1X2_RASTER_PIXELS;
                rates.shadingRateTable[SHADING_RATE_2x2] = NV_PIXEL_X1_PER_2X2_RASTER_PIXELS;
                rates.shadingRateTable[SHADING_RATE_4x2] = NV_PIXEL_X1_PER_4X2_RASTER_PIXELS;
                rates.shadingRateTable[SHADING_RATE_2x4] = NV_PIXEL_X1_PER_2X4_RASTER_PIXELS;
                rates.shadingRateTable[SHADING_RATE_4x4] = NV_PIXEL_X1_PER_4X4_RASTER_PIXELS;
            }

            // NV_PIXEL_SHADING_RATE innerShadingRate{NV_PIXEL_X1_PER_RASTER_PIXEL};
            // NV_PIXEL_SHADING_RATE middleShadingRate{NV_PIXEL_X1_PER_RASTER_PIXEL};
            // NV_PIXEL_SHADING_RATE outerShadingRate{NV_PIXEL_X1_PER_RASTER_PIXEL};

            ComPtr<ID3D11NvShadingRateResourceView> views[ViewCount + 1];
            ComPtr<ID3D11NvShadingRateResourceView> viewsVPRT;

            NV_D3D11_VIEWPORT_SHADING_RATE_DESC rates;

        } m_NvShadingRateResources;

        std::shared_ptr<IShaderBuffer> m_cbShadingRate;
        std::shared_ptr<IComputeShader> m_csShadingRate;

        std::shared_ptr<ITexture> m_shadingRateMask[ViewCount + 1];
        std::shared_ptr<ITexture> m_shadingRateMaskVPRT;

#ifdef _DEBUG
        bool m_isCapturing{false};
        uint32_t m_captureID{0};
        uint32_t m_captureFileIndex;

        std::shared_ptr<ITexture> m_currentRenderTarget;
        std::optional<Eye> m_currentEyeHint;
#endif

        // Returns inner, middle, outer downsampling as a power of 2.
        static std::tuple<int, int, int> getShadingRateForQuality(VariableShadingRateQuality quality) {
            switch (quality) {
            case VariableShadingRateQuality::Quality:
                return std::make_tuple(0 /* 1x */, 1 /* 1/2x */, 2 /* 1/4x */);
            case VariableShadingRateQuality::Performance:
            default:
                return std::make_tuple(0 /* 1x */, 2 /* 1/4x */, 4 /* 1/16x */);
            }
        }

        // Returns inner and outer radius, as a percentage.
        static std::tuple<int, int> getRadiusForPattern(VariableShadingRatePattern pattern) {
            switch (pattern) {
            case VariableShadingRatePattern::Wide:
                return std::make_tuple(55, 80);
            case VariableShadingRatePattern::Narrow:
                return std::make_tuple(30, 55);
            case VariableShadingRatePattern::Balanced:
            default:
                return std::make_tuple(50, 60);
            }
        }

        // std::array<NV_PIXEL_SHADING_RATE, 16> getNVAPIShadingRateTable() const {
        //    PixelShadingRate shadingRateTable[MAX_SHADING_RATES];
        //}

        NV_PIXEL_SHADING_RATE getNVAPIShadingRate(int samplingPow2, bool preferHorizontal) {
            // TODO:
            static const NV_PIXEL_SHADING_RATE lut[] = {NV_PIXEL_X1_PER_RASTER_PIXEL,
                                                        NV_PIXEL_X1_PER_2X1_RASTER_PIXELS,
                                                        NV_PIXEL_X1_PER_2X2_RASTER_PIXELS,
                                                        NV_PIXEL_X1_PER_4X2_RASTER_PIXELS,
                                                        NV_PIXEL_X1_PER_4X4_RASTER_PIXELS};

            static_assert(NV_PIXEL_X1_PER_1X2_RASTER_PIXELS == (NV_PIXEL_X1_PER_2X1_RASTER_PIXELS + 1));
            static_assert(NV_PIXEL_X1_PER_2X4_RASTER_PIXELS == (NV_PIXEL_X1_PER_4X2_RASTER_PIXELS + 1));

            if (size_t(samplingPow2) < std::size(lut)) {
            }
            switch (samplingPow2) {
            case 0:
                return NV_PIXEL_X1_PER_RASTER_PIXEL;
            case 1:
                return preferHorizontal ? NV_PIXEL_X1_PER_1X2_RASTER_PIXELS : NV_PIXEL_X1_PER_2X1_RASTER_PIXELS;
            case 2:
                return NV_PIXEL_X1_PER_2X2_RASTER_PIXELS;
            case 3:
                return preferHorizontal ? NV_PIXEL_X1_PER_2X4_RASTER_PIXELS : NV_PIXEL_X1_PER_4X2_RASTER_PIXELS;
            case 4:
                return NV_PIXEL_X1_PER_4X4_RASTER_PIXELS;
            default:
                // Useful for debugging.
                return NV_PIXEL_X0_CULL_RASTER_PIXELS;
            }
        }

        D3D12_SHADING_RATE getD3D12ShadingRate(int samplingPow2, bool preferHorizontal) {
            switch (samplingPow2) {
            case 0:
                return D3D12_SHADING_RATE_1X1;
            case 1:
                return preferHorizontal ? D3D12_SHADING_RATE_1X2 : D3D12_SHADING_RATE_2X1;
            case 2:
                return D3D12_SHADING_RATE_2X2;
            case 3:
                return preferHorizontal ? D3D12_SHADING_RATE_2X4 : D3D12_SHADING_RATE_4X2;
            case 4:
            default:
                return D3D12_SHADING_RATE_4X4;
            }
        }
    };

} // namespace

namespace toolkit::graphics {
    std::shared_ptr<IVariableRateShader> CreateVariableRateShader(std::shared_ptr<IConfigManager> configManager,
                                                                  std::shared_ptr<IDevice> graphicsDevice,
                                                                  std::shared_ptr<input::IEyeTracker> eyeTracker,
                                                                  uint32_t targetWidth,
                                                                  uint32_t targetHeight) {
        try {
            uint32_t tileSize = 0;
            uint32_t tileRateMax = 0;

            if (auto device11 = graphicsDevice->getAs<D3D11>()) {
                auto status = NvAPI_Initialize();
                if (status != NVAPI_OK) {
                    NvAPI_ShortString errorMessage;
                    ZeroMemory(errorMessage, sizeof(errorMessage));
                    if (NvAPI_GetErrorMessage(status, errorMessage) == NVAPI_OK) {
                        Log("Failed to initialize NVAPI: %s\n", errorMessage);
                    }
                    throw FeatureNotSupported();
                }

                NV_D3D1x_GRAPHICS_CAPS graphicCaps;
                ZeroMemory(&graphicCaps, sizeof(graphicCaps));
                status = NvAPI_D3D1x_GetGraphicsCapabilities(device11, NV_D3D1x_GRAPHICS_CAPS_VER, &graphicCaps);
                if (status != NVAPI_OK || !graphicCaps.bVariablePixelRateShadingSupported) {
                    Log("VRS (NVidia) is not supported for this adapter\n");
                    throw FeatureNotSupported();
                }

                // We would normally pass 4 (for 1/16x) but we also want to allow "tile culling".
                tileSize = NV_VARIABLE_PIXEL_SHADING_TILE_WIDTH;
                tileRateMax = 5; // integer_log2(tileSize) + 1;

            } else if (auto device12 = graphicsDevice->getAs<D3D12>()) {
                D3D12_FEATURE_DATA_D3D12_OPTIONS6 options;
                ZeroMemory(&options, sizeof(options));
                if (FAILED(device12->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &options, sizeof(options))) ||
                    options.VariableShadingRateTier != D3D12_VARIABLE_SHADING_RATE_TIER_2 ||
                    options.ShadingRateImageTileSize < 2u) {
                    Log("VRS (DX12 Tier2) is not supported for this adapter\n");
                    throw FeatureNotSupported();
                }

                tileSize = options.ShadingRateImageTileSize;
                tileRateMax = integer_log2(tileSize);

            } else {
                throw std::runtime_error("Unsupported graphics runtime");
            }

            return std::make_shared<VariableRateShader>(
                configManager, graphicsDevice, eyeTracker, targetWidth, targetHeight, tileSize, tileRateMax);

        } catch (FeatureNotSupported&) {
            return nullptr;
        }
    }

} // namespace toolkit::graphics
