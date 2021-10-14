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

#define __KERNEL_GPU__
#define __KERNEL_METAL__
#define CCL_NAMESPACE_BEGIN
#define CCL_NAMESPACE_END

#ifndef ATTR_FALLTHROUGH
#  define ATTR_FALLTHROUGH
#endif

#include <metal_atomic>
#include <metal_pack>
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wsign-compare"

/* Qualifiers */

#define ccl_device
#define ccl_device_inline ccl_device
#define ccl_device_forceinline ccl_device
#define ccl_device_noinline ccl_device __attribute__((noinline))
#define ccl_device_noinline_cpu ccl_device
#define ccl_global device
#define ccl_static_constant static constant constexpr
#define ccl_device_constant constant
#define ccl_constant const device
#define ccl_gpu_shared threadgroup
#define ccl_private thread
#define ccl_may_alias
#define ccl_restrict __restrict
#define ccl_loop_no_unroll
#define ccl_align(n) alignas(n)
#define ccl_optional_struct_init

/* No assert supported for Metal */

#define kernel_assert(cond)

/* make_type definitions with Metal style element initializers */
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

#define make_float2(x, y) float2(x, y)
#define make_float3(x, y, z) float3(x, y, z)
#define make_float4(x, y, z, w) float4(x, y, z, w)
#define make_int2(x, y) int2(x, y)
#define make_int3(x, y, z) int3(x, y, z)
#define make_int4(x, y, z, w) int4(x, y, z, w)
#define make_uchar4(x, y, z, w) uchar4(x, y, z, w)

/* Math functions */

#define __uint_as_float(x) as_type<float>(x)
#define __float_as_uint(x) as_type<uint>(x)
#define __int_as_float(x) as_type<float>(x)
#define __float_as_int(x) as_type<int>(x)
#define __float2half(x) half(x)
#define powf(x, y) pow(float(x), float(y))
#define fabsf(x) fabs(float(x))
#define copysignf(x, y) copysign(float(x), float(y))
#define asinf(x) asin(float(x))
#define acosf(x) acos(float(x))
#define atanf(x) atan(float(x))
#define floorf(x) floor(float(x))
#define ceilf(x) ceil(float(x))
#define hypotf(x, y) hypot(float(x), float(y))
#define atan2f(x, y) atan2(float(x), float(y))
#define fmaxf(x, y) fmax(float(x), float(y))
#define fminf(x, y) fmin(float(x), float(y))
#define fmodf(x, y) fmod(float(x), float(y))
#define sinhf(x) sinh(float(x))
#define coshf(x) cosh(float(x))
#define tanhf(x) tanh(float(x))

/* Use native functions with possibly lower precision for performance,
 * no issues found so far. */
#define trigmode fast
#define sinf(x) trigmode::sin(float(x))
#define cosf(x) trigmode::cos(float(x))
#define tanf(x) trigmode::tan(float(x))
#define expf(x) trigmode::exp(float(x))
#define sqrtf(x) trigmode::sqrt(float(x))
#define logf(x) trigmode::log(float(x))

#define NULL 0
