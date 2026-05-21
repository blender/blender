/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 */

#pragma once

#include "DNA_armature_types.h"
#include "DNA_listBase.h"

#include "RNA_types.hh"

#include "BLI_span.hh"

#include "ED_anim_transformable.hh"

namespace blender {

struct Base;
struct Bone;
struct EditBone;
struct GPUSelectResult;
struct IDProperty;
struct LinkData;
struct Object;
struct Scene;
struct bArmature;
struct bContext;
struct bPoseChannel;
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
void ARMATURE_OT_duplicate_rename(wmOperatorType *ot);
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

/**
 * Types of transforms to apply to a tPchanFCurveLink.
 */
enum eAction_TransformFlags {
  ACT_TRANS_LOC = (1 << 0),
  ACT_TRANS_ROT = (1 << 1),
  ACT_TRANS_SCALE = (1 << 2),

  /* BBone shape - for all the parameters, provided one is set. */
  ACT_TRANS_BBONE = (1 << 3),
  ACT_TRANS_PROP = (1 << 4),

  ACT_TRANS_ONLY = (ACT_TRANS_LOC | ACT_TRANS_ROT | ACT_TRANS_SCALE),
  ACT_TRANS_ALL = (ACT_TRANS_ONLY | ACT_TRANS_PROP),
};

/* Stores values of an RNA property for use at a later date. */
struct PropertySnapshot {
  PropertyRNA *property;
  /* Non-float properties are also stored as float. The length of the array matches the length of
   * the property. */
  Array<float> values;
};

/* Temporary struct wrapping data used for pose sliding. */
struct SlideSubject {
  SlideSubject *next, *prev;

  /** F-Curves for this PoseChannel (wrapped with LinkData) */
  /** The AnimTransformable which the data is attached to */
  ed::AnimTransformable *transformable;
  /* A pointer to the data represented by this link. */
  PointerRNA ptr;
  /** F-Curves for this AnimTransformable. */
  Vector<FCurve *> fcurves;
  /* This is used as an optimization to only do blending on transform types that actually have
   * animation. */
  eAction_TransformFlags transform_flag;

  /** Transform values at start of operator (to be restored before each modal step). */
  ed::TransformFloats old_loc;
  ed::Rotation old_rot;
  ed::TransformFloats old_scale;

  /* Additional properties of the transformable to affect which are not custom properties. Bones
   * use this to store bbone data, e.g. `bbone_rollin`. */
  Vector<PropertySnapshot> additional_properties;

  /* Custom properties defined via the UI. See ID::properties. */
  Vector<PropertySnapshot> properties;
  /* User defined properties through addons. See ID::system_properties. */
  Vector<PropertySnapshot> system_properties;
};

/* ----------- */

/**
 * Build up a list of SlideSubject. The items put into the list depend on the mode of
 * the context.
 */
void slide_subjects_get(bContext *C, ListBaseT<SlideSubject> *slide_subjects);
/** Free all slide subjects. */
void slide_subjects_free(ListBaseT<SlideSubject> *slide_subjects);

/**
 * Helper for apply() / reset() - refresh the data.
 */
void slide_subjects_refresh(bContext *C, ID *id);
/**
 * Reset changes made to current slide subjects back to their stored values.
 */
void slide_subjects_reset(ListBaseT<SlideSubject> *slide_subjects);
/** Perform auto-key-framing after changes were made + confirmed. */
void slide_subjects_autokey(bContext *C,
                            Scene *scene,
                            const ListBaseT<SlideSubject> *slide_subjects,
                            float cframe);

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

EditBone *make_boneList(ListBaseT<EditBone> *edbo, ListBaseT<Bone> *bones, Bone *actBone);

/* Duplicate method. */

EditBone *duplicateEditBone(EditBone *cur_bone,
                            const char *name,
                            ListBaseT<EditBone> *editbones,
                            Object *ob);

/* Duplicate method (cross objects). */

/**
 * \param editbones: The target list.
 */
EditBone *duplicateEditBoneObjects(EditBone *cur_bone,
                                   const char *name,
                                   ListBaseT<EditBone> *editbones,
                                   Object *src_ob,
                                   Object *dst_ob);

/** Adds an EditBone between the nominated locations (should be in the right space). */
EditBone *add_points_bone(Object *obedit, float head[3], float tail[3]);
void bone_free(bArmature *arm, EditBone *bone);

void armature_tag_select_mirrored(bArmature *arm);
/**
 * Helper function for tools to work on mirrored parts.
 * it leaves mirrored bones selected then too, which is a good indication of what happened.
 */
void armature_select_mirrored_ex(bArmature *arm, eBone_Flag flag);
void armature_select_mirrored(bArmature *arm);
/** Only works when tagged. */
void armature_tag_unselect(bArmature *arm);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Selection Picking
 * \{ */

EditBone *ED_armature_pick_ebone_from_selectbuffer(Span<Base *> bases,
                                                   const GPUSelectResult *hit_results,
                                                   int hits,
                                                   bool findunsel,
                                                   bool do_nearest,
                                                   Base **r_base);
bPoseChannel *ED_armature_pick_pchan_from_selectbuffer(Span<Base *> bases,
                                                       const GPUSelectResult *hit_results,
                                                       int hits,
                                                       bool findunsel,
                                                       bool do_nearest,
                                                       Base **r_base);
Bone *ED_armature_pick_bone_from_selectbuffer(Span<Base *> bases,
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

}  // namespace blender
