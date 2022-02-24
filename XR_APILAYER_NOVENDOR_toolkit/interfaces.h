// MIT License
//
// Copyright(c) 2021-2022 Matthieu Bucchianeri
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

#pragma once

namespace toolkit {

    struct FeatureNotSupported : public std::exception {
        const char* what() const throw() {
            return "Feature is not supported";
        }
    };

    namespace {

        // A generic timer.
        struct ITimer {
            virtual ~ITimer() = default;

            virtual void start() = 0;
            virtual void stop() = 0;

            virtual uint64_t query(bool reset = true) const = 0;
        };

    } // namespace

    namespace utilities {

        // 2 views to process, one per eye.
        constexpr uint32_t ViewCount = 2;
        enum class Eye : uint32_t { Left, Right };

        // A CPU synchronous timer.
        struct ICpuTimer : public ITimer {};

    } // namespace utilities

    namespace config {

        const std::string SettingScreenshotEnabled = "enable_screenshot";
        const std::string SettingMenuEyeOffset = "menu_eye_offset";
        const std::string SettingOverlayType = "overlay";
        const std::string SettingMenuFontSize = "font_size";
        const std::string SettingMenuTimeout = "menu_timeout";
        const std::string SettingMenuExpert = "expert_menu";
        const std::string SettingScalingType = "scaling_type";
        const std::string SettingScaling = "scaling";
        const std::string SettingAnamorphic = "anamorphic";
        const std::string SettingSharpness = "sharpness";
        const std::string SettingICD = "world_scale";
        const std::string SettingFOV = "fov";
        const std::string SettingHandTrackingEnabled = "enable_hand_tracking";
        const std::string SettingHandVisibilityAndSkinTone = "hand_visibility";
        const std::string SettingHandTimeout = "hand_timeout";
        const std::string SettingPredictionDampen = "prediction_dampen";
        const std::string SettingBypassMsftHandInteractionCheck = "allow_msft_hand_interaction";
        const std::string SettingMotionReprojectionRate = "motion_reprojection_rate";
        const std::string SettingVRS = "vrs";
        const std::string SettingVRSQuality = "vrs_quality";
        const std::string SettingVRSPattern = "vrs_pattern";
        const std::string SettingVRSOuter = "vrs_outer";
        const std::string SettingVRSOuterRadius = "vrs_outer_radius";
        const std::string SettingVRSMiddle = "vrs_middle";
        const std::string SettingVRSInnerRadius = "vrs_inner_radius";
        const std::string SettingVRSInner = "vrs_inner";
        const std::string SettingVRSXOffset = "vrs_x_offset";
        const std::string SettingVRSYOffset = "vrs_y_offset";
        const std::string SettingMipMapBias = "mipmap_bias";
        const std::string SettingBrightness = "brightness";
        const std::string SettingContrast = "contrast";
        const std::string SettingSaturation = "saturation";

        enum class OverlayType { None = 0, FPS, Advanced, MaxValue };
        enum class MenuFontSize { Small = 0, Medium, Large, MaxValue };
        enum class MenuTimeout { Small = 0, Medium, Large, MaxValue };
        enum class ScalingType { None = 0, NIS, FSR, MaxValue };
        enum class HandTrackingEnabled { Off = 0, Both, Left, Right, MaxValue };
        enum class HandTrackingVisibility { Hidden = 0, Bright, Medium, Dark, Darker, MaxValue };
        enum class MotionReprojectionRate { Off = 1, R_45Hz, R_30Hz, R_22Hz, MaxValue };
        enum class VariableShadingRateType { None = 0, Preset, Custom, MaxValue };
        enum class VariableShadingRateQuality { Performance = 0, Quality, MaxValue };
        enum class VariableShadingRatePattern { Wide = 0, Balanced, Narrow, MaxValue };
        enum class MipMapBias { Off = 0, Anisotropic, All, MaxValue };

        struct IConfigManager {
            virtual ~IConfigManager() = default;

            // Tick to indicate that the game loop ran successfully. This is used for deferred write to the config
            // database.
            virtual void tick() = 0;

            virtual void setDefault(const std::string& name, int value) = 0;

            virtual int getValue(const std::string& name) const = 0;
            virtual int peekValue(const std::string& name) const = 0;
            virtual void setValue(const std::string& name, int value, bool noCommitDelay = false) = 0;
            virtual bool hasChanged(const std::string& name) const = 0;

            virtual void deleteValue(const std::string& name) = 0;
            virtual void resetToDefaults() = 0;

            virtual void hardReset() = 0;

            virtual bool isSafeMode() const = 0;
            virtual bool isExperimentalMode() const = 0;

            template <typename T, std::enable_if_t<std::is_enum<T>::value, bool> = true>
            void setEnumDefault(const std::string& name, T value) {
                setDefault(name, (int)value);
            }

            template <typename T, std::enable_if_t<std::is_enum<T>::value, bool> = true>
            T getEnumValue(const std::string& name) const {
                return (T)getValue(name);
            }

            template <typename T, std::enable_if_t<std::is_enum<T>::value, bool> = true>
            T peekEnumValue(const std::string& name) const {
                return (T)peekValue(name);
            }
        };

    } // namespace config

    namespace graphics {

        enum class Api { D3D11, D3D12 };

        // Type traits for D3D11.
        struct D3D11 {
            static constexpr Api Api = Api::D3D11;

            using Device = ID3D11Device*;
            using Context = ID3D11DeviceContext*;
            using Texture = ID3D11Texture2D*;
            using Buffer = ID3D11Buffer*;
            struct MeshData {
                ID3D11Buffer* vertexBuffer;
                UINT stride;
                ID3D11Buffer* indexBuffer;
                UINT numIndices;
            };
            using Mesh = MeshData*;
            using PixelShader = ID3D11PixelShader*;
            using ComputeShader = ID3D11ComputeShader*;
            using ShaderInputView = ID3D11ShaderResourceView*;
            using ComputeShaderOutputView = ID3D11UnorderedAccessView*;
            using RenderTargetView = ID3D11RenderTargetView*;
            using DepthStencilView = ID3D11DepthStencilView*;
        };

        // Type traits for D3D12.
        struct D3D12 {
            static constexpr Api Api = Api::D3D12;

            using Device = ID3D12Device*;
            using Context = ID3D12GraphicsCommandList*;
            using Texture = ID3D12Resource*;
            using Buffer = ID3D12Resource*;
            struct MeshData {
                D3D12_VERTEX_BUFFER_VIEW* vertexBuffer;
                D3D12_INDEX_BUFFER_VIEW* indexBuffer;
                UINT numIndices;
            };
            using Mesh = MeshData*;
            struct ShaderData {
                ID3D12RootSignature* rootSignature;
                ID3D12PipelineState* pipelineState;
            };
            using PixelShader = ShaderData*;
            using ComputeShader = ShaderData*;
            using ShaderInputView = D3D12_CPU_DESCRIPTOR_HANDLE*;
            using ComputeShaderOutputView = D3D12_CPU_DESCRIPTOR_HANDLE*;
            using RenderTargetView = D3D12_CPU_DESCRIPTOR_HANDLE*;
            using DepthStencilView = D3D12_CPU_DESCRIPTOR_HANDLE*;
        };

        // A few handy texture formats.
        enum class TextureFormat { R32G32B32A32_FLOAT, R16G16B16A16_UNORM, R10G10B10A2_UNORM, R8G8B8A8_UNORM };

        // A list of supported GPU Architectures.
        enum class GpuArchitecture { Unknown, AMD, Intel, NVidia };

        enum class TextStyle { Normal, Bold };

        struct IDevice;
        struct ITexture;

        struct View {
            XrPosef pose;
            XrFovf fov;
            xr::math::NearFar nearFar;
        };

        // A shader that will be rendered on a quad wrapping the entire target.
        struct IQuadShader {
            virtual ~IQuadShader() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            typename ApiTraits::PixelShader getNative() const {
                if (ApiTraits::Api != getApi()) {
                    throw std::runtime_error("Api mismatch");
                }
                return reinterpret_cast<typename ApiTraits::PixelShader>(getNativePtr());
            }
        };

        // A compute shader.
        struct IComputeShader {
            virtual ~IComputeShader() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;

            virtual void updateThreadGroups(const std::array<unsigned int, 3>& threadGroups) = 0;
            virtual const std::array<unsigned int, 3>& getThreadGroups() const = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            typename ApiTraits::ComputeShader getNative() const {
                if (ApiTraits::Api != getApi()) {
                    throw std::runtime_error("Api mismatch");
                }
                return reinterpret_cast<typename ApiTraits::ComputeShader>(getNativePtr());
            }
        };

        // The view of a texture in input of a shader.
        struct IShaderInputTextureView {
            virtual ~IShaderInputTextureView() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            typename ApiTraits::ShaderInputView getNative() const {
                if (ApiTraits::Api != getApi()) {
                    throw std::runtime_error("Api mismatch");
                }
                return reinterpret_cast<typename ApiTraits::ShaderInputView>(getNativePtr());
            }
        };

        // The view of a texture in output of a compute shader.
        struct IComputeShaderOutputView {
            virtual ~IComputeShaderOutputView() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            typename ApiTraits::ComputeShaderOutputView getNative() const {
                if (ApiTraits::Api != getApi()) {
                    throw std::runtime_error("Api mismatch");
                }
                return reinterpret_cast<typename ApiTraits::ComputeShaderOutputView>(getNativePtr());
            }
        };

        // The view of a texture in output of a quad shader or used for rendering.
        struct IRenderTargetView {
            virtual ~IRenderTargetView() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            typename ApiTraits::RenderTargetView getNative() const {
                if (ApiTraits::Api != getApi()) {
                    throw std::runtime_error("Api mismatch");
                }
                return reinterpret_cast<typename ApiTraits::RenderTargetView>(getNativePtr());
            }
        };

        // The view of a texture as a depth buffer.
        struct IDepthStencilView {
            virtual ~IDepthStencilView() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            typename ApiTraits::DepthStencilView getNative() const {
                if (ApiTraits::Api != getApi()) {
                    throw std::runtime_error("Api mismatch");
                }
                return reinterpret_cast<typename ApiTraits::DepthStencilView>(getNativePtr());
            }
        };

        // A texture, plain and simple!
        struct ITexture {
            virtual ~ITexture() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;
            virtual const XrSwapchainCreateInfo& getInfo() const = 0;
            virtual bool isArray() const = 0;

            virtual std::shared_ptr<IShaderInputTextureView> getShaderInputView() const = 0;
            virtual std::shared_ptr<IShaderInputTextureView> getShaderInputView(uint32_t slice) const = 0;
            virtual std::shared_ptr<IComputeShaderOutputView> getComputeShaderOutputView() const = 0;
            virtual std::shared_ptr<IComputeShaderOutputView> getComputeShaderOutputView(uint32_t slice) const = 0;
            virtual std::shared_ptr<IRenderTargetView> getRenderTargetView() const = 0;
            virtual std::shared_ptr<IRenderTargetView> getRenderTargetView(uint32_t slice) const = 0;
            virtual std::shared_ptr<IDepthStencilView> getDepthStencilView() const = 0;
            virtual std::shared_ptr<IDepthStencilView> getDepthStencilView(uint32_t slice) const = 0;

            virtual void uploadData(const void* buffer, uint32_t rowPitch, int32_t slice = -1) = 0;
            virtual void saveToFile(const std::string& path) const = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            typename ApiTraits::Texture getNative() const {
                if (ApiTraits::Api != getApi()) {
                    throw std::runtime_error("Api mismatch");
                }
                return reinterpret_cast<typename ApiTraits::Texture>(getNativePtr());
            }
        };

        // A buffer to be used with shaders.
        struct IShaderBuffer {
            virtual ~IShaderBuffer() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;

            virtual void uploadData(const void* buffer, size_t count) = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            typename ApiTraits::Buffer getNative() const {
                if (ApiTraits::Api != getApi()) {
                    throw std::runtime_error("Api mismatch");
                }
                return reinterpret_cast<typename ApiTraits::Buffer>(getNativePtr());
            }
        };

        struct SimpleMeshVertex {
            XrVector3f Position;
            XrVector3f Color;
        };

        // A simple (unskinned) mesh.
        struct ISimpleMesh {
            virtual ~ISimpleMesh() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            typename ApiTraits::Mesh getNative() const {
                if (ApiTraits::Api != getApi()) {
                    throw std::runtime_error("Api mismatch");
                }
                return reinterpret_cast<typename ApiTraits::Mesh>(getNativePtr());
            }
        };

        // A GPU asynchronous timer.
        struct IGpuTimer : public ITimer {
            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;
        };

        // A graphics execution context (eg: command list).
        struct IContext {
            virtual ~IContext() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            typename ApiTraits::Context getNative() const {
                if (ApiTraits::Api != getApi()) {
                    throw std::runtime_error("Api mismatch");
                }
                return reinterpret_cast<typename ApiTraits::Context>(getNativePtr());
            }
        };

        // A graphics device.
        struct IDevice {
            virtual ~IDevice() = default;

            virtual Api getApi() const = 0;

            virtual const std::string& getDeviceName() const = 0;
            virtual GpuArchitecture GetGpuArchitecture() const = 0;

            virtual int64_t getTextureFormat(TextureFormat format) const = 0;
            virtual bool isTextureFormatSRGB(int64_t format) const = 0;

            virtual void saveContext(bool clear = true) = 0;
            virtual void restoreContext() = 0;
            virtual void flushContext(bool blocking = false, bool isEndOfFrame = false) = 0;

            virtual std::shared_ptr<ITexture> createTexture(const XrSwapchainCreateInfo& info,
                                                            const std::optional<std::string>& debugName,
                                                            uint32_t rowPitch = 0,
                                                            uint32_t imageSize = 0,
                                                            const void* initialData = nullptr) = 0;
            virtual std::shared_ptr<IShaderBuffer> createBuffer(size_t size,
                                                                const std::optional<std::string>& debugName,
                                                                const void* initialData = nullptr,
                                                                bool immutable = false) = 0;
            virtual std::shared_ptr<ISimpleMesh> createSimpleMesh(std::vector<SimpleMeshVertex>& vertices,
                                                                  std::vector<uint16_t>& indices,
                                                                  const std::optional<std::string>& debugName) = 0;
            virtual std::shared_ptr<IQuadShader> createQuadShader(const std::string& shaderPath,
                                                                  const std::string& entryPoint,
                                                                  const std::optional<std::string>& debugName,
                                                                  const D3D_SHADER_MACRO* defines = nullptr,
                                                                  const std::string includePath = "") = 0;
            virtual std::shared_ptr<IComputeShader> createComputeShader(const std::string& shaderPath,
                                                                        const std::string& entryPoint,
                                                                        const std::optional<std::string>& debugName,
                                                                        const std::array<unsigned int, 3>& threadGroups,
                                                                        const D3D_SHADER_MACRO* defines = nullptr,
                                                                        const std::string includePath = "") = 0;
            virtual std::shared_ptr<IGpuTimer> createTimer() = 0;

            // Must be invoked prior to setting the input/output.
            virtual void setShader(std::shared_ptr<IQuadShader> shader) = 0;

            // Must be invoked prior to setting the input/output.
            virtual void setShader(std::shared_ptr<IComputeShader> shader) = 0;

            virtual void setShaderInput(uint32_t slot, std::shared_ptr<ITexture> input, int32_t slice = -1) = 0;
            virtual void setShaderInput(uint32_t slot, std::shared_ptr<IShaderBuffer> input) = 0;
            virtual void setShaderOutput(uint32_t slot, std::shared_ptr<ITexture> output, int32_t slice = -1) = 0;

            virtual void dispatchShader(bool doNotClear = false) const = 0;

            virtual void unsetRenderTargets() = 0;
            virtual void setRenderTargets(std::vector<std::shared_ptr<ITexture>> renderTargets,
                                          std::shared_ptr<ITexture> depthBuffer = {}) = 0;
            virtual void setRenderTargets(std::vector<std::pair<std::shared_ptr<ITexture>, int32_t>> renderTargets,
                                          std::pair<std::shared_ptr<ITexture>, int32_t> depthBuffer = {}) = 0;

            virtual void clearColor(float top, float left, float bottom, float right, const XrColor4f& color) const = 0;
            virtual void clearDepth(float value) = 0;

            virtual void setViewProjection(const View& view) = 0;
            virtual void draw(std::shared_ptr<ISimpleMesh> mesh,
                              const XrPosef& pose,
                              XrVector3f scaling = {1.0f, 1.0f, 1.0f}) = 0;

            virtual float drawString(std::wstring string,
                                     TextStyle style,
                                     float size,
                                     float x,
                                     float y,
                                     uint32_t color,
                                     bool measure = false,
                                     int alignment = FW1_LEFT) = 0;
            virtual float drawString(std::string string,
                                     TextStyle style,
                                     float size,
                                     float x,
                                     float y,
                                     uint32_t color,
                                     bool measure = false,
                                     int alignment = FW1_LEFT) = 0;
            virtual float measureString(std::wstring string, TextStyle style, float size) const = 0;
            virtual float measureString(std::string string, TextStyle style, float size) const = 0;
            virtual void beginText() = 0;
            virtual void flushText() = 0;

            virtual void setMipMapBias(config::MipMapBias biasing, float bias = 0.f) = 0;
            virtual uint32_t getNumBiasedSamplersThisFrame() const = 0;

            virtual void resolveQueries() = 0;

            virtual void blockCallbacks() = 0;
            virtual void unblockCallbacks() = 0;

            using SetRenderTargetEvent =
                std::function<void(std::shared_ptr<IContext>, std::shared_ptr<ITexture> renderTarget)>;
            virtual void registerSetRenderTargetEvent(SetRenderTargetEvent event) = 0;
            using UnsetRenderTargetEvent = std::function<void(std::shared_ptr<IContext>)>;
            virtual void registerUnsetRenderTargetEvent(UnsetRenderTargetEvent event) = 0;
            using CopyTextureEvent = std::function<void(std::shared_ptr<IContext> /* context */,
                                                        std::shared_ptr<ITexture> /* source */,
                                                        std::shared_ptr<ITexture> /* destination */,
                                                        int /* sourceSlice */,
                                                        int /* destinationSlice */)>;
            virtual void registerCopyTextureEvent(CopyTextureEvent event) = 0;

            virtual void shutdown() = 0;

            virtual uint32_t getBufferAlignmentConstraint() const = 0;
            virtual uint32_t getTextureAlignmentConstraint() const = 0;

            virtual void* getNativePtr() const = 0;
            virtual void* getContextPtr() const = 0;

            template <typename ApiTraits>
            typename ApiTraits::Device getAs() const {
                return ApiTraits::Api == getApi() ? reinterpret_cast<typename ApiTraits::Device>(getNativePtr())
                                                  : nullptr;
            }

            template <typename ApiTraits>
            typename ApiTraits::Device getNative() const {
                if (ApiTraits::Api != getApi()) {
                    throw std::runtime_error("Api mismatch");
                }
                return reinterpret_cast<typename ApiTraits::Device>(getNativePtr());
            }

            template <typename ApiTraits>
            typename ApiTraits::Context getContext() const {
                if (ApiTraits::Api != getApi()) {
                    throw std::runtime_error("Api mismatch");
                }
                return reinterpret_cast<typename ApiTraits::Context>(getContextPtr());
            }
        };

        // A texture upscaler (such as NIS).
        struct IUpscaler {
            virtual ~IUpscaler() = default;

            virtual void update() = 0;
            virtual void upscale(std::shared_ptr<ITexture> input,
                                 std::shared_ptr<ITexture> output,
                                 int32_t slice = -1) = 0;
        };

        // A texture post-processor.
        struct IImageProcessor {
            virtual ~IImageProcessor() = default;

            virtual void update() = 0;
            virtual void process(std::shared_ptr<ITexture> input,
                                 std::shared_ptr<ITexture> output,
                                 int32_t slice = -1) = 0;
        };

        struct IFrameAnalyzer {
            virtual ~IFrameAnalyzer() = default;

            virtual void registerColorSwapchainImage(std::shared_ptr<ITexture> source, utilities::Eye eye) = 0;

            virtual void resetForFrame() = 0;
            virtual void prepareForEndFrame() = 0;

            virtual void onSetRenderTarget(std::shared_ptr<IContext> context,
                                           std::shared_ptr<ITexture> renderTarget) = 0;
            virtual void onUnsetRenderTarget(std::shared_ptr<graphics::IContext> context) = 0;

            virtual void onCopyTexture(std::shared_ptr<ITexture> source,
                                       std::shared_ptr<ITexture> destination,
                                       int sourceSlice = -1,
                                       int destinationSlice = -1) = 0;

            virtual std::optional<utilities::Eye> getEyeHint() const = 0;
        };

        // A Variable Rate Shader (VRS) control implementation.
        struct IVariableRateShader {
            virtual ~IVariableRateShader() = default;

            virtual void update() = 0;

            virtual bool onSetRenderTarget(std::shared_ptr<IContext> context,
                                           std::shared_ptr<ITexture> renderTarget,
                                           std::optional<utilities::Eye> eyeHint) = 0;
            virtual void onUnsetRenderTarget(std::shared_ptr<graphics::IContext> context) = 0;
            virtual void disable(std::shared_ptr<graphics::IContext> context = nullptr) = 0;

            virtual void
            setViewProjectionCenters(float leftCenterX, float leftCenterY, float rightCenterX, float rightCenterY) = 0;

            virtual uint8_t getMaxDownsamplePow2() const = 0;

#ifdef _DEBUG
            virtual void startCapture() = 0;
            virtual void stopCapture() = 0;
#endif
        };

    } // namespace graphics

    namespace input {

        enum class Hand : uint32_t { Left, Right };

        struct GesturesState {
            float pinchValue[2]{NAN, NAN};
            float thumbPressValue[2]{NAN, NAN};
            float indexBendValue[2]{NAN, NAN};
            float fingerGunValue[2]{NAN, NAN};
            float squeezeValue[2]{NAN, NAN};
            float wristTapValue[2]{NAN, NAN};
            float palmTapValue[2]{NAN, NAN};
            float indexTipTapValue[2]{NAN, NAN};
            float custom1Value[2]{NAN, NAN};

            uint32_t numTrackingLosses[2]{0, 0};
        };

        struct IHandTracker {
            virtual ~IHandTracker() = default;

            virtual XrPath getInteractionProfile() const = 0;

            virtual void registerAction(XrAction action, XrActionSet actionSet) = 0;
            virtual void unregisterAction(XrAction action) = 0;
            virtual void registerActionSpace(XrSpace space,
                                             const std::string& path,
                                             const XrPosef& poseInActionSpace) = 0;
            virtual void unregisterActionSpace(XrSpace space) = 0;

            virtual void registerBindings(const XrInteractionProfileSuggestedBinding& bindings) = 0;

            virtual const std::string getFullPath(XrAction action, XrPath subactionPath) = 0;

            virtual void beginSession(XrSession session,
                                      std::shared_ptr<toolkit::graphics::IDevice> graphicsDevice) = 0;
            virtual void endSession() = 0;

            virtual void sync(XrTime frameTime, const XrActionsSyncInfo& syncInfo) = 0;
            virtual bool locate(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation& location) const = 0;
            virtual void render(const XrPosef& pose,
                                XrSpace baseSpace,
                                std::shared_ptr<graphics::ITexture> renderTarget) const = 0;

            virtual bool getActionState(const XrActionStateGetInfo& getInfo, XrActionStateBoolean& state) const = 0;
            virtual bool getActionState(const XrActionStateGetInfo& getInfo, XrActionStateFloat& state) const = 0;
            virtual bool isTrackedRecently(Hand hand) const = 0;

            virtual const GesturesState& getGesturesState() const = 0;
        };

    } // namespace input

    namespace menu {

        struct MenuStatistics {
            float fps{0.0f};
            uint64_t appCpuTimeUs{0};
            uint64_t appGpuTimeUs{0};
            uint64_t endFrameCpuTimeUs{0};
            uint64_t preProcessorGpuTimeUs{0};
            uint64_t upscalerGpuTimeUs{0};
            uint64_t postProcessorGpuTimeUs{0};
            uint64_t overlayCpuTimeUs{0};
            uint64_t overlayGpuTimeUs{0};
            uint64_t handTrackingCpuTimeUs{0};

            uint64_t predictionTimeUs{0};
            float icd{0.0f};
            float totalFov{0.0f};

            bool hasColorBuffer[utilities::ViewCount]{false, false};
            bool hasDepthBuffer[utilities::ViewCount]{false, false};
            uint32_t numBiasedSamplers{0};
            uint32_t numRenderTargetsWithVRS{0};
        };

        // A menu handler.
        struct IMenuHandler {
            virtual ~IMenuHandler() = default;

            virtual void handleInput() = 0;
            virtual void render(utilities::Eye eye, std::shared_ptr<graphics::ITexture> renderTarget) const = 0;
            virtual void updateStatistics(const MenuStatistics& stats) = 0;
            virtual void updateGesturesState(const input::GesturesState& state) = 0;
        };

    } // namespace menu

} // namespace toolkit
