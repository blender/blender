/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#ifdef __cplusplus
#  include "draw_curves_private.hh"
#  include "draw_hair_private.h"

struct GPUShader;

enum eParticleRefineShaderType {
  PART_REFINE_SHADER_TRANSFORM_FEEDBACK,
  PART_REFINE_SHADER_TRANSFORM_FEEDBACK_WORKAROUND,
  PART_REFINE_SHADER_COMPUTE,
};

/* draw_shader.cc */

GPUShader *DRW_shader_hair_refine_get(ParticleRefineShader refinement,
                                      eParticleRefineShaderType sh_type);

GPUShader *DRW_shader_curves_refine_get(CurvesEvalShader type, eParticleRefineShaderType sh_type);

GPUShader *DRW_shader_debug_print_display_get();
GPUShader *DRW_shader_debug_draw_display_get();
GPUShader *DRW_shader_draw_visibility_compute_get();
GPUShader *DRW_shader_draw_view_finalize_get();
GPUShader *DRW_shader_draw_resource_finalize_get();
GPUShader *DRW_shader_draw_command_generate_get();

#endif

#ifdef __cplusplus
extern "C" {
#endif

void DRW_shaders_free(void);

#ifdef __cplusplus
}
#endif
