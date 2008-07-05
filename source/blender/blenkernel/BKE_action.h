/*  BKE_action.h   May 2001
 *  
 *  Blender kernel action functionality
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_ACTION_H
#define BKE_ACTION_H

#include "DNA_listBase.h"

/**
 * The following structures are defined in DNA_action_types.h
 */

struct bAction;
struct bActionChannel;
struct bPose;
struct bPoseChannel;
struct Object;
struct ID;

/* Kernel prototypes */
#ifdef __cplusplus
extern "C" {
#endif

	
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
void copy_pose(struct bPose **dst, struct bPose *src,
			   int copyconstraints);

/**
 * Deallocate the action's channels including constraint channels.
 * does not free the action structure.
 */
void free_action(struct bAction * id);

void make_local_action(struct bAction *act);

/* only for armatures, doing pose actions only too */
void do_all_pose_actions(struct Object *);
/* only for objects, doing only 1 channel */
void do_all_object_actions(struct Object *);
/* only for Mesh, Curve, Surface, Lattice, doing only Shape channel */
void do_all_shape_actions(struct Object *);


/**
 * Return a pointer to the pose channel of the given name
 * from this pose.
 */
struct  bPoseChannel *get_pose_channel(const struct bPose *pose,
									   const char *name);

/** 
 * Looks to see if the channel with the given name
 * already exists in this pose - if not a new one is
 * allocated and initialized.
 */
struct bPoseChannel *verify_pose_channel(struct bPose* pose, 
										 const char* name);

/* sets constraint flags */
void update_pose_constraint_flags(struct bPose *pose);

/* clears BONE_UNKEYED flags for frame changing */
void framechange_poses_clear_unkeyed(void);

/**
 * Allocate a new bAction on the heap and copy 
 * the contents of src into it. If src is NULL NULL is returned.
 */

struct bAction *copy_action(struct bAction *src);

/**
 * Some kind of bounding box operation on the action.
 */
void calc_action_range(const struct bAction *act, float *start, float *end, int incl_hidden);

/**
 * Set the pose channels from the given action.
 */
void extract_pose_from_action(struct bPose *pose, struct bAction *act, float ctime);

/**
 * Get the effects of the given action using a workob 
 */
void what_does_obaction(struct Object *ob, struct bAction *act, float cframe);

/**
 * Iterate through the action channels of the action
 * and return the channel with the given name.
 * Returns NULL if no channel.
 */
struct bActionChannel *get_action_channel(struct bAction *act,  const char *name);
/**
 * Iterate through the action channels of the action
 * and return the channel with the given name.
 * Returns and adds new channel if no channel.
 */
struct bActionChannel *verify_action_channel(struct bAction *act, const char *name);

  /* baking */
struct bAction *bake_obIPO_to_action(struct Object *ob);

/* exported for game engine */
void blend_poses(struct bPose *dst, struct bPose *src, float srcweight, short mode);
void extract_pose_from_pose(struct bPose *pose, const struct bPose *src);

/* for proxy */
void copy_pose_result(struct bPose *to, struct bPose *from);
/* clear all transforms */
void rest_pose(struct bPose *pose);

/* map global time (frame nr) to strip converted time, doesn't clip */
float get_action_frame(struct Object *ob, float cframe);
/* map strip time to global time (frame nr)  */
float get_action_frame_inv(struct Object *ob, float cframe);
/* builds a list of NlaIpoChannel with ipo values to write in datablock */
void extract_ipochannels_from_action(ListBase *lb, struct ID *id, struct bAction *act, char *name, float ctime);
/* write values returned by extract_ipochannels_from_action, returns the number of value written */
int execute_ipochannels(ListBase *lb);

#ifdef __cplusplus
};
#endif

#endif

