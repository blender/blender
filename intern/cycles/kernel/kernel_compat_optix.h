/*
 * Copyright 2019, NVIDIA Corporation.
 * Copyright 2019, Blender Foundation.
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

#ifndef __KERNEL_COMPAT_OPTIX_H__
#define __KERNEL_COMPAT_OPTIX_H__

#define OPTIX_DONT_INCLUDE_CUDA
#include <optix.h>

#define __KERNEL_GPU__
#define __KERNEL_CUDA__  // OptiX kernels are implicitly CUDA kernels too
#define __KERNEL_OPTIX__
#define CCL_NAMESPACE_BEGIN
#define CCL_NAMESPACE_END

#ifndef ATTR_FALLTHROUGH
#  define ATTR_FALLTHROUGH
#endif

#ifdef __CUDACC_RTC__
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
#else
#  include <stdint.h>
#endif
typedef unsigned short half;
typedef unsigned long long CUtexObject;

#ifdef CYCLES_CUBIN_CC
#  define FLT_MIN 1.175494350822287507969e-38f
#  define FLT_MAX 340282346638528859811704183484516925440.0f
#  define FLT_EPSILON 1.192092896e-07F
#endif

__device__ half __float2half(const float f)
{
  half val;
  asm("{  cvt.rn.f16.f32 %0, %1;}\n" : "=h"(val) : "f"(f));
  return val;
}

/* Selective nodes compilation. */
#ifndef __NODES_MAX_GROUP__
#  define __NODES_MAX_GROUP__ NODE_GROUP_LEVEL_MAX
#endif
#ifndef __NODES_FEATURES__
#  define __NODES_FEATURES__ NODE_FEATURE_ALL
#endif

#define ccl_device \
  __device__ __forceinline__  // Function calls are bad for OptiX performance, so inline everything
#define ccl_device_inline ccl_device
#define ccl_device_forceinline ccl_device
#define ccl_device_noinline __device__ __noinline__
#define ccl_device_noinline_cpu ccl_device
#define ccl_global
#define ccl_static_constant __constant__
#define ccl_constant const
#define ccl_local
#define ccl_local_param
#define ccl_private
#define ccl_may_alias
#define ccl_addr_space
#define ccl_loop_no_unroll
#define ccl_restrict __restrict__
#define ccl_ref
#define ccl_align(n) __align__(n)

// Zero initialize structs to help the compiler figure out scoping
#define ccl_optional_struct_init = {}

#define kernel_data __params.data  // See kernel_globals.h
#define kernel_tex_array(t) __params.t
#define kernel_tex_fetch(t, index) __params.t[(index)]

#define kernel_assert(cond)

/* Types */

#include "util/util_half.h"
#include "util/util_types.h"

#endif /* __KERNEL_COMPAT_OPTIX_H__ */
