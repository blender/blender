/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_HASH_H__
#define __UTIL_HASH_H__

#include "util/types.h"

CCL_NAMESPACE_BEGIN

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
  return (float)hash_uint(kx) / (float)0xFFFFFFFFu;
}

ccl_device_inline float hash_uint2_to_float(uint kx, uint ky)
{
  return (float)hash_uint2(kx, ky) / (float)0xFFFFFFFFu;
}

ccl_device_inline float hash_uint3_to_float(uint kx, uint ky, uint kz)
{
  return (float)hash_uint3(kx, ky, kz) / (float)0xFFFFFFFFu;
}

ccl_device_inline float hash_uint4_to_float(uint kx, uint ky, uint kz, uint kw)
{
  return (float)hash_uint4(kx, ky, kz, kw) / (float)0xFFFFFFFFu;
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
