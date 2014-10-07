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

/** \file blender/gpu/intern/gpu_simple_shader.c
 *  \ingroup gpu
 *
 * GLSL shaders to replace fixed function OpenGL materials and lighting. These
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

#include "GPU_glew.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "GPU_extensions.h"
#include "GPU_simple_shader.h"

/* State */

// #define NUM_OPENGL_LIGHTS 8

static struct {
	GPUShader *cached_shaders[GPU_SHADER_OPTION_COMBINATIONS];
	bool failed_shaders[GPU_SHADER_OPTION_COMBINATIONS];

	bool need_normals;

	int lights_enabled;
	int lights_directional;
} GPU_MATERIAL_STATE;

/* Init / exit */

void GPU_simple_shaders_init(void)
{
	memset(&GPU_MATERIAL_STATE, 0, sizeof(GPU_MATERIAL_STATE));
}

void GPU_simple_shaders_exit(void)
{
	int i;
	
	for (i = 0; i < GPU_SHADER_OPTION_COMBINATIONS; i++)
		if (GPU_MATERIAL_STATE.cached_shaders[i])
			GPU_shader_free(GPU_MATERIAL_STATE.cached_shaders[i]);
}

/* Shader lookup / create */

static bool solid_compatible_lighting(void)
{
	int enabled = GPU_MATERIAL_STATE.lights_enabled;
	int directional = GPU_MATERIAL_STATE.lights_directional;

	/* more than 3 lights? */
	if (enabled >= (1 << 3))
		return false;

	/* all directional? */
	return ((directional & enabled) == enabled);
}

#if 0
static int detect_options()
{
	GLint two_sided;
	int options = 0;

	if (glIsEnabled(GL_TEXTURE_2D))
		options |= GPU_SHADER_TEXTURE_2D;
	if (glIsEnabled(GL_COLOR_MATERIAL))
		options |= GPU_SHADER_OVERRIDE_DIFFUSE;

	if (glIsEnabled(GL_LIGHTING))
		options |= GPU_SHADER_LIGHTING;

	glGetIntegerv(GL_LIGHT_MODEL_TWO_SIDE, &two_sided);
	if (two_sided == GL_TRUE)
		options |= GPU_SHADER_TWO_SIDED;
	
	return options;
}
#endif

static GPUShader *gpu_simple_shader(int options)
{
	/* glsl code */
	extern char datatoc_gpu_shader_simple_vert_glsl[];
	extern char datatoc_gpu_shader_simple_frag_glsl[];
	GPUShader *shader;

	/* detect if we can do faster lighting for solid draw mode */
	if (options & GPU_SHADER_LIGHTING)
		if (solid_compatible_lighting())
			options |= GPU_SHADER_SOLID_LIGHTING;

	/* cached shaders */
	shader = GPU_MATERIAL_STATE.cached_shaders[options];

	if (!shader && !GPU_MATERIAL_STATE.failed_shaders[options]) {
		/* create shader if it doesn't exist yet */
		char defines[64*GPU_SHADER_OPTIONS_NUM] = "";

		if (options & GPU_SHADER_OVERRIDE_DIFFUSE)
			strcat(defines, "#define USE_COLOR\n");
		if (options & GPU_SHADER_TWO_SIDED)
			strcat(defines, "#define USE_TWO_SIDED\n");
		if (options & GPU_SHADER_TEXTURE_2D)
			strcat(defines, "#define USE_TEXTURE\n");

		if (options & GPU_SHADER_SOLID_LIGHTING)
			strcat(defines, "#define USE_SOLID_LIGHTING\n");
		else if (options & GPU_SHADER_LIGHTING)
			strcat(defines, "#define USE_SCENE_LIGHTING\n");

		shader = GPU_shader_create(
			datatoc_gpu_shader_simple_vert_glsl,
			datatoc_gpu_shader_simple_frag_glsl,
			NULL, defines);
		
		if (shader) {
			/* set texture map to first texture unit */
			if (options & GPU_SHADER_TEXTURE_2D)
				glUniform1i(GPU_shader_get_uniform(shader, "texture_map"), 0);

			GPU_MATERIAL_STATE.cached_shaders[options] = shader;
		}
		else
			GPU_MATERIAL_STATE.failed_shaders[options] = true;
	}

	return shader;
}

/* Bind / unbind */

void GPU_simple_shader_bind(int options)
{
	if (GPU_glsl_support()) {
		GPUShader *shader = gpu_simple_shader(options);

		if (shader)
			GPU_shader_bind(shader);
	}
	else {
		// XXX where does this fit, depends on ortho/persp?

		if (options & GPU_SHADER_LIGHTING)
			glEnable(GL_LIGHTING);

		if (options & GPU_SHADER_TWO_SIDED)
			glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

		if (options & GPU_SHADER_OVERRIDE_DIFFUSE) {
			glEnable(GL_COLOR_MATERIAL);
			glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
		}

		if (options & GPU_SHADER_TEXTURE_2D)
			glEnable(GL_TEXTURE_2D);
	}

	/* temporary hack, should be solved outside of this file */
	GPU_MATERIAL_STATE.need_normals = (options & GPU_SHADER_LIGHTING);
}

void GPU_simple_shader_unbind(void)
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

void GPU_simple_shader_colors(const float diffuse[3], const float specular[3],
	int shininess, float alpha)
{
	float gl_diffuse[4], gl_specular[4];

	copy_v3_v3(gl_diffuse, diffuse);
	gl_diffuse[3] = alpha;

	copy_v3_v3(gl_specular, specular);
	gl_specular[3] = 1.0f;

	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, gl_diffuse);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, gl_specular);
	glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, CLAMPIS(shininess, 1, 128));
}

bool GPU_simple_shader_need_normals(void)
{
	return GPU_MATERIAL_STATE.need_normals;
}

void GPU_simple_shader_light_set(int light_num, GPULightData *light)
{
	int light_bit = (1 << light_num);

	GPU_MATERIAL_STATE.lights_enabled &= ~light_bit;
	GPU_MATERIAL_STATE.lights_directional &= ~light_bit;

	if (light) {
		glEnable(GL_LIGHT0+light_num);

		glLightfv(GL_LIGHT0+light_num, GL_POSITION, light->position); 
		glLightfv(GL_LIGHT0+light_num, GL_DIFFUSE, light->diffuse);
		glLightfv(GL_LIGHT0+light_num, GL_SPECULAR, light->specular);

		glLightf(GL_LIGHT0+light_num, GL_CONSTANT_ATTENUATION, light->constant_attenuation);
		glLightf(GL_LIGHT0+light_num, GL_LINEAR_ATTENUATION, light->linear_attenuation);
		glLightf(GL_LIGHT0+light_num, GL_QUADRATIC_ATTENUATION, light->quadratic_attenuation);

		glLightfv(GL_LIGHT0+light_num, GL_SPOT_DIRECTION, light->spot_direction);
		glLightf(GL_LIGHT0+light_num, GL_SPOT_CUTOFF, light->spot_cutoff);
		glLightf(GL_LIGHT0+light_num, GL_SPOT_EXPONENT, light->spot_exponent);

		GPU_MATERIAL_STATE.lights_enabled |= light_bit;
		if (light->position[3] == 0.0f)
			GPU_MATERIAL_STATE.lights_directional |= light_bit;
	}
	else {
		const float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};

		glLightfv(GL_LIGHT0+light_num, GL_POSITION, zero); 
		glLightfv(GL_LIGHT0+light_num, GL_DIFFUSE, zero); 
		glLightfv(GL_LIGHT0+light_num, GL_SPECULAR, zero);

		glDisable(GL_LIGHT0+light_num);
	}
}

void GPU_simple_shader_light_set_viewer(bool local)
{
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, (local)? GL_TRUE: GL_FALSE);
}

