/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "ANIM_keyframing.hh"

#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_deform.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_material.h"
#include "BKE_object_deform.h"
#include "BKE_paint.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "BLI_assert.h"
#include "BLI_index_mask.hh"
#include "BLI_math_vector.hh"
#include "BLI_rect.h"
#include "BLI_string.h"

#include "DNA_brush_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "DEG_depsgraph_query.hh"

#include "GEO_join_geometries.hh"
#include "GEO_simplify_curves.hh"
#include "GEO_smooth_curves.hh"

#include "ED_grease_pencil.hh"
#include "ED_image.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_toolsystem.hh"

#include "WM_types.hh"
#include "curves_sculpt_intern.hh"
#include "grease_pencil_intern.hh"
#include "paint_intern.hh"
#include "wm_event_types.hh"

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

static GreasePencilStrokeOperation *get_stroke_operation(bContext &C, wmOperator *op)
{
  const Paint *paint = BKE_paint_get_active_from_context(&C);
  const Brush &brush = *BKE_paint_brush_for_read(paint);
  const PaintMode mode = BKE_paintmode_get_active_from_context(&C);
  const BrushStrokeMode stroke_mode = BrushStrokeMode(RNA_enum_get(op->ptr, "mode"));

  if (mode == PaintMode::GPencil) {
    /* FIXME: Somehow store the unique_ptr in the PaintStroke. */
    switch (eBrushGPaintTool(brush.gpencil_tool)) {
      case GPAINT_TOOL_DRAW:
        return greasepencil::new_paint_operation().release();
      case GPAINT_TOOL_ERASE:
        return greasepencil::new_erase_operation().release();
      case GPAINT_TOOL_FILL:
        /* Fill tool keymap uses the paint operator as alternative mode. */
        return greasepencil::new_paint_operation().release();
      case GPAINT_TOOL_TINT:
        return greasepencil::new_tint_operation().release();
    }
  }
  else if (mode == PaintMode::SculptGreasePencil) {
    switch (eBrushGPSculptTool(brush.gpencil_sculpt_tool)) {
      case GPSCULPT_TOOL_SMOOTH:
        return greasepencil::new_smooth_operation(stroke_mode).release();
      case GPSCULPT_TOOL_THICKNESS:
        return greasepencil::new_thickness_operation(stroke_mode).release();
      case GPSCULPT_TOOL_STRENGTH:
        return greasepencil::new_strength_operation(stroke_mode).release();
      case GPSCULPT_TOOL_GRAB:
        return greasepencil::new_grab_operation(stroke_mode).release();
      case GPSCULPT_TOOL_PUSH:
        return greasepencil::new_push_operation(stroke_mode).release();
      case GPSCULPT_TOOL_TWIST:
        return greasepencil::new_twist_operation(stroke_mode).release();
      case GPSCULPT_TOOL_PINCH:
        return greasepencil::new_pinch_operation(stroke_mode).release();
      case GPSCULPT_TOOL_RANDOMIZE:
        return greasepencil::new_randomize_operation(stroke_mode).release();
      case GPSCULPT_TOOL_CLONE:
        return greasepencil::new_clone_operation(stroke_mode).release();
    }
  }
  else if (mode == PaintMode::WeightGPencil) {
    switch (eBrushGPWeightTool(brush.gpencil_weight_tool)) {
      case GPWEIGHT_TOOL_DRAW:
        return greasepencil::new_weight_paint_draw_operation(stroke_mode).release();
        break;
      case GPWEIGHT_TOOL_BLUR:
        return greasepencil::new_weight_paint_blur_operation().release();
        break;
      case GPWEIGHT_TOOL_AVERAGE:
        return greasepencil::new_weight_paint_average_operation().release();
        break;
      case GPWEIGHT_TOOL_SMEAR:
        return greasepencil::new_weight_paint_smear_operation().release();
        break;
    }
  }
  return nullptr;
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
    GreasePencilStrokeOperation *new_operation = get_stroke_operation(*C, op);
    BLI_assert(new_operation != nullptr);
    paint_stroke_set_mode_data(stroke, new_operation);
    new_operation->on_stroke_begin(*C, sample);
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
    operation->~GreasePencilStrokeOperation();
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

static bool stroke_test_start(bContext *C, wmOperator *op, const float mouse[2])
{
  UNUSED_VARS(C, op, mouse);
  return true;
}

static int grease_pencil_brush_stroke_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int return_value = ed::greasepencil::grease_pencil_draw_operator_invoke(C, op);
  if (return_value != OPERATOR_RUNNING_MODAL) {
    return return_value;
  }

  op->customdata = paint_stroke_new(C,
                                    op,
                                    stroke_get_location,
                                    stroke_test_start,
                                    stroke_update_step,
                                    stroke_redraw,
                                    stroke_done,
                                    event->type);

  return_value = op->type->modal(C, op, event);
  if (return_value == OPERATOR_FINISHED) {
    return OPERATOR_FINISHED;
  }

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int grease_pencil_brush_stroke_modal(bContext *C, wmOperator *op, const wmEvent *event)
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

static int grease_pencil_sculpt_paint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const Scene *scene = CTX_data_scene(C);
  const Object *object = CTX_data_active_object(C);
  if (!object || object->type != OB_GREASE_PENCIL) {
    return OPERATOR_CANCELLED;
  }

  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  if (!grease_pencil.has_active_layer()) {
    BKE_report(op->reports, RPT_ERROR, "No active Grease Pencil layer");
    return OPERATOR_CANCELLED;
  }

  const Paint *paint = BKE_paint_get_active_from_context(C);
  const Brush *brush = BKE_paint_brush_for_read(paint);
  if (brush == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bke::greasepencil::Layer &active_layer = *grease_pencil.get_active_layer();

  if (!active_layer.is_editable()) {
    BKE_report(op->reports, RPT_ERROR, "Active layer is locked or hidden");
    return OPERATOR_CANCELLED;
  }

  /* Ensure a drawing at the current keyframe. */
  bool inserted_keyframe = false;
  if (!ed::greasepencil::ensure_active_keyframe(*scene, grease_pencil, inserted_keyframe)) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil frame to draw on");
    return OPERATOR_CANCELLED;
  }
  if (inserted_keyframe) {
    WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, nullptr);
  }

  op->customdata = paint_stroke_new(C,
                                    op,
                                    stroke_get_location,
                                    stroke_test_start,
                                    stroke_update_step,
                                    stroke_redraw,
                                    stroke_done,
                                    event->type);

  const int return_value = op->type->modal(C, op, event);
  if (return_value == OPERATOR_FINISHED) {
    return OPERATOR_FINISHED;
  }

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int grease_pencil_sculpt_paint_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  return paint_stroke_modal(C, op, event, reinterpret_cast<PaintStroke **>(&op->customdata));
}

static void grease_pencil_sculpt_paint_cancel(bContext *C, wmOperator *op)
{
  paint_stroke_cancel(C, op, static_cast<PaintStroke *>(op->customdata));
}

static void GREASE_PENCIL_OT_sculpt_paint(wmOperatorType *ot)
{
  ot->name = "Grease Pencil Draw";
  ot->idname = "GREASE_PENCIL_OT_sculpt_paint";
  ot->description = "Draw a new stroke in the active Grease Pencil object";

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

static int grease_pencil_weight_brush_stroke_invoke(bContext *C,
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

  const int return_value = op->type->modal(C, op, event);
  if (return_value == OPERATOR_FINISHED) {
    return OPERATOR_FINISHED;
  }

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int grease_pencil_weight_brush_stroke_modal(bContext *C,
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
/** \name Bucket Fill Operator
 * \{ */

struct GreasePencilFillOpData {
  blender::bke::greasepencil::Layer &layer;

  /* Brush properties, some of these are modified by modal keys. */
  int flag;
  eGP_FillExtendModes fill_extend_mode;
  float fill_extend_fac;

  int material_index;
  /* Toggle inverse filling. */
  bool invert = false;
  /* Toggle precision mode. */
  bool precision = false;

  /* Mouse position where fill was initialized */
  float2 fill_mouse_pos;
  /* Extension lines mode is enabled (middle mouse button). */
  bool is_extension_mode = false;
  /* Mouse position where the extension mode was enabled. */
  float2 extension_mouse_pos;

  ~GreasePencilFillOpData()
  {
    // TODO Remove drawing handler.
    // if (this->draw_handle_3d) {
    //   ED_region_draw_cb_exit(this->region_type, this->draw_handle_3d);
    // }
  }

  static GreasePencilFillOpData from_context(bContext &C,
                                             blender::bke::greasepencil::Layer &layer,
                                             const int material_index,
                                             const bool invert,
                                             const bool precision)
  {
    using blender::bke::greasepencil::Layer;

    const ToolSettings &ts = *CTX_data_tool_settings(&C);
    const Brush &brush = *BKE_paint_brush(&ts.gp_paint->paint);

    /* Enable custom drawing handlers to show help lines */
    const bool do_extend = (brush.gpencil_settings->flag & GP_BRUSH_FILL_SHOW_EXTENDLINES);
    const bool help_lines = do_extend ||
                            (brush.gpencil_settings->flag & GP_BRUSH_FILL_SHOW_HELPLINES);
    if (help_lines) {
      // TODO register 3D view overlay to render help lines.
      // this->region_type = region.type;
      // this->draw_handle_3d = ED_region_draw_cb_activate(
      //     region.type, grease_pencil_fill_draw_3d, tgpf, REGION_DRAW_POST_VIEW);
    }

    return {layer,
            brush.gpencil_settings->flag,
            eGP_FillExtendModes(brush.gpencil_settings->fill_extend_mode),
            brush.gpencil_settings->fill_extend_fac,
            material_index,
            invert,
            precision};
  }
};

static void grease_pencil_fill_status_indicators(bContext &C,
                                                 const GreasePencilFillOpData &op_data)
{
  const bool is_extend = (op_data.fill_extend_mode == GP_FILL_EMODE_EXTEND);
  const bool use_stroke_collide = (op_data.flag & GP_BRUSH_FILL_STROKE_COLLIDE) != 0;
  const float fill_extend_fac = op_data.fill_extend_fac;

  char status_str[UI_MAX_DRAW_STR];
  BLI_snprintf(status_str,
               sizeof(status_str),
               IFACE_("Fill: ESC/RMB cancel, LMB Fill, Shift Draw on Back, MMB Adjust Extend, S: "
                      "Switch Mode, D: "
                      "Stroke Collision | %s %s (%.3f)"),
               (is_extend) ? IFACE_("Extend") : IFACE_("Radius"),
               (is_extend && use_stroke_collide) ? IFACE_("Stroke: ON") : IFACE_("Stroke: OFF"),
               fill_extend_fac);

  ED_workspace_status_text(&C, status_str);
}

static void grease_pencil_update_extend(bContext &C, const GreasePencilFillOpData &op_data)
{
  if (op_data.fill_extend_fac > 0.0f) {
    // TODO update extension lines.
  }
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
      return VArray<bool>::ForFunc(all_layers.size(), [active_layer_index](const int index) {
        return index != active_layer_index;
      });
    case GP_FILL_GPLMODE_ABOVE:
      return VArray<bool>::ForFunc(all_layers.size(), [active_layer_index](const int index) {
        return index != active_layer_index + 1;
      });
    case GP_FILL_GPLMODE_BELOW:
      return VArray<bool>::ForFunc(all_layers.size(), [active_layer_index](const int index) {
        return index != active_layer_index - 1;
      });
    case GP_FILL_GPLMODE_ALL_ABOVE:
      return VArray<bool>::ForFunc(all_layers.size(), [active_layer_index](const int index) {
        return index <= active_layer_index;
      });
    case GP_FILL_GPLMODE_ALL_BELOW:
      return VArray<bool>::ForFunc(all_layers.size(), [active_layer_index](const int index) {
        return index >= active_layer_index;
      });
    case GP_FILL_GPLMODE_VISIBLE:
      return VArray<bool>::ForFunc(all_layers.size(), [grease_pencil](const int index) {
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
  if (curves.points_num() == 0) {
    return;
  }
  if (stroke_mask.is_empty()) {
    return;
  }

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const VArray<bool> cyclic = curves.cyclic();
  const VArray<bool> point_selection = VArray<bool>::ForSingle(true, curves.points_num());

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
  const Brush &brush = *BKE_paint_brush(&ts.gp_paint->paint);
  const float2 mouse_position = float2(event.mval);
  const int simplify_levels = brush.gpencil_settings->fill_simplylvl;

  if (!grease_pencil.has_active_layer()) {
    return false;
  }
  /* Add drawings in the active layer if autokey is enabled. */
  Vector<FillToolTargetInfo> target_drawings = ensure_editable_drawings(
      scene, grease_pencil, *grease_pencil.get_active_layer());

  const VArray<bool> boundary_layers = get_fill_boundary_layers(
      grease_pencil, eGP_FillLayerModes(brush.gpencil_settings->fill_layer_mode));

  for (const FillToolTargetInfo &info : target_drawings) {
    const Layer &layer = *grease_pencil.layers()[info.target.layer_index];

    bke::CurvesGeometry fill_curves = fill_strokes(view_context,
                                                   brush,
                                                   scene,
                                                   layer,
                                                   boundary_layers,
                                                   info.sources,
                                                   op_data.invert,
                                                   mouse_position,
                                                   fit_method,
                                                   op_data.material_index,
                                                   keep_images);

    smooth_fill_strokes(fill_curves, fill_curves.curves_range());

    if (simplify_levels > 0) {
      fill_curves = simplify_fixed(fill_curves, brush.gpencil_settings->fill_simplylvl);
    }

    Curves *dst_curves_id = curves_new_nomain(std::move(info.target.drawing.strokes_for_write()));
    Curves *fill_curves_id = curves_new_nomain(fill_curves);
    Array<bke::GeometrySet> geometry_sets = {bke::GeometrySet::from_curves(dst_curves_id),
                                             bke::GeometrySet::from_curves(fill_curves_id)};
    bke::GeometrySet joined_geometry_set = geometry::join_geometries(geometry_sets, {});
    bke::CurvesGeometry joined_curves =
        (joined_geometry_set.has_curves() ?
             std::move(joined_geometry_set.get_curves_for_write()->geometry.wrap()) :
             bke::CurvesGeometry());
    info.target.drawing.strokes_for_write() = std::move(joined_curves);
    info.target.drawing.tag_topology_changed();
  }

  WM_cursor_modal_restore(&win);

  /* Save extend value for next operation. */
  brush.gpencil_settings->fill_extend_fac = op_data.fill_extend_fac;

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
  BKE_curvemapping_init(brush.gpencil_settings->curve_rand_hue);
  BKE_curvemapping_init(brush.gpencil_settings->curve_rand_saturation);
  BKE_curvemapping_init(brush.gpencil_settings->curve_rand_value);

  Material *material = BKE_grease_pencil_object_material_ensure_from_active_input_brush(
      &bmain, &ob, &brush);
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
  Object &ob = *CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob.data);

  WM_cursor_modal_restore(CTX_wm_window(&C));

  if (op.customdata) {
    MEM_delete(static_cast<GreasePencilFillOpData *>(op.customdata));
    op.customdata = nullptr;
  }

  /* Clear status message area. */
  ED_workspace_status_text(&C, nullptr);

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_main_add_notifier(NC_GEOM | ND_DATA, nullptr);
  WM_event_add_notifier(&C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
}

static int grease_pencil_fill_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
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
  const auto &op_data = *static_cast<GreasePencilFillOpData *>(op->customdata);

  WM_cursor_modal_set(CTX_wm_window(C), WM_CURSOR_PAINT_BRUSH);
  grease_pencil_fill_status_indicators(*C, op_data);

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, nullptr);

  /* Add a modal handler for this operator. */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

enum class FillToolModalKey : int8_t {
  Cancel = 1,
  Confirm,
  GapClosureMode,
  ExtensionsLengthen,
  ExtensionsShorten,
  ExtensionsDrag,
  ExtensionsCollide,
  Invert,
  Precision,
};

static int grease_pencil_fill_event_modal_map(bContext *C, wmOperator *op, const wmEvent *event)
{
  auto &op_data = *static_cast<GreasePencilFillOpData *>(op->customdata);
  const bool show_extend = (op_data.flag & GP_BRUSH_FILL_SHOW_EXTENDLINES);
  // const bool help_lines = (((op_data.flag & GP_BRUSH_FILL_SHOW_HELPLINES) || show_extend) &&
  //                          !is_inverted);
  // const bool extend_lines = (op_data.fill_extend_fac > 0.0f);
  const float extension_delta = (op_data.precision ? 0.01f : 0.1f);

  switch (event->val) {
    case int(FillToolModalKey::Cancel):
      return OPERATOR_CANCELLED;

    case int(FillToolModalKey::Confirm): {
      /* Ignore in extension mode. */
      if (op_data.is_extension_mode) {
        break;
      }

      op_data.fill_mouse_pos = float2(event->mval);
      return (grease_pencil_apply_fill(*C, *op, *event) ? OPERATOR_FINISHED : OPERATOR_CANCELLED);
    }

    case int(FillToolModalKey::GapClosureMode):
      if (show_extend && event->val == KM_PRESS) {
        /* Toggle mode. */
        if (op_data.fill_extend_mode == GP_FILL_EMODE_EXTEND) {
          op_data.fill_extend_mode = GP_FILL_EMODE_RADIUS;
        }
        else {
          op_data.fill_extend_mode = GP_FILL_EMODE_EXTEND;
        }
        grease_pencil_update_extend(*C, op_data);
      }
      break;

    case int(FillToolModalKey::ExtensionsLengthen):
      op_data.fill_extend_fac = std::max(op_data.fill_extend_fac - extension_delta, 0.0f);
      grease_pencil_update_extend(*C, op_data);
      break;

    case int(FillToolModalKey::ExtensionsShorten):
      op_data.fill_extend_fac = std::min(op_data.fill_extend_fac + extension_delta, 10.0f);
      grease_pencil_update_extend(*C, op_data);
      break;

    case int(FillToolModalKey::ExtensionsDrag): {
      if (event->val == KM_PRESS) {
        /* Consider initial offset as zero position. */
        op_data.is_extension_mode = true;
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
        op_data.is_extension_mode = false;
      }
      /* Update cursor line. */
      WM_main_add_notifier(NC_GEOM | ND_DATA, nullptr);
      WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
      break;
    }

    case int(FillToolModalKey::ExtensionsCollide):
      if (show_extend && event->val == KM_PRESS) {
        op_data.flag ^= GP_BRUSH_FILL_STROKE_COLLIDE;
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

static int grease_pencil_fill_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  const RegionView3D &rv3d = *CTX_wm_region_view3d(C);

  auto &op_data = *static_cast<GreasePencilFillOpData *>(op->customdata);

  int estate = OPERATOR_RUNNING_MODAL;
  switch (event->type) {
    case EVT_MODAL_MAP:
      estate = grease_pencil_fill_event_modal_map(C, op, event);
      break;
    case MOUSEMOVE: {
      if (!op_data.is_extension_mode) {
        break;
      }

      const Object &ob = *CTX_data_active_object(C);
      const float pixel_size = ED_view3d_pixel_size(&rv3d, ob.loc);
      const float2 mouse_pos = float2(event->mval);
      const float initial_dist = math::distance(op_data.extension_mouse_pos,
                                                op_data.fill_mouse_pos);
      const float current_dist = math::distance(mouse_pos, op_data.fill_mouse_pos);

      float delta = (current_dist - initial_dist) * pixel_size * 0.5f;
      op_data.fill_extend_fac = std::clamp(op_data.fill_extend_fac + delta, 0.0f, 10.0f);

      /* Update cursor line and extend lines. */
      WM_main_add_notifier(NC_GEOM | ND_DATA, nullptr);
      WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

      grease_pencil_update_extend(*C, op_data);
      break;
    }
    default:
      break;
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

  prop = RNA_def_boolean(ot->srna, "on_back", false, "Draw on Back", "Send new stroke to back");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna, "invert", false, "Invert", "Find boundary of unfilled instead of filled regions");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna, "precision", false, "Precision", "Use precision movement for extension lines");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
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
  WM_operatortype_append(GREASE_PENCIL_OT_fill);
}

void ED_filltool_modal_keymap(wmKeyConfig *keyconf)
{
  using namespace blender::ed::greasepencil;
  using blender::ed::sculpt_paint::FillToolModalKey;

  static const EnumPropertyItem modal_items[] = {
      {int(FillToolModalKey::Cancel), "CANCEL", 0, "Cancel", ""},
      {int(FillToolModalKey::Confirm), "CONFIRM", 0, "Confirm", ""},
      {int(FillToolModalKey::GapClosureMode), "GAP_CLOSURE_MODE", 0, "Gap Closure Mode", ""},
      {int(FillToolModalKey::ExtensionsLengthen),
       "EXTENSIONS_LENGTHEN",
       0,
       "Length Extensions",
       ""},
      {int(FillToolModalKey::ExtensionsShorten),
       "EXTENSIONS_SHORTEN",
       0,
       "Shorten Extensions",
       ""},
      {int(FillToolModalKey::ExtensionsDrag), "EXTENSIONS_DRAG", 0, "Drag Extensions", ""},
      {int(FillToolModalKey::ExtensionsCollide),
       "EXTENSIONS_COLLIDE",
       0,
       "Collide Extensions",
       ""},
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
