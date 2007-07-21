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
 * Contributor(s): 2007, Joshua Leung, major recode
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * Constraint DNA data
 */

#ifndef DNA_CONSTRAINT_TYPES_H
#define DNA_CONSTRAINT_TYPES_H

#include "DNA_ID.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"

struct Action;
struct Text;
#ifndef __cplusplus
struct PyObject;
#endif

/* channels reside in Object or Action (ListBase) constraintChannels */
typedef struct bConstraintChannel {
	struct bConstraintChannel *next, *prev;
	Ipo			*ipo;
	short		flag;
	char		name[30];
} bConstraintChannel;

/* A Constraint */
typedef struct bConstraint {
	struct bConstraint *next, *prev;
	void		*data;		/*	Constraint data	(a valid constraint type) */
	short		type;		/*	Constraint type	*/
	short		flag;		/*	Flag - General Settings	*/
	
	char 		ownspace;	/* 	Space that owner should be evaluated in 	*/
	char		tarspace;	/* 	Space that target should be evaluated in 	*/
	
	char		name[30];	/*	Constraint name	*/
	
	float		enforce;	/* 	Amount of influence exherted by constraint (0.0-1.0) */
} bConstraint;


/* Python Script Constraint */
typedef struct bPythonConstraint {
	Object *tar;			/* object to use as target (if required) */
	char subtarget[32]; 	/* bone to use as subtarget (if required) */
	
	struct Text *text;		/* text-buffer (containing script) to execute */
	IDProperty *prop;		/* 'id-properties' used to store custom properties for constraint */
	
	int flag;				/* general settings/state indicators accessed by bitmapping */
	int pad;
} bPythonConstraint;

/* Single-target subobject constraints ---------------------  */
/* Inverse-Kinematics (IK) constraint */
typedef struct bKinematicConstraint {
	Object		*tar;
	short		iterations;		/* Maximum number of iterations to try */
	short		flag;			/* Like CONSTRAINT_IK_TIP */
	int			rootbone;	/* index to rootbone, if zero go all the way to mother bone */
	char		subtarget[32];	/* String to specify sub-object target */

	float		weight;			/* Weight of goal in IK tree */
	float		orientweight;	/* Amount of rotation a target applies on chain */
	float		grabtarget[3];	/* for target-less IK */
	int			pad;
} bKinematicConstraint;

/* Track To Constraint */
typedef struct bTrackToConstraint {
	Object		*tar;
	int			reserved1; /* I'll be using reserved1 and reserved2 as Track and Up flags, not sure if that's what they were intented for anyway. Not sure either if it would create backward incompatibility if I were to rename them. - theeth*/
	int			reserved2;
	int			flags;
	int			pad;
	char		subtarget[32];
} bTrackToConstraint;

/* Copy Rotation Constraint */
typedef struct bRotateLikeConstraint {
	Object		*tar;
	int			flag;
	int			reserved1;
	char		subtarget[32];
} bRotateLikeConstraint;

/* Copy Location Constraint */
typedef struct bLocateLikeConstraint {
	Object		*tar;
	int			flag;
	int			reserved1;
	char		subtarget[32];
} bLocateLikeConstraint;

/* Floor Constraint */
typedef struct bMinMaxConstraint {
	Object		*tar;
	int			minmaxflag;
	float			offset;
	int				flag;
	short			sticky, stuck, pad1, pad2; /* for backward compatability */
	float			cache[3];
	char		subtarget[32];
} bMinMaxConstraint;

/* Copy Scale Constraint */
typedef struct bSizeLikeConstraint {
	Object		*tar;
	int			flag;
	int			reserved1;
	char		subtarget[32];
} bSizeLikeConstraint;

/* Action Constraint */
typedef struct bActionConstraint {
	Object		*tar;
	short		type;
	short		local;	/* was used in versions prior to the Constraints recode */
	int		start;
	int		end;
	float		min;
	float		max;
	int             pad;
	struct bAction	*act;
	char		subtarget[32];
} bActionConstraint;

/* Locked Axis Tracking constraint */
typedef struct bLockTrackConstraint {
	Object		*tar;
	int			trackflag;
	int			lockflag;
	char		subtarget[32];
} bLockTrackConstraint;

/* Follow Path constraints */
typedef struct bFollowPathConstraint {
	Object		*tar;	/* Must be path object */
	float		offset; /* Offset in time on the path (in frame) */
	int			followflag;
	int			trackflag;
	int			upflag;
} bFollowPathConstraint;

/* Stretch to constraint */
typedef struct bStretchToConstraint {
	Object		*tar;
	int			volmode; 
	int         plane;
	float		orglength;
	float		bulge;
	char		subtarget[32];
} bStretchToConstraint;

/* Rigid Body constraint */
typedef struct bRigidBodyJointConstraint {
	Object		*tar;
	Object		*child;
	int         type;
	float       pivX;
	float       pivY;
	float       pivZ;
	float       axX;
	float       axY;
	float       axZ;
	float       minLimit[6];
	float       maxLimit[6];
	float       extraFz;
	short		flag;
	short		pad;
	short		pad1;
	short		pad2;
} bRigidBodyJointConstraint;

/* Clamp-To Constraint */
typedef struct bClampToConstraint {
	Object 		*tar;			/* 'target' must be a curve */
	int			flag;			/* which axis/plane to compare owner's location on  */
	int			pad;
} bClampToConstraint;

/* Child Of Constraint */
typedef struct bChildOfConstraint {
	Object 		*tar;			/* object which will act as parent (or target comes from) */
	int 		flag;			/* settings */
	int			pad;
	float		invmat[4][4];	/* parent-inverse matrix to use */
	char 		subtarget[32];	/* string to specify a subobject target */
} bChildOfConstraint;

/* Generic Transform->Transform Constraint */
typedef struct bTransformConstraint {
	Object 		*tar;			/* target (i.e. 'driver' object/bone) */
	char 		subtarget[32];	
	
	short		from, to;		/* can be loc(0) , rot(1),  or size(2) */
	char		map[3];			/* defines which target-axis deform is copied by each owner-axis */
	char		expo;			/* extrapolate motion? if 0, confine to ranges */
	
	float		from_min[3];	/* from_min/max defines range of target transform 	*/
	float		from_max[3];	/* 	to map on to to_min/max range. 			*/
	
	float		to_min[3];		/* range of motion on owner caused by target  */
	float		to_max[3];	
} bTransformConstraint;

/* transform limiting constraints - zero target ----------------------------  */
/* Limit Location Constraint */
typedef struct bLocLimitConstraint {
	float 		xmin, xmax;
	float 		ymin, ymax;
	float		zmin, zmax;
	short 		flag;
	short 		flag2;
} bLocLimitConstraint;

/* Limit Rotation Constraint */
typedef struct bRotLimitConstraint {
	float		xmin, xmax;
	float		ymin, ymax;
	float		zmin, zmax;
	short 		flag;
	short		pad1;
} bRotLimitConstraint;

/* Limit Scaling Constraint */
typedef struct bSizeLimitConstraint {
	float 		xmin, xmax;
	float		ymin, ymax;
	float 		zmin, zmax;
	short 		flag;
	short		pad1;
} bSizeLimitConstraint;

/* bConstraint.type */
#define CONSTRAINT_TYPE_NULL		0
#define CONSTRAINT_TYPE_CHILDOF		1		/* Unimplemented non longer :) - during constraints recode, Aligorith */
#define CONSTRAINT_TYPE_TRACKTO		2	
#define CONSTRAINT_TYPE_KINEMATIC	3	
#define CONSTRAINT_TYPE_FOLLOWPATH	4
#define CONSTRAINT_TYPE_ROTLIMIT	5		/* Unimplemented no longer :) - Aligorith */
#define CONSTRAINT_TYPE_LOCLIMIT	6		/* Unimplemented no longer :) - Aligorith */
#define CONSTRAINT_TYPE_SIZELIMIT	7		/* Unimplemented no longer :) - Aligorith */
#define CONSTRAINT_TYPE_ROTLIKE		8	
#define CONSTRAINT_TYPE_LOCLIKE		9	
#define CONSTRAINT_TYPE_SIZELIKE	10
#define CONSTRAINT_TYPE_PYTHON		11		/* Unimplemented no longer :) - Aligorith. Scripts */
#define CONSTRAINT_TYPE_ACTION		12
#define CONSTRAINT_TYPE_LOCKTRACK	13		/* New Tracking constraint that locks an axis in place - theeth */
#define CONSTRAINT_TYPE_DISTANCELIMIT 14 	/* was never properly coded - removed! */
#define CONSTRAINT_TYPE_STRETCHTO	15  	/* claiming this to be mine :) is in tuhopuu bjornmose */ 
#define CONSTRAINT_TYPE_MINMAX      16  	/* floor constraint */
#define CONSTRAINT_TYPE_RIGIDBODYJOINT 17 	/* rigidbody constraint */
#define CONSTRAINT_TYPE_CLAMPTO		18  	/* clampto constraint */	
#define CONSTRAINT_TYPE_TRANSFORM	19 		/* transformation constraint */	

/* bConstraint->flag */
		/* expand for UI */
#define CONSTRAINT_EXPAND		0x01
		/* pre-check for illegal object name or bone name */
#define CONSTRAINT_DISABLE		0x04
		/* flags 0x2 and 0x8 were used in past, skip this */
		/* to indicate which Ipo should be shown, maybe for 3d access later too */
#define CONSTRAINT_ACTIVE		0x10
		/* flag 0x20 was used to indicate that a constraint was evaluated using a 'local' hack for posebones only  */
		/* to indicate that the owner's space should only be changed into ownspace, but not out of it */
#define CONSTRAINT_SPACEONCE	0x40

/* bConstraint->ownspace/tarspace */
	/* default for all - worldspace */
#define CONSTRAINT_SPACE_WORLD			0
	/* for objects (relative to parent/without parent influence), for bones (along normals of bone, without parent/restposi) */
#define CONSTRAINT_SPACE_LOCAL			1
	/* for posechannels - pose space  */
#define CONSTRAINT_SPACE_POSE			2
	/* for posechannels - local with parent  */
#define CONSTRAINT_SPACE_PARLOCAL		3

/* bConstraintChannel.flag */
#define CONSTRAINT_CHANNEL_SELECT		0x01
#define CONSTRAINT_CHANNEL_PROTECTED 	0x02

/**
 * The flags for ROTLIKE, LOCLIKE and SIZELIKE should be kept identical
 * (that is, same effect, different name). It simplifies the Python API access a lot.
 */

/* bRotateLikeConstraint.flag */
#define ROTLIKE_X		0x01
#define ROTLIKE_Y		0x02
#define ROTLIKE_Z		0x04
#define ROTLIKE_X_INVERT	0x10
#define ROTLIKE_Y_INVERT	0x20
#define ROTLIKE_Z_INVERT	0x40
#define ROTLIKE_OFFSET	0x80

/* bLocateLikeConstraint.flag */
#define LOCLIKE_X			0x01
#define LOCLIKE_Y			0x02
#define LOCLIKE_Z			0x04
#define LOCLIKE_TIP			0x08
#define LOCLIKE_X_INVERT	0x10
#define LOCLIKE_Y_INVERT	0x20
#define LOCLIKE_Z_INVERT	0x40
#define LOCLIKE_OFFSET		0x80
 
/* bSizeLikeConstraint.flag */
#define SIZELIKE_X		0x01
#define SIZELIKE_Y		0x02
#define SIZELIKE_Z		0x04
#define SIZELIKE_OFFSET 0x80

/* Axis flags */
#define LOCK_X		0x00
#define LOCK_Y		0x01
#define LOCK_Z		0x02

#define UP_X		0x00
#define UP_Y		0x01
#define UP_Z		0x02

#define TRACK_X		0x00
#define TRACK_Y		0x01
#define TRACK_Z		0x02
#define TRACK_nX	0x03
#define TRACK_nY	0x04
#define TRACK_nZ	0x05

/* bTrackToConstraint->flags */
#define TARGET_Z_UP 0x01

#define VOLUME_XZ	0x00
#define VOLUME_X	0x01
#define VOLUME_Z	0x02
#define NO_VOLUME	0x03

#define PLANE_X		0x00
#define PLANE_Y		0x01
#define PLANE_Z		0x02

/* Clamp-To Constraint ->flag */
#define CLAMPTO_AUTO	0
#define CLAMPTO_X		1
#define	CLAMPTO_Y		2
#define CLAMPTO_Z		3

/* bKinematicConstraint->flag */
#define CONSTRAINT_IK_TIP		1
#define CONSTRAINT_IK_ROT		2
#define CONSTRAINT_IK_AUTO		4
#define CONSTRAINT_IK_TEMP		8
#define CONSTRAINT_IK_STRETCH	16
#define CONSTRAINT_IK_POS		32

/* MinMax (floor) flags */
#define MINMAX_STICKY	0x01
#define MINMAX_STUCK	0x02
#define MINMAX_USEROT	0x04

/* transform limiting constraints -> flag  */
#define LIMIT_XMIN 0x01
#define LIMIT_XMAX 0x02
#define LIMIT_YMIN 0x04
#define LIMIT_YMAX 0x08
#define LIMIT_ZMIN 0x10
#define LIMIT_ZMAX 0x20

#define LIMIT_XROT 0x01
#define LIMIT_YROT 0x02
#define LIMIT_ZROT 0x04

/* not used anymore - for older Limit Location constraints only */
#define LIMIT_NOPARENT 0x01

/* python constraint -> flag */
#define PYCON_USETARGETS	0x01
#define PYCON_SCRIPTERROR	0x02

/* ChildOf Constraint -> flag */
#define CHILDOF_LOCX 	0x001
#define CHILDOF_LOCY	0x002
#define CHILDOF_LOCZ	0x004
#define CHILDOF_ROTX	0x008
#define CHILDOF_ROTY	0x010
#define CHILDOF_ROTZ	0x020
#define CHILDOF_SIZEX	0x040
#define CHILDOF_SIZEY	0x080
#define CHILDOF_SIZEZ	0x100

/* Rigid-Body Constraint */
#define CONSTRAINT_DRAW_PIVOT 0x40

/* important: these defines need to match up with PHY_DynamicTypes headerfile */
#define CONSTRAINT_RB_BALL		1
#define CONSTRAINT_RB_HINGE		2
#define CONSTRAINT_RB_CONETWIST 4
#define CONSTRAINT_RB_VEHICLE	11
#define CONSTRAINT_RB_GENERIC6DOF 12

#endif
