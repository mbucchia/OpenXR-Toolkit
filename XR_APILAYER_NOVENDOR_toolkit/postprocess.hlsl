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
    float4 Params1;  // Contrast, Brightness, Exposure, Saturation (-1..+1 params)
    float4 Params2;  // ColorGainR, ColorGainG, ColorGainB (-1..+1 params)
    float4 Params3;  // Highlights, Shadows, Vibrance (0..1 params)
    float4 Params4;  // Gaze.xy, Ring.zw (anti-flicker)
    float4 Rings12;  // 1/(a1^2), 1/(b1^2), 1/(a2^2), 1/(b2^2)
    float4 Rings34;  // 1/(a3^2), 1/(b3^2), 1/(a4^2), 1/(b4^2)
};

SamplerState sourceSampler : register(s0);

#ifdef VPRT
Texture2DArray sourceTexture : register(t0);
#define SAMPLE_TEXTURE(texcoord) sourceTexture.Sample(sourceSampler, float3((texcoord), 0))
#else
Texture2D sourceTexture : register(t0);
#define SAMPLE_TEXTURE(texcoord) sourceTexture.Sample(sourceSampler, (texcoord))
#endif

#ifndef FLT_EPSILON
#define FLT_EPSILON     1.192092896e-07
#endif

#ifndef VRS_NUM_RATES
#define VRS_NUM_RATES 4
#endif

#if 0
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

float Smoothstep(float x, float p) {
    x = saturate(x);
    float x_ = x < 0.5 ? x * 2.0 : x * -2.0 + 2.0;
    float y = pow(x_, p);
    return x < 0.5 ? y * 0.5 : y * -0.5 + 1.0;
}
#endif

float3 srgb2linear(float3 c ) {
  //return pow(c, 2.2);
  return c*c; // fast aproximation
}
float3 linear2srgb(float3 c) {
  //return pow(c, 1.0/2.2);
  return sqrt(c); // fast aproximation
}

float SafePow(float value, float power) {
  return pow(max(abs(value), FLT_EPSILON), power);
}
float3 SafePow(float3 value, float3 power) {
  return pow(max(abs(value), FLT_EPSILON), power);
}

// -1..+1
float3 AdjustContrast(float3 color, float scale) {
  float luminance = dot(saturate(color), float3(0.2125, 0.7154, 0.0721));
  float contrast = luminance * luminance * (3.0 - 2.0 * luminance); // smoothstep
  contrast = lerp(luminance, contrast, scale);
  return max(color + contrast - luminance, 0.0);
}

// -1..+1 (better: +- 0.8)
float3 AdjustBrightness(float3 color, float scale) {
  return SafePow(color, (1.0 - scale));
}

// -1..+1 (better: +-3 F-stops)
float3 AdjustExposure(float3 color, float scale) {
  return color * pow(2.0, scale);
}

// -1..+1 (better: +-4 F-stops)
float3 AdjustExposureToneMap(float3 color, float scale) {
  color = -(color / min(color - 1.0, -0.1)); // color /= exp(0);
  color*= exp(scale); // inverse + forward Reinhard tone mapping
  return color / (1.0 + color);
}

// 0..+1
float3 AdjustVibrance(float3 color, float scale) {
  float average = (color.r + color.g + color.b) / 3.0;
  float highest = max(color.r, max(color.g, color.b));
  float amount = (average - highest) * scale;
  return lerp(color, highest, amount);
}

// -1..+1
float3 AdjustSaturation(float3 color, float amount) {
  float luminance = dot(saturate(color), float3(0.2125, 0.7154, 0.0721));
  return luminance + (color - luminance) * (amount + 1.0);
}

// -1..+1
float3 AdjustGains(float3 color, float3 gains) {
  return saturate(color * (gains + 1));
}

// 0..1 (https://www.desmos.com/calculator/wmiuegrnli)
float3 AdjustHighlightsShadows(float3 color, float2 amount) {
  float2 inv_hs = rcp(amount + 1.0);
  float luma = dot(saturate(color), float3(0.3,0.3,0.3));
  float h = 1.0 - SafePow((1.0 - luma), inv_hs.x); // highlights
  float s = SafePow(luma, inv_hs.y); // shadows
  return (color/luma) * (h + s - luma);
}

#if VRS_NUM_RATES >= 1
float3 AdjustRingColor(float3 color, float2 pos_uv, float blend) {
  //uint w,h; sourceTexture.GetDimensions(w, h);
  // TODO: align by multiple of 16 

  float2 pos_xy = float2(2.0f,-2.0f) * pos_uv + float2(-1.0f,+1.0f); // uv to ndc (y flip)
  pos_xy -= Params4.xy; // ndc to gaze ndc
  pos_xy *= pos_xy;

  float3 ringColor = float3(0,0,0);
  if      (dot(pos_xy, Rings12.xy) <= 1.0f) ringColor = float3(1.0f, 0.0f, 0.0f);
#if VRS_NUM_RATES >= 2
  else if (dot(pos_xy, Rings12.zw) <= 1.0f) ringColor = float3(1.0f, 1.0f, 0.0f);
#if VRS_NUM_RATES >= 3
  else if (dot(pos_xy, Rings34.xy) <= 1.0f) ringColor = float3(0.0f, 1.0f, 0.0f);
#if VRS_NUM_RATES >= 4
  else if (dot(pos_xy, Rings34.zw) <= 1.0f) ringColor = float3(0.0f, 1.0f, 1.0f);
#endif
#endif
#endif
  return lerp(color, ringColor, blend);
}
#endif


// For now, our shader only does a copy, effectively allowing Direct3D to convert between color formats.
float4 mainPostProcess(in float4 position : SV_POSITION, in float2 texcoord : TEXCOORD0) : SV_TARGET {
  float3 color = SAMPLE_TEXTURE(texcoord).rgb;

  if (!any(color)) {
    //discard;
    return float4(1,0,0,1);
  }

#ifdef POST_PROCESS_SRC_SRGB
  color = srgb2linear(color);
#endif
  
  // adjust color input gains.
  if (any(Params2.rgb)) {
    color = AdjustGains(color, Params2.rgb);
  }
  // adjust lighting and saturation.
  if (any(Params1)) {
    color = AdjustContrast(color, Params1.x);
    color = AdjustBrightness(color, Params1.y);
    color = AdjustExposure(color, Params1.z);
    color = AdjustSaturation(color, Params1.w);
  }
  // boost colors
  if (any(Params3.z)) {
    color = AdjustVibrance(color, Params3.z);
  }
  // expand/crush luma for output.
  if (any(Params3.xy)) {
    color = AdjustHighlightsShadows(color, Params3.xy);
  }

#if VRS_NUM_RATES >= 1
  if (any(Rings34.zw)) {
    color = AdjustRingColor(color, texcoord, 0.1);
  }

  //if (any(Params4.zw)) {
    // Anti-Flicker
    //if (dot(pos_xy * pos_xy, Params4.zw) >= 1.0f) {
    //  color = BlendScreen(color, float3(0.0,0.05,0.0));
    //}
  //}
#endif

#ifdef POST_PROCESS_DST_SRGB
  color = linear2srgb(saturate(color));
#else
  color = saturate(color);
#endif

  return float4(color, 1.0);
}

float4 mainPassThrough(in float4 position : SV_POSITION, in float2 texcoord : TEXCOORD0) : SV_TARGET {
  float3 color = SAMPLE_TEXTURE(texcoord).rgb;

  if (!any(color)) {
    //discard;
    return float4(1,0,0,1);
  }

#ifdef PASS_THROUGH_USE_GAINS
  // adjust color input gains.
  if (any(Params2.rgb)) {

#ifdef POST_PROCESS_SRC_SRGB
    color = srgb2linear(color);
#endif

    color = AdjustGains(color, Params2.rgb);

#ifdef POST_PROCESS_DST_SRGB
    color = linear2srgb(color);
#endif

  }
#endif // PASS_THROUGH_USE_GAINS

#if VRS_NUM_RATES >= 1
  if (any(Rings34.zw)) {
    color = AdjustRingColor(color, texcoord, 0.1);
  }

  //if (any(Params4.zw)) {
  //  float2 pos_xy = float2(2.0f,-2.0f) * texcoord + float2(-1.0f,+1.0f) - Params4.xy;
  //  if (dot(pos_xy * pos_xy, Params4.zw) >= 1.0f) {
  //    // Green tint for debug
  //    color = BlendScreen(color, float3(0.0,0.05,0.0));
  //  }
  //}
#endif

  return float4(color, 1.0);
}

// clang-format on
