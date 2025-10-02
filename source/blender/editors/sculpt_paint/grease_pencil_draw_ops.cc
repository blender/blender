/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "ANIM_keyframing.hh"

#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"
#include "BKE_deform.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_material.hh"
#include "BKE_object_deform.h"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "BLI_array_utils.hh"
#include "BLI_assert.h"
#include "BLI_bounds.hh"
#include "BLI_color.hh"
#include "BLI_index_mask.hh"
#include "BLI_kdopbvh.hh"
#include "BLI_kdtree.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_offset_indices.hh"
#include "BLI_rect.h"

#include "DNA_brush_enums.h"
#include "DNA_brush_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "DEG_depsgraph_query.hh"

#include "GEO_curves_remove_and_split.hh"
#include "GEO_join_geometries.hh"
#include "GEO_smooth_curves.hh"

#include "ED_grease_pencil.hh"
#include "ED_image.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_view3d.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "grease_pencil_intern.hh"
#include "paint_intern.hh"
#include "wm_event_types.hh"

#include <algorithm>
#include <fmt/format.h>
#include <optional>

namespace blender::ed::sculpt_paint {

/* -------------------------------------------------------------------- */
/** \name Common Paint Operator Functions
 * \{ */

static bool stroke_get_location(bContext * /*C*/,
                                float out[3],
                                const float mouse[2],
                                bool /*force_original*/)
{
  out[0] = mouse[0];
  out[1] = mouse[1];
  out[2] = 0;
  return true;
}

static std::unique_ptr<GreasePencilStrokeOperation> get_stroke_operation(bContext &C,
                                                                         wmOperator *op)
{
  const Paint *paint = BKE_paint_get_active_from_context(&C);
  const Brush &brush = *BKE_paint_brush_for_read(paint);
  const PaintMode mode = BKE_paintmode_get_active_from_context(&C);
  const BrushStrokeMode stroke_mode = BrushStrokeMode(RNA_enum_get(op->ptr, "mode"));

  if (mode == PaintMode::GPencil) {
    if (eBrushGPaintType(brush.gpencil_brush_type) == GPAINT_BRUSH_TYPE_DRAW &&
        stroke_mode == BRUSH_STROKE_ERASE)
    {
      /* Special case: We're using the draw tool but with the eraser mode, so create an erase
       * operation. */
      return greasepencil::new_erase_operation(true);
    }
    /* FIXME: Somehow store the unique_ptr in the PaintStroke. */
    switch (eBrushGPaintType(brush.gpencil_brush_type)) {
      case GPAINT_BRUSH_TYPE_DRAW:
        return greasepencil::new_paint_operation();
      case GPAINT_BRUSH_TYPE_ERASE:
        return greasepencil::new_erase_operation();
      case GPAINT_BRUSH_TYPE_FILL:
        /* Fill tool keymap uses the paint operator to draw fill guides. */
        return greasepencil::new_paint_operation(/* do_fill_guides = */ true);
      case GPAINT_BRUSH_TYPE_TINT:
        return greasepencil::new_tint_operation(stroke_mode == BRUSH_STROKE_ERASE);
    }
  }
  else if (mode == PaintMode::SculptGPencil) {

    if (stroke_mode == BRUSH_STROKE_SMOOTH) {
      return greasepencil::new_smooth_operation(stroke_mode, true);
    }
    switch (eBrushGPSculptType(brush.gpencil_sculpt_brush_type)) {
      case GPSCULPT_BRUSH_TYPE_SMOOTH:
        return greasepencil::new_smooth_operation(stroke_mode);
      case GPSCULPT_BRUSH_TYPE_THICKNESS:
        return greasepencil::new_thickness_operation(stroke_mode);
      case GPSCULPT_BRUSH_TYPE_STRENGTH:
        return greasepencil::new_strength_operation(stroke_mode);
      case GPSCULPT_BRUSH_TYPE_GRAB:
        return greasepencil::new_grab_operation(stroke_mode);
      case GPSCULPT_BRUSH_TYPE_PUSH:
        return greasepencil::new_push_operation(stroke_mode);
      case GPSCULPT_BRUSH_TYPE_TWIST:
        return greasepencil::new_twist_operation(stroke_mode);
      case GPSCULPT_BRUSH_TYPE_PINCH:
        return greasepencil::new_pinch_operation(stroke_mode);
      case GPSCULPT_BRUSH_TYPE_RANDOMIZE:
        return greasepencil::new_randomize_operation(stroke_mode);
      case GPSCULPT_BRUSH_TYPE_CLONE:
        return greasepencil::new_clone_operation(stroke_mode);
    }
  }
  else if (mode == PaintMode::WeightGPencil) {
    switch (eBrushGPWeightType(brush.gpencil_weight_brush_type)) {
      case GPWEIGHT_BRUSH_TYPE_DRAW:
        return greasepencil::new_weight_paint_draw_operation(stroke_mode);
      case GPWEIGHT_BRUSH_TYPE_BLUR:
        return greasepencil::new_weight_paint_blur_operation();
      case GPWEIGHT_BRUSH_TYPE_AVERAGE:
        return greasepencil::new_weight_paint_average_operation();
      case GPWEIGHT_BRUSH_TYPE_SMEAR:
        return greasepencil::new_weight_paint_smear_operation();
    }
  }
  else if (mode == PaintMode::VertexGPencil) {
    switch (eBrushGPVertexType(brush.gpencil_vertex_brush_type)) {
      case GPVERTEX_BRUSH_TYPE_DRAW:
        return greasepencil::new_vertex_paint_operation(stroke_mode);
      case GPVERTEX_BRUSH_TYPE_BLUR:
        return greasepencil::new_vertex_blur_operation();
      case GPVERTEX_BRUSH_TYPE_AVERAGE:
        return greasepencil::new_vertex_average_operation();
      case GPVERTEX_BRUSH_TYPE_SMEAR:
        return greasepencil::new_vertex_smear_operation();
      case GPVERTEX_BRUSH_TYPE_REPLACE:
        return greasepencil::new_vertex_replace_operation();
      case GPVERTEX_BRUSH_TYPE_TINT:
        /* Unused. */
        BLI_assert_unreachable();
        return nullptr;
    }
  }
  return nullptr;
}

static bool stroke_test_start(bContext *C, wmOperator *op, const float mouse[2])
{
  UNUSED_VARS(C, op, mouse);
  return true;
}

static void stroke_update_step(bContext *C,
                               wmOperator *op,
                               PaintStroke *stroke,
                               PointerRNA *stroke_element)
{
  GreasePencilStrokeOperation *operation = static_cast<GreasePencilStrokeOperation *>(
      paint_stroke_mode_data(stroke));

  InputSample sample;
  RNA_float_get_array(stroke_element, "mouse", sample.mouse_position);
  sample.pressure = RNA_float_get(stroke_element, "pressure");

  if (!operation) {
    std::unique_ptr<GreasePencilStrokeOperation> new_operation = get_stroke_operation(*C, op);
    BLI_assert(new_operation != nullptr);
    new_operation->on_stroke_begin(*C, sample);
    paint_stroke_set_mode_data(stroke, std::move(new_operation));
  }
  else {
    operation->on_stroke_extended(*C, sample);
  }
}

static void stroke_redraw(const bContext *C, PaintStroke * /*stroke*/, bool /*final*/)
{
  ED_region_tag_redraw(CTX_wm_region(C));
}

static void stroke_done(const bContext *C, PaintStroke *stroke)
{
  GreasePencilStrokeOperation *operation = static_cast<GreasePencilStrokeOperation *>(
      paint_stroke_mode_data(stroke));
  if (operation != nullptr) {
    operation->on_stroke_done(*C);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Brush Stroke Operator
 * \{ */

static bool grease_pencil_brush_stroke_poll(bContext *C)
{
  if (!ed::greasepencil::grease_pencil_painting_poll(C)) {
    return false;
  }
  if (!WM_toolsystem_active_tool_is_brush(C)) {
    return false;
  }
  return true;
}

static wmOperatorStatus grease_pencil_brush_stroke_invoke(bContext *C,
                                                          wmOperator *op,
                                                          const wmEvent *event)
{
  if (event->tablet.active == EVT_TABLET_ERASER) {
    RNA_enum_set(op->ptr, "mode", BRUSH_STROKE_ERASE);
  }

  const bool use_duplicate_previous_key = [&]() -> bool {
    const Paint *paint = BKE_paint_get_active_from_context(C);
    const Brush &brush = *BKE_paint_brush_for_read(paint);
    const PaintMode mode = BKE_paintmode_get_active_from_context(C);
    const BrushStrokeMode stroke_mode = BrushStrokeMode(RNA_enum_get(op->ptr, "mode"));

    if (mode == PaintMode::GPencil) {
      /* For the eraser and tint tool, we don't want auto-key to create an empty keyframe, so we
       * duplicate the previous frame. */
      if (ELEM(eBrushGPaintType(brush.gpencil_brush_type),
               GPAINT_BRUSH_TYPE_ERASE,
               GPAINT_BRUSH_TYPE_TINT))
      {
        return true;
      }
      /* Same for the temporary eraser when using the draw tool. */
      if (eBrushGPaintType(brush.gpencil_brush_type) == GPAINT_BRUSH_TYPE_DRAW &&
          stroke_mode == BRUSH_STROKE_ERASE)
      {
        return true;
      }
    }
    return false;
  }();
  wmOperatorStatus retval = ed::greasepencil::grease_pencil_draw_operator_invoke(
      C, op, use_duplicate_previous_key);
  if (retval != OPERATOR_RUNNING_MODAL) {
    return retval;
  }

  op->customdata = paint_stroke_new(C,
                                    op,
                                    stroke_get_location,
                                    stroke_test_start,
                                    stroke_update_step,
                                    stroke_redraw,
                                    stroke_done,
                                    event->type);

  retval = op->type->modal(C, op, event);
  OPERATOR_RETVAL_CHECK(retval);

  if (retval == OPERATOR_FINISHED) {
    return OPERATOR_FINISHED;
  }

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus grease_pencil_brush_stroke_modal(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent *event)
{
  return paint_stroke_modal(C, op, event, reinterpret_cast<PaintStroke **>(&op->customdata));
}

static void grease_pencil_brush_stroke_cancel(bContext *C, wmOperator *op)
{
  paint_stroke_cancel(C, op, static_cast<PaintStroke *>(op->customdata));
}

static void GREASE_PENCIL_OT_brush_stroke(wmOperatorType *ot)
{
  ot->name = "Grease Pencil Draw";
  ot->idname = "GREASE_PENCIL_OT_brush_stroke";
  ot->description = "Draw a new stroke in the active Grease Pencil object";

  ot->poll = grease_pencil_brush_stroke_poll;
  ot->invoke = grease_pencil_brush_stroke_invoke;
  ot->modal = grease_pencil_brush_stroke_modal;
  ot->cancel = grease_pencil_brush_stroke_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  paint_stroke_operator_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Operator
 * \{ */

static bool grease_pencil_sculpt_paint_poll(bContext *C)
{
  if (!ed::greasepencil::grease_pencil_sculpting_poll(C)) {
    return false;
  }
  if (!WM_toolsystem_active_tool_is_brush(C)) {
    return false;
  }
  return true;
}

static wmOperatorStatus grease_pencil_sculpt_paint_invoke(bContext *C,
                                                          wmOperator *op,
                                                          const wmEvent *event)
{
  const Scene *scene = CTX_data_scene(C);
  const Object *object = CTX_data_active_object(C);
  if (!object || object->type != OB_GREASE_PENCIL) {
    return OPERATOR_CANCELLED;
  }

  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  if (!ed::greasepencil::has_editable_layer(grease_pencil)) {
    BKE_report(op->reports, RPT_ERROR, "No editable Grease Pencil layer");
    return OPERATOR_CANCELLED;
  }

  const Paint *paint = BKE_paint_get_active_from_context(C);
  const Brush *brush = BKE_paint_brush_for_read(paint);
  if (brush == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* Ensure a drawing at the current keyframe. */
  bool inserted_keyframe = false;
  /* For the sculpt tools, we don't want the auto-key to create an empty keyframe, so we duplicate
   * the previous key. */
  const bool use_duplicate_previous_key = true;
  for (bke::greasepencil::Layer *layer : grease_pencil.layers_for_write()) {
    if (layer->is_editable() &&
        ed::greasepencil::ensure_active_keyframe(
            *scene, grease_pencil, *layer, use_duplicate_previous_key, inserted_keyframe))
    {
      inserted_keyframe = true;
    }
  }
  if (!inserted_keyframe) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil frame to draw on");
    return OPERATOR_CANCELLED;
  }
  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, nullptr);

  op->customdata = paint_stroke_new(C,
                                    op,
                                    stroke_get_location,
                                    stroke_test_start,
                                    stroke_update_step,
                                    stroke_redraw,
                                    stroke_done,
                                    event->type);

  const wmOperatorStatus retval = op->type->modal(C, op, event);
  OPERATOR_RETVAL_CHECK(retval);

  if (retval == OPERATOR_FINISHED) {
    return OPERATOR_FINISHED;
  }

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus grease_pencil_sculpt_paint_modal(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent *event)
{
  return paint_stroke_modal(C, op, event, reinterpret_cast<PaintStroke **>(&op->customdata));
}

static void grease_pencil_sculpt_paint_cancel(bContext *C, wmOperator *op)
{
  paint_stroke_cancel(C, op, static_cast<PaintStroke *>(op->customdata));
}

static void GREASE_PENCIL_OT_sculpt_paint(wmOperatorType *ot)
{
  ot->name = "Grease Pencil Sculpt";
  ot->idname = "GREASE_PENCIL_OT_sculpt_paint";
  ot->description = "Sculpt strokes in the active Grease Pencil object";

  ot->poll = grease_pencil_sculpt_paint_poll;
  ot->invoke = grease_pencil_sculpt_paint_invoke;
  ot->modal = grease_pencil_sculpt_paint_modal;
  ot->cancel = grease_pencil_sculpt_paint_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  paint_stroke_operator_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Weight Brush Stroke Operator
 * \{ */

static bool grease_pencil_weight_brush_stroke_poll(bContext *C)
{
  if (!ed::greasepencil::grease_pencil_weight_painting_poll(C)) {
    return false;
  }
  if (!WM_toolsystem_active_tool_is_brush(C)) {
    return false;
  }
  return true;
}

static wmOperatorStatus grease_pencil_weight_brush_stroke_invoke(bContext *C,
                                                                 wmOperator *op,
                                                                 const wmEvent *event)
{
  const Scene *scene = CTX_data_scene(C);
  const Object *object = CTX_data_active_object(C);
  if (!object || object->type != OB_GREASE_PENCIL) {
    return OPERATOR_CANCELLED;
  }

  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const Paint *paint = BKE_paint_get_active_from_context(C);
  const Brush *brush = BKE_paint_brush_for_read(paint);
  if (brush == nullptr) {
    return OPERATOR_CANCELLED;
  }

  const Vector<ed::greasepencil::MutableDrawingInfo> drawings =
      ed::greasepencil::retrieve_editable_drawings(*scene, grease_pencil);
  if (drawings.is_empty()) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil frame to draw weight on");
    return OPERATOR_CANCELLED;
  }

  const int active_defgroup_nr = BKE_object_defgroup_active_index_get(object) - 1;
  if (active_defgroup_nr >= 0 && BKE_object_defgroup_active_is_locked(object)) {
    BKE_report(op->reports, RPT_WARNING, "Active group is locked, aborting");
    return OPERATOR_CANCELLED;
  }

  op->customdata = paint_stroke_new(C,
                                    op,
                                    stroke_get_location,
                                    stroke_test_start,
                                    stroke_update_step,
                                    stroke_redraw,
                                    stroke_done,
                                    event->type);

  const wmOperatorStatus retval = op->type->modal(C, op, event);
  OPERATOR_RETVAL_CHECK(retval);

  if (retval == OPERATOR_FINISHED) {
    return OPERATOR_FINISHED;
  }

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus grease_pencil_weight_brush_stroke_modal(bContext *C,
                                                                wmOperator *op,
                                                                const wmEvent *event)
{
  return paint_stroke_modal(C, op, event, reinterpret_cast<PaintStroke **>(&op->customdata));
}

static void grease_pencil_weight_brush_stroke_cancel(bContext *C, wmOperator *op)
{
  paint_stroke_cancel(C, op, static_cast<PaintStroke *>(op->customdata));
}

static void GREASE_PENCIL_OT_weight_brush_stroke(wmOperatorType *ot)
{
  ot->name = "Grease Pencil Paint Weight";
  ot->idname = "GREASE_PENCIL_OT_weight_brush_stroke";
  ot->description = "Draw weight on stroke points in the active Grease Pencil object";

  ot->poll = grease_pencil_weight_brush_stroke_poll;
  ot->invoke = grease_pencil_weight_brush_stroke_invoke;
  ot->modal = grease_pencil_weight_brush_stroke_modal;
  ot->cancel = grease_pencil_weight_brush_stroke_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  paint_stroke_operator_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Brush Stroke Operator
 * \{ */

static bool grease_pencil_vertex_brush_stroke_poll(bContext *C)
{
  if (!ed::greasepencil::grease_pencil_vertex_painting_poll(C)) {
    return false;
  }
  if (!WM_toolsystem_active_tool_is_brush(C)) {
    return false;
  }
  return true;
}

static wmOperatorStatus grease_pencil_vertex_brush_stroke_invoke(bContext *C,
                                                                 wmOperator *op,
                                                                 const wmEvent *event)
{
  const Scene *scene = CTX_data_scene(C);
  const Object *object = CTX_data_active_object(C);
  if (!object || object->type != OB_GREASE_PENCIL) {
    return OPERATOR_CANCELLED;
  }

  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  if (!ed::greasepencil::has_editable_layer(grease_pencil)) {
    BKE_report(op->reports, RPT_ERROR, "No editable Grease Pencil layer");
    return OPERATOR_CANCELLED;
  }

  const Paint *paint = BKE_paint_get_active_from_context(C);
  const Brush *brush = BKE_paint_brush_for_read(paint);
  if (brush == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* Ensure a drawing at the current keyframe. */
  bool inserted_keyframe = false;
  /* For the vertex paint tools, we don't want the auto-key to create an empty keyframe, so we
   * duplicate the previous key. */
  const bool use_duplicate_previous_key = true;
  for (bke::greasepencil::Layer *layer : grease_pencil.layers_for_write()) {
    if (layer->is_editable() &&
        ed::greasepencil::ensure_active_keyframe(
            *scene, grease_pencil, *layer, use_duplicate_previous_key, inserted_keyframe))
    {
      inserted_keyframe = true;
    }
  }
  if (!inserted_keyframe) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil frame to draw on");
    return OPERATOR_CANCELLED;
  }
  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, nullptr);

  op->customdata = paint_stroke_new(C,
                                    op,
                                    stroke_get_location,
                                    stroke_test_start,
                                    stroke_update_step,
                                    stroke_redraw,
                                    stroke_done,
                                    event->type);

  const wmOperatorStatus retval = op->type->modal(C, op, event);
  OPERATOR_RETVAL_CHECK(retval);

  if (retval == OPERATOR_FINISHED) {
    return OPERATOR_FINISHED;
  }

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus grease_pencil_vertex_brush_stroke_modal(bContext *C,
                                                                wmOperator *op,
                                                                const wmEvent *event)
{
  return paint_stroke_modal(C, op, event, reinterpret_cast<PaintStroke **>(&op->customdata));
}

static void grease_pencil_vertex_brush_stroke_cancel(bContext *C, wmOperator *op)
{
  paint_stroke_cancel(C, op, static_cast<PaintStroke *>(op->customdata));
}

static void GREASE_PENCIL_OT_vertex_brush_stroke(wmOperatorType *ot)
{
  ot->name = "Grease Pencil Paint Vertex";
  ot->idname = "GREASE_PENCIL_OT_vertex_brush_stroke";
  ot->description = "Draw on vertex colors in the active Grease Pencil object";

  ot->poll = grease_pencil_vertex_brush_stroke_poll;
  ot->invoke = grease_pencil_vertex_brush_stroke_invoke;
  ot->modal = grease_pencil_vertex_brush_stroke_modal;
  ot->cancel = grease_pencil_vertex_brush_stroke_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  paint_stroke_operator_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bucket Fill Operator
 * \{ */

struct GreasePencilFillOpData {
  blender::bke::greasepencil::Layer &layer;

  /* Material of the generated stroke. */
  int material_index;
  /* Toggle inverse filling. */
  bool invert = false;
  /* Toggle precision mode. */
  bool precision = false;

  /* Methods for gap filling. */
  eGP_FillExtendModes extension_mode;
  /* Length of extension lines. */
  float extension_length;
  /* Cut off extension lines at first intersection. */
  bool extension_cut;

  /* Draw boundaries stroke overlay. */
  bool show_boundaries;
  /* Draw extension lines overlay. */
  bool show_extension;

  /* Mouse position where fill was initialized */
  float2 fill_mouse_pos;
  /* Extension lines drag mode is enabled (middle mouse button). */
  bool is_extension_drag_active = false;
  /* Mouse position where the extension mode was enabled. */
  float2 extension_mouse_pos;

  /* Overlay draw callback for helper lines, etc. */
  void *overlay_cb_handle;

  static GreasePencilFillOpData from_context(bContext &C,
                                             blender::bke::greasepencil::Layer &layer,
                                             const int material_index,
                                             const bool invert,
                                             const bool precision)
  {
    using blender::bke::greasepencil::Layer;

    const ToolSettings &ts = *CTX_data_tool_settings(&C);
    const Brush &brush = *BKE_paint_brush(&ts.gp_paint->paint);
    const eGP_FillExtendModes extension_mode = eGP_FillExtendModes(
        brush.gpencil_settings->fill_extend_mode);
    const bool show_boundaries = brush.gpencil_settings->flag & GP_BRUSH_FILL_SHOW_HELPLINES;
    const bool show_extension = brush.gpencil_settings->flag & GP_BRUSH_FILL_SHOW_EXTENDLINES;
    const float extension_length = brush.gpencil_settings->fill_extend_fac *
                                   bke::greasepencil::LEGACY_RADIUS_CONVERSION_FACTOR;
    const bool extension_cut = brush.gpencil_settings->flag & GP_BRUSH_FILL_STROKE_COLLIDE;
    const bool brush_invert = brush.gpencil_settings->fill_direction == BRUSH_DIR_IN;
    /* Both operator properties and brush properties can invert. Actual invert is XOR of both. */
    const bool combined_invert = (invert != brush_invert);

    return {layer,
            material_index,
            combined_invert,
            precision,
            extension_mode,
            extension_length,
            extension_cut,
            show_boundaries,
            show_extension};
  }
};

/* Find and cut extension lines at intersections with other lines and strokes. */
static void grease_pencil_fill_extension_cut(const bContext &C,
                                             ed::greasepencil::ExtensionData &extension_data,
                                             Span<int> origin_drawings,
                                             Span<int> origin_points)
{
  const RegionView3D &rv3d = *CTX_wm_region_view3d(&C);
  const Scene &scene = *CTX_data_scene(&C);
  const Object &object = *CTX_data_active_object(&C);
  const GreasePencil &grease_pencil = *static_cast<const GreasePencil *>(object.data);

  const float4x4 view_matrix = float4x4(rv3d.viewmat);

  const Vector<ed::greasepencil::DrawingInfo> drawings =
      ed::greasepencil::retrieve_visible_drawings(scene, grease_pencil, false);

  const IndexRange bvh_extension_range = extension_data.lines.starts.index_range();
  Array<int> bvh_curve_offsets_data(drawings.size() + 1);
  for (const int i : drawings.index_range()) {
    bvh_curve_offsets_data[i] = drawings[i].drawing.strokes().points_num();
  }
  const OffsetIndices bvh_curve_offsets = offset_indices::accumulate_counts_to_offsets(
      bvh_curve_offsets_data, bvh_extension_range.size());

  /* Upper bound for segment count. Arrays are sized for easy index mapping, exact count isn't
   * necessary. Not all entries are added to the BVH tree. */
  const int max_bvh_lines = bvh_curve_offsets.data().last();
  /* Cached view positions for lines. */
  Array<float2> view_starts(max_bvh_lines);
  Array<float2> view_ends(max_bvh_lines);

  BVHTree *tree = BLI_bvhtree_new(max_bvh_lines, 0.0f, 4, 6);
  BLI_SCOPED_DEFER([&]() { BLI_bvhtree_free(tree); });

  /* Insert extension lines for intersection.
   * Note: These are added first, so that the extension index matches its index in BVH data. */
  for (const int i_line : bvh_extension_range.index_range()) {
    const float2 start =
        math::transform_point(view_matrix, extension_data.lines.starts[i_line]).xy();
    const float2 end = math::transform_point(view_matrix, extension_data.lines.ends[i_line]).xy();

    const int bvh_index = bvh_extension_range[i_line];
    view_starts[bvh_index] = start;
    view_ends[bvh_index] = end;

    const float bb[6] = {start.x, start.y, 0.0f, end.x, end.y, 0.0f};
    BLI_bvhtree_insert(tree, bvh_index, bb, 2);
  }

  /* Insert segments for cutting extensions on stroke intersection. */
  for (const int i_drawing : drawings.index_range()) {
    const ed::greasepencil::DrawingInfo &info = drawings[i_drawing];
    const bke::CurvesGeometry &curves = info.drawing.strokes();
    const OffsetIndices points_by_curve = curves.points_by_curve();
    const Span<float3> positions = curves.positions();
    const VArray<bool> cyclic = curves.cyclic();
    const bke::greasepencil::Layer &layer = grease_pencil.layer(info.layer_index);
    const float4x4 layer_to_view = view_matrix * layer.to_world_space(object);

    for (const int i_curve : curves.curves_range()) {
      const bool is_cyclic = cyclic[i_curve];
      const IndexRange points = points_by_curve[i_curve];

      for (const int i_point : points.drop_back(1)) {
        const float2 start = math::transform_point(layer_to_view, positions[i_point]).xy();
        const float2 end = math::transform_point(layer_to_view, positions[i_point + 1]).xy();

        const int bvh_index = bvh_curve_offsets[i_drawing][i_point];
        view_starts[bvh_index] = start;
        view_ends[bvh_index] = end;

        const float bb[6] = {start.x, start.y, 0.0f, end.x, end.y, 0.0f};
        BLI_bvhtree_insert(tree, bvh_index, bb, 2);
      }
      /* Last->first point segment only used for cyclic curves. */
      if (is_cyclic) {
        const float2 start = math::transform_point(layer_to_view, positions[points.last()]).xy();
        const float2 end = math::transform_point(layer_to_view, positions[points.first()]).xy();

        const int bvh_index = bvh_curve_offsets[i_drawing][points.last()];
        view_starts[bvh_index] = start;
        view_ends[bvh_index] = end;

        const float bb[6] = {start.x, start.y, 0.0f, end.x, end.y, 0.0f};
        BLI_bvhtree_insert(tree, bvh_index, bb, 2);
      }
    }
  }

  BLI_bvhtree_balance(tree);

  struct RaycastArgs {
    Span<float2> starts;
    Span<float2> ends;
    /* Indices that may need to be ignored to avoid self-intersection. */
    int ignore_index1;
    int ignore_index2;
  };
  BVHTree_RayCastCallback callback =
      [](void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit) {
        using Result = math::isect_result<float2>;

        const RaycastArgs &args = *static_cast<const RaycastArgs *>(userdata);
        if (ELEM(index, args.ignore_index1, args.ignore_index2)) {
          return;
        }

        const float2 ray_start = float2(ray->origin);
        const float2 ray_end = ray_start + float2(ray->direction) * ray->radius;
        const float2 &line_start = args.starts[index];
        const float2 &line_end = args.ends[index];
        Result result = math::isect_seg_seg(ray_start, ray_end, line_start, line_end);
        if (result.kind <= 0) {
          return;
        }
        const float dist = result.lambda * math::distance(ray_start, ray_end);
        if (dist >= hit->dist) {
          return;
        }
        /* These always need to be calculated for the BVH traversal function. */
        hit->index = index;
        hit->dist = result.lambda * math::distance(ray_start, ray_end);
        /* Don't need the hit point, only the lambda. */
        hit->no[0] = result.lambda;
      };

  /* Store intersections first before applying to the data, so that subsequent ray-casts use
   * original end points until all intersections are found. */
  Vector<float3> new_extension_ends(extension_data.lines.ends.size());
  for (const int i_line : extension_data.lines.starts.index_range()) {
    const float2 start =
        math::transform_point(view_matrix, extension_data.lines.starts[i_line]).xy();
    const float2 end = math::transform_point(view_matrix, extension_data.lines.ends[i_line]).xy();
    float length;
    const float2 dir = math::normalize_and_get_length(end - start, length);

    const int bvh_index = i_line;
    const int origin_drawing = origin_drawings[i_line];
    const int origin_point = origin_points[i_line];
    const int bvh_origin_index = bvh_curve_offsets[origin_drawing][origin_point];

    RaycastArgs args = {view_starts, view_ends, bvh_index, bvh_origin_index};
    BVHTreeRayHit hit;
    hit.index = -1;
    hit.dist = FLT_MAX;
    BLI_bvhtree_ray_cast(
        tree, float3(start, 0.0f), float3(dir, 0.0f), length, &hit, callback, &args);

    if (hit.index >= 0) {
      const float lambda = hit.no[0];
      new_extension_ends[i_line] = math::interpolate(
          extension_data.lines.starts[i_line], extension_data.lines.ends[i_line], lambda);
    }
    else {
      new_extension_ends[i_line] = extension_data.lines.ends[i_line];
    }
  }

  extension_data.lines.ends = std::move(new_extension_ends);
}

/* Find closest point in each circle and generate extension lines between such pairs. */
static void grease_pencil_fill_extension_lines_from_circles(
    const bContext &C,
    ed::greasepencil::ExtensionData &extension_data,
    Span<int> /*origin_drawings*/,
    Span<int> /*origin_points*/)
{
  const RegionView3D &rv3d = *CTX_wm_region_view3d(&C);
  const Scene &scene = *CTX_data_scene(&C);
  const Object &object = *CTX_data_active_object(&C);
  const GreasePencil &grease_pencil = *static_cast<const GreasePencil *>(object.data);

  const float4x4 view_matrix = float4x4(rv3d.viewmat);

  const Vector<ed::greasepencil::DrawingInfo> drawings =
      ed::greasepencil::retrieve_visible_drawings(scene, grease_pencil, false);

  const IndexRange circles_range = extension_data.circles.centers.index_range();
  /* TODO Include high-curvature feature points. */
  const IndexRange feature_points_range = circles_range.after(0);
  const IndexRange kd_points_range = IndexRange(circles_range.size() +
                                                feature_points_range.size());

  /* Upper bound for segment count. Arrays are sized for easy index mapping, exact count isn't
   * necessary. Not all entries are added to the BVH tree. */
  const int max_kd_entries = kd_points_range.size();
  /* Cached view positions for lines. */
  Array<float2> view_centers(max_kd_entries);
  Array<float> view_radii(max_kd_entries);

  KDTree_2d *kdtree = BLI_kdtree_2d_new(max_kd_entries);

  /* Insert points for overlap tests. */
  for (const int point_i : circles_range.index_range()) {
    const float2 center =
        math::transform_point(view_matrix, extension_data.circles.centers[point_i]).xy();
    const float radius = math::average(math::to_scale(view_matrix)) *
                         extension_data.circles.radii[point_i];

    const int kd_index = circles_range[point_i];
    view_centers[kd_index] = center;
    view_radii[kd_index] = radius;

    BLI_kdtree_2d_insert(kdtree, kd_index, center);
  }
  for (const int i_point : feature_points_range.index_range()) {
    /* TODO Insert feature points into the KDTree. */
    UNUSED_VARS(i_point);
  }
  BLI_kdtree_2d_balance(kdtree);

  struct {
    Vector<float3> starts;
    Vector<float3> ends;
  } connection_lines;
  /* Circles which can be kept because they generate no extension lines. */
  Vector<int> keep_circle_indices;
  keep_circle_indices.reserve(circles_range.size());

  for (const int point_i : circles_range.index_range()) {
    const int kd_index = circles_range[point_i];
    const float2 center = view_centers[kd_index];
    const float radius = view_radii[kd_index];

    bool found = false;
    BLI_kdtree_2d_range_search_cb_cpp(
        kdtree,
        center,
        radius,
        [&](const int other_point_i, const float * /*co*/, float /*dist_sq*/) {
          if (other_point_i == kd_index) {
            return true;
          }

          found = true;
          connection_lines.starts.append(extension_data.circles.centers[point_i]);
          if (circles_range.contains(other_point_i)) {
            connection_lines.ends.append(extension_data.circles.centers[other_point_i]);
          }
          else if (feature_points_range.contains(other_point_i)) {
            /* TODO copy feature point to connection_lines (beware of start index!). */
            connection_lines.ends.append(float3(0));
          }
          else {
            BLI_assert_unreachable();
          }
          return true;
        });
    /* Keep the circle if no extension line was found. */
    if (!found) {
      keep_circle_indices.append(point_i);
    }
  }

  BLI_kdtree_2d_free(kdtree);

  /* Add new extension lines. */
  extension_data.lines.starts.extend(connection_lines.starts);
  extension_data.lines.ends.extend(connection_lines.ends);
  /* Remove circles that formed extension lines. */
  Vector<float3> old_centers = std::move(extension_data.circles.centers);
  Vector<float> old_radii = std::move(extension_data.circles.radii);
  extension_data.circles.centers.resize(keep_circle_indices.size());
  extension_data.circles.radii.resize(keep_circle_indices.size());
  array_utils::gather(old_centers.as_span(),
                      keep_circle_indices.as_span(),
                      extension_data.circles.centers.as_mutable_span());
  array_utils::gather(old_radii.as_span(),
                      keep_circle_indices.as_span(),
                      extension_data.circles.radii.as_mutable_span());
}

static ed::greasepencil::ExtensionData grease_pencil_fill_get_extension_data(
    const bContext &C, const GreasePencilFillOpData &op_data)
{
  const Scene &scene = *CTX_data_scene(&C);
  const Object &object = *CTX_data_active_object(&C);
  const GreasePencil &grease_pencil = *static_cast<const GreasePencil *>(object.data);

  const Vector<ed::greasepencil::DrawingInfo> drawings =
      ed::greasepencil::retrieve_visible_drawings(scene, grease_pencil, false);

  ed::greasepencil::ExtensionData extension_data;
  Vector<int> origin_points;
  Vector<int> origin_drawings;
  for (const int i_drawing : drawings.index_range()) {
    const ed::greasepencil::DrawingInfo &info = drawings[i_drawing];
    const bke::CurvesGeometry &curves = info.drawing.strokes();
    const OffsetIndices points_by_curve = curves.points_by_curve();
    const Span<float3> positions = curves.positions();
    const VArray<bool> cyclic = curves.cyclic();
    const float4x4 layer_to_world = grease_pencil.layer(info.layer_index).to_world_space(object);

    for (const int i_curve : curves.curves_range()) {
      const IndexRange points = points_by_curve[i_curve];
      /* No extension lines on cyclic curves. */
      if (cyclic[i_curve]) {
        continue;
      }
      /* Can't compute directions for single-point curves. */
      if (points.size() < 2) {
        continue;
      }

      const float3 pos_head = math::transform_point(layer_to_world, positions[points[0]]);
      const float3 pos_tail = math::transform_point(layer_to_world, positions[points.last()]);
      const float3 pos_head_next = math::transform_point(layer_to_world, positions[points[1]]);
      const float3 pos_tail_prev = math::transform_point(layer_to_world,
                                                         positions[points.last(1)]);
      const float3 dir_head = math::normalize(pos_head - pos_head_next);
      const float3 dir_tail = math::normalize(pos_tail - pos_tail_prev);
      /* Initial length before intersection tests. */
      const float length = op_data.extension_length;

      switch (op_data.extension_mode) {
        case GP_FILL_EMODE_EXTEND:
          extension_data.lines.starts.append(pos_head);
          extension_data.lines.ends.append(pos_head + dir_head * length);
          origin_drawings.append(i_drawing);
          origin_points.append(points.first());

          extension_data.lines.starts.append(pos_tail);
          extension_data.lines.ends.append(pos_tail + dir_tail * length);
          origin_drawings.append(i_drawing);
          /* Segment index is the start point. */
          origin_points.append(points.last() - 1);
          break;
        case GP_FILL_EMODE_RADIUS:
          extension_data.circles.centers.append(pos_head);
          extension_data.circles.radii.append(length);
          origin_drawings.append(i_drawing);
          origin_points.append(points.first());

          extension_data.circles.centers.append(pos_tail);
          extension_data.circles.radii.append(length);
          origin_drawings.append(i_drawing);
          /* Segment index is the start point. */
          origin_points.append(points.last() - 1);
          break;
      }
    }
  }

  switch (op_data.extension_mode) {
    case GP_FILL_EMODE_EXTEND:
      /* Intersection test against strokes and other extension lines. */
      if (op_data.extension_cut) {
        grease_pencil_fill_extension_cut(C, extension_data, origin_drawings, origin_points);
      }
      break;
    case GP_FILL_EMODE_RADIUS:
      grease_pencil_fill_extension_lines_from_circles(
          C, extension_data, origin_drawings, origin_points);
      break;
  }

  return extension_data;
}

static void grease_pencil_fill_status_indicators(bContext &C,
                                                 const GreasePencilFillOpData &op_data)
{
  const bool is_extend = (op_data.extension_mode == GP_FILL_EMODE_EXTEND);

  WorkspaceStatus status(&C);
  status.item(IFACE_("Cancel"), ICON_EVENT_ESC);
  status.item(IFACE_("Fill"), ICON_MOUSE_LMB);
  status.item(
      fmt::format("{} ({})", IFACE_("Mode"), (is_extend ? IFACE_("Extend") : IFACE_("Radius"))),
      ICON_EVENT_S);
  status.item(fmt::format("{} ({:.3f})",
                          is_extend ? IFACE_("Length") : IFACE_("Radius"),
                          op_data.extension_length),
              ICON_MOUSE_MMB_SCROLL);
  if (is_extend) {
    status.item_bool(IFACE_("Collision"), op_data.extension_cut, ICON_EVENT_D);
  }
}

/* Draw callback for fill tool overlay. */
static void grease_pencil_fill_overlay_cb(const bContext *C, ARegion * /*region*/, void *arg)
{
  const ARegion &region = *CTX_wm_region(C);
  const RegionView3D &rv3d = *CTX_wm_region_view3d(C);
  const Scene &scene = *CTX_data_scene(C);
  const Object &object = *CTX_data_active_object(C);
  const GreasePencil &grease_pencil = *static_cast<const GreasePencil *>(object.data);
  auto &op_data = *static_cast<GreasePencilFillOpData *>(arg);

  const float4x4 world_to_view = float4x4(rv3d.viewmat);
  /* Note; the initial view matrix is already set, clear to draw in view space. */
  ed::greasepencil::image_render::clear_view_matrix();

  const ColorGeometry4f stroke_curves_color = ColorGeometry4f(1, 0, 0, 1);
  const ColorGeometry4f extension_lines_color = ColorGeometry4f(0, 1, 1, 1);
  const ColorGeometry4f extension_circles_color = ColorGeometry4f(1, 0.5, 0, 1);

  const Vector<ed::greasepencil::DrawingInfo> drawings =
      ed::greasepencil::retrieve_visible_drawings(scene, grease_pencil, false);

  if (op_data.show_boundaries) {
    const Vector<ed::greasepencil::DrawingInfo> drawings =
        ed::greasepencil::retrieve_visible_drawings(scene, grease_pencil, false);

    for (const ed::greasepencil::DrawingInfo &info : drawings) {
      const IndexMask curve_mask = info.drawing.strokes().curves_range();
      const VArray<ColorGeometry4f> colors = VArray<ColorGeometry4f>::from_single(
          stroke_curves_color, info.drawing.strokes().points_num());
      const float4x4 layer_to_world = grease_pencil.layer(info.layer_index).to_world_space(object);
      const bool use_xray = false;
      const float radius_scale = 1.0f;

      ed::greasepencil::image_render::draw_grease_pencil_strokes(rv3d,
                                                                 int2(region.winx, region.winy),
                                                                 object,
                                                                 info.drawing,
                                                                 layer_to_world,
                                                                 curve_mask,
                                                                 colors,
                                                                 use_xray,
                                                                 radius_scale);
    }
  }

  if (op_data.show_extension) {
    const ed::greasepencil::ExtensionData extensions = grease_pencil_fill_get_extension_data(
        *C, op_data);

    const float line_width = 2.0f;

    const IndexRange lines_range = extensions.lines.starts.index_range();
    if (!lines_range.is_empty()) {
      const VArray<ColorGeometry4f> line_colors = VArray<ColorGeometry4f>::from_single(
          extension_lines_color, lines_range.size());

      ed::greasepencil::image_render::draw_lines(world_to_view,
                                                 lines_range,
                                                 extensions.lines.starts,
                                                 extensions.lines.ends,
                                                 line_colors,
                                                 line_width);
    }
    const IndexRange circles_range = extensions.circles.centers.index_range();
    if (!circles_range.is_empty()) {
      const VArray<ColorGeometry4f> circle_colors = VArray<ColorGeometry4f>::from_single(
          extension_circles_color, circles_range.size());

      ed::greasepencil::image_render::draw_circles(
          world_to_view,
          circles_range,
          extensions.circles.centers,
          VArray<float>::from_span(extensions.circles.radii),
          circle_colors,
          float2(region.winx, region.winy),
          line_width,
          false);
    }
  }
}

static void grease_pencil_fill_update_overlay(const ARegion &region,
                                              GreasePencilFillOpData &op_data)
{
  const bool needs_overlay = op_data.show_boundaries || op_data.show_extension;

  if (needs_overlay) {
    if (op_data.overlay_cb_handle == nullptr) {
      op_data.overlay_cb_handle = ED_region_draw_cb_activate(
          region.runtime->type, grease_pencil_fill_overlay_cb, &op_data, REGION_DRAW_POST_VIEW);
    }
  }
  else {
    if (op_data.overlay_cb_handle) {
      ED_region_draw_cb_exit(region.runtime->type, op_data.overlay_cb_handle);
      op_data.overlay_cb_handle = nullptr;
    }
  }
}

static void grease_pencil_update_extend(bContext &C, GreasePencilFillOpData &op_data)
{
  grease_pencil_fill_update_overlay(*CTX_wm_region(&C), op_data);
  grease_pencil_fill_status_indicators(C, op_data);
  WM_event_add_notifier(&C, NC_GPENCIL | NA_EDITED, nullptr);
}

/* Layer mode defines layers where only marked boundary strokes are used. */
static VArray<bool> get_fill_boundary_layers(const GreasePencil &grease_pencil,
                                             eGP_FillLayerModes fill_layer_mode)
{
  BLI_assert(grease_pencil.has_active_layer());
  const IndexRange all_layers = grease_pencil.layers().index_range();
  const int active_layer_index = *grease_pencil.get_layer_index(*grease_pencil.get_active_layer());

  switch (fill_layer_mode) {
    case GP_FILL_GPLMODE_ACTIVE:
      return VArray<bool>::from_std_func(all_layers.size(), [active_layer_index](const int index) {
        return index != active_layer_index;
      });
    case GP_FILL_GPLMODE_ABOVE:
      return VArray<bool>::from_std_func(all_layers.size(), [active_layer_index](const int index) {
        return index != active_layer_index + 1;
      });
    case GP_FILL_GPLMODE_BELOW:
      return VArray<bool>::from_std_func(all_layers.size(), [active_layer_index](const int index) {
        return index != active_layer_index - 1;
      });
    case GP_FILL_GPLMODE_ALL_ABOVE:
      return VArray<bool>::from_std_func(all_layers.size(), [active_layer_index](const int index) {
        return index <= active_layer_index;
      });
    case GP_FILL_GPLMODE_ALL_BELOW:
      return VArray<bool>::from_std_func(all_layers.size(), [active_layer_index](const int index) {
        return index >= active_layer_index;
      });
    case GP_FILL_GPLMODE_VISIBLE:
      return VArray<bool>::from_std_func(all_layers.size(), [grease_pencil](const int index) {
        return !grease_pencil.layers()[index]->is_visible();
      });
  }
  return {};
}

/* Array of visible drawings to use as borders for generating a stroke in the editable drawing on
 * the active layer. This is provided for every frame in the multi-frame edit range. */
struct FillToolTargetInfo {
  ed::greasepencil::MutableDrawingInfo target;
  Vector<ed::greasepencil::DrawingInfo> sources;
};

static Vector<FillToolTargetInfo> ensure_editable_drawings(const Scene &scene,
                                                           GreasePencil &grease_pencil,
                                                           bke::greasepencil::Layer &target_layer)
{
  using namespace bke::greasepencil;
  using ed::greasepencil::DrawingInfo;
  using ed::greasepencil::MutableDrawingInfo;

  const ToolSettings *toolsettings = scene.toolsettings;
  const bool use_multi_frame_editing = (toolsettings->gpencil_flags &
                                        GP_USE_MULTI_FRAME_EDITING) != 0;
  const bool use_autokey = blender::animrig::is_autokey_on(&scene);
  const bool use_duplicate_frame = (scene.toolsettings->gpencil_flags & GP_TOOL_FLAG_RETAIN_LAST);
  const int target_layer_index = *grease_pencil.get_layer_index(target_layer);

  VectorSet<int> target_frames;
  /* Add drawing on the current frame. */
  target_frames.add(scene.r.cfra);
  /* Multi-frame edit: Add drawing on frames that are selected in any layer. */
  if (use_multi_frame_editing) {
    for (const Layer *layer : grease_pencil.layers()) {
      for (const auto [frame_number, frame] : layer->frames().items()) {
        if (frame.is_selected()) {
          target_frames.add(frame_number);
        }
      }
    }
  }

  /* Create new drawings when autokey is enabled. */
  if (use_autokey) {
    for (const int frame_number : target_frames) {
      if (!target_layer.frames().contains(frame_number)) {
        if (use_duplicate_frame) {
          grease_pencil.insert_duplicate_frame(
              target_layer, *target_layer.start_frame_at(frame_number), frame_number, false);
        }
        else {
          grease_pencil.insert_frame(target_layer, frame_number);
        }
      }
    }
  }

  Vector<FillToolTargetInfo> drawings;
  for (const int frame_number : target_frames) {
    if (Drawing *target_drawing = grease_pencil.get_editable_drawing_at(target_layer,
                                                                        frame_number))
    {
      MutableDrawingInfo target = {*target_drawing, target_layer_index, frame_number, 1.0f};

      Vector<DrawingInfo> sources;
      for (const Layer *source_layer : grease_pencil.layers()) {
        if (const Drawing *source_drawing = grease_pencil.get_drawing_at(*source_layer,
                                                                         frame_number))
        {
          const int source_layer_index = *grease_pencil.get_layer_index(*source_layer);
          sources.append({*source_drawing, source_layer_index, frame_number, 0});
        }
      }

      drawings.append({std::move(target), std::move(sources)});
    }
  }

  return drawings;
}

static void smooth_fill_strokes(bke::CurvesGeometry &curves, const IndexMask &stroke_mask)
{
  const int iterations = 20;
  if (curves.is_empty()) {
    return;
  }
  if (stroke_mask.is_empty()) {
    return;
  }

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const VArray<bool> cyclic = curves.cyclic();
  const VArray<bool> point_selection = VArray<bool>::from_single(true, curves.points_num());

  bke::GSpanAttributeWriter positions = attributes.lookup_for_write_span("position");
  geometry::smooth_curve_attribute(stroke_mask,
                                   points_by_curve,
                                   point_selection,
                                   cyclic,
                                   iterations,
                                   1.0f,
                                   false,
                                   true,
                                   positions.span);
  positions.finish();
  curves.tag_positions_changed();
}

static bke::CurvesGeometry simplify_fixed(bke::CurvesGeometry &curves, const int step)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const Array<int> point_to_curve_map = curves.point_to_curve_map();

  IndexMaskMemory memory;
  IndexMask points_to_keep = IndexMask::from_predicate(
      curves.points_range(), GrainSize(2048), memory, [&](const int64_t i) {
        const int curve_i = point_to_curve_map[i];
        const IndexRange points = points_by_curve[curve_i];
        if (points.size() <= 2) {
          return true;
        }
        const int local_i = i - points.start();
        return (local_i % int(math::pow(2.0f, float(step))) == 0) || points.last() == i;
      });

  return bke::curves_copy_point_selection(curves, points_to_keep, {});
}

static bool grease_pencil_apply_fill(bContext &C, wmOperator &op, const wmEvent &event)
{
  using bke::greasepencil::Layer;
  using ed::greasepencil::DrawingInfo;
  using ed::greasepencil::MutableDrawingInfo;

  constexpr const ed::greasepencil::FillToolFitMethod fit_method =
      ed::greasepencil::FillToolFitMethod::FitToView;
  /* Debug setting: keep image data blocks for inspection. */
  constexpr const bool keep_images = false;

  ARegion &region = *CTX_wm_region(&C);
  /* Perform bounds check. */
  const bool in_bounds = BLI_rcti_isect_pt_v(&region.winrct, event.xy);
  if (!in_bounds) {
    return false;
  }

  wmWindow &win = *CTX_wm_window(&C);
  const ViewContext view_context = ED_view3d_viewcontext_init(&C, CTX_data_depsgraph_pointer(&C));
  const Scene &scene = *CTX_data_scene(&C);
  Object &object = *CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);
  auto &op_data = *static_cast<GreasePencilFillOpData *>(op.customdata);
  const ToolSettings &ts = *CTX_data_tool_settings(&C);
  Brush &brush = *BKE_paint_brush(&ts.gp_paint->paint);
  const float2 mouse_position = float2(event.mval);
  const int simplify_levels = brush.gpencil_settings->fill_simplylvl;
  const std::optional<float> alpha_threshold =
      (brush.gpencil_settings->flag & GP_BRUSH_FILL_HIDE) ?
          std::nullopt :
          std::make_optional(brush.gpencil_settings->fill_threshold);
  const bool on_back = (ts.gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK);
  const bool auto_remove_fill_guides = (brush.gpencil_settings->flag &
                                        GP_BRUSH_FILL_AUTO_REMOVE_FILL_GUIDES) != 0;

  if (!grease_pencil.has_active_layer()) {
    return false;
  }
  /* Add drawings in the active layer if autokey is enabled. */
  Vector<FillToolTargetInfo> target_drawings = ensure_editable_drawings(
      scene, grease_pencil, *grease_pencil.get_active_layer());

  const VArray<bool> boundary_layers = get_fill_boundary_layers(
      grease_pencil, eGP_FillLayerModes(brush.gpencil_settings->fill_layer_mode));

  bool did_create_fill = false;
  for (const FillToolTargetInfo &info : target_drawings) {
    const Layer &layer = *grease_pencil.layers()[info.target.layer_index];

    const ed::greasepencil::ExtensionData extensions = grease_pencil_fill_get_extension_data(
        C, op_data);

    bke::CurvesGeometry fill_curves = fill_strokes(view_context,
                                                   brush,
                                                   scene,
                                                   layer,
                                                   boundary_layers,
                                                   info.sources,
                                                   op_data.invert,
                                                   alpha_threshold,
                                                   mouse_position,
                                                   extensions,
                                                   fit_method,
                                                   op_data.material_index,
                                                   keep_images);
    if (fill_curves.is_empty()) {
      continue;
    }

    smooth_fill_strokes(fill_curves, fill_curves.curves_range());

    if (simplify_levels > 0) {
      fill_curves = simplify_fixed(fill_curves, brush.gpencil_settings->fill_simplylvl);
    }

    bke::CurvesGeometry &dst_curves = info.target.drawing.strokes_for_write();
    if (auto_remove_fill_guides) {
      /* Remove strokes that were created using the fill tool as boundary strokes. */
      ed::greasepencil::remove_fill_guides(dst_curves);
    }

    /* If the `fill_strokes` function creates the "fill_opacity" attribute, make sure that we
     * initialize this to full opacity on the target geometry. */
    if (fill_curves.attributes().contains("fill_opacity") &&
        !dst_curves.attributes().contains("fill_opacity"))
    {
      bke::SpanAttributeWriter<float> fill_opacities =
          dst_curves.attributes_for_write().lookup_or_add_for_write_span<float>(
              "fill_opacity",
              bke::AttrDomain::Curve,
              bke::AttributeInitVArray(VArray<float>::from_single(1.0f, dst_curves.curves_num())));
      fill_opacities.finish();
    }

    Curves *dst_curves_id = curves_new_nomain(dst_curves);
    Curves *fill_curves_id = curves_new_nomain(fill_curves);
    const Array<bke::GeometrySet> geometry_sets = {
        bke::GeometrySet::from_curves(on_back ? fill_curves_id : dst_curves_id),
        bke::GeometrySet::from_curves(on_back ? dst_curves_id : fill_curves_id)};
    const int num_new_curves = fill_curves.curves_num();
    const IndexRange new_curves_range = (on_back ?
                                             IndexRange(num_new_curves) :
                                             dst_curves.curves_range().after(num_new_curves));

    bke::GeometrySet joined_geometry_set = geometry::join_geometries(geometry_sets, {});
    if (joined_geometry_set.has_curves()) {
      dst_curves = joined_geometry_set.get_curves_for_write()->geometry.wrap();
      info.target.drawing.tag_topology_changed();

      /* Compute texture matrix for the new curves. */
      const ed::greasepencil::DrawingPlacement placement(
          scene, region, *view_context.v3d, object, &layer);
      const float4x2 texture_space = ed::greasepencil::calculate_texture_space(
          &scene, &region, mouse_position, placement);
      Array<float4x2> texture_matrices(num_new_curves, texture_space);
      info.target.drawing.set_texture_matrices(texture_matrices, new_curves_range);
    }

    did_create_fill = true;
  }

  if (!did_create_fill) {
    BKE_reportf(op.reports, RPT_ERROR, "Unable to fill unclosed areas");
  }

  WM_cursor_modal_restore(&win);

  /* Save extend value for next operation. */
  brush.gpencil_settings->fill_extend_fac = op_data.extension_length /
                                            bke::greasepencil::LEGACY_RADIUS_CONVERSION_FACTOR;
  BKE_brush_tag_unsaved_changes(&brush);

  return true;
}

static bool grease_pencil_fill_init(bContext &C, wmOperator &op)
{
  using blender::bke::greasepencil::Layer;

  Main &bmain = *CTX_data_main(&C);
  Scene &scene = *CTX_data_scene(&C);
  Object &ob = *CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob.data);
  Paint &paint = scene.toolsettings->gp_paint->paint;
  Brush &brush = *BKE_paint_brush(&paint);

  Layer *layer = grease_pencil.get_active_layer();
  /* Cannot paint in locked layer. */
  if (layer && layer->is_locked()) {
    return false;
  }
  if (layer == nullptr) {
    layer = &grease_pencil.add_layer("GP_Layer");
  }

  if (brush.gpencil_settings == nullptr) {
    BKE_brush_init_gpencil_settings(&brush);
  }
  BKE_curvemapping_init(brush.gpencil_settings->curve_sensitivity);
  BKE_curvemapping_init(brush.gpencil_settings->curve_strength);
  BKE_curvemapping_init(brush.gpencil_settings->curve_jitter);
  BKE_curvemapping_init(brush.gpencil_settings->curve_rand_pressure);
  BKE_curvemapping_init(brush.gpencil_settings->curve_rand_strength);
  BKE_curvemapping_init(brush.gpencil_settings->curve_rand_uv);
  BKE_curvemapping_init(brush.curve_rand_hue);
  BKE_curvemapping_init(brush.curve_rand_saturation);
  BKE_curvemapping_init(brush.curve_rand_value);

  Material *material = BKE_grease_pencil_object_material_ensure_from_brush(&bmain, &ob, &brush);
  const int material_index = BKE_object_material_index_get(&ob, material);

  const bool invert = RNA_boolean_get(op.ptr, "invert");
  const bool precision = RNA_boolean_get(op.ptr, "precision");

  op.customdata = MEM_new<GreasePencilFillOpData>(
      __func__,
      GreasePencilFillOpData::from_context(C, *layer, material_index, invert, precision));
  return true;
}

static void grease_pencil_fill_exit(bContext &C, wmOperator &op)
{
  const ARegion &region = *CTX_wm_region(&C);
  Object &ob = *CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob.data);

  WM_cursor_modal_restore(CTX_wm_window(&C));

  if (op.customdata) {
    auto &op_data = *static_cast<GreasePencilFillOpData *>(op.customdata);

    if (op_data.overlay_cb_handle) {
      ED_region_draw_cb_exit(region.runtime->type, op_data.overlay_cb_handle);
      op_data.overlay_cb_handle = nullptr;
    }

    MEM_delete(static_cast<GreasePencilFillOpData *>(op.customdata));
    op.customdata = nullptr;
  }

  /* Clear status message area. */
  ED_workspace_status_text(&C, nullptr);

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_main_add_notifier(NC_GEOM | ND_DATA, nullptr);
  WM_event_add_notifier(&C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
}

static wmOperatorStatus grease_pencil_fill_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent * /*event*/)
{
  const ARegion &region = *CTX_wm_region(C);
  ToolSettings &ts = *CTX_data_tool_settings(C);
  Brush &brush = *BKE_paint_brush(&ts.gp_paint->paint);
  Object &ob = *CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob.data);

  /* Fill tool needs a material (cannot use default material). */
  if ((brush.gpencil_settings->flag & GP_BRUSH_MATERIAL_PINNED) &&
      brush.gpencil_settings->material == nullptr)
  {
    BKE_report(op->reports, RPT_ERROR, "Fill tool needs active material");
    return OPERATOR_CANCELLED;
  }
  if (BKE_object_material_get(&ob, ob.actcol) == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Fill tool needs active material");
    return OPERATOR_CANCELLED;
  }
  if (!grease_pencil_fill_init(*C, *op)) {
    grease_pencil_fill_exit(*C, *op);
    return OPERATOR_CANCELLED;
  }
  auto &op_data = *static_cast<GreasePencilFillOpData *>(op->customdata);

  WM_cursor_modal_set(CTX_wm_window(C), WM_CURSOR_PAINT_BRUSH);
  grease_pencil_fill_status_indicators(*C, op_data);
  grease_pencil_fill_update_overlay(region, op_data);

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, nullptr);

  /* Add a modal handler for this operator. */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

enum class FillToolModalKey : int8_t {
  Cancel = 1,
  Confirm,
  ExtensionModeToggle,
  ExtensionLengthen,
  ExtensionShorten,
  ExtensionDrag,
  ExtensionCollide,
  Invert,
  Precision,
};

static wmOperatorStatus grease_pencil_fill_event_modal_map(bContext *C,
                                                           wmOperator *op,
                                                           const wmEvent *event)
{
  auto &op_data = *static_cast<GreasePencilFillOpData *>(op->customdata);
  /* Extension line length increment, for normal and precise mode respectively. */
  const float extension_delta = (op_data.precision ? 0.002f : 0.02f);

  switch (event->val) {
    case int(FillToolModalKey::Cancel):
      return OPERATOR_CANCELLED;

    case int(FillToolModalKey::Confirm): {
      /* Ignore in extension mode. */
      if (op_data.is_extension_drag_active) {
        break;
      }

      op_data.fill_mouse_pos = float2(event->mval);
      return (grease_pencil_apply_fill(*C, *op, *event) ? OPERATOR_FINISHED : OPERATOR_CANCELLED);
    }

    case int(FillToolModalKey::ExtensionModeToggle):
      if (op_data.show_extension) {
        /* Toggle mode. */
        if (op_data.extension_mode == GP_FILL_EMODE_EXTEND) {
          op_data.extension_mode = GP_FILL_EMODE_RADIUS;
        }
        else {
          op_data.extension_mode = GP_FILL_EMODE_EXTEND;
        }
        grease_pencil_update_extend(*C, op_data);
      }
      break;

    case int(FillToolModalKey::ExtensionLengthen):
      op_data.extension_length = std::max(op_data.extension_length - extension_delta, 0.0f);
      grease_pencil_update_extend(*C, op_data);
      break;

    case int(FillToolModalKey::ExtensionShorten):
      op_data.extension_length = std::min(op_data.extension_length + extension_delta, 10.0f);
      grease_pencil_update_extend(*C, op_data);
      break;

    case int(FillToolModalKey::ExtensionDrag): {
      if (event->val == KM_PRESS) {
        /* Consider initial offset as zero position. */
        op_data.is_extension_drag_active = true;
        /* TODO This is the GPv2 logic and it's weird. Should be reconsidered, for now use the
         * same method. */
        const float2 base_pos = float2(event->mval);
        constexpr const float gap = 300.0f;
        op_data.extension_mouse_pos = (math::distance(base_pos, op_data.fill_mouse_pos) >= gap ?
                                           base_pos :
                                           base_pos - float2(gap, 0));
        WM_cursor_set(CTX_wm_window(C), WM_CURSOR_EW_ARROW);
      }
      if (event->val == KM_RELEASE) {
        WM_cursor_modal_set(CTX_wm_window(C), WM_CURSOR_PAINT_BRUSH);
        op_data.is_extension_drag_active = false;
      }
      /* Update cursor line. */
      WM_main_add_notifier(NC_GEOM | ND_DATA, nullptr);
      WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
      break;
    }

    case int(FillToolModalKey::ExtensionCollide):
      if (op_data.show_extension) {
        op_data.extension_cut = !op_data.extension_cut;
        grease_pencil_update_extend(*C, op_data);
      }
      break;

    case int(FillToolModalKey::Invert):
      op_data.invert = !op_data.invert;
      break;

    case int(FillToolModalKey::Precision):
      op_data.precision = !op_data.precision;
      break;

    default:
      BLI_assert_unreachable();
      break;
  }
  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus grease_pencil_fill_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  const RegionView3D &rv3d = *CTX_wm_region_view3d(C);

  auto &op_data = *static_cast<GreasePencilFillOpData *>(op->customdata);

  wmOperatorStatus estate = OPERATOR_CANCELLED;
  if (!op_data.show_extension) {
    /* Apply fill immediately if "Visual Aids" (aka. extension lines) is disabled. */
    op_data.fill_mouse_pos = float2(event->mval);
    estate = (grease_pencil_apply_fill(*C, *op, *event) ? OPERATOR_FINISHED : OPERATOR_CANCELLED);
  }
  else {
    estate = OPERATOR_RUNNING_MODAL;
    switch (event->type) {
      case EVT_MODAL_MAP:
        estate = grease_pencil_fill_event_modal_map(C, op, event);
        break;
      case MOUSEMOVE: {
        if (!op_data.is_extension_drag_active) {
          break;
        }

        const Object &ob = *CTX_data_active_object(C);
        const float pixel_size = ED_view3d_pixel_size(&rv3d, ob.loc);
        const float2 mouse_pos = float2(event->mval);
        const float initial_dist = math::distance(op_data.extension_mouse_pos,
                                                  op_data.fill_mouse_pos);
        const float current_dist = math::distance(mouse_pos, op_data.fill_mouse_pos);

        float delta = (current_dist - initial_dist) * pixel_size * 0.5f;
        op_data.extension_length = std::clamp(op_data.extension_length + delta, 0.0f, 10.0f);

        /* Update cursor line and extend lines. */
        WM_main_add_notifier(NC_GEOM | ND_DATA, nullptr);
        WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

        grease_pencil_update_extend(*C, op_data);
        break;
      }
      default:
        break;
    }
  }

  /* Process last operations before exiting. */
  switch (estate) {
    case OPERATOR_FINISHED:
      grease_pencil_fill_exit(*C, *op);
      WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, nullptr);
      break;

    case OPERATOR_CANCELLED:
      grease_pencil_fill_exit(*C, *op);
      break;

    default:
      break;
  }

  return estate;
}

static void grease_pencil_fill_cancel(bContext *C, wmOperator *op)
{
  grease_pencil_fill_exit(*C, *op);
}

static void GREASE_PENCIL_OT_fill(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Grease Pencil Fill";
  ot->idname = "GREASE_PENCIL_OT_fill";
  ot->description = "Fill with color the shape formed by strokes";

  ot->poll = ed::greasepencil::grease_pencil_painting_poll;
  ot->invoke = grease_pencil_fill_invoke;
  ot->modal = grease_pencil_fill_modal;
  ot->cancel = grease_pencil_fill_cancel;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  prop = RNA_def_boolean(
      ot->srna, "invert", false, "Invert", "Find boundary of unfilled instead of filled regions");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna, "precision", false, "Precision", "Use precision movement for extension lines");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static bke::greasepencil::Drawing *get_current_drawing_or_duplicate_for_autokey(
    const Scene &scene, GreasePencil &grease_pencil, const int layer_index)
{
  using namespace bke::greasepencil;
  const int current_frame = scene.r.cfra;
  Layer &layer = grease_pencil.layer(layer_index);
  if (!layer.has_drawing_at(current_frame) && !blender::animrig::is_autokey_on(&scene)) {
    return nullptr;
  }

  const std::optional<int> previous_key_frame_start = layer.start_frame_at(current_frame);
  const bool has_previous_key = previous_key_frame_start.has_value();
  if (blender::animrig::is_autokey_on(&scene) && has_previous_key) {
    grease_pencil.insert_duplicate_frame(layer, *previous_key_frame_start, current_frame, false);
  }
  return grease_pencil.get_drawing_at(layer, current_frame);
}

static bool remove_points_and_split_from_drawings(
    const Scene &scene,
    GreasePencil &grease_pencil,
    const Span<ed::greasepencil::MutableDrawingInfo> drawings,
    const Span<IndexMask> points_to_remove_per_drawing)
{
  using namespace bke::greasepencil;
  using namespace ed::greasepencil;
  bool changed = false;
  for (const int drawing_i : drawings.index_range()) {
    const MutableDrawingInfo &info = drawings[drawing_i];
    const IndexMask points_to_remove = points_to_remove_per_drawing[drawing_i];
    if (points_to_remove.is_empty()) {
      continue;
    }

    if (Drawing *drawing = get_current_drawing_or_duplicate_for_autokey(
            scene, grease_pencil, info.layer_index))
    {
      drawing->strokes_for_write() = geometry::remove_points_and_split(drawing->strokes(),
                                                                       points_to_remove);
      drawing->tag_topology_changed();
      changed = true;
    }
  }

  return changed;
}

static inline bool is_point_inside_bounds(const Bounds<int2> bounds, const int2 point)
{
  if (point.x < bounds.min.x) {
    return false;
  }
  if (point.x > bounds.max.x) {
    return false;
  }
  if (point.y < bounds.min.y) {
    return false;
  }
  if (point.y > bounds.max.y) {
    return false;
  }
  return true;
}

static inline bool is_point_inside_lasso(const Array<int2> lasso, const int2 point)
{
  return isect_point_poly_v2_int(
      point, reinterpret_cast<const int (*)[2]>(lasso.data()), uint(lasso.size()));
}

static wmOperatorStatus grease_pencil_erase_lasso_exec(bContext *C, wmOperator *op)
{
  using namespace bke::greasepencil;
  using namespace ed::greasepencil;
  const Scene *scene = CTX_data_scene(C);
  const Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const ARegion *region = CTX_wm_region(C);
  Object *object = CTX_data_active_object(C);
  const Object *ob_eval = DEG_get_evaluated(depsgraph, object);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const Array<int2> lasso = WM_gesture_lasso_path_to_array(C, op);
  if (lasso.is_empty()) {
    return OPERATOR_FINISHED;
  }

  const Bounds<int2> lasso_bounds_int = *bounds::min_max(lasso.as_span());
  const Bounds<float2> lasso_bounds(float2(lasso_bounds_int.min), float2(lasso_bounds_int.max));

  const Vector<MutableDrawingInfo> drawings = ed::greasepencil::retrieve_editable_drawings(
      *scene, grease_pencil);
  Array<IndexMaskMemory> memories(drawings.size());
  Array<IndexMask> points_to_remove_per_drawing(drawings.size());
  threading::parallel_for(drawings.index_range(), 1, [&](const IndexRange range) {
    for (const int drawing_i : range) {
      const MutableDrawingInfo &info = drawings[drawing_i];
      const bke::greasepencil::Layer &layer = grease_pencil.layer(info.layer_index);
      const bke::crazyspace::GeometryDeformation deformation =
          bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
              ob_eval, *object, info.drawing);
      const float4x4 layer_to_world = layer.to_world_space(*ob_eval);

      const bke::CurvesGeometry &curves = info.drawing.strokes();
      Array<float2> screen_space_positions(curves.points_num());
      threading::parallel_for(curves.points_range(), 4096, [&](const IndexRange points) {
        for (const int point : points) {
          const float3 pos = math::transform_point(layer_to_world, deformation.positions[point]);
          eV3DProjStatus result = ED_view3d_project_float_global(
              region, pos, screen_space_positions[point], V3D_PROJ_TEST_NOP);
          if (result != V3D_PROJ_RET_OK) {
            screen_space_positions[point] = float2(0);
          }
        }
      });

      const OffsetIndices<int> points_by_curve = curves.points_by_curve();
      Array<Bounds<float2>> screen_space_curve_bounds(curves.curves_num());
      threading::parallel_for(curves.curves_range(), 512, [&](const IndexRange range) {
        for (const int curve : range) {
          screen_space_curve_bounds[curve] = *bounds::min_max(
              screen_space_positions.as_span().slice(points_by_curve[curve]));
        }
      });

      IndexMaskMemory &memory = memories[drawing_i];
      const IndexMask curve_selection = IndexMask::from_predicate(
          curves.curves_range(), GrainSize(512), memory, [&](const int64_t index) {
            /* For a single point curve, its screen_space_curve_bounds Bounds will be empty (by
             * definition), so intersecting will fail. Check if the single point is in the bounds
             * instead. */
            const IndexRange points = points_by_curve[index];
            if (points.size() == 1) {
              return is_point_inside_bounds(lasso_bounds_int,
                                            int2(screen_space_positions[points.first()]));
            }

            return bounds::intersect(lasso_bounds, screen_space_curve_bounds[index]).has_value();
          });

      if (curve_selection.is_empty()) {
        return;
      }

      Array<bool> points_to_remove(curves.points_num(), false);
      curve_selection.foreach_index(GrainSize(512), [&](const int64_t curve_i) {
        for (const int point : points_by_curve[curve_i]) {
          points_to_remove[point] = is_point_inside_lasso(lasso,
                                                          int2(screen_space_positions[point]));
        }
      });
      points_to_remove_per_drawing[drawing_i] = IndexMask::from_bools(points_to_remove, memory);
    }
  });

  const bool changed = remove_points_and_split_from_drawings(
      *scene, grease_pencil, drawings.as_span(), points_to_remove_per_drawing);
  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_erase_lasso(wmOperatorType *ot)
{
  ot->name = "Grease Pencil Erase Lasso";
  ot->idname = "GREASE_PENCIL_OT_erase_lasso";
  ot->description = "Erase points in the lasso region";

  ot->poll = ed::greasepencil::grease_pencil_painting_poll;
  ot->invoke = WM_gesture_lasso_invoke;
  ot->exec = grease_pencil_erase_lasso_exec;
  ot->modal = WM_gesture_lasso_modal;
  ot->cancel = WM_gesture_lasso_cancel;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  WM_operator_properties_gesture_lasso(ot);
}

static wmOperatorStatus grease_pencil_erase_box_exec(bContext *C, wmOperator *op)
{
  using namespace bke::greasepencil;
  using namespace ed::greasepencil;
  const Scene *scene = CTX_data_scene(C);
  const Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const ARegion *region = CTX_wm_region(C);
  Object *object = CTX_data_active_object(C);
  const Object *ob_eval = DEG_get_evaluated(depsgraph, object);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const Bounds<int2> box_bounds = WM_operator_properties_border_to_bounds(op);
  if (box_bounds.is_empty()) {
    return OPERATOR_FINISHED;
  }

  const Vector<MutableDrawingInfo> drawings = ed::greasepencil::retrieve_editable_drawings(
      *scene, grease_pencil);
  Array<IndexMaskMemory> memories(drawings.size());
  Array<IndexMask> points_to_remove_per_drawing(drawings.size());
  threading::parallel_for(drawings.index_range(), 1, [&](const IndexRange range) {
    for (const int drawing_i : range) {
      const MutableDrawingInfo &info = drawings[drawing_i];
      const bke::greasepencil::Layer &layer = grease_pencil.layer(info.layer_index);
      const bke::crazyspace::GeometryDeformation deformation =
          bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
              ob_eval, *object, info.drawing);
      const float4x4 layer_to_world = layer.to_world_space(*ob_eval);

      const bke::CurvesGeometry &curves = info.drawing.strokes();
      Array<float2> screen_space_positions(curves.points_num());
      threading::parallel_for(curves.points_range(), 4096, [&](const IndexRange points) {
        for (const int point : points) {
          const float3 pos = math::transform_point(layer_to_world, deformation.positions[point]);
          eV3DProjStatus result = ED_view3d_project_float_global(
              region, pos, screen_space_positions[point], V3D_PROJ_TEST_NOP);
          if (result != V3D_PROJ_RET_OK) {
            screen_space_positions[point] = float2(0);
          }
        }
      });

      IndexMaskMemory &memory = memories[drawing_i];
      points_to_remove_per_drawing[drawing_i] = IndexMask::from_predicate(
          curves.points_range(), GrainSize(4096), memory, [&](const int64_t index) {
            return is_point_inside_bounds(box_bounds, int2(screen_space_positions[index]));
          });
    }
  });

  const bool changed = remove_points_and_split_from_drawings(
      *scene, grease_pencil, drawings.as_span(), points_to_remove_per_drawing);
  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_erase_box(wmOperatorType *ot)
{
  ot->name = "Grease Pencil Box Erase";
  ot->idname = "GREASE_PENCIL_OT_erase_box";
  ot->description = "Erase points in the box region";

  ot->poll = ed::greasepencil::grease_pencil_painting_poll;
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = grease_pencil_erase_box_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  WM_operator_properties_border(ot);
}

/** \} */

}  // namespace blender::ed::sculpt_paint

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void ED_operatortypes_grease_pencil_draw()
{
  using namespace blender::ed::sculpt_paint;
  WM_operatortype_append(GREASE_PENCIL_OT_brush_stroke);
  WM_operatortype_append(GREASE_PENCIL_OT_sculpt_paint);
  WM_operatortype_append(GREASE_PENCIL_OT_weight_brush_stroke);
  WM_operatortype_append(GREASE_PENCIL_OT_vertex_brush_stroke);
  WM_operatortype_append(GREASE_PENCIL_OT_fill);
  WM_operatortype_append(GREASE_PENCIL_OT_erase_lasso);
  WM_operatortype_append(GREASE_PENCIL_OT_erase_box);
}

void ED_filltool_modal_keymap(wmKeyConfig *keyconf)
{
  using namespace blender::ed::greasepencil;
  using blender::ed::sculpt_paint::FillToolModalKey;

  static const EnumPropertyItem modal_items[] = {
      {int(FillToolModalKey::Cancel), "CANCEL", 0, "Cancel", ""},
      {int(FillToolModalKey::Confirm), "CONFIRM", 0, "Confirm", ""},
      {int(FillToolModalKey::ExtensionModeToggle),
       "EXTENSION_MODE_TOGGLE",
       0,
       "Toggle Extension Mode",
       ""},
      {int(FillToolModalKey::ExtensionLengthen),
       "EXTENSION_LENGTHEN",
       0,
       "Lengthen Extensions",
       ""},
      {int(FillToolModalKey::ExtensionShorten), "EXTENSION_SHORTEN", 0, "Shorten Extensions", ""},
      {int(FillToolModalKey::ExtensionDrag), "EXTENSION_DRAG", 0, "Drag Extensions", ""},
      {int(FillToolModalKey::ExtensionCollide), "EXTENSION_COLLIDE", 0, "Collide Extensions", ""},
      {int(FillToolModalKey::Invert), "INVERT", 0, "Invert", ""},
      {int(FillToolModalKey::Precision), "PRECISION", 0, "Precision", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Fill Tool Modal Map");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Fill Tool Modal Map", modal_items);

  WM_modalkeymap_assign(keymap, "GREASE_PENCIL_OT_fill");
}

/** \} */
