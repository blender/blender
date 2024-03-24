/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.hh"

#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_math_vector.hh"
#include "BLI_span.hh"

#include "GPU_batch.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_duplilist.hh"
#include "BKE_geometry_set.hh"

#include "draw_cache_extract.hh"
#include "draw_cache_impl.hh"
#include "overlay_private.hh"

void OVERLAY_viewer_attribute_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  const DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL |
                         DRW_STATE_BLEND_ALPHA;
  DRW_PASS_CREATE(psl->attribute_ps, state | pd->clipping_state);

  GPUShader *mesh_sh = OVERLAY_shader_viewer_attribute_mesh();
  GPUShader *pointcloud_sh = OVERLAY_shader_viewer_attribute_pointcloud();
  GPUShader *curve_sh = OVERLAY_shader_viewer_attribute_curve();
  GPUShader *curves_sh = OVERLAY_shader_viewer_attribute_curves();
  GPUShader *uniform_sh = OVERLAY_shader_uniform_color();
  GPUShader *uniform_pointcloud_sh = OVERLAY_shader_uniform_color_pointcloud();
  pd->viewer_attribute_mesh_grp = DRW_shgroup_create(mesh_sh, psl->attribute_ps);
  pd->viewer_attribute_pointcloud_grp = DRW_shgroup_create(pointcloud_sh, psl->attribute_ps);
  pd->viewer_attribute_curve_grp = DRW_shgroup_create(curve_sh, psl->attribute_ps);
  pd->viewer_attribute_curves_grp = DRW_shgroup_create(curves_sh, psl->attribute_ps);
  pd->viewer_attribute_instance_grp = DRW_shgroup_create(uniform_sh, psl->attribute_ps);
  pd->viewer_attribute_instance_pointcloud_grp = DRW_shgroup_create(uniform_pointcloud_sh,
                                                                    psl->attribute_ps);
}

static void populate_cache_for_instance(Object &object,
                                        OVERLAY_PrivateData &pd,
                                        const DupliObject &dupli_object,
                                        const float opacity)
{
  using namespace blender;
  using namespace blender::bke;
  using namespace blender::draw;

  const GeometrySet &base_geometry = *dupli_object.preview_base_geometry;
  const InstancesComponent &instances = *base_geometry.get_component<InstancesComponent>();
  const AttributeAccessor instance_attributes = *instances.attributes();
  const VArray attribute = *instance_attributes.lookup<ColorGeometry4f>(".viewer");
  if (!attribute) {
    return;
  }
  ColorGeometry4f color = attribute.get(dupli_object.preview_instance_index);
  color.a *= opacity;
  switch (object.type) {
    case OB_MESH: {
      {
        DRWShadingGroup *sub_grp = DRW_shgroup_create_sub(pd.viewer_attribute_instance_grp);
        DRW_shgroup_uniform_vec4_copy(sub_grp, "ucolor", color);
        GPUBatch *batch = DRW_cache_mesh_surface_get(&object);
        DRW_shgroup_call(sub_grp, batch, &object);
      }
      if (GPUBatch *batch = DRW_cache_mesh_loose_edges_get(&object)) {
        DRWShadingGroup *sub_grp = DRW_shgroup_create_sub(pd.viewer_attribute_instance_grp);
        DRW_shgroup_uniform_vec4_copy(sub_grp, "ucolor", color);
        DRW_shgroup_call(sub_grp, batch, &object);
      }
      break;
    }
    case OB_POINTCLOUD: {
      DRWShadingGroup *sub_grp = DRW_shgroup_pointcloud_create_sub(
          &object, pd.viewer_attribute_pointcloud_grp, nullptr);
      DRW_shgroup_uniform_vec4_copy(sub_grp, "ucolor", color);
      break;
    }
    case OB_CURVES_LEGACY: {
      DRWShadingGroup *sub_grp = DRW_shgroup_create_sub(pd.viewer_attribute_instance_grp);
      DRW_shgroup_uniform_vec4_copy(sub_grp, "ucolor", color);
      GPUBatch *batch = DRW_cache_curve_edge_wire_get(&object);
      DRW_shgroup_call_obmat(sub_grp, batch, object.object_to_world().ptr());
      break;
    }
    case OB_CURVES: {
      /* Not supported yet because instances of this type are currently drawn as legacy curves.
       */
      break;
    }
  }
}

static bool attribute_type_supports_viewer_overlay(const eCustomDataType data_type)
{
  return CD_TYPE_AS_MASK(data_type) & (CD_MASK_PROP_ALL & ~CD_MASK_PROP_QUATERNION);
}

static void populate_cache_for_geometry(Object &object,
                                        OVERLAY_PrivateData &pd,
                                        const float opacity)
{
  using namespace blender;
  using namespace blender::bke;
  using namespace blender::draw;

  switch (object.type) {
    case OB_MESH: {
      Mesh *mesh = static_cast<Mesh *>(object.data);
      if (const std::optional<bke::AttributeMetaData> meta_data =
              mesh->attributes().lookup_meta_data(".viewer"))
      {
        if (attribute_type_supports_viewer_overlay(meta_data->data_type)) {
          GPUBatch *batch = DRW_cache_mesh_surface_viewer_attribute_get(&object);
          DRW_shgroup_uniform_float_copy(pd.viewer_attribute_mesh_grp, "opacity", opacity);
          DRW_shgroup_call(pd.viewer_attribute_mesh_grp, batch, &object);
        }
      }
      break;
    }
    case OB_POINTCLOUD: {
      PointCloud *pointcloud = static_cast<PointCloud *>(object.data);
      if (const std::optional<bke::AttributeMetaData> meta_data =
              pointcloud->attributes().lookup_meta_data(".viewer"))
      {
        if (attribute_type_supports_viewer_overlay(meta_data->data_type)) {
          gpu::VertBuf **vertbuf = DRW_pointcloud_evaluated_attribute(pointcloud, ".viewer");
          DRWShadingGroup *grp = DRW_shgroup_pointcloud_create_sub(
              &object, pd.viewer_attribute_pointcloud_grp, nullptr);
          DRW_shgroup_uniform_float_copy(grp, "opacity", opacity);
          DRW_shgroup_buffer_texture_ref(grp, "attribute_tx", vertbuf);
        }
      }
      break;
    }
    case OB_CURVES_LEGACY: {
      Curve *curve = static_cast<Curve *>(object.data);
      if (curve->curve_eval) {
        const bke::CurvesGeometry &curves = curve->curve_eval->geometry.wrap();
        if (const std::optional<bke::AttributeMetaData> meta_data =
                curves.attributes().lookup_meta_data(".viewer"))
        {
          if (attribute_type_supports_viewer_overlay(meta_data->data_type)) {
            GPUBatch *batch = DRW_cache_curve_edge_wire_viewer_attribute_get(&object);
            DRW_shgroup_uniform_float_copy(pd.viewer_attribute_curve_grp, "opacity", opacity);
            DRW_shgroup_call_obmat(
                pd.viewer_attribute_curve_grp, batch, object.object_to_world().ptr());
          }
        }
      }
      break;
    }
    case OB_CURVES: {
      Curves *curves_id = static_cast<Curves *>(object.data);
      const bke::CurvesGeometry &curves = curves_id->geometry.wrap();
      if (const std::optional<bke::AttributeMetaData> meta_data =
              curves.attributes().lookup_meta_data(".viewer"))
      {
        if (attribute_type_supports_viewer_overlay(meta_data->data_type)) {
          bool is_point_domain;
          gpu::VertBuf **texture = DRW_curves_texture_for_evaluated_attribute(
              curves_id, ".viewer", &is_point_domain);
          DRWShadingGroup *grp = DRW_shgroup_curves_create_sub(
              &object, pd.viewer_attribute_curves_grp, nullptr);
          DRW_shgroup_uniform_float_copy(pd.viewer_attribute_curves_grp, "opacity", opacity);
          DRW_shgroup_uniform_bool_copy(grp, "is_point_domain", is_point_domain);
          DRW_shgroup_buffer_texture(grp, "color_tx", *texture);
        }
      }
      break;
    }
  }
}

void OVERLAY_viewer_attribute_cache_populate(OVERLAY_Data *vedata, Object *object)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const float opacity = vedata->stl->pd->overlay.viewer_attribute_opacity;
  DupliObject *dupli_object = DRW_object_get_dupli(object);

  if (dupli_object->preview_instance_index >= 0) {
    const auto &instances =
        *dupli_object->preview_base_geometry->get_component<blender::bke::InstancesComponent>();
    if (const std::optional<blender::bke::AttributeMetaData> meta_data =
            instances.attributes()->lookup_meta_data(".viewer"))
    {
      if (attribute_type_supports_viewer_overlay(meta_data->data_type)) {
        populate_cache_for_instance(*object, *pd, *dupli_object, opacity);
        return;
      }
    }
  }
  populate_cache_for_geometry(*object, *pd, opacity);
}

void OVERLAY_viewer_attribute_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  DRW_draw_pass(psl->attribute_ps);
}
