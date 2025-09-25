/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/defines.h"
#include "util/math.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

/* [0, uint_max] -> [0.0, 1.0) */
ccl_device_forceinline float uint_to_float_excl(const uint n)
{
  /* NOTE: we divide by 4294967808 instead of 2^32 because the latter
   * leads to a [0.0, 1.0] mapping instead of [0.0, 1.0) due to floating
   * point rounding error. 4294967808 unfortunately leaves (precisely)
   * one unused ULP between the max number this outputs and 1.0, but
   * that's the best you can do with this construction. */
  return (float)n * (1.0f / 4294967808.0f);
}

/* [0, uint_max] -> [0.0, 1.0] */
ccl_device_forceinline float uint_to_float_incl(const uint n)
{
  return (float)n * (1.0f / (float)0xFFFFFFFFu);
}

/* PCG 2D, 3D and 4D hash functions,
 * from "Hash Functions for GPU Rendering" JCGT 2020
 * https://jcgt.org/published/0009/03/02/
 *
 * Slightly modified to only use signed integers,
 * so that they can also be implemented in OSL.
 *
 * Silence UBsan warnings about signed integer overflow. */

ccl_ignore_integer_overflow ccl_device_inline int2 hash_pcg2d_i(int2 v)
{
  v = v * make_int2(1664525) + make_int2(1013904223);
  v.x += v.y * 1664525;
  v.y += v.x * 1664525;
  v = v ^ (v >> 16);
  v.x += v.y * 1664525;
  v.y += v.x * 1664525;
  return v & make_int2(0x7FFFFFFF);
}

ccl_ignore_integer_overflow ccl_device_inline int3 hash_pcg3d_i(int3 v)
{
  v = v * make_int3(1664525) + make_int3(1013904223);
  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v = v ^ (v >> 16);
  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  return v & make_int3(0x7FFFFFFF);
}

ccl_ignore_integer_overflow ccl_device_inline int4 hash_pcg4d_i(int4 v)
{
  v = v * make_int4(1664525) + make_int4(1013904223);
  v.x += v.y * v.w;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v.w += v.y * v.z;
  v = v ^ (v >> 16);
  v.x += v.y * v.w;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v.w += v.y * v.z;
  return v & make_int4(0x7FFFFFFF);
}

/* ***** Jenkins Lookup3 Hash Functions ***** */

/* Source: http://burtleburtle.net/bob/c/lookup3.c */

#define rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))

#define mix(a, b, c) \
  { \
    a -= c; \
    a ^= rot(c, 4); \
    c += b; \
    b -= a; \
    b ^= rot(a, 6); \
    a += c; \
    c -= b; \
    c ^= rot(b, 8); \
    b += a; \
    a -= c; \
    a ^= rot(c, 16); \
    c += b; \
    b -= a; \
    b ^= rot(a, 19); \
    a += c; \
    c -= b; \
    c ^= rot(b, 4); \
    b += a; \
  } \
  ((void)0)

#define final(a, b, c) \
  { \
    c ^= b; \
    c -= rot(b, 14); \
    a ^= c; \
    a -= rot(c, 11); \
    b ^= a; \
    b -= rot(a, 25); \
    c ^= b; \
    c -= rot(b, 16); \
    a ^= c; \
    a -= rot(c, 4); \
    b ^= a; \
    b -= rot(a, 14); \
    c ^= b; \
    c -= rot(b, 24); \
  } \
  ((void)0)

ccl_device_inline uint hash_uint(const uint kx)
{
  uint a;
  uint b;
  uint c;
  a = b = c = 0xdeadbeef + (1 << 2) + 13;

  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline uint hash_uint2(const uint kx, const uint ky)
{
  uint a;
  uint b;
  uint c;
  a = b = c = 0xdeadbeef + (2 << 2) + 13;

  b += ky;
  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline uint hash_uint3(const uint kx, const uint ky, const uint kz)
{
  uint a;
  uint b;
  uint c;
  a = b = c = 0xdeadbeef + (3 << 2) + 13;

  c += kz;
  b += ky;
  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline uint hash_uint4(const uint kx, const uint ky, const uint kz, const uint kw)
{
  uint a;
  uint b;
  uint c;
  a = b = c = 0xdeadbeef + (4 << 2) + 13;

  a += kx;
  b += ky;
  c += kz;
  mix(a, b, c);

  a += kw;
  final(a, b, c);

  return c;
}

#undef rot
#undef final
#undef mix

/* Hashing uint or uint[234] into a float in the range [0, 1]. */

ccl_device_inline float hash_uint_to_float(const uint kx)
{
  return uint_to_float_incl(hash_uint(kx));
}

ccl_device_inline float hash_uint2_to_float(const uint kx, const uint ky)
{
  return uint_to_float_incl(hash_uint2(kx, ky));
}

ccl_device_inline float hash_uint3_to_float(const uint kx, const uint ky, const uint kz)
{
  return uint_to_float_incl(hash_uint3(kx, ky, kz));
}

ccl_device_inline float hash_uint4_to_float(const uint kx,
                                            const uint ky,
                                            const uint kz,
                                            const uint kw)
{
  return uint_to_float_incl(hash_uint4(kx, ky, kz, kw));
}

/* Hashing float or float[234] into a float in the range [0, 1]. */

ccl_device_inline float hash_float_to_float(const float k)
{
  return hash_uint_to_float(__float_as_uint(k));
}

ccl_device_inline float hash_float2_to_float(const float2 k)
{
  return hash_uint2_to_float(__float_as_uint(k.x), __float_as_uint(k.y));
}

ccl_device_inline float hash_float3_to_float(const float3 k)
{
  return hash_uint3_to_float(__float_as_uint(k.x), __float_as_uint(k.y), __float_as_uint(k.z));
}

ccl_device_inline float hash_float4_to_float(const float4 k)
{
  return hash_uint4_to_float(
      __float_as_uint(k.x), __float_as_uint(k.y), __float_as_uint(k.z), __float_as_uint(k.w));
}

/* Hashing int[234] into float[234] of components in the range [0, 1].
 * These are based on PCG 2D/3D/4D. */

ccl_device_inline float2 hash_int2_to_float2(const int2 k)
{
  int2 h = hash_pcg2d_i(k);
  float2 f = make_float2((float)h.x, (float)h.y);
  return f * (1.0f / (float)0x7FFFFFFFu);
}

ccl_device_inline float3 hash_int3_to_float3(const int3 k)
{
  int3 h = hash_pcg3d_i(k);
  float3 f = make_float3((float)h.x, (float)h.y, (float)h.z);
  return f * (1.0f / (float)0x7FFFFFFFu);
}

ccl_device_inline float4 hash_int4_to_float4(const int4 k)
{
  int4 h = hash_pcg4d_i(k);
  float4 f = make_float4(h);
  return f * (1.0f / (float)0x7FFFFFFFu);
}

ccl_device_inline float3 hash_int2_to_float3(const int2 k)
{
  return hash_int3_to_float3(make_int3(k.x, k.y, 0));
}

ccl_device_inline float3 hash_int4_to_float3(const int4 k)
{
  return make_float3(hash_int4_to_float4(k));
}

/* Hashing int[234] / float[234] into float[234] of components in the range [0, 1].
 *
 * Note that while using a more modern hash (e.g. PCG) would be faster, the current
 * behavior has to be kept to match what is possible in OSL (OSL lacks bit casts and unsigned
 * integers). */

ccl_device_inline float2 hash_float2_to_float2(const float2 k)
{
  return make_float2(hash_float2_to_float(k), hash_float3_to_float(make_float3(k.x, k.y, 1.0)));
}

ccl_device_inline float3 hash_float3_to_float3(const float3 k)
{
  return make_float3(hash_float3_to_float(k),
                     hash_float4_to_float(make_float4(k.x, k.y, k.z, 1.0)),
                     hash_float4_to_float(make_float4(k.x, k.y, k.z, 2.0)));
}

ccl_device_inline float4 hash_float4_to_float4(const float4 k)
{
  return make_float4(hash_float4_to_float(k),
                     hash_float4_to_float(make_float4(k.w, k.x, k.y, k.z)),
                     hash_float4_to_float(make_float4(k.z, k.w, k.x, k.y)),
                     hash_float4_to_float(make_float4(k.y, k.z, k.w, k.x)));
}

/* Hashing float or float[234] into float3 of components in range [0, 1]. */

ccl_device_inline float3 hash_float_to_float3(const float k)
{
  return make_float3(hash_float_to_float(k),
                     hash_float2_to_float(make_float2(k, 1.0)),
                     hash_float2_to_float(make_float2(k, 2.0)));
}

ccl_device_inline float3 hash_float2_to_float3(const float2 k)
{
  return make_float3(hash_float2_to_float(k),
                     hash_float3_to_float(make_float3(k.x, k.y, 1.0)),
                     hash_float3_to_float(make_float3(k.x, k.y, 2.0)));
}

ccl_device_inline float3 hash_float4_to_float3(const float4 k)
{
  return make_float3(hash_float4_to_float(k),
                     hash_float4_to_float(make_float4(k.z, k.x, k.w, k.y)),
                     hash_float4_to_float(make_float4(k.w, k.z, k.y, k.x)));
}

/* Hashing float or float[234] into float2 of components in range [0, 1]. */

ccl_device_inline float2 hash_float_to_float2(const float k)
{
  return make_float2(hash_float_to_float(k), hash_float2_to_float(make_float2(k, 1.0)));
}

ccl_device_inline float2 hash_float3_to_float2(const float3 k)
{
  return make_float2(hash_float3_to_float(make_float3(k.x, k.y, k.z)),
                     hash_float3_to_float(make_float3(k.z, k.x, k.y)));
}

ccl_device_inline float2 hash_float4_to_float2(const float4 k)
{
  return make_float2(hash_float4_to_float(make_float4(k.x, k.y, k.z, k.w)),
                     hash_float4_to_float(make_float4(k.z, k.x, k.w, k.y)));
}

/* SSE Versions Of Jenkins Lookup3 Hash Functions */

#ifdef __KERNEL_SSE__
#  define rot(x, k) (((x) << (k)) | (srl(x, 32 - (k))))

#  define mix(a, b, c) \
    { \
      a -= c; \
      a ^= rot(c, 4); \
      c += b; \
      b -= a; \
      b ^= rot(a, 6); \
      a += c; \
      c -= b; \
      c ^= rot(b, 8); \
      b += a; \
      a -= c; \
      a ^= rot(c, 16); \
      c += b; \
      b -= a; \
      b ^= rot(a, 19); \
      a += c; \
      c -= b; \
      c ^= rot(b, 4); \
      b += a; \
    }

#  define final(a, b, c) \
    { \
      c ^= b; \
      c -= rot(b, 14); \
      a ^= c; \
      a -= rot(c, 11); \
      b ^= a; \
      b -= rot(a, 25); \
      c ^= b; \
      c -= rot(b, 16); \
      a ^= c; \
      a -= rot(c, 4); \
      b ^= a; \
      b -= rot(a, 14); \
      c ^= b; \
      c -= rot(b, 24); \
    }

ccl_device_inline int4 hash_int4(const int4 kx)
{
  int4 a;
  int4 b;
  int4 c;
  a = b = c = make_int4(0xdeadbeef + (1 << 2) + 13);

  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline int4 hash_int4_2(const int4 kx, const int4 ky)
{
  int4 a;
  int4 b;
  int4 c;
  a = b = c = make_int4(0xdeadbeef + (2 << 2) + 13);

  b += ky;
  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline int4 hash_int4_3(const int4 kx, const int4 ky, const int4 kz)
{
  int4 a;
  int4 b;
  int4 c;
  a = b = c = make_int4(0xdeadbeef + (3 << 2) + 13);

  c += kz;
  b += ky;
  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline int4 hash_int4_4(const int4 kx, const int4 ky, const int4 kz, const int4 kw)
{
  int4 a;
  int4 b;
  int4 c;
  a = b = c = make_int4(0xdeadbeef + (4 << 2) + 13);

  a += kx;
  b += ky;
  c += kz;
  mix(a, b, c);

  a += kw;
  final(a, b, c);

  return c;
}

#  if defined(__KERNEL_AVX2__)
ccl_device_inline vint8 hash_int8(vint8 kx)
{
  vint8 a, b, c;
  a = b = c = make_vint8(0xdeadbeef + (1 << 2) + 13);

  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline vint8 hash_int8_2(vint8 kx, vint8 ky)
{
  vint8 a, b, c;
  a = b = c = make_vint8(0xdeadbeef + (2 << 2) + 13);

  b += ky;
  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline vint8 hash_int8_3(vint8 kx, vint8 ky, vint8 kz)
{
  vint8 a, b, c;
  a = b = c = make_vint8(0xdeadbeef + (3 << 2) + 13);

  c += kz;
  b += ky;
  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline vint8 hash_int8_4(vint8 kx, vint8 ky, vint8 kz, vint8 kw)
{
  vint8 a, b, c;
  a = b = c = make_vint8(0xdeadbeef + (4 << 2) + 13);

  a += kx;
  b += ky;
  c += kz;
  mix(a, b, c);

  a += kw;
  final(a, b, c);

  return c;
}
#  endif

#  undef rot
#  undef final
#  undef mix

#endif

/* ***** Hash Prospector Hash Functions *****
 *
 * These are based on the high-quality 32-bit hash/mixing functions from
 * https://github.com/skeeto/hash-prospector
 */

ccl_device_inline uint hash_hp_uint(uint i)
{
  // The actual mixing function from Hash Prospector.
  i ^= i >> 16;
  i *= 0x21f0aaad;
  i ^= i >> 15;
  i *= 0xd35a2d97;
  i ^= i >> 15;

  // The xor is just to make input zero not map to output zero.
  // The number is randomly selected and isn't special.
  return i ^ 0xe6fe3beb;
}

/* Seedable version of hash_hp_uint() above. */
ccl_device_inline uint hash_hp_seeded_uint(const uint i, uint seed)
{
  // Manipulate the seed so it doesn't interact poorly with n when they
  // are both e.g. incrementing.  This isn't fool-proof, but is good
  // enough for practical use.
  seed ^= seed << 19;

  return hash_hp_uint(i ^ seed);
}

/* Outputs [0.0, 1.0). */
ccl_device_inline float hash_hp_float(const uint i)
{
  return uint_to_float_excl(hash_hp_uint(i));
}

/* Outputs [0.0, 1.0). */
ccl_device_inline float hash_hp_seeded_float(const uint i, const uint seed)
{
  return uint_to_float_excl(hash_hp_seeded_uint(i, seed));
}

/* ***** Modified Wang Hash Functions *****
 *
 * These are based on a bespoke modified version of the Wang hash, and
 * can serve as a faster hash when quality isn't critical.
 *
 * The original Wang hash is documented here:
 * https://www.burtleburtle.net/bob/hash/integer.html
 */

ccl_device_inline uint hash_wang_seeded_uint(uint i, const uint seed)
{
  i = (i ^ 61) ^ seed;
  i += i << 3;
  i ^= i >> 4;
  i *= 0x27d4eb2d;
  return i;
}

/* Outputs [0.0, 1.0). */
ccl_device_inline float hash_wang_seeded_float(const uint i, const uint seed)
{
  return uint_to_float_excl(hash_wang_seeded_uint(i, seed));
}

/* ***** Index Shuffling Hash Function *****
 *
 * This function takes an index, the length of the thing the index points
 * into, and returns a shuffled index.  For example, if you pass indices
 * 0 through 19 to this function with a length parameter of 20, it will
 * return the indices in a shuffled order with no repeats.  Indices
 * larger than the length parameter will simply repeat the same shuffled
 * pattern over and over.
 *
 * This is useful for iterating over an array in random shuffled order
 * without having to shuffle the array itself.
 *
 * Passing different seeds results in different random shuffles.
 *
 * This function runs in average O(1) time.
 *
 * See https://andrew-helmer.github.io/permute/ for details on how this
 * works.
 */
ccl_device_inline uint hash_shuffle_uint(uint i, const uint length, const uint seed)
{
  i = i % length;
  const uint mask = (1 << (32 - count_leading_zeros(length - 1))) - 1;

  do {
    i ^= seed;
    i *= 0xe170893d;
    i ^= seed >> 16;
    i ^= (i & mask) >> 4;
    i ^= seed >> 8;
    i *= 0x0929eb3f;
    i ^= seed >> 23;
    i ^= (i & mask) >> 1;
    i *= 1 | seed >> 27;
    i *= 0x6935fa69;
    i ^= (i & mask) >> 11;
    i *= 0x74dcb303;
    i ^= (i & mask) >> 2;
    i *= 0x9e501cc3;
    i ^= (i & mask) >> 2;
    i *= 0xc860a3df;
    i &= mask;
    i ^= i >> 5;
  } while (i >= length);

  return i;
}

/**
 * 2D hash recommended from "Hash Functions for GPU Rendering" JCGT Vol. 9, No. 3, 2020
 * See https://www.shadertoy.com/view/4tXyWN and https://www.shadertoy.com/view/XlGcRh
 * http://www.jcgt.org/published/0009/03/02/paper.pdf
 */
ccl_device_inline uint hash_iqnt2d(const uint x, const uint y)
{
  const uint qx = 1103515245U * ((x >> 1U) ^ (y));
  const uint qy = 1103515245U * ((y >> 1U) ^ (x));
  const uint n = 1103515245U * ((qx) ^ (qy >> 3U));

  return n;
}

/* ********** */

#ifndef __KERNEL_GPU__
static inline uint hash_string(const char *str)
{
  uint i = 0;
  uint c;

  while ((c = *str++)) {
    i = i * 37 + c;
  }

  return i;
}
#endif

CCL_NAMESPACE_END
