/* SPDX-FileCopyrightText: 2022-2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/** \file
 * \ingroup mikktspace
 */

#pragma once

#include <cassert>
#include <cmath>

#ifndef M_PI_F
#  define M_PI_F (3.1415926535897932f) /* pi */
#endif

namespace mikk {

inline bool not_zero(const float fX)
{
  return fabsf(fX) > FLT_MIN;
}

/* Helpers for (un)packing a 2-bit vertex index and a 30-bit face index to one integer. */
static uint pack_index(const uint face, const uint vert)
{
  assert((vert & 0x3) == vert);
  return (face << 2) | (vert & 0x3);
}

static void unpack_index(uint &face, uint &vert, const uint indexIn)
{
  vert = indexIn & 0x3;
  face = indexIn >> 2;
}

/* From intern/cycles/util/math_fast.h */
inline float fast_acosf(float x)
{
  const float f = fabsf(x);
  /* clamp and crush denormals. */
  const float m = (f < 1.0f) ? 1.0f - (1.0f - f) : 1.0f;
  /* Based on http://www.pouet.net/topic.php?which=9132&page=2
   * 85% accurate (ULP 0)
   * Examined 2130706434 values of acos:
   *   15.2000597 avg ULP diff, 4492 max ULP, 4.51803e-05 max error // without "denormal crush"
   * Examined 2130706434 values of acos:
   *   15.2007108 avg ULP diff, 4492 max ULP, 4.51803e-05 max error // with "denormal crush"
   */
  const float a = sqrtf(1.0f - m) *
                  (1.5707963267f + m * (-0.213300989f + m * (0.077980478f + m * -0.02164095f)));
  return x < 0 ? M_PI_F - a : a;
}

static uint rotl(uint x, uint k)
{
  return (x << k) | (x >> (32 - k));
}

static uint hash_uint3(uint kx, uint ky, uint kz)
{
  uint a, b, c;
  a = b = c = 0xdeadbeef + (2 << 2) + 13;

  c += kz;
  b += ky;
  a += kx;

  c = (c ^ b) - rotl(b, 14);
  a = (a ^ c) - rotl(c, 11);
  b = (b ^ a) - rotl(a, 25);
  c = (c ^ b) - rotl(b, 16);

  return c;
}

static uint hash_uint3_fast(const uint x, const uint y, const uint z)
{
  return (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
}

static uint float_as_uint(const float v)
{
  return *((uint *)(&v));
}

static float uint_as_float(const uint v)
{
  return *((float *)(&v));
}

static uint hash_float3_fast(const float x, const float y, const float z)
{
  return hash_uint3_fast(float_as_uint(x), float_as_uint(y), float_as_uint(z));
}

static uint hash_float3x3(const float3 &x, const float3 &y, const float3 &z)
{
  return hash_uint3(hash_float3_fast(x.x, x.y, x.z),
                    hash_float3_fast(y.x, y.y, y.z),
                    hash_float3_fast(z.x, z.y, z.z));
}

template<typename T, typename KeyGetter>
void radixsort(std::vector<T> &data, std::vector<T> &data2, KeyGetter getKey)
{
  typedef decltype(getKey(data[0])) key_t;
  constexpr size_t datasize = sizeof(key_t);
  static_assert(datasize % 2 == 0);
  static_assert(std::is_integral<key_t>::value);

  uint bins[datasize][257] = {{0}};

  /* Count number of elements per bin. */
  for (const T &item : data) {
    key_t key = getKey(item);
    for (uint pass = 0; pass < datasize; pass++)
      bins[pass][((key >> (8 * pass)) & 0xff) + 1]++;
  }

  /* Compute prefix sum to find position of each bin in the sorted array. */
  for (uint pass = 0; pass < datasize; pass++) {
    for (uint i = 2; i < 256; i++) {
      bins[pass][i] += bins[pass][i - 1];
    }
  }

  int shift = 0;
  for (uint pass = 0; pass < datasize; pass++, shift += 8) {
    /* Insert the elements in their correct location based on their bin. */
    for (const T &item : data) {
      uint pos = bins[pass][(getKey(item) >> shift) & 0xff]++;
      data2[pos] = item;
    }

    /* Swap arrays. */
    std::swap(data, data2);
  }
}

static void float_add_atomic(float *val, float add)
{
  /* Hacky, but atomic floats are only supported from C++20 onward.
   * This works in practice since `std::atomic<uint32_t>` is really just an `uint32_t` in memory,
   * so this cast lets us do a 32-bit CAS operation (which is used to build the atomic float
   * operation) without needing any external libraries or compiler-specific builtins. */
  std::atomic<uint32_t> *atomic_val = reinterpret_cast<std::atomic<uint32_t> *>(val);
  for (;;) {
    uint32_t old_v = atomic_val->load();
    uint32_t new_v = float_as_uint(uint_as_float(old_v) + add);
    if (atomic_val->compare_exchange_weak(old_v, new_v)) {
      return;
    }
  }
}

}  // namespace mikk
