/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define __KERNEL_GPU__
#define __KERNEL_HIP__
#define CCL_NAMESPACE_BEGIN
#define CCL_NAMESPACE_END

#ifndef ATTR_FALLTHROUGH
#  define ATTR_FALLTHROUGH
#endif

#ifdef __HIPCC_RTC__
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
#else
#  include <stdint.h>
#endif

#ifdef CYCLES_HIPBIN_CC
#  define FLT_MIN 1.175494350822287507969e-38f
#  define FLT_MAX 340282346638528859811704183484516925440.0f
#  define FLT_EPSILON 1.192092896e-07F
#endif

/* Qualifiers */

#define ccl_device __device__ __inline__
#define ccl_device_extern extern "C" __device__
#define ccl_device_inline __device__ __inline__
#define ccl_device_forceinline __device__ __forceinline__
#define ccl_device_noinline __device__ __noinline__
#define ccl_device_noinline_cpu ccl_device
#define ccl_device_inline_method ccl_device
#define ccl_global
#define ccl_inline_constant __constant__
#define ccl_device_constant __constant__ __device__
#define ccl_static_constexpr static constexpr
#define ccl_constant const
#define ccl_gpu_shared __shared__
#define ccl_private
#define ccl_ray_data ccl_private
#define ccl_may_alias
#define ccl_restrict __restrict__
#define ccl_align(n) __align__(n)
#define ccl_optional_struct_init

#define kernel_assert(cond)

/* Types */
#ifdef __HIP__
#  include "hip/hip_fp16.h"
#  include "hip/hip_runtime.h"
#endif

#ifdef _MSC_VER
#  include <immintrin.h>
#endif

#define ccl_gpu_thread_idx_x (threadIdx.x)
#define ccl_gpu_block_dim_x (blockDim.x)
#define ccl_gpu_block_idx_x (blockIdx.x)
#define ccl_gpu_grid_dim_x (gridDim.x)
#define ccl_gpu_warp_size (warpSize)
#define ccl_gpu_thread_mask(thread_warp) uint64_t((1ull << thread_warp) - 1)

#define ccl_gpu_global_id_x() (ccl_gpu_block_idx_x * ccl_gpu_block_dim_x + ccl_gpu_thread_idx_x)
#define ccl_gpu_global_size_x() (ccl_gpu_grid_dim_x * ccl_gpu_block_dim_x)

/* GPU warp synchronization */

#define ccl_gpu_syncthreads() __syncthreads()
#define ccl_gpu_ballot(predicate) __ballot(predicate)

/* GPU texture objects */
typedef hipTextureObject_t ccl_gpu_tex_object_2D;
typedef hipTextureObject_t ccl_gpu_tex_object_3D;

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

/* Use fast math functions */

#define cosf(x) __cosf(((float)(x)))
#define sinf(x) __sinf(((float)(x)))
#define powf(x, y) __powf(((float)(x)), ((float)(y)))
#define tanf(x) __tanf(((float)(x)))
#define logf(x) __logf(((float)(x)))
#define expf(x) __expf(((float)(x)))

/* Types */

#include "util/half.h"
#include "util/types.h"
