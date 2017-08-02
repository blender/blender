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

#ifndef __UTIL_TYPES_H__
#define __UTIL_TYPES_H__

#ifndef __KERNEL_OPENCL__
#  include <stdlib.h>
#endif

/* Standard Integer Types */

#if !defined(__KERNEL_GPU__) && !defined(_WIN32)
#  include <stdint.h>
#endif

#include "util/util_defines.h"

#ifndef __KERNEL_GPU__
#  include "util/util_simd.h"
#endif

CCL_NAMESPACE_BEGIN

/* Types
 *
 * Define simpler unsigned type names, and integer with defined number of bits.
 * Also vector types, named to be compatible with OpenCL builtin types, while
 * working for CUDA and C++ too. */

/* Shorter Unsigned Names */

#ifndef __KERNEL_OPENCL__
typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned short ushort;
#endif

/* Fixed Bits Types */

#ifdef __KERNEL_OPENCL__
typedef ulong uint64_t;
#endif

#ifndef __KERNEL_GPU__
#  ifdef _WIN32
typedef signed char int8_t;
typedef unsigned char uint8_t;

typedef signed short int16_t;
typedef unsigned short uint16_t;

typedef signed int int32_t;
typedef unsigned int uint32_t;

typedef long long int64_t;
typedef unsigned long long uint64_t;
#    ifdef __KERNEL_64_BIT__
typedef int64_t ssize_t;
#    else
typedef int32_t ssize_t;
#    endif
#  endif  /* _WIN32 */

/* Generic Memory Pointer */

typedef uint64_t device_ptr;
#endif  /* __KERNEL_GPU__ */

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

/* Interpolation types for textures
 * cuda also use texture space to store other objects */
enum InterpolationType {
	INTERPOLATION_NONE = -1,
	INTERPOLATION_LINEAR = 0,
	INTERPOLATION_CLOSEST = 1,
	INTERPOLATION_CUBIC = 2,
	INTERPOLATION_SMART = 3,

	INTERPOLATION_NUM_TYPES,
};

/* Texture types
 * Since we store the type in the lower bits of a flat index,
 * the shift and bit mask constant below need to be kept in sync.
 */

enum ImageDataType {
	IMAGE_DATA_TYPE_FLOAT4 = 0,
	IMAGE_DATA_TYPE_BYTE4 = 1,
	IMAGE_DATA_TYPE_HALF4 = 2,
	IMAGE_DATA_TYPE_FLOAT = 3,
	IMAGE_DATA_TYPE_BYTE = 4,
	IMAGE_DATA_TYPE_HALF = 5,

	IMAGE_DATA_NUM_TYPES
};

#define IMAGE_DATA_TYPE_SHIFT 3
#define IMAGE_DATA_TYPE_MASK 0x7

/* Extension types for textures.
 *
 * Defines how the image is extrapolated past its original bounds.
 */
enum ExtensionType {
	/* Cause the image to repeat horizontally and vertically. */
	EXTENSION_REPEAT = 0,
	/* Extend by repeating edge pixels of the image. */
	EXTENSION_EXTEND = 1,
	/* Clip to image size and set exterior pixels as transparent. */
	EXTENSION_CLIP = 2,

	EXTENSION_NUM_TYPES,
};

CCL_NAMESPACE_END

/* Vectorized types declaration. */
#include "util/util_types_uchar2.h"
#include "util/util_types_uchar3.h"
#include "util/util_types_uchar4.h"

#include "util/util_types_int2.h"
#include "util/util_types_int3.h"
#include "util/util_types_int4.h"

#include "util/util_types_uint2.h"
#include "util/util_types_uint3.h"
#include "util/util_types_uint4.h"

#include "util/util_types_float2.h"
#include "util/util_types_float3.h"
#include "util/util_types_float4.h"

#include "util/util_types_vector3.h"

/* Vectorized types implementation. */
#include "util/util_types_uchar2_impl.h"
#include "util/util_types_uchar3_impl.h"
#include "util/util_types_uchar4_impl.h"

#include "util/util_types_int2_impl.h"
#include "util/util_types_int3_impl.h"
#include "util/util_types_int4_impl.h"

#include "util/util_types_uint2_impl.h"
#include "util/util_types_uint3_impl.h"
#include "util/util_types_uint4_impl.h"

#include "util/util_types_float2_impl.h"
#include "util/util_types_float3_impl.h"
#include "util/util_types_float4_impl.h"

#include "util/util_types_vector3_impl.h"

/* SSE types. */
#ifndef __KERNEL_GPU__
#  include "util/util_sseb.h"
#  include "util/util_ssei.h"
#  include "util/util_ssef.h"
#  include "util/util_avxf.h"
#endif

#endif /* __UTIL_TYPES_H__ */

