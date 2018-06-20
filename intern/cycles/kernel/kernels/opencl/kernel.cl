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

/* OpenCL kernel entry points - unfinished */

#include "kernel/kernel_compat_opencl.h"
#include "kernel/kernel_math.h"
#include "kernel/kernel_types.h"
#include "kernel/kernel_globals.h"
#include "kernel/kernel_color.h"
#include "kernel/kernels/opencl/kernel_opencl_image.h"

#include "kernel/kernel_film.h"

#if defined(__COMPILE_ONLY_MEGAKERNEL__) || !defined(__NO_BAKING__)
#  include "kernel/kernel_path.h"
#  include "kernel/kernel_path_branched.h"
#else  /* __COMPILE_ONLY_MEGAKERNEL__ */
/* Include only actually used headers for the case
 * when path tracing kernels are not needed.
 */
#  include "kernel/kernel_random.h"
#  include "kernel/kernel_differential.h"
#  include "kernel/kernel_montecarlo.h"
#  include "kernel/kernel_projection.h"
#  include "kernel/geom/geom.h"
#  include "kernel/bvh/bvh.h"

#  include "kernel/kernel_accumulate.h"
#  include "kernel/kernel_camera.h"
#  include "kernel/kernel_shader.h"
#endif  /* defined(__COMPILE_ONLY_MEGAKERNEL__) || !defined(__NO_BAKING__) */

#include "kernel/kernel_bake.h"

#ifdef __COMPILE_ONLY_MEGAKERNEL__

__kernel void kernel_ocl_path_trace(
	ccl_constant KernelData *data,
	ccl_global float *buffer,

	KERNEL_BUFFER_PARAMS,

	int sample,
	int sx, int sy, int sw, int sh, int offset, int stride)
{
	KernelGlobals kglobals, *kg = &kglobals;

	kg->data = data;

	kernel_set_buffer_pointers(kg, KERNEL_BUFFER_ARGS);
	kernel_set_buffer_info(kg);

	int x = sx + ccl_global_id(0);
	int y = sy + ccl_global_id(1);

	if(x < sx + sw && y < sy + sh)
		kernel_path_trace(kg, buffer, sample, x, y, offset, stride);
}

#else  /* __COMPILE_ONLY_MEGAKERNEL__ */

__kernel void kernel_ocl_displace(
	ccl_constant KernelData *data,
	ccl_global uint4 *input,
	ccl_global float4 *output,

	KERNEL_BUFFER_PARAMS,

	int type, int sx, int sw, int offset, int sample)
{
	KernelGlobals kglobals, *kg = &kglobals;

	kg->data = data;

	kernel_set_buffer_pointers(kg, KERNEL_BUFFER_ARGS);
	kernel_set_buffer_info(kg);

	int x = sx + ccl_global_id(0);

	if(x < sx + sw) {
		kernel_displace_evaluate(kg, input, output, x);
	}
}
__kernel void kernel_ocl_background(
	ccl_constant KernelData *data,
	ccl_global uint4 *input,
	ccl_global float4 *output,

	KERNEL_BUFFER_PARAMS,

	int type, int sx, int sw, int offset, int sample)
{
	KernelGlobals kglobals, *kg = &kglobals;

	kg->data = data;

	kernel_set_buffer_pointers(kg, KERNEL_BUFFER_ARGS);
	kernel_set_buffer_info(kg);

	int x = sx + ccl_global_id(0);

	if(x < sx + sw) {
		kernel_background_evaluate(kg, input, output, x);
	}
}

__kernel void kernel_ocl_bake(
	ccl_constant KernelData *data,
	ccl_global uint4 *input,
	ccl_global float4 *output,

	KERNEL_BUFFER_PARAMS,

	int type, int filter, int sx, int sw, int offset, int sample)
{
	KernelGlobals kglobals, *kg = &kglobals;

	kg->data = data;

	kernel_set_buffer_pointers(kg, KERNEL_BUFFER_ARGS);
	kernel_set_buffer_info(kg);

	int x = sx + ccl_global_id(0);

	if(x < sx + sw) {
#ifdef __NO_BAKING__
		output[x] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
#else
		kernel_bake_evaluate(kg, input, output, (ShaderEvalType)type, filter, x, offset, sample);
#endif
	}
}

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

#endif  /* __COMPILE_ONLY_MEGAKERNEL__ */
