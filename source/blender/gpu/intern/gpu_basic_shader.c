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

/** \file blender/gpu/intern/gpu_basic_shader.c
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

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "GPU_basic_shader.h"
#include "GPU_glew.h"
#include "GPU_shader.h"

/* State */

static struct {
	GPUShader *cached_shaders[GPU_SHADER_OPTION_COMBINATIONS];
	bool failed_shaders[GPU_SHADER_OPTION_COMBINATIONS];

	int bound_options;

	int lights_enabled;
	int lights_directional;
	float line_width;
	GLint viewport[4];
} GPU_MATERIAL_STATE;


/* Stipple patterns */
/* ******************************************** */
const GLubyte stipple_halftone[128] = {
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
};

const GLubyte stipple_quarttone[128] = {
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
};

const GLubyte stipple_diag_stripes_pos[128] = {
	0x00, 0xff, 0x00, 0xff, 0x01, 0xfe, 0x01, 0xfe,
	0x03, 0xfc, 0x03, 0xfc, 0x07, 0xf8, 0x07, 0xf8,
	0x0f, 0xf0, 0x0f, 0xf0, 0x1f, 0xe0, 0x1f, 0xe0,
	0x3f, 0xc0, 0x3f, 0xc0, 0x7f, 0x80, 0x7f, 0x80,
	0xff, 0x00, 0xff, 0x00, 0xfe, 0x01, 0xfe, 0x01,
	0xfc, 0x03, 0xfc, 0x03, 0xf8, 0x07, 0xf8, 0x07,
	0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f,
	0xc0, 0x3f, 0xc0, 0x3f, 0x80, 0x7f, 0x80, 0x7f,
	0x00, 0xff, 0x00, 0xff, 0x01, 0xfe, 0x01, 0xfe,
	0x03, 0xfc, 0x03, 0xfc, 0x07, 0xf8, 0x07, 0xf8,
	0x0f, 0xf0, 0x0f, 0xf0, 0x1f, 0xe0, 0x1f, 0xe0,
	0x3f, 0xc0, 0x3f, 0xc0, 0x7f, 0x80, 0x7f, 0x80,
	0xff, 0x00, 0xff, 0x00, 0xfe, 0x01, 0xfe, 0x01,
	0xfc, 0x03, 0xfc, 0x03, 0xf8, 0x07, 0xf8, 0x07,
	0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f,
	0xc0, 0x3f, 0xc0, 0x3f, 0x80, 0x7f, 0x80, 0x7f,
};

const GLubyte stipple_diag_stripes_neg[128] = {
	0xff, 0x00, 0xff, 0x00, 0xfe, 0x01, 0xfe, 0x01,
	0xfc, 0x03, 0xfc, 0x03, 0xf8, 0x07, 0xf8, 0x07,
	0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f,
	0xc0, 0x3f, 0xc0, 0x3f, 0x80, 0x7f, 0x80, 0x7f,
	0x00, 0xff, 0x00, 0xff, 0x01, 0xfe, 0x01, 0xfe,
	0x03, 0xfc, 0x03, 0xfc, 0x07, 0xf8, 0x07, 0xf8,
	0x0f, 0xf0, 0x0f, 0xf0, 0x1f, 0xe0, 0x1f, 0xe0,
	0x3f, 0xc0, 0x3f, 0xc0, 0x7f, 0x80, 0x7f, 0x80,
	0xff, 0x00, 0xff, 0x00, 0xfe, 0x01, 0xfe, 0x01,
	0xfc, 0x03, 0xfc, 0x03, 0xf8, 0x07, 0xf8, 0x07,
	0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f,
	0xc0, 0x3f, 0xc0, 0x3f, 0x80, 0x7f, 0x80, 0x7f,
	0x00, 0xff, 0x00, 0xff, 0x01, 0xfe, 0x01, 0xfe,
	0x03, 0xfc, 0x03, 0xfc, 0x07, 0xf8, 0x07, 0xf8,
	0x0f, 0xf0, 0x0f, 0xf0, 0x1f, 0xe0, 0x1f, 0xe0,
	0x3f, 0xc0, 0x3f, 0xc0, 0x7f, 0x80, 0x7f, 0x80,
};

const GLubyte stipple_checker_8px[128] = {
	255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
	255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
	0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
	0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
	255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
	255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
	0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
	0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
};

const GLubyte stipple_hexagon[128] = {
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
};
/* ********************************************* */

/* Init / exit */

void GPU_basic_shaders_init(void)
{
	memset(&GPU_MATERIAL_STATE, 0, sizeof(GPU_MATERIAL_STATE));
}

void GPU_basic_shaders_exit(void)
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

static GPUShader *gpu_basic_shader(int options)
{
	/* glsl code */
	extern char datatoc_gpu_shader_basic_vert_glsl[];
	extern char datatoc_gpu_shader_basic_frag_glsl[];
	extern char datatoc_gpu_shader_basic_geom_glsl[];
	char *geom_glsl = NULL;
	GPUShader *shader;

	/* detect if we can do faster lighting for solid draw mode */
	if (options & GPU_SHADER_LIGHTING)
		if (solid_compatible_lighting())
			options |= GPU_SHADER_SOLID_LIGHTING;

	/* cached shaders */
	shader = GPU_MATERIAL_STATE.cached_shaders[options];

	if (!shader && !GPU_MATERIAL_STATE.failed_shaders[options]) {
		/* create shader if it doesn't exist yet */
		char defines[64 * GPU_SHADER_OPTIONS_NUM] = "";

		if (options & GPU_SHADER_USE_COLOR)
			strcat(defines, "#define USE_COLOR\n");
		if (options & GPU_SHADER_TWO_SIDED)
			strcat(defines, "#define USE_TWO_SIDED\n");
		if (options & (GPU_SHADER_TEXTURE_2D | GPU_SHADER_TEXTURE_RECT))
			strcat(defines, "#define USE_TEXTURE\n");
		if (options & GPU_SHADER_TEXTURE_RECT)
			strcat(defines, "#define USE_TEXTURE_RECTANGLE\n");
		if (options & GPU_SHADER_STIPPLE)
			strcat(defines, "#define USE_STIPPLE\n");
		if (options & GPU_SHADER_LINE) {
			strcat(defines, "#define DRAW_LINE\n");
			geom_glsl = datatoc_gpu_shader_basic_geom_glsl;
		}
		if (options & GPU_SHADER_FLAT_NORMAL)
			strcat(defines, "#define USE_FLAT_NORMAL\n");
		if (options & GPU_SHADER_SOLID_LIGHTING)
			strcat(defines, "#define USE_SOLID_LIGHTING\n");
		else if (options & GPU_SHADER_LIGHTING)
			strcat(defines, "#define USE_SCENE_LIGHTING\n");

		shader = GPU_shader_create(
			datatoc_gpu_shader_basic_vert_glsl,
			datatoc_gpu_shader_basic_frag_glsl,
			geom_glsl,
			NULL,
			defines,
			__func__);

		if (shader) {
			/* set texture map to first texture unit */
			if (options & (GPU_SHADER_TEXTURE_2D | GPU_SHADER_TEXTURE_RECT)) {
				GPU_shader_bind(shader);
				glUniform1i(GPU_shader_get_uniform(shader, "texture_map"), 0);
				GPU_shader_unbind();
			}

			GPU_MATERIAL_STATE.cached_shaders[options] = shader;
		}
		else
			GPU_MATERIAL_STATE.failed_shaders[options] = true;
	}

	return shader;
}

static void gpu_basic_shader_uniform_autoset(GPUShader *shader, int options)
{
	if (options & GPU_SHADER_LINE) {
		glGetIntegerv(GL_VIEWPORT, &GPU_MATERIAL_STATE.viewport[0]);
		glUniform4iv(GPU_shader_get_uniform(shader, "viewport"), 1, &GPU_MATERIAL_STATE.viewport[0]);
		glUniform1f(GPU_shader_get_uniform(shader, "line_width"), GPU_MATERIAL_STATE.line_width);
	}
}

/* Bind / unbind */

void GPU_basic_shader_bind(int options)
{
	if (options) {
		GPUShader *shader = gpu_basic_shader(options);

		if (shader) {
			GPU_shader_bind(shader);
			gpu_basic_shader_uniform_autoset(shader, options);
		}
	}
	else {
		GPU_shader_unbind();
	}

	GPU_MATERIAL_STATE.bound_options = options;
}

void GPU_basic_shader_bind_enable(int options)
{
	GPU_basic_shader_bind(GPU_MATERIAL_STATE.bound_options | options);
}

void GPU_basic_shader_bind_disable(int options)
{
	GPU_basic_shader_bind(GPU_MATERIAL_STATE.bound_options & ~options);
}

int GPU_basic_shader_bound_options(void)
{
	/* ideally this should disappear, anything that uses this is making fragile
	 * assumptions that the basic shader is bound and not another shader */
	return GPU_MATERIAL_STATE.bound_options;
}

/* Material Colors */

void GPU_basic_shader_colors(
        const float diffuse[3], const float specular[3],
        int shininess, float alpha)
{
	UNUSED_VARS(diffuse, specular, shininess, alpha);
	return;
}

void GPU_basic_shader_light_set(int light_num, GPULightData *light)
{
	UNUSED_VARS(light_num, light);
	return;
}

void GPU_basic_shader_light_set_viewer(bool local)
{
	UNUSED_VARS(local);
	return;
}

void GPU_basic_shader_stipple(GPUBasicShaderStipple stipple_id)
{
	glUniform1i(GPU_shader_get_uniform(gpu_basic_shader(GPU_MATERIAL_STATE.bound_options), "stipple_id"), stipple_id);
}

void GPU_basic_shader_line_width(float line_width)
{
	GPU_MATERIAL_STATE.line_width = line_width;
	if (GPU_MATERIAL_STATE.bound_options & GPU_SHADER_LINE) {
		glUniform1f(GPU_shader_get_uniform(gpu_basic_shader(GPU_MATERIAL_STATE.bound_options), "line_width"), line_width);
	}
}

void GPU_basic_shader_line_stipple(GLint stipple_factor, GLushort stipple_pattern)
{
	glUniform1i(GPU_shader_get_uniform(gpu_basic_shader(GPU_MATERIAL_STATE.bound_options), "stipple_factor"), stipple_factor);
	glUniform1i(GPU_shader_get_uniform(gpu_basic_shader(GPU_MATERIAL_STATE.bound_options), "stipple_pattern"), stipple_pattern);
}
