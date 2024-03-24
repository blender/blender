/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "DNA_pointcloud_types.h"

#include "GPU_batch.hh"
#include "GPU_capabilities.hh"
#include "GPU_compute.hh"
#include "GPU_material.hh"
#include "GPU_shader.hh"
#include "GPU_texture.hh"
#include "GPU_vertex_buffer.hh"

#include "DRW_gpu_wrapper.hh"
#include "DRW_render.hh"

#include "draw_attributes.hh"
#include "draw_cache_impl.hh"
#include "draw_common.hh"
#include "draw_common_c.hh"
#include "draw_manager_c.hh"
#include "draw_pointcloud_private.hh"
/* For drw_curves_get_attribute_sampler_name. */
#include "draw_curves_private.hh"

namespace blender::draw {

static gpu::VertBuf *g_dummy_vbo = nullptr;

void DRW_pointcloud_init()
{
  if (g_dummy_vbo == nullptr) {
    /* initialize vertex format */
    GPUVertFormat format = {0};
    uint dummy_id = GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

    g_dummy_vbo = GPU_vertbuf_create_with_format_ex(
        &format, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);

    const float vert[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GPU_vertbuf_data_alloc(g_dummy_vbo, 1);
    GPU_vertbuf_attr_fill(g_dummy_vbo, dummy_id, vert);
  }
}

DRWShadingGroup *DRW_shgroup_pointcloud_create_sub(Object *object,
                                                   DRWShadingGroup *shgrp_parent,
                                                   GPUMaterial *gpu_material)
{
  PointCloud &pointcloud = *static_cast<PointCloud *>(object->data);

  DRWShadingGroup *shgrp = DRW_shgroup_create_sub(shgrp_parent);

  /* Fix issue with certain driver not drawing anything if there is no texture bound to
   * "ac", "au", "u" or "c". */
  DRW_shgroup_buffer_texture(shgrp, "u", g_dummy_vbo);
  DRW_shgroup_buffer_texture(shgrp, "au", g_dummy_vbo);
  DRW_shgroup_buffer_texture(shgrp, "c", g_dummy_vbo);
  DRW_shgroup_buffer_texture(shgrp, "ac", g_dummy_vbo);

  gpu::VertBuf *pos_rad_buf = pointcloud_position_and_radius_get(&pointcloud);
  DRW_shgroup_buffer_texture(shgrp, "ptcloud_pos_rad_tx", pos_rad_buf);

  if (gpu_material != nullptr) {

    // const DRW_Attributes &attrs = cache->attr_used;
    // for (int i = 0; i < attrs.num_requests; i++) {
    //   const DRW_AttributeRequest &request = attrs.requests[i];

    //   char sampler_name[32];
    //   /* \note reusing curve attribute function. */
    //   drw_curves_get_attribute_sampler_name(request.attribute_name, sampler_name);

    //   GPUTexture *attribute_buf = DRW_pointcloud_evaluated_attribute(&pointcloud);
    //   if (!cache->attributes_buf[i]) {
    //     continue;
    //   }
    //   DRW_shgroup_buffer_texture_ref(shgrp, sampler_name, attribute_buf);
    // }

    /* Only single material supported for now. */
    GPUBatch **geom = pointcloud_surface_shaded_get(&pointcloud, &gpu_material, 1);
    DRW_shgroup_call(shgrp, geom[0], object);
  }
  else {
    GPUBatch *geom = pointcloud_surface_get(&pointcloud);
    DRW_shgroup_call(shgrp, geom, object);
  }
  return shgrp;
}

void DRW_pointcloud_free()
{
  GPU_VERTBUF_DISCARD_SAFE(g_dummy_vbo);
}

template<typename PassT>
GPUBatch *point_cloud_sub_pass_setup_implementation(PassT &sub_ps,
                                                    Object *object,
                                                    GPUMaterial *gpu_material)
{
  BLI_assert(object->type == OB_POINTCLOUD);
  PointCloud &pointcloud = *static_cast<PointCloud *>(object->data);

  /* Fix issue with certain driver not drawing anything if there is no texture bound to
   * "ac", "au", "u" or "c". */
  sub_ps.bind_texture("u", g_dummy_vbo);
  sub_ps.bind_texture("au", g_dummy_vbo);
  sub_ps.bind_texture("c", g_dummy_vbo);
  sub_ps.bind_texture("ac", g_dummy_vbo);

  gpu::VertBuf *pos_rad_buf = pointcloud_position_and_radius_get(&pointcloud);
  sub_ps.bind_texture("ptcloud_pos_rad_tx", pos_rad_buf);

  if (gpu_material != nullptr) {
    ListBase gpu_attrs = GPU_material_attributes(gpu_material);
    LISTBASE_FOREACH (GPUMaterialAttribute *, gpu_attr, &gpu_attrs) {
      char sampler_name[32];
      /** NOTE: Reusing curve attribute function. */
      drw_curves_get_attribute_sampler_name(gpu_attr->name, sampler_name);

      gpu::VertBuf **attribute_buf = DRW_pointcloud_evaluated_attribute(&pointcloud,
                                                                        gpu_attr->name);
      sub_ps.bind_texture(sampler_name, (attribute_buf) ? attribute_buf : &g_dummy_vbo);
    }
  }

  GPUBatch *geom = pointcloud_surface_get(&pointcloud);
  return geom;
}

GPUBatch *point_cloud_sub_pass_setup(PassMain::Sub &sub_ps,
                                     Object *object,
                                     GPUMaterial *gpu_material)
{
  return point_cloud_sub_pass_setup_implementation(sub_ps, object, gpu_material);
}

GPUBatch *point_cloud_sub_pass_setup(PassSimple::Sub &sub_ps,
                                     Object *object,
                                     GPUMaterial *gpu_material)
{
  return point_cloud_sub_pass_setup_implementation(sub_ps, object, gpu_material);
}

}  // namespace blender::draw
