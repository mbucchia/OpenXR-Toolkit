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

    // The number of frames before freeing an unused set of VRS mask textures.
    constexpr uint16_t MaxAge = 100;

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

    struct ShadingRateMask {
        uint32_t widthInTiles;
        uint32_t heightInTiles;

        // The generation of this mask. If the generation is too old, the mask must be updated.
        uint64_t gen;

        // The number of frames since the mask was last used during a rendering pass.
        uint16_t age;

        std::shared_ptr<IShaderBuffer> cbShading[ViewCount + 1];
        std::shared_ptr<ITexture> mask[ViewCount + 1];
        std::shared_ptr<ITexture> maskVPRT;
    };

    inline XrVector2f MakeRingParam(XrVector2f size) {
        size.x = std::max(size.x, FLT_EPSILON);
        size.y = std::max(size.y, FLT_EPSILON);
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
                           uint32_t tileRateMax,
                           bool isPimaxFovHackSupported)
            : m_configManager(configManager), m_device(graphicsDevice), m_eyeTracker(eyeTracker),
              m_renderWidth(renderWidth), m_renderHeight(renderHeight),
              m_renderRatio(float(renderWidth) / renderHeight), m_tileSize(tileSize), m_tileRateMax(tileRateMax),
              m_supportFOVHack(isPimaxFovHackSupported) {
            createRenderResources(m_renderWidth, m_renderHeight);

            // Set initial projection center
            std::fill_n(m_gazeOffset, std::size(m_gazeOffset), XrVector2f{0.f, 0.f});
            std::fill_n(m_gazeLocation, std::size(m_gazeLocation), XrVector2f{0.f, 0.f});

            // Request update.
            m_currentGen++;
        }

        ~VariableRateShader() override {
            disable();

            // TODO: Leak NVAPI resources for now, since there is an occasional crash.
            for (size_t i = 0; i < m_NvShadingRateResources.views.size(); i++) {
                LeakNVAPIResource(i);
            }
        }

        void LeakNVAPIResource(size_t index) {
            for (auto& view : m_NvShadingRateResources.views[index]) {
                view.Detach();
            }
            m_NvShadingRateResources.viewsVPRT[index].Detach();
        }

        void beginFrame(XrTime frameTime) override {
            // When using eye tracking we must render the views every frame.
            if (m_usingEyeTracking) {
                // TODO: What do we do upon (permanent) loss of tracking?
                updateGaze();

                m_currentGen++;
            }

            // Age all masks. If they are used in this frame, their age will be reset to 0.
            size_t index = 0;
            for (auto it = m_shadingRateMask.begin(); it != m_shadingRateMask.end();) {
                // Evict old entries.
                if (++it->age > MaxAge) {
                    it = m_shadingRateMask.erase(it);

                    if (m_NvShadingRateResources.views.size()) {
                        // TODO: Leak NVAPI resources for now, since there is an occasional crash.
                        LeakNVAPIResource(index);
                        m_NvShadingRateResources.views.erase(m_NvShadingRateResources.views.begin() + index);
                        m_NvShadingRateResources.viewsVPRT.erase(m_NvShadingRateResources.viewsVPRT.begin() + index);
                    }
                } else {
                    it++;
                    index++;
                }
            }
        }

        void endFrame() override {
            disable();
        }

        void update() override {
            const auto mode = m_configManager->getEnumValue<VariableShadingRateType>(config::SettingVRS);
            const auto hasModeChanged = mode != m_mode;

            if (hasModeChanged)
                m_mode = mode;

            if (mode != VariableShadingRateType::None) {
                m_usingEyeTracking = m_eyeTracker && m_configManager->getValue(SettingEyeTrackingEnabled);

                const auto hasPatternChanged = hasModeChanged || checkUpdateRings(mode);
                const auto hasQualityChanged = hasModeChanged || checkUpdateRates(mode);

                if (hasPatternChanged) {
                    updateRings(mode);
                    updateGaze();
                }

                if (hasQualityChanged)
                    updateRates(mode);

                // Only update the texture when necessary.
                if (hasQualityChanged || hasPatternChanged) {
                    m_currentGen++;
                }

            } else if (m_usingEyeTracking) {
                m_usingEyeTracking = false;
            }
        }

        bool onSetRenderTarget(std::shared_ptr<graphics::IContext> context,
                               std::shared_ptr<ITexture> renderTarget,
                               Eye eyeHint) override {
            const auto& info = renderTarget->getInfo();

            if (m_mode == VariableShadingRateType::None || !isVariableRateShadingCandidate(info)) {
                disable(context);
                return false;
            }

            TraceLoggingWrite(g_traceProvider, "EnableVariableRateShading", TLArg(to_integral(eyeHint), "Eye"));

            if (auto context11 = context->getAs<D3D11>()) {
                // TODO: for now redraw all the views until we implement better logic
                // const auto updateSingleRTV = eyeHint != Eye::Both && info.arraySize == 1;
                // const auto updateArrayRTV = info.arraySize > 1;
                // updateViews(updateSingleRTV, updateArrayRTV, false);
                size_t maskIndex;
                updateViews(context11, getOrCreateMaskResources(info.width, info.height, &maskIndex));

                // We set VRS on 2 viewports in case the stereo view renders in parallel.
                NV_D3D11_VIEWPORTS_SHADING_RATE_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.version = NV_D3D11_VIEWPORTS_SHADING_RATE_DESC_VER;
                desc.numViewports = 2;
                desc.pViewports = m_nvRates;
                CHECK_NVCMD(NvAPI_D3D11_RSSetViewportsPixelShadingRates(context11, &desc));

                auto& mask = info.arraySize == 2 ? m_NvShadingRateResources.viewsVPRT[maskIndex]
                                                 : m_NvShadingRateResources.views[maskIndex][to_integral(eyeHint)];

                CHECK_NVCMD(NvAPI_D3D11_RSSetShadingRateResourceView(context11, get(mask)));

                doCapture(context /* post */);
                doCapture(context, renderTarget, eyeHint);

            } else if (auto context12 = context->getAs<D3D12>()) {
                ComPtr<ID3D12GraphicsCommandList5> vrsCommandList;
                if (FAILED(context12->QueryInterface(set(vrsCommandList)))) {
                    DebugLog("VRS: failed to query ID3D12GraphicsCommandList5\n");
                    return false;
                }

                // TODO: for now redraw all the views until we implement better logic
                auto& maskForSize = getOrCreateMaskResources(info.width, info.height);
                updateViews(get(vrsCommandList), maskForSize);

                // TODO: With DX12, the mask cannot be a texture array. For now we just use the generic mask.

                // Use the special SHADING_RATE_SOURCE resource state for barriers on the VRS surface
                auto mask = maskForSize.mask[to_integral(eyeHint)]->getAs<D3D12>();
                m_Dx12ShadingRateResources.RSSetShadingRateImage(get(vrsCommandList), mask, to_integral(eyeHint));

            } else {
                throw std::runtime_error("Unsupported graphics runtime");
            }

            return true;
        }

        void onUnsetRenderTarget(std::shared_ptr<graphics::IContext> context) override {
            disable(context);
        }

        void setViewProjectionCenters(XrVector2f left, XrVector2f right) override {
            m_gazeOffset[0] = left;
            m_gazeOffset[1] = right;
        }

        uint8_t getMaxRate() const override {
            return static_cast<uint8_t>(m_tileRateMax);
        }

        uint64_t getCurrentGen() const override {
            return m_mode != VariableShadingRateType::None ? m_currentGen : 0;
        }

        void getShaderState(VariableRateShaderState& state, utilities::Eye eye) const override {
            static_assert(ARRAYSIZE(VariableRateShaderState::gazeXY) == ARRAYSIZE(m_gazeLocation));
            static_assert(ARRAYSIZE(VariableRateShaderState::rings) == ARRAYSIZE(m_Rings));
            static_assert(ARRAYSIZE(VariableRateShaderState::rates) == ARRAYSIZE(m_Rates[0]));

            std::copy_n(m_gazeLocation, std::size(m_gazeLocation), state.gazeXY);
            std::copy_n(m_Rings, std::size(m_Rings), state.rings);

            for (size_t i = 0; i < std::size(m_Rates); i++) {
                state.rates[i] = shadingRateToSettingsRate(m_Rates[to_integral(eye)][i]);
            }

            state.mode = m_usingEyeTracking ? 2 : m_mode != VariableShadingRateType::None;
        }

        void startCapture() override {
            DebugLog("VRS: Start capture\n");
            TraceLoggingWrite(g_traceProvider, "StartVariableRateShadingCapture");
            m_captureID++;
            m_captureFileIndex = 0;
            m_isCapturing = true;
        }

        void stopCapture() override {
            if (m_isCapturing) {
                DebugLog("VRS: Stop capture\n");
                TraceLoggingWrite(g_traceProvider, "StopVariableRateShadingCapture");
                m_isCapturing = false;
            }
        }

        void doCapture(std::shared_ptr<graphics::IContext> context,
                       std::shared_ptr<ITexture> renderTarget = nullptr,
                       Eye eyeHint = Eye::Both) {
            if (!m_isCapturing)
                return;

            const auto is_post = !renderTarget;
            if (!is_post) {
                TraceLoggingWrite(g_traceProvider,
                                  "VariableRateShadingCapture",
                                  TLArg(m_captureID, "CaptureID"),
                                  TLArg(m_captureFileIndex, "CaptureFileIndex"));

                m_captureRT = std::move(renderTarget);
                m_captureEye = eyeHint;
            }

            if (m_captureRT) {
                constexpr const char* eyeHintLabels[] = {"left", "rite", "gene"};
                const auto& info = m_captureRT->getInfo();
                m_captureRT->saveToFile(
                    (localAppData / "screenshots" /
                     fmt::format("vrs_{}_{}_{}_{}.dds",
                                 m_captureID,
                                 m_captureFileIndex,
                                 is_post ? "pst" : "pre",
                                 info.arraySize == 2 ? "dual" : eyeHintLabels[to_integral(m_captureEye)]))
                        .string());
            }

            if (is_post) {
                m_captureRT = nullptr;
            }
        }

      private:
        void createRenderResources(uint32_t renderWidth, uint32_t renderHeigh) {
            // Initialize compute shader
            {
                const auto shadersDir = dllHome / "shaders";
                const auto shaderFile = shadersDir / "VRS.hlsl";

                utilities::shader::Defines defines;
                defines.add("VRS_TILE_X", m_tileSize);
                defines.add("VRS_TILE_Y", m_tileSize);
                defines.add("VRS_NUM_RATES", 3);

                // Dispatch 64 threads per group.
                defines.add("VRS_NUM_THREADS_X", 8);
                defines.add("VRS_NUM_THREADS_Y", 8);

                m_csShading = m_device->createComputeShader(shaderFile, "mainCS", "VRS CS", {1, 1, 1}, defines.get());
            }

            // Initialize API-specific shading rate resources.
            if (auto device11 = m_device->getAs<D3D11>()) {
                m_NvShadingRateResources.initialize();
                resetShadingRates(Api::D3D11);

            } else if (m_device->getAs<D3D12>()) {
                m_Dx12ShadingRateResources.initialize();
                resetShadingRates(Api::D3D12);
            }

            // Setup shader constants
            updateRates(m_mode);
            updateRings(m_mode);
            updateGaze();
        }

        void disable(std::shared_ptr<graphics::IContext> context = nullptr) {
            TraceLoggingWrite(g_traceProvider, "DisableVariableRateShading");
            if (m_device->getApi() == Api::D3D11) {
                auto context11 = context ? context->getAs<D3D11>() : m_device->getContextAs<D3D11>();

                NV_D3D11_VIEWPORTS_SHADING_RATE_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.version = NV_D3D11_VIEWPORTS_SHADING_RATE_DESC_VER;
                CHECK_NVCMD(NvAPI_D3D11_RSSetViewportsPixelShadingRates(context11, &desc));
                CHECK_NVCMD(NvAPI_D3D11_RSSetShadingRateResourceView(context11, nullptr));

                doCapture(context /* post */);
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
                       m_configManager->hasChanged(SettingVRSPreferHorizontal) ||
                       m_configManager->hasChanged(SettingVRSLeftRightBias);
            }
            return false;
        }

        void updateRates(VariableShadingRateType mode) {
            if (mode == VariableShadingRateType::Preset) {
                const auto quality = m_configManager->getEnumValue<VariableShadingRateQuality>(SettingVRSQuality);
                for (size_t i = 0; i < 3; i++) {
                    const auto rate = i + (quality != VariableShadingRateQuality::Quality ? i : 0);
                    m_Rates[2][i] = m_Rates[1][i] = m_Rates[0][i] = settingsRateToShadingRate(rate);
                    m_Rates[i][3] = SHADING_RATE_CULL;
                }

            } else if (mode == VariableShadingRateType::Custom) {
                const auto leftRightBias = m_configManager->getValue(SettingVRSLeftRightBias);
                const auto preferHorizontal = m_configManager->getValue(SettingVRSPreferHorizontal) != 0;

                const int rates[3] = {m_configManager->getValue(SettingVRSInner),
                                      m_configManager->getValue(SettingVRSMiddle),
                                      m_configManager->getValue(SettingVRSOuter)};

                const int rateBias[3] = {std::min(leftRightBias, 0), std::max(leftRightBias, 0), 0};

                for (size_t eye = 0; eye < 3; eye++) {
                    m_Rates[eye][0] = settingsRateToShadingRate(rates[0], rateBias[eye], preferHorizontal);
                    m_Rates[eye][1] = settingsRateToShadingRate(rates[1], rateBias[eye], preferHorizontal);
                    m_Rates[eye][2] = settingsRateToShadingRate(rates[2], rateBias[eye], preferHorizontal);
                    m_Rates[eye][3] = SHADING_RATE_CULL;
                }
            }

            TraceLoggingWrite(g_traceProvider,
                              "VariableRateShading_Rates",
                              TLArg(m_shadingRates[m_Rates[2][0]], "Rate1"),
                              TLArg(m_shadingRates[m_Rates[2][1]], "Rate2"),
                              TLArg(m_shadingRates[m_Rates[2][2]], "Rate3"),
                              TLArg(m_shadingRates[m_Rates[2][3]], "Rate4"));
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
                       m_configManager->hasChanged(SettingVRSXOffset) ||
                       m_configManager->hasChanged(SettingVRSYOffset) ||
                       (m_supportFOVHack && m_configManager->hasChanged(config::SettingPimaxFOVHack));
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

            // When doing the Pimax FOV hack, we swap left and right eyes.
            m_swapViews = m_supportFOVHack && m_configManager->getValue(config::SettingPimaxFOVHack);

            const auto semiMajorFactor = m_configManager->getValue(SettingVRSXScale);
            m_Rings[0] = MakeRingParam({radius[0] * semiMajorFactor * 0.0001f, radius[0] * 0.01f});
            m_Rings[1] = MakeRingParam({radius[1] * semiMajorFactor * 0.0001f, radius[1] * 0.01f});
            m_Rings[2] = MakeRingParam({100.f, 100.f}); // large enough
            m_Rings[3] = MakeRingParam({100.f, 100.f}); // large enough

            m_gazeOffset[2].x = m_configManager->getValue(SettingVRSXOffset) * 0.01f;
            m_gazeOffset[2].y = m_configManager->getValue(SettingVRSYOffset) * 0.01f;

            TraceLoggingWrite(
                g_traceProvider, "VariableRateShading_Rings", TLArg(radius[0], "Ring1"), TLArg(radius[1], "Ring2"));
        }

        void updateGaze() {
            using namespace xr::math;
            XrVector2f gaze[ViewCount];
            if (!m_usingEyeTracking || !m_eyeTracker || !m_eyeTracker->getProjectedGaze(gaze)) {
                gaze[0] = m_gazeOffset[0];
                gaze[1] = m_gazeOffset[1];
            }
            // location = view center + view offset (L/R)
            m_gazeLocation[0] = gaze[0] + m_gazeOffset[2];
            m_gazeLocation[1] = gaze[1] + XrVector2f{-m_gazeOffset[2].x, m_gazeOffset[2].y};

            // The generic mask only supports vertical offsets.
            m_gazeLocation[2].x = 0;
            m_gazeLocation[2].y = m_gazeOffset[2].y;
        }

        ShadingRateMask& getOrCreateMaskResources(uint32_t width, uint32_t height, size_t* index = nullptr) {
            const auto texW = xr::math::DivideRoundingUp(width, m_tileSize);
            const auto texH = xr::math::DivideRoundingUp(height, m_tileSize);

            // Look-up existing resources.
            for (size_t i = 0; i < m_shadingRateMask.size(); i++) {
                if (m_shadingRateMask[i].widthInTiles == texW && m_shadingRateMask[i].heightInTiles == texH) {
                    if (index) {
                        *index = i;
                    }
                    return m_shadingRateMask[i];
                }
            }

            TraceLoggingWrite(g_traceProvider, "VariableRateShading_Mask", TLArg(width), TLArg(height));

            ShadingRateMask newMask;
            newMask.widthInTiles = texW;
            newMask.heightInTiles = texH;
            newMask.age = 0;
            newMask.gen = 0;

            // Initialize shading rate resources
            XrSwapchainCreateInfo info;
            ZeroMemory(&info, sizeof(info));
            info.width = texW;
            info.height = texH;
            info.format = DXGI_FORMAT_R8_UINT;
            info.arraySize = 1;
            info.mipCount = 1;
            info.sampleCount = 1;
            info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;

            for (auto& it : newMask.mask) {
                it = m_device->createTexture(info, "VRS TEX2D");
            }
            info.arraySize = 2;
            newMask.maskVPRT = m_device->createTexture(info, "VRS VPRT TEX2D");

            for (auto& it : newMask.cbShading) {
                it = m_device->createBuffer(sizeof(ShadingConstants), "VRS CB");
            }

            m_shadingRateMask.push_back(newMask);

            if (auto device11 = m_device->getAs<D3D11>()) {
                NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.version = NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC_VER;
                desc.Format = DXGI_FORMAT_R8_UINT;
                desc.ViewDimension = NV_SRRV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MipSlice = 0;

                const auto newIndex = m_NvShadingRateResources.views.size();
                m_NvShadingRateResources.views.push_back({});
                m_NvShadingRateResources.viewsVPRT.push_back({});

                for (size_t i = 0; i < std::size(newMask.mask); i++) {
                    CHECK_NVCMD(
                        NvAPI_D3D11_CreateShadingRateResourceView(device11,
                                                                  newMask.mask[i]->getAs<D3D11>(),
                                                                  &desc,
                                                                  set(m_NvShadingRateResources.views[newIndex][i])));
                }

                desc.ViewDimension = NV_SRRV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.ArraySize = 2;
                CHECK_NVCMD(
                    NvAPI_D3D11_CreateShadingRateResourceView(device11,
                                                              newMask.maskVPRT->getAs<D3D11>(),
                                                              &desc,
                                                              set(m_NvShadingRateResources.viewsVPRT[newIndex])));
            }

            if (index) {
                *index = m_shadingRateMask.size() - 1;
            }
            return m_shadingRateMask.back();
        }

        void updateViews(D3D11::Context pContext, ShadingRateMask& mask) {
            // Reset the age to keep this mask active.
            mask.age = 0;

            // Check if this mask needs to be updated.
            if (mask.gen == m_currentGen) {
                return;
            }
            mask.gen = m_currentGen;

            const auto dispatchX = xr::math::DivideRoundingUp(mask.widthInTiles, 8);
            const auto dispatchY = xr::math::DivideRoundingUp(mask.heightInTiles, 8);

            for (size_t i = 0; i < std::size(mask.mask); i++) {
                const auto constants = makeShadingConstants(i, mask.widthInTiles, mask.heightInTiles);
                mask.cbShading[i]->uploadData(&constants, sizeof(constants));

                m_csShading->updateThreadGroups({dispatchX, dispatchY, 1});
                m_device->setShader(m_csShading, SamplerType::NearestClamp);
                m_device->setShaderInput(0, mask.cbShading[i]);
                m_device->setShaderInput(0, mask.mask[i]);
                m_device->setShaderOutput(0, mask.mask[i]);
                m_device->dispatchShader();
            }

            if (pContext) {
                auto pDstResource = mask.maskVPRT->getAs<D3D11>();
                pContext->CopySubresourceRegion(pDstResource, 0, 0, 0, 0, mask.mask[0]->getAs<D3D11>(), 0, nullptr);
                pContext->CopySubresourceRegion(pDstResource, 1, 0, 0, 0, mask.mask[1]->getAs<D3D11>(), 0, nullptr);
            }
        }

        void updateViews(ID3D12GraphicsCommandList5* pCommandList, ShadingRateMask& mask) {
            // Reset the age to keep this mask active.
            mask.age = 0;

            // Check if this mask needs to be updated.
            if (mask.gen == m_currentGen) {
                return;
            }
            mask.gen = m_currentGen;

            const auto dispatchX = xr::math::DivideRoundingUp(mask.widthInTiles, 8);
            const auto dispatchY = xr::math::DivideRoundingUp(mask.heightInTiles, 8);

            for (size_t i = 0; i < std::size(mask.mask); i++) {
                const auto constants = makeShadingConstants(i, mask.widthInTiles, mask.heightInTiles);
                mask.cbShading[i]->uploadData(&constants, sizeof(constants));

                m_Dx12ShadingRateResources.ResourceBarrier(
                    pCommandList, mask.mask[i]->getAs<D3D12>(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, i);

                m_csShading->updateThreadGroups({dispatchX, dispatchY, 1});
                m_device->setShader(m_csShading, SamplerType::NearestClamp);
                m_device->setShaderInput(0, mask.cbShading[i]);
                m_device->setShaderInput(0, mask.mask[i]);
                m_device->setShaderOutput(0, mask.mask[i]);
                m_device->dispatchShader();
            }
        }

        ShadingConstants makeShadingConstants(size_t eye, uint32_t texW, uint32_t texH) {
            ShadingConstants constants;
            constants.GazeXY = m_gazeLocation[eye ^ (eye != 2 && m_swapViews)];
            constants.InvDim = {1.f / texW, 1.f / texH};
            for (size_t i = 0; i < std::size(m_Rings); i++) {
                constants.Rings[i] = m_Rings[i];
                constants.Rates[i] = m_shadingRates[m_Rates[eye][i]];
            }
            return constants;
        }

        uint8_t settingsRateToShadingRate(size_t settingsRate, int rateBias = 0, bool preferHorizontal = false) const {
            static const uint8_t lut[to_integral(VariableShadingRateVal::MaxValue) - 1] = {
                SHADING_RATE_x1, SHADING_RATE_2x1, SHADING_RATE_2x2, SHADING_RATE_4x2, SHADING_RATE_4x4};

            static_assert(SHADING_RATE_1x2 == (SHADING_RATE_2x1 + 1), "preferHorizonal arithmetic");
            static_assert(SHADING_RATE_2x4 == (SHADING_RATE_4x2 + 1), "preferHorizonal arithmetic");

            if (settingsRate < std::size(lut)) {
                auto rate = lut[std::min(settingsRate + abs(rateBias), std::size(lut) - 1)];
                if (preferHorizontal) {
                    rate += (rate == SHADING_RATE_2x1 || rate == SHADING_RATE_4x2);
                }
                return rate;
            }
            return SHADING_RATE_CULL;
        }

        uint8_t shadingRateToSettingsRate(uint8_t shadingRate) const {
            // VariableShadingRateVal::R_x1 to VariableShadingRateVal::R_Cull
            static const uint8_t lut[ARRAYSIZE(m_shadingRates)] = {5, 0, 0, 0, 0, 0, 1, 1, 2, 3, 3, 4};
            return lut[shadingRate < std::size(lut) ? shadingRate : 0];
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
            TraceLoggingWrite(g_traceProvider,
                              "IsVariableRateShadingCandidate",
                              TLArg(info.width, "Width"),
                              TLArg(info.height, "Height"),
                              TLArg(info.arraySize, "ArraySize"),
                              TLArg(info.format, "Format"));

            // Check for proportionality with the size of our render target.
            // Also check that the texture is not under 50% of the render scale. We expect that no one should use
            // in-app render scale that is so small.
            if (info.width < (m_renderWidth * 0.51f))
                return false;

            const float aspectRatio = (float)info.width / info.height;
            if (std::abs(aspectRatio - m_renderRatio) > 0.01f)
                return false;

            if (info.arraySize > 2)
                return false;

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

        const bool m_supportFOVHack;
        bool m_usingEyeTracking{false};
        bool m_swapViews{false};
        bool m_isCapturing{false};

        // The current "generation" of the mask parameters.
        uint64_t m_currentGen{0};

        VariableShadingRateType m_mode{VariableShadingRateType::None};

        // ShadingConstants
        XrVector2f m_gazeOffset[ViewCount + 1];
        XrVector2f m_gazeLocation[ViewCount + 1];
        XrVector2f m_Rings[4];
        uint8_t m_Rates[ViewCount + 1][4];

        // ShadingRates to Graphics API specific rates LUT.
        uint8_t m_shadingRates[SHADING_RATE_COUNT];

        std::shared_ptr<IComputeShader> m_csShading;
        std::vector<ShadingRateMask> m_shadingRateMask;

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
            std::vector<std::array<ComPtr<ID3D11NvShadingRateResourceView>, ViewCount + 1>> views;
            std::vector<ComPtr<ID3D11NvShadingRateResourceView>> viewsVPRT;

        } m_NvShadingRateResources;

        struct {
            void initialize() {
                // Make sure we got a valid initial state
                state[2] = state[1] = state[0] = D3D12_RESOURCE_STATE_COMMON;
                boundState = 0;
            }

            void ResourceBarrier(ID3D12GraphicsCommandList5* pCommandList,
                                 ID3D12Resource* pResource,
                                 D3D12_RESOURCE_STATES newState,
                                 size_t idx) {
                if (state[idx] != newState) {
                    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(pResource, state[idx], newState);
                    pCommandList->ResourceBarrier(1, &barrier);
                    state[idx] = newState;
                }
            }

            void RSSetShadingRateImage(ID3D12GraphicsCommandList5* pCommandList,
                                       ID3D12Resource* pResource,
                                       size_t idx) {
                const uint32_t boundMask = 1u << idx;
                if (!(boundState & boundMask)) {
                    ResourceBarrier(pCommandList, pResource, D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE, idx);

                    // RSSetShadingRate() function sets both the combiners and the per-drawcall shading rate.
                    // We set to 1X1 for all sources and all combiners to MAX, so that the coarsest wins
                    // (per-drawcall, per-primitive, VRS surface).

                    static const D3D12_SHADING_RATE_COMBINER combiners[D3D12_RS_SET_SHADING_RATE_COMBINER_COUNT] = {
                        D3D12_SHADING_RATE_COMBINER_MAX, D3D12_SHADING_RATE_COMBINER_MAX};

                    pCommandList->RSSetShadingRate(D3D12_SHADING_RATE_1X1, combiners);
                    pCommandList->RSSetShadingRateImage(pResource);

                    boundState = boundMask;
                }
            }

            void RSUnsetShadingRateImages(ID3D12GraphicsCommandList5* pCommandList) {
                // To disable VRS, set shading rate to 1X1 with no combiners, and no RSSetShadingRateImage()
                pCommandList->RSSetShadingRate(D3D12_SHADING_RATE_1X1, nullptr);
                pCommandList->RSSetShadingRateImage(nullptr);
                boundState = 0;
            }

            D3D12_RESOURCE_STATES state[ViewCount + 1];
            uint32_t boundState;

        } m_Dx12ShadingRateResources;

        // We use a constant table and a varying shading rate texture filled with a compute shader.
        inline static NV_D3D11_VIEWPORT_SHADING_RATE_DESC m_nvRates[2] = {};

        std::shared_ptr<ITexture> m_captureRT;
        uint32_t m_captureID{0};
        uint32_t m_captureFileIndex;
        Eye m_captureEye{Eye::Both};
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
                                                                  uint32_t displayHeight,
                                                                  bool isPimaxFovHackSupported) {
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
                                                        tileRateMax,
                                                        isPimaxFovHackSupported);

        } catch (FeatureNotSupported&) {
            return nullptr;
        }
    }

} // namespace toolkit::graphics
