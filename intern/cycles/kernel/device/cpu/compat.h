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

/* Texture types to be compatible with CUDA textures. These are really just
 * simple arrays and after inlining fetch hopefully revert to being a simple
 * pointer lookup. */
template<typename T> struct texture {
  ccl_always_inline const T &fetch(int index) const
  {
    kernel_assert(index >= 0 && index < width);
    return data[index];
  }

  T *data;
  int width;
};

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
