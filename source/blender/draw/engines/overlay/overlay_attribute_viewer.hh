/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "DNA_curve_types.h"
#include "DNA_pointcloud_types.h"

#include "draw_cache.hh"
#include "draw_cache_impl.hh"
#include "overlay_base.hh"

namespace blender::draw::overlay {

/**
 * Displays geometry node viewer output.
 * Values are displayed as vertex or face colors on top of the active object.
 */
class AttributeViewer : Overlay {
 private:
  PassMain ps_ = {"attribute_viewer_ps_"};

  PassMain::Sub *mesh_sub_ = nullptr;
  PassMain::Sub *pointcloud_sub_ = nullptr;
  PassMain::Sub *curve_sub_ = nullptr;
  PassMain::Sub *curves_sub_ = nullptr;
  PassMain::Sub *instance_sub_ = nullptr;

 public:
  void begin_sync(Resources &res, const State &state) final
  {
    ps_.init();
    enabled_ = state.is_space_v3d() && !res.is_selection() && state.show_attribute_viewer();
    if (!enabled_) {
      return;
    };
    ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
    ps_.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
    ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA,
                  state.clipping_plane_count);

    auto create_sub = [&](const char *name, gpu::Shader *shader) {
      auto &sub = ps_.sub(name);
      sub.shader_set(shader);
      return &sub;
    };

    mesh_sub_ = create_sub("mesh", res.shaders->attribute_viewer_mesh.get());
    pointcloud_sub_ = create_sub("pointcloud", res.shaders->attribute_viewer_pointcloud.get());
    curve_sub_ = create_sub("curve", res.shaders->attribute_viewer_curve.get());
    curves_sub_ = create_sub("curves", res.shaders->attribute_viewer_curves.get());
    instance_sub_ = create_sub("instance", res.shaders->uniform_color.get());
  }

  void object_sync(Manager &manager,
                   const ObjectRef &ob_ref,
                   Resources & /*res*/,
                   const State &state) final
  {
    const bool is_preview = ob_ref.preview_base_geometry() != nullptr;
    if (!enabled_ || !is_preview) {
      return;
    }

    if (ob_ref.preview_instance_index() >= 0) {
      const auto &instances =
          *ob_ref.preview_base_geometry()->get_component<blender::bke::InstancesComponent>();
      if (const std::optional<blender::bke::AttributeMetaData> meta_data =
              instances.attributes()->lookup_meta_data(".viewer"))
      {
        if (attribute_type_supports_viewer_overlay(meta_data->data_type)) {
          populate_for_instance(ob_ref, state, manager);
          return;
        }
      }
    }
    populate_for_geometry(ob_ref, state, manager);
  }

  void pre_draw(Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    manager.generate_commands(ps_, view);
  }

  void draw_line(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit_only(ps_, view);
  }

 private:
  void populate_for_instance(const ObjectRef &ob_ref, const State &state, Manager &manager)
  {
    Object &object = *ob_ref.object;
    const bke::GeometrySet &base_geometry = *ob_ref.preview_base_geometry();
    const bke::InstancesComponent &instances =
        *base_geometry.get_component<bke::InstancesComponent>();
    const bke::AttributeAccessor instance_attributes = *instances.attributes();
    const VArray attribute = *instance_attributes.lookup<ColorGeometry4f>(".viewer");
    if (!attribute) {
      return;
    }
    ColorGeometry4f color = attribute.get(ob_ref.preview_instance_index());
    color.a *= state.overlay.viewer_attribute_opacity;
    switch (object.type) {
      case OB_MESH: {
        ResourceHandleRange res_handle = manager.unique_handle(ob_ref);

        {
          gpu::Batch *batch = DRW_cache_mesh_surface_get(&object);
          auto &sub = *instance_sub_;
          sub.push_constant("ucolor", float4(color));
          sub.draw(batch, res_handle);
        }
        if (gpu::Batch *batch = DRW_cache_mesh_loose_edges_get(&object)) {
          auto &sub = *instance_sub_;
          sub.push_constant("ucolor", float4(color));
          sub.draw(batch, res_handle);
        }

        break;
      }
      case OB_POINTCLOUD: {
        auto &sub = *pointcloud_sub_;
        gpu::Batch *batch = pointcloud_sub_pass_setup(sub, &object, nullptr);
        sub.push_constant("ucolor", float4(color));
        sub.draw(batch, manager.unique_handle(ob_ref));
        break;
      }
      case OB_CURVES_LEGACY: {
        gpu::Batch *batch = DRW_cache_curve_edge_wire_get(&object);
        auto &sub = *instance_sub_;
        sub.push_constant("ucolor", float4(color));
        ResourceHandleRange res_handle = manager.unique_handle(ob_ref);
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

  static bool attribute_type_supports_viewer_overlay(const bke::AttrType data_type)
  {
    return !ELEM(data_type, bke::AttrType::Quaternion, bke::AttrType::Float4x4);
  }

  void populate_for_geometry(const ObjectRef &ob_ref, const State &state, Manager &manager)
  {
    const float opacity = state.overlay.viewer_attribute_opacity;
    Object &object = *ob_ref.object;
    switch (object.type) {
      case OB_MESH: {
        Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(object);
        if (const std::optional<bke::AttributeMetaData> meta_data =
                mesh.attributes().lookup_meta_data(".viewer"))
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
        PointCloud &pointcloud = DRW_object_get_data_for_drawing<PointCloud>(object);
        if (const std::optional<bke::AttributeMetaData> meta_data =
                pointcloud.attributes().lookup_meta_data(".viewer"))
        {
          if (attribute_type_supports_viewer_overlay(meta_data->data_type)) {
            gpu::VertBuf **vertbuf = DRW_pointcloud_evaluated_attribute(&pointcloud, ".viewer");
            /* Avoid trying to bind an empty `vertbuf` which causes assert / undefined behavior. */
            if (pointcloud.totpoint > 0 && vertbuf != nullptr) {
              auto &sub = *pointcloud_sub_;
              gpu::Batch *batch = pointcloud_sub_pass_setup(sub, &object, nullptr);
              sub.push_constant("opacity", opacity);
              sub.bind_texture("attribute_tx", vertbuf);
              sub.draw(batch, manager.unique_handle(ob_ref));
            }
          }
        }
        break;
      }
      case OB_CURVES_LEGACY: {
        Curve &curve = DRW_object_get_data_for_drawing<Curve>(object);
        if (curve.curve_eval) {
          const bke::CurvesGeometry &curves = curve.curve_eval->geometry.wrap();
          if (const std::optional<bke::AttributeMetaData> meta_data =
                  curves.attributes().lookup_meta_data(".viewer"))
          {
            if (attribute_type_supports_viewer_overlay(meta_data->data_type)) {
              gpu::Batch *batch = DRW_cache_curve_edge_wire_viewer_attribute_get(&object);
              auto &sub = *curve_sub_;
              sub.push_constant("opacity", opacity);
              ResourceHandleRange res_handle = manager.unique_handle(ob_ref);
              sub.draw(batch, res_handle);
            }
          }
        }
        break;
      }
      case OB_CURVES: {
        ::Curves &curves_id = DRW_object_get_data_for_drawing<::Curves>(object);
        const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
        if (const std::optional<bke::AttributeMetaData> meta_data =
                curves.attributes().lookup_meta_data(".viewer"))
        {
          if (attribute_type_supports_viewer_overlay(meta_data->data_type)) {
            bool is_point_domain;
            bool is_valid;
            gpu::VertBufPtr &texture = DRW_curves_texture_for_evaluated_attribute(
                &curves_id, ".viewer", is_point_domain, is_valid);
            if (is_valid) {
              auto &sub = *curves_sub_;
              const char *error = nullptr;
              /* The error string will always have been printed by the engine already.
               * No need to display it twice. */
              gpu::Batch *batch = curves_sub_pass_setup(sub, state.scene, ob_ref.object, error);
              sub.push_constant("opacity", opacity);
              sub.push_constant("is_point_domain", is_point_domain);
              sub.bind_texture("color_tx", texture);
              sub.draw(batch, manager.unique_handle(ob_ref));
            }
          }
        }
        break;
      }
    }
  }
};
}  // namespace blender::draw::overlay
