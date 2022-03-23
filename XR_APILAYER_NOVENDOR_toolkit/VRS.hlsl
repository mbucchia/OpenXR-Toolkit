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

// combine EASU and RCAS constants in a single buffer
cbuffer cb : register(b0)
{
  float4 Gaze;   // u,v,1/w,1/h
  float4 Ring12; // sx1,sy1,sx2,sy2
  float4 Ring34; // sx3,sy3,sx4,sy4
  int Rates[4];  // r1,r2,r3,r4
};

RWTexture2D<uint> u_Output : register(u0);

[numthreads(16, 16, 1)]
void mainCS(in int2 pos : SV_DispatchThreadID)
{
  // screen space (w,h) to screen uv (0,1)
  float2 posUV = (pos << 4) * Gaze.zw;
  
  // relative screen uv to gaze in (-1,+1) 
  float2 ndcUV = 2.0f * (posUV - Gaze.xy) - 1.0f;
  
  // Render 3 ellipses with their shading rates
  // Equation: x^2 / a^2 + y^2 / b^2 == 1
  // https://www.desmos.com/calculator/tevuazt8xl

  static const int kNumRates = 3;

  // Numerators (x^2, y^2)
  float2 ndcSQ = ndcUV * 2.0f;

  // Denominators (a^2, b^2)
  float2 shadingRings[kNumRates] =
  {
    1.0f / (Ring12.xy * Ring12.xy),
    1.0f / (Ring12.zw * Ring12.zw),
    1.0f / (Ring34.xy * Ring34.xy)
  };

  int i = 0;
  for (i = 0; i < kNumRates; i++) {
    if (dot(ndcSQ, shadingRings[i]) <= 1) {
      u_Output[pos] = Rates[i];
      break;
    }
  }

  if (i == kNumRates)
    u_Output[pos] = 0; // cull
}

