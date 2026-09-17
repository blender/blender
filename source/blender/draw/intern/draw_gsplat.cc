/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "DNA_pointcloud_types.h"

#include "GPU_batch.hh"
#include "GPU_material.hh"
#include "GPU_shader.hh"
#include "GPU_texture.hh"
#include "GPU_vertex_buffer.hh"

#include "DRW_render.hh"

#include "draw_cache_impl.hh"
#include "draw_common.hh"
#include "draw_common_c.hh"
#include "draw_context_private.hh"
#include "draw_gsplat_private.hh"
#include "draw_shader.hh"
/* For drw_curves_get_attribute_sampler_name. */
#include "draw_curves_private.hh"

namespace blender::draw {

void DRW_gsplat_init(DRWData *drw_data)
{
  if (drw_data == nullptr) {
    drw_data = drw_get().data;
  }
  if (drw_data->gsplat_module == nullptr) {
    drw_data->gsplat_module = MEM_new<GSplatModule>("GSplatModule");
  }
}

void DRW_gsplat_module_free(GSplatModule *gsplat_module)
{
  MEM_delete(gsplat_module);
}

namespace detail {

template<typename PassT>
void compute_pass_setup(PassT &ps, const ObjectRef &ob_ref, const ResourceHandleRange &res_handle)
{
  PointCloud &pointcloud = DRW_object_get_data_for_drawing<PointCloud>(*ob_ref.object);
  GSplatEvalCache &cache = GSplatEvalCache::get(pointcloud);

  /* TODO(not_mark): account for instancing in compute pass, otherwise int(res_handle.index()). */
  const int resource_id = res_handle.id_range().first();
  const float2 viewport_size = DRW_context_get()->viewport_size_get();

  auto &sub_ps = ps.sub("GSplatSubPass");

  sub_ps.bind_ubo("infos", cache.infos_buf_get());

  sub_ps.bind_ssbo("shape_data", cache.shape_data_buf_get());
  sub_ps.bind_ssbo("radiance_data", cache.radiance_data_buf_get());
  sub_ps.bind_ssbo("ellipses_comp", cache.ellipses_comp_buf_get());
  sub_ps.bind_ssbo("radiance_comp", cache.radiance_comp_buf_get());

  sub_ps.push_constant("viewport_size", viewport_size);
  sub_ps.push_constant("num_points", pointcloud.totpoint);
  sub_ps.push_constant("resource_id", resource_id);

  sub_ps.dispatch(divide_ceil_u(pointcloud.totpoint, DRW_GSPLAT_GROUP_SIZE));
}

template<typename PassT>
gpu::Batch *draw_subpass_setup(PassT &sub_ps,
                               const ObjectRef &ob_ref,
                               const ResourceHandleRange &res_handle,
                               GPUMaterial *gpu_material)
{
  const Object *object = ob_ref.object;
  BLI_assert(ob_ref.object->type == OB_POINTCLOUD);

  PointCloud &pointcloud = DRW_object_get_data_for_drawing<PointCloud>(*object);
  if (pointcloud.type != PointCloudType::GSplat) {
    return nullptr;
  }

  /* An empty pointcloud should never result in a draw-call. However, the buffer binding commands
   * will still be executed. In this case, in order to avoid assertion, we bind dummy VBOs. */
  const bool is_empty = pointcloud.totpoint == 0;

  /* GSplats currently rely on per-object, per-view compute prepasses, and geometry subpasses then
   * mostly resolve their outputs into the view. */
  GSplatModule &module = *drw_get().data->gsplat_module;
  if (!is_empty) {
    module.sync_object(ob_ref, res_handle);
  }

  /* Ensure we have no unbound resources, required for Vulkan.
   * Certain GL drivers also refuse to draw anything otherwise. */
  sub_ps.bind_texture("u", module.dummy_vbo);
  sub_ps.bind_texture("au", module.dummy_vbo);
  sub_ps.bind_texture("a", module.dummy_vbo);
  sub_ps.bind_texture("c", module.dummy_vbo);
  sub_ps.bind_texture("ac", module.dummy_vbo);

  GSplatEvalCache &cache = GSplatEvalCache::get(pointcloud);
  sub_ps.bind_ubo("infos", is_empty ? module.dummy_ubo : cache.infos_buf_get());
  sub_ps.bind_texture("shape_data_tx", is_empty ? module.dummy_vbo : cache.shape_data_buf_get());
  sub_ps.bind_texture("ellipses_comp_tx",
                      is_empty ? module.dummy_vbo : cache.ellipses_comp_buf_get());
  sub_ps.bind_texture("radiance_comp_tx",
                      is_empty ? module.dummy_vbo : cache.radiance_comp_buf_get());

  float2 viewport_size = DRW_context_get()->viewport_size_get();
  sub_ps.push_constant("viewport_size", viewport_size);

  if (gpu_material != nullptr) {
    ListBaseT<GPUMaterialAttribute> gpu_attrs = GPU_material_attributes(gpu_material);
    for (GPUMaterialAttribute &gpu_attr : gpu_attrs) {
      /** NOTE: Reusing curve attribute function. */
      char sampler_name[32];
      drw_curves_get_attribute_sampler_name(gpu_attr.name, sampler_name);

      if (STREQ(gpu_attr.name, "radiance")) {
        /* Radiance attribute is specifically for gsplat spherical harmonics output. */
        sub_ps.bind_texture(sampler_name,
                            is_empty ? module.dummy_vbo : cache.radiance_comp_buf_get());
      }
      else {
        gpu::VertBuf **attribute_buf = DRW_gsplat_evaluated_attribute(&pointcloud, gpu_attr.name);
        sub_ps.bind_texture(sampler_name,
                            (attribute_buf && !is_empty) ? attribute_buf : &module.dummy_vbo);
      }
    }
  }

  return cache.surface_get(pointcloud);
}

}  // namespace detail

void GSplatModule::begin_sync()
{
  compute_objects_.clear();

  compute_ellipses_ps_.init();
  compute_ellipses_ps_.state_set(DRW_STATE_NO_DRAW);
  compute_ellipses_ps_.shader_set(DRW_shader_gsplat_compute_get(GSplatEvalShader::Ellipses));

  compute_radiance_ps_.init();
  compute_radiance_ps_.state_set(DRW_STATE_NO_DRAW);
  compute_radiance_ps_.shader_set(DRW_shader_gsplat_compute_get(GSplatEvalShader::Radiance));

  compute_ellipses_radiance_ps_.init();
  compute_ellipses_radiance_ps_.state_set(DRW_STATE_NO_DRAW);
  compute_ellipses_radiance_ps_.shader_set(
      DRW_shader_gsplat_compute_get(GSplatEvalShader::EllipsesRadiance));
}

void GSplatModule::sync_object(const ObjectRef &ob_ref, const ResourceHandleRange &res_handle)
{
  /* Do not create subpasses if these are already created for this object
   * in an earlier object_sync. Passes can be reused across submissions. */
  /* TODO(not_mark): account for instancing in sync. For now, take first object. */
  ObjectKey ob_key = ob_ref.is_range() ? ObjectKey(ob_ref, 0) : ObjectKey(ob_ref);
  if (compute_objects_.contains(ob_key)) {
    return;
  }
  compute_objects_.add(ob_key);

  /* Synchronize compute passes. */
  detail::compute_pass_setup(compute_ellipses_ps_, ob_ref, res_handle);
  detail::compute_pass_setup(compute_radiance_ps_, ob_ref, res_handle);
  detail::compute_pass_setup(compute_ellipses_radiance_ps_, ob_ref, res_handle);
}

void GSplatModule::update(draw::Manager &manager, draw::View &view, GSplatEvalShader type)
{
  if (compute_objects_.is_empty()) {
    /* No objects synced, skip submission. */
    return;
  }
  manager.submit(get_ps(type), view);
}

void DRW_gsplat_begin_sync()
{
  GSplatModule &module = *drw_get().data->gsplat_module;
  module.begin_sync();
}

void DRW_gsplat_ensure_ellipses(draw::Manager &manager, draw::View &view)
{
  GSplatModule &module = *drw_get().data->gsplat_module;
  module.update(manager, view, GSplatEvalShader::Ellipses);
}

void DRW_gsplat_ensure_radiance(draw::Manager &manager, draw::View &view)
{
  GSplatModule &module = *drw_get().data->gsplat_module;
  module.update(manager, view, GSplatEvalShader::Radiance);
}

void DRW_gsplat_ensure_ellipses_radiance(draw::Manager &manager, draw::View &view)
{
  GSplatModule &module = *drw_get().data->gsplat_module;
  module.update(manager, view, GSplatEvalShader::EllipsesRadiance);
}

bool pointcloud_is_gsplat(Object *object)
{
  /* GSplats and pointclouds share the base DNA type. We treat them separately
   * in draw engines as the implementations are quite different. This check is
   * done often, so it is kept as a helper function for now. */
  if (object->type == OB_POINTCLOUD) {
    PointCloud &pointcloud = DRW_object_get_data_for_drawing<PointCloud>(*object);
    return pointcloud.type == PointCloudType::GSplat;
  }
  return false;
}

gpu::Batch *gsplat_sub_pass_setup(PassMain::Sub &sub_ps,
                                  const ObjectRef &ob_ref,
                                  const ResourceHandleRange &res_handle,
                                  GPUMaterial *gpu_material)
{
  return detail::draw_subpass_setup(sub_ps, ob_ref, res_handle, gpu_material);
}

gpu::Batch *gsplat_sub_pass_setup(PassSimple::Sub &sub_ps,
                                  const ObjectRef &ob_ref,
                                  const ResourceHandleRange &res_handle,
                                  GPUMaterial *gpu_material)
{
  return detail::draw_subpass_setup(sub_ps, ob_ref, res_handle, gpu_material);
}

}  // namespace blender::draw
