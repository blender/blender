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
 * The Original Code is Copyright (C) 2017 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup draw
 *
 * \brief Contains procedural GPU hair drawing methods.
 */

#include "DRW_render.h"

#include "BLI_utildefines.h"
#include "BLI_string_utils.h"

#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_customdata_types.h"

#include "BKE_anim.h"

#include "GPU_batch.h"
#include "GPU_shader.h"

#include "draw_hair_private.h"

#ifndef __APPLE__
#  define USE_TRANSFORM_FEEDBACK
#endif

typedef enum ParticleRefineShader {
  PART_REFINE_CATMULL_ROM = 0,
  PART_REFINE_MAX_SHADER,
} ParticleRefineShader;

#ifndef USE_TRANSFORM_FEEDBACK
typedef struct ParticleRefineCall {
  struct ParticleRefineCall *next;
  GPUVertBuf *vbo;
  DRWShadingGroup *shgrp;
  uint vert_len;
} ParticleRefineCall;

static ParticleRefineCall *g_tf_calls = NULL;
static int g_tf_id_offset;
static int g_tf_target_width;
static int g_tf_target_height;
#endif

static GPUShader *g_refine_shaders[PART_REFINE_MAX_SHADER] = {NULL};
static DRWPass *g_tf_pass; /* XXX can be a problem with multiple DRWManager in the future */

extern char datatoc_common_hair_lib_glsl[];
extern char datatoc_common_hair_refine_vert_glsl[];
extern char datatoc_gpu_shader_3D_smooth_color_frag_glsl[];

static GPUShader *hair_refine_shader_get(ParticleRefineShader sh)
{
  if (g_refine_shaders[sh]) {
    return g_refine_shaders[sh];
  }

  char *vert_with_lib = BLI_string_joinN(datatoc_common_hair_lib_glsl,
                                         datatoc_common_hair_refine_vert_glsl);

#ifdef USE_TRANSFORM_FEEDBACK
  const char *var_names[1] = {"finalColor"};
  g_refine_shaders[sh] = DRW_shader_create_with_transform_feedback(
      vert_with_lib, NULL, "#define HAIR_PHASE_SUBDIV\n", GPU_SHADER_TFB_POINTS, var_names, 1);
#else
  g_refine_shaders[sh] = DRW_shader_create(vert_with_lib,
                                           NULL,
                                           datatoc_gpu_shader_3D_smooth_color_frag_glsl,
                                           "#define HAIR_PHASE_SUBDIV\n"
                                           "#define TF_WORKAROUND\n");
#endif

  MEM_freeN(vert_with_lib);

  return g_refine_shaders[sh];
}

void DRW_hair_init(void)
{
#ifdef USE_TRANSFORM_FEEDBACK
  g_tf_pass = DRW_pass_create("Update Hair Pass", DRW_STATE_TRANS_FEEDBACK);
#else
  g_tf_pass = DRW_pass_create("Update Hair Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_POINT);
#endif
}

typedef struct DRWHairInstanceData {
  DrawData dd;

  float mat[4][4];
} DRWHairInstanceData;

static DRWShadingGroup *drw_shgroup_create_hair_procedural_ex(Object *object,
                                                              ParticleSystem *psys,
                                                              ModifierData *md,
                                                              DRWPass *hair_pass,
                                                              struct GPUMaterial *gpu_mat,
                                                              GPUShader *gpu_shader)
{
  /* TODO(fclem): Pass the scene as parameter */
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  static float unit_mat[4][4] = {
      {1, 0, 0, 0},
      {0, 1, 0, 0},
      {0, 0, 1, 0},
      {0, 0, 0, 1},
  };
  float(*dupli_mat)[4];
  Object *dupli_parent = DRW_object_get_dupli_parent(object);
  DupliObject *dupli_object = DRW_object_get_dupli(object);

  int subdiv = scene->r.hair_subdiv;
  int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

  ParticleHairCache *hair_cache;
  ParticleSettings *part = psys->part;
  bool need_ft_update = particles_ensure_procedural_data(
      object, psys, md, &hair_cache, subdiv, thickness_res);

  DRWShadingGroup *shgrp;
  if (gpu_mat) {
    shgrp = DRW_shgroup_material_create(gpu_mat, hair_pass);
  }
  else if (gpu_shader) {
    shgrp = DRW_shgroup_create(gpu_shader, hair_pass);
  }
  else {
    shgrp = NULL;
    BLI_assert(0);
  }

  /* TODO optimize this. Only bind the ones GPUMaterial needs. */
  for (int i = 0; i < hair_cache->num_uv_layers; ++i) {
    for (int n = 0; n < MAX_LAYER_NAME_CT && hair_cache->uv_layer_names[i][n][0] != '\0'; ++n) {
      DRW_shgroup_uniform_texture(shgrp, hair_cache->uv_layer_names[i][n], hair_cache->uv_tex[i]);
    }
  }
  for (int i = 0; i < hair_cache->num_col_layers; ++i) {
    for (int n = 0; n < MAX_LAYER_NAME_CT && hair_cache->col_layer_names[i][n][0] != '\0'; ++n) {
      DRW_shgroup_uniform_texture(
          shgrp, hair_cache->col_layer_names[i][n], hair_cache->col_tex[i]);
    }
  }

  if ((dupli_parent != NULL) && (dupli_object != NULL)) {
    DRWHairInstanceData *hair_inst_data = (DRWHairInstanceData *)DRW_drawdata_ensure(
        &object->id,
        (DrawEngineType *)&drw_shgroup_create_hair_procedural_ex,
        sizeof(DRWHairInstanceData),
        NULL,
        NULL);
    dupli_mat = hair_inst_data->mat;
    if (dupli_object->type & OB_DUPLICOLLECTION) {
      copy_m4_m4(dupli_mat, dupli_parent->obmat);
    }
    else {
      copy_m4_m4(dupli_mat, dupli_object->ob->obmat);
      invert_m4(dupli_mat);
      mul_m4_m4m4(dupli_mat, object->obmat, dupli_mat);
    }
  }
  else {
    dupli_mat = unit_mat;
  }

  DRW_shgroup_uniform_texture(shgrp, "hairPointBuffer", hair_cache->final[subdiv].proc_tex);
  DRW_shgroup_uniform_int(shgrp, "hairStrandsRes", &hair_cache->final[subdiv].strands_res, 1);
  DRW_shgroup_uniform_int_copy(shgrp, "hairThicknessRes", thickness_res);
  DRW_shgroup_uniform_float(shgrp, "hairRadShape", &part->shape, 1);
  DRW_shgroup_uniform_mat4(shgrp, "hairDupliMatrix", dupli_mat);
  DRW_shgroup_uniform_float_copy(shgrp, "hairRadRoot", part->rad_root * part->rad_scale * 0.5f);
  DRW_shgroup_uniform_float_copy(shgrp, "hairRadTip", part->rad_tip * part->rad_scale * 0.5f);
  DRW_shgroup_uniform_bool_copy(
      shgrp, "hairCloseTip", (part->shape_flag & PART_SHAPE_CLOSE_TIP) != 0);
  /* TODO(fclem): Until we have a better way to cull the hair and render with orco, bypass culling
   * test. */
  DRW_shgroup_call_object_no_cull(
      shgrp, hair_cache->final[subdiv].proc_hairs[thickness_res - 1], object);

  /* Transform Feedback subdiv. */
  if (need_ft_update) {
    int final_points_len = hair_cache->final[subdiv].strands_res * hair_cache->strands_len;
    GPUShader *tf_shader = hair_refine_shader_get(PART_REFINE_CATMULL_ROM);

#ifdef USE_TRANSFORM_FEEDBACK
    DRWShadingGroup *tf_shgrp = DRW_shgroup_transform_feedback_create(
        tf_shader, g_tf_pass, hair_cache->final[subdiv].proc_buf);
#else
    DRWShadingGroup *tf_shgrp = DRW_shgroup_create(tf_shader, g_tf_pass);

    ParticleRefineCall *pr_call = MEM_mallocN(sizeof(*pr_call), __func__);
    pr_call->next = g_tf_calls;
    pr_call->vbo = hair_cache->final[subdiv].proc_buf;
    pr_call->shgrp = tf_shgrp;
    pr_call->vert_len = final_points_len;
    g_tf_calls = pr_call;
    DRW_shgroup_uniform_int(tf_shgrp, "targetHeight", &g_tf_target_height, 1);
    DRW_shgroup_uniform_int(tf_shgrp, "targetWidth", &g_tf_target_width, 1);
    DRW_shgroup_uniform_int(tf_shgrp, "idOffset", &g_tf_id_offset, 1);
#endif

    DRW_shgroup_uniform_texture(tf_shgrp, "hairPointBuffer", hair_cache->point_tex);
    DRW_shgroup_uniform_texture(tf_shgrp, "hairStrandBuffer", hair_cache->strand_tex);
    DRW_shgroup_uniform_texture(tf_shgrp, "hairStrandSegBuffer", hair_cache->strand_seg_tex);
    DRW_shgroup_uniform_int(tf_shgrp, "hairStrandsRes", &hair_cache->final[subdiv].strands_res, 1);
    DRW_shgroup_call_procedural_points(tf_shgrp, final_points_len, NULL);
  }

  return shgrp;
}

DRWShadingGroup *DRW_shgroup_hair_create(
    Object *object, ParticleSystem *psys, ModifierData *md, DRWPass *hair_pass, GPUShader *shader)
{
  return drw_shgroup_create_hair_procedural_ex(object, psys, md, hair_pass, NULL, shader);
}

DRWShadingGroup *DRW_shgroup_material_hair_create(Object *object,
                                                  ParticleSystem *psys,
                                                  ModifierData *md,
                                                  DRWPass *hair_pass,
                                                  struct GPUMaterial *material)
{
  return drw_shgroup_create_hair_procedural_ex(object, psys, md, hair_pass, material, NULL);
}

void DRW_hair_update(void)
{
#ifndef USE_TRANSFORM_FEEDBACK
  /**
   * Workaround to tranform feedback not working on mac.
   * On some system it crashes (see T58489) and on some other it renders garbage (see T60171).
   *
   * So instead of using transform feedback we render to a texture,
   * readback the result to system memory and reupload as VBO data.
   * It is really not ideal performance wise, but it is the simplest
   * and the most local workaround that still uses the power of the GPU.
   */

  if (g_tf_calls == NULL) {
    return;
  }

  /* Search ideal buffer size. */
  uint max_size = 0;
  for (ParticleRefineCall *pr_call = g_tf_calls; pr_call; pr_call = pr_call->next) {
    max_size = max_ii(max_size, pr_call->vert_len);
  }

  /* Create target Texture / Framebuffer */
  /* Don't use max size as it can be really heavy and fail.
   * Do chunks of maximum 2048 * 2048 hair points. */
  int width = 2048;
  int height = min_ii(width, 1 + max_size / width);
  GPUTexture *tex = DRW_texture_pool_query_2d(width, height, GPU_RGBA32F, (void *)DRW_hair_update);
  g_tf_target_height = height;
  g_tf_target_width = width;

  GPUFrameBuffer *fb = NULL;
  GPU_framebuffer_ensure_config(&fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(tex),
                                });

  float *data = MEM_mallocN(sizeof(float) * 4 * width * height, "tf fallback buffer");

  GPU_framebuffer_bind(fb);
  while (g_tf_calls != NULL) {
    ParticleRefineCall *pr_call = g_tf_calls;
    g_tf_calls = g_tf_calls->next;

    g_tf_id_offset = 0;
    while (pr_call->vert_len > 0) {
      int max_read_px_len = min_ii(width * height, pr_call->vert_len);

      DRW_draw_pass_subset(g_tf_pass, pr_call->shgrp, pr_call->shgrp);
      /* Readback result to main memory. */
      GPU_framebuffer_read_color(fb, 0, 0, width, height, 4, 0, data);
      /* Upload back to VBO. */
      GPU_vertbuf_use(pr_call->vbo);
      glBufferSubData(GL_ARRAY_BUFFER,
                      sizeof(float) * 4 * g_tf_id_offset,
                      sizeof(float) * 4 * max_read_px_len,
                      data);

      g_tf_id_offset += max_read_px_len;
      pr_call->vert_len -= max_read_px_len;
    }

    MEM_freeN(pr_call);
  }

  MEM_freeN(data);
  GPU_framebuffer_free(fb);
#else
  /* TODO(fclem): replace by compute shader. */
  /* Just render using transform feedback. */
  DRW_draw_pass(g_tf_pass);
#endif
}

void DRW_hair_free(void)
{
  for (int i = 0; i < PART_REFINE_MAX_SHADER; ++i) {
    DRW_SHADER_FREE_SAFE(g_refine_shaders[i]);
  }
}
