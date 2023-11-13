/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief Blender kernel action and pose functionality.
 */

#include "BLI_compiler_attrs.h"

#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BlendDataReader;
struct BlendLibReader;
struct BlendWriter;
struct bArmature;

/* The following structures are defined in DNA_action_types.h, and DNA_anim_types.h */
struct AnimationEvalContext;
struct BoneColor;
struct FCurve;
struct ID;
struct Main;
struct Object;
struct bAction;
struct bActionGroup;
struct bItasc;
struct bPose;
struct bPoseChannel;
struct bPoseChannel_Runtime;

/* Action Lib Stuff ----------------- */

/* Allocate a new bAction with the given name */
struct bAction *BKE_action_add(struct Main *bmain, const char name[]);

/* Action API ----------------- */

/**
 * Types of transforms applied to the given item:
 * - these are the return flags for #BKE_action_get_item_transform_flags()
 */
typedef enum eAction_TransformFlags {
  /* location */
  ACT_TRANS_LOC = (1 << 0),
  /* rotation */
  ACT_TRANS_ROT = (1 << 1),
  /* scaling */
  ACT_TRANS_SCALE = (1 << 2),

  /* bbone shape - for all the parameters, provided one is set */
  ACT_TRANS_BBONE = (1 << 3),

  /* strictly not a transform, but custom properties are also
   * quite often used in modern rigs
   */
  ACT_TRANS_PROP = (1 << 4),

  /* all flags */
  ACT_TRANS_ONLY = (ACT_TRANS_LOC | ACT_TRANS_ROT | ACT_TRANS_SCALE),
  ACT_TRANS_ALL = (ACT_TRANS_ONLY | ACT_TRANS_PROP),
} eAction_TransformFlags;

/**
 * Return flags indicating which transforms the given object/posechannel has
 * - if 'curves' is provided, a list of links to these curves are also returned
 *   whose nodes WILL NEED FREEING.
 */
eAction_TransformFlags BKE_action_get_item_transform_flags(struct bAction *act,
                                                           struct Object *ob,
                                                           struct bPoseChannel *pchan,
                                                           ListBase *curves)
    ATTR_WARN_UNUSED_RESULT;

/**
 * Calculate the extents of given action.
 */
void BKE_action_frame_range_calc(const struct bAction *act,
                                 bool include_modifiers,
                                 float *r_start,
                                 float *r_end) ATTR_NONNULL(3, 4);

/**
 * Retrieve the intended playback frame range, using the manually set range if available,
 * or falling back to scanning F-Curves for their first & last frames otherwise.
 */
void BKE_action_frame_range_get(const struct bAction *act, float *r_start, float *r_end)
    ATTR_NONNULL(2, 3);

/**
 * Check if the given action has any keyframes.
 */
bool BKE_action_has_motion(const struct bAction *act) ATTR_WARN_UNUSED_RESULT;

/**
 * Is the action configured as cyclic.
 */
bool BKE_action_is_cyclic(const struct bAction *act) ATTR_WARN_UNUSED_RESULT;

/**
 * Remove all fcurves from the action.
 */
void BKE_action_fcurves_clear(struct bAction *act);

/* Action Groups API ----------------- */

/**
 * Get the active action-group for an Action.
 */
struct bActionGroup *get_active_actiongroup(struct bAction *act) ATTR_WARN_UNUSED_RESULT;

/**
 * Make the given Action-Group the active one.
 */
void set_active_action_group(struct bAction *act, struct bActionGroup *agrp, short select);

/**
 * Sync colors used for action/bone group with theme settings.
 */
void action_group_colors_sync(struct bActionGroup *grp, const struct bActionGroup *ref_grp);

/**
 * Set colors used on this action group.
 */
void action_group_colors_set(struct bActionGroup *grp, const struct BoneColor *color);

/**
 * Set colors used on this action group, using the color of the pose bone.
 *
 * If `pchan->color` is set to a non-default color, that is used. Otherwise the
 * armature bone color is used.
 */
void action_group_colors_set_from_posebone(bActionGroup *grp, const bPoseChannel *pchan);

/**
 * Add a new action group with the given name to the action>
 */
struct bActionGroup *action_groups_add_new(struct bAction *act, const char name[]);

/**
 * Add given channel into (active) group
 * - assumes that channel is not linked to anything anymore
 * - always adds at the end of the group
 */
void action_groups_add_channel(struct bAction *act,
                               struct bActionGroup *agrp,
                               struct FCurve *fcurve);

/**
 * Remove the given channel from all groups.
 */
void action_groups_remove_channel(struct bAction *act, struct FCurve *fcu);

/**
 * Reconstruct group channel pointers.
 * Assumes that the groups referred to by the FCurves are already in act->groups.
 * Reorders the main channel list to match group order.
 */
void BKE_action_groups_reconstruct(struct bAction *act);

/**
 * Find a group with the given name.
 */
struct bActionGroup *BKE_action_group_find_name(struct bAction *act, const char name[]);

/**
 * Clear all 'temp' flags on all groups.
 */
void action_groups_clear_tempflags(struct bAction *act);

/**
 * Return whether the action has one unique point in time keyed.
 *
 * This is mostly for the pose library, which will have different behavior depending on whether an
 * Action corresponds to a "pose" (one keyframe) or "animation snippet" (multiple keyframes).
 *
 * \return `false` when there is no keyframe at all or keys on different points in time, `true`
 * when exactly one point in time is keyed.
 */
bool BKE_action_has_single_frame(const struct bAction *act) ATTR_WARN_UNUSED_RESULT;

/* Pose API ----------------- */

void BKE_pose_channel_free(struct bPoseChannel *pchan) ATTR_NONNULL(1);
/**
 * Deallocates a pose channel.
 * Does not free the pose channel itself.
 */
void BKE_pose_channel_free_ex(struct bPoseChannel *pchan, bool do_id_user) ATTR_NONNULL(1);

/**
 * Clears the runtime cache of a pose channel without free.
 */
void BKE_pose_channel_runtime_reset(struct bPoseChannel_Runtime *runtime) ATTR_NONNULL(1);
/**
 * Reset all non-persistent fields.
 */
void BKE_pose_channel_runtime_reset_on_copy(struct bPoseChannel_Runtime *runtime) ATTR_NONNULL(1);

/**
 * Deallocates runtime cache of a pose channel
 */
void BKE_pose_channel_runtime_free(struct bPoseChannel_Runtime *runtime) ATTR_NONNULL(1);

/**
 * Deallocates runtime cache of a pose channel's B-Bone shape.
 */
void BKE_pose_channel_free_bbone_cache(struct bPoseChannel_Runtime *runtime) ATTR_NONNULL(1);

void BKE_pose_channels_free(struct bPose *pose) ATTR_NONNULL(1);
/**
 * Removes and deallocates all channels from a pose.
 * Does not free the pose itself.
 */
void BKE_pose_channels_free_ex(struct bPose *pose, bool do_id_user) ATTR_NONNULL(1);

/**
 * Removes the hash for quick lookup of channels, must be done when adding/removing channels.
 */
void BKE_pose_channels_hash_ensure(struct bPose *pose) ATTR_NONNULL(1);
void BKE_pose_channels_hash_free(struct bPose *pose) ATTR_NONNULL(1);

/**
 * Selectively remove pose channels.
 */
void BKE_pose_channels_remove(struct Object *ob,
                              bool (*filter_fn)(const char *bone_name, void *user_data),
                              void *user_data) ATTR_NONNULL(1, 2);

void BKE_pose_free_data_ex(struct bPose *pose, bool do_id_user) ATTR_NONNULL(1);
void BKE_pose_free_data(struct bPose *pose) ATTR_NONNULL(1);
void BKE_pose_free(struct bPose *pose);
/**
 * Removes and deallocates all data from a pose, and also frees the pose.
 */
void BKE_pose_free_ex(struct bPose *pose, bool do_id_user);
/**
 * Allocate a new pose on the heap, and copy the src pose and its channels
 * into the new pose. *dst is set to the newly allocated structure, and assumed to be NULL.
 *
 * \param dst: Should be freed already, makes entire duplicate.
 */
void BKE_pose_copy_data_ex(struct bPose **dst,
                           const struct bPose *src,
                           int flag,
                           bool copy_constraints);
void BKE_pose_copy_data(struct bPose **dst, const struct bPose *src, bool copy_constraints);
/**
 * Copy the internal members of each pose channel including constraints
 * and ID-Props, used when duplicating bones in edit-mode.
 * (unlike copy_pose_channel_data which only does posing-related stuff).
 *
 * \note use when copying bones in edit-mode (on returned value from #BKE_pose_channel_ensure)
 */
void BKE_pose_channel_copy_data(struct bPoseChannel *pchan, const struct bPoseChannel *pchan_from);
void BKE_pose_channel_session_uuid_generate(struct bPoseChannel *pchan);
/**
 * Return a pointer to the pose channel of the given name
 * from this pose.
 */
struct bPoseChannel *BKE_pose_channel_find_name(const struct bPose *pose, const char *name);
/**
 * Checks if the bone is on a visible bone collection
 *
 * \return true if on a visible layer, false otherwise.
 */
bool BKE_pose_is_bonecoll_visible(const struct bArmature *arm,
                                  const struct bPoseChannel *pchan) ATTR_WARN_UNUSED_RESULT;
/**
 * Find the active pose-channel for an object
 *
 * \param check_bonecoll: checks if the bone is on a visible bone collection (this might be skipped
 * (e.g. for "Show Active" from the Outliner).
 * \return #bPoseChannel if found or NULL.
 * \note #Object, not #bPose is used here, as we need info (collection/active bone) from Armature.
 */
struct bPoseChannel *BKE_pose_channel_active(struct Object *ob, bool check_bonecoll);
/**
 * Find the active pose-channel for an object if it is on a visible bone collection
 * (calls #BKE_pose_channel_active with check_bonecoll set to true)
 *
 * \return #bPoseChannel if found or NULL.
 * \note #Object, not #bPose is used here, as we need info (collection/active bone) from Armature.
 */
struct bPoseChannel *BKE_pose_channel_active_if_bonecoll_visible(struct Object *ob)
    ATTR_WARN_UNUSED_RESULT;
/**
 * Use this when detecting the "other selected bone",
 * when we have multiple armatures in pose mode.
 *
 * In this case the active-selected is an obvious choice when finding the target for a
 * constraint for eg. however from the users perspective the active pose bone of the
 * active object is the _real_ active bone, so any other non-active selected bone
 * is a candidate for being the other selected bone, see: #58447.
 */
struct bPoseChannel *BKE_pose_channel_active_or_first_selected(struct Object *ob)
    ATTR_WARN_UNUSED_RESULT;
/**
 * Looks to see if the channel with the given name already exists
 * in this pose - if not a new one is allocated and initialized.
 *
 * \note Use with care, not on Armature poses but for temporal ones.
 * \note (currently used for action constraints and in rebuild_pose).
 */
struct bPoseChannel *BKE_pose_channel_ensure(struct bPose *pose, const char *name) ATTR_NONNULL(2);
/**
 * \see #ED_armature_ebone_get_mirrored (edit-mode, matching function)
 */
struct bPoseChannel *BKE_pose_channel_get_mirrored(const struct bPose *pose,
                                                   const char *name) ATTR_WARN_UNUSED_RESULT;

void BKE_pose_check_uuids_unique_and_report(const struct bPose *pose);

#ifndef NDEBUG
bool BKE_pose_channels_is_valid(const struct bPose *pose) ATTR_WARN_UNUSED_RESULT;
#endif

/**
 * Checks for IK constraint, Spline IK, and also for Follow-Path constraint.
 * can do more constraints flags later. pose should be entirely OK.
 */
void BKE_pose_update_constraint_flags(struct bPose *pose) ATTR_NONNULL(1);

/**
 * Tag constraint flags for update.
 */
void BKE_pose_tag_update_constraint_flags(struct bPose *pose) ATTR_NONNULL(1);

/**
 * Return the name of structure pointed by `pose->ikparam`.
 */
const char *BKE_pose_ikparam_get_name(struct bPose *pose) ATTR_WARN_UNUSED_RESULT;

/**
 * Allocate and initialize `pose->ikparam` according to `pose->iksolver`.
 */
void BKE_pose_ikparam_init(struct bPose *pose) ATTR_NONNULL(1);

/**
 * Initialize a #bItasc structure with default value.
 */
void BKE_pose_itasc_init(struct bItasc *itasc);

/**
 * Checks if a bone is part of an IK chain or not.
 */
bool BKE_pose_channel_in_IK_chain(struct Object *ob, struct bPoseChannel *pchan);

/* Bone Groups API --------------------- */

/**
 * Adds a new bone-group (name may be NULL).
 */
struct bActionGroup *BKE_pose_add_group(struct bPose *pose, const char *name) ATTR_NONNULL(1);

/**
 * Remove the given bone-group (expects 'virtual' index (+1 one, used by active_group etc.))
 * index might be invalid ( < 1), in which case it will be find from grp.
 */
void BKE_pose_remove_group(struct bPose *pose, struct bActionGroup *grp, int index)
    ATTR_NONNULL(1);
/**
 * Remove the indexed bone-group (expects 'virtual' index (+1 one, used by active_group etc.)).
 */
void BKE_pose_remove_group_index(struct bPose *pose, int index) ATTR_NONNULL(1);

/* Assorted Evaluation ----------------- */

/**
 * For the calculation of the effects of an Action at the given frame on an object
 * This is currently only used for the Action Constraint
 */
void what_does_obaction(struct Object *ob,
                        struct Object *workob,
                        struct bPose *pose,
                        struct bAction *act,
                        char groupname[],
                        const struct AnimationEvalContext *anim_eval_context) ATTR_NONNULL(1, 2);

void BKE_pose_copy_pchan_result(struct bPoseChannel *pchanto, const struct bPoseChannel *pchanfrom)
    ATTR_NONNULL(1, 2);
/**
 * Both poses should be in sync.
 */
bool BKE_pose_copy_result(struct bPose *to, struct bPose *from);
/**
 * Zero the pose transforms for the entire pose or only for selected bones.
 */
void BKE_pose_rest(struct bPose *pose, bool selected_bones_only);

/**
 * Tag pose for recalculation. Also tag all related data to be recalculated.
 */
void BKE_pose_tag_recalc(struct Main *bmain, struct bPose *pose) ATTR_NONNULL(1, 2);

void BKE_pose_blend_write(struct BlendWriter *writer, struct bPose *pose, struct bArmature *arm)
    ATTR_NONNULL(1, 2, 3);
void BKE_pose_blend_read_data(struct BlendDataReader *reader,
                              struct ID *id_owner,
                              struct bPose *pose) ATTR_NONNULL(1, 2);
void BKE_pose_blend_read_after_liblink(struct BlendLibReader *reader,
                                       struct Object *ob,
                                       struct bPose *pose) ATTR_NONNULL(1, 2);

/* `action_mirror.cc` */

void BKE_action_flip_with_pose(struct bAction *act, struct Object *ob_arm) ATTR_NONNULL(1, 2);

#ifdef __cplusplus
};
#endif
