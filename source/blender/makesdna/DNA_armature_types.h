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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_armature_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_ARMATURE_TYPES_H__
#define __DNA_ARMATURE_TYPES_H__

#include "DNA_listBase.h"
#include "DNA_ID.h"

struct AnimData;

/* this system works on different transformation space levels;
 *
 * 1) Bone Space;      with each Bone having own (0,0,0) origin
 * 2) Armature Space;  the rest position, in Object space, Bones Spaces are applied hierarchical
 * 3) Pose Space;      the animation position, in Object space
 * 4) World Space;     Object matrix applied to Pose or Armature space
 *
 */

typedef struct Bone {
	struct Bone		*next, *prev;	/*	Next/prev elements within this list	*/
	IDProperty 		*prop;			/* User-Defined Properties on this Bone */
	struct Bone		*parent;		/*	Parent (ik parent if appropriate flag is set		*/
	ListBase		childbase;		/*	Children	*/
	char			name[64];		/*  Name of the bone - must be unique within the armature, MAXBONENAME */

	float			roll;   /*  roll is input for editmode, length calculated */
	float			head[3];		
	float			tail[3];		/*	head/tail and roll in Bone Space	*/
	float			bone_mat[3][3]; /*  rotation derived from head/tail/roll */
	
	int				flag;
	
	float			arm_head[3];		
	float			arm_tail[3];	/*	head/tail in Armature Space (rest pos) */
	float			arm_mat[4][4];  /*  matrix: (bonemat(b)+head(b))*arm_mat(b-1), rest pos*/
	float           arm_roll;        /* roll in Armature Space (rest pos) */
	
	float			dist, weight;			/*  dist, weight: for non-deformgroup deforms */
	float			xwidth, length, zwidth;	/*  width: for block bones. keep in this order, transform! */
	float			ease1, ease2;			/*  length of bezier handles */
	float			rad_head, rad_tail;	/* radius for head/tail sphere, defining deform as well, parent->rad_tip overrides rad_head*/
	
	float			size[3];		/*  patch for upward compat, UNUSED! */
	int				layer;			/* layers that bone appears on */
	short			segments;		/*  for B-bones */
	short 			pad[1];
} Bone;

typedef struct bArmature {
	ID			id;
	struct AnimData *adt;
	
	ListBase	bonebase;
	ListBase	chainbase;
	ListBase	*edbo;					/* editbone listbase, we use pointer so we can check state */
	
	/* active bones should work like active object where possible
	 * - active and selection are unrelated
	 * - active & hidden is not allowed 
	 * - from the user perspective active == last selected
	 * - active should be ignored when not visible (hidden layer) */

	Bone		*act_bone;				/* active bone (when not in editmode) */
	void		*act_edbone;			/* active editbone (in editmode) */

	void		*sketch;				/* sketch struct for etch-a-ton */
	
	int			flag;
	int			drawtype;
	int			gevertdeformer;			/* how vertex deformation is handled in the ge */
	int			pad;
	short		deformflag; 
	short		pathflag;
	
	unsigned int layer_used;		/* for UI, to show which layers are there */
	unsigned int layer, layer_protected;		/* for buttons to work, both variables in this order together */
	
// XXX depreceated... old animaton system (armature only viz) ---
	short		ghostep, ghostsize;		/* number of frames to ghosts to show, and step between them  */
	short		ghosttype, pathsize;		/* ghost drawing options and number of frames between points of path */
	int			ghostsf, ghostef;		/* start and end frames of ghost-drawing range */
	int 		pathsf, pathef;			/* start and end frames of path-calculation range for all bones */
	int			pathbc, pathac;			/* number of frames before/after current frame of path-calculation for all bones  */
// XXX end of depreceated code ---------------------------------- 
} bArmature;

/* armature->flag */
/* don't use bit 7, was saved in files to disable stuff */
typedef enum eArmature_Flag {
	ARM_RESTPOS			= (1<<0),
	ARM_DRAWXRAY		= (1<<1),	/* XRAY is here only for backwards converting */
	ARM_DRAWAXES		= (1<<2),
	ARM_DRAWNAMES		= (1<<3), 
	ARM_POSEMODE		= (1<<4), 
	ARM_EDITMODE		= (1<<5), 
	ARM_DELAYDEFORM 	= (1<<6), 
	ARM_DONT_USE    	= (1<<7),
	ARM_MIRROR_EDIT		= (1<<8),
	ARM_AUTO_IK			= (1<<9),
	ARM_NO_CUSTOM		= (1<<10), 	/* made option negative, for backwards compat */
	ARM_COL_CUSTOM		= (1<<11),	/* draw custom colors  */
	ARM_GHOST_ONLYSEL 	= (1<<12),	/* when ghosting, only show selected bones (this should belong to ghostflag instead) */ // XXX depreceated
	ARM_DS_EXPAND 		= (1<<13)
} eArmature_Flag;

/* armature->drawtype */
typedef enum eArmature_Drawtype {
	ARM_OCTA = 0,
	ARM_LINE,
	ARM_B_BONE,
	ARM_ENVELOPE,
	ARM_WIRE
} eArmature_Drawtype;

/* armature->gevertdeformer */
typedef enum eArmature_VertDeformer {
	ARM_VDEF_BLENDER,
	ARM_VDEF_BGE_CPU
} eArmature_VertDeformer;

/* armature->deformflag */
typedef enum eArmature_DeformFlag {
	ARM_DEF_VGROUP			= (1<<0),
	ARM_DEF_ENVELOPE		= (1<<1),
	ARM_DEF_QUATERNION		= (1<<2),
	ARM_DEF_B_BONE_REST		= (1<<3),	/* deprecated */
	ARM_DEF_INVERT_VGROUP	= (1<<4)
} eArmature_DeformFlag;

/* armature->pathflag */
// XXX depreceated... old animation system (armature only viz)
typedef enum eArmature_PathFlag {
	ARM_PATH_FNUMS		= (1<<0),
	ARM_PATH_KFRAS		= (1<<1),
	ARM_PATH_HEADS		= (1<<2),
	ARM_PATH_ACFRA		= (1<<3),
	ARM_PATH_KFNOS		= (1<<4)
} eArmature_PathFlag;

/* armature->ghosttype */
// XXX depreceated... old animation system (armature only viz)
typedef enum eArmature_GhostType {
	ARM_GHOST_CUR = 0,
	ARM_GHOST_RANGE,
	ARM_GHOST_KEYS
} eArmature_GhostType;

/* bone->flag */
typedef enum eBone_Flag {
	BONE_SELECTED 				= (1<<0),
	BONE_ROOTSEL				= (1<<1),
	BONE_TIPSEL					= (1<<2),
	BONE_TRANSFORM  			= (1<<3),	/* Used instead of BONE_SELECTED during transform */
	BONE_CONNECTED 				= (1<<4),	/* when bone has a parent, connect head of bone to parent's tail*/
	/* 32 used to be quatrot, was always set in files, do not reuse unless you clear it always */	
	BONE_HIDDEN_P				= (1<<6), 	/* hidden Bones when drawing PoseChannels */	
	BONE_DONE					= (1<<7),	/* For detecting cyclic dependancies */
	BONE_DRAW_ACTIVE			= (1<<8), 	/* active is on mouse clicks only - deprecated, ONLY USE FOR DRAWING */
	BONE_HINGE					= (1<<9),	/* No parent rotation or scale */
	BONE_HIDDEN_A				= (1<<10), 	/* hidden Bones when drawing Armature Editmode */
	BONE_MULT_VG_ENV 			= (1<<11), 	/* multiplies vgroup with envelope */
	BONE_NO_DEFORM				= (1<<12),	/* bone doesn't deform geometry */
	BONE_UNKEYED				= (1<<13), 	/* set to prevent destruction of its unkeyframed pose (after transform) */		
	BONE_HINGE_CHILD_TRANSFORM 	= (1<<14), 	/* set to prevent hinge child bones from influencing the transform center */
	BONE_NO_SCALE				= (1<<15), 	/* No parent scale */
	BONE_HIDDEN_PG				= (1<<16),	/* hidden bone when drawing PoseChannels (for ghost drawing) */
	BONE_DRAWWIRE				= (1<<17),	/* bone should be drawn as OB_WIRE, regardless of draw-types of view+armature */
	BONE_NO_CYCLICOFFSET		= (1<<18),	/* when no parent, bone will not get cyclic offset */
	BONE_EDITMODE_LOCKED		= (1<<19),	/* bone transforms are locked in EditMode */
	BONE_TRANSFORM_CHILD		= (1<<20),	/* Indicates that a parent is also being transformed */
	BONE_UNSELECTABLE			= (1<<21),	/* bone cannot be selected */
	BONE_NO_LOCAL_LOCATION		= (1<<22)	/* bone location is in armature space */
} eBone_Flag;

#define MAXBONENAME 64

#endif
