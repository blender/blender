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
 * limitations under the License
 */

#ifndef __KERNEL_COMPAT_OPENCL_H__
#define __KERNEL_COMPAT_OPENCL_H__

#define __KERNEL_GPU__
#define __KERNEL_OPENCL__

/* no namespaces in opencl */
#define CCL_NAMESPACE_BEGIN
#define CCL_NAMESPACE_END

#ifdef __KERNEL_OPENCL_AMD__
#define __CL_NO_FLOAT3__
#endif

#ifdef __CL_NO_FLOAT3__
#define float3 float4
#endif

#ifdef __CL_NOINLINE__
#define ccl_noinline __attribute__((noinline))
#else
#define ccl_noinline
#endif

/* in opencl all functions are device functions, so leave this empty */
#define ccl_device
#define ccl_device_inline ccl_device
#define ccl_device_noinline ccl_device ccl_noinline
#define ccl_may_alias
#define ccl_constant __constant
#define ccl_global __global

/* no assert in opencl */
#define kernel_assert(cond)

/* make_type definitions with opencl style element initializers */
#ifdef make_float2
#undef make_float2
#endif
#ifdef make_float3
#undef make_float3
#endif
#ifdef make_float4
#undef make_float4
#endif
#ifdef make_int2
#undef make_int2
#endif
#ifdef make_int3
#undef make_int3
#endif
#ifdef make_int4
#undef make_int4
#endif

#define make_float2(x, y) ((float2)(x, y))
#ifdef __CL_NO_FLOAT3__
#define make_float3(x, y, z) ((float4)(x, y, z, 0.0f))
#else
#define make_float3(x, y, z) ((float3)(x, y, z))
#endif
#define make_float4(x, y, z, w) ((float4)(x, y, z, w))
#define make_int2(x, y) ((int2)(x, y))
#define make_int3(x, y, z) ((int3)(x, y, z))
#define make_int4(x, y, z, w) ((int4)(x, y, z, w))

/* math functions */
#define __uint_as_float(x) as_float(x)
#define __float_as_uint(x) as_uint(x)
#define __int_as_float(x) as_float(x)
#define __float_as_int(x) as_int(x)
#define sqrtf(x) sqrt(((float)x))
#define cosf(x) cos(((float)x))
#define sinf(x) sin(((float)x))
#define powf(x, y) pow(((float)x), ((float)y))
#define fabsf(x) fabs(((float)x))
#define copysignf(x, y) copysign(((float)x), ((float)y))
#define cosf(x) cos(((float)x))
#define asinf(x) asin(((float)x))
#define acosf(x) acos(((float)x))
#define atanf(x) atan(((float)x))
#define tanf(x) tan(((float)x))
#define logf(x) log(((float)x))
#define floorf(x) floor(((float)x))
#define ceilf(x) ceil(((float)x))
#define expf(x) exp(((float)x))
#define hypotf(x, y) hypot(((float)x), ((float)y))
#define atan2f(x, y) atan2(((float)x), ((float)y))
#define fmaxf(x, y) fmax(((float)x), ((float)y))
#define fminf(x, y) fmin(((float)x), ((float)y))
#define fmodf(x, y) fmod((float)x, (float)y)

/* data lookup defines */
#define kernel_data (*kg->data)
#define kernel_tex_fetch(t, index) kg->t[index]

/* define NULL */
#define NULL 0

#include "util_half.h"
#include "util_types.h"

#endif /* __KERNEL_COMPAT_OPENCL_H__ */

