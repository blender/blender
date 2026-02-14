/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if !defined(__KERNEL_METAL__)
#  include <cstdlib>
#endif

/* Standard Integer Types */

#if !defined(__KERNEL_GPU__)
#  include <cstdint>  // IWYU pragma: export
#  include <cstdio>
#endif

#include "util/defines.h"

#ifndef __KERNEL_GPU__
#  include "util/optimization.h"  // IWYU pragma: export
#  include "util/simd.h"          // IWYU pragma: export
#endif

CCL_NAMESPACE_BEGIN

/* Types
 *
 * Define simpler unsigned type names, and integer with defined number of bits.
 * Also vector types, named to be compatible with OpenCL builtin types, while
 * working for CUDA and C++ too. */

/* Shorter Unsigned Names */

using uchar = unsigned char;
using uint = unsigned int;
using ushort = unsigned short;

/* Fixed Bits Types */

#ifndef __KERNEL_GPU__
/* Generic Memory Pointer */

using device_ptr = uint64_t;
#endif /* __KERNEL_GPU__ */

ccl_device_inline size_t align_up(const size_t offset, const size_t alignment)
{
  return (offset + alignment - 1) & ~(alignment - 1);
}

ccl_device_inline size_t divide_up(const size_t x, const size_t y)
{
  return (x + y - 1) / y;
}

ccl_device_inline size_t divide_up_by_shift(const size_t x, const size_t y)
{
  return (x + ((1 << y) - 1)) >> y;
}

ccl_device_inline size_t round_up(const size_t x, const size_t multiple)
{
  return ((x + multiple - 1) / multiple) * multiple;
}

ccl_device_inline size_t round_down(const size_t x, const size_t multiple)
{
  return (x / multiple) * multiple;
}

ccl_device_inline bool is_power_of_two(const size_t x)
{
  return (x & (x - 1)) == 0;
}

CCL_NAMESPACE_END

/* Device side printf only tested on CUDA, may work on more GPU devices. */
#if !defined(__KERNEL_GPU__) || defined(__KERNEL_CUDA__)
#  define __KERNEL_PRINTF__
#endif

#if defined __METAL_PRINTF__
#  define print_float(label, a) metal::os_log_default.log_debug(label ": %.8f", a)
#else
ccl_device_inline void print_float(const ccl_private char *label, const float a)
{
#  ifdef __KERNEL_PRINTF__
  printf("%s: %.8f\n", label, (double)a);
#  endif
}
#endif

/* Most GPU APIs matching native vector types, so we only need to implement them for
 * CPU and oneAPI. */
#if defined(__KERNEL_GPU__) && !defined(__KERNEL_ONEAPI__)
#  define __KERNEL_NATIVE_VECTOR_TYPES__
#endif
