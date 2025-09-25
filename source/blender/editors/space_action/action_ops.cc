/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spaction
 */

#include <cstdlib>

#include "DNA_space_types.h"

#include "ED_anim_api.hh"
#include "ED_transform.hh"

#include "action_intern.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

/* ************************** registration - operator types **********************************/

void action_operatortypes()
{
  /* keyframes */
  /* selection */
  WM_operatortype_append(ACTION_OT_clickselect);
  WM_operatortype_append(ACTION_OT_select_all);
  WM_operatortype_append(ACTION_OT_select_box);
  WM_operatortype_append(ACTION_OT_select_lasso);
  WM_operatortype_append(ACTION_OT_select_circle);
  WM_operatortype_append(ACTION_OT_select_column);
  WM_operatortype_append(ACTION_OT_select_linked);
  WM_operatortype_append(ACTION_OT_select_more);
  WM_operatortype_append(ACTION_OT_select_less);
  WM_operatortype_append(ACTION_OT_select_leftright);

  /* editing */
  WM_operatortype_append(ACTION_OT_snap);
  WM_operatortype_append(ACTION_OT_mirror);
  WM_operatortype_append(ACTION_OT_frame_jump);
  WM_operatortype_append(ACTION_OT_handle_type);
  WM_operatortype_append(ACTION_OT_interpolation_type);
  WM_operatortype_append(ACTION_OT_extrapolation_type);
  WM_operatortype_append(ACTION_OT_easing_type);
  WM_operatortype_append(ACTION_OT_keyframe_type);
  WM_operatortype_append(ACTION_OT_bake_keys);
  WM_operatortype_append(ACTION_OT_clean);
  WM_operatortype_append(ACTION_OT_delete);
  WM_operatortype_append(ACTION_OT_duplicate);
  WM_operatortype_append(ACTION_OT_keyframe_insert);
  WM_operatortype_append(ACTION_OT_copy);
  WM_operatortype_append(ACTION_OT_paste);

  WM_operatortype_append(ACTION_OT_new);
  WM_operatortype_append(ACTION_OT_unlink);

  WM_operatortype_append(ACTION_OT_push_down);
  WM_operatortype_append(ACTION_OT_stash);
  WM_operatortype_append(ACTION_OT_stash_and_create);

  WM_operatortype_append(ACTION_OT_previewrange_set);
  WM_operatortype_append(ACTION_OT_view_all);
  WM_operatortype_append(ACTION_OT_view_selected);
  WM_operatortype_append(ACTION_OT_view_frame);

  WM_operatortype_append(ACTION_OT_markers_make_local);
}

void ED_operatormacros_action()
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro("ACTION_OT_duplicate_move",
                                    "Duplicate",
                                    "Make a copy of all selected keyframes and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "ACTION_OT_duplicate");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_transform");
  RNA_enum_set(otmacro->ptr, "mode", blender::ed::transform::TFM_TIME_TRANSLATE);
  RNA_boolean_set(otmacro->ptr, "use_duplicated_keyframes", true);
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
}

/* ************************** registration - keymaps **********************************/

/* --------------- */

void action_keymap(wmKeyConfig *keyconf)
{
  /* keymap for all regions */
  WM_keymap_ensure(keyconf, "Dopesheet Generic", SPACE_ACTION, RGN_TYPE_WINDOW);

  /* channels */
  /* Channels are not directly handled by the Action Editor module,
   * but are inherited from the Animation module.
   * All the relevant operations, keymaps, drawing, etc.
   * can therefore all be found in that module instead, as these
   * are all used for the Graph-Editor too.
   */

  /* keyframes */
  WM_keymap_ensure(keyconf, "Dopesheet", SPACE_ACTION, RGN_TYPE_WINDOW);
}
