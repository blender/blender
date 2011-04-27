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

#include "util_types.h"

CCL_NAMESPACE_BEGIN

#define __device
#define __device_inline

__device float kernel_tex_interp_(__global float *data, int width, float x)
{
	x = clamp(x, 0.0f, 1.0f)*width;

	int index = min((int)x, width-1);
	int nindex = min(index+1, width-1);
	float t = x - index;

	return (1.0f - t)*data[index] + t*data[nindex];
}

#define kernel_data (*kg->data)
#define kernel_tex_interp(t, x) \
	kernel_tex_interp_(kg->t, kg->t##_width, x);

CCL_NAMESPACE_END

#endif /* __KERNEL_COMPAT_OPENCL_H__ */

