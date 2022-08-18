/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_HASH_H__
#define __UTIL_HASH_H__

#include "util/types.h"

CCL_NAMESPACE_BEGIN

/* [0, uint_max] -> [0.0, 1.0) */
ccl_device_forceinline float uint_to_float_excl(uint n)
{
  // Note: we divide by 4294967808 instead of 2^32 because the latter
  // leads to a [0.0, 1.0] mapping instead of [0.0, 1.0) due to floating
  // point rounding error. 4294967808 unfortunately leaves (precisely)
  // one unused ulp between the max number this outputs and 1.0, but
  // that's the best you can do with this construction.
  return (float)n * (1.0f / 4294967808.0f);
}

/* [0, uint_max] -> [0.0, 1.0] */
ccl_device_forceinline float uint_to_float_incl(uint n)
{
  return (float)n * (1.0f / (float)0xFFFFFFFFu);
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

ccl_device_inline uint hash_uint(uint kx)
{
  uint a, b, c;
  a = b = c = 0xdeadbeef + (1 << 2) + 13;

  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline uint hash_uint2(uint kx, uint ky)
{
  uint a, b, c;
  a = b = c = 0xdeadbeef + (2 << 2) + 13;

  b += ky;
  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline uint hash_uint3(uint kx, uint ky, uint kz)
{
  uint a, b, c;
  a = b = c = 0xdeadbeef + (3 << 2) + 13;

  c += kz;
  b += ky;
  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline uint hash_uint4(uint kx, uint ky, uint kz, uint kw)
{
  uint a, b, c;
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

ccl_device_inline float hash_uint_to_float(uint kx)
{
  return uint_to_float_incl(hash_uint(kx));
}

ccl_device_inline float hash_uint2_to_float(uint kx, uint ky)
{
  return uint_to_float_incl(hash_uint2(kx, ky));
}

ccl_device_inline float hash_uint3_to_float(uint kx, uint ky, uint kz)
{
  return uint_to_float_incl(hash_uint3(kx, ky, kz));
}

ccl_device_inline float hash_uint4_to_float(uint kx, uint ky, uint kz, uint kw)
{
  return uint_to_float_incl(hash_uint4(kx, ky, kz, kw));
}

/* Hashing float or float[234] into a float in the range [0, 1]. */

ccl_device_inline float hash_float_to_float(float k)
{
  return hash_uint_to_float(__float_as_uint(k));
}

ccl_device_inline float hash_float2_to_float(float2 k)
{
  return hash_uint2_to_float(__float_as_uint(k.x), __float_as_uint(k.y));
}

ccl_device_inline float hash_float3_to_float(float3 k)
{
  return hash_uint3_to_float(__float_as_uint(k.x), __float_as_uint(k.y), __float_as_uint(k.z));
}

ccl_device_inline float hash_float4_to_float(float4 k)
{
  return hash_uint4_to_float(
      __float_as_uint(k.x), __float_as_uint(k.y), __float_as_uint(k.z), __float_as_uint(k.w));
}

/* Hashing float[234] into float[234] of components in the range [0, 1]. */

ccl_device_inline float2 hash_float2_to_float2(float2 k)
{
  return make_float2(hash_float2_to_float(k), hash_float3_to_float(make_float3(k.x, k.y, 1.0)));
}

ccl_device_inline float3 hash_float3_to_float3(float3 k)
{
  return make_float3(hash_float3_to_float(k),
                     hash_float4_to_float(make_float4(k.x, k.y, k.z, 1.0)),
                     hash_float4_to_float(make_float4(k.x, k.y, k.z, 2.0)));
}

ccl_device_inline float4 hash_float4_to_float4(float4 k)
{
  return make_float4(hash_float4_to_float(k),
                     hash_float4_to_float(make_float4(k.w, k.x, k.y, k.z)),
                     hash_float4_to_float(make_float4(k.z, k.w, k.x, k.y)),
                     hash_float4_to_float(make_float4(k.y, k.z, k.w, k.x)));
}

/* Hashing float or float[234] into float3 of components in range [0, 1]. */

ccl_device_inline float3 hash_float_to_float3(float k)
{
  return make_float3(hash_float_to_float(k),
                     hash_float2_to_float(make_float2(k, 1.0)),
                     hash_float2_to_float(make_float2(k, 2.0)));
}

ccl_device_inline float3 hash_float2_to_float3(float2 k)
{
  return make_float3(hash_float2_to_float(k),
                     hash_float3_to_float(make_float3(k.x, k.y, 1.0)),
                     hash_float3_to_float(make_float3(k.x, k.y, 2.0)));
}

ccl_device_inline float3 hash_float4_to_float3(float4 k)
{
  return make_float3(hash_float4_to_float(k),
                     hash_float4_to_float(make_float4(k.z, k.x, k.w, k.y)),
                     hash_float4_to_float(make_float4(k.w, k.z, k.y, k.x)));
}

/* SSE Versions Of Jenkins Lookup3 Hash Functions */

#ifdef __KERNEL_SSE2__
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

ccl_device_inline ssei hash_ssei(ssei kx)
{
  ssei a, b, c;
  a = b = c = ssei(0xdeadbeef + (1 << 2) + 13);

  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline ssei hash_ssei2(ssei kx, ssei ky)
{
  ssei a, b, c;
  a = b = c = ssei(0xdeadbeef + (2 << 2) + 13);

  b += ky;
  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline ssei hash_ssei3(ssei kx, ssei ky, ssei kz)
{
  ssei a, b, c;
  a = b = c = ssei(0xdeadbeef + (3 << 2) + 13);

  c += kz;
  b += ky;
  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline ssei hash_ssei4(ssei kx, ssei ky, ssei kz, ssei kw)
{
  ssei a, b, c;
  a = b = c = ssei(0xdeadbeef + (4 << 2) + 13);

  a += kx;
  b += ky;
  c += kz;
  mix(a, b, c);

  a += kw;
  final(a, b, c);

  return c;
}

#  if defined(__KERNEL_AVX__)
ccl_device_inline avxi hash_avxi(avxi kx)
{
  avxi a, b, c;
  a = b = c = avxi(0xdeadbeef + (1 << 2) + 13);

  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline avxi hash_avxi2(avxi kx, avxi ky)
{
  avxi a, b, c;
  a = b = c = avxi(0xdeadbeef + (2 << 2) + 13);

  b += ky;
  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline avxi hash_avxi3(avxi kx, avxi ky, avxi kz)
{
  avxi a, b, c;
  a = b = c = avxi(0xdeadbeef + (3 << 2) + 13);

  c += kz;
  b += ky;
  a += kx;
  final(a, b, c);

  return c;
}

ccl_device_inline avxi hash_avxi4(avxi kx, avxi ky, avxi kz, avxi kw)
{
  avxi a, b, c;
  a = b = c = avxi(0xdeadbeef + (4 << 2) + 13);

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
 * These are based on the high-quality 32-bit hash/mixings functions from
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
ccl_device_inline uint hash_hp_seeded_uint(uint i, uint seed)
{
  // Manipulate the seed so it doesn't interact poorly with n when they
  // are both e.g. incrementing.  This isn't fool-proof, but is good
  // enough for practical use.
  seed ^= seed << 19;

  return hash_hp_uint(i ^ seed);
}

/* Outputs [0.0, 1.0]. */
ccl_device_inline float hash_hp_seeded_float(uint i, uint seed)
{
  return uint_to_float_incl(hash_hp_seeded_uint(i, seed));
}

/* ***** CMJ Hash Functions *****
 *
 * These are based on one of the hash functions in the paper
 * "Correlated Multi-Jittered Sampling" by Andrew Kensler, 2013.
 *
 * These are here for backwards-compatibility, and can be replaced
 * by the Hash Prospector hashes above at some point.
 * See https://developer.blender.org/D15679#426304
 */

ccl_device_inline uint hash_cmj_seeded_uint(uint i, uint seed)
{
  i ^= seed;
  i ^= i >> 17;
  i ^= i >> 10;
  i *= 0xb36534e5;
  i ^= i >> 12;
  i ^= i >> 21;
  i *= 0x93fc4795;
  i ^= 0xdf6e307f;
  i ^= i >> 17;
  i *= 1 | seed >> 18;

  return i;
}

/* Outputs [0.0, 1.0]. */
ccl_device_inline float hash_cmj_seeded_float(uint i, uint seed)
{
  return uint_to_float_excl(hash_cmj_seeded_uint(i, seed));
}

/* ***** Modified Wang Hash Functions *****
 *
 * These are based on a bespoke modified version of the Wang hash, and
 * can serve as a faster hash when quality isn't critical.
 *
 * The original Wang hash is documented here:
 * https://www.burtleburtle.net/bob/hash/integer.html
 */

ccl_device_inline uint hash_wang_seeded_uint(uint i, uint seed)
{
  i = (i ^ 61) ^ seed;
  i += i << 3;
  i ^= i >> 4;
  i *= 0x27d4eb2d;
  return i;
}

/* Outputs [0.0, 1.0]. */
ccl_device_inline float hash_wang_seeded_float(uint i, uint seed)
{
  return uint_to_float_incl(hash_wang_seeded_uint(i, seed));
}

/* ********** */

#ifndef __KERNEL_GPU__
static inline uint hash_string(const char *str)
{
  uint i = 0, c;

  while ((c = *str++))
    i = i * 37 + c;

  return i;
}
#endif

CCL_NAMESPACE_END

#endif /* __UTIL_HASH_H__ */
