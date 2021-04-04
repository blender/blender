/*
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bke
 * \brief Blender kernel action and pose functionality.
 */

#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BlendDataReader;
struct BlendExpander;
struct BlendLibReader;
struct BlendWriter;
struct bArmature;

/* The following structures are defined in DNA_action_types.h, and DNA_anim_types.h */
struct AnimationEvalContext;
struct FCurve;
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

/* types of transforms applied to the given item
 * - these are the return flags for action_get_item_transforms()
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

/* Return flags indicating which transforms the given object/posechannel has
 * - if 'curves' is provided, a list of links to these curves are also returned
 *   whose nodes WILL NEED FREEING
 */
short action_get_item_transforms(struct bAction *act,
                                 struct Object *ob,
                                 struct bPoseChannel *pchan,
                                 ListBase *curves);

/* Some kind of bounding box operation on the action */
void calc_action_range(const struct bAction *act, float *start, float *end, short incl_modifiers);

/* Does action have any motion data at all? */
bool action_has_motion(const struct bAction *act);

/* Action Groups API ----------------- */

/* Get the active action-group for an Action */
struct bActionGroup *get_active_actiongroup(struct bAction *act);

/* Make the given Action Group the active one */
void set_active_action_group(struct bAction *act, struct bActionGroup *agrp, short select);

/* Sync colors used for action/bone group with theme settings */
void action_group_colors_sync(struct bActionGroup *grp, const struct bActionGroup *ref_grp);

/* Add a new action group with the given name to the action */
struct bActionGroup *action_groups_add_new(struct bAction *act, const char name[]);

/* Add given channel into (active) group  */
void action_groups_add_channel(struct bAction *act,
                               struct bActionGroup *agrp,
                               struct FCurve *fcurve);

/* Remove the given channel from all groups */
void action_groups_remove_channel(struct bAction *act, struct FCurve *fcu);

/* Reconstruct group channel pointers. */
void BKE_action_groups_reconstruct(struct bAction *act);

/* Find a group with the given name */
struct bActionGroup *BKE_action_group_find_name(struct bAction *act, const char name[]);

/* Clear all 'temp' flags on all groups */
void action_groups_clear_tempflags(struct bAction *act);

/* Pose API ----------------- */

void BKE_pose_channel_free(struct bPoseChannel *pchan);
void BKE_pose_channel_free_ex(struct bPoseChannel *pchan, bool do_id_user);

void BKE_pose_channel_runtime_reset(struct bPoseChannel_Runtime *runtime);
void BKE_pose_channel_runtime_reset_on_copy(struct bPoseChannel_Runtime *runtime);

void BKE_pose_channel_runtime_free(struct bPoseChannel_Runtime *runtime);

void BKE_pose_channel_free_bbone_cache(struct bPoseChannel_Runtime *runtime);

void BKE_pose_channels_free(struct bPose *pose);
void BKE_pose_channels_free_ex(struct bPose *pose, bool do_id_user);

void BKE_pose_channels_hash_make(struct bPose *pose);
void BKE_pose_channels_hash_free(struct bPose *pose);

void BKE_pose_channels_remove(struct Object *ob,
                              bool (*filter_fn)(const char *bone_name, void *user_data),
                              void *user_data);

void BKE_pose_free_data_ex(struct bPose *pose, bool do_id_user);
void BKE_pose_free_data(struct bPose *pose);
void BKE_pose_free(struct bPose *pose);
void BKE_pose_free_ex(struct bPose *pose, bool do_id_user);
void BKE_pose_copy_data_ex(struct bPose **dst,
                           const struct bPose *src,
                           const int flag,
                           const bool copy_constraints);
void BKE_pose_copy_data(struct bPose **dst, const struct bPose *src, const bool copy_constraints);
void BKE_pose_channel_copy_data(struct bPoseChannel *pchan, const struct bPoseChannel *pchan_from);
void BKE_pose_channel_session_uuid_generate(struct bPoseChannel *pchan);
struct bPoseChannel *BKE_pose_channel_find_name(const struct bPose *pose, const char *name);
struct bPoseChannel *BKE_pose_channel_active(struct Object *ob);
struct bPoseChannel *BKE_pose_channel_active_or_first_selected(struct Object *ob);
struct bPoseChannel *BKE_pose_channel_verify(struct bPose *pose, const char *name);
struct bPoseChannel *BKE_pose_channel_get_mirrored(const struct bPose *pose, const char *name);

void BKE_pose_check_uuids_unique_and_report(const struct bPose *pose);

#ifndef NDEBUG
bool BKE_pose_channels_is_valid(const struct bPose *pose);
#endif

/* sets constraint flags */
void BKE_pose_update_constraint_flags(struct bPose *pose);

/* tag constraint flags for update */
void BKE_pose_tag_update_constraint_flags(struct bPose *pose);

/* return the name of structure pointed by pose->ikparam */
const char *BKE_pose_ikparam_get_name(struct bPose *pose);

/* allocate and initialize pose->ikparam according to pose->iksolver */
void BKE_pose_ikparam_init(struct bPose *pose);

/* initialize a bItasc structure with default value */
void BKE_pose_itasc_init(struct bItasc *itasc);

/* Checks if a bone is part of an IK chain or not */
bool BKE_pose_channel_in_IK_chain(struct Object *ob, struct bPoseChannel *pchan);

/* Bone Groups API --------------------- */

/* Adds a new bone-group */
struct bActionGroup *BKE_pose_add_group(struct bPose *pose, const char *name);

/* Remove a bone-group */
void BKE_pose_remove_group(struct bPose *pose, struct bActionGroup *grp, const int index);
/* Remove the matching bone-group from its index */
void BKE_pose_remove_group_index(struct bPose *pose, const int index);

/* Assorted Evaluation ----------------- */

/* Used for the Action Constraint */
void what_does_obaction(struct Object *ob,
                        struct Object *workob,
                        struct bPose *pose,
                        struct bAction *act,
                        char groupname[],
                        const struct AnimationEvalContext *anim_eval_context);

/* for proxy */
void BKE_pose_copy_pchan_result(struct bPoseChannel *pchanto,
                                const struct bPoseChannel *pchanfrom);
bool BKE_pose_copy_result(struct bPose *to, struct bPose *from);
/* Clear transforms. */
void BKE_pose_rest(struct bPose *pose, bool selected_bones_only);

/* Tag pose for recalc. Also tag all related data to be recalc. */
void BKE_pose_tag_recalc(struct Main *bmain, struct bPose *pose);

void BKE_pose_blend_write(struct BlendWriter *writer, struct bPose *pose, struct bArmature *arm);
void BKE_pose_blend_read_data(struct BlendDataReader *reader, struct bPose *pose);
void BKE_pose_blend_read_lib(struct BlendLibReader *reader, struct Object *ob, struct bPose *pose);
void BKE_pose_blend_read_expand(struct BlendExpander *expander, struct bPose *pose);

/* action_mirror.c */
void BKE_action_flip_with_pose(struct bAction *act, struct Object *ob_arm);

#ifdef __cplusplus
};
#endif
