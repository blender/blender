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

#include "../../kernel_compat_opencl.h"
#include "../../kernel_math.h"
#include "../../kernel_types.h"
#include "../../kernel_globals.h"
#include "../../kernel_image_opencl.h"

#include "../../kernel_film.h"

#if defined(__COMPILE_ONLY_MEGAKERNEL__) || !defined(__NO_BAKING__)
#  include "../../kernel_path.h"
#  include "../../kernel_path_branched.h"
#else  /* __COMPILE_ONLY_MEGAKERNEL__ */
/* Include only actually used headers for the case
 * when path tracing kernels are not needed.
 */
#  include "../../kernel_random.h"
#  include "../../kernel_differential.h"
#  include "../../kernel_montecarlo.h"
#  include "../../kernel_projection.h"
#  include "../../geom/geom.h"
#  include "../../bvh/bvh.h"

#  include "../../kernel_accumulate.h"
#  include "../../kernel_camera.h"
#  include "../../kernel_shader.h"
#endif  /* defined(__COMPILE_ONLY_MEGAKERNEL__) || !defined(__NO_BAKING__) */

#include "../../kernel_bake.h"

#ifdef __COMPILE_ONLY_MEGAKERNEL__

__kernel void kernel_ocl_path_trace(
	ccl_constant KernelData *data,
	ccl_global float *buffer,
	ccl_global uint *rng_state,

#define KERNEL_TEX(type, ttype, name) \
	ccl_global type *name,
#include "../../kernel_textures.h"

	int sample,
	int sx, int sy, int sw, int sh, int offset, int stride)
{
	KernelGlobals kglobals, *kg = &kglobals;

	kg->data = data;

#define KERNEL_TEX(type, ttype, name) \
	kg->name = name;
#include "../../kernel_textures.h"

	int x = sx + get_global_id(0);
	int y = sy + get_global_id(1);

	if(x < sx + sw && y < sy + sh)
		kernel_path_trace(kg, buffer, rng_state, sample, x, y, offset, stride);
}

#else  /* __COMPILE_ONLY_MEGAKERNEL__ */

__kernel void kernel_ocl_shader(
	ccl_constant KernelData *data,
	ccl_global uint4 *input,
	ccl_global float4 *output,
	ccl_global float *output_luma,

#define KERNEL_TEX(type, ttype, name) \
	ccl_global type *name,
#include "../../kernel_textures.h"

	int type, int sx, int sw, int offset, int sample)
{
	KernelGlobals kglobals, *kg = &kglobals;

	kg->data = data;

#define KERNEL_TEX(type, ttype, name) \
	kg->name = name;
#include "../../kernel_textures.h"

	int x = sx + get_global_id(0);

	if(x < sx + sw) {
		kernel_shader_evaluate(kg,
		                       input,
		                       output,
		                       output_luma,
		                       (ShaderEvalType)type,
		                       x,
		                       sample);
	}
}

__kernel void kernel_ocl_bake(
	ccl_constant KernelData *data,
	ccl_global uint4 *input,
	ccl_global float4 *output,

#define KERNEL_TEX(type, ttype, name) \
	ccl_global type *name,
#include "../../kernel_textures.h"

	int type, int filter, int sx, int sw, int offset, int sample)
{
	KernelGlobals kglobals, *kg = &kglobals;

	kg->data = data;

#define KERNEL_TEX(type, ttype, name) \
	kg->name = name;
#include "../../kernel_textures.h"

	int x = sx + get_global_id(0);

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

#define KERNEL_TEX(type, ttype, name) \
	ccl_global type *name,
#include "../../kernel_textures.h"

	float sample_scale,
	int sx, int sy, int sw, int sh, int offset, int stride)
{
	KernelGlobals kglobals, *kg = &kglobals;

	kg->data = data;

#define KERNEL_TEX(type, ttype, name) \
	kg->name = name;
#include "../../kernel_textures.h"

	int x = sx + get_global_id(0);
	int y = sy + get_global_id(1);

	if(x < sx + sw && y < sy + sh)
		kernel_film_convert_to_byte(kg, rgba, buffer, sample_scale, x, y, offset, stride);
}

__kernel void kernel_ocl_convert_to_half_float(
	ccl_constant KernelData *data,
	ccl_global uchar4 *rgba,
	ccl_global float *buffer,

#define KERNEL_TEX(type, ttype, name) \
	ccl_global type *name,
#include "../../kernel_textures.h"

	float sample_scale,
	int sx, int sy, int sw, int sh, int offset, int stride)
{
	KernelGlobals kglobals, *kg = &kglobals;

	kg->data = data;

#define KERNEL_TEX(type, ttype, name) \
	kg->name = name;
#include "../../kernel_textures.h"

	int x = sx + get_global_id(0);
	int y = sy + get_global_id(1);

	if(x < sx + sw && y < sy + sh)
		kernel_film_convert_to_half_float(kg, rgba, buffer, sample_scale, x, y, offset, stride);
}

#endif  /* __COMPILE_ONLY_MEGAKERNEL__ */
