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

#ifndef __KERNEL_COMPAT_OPENCL_H__
#define __KERNEL_COMPAT_OPENCL_H__

#define __KERNEL_GPU__
#define __KERNEL_OPENCL__

/* no namespaces in opencl */
#define CCL_NAMESPACE_BEGIN
#define CCL_NAMESPACE_END

#ifdef __CL_NOINLINE__
#  define ccl_noinline __attribute__((noinline))
#else
#  define ccl_noinline
#endif

/* in opencl all functions are device functions, so leave this empty */
#define ccl_device
#define ccl_device_inline ccl_device
#define ccl_device_forceinline ccl_device
#define ccl_device_noinline ccl_device ccl_noinline
#define ccl_may_alias
#define ccl_static_constant static __constant
#define ccl_constant __constant
#define ccl_global __global
#define ccl_local __local
#define ccl_local_param __local
#define ccl_private __private
#define ccl_restrict restrict
#define ccl_ref
#define ccl_align(n) __attribute__((aligned(n)))

#ifdef __SPLIT_KERNEL__
#  define ccl_addr_space __global
#else
#  define ccl_addr_space
#endif

#define ATTR_FALLTHROUGH

#define ccl_local_id(d) get_local_id(d)
#define ccl_global_id(d) get_global_id(d)

#define ccl_local_size(d) get_local_size(d)
#define ccl_global_size(d) get_global_size(d)

#define ccl_group_id(d) get_group_id(d)
#define ccl_num_groups(d) get_num_groups(d)

/* Selective nodes compilation. */
#ifndef __NODES_MAX_GROUP__
#  define __NODES_MAX_GROUP__ NODE_GROUP_LEVEL_MAX
#endif
#ifndef __NODES_FEATURES__
#  define __NODES_FEATURES__ NODE_FEATURE_ALL
#endif

/* no assert in opencl */
#define kernel_assert(cond)

/* make_type definitions with opencl style element initializers */
#ifdef make_float2
#  undef make_float2
#endif
#ifdef make_float3
#  undef make_float3
#endif
#ifdef make_float4
#  undef make_float4
#endif
#ifdef make_int2
#  undef make_int2
#endif
#ifdef make_int3
#  undef make_int3
#endif
#ifdef make_int4
#  undef make_int4
#endif
#ifdef make_uchar4
#  undef make_uchar4
#endif

#define make_float2(x, y) ((float2)(x, y))
#define make_float3(x, y, z) ((float3)(x, y, z))
#define make_float4(x, y, z, w) ((float4)(x, y, z, w))
#define make_int2(x, y) ((int2)(x, y))
#define make_int3(x, y, z) ((int3)(x, y, z))
#define make_int4(x, y, z, w) ((int4)(x, y, z, w))
#define make_uchar4(x, y, z, w) ((uchar4)(x, y, z, w))

/* math functions */
#define __uint_as_float(x) as_float(x)
#define __float_as_uint(x) as_uint(x)
#define __int_as_float(x) as_float(x)
#define __float_as_int(x) as_int(x)
#define powf(x, y) pow(((float)(x)), ((float)(y)))
#define fabsf(x) fabs(((float)(x)))
#define copysignf(x, y) copysign(((float)(x)), ((float)(y)))
#define asinf(x) asin(((float)(x)))
#define acosf(x) acos(((float)(x)))
#define atanf(x) atan(((float)(x)))
#define floorf(x) floor(((float)(x)))
#define ceilf(x) ceil(((float)(x)))
#define hypotf(x, y) hypot(((float)(x)), ((float)(y)))
#define atan2f(x, y) atan2(((float)(x)), ((float)(y)))
#define fmaxf(x, y) fmax(((float)(x)), ((float)(y)))
#define fminf(x, y) fmin(((float)(x)), ((float)(y)))
#define fmodf(x, y) fmod((float)(x), (float)(y))
#define sinhf(x) sinh(((float)(x)))

#ifndef __CL_USE_NATIVE__
#  define sinf(x) native_sin(((float)(x)))
#  define cosf(x) native_cos(((float)(x)))
#  define tanf(x) native_tan(((float)(x)))
#  define expf(x) native_exp(((float)(x)))
#  define sqrtf(x) native_sqrt(((float)(x)))
#  define logf(x) native_log(((float)(x)))
#  define rcp(x)  native_recip(x)
#else
#  define sinf(x) sin(((float)(x)))
#  define cosf(x) cos(((float)(x)))
#  define tanf(x) tan(((float)(x)))
#  define expf(x) exp(((float)(x)))
#  define sqrtf(x) sqrt(((float)(x)))
#  define logf(x) log(((float)(x)))
#  define rcp(x)  recip(x))
#endif

/* data lookup defines */
#define kernel_data (*kg->data)
#define kernel_tex_array(tex) ((const ccl_global tex##_t*)(kg->buffers[kg->tex.cl_buffer] + kg->tex.data))
#define kernel_tex_fetch(tex, index) kernel_tex_array(tex)[(index)]

/* define NULL */
#define NULL 0

/* enable extensions */
#ifdef __KERNEL_CL_KHR_FP16__
#pragma OPENCL EXTENSION cl_khr_fp16 : enable
#endif

#include "util/util_half.h"
#include "util/util_types.h"

#endif /* __KERNEL_COMPAT_OPENCL_H__ */
