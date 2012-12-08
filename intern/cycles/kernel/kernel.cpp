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

/* CPU kernel entry points */

#include "kernel.h"
#include "kernel_compat_cpu.h"
#include "kernel_math.h"
#include "kernel_types.h"
#include "kernel_globals.h"
#include "kernel_film.h"
#include "kernel_path.h"
#include "kernel_displace.h"

CCL_NAMESPACE_BEGIN

/* Memory Copy */

void kernel_const_copy(KernelGlobals *kg, const char *name, void *host, size_t size)
{
	if(strcmp(name, "__data") == 0)
		memcpy(&kg->__data, host, size);
	else
		assert(0);
}

void kernel_tex_copy(KernelGlobals *kg, const char *name, device_ptr mem, size_t width, size_t height)
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
			tex->width = width;
			tex->height = height;
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
			tex->width = width;
			tex->height = height;
		}
	}
	else
		assert(0);
}

/* Path Tracing */

void kernel_cpu_path_trace(KernelGlobals *kg, float *buffer, unsigned int *rng_state, int sample, int x, int y, int offset, int stride)
{
	kernel_path_trace(kg, buffer, rng_state, sample, x, y, offset, stride);
}

/* Tonemapping */

void kernel_cpu_tonemap(KernelGlobals *kg, uchar4 *rgba, float *buffer, int sample, int resolution, int x, int y, int offset, int stride)
{
	kernel_film_tonemap(kg, rgba, buffer, sample, resolution, x, y, offset, stride);
}

/* Shader Evaluation */

void kernel_cpu_shader(KernelGlobals *kg, uint4 *input, float4 *output, int type, int i)
{
	kernel_shader_evaluate(kg, input, output, (ShaderEvalType)type, i);
}

CCL_NAMESPACE_END

