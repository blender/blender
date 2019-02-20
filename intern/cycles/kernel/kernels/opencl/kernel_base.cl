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

/* OpenCL base kernels entry points */

#include "kernel/kernel_compat_opencl.h"
#include "kernel/kernel_types.h"
#include "kernel/kernel_globals.h"

#include "kernel/kernel_film.h"


__kernel void kernel_ocl_convert_to_byte(
	ccl_constant KernelData *data,
	ccl_global uchar4 *rgba,
	ccl_global float *buffer,

	KERNEL_BUFFER_PARAMS,

	float sample_scale,
	int sx, int sy, int sw, int sh, int offset, int stride)
{
	KernelGlobals kglobals, *kg = &kglobals;

	kg->data = data;

	kernel_set_buffer_pointers(kg, KERNEL_BUFFER_ARGS);
	kernel_set_buffer_info(kg);

	int x = sx + ccl_global_id(0);
	int y = sy + ccl_global_id(1);

	if(x < sx + sw && y < sy + sh)
		kernel_film_convert_to_byte(kg, rgba, buffer, sample_scale, x, y, offset, stride);
}

__kernel void kernel_ocl_convert_to_half_float(
	ccl_constant KernelData *data,
	ccl_global uchar4 *rgba,
	ccl_global float *buffer,

	KERNEL_BUFFER_PARAMS,

	float sample_scale,
	int sx, int sy, int sw, int sh, int offset, int stride)
{
	KernelGlobals kglobals, *kg = &kglobals;

	kg->data = data;

	kernel_set_buffer_pointers(kg, KERNEL_BUFFER_ARGS);
	kernel_set_buffer_info(kg);

	int x = sx + ccl_global_id(0);
	int y = sy + ccl_global_id(1);

	if(x < sx + sw && y < sy + sh)
		kernel_film_convert_to_half_float(kg, rgba, buffer, sample_scale, x, y, offset, stride);
}

__kernel void kernel_ocl_zero_buffer(ccl_global float4 *buffer, uint64_t size, uint64_t offset)
{
	size_t i = ccl_global_id(0) + ccl_global_id(1) * ccl_global_size(0);

	if(i < size / sizeof(float4)) {
		buffer[i+offset/sizeof(float4)] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
	}
	else if(i == size / sizeof(float4)) {
		ccl_global uchar *b = (ccl_global uchar*)&buffer[i+offset/sizeof(float4)];

		for(i = 0; i < size % sizeof(float4); i++) {
			*(b++) = 0;
		}
	}
}
