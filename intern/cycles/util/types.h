/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_TYPES_H__
#define __UTIL_TYPES_H__

#if !defined(__KERNEL_METAL__)
#  include <stdlib.h>
#endif

/* Standard Integer Types */

#if !defined(__KERNEL_GPU__)
#  include <stdint.h>
#  include <stdio.h>
#endif

#include "util/defines.h"

#ifndef __KERNEL_GPU__
#  include "util/optimization.h"
#  include "util/simd.h"
#endif

CCL_NAMESPACE_BEGIN

/* Types
 *
 * Define simpler unsigned type names, and integer with defined number of bits.
 * Also vector types, named to be compatible with OpenCL builtin types, while
 * working for CUDA and C++ too. */

/* Shorter Unsigned Names */

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned short ushort;

/* Fixed Bits Types */

#ifndef __KERNEL_GPU__
/* Generic Memory Pointer */

typedef uint64_t device_ptr;
#endif /* __KERNEL_GPU__ */

ccl_device_inline size_t align_up(size_t offset, size_t alignment)
{
  return (offset + alignment - 1) & ~(alignment - 1);
}

ccl_device_inline size_t divide_up(size_t x, size_t y)
{
  return (x + y - 1) / y;
}

ccl_device_inline size_t round_up(size_t x, size_t multiple)
{
  return ((x + multiple - 1) / multiple) * multiple;
}

ccl_device_inline size_t round_down(size_t x, size_t multiple)
{
  return (x / multiple) * multiple;
}

ccl_device_inline bool is_power_of_two(size_t x)
{
  return (x & (x - 1)) == 0;
}

CCL_NAMESPACE_END

/* Device side printf only tested on CUDA, may work on more GPU devices. */
#if !defined(__KERNEL_GPU__) || defined(__KERNEL_CUDA__)
#  define __KERNEL_PRINTF__
#endif

ccl_device_inline void print_float(ccl_private const char *label, const float a)
{
#ifdef __KERNEL_PRINTF__
  printf("%s: %.8f\n", label, (double)a);
#endif
}

/* Most GPU APIs matching native vector types, so we only need to implement them for
 * CPU and oneAPI. */
#if defined(__KERNEL_GPU__) && !defined(__KERNEL_ONEAPI__)
#  define __KERNEL_NATIVE_VECTOR_TYPES__
#endif

/* Vectorized types declaration. */
#include "util/types_uchar2.h"
#include "util/types_uchar3.h"
#include "util/types_uchar4.h"

#include "util/types_int2.h"
#include "util/types_int3.h"
#include "util/types_int4.h"
#include "util/types_int8.h"

#include "util/types_uint2.h"
#include "util/types_uint3.h"
#include "util/types_uint4.h"

#include "util/types_ushort4.h"

#include "util/types_float2.h"
#include "util/types_float3.h"
#include "util/types_float4.h"
#include "util/types_float8.h"

#include "util/types_spectrum.h"

/* Vectorized types implementation. */
#include "util/types_uchar2_impl.h"
#include "util/types_uchar3_impl.h"
#include "util/types_uchar4_impl.h"

#include "util/types_int2_impl.h"
#include "util/types_int3_impl.h"
#include "util/types_int4_impl.h"
#include "util/types_int8_impl.h"

#include "util/types_uint2_impl.h"
#include "util/types_uint3_impl.h"
#include "util/types_uint4_impl.h"

#include "util/types_float2_impl.h"
#include "util/types_float3_impl.h"
#include "util/types_float4_impl.h"
#include "util/types_float8_impl.h"

#endif /* __UTIL_TYPES_H__ */
