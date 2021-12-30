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

namespace toolkit {

    struct LayerStatistics {
        float fps{0.0f};
        uint64_t endFrameCpuTimeUs{0};
        uint64_t preProcessorGpuTimeUs{0};
        uint64_t upscalerGpuTimeUs{0};
        uint64_t postProcessorGpuTimeUs{0};
        uint64_t overlayCpuTimeUs{0};
        uint64_t overlayGpuTimeUs{0};
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
        // A CPU synchronous timer.
        struct ICpuTimer : public ITimer {
        };
    }

    namespace config {

        const std::string SettingOverlayType = "overlay";
        const std::string SettingMenuFontSize = "font_size";
        const std::string SettingMenuTimeout = "menu_timeout";
        const std::string SettingScalingType = "scaling_type";
        const std::string SettingScaling = "scaling";
        const std::string SettingSharpness = "sharpness";

        enum class OverlayType { None = 0, FPS, Advanced, MaxValue };
        enum class MenuFontSize { Small = 0, Medium, Large, MaxValue };
        enum class MenuTimeout { Small = 0, Medium, Large, MaxValue };
        enum class ScalingType { None = 0, NIS, MaxValue };

        struct IConfigManager {
            virtual ~IConfigManager() = default;

            // Tick to indicate that the game loop ran successfully. This is used for deferred write to the config
            // database.
            virtual void tick() = 0;

            virtual void setDefault(const std::string& name, int value) = 0;

            virtual int getValue(const std::string& name) const = 0;
            virtual int peekValue(const std::string& name) const = 0;
            virtual void setValue(const std::string& name, int value) = 0;
            virtual bool hasChanged(const std::string& name) const = 0;

            virtual void resetToDefaults() = 0;

            virtual void hardReset() = 0;

            template <typename T, std::enable_if_t<std::is_enum<T>::value, bool> = true>
            void setEnumDefault(const std::string& name, T value) {
                setDefault(name, (int)value);
            }

            template <typename T, std::enable_if_t<std::is_enum<T>::value, bool> = true>
            T getEnumValue(const std::string& name) const {
                return (T)getValue(name);
            }
        };

    } // namespace config

    namespace graphics {

        enum class Api { D3D11 };

        // Type traits for D3D11.
        struct D3D11 {
            static constexpr Api Api = Api::D3D11;

            using Device = ID3D11Device*;
            using Context = ID3D11DeviceContext*;
            using Texture = ID3D11Texture2D*;
            using Buffer = ID3D11Buffer*;
            using PixelShader = ID3D11PixelShader*;
            using ComputeShader = ID3D11ComputeShader*;
            using ShaderInputView = ID3D11ShaderResourceView*;
            using ComputeShaderOutputView = ID3D11UnorderedAccessView*;
            using RenderTargetView = ID3D11RenderTargetView*;
        };

        // A few handy texture formats.
        // TODO: Extend as we start needing more formats.
        enum class TextureFormat { R32G32B32A32_FLOAT, R16G16B16A16_UNORM };

        struct IDevice;
        struct ITexture;

        // A shader that will be rendered on a quad wrapping the entire target.
        struct IQuadShader {
            virtual ~IQuadShader() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            typename ApiTraits::PixelShader getNative() const {
                if (ApiTraits::Api != getApi()) {
                    throw new std::runtime_error("Api mismatch");
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
                    throw new std::runtime_error("Api mismatch");
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
                    throw new std::runtime_error("Api mismatch");
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
                    throw new std::runtime_error("Api mismatch");
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
                    throw new std::runtime_error("Api mismatch");
                }
                return reinterpret_cast<typename ApiTraits::RenderTargetView>(getNativePtr());
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

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            typename ApiTraits::Texture getNative() const {
                if (ApiTraits::Api != getApi()) {
                    throw new std::runtime_error("Api mismatch");
                }
                return reinterpret_cast<typename ApiTraits::Texture>(getNativePtr());
            }
        };

        // A buffer to be used with shaders.
        struct IShaderBuffer {
            virtual ~IShaderBuffer() = default;

            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;

            virtual void uploadData(void* buffer, size_t count) = 0;

            virtual void* getNativePtr() const = 0;

            template <typename ApiTraits>
            typename ApiTraits::Buffer getNative() const {
                if (ApiTraits::Api != getApi()) {
                    throw new std::runtime_error("Api mismatch");
                }
                return reinterpret_cast<typename ApiTraits::Buffer>(getNativePtr());
            }
        };

        // A GPU asynchronous timer.
        struct IGpuTimer : public ITimer {
            virtual Api getApi() const = 0;
            virtual std::shared_ptr<IDevice> getDevice() const = 0;
        };

        // A graphics device.
        struct IDevice {
            virtual ~IDevice() = default;

            virtual Api getApi() const = 0;

            virtual const std::string& getDeviceName() const = 0;

            virtual int64_t getTextureFormat(TextureFormat format) const = 0;
            virtual bool isTextureFormatSRGB(int64_t format) const = 0;

            virtual std::shared_ptr<ITexture> createTexture(const XrSwapchainCreateInfo& info,
                                                            const std::optional<std::string>& debugName,
                                                            uint32_t rowPitch = 0,
                                                            uint32_t imageSize = 0,
                                                            const void* initialData = nullptr) = 0;
            virtual std::shared_ptr<IShaderBuffer> createBuffer(size_t size,
                                                                const std::optional<std::string>& debugName,
                                                                const void* initialData = nullptr) = 0;
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

            virtual void clearRenderTargets() = 0;
            virtual void setRenderTargets(std::vector<std::shared_ptr<ITexture>> renderTargets) = 0;
            virtual void setRenderTargets(std::vector<std::pair<std::shared_ptr<ITexture>, int32_t>> renderTargets) = 0;

            virtual void* getNativePtr() const = 0;
            virtual void* getContextPtr() const = 0;

            template <typename ApiTraits>
            typename ApiTraits::Device getNative() const {
                if (ApiTraits::Api != getApi()) {
                    throw new std::runtime_error("Api mismatch");
                }
                return reinterpret_cast<typename ApiTraits::Device>(getNativePtr());
            }

            template <typename ApiTraits>
            typename ApiTraits::Context getContext() const {
                if (ApiTraits::Api != getApi()) {
                    throw new std::runtime_error("Api mismatch");
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

    } // namespace graphics

    namespace menu {

        // A menu handler.
        struct IMenuHandler {
            virtual ~IMenuHandler() = default;

            virtual void handleInput() = 0;
            virtual void render(std::shared_ptr<graphics::ITexture> renderTarget) const = 0;
            virtual void updateStatistics(const LayerStatistics& stats) = 0;
        };

    } // namespace menu

} // namespace toolkit
