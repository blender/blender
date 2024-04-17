/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BKE_context.hh"

#include "DNA_object_enums.h"
#include "DNA_scene_types.h"

#include "ED_grease_pencil.hh"
#include "ED_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"

namespace blender::ed::greasepencil {

bool active_grease_pencil_poll(bContext *C)
{
  Object *object = CTX_data_active_object(C);
  if (object == nullptr || object->type != OB_GREASE_PENCIL) {
    return false;
  }
  return true;
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
  if ((object->mode & OB_MODE_EDIT) == 0) {
    return false;
  }
  return true;
}

bool active_grease_pencil_layer_poll(bContext *C)
{
  Object *object = CTX_data_active_object(C);
  if (object == nullptr || object->type != OB_GREASE_PENCIL) {
    return false;
  }
  const GreasePencil *grease_pencil = static_cast<GreasePencil *>(object->data);
  return grease_pencil->has_active_layer();
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

bool grease_pencil_sculpting_poll(bContext *C)
{
  if (!active_grease_pencil_poll(C)) {
    return false;
  }
  Object *object = CTX_data_active_object(C);
  if ((object->mode & OB_MODE_SCULPT_GPENCIL_LEGACY) == 0) {
    return false;
  }
  ToolSettings *ts = CTX_data_tool_settings(C);
  if (!ts || !ts->gp_sculptpaint) {
    return false;
  }
  return true;
}

static void keymap_grease_pencil_edit_mode(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(
      keyconf, "Grease Pencil Edit Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = editable_grease_pencil_poll;
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

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil()
{
  ED_operatortypes_grease_pencil_draw();
  ED_operatortypes_grease_pencil_frames();
  ED_operatortypes_grease_pencil_layers();
  ED_operatortypes_grease_pencil_select();
  ED_operatortypes_grease_pencil_edit();
  ED_operatortypes_grease_pencil_material();
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
}

void ED_keymap_grease_pencil(wmKeyConfig *keyconf)
{
  using namespace blender::ed::greasepencil;
  keymap_grease_pencil_edit_mode(keyconf);
  keymap_grease_pencil_paint_mode(keyconf);
  keymap_grease_pencil_sculpt_mode(keyconf);
}
