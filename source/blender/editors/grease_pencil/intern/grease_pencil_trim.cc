/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BLI_array.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_lasso_2d.hh"
#include "BLI_math_geom.h"
#include "BLI_rect.h"
#include "BLI_task.hh"

#include "DNA_brush_types.h"

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"
#include "BKE_paint.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"

namespace blender::ed::greasepencil {

static constexpr int BBOX_PADDING = 2;

/**
 * Apply the stroke trim to a drawing.
 */
static bool execute_trim_on_drawing(const int layer_index,
                                    const int frame_number,
                                    const Object &ob_eval,
                                    Object &obact,
                                    const ARegion &region,
                                    const float4x4 &projection,
                                    const Span<int2> mcoords,
                                    const bool keep_caps,
                                    bke::greasepencil::Drawing &drawing)
{
  const bke::CurvesGeometry &src = drawing.strokes();
  const OffsetIndices<int> src_points_by_curve = src.points_by_curve();

  /* Get evaluated geometry. */
  bke::crazyspace::GeometryDeformation deformation =
      bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
          &ob_eval, obact, layer_index, frame_number);

  /* Compute screen space positions. */
  Array<float2> screen_space_positions(src.points_num());
  threading::parallel_for(src.points_range(), 4096, [&](const IndexRange src_points) {
    for (const int src_point : src_points) {
      screen_space_positions[src_point] = ED_view3d_project_float_v2_m4(
          &region, deformation.positions[src_point], projection);
    }
  });

  /* Compute bounding boxes of curves in screen space. The bounding boxes are used to speed
   * up the search for intersecting curves. */
  Array<rcti> screen_space_bbox(src.curves_num());
  threading::parallel_for(src.curves_range(), 512, [&](const IndexRange src_curves) {
    for (const int src_curve : src_curves) {
      rcti *bbox = &screen_space_bbox[src_curve];
      BLI_rcti_init_minmax(bbox);

      const IndexRange src_points = src_points_by_curve[src_curve];
      for (const int src_point : src_points) {
        BLI_rcti_do_minmax_v(bbox, int2(screen_space_positions[src_point]));
      }

      /* Add some padding, otherwise we could just miss intersections. */
      BLI_rcti_pad(bbox, BBOX_PADDING, BBOX_PADDING);
    }
  });

  rcti bbox_lasso;
  BLI_lasso_boundbox(&bbox_lasso, mcoords);

  /* Collect curves and curve points inside the lasso area. */
  Vector<int> selected_curves;
  Vector<Vector<int>> selected_points_in_curves;

  IndexMaskMemory memory;
  const IndexMask editable_strokes = blender::ed::greasepencil::retrieve_editable_strokes(
      obact, drawing, layer_index, memory);
  editable_strokes.foreach_index([&](const int src_curve) {
    /* To speed things up: do a bounding box check on the curve and the lasso area. */
    if (!BLI_rcti_isect(&bbox_lasso, &screen_space_bbox[src_curve], nullptr)) {
      return;
    }

    /* Look for curve points inside the lasso area. */
    Vector<int> selected_points;
    for (const int src_point : src_points_by_curve[src_curve]) {
      /* Check if point is inside the lasso area. */
      if (BLI_rcti_isect_pt_v(&bbox_lasso, int2(screen_space_positions[src_point])) &&
          BLI_lasso_is_point_inside(mcoords,
                                    int(screen_space_positions[src_point].x),
                                    int(screen_space_positions[src_point].y),
                                    IS_CLIPPED))
      {
        if (selected_points.is_empty()) {
          selected_curves.append(src_curve);
        }
        selected_points.append(src_point);
      }
    }
    if (!selected_points.is_empty()) {
      selected_points_in_curves.append(std::move(selected_points));
    }
  });

  const IndexMask curve_selection = IndexMask::from_indices(selected_curves.as_span(), memory);
  /* Abort when the lasso area is empty. */
  if (curve_selection.is_empty()) {
    return false;
  }

  /* Apply trim. */
  bke::CurvesGeometry cut_strokes = ed::greasepencil::trim::trim_curve_segments(
      src,
      screen_space_positions,
      screen_space_bbox,
      curve_selection,
      selected_points_in_curves,
      keep_caps);

  /* Set the new geometry. */
  drawing.strokes_for_write() = std::move(cut_strokes);
  drawing.tag_topology_changed();

  return true;
}

/**
 * Apply the stroke trim to all layers.
 */
static int stroke_trim_execute(const bContext *C, const Span<int2> mcoords)
{
  const Scene *scene = CTX_data_scene(C);
  const ARegion *region = CTX_wm_region(C);
  const RegionView3D *rv3d = CTX_wm_region_view3d(C);
  const Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *obact = CTX_data_active_object(C);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, obact);

  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(obact->data);

  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  if (brush->gpencil_settings == nullptr) {
    BKE_brush_init_gpencil_settings(brush);
  }
  const bool keep_caps = (brush->gpencil_settings->flag & GP_BRUSH_ERASER_KEEP_CAPS) != 0;
  const bool active_layer_only = (brush->gpencil_settings->flag & GP_BRUSH_ACTIVE_LAYER_ONLY) != 0;
  std::atomic<bool> changed = false;

  if (active_layer_only) {
    /* Apply trim on drawings of active layer. */
    if (!grease_pencil.has_active_layer()) {
      return OPERATOR_CANCELLED;
    }
    const bke::greasepencil::Layer &layer = *grease_pencil.get_active_layer();
    const float4x4 layer_to_world = layer.to_world_space(*ob_eval);
    const float4x4 projection = ED_view3d_ob_project_mat_get_from_obmat(rv3d, layer_to_world);
    const Vector<ed::greasepencil::MutableDrawingInfo> drawings =
        ed::greasepencil::retrieve_editable_drawings_from_layer(*scene, grease_pencil, layer);
    threading::parallel_for_each(drawings, [&](const ed::greasepencil::MutableDrawingInfo &info) {
      if (execute_trim_on_drawing(info.layer_index,
                                  info.frame_number,
                                  *ob_eval,
                                  *obact,
                                  *region,
                                  projection,
                                  mcoords,
                                  keep_caps,
                                  info.drawing))
      {
        changed = true;
      }
    });
  }
  else {
    /* Apply trim on every editable drawing. */
    const Vector<ed::greasepencil::MutableDrawingInfo> drawings =
        ed::greasepencil::retrieve_editable_drawings(*scene, grease_pencil);
    threading::parallel_for_each(drawings, [&](const ed::greasepencil::MutableDrawingInfo &info) {
      const bke::greasepencil::Layer &layer = grease_pencil.layer(info.layer_index);
      const float4x4 layer_to_world = layer.to_world_space(*ob_eval);
      const float4x4 projection = ED_view3d_ob_project_mat_get_from_obmat(rv3d, layer_to_world);
      if (execute_trim_on_drawing(info.layer_index,
                                  info.frame_number,
                                  *ob_eval,
                                  *obact,
                                  *region,
                                  projection,
                                  mcoords,
                                  keep_caps,
                                  info.drawing))
      {
        changed = true;
      }
    });
  }

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static int grease_pencil_stroke_trim(bContext *C, wmOperator *op)
{
  const Array<int2> mcoords = WM_gesture_lasso_path_to_array(C, op);

  if (mcoords.is_empty()) {
    return OPERATOR_PASS_THROUGH;
  }

  return stroke_trim_execute(C, mcoords);
}

}  // namespace blender::ed::greasepencil

void GREASE_PENCIL_OT_stroke_trim(wmOperatorType *ot)
{
  using namespace blender::ed::greasepencil;

  ot->name = "Grease Pencil Trim";
  ot->idname = "GREASE_PENCIL_OT_stroke_trim";
  ot->description = "Delete stroke points in between intersecting strokes";

  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = grease_pencil_stroke_trim;
  ot->poll = grease_pencil_painting_poll;
  ot->cancel = WM_gesture_lasso_cancel;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  WM_operator_properties_gesture_lasso(ot);
}
