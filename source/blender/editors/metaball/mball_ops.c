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
 * \ingroup edmeta
 */

#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mball.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_object.h"

#include "mball_intern.h"

void ED_operatortypes_metaball(void)
{
  WM_operatortype_append(MBALL_OT_delete_metaelems);
  WM_operatortype_append(MBALL_OT_duplicate_metaelems);

  WM_operatortype_append(MBALL_OT_hide_metaelems);
  WM_operatortype_append(MBALL_OT_reveal_metaelems);

  WM_operatortype_append(MBALL_OT_select_all);
  WM_operatortype_append(MBALL_OT_select_similar);
  WM_operatortype_append(MBALL_OT_select_random_metaelems);
}

void ED_operatormacros_metaball(void)
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro("MBALL_OT_duplicate_move",
                                    "Duplicate",
                                    "Make copies of the selected metaelements and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "MBALL_OT_duplicate_metaelems");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
}

void ED_keymap_metaball(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Metaball", 0, 0);
  keymap->poll = ED_operator_editmball;
}
