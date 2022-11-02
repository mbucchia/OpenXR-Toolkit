// CAS Sample
//
// Copyright(c) 2019 Advanced Micro Devices, Inc.All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

cbuffer cb : register(b0) {
    uint4 const0;
    uint4 const1;
};

Texture2D InputTexture : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

#define A_GPU 1
#define A_HLSL 1

#if CAS_SAMPLE_FP16

#define A_HALF 1
#define CAS_PACKED_ONLY 1

#endif

#include "ffx_a.h"

#if CAS_SAMPLE_FP16

AH3 CasLoadH(ASW2 p) {
    return InputTexture.Load(ASU3(p, 0)).rgb;
}

// Lets you transform input from the load into a linear color space between 0 and 1. See ffx_cas.h
// In this case, our input is already linear and between 0 and 1
void CasInputH(inout AH2 r, inout AH2 g, inout AH2 b) {
}

#else

AF3 CasLoad(ASU2 p) {
    return InputTexture.Load(int3(p, 0)).rgb;
}

// Lets you transform input from the load into a linear color space between 0 and 1. See ffx_cas.h
// In this case, our input is already linear and between 0 and 1
void CasInput(inout AF1 r, inout AF1 g, inout AF1 b) {
}

#endif

#include "ffx_cas.h"

[numthreads(CAS_THREAD_GROUP_SIZE, 1, 1)]
void mainCS(uint3 LocalThreadId
                                               : SV_GroupThreadID, uint3 WorkGroupId
                                               : SV_GroupID) {
    // Do remapping of local xy in workgroup for a more PS-like swizzle pattern.
    AU2 gxy = ARmp8x8(LocalThreadId.x) + AU2(WorkGroupId.x << 4u, WorkGroupId.y << 4u);

    bool sharpenOnly;
#if CAS_SAMPLE_SHARPEN_ONLY
    sharpenOnly = true;
#else
    sharpenOnly = false;
#endif

#if CAS_SAMPLE_FP16

    // Filter.
    AH4 c0, c1;
    AH2 cR, cG, cB;

    CasFilterH(cR, cG, cB, gxy, const0, const1, sharpenOnly);
    CasDepack(c0, c1, cR, cG, cB);
    OutputTexture[ASU2(gxy)] = AF4(c0);
    OutputTexture[ASU2(gxy) + ASU2(8, 0)] = AF4(c1);
    gxy.y += 8u;

    CasFilterH(cR, cG, cB, gxy, const0, const1, sharpenOnly);
    CasDepack(c0, c1, cR, cG, cB);
    OutputTexture[ASU2(gxy)] = AF4(c0);
    OutputTexture[ASU2(gxy) + ASU2(8, 0)] = AF4(c1);

#else

    // Filter.
    AF3 c;

    CasFilter(c.r, c.g, c.b, gxy, const0, const1, sharpenOnly);
    OutputTexture[ASU2(gxy)] = AF4(c, 1);
    gxy.x += 8u;

    CasFilter(c.r, c.g, c.b, gxy, const0, const1, sharpenOnly);
    OutputTexture[ASU2(gxy)] = AF4(c, 1);
    gxy.y += 8u;

    CasFilter(c.r, c.g, c.b, gxy, const0, const1, sharpenOnly);
    OutputTexture[ASU2(gxy)] = AF4(c, 1);
    gxy.x -= 8u;

    CasFilter(c.r, c.g, c.b, gxy, const0, const1, sharpenOnly);
    OutputTexture[ASU2(gxy)] = AF4(c, 1);

#endif
}
