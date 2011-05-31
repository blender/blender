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

#ifndef __KERNEL_COMPAT_OPENCL_H__
#define __KERNEL_COMPAT_OPENCL_H__

#define __KERNEL_GPU__
#define __KERNEL_OPENCL__

CCL_NAMESPACE_BEGIN

#pragma OPENCL EXTENSION cl_khr_byte_addressable_store: enable

#define __device
#define __device_inline

#define kernel_assert(cond)

__device float kernel_tex_interp_(__global float *data, int width, float x)
{
	x = clamp(x, 0.0f, 1.0f)*width;

	int index = min((int)x, width-1);
	int nindex = min(index+1, width-1);
	float t = x - index;

	return (1.0f - t)*data[index] + t*data[nindex];
}

#define make_float2(x, y) ((float2)(x, y))
#define make_float3(x, y, z) ((float3)(x, y, z, 0.0f))
#define make_float4(x, y, z, w) ((float4)(x, y, z, w))
#define make_int2(x, y) ((int2)(x, y))
#define make_int3(x, y, z) ((int3)(x, y, z, 0))
#define make_int4(x, y, z, w) ((int4)(x, y, z, w))

typedef float4 float3;
typedef int4 int3;

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

#define kernel_data (*kg->data)
#define kernel_tex_interp(t, x) \
	kernel_tex_interp_(kg->t, kg->t##_width, x)
#define kernel_tex_fetch(t, index) \
	kg->t[index]

#define NULL 0

#include "util_types.h"

CCL_NAMESPACE_END

#endif /* __KERNEL_COMPAT_OPENCL_H__ */

