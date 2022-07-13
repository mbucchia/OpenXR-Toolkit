// MIT License
//
// Variable Rate Shading Compute Shader
// Copyright(c) 2021-2022 Jean-Luc Dupiot - Reality XP
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// clang-format off

#ifndef VRS_TILE_X
#define VRS_TILE_X 16
#endif

#ifndef VRS_TILE_Y
#define VRS_TILE_Y 16
#endif

#ifndef VRS_NUM_RATES
#define VRS_NUM_RATES 4
#endif

#ifndef VRS_DEFAULT_RATE
#define VRS_DEFAULT_RATE 0
#endif

#ifndef VRS_USE_DIM_RATIO
#define VRS_USE_DIM_RATIO 0
#endif

// Render up to 4 ellipses with their shading rates
// Equation: x^2 / a^2 + y^2 / b^2 == 1
// https://www.desmos.com/calculator/tevuazt8xl

cbuffer cb : register(b0)
{
  float4 Gaze;    // ndc_x, ndc_y, 1/w, 1/h
  float4 Rings12; // 1/(a1^2), 1/(b1^2), 1/(a2^2), 1/(b2^2)
  float4 Rings34; // 1/(a3^2), 1/(b3^2), 1/(a4^2), 1/(b4^2)
  uint4  Rates;   // r1, r2, r3, r4
};

RWTexture2D<uint> u_Output : register(u0);

float2 ScreenToNdc(float2 uv, float2 center) {
  const float2 pos_ndc = uv * float2(2.0f,-2.0f) + float2(-1.0f,+1.0f); // uv to ndc (y flip)
  return pos_ndc - center; // ndc to gaze ndc;
}

float2 ScreenToGazeRing(float2 uv) {
#if VRS_USE_DIM_RATIO
  // adjust ellipse scale with texture scale ratio
  // (w > h) ? scale x by w/h : scale y by h/w 
  // (1/w < 1/h) ? scale x by (1/h)/(1/w) : scale y by (1/w)/(1/h)
  const float2 scale = (Gaze.z < Gaze.w) ? float2(Gaze.w / Gaze.z, 1.0f) : float2(1.0f, Gaze.z / Gaze.w);
  const float2 pos_xy = ScreenToNdc(uv, Gaze.xy) * scale;
#else
  const float2 pos_xy = ScreenToNdc(uv, Gaze.xy);
#endif
  return pos_xy * pos_xy;
}

[numthreads(VRS_NUM_THREADS_X, VRS_NUM_THREADS_Y, 1)]
void mainCS(in int2 pos : SV_DispatchThreadID) {
  // screen space (w,h) to uv (0,1)
  const float2 pos_uv = (pos + 0.5f) * Gaze.zw;

  // uv to ring ndc (xy^2)
  const float2 pos_xy = ScreenToGazeRing(pos_uv);

  uint rate;
  if      (dot(pos_xy, Rings12.xy) <= 1.0f) rate = Rates.x;
#if VRS_NUM_RATES >= 2
  else if (dot(pos_xy, Rings12.zw) <= 1.0f) rate = Rates.y;
#if VRS_NUM_RATES >= 3
  else if (dot(pos_xy, Rings34.xy) <= 1.0f) rate = Rates.z;
#if VRS_NUM_RATES >= 4
  else if (dot(pos_xy, Rings34.zw) <= 1.0f) rate = Rates.w;
#endif
#endif
#endif
  else rate = VRS_DEFAULT_RATE;

  u_Output[pos] = rate;
}

// clang-format on
