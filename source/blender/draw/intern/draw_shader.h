/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup draw
 */

#pragma once

#include "draw_curves_private.h"
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

/* draw_shader.cc */

struct GPUShader *DRW_shader_hair_refine_get(ParticleRefineShader refinement,
                                             eParticleRefineShaderType sh_type);

struct GPUShader *DRW_shader_curves_refine_get(CurvesEvalShader type,
                                               eParticleRefineShaderType sh_type);

struct GPUShader *DRW_shader_debug_print_display_get(void);
struct GPUShader *DRW_shader_debug_draw_display_get(void);

void DRW_shaders_free(void);

#ifdef __cplusplus
}
#endif
