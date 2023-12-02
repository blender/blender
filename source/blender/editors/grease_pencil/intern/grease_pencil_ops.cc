/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "ED_grease_pencil.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"

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
}
