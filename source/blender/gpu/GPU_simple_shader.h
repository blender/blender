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

/** \file GPU_simple_shader.h
 *  \ingroup gpu
 */

#ifndef __GPU_SIMPLE_SHADER_H__
#define __GPU_SIMPLE_SHADER_H__

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed Function Shader */

typedef enum GPUSimpleShaderOption {
	GPU_SHADER_OVERRIDE_DIFFUSE = (1<<0),   /* replace diffuse with glcolor */
	GPU_SHADER_LIGHTING = (1<<1),           /* use lighting */
	GPU_SHADER_TWO_SIDED = (1<<2),          /* flip normals towards viewer */
	GPU_SHADER_TEXTURE_2D = (1<<3),         /* use 2D texture to replace diffuse color */

	GPU_SHADER_SOLID_LIGHTING = (1<<4),     /* use faster lighting (set automatically) */
	GPU_SHADER_OPTIONS_NUM = 5,
	GPU_SHADER_OPTION_COMBINATIONS = (1<<GPU_SHADER_OPTIONS_NUM)
} GPUSimpleShaderOption;

void GPU_simple_shaders_init(void);
void GPU_simple_shaders_exit(void);

void GPU_simple_shader_bind(int options);
void GPU_simple_shader_unbind(void);

void GPU_simple_shader_colors(const float diffuse[3], const float specular[3],
	int shininess, float alpha);

bool GPU_simple_shader_need_normals(void);

/* Fixed Function Lighting */

typedef struct GPULightData {
	float position[4];
	float diffuse[4];
	float specular[4];

	float constant_attenuation;
	float linear_attenuation;
	float quadratic_attenuation;

	float spot_direction[3];
	float spot_cutoff;
	float spot_exponent;
} GPULightData;

void GPU_simple_shader_light_set(int light_num, GPULightData *light);
void GPU_simple_shader_light_set_viewer(bool local);

#ifdef __cplusplus
}
#endif

#endif

