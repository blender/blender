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

/* OpenCL kernel entry points - unfinished */

#include "kernel_compat_opencl.h"
#include "kernel_math.h"
#include "kernel_types.h"
#include "kernel_globals.h"

typedef struct KernelGlobals {
	__constant KernelData *data;

	__global float *__response_curve_R;
	int __response_curve_R_width;

	__global float *__response_curve_G;
	int __response_curve_G_width;

	__global float *__response_curve_B;
	int __response_curve_B_width;
} KernelGlobals;

#include "kernel_film.h"
//#include "kernel_path.h"
//#include "kernel_displace.h"

__kernel void kernel_ocl_path_trace(__constant KernelData *data, __global float4 *buffer, __global uint *rng_state, int pass, int sx, int sy, int sw, int sh)
{
	KernelGlobals kglobals, *kg = &kglobals;
	kg->data = data;

	int x = get_global_id(0);
	int y = get_global_id(1);
	int w = kernel_data.cam.width;

	if(x < sx + sw && y < sy + sh) {
		if(pass == 0) {
			buffer[x + w*y].x = 0.5f;
			buffer[x + w*y].y = 0.5f;
			buffer[x + w*y].z = 0.5f;
		}
		else {
			buffer[x + w*y].x += 0.5f;
			buffer[x + w*y].y += 0.5f;
			buffer[x + w*y].z += 0.5f;
		}
		
		//= make_float3(1.0f, 0.9f, 0.0f);
		//kernel_path_trace(buffer, rng_state, pass, x, y);
	}
}

__kernel void kernel_ocl_tonemap(
	__constant KernelData *data,
	__global uchar4 *rgba,
	__global float4 *buffer,
	__global float *__response_curve_R,
	int __response_curve_R_width,
	__global float *__response_curve_G,
	int __response_curve_G_width,
	__global float *__response_curve_B,
	int __response_curve_B_width,
	int pass, int resolution,
	int sx, int sy, int sw, int sh)
{
	KernelGlobals kglobals, *kg = &kglobals;

	kg->data = data;
	kg->__response_curve_R = __response_curve_R;
	kg->__response_curve_R_width = __response_curve_R_width;
	kg->__response_curve_G = __response_curve_G;
	kg->__response_curve_G_width = __response_curve_G_width;
	kg->__response_curve_B = __response_curve_B;
	kg->__response_curve_B_width = __response_curve_B_width;

	int x = sx + get_global_id(0);
	int y = sy + get_global_id(1);

	if(x < sx + sw && y < sy + sh)
		kernel_film_tonemap(kg, rgba, buffer, pass, resolution, x, y);
}

__kernel void kernel_ocl_displace(__global uint4 *input, __global float3 *offset, int sx)
{
	int x = sx + get_global_id(0);

	kernel_displace(input, offset, x);
}

