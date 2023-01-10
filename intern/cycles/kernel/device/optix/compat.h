/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2019, NVIDIA Corporation.
 * Copyright 2019-2022 Blender Foundation. */

#pragma once

#define OPTIX_DONT_INCLUDE_CUDA
#include <optix.h>

#define __KERNEL_GPU__
#define __KERNEL_CUDA__ /* OptiX kernels are implicitly CUDA kernels too */
#define __KERNEL_OPTIX__
#define CCL_NAMESPACE_BEGIN
#define CCL_NAMESPACE_END

#ifndef ATTR_FALLTHROUGH
#  define ATTR_FALLTHROUGH
#endif

/* Manual definitions so we can compile without CUDA toolkit. */

#ifdef __CUDACC_RTC__
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
#else
#  include <stdint.h>
#endif

#ifdef CYCLES_CUBIN_CC
#  define FLT_MIN 1.175494350822287507969e-38f
#  define FLT_MAX 340282346638528859811704183484516925440.0f
#  define FLT_EPSILON 1.192092896e-07F
#endif

#define ccl_device \
  static __device__ \
      __forceinline__  // Function calls are bad for OptiX performance, so inline everything
#define ccl_device_extern extern "C" __device__
#define ccl_device_inline ccl_device
#define ccl_device_forceinline ccl_device
#define ccl_device_inline_method __device__ __forceinline__
#define ccl_device_noinline static __device__ __noinline__
#define ccl_device_noinline_cpu ccl_device
#define ccl_global
#define ccl_inline_constant static __constant__
#define ccl_device_constant __constant__ __device__
#define ccl_constant const
#define ccl_gpu_shared __shared__
#define ccl_private
#define ccl_may_alias
#define ccl_restrict __restrict__
#define ccl_loop_no_unroll
#define ccl_align(n) __align__(n)

/* Zero initialize structs to help the compiler figure out scoping */
#define ccl_optional_struct_init = {}

/* No assert supported for CUDA */

#define kernel_assert(cond)

/* GPU texture objects */

typedef unsigned long long CUtexObject;
typedef CUtexObject ccl_gpu_tex_object_2D;
typedef CUtexObject ccl_gpu_tex_object_3D;

template<typename T>
ccl_device_forceinline T ccl_gpu_tex_object_read_2D(const ccl_gpu_tex_object_2D texobj,
                                                    const float x,
                                                    const float y)
{
  return tex2D<T>(texobj, x, y);
}

template<typename T>
ccl_device_forceinline T ccl_gpu_tex_object_read_3D(const ccl_gpu_tex_object_3D texobj,
                                                    const float x,
                                                    const float y,
                                                    const float z)
{
  return tex3D<T>(texobj, x, y, z);
}

/* Half */

typedef unsigned short half;

ccl_device_forceinline half __float2half(const float f)
{
  half val;
  asm("{  cvt.rn.f16.f32 %0, %1;}\n" : "=h"(val) : "f"(f));
  return val;
}

ccl_device_forceinline float __half2float(const half h)
{
  float val;
  asm("{  cvt.f32.f16 %0, %1;}\n" : "=f"(val) : "h"(h));
  return val;
}

/* Types */

#include "util/half.h"
#include "util/types.h"
