/*  DNA_action_types.h   May 2001
 *  
 *  support for the "action" datatype
 *
 *	Reevan McKay
 *
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */


#ifndef DNA_ACTION_TYPES_H
#define DNA_ACTION_TYPES_H

#include "DNA_listBase.h"
#include "DNA_ID.h"
#include "DNA_view2d_types.h"

struct SpaceLink;
struct ListBase;

/* PoseChannel stores the results of Actions (ipos) and transform information 
   with respect to the restposition of Armature bones */

typedef struct bPoseChannel {
	struct bPoseChannel	*next, *prev;
	ListBase			constraints;
	char				name[32];	/* Channels need longer names than normal blender objects */
	short				flag;		/* dynamic, for detecting transform changes */
	short				constflag;  /* for quick detecting which constraints affect this channel */
	int					depth;		/* dependency sorting help */
	
	struct Bone			*bone;		/* set on read file or rebuild pose */
	struct bPoseChannel *parent;	/* set on read file or rebuild pose */
	struct ListBase		 chain;		/* only while evaluating pose */
	
	float		loc[3];				/* written in by actions or transform */
	float		size[3];
	float		quat[4];
	
	float		chan_mat[4][4];		/* matrix result of loc/quat/size , and where we put deform in, see next line */
	float		pose_mat[4][4];		/* constraints accumulate here. in the end, pose_mat = bone->arm_mat * chan_mat */
	float		ik_mat[3][3];		/* for itterative IK */
	
	float		pose_head[3];		/* actually pose_mat[3] */
	float		pose_tail[3];		/* also used for drawing help lines... */
	int pad;
} bPoseChannel;


typedef struct bPose{
	ListBase			chanbase;
	int flag, pad;
} bPose;

typedef struct bActionChannel {
	struct bActionChannel	*next, *prev;
	struct Ipo				*ipo;
	ListBase				constraintChannels;
	int		flag;
	char	name[32];		/* Channel name */
	int		reserved1;

} bActionChannel;

typedef struct bAction {
	ID				id;
	ListBase		chanbase;	/* Channels in this action */
	bActionChannel	*achan;		/* Current action channel */
	bPoseChannel	*pchan;		/* Current pose channel */
} bAction;

typedef struct SpaceAction {
	struct SpaceLink *next, *prev;
	int spacetype;
	float blockscale;
	struct ScrArea *area;

	short blockhandler[8];

	View2D v2d;	
	bAction		*action;
	int	flag;
	short pin, reserved1;
	short	actnr;
	short	lock;
	int pad2;
} SpaceAction;

/* Action Channel flags */
#define	ACHAN_SELECTED	0x00000001
#define ACHAN_HILIGHTED	0x00000002

/* Pose->flag */

#define POSE_RECALC		1

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
	POSE_OBMAT		=	0x0100,
	POSE_PARMAT		=	0x0200,
	POSE_DONE		=   0x0400,
	POSE_KEY		=	0x1000
};

/* Pose Channel constflag (constraint detection) */
#define PCHAN_HAS_IK		1
#define PCHAN_HAS_CONST		2
	/* only used for drawing Posemode, not stored in channel */
#define PCHAN_HAS_ACTION	4

#endif

