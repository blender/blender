/*
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
 * The Original Code is Copyright (C) 2021 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup draw
 */

#pragma once

#include "draw_hair_private.h"

#ifdef __cplusplus
extern "C" {
#endif

struct GPUShader;

typedef enum eParticleRefineShaderType {
  PART_REFINE_SHADER_TRANSFORM_FEEDBACK,
  PART_REFINE_SHADER_TRANSFORM_FEEDBACK_WORKAROUND,
  PART_REFINE_SHADER_COMPUTE,
} eParticleRefineShaderType;

/* draw_shader.c */
struct GPUShader *DRW_shader_hair_refine_get(ParticleRefineShader refinement,
                                             eParticleRefineShaderType sh_type);
void DRW_shaders_free(void);

#ifdef __cplusplus
}
#endif
