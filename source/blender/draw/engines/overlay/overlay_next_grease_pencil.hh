/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BLI_bounds.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_material.hh"
#include "BKE_object.hh"

#include "DNA_material_types.h"

#include "ED_grease_pencil.hh"

#include "draw_cache.hh"
#include "draw_manager_text.hh"

#include "overlay_next_base.hh"

namespace blender::draw::overlay {

/**
 * Draw grease pencil overlays.
 * Also contains grease pencil helper functions for other overlays.
 */
class GreasePencil : Overlay {
 private:
  PassSimple edit_grease_pencil_ps_ = {"GPencil Edit"};
  PassSimple::Sub *edit_points_ = nullptr;
  PassSimple::Sub *edit_lines_ = nullptr;

  PassSimple grid_ps_ = {"GPencil Grid"};

  bool show_points_ = false;
  bool show_lines_ = false;
  bool show_grid_ = false;
  bool show_weight_ = false;
  bool show_material_name_ = false;

  /* TODO(fclem): This is quite wasteful and expensive, prefer in shader Z modification like the
   * retopology offset. */
  View view_edit_cage_ = {"view_edit_cage"};
  View::OffsetData offset_data_;

 public:
  void begin_sync(Resources &res, const State &state) final
  {
    enabled_ = state.is_space_v3d();

    res.depth_planes.clear();
    res.depth_planes_count = 0;

    if (!enabled_) {
      return;
    }

    offset_data_ = state.offset_data_get();

    const View3D *v3d = state.v3d;
    const ToolSettings *ts = state.scene->toolsettings;

    show_material_name_ = (v3d->gp_flag & V3D_GP_SHOW_MATERIAL_NAME) && state.show_text;
    const bool show_lines = (v3d->gp_flag & V3D_GP_SHOW_EDIT_LINES);
    const bool show_direction = (v3d->gp_flag & V3D_GP_SHOW_STROKE_DIRECTION);

    show_points_ = show_lines_ = show_weight_ = false;

    switch (state.object_mode) {
      case OB_MODE_PAINT_GREASE_PENCIL:
        /* Draw mode. */
        break;
      case OB_MODE_VERTEX_GREASE_PENCIL:
        /* Vertex paint mode. */
        show_points_ = ts->gpencil_selectmode_vertex &
                       (GP_VERTEX_MASK_SELECTMODE_POINT | GP_VERTEX_MASK_SELECTMODE_SEGMENT);
        show_lines_ = show_lines && ts->gpencil_selectmode_vertex;
        break;
      case OB_MODE_EDIT:
        /* Edit mode. */
        show_points_ = ELEM(
            ts->gpencil_selectmode_edit, GP_SELECTMODE_POINT, GP_SELECTMODE_SEGMENT);
        show_lines_ = show_lines;
        break;
      case OB_MODE_WEIGHT_GREASE_PENCIL:
        /* Weight paint mode. */
        show_points_ = true;
        show_lines_ = show_lines;
        show_weight_ = true;
        break;
      case OB_MODE_SCULPT_GREASE_PENCIL:
        /* Sculpt mode. */
        show_points_ = ts->gpencil_selectmode_sculpt &
                       (GP_SCULPT_MASK_SELECTMODE_POINT | GP_SCULPT_MASK_SELECTMODE_SEGMENT);
        show_lines_ = show_lines && ts->gpencil_selectmode_sculpt;
        break;
      default:
        /* Not a Grease Pencil mode. */
        break;
    }

    edit_points_ = nullptr;
    edit_lines_ = nullptr;

    {
      auto &pass = edit_grease_pencil_ps_;
      pass.init();
      pass.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                         DRW_STATE_BLEND_ALPHA,
                     state.clipping_plane_count);

      if (show_points_) {
        auto &sub = pass.sub("Points");
        sub.shader_set(res.shaders.curve_edit_points.get());
        sub.bind_texture("weightTex", &res.weight_ramp_tx);
        sub.push_constant("useWeight", show_weight_);
        sub.push_constant("useGreasePencil", true);
        sub.push_constant("doStrokeEndpoints", show_direction);
        edit_points_ = &sub;
      }

      if (show_lines_) {
        auto &sub = pass.sub("Lines");
        sub.shader_set(res.shaders.curve_edit_line.get());
        sub.bind_texture("weightTex", &res.weight_ramp_tx);
        sub.push_constant("useWeight", show_weight_);
        sub.push_constant("useGreasePencil", true);
        edit_lines_ = &sub;
      }
    }

    const bool is_depth_projection_mode = ts->gpencil_v3d_align &
                                          (GP_PROJECT_DEPTH_VIEW | GP_PROJECT_DEPTH_STROKE);
    show_grid_ = (v3d->gp_flag & V3D_GP_SHOW_GRID) && !is_depth_projection_mode;

    {
      const bool grid_xray = (v3d->gp_flag & V3D_GP_SHOW_GRID_XRAY);
      DRWState depth_write_state = (grid_xray) ? DRW_STATE_DEPTH_ALWAYS :
                                                 DRW_STATE_DEPTH_LESS_EQUAL;
      auto &pass = grid_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA | depth_write_state,
                     state.clipping_plane_count);
      if (show_grid_) {
        const float4 col_grid(float3(state.overlay.gpencil_grid_color),
                              state.overlay.gpencil_grid_opacity);
        pass.shader_set(res.shaders.grid_grease_pencil.get());
        pass.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
        pass.push_constant("color", col_grid);
      }
    }
  }

  void edit_object_sync(Manager &manager,
                        const ObjectRef &ob_ref,
                        Resources &res,
                        const State &state) final
  {
    if (!enabled_) {
      return;
    }

    Object *ob = ob_ref.object;

    if (show_points_) {
      gpu::Batch *geom = show_weight_ ?
                             DRW_cache_grease_pencil_weight_points_get(state.scene, ob) :
                             DRW_cache_grease_pencil_edit_points_get(state.scene, ob);
      if (geom) {
        edit_points_->draw(geom, manager.unique_handle(ob_ref));
      }
    }
    if (show_lines_) {
      gpu::Batch *geom = show_weight_ ? DRW_cache_grease_pencil_weight_lines_get(state.scene, ob) :
                                        DRW_cache_grease_pencil_edit_lines_get(state.scene, ob);
      if (geom) {
        edit_lines_->draw(geom, manager.unique_handle(ob_ref));
      }
    }

    if (show_material_name_) {
      draw_material_names(ob_ref, state, res);
    }
  }

  void paint_object_sync(Manager &manager,
                         const ObjectRef &ob_ref,
                         Resources &res,
                         const State &state)
  {
    /* Reuse same logic as edit mode. */
    edit_object_sync(manager, ob_ref, res, state);
  }

  void sculpt_object_sync(Manager &manager,
                          const ObjectRef &ob_ref,
                          Resources &res,
                          const State &state)
  {
    /* Reuse same logic as edit mode. */
    edit_object_sync(manager, ob_ref, res, state);
  }

  void object_sync(Manager & /*manager*/,
                   const ObjectRef &ob_ref,
                   Resources & /*res*/,
                   const State &state) final
  {
    if (!enabled_) {
      return;
    }

    if (ob_ref.object != state.object_active) {
      /* Only display for the active object. */
      return;
    }

    if (show_grid_) {
      const int grid_lines = state.v3d->overlay.gpencil_grid_subdivisions;
      const int line_count = grid_lines * 4 + 2;

      const float3 grid_offset = float3(float2(state.v3d->overlay.gpencil_grid_offset), 0.0f);
      const float3 grid_scale = float3(float2(state.v3d->overlay.gpencil_grid_scale), 0.0f);
      const float4x4 transform_mat = math::from_loc_scale<float4x4>(grid_offset, grid_scale);

      const float4x4 grid_mat = grid_matrix_get(*ob_ref.object, state.scene) * transform_mat;

      grid_ps_.push_constant("xAxis", grid_mat.x_axis());
      grid_ps_.push_constant("yAxis", grid_mat.y_axis());
      grid_ps_.push_constant("origin", grid_mat.location());
      grid_ps_.push_constant("halfLineCount", line_count / 2);
      grid_ps_.draw_procedural(GPU_PRIM_LINES, 1, line_count * 2);
    }
  }

  static void compute_depth_planes(Manager &manager,
                                   View &view,
                                   Resources &res,
                                   const State & /*state*/)
  {
    for (auto i : IndexRange(res.depth_planes_count)) {
      GreasePencilDepthPlane &plane = res.depth_planes[i];
      const float4x4 &object_to_world =
          manager.matrix_buf.current().get_or_resize(plane.handle.resource_index()).model;
      plane.plane = GreasePencil::depth_plane_get(object_to_world, plane.bounds, view);
    }
  }

  void draw_line(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit(grid_ps_, view);
  }

  void draw_color_only(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    view_edit_cage_.sync(view.viewmat(), offset_data_.winmat_polygon_offset(view.winmat(), 0.5f));

    GPU_framebuffer_bind(framebuffer);
    manager.submit(edit_grease_pencil_ps_, view_edit_cage_);
  }

  static void draw_grease_pencil(Resources &res,
                                 PassMain::Sub &pass,
                                 const Scene *scene,
                                 Object *ob,
                                 ResourceHandle res_handle,
                                 select::ID select_id = select::SelectMap::select_invalid_id())
  {
    using namespace blender;
    using namespace blender::ed::greasepencil;
    ::GreasePencil &grease_pencil = *static_cast<::GreasePencil *>(ob->data);

    const bool is_stroke_order_3d = (grease_pencil.flag & GREASE_PENCIL_STROKE_ORDER_3D) != 0;

    if (is_stroke_order_3d) {
      pass.push_constant("gpDepthPlane", float4(0.0f));
    }
    else {
      int64_t index = res.depth_planes.append_and_get_index({});
      res.depth_planes_count++;

      GreasePencilDepthPlane &plane = res.depth_planes[index];
      plane.bounds = BKE_object_boundbox_get(ob).value_or(blender::Bounds(float3(0)));
      plane.handle = res_handle;

      pass.push_constant("gpDepthPlane", &plane.plane);
    }

    int t_offset = 0;
    const Vector<DrawingInfo> drawings = retrieve_visible_drawings(*scene, grease_pencil, true);
    for (const DrawingInfo info : drawings) {

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
  static float4 depth_plane_get(const float4x4 &object_to_world,
                                const blender::Bounds<float3> &bounds,
                                const View &view)
  {
    using namespace blender::math;

    /* Find the normal most likely to represent the grease pencil object. */
    /* TODO: This does not work quite well if you use
     * strokes not aligned with the object axes. Maybe we could try to
     * compute the minimum axis of all strokes. But this would be more
     * computationally heavy and should go into the GPData evaluation. */
    float3 center = bounds.center();
    float3 size = bounds.size();
    /* Avoid division by 0.0 later. */
    size += 1e-8f;

    /* Convert Bbox unit space to object space. */
    float4x4 bbox_to_object = from_loc_scale<float4x4>(center, size * 0.5f);
    float4x4 bbox_to_world = object_to_world * bbox_to_object;

    float3 bbox_center = bbox_to_world.location();
    float3 view_vector = (view.is_persp()) ? (view.location() - bbox_center) : view.forward();

    float3x3 world_to_bbox = invert(float3x3(bbox_to_world));

    /* Normalize the vector in BBox space. */
    float3 local_plane_direction = normalize(transform_direction(world_to_bbox, view_vector));
    /* `bbox_to_world_normal` is a "normal" matrix. It transforms BBox space normals to world. */
    float3x3 bbox_to_world_normal = transpose(world_to_bbox);
    float3 plane_direction = normalize(
        transform_direction(bbox_to_world_normal, local_plane_direction));

    return float4(plane_direction, -dot(plane_direction, bbox_center));
  }

  float4x4 grid_matrix_get(const Object &object, const Scene *scene)
  {
    const ToolSettings *ts = scene->toolsettings;

    const ::GreasePencil &grease_pencil = *static_cast<::GreasePencil *>(object.data);
    const blender::bke::greasepencil::Layer *active_layer = grease_pencil.get_active_layer();

    float4x4 mat = object.object_to_world();
    if (active_layer && ts->gp_sculpt.lock_axis != GP_LOCKAXIS_CURSOR) {
      mat = active_layer->to_world_space(object);
    }
    const View3DCursor *cursor = &scene->cursor;

    /* Set the grid in the selected axis */
    switch (ts->gp_sculpt.lock_axis) {
      case GP_LOCKAXIS_X:
        std::swap(mat[0], mat[2]);
        break;
      case GP_LOCKAXIS_Y:
        std::swap(mat[1], mat[2]);
        break;
      case GP_LOCKAXIS_Z:
        /* Default. */
        break;
      case GP_LOCKAXIS_CURSOR: {
        mat = float4x4(cursor->matrix<float3x3>());
        break;
      }
      case GP_LOCKAXIS_VIEW:
        /* view aligned */
        /* TODO(fclem): Global access. */
        mat = blender::draw::View::default_get().viewinv();
        break;
    }

    mat *= 2.0f;

    if (ts->gpencil_v3d_align & GP_PROJECT_CURSOR) {
      mat.location() = cursor->location;
    }
    else if (active_layer) {
      mat.location() = active_layer->to_world_space(object).location();
    }
    else {
      mat.location() = object.object_to_world().location();
    }
    return mat;
  }

  void draw_material_names(const ObjectRef &ob_ref, const State &state, Resources &res)
  {
    Object &object = *ob_ref.object;

    uchar4 color;
    UI_GetThemeColor4ubv(res.object_wire_theme_id(ob_ref, state), color);

    ::GreasePencil &grease_pencil = *static_cast<::GreasePencil *>(object.data);

    Vector<ed::greasepencil::DrawingInfo> drawings = ed::greasepencil::retrieve_visible_drawings(
        *state.scene, grease_pencil, false);
    if (drawings.is_empty()) {
      return;
    }

    for (const ed::greasepencil::DrawingInfo &info : drawings) {
      const bke::greasepencil::Drawing &drawing = info.drawing;

      const bke::CurvesGeometry strokes = drawing.strokes();
      const OffsetIndices<int> points_by_curve = strokes.points_by_curve();
      const bke::AttrDomain domain = show_points_ ? bke::AttrDomain::Point :
                                                    bke::AttrDomain::Curve;
      const VArray<bool> selections = *strokes.attributes().lookup_or_default<bool>(
          ".selection", domain, true);
      const VArray<int> materials = *strokes.attributes().lookup_or_default<int>(
          "material_index", bke::AttrDomain::Curve, 0);
      const Span<float3> positions = strokes.positions();

      auto show_stroke_name = [&](const int stroke_i) {
        if (show_points_) {
          for (const int point_i : points_by_curve[stroke_i]) {
            if (selections[point_i]) {
              return true;
            }
          }
          return false;
        }
        return selections[stroke_i];
      };

      for (const int stroke_i : strokes.curves_range()) {
        if (!show_stroke_name(stroke_i)) {
          continue;
        }
        const int point_i = points_by_curve[stroke_i].first();
        const float3 fpt = math::transform_point(object.object_to_world(), positions[point_i]);
        Material *ma = BKE_object_material_get_eval(&object, materials[stroke_i] + 1);
        DRW_text_cache_add(state.dt,
                           fpt,
                           ma->id.name + 2,
                           strlen(ma->id.name + 2),
                           10,
                           0,
                           DRW_TEXT_CACHE_GLOBALSPACE | DRW_TEXT_CACHE_STRING_PTR,
                           color);
      }
    }
  }
};

}  // namespace blender::draw::overlay
