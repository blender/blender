/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file draw_builtin_shader.c
 *  \ingroup draw
 * Draw manager versions of #eGPUBuiltinShader, see #GPU_shader_get_builtin_shader.
 *
 * Allows for modifications to shaders (currently only clipping support).
 * Follow GPU_shader.h conventions to avoid annoyance.
 */

#include "BLI_utildefines.h"

#include "GPU_shader.h"

#include "DRW_render.h"

#include "draw_builtin_shader.h"  /* own include */


extern char datatoc_common_world_clip_lib_glsl[];

extern char datatoc_drw_shader_3D_vert_glsl[];
extern char datatoc_drw_shader_3D_smooth_color_vert_glsl[];

extern char datatoc_gpu_shader_uniform_color_frag_glsl[];
extern char datatoc_gpu_shader_3D_smooth_color_frag_glsl[];
extern char datatoc_gpu_shader_depth_only_frag_glsl[];

/* cache of built-in shaders (each is created on first use) */
static struct {
	GPUShader *builtin_shaders[GPU_NUM_BUILTIN_SHADERS];
} g_sh_data[DRW_SHADER_SLOT_LEN - 1] = {{{NULL}}};

static GPUShader *drw_shader_get_builtin_shader_clipped(eGPUBuiltinShader shader_id, bool *r_test_only)
{
	const char *world_clip_lib = datatoc_common_world_clip_lib_glsl;
	const char *world_clip_def = "#define USE_WORLD_CLIP_PLANES\n";

	if (r_test_only) {
		*r_test_only = true;
	}

	GPUShader *shader = NULL;
	switch (shader_id) {
		case GPU_SHADER_3D_UNIFORM_COLOR:
			if (r_test_only) {
				break;
			}
			shader = DRW_shader_create_from_arrays({
			        .vert = (const char *[]){world_clip_lib, datatoc_drw_shader_3D_vert_glsl, NULL},
			        .frag = (const char *[]){datatoc_gpu_shader_uniform_color_frag_glsl, NULL},
			        .defs = (const char *[]){world_clip_def, NULL}});
			break;
		case GPU_SHADER_3D_SMOOTH_COLOR:
			if (r_test_only) {
				break;
			}
			shader = DRW_shader_create_from_arrays({
			        .vert = (const char *[]){world_clip_lib, datatoc_drw_shader_3D_smooth_color_vert_glsl, NULL},
			        .frag = (const char *[]){datatoc_gpu_shader_3D_smooth_color_frag_glsl, NULL},
			        .defs = (const char *[]){world_clip_def, NULL}});
			break;
		case GPU_SHADER_3D_DEPTH_ONLY:
			if (r_test_only) {
				break;
			}
			shader = DRW_shader_create_from_arrays({
			        .vert = (const char *[]){world_clip_lib, datatoc_drw_shader_3D_vert_glsl, NULL},
			        .frag = (const char *[]){datatoc_gpu_shader_depth_only_frag_glsl, NULL},
			        .defs = (const char *[]){world_clip_def, NULL}});
			break;
		default:
			/* Unsupported, caller asserts. */
			if (r_test_only) {
				*r_test_only = false;
			}
	}
	return shader;
}

#ifndef NDEBUG
static bool drw_shader_get_builtin_shader_test_all(eGPUBuiltinShader shader_id)
{
	bool test = false;
	drw_shader_get_builtin_shader_clipped(shader_id, &test);
	return test;
}
#endif

GPUShader *DRW_shader_get_builtin_shader(eGPUBuiltinShader shader_id, eDRW_ShaderSlot slot)
{
	BLI_assert(drw_shader_get_builtin_shader_test_all(shader_id));
	if (slot == DRW_SHADER_SLOT_DEFAULT) {
		return GPU_shader_get_builtin_shader(shader_id);
	}

	GPUShader **builtin_shaders = g_sh_data[slot - 1].builtin_shaders;

	if (builtin_shaders[shader_id] != NULL) {
		return builtin_shaders[shader_id];
	}

	if (slot == DRW_SHADER_SLOT_CLIPPED) {
		builtin_shaders[shader_id] = drw_shader_get_builtin_shader_clipped(shader_id, NULL);
		return builtin_shaders[shader_id];
	}
	else {
		BLI_assert(0);
		return NULL;
	}
}

void DRW_shader_free_builtin_shaders(void)
{
	for (int j = 0; j < (DRW_SHADER_SLOT_LEN - 1); j++) {
		GPUShader **builtin_shaders = g_sh_data[j].builtin_shaders;
		for (int i = 0; i < GPU_NUM_BUILTIN_SHADERS; i++) {
			if (builtin_shaders[i]) {
				GPU_shader_free(builtin_shaders[i]);
				builtin_shaders[i] = NULL;
			}
		}
	}
}
