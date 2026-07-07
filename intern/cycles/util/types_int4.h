/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_base.h"

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__

struct ccl_try_align(16) int4 {
#  ifdef __KERNEL_SSE__
  union {
    __m128i m128;
    struct {
      int x, y, z, w;
    };
  };

  __forceinline int4() = default;
  __forceinline int4(const int4 &a) = default;
  __forceinline explicit int4(const __m128i &a) : m128(a) {}

  __forceinline operator const __m128i &() const
  {
    return m128;
  }
  __forceinline operator __m128i &()
  {
    return m128;
  }

  __forceinline int4 &operator=(const int4 &a)
  {
    m128 = a.m128;
    return *this;
  }
#  else  /* __KERNEL_SSE__ */
  int x, y, z, w;
#  endif /* __KERNEL_SSE__ */

#  ifndef __KERNEL_GPU__
  __forceinline int operator[](int i) const
  {
    util_assert(i >= 0);
    util_assert(i < 4);
    return *(&x + i);
  }
  __forceinline int &operator[](int i)
  {
    util_assert(i >= 0);
    util_assert(i < 4);
    return *(&x + i);
  }
#  endif
};

ccl_device_inline int4 make_int4(const int x, const int y, int z, const int w)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_set_epi32(w, z, y, x));
#  else
  return {x, y, z, w};
#  endif
}
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

ccl_device_inline int4 make_int4(const int i)
{
#ifdef __KERNEL_SSE__
  return int4(_mm_set1_epi32(i));
#else
  return make_int4(i, i, i, i);
#endif
}

ccl_device_inline int4 zero_int4()
{
  return make_int4(0);
}

ccl_device_inline void print_int4(const ccl_private char *label, const int4 a)
{
#ifdef __KERNEL_PRINTF__
  printf("%s: %d %d %d %d\n", label, a.x, a.y, a.z, a.w);
#endif
}

CCL_NAMESPACE_END
