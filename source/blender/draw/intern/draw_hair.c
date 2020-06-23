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

#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "DNA_customdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"

#include "BKE_duplilist.h"

#include "GPU_batch.h"
#include "GPU_shader.h"
#include "GPU_vertex_buffer.h"

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

static GPUVertBuf *g_dummy_vbo = NULL;
static GPUTexture *g_dummy_texture = NULL;
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
                                           "#define blender_srgb_to_framebuffer_space(a) a\n"
                                           "#define HAIR_PHASE_SUBDIV\n"
                                           "#define TF_WORKAROUND\n");
#endif

  MEM_freeN(vert_with_lib);

  return g_refine_shaders[sh];
}

void DRW_hair_init(void)
{
#ifdef USE_TRANSFORM_FEEDBACK
  g_tf_pass = DRW_pass_create("Update Hair Pass", 0);
#else
  g_tf_pass = DRW_pass_create("Update Hair Pass", DRW_STATE_WRITE_COLOR);
#endif

  if (g_dummy_vbo == NULL) {
    /* initialize vertex format */
    GPUVertFormat format = {0};
    uint dummy_id = GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

    g_dummy_vbo = GPU_vertbuf_create_with_format(&format);

    float vert[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GPU_vertbuf_data_alloc(g_dummy_vbo, 1);
    GPU_vertbuf_attr_fill(g_dummy_vbo, dummy_id, vert);
    /* Create vbo immediately to bind to texture buffer. */
    GPU_vertbuf_use(g_dummy_vbo);

    g_dummy_texture = GPU_texture_create_from_vertbuf(g_dummy_vbo);
  }
}

static ParticleHairCache *drw_hair_particle_cache_get(
    Object *object, ParticleSystem *psys, ModifierData *md, int subdiv, int thickness_res)
{
  bool update;
  ParticleHairCache *cache;
  if (psys) {
    /* Old particle hair. */
    update = particles_ensure_procedural_data(object, psys, md, &cache, subdiv, thickness_res);
  }
  else {
    /* New hair object. */
    update = hair_ensure_procedural_data(object, &cache, subdiv, thickness_res);
  }

  if (update) {
    int final_points_len = cache->final[subdiv].strands_res * cache->strands_len;
    if (final_points_len > 0) {
      GPUShader *tf_shader = hair_refine_shader_get(PART_REFINE_CATMULL_ROM);

#ifdef USE_TRANSFORM_FEEDBACK
      DRWShadingGroup *tf_shgrp = DRW_shgroup_transform_feedback_create(
          tf_shader, g_tf_pass, cache->final[subdiv].proc_buf);
#else
      DRWShadingGroup *tf_shgrp = DRW_shgroup_create(tf_shader, g_tf_pass);

      ParticleRefineCall *pr_call = MEM_mallocN(sizeof(*pr_call), __func__);
      pr_call->next = g_tf_calls;
      pr_call->vbo = cache->final[subdiv].proc_buf;
      pr_call->shgrp = tf_shgrp;
      pr_call->vert_len = final_points_len;
      g_tf_calls = pr_call;
      DRW_shgroup_uniform_int(tf_shgrp, "targetHeight", &g_tf_target_height, 1);
      DRW_shgroup_uniform_int(tf_shgrp, "targetWidth", &g_tf_target_width, 1);
      DRW_shgroup_uniform_int(tf_shgrp, "idOffset", &g_tf_id_offset, 1);
#endif

      DRW_shgroup_uniform_texture(tf_shgrp, "hairPointBuffer", cache->point_tex);
      DRW_shgroup_uniform_texture(tf_shgrp, "hairStrandBuffer", cache->strand_tex);
      DRW_shgroup_uniform_texture(tf_shgrp, "hairStrandSegBuffer", cache->strand_seg_tex);
      DRW_shgroup_uniform_int(tf_shgrp, "hairStrandsRes", &cache->final[subdiv].strands_res, 1);
      DRW_shgroup_call_procedural_points(tf_shgrp, NULL, final_points_len);
    }
  }
  return cache;
}

/* Note: Only valid after DRW_hair_update(). */
GPUVertBuf *DRW_hair_pos_buffer_get(Object *object, ParticleSystem *psys, ModifierData *md)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;

  int subdiv = scene->r.hair_subdiv;
  int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

  ParticleHairCache *cache = drw_hair_particle_cache_get(object, psys, md, subdiv, thickness_res);

  return cache->final[subdiv].proc_buf;
}

void DRW_hair_duplimat_get(Object *object,
                           ParticleSystem *psys,
                           ModifierData *UNUSED(md),
                           float (*dupli_mat)[4])
{
  Object *dupli_parent = DRW_object_get_dupli_parent(object);
  DupliObject *dupli_object = DRW_object_get_dupli(object);

  if (psys) {
    if ((dupli_parent != NULL) && (dupli_object != NULL)) {
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
      unit_m4(dupli_mat);
    }
  }
  else {
    /* New hair object. */
    copy_m4_m4(dupli_mat, object->obmat);
  }
}

DRWShadingGroup *DRW_shgroup_hair_create_sub(Object *object,
                                             ParticleSystem *psys,
                                             ModifierData *md,
                                             DRWShadingGroup *shgrp_parent)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  float dupli_mat[4][4];

  int subdiv = scene->r.hair_subdiv;
  int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

  ParticleHairCache *hair_cache = drw_hair_particle_cache_get(
      object, psys, md, subdiv, thickness_res);

  DRWShadingGroup *shgrp = DRW_shgroup_create_sub(shgrp_parent);

  /* TODO optimize this. Only bind the ones GPUMaterial needs. */
  for (int i = 0; i < hair_cache->num_uv_layers; i++) {
    for (int n = 0; n < MAX_LAYER_NAME_CT && hair_cache->uv_layer_names[i][n][0] != '\0'; n++) {
      DRW_shgroup_uniform_texture(shgrp, hair_cache->uv_layer_names[i][n], hair_cache->uv_tex[i]);
    }
  }
  for (int i = 0; i < hair_cache->num_col_layers; i++) {
    for (int n = 0; n < MAX_LAYER_NAME_CT && hair_cache->col_layer_names[i][n][0] != '\0'; n++) {
      DRW_shgroup_uniform_texture(
          shgrp, hair_cache->col_layer_names[i][n], hair_cache->col_tex[i]);
    }
  }

  /* Fix issue with certain driver not drawing anything if there is no texture bound to
   * "ac", "au", "u" or "c". */
  if (hair_cache->num_uv_layers == 0) {
    DRW_shgroup_uniform_texture(shgrp, "u", g_dummy_texture);
    DRW_shgroup_uniform_texture(shgrp, "au", g_dummy_texture);
  }
  if (hair_cache->num_col_layers == 0) {
    DRW_shgroup_uniform_texture(shgrp, "c", g_dummy_texture);
    DRW_shgroup_uniform_texture(shgrp, "ac", g_dummy_texture);
  }

  DRW_hair_duplimat_get(object, psys, md, dupli_mat);

  /* Get hair shape parameters. */
  float hair_rad_shape, hair_rad_root, hair_rad_tip;
  bool hair_close_tip;
  if (psys) {
    /* Old particle hair. */
    ParticleSettings *part = psys->part;
    hair_rad_shape = part->shape;
    hair_rad_root = part->rad_root * part->rad_scale * 0.5f;
    hair_rad_tip = part->rad_tip * part->rad_scale * 0.5f;
    hair_close_tip = (part->shape_flag & PART_SHAPE_CLOSE_TIP) != 0;
  }
  else {
    /* TODO: implement for new hair object. */
    hair_rad_shape = 1.0f;
    hair_rad_root = 0.005f;
    hair_rad_tip = 0.0f;
    hair_close_tip = true;
  }

  DRW_shgroup_uniform_texture(shgrp, "hairPointBuffer", hair_cache->final[subdiv].proc_tex);
  DRW_shgroup_uniform_int(shgrp, "hairStrandsRes", &hair_cache->final[subdiv].strands_res, 1);
  DRW_shgroup_uniform_int_copy(shgrp, "hairThicknessRes", thickness_res);
  DRW_shgroup_uniform_float_copy(shgrp, "hairRadShape", hair_rad_shape);
  DRW_shgroup_uniform_vec4_array_copy(shgrp, "hairDupliMatrix", dupli_mat, 4);
  DRW_shgroup_uniform_float_copy(shgrp, "hairRadRoot", hair_rad_root);
  DRW_shgroup_uniform_float_copy(shgrp, "hairRadTip", hair_rad_tip);
  DRW_shgroup_uniform_bool_copy(shgrp, "hairCloseTip", hair_close_tip);
  /* TODO(fclem): Until we have a better way to cull the hair and render with orco, bypass
   * culling test. */
  GPUBatch *geom = hair_cache->final[subdiv].proc_hairs[thickness_res - 1];
  DRW_shgroup_call_no_cull(shgrp, geom, object);

  return shgrp;
}

void DRW_hair_update(void)
{
#ifndef USE_TRANSFORM_FEEDBACK
  /**
   * Workaround to transform feedback not working on mac.
   * On some system it crashes (see T58489) and on some other it renders garbage (see T60171).
   *
   * So instead of using transform feedback we render to a texture,
   * read back the result to system memory and re-upload as VBO data.
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
  for (int i = 0; i < PART_REFINE_MAX_SHADER; i++) {
    DRW_SHADER_FREE_SAFE(g_refine_shaders[i]);
  }

  GPU_VERTBUF_DISCARD_SAFE(g_dummy_vbo);
  DRW_TEXTURE_FREE_SAFE(g_dummy_texture);
}
