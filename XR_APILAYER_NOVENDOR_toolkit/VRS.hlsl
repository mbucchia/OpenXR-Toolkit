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

// Render up to 4 ellipses with their shading rates
// Equation: x^2 / a^2 + y^2 / b^2 == 1
// https://www.desmos.com/calculator/tevuazt8xl

cbuffer cb : register(b0)
{
  float4 Gaze;   // u,v,1/w,1/h
  float4 Ring12; // a1,b1,a2,b2
  float4 Ring34; // a3,b3,a4,b4
  uint4  Rates;   // r1,r2,r3,r4
};

RWTexture2D<uint> u_Output : register(u0);

[numthreads(VRS_TILE_X, VRS_TILE_Y, 1)]
void mainCS(in int2 pos : SV_DispatchThreadID) {
  // texture space (w,h) to uv (0,1)
  const float2 pos_uv = pos * Gaze.zw;
  
  // uv pos to gaze ndc (-1,+1) 
  const float2 pos_ndc = 2.0f * (pos_uv - Gaze.xy) - 1.0f;

  // adjust ellipse scale with texture scale ratio
  // if (w > h) scale a by h/w == x by w/h == x by (1/h)/(1/w)
  const float2 scale = (Gaze.z < Gaze.w) ? float2(Gaze.w / Gaze.z, 1.0f) : float2(1.0f, Gaze.z / Gaze.w);
  const float2 pos_xy = pos_ndc * scale;

  // Numerators (x^2, y^2)
  const float2 pos_xy2 = pos_xy * pos_xy;

 #if 0
  // Denominators (a^2, b^2)
  const float2 rings_sq[VRS_NUM_RATES] = {
    1.0f / (Ring12.xy * Ring12.xy),
#if VRS_NUM_RATES >= 2
    1.0f / (Ring12.zw * Ring12.zw),
#if VRS_NUM_RATES >= 3
    1.0f / (Ring34.xy * Ring34.xy),
#if VRS_NUM_RATES >= 4
    1.0f / (Ring34.zw * Ring34.zw)
#endif
#endif
#endif
  };

  const uint rates[VRS_NUM_RATES] = {
    Rates.x,
#if VRS_NUM_RATES >= 2
    Rates.y,
#if VRS_NUM_RATES >= 3
    Rates.z,
#if VRS_NUM_RATES >= 4
    Rates.w
#endif
#endif
#endif
  };

  int i = 0;
  for (i = 0; i < VRS_NUM_RATES; i++) {
    if (dot(pos_xy2, rings_sq[i]) <= 1) {
      u_Output[pos] = rates[i];
      break;
    }
  }
  if (i == VRS_NUM_RATES) {
    u_Output[pos] = VRS_DEFAULT_RATE;
  }
#endif

  uint rate;
  if (dot(pos_xy2, rcp(Ring12.xy * Ring12.xy)) <= 1.0f) {
    rate = Rates.x;
  }
#if VRS_NUM_RATES >= 2
  else if (dot(pos_xy2, rcp(Ring12.zw * Ring12.zw)) <= 1.0f) {
    rate = Rates.y;
  }
#if VRS_NUM_RATES >= 3
  else if (dot(pos_xy2, rcp(Ring34.xy * Ring34.xy)) <= 1.0f) {
    rate = Rates.z;
  }
#if VRS_NUM_RATES >= 4
  else if (dot(pos_xy2, rcp(Ring34.zw * Ring34.zw)) <= 1.0f) {
    rate = Rates.w;
  }
#endif
#endif
#endif
  else {
    rate = VRS_DEFAULT_RATE;
  }

  u_Output[pos] = rate;
}

