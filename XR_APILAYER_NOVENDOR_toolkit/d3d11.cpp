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

#include "pch.h"

#include "shader_utilities.h"
#include "factories.h"
#include "interfaces.h"
#include "log.h"

namespace {

    using namespace toolkit;
    using namespace toolkit::graphics;
    using namespace toolkit::log;

    struct ModelConstantBuffer {
        DirectX::XMFLOAT4X4 Model;
    };

    struct ViewProjectionConstantBuffer {
        DirectX::XMFLOAT4X4 ViewProjection;
    };

    const std::string MeshShaders = R"_(
struct VSOutput {
    float4 Pos : SV_POSITION;
    float3 Color : COLOR0;
};
struct VSInput {
    float3 Pos : POSITION;
    float3 Color : COLOR0;
};
cbuffer ModelConstantBuffer : register(b0) {
    float4x4 Model;
};
cbuffer ViewProjectionConstantBuffer : register(b1) {
    float4x4 ViewProjection;
};

VSOutput vsMain(VSInput input) {
    VSOutput output;
    output.Pos = mul(mul(float4(input.Pos, 1), Model), ViewProjection);
    output.Color = input.Color;
    return output;
}

float4 psMain(VSOutput input) : SV_TARGET {
    return float4(input.Color, 1);
}
)_";

    const std::string QuadVertexShader = R"_(
void vsMain(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD0)
{
    texcoord = float2((id == 1) ? 2.0 : 0.0, (id == 2) ? 2.0 : 0.0);
    position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}
)_";

    // Wrap a pixel shader resource. Obtained from D3D11Device.
    class D3D11QuadShader : public IQuadShader {
      public:
        D3D11QuadShader(std::shared_ptr<IDevice> device, ComPtr<ID3D11PixelShader> pixelShader)
            : m_device(device), m_pixelShader(pixelShader) {
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void* getNativePtr() const override {
            return m_pixelShader.Get();
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        const ComPtr<ID3D11PixelShader> m_pixelShader;
    };

    // Wrap a compute shader resource. Obtained from D3D11Device.
    class D3D11ComputeShader : public IComputeShader {
      public:
        D3D11ComputeShader(std::shared_ptr<IDevice> device,
                           ComPtr<ID3D11ComputeShader> computeShader,
                           const std::array<unsigned int, 3>& threadGroups)
            : m_device(device), m_computeShader(computeShader), m_threadGroups(threadGroups) {
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void updateThreadGroups(const std::array<unsigned int, 3>& threadGroups) override {
            m_threadGroups = threadGroups;
        }

        const std::array<unsigned int, 3>& getThreadGroups() const {
            return m_threadGroups;
        }

        void* getNativePtr() const override {
            return m_computeShader.Get();
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        const ComPtr<ID3D11ComputeShader> m_computeShader;
        std::array<unsigned int, 3> m_threadGroups;
    };

    // Wrap a texture shader resource view. Obtained from D3D11Texture.
    class D3D11ShaderResourceView : public IShaderInputTextureView {
      public:
        D3D11ShaderResourceView(std::shared_ptr<IDevice> device, ComPtr<ID3D11ShaderResourceView> shaderResourceView)
            : m_device(device), m_shaderResourceView(shaderResourceView) {
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void* getNativePtr() const override {
            return m_shaderResourceView.Get();
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        const ComPtr<ID3D11ShaderResourceView> m_shaderResourceView;
    };

    // Wrap a texture unordered access view. Obtained from D3D11Texture.
    class D3D11UnorderedAccessView : public IComputeShaderOutputView {
      public:
        D3D11UnorderedAccessView(std::shared_ptr<IDevice> device, ComPtr<ID3D11UnorderedAccessView> unorderedAccessView)
            : m_device(device), m_unorderedAccessView(unorderedAccessView) {
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void* getNativePtr() const override {
            return m_unorderedAccessView.Get();
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        const ComPtr<ID3D11UnorderedAccessView> m_unorderedAccessView;
    };

    // Wrap a render target view. Obtained from D3D11Texture.
    class D3D11RenderTargetView : public IRenderTargetView {
      public:
        D3D11RenderTargetView(std::shared_ptr<IDevice> device, ComPtr<ID3D11RenderTargetView> renderTargetView)
            : m_device(device), m_renderTargetView(renderTargetView) {
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void* getNativePtr() const override {
            return m_renderTargetView.Get();
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        const ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    };

    // Wrap a depth/stencil buffer view. Obtained from D3D11Texture.
    class D3D11DepthStencilView : public IDepthStencilView {
      public:
        D3D11DepthStencilView(std::shared_ptr<IDevice> device, ComPtr<ID3D11DepthStencilView> depthStencilView)
            : m_device(device), m_depthStencilView(depthStencilView) {
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void clearDepth(float value) override {
            m_device->getContext<D3D11>()->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, value, 0);
        }

        void* getNativePtr() const override {
            return m_depthStencilView.Get();
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        const ComPtr<ID3D11DepthStencilView> m_depthStencilView;
    };

    // Wrap a texture resource. Obtained from D3D11Device.
    class D3D11Texture : public ITexture {
      public:
        D3D11Texture(std::shared_ptr<IDevice> device,
                     const XrSwapchainCreateInfo& info,
                     const D3D11_TEXTURE2D_DESC& textureDesc,
                     ComPtr<ID3D11Texture2D> texture)
            : m_device(device), m_info(info), m_textureDesc(textureDesc), m_texture(texture) {
            m_shaderResourceSubView.resize(info.arraySize);
            m_unorderedAccessSubView.resize(info.arraySize);
            m_renderTargetSubView.resize(info.arraySize);
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        const XrSwapchainCreateInfo& getInfo() const override {
            return m_info;
        }

        bool isArray() const override {
            return m_textureDesc.ArraySize > 1;
        }

        std::shared_ptr<IShaderInputTextureView> getShaderInputView() const override {
            return getShaderInputViewInternal(m_shaderResourceView, 0);
        }

        std::shared_ptr<IShaderInputTextureView> getShaderInputView(uint32_t slice) const override {
            return getShaderInputViewInternal(m_shaderResourceSubView[slice], slice);
        }

        std::shared_ptr<IComputeShaderOutputView> getComputeShaderOutputView() const override {
            return getComputeShaderOutputViewInternal(m_unorderedAccessView, 0);
        }

        std::shared_ptr<IComputeShaderOutputView> getComputeShaderOutputView(uint32_t slice) const override {
            return getComputeShaderOutputViewInternal(m_unorderedAccessSubView[slice], slice);
        }

        std::shared_ptr<IRenderTargetView> getRenderTargetView() const override {
            return getRenderTargetViewInternal(m_renderTargetView, 0);
        }

        std::shared_ptr<IRenderTargetView> getRenderTargetView(uint32_t slice) const override {
            return getRenderTargetViewInternal(m_renderTargetSubView[slice], slice);
        }

        std::shared_ptr<IDepthStencilView> getDepthStencilView() const override {
            return getDepthStencilViewInternal(m_depthStencilView, 0);
        }

        std::shared_ptr<IDepthStencilView> getDepthStencilView(uint32_t slice) const override {
            return getDepthStencilViewInternal(m_depthStencilSubView[slice], slice);
        }

        void saveToFile(const std::string& path) const override {
            const HRESULT hr =
                D3DX11SaveTextureToFileA(m_device->getContext<D3D11>(), m_texture.Get(), D3DX11_IFF_DDS, path.c_str());
            if (SUCCEEDED(hr)) {
                Log("Screenshot saved to %s\n", path.c_str());
            } else {
                Log("Failed to take screenshot: %d\n", hr);
            }
        }

        void* getNativePtr() const override {
            return m_texture.Get();
        }

      private:
        std::shared_ptr<D3D11ShaderResourceView> getShaderInputViewInternal(
            std::shared_ptr<D3D11ShaderResourceView>& shaderResourceView, uint32_t slice = 0) const {
            if (!shaderResourceView) {
                if (!(m_textureDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)) {
                    throw new std::runtime_error("Texture was not created with D3D11_BIND_SHADER_RESOURCE");
                }

                auto device = m_device->getNative<D3D11>();

                D3D11_SHADER_RESOURCE_VIEW_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.Format = (DXGI_FORMAT)m_info.format;
                desc.ViewDimension =
                    m_info.arraySize == 1 ? D3D11_SRV_DIMENSION_TEXTURE2D : D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.ArraySize = 1;
                desc.Texture2DArray.FirstArraySlice = slice;
                desc.Texture2DArray.MipLevels = m_info.mipCount;
                desc.Texture2DArray.MostDetailedMip = D3D11CalcSubresource(0, 0, m_info.mipCount);

                ComPtr<ID3D11ShaderResourceView> srv;
                CHECK_HRCMD(device->CreateShaderResourceView(m_texture.Get(), &desc, &srv));

                shaderResourceView = std::make_shared<D3D11ShaderResourceView>(m_device, srv);
            }
            return shaderResourceView;
        }

        std::shared_ptr<D3D11UnorderedAccessView> getComputeShaderOutputViewInternal(
            std::shared_ptr<D3D11UnorderedAccessView>& unorderedAccessView, uint32_t slice = 0) const {
            if (!unorderedAccessView) {
                if (!(m_textureDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)) {
                    throw new std::runtime_error("Texture was not created with D3D11_BIND_UNORDERED_ACCESS");
                }

                auto device = m_device->getNative<D3D11>();

                D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.Format = (DXGI_FORMAT)m_info.format;
                desc.ViewDimension =
                    m_info.arraySize == 1 ? D3D11_UAV_DIMENSION_TEXTURE2D : D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.ArraySize = 1;
                desc.Texture2DArray.FirstArraySlice = slice;
                desc.Texture2DArray.MipSlice = D3D11CalcSubresource(0, 0, m_info.mipCount);

                ComPtr<ID3D11UnorderedAccessView> uav;
                CHECK_HRCMD(device->CreateUnorderedAccessView(m_texture.Get(), &desc, &uav));

                unorderedAccessView = std::make_shared<D3D11UnorderedAccessView>(m_device, uav);
            }
            return unorderedAccessView;
        }

        std::shared_ptr<D3D11RenderTargetView> getRenderTargetViewInternal(
            std::shared_ptr<D3D11RenderTargetView>& renderTargetView, uint32_t slice = 0) const {
            if (!renderTargetView) {
                if (!(m_textureDesc.BindFlags & D3D11_BIND_RENDER_TARGET)) {
                    throw new std::runtime_error("Texture was not created with D3D11_BIND_RENDER_TARGET");
                }

                auto device = m_device->getNative<D3D11>();

                D3D11_RENDER_TARGET_VIEW_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.Format = (DXGI_FORMAT)m_info.format;
                desc.ViewDimension =
                    m_info.arraySize == 1 ? D3D11_RTV_DIMENSION_TEXTURE2D : D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.ArraySize = 1;
                desc.Texture2DArray.FirstArraySlice = slice;
                desc.Texture2DArray.MipSlice = D3D11CalcSubresource(0, 0, m_info.mipCount);

                ComPtr<ID3D11RenderTargetView> rtv;
                CHECK_HRCMD(device->CreateRenderTargetView(m_texture.Get(), &desc, &rtv));

                renderTargetView = std::make_shared<D3D11RenderTargetView>(m_device, rtv);
            }
            return renderTargetView;
        }

        std::shared_ptr<D3D11DepthStencilView> getDepthStencilViewInternal(
            std::shared_ptr<D3D11DepthStencilView>& depthStencilView, uint32_t slice = 0) const {
            if (!depthStencilView) {
                if (!(m_textureDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL)) {
                    throw new std::runtime_error("Texture was not created with D3D11_BIND_DEPTH_STENCIL");
                }

                auto device = m_device->getNative<D3D11>();

                D3D11_DEPTH_STENCIL_VIEW_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.Format = (DXGI_FORMAT)m_info.format;
                desc.ViewDimension =
                    m_info.arraySize == 1 ? D3D11_DSV_DIMENSION_TEXTURE2D : D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.ArraySize = 1;
                desc.Texture2DArray.FirstArraySlice = slice;
                desc.Texture2DArray.MipSlice = D3D11CalcSubresource(0, 0, m_info.mipCount);

                ComPtr<ID3D11DepthStencilView> rtv;
                CHECK_HRCMD(device->CreateDepthStencilView(m_texture.Get(), &desc, &rtv));

                depthStencilView = std::make_shared<D3D11DepthStencilView>(m_device, rtv);
            }
            return depthStencilView;
        }

        const std::shared_ptr<IDevice> m_device;
        const XrSwapchainCreateInfo m_info;
        const D3D11_TEXTURE2D_DESC m_textureDesc;
        const ComPtr<ID3D11Texture2D> m_texture;

        mutable std::shared_ptr<D3D11ShaderResourceView> m_shaderResourceView;
        mutable std::vector<std::shared_ptr<D3D11ShaderResourceView>> m_shaderResourceSubView;
        mutable std::shared_ptr<D3D11UnorderedAccessView> m_unorderedAccessView;
        mutable std::vector<std::shared_ptr<D3D11UnorderedAccessView>> m_unorderedAccessSubView;
        mutable std::shared_ptr<D3D11RenderTargetView> m_renderTargetView;
        mutable std::vector<std::shared_ptr<D3D11RenderTargetView>> m_renderTargetSubView;
        mutable std::shared_ptr<D3D11DepthStencilView> m_depthStencilView;
        mutable std::vector<std::shared_ptr<D3D11DepthStencilView>> m_depthStencilSubView;
    };

    // Wrap a constant buffer. Obtained from D3D11Device.
    class D3D11Buffer : public IShaderBuffer {
      public:
        D3D11Buffer(std::shared_ptr<IDevice> device, D3D11_BUFFER_DESC bufferDesc, ComPtr<ID3D11Buffer> buffer)
            : m_device(device), m_bufferDesc(bufferDesc), m_buffer(buffer) {
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void uploadData(void* buffer, size_t count) override {
            if (m_bufferDesc.ByteWidth != count) {
                throw new std::runtime_error("Upload size mismatch");
            }

            auto context = m_device->getContext<D3D11>();

            D3D11_MAPPED_SUBRESOURCE mappedResources;
            CHECK_HRCMD(context->Map(m_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResources));
            memcpy(mappedResources.pData, buffer, count);
            context->Unmap(m_buffer.Get(), 0);
        }

        void* getNativePtr() const override {
            return m_buffer.Get();
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        const D3D11_BUFFER_DESC m_bufferDesc;
        const ComPtr<ID3D11Buffer> m_buffer;
    };

    // Wrap a vertex+indices buffers. Obtained from D3D11Device.
    class D3D11SimpleMesh : public ISimpleMesh {
      public:
        D3D11SimpleMesh(std::shared_ptr<IDevice> device,
                        ComPtr<ID3D11Buffer> vertexBuffer,
                        size_t stride,
                        ComPtr<ID3D11Buffer> indexBuffer,
                        size_t numIndices)
            : m_device(device), m_vertexBuffer(vertexBuffer), m_indexBuffer(indexBuffer) {
            m_meshData.vertexBuffer = m_vertexBuffer.Get();
            m_meshData.stride = (UINT)stride;
            m_meshData.indexBuffer = m_indexBuffer.Get();
            m_meshData.numIndices = (UINT)numIndices;
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void* getNativePtr() const override {
            return reinterpret_cast<void*>(&m_meshData);
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        const ComPtr<ID3D11Buffer> m_vertexBuffer;
        const ComPtr<ID3D11Buffer> m_indexBuffer;

        mutable struct D3D11::MeshData m_meshData;
    };

    class D3D11GpuTimer : public IGpuTimer {
      public:
        D3D11GpuTimer(std::shared_ptr<IDevice> device) : m_device(device) {
            auto d3dDevice = m_device->getNative<D3D11>();

            D3D11_QUERY_DESC queryDesc;
            ZeroMemory(&queryDesc, sizeof(D3D11_QUERY_DESC));
            queryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
            CHECK_HRCMD(d3dDevice->CreateQuery(&queryDesc, &m_timeStampDis));
            queryDesc.Query = D3D11_QUERY_TIMESTAMP;
            CHECK_HRCMD(d3dDevice->CreateQuery(&queryDesc, &m_timeStampStart));
            CHECK_HRCMD(d3dDevice->CreateQuery(&queryDesc, &m_timeStampEnd));
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void start() override {
            assert(!m_valid);

            auto context = m_device->getContext<D3D11>();

            context->Begin(m_timeStampDis.Get());
            context->End(m_timeStampStart.Get());
        }

        void stop() override {
            assert(!m_valid);

            auto context = m_device->getContext<D3D11>();

            context->End(m_timeStampEnd.Get());
            context->End(m_timeStampDis.Get());
            m_valid = true;
        }

        uint64_t query(bool reset) const override {
            auto context = m_device->getContext<D3D11>();

            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disData;
            UINT64 startime;
            UINT64 endtime;

            uint64_t duration = 0;

            if (m_valid &&
                context->GetData(m_timeStampDis.Get(), &disData, sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0) ==
                    S_OK &&
                context->GetData(m_timeStampStart.Get(), &startime, sizeof(UINT64), 0) == S_OK &&
                context->GetData(m_timeStampEnd.Get(), &endtime, sizeof(UINT64), 0) == S_OK && !disData.Disjoint) {
                duration = (uint64_t)((endtime - startime) / double(disData.Frequency) * 1e6);
            }

            m_valid = !reset;

            return duration;
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        ComPtr<ID3D11Query> m_timeStampDis;
        ComPtr<ID3D11Query> m_timeStampStart;
        ComPtr<ID3D11Query> m_timeStampEnd;

        // Can the timer be queried (it might still only read 0).
        mutable bool m_valid{false};
    };

    class D3D11Device : public IDevice, public std::enable_shared_from_this<D3D11Device> {
      public:
        D3D11Device(ID3D11Device* device) : m_device(device) {
            m_device->GetImmediateContext(&m_context);

            // Create common resources.
            {
                D3D11_SAMPLER_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
                desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
                desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
                desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
                desc.MaxAnisotropy = 1;
                desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
                CHECK_HRCMD(m_device->CreateSamplerState(&desc, &m_linearClampSamplerPS));
            }
            {
                D3D11_SAMPLER_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
                desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
                desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
                desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
                desc.MaxAnisotropy = 1;
                desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
                desc.MinLOD = D3D11_MIP_LOD_BIAS_MIN;
                desc.MaxLOD = D3D11_MIP_LOD_BIAS_MAX;
                CHECK_HRCMD(m_device->CreateSamplerState(&desc, &m_linearClampSamplerCS));
            }
            {
                D3D11_RASTERIZER_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.FillMode = D3D11_FILL_SOLID;
                desc.CullMode = D3D11_CULL_NONE;
                desc.FrontCounterClockwise = TRUE;
                CHECK_HRCMD(m_device->CreateRasterizerState(&desc, &m_quadRasterizer));
                desc.MultisampleEnable = TRUE;
                CHECK_HRCMD(m_device->CreateRasterizerState(&desc, &m_quadRasterizerMSAA));
            }
            {
                ComPtr<ID3DBlob> errors;
                ComPtr<ID3DBlob> vsBytes;
                HRESULT hr = D3DCompile(MeshShaders.c_str(),
                                        MeshShaders.length(),
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        "vsMain",
                                        "vs_5_0",
                                        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS,
                                        0,
                                        &vsBytes,
                                        &errors);
                if (FAILED(hr)) {
                    if (errors) {
                        Log("%s", (char*)errors->GetBufferPointer());
                    }
                    CHECK_HRESULT(hr, "Failed to compile shader");
                }
                CHECK_HRCMD(m_device->CreateVertexShader(
                    vsBytes->GetBufferPointer(), vsBytes->GetBufferSize(), nullptr, &m_meshVertexShader));
                {
                    const std::string debugName = "SimpleMesh VS";
                    m_meshVertexShader->SetPrivateData(
                        WKPDID_D3DDebugObjectName, (UINT)debugName.size(), debugName.c_str());
                }

                const D3D11_INPUT_ELEMENT_DESC vertexDesc[] = {
                    {"POSITION",
                     0,
                     DXGI_FORMAT_R32G32B32_FLOAT,
                     0,
                     D3D11_APPEND_ALIGNED_ELEMENT,
                     D3D11_INPUT_PER_VERTEX_DATA,
                     0},
                    {"COLOR",
                     0,
                     DXGI_FORMAT_R32G32B32_FLOAT,
                     0,
                     D3D11_APPEND_ALIGNED_ELEMENT,
                     D3D11_INPUT_PER_VERTEX_DATA,
                     0},
                };

                CHECK_HRCMD(m_device->CreateInputLayout(vertexDesc,
                                                        (UINT)std::size(vertexDesc),
                                                        vsBytes->GetBufferPointer(),
                                                        vsBytes->GetBufferSize(),
                                                        &m_meshInputLayout));
            }
            {
                ComPtr<ID3DBlob> errors;
                ComPtr<ID3DBlob> psBytes;
                HRESULT hr = D3DCompile(MeshShaders.c_str(),
                                        MeshShaders.length(),
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        "psMain",
                                        "ps_5_0",
                                        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS,
                                        0,
                                        &psBytes,
                                        &errors);
                if (FAILED(hr)) {
                    if (errors) {
                        Log("%s", (char*)errors->GetBufferPointer());
                    }
                    CHECK_HRESULT(hr, "Failed to compile shader");
                }
                CHECK_HRCMD(m_device->CreatePixelShader(
                    psBytes->GetBufferPointer(), psBytes->GetBufferSize(), nullptr, &m_meshPixelShader));
                {
                    const std::string debugName = "SimpleMesh PS";
                    m_meshPixelShader->SetPrivateData(
                        WKPDID_D3DDebugObjectName, (UINT)debugName.size(), debugName.c_str());
                }
            }
            {
                D3D11_DEPTH_STENCIL_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.DepthEnable = true;
                desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
                desc.DepthFunc = D3D11_COMPARISON_GREATER;
                CHECK_HRCMD(m_device->CreateDepthStencilState(&desc, &m_reversedZDepthNoStencilTest));
            }
            {
                ComPtr<ID3DBlob> errors;
                ComPtr<ID3DBlob> vsBytes;
                HRESULT hr = D3DCompile(QuadVertexShader.c_str(),
                                        QuadVertexShader.length(),
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        "vsMain",
                                        "vs_5_0",
                                        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS,
                                        0,
                                        &vsBytes,
                                        &errors);
                if (FAILED(hr)) {
                    if (errors) {
                        Log("%s", (char*)errors->GetBufferPointer());
                    }
                    CHECK_HRESULT(hr, "Failed to compile shader");
                }
                CHECK_HRCMD(m_device->CreateVertexShader(
                    vsBytes->GetBufferPointer(), vsBytes->GetBufferSize(), nullptr, &m_quadVertexShader));
                {
                    const std::string debugName = "Quad PS";
                    m_quadVertexShader->SetPrivateData(
                        WKPDID_D3DDebugObjectName, (UINT)debugName.size(), debugName.c_str());
                }
            }

            {
                ComPtr<IDXGIDevice> dxgiDevice;
                ComPtr<IDXGIAdapter> adapter;
                DXGI_ADAPTER_DESC desc;

                CHECK_HRCMD(m_device->QueryInterface(__uuidof(IDXGIDevice),
                                                     reinterpret_cast<void**>(dxgiDevice.GetAddressOf())));
                CHECK_HRCMD(dxgiDevice->GetAdapter(&adapter));
                CHECK_HRCMD(adapter->GetDesc(&desc));

                const std::wstring wadapterDescription(desc.Description);
                std::transform(wadapterDescription.begin(),
                               wadapterDescription.end(),
                               std::back_inserter(m_deviceName),
                               [](wchar_t c) { return (char)c; });

                // Log the adapter name to help debugging customer issues.
                Log("Using adapter: %s\n", m_deviceName.c_str());
            }
        }

        ~D3D11Device() override {
            DebugLog("D3D11Device is destructed\n");
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        const std::string& getDeviceName() const override {
            return m_deviceName;
        }

        int64_t getTextureFormat(TextureFormat format) const override {
            switch (format) {
            case TextureFormat::R32G32B32A32_FLOAT:
                return (int64_t)DXGI_FORMAT_R32G32B32A32_FLOAT;

            case TextureFormat::R16G16B16A16_UNORM:
                return (int64_t)DXGI_FORMAT_R16G16B16A16_UNORM;

            case TextureFormat::R10G10B10A2_UNORM:
                return (int64_t)DXGI_FORMAT_R10G10B10A2_UNORM;

            case TextureFormat::R8G8B8A8_UNORM:
                return (int64_t)DXGI_FORMAT_R8G8B8A8_UNORM;

            default:
                throw new std::runtime_error("Unknown texture format");
            };
        }

        bool isTextureFormatSRGB(int64_t format) const override {
            switch ((DXGI_FORMAT)format) {
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
                return true;
            default:
                return false;
            };
        }

        std::shared_ptr<ITexture> createTexture(const XrSwapchainCreateInfo& info,
                                                const std::optional<std::string>& debugName,
                                                uint32_t rowPitch = 0,
                                                uint32_t imageSize = 0,
                                                const void* initialData = nullptr) override {
            D3D11_TEXTURE2D_DESC desc;
            ZeroMemory(&desc, sizeof(desc));
            desc.Format = (DXGI_FORMAT)info.format;
            desc.Width = info.width;
            desc.Height = info.height;
            desc.ArraySize = info.arraySize;
            desc.MipLevels = info.mipCount;
            desc.SampleDesc.Count = info.sampleCount;
            desc.Usage = D3D11_USAGE_DEFAULT;
            if (info.usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT) {
                desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
            }
            if (info.usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
            }
            if (info.usageFlags & XR_SWAPCHAIN_USAGE_SAMPLED_BIT) {
                desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
            }
            if (info.usageFlags & XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT) {
                desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
            }

            ComPtr<ID3D11Texture2D> texture;
            if (initialData) {
                D3D11_SUBRESOURCE_DATA data;
                ZeroMemory(&data, sizeof(data));
                data.pSysMem = initialData;
                data.SysMemPitch = static_cast<uint32_t>(rowPitch);
                data.SysMemSlicePitch = static_cast<uint32_t>(imageSize);

                CHECK_HRCMD(m_device->CreateTexture2D(&desc, &data, &texture));
            } else {
                CHECK_HRCMD(m_device->CreateTexture2D(&desc, nullptr, &texture));
            }

            if (debugName) {
                texture->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)debugName->size(), debugName->c_str());
            }

            return std::make_shared<D3D11Texture>(shared_from_this(), info, desc, texture);
        }

        std::shared_ptr<IShaderBuffer> createBuffer(size_t size,
                                                    const std::optional<std::string>& debugName,
                                                    const void* initialData,
                                                    bool immutable) override {
            D3D11_BUFFER_DESC desc;
            ZeroMemory(&desc, sizeof(desc));
            desc.ByteWidth = (UINT)size;
            desc.Usage = (initialData && immutable) ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = immutable ? 0 : D3D11_CPU_ACCESS_WRITE;

            ComPtr<ID3D11Buffer> buffer;
            if (initialData) {
                D3D11_SUBRESOURCE_DATA data;
                ZeroMemory(&data, sizeof(data));
                data.pSysMem = initialData;

                CHECK_HRCMD(m_device->CreateBuffer(&desc, &data, &buffer));
            } else {
                CHECK_HRCMD(m_device->CreateBuffer(&desc, nullptr, &buffer));
            }

            if (debugName) {
                buffer->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)debugName->size(), debugName->c_str());
            }

            return std::make_shared<D3D11Buffer>(shared_from_this(), desc, buffer);
        }

        std::shared_ptr<ISimpleMesh> createSimpleMesh(std::vector<SimpleMeshVertex>& vertices,
                                                      std::vector<uint16_t>& indices,
                                                      const std::optional<std::string>& debugName) override {
            D3D11_BUFFER_DESC desc;
            ZeroMemory(&desc, sizeof(desc));
            desc.Usage = D3D11_USAGE_IMMUTABLE;

            D3D11_SUBRESOURCE_DATA data;
            ZeroMemory(&data, sizeof(data));

            desc.ByteWidth = (UINT)vertices.size() * sizeof (SimpleMeshVertex);
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            data.pSysMem = vertices.data();
            ComPtr<ID3D11Buffer> vertexBuffer;
            CHECK_HRCMD(m_device->CreateBuffer(&desc, &data, &vertexBuffer));

            desc.ByteWidth = (UINT)indices.size() * sizeof(uint16_t);
            desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            data.pSysMem = indices.data();
            ComPtr<ID3D11Buffer> indexBuffer;
            CHECK_HRCMD(m_device->CreateBuffer(&desc, &data, &indexBuffer));

            if (debugName) {
                vertexBuffer->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)debugName->size(), debugName->c_str());
                indexBuffer->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)debugName->size(), debugName->c_str());
            }

            return std::make_shared<D3D11SimpleMesh>(
                shared_from_this(), vertexBuffer.Get(), sizeof(SimpleMeshVertex), indexBuffer.Get(), indices.size());
        }

        std::shared_ptr<IQuadShader> createQuadShader(const std::string& shaderPath,
                                                      const std::string& entryPoint,
                                                      const std::optional<std::string>& debugName,
                                                      const D3D_SHADER_MACRO* defines,
                                                      const std::string includePath) override {
            ComPtr<ID3DBlob> psBytes;
            if (!includePath.empty()) {
                utilities::shader::IncludeHeader includes({includePath});
                utilities::shader::CompileShader(
                    m_device.Get(), shaderPath, entryPoint, &psBytes, defines, &includes, "ps_5_0");
            } else {
                utilities::shader::CompileShader(
                    m_device.Get(), shaderPath, entryPoint, &psBytes, defines, nullptr, "ps_5_0");
            }

            ComPtr<ID3D11PixelShader> compiledShader;
            CHECK_HRCMD(m_device->CreatePixelShader(
                psBytes->GetBufferPointer(), psBytes->GetBufferSize(), nullptr, &compiledShader));

            if (debugName) {
                compiledShader->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)debugName->size(), debugName->c_str());
            }

            return std::make_shared<D3D11QuadShader>(shared_from_this(), compiledShader.Get());
        }

        std::shared_ptr<IComputeShader> createComputeShader(const std::string& shaderPath,
                                                            const std::string& entryPoint,
                                                            const std::optional<std::string>& debugName,
                                                            const std::array<unsigned int, 3>& threadGroups,
                                                            const D3D_SHADER_MACRO* defines,
                                                            const std::string includePath) override {
            ComPtr<ID3DBlob> csBytes;
            if (!includePath.empty()) {
                utilities::shader::IncludeHeader includes({includePath});
                utilities::shader::CompileShader(
                    m_device.Get(), shaderPath, entryPoint, &csBytes, defines, &includes, "cs_5_0");
            } else {
                utilities::shader::CompileShader(
                    m_device.Get(), shaderPath, entryPoint, &csBytes, defines, nullptr, "cs_5_0");
            }

            ComPtr<ID3D11ComputeShader> compiledShader;
            CHECK_HRCMD(m_device->CreateComputeShader(
                csBytes->GetBufferPointer(), csBytes->GetBufferSize(), nullptr, &compiledShader));

            if (debugName) {
                compiledShader->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)debugName->size(), debugName->c_str());
            }

            return std::make_shared<D3D11ComputeShader>(shared_from_this(), compiledShader.Get(), threadGroups);
        }

        std::shared_ptr<IGpuTimer> createTimer() override {
            return std::make_shared<D3D11GpuTimer>(shared_from_this());
        }

        void setShader(std::shared_ptr<IQuadShader> shader) override {
            m_currentQuadShader.reset();
            m_currentComputeShader.reset();
            m_currentShaderHighestSRV = m_currentShaderHighestUAV = m_currentShaderHighestRTV = 0;

            // Prepare to draw the quad.
            m_context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
            m_context->OMSetDepthStencilState(nullptr, 0);
            m_context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
            m_context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
            m_context->IASetInputLayout(nullptr);
            m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            m_context->VSSetShader(m_quadVertexShader.Get(), nullptr, 0);

            // TODO: This is somewhat restrictive, but for now we only support a linear sampler in slot 0.
            ID3D11SamplerState* samp[] = {m_linearClampSamplerPS.Get()};
            m_context->PSSetSamplers(0, 1, samp);
            m_context->PSSetShader(shader->getNative<D3D11>(), nullptr, 0);

            m_currentQuadShader = shader;
        }

        void setShader(std::shared_ptr<IComputeShader> shader) override {
            m_currentQuadShader.reset();
            m_currentComputeShader.reset();
            m_currentShaderHighestSRV = m_currentShaderHighestUAV = m_currentShaderHighestRTV = 0;

            // TODO: This is somewhat restrictive, but for now we only support a linear sampler in slot 0.
            ID3D11SamplerState* samp[] = {m_linearClampSamplerCS.Get()};
            m_context->CSSetSamplers(0, 1, samp);

            m_context->CSSetShader(shader->getNative<D3D11>(), nullptr, 0);

            m_currentComputeShader = shader;
        }

        void setShaderInput(uint32_t slot, std::shared_ptr<ITexture> input, int32_t slice) override {
            ID3D11ShaderResourceView* srvs[] = {slice == -1 ? input->getShaderInputView()->getNative<D3D11>()
                                                            : input->getShaderInputView(slice)->getNative<D3D11>()};
            if (m_currentQuadShader) {
                m_context->PSSetShaderResources(slot, 1, srvs);
            } else if (m_currentComputeShader) {
                m_context->CSSetShaderResources(slot, 1, srvs);
            } else {
                throw std::runtime_error("No shader is set");
            }
            m_currentShaderHighestSRV = max(m_currentShaderHighestSRV, slot);
        }

        void setShaderInput(uint32_t slot, std::shared_ptr<IShaderBuffer> input) override {
            ID3D11Buffer* cbs[] = {input->getNative<D3D11>()};
            if (m_currentQuadShader) {
                m_context->PSSetConstantBuffers(slot, 1, cbs);
            } else if (m_currentComputeShader) {
                m_context->CSSetConstantBuffers(slot, 1, cbs);
            } else {
                throw std::runtime_error("No shader is set");
            }
        }

        void setShaderOutput(uint32_t slot, std::shared_ptr<ITexture> output, int32_t slice) override {
            if (m_currentQuadShader) {
                if (slot) {
                    throw new std::runtime_error("Only use slot 0 for IQuadShader");
                }
                if (slice == -1) {
                    setRenderTargets({output}, nullptr);
                } else {
                    std::vector<std::pair<std::shared_ptr<ITexture>, int32_t>> rtvs;
                    rtvs.push_back(std::make_pair(output, slice));
                    setRenderTargets(rtvs, nullptr);
                }

                D3D11_VIEWPORT viewport;
                ZeroMemory(&viewport, sizeof(viewport));
                viewport.TopLeftX = 0.0f;
                viewport.TopLeftY = 0.0f;
                viewport.Width = (float)output->getInfo().width;
                viewport.Height = (float)output->getInfo().height;
                m_context->RSSetViewports(1, &viewport);
                m_context->RSSetState(output->getInfo().sampleCount > 1 ? m_quadRasterizerMSAA.Get()
                                                                        : m_quadRasterizer.Get());
                m_currentShaderHighestRTV = max(m_currentShaderHighestRTV, slot);

            } else if (m_currentComputeShader) {
                ID3D11UnorderedAccessView* uavs[] = {
                    slice == -1 ? output->getComputeShaderOutputView()->getNative<D3D11>()
                                : output->getComputeShaderOutputView(slice)->getNative<D3D11>()};
                m_context->CSSetUnorderedAccessViews(slot, 1, uavs, nullptr);
                m_currentShaderHighestUAV = max(m_currentShaderHighestUAV, slot);
            } else {
                throw std::runtime_error("No shader is set");
            }
        }

        void dispatchShader(bool doNotClear) const override {
            if (m_currentQuadShader) {
                m_context->Draw(3, 0);
            } else if (m_currentComputeShader) {
                m_context->Dispatch(m_currentComputeShader->getThreadGroups()[0],
                                    m_currentComputeShader->getThreadGroups()[1],
                                    m_currentComputeShader->getThreadGroups()[2]);
            } else {
                throw std::runtime_error("No shader is set");
            }

            if (!doNotClear) {
                // We must unbind all the resources to avoid D3D debug layer issues.
                {
                    std::vector<ID3D11RenderTargetView*> rtvs;
                    for (unsigned int i = 0; i < m_currentShaderHighestRTV + 1; i++) {
                        rtvs.push_back(nullptr);
                    }

                    m_context->OMSetRenderTargets((UINT)rtvs.size(), rtvs.data(), nullptr);
                    m_currentShaderHighestRTV = 0;
                }
                {
                    std::vector<ID3D11ShaderResourceView*> srvs;
                    for (unsigned int i = 0; i < m_currentShaderHighestSRV + 1; i++) {
                        srvs.push_back(nullptr);
                    }

                    if (m_currentQuadShader) {
                        m_context->PSSetShaderResources(0, (UINT)srvs.size(), srvs.data());
                    } else {
                        m_context->CSSetShaderResources(0, (UINT)srvs.size(), srvs.data());
                    }
                    m_currentShaderHighestSRV = 0;
                }
                {
                    std::vector<ID3D11UnorderedAccessView*> uavs;
                    for (unsigned int i = 0; i < m_currentShaderHighestRTV + 1; i++) {
                        uavs.push_back(nullptr);
                    }

                    m_context->CSSetUnorderedAccessViews(0, (UINT)uavs.size(), uavs.data(), nullptr);
                    m_currentShaderHighestUAV = 0;
                }
                m_currentQuadShader.reset();
                m_currentComputeShader.reset();
            }
        }

        void clearRenderTargets() override {
            std::vector<ID3D11RenderTargetView*> rtvs;

            for (int i = 0; i < 8; i++) {
                rtvs.push_back(nullptr);
            }

            m_context->OMSetRenderTargets((UINT)rtvs.size(), rtvs.data(), nullptr);

            m_currentDrawRenderTarget.reset();
            m_currentDrawDepthBuffer.reset();
            m_currentMesh.reset();
        }

        void setRenderTargets(std::vector<std::shared_ptr<ITexture>> renderTargets,
                              std::shared_ptr<ITexture> depthBuffer) override {
            std::vector<ID3D11RenderTargetView*> rtvs;

            for (auto renderTarget : renderTargets) {
                rtvs.push_back(renderTarget->getRenderTargetView()->getNative<D3D11>());
            }

            m_context->OMSetRenderTargets((UINT)rtvs.size(),
                                          rtvs.data(),
                                          depthBuffer ? depthBuffer->getDepthStencilView()->getNative<D3D11>()
                                                      : nullptr);

            m_currentDrawRenderTarget = renderTargets.size() > 0 ? renderTargets[0] : nullptr;
            m_currentDrawDepthBuffer = depthBuffer;
        }

        void setRenderTargets(std::vector<std::pair<std::shared_ptr<ITexture>, int32_t>> renderTargets,
                              std::shared_ptr<ITexture> depthBuffer) override {
            std::vector<ID3D11RenderTargetView*> rtvs;

            for (auto renderTarget : renderTargets) {
                const auto slice = renderTarget.second;

                if (slice == -1) {
                    rtvs.push_back(renderTarget.first->getRenderTargetView()->getNative<D3D11>());
                } else {
                    rtvs.push_back(renderTarget.first->getRenderTargetView(slice)->getNative<D3D11>());
                }
            }
            m_context->OMSetRenderTargets((UINT)rtvs.size(),
                                          rtvs.data(),
                                          depthBuffer ? depthBuffer->getDepthStencilView()->getNative<D3D11>()
                                                      : nullptr);

            m_currentDrawRenderTarget = renderTargets.size() > 0 ? renderTargets[0].first : nullptr;
            m_currentDrawDepthBuffer = depthBuffer;
        }

        void setViewProjection(XrPosef& eyePose, XrFovf& fov, float depthNear, float depthFar) override {
            xr::math::NearFar nearFar{depthNear, depthFar};
            const DirectX::XMMATRIX projection = xr::math::ComposeProjectionMatrix(fov, nearFar);
            const DirectX::XMMATRIX view = xr::math::LoadInvertedXrPose(eyePose);

            ViewProjectionConstantBuffer staging;
            DirectX::XMStoreFloat4x4(&staging.ViewProjection, DirectX::XMMatrixTranspose(view * projection));
            if (!m_meshViewProjectionBuffer) {
                m_meshViewProjectionBuffer =
                    createBuffer(sizeof(ViewProjectionConstantBuffer), "ViewProjection CB", nullptr, false);
            }
            m_meshViewProjectionBuffer->uploadData(&staging, sizeof(staging));

            D3D11_VIEWPORT viewport;
            ZeroMemory(&viewport, sizeof(viewport));
            viewport.TopLeftX = 0.0f;
            viewport.TopLeftY = 0.0f;
            viewport.Width = (float)m_currentDrawRenderTarget->getInfo().width;
            viewport.Height = (float)m_currentDrawRenderTarget->getInfo().height;
            m_context->RSSetViewports(1, &viewport);
            m_context->OMSetDepthStencilState(depthNear > depthFar ? m_reversedZDepthNoStencilTest.Get() : nullptr, 0);
        }

        void draw(std::shared_ptr<ISimpleMesh> mesh, XrPosef& pose, XrVector3f scaling) override {
            auto meshData = mesh->getNative<D3D11>();

            if (mesh != m_currentMesh) {
                if (!m_meshModelBuffer) {
                    m_meshModelBuffer = createBuffer(sizeof(ModelConstantBuffer), "Model CB", nullptr, false);
                }
                ID3D11Buffer* const constantBuffers[] = {m_meshModelBuffer->getNative<D3D11>(),
                                                         m_meshViewProjectionBuffer->getNative<D3D11>()};
                m_context->VSSetConstantBuffers(0, (UINT)std::size(constantBuffers), constantBuffers);
                m_context->VSSetShader(m_meshVertexShader.Get(), nullptr, 0);
                m_context->PSSetShader(m_meshPixelShader.Get(), nullptr, 0);
                m_context->GSSetShader(nullptr, nullptr, 0);

                const UINT strides[] = {meshData->stride};
                const UINT offsets[] = {0};
                ID3D11Buffer* vertexBuffers[] = {meshData->vertexBuffer};
                m_context->IASetVertexBuffers(0, (UINT)std::size(vertexBuffers), vertexBuffers, strides, offsets);
                m_context->IASetIndexBuffer(meshData->indexBuffer, DXGI_FORMAT_R16_UINT, 0);
                m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                m_context->IASetInputLayout(m_meshInputLayout.Get());
            }

            ModelConstantBuffer model;
            const DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScaling(scaling.x, scaling.y, scaling.z);
            DirectX::XMStoreFloat4x4(&model.Model,
                                     DirectX::XMMatrixTranspose(scaleMatrix * xr::math::LoadXrPose(pose)));
            m_meshModelBuffer->uploadData(&model, sizeof(model));

            m_context->DrawIndexedInstanced(meshData->numIndices, 1, 0, 0, 0);
        }

        void* getNativePtr() const override {
            return m_device.Get();
        }

        void* getContextPtr() const override {
            return m_context.Get();
        }

      private:
        const ComPtr<ID3D11Device> m_device;
        ComPtr<ID3D11DeviceContext> m_context;
        std::string m_deviceName;

        ComPtr<ID3D11SamplerState> m_linearClampSamplerPS;
        ComPtr<ID3D11SamplerState> m_linearClampSamplerCS;
        ComPtr<ID3D11RasterizerState> m_quadRasterizer;
        ComPtr<ID3D11RasterizerState> m_quadRasterizerMSAA;
        ComPtr<ID3D11VertexShader> m_quadVertexShader;
        ComPtr<ID3D11DepthStencilState> m_reversedZDepthNoStencilTest;
        ComPtr<ID3D11VertexShader> m_meshVertexShader;
        ComPtr<ID3D11PixelShader> m_meshPixelShader;
        ComPtr<ID3D11InputLayout> m_meshInputLayout;
        std::shared_ptr<IShaderBuffer> m_meshViewProjectionBuffer;
        std::shared_ptr<IShaderBuffer> m_meshModelBuffer;

        std::shared_ptr<ITexture> m_currentDrawRenderTarget;
        std::shared_ptr<ITexture> m_currentDrawDepthBuffer;
        std::shared_ptr<ISimpleMesh> m_currentMesh;
        mutable std::shared_ptr<IQuadShader> m_currentQuadShader;
        mutable std::shared_ptr<IComputeShader> m_currentComputeShader;
        mutable uint32_t m_currentShaderHighestSRV;
        mutable uint32_t m_currentShaderHighestUAV;
        mutable uint32_t m_currentShaderHighestRTV;
    };

} // namespace

namespace toolkit::graphics {
    std::shared_ptr<IDevice> WrapD3D11Device(ID3D11Device* device) {
        return std::make_shared<D3D11Device>(device);
    }

    std::shared_ptr<ITexture> WrapD3D11Texture(std::shared_ptr<IDevice> device,
                                               const XrSwapchainCreateInfo& info,
                                               ID3D11Texture2D* texture,
                                               const std::optional<std::string>& debugName) {
        if (device->getApi() != Api::D3D11) {
            throw new std::runtime_error("Not a D3D11 device");
        }

        if (debugName) {
            texture->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)debugName->size(), debugName->c_str());
        }

        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);
        return std::make_shared<D3D11Texture>(device, info, desc, texture);
    }

} // namespace toolkit::graphics
