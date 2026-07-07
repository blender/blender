/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

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
  }

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
  }

uint hash_uint(uint kx)
{
  uint a, b, c;
  a = b = c = 0xdeadbeefu + (1u << 2u) + 13u;

  a += kx;
  final(a, b, c);

  return c;
}

uint hash_uint2(uint kx, uint ky)
{
  uint a, b, c;
  a = b = c = 0xdeadbeefu + (2u << 2u) + 13u;

  b += ky;
  a += kx;
  final(a, b, c);

  return c;
}

uint hash_uint3(uint kx, uint ky, uint kz)
{
  uint a, b, c;
  a = b = c = 0xdeadbeefu + (3u << 2u) + 13u;

  c += kz;
  b += ky;
  a += kx;
  final(a, b, c);

  return c;
}

uint hash_uint4(uint kx, uint ky, uint kz, uint kw)
{
  uint a, b, c;
  a = b = c = 0xdeadbeefu + (4u << 2u) + 13u;

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

uint hash_int(int kx)
{
  return hash_uint(uint(kx));
}

uint hash_int2(int kx, int ky)
{
  return hash_uint2(uint(kx), uint(ky));
}

uint hash_int3(int kx, int ky, int kz)
{
  return hash_uint3(uint(kx), uint(ky), uint(kz));
}

uint hash_int4(int kx, int ky, int kz, int kw)
{
  return hash_uint4(uint(kx), uint(ky), uint(kz), uint(kw));
}

/* PCG 2D, 3D and 4D hash functions,
 * from "Hash Functions for GPU Rendering" JCGT 2020
 * https://jcgt.org/published/0009/03/02/
 *
 * Slightly modified to only use signed integers,
 * so that they can also be implemented in OSL. */

int2 hash_pcg2d_i(int2 v)
{
  v = v * 1664525 + 1013904223;
  v.x += v.y * 1664525;
  v.y += v.x * 1664525;
  v = v ^ (v >> 16);
  v.x += v.y * 1664525;
  v.y += v.x * 1664525;
  return v;
}

int3 hash_pcg3d_i(int3 v)
{
  v = v * 1664525 + 1013904223;
  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v = v ^ (v >> 16);
  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  return v;
}

int4 hash_pcg4d_i(int4 v)
{
  v = v * 1664525 + 1013904223;
  v.x += v.y * v.w;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v.w += v.y * v.z;
  v = v ^ (v >> 16);
  v.x += v.y * v.w;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v.w += v.y * v.z;
  return v;
}

/* Hashing uint or uint[234] into a float in the range [0, 1]. */

float hash_uint_to_float(uint kx)
{
  return float(hash_uint(kx)) / float(0xFFFFFFFFu);
}

float hash_uint2_to_float(uint kx, uint ky)
{
  return float(hash_uint2(kx, ky)) / float(0xFFFFFFFFu);
}

float hash_uint3_to_float(uint kx, uint ky, uint kz)
{
  return float(hash_uint3(kx, ky, kz)) / float(0xFFFFFFFFu);
}

float hash_uint4_to_float(uint kx, uint ky, uint kz, uint kw)
{
  return float(hash_uint4(kx, ky, kz, kw)) / float(0xFFFFFFFFu);
}

/* Hashing float or vec[234] into a float in the range [0, 1]. */

float hash_float_to_float(float k)
{
  return hash_uint_to_float(floatBitsToUint(k));
}

float hash_vec2_to_float(float2 k)
{
  return hash_uint2_to_float(floatBitsToUint(k.x), floatBitsToUint(k.y));
}

float hash_vec3_to_float(float3 k)
{
  return hash_uint3_to_float(floatBitsToUint(k.x), floatBitsToUint(k.y), floatBitsToUint(k.z));
}

float hash_vec4_to_float(float4 k)
{
  return hash_uint4_to_float(
      floatBitsToUint(k.x), floatBitsToUint(k.y), floatBitsToUint(k.z), floatBitsToUint(k.w));
}

/* Hashing vec[234] into vec[234] of components in the range [0, 1]. */

float2 hash_vec2_to_vec2(float2 k)
{
  return float2(hash_vec2_to_float(k), hash_vec3_to_float(float3(k, 1.0f)));
}

float3 hash_vec3_to_vec3(float3 k)
{
  return float3(hash_vec3_to_float(k),
                hash_vec4_to_float(float4(k, 1.0f)),
                hash_vec4_to_float(float4(k, 2.0f)));
}

float4 hash_vec4_to_vec4(float4 k)
{
  return float4(hash_vec4_to_float(k.xyzw),
                hash_vec4_to_float(k.wxyz),
                hash_vec4_to_float(k.zwxy),
                hash_vec4_to_float(k.yzwx));
}

/* Hashing a number of integers into floats in [0..1] range. */

float2 hash_int2_to_vec2(int2 k)
{
  int2 h = hash_pcg2d_i(k);
  return float2(h & 0x7fffffff) * (1.0 / float(0x7fffffff));
}

float3 hash_int3_to_vec3(int3 k)
{
  int3 h = hash_pcg3d_i(k);
  return float3(h & 0x7fffffff) * (1.0 / float(0x7fffffff));
}

float4 hash_int4_to_vec4(int4 k)
{
  int4 h = hash_pcg4d_i(k);
  return float4(h & 0x7fffffff) * (1.0 / float(0x7fffffff));
}

float3 hash_int2_to_vec3(int2 k)
{
  return hash_int3_to_vec3(int3(k.x, k.y, 0));
}

float3 hash_int4_to_vec3(int4 k)
{
  return hash_int4_to_vec4(k).xyz;
}

/* Hashing float or vec[234] into vec3 of components in range [0, 1]. */

float3 hash_float_to_vec3(float k)
{
  return float3(hash_float_to_float(k),
                hash_vec2_to_float(float2(k, 1.0f)),
                hash_vec2_to_float(float2(k, 2.0f)));
}

float3 hash_vec2_to_vec3(float2 k)
{
  return float3(hash_vec2_to_float(k),
                hash_vec3_to_float(float3(k, 1.0f)),
                hash_vec3_to_float(float3(k, 2.0f)));
}

float3 hash_vec4_to_vec3(float4 k)
{
  return float3(
      hash_vec4_to_float(k.xyzw), hash_vec4_to_float(k.zxwy), hash_vec4_to_float(k.wzyx));
}

/* Hashing float or vec[234] into vec2 of components in range [0, 1]. */

float2 hash_float_to_vec2(float k)
{
  return float2(hash_float_to_float(k), hash_vec2_to_float(float2(k, 1.0f)));
}

float2 hash_vec3_to_vec2(float3 k)
{
  return float2(hash_vec3_to_float(k.xyz), hash_vec3_to_float(k.zxy));
}

float2 hash_vec4_to_vec2(float4 k)
{
  return float2(hash_vec4_to_float(k.xyzw), hash_vec4_to_float(k.zxwy));
}

/* Other Hash Functions */

float integer_noise(int n)
{
  /* Integer bit-shifts for these calculations can cause precision problems on macOS.
   * Using uint resolves these issues. */
  uint nn;
  nn = (uint(n) + 1013u) & 0x7fffffffu;
  nn = (nn >> 13u) ^ nn;
  nn = (uint(nn * (nn * nn * 60493u + 19990303u)) + 1376312589u) & 0x7fffffffu;
  return 0.5f * (float(nn) / 1073741824.0f);
}

float wang_hash_noise(uint s)
{
  s = (s ^ 61u) ^ (s >> 16u);
  s *= 9u;
  s = s ^ (s >> 4u);
  s *= 0x27d4eb2du;
  s = s ^ (s >> 15u);

  return fract(float(s) / 4294967296.0f);
}
