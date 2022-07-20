/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#define __KERNEL_CPU__

/* Release kernel has too much false-positive maybe-uninitialized warnings,
 * which makes it possible to miss actual warnings.
 */
#if (defined(__GNUC__) && !defined(__clang__)) && defined(NDEBUG)
#  pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#  pragma GCC diagnostic ignored "-Wuninitialized"
#endif

#include "util/half.h"
#include "util/math.h"
#include "util/simd.h"
#include "util/texture.h"
#include "util/types.h"

/* On x86_64, versions of glibc < 2.16 have an issue where expf is
 * much slower than the double version.  This was fixed in glibc 2.16.
 */
#if !defined(__KERNEL_GPU__) && defined(__x86_64__) && defined(__x86_64__) && \
    defined(__GNU_LIBRARY__) && defined(__GLIBC__) && defined(__GLIBC_MINOR__) && \
    (__GLIBC__ <= 2 && __GLIBC_MINOR__ < 16)
#  define expf(x) ((float)exp((double)(x)))
#endif

CCL_NAMESPACE_BEGIN

/* Assertions inside the kernel only work for the CPU device, so we wrap it in
 * a macro which is empty for other devices */

#define kernel_assert(cond) assert(cond)

/* Macros to handle different memory storage on different devices */

#ifdef __KERNEL_SSE2__
typedef vector3<sseb> sse3b;
typedef vector3<ssef> sse3f;
typedef vector3<ssei> sse3i;

ccl_device_inline void print_sse3b(const char *label, sse3b &a)
{
  print_sseb(label, a.x);
  print_sseb(label, a.y);
  print_sseb(label, a.z);
}

ccl_device_inline void print_sse3f(const char *label, sse3f &a)
{
  print_ssef(label, a.x);
  print_ssef(label, a.y);
  print_ssef(label, a.z);
}

ccl_device_inline void print_sse3i(const char *label, sse3i &a)
{
  print_ssei(label, a.x);
  print_ssei(label, a.y);
  print_ssei(label, a.z);
}

#  if defined(__KERNEL_AVX__) || defined(__KERNEL_AVX2__)
typedef vector3<avxf> avx3f;
#  endif

#endif

CCL_NAMESPACE_END
