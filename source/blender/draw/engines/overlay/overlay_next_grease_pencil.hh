/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BLI_bounds.hh"
#include "BLI_math_matrix.hh"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_object.hh"

#include "ED_grease_pencil.hh"

#include "overlay_next_private.hh"

namespace blender::draw::overlay {

class GreasePencil {
 public:
  struct ViewParameters {
    bool is_perspective;
    union {
      /* Z axis if ortho or position if perspective. */
      float3 z_axis;
      float3 location;
    };

    ViewParameters() = default;

    ViewParameters(bool is_perspective, const float4x4 &viewinv)
    {
      if (is_perspective) {
        location = viewinv.location();
      }
      else {
        z_axis = viewinv.z_axis();
      }
    }
  };

 public:
  static void draw_grease_pencil(PassMain::Sub &pass,
                                 const ViewParameters &view,
                                 const Scene *scene,
                                 Object *ob,
                                 ResourceHandle res_handle,
                                 select::ID select_id = select::SelectMap::select_invalid_id())
  {
    using namespace blender;
    using namespace blender::ed::greasepencil;
    ::GreasePencil &grease_pencil = *static_cast<::GreasePencil *>(ob->data);

    float4 plane = (grease_pencil.flag & GREASE_PENCIL_STROKE_ORDER_3D) ?
                       float4(0.0f) :
                       depth_plane_get(ob, view);
    pass.push_constant("gpDepthPlane", plane);

    int t_offset = 0;
    const Vector<DrawingInfo> drawings = retrieve_visible_drawings(*scene, grease_pencil, true);
    for (const DrawingInfo info : drawings) {
      const bool is_stroke_order_3d = (grease_pencil.flag & GREASE_PENCIL_STROKE_ORDER_3D) != 0;

      const float object_scale = mat4_to_scale(ob->object_to_world().ptr());
      const float thickness_scale = bke::greasepencil::LEGACY_RADIUS_CONVERSION_FACTOR;

      gpu::VertBuf *position_tx = draw::DRW_cache_grease_pencil_position_buffer_get(scene, ob);
      gpu::VertBuf *color_tx = draw::DRW_cache_grease_pencil_color_buffer_get(scene, ob);

      pass.push_constant("gpStrokeOrder3d", is_stroke_order_3d);
      pass.push_constant("gpThicknessScale", object_scale);
      pass.push_constant("gpThicknessOffset", 0.0f);
      pass.push_constant("gpThicknessWorldScale", thickness_scale);
      pass.bind_texture("gp_pos_tx", position_tx);
      pass.bind_texture("gp_col_tx", color_tx);

      const bke::CurvesGeometry &curves = info.drawing.strokes();
      const OffsetIndices<int> points_by_curve = curves.evaluated_points_by_curve();
      const bke::AttributeAccessor attributes = curves.attributes();
      const VArray<int> stroke_materials = *attributes.lookup_or_default<int>(
          "material_index", bke::AttrDomain::Curve, 0);
      const VArray<bool> cyclic = *attributes.lookup_or_default<bool>(
          "cyclic", bke::AttrDomain::Curve, false);

      IndexMaskMemory memory;
      const IndexMask visible_strokes = ed::greasepencil::retrieve_visible_strokes(
          *ob, info.drawing, memory);

      visible_strokes.foreach_index([&](const int stroke_i) {
        const IndexRange points = points_by_curve[stroke_i];
        const int material_index = stroke_materials[stroke_i];
        MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, material_index + 1);

        const bool hide_onion = info.onion_id != 0;
        const bool hide_material = (gp_style->flag & GP_MATERIAL_HIDE) != 0;

        const int num_stroke_triangles = (points.size() >= 3) ? (points.size() - 2) : 0;
        const int num_stroke_vertices = (points.size() +
                                         int(cyclic[stroke_i] && (points.size() >= 3)));

        if (hide_material || hide_onion) {
          t_offset += num_stroke_triangles;
          t_offset += num_stroke_vertices * 2;
          return;
        }

        blender::gpu::Batch *geom = draw::DRW_cache_grease_pencil_get(scene, ob);

        const bool show_stroke = (gp_style->flag & GP_MATERIAL_STROKE_SHOW) != 0;
        const bool show_fill = (points.size() >= 3) &&
                               (gp_style->flag & GP_MATERIAL_FILL_SHOW) != 0;

        if (show_fill) {
          int v_first = t_offset * 3;
          int v_count = num_stroke_triangles * 3;
          pass.draw(geom, 1, v_count, v_first, res_handle, select_id.get());
        }

        t_offset += num_stroke_triangles;

        if (show_stroke) {
          int v_first = t_offset * 3;
          int v_count = num_stroke_vertices * 2 * 3;
          pass.draw(geom, 1, v_count, v_first, res_handle, select_id.get());
        }
        t_offset += num_stroke_vertices * 2;
      });
    }
  }

 private:
  /* Returns the normal plane in NDC space. */
  static float4 depth_plane_get(const Object *ob, const ViewParameters &view)
  {
    using namespace blender::math;

    /* Find the normal most likely to represent the grease pencil object. */
    /* TODO: This does not work quite well if you use
     * strokes not aligned with the object axes. Maybe we could try to
     * compute the minimum axis of all strokes. But this would be more
     * computationally heavy and should go into the GPData evaluation. */
    const std::optional<blender::Bounds<float3>> bounds = BKE_object_boundbox_get(ob).value_or(
        blender::Bounds(float3(0)));

    float3 center = midpoint(bounds->min, bounds->max);
    float3 size = (bounds->max - bounds->min) * 0.5f;
    /* Avoid division by 0.0 later. */
    size += 1e-8f;
    /* Convert Bbox unit space to object space. */
    float4x4 bbox_to_object = from_loc_scale<float4x4>(center, size);
    float4x4 bbox_to_world = ob->object_to_world() * bbox_to_object;

    float3 bbox_center = bbox_to_world.location();
    float3 view_vector = (view.is_perspective) ? view.location - bbox_center : view.z_axis;

    float3x3 world_to_bbox = invert(float3x3(bbox_to_world));

    /* Normalize the vector in BBox space. */
    float3 local_plane_direction = normalize(transform_direction(world_to_bbox, view_vector));
    /* `bbox_to_world_normal` is a "normal" matrix. It transforms BBox space normals to world. */
    float3x3 bbox_to_world_normal = transpose(world_to_bbox);
    float3 plane_direction = normalize(
        transform_direction(bbox_to_world_normal, local_plane_direction));

    return float4(plane_direction, -dot(plane_direction, bbox_center));
  }
};

}  // namespace blender::draw::overlay
