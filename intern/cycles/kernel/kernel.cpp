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

/* Globals */

KernelGlobals *kernel_globals_create()
{
	KernelGlobals *kg = new KernelGlobals();
#ifdef WITH_OSL
	kg->osl.use = false;
#endif
	return kg;
}

void kernel_globals_free(KernelGlobals *kg)
{
	delete kg;
}

/* OSL */

#ifdef WITH_OSL

void *kernel_osl_memory(KernelGlobals *kg)
{
	return (void*)&kg->osl;
}

bool kernel_osl_use(KernelGlobals *kg)
{
	return kg->osl.use;
}

#endif

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

	else if(strstr(name, "__tex_image")) {
		texture_image_uchar4 *tex = NULL;
		int id = atoi(name + strlen("__tex_image_"));

		switch(id) {
			case 0: tex = &kg->__tex_image_000; break;
			case 1: tex = &kg->__tex_image_001; break;
			case 2: tex = &kg->__tex_image_002; break;
			case 3: tex = &kg->__tex_image_003; break;
			case 4: tex = &kg->__tex_image_004; break;
			case 5: tex = &kg->__tex_image_005; break;
			case 6: tex = &kg->__tex_image_006; break;
			case 7: tex = &kg->__tex_image_007; break;
			case 8: tex = &kg->__tex_image_008; break;
			case 9: tex = &kg->__tex_image_009; break;
			case 10: tex = &kg->__tex_image_010; break;
			case 11: tex = &kg->__tex_image_011; break;
			case 12: tex = &kg->__tex_image_012; break;
			case 13: tex = &kg->__tex_image_013; break;
			case 14: tex = &kg->__tex_image_014; break;
			case 15: tex = &kg->__tex_image_015; break;
			case 16: tex = &kg->__tex_image_016; break;
			case 17: tex = &kg->__tex_image_017; break;
			case 18: tex = &kg->__tex_image_018; break;
			case 19: tex = &kg->__tex_image_019; break;
			case 20: tex = &kg->__tex_image_020; break;
			case 21: tex = &kg->__tex_image_021; break;
			case 22: tex = &kg->__tex_image_022; break;
			case 23: tex = &kg->__tex_image_023; break;
			case 24: tex = &kg->__tex_image_024; break;
			case 25: tex = &kg->__tex_image_025; break;
			case 26: tex = &kg->__tex_image_026; break;
			case 27: tex = &kg->__tex_image_027; break;
			case 28: tex = &kg->__tex_image_028; break;
			case 29: tex = &kg->__tex_image_029; break;
			case 30: tex = &kg->__tex_image_030; break;
			case 31: tex = &kg->__tex_image_031; break;
			case 32: tex = &kg->__tex_image_032; break;
			case 33: tex = &kg->__tex_image_033; break;
			case 34: tex = &kg->__tex_image_034; break;
			case 35: tex = &kg->__tex_image_035; break;
			case 36: tex = &kg->__tex_image_036; break;
			case 37: tex = &kg->__tex_image_037; break;
			case 38: tex = &kg->__tex_image_038; break;
			case 39: tex = &kg->__tex_image_039; break;
			case 40: tex = &kg->__tex_image_040; break;
			case 41: tex = &kg->__tex_image_041; break;
			case 42: tex = &kg->__tex_image_042; break;
			case 43: tex = &kg->__tex_image_043; break;
			case 44: tex = &kg->__tex_image_044; break;
			case 45: tex = &kg->__tex_image_045; break;
			case 46: tex = &kg->__tex_image_046; break;
			case 47: tex = &kg->__tex_image_047; break;
			case 48: tex = &kg->__tex_image_048; break;
			case 49: tex = &kg->__tex_image_049; break;
			case 50: tex = &kg->__tex_image_050; break;
			case 51: tex = &kg->__tex_image_051; break;
			case 52: tex = &kg->__tex_image_052; break;
			case 53: tex = &kg->__tex_image_053; break;
			case 54: tex = &kg->__tex_image_054; break;
			case 55: tex = &kg->__tex_image_055; break;
			case 56: tex = &kg->__tex_image_056; break;
			case 57: tex = &kg->__tex_image_057; break;
			case 58: tex = &kg->__tex_image_058; break;
			case 59: tex = &kg->__tex_image_059; break;
			case 60: tex = &kg->__tex_image_060; break;
			case 61: tex = &kg->__tex_image_061; break;
			case 62: tex = &kg->__tex_image_062; break;
			case 63: tex = &kg->__tex_image_063; break;
			case 64: tex = &kg->__tex_image_064; break;
			case 65: tex = &kg->__tex_image_065; break;
			case 66: tex = &kg->__tex_image_066; break;
			case 67: tex = &kg->__tex_image_067; break;
			case 68: tex = &kg->__tex_image_068; break;
			case 69: tex = &kg->__tex_image_069; break;
			case 70: tex = &kg->__tex_image_070; break;
			case 71: tex = &kg->__tex_image_071; break;
			case 72: tex = &kg->__tex_image_072; break;
			case 73: tex = &kg->__tex_image_073; break;
			case 74: tex = &kg->__tex_image_074; break;
			case 75: tex = &kg->__tex_image_075; break;
			case 76: tex = &kg->__tex_image_076; break;
			case 77: tex = &kg->__tex_image_077; break;
			case 78: tex = &kg->__tex_image_078; break;
			case 79: tex = &kg->__tex_image_079; break;
			case 80: tex = &kg->__tex_image_080; break;
			case 81: tex = &kg->__tex_image_081; break;
			case 82: tex = &kg->__tex_image_082; break;
			case 83: tex = &kg->__tex_image_083; break;
			case 84: tex = &kg->__tex_image_084; break;
			case 85: tex = &kg->__tex_image_085; break;
			case 86: tex = &kg->__tex_image_086; break;
			case 87: tex = &kg->__tex_image_087; break;
			case 88: tex = &kg->__tex_image_088; break;
			case 89: tex = &kg->__tex_image_089; break;
			case 90: tex = &kg->__tex_image_090; break;
			case 91: tex = &kg->__tex_image_091; break;
			case 92: tex = &kg->__tex_image_092; break;
			case 93: tex = &kg->__tex_image_093; break;
			case 94: tex = &kg->__tex_image_094; break;
			case 95: tex = &kg->__tex_image_095; break;
			case 96: tex = &kg->__tex_image_096; break;
			case 97: tex = &kg->__tex_image_097; break;
			case 98: tex = &kg->__tex_image_098; break;
			case 99: tex = &kg->__tex_image_099; break;
			default: break;
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

void kernel_cpu_path_trace(KernelGlobals *kg, float4 *buffer, unsigned int *rng_state, int sample, int x, int y, int offset, int stride)
{
	kernel_path_trace(kg, buffer, rng_state, sample, x, y, offset, stride);
}

/* Tonemapping */

void kernel_cpu_tonemap(KernelGlobals *kg, uchar4 *rgba, float4 *buffer, int sample, int resolution, int x, int y, int offset, int stride)
{
	kernel_film_tonemap(kg, rgba, buffer, sample, resolution, x, y, offset, stride);
}

/* Shader Evaluation */

void kernel_cpu_shader(KernelGlobals *kg, uint4 *input, float4 *output, int type, int i)
{
	kernel_shader_evaluate(kg, input, output, (ShaderEvalType)type, i);
}

CCL_NAMESPACE_END

