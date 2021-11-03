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

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "BLI_float2.hh"
#include "BLI_float3.hh"
#include "BLI_float4.hh"
#include "BLI_math_base_safe.h"
#include "BLI_noise.hh"
#include "BLI_utildefines.h"

namespace blender::noise {
/* ------------------------------
 * Jenkins Lookup3 Hash Functions
 * ------------------------------
 *
 * https://burtleburtle.net/bob/c/lookup3.c
 *
 */

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
  return static_cast<float>(k) / static_cast<float>(0xFFFFFFFFu);
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

/* ------------
 * Perlin Noise
 * ------------
 *
 * Perlin, Ken. "Improving noise." Proceedings of the 29th annual conference on Computer graphics
 * and interactive techniques. 2002.
 *
 * This implementation is functionally identical to the implementations in EEVEE, OSL, and SVM. So
 * any changes should be applied in all relevant implementations.
 */

/* Linear Interpolation. */
BLI_INLINE float mix(float v0, float v1, float x)
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
 *
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
  i = (int)x - ((x < 0) ? 1 : 0);
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
  int n = static_cast<int>(octaves);
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
 * OSL only support float hashes and we need to maintain compatibility with it.
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

/* --------------
 * Musgrave Noise
 * --------------
 */

/* 1D Musgrave fBm
 *
 * H: fractal increment parameter
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 *
 * from "Texturing and Modelling: A procedural approach"
 */

float musgrave_fBm(const float co,
                   const float H,
                   const float lacunarity,
                   const float octaves_unclamped)
{
  float p = co;
  float value = 0.0f;
  float pwr = 1.0f;
  const float pwHL = powf(lacunarity, -H);
  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; i < (int)octaves; i++) {
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

/* 1D Musgrave Multifractal
 *
 * H: highest fractal dimension
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 */

float musgrave_multi_fractal(const float co,
                             const float H,
                             const float lacunarity,
                             const float octaves_unclamped)
{
  float p = co;
  float value = 1.0f;
  float pwr = 1.0f;
  const float pwHL = powf(lacunarity, -H);
  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; i < (int)octaves; i++) {
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

/* 1D Musgrave Heterogeneous Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float musgrave_hetero_terrain(const float co,
                              const float H,
                              const float lacunarity,
                              const float octaves_unclamped,
                              const float offset)
{
  float p = co;
  const float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;
  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  /* first unscaled octave of function; later octaves are scaled */
  float value = offset + perlin_signed(p);
  p *= lacunarity;

  for (int i = 1; i < (int)octaves; i++) {
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

/* 1D Hybrid Additive/Multiplicative Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float musgrave_hybrid_multi_fractal(const float co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves_unclamped,
                                    const float offset,
                                    const float gain)
{
  float p = co;
  const float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  float value = perlin_signed(p) + offset;
  float weight = gain * value;
  p *= lacunarity;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; (weight > 0.001f) && (i < (int)octaves); i++) {
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
  if (rmd != 0.0f) {
    value += rmd * ((perlin_signed(p) + offset) * pwr);
  }

  return value;
}

/* 1D Ridged Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float musgrave_ridged_multi_fractal(const float co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves_unclamped,
                                    const float offset,
                                    const float gain)
{
  float p = co;
  const float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  float signal = offset - fabsf(perlin_signed(p));
  signal *= signal;
  float value = signal;
  float weight = 1.0f;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; i < (int)octaves; i++) {
    p *= lacunarity;
    weight = CLAMPIS(signal * gain, 0.0f, 1.0f);
    signal = offset - fabsf(perlin_signed(p));
    signal *= signal;
    signal *= weight;
    value += signal * pwr;
    pwr *= pwHL;
  }

  return value;
}

/* 2D Musgrave fBm
 *
 * H: fractal increment parameter
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 *
 * from "Texturing and Modelling: A procedural approach"
 */

float musgrave_fBm(const float2 co,
                   const float H,
                   const float lacunarity,
                   const float octaves_unclamped)
{
  float2 p = co;
  float value = 0.0f;
  float pwr = 1.0f;
  const float pwHL = powf(lacunarity, -H);
  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; i < (int)octaves; i++) {
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

/* 2D Musgrave Multifractal
 *
 * H: highest fractal dimension
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 */

float musgrave_multi_fractal(const float2 co,
                             const float H,
                             const float lacunarity,
                             const float octaves_unclamped)
{
  float2 p = co;
  float value = 1.0f;
  float pwr = 1.0f;
  const float pwHL = powf(lacunarity, -H);
  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; i < (int)octaves; i++) {
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

/* 2D Musgrave Heterogeneous Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float musgrave_hetero_terrain(const float2 co,
                              const float H,
                              const float lacunarity,
                              const float octaves_unclamped,
                              const float offset)
{
  float2 p = co;
  const float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  /* first unscaled octave of function; later octaves are scaled */
  float value = offset + perlin_signed(p);
  p *= lacunarity;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; i < (int)octaves; i++) {
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

/* 2D Hybrid Additive/Multiplicative Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float musgrave_hybrid_multi_fractal(const float2 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves_unclamped,
                                    const float offset,
                                    const float gain)
{
  float2 p = co;
  const float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  float value = perlin_signed(p) + offset;
  float weight = gain * value;
  p *= lacunarity;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; (weight > 0.001f) && (i < (int)octaves); i++) {
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
  if (rmd != 0.0f) {
    value += rmd * ((perlin_signed(p) + offset) * pwr);
  }

  return value;
}

/* 2D Ridged Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float musgrave_ridged_multi_fractal(const float2 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves_unclamped,
                                    const float offset,
                                    const float gain)
{
  float2 p = co;
  const float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  float signal = offset - fabsf(perlin_signed(p));
  signal *= signal;
  float value = signal;
  float weight = 1.0f;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; i < (int)octaves; i++) {
    p *= lacunarity;
    weight = CLAMPIS(signal * gain, 0.0f, 1.0f);
    signal = offset - fabsf(perlin_signed(p));
    signal *= signal;
    signal *= weight;
    value += signal * pwr;
    pwr *= pwHL;
  }

  return value;
}

/* 3D Musgrave fBm
 *
 * H: fractal increment parameter
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 *
 * from "Texturing and Modelling: A procedural approach"
 */

float musgrave_fBm(const float3 co,
                   const float H,
                   const float lacunarity,
                   const float octaves_unclamped)
{
  float3 p = co;
  float value = 0.0f;
  float pwr = 1.0f;
  const float pwHL = powf(lacunarity, -H);

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; i < (int)octaves; i++) {
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

/* 3D Musgrave Multifractal
 *
 * H: highest fractal dimension
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 */

float musgrave_multi_fractal(const float3 co,
                             const float H,
                             const float lacunarity,
                             const float octaves_unclamped)
{
  float3 p = co;
  float value = 1.0f;
  float pwr = 1.0f;
  const float pwHL = powf(lacunarity, -H);

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; i < (int)octaves; i++) {
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

/* 3D Musgrave Heterogeneous Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float musgrave_hetero_terrain(const float3 co,
                              const float H,
                              const float lacunarity,
                              const float octaves_unclamped,
                              const float offset)
{
  float3 p = co;
  const float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  /* first unscaled octave of function; later octaves are scaled */
  float value = offset + perlin_signed(p);
  p *= lacunarity;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; i < (int)octaves; i++) {
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

/* 3D Hybrid Additive/Multiplicative Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float musgrave_hybrid_multi_fractal(const float3 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves_unclamped,
                                    const float offset,
                                    const float gain)
{
  float3 p = co;
  const float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  float value = perlin_signed(p) + offset;
  float weight = gain * value;
  p *= lacunarity;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; (weight > 0.001f) && (i < (int)octaves); i++) {
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
  if (rmd != 0.0f) {
    value += rmd * ((perlin_signed(p) + offset) * pwr);
  }

  return value;
}

/* 3D Ridged Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float musgrave_ridged_multi_fractal(const float3 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves_unclamped,
                                    const float offset,
                                    const float gain)
{
  float3 p = co;
  const float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  float signal = offset - fabsf(perlin_signed(p));
  signal *= signal;
  float value = signal;
  float weight = 1.0f;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; i < (int)octaves; i++) {
    p *= lacunarity;
    weight = CLAMPIS(signal * gain, 0.0f, 1.0f);
    signal = offset - fabsf(perlin_signed(p));
    signal *= signal;
    signal *= weight;
    value += signal * pwr;
    pwr *= pwHL;
  }

  return value;
}

/* 4D Musgrave fBm
 *
 * H: fractal increment parameter
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 *
 * from "Texturing and Modelling: A procedural approach"
 */

float musgrave_fBm(const float4 co,
                   const float H,
                   const float lacunarity,
                   const float octaves_unclamped)
{
  float4 p = co;
  float value = 0.0f;
  float pwr = 1.0f;
  const float pwHL = powf(lacunarity, -H);

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; i < (int)octaves; i++) {
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

/* 4D Musgrave Multifractal
 *
 * H: highest fractal dimension
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 */

float musgrave_multi_fractal(const float4 co,
                             const float H,
                             const float lacunarity,
                             const float octaves_unclamped)
{
  float4 p = co;
  float value = 1.0f;
  float pwr = 1.0f;
  const float pwHL = powf(lacunarity, -H);

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 0; i < (int)octaves; i++) {
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

/* 4D Musgrave Heterogeneous Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float musgrave_hetero_terrain(const float4 co,
                              const float H,
                              const float lacunarity,
                              const float octaves_unclamped,
                              const float offset)
{
  float4 p = co;
  const float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  /* first unscaled octave of function; later octaves are scaled */
  float value = offset + perlin_signed(p);
  p *= lacunarity;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; i < (int)octaves; i++) {
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

/* 4D Hybrid Additive/Multiplicative Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float musgrave_hybrid_multi_fractal(const float4 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves_unclamped,
                                    const float offset,
                                    const float gain)
{
  float4 p = co;
  const float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  float value = perlin_signed(p) + offset;
  float weight = gain * value;
  p *= lacunarity;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; (weight > 0.001f) && (i < (int)octaves); i++) {
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
  if (rmd != 0.0f) {
    value += rmd * ((perlin_signed(p) + offset) * pwr);
  }

  return value;
}

/* 4D Ridged Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float musgrave_ridged_multi_fractal(const float4 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves_unclamped,
                                    const float offset,
                                    const float gain)
{
  float4 p = co;
  const float pwHL = powf(lacunarity, -H);
  float pwr = pwHL;

  float signal = offset - fabsf(perlin_signed(p));
  signal *= signal;
  float value = signal;
  float weight = 1.0f;

  const float octaves = CLAMPIS(octaves_unclamped, 0.0f, 15.0f);

  for (int i = 1; i < (int)octaves; i++) {
    p *= lacunarity;
    weight = CLAMPIS(signal * gain, 0.0f, 1.0f);
    signal = offset - fabsf(perlin_signed(p));
    signal *= signal;
    signal *= weight;
    value += signal * pwr;
    pwr *= pwHL;
  }

  return value;
}

/*
 * Voronoi: Ported from Cycles code.
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
 */

/* **** 1D Voronoi **** */

/* Ensure to align with DNA. */
enum {
  NOISE_SHD_VORONOI_EUCLIDEAN = 0,
  NOISE_SHD_VORONOI_MANHATTAN = 1,
  NOISE_SHD_VORONOI_CHEBYCHEV = 2,
  NOISE_SHD_VORONOI_MINKOWSKI = 3,
};

BLI_INLINE float voronoi_distance(const float a, const float b)
{
  return fabsf(b - a);
}

void voronoi_f1(
    const float w, const float randomness, float *r_distance, float3 *r_color, float *r_w)
{
  const float cellPosition = floorf(w);
  const float localPosition = w - cellPosition;

  float minDistance = 8.0f;
  float targetOffset = 0.0f;
  float targetPosition = 0.0f;
  for (int i = -1; i <= 1; i++) {
    const float cellOffset = i;
    const float pointPosition = cellOffset +
                                hash_float_to_float(cellPosition + cellOffset) * randomness;
    const float distanceToPoint = voronoi_distance(pointPosition, localPosition);
    if (distanceToPoint < minDistance) {
      targetOffset = cellOffset;
      minDistance = distanceToPoint;
      targetPosition = pointPosition;
    }
  }
  if (r_distance != nullptr) {
    *r_distance = minDistance;
  }
  if (r_color != nullptr) {
    *r_color = hash_float_to_float3(cellPosition + targetOffset);
  }
  if (r_w != nullptr) {
    *r_w = targetPosition + cellPosition;
  }
}

void voronoi_smooth_f1(const float w,
                       const float smoothness,
                       const float randomness,
                       float *r_distance,
                       float3 *r_color,
                       float *r_w)
{
  const float cellPosition = floorf(w);
  const float localPosition = w - cellPosition;
  const float smoothness_clamped = max_ff(smoothness, FLT_MIN);

  float smoothDistance = 8.0f;
  float smoothPosition = 0.0f;
  float3 smoothColor = float3(0.0f, 0.0f, 0.0f);
  for (int i = -2; i <= 2; i++) {
    const float cellOffset = i;
    const float pointPosition = cellOffset +
                                hash_float_to_float(cellPosition + cellOffset) * randomness;
    const float distanceToPoint = voronoi_distance(pointPosition, localPosition);
    const float h = smoothstep(
        0.0f, 1.0f, 0.5f + 0.5f * (smoothDistance - distanceToPoint) / smoothness_clamped);
    float correctionFactor = smoothness * h * (1.0f - h);
    smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
    if (r_color != nullptr || r_w != nullptr) {
      correctionFactor /= 1.0f + 3.0f * smoothness;
      if (r_color != nullptr) {
        const float3 cellColor = hash_float_to_float3(cellPosition + cellOffset);
        smoothColor = float3::interpolate(smoothColor, cellColor, h) - correctionFactor;
      }
      if (r_w != nullptr) {
        smoothPosition = mix(smoothPosition, pointPosition, h) - correctionFactor;
      }
    }
  }
  if (r_distance != nullptr) {
    *r_distance = smoothDistance;
  }
  if (r_color != nullptr) {
    *r_color = smoothColor;
  }
  if (r_w != nullptr) {
    *r_w = cellPosition + smoothPosition;
  }
}

void voronoi_f2(
    const float w, const float randomness, float *r_distance, float3 *r_color, float *r_w)
{
  const float cellPosition = floorf(w);
  const float localPosition = w - cellPosition;

  float distanceF1 = 8.0f;
  float distanceF2 = 8.0f;
  float offsetF1 = 0.0f;
  float positionF1 = 0.0f;
  float offsetF2 = 0.0f;
  float positionF2 = 0.0f;
  for (int i = -1; i <= 1; i++) {
    const float cellOffset = i;
    const float pointPosition = cellOffset +
                                hash_float_to_float(cellPosition + cellOffset) * randomness;
    const float distanceToPoint = voronoi_distance(pointPosition, localPosition);
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
  if (r_distance != nullptr) {
    *r_distance = distanceF2;
  }
  if (r_color != nullptr) {
    *r_color = hash_float_to_float3(cellPosition + offsetF2);
  }
  if (r_w != nullptr) {
    *r_w = positionF2 + cellPosition;
  }
}

void voronoi_distance_to_edge(const float w, const float randomness, float *r_distance)
{
  const float cellPosition = floorf(w);
  const float localPosition = w - cellPosition;

  const float midPointPosition = hash_float_to_float(cellPosition) * randomness;
  const float leftPointPosition = -1.0f + hash_float_to_float(cellPosition - 1.0f) * randomness;
  const float rightPointPosition = 1.0f + hash_float_to_float(cellPosition + 1.0f) * randomness;
  const float distanceToMidLeft = fabsf((midPointPosition + leftPointPosition) / 2.0f -
                                        localPosition);
  const float distanceToMidRight = fabsf((midPointPosition + rightPointPosition) / 2.0f -
                                         localPosition);

  *r_distance = std::min(distanceToMidLeft, distanceToMidRight);
}

void voronoi_n_sphere_radius(const float w, const float randomness, float *r_radius)
{
  const float cellPosition = floorf(w);
  const float localPosition = w - cellPosition;

  float closestPoint = 0.0f;
  float closestPointOffset = 0.0f;
  float minDistance = 8.0f;
  for (int i = -1; i <= 1; i++) {
    const float cellOffset = i;
    const float pointPosition = cellOffset +
                                hash_float_to_float(cellPosition + cellOffset) * randomness;
    const float distanceToPoint = fabsf(pointPosition - localPosition);
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
    const float cellOffset = i + closestPointOffset;
    const float pointPosition = cellOffset +
                                hash_float_to_float(cellPosition + cellOffset) * randomness;
    const float distanceToPoint = fabsf(closestPoint - pointPosition);
    if (distanceToPoint < minDistance) {
      minDistance = distanceToPoint;
      closestPointToClosestPoint = pointPosition;
    }
  }
  *r_radius = fabsf(closestPointToClosestPoint - closestPoint) / 2.0f;
}

/* **** 2D Voronoi **** */

static float voronoi_distance(const float2 a,
                              const float2 b,
                              const int metric,
                              const float exponent)
{
  switch (metric) {
    case NOISE_SHD_VORONOI_EUCLIDEAN:
      return float2::distance(a, b);
    case NOISE_SHD_VORONOI_MANHATTAN:
      return fabsf(a.x - b.x) + fabsf(a.y - b.y);
    case NOISE_SHD_VORONOI_CHEBYCHEV:
      return std::max(fabsf(a.x - b.x), fabsf(a.y - b.y));
    case NOISE_SHD_VORONOI_MINKOWSKI:
      return powf(powf(fabsf(a.x - b.x), exponent) + powf(fabsf(a.y - b.y), exponent),
                  1.0f / exponent);
    default:
      BLI_assert_unreachable();
      break;
  }
  return 0.0f;
}

void voronoi_f1(const float2 coord,
                const float exponent,
                const float randomness,
                const int metric,
                float *r_distance,
                float3 *r_color,
                float2 *r_position)
{
  const float2 cellPosition = float2::floor(coord);
  const float2 localPosition = coord - cellPosition;

  float minDistance = 8.0f;
  float2 targetOffset = float2(0.0f, 0.0f);
  float2 targetPosition = float2(0.0f, 0.0f);
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      const float2 cellOffset = float2(i, j);
      const float2 pointPosition = cellOffset +
                                   hash_float_to_float2(cellPosition + cellOffset) * randomness;
      float distanceToPoint = voronoi_distance(pointPosition, localPosition, metric, exponent);
      if (distanceToPoint < minDistance) {
        targetOffset = cellOffset;
        minDistance = distanceToPoint;
        targetPosition = pointPosition;
      }
    }
  }
  if (r_distance != nullptr) {
    *r_distance = minDistance;
  }
  if (r_color != nullptr) {
    *r_color = hash_float_to_float3(cellPosition + targetOffset);
  }
  if (r_position != nullptr) {
    *r_position = targetPosition + cellPosition;
  }
}

void voronoi_smooth_f1(const float2 coord,
                       const float smoothness,
                       const float exponent,
                       const float randomness,
                       const int metric,
                       float *r_distance,
                       float3 *r_color,
                       float2 *r_position)
{
  const float2 cellPosition = float2::floor(coord);
  const float2 localPosition = coord - cellPosition;
  const float smoothness_clamped = max_ff(smoothness, FLT_MIN);

  float smoothDistance = 8.0f;
  float3 smoothColor = float3(0.0f, 0.0f, 0.0f);
  float2 smoothPosition = float2(0.0f, 0.0f);
  for (int j = -2; j <= 2; j++) {
    for (int i = -2; i <= 2; i++) {
      const float2 cellOffset = float2(i, j);
      const float2 pointPosition = cellOffset +
                                   hash_float_to_float2(cellPosition + cellOffset) * randomness;
      const float distanceToPoint = voronoi_distance(
          pointPosition, localPosition, metric, exponent);
      const float h = smoothstep(
          0.0f, 1.0f, 0.5f + 0.5f * (smoothDistance - distanceToPoint) / smoothness_clamped);
      float correctionFactor = smoothness * h * (1.0f - h);
      smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
      if (r_color != nullptr || r_position != nullptr) {
        correctionFactor /= 1.0f + 3.0f * smoothness;
        if (r_color != nullptr) {
          const float3 cellColor = hash_float_to_float3(cellPosition + cellOffset);
          smoothColor = float3::interpolate(smoothColor, cellColor, h) - correctionFactor;
        }
        if (r_position != nullptr) {
          smoothPosition = float2::interpolate(smoothPosition, pointPosition, h) -
                           correctionFactor;
        }
      }
    }
  }
  if (r_distance != nullptr) {
    *r_distance = smoothDistance;
  }
  if (r_color != nullptr) {
    *r_color = smoothColor;
  }
  if (r_position != nullptr) {
    *r_position = cellPosition + smoothPosition;
  }
}

void voronoi_f2(const float2 coord,
                const float exponent,
                const float randomness,
                const int metric,
                float *r_distance,
                float3 *r_color,
                float2 *r_position)
{
  const float2 cellPosition = float2::floor(coord);
  const float2 localPosition = coord - cellPosition;

  float distanceF1 = 8.0f;
  float distanceF2 = 8.0f;
  float2 offsetF1 = float2(0.0f, 0.0f);
  float2 positionF1 = float2(0.0f, 0.0f);
  float2 offsetF2 = float2(0.0f, 0.0f);
  float2 positionF2 = float2(0.0f, 0.0f);
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      const float2 cellOffset = float2(i, j);
      const float2 pointPosition = cellOffset +
                                   hash_float_to_float2(cellPosition + cellOffset) * randomness;
      const float distanceToPoint = voronoi_distance(
          pointPosition, localPosition, metric, exponent);
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
  if (r_distance != nullptr) {
    *r_distance = distanceF2;
  }
  if (r_color != nullptr) {
    *r_color = hash_float_to_float3(cellPosition + offsetF2);
  }
  if (r_position != nullptr) {
    *r_position = positionF2 + cellPosition;
  }
}

void voronoi_distance_to_edge(const float2 coord, const float randomness, float *r_distance)
{
  const float2 cellPosition = float2::floor(coord);
  const float2 localPosition = coord - cellPosition;

  float2 vectorToClosest = float2(0.0f, 0.0f);
  float minDistance = 8.0f;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      const float2 cellOffset = float2(i, j);
      const float2 vectorToPoint = cellOffset +
                                   hash_float_to_float2(cellPosition + cellOffset) * randomness -
                                   localPosition;
      const float distanceToPoint = dot_v2v2(vectorToPoint, vectorToPoint);
      if (distanceToPoint < minDistance) {
        minDistance = distanceToPoint;
        vectorToClosest = vectorToPoint;
      }
    }
  }

  minDistance = 8.0f;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      const float2 cellOffset = float2(i, j);
      const float2 vectorToPoint = cellOffset +
                                   hash_float_to_float2(cellPosition + cellOffset) * randomness -
                                   localPosition;
      const float2 perpendicularToEdge = vectorToPoint - vectorToClosest;
      if (dot_v2v2(perpendicularToEdge, perpendicularToEdge) > 0.0001f) {
        const float distanceToEdge = dot_v2v2((vectorToClosest + vectorToPoint) / 2.0f,
                                              perpendicularToEdge.normalized());
        minDistance = std::min(minDistance, distanceToEdge);
      }
    }
  }
  *r_distance = minDistance;
}

void voronoi_n_sphere_radius(const float2 coord, const float randomness, float *r_radius)
{
  const float2 cellPosition = float2::floor(coord);
  const float2 localPosition = coord - cellPosition;

  float2 closestPoint = float2(0.0f, 0.0f);
  float2 closestPointOffset = float2(0.0f, 0.0f);
  float minDistance = 8.0f;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      const float2 cellOffset = float2(i, j);
      const float2 pointPosition = cellOffset +
                                   hash_float_to_float2(cellPosition + cellOffset) * randomness;
      const float distanceToPoint = float2::distance(pointPosition, localPosition);
      if (distanceToPoint < minDistance) {
        minDistance = distanceToPoint;
        closestPoint = pointPosition;
        closestPointOffset = cellOffset;
      }
    }
  }

  minDistance = 8.0f;
  float2 closestPointToClosestPoint = float2(0.0f, 0.0f);
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      if (i == 0 && j == 0) {
        continue;
      }
      const float2 cellOffset = float2(i, j) + closestPointOffset;
      const float2 pointPosition = cellOffset +
                                   hash_float_to_float2(cellPosition + cellOffset) * randomness;
      const float distanceToPoint = float2::distance(closestPoint, pointPosition);
      if (distanceToPoint < minDistance) {
        minDistance = distanceToPoint;
        closestPointToClosestPoint = pointPosition;
      }
    }
  }
  *r_radius = float2::distance(closestPointToClosestPoint, closestPoint) / 2.0f;
}

/* **** 3D Voronoi **** */

static float voronoi_distance(const float3 a,
                              const float3 b,
                              const int metric,
                              const float exponent)
{
  switch (metric) {
    case NOISE_SHD_VORONOI_EUCLIDEAN:
      return float3::distance(a, b);
    case NOISE_SHD_VORONOI_MANHATTAN:
      return fabsf(a.x - b.x) + fabsf(a.y - b.y) + fabsf(a.z - b.z);
    case NOISE_SHD_VORONOI_CHEBYCHEV:
      return std::max(fabsf(a.x - b.x), std::max(fabsf(a.y - b.y), fabsf(a.z - b.z)));
    case NOISE_SHD_VORONOI_MINKOWSKI:
      return powf(powf(fabsf(a.x - b.x), exponent) + powf(fabsf(a.y - b.y), exponent) +
                      powf(fabsf(a.z - b.z), exponent),
                  1.0f / exponent);
    default:
      BLI_assert_unreachable();
      break;
  }
  return 0.0f;
}

void voronoi_f1(const float3 coord,
                const float exponent,
                const float randomness,
                const int metric,
                float *r_distance,
                float3 *r_color,
                float3 *r_position)
{
  const float3 cellPosition = float3::floor(coord);
  const float3 localPosition = coord - cellPosition;

  float minDistance = 8.0f;
  float3 targetOffset = float3(0.0f, 0.0f, 0.0f);
  float3 targetPosition = float3(0.0f, 0.0f, 0.0f);
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        const float3 cellOffset = float3(i, j, k);
        const float3 pointPosition = cellOffset +
                                     hash_float_to_float3(cellPosition + cellOffset) * randomness;
        const float distanceToPoint = voronoi_distance(
            pointPosition, localPosition, metric, exponent);
        if (distanceToPoint < minDistance) {
          targetOffset = cellOffset;
          minDistance = distanceToPoint;
          targetPosition = pointPosition;
        }
      }
    }
  }
  if (r_distance != nullptr) {
    *r_distance = minDistance;
  }
  if (r_color != nullptr) {
    *r_color = hash_float_to_float3(cellPosition + targetOffset);
  }
  if (r_position != nullptr) {
    *r_position = targetPosition + cellPosition;
  }
}

void voronoi_smooth_f1(const float3 coord,
                       const float smoothness,
                       const float exponent,
                       const float randomness,
                       const int metric,
                       float *r_distance,
                       float3 *r_color,
                       float3 *r_position)
{
  const float3 cellPosition = float3::floor(coord);
  const float3 localPosition = coord - cellPosition;
  const float smoothness_clamped = max_ff(smoothness, FLT_MIN);

  float smoothDistance = 8.0f;
  float3 smoothColor = float3(0.0f, 0.0f, 0.0f);
  float3 smoothPosition = float3(0.0f, 0.0f, 0.0f);
  for (int k = -2; k <= 2; k++) {
    for (int j = -2; j <= 2; j++) {
      for (int i = -2; i <= 2; i++) {
        const float3 cellOffset = float3(i, j, k);
        const float3 pointPosition = cellOffset +
                                     hash_float_to_float3(cellPosition + cellOffset) * randomness;
        const float distanceToPoint = voronoi_distance(
            pointPosition, localPosition, metric, exponent);
        const float h = smoothstep(
            0.0f, 1.0f, 0.5f + 0.5f * (smoothDistance - distanceToPoint) / smoothness_clamped);
        float correctionFactor = smoothness * h * (1.0f - h);
        smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
        if (r_color != nullptr || r_position != nullptr) {
          correctionFactor /= 1.0f + 3.0f * smoothness;
          if (r_color != nullptr) {
            const float3 cellColor = hash_float_to_float3(cellPosition + cellOffset);
            smoothColor = float3::interpolate(smoothColor, cellColor, h) - correctionFactor;
          }
          if (r_position != nullptr) {
            smoothPosition = float3::interpolate(smoothPosition, pointPosition, h) -
                             correctionFactor;
          }
        }
      }
    }
  }
  if (r_distance != nullptr) {
    *r_distance = smoothDistance;
  }
  if (r_color != nullptr) {
    *r_color = smoothColor;
  }
  if (r_position != nullptr) {
    *r_position = cellPosition + smoothPosition;
  }
}

void voronoi_f2(const float3 coord,
                const float exponent,
                const float randomness,
                const int metric,
                float *r_distance,
                float3 *r_color,
                float3 *r_position)
{
  const float3 cellPosition = float3::floor(coord);
  const float3 localPosition = coord - cellPosition;

  float distanceF1 = 8.0f;
  float distanceF2 = 8.0f;
  float3 offsetF1 = float3(0.0f, 0.0f, 0.0f);
  float3 positionF1 = float3(0.0f, 0.0f, 0.0f);
  float3 offsetF2 = float3(0.0f, 0.0f, 0.0f);
  float3 positionF2 = float3(0.0f, 0.0f, 0.0f);
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        const float3 cellOffset = float3(i, j, k);
        const float3 pointPosition = cellOffset +
                                     hash_float_to_float3(cellPosition + cellOffset) * randomness;
        const float distanceToPoint = voronoi_distance(
            pointPosition, localPosition, metric, exponent);
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
  if (r_distance != nullptr) {
    *r_distance = distanceF2;
  }
  if (r_color != nullptr) {
    *r_color = hash_float_to_float3(cellPosition + offsetF2);
  }
  if (r_position != nullptr) {
    *r_position = positionF2 + cellPosition;
  }
}

void voronoi_distance_to_edge(const float3 coord, const float randomness, float *r_distance)
{
  const float3 cellPosition = float3::floor(coord);
  const float3 localPosition = coord - cellPosition;

  float3 vectorToClosest = float3(0.0f, 0.0f, 0.0f);
  float minDistance = 8.0f;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        const float3 cellOffset = float3(i, j, k);
        const float3 vectorToPoint = cellOffset +
                                     hash_float_to_float3(cellPosition + cellOffset) * randomness -
                                     localPosition;
        const float distanceToPoint = dot_v3v3(vectorToPoint, vectorToPoint);
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
        const float3 cellOffset = float3(i, j, k);
        const float3 vectorToPoint = cellOffset +
                                     hash_float_to_float3(cellPosition + cellOffset) * randomness -
                                     localPosition;
        const float3 perpendicularToEdge = vectorToPoint - vectorToClosest;
        if (dot_v3v3(perpendicularToEdge, perpendicularToEdge) > 0.0001f) {
          const float distanceToEdge = dot_v3v3((vectorToClosest + vectorToPoint) / 2.0f,
                                                perpendicularToEdge.normalized());
          minDistance = std::min(minDistance, distanceToEdge);
        }
      }
    }
  }
  *r_distance = minDistance;
}

void voronoi_n_sphere_radius(const float3 coord, const float randomness, float *r_radius)
{
  const float3 cellPosition = float3::floor(coord);
  const float3 localPosition = coord - cellPosition;

  float3 closestPoint = float3(0.0f, 0.0f, 0.0f);
  float3 closestPointOffset = float3(0.0f, 0.0f, 0.0f);
  float minDistance = 8.0f;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        const float3 cellOffset = float3(i, j, k);
        const float3 pointPosition = cellOffset +
                                     hash_float_to_float3(cellPosition + cellOffset) * randomness;
        const float distanceToPoint = float3::distance(pointPosition, localPosition);
        if (distanceToPoint < minDistance) {
          minDistance = distanceToPoint;
          closestPoint = pointPosition;
          closestPointOffset = cellOffset;
        }
      }
    }
  }

  minDistance = 8.0f;
  float3 closestPointToClosestPoint = float3(0.0f, 0.0f, 0.0f);
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        if (i == 0 && j == 0 && k == 0) {
          continue;
        }
        const float3 cellOffset = float3(i, j, k) + closestPointOffset;
        const float3 pointPosition = cellOffset +
                                     hash_float_to_float3(cellPosition + cellOffset) * randomness;
        const float distanceToPoint = float3::distance(closestPoint, pointPosition);
        if (distanceToPoint < minDistance) {
          minDistance = distanceToPoint;
          closestPointToClosestPoint = pointPosition;
        }
      }
    }
  }
  *r_radius = float3::distance(closestPointToClosestPoint, closestPoint) / 2.0f;
}

/* **** 4D Voronoi **** */

static float voronoi_distance(const float4 a,
                              const float4 b,
                              const int metric,
                              const float exponent)
{
  switch (metric) {
    case NOISE_SHD_VORONOI_EUCLIDEAN:
      return float4::distance(a, b);
    case NOISE_SHD_VORONOI_MANHATTAN:
      return fabsf(a.x - b.x) + fabsf(a.y - b.y) + fabsf(a.z - b.z) + fabsf(a.w - b.w);
    case NOISE_SHD_VORONOI_CHEBYCHEV:
      return std::max(fabsf(a.x - b.x),
                      std::max(fabsf(a.y - b.y), std::max(fabsf(a.z - b.z), fabsf(a.w - b.w))));
    case NOISE_SHD_VORONOI_MINKOWSKI:
      return powf(powf(fabsf(a.x - b.x), exponent) + powf(fabsf(a.y - b.y), exponent) +
                      powf(fabsf(a.z - b.z), exponent) + powf(fabsf(a.w - b.w), exponent),
                  1.0f / exponent);
    default:
      BLI_assert_unreachable();
      break;
  }
  return 0.0f;
}

void voronoi_f1(const float4 coord,
                const float exponent,
                const float randomness,
                const int metric,
                float *r_distance,
                float3 *r_color,
                float4 *r_position)
{
  const float4 cellPosition = float4::floor(coord);
  const float4 localPosition = coord - cellPosition;

  float minDistance = 8.0f;
  float4 targetOffset = float4(0.0f, 0.0f, 0.0f, 0.0f);
  float4 targetPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          const float4 cellOffset = float4(i, j, k, u);
          const float4 pointPosition = cellOffset +
                                       hash_float_to_float4(cellPosition + cellOffset) *
                                           randomness;
          const float distanceToPoint = voronoi_distance(
              pointPosition, localPosition, metric, exponent);
          if (distanceToPoint < minDistance) {
            targetOffset = cellOffset;
            minDistance = distanceToPoint;
            targetPosition = pointPosition;
          }
        }
      }
    }
  }
  if (r_distance != nullptr) {
    *r_distance = minDistance;
  }
  if (r_color != nullptr) {
    *r_color = hash_float_to_float3(cellPosition + targetOffset);
  }
  if (r_position != nullptr) {
    *r_position = targetPosition + cellPosition;
  }
}

void voronoi_smooth_f1(const float4 coord,
                       const float smoothness,
                       const float exponent,
                       const float randomness,
                       const int metric,
                       float *r_distance,
                       float3 *r_color,
                       float4 *r_position)
{
  const float4 cellPosition = float4::floor(coord);
  const float4 localPosition = coord - cellPosition;
  const float smoothness_clamped = max_ff(smoothness, FLT_MIN);

  float smoothDistance = 8.0f;
  float3 smoothColor = float3(0.0f, 0.0f, 0.0f);
  float4 smoothPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);
  for (int u = -2; u <= 2; u++) {
    for (int k = -2; k <= 2; k++) {
      for (int j = -2; j <= 2; j++) {
        for (int i = -2; i <= 2; i++) {
          const float4 cellOffset = float4(i, j, k, u);
          const float4 pointPosition = cellOffset +
                                       hash_float_to_float4(cellPosition + cellOffset) *
                                           randomness;
          const float distanceToPoint = voronoi_distance(
              pointPosition, localPosition, metric, exponent);
          const float h = smoothstep(
              0.0f, 1.0f, 0.5f + 0.5f * (smoothDistance - distanceToPoint) / smoothness_clamped);
          float correctionFactor = smoothness * h * (1.0f - h);
          smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
          if (r_color != nullptr || r_position != nullptr) {
            correctionFactor /= 1.0f + 3.0f * smoothness;
            if (r_color != nullptr) {
              const float3 cellColor = hash_float_to_float3(cellPosition + cellOffset);
              smoothColor = float3::interpolate(smoothColor, cellColor, h) - correctionFactor;
            }
            if (r_position != nullptr) {
              smoothPosition = float4::interpolate(smoothPosition, pointPosition, h) -
                               correctionFactor;
            }
          }
        }
      }
    }
  }
  if (r_distance != nullptr) {
    *r_distance = smoothDistance;
  }
  if (r_color != nullptr) {
    *r_color = smoothColor;
  }
  if (r_position != nullptr) {
    *r_position = cellPosition + smoothPosition;
  }
}

void voronoi_f2(const float4 coord,
                const float exponent,
                const float randomness,
                const int metric,
                float *r_distance,
                float3 *r_color,
                float4 *r_position)
{
  const float4 cellPosition = float4::floor(coord);
  const float4 localPosition = coord - cellPosition;

  float distanceF1 = 8.0f;
  float distanceF2 = 8.0f;
  float4 offsetF1 = float4(0.0f, 0.0f, 0.0f, 0.0f);
  float4 positionF1 = float4(0.0f, 0.0f, 0.0f, 0.0f);
  float4 offsetF2 = float4(0.0f, 0.0f, 0.0f, 0.0f);
  float4 positionF2 = float4(0.0f, 0.0f, 0.0f, 0.0f);
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          const float4 cellOffset = float4(i, j, k, u);
          const float4 pointPosition = cellOffset +
                                       hash_float_to_float4(cellPosition + cellOffset) *
                                           randomness;
          const float distanceToPoint = voronoi_distance(
              pointPosition, localPosition, metric, exponent);
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
  if (r_distance != nullptr) {
    *r_distance = distanceF2;
  }
  if (r_color != nullptr) {
    *r_color = hash_float_to_float3(cellPosition + offsetF2);
  }
  if (r_position != nullptr) {
    *r_position = positionF2 + cellPosition;
  }
}

void voronoi_distance_to_edge(const float4 coord, const float randomness, float *r_distance)
{
  const float4 cellPosition = float4::floor(coord);
  const float4 localPosition = coord - cellPosition;

  float4 vectorToClosest = float4(0.0f, 0.0f, 0.0f, 0.0f);
  float minDistance = 8.0f;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          const float4 cellOffset = float4(i, j, k, u);
          const float4 vectorToPoint = cellOffset +
                                       hash_float_to_float4(cellPosition + cellOffset) *
                                           randomness -
                                       localPosition;
          const float distanceToPoint = dot_v4v4(vectorToPoint, vectorToPoint);
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
          const float4 cellOffset = float4(i, j, k, u);
          const float4 vectorToPoint = cellOffset +
                                       hash_float_to_float4(cellPosition + cellOffset) *
                                           randomness -
                                       localPosition;
          const float4 perpendicularToEdge = vectorToPoint - vectorToClosest;
          if (dot_v4v4(perpendicularToEdge, perpendicularToEdge) > 0.0001f) {
            const float distanceToEdge = dot_v4v4((vectorToClosest + vectorToPoint) / 2.0f,
                                                  float4::normalize(perpendicularToEdge));
            minDistance = std::min(minDistance, distanceToEdge);
          }
        }
      }
    }
  }
  *r_distance = minDistance;
}

void voronoi_n_sphere_radius(const float4 coord, const float randomness, float *r_radius)
{
  const float4 cellPosition = float4::floor(coord);
  const float4 localPosition = coord - cellPosition;

  float4 closestPoint = float4(0.0f, 0.0f, 0.0f, 0.0f);
  float4 closestPointOffset = float4(0.0f, 0.0f, 0.0f, 0.0f);
  float minDistance = 8.0f;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          const float4 cellOffset = float4(i, j, k, u);
          const float4 pointPosition = cellOffset +
                                       hash_float_to_float4(cellPosition + cellOffset) *
                                           randomness;
          const float distanceToPoint = float4::distance(pointPosition, localPosition);
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
  float4 closestPointToClosestPoint = float4(0.0f, 0.0f, 0.0f, 0.0f);
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          if (i == 0 && j == 0 && k == 0 && u == 0) {
            continue;
          }
          const float4 cellOffset = float4(i, j, k, u) + closestPointOffset;
          const float4 pointPosition = cellOffset +
                                       hash_float_to_float4(cellPosition + cellOffset) *
                                           randomness;
          const float distanceToPoint = float4::distance(closestPoint, pointPosition);
          if (distanceToPoint < minDistance) {
            minDistance = distanceToPoint;
            closestPointToClosestPoint = pointPosition;
          }
        }
      }
    }
  }
  *r_radius = float4::distance(closestPointToClosestPoint, closestPoint) / 2.0f;
}

}  // namespace blender::noise
