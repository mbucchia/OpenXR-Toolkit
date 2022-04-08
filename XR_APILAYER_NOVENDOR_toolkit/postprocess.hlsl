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
    //float4x4 BrightnessContrastSaturationMatrix;
    float4 Scale;   // Contrast, Brightness, Exposure, Vibrance (-1..+1 params)
    float4 Amount;  // Saturation, Highlights, Shadows (0..1 params)
};
SamplerState samplerLinearClamp : register(s0);

#ifndef VPRT
#define SAMPLE_TEXTURE(source, texcoord) source.Sample(samplerLinearClamp, (texcoord))
Texture2D sourceTexture : register(t0);
#else
#define SAMPLE_TEXTURE(source, texcoord) source.Sample(samplerLinearClamp, float3((texcoord), 0))
Texture2DArray sourceTexture : register(t0);
#endif

#if 0
float3 rgb2yuv(float3 col) {
  return col.r * float3(0.299,-0.147, 0.615) + col.g * float3(0.587,-0.289,-0.515) + col.b * float3(0.114, 0.436,-0.100);
}

float3 yuv2rgb(float3 yuv) {
  return float3(yuv.r,yuv.r,yuv.r) + yuv.g * float3(0.0, -0.395, 2.032) + yuv.b * float3(1.14, -0.581, 0.0);
}

float3 ColorAdjustment(float3 col, float2 offset, float lift, float gain, float contrast, float saturation, float gamma) {
  float3 yuv = rgb2yuv(col);
  yuv.gb *= saturation;
  yuv.gb += lerp(0.0, offset, yuv.r);
  col = yuv2rgb(yuv);

  col = lerp( col, col * 2.0 - 0.5, contrast );
  //col = saturate(col);

  col = (col + lift) * gain;
  //col = saturate(col);
    
  col = pow(col, gamma);
  return col;
}
#endif

// -1..+1
float3 AdjustContrast(float3 color, float scale) {
  return (color - 0.5) * (scale + 1.0) + 0.5;
}

// -1..+1
float3 AdjustBrightness(float3 color, float scale) {
  return pow(color, 1.0 - scale);
}

// -1..+1 (better: +-4)
float3 AdjustExposure(float3 color, float scale) {
  return color * pow(2.0, scale);
}

// -1..+1 (better: +-3)
float3 AdjustVibrance(float3 color, float scale) {
  float average = (color.r + color.g + color.b) / 3.0;
  float highest = max(color.r, max(color.g, color.b));
  float amount = (average - highest) * scale;
  return lerp(color, highest, amount);
}

// 0..1
float3 AdjustSaturation(float3 color, float amount) {
  float luminance = dot(saturate(color), float3(0.2125,0.7154,0.0721));
  return lerp(float3(luminance,luminance,luminance), color, amount);
}

// 0..1 (https://www.desmos.com/calculator/wmiuegrnli)
float3 AdjustHighlightsShadows(float3 color, float highlights, float shadows) {
  float luma = dot(saturate(color), float3(0.3,0.3,0.3));
  float h = 1.0 - pow((1.0 - luma), 1.0/(2.0-highlights));
  float s = pow(luma, 1.0/(1.0+shadows));
  return (color/luma) * (h + s - luma);
}

// For now, our shader only does a copy, effectively allowing Direct3D to convert between color formats.
float4 main(in float4 position : SV_POSITION, in float2 texcoord : TEXCOORD0) : SV_TARGET {
#if 0
    float4 inputColor = SAMPLE_TEXTURE(sourceTexture, texcoord);
    float4 outputColor = mul(BrightnessContrastSaturationMatrix, inputColor);
    outputColor.a = inputColor.a;
    return outputColor;
#endif
#if 1
  float3 col = SAMPLE_TEXTURE(sourceTexture, texcoord).rgb;
  col = AdjustContrast(col, Scale.x);
  col = AdjustBrightness(col, Scale.y);
  col = AdjustExposure(col, Scale.z);
  col = AdjustSaturation(col, Amount.x);
  col = AdjustVibrance(col, Scale.w);
  col = AdjustHighlightsShadows(col, Amount.y, Amount.z);
  return float4(col, 1.0);
#endif
}

// clang-format on
