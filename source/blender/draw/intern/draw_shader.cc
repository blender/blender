/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "BLI_dynstr.h"
#include "BLI_string_utils.h"

#include "GPU_batch.h"
#include "GPU_index_buffer.h"
#include "GPU_vertex_buffer.h"

#include "draw_shader.h"

extern "C" char datatoc_common_hair_lib_glsl[];
extern "C" char datatoc_common_hair_refine_vert_glsl[];

static struct {
  GPUShader *hair_refine_sh[PART_REFINE_MAX_SHADER];
  GPUShader *debug_print_display_sh;
  GPUShader *debug_draw_display_sh;
  GPUShader *draw_visibility_compute_sh;
  GPUShader *draw_view_finalize_sh;
  GPUShader *draw_resource_finalize_sh;
  GPUShader *draw_command_generate_sh;
} e_data = {{nullptr}};

/* -------------------------------------------------------------------- */
/** \name Hair refinement
 * \{ */

static GPUShader *hair_refine_shader_compute_create(ParticleRefineShader /*refinement*/)
{
  return GPU_shader_create_from_info_name("draw_hair_refine_compute");
}

static GPUShader *hair_refine_shader_transform_feedback_create(ParticleRefineShader /*refinement*/)
{
  return GPU_shader_create_from_info_name("legacy_hair_refine_shader_transform_feedback");
}

static GPUShader *hair_refine_shader_transform_feedback_workaround_create(
    ParticleRefineShader /*refinement*/)
{
  return GPU_shader_create_from_info_name("draw_hair_refine_transform_feedback_workaround");
}

GPUShader *DRW_shader_hair_refine_get(ParticleRefineShader refinement,
                                      eParticleRefineShaderType sh_type)
{
  if (e_data.hair_refine_sh[refinement] == nullptr) {
    GPUShader *sh = nullptr;
    switch (sh_type) {
      case PART_REFINE_SHADER_COMPUTE:
        sh = hair_refine_shader_compute_create(refinement);
        break;
      case PART_REFINE_SHADER_TRANSFORM_FEEDBACK:
        sh = hair_refine_shader_transform_feedback_create(refinement);
        break;
      case PART_REFINE_SHADER_TRANSFORM_FEEDBACK_WORKAROUND:
        sh = hair_refine_shader_transform_feedback_workaround_create(refinement);
        break;
      default:
        BLI_assert_msg(0, "Incorrect shader type");
    }
    e_data.hair_refine_sh[refinement] = sh;
  }

  return e_data.hair_refine_sh[refinement];
}

GPUShader *DRW_shader_curves_refine_get(CurvesEvalShader type, eParticleRefineShaderType sh_type)
{
  /* TODO: Implement curves evaluation types (Bezier and Catmull Rom). */
  if (e_data.hair_refine_sh[type] == nullptr) {
    GPUShader *sh = nullptr;
    switch (sh_type) {
      case PART_REFINE_SHADER_COMPUTE:
        sh = hair_refine_shader_compute_create(PART_REFINE_CATMULL_ROM);
        break;
      case PART_REFINE_SHADER_TRANSFORM_FEEDBACK:
        sh = hair_refine_shader_transform_feedback_create(PART_REFINE_CATMULL_ROM);
        break;
      case PART_REFINE_SHADER_TRANSFORM_FEEDBACK_WORKAROUND:
        sh = hair_refine_shader_transform_feedback_workaround_create(PART_REFINE_CATMULL_ROM);
        break;
      default:
        BLI_assert_msg(0, "Incorrect shader type");
    }
    e_data.hair_refine_sh[type] = sh;
  }

  return e_data.hair_refine_sh[type];
}

GPUShader *DRW_shader_debug_print_display_get()
{
  if (e_data.debug_print_display_sh == nullptr) {
    e_data.debug_print_display_sh = GPU_shader_create_from_info_name("draw_debug_print_display");
  }
  return e_data.debug_print_display_sh;
}

GPUShader *DRW_shader_debug_draw_display_get()
{
  if (e_data.debug_draw_display_sh == nullptr) {
    e_data.debug_draw_display_sh = GPU_shader_create_from_info_name("draw_debug_draw_display");
  }
  return e_data.debug_draw_display_sh;
}

GPUShader *DRW_shader_draw_visibility_compute_get()
{
  if (e_data.draw_visibility_compute_sh == nullptr) {
    e_data.draw_visibility_compute_sh = GPU_shader_create_from_info_name(
        "draw_visibility_compute");
  }
  return e_data.draw_visibility_compute_sh;
}

GPUShader *DRW_shader_draw_view_finalize_get()
{
  if (e_data.draw_view_finalize_sh == nullptr) {
    e_data.draw_view_finalize_sh = GPU_shader_create_from_info_name("draw_view_finalize");
  }
  return e_data.draw_view_finalize_sh;
}

GPUShader *DRW_shader_draw_resource_finalize_get()
{
  if (e_data.draw_resource_finalize_sh == nullptr) {
    e_data.draw_resource_finalize_sh = GPU_shader_create_from_info_name("draw_resource_finalize");
  }
  return e_data.draw_resource_finalize_sh;
}

GPUShader *DRW_shader_draw_command_generate_get()
{
  if (e_data.draw_command_generate_sh == nullptr) {
    e_data.draw_command_generate_sh = GPU_shader_create_from_info_name("draw_command_generate");
  }
  return e_data.draw_command_generate_sh;
}

/** \} */

void DRW_shaders_free()
{
  for (int i = 0; i < PART_REFINE_MAX_SHADER; i++) {
    DRW_SHADER_FREE_SAFE(e_data.hair_refine_sh[i]);
  }
  DRW_SHADER_FREE_SAFE(e_data.debug_print_display_sh);
  DRW_SHADER_FREE_SAFE(e_data.debug_draw_display_sh);
  DRW_SHADER_FREE_SAFE(e_data.draw_visibility_compute_sh);
  DRW_SHADER_FREE_SAFE(e_data.draw_view_finalize_sh);
  DRW_SHADER_FREE_SAFE(e_data.draw_resource_finalize_sh);
  DRW_SHADER_FREE_SAFE(e_data.draw_command_generate_sh);
}
