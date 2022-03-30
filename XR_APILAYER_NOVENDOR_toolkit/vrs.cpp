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

#include "shader_utilities.h"
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
        SHADING_RATE_COUNT
    };

    // Constant buffer
    struct alignas(16) ShadingConstants {
        XrVector2f GazeXY;   // ndc
        XrVector2f InvDim;   // 1/w, 1/h
        XrVector2f Rings[4]; // 1/(a1^2), 1/(b1^2)
        uint32_t Rates[4];   // r1, r2, r3, r4
    };

    inline XrVector2f MakeRingParam(XrVector2f size) {
        return {1.f / (size.x * size.x), 1.f / (size.y * size.y)};
    }

    class VariableRateShader : public IVariableRateShader {
      public:
        VariableRateShader(std::shared_ptr<IConfigManager> configManager,
                           std::shared_ptr<IDevice> graphicsDevice,
                           std::shared_ptr<input::IEyeTracker> eyeTracker,
                           uint32_t renderWidth,
                           uint32_t renderHeight,
                           uint32_t displayWidth,
                           uint32_t displayHeight,
                           uint32_t tileSize,
                           uint32_t tileRateMax)
            : m_configManager(configManager), m_device(graphicsDevice), m_eyeTracker(eyeTracker),
              m_renderWidth(renderWidth), m_renderHeight(renderHeight),
              m_renderRatio(float(renderWidth) / renderHeight), m_displayRatio(float(displayHeight) / displayWidth),
              m_tileSize(tileSize), m_tileRateMax(tileRateMax) {
            createRenderResources(m_renderWidth, m_renderHeight);

            // Set initial projection center
            std::fill_n(m_gazeOffset, std::size(m_gazeOffset), XrVector2f{0.f, 0.f});
            updateGazeLocation({0.f, 0.f}, Eye::Both);

            m_needUpdateViews = true;
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
            // When using eye tracking we must render the views every frame.
            if (m_usingEyeTracking)
                m_needUpdateViews = true;
        }

        void endFrame() override {
            disable();
        }

        void update() override {
            const auto mode = m_configManager->getEnumValue<VariableShadingRateType>(config::SettingVRS);
            if (mode != VariableShadingRateType::None) {
                m_usingEyeTracking = m_eyeTracker && m_configManager->getValue(SettingEyeTrackingEnabled);

                const bool hasModeChanged = mode != m_mode;
                const bool hasPatternChanged = hasModeChanged || checkUpdateRings(mode);
                const bool hasQualityChanged = hasModeChanged || checkUpdateRates(mode);

                if (hasPatternChanged)
                    updateRings(mode);

                if (hasQualityChanged)
                    updateRates(mode);

                // Only update the texture when necessary.
                if (hasQualityChanged || hasPatternChanged) {
                    m_mode = mode;
                    m_needUpdateViews = true;
                }

            } else if (m_usingEyeTracking) {
                m_usingEyeTracking = false;
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

            DebugLog("VRS: Enable\n");
            if (auto context11 = context->getAs<D3D11>()) {
                // Attempt to update the foveation mask based on eye gaze.
                if (m_needUpdateViews) {
                    m_needUpdateViews = false;
                    // TODO: What do we do upon (permanent) loss of tracking?
                    // Update the foveation mask based on eye gaze.
                    updateGaze();

                    // TODO: for now redraw all the views until we implement better logic
                    // const auto updateSingleRTV = eyeHint.has_value() && info.arraySize == 1;
                    // const auto updateArrayRTV = info.arraySize > 1;
                    // updateViews(updateSingleRTV, updateArrayRTV, false);
                    updateViews11();
                }

                // We set VRS on 2 viewports in case the stereo view renders in parallel.
                NV_D3D11_VIEWPORTS_SHADING_RATE_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.version = NV_D3D11_VIEWPORTS_SHADING_RATE_DESC_VER;
                desc.numViewports = 2;
                desc.pViewports = m_nvRates;
                CHECK_NVCMD(NvAPI_D3D11_RSSetViewportsPixelShadingRates(context11, &desc));

                auto idx = static_cast<size_t>(eyeHint.value_or(Eye::Both));
                auto& mask =
                    info.arraySize == 2 ? m_NvShadingRateResources.viewsVPRT : m_NvShadingRateResources.views[idx];

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

                // Attempt to update the foveation mask based on eye gaze.
                if (m_needUpdateViews) {
                    m_needUpdateViews = false;
                    // TODO: What do we do upon (permanent) loss of tracking?
                    // Update the foveation mask based on eye gaze.
                    updateGaze();

                    // TODO: for now redraw all the views until we implement better logic
                    updateViews12(get(vrsCommandList));
                }

                // TODO: With DX12, the mask cannot be a texture array. For now we just use the generic mask.

                // Use the special SHADING_RATE_SOURCE resource state for barriers on the VRS surface
                auto idx = static_cast<size_t>(eyeHint.value_or(Eye::Both));
                auto mask = m_shadingRateMask[idx]->getAs<D3D12>();

                m_Dx12ShadingRateResources.RSSetShadingRateImage(get(vrsCommandList), mask, idx);

            } else {
                throw std::runtime_error("Unsupported graphics runtime");
            }

            return true;
        }

        void onUnsetRenderTarget(std::shared_ptr<graphics::IContext> context) override {
            disable(context);
        }

        void updateGazeLocation(XrVector2f gaze, Eye eye) override {
            // works with left, right and both
            if (eye != Eye::Right)
                m_gazeLocation[0] = gaze;
            if (eye != Eye::Left)
                m_gazeLocation[1] = gaze;
            if (eye == Eye::Both)
                m_gazeLocation[2] = gaze;
        }

        void setViewProjectionCenters(XrVector2f left, XrVector2f right) override {
            m_gazeOffset[0] = left;
            m_gazeOffset[1] = right;
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
                            (localAppData / "screenshots" / fmt::format("vrs_{}_mask_left.png", m_captureID)).string());
                        m_shadingRateMask[1]->saveToFile(
                            (localAppData / "screenshots" / fmt::format("vrs_{}_mask_right.png", m_captureID))
                                .string());
                        m_shadingRateMask[2]->saveToFile(
                            (localAppData / "screenshots" / fmt::format("vrs_{}_mask_generic.png", m_captureID))
                                .string());
                        m_shadingRateMaskVPRT->saveToFile(
                            (localAppData / "screenshots" / fmt::format("vrs_{}_mask_vprt.png", m_captureID)).string());
                    }

                    DebugLog("VRS: Capturing file ID: %d\n", m_captureFileIndex);

                    renderTarget->saveToFile(
                        (localAppData / "screenshots" /
                         fmt::format("vrs_{}_{}_{}_pre.png",
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
                         fmt::format("vrs_{}_{}_{}_post.png",
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

            // Initialize VRS shader constants
            m_constants.InvDim = {1.f / texW, 1.f / texH};

            // Initialize shading rate resources
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

            // Initialize compute shader
            {
                const auto shadersDir = dllHome / "shaders";
                const auto shaderFile = shadersDir / "VRS.hlsl";

                utilities::shader::Defines defines;
                defines.add("VRS_TILE_X", m_tileSize);
                defines.add("VRS_TILE_Y", m_tileSize);
                defines.add("VRS_NUM_RATES", 3);

                m_csShading =
                    m_device->createComputeShader(shaderFile, "mainCS", "VRS CS", {texW, texH, 1}, defines.get());

                m_cbShading = m_device->createBuffer(sizeof(ShadingConstants), "VRS CB");
            }

            // Initialize API-specific shading rate resources.
            if (auto device11 = m_device->getAs<D3D11>()) {
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

                m_NvShadingRateResources.initialize();
                resetShadingRates(Api::D3D11);

            } else if (m_device->getAs<D3D12>()) {
                m_Dx12ShadingRateResources.initialize();
                resetShadingRates(Api::D3D12);
            }

            // Setup shader constants
            updateRates(m_mode);
            updateRings(m_mode);

            DebugLog("VRS: renderWidth=%u renderHeight=%u tileSize=%u\n", m_renderWidth, m_renderHeight, m_tileSize);
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

                m_Dx12ShadingRateResources.RSUnsetShadingRateImages(get(vrsCommandList));

            } else {
                throw std::runtime_error("Unsupported graphics runtime");
            }
        }

        bool checkUpdateRates(VariableShadingRateType mode) const {
            if (mode == VariableShadingRateType::Preset) {
                return m_configManager->hasChanged(SettingVRSQuality);
            }
            if (mode == VariableShadingRateType::Custom) {
                return m_configManager->hasChanged(SettingVRSInner) || m_configManager->hasChanged(SettingVRSMiddle) ||
                       m_configManager->hasChanged(SettingVRSOuter) ||
                       m_configManager->hasChanged(SettingVRSPreferHorizontal);
            }
            return false;
        }

        void updateRates(VariableShadingRateType mode) {
            uint32_t rates[3];

            if (mode == VariableShadingRateType::Preset) {
                const auto quality = m_configManager->getEnumValue<VariableShadingRateQuality>(SettingVRSQuality);
                for (int i = 0; i < 3; i++) {
                    rates[i] = i + (quality != VariableShadingRateQuality::Quality ? i : 0);
                }
            } else if (mode == VariableShadingRateType::Custom) {
                rates[0] = m_configManager->getValue(SettingVRSInner);
                rates[1] = m_configManager->getValue(SettingVRSMiddle);
                rates[2] = m_configManager->getValue(SettingVRSOuter);
            }

            const bool preferHorizontal =
                m_mode == VariableShadingRateType::Custom && m_configManager->getValue(SettingVRSPreferHorizontal);

            m_constants.Rates[0] = settingsRateToShadingRate(rates[0], preferHorizontal);
            m_constants.Rates[1] = settingsRateToShadingRate(rates[1], preferHorizontal);
            m_constants.Rates[2] = settingsRateToShadingRate(rates[2], preferHorizontal);
            m_constants.Rates[3] = m_shadingRates[SHADING_RATE_CULL];

            DebugLog("VRS: Rates= %u %u %u %u\n",
                     m_constants.Rates[0],
                     m_constants.Rates[1],
                     m_constants.Rates[2],
                     m_constants.Rates[3]);
        }

        bool checkUpdateRings(VariableShadingRateType mode) const {
            if (mode == VariableShadingRateType::Preset) {
                return m_configManager->hasChanged(SettingVRSPattern);
            }
            if (mode == VariableShadingRateType::Custom) {
                const bool hasInnerRadiusChanged = m_configManager->hasChanged(SettingVRSInnerRadius);
                const bool hasOuterRadiusChanged = m_configManager->hasChanged(SettingVRSOuterRadius);

                if (hasInnerRadiusChanged || hasOuterRadiusChanged) {
                    // Enforce inner and outer radius boundaries.
                    int innerRadius = m_configManager->getValue(SettingVRSInnerRadius);
                    int outerRadius = m_configManager->getValue(SettingVRSOuterRadius);
                    if (innerRadius > outerRadius) {
                        if (hasInnerRadiusChanged) {
                            m_configManager->setValue(SettingVRSOuterRadius, innerRadius);
                        } else if (hasOuterRadiusChanged) {
                            m_configManager->setValue(SettingVRSInnerRadius, outerRadius);
                        }
                    }
                    return true;
                }
                return m_configManager->hasChanged(SettingVRSXScale) ||
                       m_configManager->hasChanged(SettingVRSXOffset) || m_configManager->hasChanged(SettingVRSYOffset);
            }
            return false;
        }

        void updateRings(VariableShadingRateType mode) {
            uint32_t radius[2] = {10000u, 10000u};

            if (mode == VariableShadingRateType::Preset) {
                const auto pattern = m_configManager->getEnumValue<VariableShadingRatePattern>(SettingVRSPattern);
                if (pattern == VariableShadingRatePattern::Wide) {
                    radius[0] = 55, radius[1] = 80;
                } else if (pattern == VariableShadingRatePattern::Narrow) {
                    radius[0] = 30, radius[1] = 55;
                } else { // VariableShadingRatePattern::Balanced
                    radius[0] = 50, radius[1] = 60;
                }
            }
            if (mode == VariableShadingRateType::Custom) {
                radius[0] = m_configManager->getValue(SettingVRSInnerRadius);
                radius[1] = m_configManager->getValue(SettingVRSOuterRadius);
            }

            const auto semiMajorFactor = m_configManager->getValue(SettingVRSXScale);
            m_constants.Rings[0] = MakeRingParam({radius[0] * semiMajorFactor * 0.0001f, radius[0] * 0.01f});
            m_constants.Rings[1] = MakeRingParam({radius[1] * semiMajorFactor * 0.0001f, radius[1] * 0.01f});
            m_constants.Rings[2] = MakeRingParam({100.f, 100.f}); // large enough
            m_constants.Rings[3] = MakeRingParam({100.f, 100.f}); // large enough

            m_gazeOffset[2].x = m_configManager->getValue(SettingVRSXOffset) * 0.005f; // +- 50%
            m_gazeOffset[2].y = m_configManager->getValue(SettingVRSYOffset) * 0.005f; // +- 50%

            // updateFallbackRTV
            m_gazeLocation[2].x = 0;
            m_gazeLocation[2].y = m_gazeOffset[2].y;

            DebugLog("VRS: Rings= %u %u\n", radius[0], radius[1]);
        }

        void updateGaze() {
            // In normalized screen coordinates.
            XrVector2f gaze[ViewCount];
            if (!m_usingEyeTracking) {
                gaze[1] = gaze[0] = m_gazeOffset[2];
                gaze[1].x = -gaze[0].x;

            } else if (!m_eyeTracker || !m_eyeTracker->getProjectedGaze(gaze)) {
                // recenter when loosing eye tracking
                gaze[1] = gaze[0] = {0.f, 0.f};
            }

            m_gazeLocation[0] = gaze[0];
            m_gazeLocation[1] = gaze[1];
        }

        void updateViews11(bool updateSingleRTV = true, bool updateArrayRTV = true, bool updateFallbackRTV = true) {
            for (size_t i = 0; i < std::size(m_shadingRateMask); i++) {
                auto update_dispatch = i != 2 ? (updateSingleRTV || updateArrayRTV) : updateFallbackRTV;
                if (update_dispatch) {
                    m_constants.GazeXY = m_gazeLocation[i];
                    m_cbShading->uploadData(&m_constants, sizeof(m_constants));

                    m_device->setShader(m_csShading);
                    m_device->setShaderInput(0, m_cbShading);
                    m_device->setShaderInput(0, m_shadingRateMask[i]);
                    m_device->setShaderOutput(0, m_shadingRateMask[i]);
                    m_device->dispatchShader();

                    if (i != 2 && updateArrayRTV) {
                        m_device->setShader(m_csShading);
                        m_device->setShaderInput(0, m_cbShading);
                        m_device->setShaderInput(0, m_shadingRateMaskVPRT, static_cast<int>(i));
                        m_device->setShaderOutput(0, m_shadingRateMaskVPRT, static_cast<int>(i));
                        m_device->dispatchShader();
                    }
                }
            }
        }

        void updateViews12(ID3D12GraphicsCommandList5* pCommandList) {
            for (size_t i = 0; i < std::size(m_shadingRateMask); i++) {
                m_constants.GazeXY = m_gazeLocation[i];
                m_cbShading->uploadData(&m_constants, sizeof(m_constants));

                m_Dx12ShadingRateResources.ResourceBarrier(
                    pCommandList, m_shadingRateMask[i]->getAs<D3D12>(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, i);

                m_device->setShader(m_csShading);
                m_device->setShaderInput(0, m_cbShading);
                m_device->setShaderInput(0, m_shadingRateMask[i]);
                m_device->setShaderOutput(0, m_shadingRateMask[i]);
                m_device->dispatchShader();
            }
        }

        uint32_t settingsRateToShadingRate(uint32_t settingsRate, bool preferHorizontal) const {
            static const uint8_t lut[] = {SHADING_RATE_x1,
                                          SHADING_RATE_2x1,
                                          SHADING_RATE_2x2,
                                          SHADING_RATE_4x2,
                                          SHADING_RATE_4x4,
                                          SHADING_RATE_CULL};

            static_assert(SHADING_RATE_1x2 == (SHADING_RATE_2x1 + 1), "preferHorizonal shading rate arithmetic");
            static_assert(SHADING_RATE_2x4 == (SHADING_RATE_4x2 + 1), "preferHorizonal shading rate arithmetic");

            // preferHorizontal applies to odd settingsRate only, this prevents (CULL+1)
            preferHorizontal &= settingsRate < 4;
            auto rate =
                lut[std::min(size_t(settingsRate), std::size(lut))] + (settingsRate & uint32_t(preferHorizontal));
            return m_shadingRates[rate];
        }

        void resetShadingRates(Api api) {
            if (api == Api::D3D11) {
                // Implementation uses a constant table with a varying shading rate texture
                // We set VRS on 2 viewports in case the stereo view renders in parallel.
                m_shadingRates[SHADING_RATE_CULL] = NV_PIXEL_X0_CULL_RASTER_PIXELS;
                m_shadingRates[SHADING_RATE_x16] = NV_PIXEL_X16_PER_RASTER_PIXEL;
                m_shadingRates[SHADING_RATE_x8] = NV_PIXEL_X8_PER_RASTER_PIXEL;
                m_shadingRates[SHADING_RATE_x4] = NV_PIXEL_X4_PER_RASTER_PIXEL;
                m_shadingRates[SHADING_RATE_x2] = NV_PIXEL_X2_PER_RASTER_PIXEL;
                m_shadingRates[SHADING_RATE_x1] = NV_PIXEL_X1_PER_RASTER_PIXEL;
                m_shadingRates[SHADING_RATE_2x1] = NV_PIXEL_X1_PER_2X1_RASTER_PIXELS;
                m_shadingRates[SHADING_RATE_1x2] = NV_PIXEL_X1_PER_1X2_RASTER_PIXELS;
                m_shadingRates[SHADING_RATE_2x2] = NV_PIXEL_X1_PER_2X2_RASTER_PIXELS;
                m_shadingRates[SHADING_RATE_4x2] = NV_PIXEL_X1_PER_4X2_RASTER_PIXELS;
                m_shadingRates[SHADING_RATE_2x4] = NV_PIXEL_X1_PER_2X4_RASTER_PIXELS;
                m_shadingRates[SHADING_RATE_4x4] = NV_PIXEL_X1_PER_4X4_RASTER_PIXELS;

                for (size_t i = 0; i < std::size(m_shadingRates); i++) {
                    m_nvRates[0].shadingRateTable[i] = static_cast<NV_PIXEL_SHADING_RATE>(m_shadingRates[i]);
                }
                m_nvRates[0].enableVariablePixelShadingRate = true;
                m_nvRates[1] = m_nvRates[0];
            }
            if (api == Api::D3D12) {
                // Implementation uses a varying shading rate texture.
                // We use a constant table to lookup the shader constants only
                m_shadingRates[SHADING_RATE_CULL] = D3D12_SHADING_RATE_4X4;
                m_shadingRates[SHADING_RATE_x16] = D3D12_SHADING_RATE_1X1;
                m_shadingRates[SHADING_RATE_x8] = D3D12_SHADING_RATE_1X1;
                m_shadingRates[SHADING_RATE_x4] = D3D12_SHADING_RATE_1X1;
                m_shadingRates[SHADING_RATE_x2] = D3D12_SHADING_RATE_1X1;
                m_shadingRates[SHADING_RATE_x1] = D3D12_SHADING_RATE_1X1;
                m_shadingRates[SHADING_RATE_2x1] = D3D12_SHADING_RATE_2X1;
                m_shadingRates[SHADING_RATE_1x2] = D3D12_SHADING_RATE_1X2;
                m_shadingRates[SHADING_RATE_2x2] = D3D12_SHADING_RATE_2X2;
                m_shadingRates[SHADING_RATE_4x2] = D3D12_SHADING_RATE_4X2;
                m_shadingRates[SHADING_RATE_2x4] = D3D12_SHADING_RATE_2X4;
                m_shadingRates[SHADING_RATE_4x4] = D3D12_SHADING_RATE_4X4;
            }
        }

        bool isVariableRateShadingCandidate(const XrSwapchainCreateInfo& info) const {
            // Check for proportionality with the size of our render target.
            // Also check that the texture is not under 50% of the render scale. We expect that no one should use
            // in-app render scale that is so small.
            DebugLog("VRS: info.width=%u info.height=%u\n", info.width, info.height);
            if (info.width < (m_renderWidth / 2))
                return false;

            const float aspectRatio = (float)info.width / info.height;
            if (std::abs(aspectRatio - m_renderRatio) > 0.01f)
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

        const uint32_t m_renderWidth;
        const uint32_t m_renderHeight;
        const uint32_t m_tileSize;
        const uint32_t m_tileRateMax;
        const float m_renderRatio;
        const float m_displayRatio;

        XrVector2f m_gazeOffset[ViewCount + 1];
        XrVector2f m_gazeLocation[ViewCount + 1];

        VariableShadingRateType m_mode{VariableShadingRateType::None};
        bool m_usingEyeTracking{false};
        bool m_needUpdateViews{false};

        // ShadingRates to Graphics API specific rates LUT.
        uint8_t m_shadingRates[SHADING_RATE_COUNT];
        ShadingConstants m_constants;

        std::shared_ptr<IShaderBuffer> m_cbShading;
        std::shared_ptr<IComputeShader> m_csShading;
        std::shared_ptr<ITexture> m_shadingRateMask[ViewCount + 1];
        std::shared_ptr<ITexture> m_shadingRateMaskVPRT;

        struct {
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

            void initialize() {
                // Make sure to unload NvAPI on destruction
                deferredUnloadNvAPI.needUnload = true;
            }
            ComPtr<ID3D11NvShadingRateResourceView> views[ViewCount + 1];
            ComPtr<ID3D11NvShadingRateResourceView> viewsVPRT;

        } m_NvShadingRateResources;

        struct {
            void initialize() {
                // Make sure we got a valid initial state
                state[2] = state[1] = state[0] = D3D12_RESOURCE_STATE_COMMON;
                bound[2] = bound[1] = bound[0] = false;
                // statesVPRT = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            }

            void ResourceBarrier(ID3D12GraphicsCommandList5* pCommandList,
                                 ID3D12Resource* pResource,
                                 D3D12_RESOURCE_STATES newState,
                                 size_t idx) {
                if (!bound[idx] && state[idx] != newState) {
                    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(pResource, state[idx], newState);
                    pCommandList->ResourceBarrier(1, &barrier);
                    state[idx] = newState;
                }
            }

            void RSSetShadingRateImage(ID3D12GraphicsCommandList5* pCommandList,
                                       ID3D12Resource* pResource,
                                       size_t idx) {
                if (!bound[idx]) {
                    ResourceBarrier(pCommandList, pResource, D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE, idx);

                    // RSSetShadingRate() function sets both the combiners and the per-drawcall shading rate.
                    // We set to 1X1 for all sources and all combiners to MAX, so that the coarsest wins (per-drawcall,
                    // per-primitive, VRS surface).

                    static const D3D12_SHADING_RATE_COMBINER combiners[D3D12_RS_SET_SHADING_RATE_COMBINER_COUNT] = {
                        D3D12_SHADING_RATE_COMBINER_MAX, D3D12_SHADING_RATE_COMBINER_MAX};

                    pCommandList->RSSetShadingRate(D3D12_SHADING_RATE_1X1, combiners);
                    pCommandList->RSSetShadingRateImage(pResource);

                    bound[idx] = true;
                }
            }

            void RSUnsetShadingRateImages(ID3D12GraphicsCommandList5* pCommandList) {
                // To disable VRS, set shading rate to 1X1 with no combiners, and no RSSetShadingRateImage()
                pCommandList->RSSetShadingRate(D3D12_SHADING_RATE_1X1, nullptr);
                pCommandList->RSSetShadingRateImage(nullptr);

                bound[2] = bound[1] = bound[0] = false;
            }

            D3D12_RESOURCE_STATES state[ViewCount + 1];
            // D3D12_RESOURCE_STATES statesVPRT;
            bool bound[ViewCount + 1];

        } m_Dx12ShadingRateResources;

        // We use a constant table and a varying shading rate texture filled with a compute shader.
        inline static NV_D3D11_VIEWPORT_SHADING_RATE_DESC m_nvRates[2] = {};

#ifdef _DEBUG
        bool m_isCapturing{false};
        uint32_t m_captureID{0};
        uint32_t m_captureFileIndex;

        std::shared_ptr<ITexture> m_currentRenderTarget;
        std::optional<Eye> m_currentEyeHint;
#endif
    }; // namespace

} // namespace

#ifdef _DEBUG
#include <dxgi1_3.h> // DXGIGetDebugInterface1
#endif

namespace toolkit::graphics {
    std::shared_ptr<IVariableRateShader> CreateVariableRateShader(std::shared_ptr<IConfigManager> configManager,
                                                                  std::shared_ptr<IDevice> graphicsDevice,
                                                                  std::shared_ptr<input::IEyeTracker> eyeTracker,
                                                                  uint32_t renderWidth,
                                                                  uint32_t renderHeight,
                                                                  uint32_t displayWidth,
                                                                  uint32_t displayHeight) {
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
#ifdef _DEBUG
                // For now, disable variable rate shading when running under PIX.
                // This is only needed until PIX supports VRS.
                IID graphicsAnalysisID;
                if (SUCCEEDED(IIDFromString(L"{9F251514-9D4D-4902-9D60-18988AB7D4B5}", &graphicsAnalysisID))) {
                    ComPtr<IUnknown> graphicsAnalysis;
                    if (SUCCEEDED(DXGIGetDebugInterface1(0, graphicsAnalysisID, &graphicsAnalysis))) {
                        Log("VRS (DX12 Tier2) is not supported undex PIX\n");
                        throw FeatureNotSupported();
                    }
                }
#endif
                tileSize = options.ShadingRateImageTileSize;
                tileRateMax = integer_log2(tileSize);

            } else {
                throw std::runtime_error("Unsupported graphics runtime");
            }

            return std::make_shared<VariableRateShader>(configManager,
                                                        graphicsDevice,
                                                        eyeTracker,
                                                        renderWidth,
                                                        renderHeight,
                                                        displayWidth,
                                                        displayHeight,
                                                        tileSize,
                                                        tileRateMax);

        } catch (FeatureNotSupported&) {
            return nullptr;
        }
    }

} // namespace toolkit::graphics
