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

#ifdef VPRT
Texture2DArray sourceTexture : register(t0);
#else
Texture2D sourceTexture : register(t0);
#endif

#ifdef POST_PROCESS_SAMPLE_LINEAR
#ifdef VPRT
#define SAMPLE_TEXTURE(source, texcoord) source.Sample(samplerLinearClamp, float3((texcoord), 0)).rgb
#else
#define SAMPLE_TEXTURE(source, texcoord) source.Sample(samplerLinearClamp, (texcoord)).rgb
#endif
#else
#ifdef VPRT
float3 SAMPLE_TEXTURE(Texture2DArray source, float2 texcoord) {
  float3 dims;
  source.GetDimensions(dims.x, dims.y, dims.z);
  return sourceTexture.Load(int4(texcoord * dims.xy + 0.5, 0, 0)).rgb;
}
#else
float3 SAMPLE_TEXTURE(Texture2D source, float2 texcoord) {
  float2 dims;
  source.GetDimensions(dims.x, dims.y);
  return sourceTexture.Load(int3(texcoord * dims.xy + 0.5, 0)).rgb;
}
#endif
#endif

// http://www.martinreddy.net/gfx/faqs/colorconv.faq

float3 sRGB_to_YUV(float3 col) {
  return col.r * float3(0.299,-0.147, 0.615) +
         col.g * float3(0.587,-0.289,-0.515) +
         col.b * float3(0.114, 0.436,-0.100);
}

float3 YUV_to_sRGB(float3 yuv) {
  return yuv.r +
         yuv.g * float3(0.0 , -0.396, 2.029) +
         yuv.b * float3(1.14, -0.581, 0.0  );
}

float3 linear_to_YUV(float3 col) {
  return col.r * float3(0.431, 0.222, 0.020) +
         col.g * float3(0.342, 0.707, 0.130) +
         col.b * float3(0.178, 0.071, 0.939);
}

float3 YUV_to_linear(float3 yuv) {
  return yuv.r * float3( 3.063,-3.969, 0.068) +
         yuv.g * float3(-1.393, 1.876, 0.229) +
         yuv.b * float3( 0.476, 0.042, 1.069);
}

float3 RGB_to_BT601(float3 col) {
  return col.r * float3(0.299,-0.169, 0.500) +
         col.g * float3(0.587,-0.331,-0.419) +
         col.b * float3(0.114, 0.500,-0.081);
}

float3 BT601_to_RGB(float3 yuv) {
  return yuv.r +
         yuv.g * float3(0.0  ,-0.344, 1.773) +
         yuv.b * float3(1.403,-0.714, 0.0  );
}

#ifndef FLT_EPSILON
#define FLT_EPSILON     1.192092896e-07
#endif

#ifdef POST_PROCESS_SRC_SRGB
float3 srgb2linear(float3 c ) {
  //return pow(c, 2.2);
  return saturate(c*c);
}
#endif

#ifdef POST_PROCESS_DST_SRGB
float3 linear2srgb(float3 c) {
  //return pow(c, 1.0/2.2);
  return sqrt(c);
}
#endif

float3 SafePow(float3 value, float3 power) {
  return pow(max(abs(value), FLT_EPSILON), power);
}
#if 0
float Smoothstep(float x, float p) {
    x = saturate(x);
    float x_ = x < 0.5 ? x * 2.0 : x * -2.0 + 2.0;
    float y = pow(x_, p);
    return x < 0.5 ? y * 0.5 : y * -0.5 + 1.0;
}
#endif

// -1..+1 (better: +- 0.1)
float3 AdjustContrast(float3 color, float scale) {
  return saturate(((color - 0.5) * (1.0 + scale)) + 0.5);
}

// -1..+1 (better: +- 0.8)
float3 AdjustBrightness(float3 color, float scale) {
  return SafePow(color, (1.0 - scale));
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
  return lerp(luminance, color, amount);
}

// 0..1 (https://www.desmos.com/calculator/wmiuegrnli)
float3 AdjustHighlightsShadows(float3 color, float highlights, float shadows) {
  float luma = dot(saturate(color), float3(0.3,0.3,0.3));
  float h = 1.0 - SafePow((1.0 - luma), 1.0/(2.0-highlights));
  float s = SafePow(luma, 1.0/(1.0+shadows));
  return (color/luma) * (h + s - luma);
}

// For now, our shader only does a copy, effectively allowing Direct3D to convert between color formats.
float4 mainPostProcess(in float4 position : SV_POSITION, in float2 texcoord : TEXCOORD0) : SV_TARGET {
#if 0
    float4 inputColor = SAMPLE_TEXTURE(sourceTexture, texcoord);
    float4 outputColor = mul(BrightnessContrastSaturationMatrix, inputColor);
    outputColor.a = inputColor.a;
    return outputColor;
#endif
#if 1

  float3 color = SAMPLE_TEXTURE(sourceTexture, texcoord);

  color = AdjustContrast(color, Scale.x);

#ifdef POST_PROCESS_SRC_SRGB
  color = srgb2linear(color);
 #endif

  color = AdjustBrightness(color, Scale.y);
  color = AdjustExposure(color, Scale.z);
  color = AdjustSaturation(color, Amount.x);
  color = AdjustHighlightsShadows(color, Amount.y, Amount.z);

#ifdef POST_PROCESS_DST_SRGB
  color = linear2srgb(color);
#endif

  color = AdjustVibrance(color, Scale.w);
  return float4(saturate(color), 1.0);
#endif
}

// clang-format on
