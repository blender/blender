/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Contains procedural GPU hair drawing methods.
 */

#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "DNA_curves_types.h"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"

#include "GPU_batch.hh"
#include "GPU_capabilities.hh"
#include "GPU_compute.hh"
#include "GPU_material.hh"
#include "GPU_shader.hh"
#include "GPU_texture.hh"
#include "GPU_vertex_buffer.hh"

#include "DRW_gpu_wrapper.hh"
#include "DRW_render.hh"

#include "draw_cache_impl.hh"
#include "draw_common.hh"
#include "draw_curves_private.hh"
#include "draw_hair_private.hh"
#include "draw_manager_c.hh"
#include "draw_shader.hh"

namespace blender::draw {

struct CurvesEvalCall {
  CurvesEvalCall *next;
  GPUVertBuf *vbo;
  DRWShadingGroup *shgrp;
  uint vert_len;
};

static GPUVertBuf *g_dummy_vbo = nullptr;
static DRWPass *g_tf_pass; /* XXX can be a problem with multiple DRWManager in the future */

using CurvesInfosBuf = UniformBuffer<CurvesInfos>;

struct CurvesUniformBufPool {
  Vector<std::unique_ptr<CurvesInfosBuf>> ubos;
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

static void drw_curves_ensure_dummy_vbo()
{
  if (g_dummy_vbo != nullptr) {
    return;
  }
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

  drw_curves_ensure_dummy_vbo();
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
  DRW_shgroup_uniform_int(shgrp, "hairStrandsRes", &cache->final[subdiv].resolution, 1);
}

static void drw_curves_cache_update_compute(CurvesEvalCache *cache,
                                            const int subdiv,
                                            const int curves_num,
                                            GPUVertBuf *output_buf,
                                            GPUVertBuf *input_buf)
{
  GPUShader *shader = DRW_shader_curves_refine_get(CURVES_EVAL_CATMULL_ROM);
  DRWShadingGroup *shgrp = DRW_shgroup_create(shader, g_tf_pass);
  drw_curves_cache_shgrp_attach_resources(shgrp, cache, input_buf, subdiv);
  DRW_shgroup_vertex_buffer(shgrp, "posTime", output_buf);

  const int max_strands_per_call = GPU_max_work_group_count(0);
  int strands_start = 0;
  while (strands_start < curves_num) {
    int batch_strands_len = std::min(curves_num - strands_start, max_strands_per_call);
    DRWShadingGroup *subgroup = DRW_shgroup_create_sub(shgrp);
    DRW_shgroup_uniform_int_copy(subgroup, "hairStrandOffset", strands_start);
    DRW_shgroup_call_compute(subgroup, batch_strands_len, cache->final[subdiv].resolution, 1);
    strands_start += batch_strands_len;
  }
}

static void drw_curves_cache_update_compute(CurvesEvalCache *cache, const int subdiv)
{
  const int curves_num = cache->curves_num;
  const int final_points_len = cache->final[subdiv].resolution * curves_num;
  if (final_points_len == 0) {
    return;
  }

  drw_curves_cache_update_compute(
      cache, subdiv, curves_num, cache->final[subdiv].proc_buf, cache->proc_point_buf);

  const DRW_Attributes &attrs = cache->final[subdiv].attr_used;
  for (int i = 0; i < attrs.num_requests; i++) {
    /* Only refine point attributes. */
    if (attrs.requests[i].domain == bke::AttrDomain::Curve) {
      continue;
    }

    drw_curves_cache_update_compute(cache,
                                    subdiv,
                                    curves_num,
                                    cache->final[subdiv].attributes_buf[i],
                                    cache->proc_attributes_buf[i]);
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
    drw_curves_cache_update_compute(cache, subdiv);
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
  const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
  if (curves.curves_num() >= 1) {
    VArray<float> radii = *curves.attributes().lookup_or_default(
        "radius", bke::AttrDomain::Point, 0.005f);
    const IndexRange first_curve_points = curves.points_by_curve()[0];
    const float first_radius = radii[first_curve_points.first()];
    const float last_radius = radii[first_curve_points.last()];
    const float middle_radius = radii[first_curve_points.size() / 2];
    hair_rad_root = radii[first_curve_points.first()];
    hair_rad_tip = radii[first_curve_points.last()];
    hair_rad_shape = std::clamp(
        math::safe_divide(middle_radius - first_radius, last_radius - first_radius) * 2.0f - 1.0f,
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

    if (request.domain == bke::AttrDomain::Curve) {
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
      curves_infos.is_point_attribute[index][0] = request.domain == bke::AttrDomain::Point;
    }
  }

  curves_infos.push_update();

  DRW_shgroup_uniform_block(shgrp, "drw_curves", curves_infos);

  DRW_shgroup_uniform_int(shgrp, "hairStrandsRes", &curves_cache->final[subdiv].resolution, 1);
  DRW_shgroup_uniform_int_copy(shgrp, "hairThicknessRes", thickness_res);
  DRW_shgroup_uniform_float_copy(shgrp, "hairRadShape", hair_rad_shape);
  DRW_shgroup_uniform_mat4_copy(shgrp, "hairDupliMatrix", object->object_to_world().ptr());
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

  DRW_draw_pass(g_tf_pass);
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
}

void DRW_curves_free()
{
  DRW_hair_free();

  GPU_VERTBUF_DISCARD_SAFE(g_dummy_vbo);
}

/* New Draw Manager. */

static PassSimple *g_pass = nullptr;

void curves_init()
{
  if (!g_pass) {
    g_pass = MEM_new<PassSimple>("drw_curves g_pass", "Update Curves Pass");
  }
  g_pass->init();
  g_pass->state_set(DRW_STATE_NO_DRAW);
}

static CurvesEvalCache *curves_cache_get(Curves &curves,
                                         GPUMaterial *gpu_material,
                                         int subdiv,
                                         int thickness_res)
{
  CurvesEvalCache *cache;
  const bool update = curves_ensure_procedural_data(
      &curves, &cache, gpu_material, subdiv, thickness_res);

  if (!update) {
    return cache;
  }

  const int curves_num = cache->curves_num;
  const int final_points_len = cache->final[subdiv].resolution * curves_num;

  auto cache_update = [&](GPUVertBuf *output_buf, GPUVertBuf *input_buf) {
    PassSimple::Sub &ob_ps = g_pass->sub("Object Pass");

    ob_ps.shader_set(DRW_shader_curves_refine_get(CURVES_EVAL_CATMULL_ROM));

    ob_ps.bind_texture("hairPointBuffer", input_buf);
    ob_ps.bind_texture("hairStrandBuffer", cache->proc_strand_buf);
    ob_ps.bind_texture("hairStrandSegBuffer", cache->proc_strand_seg_buf);
    ob_ps.push_constant("hairStrandsRes", &cache->final[subdiv].resolution);
    ob_ps.bind_ssbo("posTime", output_buf);

    const int max_strands_per_call = GPU_max_work_group_count(0);
    int strands_start = 0;
    while (strands_start < curves_num) {
      int batch_strands_len = std::min(curves_num - strands_start, max_strands_per_call);
      PassSimple::Sub &sub_ps = ob_ps.sub("Sub Pass");
      sub_ps.push_constant("hairStrandOffset", strands_start);
      sub_ps.dispatch(int3(batch_strands_len, cache->final[subdiv].resolution, 1));
      strands_start += batch_strands_len;
    }
  };

  if (final_points_len > 0) {
    cache_update(cache->final[subdiv].proc_buf, cache->proc_point_buf);

    const DRW_Attributes &attrs = cache->final[subdiv].attr_used;
    for (int i : IndexRange(attrs.num_requests)) {
      /* Only refine point attributes. */
      if (attrs.requests[i].domain != bke::AttrDomain::Curve) {
        cache_update(cache->final[subdiv].attributes_buf[i], cache->proc_attributes_buf[i]);
      }
    }
  }

  return cache;
}

GPUVertBuf *curves_pos_buffer_get(Scene *scene, Object *object)
{
  const int subdiv = scene->r.hair_subdiv;
  const int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

  Curves &curves = *static_cast<Curves *>(object->data);
  CurvesEvalCache *cache = curves_cache_get(curves, nullptr, subdiv, thickness_res);

  return cache->final[subdiv].proc_buf;
}

void curves_update(Manager &manager)
{
  manager.submit(*g_pass);
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
}

void curves_free()
{
  MEM_delete(g_pass);
  g_pass = nullptr;
}

template<typename PassT>
GPUBatch *curves_sub_pass_setup_implementation(PassT &sub_ps,
                                               const Scene *scene,
                                               Object *ob,
                                               GPUMaterial *gpu_material)
{
  /** NOTE: This still relies on the old DRW_curves implementation. */

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
  const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
  if (curves.curves_num() >= 1) {
    VArray<float> radii = *curves.attributes().lookup_or_default(
        "radius", bke::AttrDomain::Point, 0.005f);
    const IndexRange first_curve_points = curves.points_by_curve()[0];
    const float first_radius = radii[first_curve_points.first()];
    const float last_radius = radii[first_curve_points.last()];
    const float middle_radius = radii[first_curve_points.size() / 2];
    hair_rad_root = radii[first_curve_points.first()];
    hair_rad_tip = radii[first_curve_points.last()];
    hair_rad_shape = std::clamp(
        math::safe_divide(middle_radius - first_radius, last_radius - first_radius) * 2.0f - 1.0f,
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

    if (request.domain == bke::AttrDomain::Curve) {
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
      curves_infos.is_point_attribute[index][0] = request.domain == bke::AttrDomain::Point;
    }
  }

  curves_infos.push_update();

  sub_ps.bind_ubo("drw_curves", curves_infos);

  sub_ps.push_constant("hairStrandsRes", &curves_cache->final[subdiv].resolution, 1);
  sub_ps.push_constant("hairThicknessRes", thickness_res);
  sub_ps.push_constant("hairRadShape", hair_rad_shape);
  sub_ps.push_constant("hairDupliMatrix", ob->object_to_world());
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
