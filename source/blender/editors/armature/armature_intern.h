/**
 * $Id:
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_ARMATURE_INTERN_H
#define ED_ARMATURE_INTERN_H

/* internal exports only */
struct wmOperatorType;

/* editarmature.c operators */
void ARMATURE_OT_bone_primitive_add(struct wmOperatorType *ot);

void ARMATURE_OT_align(struct wmOperatorType *ot);
void ARMATURE_OT_calculate_roll(struct wmOperatorType *ot);
void ARMATURE_OT_switch_direction(struct wmOperatorType *ot);

void ARMATURE_OT_subdivs(struct wmOperatorType *ot);
void ARMATURE_OT_subdivide_simple(struct wmOperatorType *ot);
void ARMATURE_OT_subdivide_multi(struct wmOperatorType *ot);

void ARMATURE_OT_parent_set(struct wmOperatorType *ot);
void ARMATURE_OT_parent_clear(struct wmOperatorType *ot);

void ARMATURE_OT_select_all_toggle(struct wmOperatorType *ot);
void ARMATURE_OT_select_inverse(struct wmOperatorType *ot);
void ARMATURE_OT_select_hierarchy(struct wmOperatorType *ot);
void ARMATURE_OT_select_linked(struct wmOperatorType *ot);

void ARMATURE_OT_delete(struct wmOperatorType *ot);
void ARMATURE_OT_duplicate(struct wmOperatorType *ot);
void ARMATURE_OT_extrude(struct wmOperatorType *ot);
void ARMATURE_OT_click_extrude(struct wmOperatorType *ot);
void ARMATURE_OT_fill(struct wmOperatorType *ot);
void ARMATURE_OT_merge(struct wmOperatorType *ot);
void ARMATURE_OT_separate(struct wmOperatorType *ot);

void ARMATURE_OT_autoside_names(struct wmOperatorType *ot);
void ARMATURE_OT_flip_names(struct wmOperatorType *ot);

void ARMATURE_OT_flags_set(struct wmOperatorType *ot);

void ARMATURE_OT_armature_layers(struct wmOperatorType *ot);
void ARMATURE_OT_bone_layers(struct wmOperatorType *ot);

/* ******************************************************* */
/* Pose-Mode Operators */
void POSE_OT_hide(struct wmOperatorType *ot);
void POSE_OT_reveal(struct wmOperatorType *ot);

void POSE_OT_apply(struct wmOperatorType *ot);

void POSE_OT_rot_clear(struct wmOperatorType *ot);
void POSE_OT_loc_clear(struct wmOperatorType *ot);
void POSE_OT_scale_clear(struct wmOperatorType *ot);

void POSE_OT_copy(struct wmOperatorType *ot);
void POSE_OT_paste(struct wmOperatorType *ot);

void POSE_OT_select_all_toggle(struct wmOperatorType *ot);
void POSE_OT_select_inverse(struct wmOperatorType *ot);
void POSE_OT_select_parent(struct wmOperatorType *ot);
void POSE_OT_select_hierarchy(struct wmOperatorType *ot);
void POSE_OT_select_linked(struct wmOperatorType *ot);
void POSE_OT_select_constraint_target(struct wmOperatorType *ot);

void POSE_OT_group_add(struct wmOperatorType *ot);
void POSE_OT_group_remove(struct wmOperatorType *ot);
void POSE_OT_group_remove(struct wmOperatorType *ot);
void POSE_OT_group_assign(struct wmOperatorType *ot);
void POSE_OT_group_unassign(struct wmOperatorType *ot);

void POSE_OT_paths_calculate(struct wmOperatorType *ot);
void POSE_OT_paths_clear(struct wmOperatorType *ot);

void POSE_OT_autoside_names(struct wmOperatorType *ot);
void POSE_OT_flip_names(struct wmOperatorType *ot);

void POSE_OT_flags_set(struct wmOperatorType *ot);

void POSE_OT_armature_layers(struct wmOperatorType *ot);
void POSE_OT_bone_layers(struct wmOperatorType *ot);

/* ******************************************************* */
/* Etch-A-Ton */

void SKETCH_OT_gesture(struct wmOperatorType *ot);
void SKETCH_OT_delete(struct wmOperatorType *ot);
void SKETCH_OT_draw_stroke(struct wmOperatorType *ot);
void SKETCH_OT_draw_preview(struct wmOperatorType *ot);
void SKETCH_OT_finish_stroke(struct wmOperatorType *ot);
void SKETCH_OT_cancel_stroke(struct wmOperatorType *ot);
void SKETCH_OT_select(struct wmOperatorType *ot);

/* ******************************************************* */
/* PoseLib */

void POSELIB_OT_pose_add(struct wmOperatorType *ot);
void POSELIB_OT_pose_remove(struct wmOperatorType *ot);
void POSELIB_OT_pose_rename(struct wmOperatorType *ot);
void POSELIB_OT_browse_interactive(struct wmOperatorType *ot);

/* ******************************************************* */
/* Pose Sliding Tools */

void POSE_OT_push(struct wmOperatorType *ot);
void POSE_OT_relax(struct wmOperatorType *ot);
void POSE_OT_breakdown(struct wmOperatorType *ot);

/* ******************************************************* */
/* editarmature.c */
struct bArmature;
struct EditBone;
struct ListBase;

EditBone *make_boneList(struct ListBase *edbo, struct ListBase *bones, struct EditBone *parent, struct Bone *actBone);
struct EditBone *addEditBone(struct bArmature *arm, char *name);
void BIF_sk_selectStroke(struct bContext *C, short mval[2], short extend);

/* duplicate method */
void preEditBoneDuplicate(struct ListBase *editbones);
struct EditBone *duplicateEditBone(struct EditBone *curBone, char *name, struct ListBase *editbones, struct Object *ob);
void updateDuplicateSubtarget(struct EditBone *dupBone, struct ListBase *editbones, struct Object *ob);

/* duplicate method (cross objects */

/* editbones is the target list */
struct EditBone *duplicateEditBoneObjects(struct EditBone *curBone, char *name, struct ListBase *editbones, struct Object *src_ob, struct Object *dst_ob);

/* editbones is the source list */
void updateDuplicateSubtargetObjects(struct EditBone *dupBone, struct ListBase *editbones, struct Object *src_ob, struct Object *dst_ob);

#endif /* ED_ARMATURE_INTERN_H */

