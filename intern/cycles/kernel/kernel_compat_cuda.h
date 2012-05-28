/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __KERNEL_COMPAT_CUDA_H__
#define __KERNEL_COMPAT_CUDA_H__

#define __KERNEL_GPU__
#define __KERNEL_CUDA__
#define CCL_NAMESPACE_BEGIN
#define CCL_NAMESPACE_END

#include <cuda.h>
#include <float.h>

#include "util_types.h"

/* Qualifier wrappers for different names on different devices */

#define __device  __device__ __inline__
#define __device_inline  __device__ __inline__
#define __device_noinline  __device__ __noinline__
#define __global
#define __shared __shared__
#define __constant

/* No assert supported for CUDA */

#define kernel_assert(cond)

/* Textures */

typedef texture<float4, 1> texture_float4;
typedef texture<float2, 1> texture_float2;
typedef texture<float, 1> texture_float;
typedef texture<uint, 1> texture_uint;
typedef texture<int, 1> texture_int;
typedef texture<uint4, 1> texture_uint4;
typedef texture<uchar4, 1> texture_uchar4;
typedef texture<float4, 2> texture_image_float4;
typedef texture<uchar4, 2, cudaReadModeNormalizedFloat> texture_image_uchar4;

/* Macros to handle different memory storage on different devices */

#define kernel_tex_fetch(t, index) tex1Dfetch(t, index)
#define kernel_tex_interp(t, x, size) tex1D(t, x)
#define kernel_tex_image_interp(t, x, y) tex2D(t, x, y)

#define kernel_data __data

/* Use fast math functions */

#define cosf(x) __cosf(((float)x))
#define sinf(x) __sinf(((float)x))
#define powf(x, y) __powf(((float)x), ((float)y))
#define cosf(x) __cosf(((float)x))
#define tanf(x) __tanf(((float)x))
#define logf(x) __logf(((float)x))
#define expf(x) __expf(((float)x))

#endif /* __KERNEL_COMPAT_CUDA_H__ */

