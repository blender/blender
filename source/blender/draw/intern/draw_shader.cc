/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. */

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
extern "C" char datatoc_common_hair_refine_comp_glsl[];
extern "C" char datatoc_gpu_shader_3D_smooth_color_frag_glsl[];

static struct {
  struct GPUShader *hair_refine_sh[PART_REFINE_MAX_SHADER];
} e_data = {{nullptr}};

/* -------------------------------------------------------------------- */
/** \name Hair refinement
 * \{ */

static GPUShader *hair_refine_shader_compute_create(ParticleRefineShader UNUSED(refinement))
{
  return GPU_shader_create_from_info_name("draw_hair_refine_compute");
}

static GPUShader *hair_refine_shader_transform_feedback_create(
    ParticleRefineShader UNUSED(refinement))
{
  GPUShader *sh = nullptr;

  std::string shader_src = std::string(datatoc_common_hair_lib_glsl) +
                           std::string(datatoc_common_hair_refine_vert_glsl);

  const char *var_names[1] = {"finalColor"};
  sh = DRW_shader_create_with_transform_feedback(shader_src.c_str(),
                                                 nullptr,
                                                 "#define HAIR_PHASE_SUBDIV\n"
                                                 "#define USE_TF\n",
                                                 GPU_SHADER_TFB_POINTS,
                                                 var_names,
                                                 1);

  return sh;
}

static GPUShader *hair_refine_shader_transform_feedback_workaround_create(
    ParticleRefineShader UNUSED(refinement))
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

/** \} */

void DRW_shaders_free()
{
  for (int i = 0; i < PART_REFINE_MAX_SHADER; i++) {
    DRW_SHADER_FREE_SAFE(e_data.hair_refine_sh[i]);
  }
}
