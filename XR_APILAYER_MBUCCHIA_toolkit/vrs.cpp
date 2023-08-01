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

    using namespace xr::math;

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
        std::shared_ptr<ITexture> maskDoubleWide;
        std::shared_ptr<ITexture> maskTextureArray;
    };

    inline XrVector2f MakeRingParam(XrVector2f size) {
        size.x = std::max(size.x, FLT_EPSILON);
        size.y = std::max(size.y, FLT_EPSILON);
        return {1.f / (size.x * size.x), 1.f / (size.y * size.y)};
    }

    class VariableRateShader : public IVariableRateShader {
      public:
        VariableRateShader(OpenXrApi& openXR,
                           std::shared_ptr<IConfigManager> configManager,
                           std::shared_ptr<IDevice> graphicsDevice,
                           std::shared_ptr<input::IEyeTracker> eyeTracker,
                           uint32_t renderWidth,
                           uint32_t renderHeight,
                           uint32_t displayWidth,
                           uint32_t displayHeight,
                           uint32_t tileSize,
                           uint32_t tileRateMax,
                           bool hasVisibilityMask)
            : m_openXR(openXR), m_configManager(configManager), m_device(graphicsDevice), m_eyeTracker(eyeTracker),
              m_renderWidth(renderWidth), m_renderHeight(renderHeight),
              m_renderRatio(float(renderWidth) / renderHeight), m_tileSize(tileSize), m_tileRateMax(tileRateMax),
              m_actualRenderWidth(renderWidth), m_hasVisibilityMask(hasVisibilityMask) {
            createRenderResources(m_renderWidth, m_renderHeight);

            // Set initial projection center
            std::fill_n(m_gazeOffset, std::size(m_gazeOffset), XrVector2f{0.f, 0.f});
            updateGazeLocation({0.f, 0.f}, Eye::Both);

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
            m_NvShadingRateResources.viewsDoubleWide[index].Detach();
            m_NvShadingRateResources.viewsTextureArray[index].Detach();
        }

        void beginSession(XrSession session) override {
            // Create HAM buffers.
            if (m_hasVisibilityMask) {
                for (uint32_t i = 0; i < ViewCount; i++) {
                    XrVisibilityMaskKHR mask{XR_TYPE_VISIBILITY_MASK_KHR};
                    if (XR_FAILED(m_openXR.xrGetVisibilityMaskKHR(session,
                                                                  XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                                  i,
                                                                  XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR,
                                                                  &mask))) {
                        break;
                    }

                    if (!mask.indexCountOutput) {
                        break;
                    }

                    std::vector<XrVector2f> rawVertices(mask.vertexCountOutput);
                    std::vector<uint32_t> rawIndices(mask.indexCountOutput);

                    mask.indexCapacityInput = (uint32_t)rawIndices.size();
                    mask.indices = rawIndices.data();
                    mask.vertexCapacityInput = (uint32_t)rawVertices.size();
                    mask.vertices = rawVertices.data();
                    CHECK_XRCMD(m_openXR.xrGetVisibilityMaskKHR(session,
                                                                XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                                i,
                                                                XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR,
                                                                &mask));

                    std::vector<SimpleMeshVertex> vertices(mask.vertexCountOutput);
                    for (uint32_t j = 0; j < vertices.size(); j++) {
                        vertices[j].Position = {rawVertices[j].x, rawVertices[j].y, -1.0f};
                        vertices[j].Color = {(float)m_shadingRates[SHADING_RATE_CULL],
                                             (float)m_shadingRates[SHADING_RATE_CULL],
                                             (float)m_shadingRates[SHADING_RATE_CULL]};
                    }

                    std::vector<uint16_t> indices(mask.indexCountOutput);
                    for (uint32_t j = 0; j < indices.size(); j++) {
                        indices[j] = rawIndices[j];
                    }

                    m_HAM[i] = m_device->createSimpleMesh(vertices, indices, "VRS HAM");
                }
            }

            m_session = session;
        }

        void endSession() override {
            for (uint32_t i = 0; i < ViewCount; i++) {
                m_HAM[i].reset();
            }

            m_isHAMReady = false;
        }

        void beginFrame(XrTime frameTime) override {
            if (m_HAM[0] && m_HAM[1] && !m_isHAMReady) {
                // Create projection for stamping HAM.
                XrViewLocateInfo info{XR_TYPE_VIEW_LOCATE_INFO};
                info.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                info.displayTime = frameTime;

                XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr};
                referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
                referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity();
                CHECK_XRCMD(m_openXR.xrCreateReferenceSpace(m_session, &referenceSpaceCreateInfo, &info.space));

                XrViewState state{XR_TYPE_VIEW_STATE, nullptr};
                XrView eyeInViewSpace[2] = {{XR_TYPE_VIEW, nullptr}, {XR_TYPE_VIEW, nullptr}};
                uint32_t viewCountOutput;
                CHECK_XRCMD(m_openXR.xrLocateViews(m_session, &info, &state, 2, &viewCountOutput, eyeInViewSpace));
                for (uint32_t i = 0; i < ViewCount; i++) {
                    m_viewProjection[i].Pose = Pose::Identity();
                    m_viewProjection[i].Fov = eyeInViewSpace[i].fov;
                    m_viewProjection[i].NearFar = {0.001f, 100.f};
                }

                CHECK_XRCMD(m_openXR.xrDestroySpace(info.space));

                m_isHAMReady = true;

                m_currentGen++;
            }

            // When using eye tracking we must render the views every frame.
            if (m_usingEyeTracking) {
                // TODO: What do we do upon (permanent) loss of tracking?
                updateGaze();

                m_currentGen++;
            }

            m_device->blockCallbacks();
            m_device->saveContext();

            {
                std::unique_lock lock(m_shadingRateMaskLock);

                // Update all masks.
                size_t index = 0;
                for (auto it = m_shadingRateMask.begin(); it != m_shadingRateMask.end();) {
                    // Age all masks.
                    if (++it->age > MaxAge) {
                        // Evict old entries. If a mask is used in a frame, its age is to 0.
                        TraceLocalActivity(local);
                        TraceLoggingWriteStart(local,
                                               "VariableRateShading_DestroyMask",
                                               TLArg(it->widthInTiles, "WidthInTiles"),
                                               TLArg(it->heightInTiles, "HeightInTiles"),
                                               TLArg("DiedOfAge", "State"));

                        it = m_shadingRateMask.erase(it);

                        if (m_NvShadingRateResources.views.size()) {
                            // TODO: Leak NVAPI resources for now, since there is an occasional crash.
                            LeakNVAPIResource(index);
                            m_NvShadingRateResources.views.erase(m_NvShadingRateResources.views.begin() + index);
                            m_NvShadingRateResources.viewsDoubleWide.erase(
                                m_NvShadingRateResources.viewsDoubleWide.begin() + index);
                            m_NvShadingRateResources.viewsTextureArray.erase(
                                m_NvShadingRateResources.viewsTextureArray.begin() + index);
                        }

                        TraceLoggingWriteStop(local, "VariableRateShading_DestroyMask");
                    } else {
                        // If this mask is still valid...

                        // ...and is pending creation, create it.
                        if (!it->mask[0]) {
                            createMaskResources(*it);
                        }

                        // ...and eventually, update it.
                        updateViews(*it);

                        it++;
                        index++;
                    }
                }
            }

            m_device->restoreContext();
            m_device->flushContext(false, false);
            m_device->unblockCallbacks();

            m_renderScales.clear();
        }

        void endFrame() override {
            {
                // Find the most likely actual render resolution.
                std::unique_lock lock(m_renderScalesLock);

                uint32_t maxCount = 0;
                uint32_t renderWidth = 0;

                for (auto it : m_renderScales) {
                    if (it.second >= maxCount && it.first > renderWidth) {
                        renderWidth = it.first;
                        maxCount = it.second;
                    }
                }

                m_actualRenderWidth = renderWidth;
            }

            disable();
        }

        void update() override {
            const auto mode = m_configManager->getEnumValue<VariableShadingRateType>(config::SettingVRS);
            const auto hasModeChanged = mode != m_mode;

            if (hasModeChanged)
                m_mode = mode;

            if (mode != VariableShadingRateType::None) {
                const bool usingEyeTracking = m_eyeTracker && m_configManager->getValue(SettingEyeTrackingEnabled);

                const auto hasPatternChanged =
                    m_usingEyeTracking != usingEyeTracking || hasModeChanged || checkUpdateRings(mode);
                const auto hasQualityChanged = hasModeChanged || checkUpdateRates(mode);

                m_usingEyeTracking = usingEyeTracking;

                if (hasPatternChanged) {
                    updateRings(mode);
                    updateGaze();
                }

                if (hasQualityChanged)
                    updateRates(mode);

                // Only update the texture when necessary.
                const bool isHAMEnabled = !m_configManager->peekValue(SettingDisableHAM);
                if (hasQualityChanged || hasPatternChanged || isHAMEnabled != m_isHAMEnabled ||
                    m_configManager->hasChanged(SettingVRSCullHAM)) {
                    m_currentGen++;
                }

                // We can't use config's hasChanged since we don't own this setting.
                m_isHAMEnabled = isHAMEnabled;

            } else if (m_usingEyeTracking) {
                m_usingEyeTracking = false;
            }

            m_filterScale = m_configManager->getValue(SettingVRSScaleFilter) / 100.f;
        }

        bool onSetRenderTarget(std::shared_ptr<graphics::IContext> context,
                               std::shared_ptr<ITexture> renderTarget,
                               std::optional<Eye> eyeHint) override {
            const auto& info = renderTarget->getInfo();

            bool isDoubleWide = false;
            if (m_mode == VariableShadingRateType::None || !isVariableRateShadingCandidate(info, isDoubleWide)) {
                disable(context);
                return false;
            }

            const Eye eye = eyeHint.value_or(Eye::Both);
            TraceLoggingWrite(g_traceProvider, "EnableVariableRateShading", TLArg(isDoubleWide, "IsDoubleWide"));

            size_t maskIndex;
            {
                std::unique_lock lock(m_shadingRateMaskLock);

                if (!getMaskIndex(isDoubleWide ? info.width / 2 : info.width, info.height, maskIndex)) {
                    // Creation was deferred to the next frame.
                    TraceLoggingWrite(
                        g_traceProvider, "SkipEnableVariableRateShading", TLArg("DeferredCreation", "Reason"));
                    return true;
                }

                // Reset the age to keep this mask active.
                m_shadingRateMask[maskIndex].age = 0;
            }

            if (auto context11 = context->getAs<D3D11>()) {
                if (m_currentState.isActive && m_currentState.width == info.width &&
                    m_currentState.height == info.height && m_currentState.eye == eye) {
                    TraceLoggingWrite(g_traceProvider, "SkipEnableVariableRateShading", TLArg("AlreadySet", "Reason"));
                    return true;
                }

                // We set VRS on all viewports in case multiple views render in parallel.
                NV_D3D11_VIEWPORTS_SHADING_RATE_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.version = NV_D3D11_VIEWPORTS_SHADING_RATE_DESC_VER;
                desc.numViewports = (uint32_t)std::size(m_nvRates);
                desc.pViewports = m_nvRates;
                CHECK_NVCMD(NvAPI_D3D11_RSSetViewportsPixelShadingRates(context11, &desc));

                auto& mask = isDoubleWide          ? m_NvShadingRateResources.viewsDoubleWide[maskIndex]
                             : info.arraySize == 2 ? m_NvShadingRateResources.viewsTextureArray[maskIndex]
                                                   : m_NvShadingRateResources.views[maskIndex][(size_t)eye];

                CHECK_NVCMD(NvAPI_D3D11_RSSetShadingRateResourceView(context11, get(mask)));

                doCapture(/* post */);
                doCapture(renderTarget, eyeHint);

                m_currentState.width = info.width;
                m_currentState.height = info.height;
                m_currentState.eye = eye;
                m_currentState.isActive = true;
            } else if (auto context12 = context->getAs<D3D12>()) {
                ComPtr<ID3D12GraphicsCommandList5> vrsCommandList;
                if (FAILED(context12->QueryInterface(set(vrsCommandList)))) {
                    DebugLog("VRS: failed to query ID3D12GraphicsCommandList5\n");
                    return false;
                }

                // TODO: With DX12, the mask cannot be a texture array. For now we just use the generic mask.

                auto mask = isDoubleWide ? m_shadingRateMask[maskIndex].maskDoubleWide
                                         : m_shadingRateMask[maskIndex].mask[(size_t)eye];

                // RSSetShadingRate() function sets both the combiners and the per-drawcall shading rate.
                // We set to 1X1 for all sources and all combiners to MAX, so that the coarsest wins (per-drawcall,
                // per-primitive, VRS surface).
                static const D3D12_SHADING_RATE_COMBINER combiners[D3D12_RS_SET_SHADING_RATE_COMBINER_COUNT] = {
                    D3D12_SHADING_RATE_COMBINER_MAX, D3D12_SHADING_RATE_COMBINER_MAX};
                vrsCommandList->RSSetShadingRate(D3D12_SHADING_RATE_1X1, combiners);
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

        uint32_t getActualRenderWidth() const override {
            return m_actualRenderWidth;
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

        void doCapture(std::shared_ptr<ITexture> renderTarget = nullptr, std::optional<Eye> eyeHint = std::nullopt) {
            if (m_isCapturing) {
                if (renderTarget) {
                    const auto& info = renderTarget->getInfo();

                    TraceLoggingWrite(g_traceProvider,
                                      "VariableRateShadingCapture",
                                      TLArg(m_captureID, "CaptureID"),
                                      TLArg(m_captureFileIndex, "CaptureFileIndex"));

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
                if (!m_currentState.isActive) {
                    TraceLoggingWrite(g_traceProvider, "SkipDisableVariableRateShading");
                    return;
                }

                auto context11 = context ? context->getAs<D3D11>() : m_device->getContextAs<D3D11>();

                NV_D3D11_VIEWPORTS_SHADING_RATE_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.version = NV_D3D11_VIEWPORTS_SHADING_RATE_DESC_VER;
                CHECK_NVCMD(NvAPI_D3D11_RSSetViewportsPixelShadingRates(context11, &desc));
                CHECK_NVCMD(NvAPI_D3D11_RSSetShadingRateResourceView(context11, nullptr));

                doCapture(/* post */);

                m_currentState.isActive = false;
            } else if (m_device->getApi() == Api::D3D12) {
                auto context12 = context ? context->getAs<D3D12>() : m_device->getContextAs<D3D12>();

                ComPtr<ID3D12GraphicsCommandList5> vrsCommandList;
                if (FAILED(context12->QueryInterface(set(vrsCommandList)))) {
                    DebugLog("VRS: failed to query ID3D12GraphicsCommandList5\n");
                    return;
                }

                vrsCommandList->RSSetShadingRate(D3D12_SHADING_RATE_1X1, nullptr);
                vrsCommandList->RSSetShadingRateImage(nullptr);
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
                    m_Rates[i][3] = m_shadingRates[SHADING_RATE_CULL];
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
                    m_Rates[eye][3] = m_shadingRates[SHADING_RATE_CULL];
                }
            }

            TraceLoggingWrite(g_traceProvider,
                              "VariableRateShading_Rates",
                              TLArg(m_Rates[2][0], "Rate1"),
                              TLArg(m_Rates[2][1], "Rate2"),
                              TLArg(m_Rates[2][2], "Rate3"),
                              TLArg(m_Rates[2][3], "Rate4"));
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
                        // We (re)write both values to ensure they are committed at the same time (immediately).
                        if (hasInnerRadiusChanged) {
                            m_configManager->setValue(SettingVRSOuterRadius, innerRadius, true);
                            m_configManager->setValue(SettingVRSInnerRadius, innerRadius, true);
                        } else if (hasOuterRadiusChanged) {
                            m_configManager->setValue(SettingVRSInnerRadius, outerRadius, true);
                            m_configManager->setValue(SettingVRSOuterRadius, outerRadius, true);
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
            XrVector2f gaze[ViewCount];
            // We've determined experimentally that +4% offset gives best results.
            float xOffset = m_gazeOffset[2].x + 0.04f;
            if (!m_usingEyeTracking || !m_eyeTracker || !m_eyeTracker->getProjectedGaze(gaze)) {
                gaze[0] = m_gazeOffset[0];
                gaze[1] = m_gazeOffset[1];
            }
            // location = view center + view offset (L/R)
            m_gazeLocation[0] = gaze[0] + XrVector2f{xOffset, m_gazeOffset[2].y};
            m_gazeLocation[1] = gaze[1] + XrVector2f{-xOffset, m_gazeOffset[2].y};

            // The generic mask only supports vertical offsets.
            m_gazeLocation[2].x = 0;
            m_gazeLocation[2].y = m_gazeOffset[2].y;
        }

        bool getMaskIndex(uint32_t width, uint32_t height, size_t& index) {
            const auto texW = xr::math::DivideRoundingUp(width, m_tileSize);
            const auto texH = xr::math::DivideRoundingUp(height, m_tileSize);

            // Look-up existing resources.
            for (size_t i = 0; i < m_shadingRateMask.size(); i++) {
                if (m_shadingRateMask[i].widthInTiles == texW && m_shadingRateMask[i].heightInTiles == texH) {
                    index = i;

                    // Do not return invalid masks deferred to the next frame.
                    return !!m_shadingRateMask[i].mask[0];
                }
            }

            TraceLoggingWrite(g_traceProvider,
                              "VariableRateShading_CreateMask",
                              TLArg(width, "Width"),
                              TLArg(height, "Height"),
                              TLArg(texW, "WidthInTiles"),
                              TLArg(texH, "HeightInTiles"),
                              TLArg("Deferred", "State"));

            ShadingRateMask newMask;
            newMask.widthInTiles = texW;
            newMask.heightInTiles = texH;
            newMask.age = 0;
            newMask.gen = 0;

            // Defer creation to the next beginFrame() event.
            m_shadingRateMask.push_back(newMask);

            return false;
        }

        void createMaskResources(ShadingRateMask& mask) {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local,
                                   "VariableRateShading_CreateMask",
                                   TLArg(mask.widthInTiles, "WidthInTiles"),
                                   TLArg(mask.heightInTiles, "HeightInTiles"),
                                   TLArg("Current", "State"));

            // Initialize shading rate resources
            XrSwapchainCreateInfo info;
            ZeroMemory(&info, sizeof(info));
            info.width = mask.widthInTiles;
            info.height = mask.heightInTiles;
            info.format = DXGI_FORMAT_R8_UINT;
            info.arraySize = 1;
            info.mipCount = 1;
            info.sampleCount = 1;
            info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
                              XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;

            for (auto& it : mask.mask) {
                it = m_device->createTexture(info, "VRS TEX2D");
            }
            info.width *= 2;
            mask.maskDoubleWide = m_device->createTexture(info, "VRS DoubleWide TEX2D");
            info.width = mask.widthInTiles;
            info.arraySize = 2;
            mask.maskTextureArray = m_device->createTexture(info, "VRS TextureArray TEX2D");

            for (auto& it : mask.cbShading) {
                it = m_device->createBuffer(sizeof(ShadingConstants), "VRS CB");
            }

            if (auto device11 = m_device->getAs<D3D11>()) {
                NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.version = NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC_VER;
                desc.Format = DXGI_FORMAT_R8_UINT;
                desc.ViewDimension = NV_SRRV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MipSlice = 0;

                const auto newIndex = m_NvShadingRateResources.views.size();
                m_NvShadingRateResources.views.push_back({});
                m_NvShadingRateResources.viewsDoubleWide.push_back({});
                m_NvShadingRateResources.viewsTextureArray.push_back({});

                for (size_t i = 0; i < std::size(mask.mask); i++) {
                    CHECK_NVCMD(
                        NvAPI_D3D11_CreateShadingRateResourceView(device11,
                                                                  mask.mask[i]->getAs<D3D11>(),
                                                                  &desc,
                                                                  set(m_NvShadingRateResources.views[newIndex][i])));
                }

                CHECK_NVCMD(
                    NvAPI_D3D11_CreateShadingRateResourceView(device11,
                                                              mask.maskDoubleWide->getAs<D3D11>(),
                                                              &desc,
                                                              set(m_NvShadingRateResources.viewsDoubleWide[newIndex])));

                desc.ViewDimension = NV_SRRV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.ArraySize = 2;
                CHECK_NVCMD(NvAPI_D3D11_CreateShadingRateResourceView(
                    device11,
                    mask.maskTextureArray->getAs<D3D11>(),
                    &desc,
                    set(m_NvShadingRateResources.viewsTextureArray[newIndex])));
            }

            TraceLoggingWriteStop(local, "VariableRateShading_CreateMask");
        }

        void updateViews(ShadingRateMask& mask) {
            // Check if this mask needs to be updated.
            if (mask.gen == m_currentGen) {
                return;
            }
            mask.gen = m_currentGen;

            TraceLocalActivity(local);
            TraceLoggingWriteStart(local,
                                   "VariableRateShading_UpdateMask",
                                   TLArg(mask.widthInTiles, "WidthInTiles"),
                                   TLArg(mask.heightInTiles, "HeightInTiles"));

            for (size_t i = 0; i < std::size(mask.mask); i++) {
                m_device->setRenderTargets(1, &mask.mask[i]);
                m_device->clearColor(
                    0.f, 0.f, (float)mask.heightInTiles, (float)mask.widthInTiles, {255.f, 255.f, 255.f, 255.f});

                // Initialize mask with HAM culling if needed.
                if (i < ViewCount && m_isHAMReady && !m_configManager->peekValue(SettingDisableHAM) &&
                    m_configManager->getValue(SettingVRSCullHAM)) {
                    m_device->setViewProjection(m_viewProjection[i]);
                    m_device->draw(m_HAM[i], Pose::Identity(), {1.f, 1.f, 1.f}, true);
                }
            }

            // Draw the rings into the mask.
            const auto dispatchX = xr::math::DivideRoundingUp(mask.widthInTiles, 8);
            const auto dispatchY = xr::math::DivideRoundingUp(mask.heightInTiles, 8);
            for (size_t i = 0; i < std::size(mask.mask); i++) {
                const auto constants = makeShadingConstants(i, mask.widthInTiles, mask.heightInTiles);
                mask.cbShading[i]->uploadData(&constants, sizeof(constants));

                m_csShading->updateThreadGroups({dispatchX, dispatchY, 1});
                m_device->setShader(m_csShading, SamplerType::NearestClamp);
                m_device->setShaderInput(0, mask.cbShading[i]);
                mask.mask[i]->setState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                m_device->setShaderOutput(0, mask.mask[i]);
                m_device->dispatchShader();
                mask.mask[i]->setState(D3D12_RESOURCE_STATE_COPY_SOURCE);
            }

            // Copy to the double wide/texture arrays mask.
            mask.mask[0]->copyTo(mask.maskDoubleWide, 0, 0, 0);
            mask.mask[1]->copyTo(mask.maskDoubleWide, mask.widthInTiles, 0, 0);
            mask.mask[0]->copyTo(mask.maskTextureArray, 0, 0, 0);
            mask.mask[1]->copyTo(mask.maskTextureArray, 0, 0, 1);

            for (size_t i = 0; i < std::size(mask.mask); i++) {
                mask.mask[i]->setState(D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE);
            }

            TraceLoggingWriteStop(local, "VariableRateShading_UpdateMask");
        }

        ShadingConstants makeShadingConstants(size_t eye, uint32_t texW, uint32_t texH) {
            ShadingConstants constants;
            constants.GazeXY = m_gazeLocation[eye];
            constants.InvDim = {1.f / texW, 1.f / texH};
            for (size_t i = 0; i < std::size(m_Rings); i++) {
                constants.Rings[i] = m_Rings[i];
                constants.Rates[i] = m_Rates[eye][i];
            }
            return constants;
        }

        uint8_t settingsRateToShadingRate(size_t settingsRate, int rateBias = 0, bool preferHorizontal = false) const {
            static const uint8_t lut[] = {
                SHADING_RATE_x1, SHADING_RATE_2x1, SHADING_RATE_2x2, SHADING_RATE_4x2, SHADING_RATE_4x4};

            static_assert(SHADING_RATE_1x2 == (SHADING_RATE_2x1 + 1), "preferHorizonal arithmetic");
            static_assert(SHADING_RATE_2x4 == (SHADING_RATE_4x2 + 1), "preferHorizonal arithmetic");

            if (settingsRate < std::size(lut)) {
                auto rate = lut[std::min(settingsRate + abs(rateBias), std::size(lut) - 1)];
                if (preferHorizontal) {
                    rate += (rate == SHADING_RATE_2x1 || rate == SHADING_RATE_4x2);
                }
                return m_shadingRates[rate];
            }
            return m_shadingRates[SHADING_RATE_CULL];
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
                for (size_t i = 1; i < std::size(m_nvRates); i++) {
                    m_nvRates[i] = m_nvRates[0];
                }
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

        bool isVariableRateShadingCandidate(const XrSwapchainCreateInfo& info, bool& isDoubleWide) {
            TraceLoggingWrite(g_traceProvider,
                              "IsVariableRateShadingCandidate",
                              TLArg(info.width, "Width"),
                              TLArg(info.height, "Height"),
                              TLArg(info.arraySize, "ArraySize"),
                              TLArg(info.format, "Format"));

            const float aspectRatio = (float)info.width / info.height;
            const float aspectRatioWidthDiv2 = (float)(info.width / 2) / info.height;
            isDoubleWide = std::abs(aspectRatioWidthDiv2 - m_renderRatio) <= 0.01f;

            const auto trackRenderScale = [&](const uint32_t width) {
                // Ignore things that are definitely too low. We use DLSS's Ultra Performance (33%) as our lower bound.
                if (width < 0.32f * m_renderWidth) {
                    return;
                }

                std::unique_lock lock(m_renderScalesLock);

                auto it = m_renderScales.find(width);
                if (it == m_renderScales.end()) {
                    m_renderScales.insert_or_assign(width, 1u);
                } else {
                    it->second++;
                }
            };

            if (!isDoubleWide) {
                if (std::abs(aspectRatio - m_renderRatio) > 0.01f) {
                    return false;
                }

                trackRenderScale(info.width);

                // Check for proportionality with the size of our render target.
                if (info.width < (m_actualRenderWidth * m_filterScale)) {
                    return false;
                }

                if (info.arraySize > 2) {
                    return false;
                }
            } else {
                trackRenderScale(info.width / 2);

                // Check for proportionality with the size of our render target.
                if (info.width / 2 < (m_actualRenderWidth * m_filterScale)) {
                    return false;
                }
            }

            return true;
        }

        OpenXrApi& m_openXR;
        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_device;
        const std::shared_ptr<input::IEyeTracker> m_eyeTracker;

        const uint32_t m_renderWidth;
        const uint32_t m_renderHeight;
        const uint32_t m_tileSize;
        const uint32_t m_tileRateMax;
        const float m_renderRatio;

        XrSession m_session{XR_NULL_HANDLE};
        bool m_usingEyeTracking{false};

        std::map<uint32_t, uint32_t> m_renderScales;
        uint32_t m_actualRenderWidth;
        std::mutex m_renderScalesLock;
        float m_filterScale{0.51f};

        bool m_hasVisibilityMask{false};

        // This is valid for D3D11 only (single-threaded).
        struct {
            bool isActive{false};
            uint32_t width;
            uint32_t height;
            Eye eye;
        } m_currentState;

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
        std::mutex m_shadingRateMaskLock;

        bool m_isHAMEnabled{false};
        bool m_isHAMReady{false};
        ViewProjection m_viewProjection[ViewCount];
        std::shared_ptr<ISimpleMesh> m_HAM[ViewCount];

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
            std::vector<ComPtr<ID3D11NvShadingRateResourceView>> viewsDoubleWide;
            std::vector<ComPtr<ID3D11NvShadingRateResourceView>> viewsTextureArray;

        } m_NvShadingRateResources;

        // We use a constant table and a varying shading rate texture filled with a compute shader.
        inline static NV_D3D11_VIEWPORT_SHADING_RATE_DESC
            m_nvRates[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};

        bool m_isCapturing{false};
        uint32_t m_captureID{0};
        uint32_t m_captureFileIndex;

        std::shared_ptr<ITexture> m_currentRenderTarget;
        std::optional<Eye> m_currentEyeHint;
    }; // namespace

} // namespace

#ifdef _DEBUG
#include <dxgi1_3.h> // DXGIGetDebugInterface1
#endif

namespace toolkit::graphics {
    std::shared_ptr<IVariableRateShader> CreateVariableRateShader(OpenXrApi& openXR,
                                                                  std::shared_ptr<IConfigManager> configManager,
                                                                  std::shared_ptr<IDevice> graphicsDevice,
                                                                  std::shared_ptr<input::IEyeTracker> eyeTracker,
                                                                  uint32_t renderWidth,
                                                                  uint32_t renderHeight,
                                                                  uint32_t displayWidth,
                                                                  uint32_t displayHeight,
                                                                  bool hasVisibilityMask) {
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

            return std::make_shared<VariableRateShader>(openXR,
                                                        configManager,
                                                        graphicsDevice,
                                                        eyeTracker,
                                                        renderWidth,
                                                        renderHeight,
                                                        displayWidth,
                                                        displayHeight,
                                                        tileSize,
                                                        tileRateMax,
                                                        hasVisibilityMask);

        } catch (FeatureNotSupported&) {
            return nullptr;
        }
    }

} // namespace toolkit::graphics
