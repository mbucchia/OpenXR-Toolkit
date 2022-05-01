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

        // Quick and dirty API helper
        template <class T, class... Types>
        inline constexpr bool is_any_of_v = std::disjunction_v<std::is_same<T, Types>...>;

        template <typename E>
        inline constexpr auto to_integral(E e) -> typename std::underlying_type_t<E> {
            return static_cast<std::underlying_type_t<E>>(e);
        }

    } // namespace

    namespace math {

        // Missing helpers in XrMath.h
        inline DirectX::XMVECTOR XM_CALLCONV LoadXrFov(const XrFovf& fov) {
            return DirectX::XMLoadFloat4(&xr::math::detail::implement_math_cast<DirectX::XMFLOAT4>(fov));
        }

        inline void XM_CALLCONV StoreXrFov(XrFovf* outVec, DirectX::FXMVECTOR inVec) {
            DirectX::XMStoreFloat4(&xr::math::detail::implement_math_cast<DirectX::XMFLOAT4>(*outVec), inVec);
        }

        inline DirectX::XMVECTOR XM_CALLCONV ConvertToDegrees(const XrFovf& fov) {
            using namespace DirectX;
            constexpr XMVECTORF32 kRadToDeg = {{{180 / XM_PI, 180 / XM_PI, 180 / XM_PI, 180 / XM_PI}}};
            return LoadXrFov(fov) * kRadToDeg;
        }

        inline DirectX::XMVECTOR XM_CALLCONV ConvertToRadians(const XrFovf& fov) {
            using namespace DirectX;
            constexpr XMVECTORF32 kDegToRad = {{{XM_PI / 180, XM_PI / 180, XM_PI / 180, XM_PI / 180}}};
            return LoadXrFov(fov) * kDegToRad;
        }

    } // namespace math

    namespace utilities {

        // 2 views to process, one per eye.
        constexpr uint32_t ViewCount = 2;
        enum class Eye : uint32_t { Left, Right, Both };

        // A CPU synchronous timer.
        struct ICpuTimer : public ITimer {};

        // [-1,+1] (+up) -> [0..1] (+dn)
        inline constexpr XrVector2f NdcToScreen(XrVector2f v) {
            return {(v.x + 1.f) * 0.5f, (v.y - 1.f) * -0.5f};
        }

        // [0..1] (+dn) -> [-1,+1] (+up)
        inline constexpr XrVector2f ScreenToNdc(XrVector2f v) {
            return {(v.x * 2.f) - 1.f, (v.y * -2.f) + 1.f};
        }

    } // namespace utilities

    namespace config {

        const std::string SettingDeveloper = "developer";
        const std::string SettingReloadShaders = "reload_shaders";
        const std::string SettingScreenshotEnabled = "enable_screenshot";
        const std::string SettingScreenshotFileFormat = "screenshot_fileformat";
        const std::string SettingScreenshotKey = "key_screenshot";
        const std::string SettingKeyCtrlModifier = "ctrl_modifier";
        const std::string SettingKeyAltModifier = "alt_modifier";
        const std::string SettingMenuKeyUp = "key_up";
        const std::string SettingMenuKeyDown = "key_menu";
        const std::string SettingMenuKeyLeft = "key_left";
        const std::string SettingMenuKeyRight = "key_right";
        const std::string SettingMenuEyeVisibility = "menu_eye";
        const std::string SettingMenuEyeOffset = "menu_eye_offset";
        const std::string SettingOverlayType = "overlay";
        const std::string SettingMenuFontSize = "font_size";
        const std::string SettingMenuTimeout = "menu_timeout";
        const std::string SettingMenuExpert = "expert_menu";
        const std::string SettingScalingType = "scaling_type";
        const std::string SettingScaling = "scaling";
        const std::string SettingAnamorphic = "anamorphic";
        const std::string SettingSharpness = "sharpness";
        const std::string SettingMipMapBias = "mipmap_bias";
        const std::string SettingICD = "world_scale";
        const std::string SettingFOVType = "fov_type";
        const std::string SettingFOV = "fov";
        const std::string SettingFOVUp = "fov_up";
        const std::string SettingFOVDown = "fov_down";
        const std::string SettingFOVLeftLeft = "fov_ll";
        const std::string SettingFOVLeftRight = "fov_lr";
        const std::string SettingFOVRightLeft = "fov_rl";
        const std::string SettingFOVRightRight = "fov_rr";
        const std::string SettingPimaxFOVHack = "pimax_fov";
        const std::string SettingHandTrackingEnabled = "enable_hand_tracking";
        const std::string SettingHandVisibilityAndSkinTone = "hand_visibility";
        const std::string SettingHandTimeout = "hand_timeout";
        const std::string SettingPredictionDampen = "prediction_dampen";
        const std::string SettingBypassMsftHandInteractionCheck = "allow_msft_hand_interaction";
        const std::string SettingBypassMsftEyeGazeInteractionCheck = "allow_msft_eye_gaze_interaction";
        const std::string SettingMotionReprojection = "motion_reprojection";
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
        const std::string SettingVRSXScale = "vrs_x_scale";
        const std::string SettingVRSYOffset = "vrs_y_offset";
        const std::string SettingVRSPreferHorizontal = "vrs_prefer_horizontal";
        const std::string SettingVRSLeftRightBias = "vrs_lr_bias";
        const std::string SettingPostProcess = "post_process";
        const std::string SettingPostSunGlasses = "post_sunglasses";
        const std::string SettingPostContrast = "post_contrast";
        const std::string SettingPostBrightness = "post_brightness";
        const std::string SettingPostExposure = "post_exposure";
        const std::string SettingPostSaturation = "post_saturation";
        const std::string SettingPostVibrance = "post_vibrance";
        const std::string SettingPostColorGainR = "post_gain_r";
        const std::string SettingPostColorGainG = "post_gain_g";
        const std::string SettingPostColorGainB = "post_gain_b";
        const std::string SettingPostHighlights = "post_highlights";
        const std::string SettingPostShadows = "post_shadows";
        const std::string SettingEyeTrackingEnabled = "eye_tracking";
        const std::string SettingEyeProjectionDistance = "eye_projection";
        const std::string SettingEyeDebug = "eye_debug";
        const std::string SettingEyeDebugWithController = "eye_controller_debug";
        const std::string SettingResolutionOverride = "override_resolution";
        const std::string SettingResolutionWidth = "resolution_width";

        enum class OffOnType { Off = 0, On, MaxValue };
        enum class NoYesType { No = 0, Yes, MaxValue };
        enum class OverlayType { None = 0, FPS, Advanced, Developer, MaxValue };
        enum class MenuFontSize { Small = 0, Medium, Large, MaxValue };
        enum class MenuTimeout { Small = 0, Medium, Large, None, MaxValue };
        enum class ScalingType { None = 0, NIS, FSR, MaxValue };
        enum class MipMapBias { Off = 0, Anisotropic, All, MaxValue };
        enum class HandTrackingEnabled { Off = 0, Both, Left, Right, MaxValue };
        enum class HandTrackingVisibility { Hidden = 0, Bright, Medium, Dark, Darker, MaxValue };
        enum class MotionReprojection { Default = 0, Off, On, MaxValue };
        enum class MotionReprojectionRate { Off = 1, R_45Hz, R_30Hz, R_22Hz, MaxValue };
        enum class VariableShadingRateType { None = 0, Preset, Custom, MaxValue };
        enum class VariableShadingRateQuality { Performance = 0, Quality, MaxValue };
        enum class VariableShadingRatePattern { Wide = 0, Balanced, Narrow, MaxValue };
        enum class VariableShadingRateDir { Vertical, Horizontal, MaxValue };
        enum class VariableShadingRateVal { R_x1, R_2x1, R_2x2, R_4x2, R_4x4, R_Cull, MaxValue };
        enum class PostProcessType { Off = 0, On, MaxValue };
        enum class PostSunGlassesType { None = 0, Light, Dark, Night, MaxValue };
        enum class FovModeType { Simple, Advanced, MaxValue };
        enum class ScreenshotFileFormat { DDS = 0, PNG, JPG, BMP, MaxValue };

        template <typename ConfigEnumType>
        extern std::string_view to_string_view(ConfigEnumType);

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
                setDefault(name, static_cast<int>(to_integral(value)));
            }

            template <typename T, std::enable_if_t<std::is_enum<T>::value, bool> = true>
            T getEnumValue(const std::string& name) const {
                const auto value = getValue(name);
                return static_cast<T>(std::clamp(value, std::underlying_type_t<T>(0), to_integral(T::MaxValue) - 1));
            }

            template <typename T, std::enable_if_t<std::is_enum<T>::value, bool> = true>
            T peekEnumValue(const std::string& name) const {
                const auto value = peekValue(name);
                return static_cast<T>(std::clamp(value, std::underlying_type_t<T>(0), to_integral(T::MaxValue) - 1));
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
                ID3D11Buffer* indexBuffer;
                UINT stride;
                UINT numIndices;
            };
            using Mesh = MeshData*;
            using PixelShader = ID3D11PixelShader*;
            using ComputeShader = ID3D11ComputeShader*;
            using ShaderInputView = ID3D11ShaderResourceView*;
            using ComputeShaderOutputView = ID3D11UnorderedAccessView*;
            using RenderTargetView = ID3D11RenderTargetView*;
            using DepthStencilView = ID3D11DepthStencilView*;

            // clang-format off
            template <typename T>
            static constexpr bool is_concrete_api_v = is_any_of_v<T,
              Device, Context, Texture, Buffer, Mesh, PixelShader, ComputeShader, ShaderInputView, RenderTargetView, DepthStencilView>;
            // clang-format on
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

            // clang-format off
            template <typename T>
            static constexpr bool is_concrete_api_v = is_any_of_v<T,
              Device, Context, Texture, Buffer, Mesh, PixelShader, ComputeShader, ShaderInputView, RenderTargetView, DepthStencilView>;
            // clang-format on
        };

        // Graphics API helper
        template <typename ConcreteType, typename InterfaceType>
        inline auto GetAs(const InterfaceType* pInterface) {
            constexpr auto api = D3D12::is_concrete_api_v<ConcreteType> ? Api::D3D12 : Api::D3D11;
            return reinterpret_cast<ConcreteType>(api == pInterface->getApi() ? pInterface->getNativePtr() : nullptr);
        }

        // A few handy texture formats.
        enum class TextureFormat { R32G32B32A32_FLOAT, R16G16B16A16_UNORM, R10G10B10A2_UNORM, R8G8B8A8_UNORM };

        enum class SamplerType { NearestClamp, LinearClamp };

        // A list of supported GPU Architectures.
        enum class GpuArchitecture { Unknown, AMD, Intel, NVidia };

        enum class TextStyle { Normal, Bold };

        struct IDevice;
        struct ITexture;

        // A shader that will be rendered on a quad wrapping the entire target.
        struct IQuadShader {
            virtual ~IQuadShader() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            auto getAs() const {
                return GetAs<typename ApiTraits::PixelShader>(this);
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
            auto getAs() const {
                return GetAs<typename ApiTraits::ComputeShader>(this);
            }
        };

        // The view of a texture in input of a shader.
        struct IShaderInputTextureView {
            virtual ~IShaderInputTextureView() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            auto getAs() const {
                return GetAs<typename ApiTraits::ShaderInputView>(this);
            }
        };

        // The view of a texture in output of a compute shader.
        struct IComputeShaderOutputView {
            virtual ~IComputeShaderOutputView() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            auto getAs() const {
                return GetAs<typename ApiTraits::ComputeShaderOutputView>(this);
            }
        };

        // The view of a texture in output of a quad shader or used for rendering.
        struct IRenderTargetView {
            virtual ~IRenderTargetView() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            auto getAs() const {
                return GetAs<typename ApiTraits::RenderTargetView>(this);
            }
        };

        // The view of a texture as a depth buffer.
        struct IDepthStencilView {
            virtual ~IDepthStencilView() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            auto getAs() const {
                return GetAs<typename ApiTraits::DepthStencilView>(this);
            }
        };

        // A texture, plain and simple!
        struct ITexture {
            virtual ~ITexture() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;
            virtual const XrSwapchainCreateInfo& getInfo() const = 0;
            virtual bool isArray() const = 0;

            virtual std::shared_ptr<IShaderInputTextureView> getShaderResourceView(int32_t slice = -1) const = 0;
            virtual std::shared_ptr<IComputeShaderOutputView> getUnorderedAccessView(int32_t slice = -1) const = 0;
            virtual std::shared_ptr<IRenderTargetView> getRenderTargetView(int32_t slice = -1) const = 0;
            virtual std::shared_ptr<IDepthStencilView> getDepthStencilView(int32_t slice = -1) const = 0;

            virtual void uploadData(const void* buffer, uint32_t rowPitch, int32_t slice = -1) = 0;
            virtual void saveToFile(const std::filesystem::path& path) const = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            auto getAs() const {
                return GetAs<typename ApiTraits::Texture>(this);
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
            auto getAs() const {
                return GetAs<typename ApiTraits::Buffer>(this);
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
            auto getAs() const {
                return GetAs<typename ApiTraits::Mesh>(this);
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
            auto getAs() const {
                return GetAs<typename ApiTraits::Context>(this);
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
                                                            std::string_view debugName,
                                                            int64_t overrideFormat = 0,
                                                            uint32_t rowPitch = 0,
                                                            uint32_t imageSize = 0,
                                                            const void* initialData = nullptr) = 0;

            virtual std::shared_ptr<IShaderBuffer> createBuffer(size_t size,
                                                                std::string_view debugName,
                                                                const void* initialData = nullptr,
                                                                bool immutable = false) = 0;

            virtual std::shared_ptr<ISimpleMesh> createSimpleMesh(std::vector<SimpleMeshVertex>& vertices,
                                                                  std::vector<uint16_t>& indices,
                                                                  std::string_view debugName) = 0;

            virtual std::shared_ptr<IQuadShader> createQuadShader(const std::filesystem::path& shaderFile,
                                                                  const std::string& entryPoint,
                                                                  std::string_view debugName,
                                                                  const D3D_SHADER_MACRO* defines = nullptr,
                                                                  std::filesystem::path includePath = "") = 0;

            virtual std::shared_ptr<IComputeShader> createComputeShader(const std::filesystem::path& shaderFile,
                                                                        const std::string& entryPoint,
                                                                        std::string_view debugName,
                                                                        const std::array<unsigned int, 3>& threadGroups,
                                                                        const D3D_SHADER_MACRO* defines = nullptr,
                                                                        std::filesystem::path includePath = "") = 0;

            virtual std::shared_ptr<IGpuTimer> createTimer() = 0;

            // Must be invoked prior to setting the input/output.
            virtual void setShader(std::shared_ptr<IQuadShader> shader, SamplerType sampler) = 0;

            // Must be invoked prior to setting the input/output.
            virtual void setShader(std::shared_ptr<IComputeShader> shader, SamplerType sampler) = 0;

            virtual void setShaderInput(uint32_t slot, std::shared_ptr<ITexture> input, int32_t slice = -1) = 0;
            virtual void setShaderInput(uint32_t slot, std::shared_ptr<IShaderBuffer> input) = 0;
            virtual void setShaderOutput(uint32_t slot, std::shared_ptr<ITexture> output, int32_t slice = -1) = 0;

            virtual void dispatchShader(bool doNotClear = false) const = 0;

            virtual void setRenderTargets(size_t numRenderTargets,
                                          std::shared_ptr<ITexture>* renderTargets,
                                          int32_t* renderSlices = nullptr,
                                          std::shared_ptr<ITexture> depthBuffer = nullptr,
                                          int32_t depthSlice = -1) = 0;
            virtual void unsetRenderTargets() = 0;

            virtual void clearColor(float top, float left, float bottom, float right, const XrColor4f& color) const = 0;
            virtual void clearDepth(float value) = 0;

            virtual void setViewProjection(const xr::math::ViewProjection& view) = 0;
            virtual void draw(std::shared_ptr<ISimpleMesh> mesh,
                              const XrPosef& pose,
                              XrVector3f scaling = {1.0f, 1.0f, 1.0f}) = 0;

            virtual float drawString(std::wstring_view string,
                                     TextStyle style,
                                     float size,
                                     float x,
                                     float y,
                                     uint32_t color,
                                     bool measure = false,
                                     int alignment = FW1_LEFT) = 0;
            virtual float drawString(std::string_view string,
                                     TextStyle style,
                                     float size,
                                     float x,
                                     float y,
                                     uint32_t color,
                                     bool measure = false,
                                     int alignment = FW1_LEFT) = 0;
            virtual float measureString(std::wstring_view string, TextStyle style, float size) const = 0;
            virtual float measureString(std::string_view string, TextStyle style, float size) const = 0;
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

            virtual void copyTexture(std::shared_ptr<ITexture> destination, std::shared_ptr<ITexture> source) = 0;

            virtual void shutdown() = 0;

            virtual uint32_t getBufferAlignmentConstraint() const = 0;
            virtual uint32_t getTextureAlignmentConstraint() const = 0;

            virtual void* getNativePtr() const = 0;
            virtual void* getContextPtr() const = 0;

            template <typename ApiTraits>
            auto getAs() const {
                return GetAs<typename ApiTraits::Device>(this);
            }

            template <typename ApiTraits>
            auto getContextAs() const {
                return reinterpret_cast<typename ApiTraits::Context>(ApiTraits::Api == getApi() ? getContextPtr()
                                                                                                : nullptr);
            }
        };

        // A texture post-processor.
        struct IImageProcessor {
            virtual ~IImageProcessor() = default;

            virtual void reload() = 0;
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

            virtual void beginFrame(XrTime frameTime) = 0;
            virtual void endFrame() = 0;
            virtual void update() = 0;

            virtual bool onSetRenderTarget(std::shared_ptr<IContext> context,
                                           std::shared_ptr<ITexture> renderTarget,
                                           std::optional<utilities::Eye> eyeHint) = 0;
            virtual void onUnsetRenderTarget(std::shared_ptr<graphics::IContext> context) = 0;

            virtual void updateGazeLocation(XrVector2f gaze, utilities::Eye eye) = 0;
            virtual void setViewProjectionCenters(XrVector2f left, XrVector2f right) = 0;

            virtual uint8_t getMaxRate() const = 0;

            virtual void startCapture() = 0;
            virtual void stopCapture() = 0;
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

            int64_t handposeAgeUs[2]{0, 0};
            size_t cacheSize[2]{0, 0};
            uint32_t numTrackingLosses[2]{0, 0};
            float hapticsFrequency[2]{NAN, NAN};
            int64_t hapticsDurationUs[2]{-2, -2};
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

            virtual void sync(XrTime frameTime, XrTime now, const XrActionsSyncInfo& syncInfo) = 0;
            virtual bool
            locate(XrSpace space, XrSpace baseSpace, XrTime time, XrTime now, XrSpaceLocation& location) const = 0;
            virtual void render(const XrPosef& pose,
                                XrSpace baseSpace,
                                XrTime time,
                                std::shared_ptr<graphics::ITexture> renderTarget) const = 0;

            virtual bool getActionState(const XrActionStateGetInfo& getInfo, XrActionStateBoolean& state) const = 0;
            virtual bool getActionState(const XrActionStateGetInfo& getInfo, XrActionStateFloat& state) const = 0;
            virtual bool isTrackedRecently(Hand hand) const = 0;

            virtual void handleOutput(Hand hand, float frequency, XrDuration duration) = 0;

            virtual const GesturesState& getGesturesState() const = 0;
        };

        struct EyeGazeState {
            XrVector3f gazeRay{};
            XrVector2f leftPoint{};
            XrVector2f rightPoint{};
        };

        struct IEyeTracker {
            virtual ~IEyeTracker() = default;

            virtual void beginSession(XrSession session) = 0;
            virtual void endSession() = 0;

            virtual void beginFrame(XrTime frameTime) = 0;
            virtual void endFrame() = 0;
            virtual void update() = 0;

            virtual XrActionSet getActionSet() const = 0;
            virtual bool getProjectedGaze(XrVector2f gaze[utilities::ViewCount]) const = 0;

            virtual bool isProjectionDistanceSupported() const = 0;

            virtual const EyeGazeState& getEyeGazeState() const = 0;
        };

    } // namespace input

    namespace menu {

        struct MenuStatistics {
            uint64_t appCpuTimeUs{0};
            uint64_t appGpuTimeUs{0};
            uint64_t waitCpuTimeUs{0};
            uint64_t endFrameCpuTimeUs{0};
            uint64_t processorGpuTimeUs[3]{0};
            uint64_t overlayCpuTimeUs{0};
            uint64_t overlayGpuTimeUs{0};
            uint64_t handTrackingCpuTimeUs{0};
            uint64_t predictionTimeUs{0};

            float fps{0.0f};
            float icd{0.0f};
            XrFovf fov[2]{{0}};
            uint32_t numBiasedSamplers{0};
            uint32_t numRenderTargetsWithVRS{0};

            bool hasColorBuffer[utilities::ViewCount]{false, false};
            bool hasDepthBuffer[utilities::ViewCount]{false, false};
        };

        // A menu handler.
        struct IMenuHandler {
            virtual ~IMenuHandler() = default;

            virtual void handleInput() = 0;
            virtual void render(utilities::Eye eye, std::shared_ptr<graphics::ITexture> renderTarget) const = 0;
            virtual void updateStatistics(const MenuStatistics& stats) = 0;
            virtual void updateGesturesState(const input::GesturesState& state) = 0;
            virtual void updateEyeGazeState(const input::EyeGazeState& state) = 0;

            virtual void setViewProjectionCenters(XrVector2f left, XrVector2f right) = 0;
        };

    } // namespace menu

} // namespace toolkit
