/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al.
 *                         All Rights Reserved. (BSD-3-Clause).
 * SPDX-FileCopyrightText: 2011 Blender Foundation (GPL-2.0-or-later).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later AND BSD-3-Clause */

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "BLI_math_base_safe.h"
#include "BLI_math_vector.hh"
#include "BLI_noise.hh"
#include "BLI_utildefines.h"

namespace blender::noise {

/* -------------------------------------------------------------------- */
/** \name Jenkins Lookup3 Hash Functions
 *
 * https://burtleburtle.net/bob/c/lookup3.c
 * \{ */

BLI_INLINE uint32_t hash_bit_rotate(uint32_t x, uint32_t k)
{
  return (x << k) | (x >> (32 - k));
}

BLI_INLINE void hash_bit_mix(uint32_t &a, uint32_t &b, uint32_t &c)
{
  a -= c;
  a ^= hash_bit_rotate(c, 4);
  c += b;
  b -= a;
  b ^= hash_bit_rotate(a, 6);
  a += c;
  c -= b;
  c ^= hash_bit_rotate(b, 8);
  b += a;
  a -= c;
  a ^= hash_bit_rotate(c, 16);
  c += b;
  b -= a;
  b ^= hash_bit_rotate(a, 19);
  a += c;
  c -= b;
  c ^= hash_bit_rotate(b, 4);
  b += a;
}

BLI_INLINE void hash_bit_final(uint32_t &a, uint32_t &b, uint32_t &c)
{
  c ^= b;
  c -= hash_bit_rotate(b, 14);
  a ^= c;
  a -= hash_bit_rotate(c, 11);
  b ^= a;
  b -= hash_bit_rotate(a, 25);
  c ^= b;
  c -= hash_bit_rotate(b, 16);
  a ^= c;
  a -= hash_bit_rotate(c, 4);
  b ^= a;
  b -= hash_bit_rotate(a, 14);
  c ^= b;
  c -= hash_bit_rotate(b, 24);
}

uint32_t hash(uint32_t kx)
{
  uint32_t a, b, c;
  a = b = c = 0xdeadbeef + (1 << 2) + 13;

  a += kx;
  hash_bit_final(a, b, c);

  return c;
}

uint32_t hash(uint32_t kx, uint32_t ky)
{
  uint32_t a, b, c;
  a = b = c = 0xdeadbeef + (2 << 2) + 13;

  b += ky;
  a += kx;
  hash_bit_final(a, b, c);

  return c;
}

uint32_t hash(uint32_t kx, uint32_t ky, uint32_t kz)
{
  uint32_t a, b, c;
  a = b = c = 0xdeadbeef + (3 << 2) + 13;

  c += kz;
  b += ky;
  a += kx;
  hash_bit_final(a, b, c);

  return c;
}

uint32_t hash(uint32_t kx, uint32_t ky, uint32_t kz, uint32_t kw)
{
  uint32_t a, b, c;
  a = b = c = 0xdeadbeef + (4 << 2) + 13;

  a += kx;
  b += ky;
  c += kz;
  hash_bit_mix(a, b, c);

  a += kw;
  hash_bit_final(a, b, c);

  return c;
}

BLI_INLINE uint32_t float_as_uint(float f)
{
  union {
    uint32_t i;
    float f;
  } u;
  u.f = f;
  return u.i;
}

uint32_t hash_float(float kx)
{
  return hash(float_as_uint(kx));
}

uint32_t hash_float(float2 k)
{
  return hash(float_as_uint(k.x), float_as_uint(k.y));
}

uint32_t hash_float(float3 k)
{
  return hash(float_as_uint(k.x), float_as_uint(k.y), float_as_uint(k.z));
}

uint32_t hash_float(float4 k)
{
  return hash(float_as_uint(k.x), float_as_uint(k.y), float_as_uint(k.z), float_as_uint(k.w));
}

/* Hashing a number of uint32_t into a float in the range [0, 1]. */

BLI_INLINE float uint_to_float_01(uint32_t k)
{
  return float(k) / float(0xFFFFFFFFu);
}

float hash_to_float(uint32_t kx)
{
  return uint_to_float_01(hash(kx));
}

float hash_to_float(uint32_t kx, uint32_t ky)
{
  return uint_to_float_01(hash(kx, ky));
}

float hash_to_float(uint32_t kx, uint32_t ky, uint32_t kz)
{
  return uint_to_float_01(hash(kx, ky, kz));
}

float hash_to_float(uint32_t kx, uint32_t ky, uint32_t kz, uint32_t kw)
{
  return uint_to_float_01(hash(kx, ky, kz, kw));
}

/* Hashing a number of floats into a float in the range [0, 1]. */

float hash_float_to_float(float k)
{
  return uint_to_float_01(hash_float(k));
}

float hash_float_to_float(float2 k)
{
  return uint_to_float_01(hash_float(k));
}

float hash_float_to_float(float3 k)
{
  return uint_to_float_01(hash_float(k));
}

float hash_float_to_float(float4 k)
{
  return uint_to_float_01(hash_float(k));
}

float2 hash_float_to_float2(float2 k)
{
  return float2(hash_float_to_float(k), hash_float_to_float(float3(k.x, k.y, 1.0)));
}

float3 hash_float_to_float3(float k)
{
  return float3(hash_float_to_float(k),
                hash_float_to_float(float2(k, 1.0)),
                hash_float_to_float(float2(k, 2.0)));
}

float3 hash_float_to_float3(float2 k)
{
  return float3(hash_float_to_float(k),
                hash_float_to_float(float3(k.x, k.y, 1.0)),
                hash_float_to_float(float3(k.x, k.y, 2.0)));
}

float3 hash_float_to_float3(float3 k)
{
  return float3(hash_float_to_float(k),
                hash_float_to_float(float4(k.x, k.y, k.z, 1.0)),
                hash_float_to_float(float4(k.x, k.y, k.z, 2.0)));
}

float3 hash_float_to_float3(float4 k)
{
  return float3(hash_float_to_float(k),
                hash_float_to_float(float4(k.z, k.x, k.w, k.y)),
                hash_float_to_float(float4(k.w, k.z, k.y, k.x)));
}

float4 hash_float_to_float4(float4 k)
{
  return float4(hash_float_to_float(k),
                hash_float_to_float(float4(k.w, k.x, k.y, k.z)),
                hash_float_to_float(float4(k.z, k.w, k.x, k.y)),
                hash_float_to_float(float4(k.y, k.z, k.w, k.x)));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Perlin Noise
 *
 * Perlin, Ken. "Improving noise." Proceedings of the 29th annual conference on Computer graphics
 * and interactive techniques. 2002.
 *
 * This implementation is functionally identical to the implementations in EEVEE, OSL, and SVM. So
 * any changes should be applied in all relevant implementations.
 * \{ */

/* Linear Interpolation. */
template<typename T> T static mix(T v0, T v1, float x)
{
  return (1 - x) * v0 + x * v1;
}

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
BLI_INLINE float mix(float v0, float v1, float v2, float v3, float x, float y)
{
  float x1 = 1.0 - x;
  return (1.0 - y) * (v0 * x1 + v1 * x) + y * (v2 * x1 + v3 * x);
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
BLI_INLINE float mix(float v0,
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
  float x1 = 1.0 - x;
  float y1 = 1.0 - y;
  float z1 = 1.0 - z;
  return z1 * (y1 * (v0 * x1 + v1 * x) + y * (v2 * x1 + v3 * x)) +
         z * (y1 * (v4 * x1 + v5 * x) + y * (v6 * x1 + v7 * x));
}

/* Quadrilinear Interpolation. */
BLI_INLINE float mix(float v0,
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
  return mix(mix(v0, v1, v2, v3, v4, v5, v6, v7, x, y, z),
             mix(v8, v9, v10, v11, v12, v13, v14, v15, x, y, z),
             w);
}

BLI_INLINE float fade(float t)
{
  return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

BLI_INLINE float negate_if(float value, uint32_t condition)
{
  return (condition != 0u) ? -value : value;
}

BLI_INLINE float noise_grad(uint32_t hash, float x)
{
  uint32_t h = hash & 15u;
  float g = 1u + (h & 7u);
  return negate_if(g, h & 8u) * x;
}

BLI_INLINE float noise_grad(uint32_t hash, float x, float y)
{
  uint32_t h = hash & 7u;
  float u = h < 4u ? x : y;
  float v = 2.0 * (h < 4u ? y : x);
  return negate_if(u, h & 1u) + negate_if(v, h & 2u);
}

BLI_INLINE float noise_grad(uint32_t hash, float x, float y, float z)
{
  uint32_t h = hash & 15u;
  float u = h < 8u ? x : y;
  float vt = ELEM(h, 12u, 14u) ? x : z;
  float v = h < 4u ? y : vt;
  return negate_if(u, h & 1u) + negate_if(v, h & 2u);
}

BLI_INLINE float noise_grad(uint32_t hash, float x, float y, float z, float w)
{
  uint32_t h = hash & 31u;
  float u = h < 24u ? x : y;
  float v = h < 16u ? y : z;
  float s = h < 8u ? z : w;
  return negate_if(u, h & 1u) + negate_if(v, h & 2u) + negate_if(s, h & 4u);
}

BLI_INLINE float floor_fraction(float x, int &i)
{
  i = int(x) - ((x < 0) ? 1 : 0);
  return x - i;
}

BLI_INLINE float perlin_noise(float position)
{
  int X;

  float fx = floor_fraction(position, X);

  float u = fade(fx);

  float r = mix(noise_grad(hash(X), fx), noise_grad(hash(X + 1), fx - 1.0), u);

  return r;
}

BLI_INLINE float perlin_noise(float2 position)
{
  int X, Y;

  float fx = floor_fraction(position.x, X);
  float fy = floor_fraction(position.y, Y);

  float u = fade(fx);
  float v = fade(fy);

  float r = mix(noise_grad(hash(X, Y), fx, fy),
                noise_grad(hash(X + 1, Y), fx - 1.0, fy),
                noise_grad(hash(X, Y + 1), fx, fy - 1.0),
                noise_grad(hash(X + 1, Y + 1), fx - 1.0, fy - 1.0),
                u,
                v);

  return r;
}

BLI_INLINE float perlin_noise(float3 position)
{
  int X, Y, Z;

  float fx = floor_fraction(position.x, X);
  float fy = floor_fraction(position.y, Y);
  float fz = floor_fraction(position.z, Z);

  float u = fade(fx);
  float v = fade(fy);
  float w = fade(fz);

  float r = mix(noise_grad(hash(X, Y, Z), fx, fy, fz),
                noise_grad(hash(X + 1, Y, Z), fx - 1, fy, fz),
                noise_grad(hash(X, Y + 1, Z), fx, fy - 1, fz),
                noise_grad(hash(X + 1, Y + 1, Z), fx - 1, fy - 1, fz),
                noise_grad(hash(X, Y, Z + 1), fx, fy, fz - 1),
                noise_grad(hash(X + 1, Y, Z + 1), fx - 1, fy, fz - 1),
                noise_grad(hash(X, Y + 1, Z + 1), fx, fy - 1, fz - 1),
                noise_grad(hash(X + 1, Y + 1, Z + 1), fx - 1, fy - 1, fz - 1),
                u,
                v,
                w);

  return r;
}

BLI_INLINE float perlin_noise(float4 position)
{
  int X, Y, Z, W;

  float fx = floor_fraction(position.x, X);
  float fy = floor_fraction(position.y, Y);
  float fz = floor_fraction(position.z, Z);
  float fw = floor_fraction(position.w, W);

  float u = fade(fx);
  float v = fade(fy);
  float t = fade(fz);
  float s = fade(fw);

  float r = mix(
      noise_grad(hash(X, Y, Z, W), fx, fy, fz, fw),
      noise_grad(hash(X + 1, Y, Z, W), fx - 1.0, fy, fz, fw),
      noise_grad(hash(X, Y + 1, Z, W), fx, fy - 1.0, fz, fw),
      noise_grad(hash(X + 1, Y + 1, Z, W), fx - 1.0, fy - 1.0, fz, fw),
      noise_grad(hash(X, Y, Z + 1, W), fx, fy, fz - 1.0, fw),
      noise_grad(hash(X + 1, Y, Z + 1, W), fx - 1.0, fy, fz - 1.0, fw),
      noise_grad(hash(X, Y + 1, Z + 1, W), fx, fy - 1.0, fz - 1.0, fw),
      noise_grad(hash(X + 1, Y + 1, Z + 1, W), fx - 1.0, fy - 1.0, fz - 1.0, fw),
      noise_grad(hash(X, Y, Z, W + 1), fx, fy, fz, fw - 1.0),
      noise_grad(hash(X + 1, Y, Z, W + 1), fx - 1.0, fy, fz, fw - 1.0),
      noise_grad(hash(X, Y + 1, Z, W + 1), fx, fy - 1.0, fz, fw - 1.0),
      noise_grad(hash(X + 1, Y + 1, Z, W + 1), fx - 1.0, fy - 1.0, fz, fw - 1.0),
      noise_grad(hash(X, Y, Z + 1, W + 1), fx, fy, fz - 1.0, fw - 1.0),
      noise_grad(hash(X + 1, Y, Z + 1, W + 1), fx - 1.0, fy, fz - 1.0, fw - 1.0),
      noise_grad(hash(X, Y + 1, Z + 1, W + 1), fx, fy - 1.0, fz - 1.0, fw - 1.0),
      noise_grad(hash(X + 1, Y + 1, Z + 1, W + 1), fx - 1.0, fy - 1.0, fz - 1.0, fw - 1.0),
      u,
      v,
      t,
      s);

  return r;
}

/* Signed versions of perlin noise in the range [-1, 1]. The scale values were computed
 * experimentally by the OSL developers to remap the noise output to the correct range. */

float perlin_signed(float position)
{
  return perlin_noise(position) * 0.2500f;
}

float perlin_signed(float2 position)
{
  return perlin_noise(position) * 0.6616f;
}

float perlin_signed(float3 position)
{
  return perlin_noise(position) * 0.9820f;
}

float perlin_signed(float4 position)
{
  return perlin_noise(position) * 0.8344f;
}

/* Positive versions of perlin noise in the range [0, 1]. */

float perlin(float position)
{
  return perlin_signed(position) / 2.0f + 0.5f;
}

float perlin(float2 position)
{
  return perlin_signed(position) / 2.0f + 0.5f;
}

float perlin(float3 position)
{
  return perlin_signed(position) / 2.0f + 0.5f;
}

float perlin(float4 position)
{
  return perlin_signed(position) / 2.0f + 0.5f;
}

/* Positive fractal perlin noise. */

template<typename T> float perlin_fractal_template(T position, float octaves, float roughness)
{
  float fscale = 1.0f;
  float amp = 1.0f;
  float maxamp = 0.0f;
  float sum = 0.0f;
  octaves = CLAMPIS(octaves, 0.0f, 15.0f);
  int n = int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = perlin(fscale * position);
    sum += t * amp;
    maxamp += amp;
    amp *= CLAMPIS(roughness, 0.0f, 1.0f);
    fscale *= 2.0f;
  }
  float rmd = octaves - std::floor(octaves);
  if (rmd == 0.0f) {
    return sum / maxamp;
  }

  float t = perlin(fscale * position);
  float sum2 = sum + t * amp;
  sum /= maxamp;
  sum2 /= maxamp + amp;
  return (1.0f - rmd) * sum + rmd * sum2;
}

float perlin_fractal(float position, float octaves, float roughness)
{
  return perlin_fractal_template(position, octaves, roughness);
}

float perlin_fractal(float2 position, float octaves, float roughness)
{
  return perlin_fractal_template(position, octaves, roughness);
}

float perlin_fractal(float3 position, float octaves, float roughness)
{
  return perlin_fractal_template(position, octaves, roughness);
}

float perlin_fractal(float4 position, float octaves, float roughness)
{
  return perlin_fractal_template(position, octaves, roughness);
}

/* The following offset functions generate random offsets to be added to
 * positions to act as a seed since the noise functions don't have seed values.
 * The offset's components are in the range [100, 200], not too high to cause
 * bad precision and not too small to be noticeable. We use float seed because
 * OSL only supports float hashes and we need to maintain compatibility with it.
 */

BLI_INLINE float random_float_offset(float seed)
{
  return 100.0f + hash_float_to_float(seed) * 100.0f;
}

BLI_INLINE float2 random_float2_offset(float seed)
{
  return float2(100.0f + hash_float_to_float(float2(seed, 0.0f)) * 100.0f,
                100.0f + hash_float_to_float(float2(seed, 1.0f)) * 100.0f);
}

BLI_INLINE float3 random_float3_offset(float seed)
{
  return float3(100.0f + hash_float_to_float(float2(seed, 0.0f)) * 100.0f,
                100.0f + hash_float_to_float(float2(seed, 1.0f)) * 100.0f,
                100.0f + hash_float_to_float(float2(seed, 2.0f)) * 100.0f);
}

BLI_INLINE float4 random_float4_offset(float seed)
{
  return float4(100.0f + hash_float_to_float(float2(seed, 0.0f)) * 100.0f,
                100.0f + hash_float_to_float(float2(seed, 1.0f)) * 100.0f,
                100.0f + hash_float_to_float(float2(seed, 2.0f)) * 100.0f,
                100.0f + hash_float_to_float(float2(seed, 3.0f)) * 100.0f);
}

/* Perlin noises to be added to the position to distort other noises. */

BLI_INLINE float perlin_distortion(float position, float strength)
{
  return perlin_signed(position + random_float_offset(0.0)) * strength;
}

BLI_INLINE float2 perlin_distortion(float2 position, float strength)
{
  return float2(perlin_signed(position + random_float2_offset(0.0f)) * strength,
                perlin_signed(position + random_float2_offset(1.0f)) * strength);
}

BLI_INLINE float3 perlin_distortion(float3 position, float strength)
{
  return float3(perlin_signed(position + random_float3_offset(0.0f)) * strength,
                perlin_signed(position + random_float3_offset(1.0f)) * strength,
                perlin_signed(position + random_float3_offset(2.0f)) * strength);
}

BLI_INLINE float4 perlin_distortion(float4 position, float strength)
{
  return float4(perlin_signed(position + random_float4_offset(0.0f)) * strength,
                perlin_signed(position + random_float4_offset(1.0f)) * strength,
                perlin_signed(position + random_float4_offset(2.0f)) * strength,
                perlin_signed(position + random_float4_offset(3.0f)) * strength);
}

/* Positive distorted fractal perlin noise. */

float perlin_fractal_distorted(float position, float octaves, float roughness, float distortion)
{
  position += perlin_distortion(position, distortion);
  return perlin_fractal(position, octaves, roughness);
}

float perlin_fractal_distorted(float2 position, float octaves, float roughness, float distortion)
{
  position += perlin_distortion(position, distortion);
  return perlin_fractal(position, octaves, roughness);
}

float perlin_fractal_distorted(float3 position, float octaves, float roughness, float distortion)
{
  position += perlin_distortion(position, distortion);
  return perlin_fractal(position, octaves, roughness);
}

float perlin_fractal_distorted(float4 position, float octaves, float roughness, float distortion)
{
  position += perlin_distortion(position, distortion);
  return perlin_fractal(position, octaves, roughness);
}

/* Positive distorted fractal perlin noise that outputs a float3. The arbitrary seeds are for
 * compatibility with shading functions. */

float3 perlin_float3_fractal_distorted(float position,
                                       float octaves,
                                       float roughness,
                                       float distortion)
{
  position += perlin_distortion(position, distortion);
  return float3(perlin_fractal(position, octaves, roughness),
                perlin_fractal(position + random_float_offset(1.0f), octaves, roughness),
                perlin_fractal(position + random_float_offset(2.0f), octaves, roughness));
}

float3 perlin_float3_fractal_distorted(float2 position,
                                       float octaves,
                                       float roughness,
                                       float distortion)
{
  position += perlin_distortion(position, distortion);
  return float3(perlin_fractal(position, octaves, roughness),
                perlin_fractal(position + random_float2_offset(2.0f), octaves, roughness),
                perlin_fractal(position + random_float2_offset(3.0f), octaves, roughness));
}

float3 perlin_float3_fractal_distorted(float3 position,
                                       float octaves,
                                       float roughness,
                                       float distortion)
{
  position += perlin_distortion(position, distortion);
  return float3(perlin_fractal(position, octaves, roughness),
                perlin_fractal(position + random_float3_offset(3.0f), octaves, roughness),
                perlin_fractal(position + random_float3_offset(4.0f), octaves, roughness));
}

float3 perlin_float3_fractal_distorted(float4 position,
                                       float octaves,
                                       float roughness,
                                       float distortion)
{
  position += perlin_distortion(position, distortion);
  return float3(perlin_fractal(position, octaves, roughness),
                perlin_fractal(position + random_float4_offset(4.0f), octaves, roughness),
                perlin_fractal(position + random_float4_offset(5.0f), octaves, roughness));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Musgrave Noise
 * \{ */

float musgrave_fBm(const float co,
                   const float H,
                   const float lacunarity,
                   const float octaves_unclamped)
{
  /* From "Texturing and Modelling: A procedural approach". */

  float p = co;
  float value = 0.0f;
  float pwr = 1.0f;
  const float pwHL = std::pow(lacunarity, -H);
  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; i < int(octaves); i++) {
    value += perlin_signed(p) * pwr;
    pwr *= pwHL;
    p *= lacunarity;
  }

  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    value += rmd * perlin_signed(p) * pwr;
  }

  return value;
}

float musgrave_multi_fractal(const float co,
                             const float H,
                             const float lacunarity,
                             const float octaves_unclamped)
{
  float p = co;
  float value = 1.0f;
  float pwr = 1.0f;
  const float pwHL = std::pow(lacunarity, -H);
  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; i < int(octaves); i++) {
    value *= (pwr * perlin_signed(p) + 1.0f);
    pwr *= pwHL;
    p *= lacunarity;
  }

  const float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    value *= (rmd * pwr * perlin_signed(p) + 1.0f); /* correct? */
  }

  return value;
}

float musgrave_hetero_terrain(const float co,
                              const float H,
                              const float lacunarity,
                              const float octaves_unclamped,
                              const float offset)
{
  float p = co;
  const float pwHL = std::pow(lacunarity, -H);
  float pwr = pwHL;
  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  /* First unscaled octave of function; later octaves are scaled. */
  float value = offset + perlin_signed(p);
  p *= lacunarity;

  for (int i = 1; i < int(octaves); i++) {
    float increment = (perlin_signed(p) + offset) * pwr * value;
    value += increment;
    pwr *= pwHL;
    p *= lacunarity;
  }

  const float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    float increment = (perlin_signed(p) + offset) * pwr * value;
    value += rmd * increment;
  }

  return value;
}

float musgrave_hybrid_multi_fractal(const float co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves_unclamped,
                                    const float offset,
                                    const float gain)
{
  float p = co;
  const float pwHL = std::pow(lacunarity, -H);

  float pwr = 1.0f;
  float value = 0.0f;
  float weight = 1.0f;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; (weight > 0.001f) && (i < int(octaves)); i++) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }

    float signal = (perlin_signed(p) + offset) * pwr;
    pwr *= pwHL;
    value += weight * signal;
    weight *= gain * signal;
    p *= lacunarity;
  }

  const float rmd = octaves - floorf(octaves);
  if ((rmd != 0.0f) && (weight > 0.001f)) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }
    float signal = (perlin_signed(p) + offset) * pwr;
    value += rmd * weight * signal;
  }

  return value;
}

float musgrave_ridged_multi_fractal(const float co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves_unclamped,
                                    const float offset,
                                    const float gain)
{
  float p = co;
  const float pwHL = std::pow(lacunarity, -H);
  float pwr = pwHL;

  float signal = offset - std::abs(perlin_signed(p));
  signal *= signal;
  float value = signal;
  float weight = 1.0f;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; i < int(octaves); i++) {
    p *= lacunarity;
    weight = CLAMPIS(signal * gain, 0.0f, 1.0f);
    signal = offset - std::abs(perlin_signed(p));
    signal *= signal;
    signal *= weight;
    value += signal * pwr;
    pwr *= pwHL;
  }

  return value;
}

float musgrave_fBm(const float2 co,
                   const float H,
                   const float lacunarity,
                   const float octaves_unclamped)
{
  /* From "Texturing and Modelling: A procedural approach". */

  float2 p = co;
  float value = 0.0f;
  float pwr = 1.0f;
  const float pwHL = std::pow(lacunarity, -H);
  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; i < int(octaves); i++) {
    value += perlin_signed(p) * pwr;
    pwr *= pwHL;
    p *= lacunarity;
  }

  const float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    value += rmd * perlin_signed(p) * pwr;
  }

  return value;
}

float musgrave_multi_fractal(const float2 co,
                             const float H,
                             const float lacunarity,
                             const float octaves_unclamped)
{
  float2 p = co;
  float value = 1.0f;
  float pwr = 1.0f;
  const float pwHL = std::pow(lacunarity, -H);
  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; i < int(octaves); i++) {
    value *= (pwr * perlin_signed(p) + 1.0f);
    pwr *= pwHL;
    p *= lacunarity;
  }

  const float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    value *= (rmd * pwr * perlin_signed(p) + 1.0f); /* correct? */
  }

  return value;
}

float musgrave_hetero_terrain(const float2 co,
                              const float H,
                              const float lacunarity,
                              const float octaves_unclamped,
                              const float offset)
{
  float2 p = co;
  const float pwHL = std::pow(lacunarity, -H);
  float pwr = pwHL;

  /* First unscaled octave of function; later octaves are scaled. */
  float value = offset + perlin_signed(p);
  p *= lacunarity;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; i < int(octaves); i++) {
    float increment = (perlin_signed(p) + offset) * pwr * value;
    value += increment;
    pwr *= pwHL;
    p *= lacunarity;
  }

  const float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    float increment = (perlin_signed(p) + offset) * pwr * value;
    value += rmd * increment;
  }

  return value;
}

float musgrave_hybrid_multi_fractal(const float2 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves_unclamped,
                                    const float offset,
                                    const float gain)
{
  float2 p = co;
  const float pwHL = std::pow(lacunarity, -H);

  float pwr = 1.0f;
  float value = 0.0f;
  float weight = 1.0f;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; (weight > 0.001f) && (i < int(octaves)); i++) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }

    float signal = (perlin_signed(p) + offset) * pwr;
    pwr *= pwHL;
    value += weight * signal;
    weight *= gain * signal;
    p *= lacunarity;
  }

  const float rmd = octaves - floorf(octaves);
  if ((rmd != 0.0f) && (weight > 0.001f)) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }
    float signal = (perlin_signed(p) + offset) * pwr;
    value += rmd * weight * signal;
  }

  return value;
}

float musgrave_ridged_multi_fractal(const float2 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves_unclamped,
                                    const float offset,
                                    const float gain)
{
  float2 p = co;
  const float pwHL = std::pow(lacunarity, -H);
  float pwr = pwHL;

  float signal = offset - std::abs(perlin_signed(p));
  signal *= signal;
  float value = signal;
  float weight = 1.0f;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; i < int(octaves); i++) {
    p *= lacunarity;
    weight = CLAMPIS(signal * gain, 0.0f, 1.0f);
    signal = offset - std::abs(perlin_signed(p));
    signal *= signal;
    signal *= weight;
    value += signal * pwr;
    pwr *= pwHL;
  }

  return value;
}

float musgrave_fBm(const float3 co,
                   const float H,
                   const float lacunarity,
                   const float octaves_unclamped)
{
  /* From "Texturing and Modelling: A procedural approach". */

  float3 p = co;
  float value = 0.0f;
  float pwr = 1.0f;
  const float pwHL = std::pow(lacunarity, -H);

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; i < int(octaves); i++) {
    value += perlin_signed(p) * pwr;
    pwr *= pwHL;
    p *= lacunarity;
  }

  const float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    value += rmd * perlin_signed(p) * pwr;
  }

  return value;
}

float musgrave_multi_fractal(const float3 co,
                             const float H,
                             const float lacunarity,
                             const float octaves_unclamped)
{
  float3 p = co;
  float value = 1.0f;
  float pwr = 1.0f;
  const float pwHL = std::pow(lacunarity, -H);

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; i < int(octaves); i++) {
    value *= (pwr * perlin_signed(p) + 1.0f);
    pwr *= pwHL;
    p *= lacunarity;
  }

  const float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    value *= (rmd * pwr * perlin_signed(p) + 1.0f); /* correct? */
  }

  return value;
}

float musgrave_hetero_terrain(const float3 co,
                              const float H,
                              const float lacunarity,
                              const float octaves_unclamped,
                              const float offset)
{
  float3 p = co;
  const float pwHL = std::pow(lacunarity, -H);
  float pwr = pwHL;

  /* first unscaled octave of function; later octaves are scaled */
  float value = offset + perlin_signed(p);
  p *= lacunarity;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; i < int(octaves); i++) {
    float increment = (perlin_signed(p) + offset) * pwr * value;
    value += increment;
    pwr *= pwHL;
    p *= lacunarity;
  }

  const float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    float increment = (perlin_signed(p) + offset) * pwr * value;
    value += rmd * increment;
  }

  return value;
}

float musgrave_hybrid_multi_fractal(const float3 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves_unclamped,
                                    const float offset,
                                    const float gain)
{
  float3 p = co;
  const float pwHL = std::pow(lacunarity, -H);

  float pwr = 1.0f;
  float value = 0.0f;
  float weight = 1.0f;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; (weight > 0.001f) && (i < int(octaves)); i++) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }

    float signal = (perlin_signed(p) + offset) * pwr;
    pwr *= pwHL;
    value += weight * signal;
    weight *= gain * signal;
    p *= lacunarity;
  }

  const float rmd = octaves - floorf(octaves);
  if ((rmd != 0.0f) && (weight > 0.001f)) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }
    float signal = (perlin_signed(p) + offset) * pwr;
    value += rmd * weight * signal;
  }

  return value;
}

float musgrave_ridged_multi_fractal(const float3 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves_unclamped,
                                    const float offset,
                                    const float gain)
{
  float3 p = co;
  const float pwHL = std::pow(lacunarity, -H);
  float pwr = pwHL;

  float signal = offset - std::abs(perlin_signed(p));
  signal *= signal;
  float value = signal;
  float weight = 1.0f;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; i < int(octaves); i++) {
    p *= lacunarity;
    weight = CLAMPIS(signal * gain, 0.0f, 1.0f);
    signal = offset - std::abs(perlin_signed(p));
    signal *= signal;
    signal *= weight;
    value += signal * pwr;
    pwr *= pwHL;
  }

  return value;
}

float musgrave_fBm(const float4 co,
                   const float H,
                   const float lacunarity,
                   const float octaves_unclamped)
{
  /* From "Texturing and Modelling: A procedural approach". */

  float4 p = co;
  float value = 0.0f;
  float pwr = 1.0f;
  const float pwHL = std::pow(lacunarity, -H);

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; i < int(octaves); i++) {
    value += perlin_signed(p) * pwr;
    pwr *= pwHL;
    p *= lacunarity;
  }

  const float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    value += rmd * perlin_signed(p) * pwr;
  }

  return value;
}

float musgrave_multi_fractal(const float4 co,
                             const float H,
                             const float lacunarity,
                             const float octaves_unclamped)
{
  float4 p = co;
  float value = 1.0f;
  float pwr = 1.0f;
  const float pwHL = std::pow(lacunarity, -H);

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; i < int(octaves); i++) {
    value *= (pwr * perlin_signed(p) + 1.0f);
    pwr *= pwHL;
    p *= lacunarity;
  }

  const float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    value *= (rmd * pwr * perlin_signed(p) + 1.0f); /* correct? */
  }

  return value;
}

float musgrave_hetero_terrain(const float4 co,
                              const float H,
                              const float lacunarity,
                              const float octaves_unclamped,
                              const float offset)
{
  float4 p = co;
  const float pwHL = std::pow(lacunarity, -H);
  float pwr = pwHL;

  /* first unscaled octave of function; later octaves are scaled */
  float value = offset + perlin_signed(p);
  p *= lacunarity;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; i < int(octaves); i++) {
    float increment = (perlin_signed(p) + offset) * pwr * value;
    value += increment;
    pwr *= pwHL;
    p *= lacunarity;
  }

  const float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    float increment = (perlin_signed(p) + offset) * pwr * value;
    value += rmd * increment;
  }

  return value;
}

float musgrave_hybrid_multi_fractal(const float4 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves_unclamped,
                                    const float offset,
                                    const float gain)
{
  float4 p = co;
  const float pwHL = std::pow(lacunarity, -H);

  float pwr = 1.0f;
  float value = 0.0f;
  float weight = 1.0f;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; (weight > 0.001f) && (i < int(octaves)); i++) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }

    float signal = (perlin_signed(p) + offset) * pwr;
    pwr *= pwHL;
    value += weight * signal;
    weight *= gain * signal;
    p *= lacunarity;
  }

  const float rmd = octaves - floorf(octaves);
  if ((rmd != 0.0f) && (weight > 0.001f)) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }
    float signal = (perlin_signed(p) + offset) * pwr;
    value += rmd * weight * signal;
  }

  return value;
}

float musgrave_ridged_multi_fractal(const float4 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves_unclamped,
                                    const float offset,
                                    const float gain)
{
  float4 p = co;
  const float pwHL = std::pow(lacunarity, -H);
  float pwr = pwHL;

  float signal = offset - std::abs(perlin_signed(p));
  signal *= signal;
  float value = signal;
  float weight = 1.0f;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; i < int(octaves); i++) {
    p *= lacunarity;
    weight = CLAMPIS(signal * gain, 0.0f, 1.0f);
    signal = offset - std::abs(perlin_signed(p));
    signal *= signal;
    signal *= weight;
    value += signal * pwr;
    pwr *= pwHL;
  }

  return value;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Voronoi Noise
 *
 * \note Ported from Cycles code.
 *
 * Original code is under the MIT License, Copyright (c) 2013 Inigo Quilez.
 *
 * Smooth Voronoi:
 *
 * - https://wiki.blender.org/wiki/User:OmarSquircleArt/GSoC2019/Documentation/Smooth_Voronoi
 *
 * Distance To Edge based on:
 *
 * - https://www.iquilezles.org/www/articles/voronoilines/voronoilines.htm
 * - https://www.shadertoy.com/view/ldl3W8
 *
 * With optimization to change -2..2 scan window to -1..1 for better performance,
 * as explained in https://www.shadertoy.com/view/llG3zy.
 * \{ */

/* Ensure to align with DNA. */

enum {
  NOISE_SHD_VORONOI_EUCLIDEAN = 0,
  NOISE_SHD_VORONOI_MANHATTAN = 1,
  NOISE_SHD_VORONOI_CHEBYCHEV = 2,
  NOISE_SHD_VORONOI_MINKOWSKI = 3,
};

enum {
  NOISE_SHD_VORONOI_F1 = 0,
  NOISE_SHD_VORONOI_F2 = 1,
  NOISE_SHD_VORONOI_SMOOTH_F1 = 2,
  NOISE_SHD_VORONOI_DISTANCE_TO_EDGE = 3,
  NOISE_SHD_VORONOI_N_SPHERE_RADIUS = 4,
};

/* ***** Distances ***** */

float voronoi_distance(const float a, const float b)
{
  return std::abs(b - a);
}

float voronoi_distance(const float2 a, const float2 b, const VoronoiParams &params)
{
  switch (params.metric) {
    case NOISE_SHD_VORONOI_EUCLIDEAN:
      return math::distance(a, b);
    case NOISE_SHD_VORONOI_MANHATTAN:
      return std::abs(a.x - b.x) + std::abs(a.y - b.y);
    case NOISE_SHD_VORONOI_CHEBYCHEV:
      return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
    case NOISE_SHD_VORONOI_MINKOWSKI:
      return std::pow(std::pow(std::abs(a.x - b.x), params.exponent) +
                          std::pow(std::abs(a.y - b.y), params.exponent),
                      1.0f / params.exponent);
    default:
      BLI_assert_unreachable();
      break;
  }
  return 0.0f;
}

float voronoi_distance(const float3 a, const float3 b, const VoronoiParams &params)
{
  switch (params.metric) {
    case NOISE_SHD_VORONOI_EUCLIDEAN:
      return math::distance(a, b);
    case NOISE_SHD_VORONOI_MANHATTAN:
      return std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z);
    case NOISE_SHD_VORONOI_CHEBYCHEV:
      return std::max(std::abs(a.x - b.x), std::max(std::abs(a.y - b.y), std::abs(a.z - b.z)));
    case NOISE_SHD_VORONOI_MINKOWSKI:
      return std::pow(std::pow(std::abs(a.x - b.x), params.exponent) +
                          std::pow(std::abs(a.y - b.y), params.exponent) +
                          std::pow(std::abs(a.z - b.z), params.exponent),
                      1.0f / params.exponent);
    default:
      BLI_assert_unreachable();
      break;
  }
  return 0.0f;
}

float voronoi_distance(const float4 a, const float4 b, const VoronoiParams &params)
{
  switch (params.metric) {
    case NOISE_SHD_VORONOI_EUCLIDEAN:
      return math::distance(a, b);
    case NOISE_SHD_VORONOI_MANHATTAN:
      return std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z) + std::abs(a.w - b.w);
    case NOISE_SHD_VORONOI_CHEBYCHEV:
      return std::max(
          std::abs(a.x - b.x),
          std::max(std::abs(a.y - b.y), std::max(std::abs(a.z - b.z), std::abs(a.w - b.w))));
    case NOISE_SHD_VORONOI_MINKOWSKI:
      return std::pow(std::pow(std::abs(a.x - b.x), params.exponent) +
                          std::pow(std::abs(a.y - b.y), params.exponent) +
                          std::pow(std::abs(a.z - b.z), params.exponent) +
                          std::pow(std::abs(a.w - b.w), params.exponent),
                      1.0f / params.exponent);
    default:
      BLI_assert_unreachable();
      break;
  }
  return 0.0f;
}

/* **** 1D Voronoi **** */

float4 voronoi_position(const float coord)
{
  return {0.0f, 0.0f, 0.0f, coord};
}

VoronoiOutput voronoi_f1(const VoronoiParams &params, const float coord)
{
  float cellPosition = floorf(coord);
  float localPosition = coord - cellPosition;

  float minDistance = 8.0f;
  float targetOffset = 0.0f;
  float targetPosition = 0.0f;
  for (int i = -1; i <= 1; i++) {
    float cellOffset = i;
    float pointPosition = cellOffset +
                          hash_float_to_float(cellPosition + cellOffset) * params.randomness;
    float distanceToPoint = voronoi_distance(pointPosition, localPosition);
    if (distanceToPoint < minDistance) {
      targetOffset = cellOffset;
      minDistance = distanceToPoint;
      targetPosition = pointPosition;
    }
  }

  VoronoiOutput octave;
  octave.distance = minDistance;
  octave.color = hash_float_to_float3(cellPosition + targetOffset);
  octave.position = voronoi_position(targetPosition + cellPosition);
  return octave;
}

VoronoiOutput voronoi_smooth_f1(const VoronoiParams &params,
                                const float coord,
                                const bool calc_color)
{
  float cellPosition = floorf(coord);
  float localPosition = coord - cellPosition;

  float smoothDistance = 8.0f;
  float smoothPosition = 0.0f;
  float3 smoothColor = {0.0f, 0.0f, 0.0f};
  for (int i = -2; i <= 2; i++) {
    float cellOffset = i;
    float pointPosition = cellOffset +
                          hash_float_to_float(cellPosition + cellOffset) * params.randomness;
    float distanceToPoint = voronoi_distance(pointPosition, localPosition);
    float h = smoothstep(
        0.0f, 1.0f, 0.5f + 0.5f * (smoothDistance - distanceToPoint) / params.smoothness);
    float correctionFactor = params.smoothness * h * (1.0f - h);
    smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
    correctionFactor /= 1.0f + 3.0f * params.smoothness;
    float3 cellColor = hash_float_to_float3(cellPosition + cellOffset);
    if (calc_color) {
      /* Only compute Color output if necessary, as it is very expensive. */
      smoothColor = mix(smoothColor, cellColor, h) - correctionFactor;
    }
    smoothPosition = mix(smoothPosition, pointPosition, h) - correctionFactor;
  }

  VoronoiOutput octave;
  octave.distance = smoothDistance;
  octave.color = smoothColor;
  octave.position = voronoi_position(cellPosition + smoothPosition);
  return octave;
}

VoronoiOutput voronoi_f2(const VoronoiParams &params, const float coord)
{
  float cellPosition = floorf(coord);
  float localPosition = coord - cellPosition;

  float distanceF1 = 8.0f;
  float distanceF2 = 8.0f;
  float offsetF1 = 0.0f;
  float positionF1 = 0.0f;
  float offsetF2 = 0.0f;
  float positionF2 = 0.0f;
  for (int i = -1; i <= 1; i++) {
    float cellOffset = i;
    float pointPosition = cellOffset +
                          hash_float_to_float(cellPosition + cellOffset) * params.randomness;
    float distanceToPoint = voronoi_distance(pointPosition, localPosition);
    if (distanceToPoint < distanceF1) {
      distanceF2 = distanceF1;
      distanceF1 = distanceToPoint;
      offsetF2 = offsetF1;
      offsetF1 = cellOffset;
      positionF2 = positionF1;
      positionF1 = pointPosition;
    }
    else if (distanceToPoint < distanceF2) {
      distanceF2 = distanceToPoint;
      offsetF2 = cellOffset;
      positionF2 = pointPosition;
    }
  }

  VoronoiOutput octave;
  octave.distance = distanceF2;
  octave.color = hash_float_to_float3(cellPosition + offsetF2);
  octave.position = voronoi_position(positionF2 + cellPosition);
  return octave;
}

float voronoi_distance_to_edge(const VoronoiParams &params, const float coord)
{
  float cellPosition = floorf(coord);
  float localPosition = coord - cellPosition;

  float midPointPosition = hash_float_to_float(cellPosition) * params.randomness;
  float leftPointPosition = -1.0f + hash_float_to_float(cellPosition - 1.0f) * params.randomness;
  float rightPointPosition = 1.0f + hash_float_to_float(cellPosition + 1.0f) * params.randomness;
  float distanceToMidLeft = fabsf((midPointPosition + leftPointPosition) / 2.0f - localPosition);
  float distanceToMidRight = fabsf((midPointPosition + rightPointPosition) / 2.0f - localPosition);

  return math::min(distanceToMidLeft, distanceToMidRight);
}

float voronoi_n_sphere_radius(const VoronoiParams &params, const float coord)
{
  float cellPosition = floorf(coord);
  float localPosition = coord - cellPosition;

  float closestPoint = 0.0f;
  float closestPointOffset = 0.0f;
  float minDistance = 8.0f;
  for (int i = -1; i <= 1; i++) {
    float cellOffset = i;
    float pointPosition = cellOffset +
                          hash_float_to_float(cellPosition + cellOffset) * params.randomness;
    float distanceToPoint = fabsf(pointPosition - localPosition);
    if (distanceToPoint < minDistance) {
      minDistance = distanceToPoint;
      closestPoint = pointPosition;
      closestPointOffset = cellOffset;
    }
  }

  minDistance = 8.0f;
  float closestPointToClosestPoint = 0.0f;
  for (int i = -1; i <= 1; i++) {
    if (i == 0) {
      continue;
    }
    float cellOffset = i + closestPointOffset;
    float pointPosition = cellOffset +
                          hash_float_to_float(cellPosition + cellOffset) * params.randomness;
    float distanceToPoint = fabsf(closestPoint - pointPosition);
    if (distanceToPoint < minDistance) {
      minDistance = distanceToPoint;
      closestPointToClosestPoint = pointPosition;
    }
  }

  return fabsf(closestPointToClosestPoint - closestPoint) / 2.0f;
}

/* **** 2D Voronoi **** */

float4 voronoi_position(const float2 coord)
{
  return {coord.x, coord.y, 0.0f, 0.0f};
}

VoronoiOutput voronoi_f1(const VoronoiParams &params, const float2 coord)
{
  float2 cellPosition = math::floor(coord);
  float2 localPosition = coord - cellPosition;

  float minDistance = 8.0f;
  float2 targetOffset = {0.0f, 0.0f};
  float2 targetPosition = {0.0f, 0.0f};
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      float2 cellOffset(i, j);
      float2 pointPosition = cellOffset +
                             hash_float_to_float2(cellPosition + cellOffset) * params.randomness;
      float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
      if (distanceToPoint < minDistance) {
        targetOffset = cellOffset;
        minDistance = distanceToPoint;
        targetPosition = pointPosition;
      }
    }
  }

  VoronoiOutput octave;
  octave.distance = minDistance;
  octave.color = hash_float_to_float3(cellPosition + targetOffset);
  octave.position = voronoi_position(targetPosition + cellPosition);
  return octave;
}

VoronoiOutput voronoi_smooth_f1(const VoronoiParams &params,
                                const float2 coord,
                                const bool calc_color)
{
  float2 cellPosition = math::floor(coord);
  float2 localPosition = coord - cellPosition;

  float smoothDistance = 8.0f;
  float3 smoothColor = {0.0f, 0.0f, 0.0f};
  float2 smoothPosition = {0.0f, 0.0f};
  for (int j = -2; j <= 2; j++) {
    for (int i = -2; i <= 2; i++) {
      float2 cellOffset(i, j);
      float2 pointPosition = cellOffset +
                             hash_float_to_float2(cellPosition + cellOffset) * params.randomness;
      float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
      float h = smoothstep(
          0.0f, 1.0f, 0.5f + 0.5f * (smoothDistance - distanceToPoint) / params.smoothness);
      float correctionFactor = params.smoothness * h * (1.0f - h);
      smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
      correctionFactor /= 1.0f + 3.0f * params.smoothness;
      if (calc_color) {
        /* Only compute Color output if necessary, as it is very expensive. */
        float3 cellColor = hash_float_to_float3(cellPosition + cellOffset);
        smoothColor = mix(smoothColor, cellColor, h) - correctionFactor;
      }
      smoothPosition = mix(smoothPosition, pointPosition, h) - correctionFactor;
    }
  }

  VoronoiOutput octave;
  octave.distance = smoothDistance;
  octave.color = smoothColor;
  octave.position = voronoi_position(cellPosition + smoothPosition);
  return octave;
}

VoronoiOutput voronoi_f2(const VoronoiParams &params, const float2 coord)
{
  float2 cellPosition = math::floor(coord);
  float2 localPosition = coord - cellPosition;

  float distanceF1 = 8.0f;
  float distanceF2 = 8.0f;
  float2 offsetF1 = {0.0f, 0.0f};
  float2 positionF1 = {0.0f, 0.0f};
  float2 offsetF2 = {0.0f, 0.0f};
  float2 positionF2 = {0.0f, 0.0f};
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      float2 cellOffset(i, j);
      float2 pointPosition = cellOffset +
                             hash_float_to_float2(cellPosition + cellOffset) * params.randomness;
      float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
      if (distanceToPoint < distanceF1) {
        distanceF2 = distanceF1;
        distanceF1 = distanceToPoint;
        offsetF2 = offsetF1;
        offsetF1 = cellOffset;
        positionF2 = positionF1;
        positionF1 = pointPosition;
      }
      else if (distanceToPoint < distanceF2) {
        distanceF2 = distanceToPoint;
        offsetF2 = cellOffset;
        positionF2 = pointPosition;
      }
    }
  }

  VoronoiOutput octave;
  octave.distance = distanceF2;
  octave.color = hash_float_to_float3(cellPosition + offsetF2);
  octave.position = voronoi_position(positionF2 + cellPosition);
  return octave;
}

float voronoi_distance_to_edge(const VoronoiParams &params, const float2 coord)
{
  float2 cellPosition = math::floor(coord);
  float2 localPosition = coord - cellPosition;

  float2 vectorToClosest = {0.0f, 0.0f};
  float minDistance = 8.0f;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      float2 cellOffset(i, j);
      float2 vectorToPoint = cellOffset +
                             hash_float_to_float2(cellPosition + cellOffset) * params.randomness -
                             localPosition;
      float distanceToPoint = math::dot(vectorToPoint, vectorToPoint);
      if (distanceToPoint < minDistance) {
        minDistance = distanceToPoint;
        vectorToClosest = vectorToPoint;
      }
    }
  }

  minDistance = 8.0f;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      float2 cellOffset(i, j);
      float2 vectorToPoint = cellOffset +
                             hash_float_to_float2(cellPosition + cellOffset) * params.randomness -
                             localPosition;
      float2 perpendicularToEdge = vectorToPoint - vectorToClosest;
      if (math::dot(perpendicularToEdge, perpendicularToEdge) > 0.0001f) {
        float distanceToEdge = math::dot((vectorToClosest + vectorToPoint) / 2.0f,
                                         math::normalize(perpendicularToEdge));
        minDistance = math::min(minDistance, distanceToEdge);
      }
    }
  }

  return minDistance;
}

float voronoi_n_sphere_radius(const VoronoiParams &params, const float2 coord)
{
  float2 cellPosition = math::floor(coord);
  float2 localPosition = coord - cellPosition;

  float2 closestPoint = {0.0f, 0.0f};
  float2 closestPointOffset = {0.0f, 0.0f};
  float minDistance = 8.0f;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      float2 cellOffset(i, j);
      float2 pointPosition = cellOffset +
                             hash_float_to_float2(cellPosition + cellOffset) * params.randomness;
      float distanceToPoint = math::distance(pointPosition, localPosition);
      if (distanceToPoint < minDistance) {
        minDistance = distanceToPoint;
        closestPoint = pointPosition;
        closestPointOffset = cellOffset;
      }
    }
  }

  minDistance = 8.0f;
  float2 closestPointToClosestPoint = {0.0f, 0.0f};
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      if (i == 0 && j == 0) {
        continue;
      }
      float2 cellOffset = float2(i, j) + closestPointOffset;
      float2 pointPosition = cellOffset +
                             hash_float_to_float2(cellPosition + cellOffset) * params.randomness;
      float distanceToPoint = math::distance(closestPoint, pointPosition);
      if (distanceToPoint < minDistance) {
        minDistance = distanceToPoint;
        closestPointToClosestPoint = pointPosition;
      }
    }
  }

  return math::distance(closestPointToClosestPoint, closestPoint) / 2.0f;
}

/* **** 3D Voronoi **** */

float4 voronoi_position(const float3 coord)
{
  return {coord.x, coord.y, coord.z, 0.0f};
}

VoronoiOutput voronoi_f1(const VoronoiParams &params, const float3 coord)
{
  float3 cellPosition = math::floor(coord);
  float3 localPosition = coord - cellPosition;

  float minDistance = 8.0f;
  float3 targetOffset = {0.0f, 0.0f, 0.0f};
  float3 targetPosition = {0.0f, 0.0f, 0.0f};
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        float3 cellOffset(i, j, k);
        float3 pointPosition = cellOffset +
                               hash_float_to_float3(cellPosition + cellOffset) * params.randomness;
        float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
        if (distanceToPoint < minDistance) {
          targetOffset = cellOffset;
          minDistance = distanceToPoint;
          targetPosition = pointPosition;
        }
      }
    }
  }

  VoronoiOutput octave;
  octave.distance = minDistance;
  octave.color = hash_float_to_float3(cellPosition + targetOffset);
  octave.position = voronoi_position(targetPosition + cellPosition);
  return octave;
}

VoronoiOutput voronoi_smooth_f1(const VoronoiParams &params,
                                const float3 coord,
                                const bool calc_color)
{
  float3 cellPosition = math::floor(coord);
  float3 localPosition = coord - cellPosition;

  float smoothDistance = 8.0f;
  float3 smoothColor = {0.0f, 0.0f, 0.0f};
  float3 smoothPosition = {0.0f, 0.0f, 0.0f};
  for (int k = -2; k <= 2; k++) {
    for (int j = -2; j <= 2; j++) {
      for (int i = -2; i <= 2; i++) {
        float3 cellOffset(i, j, k);
        float3 pointPosition = cellOffset +
                               hash_float_to_float3(cellPosition + cellOffset) * params.randomness;
        float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
        float h = smoothstep(
            0.0f, 1.0f, 0.5f + 0.5f * (smoothDistance - distanceToPoint) / params.smoothness);
        float correctionFactor = params.smoothness * h * (1.0f - h);
        smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
        correctionFactor /= 1.0f + 3.0f * params.smoothness;
        if (calc_color) {
          /* Only compute Color output if necessary, as it is very expensive. */
          float3 cellColor = hash_float_to_float3(cellPosition + cellOffset);
          smoothColor = mix(smoothColor, cellColor, h) - correctionFactor;
        }
        smoothPosition = mix(smoothPosition, pointPosition, h) - correctionFactor;
      }
    }
  }

  VoronoiOutput octave;
  octave.distance = smoothDistance;
  octave.color = smoothColor;
  octave.position = voronoi_position(cellPosition + smoothPosition);
  return octave;
}

VoronoiOutput voronoi_f2(const VoronoiParams &params, const float3 coord)
{
  float3 cellPosition = math::floor(coord);
  float3 localPosition = coord - cellPosition;

  float distanceF1 = 8.0f;
  float distanceF2 = 8.0f;
  float3 offsetF1 = {0.0f, 0.0f, 0.0f};
  float3 positionF1 = {0.0f, 0.0f, 0.0f};
  float3 offsetF2 = {0.0f, 0.0f, 0.0f};
  float3 positionF2 = {0.0f, 0.0f, 0.0f};
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        float3 cellOffset(i, j, k);
        float3 pointPosition = cellOffset +
                               hash_float_to_float3(cellPosition + cellOffset) * params.randomness;
        float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
        if (distanceToPoint < distanceF1) {
          distanceF2 = distanceF1;
          distanceF1 = distanceToPoint;
          offsetF2 = offsetF1;
          offsetF1 = cellOffset;
          positionF2 = positionF1;
          positionF1 = pointPosition;
        }
        else if (distanceToPoint < distanceF2) {
          distanceF2 = distanceToPoint;
          offsetF2 = cellOffset;
          positionF2 = pointPosition;
        }
      }
    }
  }

  VoronoiOutput octave;
  octave.distance = distanceF2;
  octave.color = hash_float_to_float3(cellPosition + offsetF2);
  octave.position = voronoi_position(positionF2 + cellPosition);
  return octave;
}

float voronoi_distance_to_edge(const VoronoiParams &params, const float3 coord)
{
  float3 cellPosition = math::floor(coord);
  float3 localPosition = coord - cellPosition;

  float3 vectorToClosest = {0.0f, 0.0f, 0.0f};
  float minDistance = 8.0f;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        float3 cellOffset(i, j, k);
        float3 vectorToPoint = cellOffset +
                               hash_float_to_float3(cellPosition + cellOffset) *
                                   params.randomness -
                               localPosition;
        float distanceToPoint = math::dot(vectorToPoint, vectorToPoint);
        if (distanceToPoint < minDistance) {
          minDistance = distanceToPoint;
          vectorToClosest = vectorToPoint;
        }
      }
    }
  }

  minDistance = 8.0f;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        float3 cellOffset(i, j, k);
        float3 vectorToPoint = cellOffset +
                               hash_float_to_float3(cellPosition + cellOffset) *
                                   params.randomness -
                               localPosition;
        float3 perpendicularToEdge = vectorToPoint - vectorToClosest;
        if (math::dot(perpendicularToEdge, perpendicularToEdge) > 0.0001f) {
          float distanceToEdge = math::dot((vectorToClosest + vectorToPoint) / 2.0f,
                                           math::normalize(perpendicularToEdge));
          minDistance = math::min(minDistance, distanceToEdge);
        }
      }
    }
  }

  return minDistance;
}

float voronoi_n_sphere_radius(const VoronoiParams &params, const float3 coord)
{
  float3 cellPosition = math::floor(coord);
  float3 localPosition = coord - cellPosition;

  float3 closestPoint = {0.0f, 0.0f, 0.0f};
  float3 closestPointOffset = {0.0f, 0.0f, 0.0f};
  float minDistance = 8.0f;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        float3 cellOffset(i, j, k);
        float3 pointPosition = cellOffset +
                               hash_float_to_float3(cellPosition + cellOffset) * params.randomness;
        float distanceToPoint = math::distance(pointPosition, localPosition);
        if (distanceToPoint < minDistance) {
          minDistance = distanceToPoint;
          closestPoint = pointPosition;
          closestPointOffset = cellOffset;
        }
      }
    }
  }

  minDistance = 8.0f;
  float3 closestPointToClosestPoint = {0.0f, 0.0f, 0.0f};
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        if (i == 0 && j == 0 && k == 0) {
          continue;
        }
        float3 cellOffset = float3(i, j, k) + closestPointOffset;
        float3 pointPosition = cellOffset +
                               hash_float_to_float3(cellPosition + cellOffset) * params.randomness;
        float distanceToPoint = math::distance(closestPoint, pointPosition);
        if (distanceToPoint < minDistance) {
          minDistance = distanceToPoint;
          closestPointToClosestPoint = pointPosition;
        }
      }
    }
  }

  return math::distance(closestPointToClosestPoint, closestPoint) / 2.0f;
}

/* **** 4D Voronoi **** */

float4 voronoi_position(const float4 coord)
{
  return coord;
}

VoronoiOutput voronoi_f1(const VoronoiParams &params, const float4 coord)
{
  float4 cellPosition = math::floor(coord);
  float4 localPosition = coord - cellPosition;

  float minDistance = 8.0f;
  float4 targetOffset = {0.0f, 0.0f, 0.0f, 0.0f};
  float4 targetPosition = {0.0f, 0.0f, 0.0f, 0.0f};
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          float4 cellOffset(i, j, k, u);
          float4 pointPosition = cellOffset + hash_float_to_float4(cellPosition + cellOffset) *
                                                  params.randomness;
          float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
          if (distanceToPoint < minDistance) {
            targetOffset = cellOffset;
            minDistance = distanceToPoint;
            targetPosition = pointPosition;
          }
        }
      }
    }
  }

  VoronoiOutput octave;
  octave.distance = minDistance;
  octave.color = hash_float_to_float3(cellPosition + targetOffset);
  octave.position = voronoi_position(targetPosition + cellPosition);
  return octave;
}

VoronoiOutput voronoi_smooth_f1(const VoronoiParams &params,
                                const float4 coord,
                                const bool calc_color)
{
  float4 cellPosition = math::floor(coord);
  float4 localPosition = coord - cellPosition;

  float smoothDistance = 8.0f;
  float3 smoothColor = {0.0f, 0.0f, 0.0f};
  float4 smoothPosition = {0.0f, 0.0f, 0.0f, 0.0f};
  for (int u = -2; u <= 2; u++) {
    for (int k = -2; k <= 2; k++) {
      for (int j = -2; j <= 2; j++) {
        for (int i = -2; i <= 2; i++) {
          float4 cellOffset(i, j, k, u);
          float4 pointPosition = cellOffset + hash_float_to_float4(cellPosition + cellOffset) *
                                                  params.randomness;
          float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
          float h = smoothstep(
              0.0f, 1.0f, 0.5f + 0.5f * (smoothDistance - distanceToPoint) / params.smoothness);
          float correctionFactor = params.smoothness * h * (1.0f - h);
          smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
          correctionFactor /= 1.0f + 3.0f * params.smoothness;
          if (calc_color) {
            /* Only compute Color output if necessary, as it is very expensive. */
            float3 cellColor = hash_float_to_float3(cellPosition + cellOffset);
            smoothColor = mix(smoothColor, cellColor, h) - correctionFactor;
          }
          smoothPosition = mix(smoothPosition, pointPosition, h) - correctionFactor;
        }
      }
    }
  }

  VoronoiOutput octave;
  octave.distance = smoothDistance;
  octave.color = smoothColor;
  octave.position = voronoi_position(cellPosition + smoothPosition);
  return octave;
}

VoronoiOutput voronoi_f2(const VoronoiParams &params, const float4 coord)
{
  float4 cellPosition = math::floor(coord);
  float4 localPosition = coord - cellPosition;

  float distanceF1 = 8.0f;
  float distanceF2 = 8.0f;
  float4 offsetF1 = {0.0f, 0.0f, 0.0f, 0.0f};
  float4 positionF1 = {0.0f, 0.0f, 0.0f, 0.0f};
  float4 offsetF2 = {0.0f, 0.0f, 0.0f, 0.0f};
  float4 positionF2 = {0.0f, 0.0f, 0.0f, 0.0f};
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          float4 cellOffset(i, j, k, u);
          float4 pointPosition = cellOffset + hash_float_to_float4(cellPosition + cellOffset) *
                                                  params.randomness;
          float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
          if (distanceToPoint < distanceF1) {
            distanceF2 = distanceF1;
            distanceF1 = distanceToPoint;
            offsetF2 = offsetF1;
            offsetF1 = cellOffset;
            positionF2 = positionF1;
            positionF1 = pointPosition;
          }
          else if (distanceToPoint < distanceF2) {
            distanceF2 = distanceToPoint;
            offsetF2 = cellOffset;
            positionF2 = pointPosition;
          }
        }
      }
    }
  }

  VoronoiOutput octave;
  octave.distance = distanceF2;
  octave.color = hash_float_to_float3(cellPosition + offsetF2);
  octave.position = voronoi_position(positionF2 + cellPosition);
  return octave;
}

float voronoi_distance_to_edge(const VoronoiParams &params, const float4 coord)
{
  float4 cellPosition = math::floor(coord);
  float4 localPosition = coord - cellPosition;

  float4 vectorToClosest = {0.0f, 0.0f, 0.0f, 0.0f};
  float minDistance = 8.0f;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          float4 cellOffset(i, j, k, u);
          float4 vectorToPoint = cellOffset +
                                 hash_float_to_float4(cellPosition + cellOffset) *
                                     params.randomness -
                                 localPosition;
          float distanceToPoint = math::dot(vectorToPoint, vectorToPoint);
          if (distanceToPoint < minDistance) {
            minDistance = distanceToPoint;
            vectorToClosest = vectorToPoint;
          }
        }
      }
    }
  }

  minDistance = 8.0f;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          float4 cellOffset(i, j, k, u);
          float4 vectorToPoint = cellOffset +
                                 hash_float_to_float4(cellPosition + cellOffset) *
                                     params.randomness -
                                 localPosition;
          float4 perpendicularToEdge = vectorToPoint - vectorToClosest;
          if (math::dot(perpendicularToEdge, perpendicularToEdge) > 0.0001f) {
            float distanceToEdge = math::dot((vectorToClosest + vectorToPoint) / 2.0f,
                                             math::normalize(perpendicularToEdge));
            minDistance = math::min(minDistance, distanceToEdge);
          }
        }
      }
    }
  }

  return minDistance;
}

float voronoi_n_sphere_radius(const VoronoiParams &params, const float4 coord)
{
  float4 cellPosition = math::floor(coord);
  float4 localPosition = coord - cellPosition;

  float4 closestPoint = {0.0f, 0.0f, 0.0f, 0.0f};
  float4 closestPointOffset = {0.0f, 0.0f, 0.0f, 0.0f};
  float minDistance = 8.0f;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          float4 cellOffset(i, j, k, u);
          float4 pointPosition = cellOffset + hash_float_to_float4(cellPosition + cellOffset) *
                                                  params.randomness;
          float distanceToPoint = math::distance(pointPosition, localPosition);
          if (distanceToPoint < minDistance) {
            minDistance = distanceToPoint;
            closestPoint = pointPosition;
            closestPointOffset = cellOffset;
          }
        }
      }
    }
  }

  minDistance = 8.0f;
  float4 closestPointToClosestPoint = {0.0f, 0.0f, 0.0f, 0.0f};
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          if (i == 0 && j == 0 && k == 0 && u == 0) {
            continue;
          }
          float4 cellOffset = float4(i, j, k, u) + closestPointOffset;
          float4 pointPosition = cellOffset + hash_float_to_float4(cellPosition + cellOffset) *
                                                  params.randomness;
          float distanceToPoint = math::distance(closestPoint, pointPosition);
          if (distanceToPoint < minDistance) {
            minDistance = distanceToPoint;
            closestPointToClosestPoint = pointPosition;
          }
        }
      }
    }
  }

  return math::distance(closestPointToClosestPoint, closestPoint) / 2.0f;
}

/* **** Fractal Voronoi **** */

template<typename T>
VoronoiOutput fractal_voronoi_x_fx(const VoronoiParams &params,
                                   const T coord,
                                   const bool calc_color /* Only used to optimize Smooth F1 */)
{
  float amplitude = 1.0f;
  float max_amplitude = 0.0f;
  float scale = 1.0f;

  VoronoiOutput output;
  const bool zero_input = params.detail == 0.0f || params.roughness == 0.0f ||
                          params.lacunarity == 0.0f;

  for (int i = 0; i <= ceilf(params.detail); ++i) {
    VoronoiOutput octave = (params.feature == NOISE_SHD_VORONOI_F1) ?
                               voronoi_f1(params, coord * scale) :
                           (params.feature == NOISE_SHD_VORONOI_SMOOTH_F1) ?
                               voronoi_smooth_f1(params, coord * scale, calc_color) :
                               voronoi_f2(params, coord * scale);

    if (zero_input) {
      max_amplitude = 1.0f;
      output = octave;
      break;
    }
    else if (i <= params.detail) {
      max_amplitude += amplitude;
      output.distance += octave.distance * amplitude;
      output.color += octave.color * amplitude;
      output.position = mix(output.position, octave.position / scale, amplitude);
      scale *= params.lacunarity;
      amplitude *= params.roughness;
    }
    else {
      float remainder = params.detail - floorf(params.detail);
      if (remainder != 0.0f) {
        max_amplitude = mix(max_amplitude, max_amplitude + amplitude, remainder);
        output.distance = mix(
            output.distance, output.distance + octave.distance * amplitude, remainder);
        output.color = mix(output.color, output.color + octave.color * amplitude, remainder);
        output.position = mix(
            output.position, mix(output.position, octave.position / scale, amplitude), remainder);
      }
    }
  }

  if (params.normalize) {
    output.distance /= max_amplitude * params.max_distance;
    output.color /= max_amplitude;
  }

  output.position = (params.scale != 0.0f) ? output.position / params.scale :
                                             float4{0.0f, 0.0f, 0.0f, 0.0f};

  return output;
}

template<typename T>
float fractal_voronoi_distance_to_edge(const VoronoiParams &params, const T coord)
{
  float amplitude = 1.0f;
  float max_amplitude = 0.5f + 0.5f * params.randomness;
  float scale = 1.0f;
  float distance = 8.0f;

  const bool zero_input = params.detail == 0.0f || params.roughness == 0.0f ||
                          params.lacunarity == 0.0f;

  for (int i = 0; i <= ceilf(params.detail); ++i) {
    const float octave_distance = voronoi_distance_to_edge(params, coord * scale);

    if (zero_input) {
      distance = octave_distance;
      break;
    }
    else if (i <= params.detail) {
      max_amplitude = mix(max_amplitude, (0.5f + 0.5f * params.randomness) / scale, amplitude);
      distance = mix(distance, math::min(distance, octave_distance / scale), amplitude);
      scale *= params.lacunarity;
      amplitude *= params.roughness;
    }
    else {
      float remainder = params.detail - floorf(params.detail);
      if (remainder != 0.0f) {
        float lerp_amplitude = mix(
            max_amplitude, (0.5f + 0.5f * params.randomness) / scale, amplitude);
        max_amplitude = mix(max_amplitude, lerp_amplitude, remainder);
        float lerp_distance = mix(
            distance, math::min(distance, octave_distance / scale), amplitude);
        distance = mix(distance, math::min(distance, lerp_distance), remainder);
      }
    }
  }

  if (params.normalize) {
    distance /= max_amplitude;
  }

  return distance;
}

/* Explicit function template instantiation */

template VoronoiOutput fractal_voronoi_x_fx<float>(const VoronoiParams &params,
                                                   const float coord,
                                                   const bool calc_color);
template VoronoiOutput fractal_voronoi_x_fx<float2>(const VoronoiParams &params,
                                                    const float2 coord,
                                                    const bool calc_color);
template VoronoiOutput fractal_voronoi_x_fx<float3>(const VoronoiParams &params,
                                                    const float3 coord,
                                                    const bool calc_color);
template VoronoiOutput fractal_voronoi_x_fx<float4>(const VoronoiParams &params,
                                                    const float4 coord,
                                                    const bool calc_color);

template float fractal_voronoi_distance_to_edge<float>(const VoronoiParams &params,
                                                       const float coord);
template float fractal_voronoi_distance_to_edge<float2>(const VoronoiParams &params,
                                                        const float2 coord);
template float fractal_voronoi_distance_to_edge<float3>(const VoronoiParams &params,
                                                        const float3 coord);
template float fractal_voronoi_distance_to_edge<float4>(const VoronoiParams &params,
                                                        const float4 coord);
/** \} */

}  // namespace blender::noise
