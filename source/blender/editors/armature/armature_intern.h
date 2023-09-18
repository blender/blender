/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* internal exports only */
struct wmOperatorType;

struct Base;
struct GPUSelectResult;
struct Object;
struct Scene;
struct bContext;
struct bPoseChannel;

struct Bone;
struct EditBone;
struct bArmature;

struct LinkData;
struct ListBase;

/* -------------------------------------------------------------------- */
/** \name Armature EditMode Operators
 * \{ */

void ARMATURE_OT_bone_primitive_add(struct wmOperatorType *ot);

void ARMATURE_OT_align(struct wmOperatorType *ot);
void ARMATURE_OT_calculate_roll(struct wmOperatorType *ot);
void ARMATURE_OT_roll_clear(struct wmOperatorType *ot);
void ARMATURE_OT_switch_direction(struct wmOperatorType *ot);

void ARMATURE_OT_subdivide(struct wmOperatorType *ot);

void ARMATURE_OT_parent_set(struct wmOperatorType *ot);
void ARMATURE_OT_parent_clear(struct wmOperatorType *ot);

void ARMATURE_OT_select_all(struct wmOperatorType *ot);
void ARMATURE_OT_select_mirror(struct wmOperatorType *ot);
void ARMATURE_OT_select_more(struct wmOperatorType *ot);
void ARMATURE_OT_select_less(struct wmOperatorType *ot);
void ARMATURE_OT_select_hierarchy(struct wmOperatorType *ot);
void ARMATURE_OT_select_linked_pick(struct wmOperatorType *ot);
void ARMATURE_OT_select_linked(struct wmOperatorType *ot);
void ARMATURE_OT_select_similar(struct wmOperatorType *ot);
void ARMATURE_OT_shortest_path_pick(struct wmOperatorType *ot);

void ARMATURE_OT_delete(struct wmOperatorType *ot);
void ARMATURE_OT_dissolve(struct wmOperatorType *ot);
void ARMATURE_OT_duplicate(struct wmOperatorType *ot);
void ARMATURE_OT_symmetrize(struct wmOperatorType *ot);
void ARMATURE_OT_extrude(struct wmOperatorType *ot);
void ARMATURE_OT_hide(struct wmOperatorType *ot);
void ARMATURE_OT_reveal(struct wmOperatorType *ot);
void ARMATURE_OT_click_extrude(struct wmOperatorType *ot);
void ARMATURE_OT_fill(struct wmOperatorType *ot);
void ARMATURE_OT_separate(struct wmOperatorType *ot);
void ARMATURE_OT_split(struct wmOperatorType *ot);

void ARMATURE_OT_autoside_names(struct wmOperatorType *ot);
void ARMATURE_OT_flip_names(struct wmOperatorType *ot);

void ARMATURE_OT_layers_show_all(struct wmOperatorType *ot);
void ARMATURE_OT_armature_layers(struct wmOperatorType *ot);
void ARMATURE_OT_bone_layers(struct wmOperatorType *ot);

void ARMATURE_OT_collection_add(struct wmOperatorType *ot);
void ARMATURE_OT_collection_remove(struct wmOperatorType *ot);
void ARMATURE_OT_collection_move(struct wmOperatorType *ot);
void ARMATURE_OT_collection_assign(struct wmOperatorType *ot);
void ARMATURE_OT_collection_unassign(struct wmOperatorType *ot);
void ARMATURE_OT_collection_select(struct wmOperatorType *ot);
void ARMATURE_OT_collection_deselect(struct wmOperatorType *ot);

void ARMATURE_OT_move_to_collection(struct wmOperatorType *ot);
void ARMATURE_OT_assign_to_collection(struct wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pose-Mode Operators
 * \{ */

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
void POSE_OT_select_linked_pick(struct wmOperatorType *ot);
void POSE_OT_select_constraint_target(struct wmOperatorType *ot);
void POSE_OT_select_grouped(struct wmOperatorType *ot);
void POSE_OT_select_mirror(struct wmOperatorType *ot);

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
void POSE_OT_paths_range_update(struct wmOperatorType *ot);

void POSE_OT_autoside_names(struct wmOperatorType *ot);
void POSE_OT_flip_names(struct wmOperatorType *ot);

void POSE_OT_rotation_mode_set(struct wmOperatorType *ot);

void POSE_OT_quaternions_flip(struct wmOperatorType *ot);

void POSE_OT_bone_layers(struct wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pose Tool Utilities (for PoseLib, Pose Sliding, etc.)
 * \{ */

/* `pose_utils.cc` */

/* Temporary data linking PoseChannels with the F-Curves they affect */
typedef struct tPChanFCurveLink {
  struct tPChanFCurveLink *next, *prev;

  /** Object this Pose Channel belongs to. */
  struct Object *ob;

  /** F-Curves for this PoseChannel (wrapped with LinkData) */
  ListBase fcurves;
  /** Pose Channel which data is attached to */
  struct bPoseChannel *pchan;

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
  struct IDProperty *oldprops;
} tPChanFCurveLink;

/* ----------- */

/** Returns a valid pose armature for this object, else returns NULL. */
struct Object *poseAnim_object_get(struct Object *ob_);
/** Get sets of F-Curves providing transforms for the bones in the Pose. */
void poseAnim_mapping_get(struct bContext *C, ListBase *pfLinks);
/** Free F-Curve <-> PoseChannel links. */
void poseAnim_mapping_free(ListBase *pfLinks);

/**
 * Helper for apply() / reset() - refresh the data.
 */
void poseAnim_mapping_refresh(struct bContext *C, struct Scene *scene, struct Object *ob);
/**
 * Reset changes made to current pose.
 */
void poseAnim_mapping_reset(ListBase *pfLinks);
/** Perform auto-key-framing after changes were made + confirmed. */
void poseAnim_mapping_autoKeyframe(struct bContext *C,
                                   struct Scene *scene,
                                   ListBase *pfLinks,
                                   float cframe);

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

void POSELIB_OT_apply_pose_asset(struct wmOperatorType *ot);
void POSELIB_OT_blend_pose_asset(struct wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pose Sliding Tools
 * \{ */

/* `pose_slide.cc` */

void POSE_OT_push(struct wmOperatorType *ot);
void POSE_OT_relax(struct wmOperatorType *ot);
void POSE_OT_blend_with_rest(struct wmOperatorType *ot);
void POSE_OT_breakdown(struct wmOperatorType *ot);
void POSE_OT_blend_to_neighbors(struct wmOperatorType *ot);

void POSE_OT_propagate(struct wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Various Armature Edit/Pose Editing API's
 * \{ */

/* Ideally, many of these defines would not be needed as everything would be strictly
 * self-contained within each file,
 * but some tools still have a bit of overlap which makes things messy -- Feb 2013
 */

struct EditBone *make_boneList(struct ListBase *edbo,
                               struct ListBase *bones,
                               struct Bone *actBone);

/* Duplicate method. */

/** Call this before doing any duplications. */
void preEditBoneDuplicate(struct ListBase *editbones);
void postEditBoneDuplicate(struct ListBase *editbones, struct Object *ob);
struct EditBone *duplicateEditBone(struct EditBone *cur_bone,
                                   const char *name,
                                   struct ListBase *editbones,
                                   struct Object *ob);

/* Duplicate method (cross objects). */

/**
 * \param editbones: The target list.
 */
struct EditBone *duplicateEditBoneObjects(struct EditBone *cur_bone,
                                          const char *name,
                                          struct ListBase *editbones,
                                          struct Object *src_ob,
                                          struct Object *dst_ob);

/** Adds an EditBone between the nominated locations (should be in the right space). */
struct EditBone *add_points_bone(struct Object *obedit, float head[3], float tail[3]);
void bone_free(struct bArmature *arm, struct EditBone *bone);

void armature_tag_select_mirrored(struct bArmature *arm);
/**
 * Helper function for tools to work on mirrored parts.
 * it leaves mirrored bones selected then too, which is a good indication of what happened.
 */
void armature_select_mirrored_ex(struct bArmature *arm, int flag);
void armature_select_mirrored(struct bArmature *arm);
/** Only works when tagged. */
void armature_tag_unselect(struct bArmature *arm);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Selection Picking
 * \{ */

struct EditBone *ED_armature_pick_ebone(struct bContext *C,
                                        const int xy[2],
                                        bool findunsel,
                                        struct Base **r_base);
struct bPoseChannel *ED_armature_pick_pchan(struct bContext *C,
                                            const int xy[2],
                                            bool findunsel,
                                            struct Base **r_base);
struct Bone *ED_armature_pick_bone(struct bContext *C,
                                   const int xy[2],
                                   bool findunsel,
                                   struct Base **r_base);

struct EditBone *ED_armature_pick_ebone_from_selectbuffer(struct Base **bases,
                                                          uint bases_len,
                                                          const struct GPUSelectResult *buffer,
                                                          short hits,
                                                          bool findunsel,
                                                          bool do_nearest,
                                                          struct Base **r_base);
struct bPoseChannel *ED_armature_pick_pchan_from_selectbuffer(struct Base **bases,
                                                              uint bases_len,
                                                              const struct GPUSelectResult *buffer,
                                                              short hits,
                                                              bool findunsel,
                                                              bool do_nearest,
                                                              struct Base **r_base);
struct Bone *ED_armature_pick_bone_from_selectbuffer(struct Base **bases,
                                                     uint bases_len,
                                                     const struct GPUSelectResult *buffer,
                                                     short hits,
                                                     bool findunsel,
                                                     bool do_nearest,
                                                     struct Base **r_base);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Iteration
 * \{ */

/**
 * XXX: bone_looper is only to be used when we want to access settings
 * (i.e. editability/visibility/selected) that context doesn't offer.
 */
int bone_looper(struct Object *ob,
                struct Bone *bone,
                void *data,
                int (*bone_func)(struct Object *, struct Bone *, void *));

/** \} */

#ifdef __cplusplus
}
#endif
