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

/* CPU kernel entry points */

#include "kernel.h"
#include "kernel_compat_cpu.h"
#include "kernel_math.h"
#include "kernel_types.h"
#include "kernel_globals.h"
#include "kernel_film.h"
#include "kernel_path.h"
#include "kernel_bake.h"

CCL_NAMESPACE_BEGIN

/* Memory Copy */

void kernel_const_copy(KernelGlobals *kg, const char *name, void *host, size_t size)
{
	if(strcmp(name, "__data") == 0)
		memcpy(&kg->__data, host, size);
	else
		assert(0);
}

void kernel_tex_copy(KernelGlobals *kg, const char *name, device_ptr mem, size_t width, size_t height, size_t depth, InterpolationType interpolation)
{
	if(0) {
	}

#define KERNEL_TEX(type, ttype, tname) \
	else if(strcmp(name, #tname) == 0) { \
		kg->tname.data = (type*)mem; \
		kg->tname.width = width; \
	}
#define KERNEL_IMAGE_TEX(type, ttype, tname)
#include "kernel_textures.h"

	else if(strstr(name, "__tex_image_float")) {
		texture_image_float4 *tex = NULL;
		int id = atoi(name + strlen("__tex_image_float_"));
		int array_index = id;

		if (array_index >= 0 && array_index < MAX_FLOAT_IMAGES) {
			tex = &kg->texture_float_images[array_index];
		}

		if(tex) {
			tex->data = (float4*)mem;
			tex->dimensions_set(width, height, depth);
			tex->interpolation = interpolation;
		}
	}
	else if(strstr(name, "__tex_image")) {
		texture_image_uchar4 *tex = NULL;
		int id = atoi(name + strlen("__tex_image_"));
		int array_index = id - MAX_FLOAT_IMAGES;

		if (array_index >= 0 && array_index < MAX_BYTE_IMAGES) {
			tex = &kg->texture_byte_images[array_index];
		}

		if(tex) {
			tex->data = (uchar4*)mem;
			tex->dimensions_set(width, height, depth);
			tex->interpolation = interpolation;
		}
	}
	else
		assert(0);
}

/* On x86-64, we can assume SSE2, so avoid the extra kernel and compile this one with SSE2 intrinsics */
#if defined(__x86_64__) || defined(_M_X64)
#define __KERNEL_SSE2__
#endif

/* quiet unused define warnings */
#if defined(__KERNEL_SSE2__)
	/* do nothing */
#endif

/* Path Tracing */

void kernel_cpu_path_trace(KernelGlobals *kg, float *buffer, unsigned int *rng_state, int sample, int x, int y, int offset, int stride)
{
#ifdef __BRANCHED_PATH__
	if(kernel_data.integrator.branched)
		kernel_branched_path_trace(kg, buffer, rng_state, sample, x, y, offset, stride);
	else
#endif
		kernel_path_trace(kg, buffer, rng_state, sample, x, y, offset, stride);
}

/* Film */

void kernel_cpu_convert_to_byte(KernelGlobals *kg, uchar4 *rgba, float *buffer, float sample_scale, int x, int y, int offset, int stride)
{
	kernel_film_convert_to_byte(kg, rgba, buffer, sample_scale, x, y, offset, stride);
}

void kernel_cpu_convert_to_half_float(KernelGlobals *kg, uchar4 *rgba, float *buffer, float sample_scale, int x, int y, int offset, int stride)
{
	kernel_film_convert_to_half_float(kg, rgba, buffer, sample_scale, x, y, offset, stride);
}

/* Shader Evaluation */

void kernel_cpu_shader(KernelGlobals *kg, uint4 *input, float4 *output, int type, int i, int offset, int sample)
{
	if(type >= SHADER_EVAL_BAKE)
		kernel_bake_evaluate(kg, input, output, (ShaderEvalType)type, i, offset, sample);
	else
		kernel_shader_evaluate(kg, input, output, (ShaderEvalType)type, i, sample);
}

CCL_NAMESPACE_END

