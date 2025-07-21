/* SPDX-FileCopyrightText: 2017 Blender Authors
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
#include "draw_pointcloud_private.hh"
/* For drw_curves_get_attribute_sampler_name. */
#include "draw_curves_private.hh"

namespace blender::draw {

struct PointCloudModule {
  gpu::VertBuf *dummy_vbo = create_dummy_vbo();

  ~PointCloudModule()
  {
    GPU_VERTBUF_DISCARD_SAFE(dummy_vbo);
  }

 private:
  gpu::VertBuf *create_dummy_vbo()
  {
    GPUVertFormat format = {0};
    uint dummy_id = GPU_vertformat_attr_add(
        &format, "dummy", gpu::VertAttrType::SFLOAT_32_32_32_32);

    gpu::VertBuf *vbo = GPU_vertbuf_create_with_format_ex(
        format, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);

    const float vert[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GPU_vertbuf_data_alloc(*vbo, 1);
    GPU_vertbuf_attr_fill(vbo, dummy_id, vert);
    return vbo;
  }
};

void DRW_pointcloud_init(DRWData *drw_data)
{
  if (drw_data == nullptr) {
    drw_data = drw_get().data;
  }
  if (drw_data->pointcloud_module == nullptr) {
    drw_data->pointcloud_module = MEM_new<PointCloudModule>("PointCloudModule");
  }
}

void DRW_pointcloud_module_free(PointCloudModule *pointcloud_module)
{
  MEM_delete(pointcloud_module);
}

template<typename PassT>
gpu::Batch *pointcloud_sub_pass_setup_implementation(PassT &sub_ps,
                                                     Object *object,
                                                     GPUMaterial *gpu_material)
{
  BLI_assert(object->type == OB_POINTCLOUD);
  PointCloud &pointcloud = DRW_object_get_data_for_drawing<PointCloud>(*object);
  /* An empty point cloud should never result in a draw-call. However, the buffer binding commands
   * will still be executed. In this case, in order to avoid assertion, we bind dummy VBOs. */
  bool is_empty = pointcloud.totpoint == 0;

  PointCloudModule &module = *drw_get().data->pointcloud_module;
  /* Ensure we have no unbound resources.
   * Required for Vulkan.
   * Fixes issues with certain GL drivers not drawing anything. */
  sub_ps.bind_texture("u", module.dummy_vbo);
  sub_ps.bind_texture("au", module.dummy_vbo);
  sub_ps.bind_texture("a", module.dummy_vbo);
  sub_ps.bind_texture("c", module.dummy_vbo);
  sub_ps.bind_texture("ac", module.dummy_vbo);

  gpu::VertBuf *pos_rad_buf = pointcloud_position_and_radius_get(&pointcloud);
  sub_ps.bind_texture("ptcloud_pos_rad_tx", is_empty ? module.dummy_vbo : pos_rad_buf);

  if (gpu_material != nullptr) {
    ListBase gpu_attrs = GPU_material_attributes(gpu_material);
    LISTBASE_FOREACH (GPUMaterialAttribute *, gpu_attr, &gpu_attrs) {
      char sampler_name[32];
      /** NOTE: Reusing curve attribute function. */
      drw_curves_get_attribute_sampler_name(gpu_attr->name, sampler_name);

      gpu::VertBuf **attribute_buf = DRW_pointcloud_evaluated_attribute(&pointcloud,
                                                                        gpu_attr->name);
      sub_ps.bind_texture(sampler_name,
                          (attribute_buf && !is_empty) ? attribute_buf : &module.dummy_vbo);
    }
  }

  gpu::Batch *geom = pointcloud_surface_get(&pointcloud);
  return geom;
}

gpu::Batch *pointcloud_sub_pass_setup(PassMain::Sub &sub_ps,
                                      Object *object,
                                      GPUMaterial *gpu_material)
{
  return pointcloud_sub_pass_setup_implementation(sub_ps, object, gpu_material);
}

gpu::Batch *pointcloud_sub_pass_setup(PassSimple::Sub &sub_ps,
                                      Object *object,
                                      GPUMaterial *gpu_material)
{
  return pointcloud_sub_pass_setup_implementation(sub_ps, object, gpu_material);
}

}  // namespace blender::draw
