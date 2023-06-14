/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
struct ccl_try_align(16) int3
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

  __forceinline int3();
  __forceinline int3(const int3 &a);
  __forceinline explicit int3(const __m128i &a);

  __forceinline operator const __m128i &() const;
  __forceinline operator __m128i &();

  __forceinline int3 &operator=(const int3 &a);
#    else  /* __KERNEL_SSE__ */
  int x, y, z, w;
#    endif /* __KERNEL_SSE__ */
#  endif

#  ifndef __KERNEL_GPU__
  __forceinline int operator[](int i) const;
  __forceinline int &operator[](int i);
#  endif
};

ccl_device_inline int3 make_int3(int x, int y, int z);
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

ccl_device_inline int3 make_int3(int i);
ccl_device_inline void print_int3(ccl_private const char *label, const int3 a);

#if defined(__KERNEL_METAL__)
/* Metal has native packed_int3. */
#elif defined(__KERNEL_CUDA__)
/* CUDA int3 is already packed. */
typedef int3 packed_int3;
#else
/* HIP int3 is not packed (https://github.com/ROCm-Developer-Tools/HIP/issues/706). */
struct packed_int3 {
  int x, y, z;

  ccl_device_inline_method packed_int3(){};

  ccl_device_inline_method packed_int3(const int px, const int py, const int pz)
      : x(px), y(py), z(pz){};

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
  __forceinline int operator[](int i) const;
  __forceinline int &operator[](int i);
#  endif
};

static_assert(sizeof(packed_int3) == 12, "packed_int3 expected to be exactly 12 bytes");
#endif

CCL_NAMESPACE_END
