/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "DNA_brush_types.h"

#include "BKE_context.hh"
#include "BKE_material.hh"
#include "BKE_paint.hh"

#include "DNA_brush_enums.h"
#include "DNA_object_enums.h"
#include "DNA_scene_types.h"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_screen.hh"

#include "WM_api.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "RNA_access.hh"

namespace blender::ed::greasepencil {

bool grease_pencil_context_poll(bContext *C)
{
  GreasePencil *grease_pencil = blender::ed::greasepencil::from_context(*C);
  if (!grease_pencil || ID_IS_LINKED(grease_pencil)) {
    return false;
  }
  return true;
}

bool active_grease_pencil_poll(bContext *C)
{
  Object *object = CTX_data_active_object(C);
  if (object == nullptr || object->type != OB_GREASE_PENCIL) {
    return false;
  }
  return true;
}

bool active_grease_pencil_material_poll(bContext *C)
{
  Object *object = CTX_data_active_object(C);
  if (object == nullptr || object->type != OB_GREASE_PENCIL) {
    return false;
  }
  short *totcolp = BKE_object_material_len_p(object);
  return *totcolp > 0;
}

bool editable_grease_pencil_poll(bContext *C)
{
  Object *object = CTX_data_active_object(C);
  if (object == nullptr || object->type != OB_GREASE_PENCIL) {
    return false;
  }
  if (!ED_operator_object_active_editable_ex(C, object)) {
    return false;
  }

  const GreasePencil *grease_pencil = static_cast<GreasePencil *>(object->data);
  if (ID_IS_LINKED(grease_pencil)) {
    return false;
  }

  return true;
}

bool editable_grease_pencil_with_region_view3d_poll(bContext *C)
{
  return ED_operator_region_view3d_active(C) && editable_grease_pencil_poll(C);
}

bool active_grease_pencil_layer_poll(bContext *C)
{
  if (!grease_pencil_context_poll(C)) {
    return false;
  }
  const GreasePencil *grease_pencil = blender::ed::greasepencil::from_context(*C);
  return grease_pencil && grease_pencil->has_active_layer();
}

bool active_grease_pencil_layer_group_poll(bContext *C)
{
  if (!grease_pencil_context_poll(C)) {
    return false;
  }
  const GreasePencil *grease_pencil = blender::ed::greasepencil::from_context(*C);
  return grease_pencil && grease_pencil->has_active_group();
}

bool editable_grease_pencil_point_selection_poll(bContext *C)
{
  if (!editable_grease_pencil_poll(C)) {
    return false;
  }

  /* Allowed: point and segment selection mode, not allowed: stroke selection mode. */
  ToolSettings *ts = CTX_data_tool_settings(C);
  return (ts->gpencil_selectmode_edit != GP_SELECTMODE_STROKE);
}

bool grease_pencil_selection_poll(bContext *C)
{
  if (!active_grease_pencil_poll(C)) {
    return false;
  }
  Object *object = CTX_data_active_object(C);
  /* Selection operators are available in multiple modes, e.g. for masking in sculpt and vertex
   * paint mode. */
  if (!ELEM(
          object->mode, OB_MODE_EDIT, OB_MODE_SCULPT_GREASE_PENCIL, OB_MODE_VERTEX_GREASE_PENCIL))
  {
    return false;
  }
  return true;
}

bool grease_pencil_painting_poll(bContext *C)
{
  if (!active_grease_pencil_poll(C)) {
    return false;
  }
  Object *object = CTX_data_active_object(C);
  if ((object->mode & OB_MODE_PAINT_GREASE_PENCIL) == 0) {
    return false;
  }
  ToolSettings *ts = CTX_data_tool_settings(C);
  if (!ts || !ts->gp_paint) {
    return false;
  }
  return true;
}

bool grease_pencil_edit_poll(bContext *C)
{
  if (!active_grease_pencil_poll(C)) {
    return false;
  }
  Object *object = CTX_data_active_object(C);
  if ((object->mode & OB_MODE_EDIT) == 0) {
    return false;
  }
  return true;
}

bool grease_pencil_sculpting_poll(bContext *C)
{
  if (!active_grease_pencil_poll(C)) {
    return false;
  }
  Object *object = CTX_data_active_object(C);
  if ((object->mode & OB_MODE_SCULPT_GREASE_PENCIL) == 0) {
    return false;
  }
  ToolSettings *ts = CTX_data_tool_settings(C);
  if (!ts || !ts->gp_sculptpaint) {
    return false;
  }
  return true;
}

bool grease_pencil_weight_painting_poll(bContext *C)
{
  if (!active_grease_pencil_poll(C)) {
    return false;
  }
  Object *object = CTX_data_active_object(C);
  if ((object->mode & OB_MODE_WEIGHT_GREASE_PENCIL) == 0) {
    return false;
  }
  ToolSettings *ts = CTX_data_tool_settings(C);
  if (!ts || !ts->gp_weightpaint) {
    return false;
  }
  return true;
}

bool grease_pencil_vertex_painting_poll(bContext *C)
{
  if (!active_grease_pencil_poll(C)) {
    return false;
  }
  Object *object = CTX_data_active_object(C);
  if ((object->mode & OB_MODE_VERTEX_GREASE_PENCIL) == 0) {
    return false;
  }
  ToolSettings *ts = CTX_data_tool_settings(C);
  if (!ts || !ts->gp_vertexpaint) {
    return false;
  }
  return true;
}

static void keymap_grease_pencil_selection(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(
      keyconf, "Grease Pencil Selection", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = grease_pencil_selection_poll;
}

static void keymap_grease_pencil_edit_mode(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(
      keyconf, "Grease Pencil Edit Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = grease_pencil_edit_poll;
}

static void keymap_grease_pencil_paint_mode(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(
      keyconf, "Grease Pencil Paint Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = grease_pencil_painting_poll;
}

static void keymap_grease_pencil_sculpt_mode(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(
      keyconf, "Grease Pencil Sculpt Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = grease_pencil_sculpting_poll;
}

static void keymap_grease_pencil_weight_paint_mode(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(
      keyconf, "Grease Pencil Weight Paint", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = grease_pencil_weight_painting_poll;
}

static void keymap_grease_pencil_vertex_paint_mode(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(
      keyconf, "Grease Pencil Vertex Paint", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = grease_pencil_vertex_painting_poll;
}

/* Enabled for all tools except the fill tool and primitive tools. */
static bool keymap_grease_pencil_brush_stroke_poll(bContext *C)
{
  if (!grease_pencil_painting_poll(C)) {
    return false;
  }
  if (!WM_toolsystem_active_tool_is_brush(C)) {
    return false;
  }

  /* Don't use the normal brush stroke keymap while the primitive tools are active. Otherwise
   * simple mouse presses start freehand drawing instead of invoking the primitive operators. Could
   * be a flag on the tool itself, for now making it a hardcoded exception. */
  if (const bToolRef *tref = WM_toolsystem_ref_from_context(C)) {
    const Set<StringRef> primitive_tools = {
        "builtin.line",
        "builtin.polyline",
        "builtin.arc",
        "builtin.curve",
        "builtin.box",
        "builtin.circle",
    };
    if (primitive_tools.contains(tref->idname)) {
      return false;
    }
  }

  ToolSettings *ts = CTX_data_tool_settings(C);
  Brush *brush = BKE_paint_brush(&ts->gp_paint->paint);
  return brush && brush->gpencil_settings && brush->gpencil_brush_type != GPAINT_BRUSH_TYPE_FILL;
}

static void keymap_grease_pencil_brush_stroke(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(
      keyconf, "Grease Pencil Brush Stroke", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = keymap_grease_pencil_brush_stroke_poll;
}

/* Enabled only for the fill tool. */
static bool keymap_grease_pencil_fill_tool_poll(bContext *C)
{
  if (!grease_pencil_painting_poll(C)) {
    return false;
  }
  if (!WM_toolsystem_active_tool_is_brush(C)) {
    return false;
  }
  ToolSettings *ts = CTX_data_tool_settings(C);
  Brush *brush = BKE_paint_brush(&ts->gp_paint->paint);
  return brush && brush->gpencil_settings && brush->gpencil_brush_type == GPAINT_BRUSH_TYPE_FILL;
}

static void keymap_grease_pencil_fill_tool(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(
      keyconf, "Grease Pencil Fill Tool", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = keymap_grease_pencil_fill_tool_poll;
}

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil()
{
  ED_operatortypes_grease_pencil_draw();
  ED_operatortypes_grease_pencil_frames();
  ED_operatortypes_grease_pencil_layers();
  ED_operatortypes_grease_pencil_select();
  ED_operatortypes_grease_pencil_edit();
  ED_operatortypes_grease_pencil_join();
  ED_operatortypes_grease_pencil_material();
  ED_operatortypes_grease_pencil_modes();
  ED_operatortypes_grease_pencil_primitives();
  ED_operatortypes_grease_pencil_weight_paint();
  ED_operatortypes_grease_pencil_vertex_paint();
  ED_operatortypes_grease_pencil_interpolate();
  ED_operatortypes_grease_pencil_lineart();
  ED_operatortypes_grease_pencil_trace();
  ED_operatortypes_grease_pencil_bake_animation();
  ED_operatortypes_grease_pencil_pen();
}

void ED_operatormacros_grease_pencil()
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  /* Duplicate + Move = Interactively place newly duplicated strokes */
  ot = WM_operatortype_append_macro(
      "GREASE_PENCIL_OT_duplicate_move",
      "Duplicate Strokes",
      "Make copies of the selected Grease Pencil strokes and move them",
      OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "GREASE_PENCIL_OT_duplicate");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);

  /* Subdivide and Smooth. */
  ot = WM_operatortype_append_macro("GREASE_PENCIL_OT_stroke_subdivide_smooth",
                                    "Subdivide and Smooth",
                                    "Subdivide strokes and smooth them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "GREASE_PENCIL_OT_stroke_subdivide");
  WM_operatortype_macro_define(ot, "GREASE_PENCIL_OT_stroke_smooth");

  /* Extrude + Move = Interactively add new points */
  ot = WM_operatortype_append_macro("GREASE_PENCIL_OT_extrude_move",
                                    "Extrude Stroke Points",
                                    "Extrude selected points and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "GREASE_PENCIL_OT_extrude");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);
}

void ED_keymap_grease_pencil(wmKeyConfig *keyconf)
{
  using namespace blender::ed::greasepencil;
  keymap_grease_pencil_selection(keyconf);
  keymap_grease_pencil_edit_mode(keyconf);
  keymap_grease_pencil_paint_mode(keyconf);
  keymap_grease_pencil_sculpt_mode(keyconf);
  keymap_grease_pencil_weight_paint_mode(keyconf);
  keymap_grease_pencil_vertex_paint_mode(keyconf);
  keymap_grease_pencil_brush_stroke(keyconf);
  keymap_grease_pencil_fill_tool(keyconf);

  ED_primitivetool_modal_keymap(keyconf);
  ED_filltool_modal_keymap(keyconf);
  ED_interpolatetool_modal_keymap(keyconf);
  ED_grease_pencil_pentool_modal_keymap(keyconf);
}
