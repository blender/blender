/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file blender/draw/intern/draw_manager_shader.c
 *  \ingroup draw
 */

#include "draw_manager.h"

#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "GPU_shader.h"

extern char datatoc_gpu_shader_2D_vert_glsl[];
extern char datatoc_gpu_shader_3D_vert_glsl[];
extern char datatoc_gpu_shader_fullscreen_vert_glsl[];

GPUShader *DRW_shader_create(const char *vert, const char *geom, const char *frag, const char *defines)
{
	return GPU_shader_create(vert, frag, geom, NULL, defines);
}

GPUShader *DRW_shader_create_with_lib(
        const char *vert, const char *geom, const char *frag, const char *lib, const char *defines)
{
	GPUShader *sh;
	char *vert_with_lib = NULL;
	char *frag_with_lib = NULL;
	char *geom_with_lib = NULL;

	vert_with_lib = BLI_string_joinN(lib, vert);
	frag_with_lib = BLI_string_joinN(lib, frag);
	if (geom) {
		geom_with_lib = BLI_string_joinN(lib, geom);
	}

	sh = GPU_shader_create(vert_with_lib, frag_with_lib, geom_with_lib, NULL, defines);

	MEM_freeN(vert_with_lib);
	MEM_freeN(frag_with_lib);
	if (geom) {
		MEM_freeN(geom_with_lib);
	}

	return sh;
}

GPUShader *DRW_shader_create_2D(const char *frag, const char *defines)
{
	return GPU_shader_create(datatoc_gpu_shader_2D_vert_glsl, frag, NULL, NULL, defines);
}

GPUShader *DRW_shader_create_3D(const char *frag, const char *defines)
{
	return GPU_shader_create(datatoc_gpu_shader_3D_vert_glsl, frag, NULL, NULL, defines);
}

GPUShader *DRW_shader_create_fullscreen(const char *frag, const char *defines)
{
	return GPU_shader_create(datatoc_gpu_shader_fullscreen_vert_glsl, frag, NULL, NULL, defines);
}

GPUShader *DRW_shader_create_3D_depth_only(void)
{
	return GPU_shader_get_builtin_shader(GPU_SHADER_3D_DEPTH_ONLY);
}

void DRW_shader_free(GPUShader *shader)
{
	GPU_shader_free(shader);
}
