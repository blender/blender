/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

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
 * SSE is supported, that is, if __KERNEL_SSE__ is defined. If it is not
 * supported, we do a standard implementation, but if it is supported, we
 * do an implementation using SSE intrinsics.
 */
#if !defined(__KERNEL_SSE__)

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

#else /* SSE is supported. */

/* ** SSE Implementation ** */

/* SSE Bilinear Interpolation:
 *
 * The function takes two float4 inputs:
 * - p : Contains the values at the points (v0, v1, v2, v3).
 * - f : Contains the values (x, y, _, _). The third and fourth values are unused.
 *
 * The interpolation is done in two steps:
 * 1. Interpolate (v0, v1) and (v2, v3) along the x axis to get g (g0, g1).
 *    (v2, v3) is generated by moving v2 and v3 to the first and second
 *    places of the float4 using the shuffle mask <2, 3, 2, 3>. The third and
 *    fourth values are unused.
 * 2. Interpolate g0 and g1 along the y axis to get the final value.
 *    g1 is generated by populating an float4 with the second value of g.
 *    Only the first value is important in the final float4.
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
ccl_device_inline float4 bi_mix(float4 p, float4 f)
{
  float4 g = mix(p, shuffle<2, 3, 2, 3>(p), shuffle<0>(f));
  return mix(g, shuffle<1>(g), shuffle<1>(f));
}

ccl_device_inline float4 fade(const float4 t)
{
  float4 a = madd(t, make_float4(6.0f), make_float4(-15.0f));
  float4 b = madd(t, a, make_float4(10.0f));
  return (t * t) * (t * b);
}

/* Negate val if the nth bit of h is 1. */
#  define negate_if_nth_bit(val, h, n) ((val) ^ cast(((h) & (1 << (n))) << (31 - (n))))

ccl_device_inline float4 grad(const int4 hash, const float4 x, const float4 y)
{
  int4 h = hash & 7;
  float4 u = select(h < 4, x, y);
  float4 v = 2.0f * select(h < 4, y, x);
  return negate_if_nth_bit(u, h, 0) + negate_if_nth_bit(v, h, 1);
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
ccl_device_noinline_cpu float perlin_2d(float x, float y)
{
  int4 XY;
  float4 fxy = floorfrac(make_float4(x, y, 0.0f, 0.0f), &XY);
  float4 uv = fade(fxy);

  int4 XY1 = XY + make_int4(1);
  int4 X = shuffle<0, 0, 0, 0>(XY, XY1);
  int4 Y = shuffle<0, 2, 0, 2>(shuffle<1, 1, 1, 1>(XY, XY1));

  int4 h = hash_int4_2(X, Y);

  float4 fxy1 = fxy - make_float4(1.0f);
  float4 fx = shuffle<0, 0, 0, 0>(fxy, fxy1);
  float4 fy = shuffle<0, 2, 0, 2>(shuffle<1, 1, 1, 1>(fxy, fxy1));

  float4 g = grad(h, fx, fy);

  return extract<0>(bi_mix(g, uv));
}

/* SSE Trilinear Interpolation:
 *
 * The function takes three float4 inputs:
 * - p : Contains the values at the points (v0, v1, v2, v3).
 * - q : Contains the values at the points (v4, v5, v6, v7).
 * - f : Contains the values (x, y, z, _). The fourth value is unused.
 *
 * The interpolation is done in three steps:
 * 1. Interpolate p and q along the x axis to get s (s0, s1, s2, s3).
 * 2. Interpolate (s0, s1) and (s2, s3) along the y axis to get g (g0, g1).
 *    (s2, s3) is generated by moving v2 and v3 to the first and second
 *    places of the float4 using the shuffle mask <2, 3, 2, 3>. The third and
 *    fourth values are unused.
 * 3. Interpolate g0 and g1 along the z axis to get the final value.
 *    g1 is generated by populating an float4 with the second value of g.
 *    Only the first value is important in the final float4.
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
ccl_device_inline float4 tri_mix(float4 p, float4 q, float4 f)
{
  float4 s = mix(p, q, shuffle<0>(f));
  float4 g = mix(s, shuffle<2, 3, 2, 3>(s), shuffle<1>(f));
  return mix(g, shuffle<1>(g), shuffle<2>(f));
}

/* 3D and 4D noise can be accelerated using AVX, so we first check if AVX
 * is supported, that is, if __KERNEL_AVX__ is defined. If it is not
 * supported, we do an SSE implementation, but if it is supported,
 * we do an implementation using AVX intrinsics.
 */
#  if !defined(__KERNEL_AVX2__)

ccl_device_inline float4 grad(const int4 hash, const float4 x, const float4 y, const float4 z)
{
  int4 h = hash & 15;
  float4 u = select(h < 8, x, y);
  float4 vt = select((h == 12) | (h == 14), x, z);
  float4 v = select(h < 4, y, vt);
  return negate_if_nth_bit(u, h, 0) + negate_if_nth_bit(v, h, 1);
}

ccl_device_inline float4
grad(const int4 hash, const float4 x, const float4 y, const float4 z, const float4 w)
{
  int4 h = hash & 31;
  float4 u = select(h < 24, x, y);
  float4 v = select(h < 16, y, z);
  float4 s = select(h < 8, z, w);
  return negate_if_nth_bit(u, h, 0) + negate_if_nth_bit(v, h, 1) + negate_if_nth_bit(s, h, 2);
}

/* SSE Quadrilinear Interpolation:
 *
 * Quadrilinear interpolation is as simple as a linear interpolation
 * between two trilinear interpolations.
 *
 */
ccl_device_inline float4 quad_mix(float4 p, float4 q, float4 r, float4 s, float4 f)
{
  return mix(tri_mix(p, q, f), tri_mix(r, s, f), shuffle<3>(f));
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
ccl_device_noinline_cpu float perlin_3d(float x, float y, float z)
{
  int4 XYZ;
  float4 fxyz = floorfrac(make_float4(x, y, z, 0.0f), &XYZ);
  float4 uvw = fade(fxyz);

  int4 XYZ1 = XYZ + make_int4(1);
  int4 Y = shuffle<1, 1, 1, 1>(XYZ, XYZ1);
  int4 Z = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(XYZ, XYZ1));

  int4 h1 = hash_int4_3(shuffle<0>(XYZ), Y, Z);
  int4 h2 = hash_int4_3(shuffle<0>(XYZ1), Y, Z);

  float4 fxyz1 = fxyz - make_float4(1.0f);
  float4 fy = shuffle<1, 1, 1, 1>(fxyz, fxyz1);
  float4 fz = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(fxyz, fxyz1));

  float4 g1 = grad(h1, shuffle<0>(fxyz), fy, fz);
  float4 g2 = grad(h2, shuffle<0>(fxyz1), fy, fz);

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
ccl_device_noinline_cpu float perlin_4d(float x, float y, float z, float w)
{
  int4 XYZW;
  float4 fxyzw = floorfrac(make_float4(x, y, z, w), &XYZW);
  float4 uvws = fade(fxyzw);

  int4 XYZW1 = XYZW + make_int4(1);
  int4 Y = shuffle<1, 1, 1, 1>(XYZW, XYZW1);
  int4 Z = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(XYZW, XYZW1));

  int4 h1 = hash_int4_4(shuffle<0>(XYZW), Y, Z, shuffle<3>(XYZW));
  int4 h2 = hash_int4_4(shuffle<0>(XYZW1), Y, Z, shuffle<3>(XYZW));

  int4 h3 = hash_int4_4(shuffle<0>(XYZW), Y, Z, shuffle<3>(XYZW1));
  int4 h4 = hash_int4_4(shuffle<0>(XYZW1), Y, Z, shuffle<3>(XYZW1));

  float4 fxyzw1 = fxyzw - make_float4(1.0f);
  float4 fy = shuffle<1, 1, 1, 1>(fxyzw, fxyzw1);
  float4 fz = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(fxyzw, fxyzw1));

  float4 g1 = grad(h1, shuffle<0>(fxyzw), fy, fz, shuffle<3>(fxyzw));
  float4 g2 = grad(h2, shuffle<0>(fxyzw1), fy, fz, shuffle<3>(fxyzw));

  float4 g3 = grad(h3, shuffle<0>(fxyzw), fy, fz, shuffle<3>(fxyzw1));
  float4 g4 = grad(h4, shuffle<0>(fxyzw1), fy, fz, shuffle<3>(fxyzw1));

  return extract<0>(quad_mix(g1, g2, g3, g4, uvws));
}

#  else /* AVX is supported. */

/* AVX Implementation */

ccl_device_inline vfloat8 grad(const vint8 hash, const vfloat8 x, const vfloat8 y, const vfloat8 z)
{
  vint8 h = hash & 15;
  vfloat8 u = select(h < 8, x, y);
  vfloat8 vt = select((h == 12) | (h == 14), x, z);
  vfloat8 v = select(h < 4, y, vt);
  return negate_if_nth_bit(u, h, 0) + negate_if_nth_bit(v, h, 1);
}

ccl_device_inline vfloat8
grad(const vint8 hash, const vfloat8 x, const vfloat8 y, const vfloat8 z, const vfloat8 w)
{
  vint8 h = hash & 31;
  vfloat8 u = select(h < 24, x, y);
  vfloat8 v = select(h < 16, y, z);
  vfloat8 s = select(h < 8, z, w);
  return negate_if_nth_bit(u, h, 0) + negate_if_nth_bit(v, h, 1) + negate_if_nth_bit(s, h, 2);
}

/* SSE Quadrilinear Interpolation:
 *
 * The interpolation is done in two steps:
 * 1. Interpolate p and q along the w axis to get s.
 * 2. Trilinearly interpolate (s0, s1, s2, s3) and (s4, s5, s6, s7) to get the final
 *    value. (s0, s1, s2, s3) and (s4, s5, s6, s7) are generated by extracting the
 *    low and high float4 from s.
 *
 */
ccl_device_inline float4 quad_mix(vfloat8 p, vfloat8 q, float4 f)
{
  float4 fv = shuffle<3>(f);
  vfloat8 s = mix(p, q, make_vfloat8(fv, fv));
  return tri_mix(low(s), high(s), f);
}

/* We use AVX to compute and interpolate 8 gradients at once.
 *
 *    Point  Offset from v0
 *     v0      (0, 0, 0)
 *     v1      (0, 0, 1)    The full AVX type is computed by inserting the following
 *     v2      (0, 1, 0)    SSE types into both the low and high parts of the AVX.
 *     v3      (0, 1, 1)
 *     v4      (1, 0, 0)
 *     v5      (1, 0, 1)    (0, 1, 0, 1) = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(V, V + 1))
 *     v6      (1, 1, 0)         ^
 *     v7      (1, 1, 1)         |
 *                 |  |__________|       (0, 0, 1, 1) = shuffle<1, 1, 1, 1>(V, V + 1)
 *                 |                          ^
 *                 |__________________________|
 *
 */
ccl_device_noinline_cpu float perlin_3d(float x, float y, float z)
{
  int4 XYZ;
  float4 fxyz = floorfrac(make_float4(x, y, z, 0.0f), &XYZ);
  float4 uvw = fade(fxyz);

  int4 XYZ1 = XYZ + make_int4(1);
  int4 X = shuffle<0>(XYZ);
  int4 X1 = shuffle<0>(XYZ1);
  int4 Y = shuffle<1, 1, 1, 1>(XYZ, XYZ1);
  int4 Z = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(XYZ, XYZ1));

  vint8 h = hash_int8_3(make_vint8(X, X1), make_vint8(Y, Y), make_vint8(Z, Z));

  float4 fxyz1 = fxyz - make_float4(1.0f);
  float4 fx = shuffle<0>(fxyz);
  float4 fx1 = shuffle<0>(fxyz1);
  float4 fy = shuffle<1, 1, 1, 1>(fxyz, fxyz1);
  float4 fz = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(fxyz, fxyz1));

  vfloat8 g = grad(h, make_vfloat8(fx, fx1), make_vfloat8(fy, fy), make_vfloat8(fz, fz));

  return extract<0>(tri_mix(low(g), high(g), uvw));
}

/* We use AVX to compute and interpolate 8 gradients at once. Since we have 16
 * gradients in 4D, we need to compute two sets of gradients at the points:
 *
 *    Point  Offset from v0
 *     v0     (0, 0, 0, 0)
 *     v1     (0, 0, 1, 0)  The full AVX type is computed by inserting the following
 *     v2     (0, 1, 0, 0)  SSE types into both the low and high parts of the AVX.
 *     v3     (0, 1, 1, 0)
 *     v4     (1, 0, 0, 0)
 *     v5     (1, 0, 1, 0)  (0, 1, 0, 1) = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(V, V + 1))
 *     v6     (1, 1, 0, 0)    ^
 *     v7     (1, 1, 1, 0)    |
 *                |  |________|    (0, 0, 1, 1) = shuffle<1, 1, 1, 1>(V, V + 1)
 *                |                       ^
 *                |_______________________|
 *
 *    Point  Offset from v0
 *     v8     (0, 0, 0, 1)
 *     v9     (0, 0, 1, 1)
 *     v10    (0, 1, 0, 1)
 *     v11    (0, 1, 1, 1)
 *     v12    (1, 0, 0, 1)
 *     v13    (1, 0, 1, 1)
 *     v14    (1, 1, 0, 1)
 *     v15    (1, 1, 1, 1)
 *
 */
ccl_device_noinline_cpu float perlin_4d(float x, float y, float z, float w)
{
  int4 XYZW;
  float4 fxyzw = floorfrac(make_float4(x, y, z, w), &XYZW);
  float4 uvws = fade(fxyzw);

  int4 XYZW1 = XYZW + make_int4(1);
  int4 X = shuffle<0>(XYZW);
  int4 X1 = shuffle<0>(XYZW1);
  int4 Y = shuffle<1, 1, 1, 1>(XYZW, XYZW1);
  int4 Z = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(XYZW, XYZW1));
  int4 W = shuffle<3>(XYZW);
  int4 W1 = shuffle<3>(XYZW1);

  vint8 h1 = hash_int8_4(make_vint8(X, X1), make_vint8(Y, Y), make_vint8(Z, Z), make_vint8(W, W));
  vint8 h2 = hash_int8_4(
      make_vint8(X, X1), make_vint8(Y, Y), make_vint8(Z, Z), make_vint8(W1, W1));

  float4 fxyzw1 = fxyzw - make_float4(1.0f);
  float4 fx = shuffle<0>(fxyzw);
  float4 fx1 = shuffle<0>(fxyzw1);
  float4 fy = shuffle<1, 1, 1, 1>(fxyzw, fxyzw1);
  float4 fz = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(fxyzw, fxyzw1));
  float4 fw = shuffle<3>(fxyzw);
  float4 fw1 = shuffle<3>(fxyzw1);

  vfloat8 g1 = grad(
      h1, make_vfloat8(fx, fx1), make_vfloat8(fy, fy), make_vfloat8(fz, fz), make_vfloat8(fw, fw));
  vfloat8 g2 = grad(h2,
                    make_vfloat8(fx, fx1),
                    make_vfloat8(fy, fy),
                    make_vfloat8(fz, fz),
                    make_vfloat8(fw1, fw1));

  return extract<0>(quad_mix(g1, g2, uvws));
}
#  endif

#  undef negate_if_nth_bit

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
  float precision_correction = 0.5f * float(fabsf(p) >= 1000000.0f);
  /* Repeat Perlin noise texture every 100000.0 on each axis to prevent floating point
   * representation issues. */
  /* The 1D variant of fmod is called fmodf. */
  p = fmodf(p, 100000.0f) + precision_correction;

  return noise_scale1(perlin_1d(p));
}

ccl_device_inline float noise_1d(float p)
{
  return 0.5f * snoise_1d(p) + 0.5f;
}

ccl_device_inline float snoise_2d(float2 p)
{
  float2 precision_correction = 0.5f * make_float2(float(fabsf(p.x) >= 1000000.0f),
                                                   float(fabsf(p.y) >= 1000000.0f));
  /* Repeat Perlin noise texture every 100000.0f on each axis to prevent floating point
   * representation issues. This causes discontinuities every 100000.0f, however at such scales
   * this usually shouldn't be noticeable. */
  p = fmod(p, 100000.0f) + precision_correction;

  return noise_scale2(perlin_2d(p.x, p.y));
}

ccl_device_inline float noise_2d(float2 p)
{
  return 0.5f * snoise_2d(p) + 0.5f;
}

ccl_device_inline float snoise_3d(float3 p)
{
  float3 precision_correction = 0.5f * make_float3(float(fabsf(p.x) >= 1000000.0f),
                                                   float(fabsf(p.y) >= 1000000.0f),
                                                   float(fabsf(p.z) >= 1000000.0f));
  /* Repeat Perlin noise texture every 100000.0f on each axis to prevent floating point
   * representation issues. This causes discontinuities every 100000.0f, however at such scales
   * this usually shouldn't be noticeable. */
  p = fmod(p, 100000.0f) + precision_correction;

  return noise_scale3(perlin_3d(p.x, p.y, p.z));
}

ccl_device_inline float noise_3d(float3 p)
{
  return 0.5f * snoise_3d(p) + 0.5f;
}

ccl_device_inline float snoise_4d(float4 p)
{
  float4 precision_correction = 0.5f * make_float4(float(fabsf(p.x) >= 1000000.0f),
                                                   float(fabsf(p.y) >= 1000000.0f),
                                                   float(fabsf(p.z) >= 1000000.0f),
                                                   float(fabsf(p.w) >= 1000000.0f));
  /* Repeat Perlin noise texture every 100000.0f on each axis to prevent floating point
   * representation issues. This causes discontinuities every 100000.0f, however at such scales
   * this usually shouldn't be noticeable. */
  p = fmod(p, 100000.0f) + precision_correction;

  return noise_scale4(perlin_4d(p.x, p.y, p.z, p.w));
}

ccl_device_inline float noise_4d(float4 p)
{
  return 0.5f * snoise_4d(p) + 0.5f;
}

CCL_NAMESPACE_END
