/**
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef DNA_ARMATURE_TYPES_H
#define DNA_ARMATURE_TYPES_H

#include "DNA_listBase.h"
#include "DNA_ID.h"

typedef struct Bone {
	struct Bone		*next, *prev;	/*	Next/prev elements within this list	*/
	struct Bone		*parent;		/*	Parent (ik parent if appropriate flag is set		*/
	ListBase		childbase;		/*	Children	*/
	char			name[32];		/* Name of the bone - must be unique within the armature */

	float			roll;				
	float			head[3];		/*	Orientation & length are implicit now */
	float			tail[3];		/*	head/tail represents rest state	*/
	int				flag;

	/*	Transformation data used for posing:
		The same information stored by other
		blenderObjects
	*/

	float dist, weight;
	float loc[3], dloc[3];
	float size[3], dsize[3];
	float quat[4], dquat[4];
	float obmat[4][4];
	float parmat[4][4];
	float defmat[4][4];
	float irestmat[4][4];	/*	Cached inverse of rest matrix (objectspace)*/
	float posemat[4][4];	/*	Cached pose matrix (objectspace)*/
	short boneclass;
	short filler1;
	short filler2;
	short filler3;
}Bone;

typedef struct bArmature {
	ID			id;
	ListBase	bonebase;
	ListBase	chainbase;
	int			flag;
	int			res1;				
	int			res2;
	int			res3;			
}bArmature;

enum {
		ARM_RESTPOSBIT	=	0,
		ARM_DRAWXRAYBIT,
		ARM_DRAWAXESBIT,
		ARM_DRAWNAMESBIT,
		ARM_POSEBIT,
		ARM_EDITBIT,
		ARM_DELAYBIT
};

enum {
		ARM_RESTPOS		=	0x00000001,
		ARM_DRAWXRAY	=	0x00000002,
		ARM_DRAWAXES	=	0x00000004,
		ARM_DRAWNAMES	=	0x00000008,
		ARM_POSEMODE	=	0x00000010,
		ARM_EDITMODE	=	0x00000020,
		ARM_DELAYDEFORM =       0x00000040
};

enum {
		BONE_SELECTED	=	0x00000001,
		BONE_ROOTSEL	=	0x00000002,
		BONE_TIPSEL		=	0x00000004,

		BONE_HILIGHTED	=	0x00000008,
		BONE_IK_TOPARENT=	0x00000010,
		BONE_QUATROT	=	0x00000020,
		BONE_HIDDEN		=	0x00000040,

		BONE_DONE		=	0x00000080,	/* For detecting cyclic dependancies */

		BONE_ISEMPTY	=	0x00000100,
		BONE_ISMUSCLE	=	0x00000200
};

enum {
		BONE_SELECTEDBIT	=	0,
		BONE_HEADSELBIT,
		BONE_TAILSELBIT,
		BONE_HILIGHTEDBIT,
		BONE_IK_TOPARENTBIT,
		BONE_QUATROTBIT,
		BONE_HIDDENBIT,
		BONE_ISEMPTYBIT,
		BONE_ISMUSCLEBIT
};

enum {
		BONE_SKINNABLE  =       0,
		BONE_UNSKINNABLE,
		BONE_HEAD,
		BONE_NECK,
		BONE_BACK,
		BONE_SHOULDER,
		BONE_ARM,
		BONE_HAND,
		BONE_FINGER,
		BONE_THUMB,
		BONE_PELVIS,
		BONE_LEG,
		BONE_FOOT,
		BONE_TOE,
		BONE_TENTACLE
};

#endif
