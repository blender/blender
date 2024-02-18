/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 */

#pragma once

struct bArmature;
struct Base;
struct bContext;
struct Bone;
struct bPoseChannel;
struct EditBone;
struct GPUSelectResult;
struct LinkData;
struct ListBase;
struct Object;
struct Scene;
struct wmOperatorType;

/* -------------------------------------------------------------------- */
/** \name Armature EditMode Operators
 * \{ */

void ARMATURE_OT_bone_primitive_add(wmOperatorType *ot);

void ARMATURE_OT_align(wmOperatorType *ot);
void ARMATURE_OT_calculate_roll(wmOperatorType *ot);
void ARMATURE_OT_roll_clear(wmOperatorType *ot);
void ARMATURE_OT_switch_direction(wmOperatorType *ot);

void ARMATURE_OT_subdivide(wmOperatorType *ot);

void ARMATURE_OT_parent_set(wmOperatorType *ot);
void ARMATURE_OT_parent_clear(wmOperatorType *ot);

void ARMATURE_OT_select_all(wmOperatorType *ot);
void ARMATURE_OT_select_mirror(wmOperatorType *ot);
void ARMATURE_OT_select_more(wmOperatorType *ot);
void ARMATURE_OT_select_less(wmOperatorType *ot);
void ARMATURE_OT_select_hierarchy(wmOperatorType *ot);
void ARMATURE_OT_select_linked_pick(wmOperatorType *ot);
void ARMATURE_OT_select_linked(wmOperatorType *ot);
void ARMATURE_OT_select_similar(wmOperatorType *ot);
void ARMATURE_OT_shortest_path_pick(wmOperatorType *ot);

void ARMATURE_OT_delete(wmOperatorType *ot);
void ARMATURE_OT_dissolve(wmOperatorType *ot);
void ARMATURE_OT_duplicate(wmOperatorType *ot);
void ARMATURE_OT_symmetrize(wmOperatorType *ot);
void ARMATURE_OT_extrude(wmOperatorType *ot);
void ARMATURE_OT_hide(wmOperatorType *ot);
void ARMATURE_OT_reveal(wmOperatorType *ot);
void ARMATURE_OT_click_extrude(wmOperatorType *ot);
void ARMATURE_OT_fill(wmOperatorType *ot);
void ARMATURE_OT_separate(wmOperatorType *ot);
void ARMATURE_OT_split(wmOperatorType *ot);

void ARMATURE_OT_autoside_names(wmOperatorType *ot);
void ARMATURE_OT_flip_names(wmOperatorType *ot);

void ARMATURE_OT_collection_add(wmOperatorType *ot);
void ARMATURE_OT_collection_remove(wmOperatorType *ot);
void ARMATURE_OT_collection_move(wmOperatorType *ot);
void ARMATURE_OT_collection_assign(wmOperatorType *ot);
void ARMATURE_OT_collection_create_and_assign(wmOperatorType *ot);
void ARMATURE_OT_collection_unassign(wmOperatorType *ot);
void ARMATURE_OT_collection_unassign_named(wmOperatorType *ot);
void ARMATURE_OT_collection_select(wmOperatorType *ot);
void ARMATURE_OT_collection_deselect(wmOperatorType *ot);

void ARMATURE_OT_move_to_collection(wmOperatorType *ot);
void ARMATURE_OT_assign_to_collection(wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pose-Mode Operators
 * \{ */

void POSE_OT_hide(wmOperatorType *ot);
void POSE_OT_reveal(wmOperatorType *ot);

void POSE_OT_armature_apply(wmOperatorType *ot);
void POSE_OT_visual_transform_apply(wmOperatorType *ot);

void POSE_OT_rot_clear(wmOperatorType *ot);
void POSE_OT_loc_clear(wmOperatorType *ot);
void POSE_OT_scale_clear(wmOperatorType *ot);
void POSE_OT_transforms_clear(wmOperatorType *ot);
void POSE_OT_user_transforms_clear(wmOperatorType *ot);

void POSE_OT_copy(wmOperatorType *ot);
void POSE_OT_paste(wmOperatorType *ot);

void POSE_OT_select_all(wmOperatorType *ot);
void POSE_OT_select_parent(wmOperatorType *ot);
void POSE_OT_select_hierarchy(wmOperatorType *ot);
void POSE_OT_select_linked(wmOperatorType *ot);
void POSE_OT_select_linked_pick(wmOperatorType *ot);
void POSE_OT_select_constraint_target(wmOperatorType *ot);
void POSE_OT_select_grouped(wmOperatorType *ot);
void POSE_OT_select_mirror(wmOperatorType *ot);

void POSE_OT_paths_calculate(wmOperatorType *ot);
void POSE_OT_paths_update(wmOperatorType *ot);
void POSE_OT_paths_clear(wmOperatorType *ot);
void POSE_OT_paths_range_update(wmOperatorType *ot);

void POSE_OT_autoside_names(wmOperatorType *ot);
void POSE_OT_flip_names(wmOperatorType *ot);

void POSE_OT_rotation_mode_set(wmOperatorType *ot);

void POSE_OT_quaternions_flip(wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pose Tool Utilities (for PoseLib, Pose Sliding, etc.)
 * \{ */

/* `pose_utils.cc` */

/* Temporary data linking PoseChannels with the F-Curves they affect */
struct tPChanFCurveLink {
  tPChanFCurveLink *next, *prev;

  /** Object this Pose Channel belongs to. */
  Object *ob;

  /** F-Curves for this PoseChannel (wrapped with LinkData) */
  ListBase fcurves;
  /** Pose Channel which data is attached to */
  bPoseChannel *pchan;

  /** RNA Path to this Pose Channel (needs to be freed when we're done) */
  char *pchan_path;

  /** transform values at start of operator (to be restored before each modal step) */
  float oldloc[3];
  float oldrot[3];
  float oldscale[3];
  float oldquat[4];
  float oldangle;
  float oldaxis[3];

  /** old bbone values (to be restored along with the transform properties) */
  float roll1, roll2;
  /** (NOTE: we haven't renamed these this time, as their names are already long enough) */
  float curve_in_x, curve_in_z;
  float curve_out_x, curve_out_z;
  float ease1, ease2;
  float scale_in[3];
  float scale_out[3];

  /** copy of custom properties at start of operator (to be restored before each modal step) */
  IDProperty *oldprops;
};

/* ----------- */

/** Returns a valid pose armature for this object, else returns NULL. */
Object *poseAnim_object_get(Object *ob_);
/** Get sets of F-Curves providing transforms for the bones in the Pose. */
void poseAnim_mapping_get(bContext *C, ListBase *pfLinks);
/** Free F-Curve <-> PoseChannel links. */
void poseAnim_mapping_free(ListBase *pfLinks);

/**
 * Helper for apply() / reset() - refresh the data.
 */
void poseAnim_mapping_refresh(bContext *C, Scene *scene, Object *ob);
/**
 * Reset changes made to current pose.
 */
void poseAnim_mapping_reset(ListBase *pfLinks);
/** Perform auto-key-framing after changes were made + confirmed. */
void poseAnim_mapping_autoKeyframe(bContext *C, Scene *scene, ListBase *pfLinks, float cframe);

/**
 * Find the next F-Curve for a PoseChannel with matching path.
 * - `path` is not just the #tPChanFCurveLink (`pfl`) rna_path,
 *   since that path doesn't have property info yet.
 */
LinkData *poseAnim_mapping_getNextFCurve(ListBase *fcuLinks, LinkData *prev, const char *path);

/** \} */

/* -------------------------------------------------------------------- */
/** \name PoseLib
 * \{ */

/* `pose_lib_2.cc` */

void POSELIB_OT_apply_pose_asset(wmOperatorType *ot);
void POSELIB_OT_blend_pose_asset(wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pose Sliding Tools
 * \{ */

/* `pose_slide.cc` */

void POSE_OT_push(wmOperatorType *ot);
void POSE_OT_relax(wmOperatorType *ot);
void POSE_OT_blend_with_rest(wmOperatorType *ot);
void POSE_OT_breakdown(wmOperatorType *ot);
void POSE_OT_blend_to_neighbors(wmOperatorType *ot);

void POSE_OT_propagate(wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Various Armature Edit/Pose Editing API's
 * \{ */

/* Ideally, many of these defines would not be needed as everything would be strictly
 * self-contained within each file,
 * but some tools still have a bit of overlap which makes things messy -- Feb 2013
 */

EditBone *make_boneList(ListBase *edbo, ListBase *bones, Bone *actBone);

/* Duplicate method. */

/** Call this before doing any duplications. */
void preEditBoneDuplicate(ListBase *editbones);
void postEditBoneDuplicate(ListBase *editbones, Object *ob);
EditBone *duplicateEditBone(EditBone *cur_bone, const char *name, ListBase *editbones, Object *ob);

/* Duplicate method (cross objects). */

/**
 * \param editbones: The target list.
 */
EditBone *duplicateEditBoneObjects(
    EditBone *cur_bone, const char *name, ListBase *editbones, Object *src_ob, Object *dst_ob);

/** Adds an EditBone between the nominated locations (should be in the right space). */
EditBone *add_points_bone(Object *obedit, float head[3], float tail[3]);
void bone_free(bArmature *arm, EditBone *bone);

void armature_tag_select_mirrored(bArmature *arm);
/**
 * Helper function for tools to work on mirrored parts.
 * it leaves mirrored bones selected then too, which is a good indication of what happened.
 */
void armature_select_mirrored_ex(bArmature *arm, int flag);
void armature_select_mirrored(bArmature *arm);
/** Only works when tagged. */
void armature_tag_unselect(bArmature *arm);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Selection Picking
 * \{ */

EditBone *ED_armature_pick_ebone(bContext *C, const int xy[2], bool findunsel, Base **r_base);
bPoseChannel *ED_armature_pick_pchan(bContext *C, const int xy[2], bool findunsel, Base **r_base);
Bone *ED_armature_pick_bone(bContext *C, const int xy[2], bool findunsel, Base **r_base);

EditBone *ED_armature_pick_ebone_from_selectbuffer(blender::Span<Base *> bases,
                                                   const GPUSelectResult *hit_results,
                                                   int hits,
                                                   bool findunsel,
                                                   bool do_nearest,
                                                   Base **r_base);
bPoseChannel *ED_armature_pick_pchan_from_selectbuffer(blender::Span<Base *> bases,
                                                       const GPUSelectResult *hit_results,
                                                       int hits,
                                                       bool findunsel,
                                                       bool do_nearest,
                                                       Base **r_base);
Bone *ED_armature_pick_bone_from_selectbuffer(blender::Span<Base *> bases,
                                              const GPUSelectResult *hit_results,
                                              int hits,
                                              bool findunsel,
                                              bool do_nearest,
                                              Base **r_base);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Iteration
 * \{ */

/**
 * XXX: bone_looper is only to be used when we want to access settings
 * (i.e. editability/visibility/selected) that context doesn't offer.
 */
int bone_looper(Object *ob, Bone *bone, void *data, int (*bone_func)(Object *, Bone *, void *));

/** \} */
