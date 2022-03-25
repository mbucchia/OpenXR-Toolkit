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
                           uint32_t targetWidth,
                           uint32_t targetHeight,
                           uint32_t tileSize,
                           uint32_t tileRateMax)
            : m_configManager(configManager), m_device(graphicsDevice), m_eyeTracker(eyeTracker),
              m_targetWidth(targetWidth), m_targetHeight(targetHeight), m_tileSize(tileSize),
              m_tileRateMax(tileRateMax), m_targetAspectRatio((float)m_targetWidth / m_targetHeight) {
            // Make sure to unload NvAPI on destruction
            m_NvShadingRateResources.deferredUnloadNvAPI.needUnload = m_device->getApi() == Api::D3D11;

            createRenderResources(m_targetWidth, m_targetHeight);

            // Set initial projection center
            updateGazeLocation({0.f, 0.f}, Eye::Both);
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

                updateShadingRings();

                // TODO: do we need to call this one here always?

                updateRenderResources();

                // updateFoveationMasks(0 /*m_projCenterX*/, 0 /*m_projCenterY*/, innerRadius, outerRadius);

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
                XrVector2f gaze[ViewCount];
                if (m_eyeTracker->getProjectedGaze(gaze)) {
                    const auto updateSingleRTV = eyeHint.has_value() && info.arraySize == 1;
                    const auto updateArrayRTV = info.arraySize > 1;

                    updateGazeLocation(gaze[0], Eye::Left);
                    updateGazeLocation(gaze[1], Eye::Right);
                    updateRenderResources(updateSingleRTV, updateArrayRTV, false);

                    m_needUpdateEye = false;

                    // TODO: What do we do upon (permanent) loss of tracking?
                }
            }

            DebugLog("VRS: Enable\n");
            if (auto context11 = context->getAs<D3D11>()) {
                // We set VRS on 2 viewports in case the stereo view renders in parallel.
                NV_D3D11_VIEWPORT_SHADING_RATE_DESC viewports[2];
                viewports[1] = viewports[0] = m_NvShadingRateResources.rates;

                NV_D3D11_VIEWPORTS_SHADING_RATE_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.version = NV_D3D11_VIEWPORTS_SHADING_RATE_DESC_VER;
                desc.numViewports = 2;
                desc.pViewports = viewports;
                CHECK_NVCMD(NvAPI_D3D11_RSSetViewportsPixelShadingRates(context11, &desc));

                auto& mask = info.arraySize == 2
                                 ? m_NvShadingRateResources.viewsVPRT
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

        void updateGazeLocation(XrVector2f gaze, Eye eye) override {
            // works with left, right and both
            if (eye != Eye::Right)
                m_gazeLocation[0] = gaze;
            if (eye != Eye::Left)
                m_gazeLocation[1] = gaze;
            m_gazeLocation[2] = {0, 0}; // updateFallbackRTV
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

                m_csShadingRate =
                    m_device->createComputeShader(shaderFile, "mainCS", "VRS CS", {texW, texH, 1}, defines.get());

                m_cbShadingRate = m_device->createBuffer(sizeof(ShadingConstants), "VRS CB");
            }

            // Initialize API-specific shading rate resources.
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

            // Initialize VRS shader constants
            m_constants.InvDim = {1.f / texW, 1.f / texH};

            updateShadingRings();
            updateShadingRates();
            updateRenderResources();

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
            uint32_t rates[3];

            if (m_mode == VariableShadingRateType::Preset) {
                const auto quality = m_configManager->getEnumValue<VariableShadingRateQuality>(SettingVRSQuality);
                for (int i = 0; i < 3; i++) {
                    rates[i] = i + (quality != VariableShadingRateQuality::Quality ? i : 0);
                }
            } else {
                rates[0] = m_configManager->getValue(SettingVRSInner);
                rates[1] = m_configManager->getValue(SettingVRSMiddle);
                rates[2] = m_configManager->getValue(SettingVRSOuter);
            }

            const bool preferHorizontal =
                m_mode == VariableShadingRateType::Custom && m_configManager->getValue(SettingVRSPreferHorizontal);

            // TODO: D3D12 shading rates and lut
            m_constants.Rates[0] = settingsRateToShadingRate(rates[0], preferHorizontal);
            m_constants.Rates[1] = settingsRateToShadingRate(rates[1], preferHorizontal);
            m_constants.Rates[2] = settingsRateToShadingRate(rates[2], preferHorizontal);
            m_constants.Rates[3] = SHADING_RATE_CULL;

            DebugLog("VRS: Rates = %u %u %u %u\n",
                     m_constants.Rates[0],
                     m_constants.Rates[1],
                     m_constants.Rates[2],
                     m_constants.Rates[3]);
        }

        void updateShadingRings() {
            uint32_t radius[2];

            if (m_mode == VariableShadingRateType::Preset) {
                const auto pattern = m_configManager->getEnumValue<VariableShadingRatePattern>(SettingVRSPattern);
                if (pattern == VariableShadingRatePattern::Wide) {
                    radius[0] = 55, radius[1] = 80;
                } else if (pattern == VariableShadingRatePattern::Narrow) {
                    radius[0] = 30, radius[1] = 55;
                } else { // VariableShadingRatePattern::Balanced
                    radius[0] = 50, radius[1] = 60;
                }
            } else {
                radius[0] = m_configManager->getValue(SettingVRSInnerRadius);
                radius[1] = m_configManager->getValue(SettingVRSOuterRadius);
            }

            // TODO:
            // const float semiMajorFactor = m_configManager->getValue(SettingVRSXScale) * 0.01f;

            m_constants.Rings[0] = MakeRingParam({radius[0] * 0.01f * m_displayRatio, radius[0] * 0.01f});
            m_constants.Rings[1] = MakeRingParam({radius[1] * 0.01f * m_displayRatio, radius[1] * 0.01f});
            m_constants.Rings[2] = MakeRingParam({10.f * m_displayRatio, 10.f});
            m_constants.Rings[3] = MakeRingParam({10.f * m_displayRatio, 10.f});

            DebugLog("VRS: Rings = %u %u\n", radius[0], radius[1]);
        }

        void updateRenderResources(bool updateSingleRTV = true,
                                   bool updateArrayRTV = true,
                                   bool updateFallbackRTV = true) {
            // TODO:
            // const auto xOffset =
            //    static_cast<int>(m_configManager->getValue(SettingVRSXOffset) * (m_targetWidth / 200.f));
            // const auto yOffset =
            //    static_cast<int>(m_configManager->getValue(SettingVRSYOffset) * (m_targetHeight / 200.f));
            // const float semiMajorFactor = m_configManager->getValue(SettingVRSXScale) / 100.f;

            for (size_t i = 0; i < std::size(m_shadingRateMask); i++) {
                auto update_dispatch = i != 2 ? (updateSingleRTV || updateArrayRTV) : updateFallbackRTV;
                if (update_dispatch) {
                    m_constants.GazeXY = m_gazeLocation[i];
                    m_cbShadingRate->uploadData(&m_constants, sizeof(m_constants));

                    m_device->setShader(m_csShadingRate);
                    m_device->setShaderInput(0, m_cbShadingRate);
                    m_device->setShaderInput(0, m_shadingRateMask[i]);
                    m_device->setShaderOutput(0, m_shadingRateMask[i]);
                    m_device->dispatchShader();

                    if (i != 2 && updateArrayRTV) {
                        m_device->setShader(m_csShadingRate);
                        m_device->setShaderInput(0, m_cbShadingRate);
                        m_device->setShaderInput(0, m_shadingRateMaskVPRT, static_cast<int>(i));
                        m_device->setShaderOutput(0, m_shadingRateMaskVPRT, static_cast<int>(i));
                        m_device->dispatchShader();
                    }
                }
            }

// TODO:
#if 0
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
#endif
        }

#if 0
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
#endif

        uint32_t settingsRateToShadingRate(uint32_t settingsRate, bool preferHorizontal) const {
            static const uint8_t lut[] = {SHADING_RATE_x1,
                                          SHADING_RATE_2x1,
                                          SHADING_RATE_2x2,
                                          SHADING_RATE_4x2,
                                          SHADING_RATE_4x4,
                                          SHADING_RATE_CULL};

            static_assert(SHADING_RATE_1x2 == (SHADING_RATE_2x1 + 1), "preferHorizonal shading rate arithmetic");
            static_assert(SHADING_RATE_2x4 == (SHADING_RATE_4x2 + 1), "preferHorizonal shading rate arithmetic");

            // preferHorizontal applies to odd settingsRate only!
            return lut[std::min(size_t(settingsRate), std::size(lut))] + (settingsRate & uint32_t(preferHorizontal));
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

            void resetRates() {
                rates.enableVariablePixelShadingRate = true;
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

            ComPtr<ID3D11NvShadingRateResourceView> views[ViewCount + 1];
            ComPtr<ID3D11NvShadingRateResourceView> viewsVPRT;

            NV_D3D11_VIEWPORT_SHADING_RATE_DESC rates;

        } m_NvShadingRateResources;

        std::shared_ptr<IShaderBuffer> m_cbShadingRate;
        std::shared_ptr<IComputeShader> m_csShadingRate;

        std::shared_ptr<ITexture> m_shadingRateMask[ViewCount + 1];
        std::shared_ptr<ITexture> m_shadingRateMaskVPRT;

        VariableShadingRateType m_mode{VariableShadingRateType::None};

        XrVector2f m_gazeLocation[ViewCount + 1];
        ShadingConstants m_constants;

        bool m_hasProjCenterChanged{false};
        bool m_needUpdateEye{false};

        // bool m_isEnabled{true};
        // int m_innerValue{0};
        // int m_middleValue{0};
        // int m_outerValue{0};

#ifdef _DEBUG
        bool m_isCapturing{false};
        uint32_t m_captureID{0};
        uint32_t m_captureFileIndex;

        std::shared_ptr<ITexture> m_currentRenderTarget;
        std::optional<Eye> m_currentEyeHint;
#endif
#if 0
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
#endif
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
