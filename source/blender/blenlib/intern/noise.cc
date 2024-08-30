/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al.
 *                         All Rights Reserved. (BSD-3-Clause).
 * SPDX-FileCopyrightText: 2011 Blender Authors (GPL-2.0-or-later).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later AND BSD-3-Clause */

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "BLI_math_base.hh"
#include "BLI_math_base_safe.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_numbers.hh"
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

uint32_t hash_float(const float4x4 &k)
{
  return hash(hash_float(k.x), hash_float(k.y), hash_float(k.z), hash_float(k.w));
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

float2 hash_float_to_float2(float3 k)
{
  return float2(hash_float_to_float(float3(k.x, k.y, k.z)),
                hash_float_to_float(float3(k.z, k.x, k.y)));
}

float2 hash_float_to_float2(float4 k)
{
  return float2(hash_float_to_float(float4(k.x, k.y, k.z, k.w)),
                hash_float_to_float(float4(k.z, k.x, k.w, k.y)));
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
  float x_floor = math::floor(x);
  i = int(x_floor);
  return x - x_floor;
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
  float precision_correction = 0.5f * float(math::abs(position) >= 1000000.0f);
  /* Repeat Perlin noise texture every 100000.0 on each axis to prevent floating point
   * representation issues. */
  position = math::mod(position, 100000.0f) + precision_correction;

  return perlin_noise(position) * 0.2500f;
}

float perlin_signed(float2 position)
{
  float2 precision_correction = 0.5f * float2(float(math::abs(position.x) >= 1000000.0f),
                                              float(math::abs(position.y) >= 1000000.0f));
  /* Repeat Perlin noise texture every 100000.0f on each axis to prevent floating point
   * representation issues. This causes discontinuities every 100000.0f, however at such scales
   * this usually shouldn't be noticeable. */
  position = math::mod(position, 100000.0f) + precision_correction;

  return perlin_noise(position) * 0.6616f;
}

float perlin_signed(float3 position)
{
  float3 precision_correction = 0.5f * float3(float(math::abs(position.x) >= 1000000.0f),
                                              float(math::abs(position.y) >= 1000000.0f),
                                              float(math::abs(position.z) >= 1000000.0f));
  /* Repeat Perlin noise texture every 100000.0f on each axis to prevent floating point
   * representation issues. This causes discontinuities every 100000.0f, however at such scales
   * this usually shouldn't be noticeable. */
  position = math::mod(position, 100000.0f) + precision_correction;

  return perlin_noise(position) * 0.9820f;
}

float perlin_signed(float4 position)
{
  float4 precision_correction = 0.5f * float4(float(math::abs(position.x) >= 1000000.0f),
                                              float(math::abs(position.y) >= 1000000.0f),
                                              float(math::abs(position.z) >= 1000000.0f),
                                              float(math::abs(position.w) >= 1000000.0f));
  /* Repeat Perlin noise texture every 100000.0f on each axis to prevent floating point
   * representation issues. This causes discontinuities every 100000.0f, however at such scales
   * this usually shouldn't be noticeable. */
  position = math::mod(position, 100000.0f) + precision_correction;

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

/* Fractal perlin noise. */

/* fBM = Fractal Brownian Motion */
template<typename T>
float perlin_fbm(
    T p, const float detail, const float roughness, const float lacunarity, const bool normalize)
{
  float fscale = 1.0f;
  float amp = 1.0f;
  float maxamp = 0.0f;
  float sum = 0.0f;

  for (int i = 0; i <= int(detail); i++) {
    float t = perlin_signed(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= roughness;
    fscale *= lacunarity;
  }
  float rmd = detail - std::floor(detail);
  if (rmd != 0.0f) {
    float t = perlin_signed(fscale * p);
    float sum2 = sum + t * amp;
    return normalize ? mix(0.5f * sum / maxamp + 0.5f, 0.5f * sum2 / (maxamp + amp) + 0.5f, rmd) :
                       mix(sum, sum2, rmd);
  }
  return normalize ? 0.5f * sum / maxamp + 0.5f : sum;
}

/* Explicit instantiation for Wave Texture. */
template float perlin_fbm<float3>(float3 p,
                                  const float detail,
                                  const float roughness,
                                  const float lacunarity,
                                  const bool normalize);

template<typename T>
float perlin_multi_fractal(T p, const float detail, const float roughness, const float lacunarity)
{
  float value = 1.0f;
  float pwr = 1.0f;

  for (int i = 0; i <= int(detail); i++) {
    value *= (pwr * perlin_signed(p) + 1.0f);
    pwr *= roughness;
    p *= lacunarity;
  }

  const float rmd = detail - floorf(detail);
  if (rmd != 0.0f) {
    value *= (rmd * pwr * perlin_signed(p) + 1.0f); /* correct? */
  }

  return value;
}

template<typename T>
float perlin_hetero_terrain(
    T p, const float detail, const float roughness, const float lacunarity, const float offset)
{
  float pwr = roughness;

  /* First unscaled octave of function; later octaves are scaled. */
  float value = offset + perlin_signed(p);
  p *= lacunarity;

  for (int i = 1; i <= int(detail); i++) {
    float increment = (perlin_signed(p) + offset) * pwr * value;
    value += increment;
    pwr *= roughness;
    p *= lacunarity;
  }

  const float rmd = detail - floorf(detail);
  if (rmd != 0.0f) {
    float increment = (perlin_signed(p) + offset) * pwr * value;
    value += rmd * increment;
  }

  return value;
}

template<typename T>
float perlin_hybrid_multi_fractal(T p,
                                  const float detail,
                                  const float roughness,
                                  const float lacunarity,
                                  const float offset,
                                  const float gain)
{
  float pwr = 1.0f;
  float value = 0.0f;
  float weight = 1.0f;

  for (int i = 0; (weight > 0.001f) && (i <= int(detail)); i++) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }

    float signal = (perlin_signed(p) + offset) * pwr;
    pwr *= roughness;
    value += weight * signal;
    weight *= gain * signal;
    p *= lacunarity;
  }

  const float rmd = detail - floorf(detail);
  if ((rmd != 0.0f) && (weight > 0.001f)) {
    if (weight > 1.0f) {
      weight = 1.0f;
    }
    float signal = (perlin_signed(p) + offset) * pwr;
    value += rmd * weight * signal;
  }

  return value;
}

template<typename T>
float perlin_ridged_multi_fractal(T p,
                                  const float detail,
                                  const float roughness,
                                  const float lacunarity,
                                  const float offset,
                                  const float gain)
{
  float pwr = roughness;

  float signal = offset - std::abs(perlin_signed(p));
  signal *= signal;
  float value = signal;
  float weight = 1.0f;

  for (int i = 1; i <= int(detail); i++) {
    p *= lacunarity;
    weight = std::clamp(signal * gain, 0.0f, 1.0f);
    signal = offset - std::abs(perlin_signed(p));
    signal *= signal;
    signal *= weight;
    value += signal * pwr;
    pwr *= roughness;
  }

  return value;
}

enum {
  NOISE_SHD_PERLIN_MULTIFRACTAL = 0,
  NOISE_SHD_PERLIN_FBM = 1,
  NOISE_SHD_PERLIN_HYBRID_MULTIFRACTAL = 2,
  NOISE_SHD_PERLIN_RIDGED_MULTIFRACTAL = 3,
  NOISE_SHD_PERLIN_HETERO_TERRAIN = 4,
};

template<typename T>
float perlin_select(T p,
                    float detail,
                    float roughness,
                    float lacunarity,
                    float offset,
                    float gain,
                    int type,
                    bool normalize)
{
  switch (type) {
    case NOISE_SHD_PERLIN_MULTIFRACTAL: {
      return perlin_multi_fractal<T>(p, detail, roughness, lacunarity);
    }
    case NOISE_SHD_PERLIN_FBM: {
      return perlin_fbm<T>(p, detail, roughness, lacunarity, normalize);
    }
    case NOISE_SHD_PERLIN_HYBRID_MULTIFRACTAL: {
      return perlin_hybrid_multi_fractal<T>(p, detail, roughness, lacunarity, offset, gain);
    }
    case NOISE_SHD_PERLIN_RIDGED_MULTIFRACTAL: {
      return perlin_ridged_multi_fractal<T>(p, detail, roughness, lacunarity, offset, gain);
    }
    case NOISE_SHD_PERLIN_HETERO_TERRAIN: {
      return perlin_hetero_terrain<T>(p, detail, roughness, lacunarity, offset);
    }
    default: {
      return 0.0;
    }
  }
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

/* Distorted fractal perlin noise. */

template<typename T>
float perlin_fractal_distorted(T position,
                               float detail,
                               float roughness,
                               float lacunarity,
                               float offset,
                               float gain,
                               float distortion,
                               int type,
                               bool normalize)
{
  position += perlin_distortion(position, distortion);
  return perlin_select<T>(position, detail, roughness, lacunarity, offset, gain, type, normalize);
}

template float perlin_fractal_distorted<float>(float position,
                                               float detail,
                                               float roughness,
                                               float lacunarity,
                                               float offset,
                                               float gain,
                                               float distortion,
                                               int type,
                                               bool normalize);
template float perlin_fractal_distorted<float2>(float2 position,
                                                float detail,
                                                float roughness,
                                                float lacunarity,
                                                float offset,
                                                float gain,
                                                float distortion,
                                                int type,
                                                bool normalize);
template float perlin_fractal_distorted<float3>(float3 position,
                                                float detail,
                                                float roughness,
                                                float lacunarity,
                                                float offset,
                                                float gain,
                                                float distortion,
                                                int type,
                                                bool normalize);
template float perlin_fractal_distorted<float4>(float4 position,
                                                float detail,
                                                float roughness,
                                                float lacunarity,
                                                float offset,
                                                float gain,
                                                float distortion,
                                                int type,
                                                bool normalize);

/* Distorted fractal perlin noise that outputs a float3. The arbitrary seeds are for
 * compatibility with shading functions. */

float3 perlin_float3_fractal_distorted(float position,
                                       float detail,
                                       float roughness,
                                       float lacunarity,
                                       float offset,
                                       float gain,
                                       float distortion,
                                       int type,
                                       bool normalize)
{
  position += perlin_distortion(position, distortion);
  return float3(
      perlin_select<float>(position, detail, roughness, lacunarity, offset, gain, type, normalize),
      perlin_select<float>(position + random_float_offset(1.0f),
                           detail,
                           roughness,
                           lacunarity,
                           offset,
                           gain,
                           type,
                           normalize),
      perlin_select<float>(position + random_float_offset(2.0f),
                           detail,
                           roughness,
                           lacunarity,
                           offset,
                           gain,
                           type,
                           normalize));
}

float3 perlin_float3_fractal_distorted(float2 position,
                                       float detail,
                                       float roughness,
                                       float lacunarity,
                                       float offset,
                                       float gain,
                                       float distortion,
                                       int type,
                                       bool normalize)
{
  position += perlin_distortion(position, distortion);
  return float3(perlin_select<float2>(
                    position, detail, roughness, lacunarity, offset, gain, type, normalize),
                perlin_select<float2>(position + random_float2_offset(2.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize),
                perlin_select<float2>(position + random_float2_offset(3.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize));
}

float3 perlin_float3_fractal_distorted(float3 position,
                                       float detail,
                                       float roughness,
                                       float lacunarity,
                                       float offset,
                                       float gain,
                                       float distortion,
                                       int type,
                                       bool normalize)
{
  position += perlin_distortion(position, distortion);
  return float3(perlin_select<float3>(
                    position, detail, roughness, lacunarity, offset, gain, type, normalize),
                perlin_select<float3>(position + random_float3_offset(3.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize),
                perlin_select<float3>(position + random_float3_offset(4.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize));
}

float3 perlin_float3_fractal_distorted(float4 position,
                                       float detail,
                                       float roughness,
                                       float lacunarity,
                                       float offset,
                                       float gain,
                                       float distortion,
                                       int type,
                                       bool normalize)
{
  position += perlin_distortion(position, distortion);
  return float3(perlin_select<float4>(
                    position, detail, roughness, lacunarity, offset, gain, type, normalize),
                perlin_select<float4>(position + random_float4_offset(4.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize),
                perlin_select<float4>(position + random_float4_offset(5.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize));
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

  float minDistance = FLT_MAX;
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

  float smoothDistance = 0.0f;
  float smoothPosition = 0.0f;
  float3 smoothColor = {0.0f, 0.0f, 0.0f};
  float h = -1.0f;
  for (int i = -2; i <= 2; i++) {
    float cellOffset = i;
    float pointPosition = cellOffset +
                          hash_float_to_float(cellPosition + cellOffset) * params.randomness;
    float distanceToPoint = voronoi_distance(pointPosition, localPosition);
    h = h == -1.0f ?
            1.0f :
            smoothstep(
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

  float distanceF1 = FLT_MAX;
  float distanceF2 = FLT_MAX;
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
  float minDistance = FLT_MAX;
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

  minDistance = FLT_MAX;
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

  float minDistance = FLT_MAX;
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

  float smoothDistance = 0.0f;
  float3 smoothColor = {0.0f, 0.0f, 0.0f};
  float2 smoothPosition = {0.0f, 0.0f};
  float h = -1.0f;
  for (int j = -2; j <= 2; j++) {
    for (int i = -2; i <= 2; i++) {
      float2 cellOffset(i, j);
      float2 pointPosition = cellOffset +
                             hash_float_to_float2(cellPosition + cellOffset) * params.randomness;
      float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
      h = h == -1.0f ?
              1.0f :
              smoothstep(0.0f,
                         1.0f,
                         0.5f + 0.5f * (smoothDistance - distanceToPoint) / params.smoothness);
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

  float distanceF1 = FLT_MAX;
  float distanceF2 = FLT_MAX;
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
  float minDistance = FLT_MAX;
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

  minDistance = FLT_MAX;
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
  float minDistance = FLT_MAX;
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

  minDistance = FLT_MAX;
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

  float minDistance = FLT_MAX;
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

  float smoothDistance = 0.0f;
  float3 smoothColor = {0.0f, 0.0f, 0.0f};
  float3 smoothPosition = {0.0f, 0.0f, 0.0f};
  float h = -1.0f;
  for (int k = -2; k <= 2; k++) {
    for (int j = -2; j <= 2; j++) {
      for (int i = -2; i <= 2; i++) {
        float3 cellOffset(i, j, k);
        float3 pointPosition = cellOffset +
                               hash_float_to_float3(cellPosition + cellOffset) * params.randomness;
        float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
        h = h == -1.0f ?
                1.0f :
                smoothstep(0.0f,
                           1.0f,
                           0.5f + 0.5f * (smoothDistance - distanceToPoint) / params.smoothness);
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

  float distanceF1 = FLT_MAX;
  float distanceF2 = FLT_MAX;
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
  float minDistance = FLT_MAX;
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

  minDistance = FLT_MAX;
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
  float minDistance = FLT_MAX;
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

  minDistance = FLT_MAX;
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

  float minDistance = FLT_MAX;
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

  float smoothDistance = 0.0f;
  float3 smoothColor = {0.0f, 0.0f, 0.0f};
  float4 smoothPosition = {0.0f, 0.0f, 0.0f, 0.0f};
  float h = -1.0f;
  for (int u = -2; u <= 2; u++) {
    for (int k = -2; k <= 2; k++) {
      for (int j = -2; j <= 2; j++) {
        for (int i = -2; i <= 2; i++) {
          float4 cellOffset(i, j, k, u);
          float4 pointPosition = cellOffset + hash_float_to_float4(cellPosition + cellOffset) *
                                                  params.randomness;
          float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
          h = h == -1.0f ?
                  1.0f :
                  smoothstep(0.0f,
                             1.0f,
                             0.5f + 0.5f * (smoothDistance - distanceToPoint) / params.smoothness);
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

  float distanceF1 = FLT_MAX;
  float distanceF2 = FLT_MAX;
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
  float minDistance = FLT_MAX;
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

  minDistance = FLT_MAX;
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
  float minDistance = FLT_MAX;
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

  minDistance = FLT_MAX;
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

/* The fractalization logic is the same as for fBM Noise, except that some additions are replaced
 * by lerps. */
template<typename T>
VoronoiOutput fractal_voronoi_x_fx(const VoronoiParams &params,
                                   const T coord,
                                   const bool calc_color /* Only used to optimize Smooth F1 */)
{
  float amplitude = 1.0f;
  float max_amplitude = 0.0f;
  float scale = 1.0f;

  VoronoiOutput output;
  const bool zero_input = params.detail == 0.0f || params.roughness == 0.0f;

  for (int i = 0; i <= ceilf(params.detail); ++i) {
    VoronoiOutput octave = (params.feature == NOISE_SHD_VORONOI_F2) ?
                               voronoi_f2(params, coord * scale) :
                           (params.feature == NOISE_SHD_VORONOI_SMOOTH_F1 &&
                            params.smoothness != 0.0f) ?
                               voronoi_smooth_f1(params, coord * scale, calc_color) :
                               voronoi_f1(params, coord * scale);

    if (zero_input) {
      max_amplitude = 1.0f;
      output = octave;
      break;
    }
    if (i <= params.detail) {
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

/* The fractalization logic is the same as for fBM Noise, except that some additions are replaced
 * by lerps. */
template<typename T>
float fractal_voronoi_distance_to_edge(const VoronoiParams &params, const T coord)
{
  float amplitude = 1.0f;
  float max_amplitude = params.max_distance;
  float scale = 1.0f;
  float distance = 8.0f;

  const bool zero_input = params.detail == 0.0f || params.roughness == 0.0f;

  for (int i = 0; i <= ceilf(params.detail); ++i) {
    const float octave_distance = voronoi_distance_to_edge(params, coord * scale);

    if (zero_input) {
      distance = octave_distance;
      break;
    }
    if (i <= params.detail) {
      max_amplitude = mix(max_amplitude, params.max_distance / scale, amplitude);
      distance = mix(distance, math::min(distance, octave_distance / scale), amplitude);
      scale *= params.lacunarity;
      amplitude *= params.roughness;
    }
    else {
      float remainder = params.detail - floorf(params.detail);
      if (remainder != 0.0f) {
        float lerp_amplitude = mix(max_amplitude, params.max_distance / scale, amplitude);
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

/* -------------------------------------------------------------------- */
/** \name Gabor Noise
 *
 * Implements Gabor noise based on the paper:
 *
 *   Lagae, Ares, et al. "Procedural noise using sparse Gabor convolution." ACM Transactions on
 *   Graphics (TOG) 28.3 (2009): 1-10.
 *
 * But with the improvements from the paper:
 *
 *   Tavernier, Vincent, et al. "Making gabor noise fast and normalized." Eurographics 2019-40th
 *   Annual Conference of the European Association for Computer Graphics. 2019.
 *
 * And compute the Phase and Intensity of the Gabor based on the paper:
 *
 *   Tricard, Thibault, et al. "Procedural phasor noise." ACM Transactions on Graphics (TOG) 38.4
 *   (2019): 1-13.
 *
 * \{ */

/* The original Gabor noise paper specifies that the impulses count for each cell should be
 * computed by sampling a Poisson distribution whose mean is the impulse density. However,
 * Tavernier's paper showed that stratified Poisson point sampling is better assuming the weights
 * are sampled using a Bernoulli distribution, as shown in Figure (3). By stratified sampling, they
 * mean a constant number of impulses per cell, so the stratification is the grid itself in that
 * sense, as described in the supplementary material of the paper. */
static constexpr int gabor_impulses_count = 8;

/* Computes a 2D Gabor kernel based on Equation (6) in the original Gabor noise paper. Where the
 * frequency argument is the F_0 parameter and the orientation argument is the w_0 parameter. We
 * assume the Gaussian envelope has a unit magnitude, that is, K = 1. That is because we will
 * eventually normalize the final noise value to the unit range, so the multiplication by the
 * magnitude will be canceled by the normalization. Further, we also assume a unit Gaussian width,
 * that is, a = 1. That is because it does not provide much artistic control. It follows that the
 * Gaussian will be truncated at pi.
 *
 * To avoid the discontinuities caused by the aforementioned truncation, the Gaussian is windowed
 * using a Hann window, that is because contrary to the claim made in the original Gabor paper,
 * truncating the Gaussian produces significant artifacts especially when differentiated for bump
 * mapping. The Hann window is C1 continuous and has limited effect on the shape of the Gaussian,
 * so it felt like an appropriate choice.
 *
 * Finally, instead of computing the Gabor value directly, we instead use the complex phasor
 * formulation described in section 3.1.1 in Tricard's paper. That's done to be able to compute the
 * phase and intensity of the Gabor noise after summation based on equations (8) and (9). The
 * return value of the Gabor kernel function is then a complex number whose real value is the
 * value computed in the original Gabor noise paper, and whose imaginary part is the sine
 * counterpart of the real part, which is the only extra computation in the new formulation.
 *
 * Note that while the original Gabor noise paper uses the cosine part of the phasor, that is, the
 * real part of the phasor, we use the sine part instead, that is, the imaginary part of the
 * phasor, as suggested by Tavernier's paper in "Section 3.3. Instance stationarity and
 * normalization", to ensure a zero mean, which should help with normalization. */
static float2 compute_2d_gabor_kernel(const float2 position,
                                      const float frequency,
                                      const float orientation)
{
  const float distance_squared = math::length_squared(position);
  const float hann_window = 0.5f + 0.5f * math::cos(math::numbers::pi * distance_squared);
  const float gaussian_envelop = math::exp(-math::numbers::pi * distance_squared);
  const float windowed_gaussian_envelope = gaussian_envelop * hann_window;

  const float2 frequency_vector = frequency * float2(cos(orientation), sin(orientation));
  const float angle = 2.0f * math::numbers::pi * math::dot(position, frequency_vector);
  const float2 phasor = float2(math::cos(angle), math::sin(angle));

  return windowed_gaussian_envelope * phasor;
}

/* Computes the approximate standard deviation of the zero mean normal distribution representing
 * the amplitude distribution of the noise based on Equation (9) in the original Gabor noise paper.
 * For simplicity, the Hann window is ignored and the orientation is fixed since the variance is
 * orientation invariant. We start integrating the squared Gabor kernel with respect to x:
 *
 *   \int_{-\infty}^{-\infty} (e^{- \pi (x^2 + y^2)} cos(2 \pi f_0 x))^2 dx
 *
 * Which gives:
 *
 *  \frac{(e^{2 \pi f_0^2}-1) e^{-2 \pi y^2 - 2 pi f_0^2}}{2^\frac{3}{2}}
 *
 * Then we similarly integrate with respect to y to get:
 *
 *  \frac{1 - e^{-2 \pi f_0^2}}{4}
 *
 * Secondly, we note that the second moment of the weights distribution is 0.5 since it is a
 * fair Bernoulli distribution. So the final standard deviation expression is square root the
 * integral multiplied by the impulse density multiplied by the second moment.
 *
 * Note however that the integral is almost constant for all frequencies larger than one, and
 * converges to an upper limit as the frequency approaches infinity, so we replace the expression
 * with the following limit:
 *
 *  \lim_{x \to \infty} \frac{1 - e^{-2 \pi f_0^2}}{4}
 *
 * To get an approximation of 0.25. */
static float compute_2d_gabor_standard_deviation()
{
  const float integral_of_gabor_squared = 0.25f;
  const float second_moment = 0.5f;
  return math::sqrt(gabor_impulses_count * second_moment * integral_of_gabor_squared);
}

/* Computes the Gabor noise value at the given position for the given cell. This is essentially the
 * sum in Equation (8) in the original Gabor noise paper, where we sum Gabor kernels sampled at a
 * random position with a random weight. The orientation of the kernel is constant for anisotropic
 * noise while it is random for isotropic noise. The original Gabor noise paper mentions that the
 * weights should be uniformly distributed in the [-1, 1] range, however, Tavernier's paper showed
 * that using a Bernoulli distribution yields better results, so that is what we do. */
static float2 compute_2d_gabor_noise_cell(const float2 cell,
                                          const float2 position,
                                          const float frequency,
                                          const float isotropy,
                                          const float base_orientation)

{
  float2 noise(0.0f);
  for (const int i : IndexRange(gabor_impulses_count)) {
    /* Compute unique seeds for each of the needed random variables. */
    const float3 seed_for_orientation(cell.x, cell.y, i * 3);
    const float3 seed_for_kernel_center(cell.x, cell.y, i * 3 + 1);
    const float3 seed_for_weight(cell.x, cell.y, i * 3 + 2);

    /* For isotropic noise, add a random orientation amount, while for anisotropic noise, use the
     * base orientation. Linearly interpolate between the two cases using the isotropy factor. Note
     * that the random orientation range spans pi as opposed to two pi, that's because the Gabor
     * kernel is symmetric around pi. */
    const float random_orientation = (noise::hash_float_to_float(seed_for_orientation) - 0.5f) *
                                     math::numbers::pi;
    const float orientation = base_orientation + random_orientation * isotropy;

    const float2 kernel_center = noise::hash_float_to_float2(seed_for_kernel_center);
    const float2 position_in_kernel_space = position - kernel_center;

    /* The kernel is windowed beyond the unit distance, so early exit with a zero for points that
     * are further than a unit radius. */
    if (math::length_squared(position_in_kernel_space) >= 1.0f) {
      continue;
    }

    /* We either add or subtract the Gabor kernel based on a Bernoulli distribution of equal
     * probability. */
    const float weight = noise::hash_float_to_float(seed_for_weight) < 0.5f ? -1.0f : 1.0f;

    noise += weight * compute_2d_gabor_kernel(position_in_kernel_space, frequency, orientation);
  }
  return noise;
}

/* Computes the Gabor noise value by dividing the space into a grid and evaluating the Gabor noise
 * in the space of each cell of the 3x3 cell neighborhood. */
static float2 compute_2d_gabor_noise(const float2 coordinates,
                                     const float frequency,
                                     const float isotropy,
                                     const float base_orientation)
{
  const float2 cell_position = math::floor(coordinates);
  const float2 local_position = coordinates - cell_position;

  float2 sum(0.0f);
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      const float2 cell_offset = float2(i, j);
      const float2 current_cell_position = cell_position + cell_offset;
      const float2 position_in_cell_space = local_position - cell_offset;
      sum += compute_2d_gabor_noise_cell(
          current_cell_position, position_in_cell_space, frequency, isotropy, base_orientation);
    }
  }

  return sum;
}

/* Identical to compute_2d_gabor_kernel, except it is evaluated in 3D space. Notice that Equation
 * (6) in the original Gabor noise paper computes the frequency vector using (cos(w_0), sin(w_0)),
 * which we also do in the 2D variant, however, for 3D, the orientation is already a unit frequency
 * vector, so we just need to scale it by the frequency value. */
static float2 compute_3d_gabor_kernel(const float3 position,
                                      const float frequency,
                                      const float3 orientation)
{
  const float distance_squared = math::length_squared(position);
  const float hann_window = 0.5f + 0.5f * math::cos(math::numbers::pi * distance_squared);
  const float gaussian_envelop = math::exp(-math::numbers::pi * distance_squared);
  const float windowed_gaussian_envelope = gaussian_envelop * hann_window;

  const float3 frequency_vector = frequency * orientation;
  const float angle = 2.0f * math::numbers::pi * math::dot(position, frequency_vector);
  const float2 phasor = float2(math::cos(angle), math::sin(angle));

  return windowed_gaussian_envelope * phasor;
}

/* Identical to compute_2d_gabor_standard_deviation except we do triple integration in 3D. The only
 * difference is the denominator in the integral expression, which is 2^{5 / 2} for the 3D case
 * instead of 4 for the 2D case. Similarly, the limit evaluates to 1 / (4 * sqrt(2)). */
static float compute_3d_gabor_standard_deviation()
{
  const float integral_of_gabor_squared = 1.0f / (4.0f * math::numbers::sqrt2);
  const float second_moment = 0.5f;
  return math::sqrt(gabor_impulses_count * second_moment * integral_of_gabor_squared);
}

/* Computes the orientation of the Gabor kernel such that it is constant for anisotropic
 * noise while it is random for isotropic noise. We randomize in spherical coordinates for a
 * uniform distribution. */
static float3 compute_3d_orientation(const float3 orientation,
                                     const float isotropy,
                                     const float4 seed)
{
  /* Return the base orientation in case we are completely anisotropic. */
  if (isotropy == 0.0) {
    return orientation;
  }

  /* Compute the orientation in spherical coordinates. */
  float inclination = math::acos(orientation.z);
  float azimuth = math::sign(orientation.y) *
                  math::acos(orientation.x / math::length(float2(orientation.x, orientation.y)));

  /* For isotropic noise, add a random orientation amount, while for anisotropic noise, use the
   * base orientation. Linearly interpolate between the two cases using the isotropy factor. Note
   * that the random orientation range is to pi as opposed to two pi, that's because the Gabor
   * kernel is symmetric around pi. */
  const float2 random_angles = noise::hash_float_to_float2(seed) * math::numbers::pi;
  inclination += random_angles.x * isotropy;
  azimuth += random_angles.y * isotropy;

  /* Convert back to Cartesian coordinates, */
  return float3(math::sin(inclination) * math::cos(azimuth),
                math::sin(inclination) * math::sin(azimuth),
                math::cos(inclination));
}

static float2 compute_3d_gabor_noise_cell(const float3 cell,
                                          const float3 position,
                                          const float frequency,
                                          const float isotropy,
                                          const float3 base_orientation)

{
  float2 noise(0.0f);
  for (const int i : IndexRange(gabor_impulses_count)) {
    /* Compute unique seeds for each of the needed random variables. */
    const float4 seed_for_orientation(cell.x, cell.y, cell.z, i * 3);
    const float4 seed_for_kernel_center(cell.x, cell.y, cell.z, i * 3 + 1);
    const float4 seed_for_weight(cell.x, cell.y, cell.z, i * 3 + 2);

    const float3 orientation = compute_3d_orientation(
        base_orientation, isotropy, seed_for_orientation);

    const float3 kernel_center = noise::hash_float_to_float3(seed_for_kernel_center);
    const float3 position_in_kernel_space = position - kernel_center;

    /* The kernel is windowed beyond the unit distance, so early exit with a zero for points that
     * are further than a unit radius. */
    if (math::length_squared(position_in_kernel_space) >= 1.0f) {
      continue;
    }

    /* We either add or subtract the Gabor kernel based on a Bernoulli distribution of equal
     * probability. */
    const float weight = noise::hash_float_to_float(seed_for_weight) < 0.5f ? -1.0f : 1.0f;

    noise += weight * compute_3d_gabor_kernel(position_in_kernel_space, frequency, orientation);
  }
  return noise;
}

/* Identical to compute_2d_gabor_noise but works in the 3D neighborhood of the noise. */
static float2 compute_3d_gabor_noise(const float3 coordinates,
                                     const float frequency,
                                     const float isotropy,
                                     const float3 base_orientation)
{
  const float3 cell_position = math::floor(coordinates);
  const float3 local_position = coordinates - cell_position;

  float2 sum(0.0f);
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        const float3 cell_offset = float3(i, j, k);
        const float3 current_cell_position = cell_position + cell_offset;
        const float3 position_in_cell_space = local_position - cell_offset;
        sum += compute_3d_gabor_noise_cell(
            current_cell_position, position_in_cell_space, frequency, isotropy, base_orientation);
      }
    }
  }

  return sum;
}

void gabor(const float2 coordinates,
           const float scale,
           const float frequency,
           const float anisotropy,
           const float orientation,
           float *r_value,
           float *r_phase,
           float *r_intensity)
{
  const float2 scaled_coordinates = coordinates * scale;
  const float isotropy = 1.0f - math::clamp(anisotropy, 0.0f, 1.0f);
  const float sanitized_frequency = math::max(0.001f, frequency);

  const float2 phasor = compute_2d_gabor_noise(
      scaled_coordinates, sanitized_frequency, isotropy, orientation);
  const float standard_deviation = compute_2d_gabor_standard_deviation();

  /* Normalize the noise by dividing by six times the standard deviation, which was determined
   * empirically. */
  const float normalization_factor = 6.0f * standard_deviation;

  /* As discussed in compute_2d_gabor_kernel, we use the imaginary part of the phasor as the Gabor
   * value. But remap to [0, 1] from [-1, 1]. */
  if (r_value) {
    *r_value = (phasor.y / normalization_factor) * 0.5f + 0.5f;
  }

  /* Compute the phase based on equation (9) in Tricard's paper. But remap the phase into the
   * [0, 1] range. */
  if (r_phase) {
    *r_phase = (math::atan2(phasor.y, phasor.x) + math::numbers::pi) / (2.0f * math::numbers::pi);
  }

  /* Compute the intensity based on equation (8) in Tricard's paper. */
  if (r_intensity) {
    *r_intensity = math::length(phasor) / normalization_factor;
  }
}

void gabor(const float3 coordinates,
           const float scale,
           const float frequency,
           const float anisotropy,
           const float3 orientation,
           float *r_value,
           float *r_phase,
           float *r_intensity)
{
  const float3 scaled_coordinates = coordinates * scale;
  const float isotropy = 1.0f - math::clamp(anisotropy, 0.0f, 1.0f);
  const float sanitized_frequency = math::max(0.001f, frequency);

  const float3 normalized_orientation = math::normalize(orientation);
  const float2 phasor = compute_3d_gabor_noise(
      scaled_coordinates, sanitized_frequency, isotropy, normalized_orientation);
  const float standard_deviation = compute_3d_gabor_standard_deviation();

  /* Normalize the noise by dividing by six times the standard deviation, which was determined
   * empirically. */
  const float normalization_factor = 6.0f * standard_deviation;

  /* As discussed in compute_2d_gabor_kernel, we use the imaginary part of the phasor as the Gabor
   * value. But remap to [0, 1] from [-1, 1]. */
  if (r_value) {
    *r_value = (phasor.y / normalization_factor) * 0.5f + 0.5f;
  }

  /* Compute the phase based on equation (9) in Tricard's paper. But remap the phase into the
   * [0, 1] range. */
  if (r_phase) {
    *r_phase = (math::atan2(phasor.y, phasor.x) + math::numbers::pi) / (2.0f * math::numbers::pi);
  }

  /* Compute the intensity based on equation (8) in Tricard's paper. */
  if (r_intensity) {
    *r_intensity = math::length(phasor) / normalization_factor;
  }
}

/** \} */

}  // namespace blender::noise
