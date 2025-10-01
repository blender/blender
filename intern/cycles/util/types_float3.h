/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_base.h"
#include "util/types_float2.h"
#include "util/types_int3.h"
#include "util/types_int4.h"

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
#  ifdef __KERNEL_ONEAPI__
/* Define float3 as packed for oneAPI. */
struct float3
#  else
struct ccl_try_align(16) float3
#  endif
{
#  ifdef __KERNEL_GPU__
  /* Compact structure for GPU. */
  float x, y, z;
#  else
  /* SIMD aligned structure for CPU. */
#    ifdef __KERNEL_SSE__
  union {
    __m128 m128;
    struct {
      float x, y, z, w;
    };
  };
#    else
  float x, y, z, w;
#    endif
#  endif

#  ifdef __KERNEL_SSE__
  /* Convenient constructors and operators for SIMD, otherwise default is enough. */
  __forceinline float3() = default;
  __forceinline float3(const float3 &a) = default;
  __forceinline explicit float3(const __m128 &a) : m128(a) {}

  __forceinline operator const __m128 &() const
  {
    return m128;
  }
  __forceinline operator __m128 &()
  {
    return m128;
  }

  __forceinline float3 &operator=(const float3 &a)
  {
    m128 = a.m128;
    return *this;
  }
#  endif

#  ifndef __KERNEL_GPU__
  __forceinline float operator[](int i) const
  {
    util_assert(i >= 0);
    util_assert(i < 3);
    return *(&x + i);
  }
  __forceinline float &operator[](int i)
  {
    util_assert(i >= 0);
    util_assert(i < 3);
    return *(&x + i);
  }
#  endif
};

ccl_device_inline float3 make_float3(const float x, const float y, float z)
{
#  if defined(__KERNEL_GPU__)
  return {x, y, z};
#  elif defined(__KERNEL_SSE__)
  return float3(_mm_set_ps(0.0f, z, y, x));
#  else
  return {x, y, z, 0.0f};
#  endif
}

#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

ccl_device_inline float3 make_float3(const float f)
{
#if defined(__KERNEL_GPU__)
  return make_float3(f, f, f);
#elif defined(__KERNEL_SSE__)
  return float3(_mm_set1_ps(f));
#else
  return {f, f, f, f};
#endif
}

ccl_device_inline float3 make_float3(const float2 a)
{
  return make_float3(a.x, a.y, 0.0f);
}

ccl_device_inline float3 make_float3(const float2 a, const float b)
{
  return make_float3(a.x, a.y, b);
}

ccl_device_inline float3 make_float3(const int3 i)
{
#ifdef __KERNEL_SSE__
  return float3(_mm_cvtepi32_ps(i.m128));
#else
  return make_float3((float)i.x, (float)i.y, (float)i.z);
#endif
}

ccl_device_inline float3 make_float3(const float3 a)
{
  return a;
}

#if defined __METAL_PRINTF__
#  define print_float3(label, a) \
    metal::os_log_default.log_debug(label ":  %.8f %.8f %.8f", a.x, a.y, a.z)
#else
ccl_device_inline void print_float3(const ccl_private char *label, const float3 a)
{
#  ifdef __KERNEL_PRINTF__
  printf("%s: %.8f %.8f %.8f\n", label, (double)a.x, (double)a.y, (double)a.z);
#  else
  (void)label;
  (void)a;
#  endif
}
#endif

ccl_device_inline float2 make_float2(const float3 a)
{
  return make_float2(a.x, a.y);
}

ccl_device_inline int4 make_int4(const float3 f)
{
#if defined(__KERNEL_GPU__)
  return make_int4((int)f.x, (int)f.y, (int)f.z, 0);
#elif defined(__KERNEL_SSE__)
  return int4(_mm_cvtps_epi32(f.m128));
#else
  return make_int4((int)f.x, (int)f.y, (int)f.z, (int)f.w);
#endif
}

ccl_device_inline int3 make_int3(const float3 f)
{
#ifdef __KERNEL_SSE__
  return int3(_mm_cvtps_epi32(f.m128));
#else
  return make_int3((int)f.x, (int)f.y, (int)f.z);
#endif
}

/* Packed float3
 *
 * Smaller float3 for storage. For math operations this must be converted to float3, so that on the
 * CPU SIMD instructions can be used. */

#if defined(__KERNEL_METAL__)
/* Metal has native packed_float3. */
#elif defined(__KERNEL_CUDA__) || defined(__KERNEL_HIP__) || defined(__KERNEL_ONEAPI__)
/* CUDA, HIP and oneAPI float3 are already packed. */
using packed_float3 = float3;
#else
struct packed_float3 {
  ccl_device_inline_method packed_float3() = default;

  ccl_device_inline_method packed_float3(const float3 &a) : x(a.x), y(a.y), z(a.z) {}

  ccl_device_inline_method operator float3() const
  {
    return make_float3(x, y, z);
  }

  ccl_device_inline_method packed_float3 &operator=(const float3 &a)
  {
    x = a.x;
    y = a.y;
    z = a.z;
    return *this;
  }

  float x, y, z;
};
#endif

static_assert(sizeof(packed_float3) == 12, "packed_float3 expected to be exactly 12 bytes");

CCL_NAMESPACE_END
