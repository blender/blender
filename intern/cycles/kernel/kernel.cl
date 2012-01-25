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

#include "kernel_film.h"
#include "kernel_path.h"

__kernel void kernel_ocl_path_trace(
	__constant KernelData *data,
	__global float *buffer,
	__global uint *rng_state,

#define KERNEL_TEX(type, ttype, name) \
	__global type *name,
#include "kernel_textures.h"

	int sample,
	int sx, int sy, int sw, int sh, int offset, int stride)
{
	KernelGlobals kglobals, *kg = &kglobals;

	kg->data = data;

#define KERNEL_TEX(type, ttype, name) \
	kg->name = name;
#include "kernel_textures.h"

	int x = sx + get_global_id(0);
	int y = sy + get_global_id(1);

	if(x < sx + sw && y < sy + sh)
		kernel_path_trace(kg, buffer, rng_state, sample, x, y, offset, stride);
}

__kernel void kernel_ocl_tonemap(
	__constant KernelData *data,
	__global uchar4 *rgba,
	__global float *buffer,

#define KERNEL_TEX(type, ttype, name) \
	__global type *name,
#include "kernel_textures.h"

	int sample, int resolution,
	int sx, int sy, int sw, int sh, int offset, int stride)
{
	KernelGlobals kglobals, *kg = &kglobals;

	kg->data = data;

#define KERNEL_TEX(type, ttype, name) \
	kg->name = name;
#include "kernel_textures.h"

	int x = sx + get_global_id(0);
	int y = sy + get_global_id(1);

	if(x < sx + sw && y < sy + sh)
		kernel_film_tonemap(kg, rgba, buffer, sample, resolution, x, y, offset, stride);
}

/*__kernel void kernel_ocl_shader(__global uint4 *input, __global float *output, int type, int sx)
{
	int x = sx + get_global_id(0);

	kernel_shader_evaluate(input, output, (ShaderEvalType)type, x);
}*/

