/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Contains procedural GPU hair drawing methods.
 */

#include "BLI_utildefines.h"

#include "DNA_curves_types.h"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"

#include "GPU_batch.hh"
#include "GPU_capabilities.hh"
#include "GPU_material.hh"
#include "GPU_shader.hh"
#include "GPU_texture.hh"
#include "GPU_vertex_buffer.hh"

#include "DRW_gpu_wrapper.hh"
#include "DRW_render.hh"

#include "draw_cache_impl.hh"
#include "draw_common.hh"
#include "draw_context_private.hh"
#include "draw_curves_private.hh"
#include "draw_hair_private.hh"
#include "draw_shader.hh"

namespace blender::draw {

using CurvesInfosBuf = UniformBuffer<CurvesInfos>;

CurvesInfosBuf &CurvesUniformBufPool::alloc()
{
  CurvesInfosBuf *ptr;
  if (used >= ubos.size()) {
    ubos.append(std::make_unique<CurvesInfosBuf>());
    ptr = ubos.last().get();
  }
  else {
    ptr = ubos[used++].get();
  }

  memset(ptr->data(), 0, sizeof(CurvesInfos));
  return *ptr;
}

gpu::VertBuf *CurvesModule::drw_curves_ensure_dummy_vbo()
{
  GPUVertFormat format = {0};
  uint dummy_id = GPU_vertformat_attr_add(&format, "dummy", gpu::VertAttrType::SFLOAT_32_32_32_32);

  gpu::VertBuf *vbo = GPU_vertbuf_create_with_format_ex(
      format, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);

  const float vert[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  GPU_vertbuf_data_alloc(*vbo, 1);
  GPU_vertbuf_attr_fill(vbo, dummy_id, vert);
  /* Create vbo immediately to bind to texture buffer. */
  GPU_vertbuf_use(vbo);
  return vbo;
}

void DRW_curves_init(DRWData *drw_data)
{
  if (drw_data == nullptr) {
    drw_data = drw_get().data;
  }
  if (drw_data->curves_module == nullptr) {
    drw_data->curves_module = MEM_new<CurvesModule>("CurvesModule");
  }
}

void DRW_curves_begin_sync(DRWData *drw_data)
{
  drw_data->curves_module->init();
}

void DRW_curves_module_free(CurvesModule *curves_module)
{
  MEM_delete(curves_module);
}

static void drw_curves_cache_update_compute(CurvesEvalCache *cache,
                                            const int curves_num,
                                            gpu::VertBuf *output_buf,
                                            gpu::VertBuf *input_buf)
{
  BLI_assert(input_buf != nullptr);
  BLI_assert(output_buf != nullptr);
  GPUShader *shader = DRW_shader_curves_refine_get(CURVES_EVAL_CATMULL_ROM);

  /* TODO(fclem): Remove Global access. */
  PassSimple &pass = drw_get().data->curves_module->refine;
  pass.shader_set(shader);
  pass.bind_texture("hairPointBuffer", input_buf);
  pass.bind_texture("hairStrandBuffer", cache->proc_strand_buf);
  pass.bind_texture("hairStrandSegBuffer", cache->proc_strand_seg_buf);
  pass.push_constant("hairStrandsRes", &cache->final.resolution);
  pass.bind_ssbo("posTime", output_buf);

  const int max_strands_per_call = GPU_max_work_group_count(0);
  int strands_start = 0;
  while (strands_start < curves_num) {
    int batch_strands_len = std::min(curves_num - strands_start, max_strands_per_call);
    pass.push_constant("hairStrandOffset", strands_start);
    pass.dispatch(int3(batch_strands_len, cache->final.resolution, 1));
    strands_start += batch_strands_len;
  }
}

static void drw_curves_cache_update_compute(CurvesEvalCache *cache)
{
  const int curves_num = cache->curves_num;
  const int final_points_len = cache->final.resolution * curves_num;
  if (final_points_len == 0) {
    return;
  }

  drw_curves_cache_update_compute(cache, curves_num, cache->final.proc_buf, cache->proc_point_buf);

  const VectorSet<std::string> &attrs = cache->final.attr_used;
  for (const int i : attrs.index_range()) {
    if (!cache->proc_attributes_point_domain[i]) {
      continue;
    }
    drw_curves_cache_update_compute(
        cache, curves_num, cache->final.attributes_buf[i], cache->proc_attributes_buf[i]);
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
    drw_curves_cache_update_compute(cache);
  }
  return cache;
}

gpu::VertBuf *DRW_curves_pos_buffer_get(Object *object)
{
  const DRWContext *draw_ctx = DRW_context_get();
  const Scene *scene = draw_ctx->scene;

  const int subdiv = scene->r.hair_subdiv;
  const int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

  Curves &curves = DRW_object_get_data_for_drawing<Curves>(*object);
  CurvesEvalCache *cache = drw_curves_cache_get(curves, nullptr, subdiv, thickness_res);

  return cache->final.proc_buf;
}

static int attribute_index_in_material(GPUMaterial *gpu_material, const StringRef name)
{
  if (!gpu_material) {
    return -1;
  }

  int index = 0;

  ListBase gpu_attrs = GPU_material_attributes(gpu_material);
  LISTBASE_FOREACH (GPUMaterialAttribute *, gpu_attr, &gpu_attrs) {
    if (gpu_attr->name == name) {
      return index;
    }

    index++;
  }

  return -1;
}

void DRW_curves_update(draw::Manager &manager)
{
  DRW_submission_start();

  /* TODO(fclem): Remove Global access. */
  PassSimple &pass = drw_get().data->curves_module->refine;

  /* NOTE: This also update legacy hairs too as they populate the same pass. */
  manager.submit(pass);
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);

  /* Make sure calling this function again will not subdivide the same data. */
  pass.init();

  DRW_submission_end();
}

/* New Draw Manager. */

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
  const int final_points_len = cache->final.resolution * curves_num;

  CurvesModule &module = *drw_get().data->curves_module;

  auto cache_update = [&](gpu::VertBuf *output_buf, gpu::VertBuf *input_buf) {
    PassSimple::Sub &ob_ps = module.refine.sub("Object Pass");

    ob_ps.shader_set(DRW_shader_curves_refine_get(CURVES_EVAL_CATMULL_ROM));

    ob_ps.bind_texture("hairPointBuffer", input_buf);
    ob_ps.bind_texture("hairStrandBuffer", cache->proc_strand_buf);
    ob_ps.bind_texture("hairStrandSegBuffer", cache->proc_strand_seg_buf);
    ob_ps.push_constant("hairStrandsRes", &cache->final.resolution);
    ob_ps.bind_ssbo("posTime", output_buf);

    const int max_strands_per_call = GPU_max_work_group_count(0);
    int strands_start = 0;
    while (strands_start < curves_num) {
      int batch_strands_len = std::min(curves_num - strands_start, max_strands_per_call);
      PassSimple::Sub &sub_ps = ob_ps.sub("Sub Pass");
      sub_ps.push_constant("hairStrandOffset", strands_start);
      sub_ps.dispatch(int3(batch_strands_len, cache->final.resolution, 1));
      strands_start += batch_strands_len;
    }
  };

  if (final_points_len > 0) {
    cache_update(cache->final.proc_buf, cache->proc_point_buf);

    const VectorSet<std::string> &attrs = cache->final.attr_used;
    for (const int i : attrs.index_range()) {
      /* Only refine point attributes. */
      if (cache->proc_attributes_point_domain[i]) {
        cache_update(cache->final.attributes_buf[i], cache->proc_attributes_buf[i]);
      }
    }
  }

  return cache;
}

gpu::VertBuf *curves_pos_buffer_get(Scene *scene, Object *object)
{
  const int subdiv = scene->r.hair_subdiv;
  const int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

  Curves &curves = DRW_object_get_data_for_drawing<Curves>(*object);
  CurvesEvalCache *cache = curves_cache_get(curves, nullptr, subdiv, thickness_res);

  return cache->final.proc_buf;
}

static std::optional<StringRef> get_first_uv_name(const bke::AttributeAccessor &attributes)
{
  std::optional<StringRef> name;
  attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.data_type == bke::AttrType::Float2) {
      name = iter.name;
      iter.stop();
    }
  });
  return name;
}

template<typename PassT>
gpu::Batch *curves_sub_pass_setup_implementation(PassT &sub_ps,
                                                 const Scene *scene,
                                                 Object *ob,
                                                 GPUMaterial *gpu_material)
{
  /** NOTE: This still relies on the old DRW_curves implementation. */

  CurvesModule &module = *drw_get().data->curves_module;
  CurvesInfosBuf &curves_infos = module.ubo_pool.alloc();
  BLI_assert(ob->type == OB_CURVES);
  Curves &curves_id = DRW_object_get_data_for_drawing<Curves>(*ob);

  const int subdiv = scene->r.hair_subdiv;
  const int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

  CurvesEvalCache *curves_cache = drw_curves_cache_get(
      curves_id, gpu_material, subdiv, thickness_res);

  /* Ensure we have no unbound resources.
   * Required for Vulkan.
   * Fixes issues with certain GL drivers not drawing anything. */
  sub_ps.bind_texture("u", module.dummy_vbo);
  sub_ps.bind_texture("au", module.dummy_vbo);
  sub_ps.bind_texture("a", module.dummy_vbo);
  sub_ps.bind_texture("c", module.dummy_vbo);
  sub_ps.bind_texture("ac", module.dummy_vbo);
  if (gpu_material) {
    ListBase attr_list = GPU_material_attributes(gpu_material);
    ListBaseWrapper<GPUMaterialAttribute> attrs(attr_list);
    for (const GPUMaterialAttribute *attr : attrs) {
      sub_ps.bind_texture(attr->input_name, module.dummy_vbo);
    }
  }

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

  sub_ps.bind_texture("hairPointBuffer", curves_cache->final.proc_buf);
  if (curves_cache->proc_length_buf) {
    sub_ps.bind_texture("l", curves_cache->proc_length_buf);
  }

  const std::optional<StringRef> uv_name = get_first_uv_name(
      curves_id.geometry.wrap().attributes());
  const VectorSet<std::string> &attrs = curves_cache->final.attr_used;
  for (const int i : attrs.index_range()) {
    const StringRef name = attrs[i];
    char sampler_name[32];
    drw_curves_get_attribute_sampler_name(name, sampler_name);

    if (!curves_cache->proc_attributes_point_domain[i]) {
      if (!curves_cache->proc_attributes_buf[i]) {
        continue;
      }
      sub_ps.bind_texture(sampler_name, curves_cache->proc_attributes_buf[i]);
      if (name == uv_name) {
        sub_ps.bind_texture("a", curves_cache->proc_attributes_buf[i]);
      }
    }
    else {
      if (!curves_cache->final.attributes_buf[i]) {
        continue;
      }
      sub_ps.bind_texture(sampler_name, curves_cache->final.attributes_buf[i]);
      if (name == uv_name) {
        sub_ps.bind_texture("a", curves_cache->final.attributes_buf[i]);
      }
    }

    /* Some attributes may not be used in the shader anymore and were not garbage collected yet, so
     * we need to find the right index for this attribute as uniforms defining the scope of the
     * attributes are based on attribute loading order, which is itself based on the material's
     * attributes. */
    const int index = attribute_index_in_material(gpu_material, name);
    if (index != -1) {
      curves_infos.is_point_attribute[index][0] = curves_cache->proc_attributes_point_domain[i];
    }
  }

  curves_infos.push_update();

  sub_ps.bind_ubo("drw_curves", curves_infos);

  sub_ps.push_constant("hairStrandsRes", &curves_cache->final.resolution, 1);
  sub_ps.push_constant("hairThicknessRes", thickness_res);
  sub_ps.push_constant("hairRadShape", hair_rad_shape);
  sub_ps.push_constant("hairDupliMatrix", ob->object_to_world());
  sub_ps.push_constant("hairRadRoot", hair_rad_root);
  sub_ps.push_constant("hairRadTip", hair_rad_tip);
  sub_ps.push_constant("hairCloseTip", hair_close_tip);

  return curves_cache->final.proc_hairs;
}

gpu::Batch *curves_sub_pass_setup(PassMain::Sub &ps,
                                  const Scene *scene,
                                  Object *ob,
                                  GPUMaterial *gpu_material)
{
  return curves_sub_pass_setup_implementation(ps, scene, ob, gpu_material);
}

gpu::Batch *curves_sub_pass_setup(PassSimple::Sub &ps,
                                  const Scene *scene,
                                  Object *ob,
                                  GPUMaterial *gpu_material)
{
  return curves_sub_pass_setup_implementation(ps, scene, ob, gpu_material);
}

}  // namespace blender::draw
