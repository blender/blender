/*  BKE_action.h   May 2001
 *  
 *  Blender kernel action and pose functionality
 *
 *	Reevan McKay
 *
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Full recode, Ton Roosendaal, Crete 2005
 *				 Full recode, Joshua Leung, 2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_ACTION_H
#define BKE_ACTION_H

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
struct bAction *copy_action(struct bAction *src);

/* Deallocate all of the Action's data, but not the Action itself */
void free_action(struct bAction *act);

// XXX is this needed?
void make_local_action(struct bAction *act);


/* Action API ----------------- */

/* types of transforms applied to the given item 
 * 	- these are the return falgs for action_get_item_transforms()
 */
typedef enum eAction_TransformFlags {
		/* location */
	ACT_TRANS_LOC	= (1<<0),
		/* rotation */
	ACT_TRANS_ROT	= (1<<1),
		/* scaling */
	ACT_TRANS_SCALE	= (1<<2),
		
		/* all flags */
	ACT_TRANS_ALL	= (ACT_TRANS_LOC|ACT_TRANS_ROT|ACT_TRANS_SCALE),
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

/* Add given channel into (active) group  */
void action_groups_add_channel(struct bAction *act, struct bActionGroup *agrp, struct FCurve *fcurve);

/* Remove the given channel from all groups */
void action_groups_remove_channel(struct bAction *act, struct FCurve *fcu);

/* Find a group with the given name */
struct bActionGroup *action_groups_find_named(struct bAction *act, const char name[]);


/* Pose API ----------------- */	
	
/**
 * Removes and deallocates all channels from a pose.
 * Does not free the pose itself.
 */
void free_pose_channels(struct bPose *pose);

/** 
 * Removes and deallocates all data from a pose, and also frees the pose.
 */
void free_pose(struct bPose *pose);

/**
 * Allocate a new pose on the heap, and copy the src pose and it's channels
 * into the new pose. *dst is set to the newly allocated structure, and assumed to be NULL.
 */ 
void copy_pose(struct bPose **dst, struct bPose *src, int copyconstraints);



/**
 * Return a pointer to the pose channel of the given name
 * from this pose.
 */
struct bPoseChannel *get_pose_channel(const struct bPose *pose, const char *name);

/**
 * Return a pointer to the active pose channel from this Object.
 * (Note: Object, not bPose is used here, as we need layer info from Armature)
 */
struct bPoseChannel *get_active_posechannel(struct Object *ob);

/** 
 * Looks to see if the channel with the given name
 * already exists in this pose - if not a new one is
 * allocated and initialized.
 */
struct bPoseChannel *verify_pose_channel(struct bPose* pose, const char* name);

/* Copy the data from the action-pose (src) into the pose */
void extract_pose_from_pose(struct bPose *pose, const struct bPose *src);

/* sets constraint flags */
void update_pose_constraint_flags(struct bPose *pose);

/* return the name of structure pointed by pose->ikparam */
const char *get_ikparam_name(struct bPose *pose);

/* allocate and initialize pose->ikparam according to pose->iksolver */
void init_pose_ikparam(struct bPose *pose);

/* initialize a bItasc structure with default value */
void init_pose_itasc(struct bItasc *itasc);

/* clears BONE_UNKEYED flags for frame changing */
// XXX to be depreceated for a more general solution in animsys...
void framechange_poses_clear_unkeyed(void);

/* Bone Groups API --------------------- */	

/* Adds a new bone-group */
void pose_add_group(struct Object *ob);

/* Remove the active bone-group */
void pose_remove_group(struct Object *ob);

/* Assorted Evaluation ----------------- */	

/* Used for the Action Constraint */
void what_does_obaction(struct Scene *scene, struct Object *ob, struct Object *workob, struct bPose *pose, struct bAction *act, char groupname[], float cframe);

/* for proxy */
void copy_pose_result(struct bPose *to, struct bPose *from);
/* clear all transforms */
void rest_pose(struct bPose *pose);

#ifdef __cplusplus
};
#endif

#endif

