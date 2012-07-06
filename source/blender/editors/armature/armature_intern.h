/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/armature/armature_intern.h
 *  \ingroup edarmature
 */

#ifndef __ARMATURE_INTERN_H__
#define __ARMATURE_INTERN_H__

/* internal exports only */
struct wmOperatorType;

struct bContext;
struct Scene;
struct Object;
struct bAction;
struct bPoseChannel;

struct bArmature;
struct EditBone;

struct ListBase;
struct LinkData;

/* ******************************************************* */
/* editarmature.c operators */
void ARMATURE_OT_bone_primitive_add(struct wmOperatorType *ot);

void ARMATURE_OT_align(struct wmOperatorType *ot);
void ARMATURE_OT_calculate_roll(struct wmOperatorType *ot);
void ARMATURE_OT_switch_direction(struct wmOperatorType *ot);

void ARMATURE_OT_subdivide(struct wmOperatorType *ot);

void ARMATURE_OT_parent_set(struct wmOperatorType *ot);
void ARMATURE_OT_parent_clear(struct wmOperatorType *ot);

void ARMATURE_OT_select_all(struct wmOperatorType *ot);
void ARMATURE_OT_select_inverse(struct wmOperatorType *ot);
void ARMATURE_OT_select_hierarchy(struct wmOperatorType *ot);
void ARMATURE_OT_select_linked(struct wmOperatorType *ot);
void ARMATURE_OT_select_similar(struct wmOperatorType *ot);

void ARMATURE_OT_delete(struct wmOperatorType *ot);
void ARMATURE_OT_duplicate(struct wmOperatorType *ot);
void ARMATURE_OT_extrude(struct wmOperatorType *ot);
void ARMATURE_OT_hide(struct wmOperatorType *ot);
void ARMATURE_OT_reveal(struct wmOperatorType *ot);
void ARMATURE_OT_click_extrude(struct wmOperatorType *ot);
void ARMATURE_OT_fill(struct wmOperatorType *ot);
void ARMATURE_OT_merge(struct wmOperatorType *ot);
void ARMATURE_OT_separate(struct wmOperatorType *ot);

void ARMATURE_OT_autoside_names(struct wmOperatorType *ot);
void ARMATURE_OT_flip_names(struct wmOperatorType *ot);

void ARMATURE_OT_layers_show_all(struct wmOperatorType *ot);
void ARMATURE_OT_armature_layers(struct wmOperatorType *ot);
void ARMATURE_OT_bone_layers(struct wmOperatorType *ot);

/* ******************************************************* */
/* Pose-Mode Operators */
void POSE_OT_hide(struct wmOperatorType *ot);
void POSE_OT_reveal(struct wmOperatorType *ot);

void POSE_OT_armature_apply(struct wmOperatorType *ot);
void POSE_OT_visual_transform_apply(struct wmOperatorType *ot);

void POSE_OT_rot_clear(struct wmOperatorType *ot);
void POSE_OT_loc_clear(struct wmOperatorType *ot);
void POSE_OT_scale_clear(struct wmOperatorType *ot);
void POSE_OT_transforms_clear(struct wmOperatorType *ot);
void POSE_OT_user_transforms_clear(struct wmOperatorType *ot);

void POSE_OT_copy(struct wmOperatorType *ot);
void POSE_OT_paste(struct wmOperatorType *ot);

void POSE_OT_select_all(struct wmOperatorType *ot);
void POSE_OT_select_parent(struct wmOperatorType *ot);
void POSE_OT_select_hierarchy(struct wmOperatorType *ot);
void POSE_OT_select_linked(struct wmOperatorType *ot);
void POSE_OT_select_constraint_target(struct wmOperatorType *ot);
void POSE_OT_select_grouped(struct wmOperatorType *ot);
void POSE_OT_select_flip_active(struct wmOperatorType *ot);

void POSE_OT_group_add(struct wmOperatorType *ot);
void POSE_OT_group_remove(struct wmOperatorType *ot);
void POSE_OT_group_move(struct wmOperatorType *ot);
void POSE_OT_group_sort(struct wmOperatorType *ot);
void POSE_OT_group_assign(struct wmOperatorType *ot);
void POSE_OT_group_unassign(struct wmOperatorType *ot);
void POSE_OT_group_select(struct wmOperatorType *ot);
void POSE_OT_group_deselect(struct wmOperatorType *ot);

void POSE_OT_paths_calculate(struct wmOperatorType *ot);
void POSE_OT_paths_update(struct wmOperatorType *ot);
void POSE_OT_paths_clear(struct wmOperatorType *ot);

void POSE_OT_autoside_names(struct wmOperatorType *ot);
void POSE_OT_flip_names(struct wmOperatorType *ot);

void POSE_OT_rotation_mode_set(struct wmOperatorType *ot);

void POSE_OT_quaternions_flip(struct wmOperatorType *ot);

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
void SKETCH_OT_convert(struct wmOperatorType *ot);
void SKETCH_OT_select(struct wmOperatorType *ot);

/* ******************************************************* */
/* Pose Tool Utilities (for PoseLib, Pose Sliding, etc.) */
/* poseUtils.c */

/* Temporary data linking PoseChannels with the F-Curves they affect */
typedef struct tPChanFCurveLink {
	struct tPChanFCurveLink *next, *prev;
	
	ListBase fcurves;               /* F-Curves for this PoseChannel (wrapped with LinkData) */
	struct bPoseChannel *pchan;     /* Pose Channel which data is attached to */
	
	char *pchan_path;               /* RNA Path to this Pose Channel (needs to be freed when we're done) */
	
	float oldloc[3];                /* transform values at start of operator (to be restored before each modal step) */
	float oldrot[3];
	float oldscale[3];
	float oldquat[4];
	float oldangle;
	float oldaxis[3];
	
	struct IDProperty *oldprops;    /* copy of custom properties at start of operator (to be restored before each modal step) */
} tPChanFCurveLink;

/* ----------- */

void poseAnim_mapping_get(struct bContext *C, ListBase *pfLinks, struct Object *ob, struct bAction *act);
void poseAnim_mapping_free(ListBase *pfLinks);

void poseAnim_mapping_refresh(struct bContext *C, struct Scene *scene, struct Object *ob);
void poseAnim_mapping_reset(ListBase *pfLinks);
void poseAnim_mapping_autoKeyframe(struct bContext *C, struct Scene *scene, struct Object *ob, ListBase *pfLinks, float cframe);

LinkData *poseAnim_mapping_getNextFCurve(ListBase *fcuLinks, LinkData *prev, const char *path);

/* ******************************************************* */
/* PoseLib */
/* poselib.c */

void POSELIB_OT_new(struct wmOperatorType *ot);
void POSELIB_OT_unlink(struct wmOperatorType *ot);

void POSELIB_OT_action_sanitize(struct wmOperatorType *ot);

void POSELIB_OT_pose_add(struct wmOperatorType *ot);
void POSELIB_OT_pose_remove(struct wmOperatorType *ot);
void POSELIB_OT_pose_rename(struct wmOperatorType *ot);

void POSELIB_OT_browse_interactive(struct wmOperatorType *ot);
void POSELIB_OT_apply_pose(struct wmOperatorType *ot);

/* ******************************************************* */
/* Pose Sliding Tools */
/* poseSlide.c */

void POSE_OT_push(struct wmOperatorType *ot);
void POSE_OT_relax(struct wmOperatorType *ot);
void POSE_OT_breakdown(struct wmOperatorType *ot);

void POSE_OT_propagate(struct wmOperatorType *ot);

/* ******************************************************* */
/* editarmature.c */

EditBone *make_boneList(struct ListBase *edbo, struct ListBase *bones, struct EditBone *parent, struct Bone *actBone);
void BIF_sk_selectStroke(struct bContext *C, const int mval[2], short extend);

/* duplicate method */
void preEditBoneDuplicate(struct ListBase *editbones);
struct EditBone *duplicateEditBone(struct EditBone *curBone, const char *name, struct ListBase *editbones, struct Object *ob);
void updateDuplicateSubtarget(struct EditBone *dupBone, struct ListBase *editbones, struct Object *ob);

/* duplicate method (cross objects */

/* editbones is the target list */
struct EditBone *duplicateEditBoneObjects(struct EditBone *curBone, const char *name, struct ListBase *editbones, struct Object *src_ob, struct Object *dst_ob);

/* editbones is the source list */
void updateDuplicateSubtargetObjects(struct EditBone *dupBone, struct ListBase *editbones, struct Object *src_ob, struct Object *dst_ob);

#endif /* __ARMATURE_INTERN_H__ */

