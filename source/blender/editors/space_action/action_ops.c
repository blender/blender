/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spaction
 */

#include <stdlib.h>
#include <math.h>

#include "DNA_space_types.h"

#include "ED_anim_api.h"
#include "ED_markers.h"
#include "ED_transform.h"
#include "ED_object.h"
#include "ED_select_utils.h"

#include "action_intern.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

/* ************************** registration - operator types **********************************/

void action_operatortypes(void)
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
  WM_operatortype_append(ACTION_OT_keyframe_type);
  WM_operatortype_append(ACTION_OT_sample);
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

  WM_operatortype_append(ACTION_OT_layer_next);
  WM_operatortype_append(ACTION_OT_layer_prev);

  WM_operatortype_append(ACTION_OT_previewrange_set);
  WM_operatortype_append(ACTION_OT_view_all);
  WM_operatortype_append(ACTION_OT_view_selected);
  WM_operatortype_append(ACTION_OT_view_frame);

  WM_operatortype_append(ACTION_OT_markers_make_local);
}

void ED_operatormacros_action(void)
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro("ACTION_OT_duplicate_move",
                                    "Duplicate",
                                    "Make a copy of all selected keyframes and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "ACTION_OT_duplicate");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_transform");
  RNA_enum_set(otmacro->ptr, "mode", TFM_TIME_DUPLICATE);
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
}

/* ************************** registration - keymaps **********************************/

/* --------------- */

void action_keymap(wmKeyConfig *keyconf)
{
  /* keymap for all regions */
  WM_keymap_ensure(keyconf, "Dopesheet Generic", SPACE_ACTION, 0);

  /* channels */
  /* Channels are not directly handled by the Action Editor module,
   * but are inherited from the Animation module.
   * All the relevant operations, keymaps, drawing, etc.
   * can therefore all be found in that module instead, as these
   * are all used for the Graph-Editor too.
   */

  /* keyframes */
  WM_keymap_ensure(keyconf, "Dopesheet", SPACE_ACTION, 0);
}
