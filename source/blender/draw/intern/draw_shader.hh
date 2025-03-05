/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "draw_curves_private.hh"
#include "draw_hair_private.hh"

struct GPUShader;

/* draw_shader.cc */

GPUShader *DRW_shader_hair_refine_get(ParticleRefineShader refinement);

GPUShader *DRW_shader_curves_refine_get(blender::draw::CurvesEvalShader type);

GPUShader *DRW_shader_debug_draw_display_get();
GPUShader *DRW_shader_draw_visibility_compute_get();
GPUShader *DRW_shader_draw_view_finalize_get();
GPUShader *DRW_shader_draw_resource_finalize_get();
GPUShader *DRW_shader_draw_command_generate_get();

/* Subdivision */
enum class SubdivShaderType {
  BUFFER_LINES = 0,
  BUFFER_LINES_LOOSE = 1,
  BUFFER_EDGE_FAC = 2,
  BUFFER_LNOR = 3,
  BUFFER_TRIS = 4,
  BUFFER_TRIS_MULTIPLE_MATERIALS = 5,
  BUFFER_NORMALS_ACCUMULATE = 6,
  BUFFER_NORMALS_FINALIZE = 7,
  BUFFER_CUSTOM_NORMALS_FINALIZE = 8,
  PATCH_EVALUATION = 9,
  PATCH_EVALUATION_FVAR = 10,
  PATCH_EVALUATION_FACE_DOTS = 11,
  PATCH_EVALUATION_FACE_DOTS_WITH_NORMALS = 12,
  PATCH_EVALUATION_ORCO = 13,
  COMP_CUSTOM_DATA_INTERP = 14,
  BUFFER_SCULPT_DATA = 15,
  BUFFER_UV_STRETCH_ANGLE = 16,
  BUFFER_UV_STRETCH_AREA = 17,
};
constexpr int SUBDIVISION_MAX_SHADERS = 18;

GPUShader *DRW_shader_subdiv_get(SubdivShaderType shader_type);
GPUShader *DRW_shader_subdiv_custom_data_get(GPUVertCompType comp_type, int dimensions);

void DRW_shaders_free();
