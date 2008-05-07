/*  
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
 * Contributor(s): Original design: Reevan McKay
 * Contributor(s): Full recode, Ton Roosendaal, Crete 2005
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#ifndef DNA_ACTION_TYPES_H
#define DNA_ACTION_TYPES_H

#include "DNA_listBase.h"
#include "DNA_ID.h"
#include "DNA_view2d_types.h"
#include "DNA_userdef_types.h"

struct SpaceLink;
struct Object;

/* -------------- Poses ----------------- */

/* PoseChannel stores the results of Actions (ipos) and transform information 
   with respect to the restposition of Armature bones */
typedef struct bPoseChannel {
	struct bPoseChannel	*next, *prev;
	ListBase			constraints;/* Constraints that act on this PoseChannel */
	char				name[32];	/* Channels need longer names than normal blender objects */
	
	short				flag;		/* dynamic, for detecting transform changes */
	short				constflag;  /* for quick detecting which constraints affect this channel */
	short				ikflag;		/* settings for IK bones */
	short               selectflag;	/* copy of bone flag, so you can work with library armatures */
	short				protectflag; /* protect channels from being transformed */
	short				agrp_index; /* index of action-group this bone belongs to (0 = default/no group) */
	
	int				    pathlen;	/* for drawing paths, the amount of frames */
	int 				pathsf;		/* for drawing paths, the start frame number */
	int					pathef;		/* for drawing paths, the end frame number */
	
	struct Bone			*bone;		/* set on read file or rebuild pose */
	struct bPoseChannel *parent;	/* set on read file or rebuild pose */
	struct bPoseChannel *child;		/* set on read file or rebuild pose, the 'ik' child, for b-bones */
	struct ListBase		 iktree;		/* only while evaluating pose */
	
	/* only while deform, stores precalculated b_bone deform mats,
	   dual quaternions */
	void				*b_bone_mats;	
	void				*dual_quat;
	void				*b_bone_dual_quats;
	
	float		loc[3];				/* written in by actions or transform */
	float		size[3];
	float		quat[4];
	
	float		chan_mat[4][4];		/* matrix result of loc/quat/size , and where we put deform in, see next line */
	float		pose_mat[4][4];		/* constraints accumulate here. in the end, pose_mat = bone->arm_mat * chan_mat */
	float		constinv[4][4];		/* inverse result of constraints. doesn't include effect of restposition, parent, and local transform*/
	
	float		pose_head[3];		/* actually pose_mat[3] */
	float		pose_tail[3];		/* also used for drawing help lines... */
	
	float		limitmin[3], limitmax[3];	/* DOF constraint */
	float		stiffness[3];				/* DOF stiffness */
	float		ikstretch;
	
	float		*path;				/* totpath x 3 x float */
	struct Object *custom;			/* draws custom object instead of this channel */
} bPoseChannel;

/* Pose-Object. It is only found under ob->pose. It is not library data, even
 * though there is a define for it (hack for the outliner).
 */
typedef struct bPose {
	ListBase chanbase; 			/* list of pose channels */
	
	short flag, proxy_layer;	/* proxy layer: copy from armature, gets synced */
	
	float ctime;				/* local action time of this pose */
	float stride_offset[3];		/* applied to object */
	float cyclic_offset[3];		/* result of match and cycles, applied in where_is_pose() */
	
	
	ListBase agroups;			/* list of bActionGroups */
	
	int active_group;			/* index of active group (starts from 1) */
	int pad;
} bPose;


/* ------------- Action  ---------------- */

/* Action-Channel Group. These are stored as a list per-Action, and are only used to 
 * group that Action's Action-Channels when displayed in the Action Editor. 
 *
 * Even though all Action-Channels live in a big list per Action, each group they are in also
 * holds references to the achans within that list which belong to it. Care must be taken to
 * ensure that action-groups never end up being the sole 'owner' of a channel.
 *
 * 
 * This is also exploited for bone-groups. Bone-Groups are stored per bPose, and are used 
 * primarily to colour bones in the 3d-view. There are other benefits too, but those are mostly related
 * to Action-Groups.
 */
typedef struct bActionGroup {
	struct bActionGroup *next, *prev;
	
	ListBase channels;			/* Note: this must not be touched by standard listbase functions */
	
	int flag;					/* settings for this action-group */
	int customCol;				/* index of custom color set to use when used for bones (0=default - used for all old files, -1=custom set) */				
	char name[32];				/* name of the group */
	
	ThemeWireColor cs;			/* color set to use when customCol == -1 */
} bActionGroup;

/* Action Channels belong to Actions. They are linked with an IPO block, and can also own 
 * Constraint Channels in certain situations. 
 *
 * Action-Channels can only belong to one group at a time, but they still live the Action's
 * list of achans (to preserve backwards compatability, and also minimise the code
 * that would need to be recoded). Grouped achans are stored at the start of the list, according
 * to the position of the group in the list, and their position within the group. 
 */
typedef struct bActionChannel {
	struct bActionChannel	*next, *prev;
	bActionGroup 			*grp;					/* Action Group this Action Channel belongs to */
	
	struct Ipo				*ipo;					/* IPO block this action channel references */
	ListBase				constraintChannels;		/* Constraint Channels (when Action Channel represents an Object or Bone) */
	
	int		flag;			/* settings accessed via bitmapping */
	char	name[32];		/* channel name */
	int		reserved1;
} bActionChannel;

/* Action. A recyclable block that contains a series of Action Channels (ipo), which define 
 * a clip of reusable animation for use in the NLA.
 */
typedef struct bAction {
	ID				id;
	
	ListBase		chanbase;	/* Action Channels in this Action */
	ListBase 		groups;		/* Action Groups in the Action */
	ListBase 		markers;	/* TimeMarkers local to this Action for labelling 'poses' */
	
	int active_marker;			/* Index of active-marker (first marker = 1) */
	int pad;
} bAction;


/* ------------- Action Editor --------------------- */

/* Action Editor Space. This is defined here instead of in DNA_space_types.h */
typedef struct SpaceAction {
	struct SpaceLink *next, *prev;
	int spacetype;
	float blockscale;
	struct ScrArea *area;

	short blockhandler[8];

	View2D v2d;	
	
	bAction		*action;		/* the currently active action */
	short flag, autosnap;		/* flag: bitmapped settings; autosnap: automatic keyframe snapping mode */
	short pin, actnr, lock;		/* pin: keep showing current action; actnr: used for finding chosen action from menu; lock: lock time to other windows */
	short actwidth;				/* width of the left-hand side name panel (in pixels?) */
	float timeslide;			/* for Time-Slide transform mode drawing - current frame? */
} SpaceAction;


/* -------------- Action Flags -------------- */

/* Action Channel flags */
typedef enum ACHAN_FLAG {
	ACHAN_SELECTED	= (1<<0),
	ACHAN_HILIGHTED = (1<<1),
	ACHAN_HIDDEN	= (1<<2),
	ACHAN_PROTECTED = (1<<3),
	ACHAN_EXPANDED 	= (1<<4),
	ACHAN_SHOWIPO	= (1<<5),
	ACHAN_SHOWCONS 	= (1<<6),
	ACHAN_MOVED     = (1<<31),
} ACHAN_FLAG; 


/* Action Group flags */
typedef enum AGRP_FLAG {
	AGRP_SELECTED 	= (1<<0),
	AGRP_ACTIVE 	= (1<<1),
	AGRP_PROTECTED 	= (1<<2),
	AGRP_EXPANDED 	= (1<<3),
	
	AGRP_TEMP		= (1<<30),
	AGRP_MOVED 		= (1<<31)
} AGRP_FLAG;

/* ------------ Action Editor Flags -------------- */

/* SpaceAction flag */
typedef enum SACTION_FLAG {
		/* during transform (only set for TimeSlide) */
	SACTION_MOVING	= (1<<0),	
		/* show sliders (if relevant) */
	SACTION_SLIDERS	= (1<<1),	
		/* draw time in seconds instead of time in frames */
	SACTION_DRAWTIME = (1<<2),
		/* don't filter action channels according to visibility */
	SACTION_NOHIDE = (1<<3),
		/* don't kill overlapping keyframes after transform */
	SACTION_NOTRANSKEYCULL = (1<<4),
		/* don't include keyframes that are out of view */
	SACTION_HORIZOPTIMISEON = (1<<5),
		/* hack for moving pose-markers (temp flag)  */
	SACTION_POSEMARKERS_MOVE = (1<<6),
		/* don't draw action channels using group colours (where applicable) */
	SACTION_NODRAWGCOLORS = (1<<7)
} SACTION_FLAG;	

/* SpaceAction AutoSnap Settings (also used by SpaceNLA) */
typedef enum SACTSNAP_MODES {
		/* no auto-snap */
	SACTSNAP_OFF = 0,	
		/* snap to 1.0 frame/second intervals */
	SACTSNAP_STEP,
		/* snap to actual frames/seconds (nla-action time) */
	SACTSNAP_FRAME,
		/* snap to nearest marker */
	SACTSNAP_MARKER,
} SACTSNAP_MODES;	
 
 
/* --------- Pose Flags --------------- */

/* Pose->flag */
typedef enum POSE_FLAG {
		/* results in armature_rebuild_pose being called */
	POSE_RECALC = (1<<0),
		/* prevents any channel from getting overridden by anim from IPO */
	POSE_LOCKED	= (1<<1),
		/* clears the POSE_LOCKED flag for the next time the pose is evaluated */
	POSE_DO_UNLOCK	= (1<<2),
		/* pose has constraints which depend on time (used when depsgraph updates for a new frame) */
	POSE_CONSTRAINTS_TIMEDEPEND = (1<<3)
} POSE_FLAG;

/* PoseChannel (transform) flags */
enum	{
	POSE_LOC		=	0x0001,
	POSE_ROT		=	0x0002,
	POSE_SIZE		=	0x0004,
	POSE_IK_MAT		=	0x0008,
	POSE_UNUSED2	=	0x0010,
	POSE_UNUSED3	=	0x0020,
	POSE_UNUSED4	=	0x0040,
	POSE_UNUSED5	=	0x0080,
	POSE_HAS_IK		=	0x0100,
	POSE_CHAIN		=	0x0200,
	POSE_DONE		=   0x0400,
	POSE_KEY		=	0x1000,
	POSE_STRIDE		=	0x2000
};

/* PoseChannel constflag (constraint detection) */
typedef enum PCHAN_CONSTFLAG {
	PCHAN_HAS_IK		= (1<<0),
	PCHAN_HAS_CONST		= (1<<1),
		/* only used for drawing Posemode, not stored in channel */
	PCHAN_HAS_ACTION	= (1<<2),
	PCHAN_HAS_TARGET	= (1<<3),
		/* only for drawing Posemode too */
	PCHAN_HAS_STRIDE	= (1<<4)
} PCHAN_CONSTFLAG;

/* PoseChannel->ikflag */
typedef enum PCHAN_IKFLAG {
	BONE_IK_NO_XDOF = (1<<0),
	BONE_IK_NO_YDOF = (1<<1),
	BONE_IK_NO_ZDOF = (1<<2),

	BONE_IK_XLIMIT	= (1<<3),
	BONE_IK_YLIMIT	= (1<<4),
	BONE_IK_ZLIMIT	= (1<<5),
	
	BONE_IK_NO_XDOF_TEMP = (1<<10),
	BONE_IK_NO_YDOF_TEMP = (1<<11),
	BONE_IK_NO_ZDOF_TEMP = (1<<12)
} PCHAN_IKFLAG;


#endif

