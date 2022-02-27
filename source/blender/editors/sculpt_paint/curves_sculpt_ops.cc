/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_utildefines.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_curves.hh"
#include "BKE_paint.h"

#include "WM_api.h"
#include "WM_toolsystem.h"

#include "ED_curves_sculpt.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "DEG_depsgraph.h"

#include "DNA_brush_types.h"
#include "DNA_curves_types.h"
#include "DNA_screen_types.h"

#include "RNA_access.h"

#include "BLI_index_mask_ops.hh"
#include "BLI_math_vector.hh"

#include "curves_sculpt_intern.h"
#include "paint_intern.h"

/* -------------------------------------------------------------------- */
/** \name Poll Functions
 * \{ */

bool CURVES_SCULPT_mode_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  return ob && ob->mode & OB_MODE_SCULPT_CURVES;
}

bool CURVES_SCULPT_mode_poll_view3d(bContext *C)
{
  if (!CURVES_SCULPT_mode_poll(C)) {
    return false;
  }
  if (CTX_wm_region_view3d(C) == nullptr) {
    return false;
  }
  return true;
}

/** \} */

namespace blender::ed::sculpt_paint {

using blender::bke::CurvesGeometry;

/* -------------------------------------------------------------------- */
/** \name * SCULPT_CURVES_OT_brush_stroke
 * \{ */

struct StrokeExtension {
  bool is_first;
  float2 mouse_position;
};

/**
 * Base class for stroke based operations in curves sculpt mode.
 */
class CurvesSculptStrokeOperation {
 public:
  virtual ~CurvesSculptStrokeOperation() = default;
  virtual void on_stroke_extended(bContext *C, const StrokeExtension &stroke_extension) = 0;
};

class DeleteOperation : public CurvesSculptStrokeOperation {
 private:
  float2 last_mouse_position_;

 public:
  void on_stroke_extended(bContext *C, const StrokeExtension &stroke_extension)
  {
    Scene &scene = *CTX_data_scene(C);
    Object &object = *CTX_data_active_object(C);
    ARegion *region = CTX_wm_region(C);
    RegionView3D *rv3d = CTX_wm_region_view3d(C);

    CurvesSculpt &curves_sculpt = *scene.toolsettings->curves_sculpt;
    Brush &brush = *BKE_paint_brush(&curves_sculpt.paint);
    const float brush_radius = BKE_brush_size_get(&scene, &brush);

    float4x4 projection;
    ED_view3d_ob_project_mat_get(rv3d, &object, projection.values);

    Curves &curves_id = *static_cast<Curves *>(object.data);
    CurvesGeometry &curves = CurvesGeometry::wrap(curves_id.geometry);
    MutableSpan<float3> positions = curves.positions();

    const float2 mouse_start = stroke_extension.is_first ? stroke_extension.mouse_position :
                                                           last_mouse_position_;
    const float2 mouse_end = stroke_extension.mouse_position;

    /* Find indices of curves that have to be removed. */
    Vector<int64_t> indices;
    const IndexMask curves_to_remove = index_mask_ops::find_indices_based_on_predicate(
        curves.curves_range(), 512, indices, [&](const int curve_i) {
          const IndexRange point_range = curves.range_for_curve(curve_i);
          for (const int segment_i : IndexRange(point_range.size() - 1)) {
            const float3 pos1 = positions[point_range[segment_i]];
            const float3 pos2 = positions[point_range[segment_i + 1]];

            float2 pos1_proj, pos2_proj;
            ED_view3d_project_float_v2_m4(region, pos1, pos1_proj, projection.values);
            ED_view3d_project_float_v2_m4(region, pos2, pos2_proj, projection.values);

            const float dist = dist_seg_seg_v2(pos1_proj, pos2_proj, mouse_start, mouse_end);
            if (dist <= brush_radius) {
              return true;
            }
          }
          return false;
        });

    /* Just reset positions instead of actually removing the curves. This is just a prototype. */
    threading::parallel_for(curves_to_remove.index_range(), 512, [&](const IndexRange range) {
      for (const int curve_i : curves_to_remove.slice(range)) {
        for (const int point_i : curves.range_for_curve(curve_i)) {
          positions[point_i] = {0.0f, 0.0f, 0.0f};
        }
      }
    });

    curves.tag_positions_changed();
    DEG_id_tag_update(&curves_id.id, ID_RECALC_GEOMETRY);
    ED_region_tag_redraw(region);

    last_mouse_position_ = stroke_extension.mouse_position;
  }
};

class MoveOperation : public CurvesSculptStrokeOperation {
 private:
  Vector<int64_t> points_to_move_indices_;
  IndexMask points_to_move_;
  float2 last_mouse_position_;

 public:
  void on_stroke_extended(bContext *C, const StrokeExtension &stroke_extension)
  {
    Scene &scene = *CTX_data_scene(C);
    Object &object = *CTX_data_active_object(C);
    ARegion *region = CTX_wm_region(C);
    View3D *v3d = CTX_wm_view3d(C);
    RegionView3D *rv3d = CTX_wm_region_view3d(C);

    CurvesSculpt &curves_sculpt = *scene.toolsettings->curves_sculpt;
    Brush &brush = *BKE_paint_brush(&curves_sculpt.paint);
    const float brush_radius = BKE_brush_size_get(&scene, &brush);

    float4x4 projection;
    ED_view3d_ob_project_mat_get(rv3d, &object, projection.values);

    Curves &curves_id = *static_cast<Curves *>(object.data);
    CurvesGeometry &curves = CurvesGeometry::wrap(curves_id.geometry);
    MutableSpan<float3> positions = curves.positions();

    if (stroke_extension.is_first) {
      /* Find point indices to move. */
      points_to_move_ = index_mask_ops::find_indices_based_on_predicate(
          curves.points_range(), 512, points_to_move_indices_, [&](const int64_t point_i) {
            const float3 position = positions[point_i];
            float2 screen_position;
            ED_view3d_project_float_v2_m4(region, position, screen_position, projection.values);
            const float distance = len_v2v2(screen_position, stroke_extension.mouse_position);
            return distance <= brush_radius;
          });
    }
    else {
      /* Move points based on mouse movement. */
      const float2 mouse_diff = stroke_extension.mouse_position - last_mouse_position_;
      threading::parallel_for(points_to_move_.index_range(), 512, [&](const IndexRange range) {
        for (const int point_i : points_to_move_.slice(range)) {
          const float3 old_position = positions[point_i];
          float2 old_position_screen;
          ED_view3d_project_float_v2_m4(
              region, old_position, old_position_screen, projection.values);
          const float2 new_position_screen = old_position_screen + mouse_diff;
          float3 new_position;
          ED_view3d_win_to_3d(v3d, region, old_position, new_position_screen, new_position);
          positions[point_i] = new_position;
        }
      });

      curves.tag_positions_changed();
      DEG_id_tag_update(&curves_id.id, ID_RECALC_GEOMETRY);
      ED_region_tag_redraw(region);
    }

    last_mouse_position_ = stroke_extension.mouse_position;
  }
};

static std::unique_ptr<CurvesSculptStrokeOperation> start_brush_operation(bContext *C,
                                                                          wmOperator *UNUSED(op))
{
  Scene &scene = *CTX_data_scene(C);
  CurvesSculpt &curves_sculpt = *scene.toolsettings->curves_sculpt;
  Brush &brush = *BKE_paint_brush(&curves_sculpt.paint);
  switch (brush.curves_sculpt_tool) {
    case CURVES_SCULPT_TOOL_TEST1:
      return std::make_unique<MoveOperation>();
    case CURVES_SCULPT_TOOL_TEST2:
      return std::make_unique<DeleteOperation>();
  }
  BLI_assert_unreachable();
  return {};
}

struct SculptCurvesBrushStrokeData {
  std::unique_ptr<CurvesSculptStrokeOperation> operation;
  PaintStroke *stroke;
};

static bool stroke_get_location(bContext *C, float out[3], const float mouse[2])
{
  out[0] = mouse[0];
  out[1] = mouse[1];
  out[2] = 0;
  UNUSED_VARS(C);
  return true;
}

static bool stroke_test_start(bContext *C, struct wmOperator *op, const float mouse[2])
{
  UNUSED_VARS(C, op, mouse);
  return true;
}

static void stroke_update_step(bContext *C,
                               wmOperator *op,
                               PaintStroke *UNUSED(stroke),
                               PointerRNA *stroke_element)
{
  SculptCurvesBrushStrokeData *op_data = static_cast<SculptCurvesBrushStrokeData *>(
      op->customdata);

  StrokeExtension stroke_extension;
  RNA_float_get_array(stroke_element, "mouse", stroke_extension.mouse_position);

  if (!op_data->operation) {
    stroke_extension.is_first = true;
    op_data->operation = start_brush_operation(C, op);
  }
  else {
    stroke_extension.is_first = false;
  }

  op_data->operation->on_stroke_extended(C, stroke_extension);
}

static void stroke_done(const bContext *C, PaintStroke *stroke)
{
  UNUSED_VARS(C, stroke);
}

static int sculpt_curves_stroke_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SculptCurvesBrushStrokeData *op_data = MEM_new<SculptCurvesBrushStrokeData>(__func__);
  op_data->stroke = paint_stroke_new(C,
                                     op,
                                     stroke_get_location,
                                     stroke_test_start,
                                     stroke_update_step,
                                     nullptr,
                                     stroke_done,
                                     event->type);
  op->customdata = op_data;

  int return_value = op->type->modal(C, op, event);
  if (return_value == OPERATOR_FINISHED) {
    paint_stroke_free(C, op, op_data->stroke);
    MEM_delete(op_data);
    return OPERATOR_FINISHED;
  }

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_curves_stroke_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  SculptCurvesBrushStrokeData *op_data = static_cast<SculptCurvesBrushStrokeData *>(
      op->customdata);
  int return_value = paint_stroke_modal(C, op, event, op_data->stroke);
  if (ELEM(return_value, OPERATOR_FINISHED, OPERATOR_CANCELLED)) {
    MEM_delete(op_data);
  }
  return return_value;
}

static void sculpt_curves_stroke_cancel(bContext *C, wmOperator *op)
{
  SculptCurvesBrushStrokeData *op_data = static_cast<SculptCurvesBrushStrokeData *>(
      op->customdata);
  paint_stroke_cancel(C, op, op_data->stroke);
  MEM_delete(op_data);
}

static void SCULPT_CURVES_OT_brush_stroke(struct wmOperatorType *ot)
{
  ot->name = "Stroke Curves Sculpt";
  ot->idname = "SCULPT_CURVES_OT_brush_stroke";
  ot->description = "Sculpt curves using a brush";

  ot->invoke = sculpt_curves_stroke_invoke;
  ot->modal = sculpt_curves_stroke_modal;
  ot->cancel = sculpt_curves_stroke_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  paint_stroke_operator_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name * CURVES_OT_sculptmode_toggle
 * \{ */

static bool curves_sculptmode_toggle_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return false;
  }
  if (ob->type != OB_CURVES) {
    return false;
  }
  return true;
}

static void curves_sculptmode_enter(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  BKE_paint_ensure(scene->toolsettings, (Paint **)&scene->toolsettings->curves_sculpt);
  CurvesSculpt *curves_sculpt = scene->toolsettings->curves_sculpt;

  ob->mode = OB_MODE_SCULPT_CURVES;

  paint_cursor_start(&curves_sculpt->paint, CURVES_SCULPT_mode_poll_view3d);
}

static void curves_sculptmode_exit(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  ob->mode = OB_MODE_OBJECT;
}

static int curves_sculptmode_toggle_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  const bool is_mode_set = ob->mode == OB_MODE_SCULPT_CURVES;

  if (is_mode_set) {
    if (!ED_object_mode_compat_set(C, ob, OB_MODE_SCULPT_CURVES, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (is_mode_set) {
    curves_sculptmode_exit(C);
  }
  else {
    curves_sculptmode_enter(C);
  }

  WM_toolsystem_update_from_context_view3d(C);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);
  return OPERATOR_CANCELLED;
}

static void CURVES_OT_sculptmode_toggle(wmOperatorType *ot)
{
  ot->name = "Curve Sculpt Mode Toggle";
  ot->idname = "CURVES_OT_sculptmode_toggle";
  ot->description = "Enter/Exit sculpt mode for curves";

  ot->exec = curves_sculptmode_toggle_exec;
  ot->poll = curves_sculptmode_toggle_poll;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;
}

}  // namespace blender::ed::sculpt_paint

/** \} */

/* -------------------------------------------------------------------- */
/** \name * Registration
 * \{ */

void ED_operatortypes_sculpt_curves()
{
  using namespace blender::ed::sculpt_paint;
  WM_operatortype_append(SCULPT_CURVES_OT_brush_stroke);
  WM_operatortype_append(CURVES_OT_sculptmode_toggle);
}

/** \} */
