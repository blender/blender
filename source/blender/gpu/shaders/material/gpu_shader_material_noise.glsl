/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_hash.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

/* clang-format off */
#define FLOORFRAC(x, x_int, x_fract) { float x_floor = floor(x); x_int = int(x_floor); x_fract = x - x_floor; }
/* clang-format on */

/* Bilinear Interpolation:
 *
 * v2          v3
 *  @ + + + + @       y
 *  +         +       ^
 *  +         +       |
 *  +         +       |
 *  @ + + + + @       @------> x
 * v0          v1
 */
float bi_mix(float v0, float v1, float v2, float v3, float x, float y)
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
float tri_mix(float v0,
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

float quad_mix(float v0,
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

float fade(float t)
{
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float negate_if(float value, uint condition)
{
  return (condition != 0u) ? -value : value;
}

float noise_grad(uint hash, float x)
{
  uint h = hash & 15u;
  float g = 1u + (h & 7u);
  return negate_if(g, h & 8u) * x;
}

float noise_grad(uint hash, float x, float y)
{
  uint h = hash & 7u;
  float u = h < 4u ? x : y;
  float v = 2.0f * (h < 4u ? y : x);
  return negate_if(u, h & 1u) + negate_if(v, h & 2u);
}

float noise_grad(uint hash, float x, float y, float z)
{
  uint h = hash & 15u;
  float u = h < 8u ? x : y;
  float vt = ((h == 12u) || (h == 14u)) ? x : z;
  float v = h < 4u ? y : vt;
  return negate_if(u, h & 1u) + negate_if(v, h & 2u);
}

float noise_grad(uint hash, float x, float y, float z, float w)
{
  uint h = hash & 31u;
  float u = h < 24u ? x : y;
  float v = h < 16u ? y : z;
  float s = h < 8u ? z : w;
  return negate_if(u, h & 1u) + negate_if(v, h & 2u) + negate_if(s, h & 4u);
}

float noise_perlin(float x)
{
  int X;
  float fx;

  FLOORFRAC(x, X, fx);

  float u = fade(fx);

  float r = mix(noise_grad(hash_int(X), fx), noise_grad(hash_int(X + 1), fx - 1.0f), u);

  return r;
}

float noise_perlin(float2 vec)
{
  int X, Y;
  float fx, fy;

  FLOORFRAC(vec.x, X, fx);
  FLOORFRAC(vec.y, Y, fy);

  float u = fade(fx);
  float v = fade(fy);

  float r = bi_mix(noise_grad(hash_int2(X, Y), fx, fy),
                   noise_grad(hash_int2(X + 1, Y), fx - 1.0f, fy),
                   noise_grad(hash_int2(X, Y + 1), fx, fy - 1.0f),
                   noise_grad(hash_int2(X + 1, Y + 1), fx - 1.0f, fy - 1.0f),
                   u,
                   v);

  return r;
}

float noise_perlin(float3 vec)
{
  int X, Y, Z;
  float fx, fy, fz;

  FLOORFRAC(vec.x, X, fx);
  FLOORFRAC(vec.y, Y, fy);
  FLOORFRAC(vec.z, Z, fz);

  float u = fade(fx);
  float v = fade(fy);
  float w = fade(fz);

  float r = tri_mix(noise_grad(hash_int3(X, Y, Z), fx, fy, fz),
                    noise_grad(hash_int3(X + 1, Y, Z), fx - 1, fy, fz),
                    noise_grad(hash_int3(X, Y + 1, Z), fx, fy - 1, fz),
                    noise_grad(hash_int3(X + 1, Y + 1, Z), fx - 1, fy - 1, fz),
                    noise_grad(hash_int3(X, Y, Z + 1), fx, fy, fz - 1),
                    noise_grad(hash_int3(X + 1, Y, Z + 1), fx - 1, fy, fz - 1),
                    noise_grad(hash_int3(X, Y + 1, Z + 1), fx, fy - 1, fz - 1),
                    noise_grad(hash_int3(X + 1, Y + 1, Z + 1), fx - 1, fy - 1, fz - 1),
                    u,
                    v,
                    w);

  return r;
}

float noise_perlin(float4 vec)
{
  int X, Y, Z, W;
  float fx, fy, fz, fw;

  FLOORFRAC(vec.x, X, fx);
  FLOORFRAC(vec.y, Y, fy);
  FLOORFRAC(vec.z, Z, fz);
  FLOORFRAC(vec.w, W, fw);

  float u = fade(fx);
  float v = fade(fy);
  float t = fade(fz);
  float s = fade(fw);

  float r = quad_mix(
      noise_grad(hash_int4(X, Y, Z, W), fx, fy, fz, fw),
      noise_grad(hash_int4(X + 1, Y, Z, W), fx - 1.0f, fy, fz, fw),
      noise_grad(hash_int4(X, Y + 1, Z, W), fx, fy - 1.0f, fz, fw),
      noise_grad(hash_int4(X + 1, Y + 1, Z, W), fx - 1.0f, fy - 1.0f, fz, fw),
      noise_grad(hash_int4(X, Y, Z + 1, W), fx, fy, fz - 1.0f, fw),
      noise_grad(hash_int4(X + 1, Y, Z + 1, W), fx - 1.0f, fy, fz - 1.0f, fw),
      noise_grad(hash_int4(X, Y + 1, Z + 1, W), fx, fy - 1.0f, fz - 1.0f, fw),
      noise_grad(hash_int4(X + 1, Y + 1, Z + 1, W), fx - 1.0f, fy - 1.0f, fz - 1.0f, fw),
      noise_grad(hash_int4(X, Y, Z, W + 1), fx, fy, fz, fw - 1.0f),
      noise_grad(hash_int4(X + 1, Y, Z, W + 1), fx - 1.0f, fy, fz, fw - 1.0f),
      noise_grad(hash_int4(X, Y + 1, Z, W + 1), fx, fy - 1.0f, fz, fw - 1.0f),
      noise_grad(hash_int4(X + 1, Y + 1, Z, W + 1), fx - 1.0f, fy - 1.0f, fz, fw - 1.0f),
      noise_grad(hash_int4(X, Y, Z + 1, W + 1), fx, fy, fz - 1.0f, fw - 1.0f),
      noise_grad(hash_int4(X + 1, Y, Z + 1, W + 1), fx - 1.0f, fy, fz - 1.0f, fw - 1.0f),
      noise_grad(hash_int4(X, Y + 1, Z + 1, W + 1), fx, fy - 1.0f, fz - 1.0f, fw - 1.0f),
      noise_grad(
          hash_int4(X + 1, Y + 1, Z + 1, W + 1), fx - 1.0f, fy - 1.0f, fz - 1.0f, fw - 1.0f),
      u,
      v,
      t,
      s);

  return r;
}

/* Remap the output of noise to a predictable range [-1, 1].
 * The scale values were computed experimentally by the OSL developers.
 */
float noise_scale1(float result)
{
  return 0.2500f * result;
}

float noise_scale2(float result)
{
  return 0.6616f * result;
}

float noise_scale3(float result)
{
  return 0.9820f * result;
}

float noise_scale4(float result)
{
  return 0.8344f * result;
}

/* Safe Signed And Unsigned Noise */

float snoise(float p)
{
  float precision_correction = 0.5f * float(abs(p) >= 1000000.0f);
  /* Repeat Perlin noise texture every 100000.0 on each axis to prevent floating point
   * representation issues. */
  p = compatible_mod(p, 100000.0f) + precision_correction;

  return noise_scale1(noise_perlin(p));
}

float noise(float p)
{
  return 0.5f * snoise(p) + 0.5f;
}

float snoise(float2 p)
{
  float2 precision_correction = 0.5f * float2(float(abs(p.x) >= 1000000.0f),
                                              float(abs(p.y) >= 1000000.0f));
  /* Repeat Perlin noise texture every 100000.0 on each axis to prevent floating point
   * representation issues. This causes discontinuities every 100000.0f, however at such scales
   * this usually shouldn't be noticeable. */
  p = compatible_mod(p, 100000.0f) + precision_correction;

  return noise_scale2(noise_perlin(p));
}

float noise(float2 p)
{
  return 0.5f * snoise(p) + 0.5f;
}

float snoise(float3 p)
{
  float3 precision_correction = 0.5f * float3(float(abs(p.x) >= 1000000.0f),
                                              float(abs(p.y) >= 1000000.0f),
                                              float(abs(p.z) >= 1000000.0f));
  /* Repeat Perlin noise texture every 100000.0 on each axis to prevent floating point
   * representation issues. This causes discontinuities every 100000.0f, however at such scales
   * this usually shouldn't be noticeable. */
  p = compatible_mod(p, 100000.0f) + precision_correction;

  return noise_scale3(noise_perlin(p));
}

float noise(float3 p)
{
  return 0.5f * snoise(p) + 0.5f;
}

float snoise(float4 p)
{
  float4 precision_correction = 0.5f * float4(float(abs(p.x) >= 1000000.0f),
                                              float(abs(p.y) >= 1000000.0f),
                                              float(abs(p.z) >= 1000000.0f),
                                              float(abs(p.w) >= 1000000.0f));
  /* Repeat Perlin noise texture every 100000.0 on each axis to prevent floating point
   * representation issues. This causes discontinuities every 100000.0f, however at such scales
   * this usually shouldn't be noticeable. */
  p = compatible_mod(p, 100000.0f) + precision_correction;

  return noise_scale4(noise_perlin(p));
}

float noise(float4 p)
{
  return 0.5f * snoise(p) + 0.5f;
}
