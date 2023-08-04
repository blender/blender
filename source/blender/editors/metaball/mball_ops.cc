/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmeta
 */

#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_mball.h"
#include "ED_screen.hh"

#include "mball_intern.h"

void ED_operatortypes_metaball()
{
  WM_operatortype_append(MBALL_OT_delete_metaelems);
  WM_operatortype_append(MBALL_OT_duplicate_metaelems);

  WM_operatortype_append(MBALL_OT_hide_metaelems);
  WM_operatortype_append(MBALL_OT_reveal_metaelems);

  WM_operatortype_append(MBALL_OT_select_all);
  WM_operatortype_append(MBALL_OT_select_similar);
  WM_operatortype_append(MBALL_OT_select_random_metaelems);
}

void ED_operatormacros_metaball()
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro("MBALL_OT_duplicate_move",
                                    "Duplicate",
                                    "Make copies of the selected metaball elements and move them",
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
