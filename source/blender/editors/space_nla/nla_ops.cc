/* SPDX-FileCopyrightText: 2009 Blender Foundation, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnla
 */

#include <cstdio>
#include <cstring>

#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_anim_api.h"
#include "ED_screen.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "nla_intern.hh" /* own include */

/* ************************** poll callbacks for operators **********************************/

bool nlaop_poll_tweakmode_off(bContext *C)
{
  Scene *scene;

  /* for now, we check 2 things:
   * 1) active editor must be NLA
   * 2) tweak-mode is currently set as a 'per-scene' flag
   *    so that it will affect entire NLA data-sets,
   *    but not all AnimData blocks will be in tweak-mode for various reasons.
   */
  if (ED_operator_nla_active(C) == 0) {
    return false;
  }

  scene = CTX_data_scene(C);
  if ((scene == nullptr) || (scene->flag & SCE_NLA_EDIT_ON)) {
    return false;
  }

  return true;
}

bool nlaop_poll_tweakmode_on(bContext *C)
{
  Scene *scene;

  /* for now, we check 2 things:
   * 1) active editor must be NLA
   * 2) tweak-mode is currently set as a 'per-scene' flag
   *    so that it will affect entire NLA data-sets,
   *    but not all AnimData blocks will be in tweak-mode for various reasons.
   */
  if (ED_operator_nla_active(C) == 0) {
    return false;
  }

  scene = CTX_data_scene(C);
  if ((scene == nullptr) || !(scene->flag & SCE_NLA_EDIT_ON)) {
    return false;
  }

  return true;
}

bool nlaedit_is_tweakmode_on(bAnimContext *ac)
{
  if (ac && ac->scene) {
    return (ac->scene->flag & SCE_NLA_EDIT_ON) != 0;
  }
  return false;
}

/* ************************** registration - operator types **********************************/

void nla_operatortypes()
{
  /* channels */
  WM_operatortype_append(NLA_OT_channels_click);

  WM_operatortype_append(NLA_OT_action_pushdown);
  WM_operatortype_append(NLA_OT_action_unlink);

  WM_operatortype_append(NLA_OT_tracks_add);
  WM_operatortype_append(NLA_OT_tracks_delete);

  WM_operatortype_append(NLA_OT_selected_objects_add);

  /* select */
  WM_operatortype_append(NLA_OT_click_select);
  WM_operatortype_append(NLA_OT_select_box);
  WM_operatortype_append(NLA_OT_select_all);
  WM_operatortype_append(NLA_OT_select_leftright);

  /* view */
  WM_operatortype_append(NLA_OT_view_all);
  WM_operatortype_append(NLA_OT_view_selected);
  WM_operatortype_append(NLA_OT_view_frame);

  WM_operatortype_append(NLA_OT_previewrange_set);

  /* edit */
  WM_operatortype_append(NLA_OT_tweakmode_enter);
  WM_operatortype_append(NLA_OT_tweakmode_exit);

  WM_operatortype_append(NLA_OT_actionclip_add);
  WM_operatortype_append(NLA_OT_transition_add);
  WM_operatortype_append(NLA_OT_soundclip_add);

  WM_operatortype_append(NLA_OT_meta_add);
  WM_operatortype_append(NLA_OT_meta_remove);

  WM_operatortype_append(NLA_OT_duplicate);
  WM_operatortype_append(NLA_OT_delete);
  WM_operatortype_append(NLA_OT_split);

  WM_operatortype_append(NLA_OT_mute_toggle);

  WM_operatortype_append(NLA_OT_swap);
  WM_operatortype_append(NLA_OT_move_up);
  WM_operatortype_append(NLA_OT_move_down);

  WM_operatortype_append(NLA_OT_action_sync_length);

  WM_operatortype_append(NLA_OT_make_single_user);

  WM_operatortype_append(NLA_OT_apply_scale);
  WM_operatortype_append(NLA_OT_clear_scale);

  WM_operatortype_append(NLA_OT_snap);

  WM_operatortype_append(NLA_OT_fmodifier_add);
  WM_operatortype_append(NLA_OT_fmodifier_copy);
  WM_operatortype_append(NLA_OT_fmodifier_paste);
}

void ED_operatormacros_nla()
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro(
      "NLA_OT_duplicate_move",
      "Duplicate",
      "Duplicate selected NLA-Strips, adding the new strips to new track(s)",
      OPTYPE_UNDO | OPTYPE_REGISTER);
  otmacro = WM_operatortype_macro_define(ot, "NLA_OT_duplicate");
  RNA_boolean_set(otmacro->ptr, "linked", false);
  WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");

  ot = WM_operatortype_append_macro(
      "NLA_OT_duplicate_linked_move",
      "Duplicate Linked",
      "Duplicate Linked selected NLA-Strips, adding the new strips to new track(s)",
      OPTYPE_UNDO | OPTYPE_REGISTER);

  otmacro = WM_operatortype_macro_define(ot, "NLA_OT_duplicate");
  RNA_boolean_set(otmacro->ptr, "linked", true);
  WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
}

/* ************************** registration - keymaps **********************************/

void nla_keymap(wmKeyConfig *keyconf)
{
  /* keymap for all regions ------------------------------------------- */
  WM_keymap_ensure(keyconf, "NLA Generic", SPACE_NLA, 0);

  /* channels ---------------------------------------------------------- */
  /* Channels are not directly handled by the NLA Editor module, but are inherited from the
   * animation module. Most of the relevant operations, keymaps, drawing, etc. can therefore all
   * be found in that module instead, as there are many similarities with the other
   * animation editors.
   *
   * However, those operations which involve clicking on channels and/or
   * the placement of them in the view are implemented here instead
   */
  WM_keymap_ensure(keyconf, "NLA Channels", SPACE_NLA, 0);

  /* data ------------------------------------------------------------- */
  WM_keymap_ensure(keyconf, "NLA Editor", SPACE_NLA, 0);
}
