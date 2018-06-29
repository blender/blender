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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_basic_shader.h
 *  \ingroup gpu
 */

#ifndef __GPU_BASIC_SHADER_H__
#define __GPU_BASIC_SHADER_H__

#include "BLI_utildefines.h"
#include "GPU_glew.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed Function Shader */

typedef enum GPUBasicShaderOption {
	GPU_SHADER_USE_COLOR =        (1 << 0),   /* use glColor, for lighting it replaces diffuse */
	GPU_SHADER_LIGHTING =         (1 << 1),   /* use lighting */
	GPU_SHADER_TWO_SIDED =        (1 << 2),   /* flip normals towards viewer */
	GPU_SHADER_TEXTURE_2D =       (1 << 3),   /* use 2D texture to replace diffuse color */
	GPU_SHADER_TEXTURE_RECT =     (1 << 4),   /* same as GPU_SHADER_TEXTURE_2D, for GL_TEXTURE_RECTANGLE */

	GPU_SHADER_SOLID_LIGHTING =   (1 << 5),   /* use faster lighting (set automatically) */
	GPU_SHADER_STIPPLE =          (1 << 6),   /* use stipple */
	GPU_SHADER_LINE =             (1 << 7),   /* draw lines */
	GPU_SHADER_FLAT_NORMAL =      (1 << 8),   /* use flat normals */
	GPU_SHADER_OPTIONS_NUM = 9,
	GPU_SHADER_OPTION_COMBINATIONS = (1 << GPU_SHADER_OPTIONS_NUM)
} GPUBasicShaderOption;

/* Keep these in sync with gpu_shader_basic_frag.glsl */
typedef enum GPUBasicShaderStipple {
	GPU_SHADER_STIPPLE_HALFTONE                        = 0,
	GPU_SHADER_STIPPLE_QUARTTONE                       = 1,
	GPU_SHADER_STIPPLE_CHECKER_8PX                     = 2,
	GPU_SHADER_STIPPLE_HEXAGON                         = 3,
	GPU_SHADER_STIPPLE_DIAG_STRIPES                    = 4,
	GPU_SHADER_STIPPLE_DIAG_STRIPES_SWAP               = 5,
	GPU_SHADER_STIPPLE_S3D_INTERLACE_ROW               = 6,
	GPU_SHADER_STIPPLE_S3D_INTERLACE_ROW_SWAP          = 7,
	GPU_SHADER_STIPPLE_S3D_INTERLACE_COLUMN            = 8,
	GPU_SHADER_STIPPLE_S3D_INTERLACE_COLUMN_SWAP       = 9,
	GPU_SHADER_STIPPLE_S3D_INTERLACE_CHECKER           = 10,
	GPU_SHADER_STIPPLE_S3D_INTERLACE_CHECKER_SWAP      = 11
} GPUBasicShaderStipple;

void GPU_basic_shaders_init(void);
void GPU_basic_shaders_exit(void);

void GPU_basic_shader_bind(int options);
void GPU_basic_shader_bind_enable(int options);
void GPU_basic_shader_bind_disable(int options);

int GPU_basic_shader_bound_options(void);

/* Only use for small blocks of code that don't support glsl shader. */
#define GPU_BASIC_SHADER_DISABLE_AND_STORE(bound_options) \
if (GPU_basic_shader_use_glsl_get()) { \
	if ((bound_options = GPU_basic_shader_bound_options())) { \
		GPU_basic_shader_bind(0); \
	} \
} \
else { bound_options = 0; } ((void)0)
#define GPU_BASIC_SHADER_ENABLE_AND_RESTORE(bound_options) \
if (GPU_basic_shader_use_glsl_get()) { \
	if (bound_options) { \
		GPU_basic_shader_bind(bound_options); \
	} \
} ((void)0)


void GPU_basic_shader_colors(
        const float diffuse[3], const float specular[3],
        int shininess, float alpha);

/* Fixed Function Lighting */

typedef enum GPULightType {
	GPU_LIGHT_POINT,
	GPU_LIGHT_SPOT,
	GPU_LIGHT_SUN
} GPULightType;

typedef struct GPULightData {
	GPULightType type;

	float position[3];
	float direction[3];

	float diffuse[3];
	float specular[3];

	float constant_attenuation;
	float linear_attenuation;
	float quadratic_attenuation;

	float spot_cutoff;
	float spot_exponent;
} GPULightData;

void GPU_basic_shader_light_set(int light_num, GPULightData *light);
void GPU_basic_shader_light_set_viewer(bool local);
void GPU_basic_shader_stipple(GPUBasicShaderStipple stipple_id);
void GPU_basic_shader_line_stipple(GLint stipple_factor, GLushort stipple_pattern);
void GPU_basic_shader_line_width(float line_width);

bool GPU_basic_shader_use_glsl_get(void);
void GPU_basic_shader_use_glsl_set(bool enabled);

#ifdef __cplusplus
}
#endif

#endif
