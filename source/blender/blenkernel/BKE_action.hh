/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 * \brief Blender kernel action and pose functionality.
 */

#include "BLI_compiler_attrs.h"
#include "BLI_function_ref.hh"
#include "BLI_span.hh"

struct BlendDataReader;
struct BlendLibReader;
struct BlendWriter;
struct bArmature;
struct BoneParentTransform;

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

namespace blender::animrig {

/**
 * Action slot handle type.
 *
 * An identifier of slots within an action that is guaranteed to be unique
 * within that action and is guaranteed not to change for a slot.
 *
 * NOTE: keep this type in sync with `ActionSlot::handle` in the action DNA
 * types. We redefine it here rather than making a type alias to avoid bringing
 * in the entirety of DNA_action_types.h for everything that includes this
 * header.
 *
 * \see `ActionSlot::handle`
 */
using slot_handle_t = int32_t;

}  // namespace blender::animrig

/* Action Lib Stuff ----------------- */

/* Allocate a new bAction with the given name */
bAction *BKE_action_add(Main *bmain, const char name[]);

/* Action API ----------------- */

/**
 * Remove all fcurves from the action.
 *
 * \note This function only supports legacy Actions.
 */
void BKE_action_fcurves_clear(bAction *act);

/* Action Groups API ----------------- */

/**
 * Get the active action-group for an Action.
 *
 * \note This function supports both legacy and layered Actions.
 */
bActionGroup *get_active_actiongroup(bAction *act) ATTR_WARN_UNUSED_RESULT;

/**
 * Make the given Action-Group the active one.
 *
 * \note This function supports both legacy and layered Actions.
 */
void set_active_action_group(bAction *act, bActionGroup *agrp, short select);

/**
 * Sync colors used for action/bone group with theme settings.
 */
void action_group_colors_sync(bActionGroup *grp, const bActionGroup *ref_grp);

/**
 * Set colors used on this action group.
 */
void action_group_colors_set(bActionGroup *grp, const BoneColor *color);

/**
 * Set colors used on this action group, using the color of the pose bone.
 *
 * If `pchan->color` is set to a non-default color, that is used. Otherwise the
 * armature bone color is used.
 *
 * Note that if `pchan->bone` is `nullptr`, this function silently does nothing.
 */
void action_group_colors_set_from_posebone(bActionGroup *grp, const bPoseChannel *pchan);

/**
 * Add a new action group with the given name to the action>
 *
 * \note This function ONLY works on legacy Actions, not on layered Actions.
 */
bActionGroup *action_groups_add_new(bAction *act, const char name[]);

/**
 * Add given channel into (active) group
 * - assumes that channel is not linked to anything anymore
 * - always adds at the end of the group
 *
 * \note This function ONLY works on legacy Actions, not on layered Actions.
 */
void action_groups_add_channel(bAction *act, bActionGroup *agrp, FCurve *fcurve);

/**
 * Remove the given channel from all groups.
 *
 * \note This function ONLY works on legacy Actions, not on layered Actions.
 */
void action_groups_remove_channel(bAction *act, FCurve *fcu);

/**
 * Reconstruct channel pointers.
 * Assumes that the groups referred to by the FCurves are already in act->groups.
 * Reorders the main channel list to match group order.
 *
 * \note This function ONLY works on legacy Actions, not on layered Actions.
 */
void BKE_action_groups_reconstruct(bAction *act);

/**
 * Find a group with the given name.
 *
 * \note This function supports only legacy Actions.
 */
bActionGroup *BKE_action_group_find_name(bAction *act, const char name[]);

/**
 * Clear all 'temp' flags on all groups.
 *
 * \note This function supports both legacy and layered Actions.
 */
void action_groups_clear_tempflags(bAction *act);

/* Pose API ----------------- */

void BKE_pose_channel_free(bPoseChannel *pchan) ATTR_NONNULL(1);
/**
 * Deallocates a pose channel.
 * Does not free the pose channel itself.
 */
void BKE_pose_channel_free_ex(bPoseChannel *pchan, bool do_id_user) ATTR_NONNULL(1);

/**
 * Clears the runtime cache of a pose channel without free.
 */
void BKE_pose_channel_runtime_reset(bPoseChannel_Runtime *runtime) ATTR_NONNULL(1);
/**
 * Reset all non-persistent fields.
 */
void BKE_pose_channel_runtime_reset_on_copy(bPoseChannel_Runtime *runtime) ATTR_NONNULL(1);

/**
 * Deallocates runtime cache of a pose channel
 */
void BKE_pose_channel_runtime_free(bPoseChannel_Runtime *runtime) ATTR_NONNULL(1);

/**
 * Deallocates runtime cache of a pose channel's B-Bone shape.
 */
void BKE_pose_channel_free_bbone_cache(bPoseChannel_Runtime *runtime) ATTR_NONNULL(1);

void BKE_pose_channels_free(bPose *pose) ATTR_NONNULL(1);
/**
 * Removes and deallocates all channels from a pose.
 * Does not free the pose itself.
 */
void BKE_pose_channels_free_ex(bPose *pose, bool do_id_user) ATTR_NONNULL(1);

/**
 * Removes the hash for quick lookup of channels, must be done when adding/removing channels.
 */
void BKE_pose_channels_hash_ensure(bPose *pose) ATTR_NONNULL(1);
void BKE_pose_channels_hash_free(bPose *pose) ATTR_NONNULL(1);

/**
 * Selectively remove pose channels.
 */
void BKE_pose_channels_remove(Object *ob,
                              bool (*filter_fn)(const char *bone_name, void *user_data),
                              void *user_data) ATTR_NONNULL(1, 2);

void BKE_pose_free_data_ex(bPose *pose, bool do_id_user) ATTR_NONNULL(1);
void BKE_pose_free_data(bPose *pose) ATTR_NONNULL(1);
void BKE_pose_free(bPose *pose);
/**
 * Removes and deallocates all data from a pose, and also frees the pose.
 */
void BKE_pose_free_ex(bPose *pose, bool do_id_user);
/**
 * Allocate a new pose on the heap, and copy the src pose and its channels
 * into the new pose. *dst is set to the newly allocated structure, and assumed to be NULL.
 *
 * \param dst: Should be freed already, makes entire duplicate.
 */
void BKE_pose_copy_data_ex(bPose **dst, const bPose *src, int flag, bool copy_constraints);
void BKE_pose_copy_data(bPose **dst, const bPose *src, bool copy_constraints);
/**
 * Copy the internal members of each pose channel including constraints
 * and ID-Props, used when duplicating bones in edit-mode.
 * (unlike copy_pose_channel_data which only does posing-related stuff).
 *
 * \note use when copying bones in edit-mode (on returned value from #BKE_pose_channel_ensure)
 */
void BKE_pose_channel_copy_data(bPoseChannel *pchan, const bPoseChannel *pchan_from);
void BKE_pose_channel_session_uid_generate(bPoseChannel *pchan);
/**
 * Return a pointer to the pose channel of the given name
 * from this pose.
 */
bPoseChannel *BKE_pose_channel_find_name(const bPose *pose, const char *name);
/**
 * Checks if the bone is on a visible bone collection
 *
 * \return true if on a visible layer, false otherwise.
 */
bool BKE_pose_is_bonecoll_visible(const bArmature *arm,
                                  const bPoseChannel *pchan) ATTR_WARN_UNUSED_RESULT;
/**
 * Find the active pose-channel for an object
 *
 * \param check_bonecoll: checks if the bone is on a visible bone collection (this might be skipped
 * (e.g. for "Show Active" from the Outliner).
 * \return #bPoseChannel if found or NULL.
 * \note #Object, not #bPose is used here, as we need info (collection/active bone) from Armature.
 */
bPoseChannel *BKE_pose_channel_active(Object *ob, bool check_bonecoll);
/**
 * Find the active pose-channel for an object if it is on a visible bone collection
 * (calls #BKE_pose_channel_active with check_bonecoll set to true)
 *
 * \return #bPoseChannel if found or NULL.
 * \note #Object, not #bPose is used here, as we need info (collection/active bone) from Armature.
 */
bPoseChannel *BKE_pose_channel_active_if_bonecoll_visible(Object *ob) ATTR_WARN_UNUSED_RESULT;
/**
 * Use this when detecting the "other selected bone",
 * when we have multiple armatures in pose mode.
 *
 * In this case the active-selected is an obvious choice when finding the target for a
 * constraint for eg. however from the users perspective the active pose bone of the
 * active object is the _real_ active bone, so any other non-active selected bone
 * is a candidate for being the other selected bone, see: #58447.
 */
bPoseChannel *BKE_pose_channel_active_or_first_selected(Object *ob) ATTR_WARN_UNUSED_RESULT;
/**
 * Looks to see if the channel with the given name already exists
 * in this pose - if not a new one is allocated and initialized.
 *
 * \note Use with care, not on Armature poses but for temporal ones.
 * \note (currently used for action constraints and in rebuild_pose).
 */
bPoseChannel *BKE_pose_channel_ensure(bPose *pose, const char *name) ATTR_NONNULL(2);
/**
 * \see #ED_armature_ebone_get_mirrored (edit-mode, matching function)
 */
bPoseChannel *BKE_pose_channel_get_mirrored(const bPose *pose,
                                            const char *name) ATTR_WARN_UNUSED_RESULT;

void BKE_pose_check_uids_unique_and_report(const bPose *pose);

#ifndef NDEBUG
bool BKE_pose_channels_is_valid(const bPose *pose) ATTR_WARN_UNUSED_RESULT;
#endif

/**
 * Checks for IK constraint, Spline IK, and also for Follow-Path constraint.
 * can do more constraints flags later. pose should be entirely OK.
 */
void BKE_pose_update_constraint_flags(bPose *pose) ATTR_NONNULL(1);

/**
 * Tag constraint flags for update.
 */
void BKE_pose_tag_update_constraint_flags(bPose *pose) ATTR_NONNULL(1);

/**
 * Return the name of structure pointed by `pose->ikparam`.
 */
const char *BKE_pose_ikparam_get_name(bPose *pose) ATTR_WARN_UNUSED_RESULT;

/**
 * Allocate and initialize `pose->ikparam` according to `pose->iksolver`.
 */
void BKE_pose_ikparam_init(bPose *pose) ATTR_NONNULL(1);

/**
 * Initialize a #bItasc structure with default value.
 */
void BKE_pose_itasc_init(bItasc *itasc);

/**
 * Checks if a bone is part of an IK chain or not.
 */
bool BKE_pose_channel_in_IK_chain(Object *ob, bPoseChannel *pchan);

/**
 * Get the transform location, accounting for POSE_TRANSFORM_AT_CUSTOM_TX.
 */
void BKE_pose_channel_transform_location(const bArmature *arm,
                                         const bPoseChannel *pose_bone,
                                         float r_pose_space_pivot[3]);

/**
 * Get the transform pose orientation, accounting for
 * POSE_TRANSFORM_AT_CUSTOM_TX.
 */
void BKE_pose_channel_transform_orientation(const bArmature *arm,
                                            const bPoseChannel *pose_bone,
                                            float r_pose_orientation[3][3]);

/* Bone Groups API --------------------- */

/**
 * Adds a new bone-group (name may be NULL).
 */
bActionGroup *BKE_pose_add_group(bPose *pose, const char *name) ATTR_NONNULL(1);

/**
 * Remove the given bone-group (expects 'virtual' index (+1 one, used by active_group etc.))
 * index might be invalid ( < 1), in which case it will be find from grp.
 */
void BKE_pose_remove_group(bPose *pose, bActionGroup *grp, int index) ATTR_NONNULL(1);
/**
 * Remove the indexed bone-group (expects 'virtual' index (+1 one, used by active_group etc.)).
 */
void BKE_pose_remove_group_index(bPose *pose, int index) ATTR_NONNULL(1);

/* Assorted Evaluation ----------------- */

/**
 * For the calculation of the effects of an Action at the given frame on an object
 * This is currently only used for the Action Constraint
 */
void what_does_obaction(Object *ob,
                        Object *workob,
                        bPose *pose,
                        bAction *act,
                        int32_t action_slot_handle,
                        char groupname[],
                        const AnimationEvalContext *anim_eval_context) ATTR_NONNULL(1, 2);

void BKE_pose_copy_pchan_result(bPoseChannel *pchanto, const bPoseChannel *pchanfrom)
    ATTR_NONNULL(1, 2);
/**
 * Both poses should be in sync.
 */
bool BKE_pose_copy_result(bPose *to, bPose *from);
/**
 * Zero the pose transforms for the entire pose or only for selected bones.
 */
void BKE_pose_rest(bPose *pose, bool selected_bones_only);

/**
 * Tag pose for recalculation. Also tag all related data to be recalculated.
 */
void BKE_pose_tag_recalc(Main *bmain, bPose *pose) ATTR_NONNULL(1, 2);

void BKE_pose_blend_write(BlendWriter *writer, bPose *pose) ATTR_NONNULL(1, 2);
void BKE_pose_blend_read_data(BlendDataReader *reader, ID *id_owner, bPose *pose)
    ATTR_NONNULL(1, 2);
void BKE_pose_blend_read_after_liblink(BlendLibReader *reader, Object *ob, bPose *pose)
    ATTR_NONNULL(1, 2);

/* `action_mirror.cc` */

/**
 * Flip the action so it can be applied as a mirror. Only data of slots that are related to the
 * given objects is mirrored.
 */
void BKE_action_flip_with_pose(bAction *act, blender::Span<Object *> objects) ATTR_NONNULL(1);

namespace blender::bke {

using FoundFCurveCallback = blender::FunctionRef<void(FCurve *fcurve, const char *bone_name)>;
using FoundFCurveCallbackConst =
    blender::FunctionRef<void(const FCurve *fcurve, const char *bone_name)>;

/**
 * Calls `callback` for every fcurve in an action slot that targets any bone.
 *
 * \param slot_handle: only FCurves from the given action slot are visited.
 */
void BKE_action_find_fcurves_with_bones(bAction *action,
                                        blender::animrig::slot_handle_t slot_handle,
                                        FoundFCurveCallback callback);
void BKE_action_find_fcurves_with_bones(const bAction *action,
                                        blender::animrig::slot_handle_t slot_handle,
                                        FoundFCurveCallbackConst callback);

};  // namespace blender::bke
