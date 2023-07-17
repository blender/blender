/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Contains procedural GPU hair drawing methods.
 */

#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "DNA_curves_types.h"

#include "BKE_curves.hh"

#include "GPU_batch.h"
#include "GPU_capabilities.h"
#include "GPU_compute.h"
#include "GPU_material.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_vertex_buffer.h"

#include "DRW_gpu_wrapper.hh"
#include "DRW_render.h"

#include "draw_cache_impl.h"
#include "draw_curves_private.hh"
#include "draw_hair_private.h"
#include "draw_manager.h"
#include "draw_shader.h"

BLI_INLINE eParticleRefineShaderType drw_curves_shader_type_get()
{
  /* NOTE: Curve refine is faster using transform feedback via vertex processing pipeline with
   * Metal and Apple Silicon GPUs. This is also because vertex work can more easily be executed in
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

struct CurvesEvalCall {
  CurvesEvalCall *next;
  GPUVertBuf *vbo;
  DRWShadingGroup *shgrp;
  uint vert_len;
};

static CurvesEvalCall *g_tf_calls = nullptr;
static int g_tf_id_offset;
static int g_tf_target_width;
static int g_tf_target_height;

static GPUVertBuf *g_dummy_vbo = nullptr;
static DRWPass *g_tf_pass; /* XXX can be a problem with multiple DRWManager in the future */

using CurvesInfosBuf = blender::draw::UniformBuffer<CurvesInfos>;

struct CurvesUniformBufPool {
  blender::Vector<std::unique_ptr<CurvesInfosBuf>> ubos;
  int used = 0;

  void reset()
  {
    used = 0;
  }

  CurvesInfosBuf &alloc()
  {
    if (used >= ubos.size()) {
      ubos.append(std::make_unique<CurvesInfosBuf>());
      return *ubos.last();
    }
    return *ubos[used++];
  }
};

static GPUShader *curves_eval_shader_get(CurvesEvalShader type)
{
  return DRW_shader_curves_refine_get(type, drw_curves_shader_type_get());
}

void DRW_curves_init(DRWData *drw_data)
{
  /* Initialize legacy hair too, to avoid verbosity in callers. */
  DRW_hair_init();

  if (drw_data->curves_ubos == nullptr) {
    drw_data->curves_ubos = MEM_new<CurvesUniformBufPool>("CurvesUniformBufPool");
  }
  CurvesUniformBufPool *pool = drw_data->curves_ubos;
  pool->reset();

  if (GPU_transform_feedback_support() || GPU_compute_shader_support()) {
    g_tf_pass = DRW_pass_create("Update Curves Pass", (DRWState)0);
  }
  else {
    g_tf_pass = DRW_pass_create("Update Curves Pass", DRW_STATE_WRITE_COLOR);
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
    /* Create vbo immediately to bind to texture buffer. */
    GPU_vertbuf_use(g_dummy_vbo);
  }
}

void DRW_curves_ubos_pool_free(CurvesUniformBufPool *pool)
{
  MEM_delete(pool);
}

static void drw_curves_cache_shgrp_attach_resources(DRWShadingGroup *shgrp,
                                                    CurvesEvalCache *cache,
                                                    GPUVertBuf *point_buf,
                                                    const int subdiv)
{
  DRW_shgroup_buffer_texture(shgrp, "hairPointBuffer", point_buf);
  DRW_shgroup_buffer_texture(shgrp, "hairStrandBuffer", cache->proc_strand_buf);
  DRW_shgroup_buffer_texture(shgrp, "hairStrandSegBuffer", cache->proc_strand_seg_buf);
  DRW_shgroup_uniform_int(shgrp, "hairStrandsRes", &cache->final[subdiv].strands_res, 1);
}

static void drw_curves_cache_update_compute(CurvesEvalCache *cache,
                                            const int subdiv,
                                            const int strands_len,
                                            GPUVertBuf *output_buf,
                                            GPUVertBuf *input_buf)
{
  GPUShader *shader = curves_eval_shader_get(CURVES_EVAL_CATMULL_ROM);
  DRWShadingGroup *shgrp = DRW_shgroup_create(shader, g_tf_pass);
  drw_curves_cache_shgrp_attach_resources(shgrp, cache, input_buf, subdiv);
  DRW_shgroup_vertex_buffer(shgrp, "posTime", output_buf);

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

static void drw_curves_cache_update_compute(CurvesEvalCache *cache, const int subdiv)
{
  const int strands_len = cache->strands_len;
  const int final_points_len = cache->final[subdiv].strands_res * strands_len;
  if (final_points_len == 0) {
    return;
  }

  drw_curves_cache_update_compute(
      cache, subdiv, strands_len, cache->final[subdiv].proc_buf, cache->proc_point_buf);

  const DRW_Attributes &attrs = cache->final[subdiv].attr_used;
  for (int i = 0; i < attrs.num_requests; i++) {
    /* Only refine point attributes. */
    if (attrs.requests[i].domain == ATTR_DOMAIN_CURVE) {
      continue;
    }

    drw_curves_cache_update_compute(cache,
                                    subdiv,
                                    strands_len,
                                    cache->final[subdiv].attributes_buf[i],
                                    cache->proc_attributes_buf[i]);
  }
}

static void drw_curves_cache_update_transform_feedback(CurvesEvalCache *cache,
                                                       GPUVertBuf *output_buf,
                                                       GPUVertBuf *input_buf,
                                                       const int subdiv,
                                                       const int final_points_len)
{
  GPUShader *tf_shader = curves_eval_shader_get(CURVES_EVAL_CATMULL_ROM);

  DRWShadingGroup *tf_shgrp = nullptr;
  if (GPU_transform_feedback_support()) {
    tf_shgrp = DRW_shgroup_transform_feedback_create(tf_shader, g_tf_pass, output_buf);
  }
  else {
    tf_shgrp = DRW_shgroup_create(tf_shader, g_tf_pass);

    CurvesEvalCall *pr_call = MEM_new<CurvesEvalCall>(__func__);
    pr_call->next = g_tf_calls;
    pr_call->vbo = output_buf;
    pr_call->shgrp = tf_shgrp;
    pr_call->vert_len = final_points_len;
    g_tf_calls = pr_call;
    DRW_shgroup_uniform_int(tf_shgrp, "targetHeight", &g_tf_target_height, 1);
    DRW_shgroup_uniform_int(tf_shgrp, "targetWidth", &g_tf_target_width, 1);
    DRW_shgroup_uniform_int(tf_shgrp, "idOffset", &g_tf_id_offset, 1);
  }
  BLI_assert(tf_shgrp != nullptr);

  drw_curves_cache_shgrp_attach_resources(tf_shgrp, cache, input_buf, subdiv);
  DRW_shgroup_call_procedural_points(tf_shgrp, nullptr, final_points_len);
}

static void drw_curves_cache_update_transform_feedback(CurvesEvalCache *cache, const int subdiv)
{
  const int final_points_len = cache->final[subdiv].strands_res * cache->strands_len;
  if (final_points_len == 0) {
    return;
  }

  drw_curves_cache_update_transform_feedback(
      cache, cache->final[subdiv].proc_buf, cache->proc_point_buf, subdiv, final_points_len);

  const DRW_Attributes &attrs = cache->final[subdiv].attr_used;
  for (int i = 0; i < attrs.num_requests; i++) {
    /* Only refine point attributes. */
    if (attrs.requests[i].domain == ATTR_DOMAIN_CURVE) {
      continue;
    }

    drw_curves_cache_update_transform_feedback(cache,
                                               cache->final[subdiv].attributes_buf[i],
                                               cache->proc_attributes_buf[i],
                                               subdiv,
                                               final_points_len);
  }
}

static CurvesEvalCache *drw_curves_cache_get(Curves &curves,
                                             GPUMaterial *gpu_material,
                                             int subdiv,
                                             int thickness_res)
{
  CurvesEvalCache *cache;
  const bool update = curves_ensure_procedural_data(
      &curves, &cache, gpu_material, subdiv, thickness_res);

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
  const Scene *scene = draw_ctx->scene;

  const int subdiv = scene->r.hair_subdiv;
  const int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

  Curves &curves = *static_cast<Curves *>(object->data);
  CurvesEvalCache *cache = drw_curves_cache_get(curves, nullptr, subdiv, thickness_res);

  return cache->final[subdiv].proc_buf;
}

static int attribute_index_in_material(GPUMaterial *gpu_material, const char *name)
{
  if (!gpu_material) {
    return -1;
  }

  int index = 0;

  ListBase gpu_attrs = GPU_material_attributes(gpu_material);
  LISTBASE_FOREACH (GPUMaterialAttribute *, gpu_attr, &gpu_attrs) {
    if (STREQ(gpu_attr->name, name)) {
      return index;
    }

    index++;
  }

  return -1;
}

DRWShadingGroup *DRW_shgroup_curves_create_sub(Object *object,
                                               DRWShadingGroup *shgrp_parent,
                                               GPUMaterial *gpu_material)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene = draw_ctx->scene;
  CurvesUniformBufPool *pool = DST.vmempool->curves_ubos;
  CurvesInfosBuf &curves_infos = pool->alloc();
  Curves &curves_id = *static_cast<Curves *>(object->data);

  const int subdiv = scene->r.hair_subdiv;
  const int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

  CurvesEvalCache *curves_cache = drw_curves_cache_get(
      curves_id, gpu_material, subdiv, thickness_res);

  DRWShadingGroup *shgrp = DRW_shgroup_create_sub(shgrp_parent);

  /* Fix issue with certain driver not drawing anything if there is nothing bound to
   * "ac", "au", "u" or "c". */
  DRW_shgroup_buffer_texture(shgrp, "u", g_dummy_vbo);
  DRW_shgroup_buffer_texture(shgrp, "au", g_dummy_vbo);
  DRW_shgroup_buffer_texture(shgrp, "c", g_dummy_vbo);
  DRW_shgroup_buffer_texture(shgrp, "ac", g_dummy_vbo);

  /* TODO: Generalize radius implementation for curves data type. */
  float hair_rad_shape = 0.0f;
  float hair_rad_root = 0.005f;
  float hair_rad_tip = 0.0f;
  bool hair_close_tip = true;

  /* Use the radius of the root and tip of the first curve for now. This is a workaround that we
   * use for now because we can't use a per-point radius yet. */
  const blender::bke::CurvesGeometry &curves = curves_id.geometry.wrap();
  if (curves.curves_num() >= 1) {
    blender::VArray<float> radii = *curves.attributes().lookup_or_default(
        "radius", ATTR_DOMAIN_POINT, 0.005f);
    const blender::IndexRange first_curve_points = curves.points_by_curve()[0];
    const float first_radius = radii[first_curve_points.first()];
    const float last_radius = radii[first_curve_points.last()];
    const float middle_radius = radii[first_curve_points.size() / 2];
    hair_rad_root = radii[first_curve_points.first()];
    hair_rad_tip = radii[first_curve_points.last()];
    hair_rad_shape = std::clamp(
        safe_divide(middle_radius - first_radius, last_radius - first_radius) * 2.0f - 1.0f,
        -1.0f,
        1.0f);
  }

  DRW_shgroup_buffer_texture(shgrp, "hairPointBuffer", curves_cache->final[subdiv].proc_buf);
  if (curves_cache->proc_length_buf) {
    DRW_shgroup_buffer_texture(shgrp, "hairLen", curves_cache->proc_length_buf);
  }

  const DRW_Attributes &attrs = curves_cache->final[subdiv].attr_used;
  for (int i = 0; i < attrs.num_requests; i++) {
    const DRW_AttributeRequest &request = attrs.requests[i];

    char sampler_name[32];
    drw_curves_get_attribute_sampler_name(request.attribute_name, sampler_name);

    if (request.domain == ATTR_DOMAIN_CURVE) {
      if (!curves_cache->proc_attributes_buf[i]) {
        continue;
      }

      DRW_shgroup_buffer_texture(shgrp, sampler_name, curves_cache->proc_attributes_buf[i]);
    }
    else {
      if (!curves_cache->final[subdiv].attributes_buf[i]) {
        continue;
      }
      DRW_shgroup_buffer_texture(
          shgrp, sampler_name, curves_cache->final[subdiv].attributes_buf[i]);
    }

    /* Some attributes may not be used in the shader anymore and were not garbage collected yet, so
     * we need to find the right index for this attribute as uniforms defining the scope of the
     * attributes are based on attribute loading order, which is itself based on the material's
     * attributes. */
    const int index = attribute_index_in_material(gpu_material, request.attribute_name);
    if (index != -1) {
      curves_infos.is_point_attribute[index][0] = request.domain == ATTR_DOMAIN_POINT;
    }
  }

  curves_infos.push_update();

  DRW_shgroup_uniform_block(shgrp, "drw_curves", curves_infos);

  DRW_shgroup_uniform_int(shgrp, "hairStrandsRes", &curves_cache->final[subdiv].strands_res, 1);
  DRW_shgroup_uniform_int_copy(shgrp, "hairThicknessRes", thickness_res);
  DRW_shgroup_uniform_float_copy(shgrp, "hairRadShape", hair_rad_shape);
  DRW_shgroup_uniform_mat4_copy(shgrp, "hairDupliMatrix", object->object_to_world);
  DRW_shgroup_uniform_float_copy(shgrp, "hairRadRoot", hair_rad_root);
  DRW_shgroup_uniform_float_copy(shgrp, "hairRadTip", hair_rad_tip);
  DRW_shgroup_uniform_bool_copy(shgrp, "hairCloseTip", hair_close_tip);
  if (gpu_material) {
    /* NOTE: This needs to happen before the drawcall to allow correct attribute extraction.
     * (see #101896) */
    DRW_shgroup_add_material_resources(shgrp, gpu_material);
  }
  /* TODO(fclem): Until we have a better way to cull the curves and render with orco, bypass
   * culling test. */
  GPUBatch *geom = curves_cache->final[subdiv].proc_hairs[thickness_res - 1];
  DRW_shgroup_call_no_cull(shgrp, geom, object);

  return shgrp;
}

void DRW_curves_update()
{

  /* Ensure there's a valid active view.
   * "Next" engines use this function, but this still uses the old Draw Manager. */
  if (DRW_view_default_get() == nullptr) {
    /* Create a dummy default view, it's not really used. */
    DRW_view_default_set(DRW_view_create(
        float4x4::identity().ptr(), float4x4::identity().ptr(), nullptr, nullptr, nullptr));
  }
  if (DRW_view_get_active() == nullptr) {
    DRW_view_set_active(DRW_view_default_get());
  }

  /* Update legacy hair too, to avoid verbosity in callers. */
  DRW_hair_update();

  if (drw_curves_shader_type_get() == PART_REFINE_SHADER_TRANSFORM_FEEDBACK_WORKAROUND) {
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
    for (CurvesEvalCall *pr_call = g_tf_calls; pr_call; pr_call = pr_call->next) {
      max_size = max_ii(max_size, pr_call->vert_len);
    }

    /* Create target Texture / Frame-buffer */
    /* Don't use max size as it can be really heavy and fail.
     * Do chunks of maximum 2048 * 2048 hair points. */
    int width = 2048;
    int height = min_ii(width, 1 + max_size / width);
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT |
                             GPU_TEXTURE_USAGE_SHADER_WRITE;
    GPUTexture *tex = DRW_texture_pool_query_2d_ex(
        width, height, GPU_RGBA32F, usage, (DrawEngineType *)DRW_curves_update);
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
  }
  else {
    /* NOTE(Metal): If compute is not supported, bind a temporary frame-buffer to avoid
     * side-effects from rendering in the active buffer.
     * We also need to guarantee that a Frame-buffer is active to perform any rendering work,
     * even if there is no output */
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

          eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
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
    if (drw_curves_shader_type_get() == PART_REFINE_SHADER_COMPUTE) {
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

void DRW_curves_free()
{
  DRW_hair_free();

  GPU_VERTBUF_DISCARD_SAFE(g_dummy_vbo);
}

/* New Draw Manager. */
#include "draw_common.hh"

namespace blender::draw {

template<typename PassT>
GPUBatch *curves_sub_pass_setup_implementation(PassT &sub_ps,
                                               const Scene *scene,
                                               Object *ob,
                                               GPUMaterial *gpu_material)
{
  CurvesUniformBufPool *pool = DST.vmempool->curves_ubos;
  CurvesInfosBuf &curves_infos = pool->alloc();
  BLI_assert(ob->type == OB_CURVES);
  Curves &curves_id = *static_cast<Curves *>(ob->data);

  const int subdiv = scene->r.hair_subdiv;
  const int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

  CurvesEvalCache *curves_cache = drw_curves_cache_get(
      curves_id, gpu_material, subdiv, thickness_res);

  /* Fix issue with certain driver not drawing anything if there is nothing bound to
   * "ac", "au", "u" or "c". */
  sub_ps.bind_texture("u", g_dummy_vbo);
  sub_ps.bind_texture("au", g_dummy_vbo);
  sub_ps.bind_texture("c", g_dummy_vbo);
  sub_ps.bind_texture("ac", g_dummy_vbo);

  /* TODO: Generalize radius implementation for curves data type. */
  float hair_rad_shape = 0.0f;
  float hair_rad_root = 0.005f;
  float hair_rad_tip = 0.0f;
  bool hair_close_tip = true;

  /* Use the radius of the root and tip of the first curve for now. This is a workaround that we
   * use for now because we can't use a per-point radius yet. */
  const blender::bke::CurvesGeometry &curves = curves_id.geometry.wrap();
  if (curves.curves_num() >= 1) {
    blender::VArray<float> radii = *curves.attributes().lookup_or_default(
        "radius", ATTR_DOMAIN_POINT, 0.005f);
    const blender::IndexRange first_curve_points = curves.points_by_curve()[0];
    const float first_radius = radii[first_curve_points.first()];
    const float last_radius = radii[first_curve_points.last()];
    const float middle_radius = radii[first_curve_points.size() / 2];
    hair_rad_root = radii[first_curve_points.first()];
    hair_rad_tip = radii[first_curve_points.last()];
    hair_rad_shape = std::clamp(
        safe_divide(middle_radius - first_radius, last_radius - first_radius) * 2.0f - 1.0f,
        -1.0f,
        1.0f);
  }

  sub_ps.bind_texture("hairPointBuffer", curves_cache->final[subdiv].proc_buf);
  if (curves_cache->proc_length_buf) {
    sub_ps.bind_texture("hairLen", curves_cache->proc_length_buf);
  }

  const DRW_Attributes &attrs = curves_cache->final[subdiv].attr_used;
  for (int i = 0; i < attrs.num_requests; i++) {
    const DRW_AttributeRequest &request = attrs.requests[i];

    char sampler_name[32];
    drw_curves_get_attribute_sampler_name(request.attribute_name, sampler_name);

    if (request.domain == ATTR_DOMAIN_CURVE) {
      if (!curves_cache->proc_attributes_buf[i]) {
        continue;
      }
      sub_ps.bind_texture(sampler_name, curves_cache->proc_attributes_buf[i]);
    }
    else {
      if (!curves_cache->final[subdiv].attributes_buf[i]) {
        continue;
      }
      sub_ps.bind_texture(sampler_name, curves_cache->final[subdiv].attributes_buf[i]);
    }

    /* Some attributes may not be used in the shader anymore and were not garbage collected yet, so
     * we need to find the right index for this attribute as uniforms defining the scope of the
     * attributes are based on attribute loading order, which is itself based on the material's
     * attributes. */
    const int index = attribute_index_in_material(gpu_material, request.attribute_name);
    if (index != -1) {
      curves_infos.is_point_attribute[index][0] = request.domain == ATTR_DOMAIN_POINT;
    }
  }

  curves_infos.push_update();

  sub_ps.bind_ubo("drw_curves", curves_infos);

  sub_ps.push_constant("hairStrandsRes", &curves_cache->final[subdiv].strands_res, 1);
  sub_ps.push_constant("hairThicknessRes", thickness_res);
  sub_ps.push_constant("hairRadShape", hair_rad_shape);
  sub_ps.push_constant("hairDupliMatrix", float4x4(ob->object_to_world));
  sub_ps.push_constant("hairRadRoot", hair_rad_root);
  sub_ps.push_constant("hairRadTip", hair_rad_tip);
  sub_ps.push_constant("hairCloseTip", hair_close_tip);

  return curves_cache->final[subdiv].proc_hairs[thickness_res - 1];
}

GPUBatch *curves_sub_pass_setup(PassMain::Sub &ps,
                                const Scene *scene,
                                Object *ob,
                                GPUMaterial *gpu_material)
{
  return curves_sub_pass_setup_implementation(ps, scene, ob, gpu_material);
}

GPUBatch *curves_sub_pass_setup(PassSimple::Sub &ps,
                                const Scene *scene,
                                Object *ob,
                                GPUMaterial *gpu_material)
{
  return curves_sub_pass_setup_implementation(ps, scene, ob, gpu_material);
}

}  // namespace blender::draw
