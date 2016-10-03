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

#include <cuda.h>
#include <cuda_fp16.h>
#include <float.h>

/* Qualifier wrappers for different names on different devices */

#define ccl_device  __device__ __inline__
#  define ccl_device_forceinline  __device__ __forceinline__
#if (__KERNEL_CUDA_VERSION__ == 80) && (__CUDA_ARCH__ < 500)
#  define ccl_device_inline  __device__ __forceinline__
#else
#  define ccl_device_inline  __device__ __inline__
#endif
#define ccl_device_noinline  __device__ __noinline__
#define ccl_global
#define ccl_constant
#define ccl_may_alias
#define ccl_addr_space
#define ccl_restrict __restrict__
#define ccl_align(n) __align__(n)

/* No assert supported for CUDA */

#define kernel_assert(cond)

/* Types */

#include "util_half.h"
#include "util_types.h"

/* Textures */

typedef texture<float4, 1> texture_float4;
typedef texture<float2, 1> texture_float2;
typedef texture<float, 1> texture_float;
typedef texture<uint, 1> texture_uint;
typedef texture<int, 1> texture_int;
typedef texture<uint4, 1> texture_uint4;
typedef texture<uchar, 1> texture_uchar;
typedef texture<uchar4, 1> texture_uchar4;
typedef texture<float4, 2> texture_image_float4;
typedef texture<float4, 3> texture_image3d_float4;
typedef texture<uchar4, 2, cudaReadModeNormalizedFloat> texture_image_uchar4;

/* Macros to handle different memory storage on different devices */

/* On Fermi cards (4xx and 5xx), we use regular textures for both data and images.
 * On Kepler (6xx) and above, we use Bindless Textures for images and arrays for data.
 *
 * Arrays are necessary in order to use the full VRAM on newer cards, and it's slightly faster.
 * Using Arrays on Fermi turned out to be slower.*/

/* Fermi */
#if __CUDA_ARCH__ < 300
#  define __KERNEL_CUDA_TEX_STORAGE__
#  define kernel_tex_fetch(t, index) tex1Dfetch(t, index)

#  define kernel_tex_image_interp(t, x, y) tex2D(t, x, y)
#  define kernel_tex_image_interp_3d(t, x, y, z) tex3D(t, x, y, z)

/* Kepler */
#else
#  define kernel_tex_fetch(t, index) t[(index)]

#  define kernel_tex_image_interp_float4(t, x, y) tex2D<float4>(t, x, y)
#  define kernel_tex_image_interp_float(t, x, y) tex2D<float>(t, x, y)
#  define kernel_tex_image_interp_3d_float4(t, x, y, z) tex3D<float4>(t, x, y, z)
#  define kernel_tex_image_interp_3d_float(t, x, y, z) tex3D<float>(t, x, y, z)
#endif

#define kernel_data __data

/* Use fast math functions */

#define cosf(x) __cosf(((float)(x)))
#define sinf(x) __sinf(((float)(x)))
#define powf(x, y) __powf(((float)(x)), ((float)(y)))
#define tanf(x) __tanf(((float)(x)))
#define logf(x) __logf(((float)(x)))
#define expf(x) __expf(((float)(x)))

#endif /* __KERNEL_COMPAT_CUDA_H__ */
