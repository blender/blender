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

#ifndef __KERNEL_COMPAT_CUDA_H__
#define __KERNEL_COMPAT_CUDA_H__

#define __KERNEL_GPU__
#define __KERNEL_CUDA__
#define CCL_NAMESPACE_BEGIN
#define CCL_NAMESPACE_END

/* Selective nodes compilation. */
#ifndef __NODES_MAX_GROUP__
#  define __NODES_MAX_GROUP__ NODE_GROUP_LEVEL_MAX
#endif
#ifndef __NODES_FEATURES__
#  define __NODES_FEATURES__ NODE_FEATURE_ALL
#endif

/* Manual definitions so we can compile without CUDA toolkit. */

typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
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

/* Qualifier wrappers for different names on different devices */

#define ccl_device __device__ __inline__
#if __CUDA_ARCH__ < 500
#  define ccl_device_inline __device__ __forceinline__
#  define ccl_device_forceinline __device__ __forceinline__
#else
#  define ccl_device_inline __device__ __inline__
#  define ccl_device_forceinline __device__ __forceinline__
#endif
#define ccl_device_noinline __device__ __noinline__
#define ccl_device_noinline_cpu ccl_device
#define ccl_global
#define ccl_static_constant __constant__
#define ccl_constant const
#define ccl_local __shared__
#define ccl_local_param
#define ccl_private
#define ccl_may_alias
#define ccl_addr_space
#define ccl_restrict __restrict__
#define ccl_loop_no_unroll
/* TODO(sergey): In theory we might use references with CUDA, however
 * performance impact yet to be investigated.
 */
#define ccl_ref
#define ccl_align(n) __align__(n)
#define ccl_optional_struct_init

#define ATTR_FALLTHROUGH

#define CCL_MAX_LOCAL_SIZE (CUDA_THREADS_BLOCK_WIDTH * CUDA_THREADS_BLOCK_WIDTH)

/* No assert supported for CUDA */

#define kernel_assert(cond)

/* Types */

#include "util/util_half.h"
#include "util/util_types.h"

/* Work item functions */

ccl_device_inline uint ccl_local_id(uint d)
{
  switch (d) {
    case 0:
      return threadIdx.x;
    case 1:
      return threadIdx.y;
    case 2:
      return threadIdx.z;
    default:
      return 0;
  }
}

#define ccl_global_id(d) (ccl_group_id(d) * ccl_local_size(d) + ccl_local_id(d))

ccl_device_inline uint ccl_local_size(uint d)
{
  switch (d) {
    case 0:
      return blockDim.x;
    case 1:
      return blockDim.y;
    case 2:
      return blockDim.z;
    default:
      return 0;
  }
}

#define ccl_global_size(d) (ccl_num_groups(d) * ccl_local_size(d))

ccl_device_inline uint ccl_group_id(uint d)
{
  switch (d) {
    case 0:
      return blockIdx.x;
    case 1:
      return blockIdx.y;
    case 2:
      return blockIdx.z;
    default:
      return 0;
  }
}

ccl_device_inline uint ccl_num_groups(uint d)
{
  switch (d) {
    case 0:
      return gridDim.x;
    case 1:
      return gridDim.y;
    case 2:
      return gridDim.z;
    default:
      return 0;
  }
}

/* Textures */

/* Use arrays for regular data. */
#define kernel_tex_fetch(t, index) t[(index)]
#define kernel_tex_array(t) (t)

#define kernel_data __data

/* Use fast math functions */

#define cosf(x) __cosf(((float)(x)))
#define sinf(x) __sinf(((float)(x)))
#define powf(x, y) __powf(((float)(x)), ((float)(y)))
#define tanf(x) __tanf(((float)(x)))
#define logf(x) __logf(((float)(x)))
#define expf(x) __expf(((float)(x)))

#endif /* __KERNEL_COMPAT_CUDA_H__ */
