/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 */

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_armature.hh"
#include "ED_screen.hh"

#include "armature_intern.h"

/* ************************** registration **********************************/

void ED_operatortypes_armature()
{
  /* Both operators `ARMATURE_OT_*` and `POSE_OT_*` are registered here. */

  /* EDIT ARMATURE */
  WM_operatortype_append(ARMATURE_OT_bone_primitive_add);

  WM_operatortype_append(ARMATURE_OT_align);
  WM_operatortype_append(ARMATURE_OT_calculate_roll);
  WM_operatortype_append(ARMATURE_OT_roll_clear);
  WM_operatortype_append(ARMATURE_OT_switch_direction);
  WM_operatortype_append(ARMATURE_OT_subdivide);

  WM_operatortype_append(ARMATURE_OT_parent_set);
  WM_operatortype_append(ARMATURE_OT_parent_clear);

  WM_operatortype_append(ARMATURE_OT_select_all);
  WM_operatortype_append(ARMATURE_OT_select_mirror);
  WM_operatortype_append(ARMATURE_OT_select_more);
  WM_operatortype_append(ARMATURE_OT_select_less);
  WM_operatortype_append(ARMATURE_OT_select_hierarchy);
  WM_operatortype_append(ARMATURE_OT_select_linked);
  WM_operatortype_append(ARMATURE_OT_select_linked_pick);
  WM_operatortype_append(ARMATURE_OT_select_similar);
  WM_operatortype_append(ARMATURE_OT_shortest_path_pick);

  WM_operatortype_append(ARMATURE_OT_delete);
  WM_operatortype_append(ARMATURE_OT_dissolve);
  WM_operatortype_append(ARMATURE_OT_duplicate);
  WM_operatortype_append(ARMATURE_OT_symmetrize);
  WM_operatortype_append(ARMATURE_OT_extrude);
  WM_operatortype_append(ARMATURE_OT_hide);
  WM_operatortype_append(ARMATURE_OT_reveal);
  WM_operatortype_append(ARMATURE_OT_click_extrude);
  WM_operatortype_append(ARMATURE_OT_fill);
  WM_operatortype_append(ARMATURE_OT_separate);
  WM_operatortype_append(ARMATURE_OT_split);

  WM_operatortype_append(ARMATURE_OT_autoside_names);
  WM_operatortype_append(ARMATURE_OT_flip_names);

  WM_operatortype_append(ARMATURE_OT_layers_show_all);
  WM_operatortype_append(ARMATURE_OT_armature_layers);
  WM_operatortype_append(ARMATURE_OT_bone_layers);

  /* POSE */
  WM_operatortype_append(POSE_OT_hide);
  WM_operatortype_append(POSE_OT_reveal);

  WM_operatortype_append(POSE_OT_armature_apply);
  WM_operatortype_append(POSE_OT_visual_transform_apply);

  WM_operatortype_append(POSE_OT_rot_clear);
  WM_operatortype_append(POSE_OT_loc_clear);
  WM_operatortype_append(POSE_OT_scale_clear);
  WM_operatortype_append(POSE_OT_transforms_clear);
  WM_operatortype_append(POSE_OT_user_transforms_clear);

  WM_operatortype_append(POSE_OT_copy);
  WM_operatortype_append(POSE_OT_paste);

  WM_operatortype_append(POSE_OT_select_all);

  WM_operatortype_append(POSE_OT_select_parent);
  WM_operatortype_append(POSE_OT_select_hierarchy);
  WM_operatortype_append(POSE_OT_select_linked);
  WM_operatortype_append(POSE_OT_select_linked_pick);
  WM_operatortype_append(POSE_OT_select_constraint_target);
  WM_operatortype_append(POSE_OT_select_grouped);
  WM_operatortype_append(POSE_OT_select_mirror);

  WM_operatortype_append(POSE_OT_group_add);
  WM_operatortype_append(POSE_OT_group_remove);
  WM_operatortype_append(POSE_OT_group_move);
  WM_operatortype_append(POSE_OT_group_sort);
  WM_operatortype_append(POSE_OT_group_assign);
  WM_operatortype_append(POSE_OT_group_unassign);
  WM_operatortype_append(POSE_OT_group_select);
  WM_operatortype_append(POSE_OT_group_deselect);

  WM_operatortype_append(POSE_OT_paths_calculate);
  WM_operatortype_append(POSE_OT_paths_update);
  WM_operatortype_append(POSE_OT_paths_clear);
  WM_operatortype_append(POSE_OT_paths_range_update);

  WM_operatortype_append(POSE_OT_autoside_names);
  WM_operatortype_append(POSE_OT_flip_names);

  WM_operatortype_append(POSE_OT_rotation_mode_set);

  WM_operatortype_append(POSE_OT_quaternions_flip);

  WM_operatortype_append(POSE_OT_bone_layers);

  WM_operatortype_append(POSE_OT_propagate);

  /* POSELIB */
  WM_operatortype_append(POSELIB_OT_apply_pose_asset);
  WM_operatortype_append(POSELIB_OT_blend_pose_asset);

  /* POSE SLIDING */
  WM_operatortype_append(POSE_OT_push);
  WM_operatortype_append(POSE_OT_relax);
  WM_operatortype_append(POSE_OT_blend_with_rest);
  WM_operatortype_append(POSE_OT_breakdown);
  WM_operatortype_append(POSE_OT_blend_to_neighbors);
}

void ED_operatormacros_armature()
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro(
      "ARMATURE_OT_duplicate_move",
      "Duplicate",
      "Make copies of the selected bones within the same armature and move them",
      OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "ARMATURE_OT_duplicate");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);

  ot = WM_operatortype_append_macro("ARMATURE_OT_extrude_move",
                                    "Extrude",
                                    "Create new bones from the selected joints and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  otmacro = WM_operatortype_macro_define(ot, "ARMATURE_OT_extrude");
  RNA_boolean_set(otmacro->ptr, "forked", false);
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);

  /* XXX would it be nicer to just be able to have standard extrude_move,
   * but set the forked property separate?
   * that would require fixing a properties bug #19733. */
  ot = WM_operatortype_append_macro("ARMATURE_OT_extrude_forked",
                                    "Extrude Forked",
                                    "Create new bones from the selected joints and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  otmacro = WM_operatortype_macro_define(ot, "ARMATURE_OT_extrude");
  RNA_boolean_set(otmacro->ptr, "forked", true);
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
}

void ED_keymap_armature(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap;

  /* Armature ------------------------ */
  /* only set in editmode armature, by space_view3d listener */
  keymap = WM_keymap_ensure(keyconf, "Armature", 0, 0);
  keymap->poll = ED_operator_editarmature;

  /* Pose ------------------------ */
  /* only set in posemode, by space_view3d listener */
  keymap = WM_keymap_ensure(keyconf, "Pose", 0, 0);
  keymap->poll = ED_operator_posemode;
}
