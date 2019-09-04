/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011, Blender Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of Sony Pictures Imageworks nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

CCL_NAMESPACE_BEGIN

/* **** Perlin Noise **** */

ccl_device float fade(float t)
{
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

ccl_device_inline float negate_if(float val, int condition)
{
  return (condition) ? -val : val;
}

ccl_device float grad1(int hash, float x)
{
  int h = hash & 15;
  float g = 1 + (h & 7);
  return negate_if(g, h & 8) * x;
}

ccl_device_noinline_cpu float perlin_1d(float x)
{
  int X;
  float fx = floorfrac(x, &X);
  float u = fade(fx);

  return mix(grad1(hash_uint(X), fx), grad1(hash_uint(X + 1), fx - 1.0f), u);
}

/* 2D, 3D, and 4D noise can be accelerated using SSE, so we first check if
 * SSE is supported, that is, if __KERNEL_SSE2__ is defined. If it is not
 * supported, we do a standard implementation, but if it is supported, we
 * do an implementation using SSE intrinsics.
 */
#ifndef __KERNEL_SSE2__

/* ** Standard Implementation ** */

/* Bilinear Interpolation:
 *
 * v2          v3
 *  @ + + + + @       y
 *  +         +       ^
 *  +         +       |
 *  +         +       |
 *  @ + + + + @       @------> x
 * v0          v1
 *
 */
ccl_device float bi_mix(float v0, float v1, float v2, float v3, float x, float y)
{
  float x1 = 1.0f - x;
  return (1.0f - y) * (v0 * x1 + v1 * x) + y * (v2 * x1 + v3 * x);
}

/* Trilinear Interpolation:
 *
 *   v6               v7
 *     @ + + + + + + @
 *     +\            +\
 *     + \           + \
 *     +  \          +  \
 *     +   \ v4      +   \ v5
 *     +    @ + + + +++ + @          z
 *     +    +        +    +      y   ^
 *  v2 @ + +++ + + + @ v3 +       \  |
 *      \   +         \   +        \ |
 *       \  +          \  +         \|
 *        \ +           \ +          +---------> x
 *         \+            \+
 *          @ + + + + + + @
 *        v0               v1
 */
ccl_device float tri_mix(float v0,
                         float v1,
                         float v2,
                         float v3,
                         float v4,
                         float v5,
                         float v6,
                         float v7,
                         float x,
                         float y,
                         float z)
{
  float x1 = 1.0f - x;
  float y1 = 1.0f - y;
  float z1 = 1.0f - z;
  return z1 * (y1 * (v0 * x1 + v1 * x) + y * (v2 * x1 + v3 * x)) +
         z * (y1 * (v4 * x1 + v5 * x) + y * (v6 * x1 + v7 * x));
}

ccl_device float quad_mix(float v0,
                          float v1,
                          float v2,
                          float v3,
                          float v4,
                          float v5,
                          float v6,
                          float v7,
                          float v8,
                          float v9,
                          float v10,
                          float v11,
                          float v12,
                          float v13,
                          float v14,
                          float v15,
                          float x,
                          float y,
                          float z,
                          float w)
{
  return mix(tri_mix(v0, v1, v2, v3, v4, v5, v6, v7, x, y, z),
             tri_mix(v8, v9, v10, v11, v12, v13, v14, v15, x, y, z),
             w);
}

ccl_device float grad2(int hash, float x, float y)
{
  int h = hash & 7;
  float u = h < 4 ? x : y;
  float v = 2.0f * (h < 4 ? y : x);
  return negate_if(u, h & 1) + negate_if(v, h & 2);
}

ccl_device float grad3(int hash, float x, float y, float z)
{
  int h = hash & 15;
  float u = h < 8 ? x : y;
  float vt = ((h == 12) || (h == 14)) ? x : z;
  float v = h < 4 ? y : vt;
  return negate_if(u, h & 1) + negate_if(v, h & 2);
}

ccl_device float grad4(int hash, float x, float y, float z, float w)
{
  int h = hash & 31;
  float u = h < 24 ? x : y;
  float v = h < 16 ? y : z;
  float s = h < 8 ? z : w;
  return negate_if(u, h & 1) + negate_if(v, h & 2) + negate_if(s, h & 4);
}

ccl_device_noinline_cpu float perlin_2d(float x, float y)
{
  int X;
  int Y;

  float fx = floorfrac(x, &X);
  float fy = floorfrac(y, &Y);

  float u = fade(fx);
  float v = fade(fy);

  float r = bi_mix(grad2(hash_uint2(X, Y), fx, fy),
                   grad2(hash_uint2(X + 1, Y), fx - 1.0f, fy),
                   grad2(hash_uint2(X, Y + 1), fx, fy - 1.0f),
                   grad2(hash_uint2(X + 1, Y + 1), fx - 1.0f, fy - 1.0f),
                   u,
                   v);

  return r;
}

ccl_device_noinline_cpu float perlin_3d(float x, float y, float z)
{
  int X;
  int Y;
  int Z;

  float fx = floorfrac(x, &X);
  float fy = floorfrac(y, &Y);
  float fz = floorfrac(z, &Z);

  float u = fade(fx);
  float v = fade(fy);
  float w = fade(fz);

  float r = tri_mix(grad3(hash_uint3(X, Y, Z), fx, fy, fz),
                    grad3(hash_uint3(X + 1, Y, Z), fx - 1.0f, fy, fz),
                    grad3(hash_uint3(X, Y + 1, Z), fx, fy - 1.0f, fz),
                    grad3(hash_uint3(X + 1, Y + 1, Z), fx - 1.0f, fy - 1.0f, fz),
                    grad3(hash_uint3(X, Y, Z + 1), fx, fy, fz - 1.0f),
                    grad3(hash_uint3(X + 1, Y, Z + 1), fx - 1.0f, fy, fz - 1.0f),
                    grad3(hash_uint3(X, Y + 1, Z + 1), fx, fy - 1.0f, fz - 1.0f),
                    grad3(hash_uint3(X + 1, Y + 1, Z + 1), fx - 1.0f, fy - 1.0f, fz - 1.0f),
                    u,
                    v,
                    w);
  return r;
}

ccl_device_noinline_cpu float perlin_4d(float x, float y, float z, float w)
{
  int X;
  int Y;
  int Z;
  int W;

  float fx = floorfrac(x, &X);
  float fy = floorfrac(y, &Y);
  float fz = floorfrac(z, &Z);
  float fw = floorfrac(w, &W);

  float u = fade(fx);
  float v = fade(fy);
  float t = fade(fz);
  float s = fade(fw);

  float r = quad_mix(
      grad4(hash_uint4(X, Y, Z, W), fx, fy, fz, fw),
      grad4(hash_uint4(X + 1, Y, Z, W), fx - 1.0f, fy, fz, fw),
      grad4(hash_uint4(X, Y + 1, Z, W), fx, fy - 1.0f, fz, fw),
      grad4(hash_uint4(X + 1, Y + 1, Z, W), fx - 1.0f, fy - 1.0f, fz, fw),
      grad4(hash_uint4(X, Y, Z + 1, W), fx, fy, fz - 1.0f, fw),
      grad4(hash_uint4(X + 1, Y, Z + 1, W), fx - 1.0f, fy, fz - 1.0f, fw),
      grad4(hash_uint4(X, Y + 1, Z + 1, W), fx, fy - 1.0f, fz - 1.0f, fw),
      grad4(hash_uint4(X + 1, Y + 1, Z + 1, W), fx - 1.0f, fy - 1.0f, fz - 1.0f, fw),
      grad4(hash_uint4(X, Y, Z, W + 1), fx, fy, fz, fw - 1.0f),
      grad4(hash_uint4(X + 1, Y, Z, W + 1), fx - 1.0f, fy, fz, fw - 1.0f),
      grad4(hash_uint4(X, Y + 1, Z, W + 1), fx, fy - 1.0f, fz, fw - 1.0f),
      grad4(hash_uint4(X + 1, Y + 1, Z, W + 1), fx - 1.0f, fy - 1.0f, fz, fw - 1.0f),
      grad4(hash_uint4(X, Y, Z + 1, W + 1), fx, fy, fz - 1.0f, fw - 1.0f),
      grad4(hash_uint4(X + 1, Y, Z + 1, W + 1), fx - 1.0f, fy, fz - 1.0f, fw - 1.0f),
      grad4(hash_uint4(X, Y + 1, Z + 1, W + 1), fx, fy - 1.0f, fz - 1.0f, fw - 1.0f),
      grad4(hash_uint4(X + 1, Y + 1, Z + 1, W + 1), fx - 1.0f, fy - 1.0f, fz - 1.0f, fw - 1.0f),
      u,
      v,
      t,
      s);

  return r;
}

#else

/* ** SSE Implementation ** */

/* SSE Bilinear Interpolation:
 *
 * The function takes two ssef inputs:
 * - p : Contains the values at the points (v0, v1, v2, v3).
 * - f : Contains the values (x, y, _, _). The third and fourth values are unused.
 *
 * The interpolation is done in two steps:
 * 1. Interpolate (v0, v1) and (v2, v3) along the x axis to get g (g0, g1).
 *    (v2, v3) is generated by moving v2 and v3 to the first and second
 *    places of the ssef using the shuffle mask <2, 3, 2, 3>. The third and
 *    fourth values are unused.
 * 2. Interplate g0 and g1 along the y axis to get the final value.
 *    g1 is generated by populating an ssef with the second value of g.
 *    Only the first value is important in the final ssef.
 *
 * v1          v3          g1
 *  @ + + + + @            @                    y
 *  +         +     (1)    +    (2)             ^
 *  +         +     --->   +    --->   final    |
 *  +         +            +                    |
 *  @ + + + + @            @                    @------> x
 * v0          v2          g0
 *
 */
ccl_device_inline ssef bi_mix(ssef p, ssef f)
{
  ssef g = mix(p, shuffle<2, 3, 2, 3>(p), shuffle<0>(f));
  return mix(g, shuffle<1>(g), shuffle<1>(f));
}

/* SSE Trilinear Interpolation:
 *
 * The function takes three ssef inputs:
 * - p : Contains the values at the points (v0, v1, v2, v3).
 * - q : Contains the values at the points (v4, v5, v6, v7).
 * - f : Contains the values (x, y, z, _). The fourth value is unused.
 *
 * The interpolation is done in three steps:
 * 1. Interpolate p and q along the x axis to get s (s0, s1, s2, s3).
 * 2. Interpolate (s0, s1) and (s2, s3) along the y axis to get g (g0, g1).
 *    (s2, s3) is generated by moving v2 and v3 to the first and second
 *    places of the ssef using the shuffle mask <2, 3, 2, 3>. The third and
 *    fourth values are unused.
 * 3. Interplate g0 and g1 along the z axis to get the final value.
 *    g1 is generated by populating an ssef with the second value of g.
 *    Only the first value is important in the final ssef.
 *
 *   v3               v7
 *     @ + + + + + + @               s3 @
 *     +\            +\                 +\
 *     + \           + \                + \
 *     +  \          +  \               +  \             g1
 *     +   \ v1      +   \ v5           +   \ s1         @
 *     +    @ + + + +++ + @             +    @           +                     z
 *     +    +        +    +    (1)      +    +    (2)    +   (3)           y   ^
 *  v2 @ + +++ + + + @ v6 +    --->  s2 @    +    --->   +   --->  final    \  |
 *      \   +         \   +              \   +           +                   \ |
 *       \  +          \  +               \  +           +                    \|
 *        \ +           \ +                \ +           @                     +---------> x
 *         \+            \+                 \+           g0
 *          @ + + + + + + @                  @
 *        v0               v4                 s0
 */
ccl_device_inline ssef tri_mix(ssef p, ssef q, ssef f)
{
  ssef s = mix(p, q, shuffle<0>(f));
  ssef g = mix(s, shuffle<2, 3, 2, 3>(s), shuffle<1>(f));
  return mix(g, shuffle<1>(g), shuffle<2>(f));
}

/* SSE Quadrilinear Interpolation:
 *
 * Quadrilinear interpolation is as simple as a linear interpolation
 * between two trilinear interpolations.
 *
 */
ccl_device_inline ssef quad_mix(ssef p, ssef q, ssef r, ssef s, ssef f)
{
  return mix(tri_mix(p, q, f), tri_mix(r, s, f), shuffle<3>(f));
}

ccl_device_inline ssef fade(const ssef &t)
{
  ssef a = madd(t, 6.0f, -15.0f);
  ssef b = madd(t, a, 10.0f);
  return (t * t) * (t * b);
}

/* Negate val if the nth bit of h is 1. */
#  define negate_if_nth_bit(val, h, n) ((val) ^ cast(((h) & (1 << (n))) << (31 - (n))))

ccl_device_inline ssef grad(const ssei &hash, const ssef &x, const ssef &y)
{
  ssei h = hash & 7;
  ssef u = select(h < 4, x, y);
  ssef v = 2.0f * select(h < 4, y, x);
  return negate_if_nth_bit(u, h, 0) + negate_if_nth_bit(v, h, 1);
}

ccl_device_inline ssef grad(const ssei &hash, const ssef &x, const ssef &y, const ssef &z)
{
  ssei h = hash & 15;
  ssef u = select(h < 8, x, y);
  ssef vt = select((h == 12) | (h == 14), x, z);
  ssef v = select(h < 4, y, vt);
  return negate_if_nth_bit(u, h, 0) + negate_if_nth_bit(v, h, 1);
}

ccl_device_inline ssef
grad(const ssei &hash, const ssef &x, const ssef &y, const ssef &z, const ssef &w)
{
  ssei h = hash & 31;
  ssef u = select(h < 24, x, y);
  ssef v = select(h < 16, y, z);
  ssef s = select(h < 8, z, w);
  return negate_if_nth_bit(u, h, 0) + negate_if_nth_bit(v, h, 1) + negate_if_nth_bit(s, h, 2);
}

/* We use SSE to compute and interpolate 4 gradients at once:
 *
 *    Point  Offset from v0
 *     v0       (0, 0)
 *     v1       (0, 1)
 *     v2       (1, 0)    (0, 1, 0, 1) = shuffle<0, 2, 0, 2>(shuffle<1, 1, 1, 1>(V, V + 1))
 *     v3       (1, 1)         ^
 *               |  |__________|       (0, 0, 1, 1) = shuffle<0, 0, 0, 0>(V, V + 1)
 *               |                          ^
 *               |__________________________|
 *
 */
ccl_device_noinline float perlin_2d(float x, float y)
{
  ssei XY;
  ssef fxy = floorfrac(ssef(x, y, 0.0f, 0.0f), &XY);
  ssef uv = fade(fxy);

  ssei XY1 = XY + 1;
  ssei X = shuffle<0, 0, 0, 0>(XY, XY1);
  ssei Y = shuffle<0, 2, 0, 2>(shuffle<1, 1, 1, 1>(XY, XY1));

  ssei h = hash_ssei2(X, Y);

  ssef fxy1 = fxy - 1.0f;
  ssef fx = shuffle<0, 0, 0, 0>(fxy, fxy1);
  ssef fy = shuffle<0, 2, 0, 2>(shuffle<1, 1, 1, 1>(fxy, fxy1));

  ssef g = grad(h, fx, fy);

  return extract<0>(bi_mix(g, uv));
}

/* We use SSE to compute and interpolate 4 gradients at once. Since we have 8
 * gradients in 3D, we need to compute two sets of gradients at the points:
 *
 *    Point  Offset from v0
 *     v0      (0, 0, 0)
 *     v1      (0, 0, 1)
 *     v2      (0, 1, 0)    (0, 1, 0, 1) = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(V, V + 1))
 *     v3      (0, 1, 1)         ^
 *                 |  |__________|       (0, 0, 1, 1) = shuffle<1, 1, 1, 1>(V, V + 1)
 *                 |                          ^
 *                 |__________________________|
 *
 *    Point  Offset from v0
 *     v4      (1, 0, 0)
 *     v5      (1, 0, 1)
 *     v6      (1, 1, 0)
 *     v7      (1, 1, 1)
 *
 */
ccl_device_noinline float perlin_3d(float x, float y, float z)
{
  ssei XYZ;
  ssef fxyz = floorfrac(ssef(x, y, z, 0.0f), &XYZ);
  ssef uvw = fade(fxyz);

  ssei XYZ1 = XYZ + 1;
  ssei Y = shuffle<1, 1, 1, 1>(XYZ, XYZ1);
  ssei Z = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(XYZ, XYZ1));

  ssei h1 = hash_ssei3(shuffle<0>(XYZ), Y, Z);
  ssei h2 = hash_ssei3(shuffle<0>(XYZ1), Y, Z);

  ssef fxyz1 = fxyz - 1.0f;
  ssef fy = shuffle<1, 1, 1, 1>(fxyz, fxyz1);
  ssef fz = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(fxyz, fxyz1));

  ssef g1 = grad(h1, shuffle<0>(fxyz), fy, fz);
  ssef g2 = grad(h2, shuffle<0>(fxyz1), fy, fz);

  return extract<0>(tri_mix(g1, g2, uvw));
}

/* We use SSE to compute and interpolate 4 gradients at once. Since we have 16
 * gradients in 4D, we need to compute four sets of gradients at the points:
 *
 *    Point  Offset from v0
 *     v0     (0, 0, 0, 0)
 *     v1     (0, 0, 1, 0)
 *     v2     (0, 1, 0, 0)  (0, 1, 0, 1) = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(V, V + 1))
 *     v3     (0, 1, 1, 0)    ^
 *                |  |________|    (0, 0, 1, 1) = shuffle<1, 1, 1, 1>(V, V + 1)
 *                |                       ^
 *                |_______________________|
 *
 *    Point  Offset from v0
 *     v4     (1, 0, 0, 0)
 *     v5     (1, 0, 1, 0)
 *     v6     (1, 1, 0, 0)
 *     v7     (1, 1, 1, 0)
 *
 *    Point  Offset from v0
 *     v8     (0, 0, 0, 1)
 *     v9     (0, 0, 1, 1)
 *     v10    (0, 1, 0, 1)
 *     v11    (0, 1, 1, 1)
 *
 *    Point  Offset from v0
 *     v12    (1, 0, 0, 1)
 *     v13    (1, 0, 1, 1)
 *     v14    (1, 1, 0, 1)
 *     v15    (1, 1, 1, 1)
 *
 */
ccl_device_noinline float perlin_4d(float x, float y, float z, float w)
{
  ssei XYZW;
  ssef fxyzw = floorfrac(ssef(x, y, z, w), &XYZW);
  ssef uvws = fade(fxyzw);

  ssei XYZW1 = XYZW + 1;
  ssei Y = shuffle<1, 1, 1, 1>(XYZW, XYZW1);
  ssei Z = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(XYZW, XYZW1));

  ssei h1 = hash_ssei4(shuffle<0>(XYZW), Y, Z, shuffle<3>(XYZW));
  ssei h2 = hash_ssei4(shuffle<0>(XYZW1), Y, Z, shuffle<3>(XYZW));

  ssei h3 = hash_ssei4(shuffle<0>(XYZW), Y, Z, shuffle<3>(XYZW1));
  ssei h4 = hash_ssei4(shuffle<0>(XYZW1), Y, Z, shuffle<3>(XYZW1));

  ssef fxyzw1 = fxyzw - 1.0f;
  ssef fy = shuffle<1, 1, 1, 1>(fxyzw, fxyzw1);
  ssef fz = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(fxyzw, fxyzw1));

  ssef g1 = grad(h1, shuffle<0>(fxyzw), fy, fz, shuffle<3>(fxyzw));
  ssef g2 = grad(h2, shuffle<0>(fxyzw1), fy, fz, shuffle<3>(fxyzw));

  ssef g3 = grad(h3, shuffle<0>(fxyzw), fy, fz, shuffle<3>(fxyzw1));
  ssef g4 = grad(h4, shuffle<0>(fxyzw1), fy, fz, shuffle<3>(fxyzw1));

  return extract<0>(quad_mix(g1, g2, g3, g4, uvws));
}
#endif

/* Remap the output of noise to a predictable range [-1, 1].
 * The scale values were computed experimentally by the OSL developers.
 */

ccl_device_inline float noise_scale1(float result)
{
  return 0.2500f * result;
}

ccl_device_inline float noise_scale2(float result)
{
  return 0.6616f * result;
}

ccl_device_inline float noise_scale3(float result)
{
  return 0.9820f * result;
}

ccl_device_inline float noise_scale4(float result)
{
  return 0.8344f * result;
}

/* Safe Signed And Unsigned Noise */

ccl_device_inline float snoise_1d(float p)
{
  float r = perlin_1d(p);
  return isinf(r) ? 0.0f : noise_scale1(r);
}

ccl_device_inline float noise_1d(float p)
{
  return 0.5f * snoise_1d(p) + 0.5f;
}

ccl_device_inline float snoise_2d(float2 p)
{
  float r = perlin_2d(p.x, p.y);
  return isinf(r) ? 0.0f : noise_scale2(r);
}

ccl_device_inline float noise_2d(float2 p)
{
  return 0.5f * snoise_2d(p) + 0.5f;
}

ccl_device_inline float snoise_3d(float3 p)
{
  float r = perlin_3d(p.x, p.y, p.z);
  return isinf(r) ? 0.0f : noise_scale3(r);
}

ccl_device_inline float noise_3d(float3 p)
{
  return 0.5f * snoise_3d(p) + 0.5f;
}

ccl_device_inline float snoise_4d(float4 p)
{
  float r = perlin_4d(p.x, p.y, p.z, p.w);
  return isinf(r) ? 0.0f : noise_scale4(r);
}

ccl_device_inline float noise_4d(float4 p)
{
  return 0.5f * snoise_4d(p) + 0.5f;
}

/* cell noise */
ccl_device float cellnoise(float3 p)
{
  int3 ip = quick_floor_to_int3(p);
  return hash_uint3_to_float(ip.x, ip.y, ip.z);
}

ccl_device float3 cellnoise3(float3 p)
{
  int3 ip = quick_floor_to_int3(p);
#ifndef __KERNEL_SSE__
  float r = hash_uint3_to_float(ip.x, ip.y, ip.z);
  float g = hash_uint3_to_float(ip.y, ip.x, ip.z);
  float b = hash_uint3_to_float(ip.y, ip.z, ip.x);
  return make_float3(r, g, b);
#else
  ssei ip_yxz = shuffle<1, 0, 2, 3>(ssei(ip.m128));
  ssei ip_xyy = shuffle<0, 1, 1, 3>(ssei(ip.m128));
  ssei ip_zzx = shuffle<2, 2, 0, 3>(ssei(ip.m128));
  ssei bits = hash_ssei3(ip_xyy, ip_yxz, ip_zzx);
  return float3(uint32_to_float(bits) * ssef(1.0f / (float)0xFFFFFFFF));
#endif
}

CCL_NAMESPACE_END
