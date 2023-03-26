// MIT License
//
// Copyright(c) 2021 Matthieu Bucchianeri
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

// clang-format off


cbuffer config : register(b0) {
    float4 Params1; // ChromaticCorrectionR, ChromaticCorrectionG, ChromaticCorrectionB (-1..+1 params), Eye (0 = left,
                    // 1 = right)
};

SamplerState sourceSampler : register(s0);

Texture2D sourceTexture : register(t0);
#define SAMPLE_TEXTURE(texcoord) sourceTexture.Sample(sourceSampler, (texcoord))

float4 main(in float4 position : SV_POSITION, in float2 texcoord : TEXCOORD0) : SV_TARGET {
    float3 color;
    float2 correctionOrigin = float2(0.313, 0.42);

    if (Params1.w) {
        correctionOrigin.x = 1 - correctionOrigin.x;
    }

    const float2 uvr = ((texcoord - correctionOrigin) * Params1.r) + correctionOrigin;
    color.r = SAMPLE_TEXTURE(uvr).r;

    const float2 uvg = ((texcoord - correctionOrigin) * Params1.g) + correctionOrigin;
    color.g = SAMPLE_TEXTURE(uvg).g;

    const float2 uvb = ((texcoord - correctionOrigin) * Params1.b) + correctionOrigin;
    color.b = SAMPLE_TEXTURE(uvb).b;

    return float4(color, 1.0);
}
