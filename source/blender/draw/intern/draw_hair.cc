/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Contains procedural GPU hair drawing methods.
 */

#include "DRW_render.h"

#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "DNA_collection_types.h"
#include "DNA_customdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"

#include "BKE_duplilist.h"

#include "GPU_batch.h"
#include "GPU_capabilities.h"
#include "GPU_compute.h"
#include "GPU_context.h"
#include "GPU_material.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_vertex_buffer.h"

#include "DRW_gpu_wrapper.hh"

#include "draw_hair_private.h"
#include "draw_shader.h"
#include "draw_shader_shared.h"

BLI_INLINE eParticleRefineShaderType drw_hair_shader_type_get()
{
  /* NOTE: Hair refine is faster using transform feedback via vertex processing pipeline with Metal
   * and Apple Silicon GPUs. This is also because vertex work can more easily be executed in
   * parallel with fragment work, whereas compute inserts an explicit dependency,
   * due to switching of command encoder types. */
  if (GPU_compute_shader_support() && GPU_shader_storage_buffer_objects_support() &&
      (GPU_backend_get_type() != GPU_BACKEND_METAL))
  {
    return PART_REFINE_SHADER_COMPUTE;
  }
  if (GPU_transform_feedback_support()) {
    return PART_REFINE_SHADER_TRANSFORM_FEEDBACK;
  }
  return PART_REFINE_SHADER_TRANSFORM_FEEDBACK_WORKAROUND;
}

struct ParticleRefineCall {
  ParticleRefineCall *next;
  GPUVertBuf *vbo;
  DRWShadingGroup *shgrp;
  uint vert_len;
};

static ParticleRefineCall *g_tf_calls = nullptr;
static int g_tf_id_offset;
static int g_tf_target_width;
static int g_tf_target_height;

static GPUVertBuf *g_dummy_vbo = nullptr;
static DRWPass *g_tf_pass; /* XXX can be a problem with multiple #DRWManager in the future */
static blender::draw::UniformBuffer<CurvesInfos> *g_dummy_curves_info = nullptr;

static GPUShader *hair_refine_shader_get(ParticleRefineShader refinement)
{
  return DRW_shader_hair_refine_get(refinement, drw_hair_shader_type_get());
}

void DRW_hair_init()
{
  if (GPU_transform_feedback_support() || GPU_compute_shader_support()) {
    g_tf_pass = DRW_pass_create("Update Hair Pass", DRW_STATE_NO_DRAW);
  }
  else {
    g_tf_pass = DRW_pass_create("Update Hair Pass", DRW_STATE_WRITE_COLOR);
  }

  if (g_dummy_vbo == nullptr) {
    /* initialize vertex format */
    GPUVertFormat format = {0};
    uint dummy_id = GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

    g_dummy_vbo = GPU_vertbuf_create_with_format_ex(
        &format, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);

    const float vert[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GPU_vertbuf_data_alloc(g_dummy_vbo, 1);
    GPU_vertbuf_attr_fill(g_dummy_vbo, dummy_id, vert);
    /* Create VBO immediately to bind to texture buffer. */
    GPU_vertbuf_use(g_dummy_vbo);

    g_dummy_curves_info = MEM_new<blender::draw::UniformBuffer<CurvesInfos>>(
        "g_dummy_curves_info");
    memset(g_dummy_curves_info->is_point_attribute,
           0,
           sizeof(g_dummy_curves_info->is_point_attribute));
    g_dummy_curves_info->push_update();
  }
}

static void drw_hair_particle_cache_shgrp_attach_resources(DRWShadingGroup *shgrp,
                                                           ParticleHairCache *cache,
                                                           const int subdiv)
{
  DRW_shgroup_buffer_texture(shgrp, "hairPointBuffer", cache->proc_point_buf);
  DRW_shgroup_buffer_texture(shgrp, "hairStrandBuffer", cache->proc_strand_buf);
  DRW_shgroup_buffer_texture(shgrp, "hairStrandSegBuffer", cache->proc_strand_seg_buf);
  DRW_shgroup_uniform_int(shgrp, "hairStrandsRes", &cache->final[subdiv].strands_res, 1);
}

static void drw_hair_particle_cache_update_compute(ParticleHairCache *cache, const int subdiv)
{
  const int strands_len = cache->strands_len;
  const int final_points_len = cache->final[subdiv].strands_res * strands_len;
  if (final_points_len > 0) {
    GPUShader *shader = hair_refine_shader_get(PART_REFINE_CATMULL_ROM);
    DRWShadingGroup *shgrp = DRW_shgroup_create(shader, g_tf_pass);
    drw_hair_particle_cache_shgrp_attach_resources(shgrp, cache, subdiv);
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

static void drw_hair_particle_cache_update_transform_feedback(ParticleHairCache *cache,
                                                              const int subdiv)
{
  const int final_points_len = cache->final[subdiv].strands_res * cache->strands_len;
  if (final_points_len > 0) {
    GPUShader *tf_shader = hair_refine_shader_get(PART_REFINE_CATMULL_ROM);

    DRWShadingGroup *tf_shgrp = nullptr;
    if (GPU_transform_feedback_support()) {
      tf_shgrp = DRW_shgroup_transform_feedback_create(
          tf_shader, g_tf_pass, cache->final[subdiv].proc_buf);
    }
    else {
      tf_shgrp = DRW_shgroup_create(tf_shader, g_tf_pass);

      ParticleRefineCall *pr_call = (ParticleRefineCall *)MEM_mallocN(sizeof(*pr_call), __func__);
      pr_call->next = g_tf_calls;
      pr_call->vbo = cache->final[subdiv].proc_buf;
      pr_call->shgrp = tf_shgrp;
      pr_call->vert_len = final_points_len;
      g_tf_calls = pr_call;
      DRW_shgroup_uniform_int(tf_shgrp, "targetHeight", &g_tf_target_height, 1);
      DRW_shgroup_uniform_int(tf_shgrp, "targetWidth", &g_tf_target_width, 1);
      DRW_shgroup_uniform_int(tf_shgrp, "idOffset", &g_tf_id_offset, 1);
    }
    BLI_assert(tf_shgrp != nullptr);

    drw_hair_particle_cache_shgrp_attach_resources(tf_shgrp, cache, subdiv);
    DRW_shgroup_call_procedural_points(tf_shgrp, nullptr, final_points_len);
  }
}

static ParticleHairCache *drw_hair_particle_cache_get(Object *object,
                                                      ParticleSystem *psys,
                                                      ModifierData *md,
                                                      GPUMaterial *gpu_material,
                                                      int subdiv,
                                                      int thickness_res)
{
  ParticleHairCache *cache;
  bool update = particles_ensure_procedural_data(
      object, psys, md, &cache, gpu_material, subdiv, thickness_res);

  if (update) {
    if (drw_hair_shader_type_get() == PART_REFINE_SHADER_COMPUTE) {
      drw_hair_particle_cache_update_compute(cache, subdiv);
    }
    else {
      drw_hair_particle_cache_update_transform_feedback(cache, subdiv);
    }
  }
  return cache;
}

GPUVertBuf *DRW_hair_pos_buffer_get(Object *object, ParticleSystem *psys, ModifierData *md)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;

  int subdiv = scene->r.hair_subdiv;
  int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

  ParticleHairCache *cache = drw_hair_particle_cache_get(
      object, psys, md, nullptr, subdiv, thickness_res);

  return cache->final[subdiv].proc_buf;
}

void DRW_hair_duplimat_get(Object *object,
                           ParticleSystem * /*psys*/,
                           ModifierData * /*md*/,
                           float (*dupli_mat)[4])
{
  Object *dupli_parent = DRW_object_get_dupli_parent(object);
  DupliObject *dupli_object = DRW_object_get_dupli(object);

  if ((dupli_parent != nullptr) && (dupli_object != nullptr)) {
    if (dupli_object->type & OB_DUPLICOLLECTION) {
      unit_m4(dupli_mat);
      Collection *collection = dupli_parent->instance_collection;
      if (collection != nullptr) {
        sub_v3_v3(dupli_mat[3], collection->instance_offset);
      }
      mul_m4_m4m4(dupli_mat, dupli_parent->object_to_world, dupli_mat);
    }
    else {
      copy_m4_m4(dupli_mat, dupli_object->ob->object_to_world);
      invert_m4(dupli_mat);
      mul_m4_m4m4(dupli_mat, object->object_to_world, dupli_mat);
    }
  }
  else {
    unit_m4(dupli_mat);
  }
}

DRWShadingGroup *DRW_shgroup_hair_create_sub(Object *object,
                                             ParticleSystem *psys,
                                             ModifierData *md,
                                             DRWShadingGroup *shgrp_parent,
                                             GPUMaterial *gpu_material)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  float dupli_mat[4][4];

  int subdiv = scene->r.hair_subdiv;
  int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

  ParticleHairCache *hair_cache = drw_hair_particle_cache_get(
      object, psys, md, gpu_material, subdiv, thickness_res);

  DRWShadingGroup *shgrp = DRW_shgroup_create_sub(shgrp_parent);

  /* TODO: optimize this. Only bind the ones #GPUMaterial needs. */
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

  /* Fix issue with certain driver not drawing anything if there is nothing bound to
   * "ac", "au", "u" or "c". */
  if (hair_cache->num_uv_layers == 0) {
    DRW_shgroup_buffer_texture(shgrp, "u", g_dummy_vbo);
    DRW_shgroup_buffer_texture(shgrp, "au", g_dummy_vbo);
  }
  if (hair_cache->num_col_layers == 0) {
    DRW_shgroup_buffer_texture(shgrp, "c", g_dummy_vbo);
    DRW_shgroup_buffer_texture(shgrp, "ac", g_dummy_vbo);
  }

  DRW_hair_duplimat_get(object, psys, md, dupli_mat);

  /* Get hair shape parameters. */
  ParticleSettings *part = psys->part;
  float hair_rad_shape = part->shape;
  float hair_rad_root = part->rad_root * part->rad_scale * 0.5f;
  float hair_rad_tip = part->rad_tip * part->rad_scale * 0.5f;
  bool hair_close_tip = (part->shape_flag & PART_SHAPE_CLOSE_TIP) != 0;

  DRW_shgroup_buffer_texture(shgrp, "hairPointBuffer", hair_cache->final[subdiv].proc_buf);
  if (hair_cache->proc_length_buf) {
    DRW_shgroup_buffer_texture(shgrp, "l", hair_cache->proc_length_buf);
  }

  DRW_shgroup_uniform_block(shgrp, "drw_curves", *g_dummy_curves_info);
  DRW_shgroup_uniform_int(shgrp, "hairStrandsRes", &hair_cache->final[subdiv].strands_res, 1);
  DRW_shgroup_uniform_int_copy(shgrp, "hairThicknessRes", thickness_res);
  DRW_shgroup_uniform_float_copy(shgrp, "hairRadShape", hair_rad_shape);
  DRW_shgroup_uniform_mat4_copy(shgrp, "hairDupliMatrix", dupli_mat);
  DRW_shgroup_uniform_float_copy(shgrp, "hairRadRoot", hair_rad_root);
  DRW_shgroup_uniform_float_copy(shgrp, "hairRadTip", hair_rad_tip);
  DRW_shgroup_uniform_bool_copy(shgrp, "hairCloseTip", hair_close_tip);
  if (gpu_material) {
    /* NOTE: This needs to happen before the drawcall to allow correct attribute extraction.
     * (see #101896) */
    DRW_shgroup_add_material_resources(shgrp, gpu_material);
  }
  /* TODO(fclem): Until we have a better way to cull the hair and render with orco, bypass
   * culling test. */
  GPUBatch *geom = hair_cache->final[subdiv].proc_hairs[thickness_res - 1];
  DRW_shgroup_call_no_cull(shgrp, geom, object);

  return shgrp;
}

void DRW_hair_update()
{
  if (drw_hair_shader_type_get() == PART_REFINE_SHADER_TRANSFORM_FEEDBACK_WORKAROUND) {
    /**
     * Workaround to transform feedback not working on mac.
     * On some system it crashes (see #58489) and on some other it renders garbage (see #60171).
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
    for (ParticleRefineCall *pr_call = g_tf_calls; pr_call; pr_call = pr_call->next) {
      max_size = max_ii(max_size, pr_call->vert_len);
    }

    /* Create target Texture / Frame-buffer */
    /* Don't use max size as it can be really heavy and fail.
     * Do chunks of maximum 2048 * 2048 hair points. */
    int width = 2048;
    int height = min_ii(width, 1 + max_size / width);
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    GPUTexture *tex = DRW_texture_pool_query_2d_ex(
        width, height, GPU_RGBA32F, usage, (DrawEngineType *)DRW_hair_update);
    g_tf_target_height = height;
    g_tf_target_width = width;

    GPUFrameBuffer *fb = nullptr;
    GPU_framebuffer_ensure_config(&fb,
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(tex),
                                  });

    float *data = (float *)MEM_mallocN(sizeof(float[4]) * width * height, "tf fallback buffer");

    GPU_framebuffer_bind(fb);
    while (g_tf_calls != nullptr) {
      ParticleRefineCall *pr_call = g_tf_calls;
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
  }
  else {
    /* NOTE(Metal): If compute is not supported, bind a temporary frame-buffer to avoid
     * side-effects from rendering in the active buffer.
     * We also need to guarantee that a frame-buffer is active to perform any rendering work,
     * even if there is no output. */
    GPUFrameBuffer *temp_fb = nullptr;
    GPUFrameBuffer *prev_fb = nullptr;
    if (GPU_type_matches_ex(GPU_DEVICE_ANY, GPU_OS_MAC, GPU_DRIVER_ANY, GPU_BACKEND_METAL)) {
      if (!(GPU_compute_shader_support() && GPU_shader_storage_buffer_objects_support())) {
        prev_fb = GPU_framebuffer_active_get();
        char errorOut[256];
        /* if the frame-buffer is invalid we need a dummy frame-buffer to be bound. */
        if (!GPU_framebuffer_check_valid(prev_fb, errorOut)) {
          int width = 64;
          int height = 64;
          eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT |
                                   GPU_TEXTURE_USAGE_SHADER_WRITE;
          GPUTexture *tex = DRW_texture_pool_query_2d_ex(
              width, height, GPU_DEPTH_COMPONENT32F, usage, (DrawEngineType *)DRW_hair_update);
          g_tf_target_height = height;
          g_tf_target_width = width;

          GPU_framebuffer_ensure_config(&temp_fb, {GPU_ATTACHMENT_TEXTURE(tex)});

          GPU_framebuffer_bind(temp_fb);
        }
      }
    }

    /* Just render the pass when using compute shaders or transform feedback. */
    DRW_draw_pass(g_tf_pass);
    if (drw_hair_shader_type_get() == PART_REFINE_SHADER_COMPUTE) {
      GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
    }

    /* Release temporary frame-buffer. */
    if (temp_fb != nullptr) {
      GPU_framebuffer_free(temp_fb);
    }
    /* Rebind existing frame-buffer */
    if (prev_fb != nullptr) {
      GPU_framebuffer_bind(prev_fb);
    }
  }
}

void DRW_hair_free()
{
  GPU_VERTBUF_DISCARD_SAFE(g_dummy_vbo);
  MEM_delete(g_dummy_curves_info);
}

/* New Draw Manager. */
#include "draw_common.hh"

namespace blender::draw {

template<typename PassT>
GPUBatch *hair_sub_pass_setup_implementation(PassT &sub_ps,
                                             const Scene *scene,
                                             Object *object,
                                             ParticleSystem *psys,
                                             ModifierData *md,
                                             GPUMaterial *gpu_material)
{
  int subdiv = scene->r.hair_subdiv;
  int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;
  ParticleHairCache *hair_cache = drw_hair_particle_cache_get(
      object, psys, md, gpu_material, subdiv, thickness_res);

  /* TODO: optimize this. Only bind the ones #GPUMaterial needs. */
  for (int i : IndexRange(hair_cache->num_uv_layers)) {
    for (int n = 0; n < MAX_LAYER_NAME_CT && hair_cache->uv_layer_names[i][n][0] != '\0'; n++) {
      sub_ps.bind_texture(hair_cache->uv_layer_names[i][n], hair_cache->uv_tex[i]);
    }
  }
  for (int i : IndexRange(hair_cache->num_col_layers)) {
    for (int n = 0; n < MAX_LAYER_NAME_CT && hair_cache->col_layer_names[i][n][0] != '\0'; n++) {
      sub_ps.bind_texture(hair_cache->col_layer_names[i][n], hair_cache->col_tex[i]);
    }
  }

  /* Fix issue with certain driver not drawing anything if there is nothing bound to
   * "ac", "au", "u" or "c". */
  if (hair_cache->num_uv_layers == 0) {
    sub_ps.bind_texture("u", g_dummy_vbo);
    sub_ps.bind_texture("au", g_dummy_vbo);
  }
  if (hair_cache->num_col_layers == 0) {
    sub_ps.bind_texture("c", g_dummy_vbo);
    sub_ps.bind_texture("ac", g_dummy_vbo);
  }

  float4x4 dupli_mat;
  DRW_hair_duplimat_get(object, psys, md, dupli_mat.ptr());

  /* Get hair shape parameters. */
  ParticleSettings *part = psys->part;
  float hair_rad_shape = part->shape;
  float hair_rad_root = part->rad_root * part->rad_scale * 0.5f;
  float hair_rad_tip = part->rad_tip * part->rad_scale * 0.5f;
  bool hair_close_tip = (part->shape_flag & PART_SHAPE_CLOSE_TIP) != 0;

  sub_ps.bind_texture("hairPointBuffer", hair_cache->final[subdiv].proc_buf);
  if (hair_cache->proc_length_buf) {
    sub_ps.bind_texture("l", hair_cache->proc_length_buf);
  }

  sub_ps.bind_ubo("drw_curves", *g_dummy_curves_info);
  sub_ps.push_constant("hairStrandsRes", &hair_cache->final[subdiv].strands_res, 1);
  sub_ps.push_constant("hairThicknessRes", thickness_res);
  sub_ps.push_constant("hairRadShape", hair_rad_shape);
  sub_ps.push_constant("hairDupliMatrix", dupli_mat);
  sub_ps.push_constant("hairRadRoot", hair_rad_root);
  sub_ps.push_constant("hairRadTip", hair_rad_tip);
  sub_ps.push_constant("hairCloseTip", hair_close_tip);

  return hair_cache->final[subdiv].proc_hairs[thickness_res - 1];
}

GPUBatch *hair_sub_pass_setup(PassMain::Sub &sub_ps,
                              const Scene *scene,
                              Object *object,
                              ParticleSystem *psys,
                              ModifierData *md,
                              GPUMaterial *gpu_material)
{
  return hair_sub_pass_setup_implementation(sub_ps, scene, object, psys, md, gpu_material);
}

GPUBatch *hair_sub_pass_setup(PassSimple::Sub &sub_ps,
                              const Scene *scene,
                              Object *object,
                              ParticleSystem *psys,
                              ModifierData *md,
                              GPUMaterial *gpu_material)
{
  return hair_sub_pass_setup_implementation(sub_ps, scene, object, psys, md, gpu_material);
}

}  // namespace blender::draw
