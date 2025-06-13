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
  BUFFER_PAINT_OVERLAY_FLAG = 7,
  PATCH_EVALUATION = 8,
  PATCH_EVALUATION_FVAR = 9,
  PATCH_EVALUATION_FACE_DOTS = 10,
  PATCH_EVALUATION_FACE_DOTS_WITH_NORMALS = 11,
  PATCH_EVALUATION_ORCO = 12,
  COMP_CUSTOM_DATA_INTERP = 13,
  BUFFER_SCULPT_DATA = 14,
  BUFFER_UV_STRETCH_ANGLE = 15,
  BUFFER_UV_STRETCH_AREA = 16,
};
constexpr int SUBDIVISION_MAX_SHADERS = 17;

GPUShader *DRW_shader_subdiv_get(SubdivShaderType shader_type);
GPUShader *DRW_shader_subdiv_custom_data_get(GPUVertCompType comp_type, int dimensions);
GPUShader *DRW_shader_subdiv_interp_corner_normals_get();

void DRW_shaders_free();
