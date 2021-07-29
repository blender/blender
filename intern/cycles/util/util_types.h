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

/* Bitness */

#if defined(__ppc64__) || defined(__PPC64__) || defined(__x86_64__) || defined(__ia64__) || defined(_M_X64)
#  define __KERNEL_64_BIT__
#endif

/* Qualifiers for kernel code shared by CPU and GPU */

#ifndef __KERNEL_GPU__
#  define ccl_device static inline
#  define ccl_device_noinline static
#  define ccl_global
#  define ccl_constant
#  define ccl_local
#  define ccl_local_param
#  define ccl_private
#  define ccl_restrict __restrict
#  define __KERNEL_WITH_SSE_ALIGN__

#  if defined(_WIN32) && !defined(FREE_WINDOWS)
#    define ccl_device_inline static __forceinline
#    define ccl_device_forceinline static __forceinline
#    define ccl_align(...) __declspec(align(__VA_ARGS__))
#    ifdef __KERNEL_64_BIT__
#      define ccl_try_align(...) __declspec(align(__VA_ARGS__))
#    else  /* __KERNEL_64_BIT__ */
#      undef __KERNEL_WITH_SSE_ALIGN__
/* No support for function arguments (error C2719). */
#      define ccl_try_align(...)
#    endif  /* __KERNEL_64_BIT__ */
#    define ccl_may_alias
#    define ccl_always_inline __forceinline
#    define ccl_never_inline __declspec(noinline)
#    define ccl_maybe_unused
#  else  /* _WIN32 && !FREE_WINDOWS */
#    define ccl_device_inline static inline __attribute__((always_inline))
#    define ccl_device_forceinline static inline __attribute__((always_inline))
#    define ccl_align(...) __attribute__((aligned(__VA_ARGS__)))
#    ifndef FREE_WINDOWS64
#      define __forceinline inline __attribute__((always_inline))
#    endif
#    define ccl_try_align(...) __attribute__((aligned(__VA_ARGS__)))
#    define ccl_may_alias __attribute__((__may_alias__))
#    define ccl_always_inline __attribute__((always_inline))
#    define ccl_never_inline __attribute__((noinline))
#    define ccl_maybe_unused __attribute__((used))
#  endif  /* _WIN32 && !FREE_WINDOWS */

/* Use to suppress '-Wimplicit-fallthrough' (in place of 'break'). */
#  if defined(__GNUC__) && (__GNUC__ >= 7)  /* gcc7.0+ only */
#    define ATTR_FALLTHROUGH __attribute__((fallthrough))
#  else
#    define ATTR_FALLTHROUGH ((void)0)
#  endif
#endif  /* __KERNEL_GPU__ */

/* Standard Integer Types */

#ifndef __KERNEL_GPU__
/* int8_t, uint16_t, and friends */
#  ifndef _WIN32
#    include <stdint.h>
#  endif
/* SIMD Types */
#  include "util/util_optimization.h"
#endif  /* __KERNEL_GPU__ */

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

/* macros */

/* hints for branch prediction, only use in code that runs a _lot_ */
#if defined(__GNUC__) && defined(__KERNEL_CPU__)
#  define LIKELY(x)       __builtin_expect(!!(x), 1)
#  define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#  define LIKELY(x)       (x)
#  define UNLIKELY(x)     (x)
#endif

#if defined(__cplusplus) && ((__cplusplus >= 201103L) || (defined(_MSC_VER) && _MSC_VER >= 1800))
#  define HAS_CPP11_FEATURES
#endif

#if defined(__GNUC__) || defined(__clang__)
#  if defined(HAS_CPP11_FEATURES)
/* Some magic to be sure we don't have reference in the type. */
template<typename T> static inline T decltype_helper(T x) { return x; }
#    define TYPEOF(x) decltype(decltype_helper(x))
#  else
#    define TYPEOF(x) typeof(x)
#  endif
#endif

/* Causes warning:
 * incompatible types when assigning to type 'Foo' from type 'Bar'
 * ... the compiler optimizes away the temp var */
#ifdef __GNUC__
#define CHECK_TYPE(var, type)  {  \
	TYPEOF(var) *__tmp;           \
	__tmp = (type *)NULL;         \
	(void)__tmp;                  \
} (void)0

#define CHECK_TYPE_PAIR(var_a, var_b)  {  \
	TYPEOF(var_a) *__tmp;                 \
	__tmp = (typeof(var_b) *)NULL;        \
	(void)__tmp;                          \
} (void)0
#else
#  define CHECK_TYPE(var, type)
#  define CHECK_TYPE_PAIR(var_a, var_b)
#endif

/* can be used in simple macros */
#define CHECK_TYPE_INLINE(val, type) \
	((void)(((type)0) != (val)))


CCL_NAMESPACE_END

#ifndef __KERNEL_GPU__
#  include <cassert>
#  define util_assert(statement)  assert(statement)
#else
#  define util_assert(statement)
#endif

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

#endif /* __UTIL_TYPES_H__ */

