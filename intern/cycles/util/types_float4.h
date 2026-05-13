/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_base.h"
#include "util/types_float3.h"
#include "util/types_int4.h"

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
struct int4;

struct ccl_try_align(16) float4 {
#  ifdef __KERNEL_SSE__
  union {
    __m128 m128;
    struct {
      float x, y, z, w;
    };
  };

  __forceinline float4() = default;
  __forceinline float4(const float4 &a) = default;
  __forceinline explicit float4(const __m128 &a) : m128(a) {}

  __forceinline operator const __m128 &() const
  {
    return m128;
  }
  __forceinline operator __m128 &()
  {
    return m128;
  }

  __forceinline float4 &operator=(const float4 &a)
  {
    m128 = a.m128;
    return *this;
  }

#  else  /* __KERNEL_SSE__ */
  float x, y, z, w;
#  endif /* __KERNEL_SSE__ */

#  ifndef __KERNEL_GPU__
  __forceinline float operator[](int i) const
  {
    util_assert(i >= 0);
    util_assert(i < 4);
    return *(&x + i);
  }
  __forceinline float &operator[](int i)
  {
    util_assert(i >= 0);
    util_assert(i < 4);
    return *(&x + i);
  }
#  endif
};

ccl_device_inline float4 make_float4(const float x, const float y, float z, const float w)
{
#  ifdef __KERNEL_SSE__
  return float4(_mm_set_ps(w, z, y, x));
#  else
  return {x, y, z, w};
#  endif
}

#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

ccl_device_inline float4 make_float4(const float f)
{
#ifdef __KERNEL_SSE__
  return float4(_mm_set1_ps(f));
#else
  return make_float4(f, f, f, f);
#endif
}

ccl_device_inline float4 make_float4(const float3 a, const float b)
{
  return make_float4(a.x, a.y, a.z, b);
}

ccl_device_inline float4 make_float4(const float3 a)
{
  return make_float4(a.x, a.y, a.z, 1.0f);
}

ccl_device_inline float4 make_homogeneous(const float3 a)
{
  return make_float4(a.x, a.y, a.z, 1.0f);
}

ccl_device_inline float4 make_float4(const int4 i)
{
#ifdef __KERNEL_SSE__
  return float4(_mm_cvtepi32_ps(i.m128));
#else
  return make_float4((float)i.x, (float)i.y, (float)i.z, (float)i.w);
#endif
}

ccl_device_inline float3 make_float3(const float4 a)
{
  return make_float3(a.x, a.y, a.z);
}

ccl_device_inline int4 make_int4(const float4 f)
{
#ifdef __KERNEL_SSE__
  return int4(_mm_cvtps_epi32(f.m128));
#else
  return make_int4((int)f.x, (int)f.y, (int)f.z, (int)f.w);
#endif
}

#if defined __METAL_PRINTF__
#  define print_float4(label, a) \
    metal::os_log_default.log_debug(label ": %.8f %.8f %.8f %.8f", (a).x, (a).y, (a).z, (a).w)
#else
ccl_device_inline void print_float4(const ccl_private char *label, const float4 a)
{
#  ifdef __KERNEL_PRINTF__
  printf("%s: %.8f %.8f %.8f %.8f\n", label, (double)a.x, (double)a.y, (double)a.z, (double)a.w);
#  endif
}
#endif

/* Packed float4.
 *
 * float4 type with no alignment requirements.
 * It does not support any mathematical operations, only conversion to float4. */

#if defined(__KERNEL_METAL__)
/* Metal has native packed_float4. */
#else
struct packed_float4 {
  packed_float4() = default;

  ccl_device_inline_method packed_float4(const float4 a) : x(a.x), y(a.y), z(a.z), w(a.w) {}

  ccl_device_inline_method operator float4() const
  {
    return make_float4(x, y, z, w);
  }

  ccl_device_inline_method packed_float4 &operator=(const float4 &a)
  {
    x = a.x;
    y = a.y;
    z = a.z;
    w = a.w;
    return *this;
  }

  float x, y, z, w;
};
#endif

static_assert(alignof(packed_float4) == alignof(float),
              "packed_float4 expected to have the same alignment as float");
static_assert(sizeof(packed_float4) == 16, "packed_float4 expected to be exactly 16 bytes");

CCL_NAMESPACE_END
