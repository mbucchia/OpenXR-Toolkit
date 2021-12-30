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

#include "postprocess.h"

cbuffer config : register(b0) {
    POST_PROCESS_CONFIG;
};
SamplerState samplerLinearClamp : register(s0);

#ifndef VPRT
#define SAMPLE_TEXTURE(source, texcoord) sourceTexture.Sample(samplerLinearClamp, (texcoord))
Texture2D sourceTexture : register(t0);
#else
#define SAMPLE_TEXTURE(source, texcoord) sourceTexture.Sample(samplerLinearClamp, float3((texcoord), 0))
Texture2DArray sourceTexture : register(t0);
#endif

// For now, our shader only does a copy, effectively allowing Direct3D to convert between color formats.
float4 main(in float4 position : SV_POSITION, in float2 texcoord : TEXCOORD0) : SV_TARGET {
    return SAMPLE_TEXTURE(sourceTexture, texcoord);
}
