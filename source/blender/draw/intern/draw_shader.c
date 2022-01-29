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
 * Copyright 2020, Blender Foundation.
 */

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

extern char datatoc_common_hair_lib_glsl[];

extern char datatoc_common_hair_refine_vert_glsl[];
extern char datatoc_common_hair_refine_comp_glsl[];
extern char datatoc_gpu_shader_3D_smooth_color_frag_glsl[];

static struct {
  struct GPUShader *hair_refine_sh[PART_REFINE_MAX_SHADER];
} e_data = {{NULL}};

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
  GPUShader *sh = NULL;

  char *shader_src = BLI_string_joinN(datatoc_common_hair_lib_glsl,
                                      datatoc_common_hair_refine_vert_glsl);
  const char *var_names[1] = {"finalColor"};
  sh = DRW_shader_create_with_transform_feedback(
      shader_src, NULL, "#define HAIR_PHASE_SUBDIV\n", GPU_SHADER_TFB_POINTS, var_names, 1);
  MEM_freeN(shader_src);

  return sh;
}

static GPUShader *hair_refine_shader_transform_feedback_workaround_create(
    ParticleRefineShader UNUSED(refinement))
{
  GPUShader *sh = NULL;

  char *shader_src = BLI_string_joinN(datatoc_common_hair_lib_glsl,
                                      datatoc_common_hair_refine_vert_glsl);
  sh = DRW_shader_create(shader_src,
                         NULL,
                         datatoc_gpu_shader_3D_smooth_color_frag_glsl,
                         "#define blender_srgb_to_framebuffer_space(a) a\n"
                         "#define HAIR_PHASE_SUBDIV\n"
                         "#define TF_WORKAROUND\n");
  MEM_freeN(shader_src);

  return sh;
}

GPUShader *DRW_shader_hair_refine_get(ParticleRefineShader refinement,
                                      eParticleRefineShaderType sh_type)
{
  if (e_data.hair_refine_sh[refinement] == NULL) {
    GPUShader *sh = NULL;
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

/** \} */

void DRW_shaders_free(void)
{
  for (int i = 0; i < PART_REFINE_MAX_SHADER; i++) {
    DRW_SHADER_FREE_SAFE(e_data.hair_refine_sh[i]);
  }
}
