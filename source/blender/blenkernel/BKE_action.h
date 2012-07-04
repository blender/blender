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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Full recode, Ton Roosendaal, Crete 2005
 *				 Full recode, Joshua Leung, 2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_ACTION_H__
#define __BKE_ACTION_H__
/** \file BKE_action.h
 *  \ingroup bke
 *  \brief Blender kernel action and pose functionality.
 *  \author Reevan McKay
 *  \author Ton Roosendaal (full recode 2005)
 *  \author Joshua Leung (full recode 2009)
 *  \since may 2001
 */

#include "DNA_listBase.h"

/* The following structures are defined in DNA_action_types.h, and DNA_anim_types.h */
struct bAction;
struct bActionGroup;
struct FCurve;
struct bPose;
struct bItasc;
struct bPoseChannel;
struct Object;
struct Scene;
struct ID;

/* Kernel prototypes */
#ifdef __cplusplus
extern "C" {
#endif

/* Action Lib Stuff ----------------- */

/* Allocate a new bAction with the given name */
struct bAction *add_empty_action(const char name[]);

/* Allocate a copy of the given Action and all its data */	
struct bAction *BKE_action_copy(struct bAction *src);

/* Deallocate all of the Action's data, but not the Action itself */
void BKE_action_free(struct bAction *act);

// XXX is this needed?
void BKE_action_make_local(struct bAction *act);


/* Action API ----------------- */

/* types of transforms applied to the given item 
 *  - these are the return falgs for action_get_item_transforms()
 */
typedef enum eAction_TransformFlags {
	/* location */
	ACT_TRANS_LOC   = (1 << 0),
	/* rotation */
	ACT_TRANS_ROT   = (1 << 1),
	/* scaling */
	ACT_TRANS_SCALE = (1 << 2),

	/* strictly not a transform, but custom properties are also
	 * quite often used in modern rigs
	 */
	ACT_TRANS_PROP  = (1 << 3),

	/* all flags */
	ACT_TRANS_ONLY  = (ACT_TRANS_LOC | ACT_TRANS_ROT | ACT_TRANS_SCALE),
	ACT_TRANS_ALL   = (ACT_TRANS_ONLY | ACT_TRANS_PROP)
} eAction_TransformFlags;

/* Return flags indicating which transforms the given object/posechannel has 
 *	- if 'curves' is provided, a list of links to these curves are also returned
 *	  whose nodes WILL NEED FREEING
 */
short action_get_item_transforms(struct bAction *act, struct Object *ob, struct bPoseChannel *pchan, ListBase *curves);


/* Some kind of bounding box operation on the action */
void calc_action_range(const struct bAction *act, float *start, float *end, short incl_modifiers);

/* Does action have any motion data at all? */
short action_has_motion(const struct bAction *act);

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
void action_groups_add_channel(struct bAction *act, struct bActionGroup *agrp, struct FCurve *fcurve);

/* Remove the given channel from all groups */
void action_groups_remove_channel(struct bAction *act, struct FCurve *fcu);

/* Find a group with the given name */
struct bActionGroup *BKE_action_group_find_name(struct bAction *act, const char name[]);

/* Clear all 'temp' flags on all groups */
void action_groups_clear_tempflags(struct bAction *act);

/* Pose API ----------------- */	
	
/**
 * Deallocates a pose channel.
 * Does not free the pose channel itself.
 */
void BKE_pose_channel_free(struct bPoseChannel *pchan);

/**
 * Removes and deallocates all channels from a pose.
 * Does not free the pose itself.
 */
void BKE_pose_channels_free(struct bPose *pose);

/**
 * Removes the hash for quick lookup of channels, must
 * be done when adding/removing channels.
 */
void BKE_pose_channels_hash_make(struct bPose *pose);
void BKE_pose_channels_hash_free(struct bPose *pose);

/** 
 * Removes and deallocates all data from a pose, and also frees the pose.
 */
void BKE_pose_free(struct bPose *pose);

/**
 * Allocate a new pose on the heap, and copy the src pose and it's channels
 * into the new pose. *dst is set to the newly allocated structure, and assumed to be NULL.
 */ 
void BKE_pose_copy_data(struct bPose **dst, struct bPose *src, int copyconstraints);

/**
 * Copy the internal members of each pose channel including constraints
 * and ID-Props, used when duplicating bones in editmode.
 */
void BKE_pose_channel_copy_data(struct bPoseChannel *pchan, const struct bPoseChannel *pchan_from);

/**
 * Return a pointer to the pose channel of the given name
 * from this pose.
 */
struct bPoseChannel *BKE_pose_channel_find_name(const struct bPose *pose, const char *name);

/**
 * Return a pointer to the active pose channel from this Object.
 * (Note: Object, not bPose is used here, as we need layer info from Armature)
 */
struct bPoseChannel *BKE_pose_channel_active(struct Object *ob);

/** 
 * Looks to see if the channel with the given name
 * already exists in this pose - if not a new one is
 * allocated and initialized.
 */
struct bPoseChannel *BKE_pose_channel_verify(struct bPose *pose, const char *name);

/* Copy the data from the action-pose (src) into the pose */
void extract_pose_from_pose(struct bPose *pose, const struct bPose *src);

/* sets constraint flags */
void BKE_pose_update_constraint_flags(struct bPose *pose);

/* return the name of structure pointed by pose->ikparam */
const char *BKE_pose_ikparam_get_name(struct bPose *pose);

/* allocate and initialize pose->ikparam according to pose->iksolver */
void BKE_pose_ikparam_init(struct bPose *pose);

/* initialize a bItasc structure with default value */
void BKE_pose_itasc_init(struct bItasc *itasc);

/* clears BONE_UNKEYED flags for frame changing */
// XXX to be depreceated for a more general solution in animsys...
void framechange_poses_clear_unkeyed(void);

/* Bone Groups API --------------------- */	

/* Adds a new bone-group */
void BKE_pose_add_group(struct Object *ob);

/* Remove the active bone-group */
void BKE_pose_remove_group(struct Object *ob);

/* Assorted Evaluation ----------------- */	

/* Used for the Action Constraint */
void what_does_obaction(struct Object *ob, struct Object *workob, struct bPose *pose, struct bAction *act, char groupname[], float cframe);

/* for proxy */
void BKE_pose_copy_result(struct bPose *to, struct bPose *from);
/* clear all transforms */
void BKE_pose_rest(struct bPose *pose);

#ifdef __cplusplus
};
#endif

#endif

