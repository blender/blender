/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Contains procedural GPU hair drawing methods.
 */

#include "BLT_translation.hh"
#include "DNA_curves_types.h"

#include "BLI_math_base.h"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"

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
#include "draw_curves_defines.hh"
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

void CurvesModule::dispatch(const int curve_count, PassSimple::Sub &pass)
{
  /* Note that the GPU_max_work_group_count can be INT_MAX.
   * Promote to 64bit int to avoid overflow. */
  const int64_t max_strands_per_call = int64_t(GPU_max_work_group_count(0)) *
                                       CURVES_PER_THREADGROUP;
  int strands_start = 0;
  while (strands_start < curve_count) {
    int batch_strands_len = std::min(int64_t(curve_count - strands_start), max_strands_per_call);
    pass.push_constant("curves_start", strands_start);
    pass.push_constant("curves_count", batch_strands_len);
    pass.dispatch(divide_ceil_u(batch_strands_len, CURVES_PER_THREADGROUP));
    strands_start += batch_strands_len;
  }
}

gpu::VertBufPtr CurvesModule::evaluate_topology_indirection(const int curve_count,
                                                            const int point_count,
                                                            CurvesEvalCache &cache,
                                                            bool is_ribbon,
                                                            bool has_cyclic)
{
  int element_count = is_ribbon ? (point_count + curve_count) : (point_count - curve_count);
  if (has_cyclic) {
    element_count += curve_count;
  }
  gpu::VertBufPtr indirection_buf = gpu::VertBuf::device_only<int>(element_count);

  PassSimple::Sub &pass = refine.sub("Topology");
  pass.shader_set(DRW_shader_curves_topology_get());
  pass.bind_ssbo("evaluated_offsets_buf", cache.evaluated_points_by_curve_buf);
  pass.bind_ssbo("curves_cyclic_buf", cache.curves_cyclic_buf);
  pass.bind_ssbo("indirection_buf", indirection_buf);
  pass.push_constant("is_ribbon_topology", is_ribbon);
  pass.push_constant("use_cyclic", has_cyclic);
  dispatch(curve_count, pass);

  return indirection_buf;
}

void CurvesModule::evaluate_curve_attribute(const bool has_catmull,
                                            const bool has_bezier,
                                            const bool has_poly,
                                            const bool has_nurbs,
                                            const bool has_cyclic,
                                            const int curve_count,
                                            CurvesEvalCache &cache,
                                            CurvesEvalShader shader_type,
                                            gpu::VertBufPtr input_buf,
                                            gpu::VertBufPtr &output_buf,
                                            gpu::VertBuf *input2_buf /* = nullptr */,
                                            float4x4 transform /* = float4x4::identity() */)
{
  BLI_assert(input_buf != nullptr);
  BLI_assert(output_buf != nullptr);

  gpu::Shader *shader = DRW_shader_curves_refine_get(shader_type);

  const char *pass_name = nullptr;

  switch (shader_type) {
    case CURVES_EVAL_POSITION:
      pass_name = "Position";
      break;
    case CURVES_EVAL_FLOAT:
      pass_name = "Float Attribute";
      break;
    case CURVES_EVAL_FLOAT2:
      pass_name = "Float2 Attribute";
      break;
    case CURVES_EVAL_FLOAT3:
      pass_name = "Float3 Attribute";
      break;
    case CURVES_EVAL_FLOAT4:
      pass_name = "Float4 Attribute";
      break;
    case CURVES_EVAL_LENGTH_INTERCEPT:
      pass_name = "Length-Intercept Attributes";
      break;
  }

  PassSimple::Sub &pass = refine.sub(pass_name);
  pass.bind_ssbo(POINTS_BY_CURVES_SLOT, cache.points_by_curve_buf);
  pass.bind_ssbo(CURVE_TYPE_SLOT, cache.curves_type_buf);
  pass.bind_ssbo(CURVE_CYCLIC_SLOT, cache.curves_cyclic_buf);
  pass.bind_ssbo(CURVE_RESOLUTION_SLOT, cache.curves_resolution_buf);
  pass.bind_ssbo(EVALUATED_POINT_SLOT, cache.evaluated_points_by_curve_buf);

  switch (shader_type) {
    case CURVES_EVAL_POSITION:
      pass.bind_ssbo(POINT_POSITIONS_SLOT, input_buf);
      pass.bind_ssbo(POINT_RADII_SLOT, input2_buf);
      pass.bind_ssbo(EVALUATED_POS_RAD_SLOT, cache.evaluated_pos_rad_buf);
      /* Move ownership of the radius input vbo to the module. */
      this->transient_buffers.append(gpu::VertBufPtr(input2_buf));
      break;
    case CURVES_EVAL_FLOAT:
    case CURVES_EVAL_FLOAT2:
    case CURVES_EVAL_FLOAT3:
    case CURVES_EVAL_FLOAT4:
      pass.bind_ssbo(POINT_ATTR_SLOT, input_buf);
      pass.bind_ssbo(EVALUATED_ATTR_SLOT, output_buf);
      break;
    case CURVES_EVAL_LENGTH_INTERCEPT:
      pass.bind_ssbo(EVALUATED_POS_RAD_SLOT, cache.evaluated_pos_rad_buf);
      pass.bind_ssbo(EVALUATED_TIME_SLOT, cache.evaluated_time_buf);
      pass.bind_ssbo(CURVES_LENGTH_SLOT, cache.curves_length_buf);
      /* Synchronize positions reads. */
      pass.barrier(GPU_BARRIER_SHADER_STORAGE);
      break;
  }

  if (has_catmull) {
    PassSimple::Sub &sub = pass.sub("Catmull-Rom");
    sub.specialize_constant(shader, "evaluated_type", int(CURVE_TYPE_CATMULL_ROM));
    sub.shader_set(shader);
    /* Dummy, not used for Catmull-Rom. */
    sub.bind_ssbo("handles_positions_left_buf", this->dummy_vbo);
    sub.bind_ssbo("handles_positions_right_buf", this->dummy_vbo);
    sub.bind_ssbo("bezier_offsets_buf", this->dummy_vbo);
    /* Bake object transform for legacy hair particle. */
    sub.push_constant("transform", transform);
    sub.push_constant("use_cyclic", has_cyclic);
    dispatch(curve_count, sub);
  }

  if (has_bezier) {
    PassSimple::Sub &sub = pass.sub("Bezier");
    sub.specialize_constant(shader, "evaluated_type", int(CURVE_TYPE_BEZIER));
    sub.shader_set(shader);
    sub.bind_ssbo("handles_positions_left_buf", cache.handles_positions_left_buf);
    sub.bind_ssbo("handles_positions_right_buf", cache.handles_positions_right_buf);
    sub.bind_ssbo("bezier_offsets_buf", cache.bezier_offsets_buf);
    /* Bake object transform for legacy hair particle. */
    sub.push_constant("transform", transform);
    sub.push_constant("use_cyclic", has_cyclic);
    dispatch(curve_count, sub);
  }

  if (has_nurbs) {
    PassSimple::Sub &sub = pass.sub("Nurbs");
    sub.specialize_constant(shader, "evaluated_type", int(CURVE_TYPE_NURBS));
    sub.shader_set(shader);
    sub.bind_ssbo("curves_resolution_buf", cache.curves_order_buf);
    sub.bind_ssbo("handles_positions_left_buf", cache.basis_cache_buf);
    sub.bind_ssbo("handles_positions_right_buf",
                  cache.control_weights_buf.get() ? cache.control_weights_buf :
                                                    cache.basis_cache_buf);
    sub.bind_ssbo("bezier_offsets_buf", cache.basis_cache_offset_buf);
    sub.push_constant("use_point_weight", cache.control_weights_buf.get() != nullptr);
    /* Bake object transform for legacy hair particle. */
    sub.push_constant("transform", transform);
    sub.push_constant("use_cyclic", has_cyclic);
    dispatch(curve_count, sub);
  }

  if (has_poly) {
    PassSimple::Sub &sub = pass.sub("Poly");
    sub.specialize_constant(shader, "evaluated_type", int(CURVE_TYPE_POLY));
    sub.shader_set(shader);
    /* Dummy, not used for Poly. */
    sub.bind_ssbo("curves_resolution_buf", this->dummy_vbo);
    sub.bind_ssbo("handles_positions_left_buf", this->dummy_vbo);
    sub.bind_ssbo("handles_positions_right_buf", this->dummy_vbo);
    sub.bind_ssbo("bezier_offsets_buf", this->dummy_vbo);
    /* Bake object transform for legacy hair particle. */
    sub.push_constant("transform", transform);
    sub.push_constant("use_cyclic", has_cyclic);
    dispatch(curve_count, sub);
  }

  /* Move ownership of the input vbo to the module. */
  this->transient_buffers.append(std::move(input_buf));
}

void CurvesModule::evaluate_curve_length_intercept(const bool has_cyclic,
                                                   const int curve_count,
                                                   CurvesEvalCache &cache)
{
  gpu::Shader *shader = DRW_shader_curves_refine_get(CURVES_EVAL_LENGTH_INTERCEPT);

  PassSimple::Sub &pass = refine.sub("Length-Intercept Attributes");
  pass.shader_set(shader);
  pass.bind_ssbo(POINTS_BY_CURVES_SLOT, cache.points_by_curve_buf);
  pass.bind_ssbo(CURVE_TYPE_SLOT, cache.curves_type_buf);
  pass.bind_ssbo(CURVE_CYCLIC_SLOT, cache.curves_cyclic_buf);
  pass.bind_ssbo(CURVE_RESOLUTION_SLOT, cache.curves_resolution_buf);
  pass.bind_ssbo(EVALUATED_POINT_SLOT, cache.evaluated_points_by_curve_buf);

  pass.bind_ssbo(EVALUATED_POS_RAD_SLOT, cache.evaluated_pos_rad_buf);
  pass.bind_ssbo(EVALUATED_TIME_SLOT, cache.evaluated_time_buf);
  pass.bind_ssbo(CURVES_LENGTH_SLOT, cache.curves_length_buf);
  pass.barrier(GPU_BARRIER_SHADER_STORAGE);
  /* Bake object transform for legacy hair particle. */
  pass.push_constant("use_cyclic", has_cyclic);
  dispatch(curve_count, pass);
}

static int attribute_index_in_material(const GPUMaterial *gpu_material,
                                       const StringRef name,
                                       bool is_curve_length = false,
                                       bool is_curve_intercept = false)
{
  if (!gpu_material) {
    return -1;
  }

  int index = 0;

  ListBase gpu_attrs = GPU_material_attributes(gpu_material);
  LISTBASE_FOREACH (GPUMaterialAttribute *, gpu_attr, &gpu_attrs) {
    if (gpu_attr->is_hair_length == true) {
      if (gpu_attr->is_hair_length == is_curve_length) {
        return index;
      }
    }
    else if (gpu_attr->is_hair_intercept == true) {
      if (gpu_attr->is_hair_intercept == is_curve_intercept) {
        return index;
      }
    }
    else if (gpu_attr->name == name) {
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
  CurvesModule &module = *drw_get().data->curves_module;

  /* NOTE: This also update legacy hairs too as they populate the same pass. */
  manager.submit(module.refine);
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);

  module.transient_buffers.clear();

  /* Make sure calling this function again will not subdivide the same data. */
  module.refine.init();

  DRW_submission_end();
}

/* New Draw Manager. */

gpu::VertBuf *curves_pos_buffer_get(Object *object)
{
  CurvesModule &module = *drw_get().data->curves_module;
  Curves &curves = DRW_object_get_data_for_drawing<Curves>(*object);

  CurvesEvalCache &cache = curves_get_eval_cache(curves);
  cache.ensure_positions(module, curves.geometry.wrap());

  return cache.evaluated_pos_rad_buf.get();
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

/* Return true if attribute exists in shader. */
static bool set_attribute_type(const GPUMaterial *gpu_material,
                               const StringRef name,
                               CurvesInfosBuf &curves_infos,
                               const bool is_point_domain)
{
  /* Some attributes may not be used in the shader anymore and were not garbage collected yet, so
   * we need to find the right index for this attribute as uniforms defining the scope of the
   * attributes are based on attribute loading order, which is itself based on the material's
   * attributes. */
  const int index = attribute_index_in_material(gpu_material, name);
  if (index == -1) {
    return false;
  }
  curves_infos.is_point_attribute[index][0] = is_point_domain;
  return true;
}

template<typename PassT>
void curves_bind_resources_implementation(PassT &sub_ps,
                                          CurvesModule &module,
                                          CurvesEvalCache &cache,
                                          const int face_per_segment,
                                          GPUMaterial *gpu_material,
                                          gpu::VertBufPtr &indirection_buf,
                                          const std::optional<StringRef> uv_name)
{
  /* Ensure we have no unbound resources.
   * Required for Vulkan.
   * Fixes issues with certain GL drivers not drawing anything. */
  sub_ps.bind_texture("u", module.dummy_vbo);
  sub_ps.bind_texture("au", module.dummy_vbo);
  sub_ps.bind_texture("a", module.dummy_vbo);
  sub_ps.bind_texture("c", module.dummy_vbo);
  sub_ps.bind_texture("ac", module.dummy_vbo);
  sub_ps.bind_texture("l", module.dummy_vbo);
  sub_ps.bind_texture("i", module.dummy_vbo);
  if (gpu_material) {
    ListBase attr_list = GPU_material_attributes(gpu_material);
    ListBaseWrapper<GPUMaterialAttribute> attrs(attr_list);
    for (const GPUMaterialAttribute *attr : attrs) {
      sub_ps.bind_texture(attr->input_name, module.dummy_vbo);
    }
  }

  CurvesInfosBuf &curves_infos = module.ubo_pool.alloc();

  {
    /* TODO(fclem): Compute only if needed. */
    const int index = attribute_index_in_material(gpu_material, "", true, false);
    if (index != -1) {
      sub_ps.bind_texture("l", cache.curves_length_buf);
      curves_infos.is_point_attribute[index][0] = false;
    }
  }
  {
    /* TODO(fclem): Compute only if needed. */
    const int index = attribute_index_in_material(gpu_material, "", false, true);
    if (index != -1) {
      sub_ps.bind_texture("i", cache.evaluated_time_buf);
      curves_infos.is_point_attribute[index][0] = true;
    }
  }

  const VectorSet<std::string> &attrs = cache.attr_used;
  for (const int i : attrs.index_range()) {
    const StringRef name = attrs[i];
    char sampler_name[32];
    drw_curves_get_attribute_sampler_name(name, sampler_name);

    if (cache.attributes_point_domain[i]) {
      if (!cache.evaluated_attributes_buf[i]) {
        continue;
      }
      if (set_attribute_type(gpu_material, name, curves_infos, true)) {
        sub_ps.bind_texture(sampler_name, cache.evaluated_attributes_buf[i]);
      }
      if (name == uv_name) {
        if (set_attribute_type(gpu_material, "", curves_infos, true)) {
          sub_ps.bind_texture("a", cache.evaluated_attributes_buf[i]);
        }
      }
    }
    else {
      if (!cache.curve_attributes_buf[i]) {
        continue;
      }
      if (set_attribute_type(gpu_material, name, curves_infos, false)) {
        sub_ps.bind_texture(sampler_name, cache.curve_attributes_buf[i]);
      }
      if (name == uv_name) {
        if (set_attribute_type(gpu_material, "", curves_infos, false)) {
          sub_ps.bind_texture("a", cache.curve_attributes_buf[i]);
        }
      }
    }
  }

  curves_infos.half_cylinder_face_count = face_per_segment;
  curves_infos.vertex_per_segment = face_per_segment < 2 ? (face_per_segment + 1) :
                                                           ((face_per_segment + 1) * 2 + 1);

  curves_infos.push_update();

  sub_ps.bind_ubo("drw_curves", curves_infos);
  sub_ps.bind_texture("curves_pos_rad_buf", cache.evaluated_pos_rad_buf);
  sub_ps.bind_texture("curves_indirection_buf", indirection_buf);
}

void curves_bind_resources(PassMain::Sub &sub_ps,
                           CurvesModule &module,
                           CurvesEvalCache &cache,
                           const int face_per_segment,
                           GPUMaterial *gpu_material,
                           gpu::VertBufPtr &indirection_buf,
                           const std::optional<StringRef> active_uv_name)
{
  curves_bind_resources_implementation(
      sub_ps, module, cache, face_per_segment, gpu_material, indirection_buf, active_uv_name);
}

void curves_bind_resources(PassSimple::Sub &sub_ps,
                           CurvesModule &module,
                           CurvesEvalCache &cache,
                           const int face_per_segment,
                           GPUMaterial *gpu_material,
                           gpu::VertBufPtr &indirection_buf,
                           const std::optional<StringRef> active_uv_name)
{
  curves_bind_resources_implementation(
      sub_ps, module, cache, face_per_segment, gpu_material, indirection_buf, active_uv_name);
}

template<typename PassT>
gpu::Batch *curves_sub_pass_setup_implementation(PassT &sub_ps,
                                                 const Scene *scene,
                                                 Object *ob,
                                                 const char *&r_error,
                                                 GPUMaterial *gpu_material = nullptr)
{
  BLI_assert(ob->type == OB_CURVES);
  Curves &curves_id = DRW_object_get_data_for_drawing<Curves>(*ob);
  const bke::CurvesGeometry &curves = curves_id.geometry.wrap();

  const int face_per_segment = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND)   ? 0 :
                               (scene->r.hair_type == SCE_HAIR_SHAPE_CYLINDER) ? 3 :
                                                                                 1;

  CurvesEvalCache &curves_cache = curves_get_eval_cache(curves_id);

  if (curves.curves_num() == 0) {
    /* Nothing to draw. Just return an empty drawcall that will be skipped. */
    bool unused_error = false;
    return curves_cache.batch_get(0, 0, face_per_segment, false, unused_error);
  }

  CurvesModule &module = *drw_get().data->curves_module;

  curves_cache.ensure_positions(module, curves);
  curves_cache.ensure_attributes(module, curves, gpu_material);

  gpu::VertBufPtr &indirection_buf = curves_cache.indirection_buf_get(
      module, curves, face_per_segment);

  const std::optional<StringRef> uv_name = get_first_uv_name(
      curves_id.geometry.wrap().attributes());

  curves_bind_resources(
      sub_ps, module, curves_cache, face_per_segment, gpu_material, indirection_buf, uv_name);

  bool error = false;
  gpu::Batch *batch = curves_cache.batch_get(curves.evaluated_points_num(),
                                             curves.curves_num(),
                                             face_per_segment,
                                             curves.has_cyclic_curve(),
                                             error);
  if (error) {
    r_error = RPT_(
        "Error: Curves object contains too many points. "
        "Reduce curve resolution or curve count to fix this issue.\n");
  }
  return batch;
}

gpu::Batch *curves_sub_pass_setup(PassMain::Sub &ps,
                                  const Scene *scene,
                                  Object *ob,
                                  const char *&r_error,
                                  GPUMaterial *gpu_material)
{
  return curves_sub_pass_setup_implementation(ps, scene, ob, r_error, gpu_material);
}

gpu::Batch *curves_sub_pass_setup(PassSimple::Sub &ps,
                                  const Scene *scene,
                                  Object *ob,
                                  const char *&r_error,
                                  GPUMaterial *gpu_material)
{
  return curves_sub_pass_setup_implementation(ps, scene, ob, r_error, gpu_material);
}

}  // namespace blender::draw
