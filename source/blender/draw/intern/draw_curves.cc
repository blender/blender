/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup draw
 *
 * \brief Contains procedural GPU hair drawing methods.
 */

#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "DNA_customdata_types.h"

#include "GPU_batch.h"
#include "GPU_capabilities.h"
#include "GPU_compute.h"
#include "GPU_material.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_vertex_buffer.h"

#include "DRW_render.h"

#include "draw_hair_private.h"
#include "draw_shader.h"

#ifndef __APPLE__
#  define USE_TRANSFORM_FEEDBACK
#  define USE_COMPUTE_SHADERS
#endif

BLI_INLINE eParticleRefineShaderType drw_curves_shader_type_get()
{
#ifdef USE_COMPUTE_SHADERS
  if (GPU_compute_shader_support() && GPU_shader_storage_buffer_objects_support()) {
    return PART_REFINE_SHADER_COMPUTE;
  }
#endif
#ifdef USE_TRANSFORM_FEEDBACK
  return PART_REFINE_SHADER_TRANSFORM_FEEDBACK;
#endif
  return PART_REFINE_SHADER_TRANSFORM_FEEDBACK_WORKAROUND;
}

#ifndef USE_TRANSFORM_FEEDBACK
struct CurvesEvalCall {
  struct CurvesEvalCall *next;
  GPUVertBuf *vbo;
  DRWShadingGroup *shgrp;
  uint vert_len;
};

static CurvesEvalCall *g_tf_calls = nullptr;
static int g_tf_id_offset;
static int g_tf_target_width;
static int g_tf_target_height;
#endif

static GPUVertBuf *g_dummy_vbo = nullptr;
static GPUTexture *g_dummy_texture = nullptr;
static DRWPass *g_tf_pass; /* XXX can be a problem with multiple DRWManager in the future */

static GPUShader *curves_eval_shader_get(CurvesEvalShader type)
{
  return DRW_shader_curves_refine_get(type, drw_curves_shader_type_get());
}

void DRW_curves_init(void)
{
  /* Initialize legacy hair too, to avoid verbosity in callers. */
  DRW_hair_init();

#if defined(USE_TRANSFORM_FEEDBACK) || defined(USE_COMPUTE_SHADERS)
  g_tf_pass = DRW_pass_create("Update Curves Pass", (DRWState)0);
#else
  g_tf_pass = DRW_pass_create("Update Curves Pass", DRW_STATE_WRITE_COLOR);
#endif

  if (g_dummy_vbo == nullptr) {
    /* initialize vertex format */
    GPUVertFormat format = {0};
    uint dummy_id = GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

    g_dummy_vbo = GPU_vertbuf_create_with_format(&format);

    const float vert[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GPU_vertbuf_data_alloc(g_dummy_vbo, 1);
    GPU_vertbuf_attr_fill(g_dummy_vbo, dummy_id, vert);
    /* Create vbo immediately to bind to texture buffer. */
    GPU_vertbuf_use(g_dummy_vbo);

    g_dummy_texture = GPU_texture_create_from_vertbuf("hair_dummy_attr", g_dummy_vbo);
  }
}

static void drw_curves_cache_shgrp_attach_resources(DRWShadingGroup *shgrp,
                                                    CurvesEvalCache *cache,
                                                    const int subdiv)
{
  DRW_shgroup_uniform_texture(shgrp, "hairPointBuffer", cache->point_tex);
  DRW_shgroup_uniform_texture(shgrp, "hairStrandBuffer", cache->strand_tex);
  DRW_shgroup_uniform_texture(shgrp, "hairStrandSegBuffer", cache->strand_seg_tex);
  DRW_shgroup_uniform_int(shgrp, "hairStrandsRes", &cache->final[subdiv].strands_res, 1);
}

static void drw_curves_cache_update_compute(CurvesEvalCache *cache, const int subdiv)
{
  const int strands_len = cache->strands_len;
  const int final_points_len = cache->final[subdiv].strands_res * strands_len;
  if (final_points_len > 0) {
    GPUShader *shader = curves_eval_shader_get(CURVES_EVAL_CATMULL_ROM);
    DRWShadingGroup *shgrp = DRW_shgroup_create(shader, g_tf_pass);
    drw_curves_cache_shgrp_attach_resources(shgrp, cache, subdiv);
    DRW_shgroup_vertex_buffer(shgrp, "posTime", cache->final[subdiv].proc_buf);

    const int max_strands_per_call = GPU_max_work_group_count(0);
    int strands_start = 0;
    while (strands_start < strands_len) {
      int batch_strands_len = MIN2(strands_len - strands_start, max_strands_per_call);
      DRWShadingGroup *subgroup = DRW_shgroup_create_sub(shgrp);
      DRW_shgroup_uniform_int_copy(subgroup, "hairStrandOffset", strands_start);
      DRW_shgroup_call_compute(subgroup, batch_strands_len, cache->final[subdiv].strands_res, 1);
      strands_start += batch_strands_len;
    }
  }
}

static void drw_curves_cache_update_transform_feedback(CurvesEvalCache *cache, const int subdiv)
{
  const int final_points_len = cache->final[subdiv].strands_res * cache->strands_len;
  if (final_points_len > 0) {
    GPUShader *tf_shader = curves_eval_shader_get(CURVES_EVAL_CATMULL_ROM);

#ifdef USE_TRANSFORM_FEEDBACK
    DRWShadingGroup *tf_shgrp = DRW_shgroup_transform_feedback_create(
        tf_shader, g_tf_pass, cache->final[subdiv].proc_buf);
#else
    DRWShadingGroup *tf_shgrp = DRW_shgroup_create(tf_shader, g_tf_pass);

    CurvesEvalCall *pr_call = MEM_new<CurvesEvalCall>(__func__);
    pr_call->next = g_tf_calls;
    pr_call->vbo = cache->final[subdiv].proc_buf;
    pr_call->shgrp = tf_shgrp;
    pr_call->vert_len = final_points_len;
    g_tf_calls = pr_call;
    DRW_shgroup_uniform_int(tf_shgrp, "targetHeight", &g_tf_target_height, 1);
    DRW_shgroup_uniform_int(tf_shgrp, "targetWidth", &g_tf_target_width, 1);
    DRW_shgroup_uniform_int(tf_shgrp, "idOffset", &g_tf_id_offset, 1);
#endif

    drw_curves_cache_shgrp_attach_resources(tf_shgrp, cache, subdiv);
    DRW_shgroup_call_procedural_points(tf_shgrp, nullptr, final_points_len);
  }
}

static CurvesEvalCache *drw_curves_cache_get(Object *object,
                                             GPUMaterial *gpu_material,
                                             int subdiv,
                                             int thickness_res)
{
  CurvesEvalCache *cache;
  bool update = curves_ensure_procedural_data(object, &cache, gpu_material, subdiv, thickness_res);

  if (update) {
    if (drw_curves_shader_type_get() == PART_REFINE_SHADER_COMPUTE) {
      drw_curves_cache_update_compute(cache, subdiv);
    }
    else {
      drw_curves_cache_update_transform_feedback(cache, subdiv);
    }
  }
  return cache;
}

GPUVertBuf *DRW_curves_pos_buffer_get(Object *object)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;

  int subdiv = scene->r.hair_subdiv;
  int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

  CurvesEvalCache *cache = drw_curves_cache_get(object, nullptr, subdiv, thickness_res);

  return cache->final[subdiv].proc_buf;
}

DRWShadingGroup *DRW_shgroup_curves_create_sub(Object *object,
                                               DRWShadingGroup *shgrp_parent,
                                               GPUMaterial *gpu_material)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;

  int subdiv = scene->r.hair_subdiv;
  int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

  CurvesEvalCache *curves_cache = drw_curves_cache_get(
      object, gpu_material, subdiv, thickness_res);

  DRWShadingGroup *shgrp = DRW_shgroup_create_sub(shgrp_parent);

  /* Fix issue with certain driver not drawing anything if there is no texture bound to
   * "ac", "au", "u" or "c". */
  DRW_shgroup_uniform_texture(shgrp, "u", g_dummy_texture);
  DRW_shgroup_uniform_texture(shgrp, "au", g_dummy_texture);
  DRW_shgroup_uniform_texture(shgrp, "c", g_dummy_texture);
  DRW_shgroup_uniform_texture(shgrp, "ac", g_dummy_texture);

  /* TODO: Generalize radius implementation for curves data type. */
  float hair_rad_shape = 0.0f;
  float hair_rad_root = 0.005f;
  float hair_rad_tip = 0.0f;
  bool hair_close_tip = true;

  DRW_shgroup_uniform_texture(shgrp, "hairPointBuffer", curves_cache->final[subdiv].proc_tex);
  if (curves_cache->length_tex) {
    DRW_shgroup_uniform_texture(shgrp, "hairLen", curves_cache->length_tex);
  }
  DRW_shgroup_uniform_int(shgrp, "hairStrandsRes", &curves_cache->final[subdiv].strands_res, 1);
  DRW_shgroup_uniform_int_copy(shgrp, "hairThicknessRes", thickness_res);
  DRW_shgroup_uniform_float_copy(shgrp, "hairRadShape", hair_rad_shape);
  DRW_shgroup_uniform_mat4_copy(shgrp, "hairDupliMatrix", object->obmat);
  DRW_shgroup_uniform_float_copy(shgrp, "hairRadRoot", hair_rad_root);
  DRW_shgroup_uniform_float_copy(shgrp, "hairRadTip", hair_rad_tip);
  DRW_shgroup_uniform_bool_copy(shgrp, "hairCloseTip", hair_close_tip);
  /* TODO(fclem): Until we have a better way to cull the curves and render with orco, bypass
   * culling test. */
  GPUBatch *geom = curves_cache->final[subdiv].proc_hairs[thickness_res - 1];
  DRW_shgroup_call_no_cull(shgrp, geom, object);

  return shgrp;
}

void DRW_curves_update()
{
  /* Update legacy hair too, to avoid verbosity in callers. */
  DRW_hair_update();

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

  if (g_tf_calls == nullptr) {
    return;
  }

  /* Search ideal buffer size. */
  uint max_size = 0;
  for (CurvesEvalCall *pr_call = g_tf_calls; pr_call; pr_call = pr_call->next) {
    max_size = max_ii(max_size, pr_call->vert_len);
  }

  /* Create target Texture / Frame-buffer */
  /* Don't use max size as it can be really heavy and fail.
   * Do chunks of maximum 2048 * 2048 hair points. */
  int width = 2048;
  int height = min_ii(width, 1 + max_size / width);
  GPUTexture *tex = DRW_texture_pool_query_2d(
      width, height, GPU_RGBA32F, (DrawEngineType *)DRW_curves_update);
  g_tf_target_height = height;
  g_tf_target_width = width;

  GPUFrameBuffer *fb = nullptr;
  GPU_framebuffer_ensure_config(&fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(tex),
                                });

  float *data = static_cast<float *>(
      MEM_mallocN(sizeof(float[4]) * width * height, "tf fallback buffer"));

  GPU_framebuffer_bind(fb);
  while (g_tf_calls != nullptr) {
    CurvesEvalCall *pr_call = g_tf_calls;
    g_tf_calls = g_tf_calls->next;

    g_tf_id_offset = 0;
    while (pr_call->vert_len > 0) {
      int max_read_px_len = min_ii(width * height, pr_call->vert_len);

      DRW_draw_pass_subset(g_tf_pass, pr_call->shgrp, pr_call->shgrp);
      /* Read back result to main memory. */
      GPU_framebuffer_read_color(fb, 0, 0, width, height, 4, 0, GPU_DATA_FLOAT, data);
      /* Upload back to VBO. */
      GPU_vertbuf_use(pr_call->vbo);
      GPU_vertbuf_update_sub(pr_call->vbo,
                             sizeof(float[4]) * g_tf_id_offset,
                             sizeof(float[4]) * max_read_px_len,
                             data);

      g_tf_id_offset += max_read_px_len;
      pr_call->vert_len -= max_read_px_len;
    }

    MEM_freeN(pr_call);
  }

  MEM_freeN(data);
  GPU_framebuffer_free(fb);
#else
  /* Just render the pass when using compute shaders or transform feedback. */
  DRW_draw_pass(g_tf_pass);
  if (drw_curves_shader_type_get() == PART_REFINE_SHADER_COMPUTE) {
    GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
  }
#endif
}

void DRW_curves_free()
{
  DRW_hair_free();

  GPU_VERTBUF_DISCARD_SAFE(g_dummy_vbo);
  DRW_TEXTURE_FREE_SAFE(g_dummy_texture);
}
