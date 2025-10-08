/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_base.h"

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
#  ifdef __KERNEL_ONEAPI__
/* Keep structure packed for oneAPI. */
struct int3
#  else
struct ccl_try_align(16) int3
#  endif
{
#  ifdef __KERNEL_GPU__
  /* Compact structure on the GPU. */
  int x, y, z;
#  else
  /* SIMD aligned structure for CPU. */
#    ifdef __KERNEL_SSE__
  union {
    __m128i m128;
    struct {
      int x, y, z, w;
    };
  };

  __forceinline int3() = default;
  __forceinline int3(const int3 &a) = default;
  __forceinline explicit int3(const __m128i &a) : m128(a) {}

  __forceinline operator const __m128i &() const
  {
    return m128;
  }

  __forceinline operator __m128i &()
  {
    return m128;
  }

  __forceinline int3 &operator=(const int3 &a)
  {
    m128 = a.m128;
    return *this;
  }
#    else  /* __KERNEL_SSE__ */
  int x, y, z, w;
#    endif /* __KERNEL_SSE__ */
#  endif

#  ifndef __KERNEL_GPU__
  __forceinline int operator[](int i) const
  {
    util_assert(i >= 0);
    util_assert(i < 3);
    return *(&x + i);
  }

  __forceinline int &operator[](int i)
  {
    util_assert(i >= 0);
    util_assert(i < 3);
    return *(&x + i);
  }
#  endif
};

ccl_device_inline int3 make_int3(const int x, const int y, int z)
{
#  if defined(__KERNEL_GPU__)
  return {x, y, z};
#  elif defined(__KERNEL_SSE__)
  return int3(_mm_set_epi32(0, z, y, x));
#  else
  return {x, y, z, 0};
#  endif
}

#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

ccl_device_inline int3 make_int3(const int i)
{
#if defined(__KERNEL_GPU__)
  return make_int3(i, i, i);
#elif defined(__KERNEL_SSE__)
  return int3(_mm_set1_epi32(i));
#else
  return {i, i, i, i};
#endif
}

ccl_device_inline void print_int3(const ccl_private char *label, const int3 a)
{
#ifdef __KERNEL_PRINTF__
  printf("%s: %d %d %d\n", label, a.x, a.y, a.z);
#endif
}

#if defined(__KERNEL_METAL__)
/* Metal has native packed_int3. */
#elif defined(__KERNEL_CUDA__) || defined(__KERNEL_ONEAPI__)
/* CUDA/oneAPI int3 is already packed. */
typedef int3 packed_int3;
#else
/* HIP int3 is not packed (https://github.com/ROCm-Developer-Tools/HIP/issues/706). */
struct packed_int3 {
  int x, y, z;

  ccl_device_inline_method packed_int3() = default;

  ccl_device_inline_method packed_int3(const int px, const int py, const int pz)
      : x(px), y(py), z(pz) {};

  ccl_device_inline_method packed_int3(const int3 &a) : x(a.x), y(a.y), z(a.z) {}

  ccl_device_inline_method operator int3() const
  {
    return make_int3(x, y, z);
  }

  ccl_device_inline_method packed_int3 &operator=(const int3 &a)
  {
    x = a.x;
    y = a.y;
    z = a.z;
    return *this;
  }

#  ifndef __KERNEL_GPU__
  __forceinline int operator[](int i) const
  {
    util_assert(i < 3);
    return *(&x + i);
  }

  __forceinline int &operator[](int i)
  {
    util_assert(i < 3);
    return *(&x + i);
  }
#  endif
};
#endif

static_assert(sizeof(packed_int3) == 12, "packed_int3 expected to be exactly 12 bytes");

ccl_device_inline packed_int3 make_packed_int3(const int x, const int y, int z)
{
  packed_int3 a = {x, y, z};
  return a;
}

CCL_NAMESPACE_END
