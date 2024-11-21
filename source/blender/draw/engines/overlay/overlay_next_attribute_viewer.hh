/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_geometry_set.hh"
#include "DNA_curve_types.h"
#include "DNA_pointcloud_types.h"

#include "draw_cache_impl.hh"
#include "overlay_next_private.hh"

namespace blender::draw::overlay {
class AttributeViewer {
 private:
  PassMain ps_ = {"attribute_viewer_ps_"};

  PassMain::Sub *mesh_sub_ = nullptr;
  PassMain::Sub *pointcloud_sub_ = nullptr;
  PassMain::Sub *curve_sub_ = nullptr;
  PassMain::Sub *curves_sub_ = nullptr;
  PassMain::Sub *instance_sub_ = nullptr;

  bool enabled_ = false;

 public:
  void begin_sync(Resources &res, const State &state)
  {
    ps_.init();
    enabled_ = state.space_type == SPACE_VIEW3D && res.selection_type == SelectionType::DISABLED &&
               (state.overlay.flag & V3D_OVERLAY_VIEWER_ATTRIBUTE);
    if (!enabled_) {
      return;
    };
    ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA,
                  state.clipping_plane_count);

    auto create_sub = [&](const char *name, GPUShader *shader) {
      auto &sub = ps_.sub(name);
      sub.shader_set(shader);
      return &sub;
    };

    mesh_sub_ = create_sub("mesh", res.shaders.attribute_viewer_mesh.get());
    pointcloud_sub_ = create_sub("pointcloud", res.shaders.attribute_viewer_pointcloud.get());
    curve_sub_ = create_sub("curve", res.shaders.attribute_viewer_curve.get());
    curves_sub_ = create_sub("curves", res.shaders.attribute_viewer_curves.get());
    instance_sub_ = create_sub("instance", res.shaders.uniform_color.get());
  }

  void object_sync(const ObjectRef &ob_ref, const State &state, Manager &manager)
  {
    const DupliObject *dupli_object = DRW_object_get_dupli(ob_ref.object);
    const bool is_preview = dupli_object != nullptr &&
                            dupli_object->preview_base_geometry != nullptr;
    if (!enabled_ || !is_preview) {
      return;
    }

    if (dupli_object->preview_instance_index >= 0) {
      const auto &instances =
          *dupli_object->preview_base_geometry->get_component<blender::bke::InstancesComponent>();
      if (const std::optional<blender::bke::AttributeMetaData> meta_data =
              instances.attributes()->lookup_meta_data(".viewer"))
      {
        if (attribute_type_supports_viewer_overlay(meta_data->data_type)) {
          populate_for_instance(ob_ref, *dupli_object, state, manager);
          return;
        }
      }
    }
    populate_for_geometry(ob_ref, state, manager);
  }

  void pre_draw(Manager &manager, View &view)
  {
    if (!enabled_) {
      return;
    }

    manager.generate_commands(ps_, view);
  }

  void draw(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit_only(ps_, view);
  }

 private:
  void populate_for_instance(const ObjectRef &ob_ref,
                             const DupliObject &dupli_object,
                             const State &state,
                             Manager &manager)
  {
    Object &object = *ob_ref.object;
    const bke::GeometrySet &base_geometry = *dupli_object.preview_base_geometry;
    const bke::InstancesComponent &instances =
        *base_geometry.get_component<bke::InstancesComponent>();
    const bke::AttributeAccessor instance_attributes = *instances.attributes();
    const VArray attribute = *instance_attributes.lookup<ColorGeometry4f>(".viewer");
    if (!attribute) {
      return;
    }
    ColorGeometry4f color = attribute.get(dupli_object.preview_instance_index);
    color.a *= state.overlay.viewer_attribute_opacity;
    switch (object.type) {
      case OB_MESH: {
        ResourceHandle res_handle = manager.unique_handle(ob_ref);

        {
          gpu::Batch *batch = DRW_cache_mesh_surface_get(&object);
          auto &sub = *instance_sub_;
          sub.push_constant("ucolor", color);
          sub.draw(batch, res_handle);
        }
        if (gpu::Batch *batch = DRW_cache_mesh_loose_edges_get(&object)) {
          auto &sub = *instance_sub_;
          sub.push_constant("ucolor", color);
          sub.draw(batch, res_handle);
        }

        break;
      }
      case OB_POINTCLOUD: {
        auto &sub = *pointcloud_sub_;
        gpu::Batch *batch = point_cloud_sub_pass_setup(sub, &object, nullptr);
        sub.push_constant("ucolor", color);
        sub.draw(batch, manager.unique_handle(ob_ref));
        break;
      }
      case OB_CURVES_LEGACY: {
        gpu::Batch *batch = DRW_cache_curve_edge_wire_get(&object);
        auto &sub = *instance_sub_;
        sub.push_constant("ucolor", color);
        ResourceHandle res_handle = manager.resource_handle(object.object_to_world());
        sub.draw(batch, res_handle);
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
    return CD_TYPE_AS_MASK(data_type) &
           (CD_MASK_PROP_ALL & ~(CD_MASK_PROP_QUATERNION | CD_MASK_PROP_FLOAT4X4));
  }

  void populate_for_geometry(const ObjectRef &ob_ref, const State &state, Manager &manager)
  {
    const float opacity = state.overlay.viewer_attribute_opacity;
    Object &object = *ob_ref.object;
    switch (object.type) {
      case OB_MESH: {
        Mesh *mesh = static_cast<Mesh *>(object.data);
        if (const std::optional<bke::AttributeMetaData> meta_data =
                mesh->attributes().lookup_meta_data(".viewer"))
        {
          if (attribute_type_supports_viewer_overlay(meta_data->data_type)) {
            gpu::Batch *batch = DRW_cache_mesh_surface_viewer_attribute_get(&object);
            auto &sub = *mesh_sub_;
            sub.push_constant("opacity", opacity);
            sub.draw(batch, manager.unique_handle(ob_ref));
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
            auto &sub = *pointcloud_sub_;
            gpu::Batch *batch = point_cloud_sub_pass_setup(sub, &object, nullptr);
            sub.push_constant("opacity", opacity);
            sub.bind_texture("attribute_tx", vertbuf);
            sub.draw(batch, manager.unique_handle(ob_ref));
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
              gpu::Batch *batch = DRW_cache_curve_edge_wire_viewer_attribute_get(&object);
              auto &sub = *curve_sub_;
              sub.push_constant("opacity", opacity);
              ResourceHandle res_handle = manager.resource_handle(object.object_to_world());
              sub.draw(batch, res_handle);
            }
          }
        }
        break;
      }
      case OB_CURVES: {
        ::Curves *curves_id = static_cast<::Curves *>(object.data);
        const bke::CurvesGeometry &curves = curves_id->geometry.wrap();
        if (const std::optional<bke::AttributeMetaData> meta_data =
                curves.attributes().lookup_meta_data(".viewer"))
        {
          if (attribute_type_supports_viewer_overlay(meta_data->data_type)) {
            bool is_point_domain;
            gpu::VertBuf **texture = DRW_curves_texture_for_evaluated_attribute(
                curves_id, ".viewer", &is_point_domain);
            auto &sub = *curves_sub_;
            gpu::Batch *batch = curves_sub_pass_setup(sub, state.scene, ob_ref.object);
            sub.push_constant("opacity", opacity);
            sub.push_constant("is_point_domain", is_point_domain);
            sub.bind_texture("color_tx", *texture);
            sub.draw(batch, manager.unique_handle(ob_ref));
          }
        }
        break;
      }
    }
  }
};
}  // namespace blender::draw::overlay
