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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_fixed_material.c
 *  \ingroup gpu
 */

/* GLSL shaders to replace fixed function OpenGL materials and lighting. These
 * are deprecated in newer OpenGL versions and missing in OpenGL ES 2.0. Also,
 * two sided lighting is no longer natively supported on NVidia cards which
 * results in slow software fallback.
 *
 * Todo:
 * - Replace glLight and glMaterial functions entirely with GLSL uniforms, to
 *   make OpenGL ES 2.0 work.
 * - Replace glTexCoord and glColor with generic attributes.
 * - Optimize for case where fewer than 3 or 8 lights are used.
 * - Optimize for case where specular is not used.
 * - Optimize for case where no texture matrix is used.
 */

#include "GL/glew.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "GPU_extensions.h"

/* Fixed function material types */

static struct {
	GPUShader *cached_shaders[GPU_FIXED_OPTION_COMBINATIONS];
	bool failed_shaders[GPU_FIXED_OPTION_COMBINATIONS];

	bool need_normals;
} GPU_MATERIAL_STATE;

/* Init / exit */

void GPU_fixed_materials_init()
{
	memset(&GPU_MATERIAL_STATE, 0, sizeof(GPU_MATERIAL_STATE));
}

void GPU_fixed_materials_exit()
{
	int i;
	
	for (i = 0; i < GPU_FIXED_OPTION_COMBINATIONS; i++)
		if (GPU_MATERIAL_STATE.cached_shaders[i])
			GPU_shader_free(GPU_MATERIAL_STATE.cached_shaders[i]);
}

/* Shader lookup / create */

static GPUShader *gpu_fixed_material_shader(int options)
{
	/* glsl code */
	extern char datatoc_gpu_shader_fixed_vertex_glsl[];
	extern char datatoc_gpu_shader_fixed_fragment_glsl[];

	/* cached shaders */
	GPUShader *shader = GPU_MATERIAL_STATE.cached_shaders[options];

	if (!shader && !GPU_MATERIAL_STATE.failed_shaders[options]) {
		/* create shader if it doesn't exist yet */
		char defines[64*GPU_FIXED_OPTIONS_NUM] = "";

		if (options & GPU_FIXED_COLOR_MATERIAL)
			strcat(defines, "#define USE_COLOR\n");
		if (options & GPU_FIXED_TWO_SIDED)
			strcat(defines, "#define USE_TWO_SIDED\n");
		if (options & GPU_FIXED_SOLID_LIGHTING)
			strcat(defines, "#define USE_SOLID_LIGHTING\n");
		if (options & GPU_FIXED_SCENE_LIGHTING)
			strcat(defines, "#define USE_SCENE_LIGHTING\n");
		if (options & GPU_FIXED_TEXTURE_2D)
			strcat(defines, "#define USE_TEXTURE\n");

		shader = GPU_shader_create(
			datatoc_gpu_shader_fixed_vertex_glsl,
			datatoc_gpu_shader_fixed_fragment_glsl,
			NULL, defines);
		
		if (shader) {
			/* set texture map to first texture unit */
			if (options & GPU_FIXED_TEXTURE_2D)
				glUniform1i(GPU_shader_get_uniform(shader, "texture_map"), 0);

			GPU_MATERIAL_STATE.cached_shaders[options] = shader;
		}
		else
			GPU_MATERIAL_STATE.failed_shaders[options] = true;
	}

	return shader;
}

/* Bind / unbind */

void GPU_fixed_material_shader_bind(int options)
{
	if (GPU_glsl_support()) {
		GPUShader *shader = gpu_fixed_material_shader(options);

		if (shader)
			GPU_shader_bind(shader);
	}
	else {
		if (options & (GPU_FIXED_SOLID_LIGHTING|GPU_FIXED_SCENE_LIGHTING))
			glEnable(GL_LIGHTING);

		if (options & GPU_FIXED_TWO_SIDED)
			glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

		if (options & GPU_FIXED_COLOR_MATERIAL) {
			glEnable(GL_COLOR_MATERIAL);
			glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
		}

		if (options & GPU_FIXED_TEXTURE_2D)
			glEnable(GL_TEXTURE_2D);
	}

	/* temporary hack, should be solved outside of this file */
	GPU_MATERIAL_STATE.need_normals = (options & (GPU_FIXED_SOLID_LIGHTING|GPU_FIXED_SCENE_LIGHTING));
}

void GPU_fixed_material_shader_unbind()
{
	if (GPU_glsl_support()) {
		GPU_shader_unbind();
	}
	else {
		glDisable(GL_LIGHTING);
		glDisable(GL_COLOR_MATERIAL);
		glDisable(GL_TEXTURE_2D);
		glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
	}
}

/* Material Colors */

void GPU_fixed_material_colors(const float diffuse[3], const float specular[3],
	int shininess, float alpha)
{
	float gl_diffuse[4], gl_specular[4];

	copy_v3_v3(gl_diffuse, diffuse);
	gl_diffuse[3] = alpha;

	copy_v3_v3(gl_specular, specular);
	gl_specular[3] = 1.0f;

	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
	glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, CLAMPIS(shininess, 1, 128));
}

bool GPU_fixed_material_need_normals()
{
	return GPU_MATERIAL_STATE.need_normals;
}

