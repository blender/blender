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

struct SpaceLink;
struct ListBase;
struct Object;

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
	short				pad2;
	
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
} bPose;

/* Action Channels belong to Actions. They are linked with an IPO block, and can also own 
 * Constraint Channels in certain situations. 
 */
typedef struct bActionChannel {
	struct bActionChannel	*next, *prev;
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
} bAction;

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

/* Action Channel flags */
#define	ACHAN_SELECTED	0x00000001
#define ACHAN_HILIGHTED	0x00000002
#define ACHAN_HIDDEN	0x00000004
#define ACHAN_PROTECTED 0x00000008
#define ACHAN_EXPANDED 	0x00000010
#define ACHAN_SHOWIPO	0x00000020
#define ACHAN_SHOWCONS 	0x00000040
#define ACHAN_MOVED     0x80000000

/* SpaceAction flag */
#define SACTION_MOVING		1	/* during transform */
#define SACTION_SLIDERS		2	/* show sliders (if relevant) - limited to shape keys for now */

/* SpaceAction AutoSnap Settings */
#define SACTSNAP_OFF	0	/* no auto-snap */
#define SACTSNAP_STEP	1	/* snap to 1.0 frame intervals */
#define SACTSNAP_FRAME	2	/* snap to actual frames (nla-action time) */

/* Pose->flag */
#define POSE_RECALC		1
#define POSE_LOCKED		2
#define POSE_DO_UNLOCK	4

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
#define PCHAN_HAS_IK		1
#define PCHAN_HAS_CONST		2
	/* only used for drawing Posemode, not stored in channel */
#define PCHAN_HAS_ACTION	4
#define PCHAN_HAS_TARGET	8
	/* only for drawing Posemode too */
#define PCHAN_HAS_STRIDE	16

/* PoseChannel->ikflag */
#define		BONE_IK_NO_XDOF 1
#define		BONE_IK_NO_YDOF 2
#define		BONE_IK_NO_ZDOF 4

#define		BONE_IK_XLIMIT	8
#define		BONE_IK_YLIMIT	16
#define		BONE_IK_ZLIMIT	32


#endif

