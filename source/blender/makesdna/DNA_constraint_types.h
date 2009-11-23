/**
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): 2007, Joshua Leung, major recode
 *
 * ***** END GPL LICENSE BLOCK *****
 * Constraint DNA data
 */

#ifndef DNA_CONSTRAINT_TYPES_H
#define DNA_CONSTRAINT_TYPES_H

#include "DNA_ID.h"
#include "DNA_listBase.h"
#include "DNA_object_types.h"

struct Action;
struct Text;
struct Ipo;

/* channels reside in Object or Action (ListBase) constraintChannels */
// XXX depreceated... old AnimSys
typedef struct bConstraintChannel {
	struct bConstraintChannel *next, *prev;
	struct Ipo			*ipo;
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
	char		tarspace;	/* 	Space that target should be evaluated in (only used if 1 target) */
	
	char		name[30];	/*	Constraint name */
	
	float		enforce;	/* 	Amount of influence exherted by constraint (0.0-1.0) */
	float		headtail;	/*	Point along subtarget bone where the actual target is. 0=head (default for all), 1=tail*/
	int			pad;
	
	struct Ipo *ipo;		/* local influence ipo or driver */ // XXX depreceated for 2.5... old animation system hack
	
	/* below are readonly fields that are set at runtime by the solver for use in the GE (only IK atm) */
	float       lin_error;		/* residual error on constraint expressed in blender unit*/
	float       rot_error;		/* residual error on constraint expressed in radiant */
} bConstraint;


/* Multiple-target constraints ---------------------  */

/* This struct defines a constraint target.
 * It is used during constraint solving regardless of how many targets the
 * constraint has.
 */
typedef struct bConstraintTarget {
	struct bConstraintTarget *next, *prev;

	Object *tar;			/* object to use as target */
	char subtarget[32];		/* subtarget - pchan or vgroup name */
	
	float matrix[4][4];		/* matrix used during constraint solving - should be cleared before each use */
	
	short space;			/* space that target should be evaluated in (overrides bConstraint->tarspace) */
	short flag;				/* runtime settings (for editor, etc.) */
	short type;				/* type of target (B_CONSTRAINT_OB_TYPE) */
	short rotOrder;			/* rotation order for target (as defined in BLI_math.h) */
} bConstraintTarget;

/* bConstraintTarget -> flag */
typedef enum B_CONSTRAINT_TARGET_FLAG {
	CONSTRAINT_TAR_TEMP = (1<<0),		/* temporary target-struct that needs to be freed after use */
} B_CONSTRAINT_TARGET_FLAG;

/* bConstraintTarget/bConstraintOb -> type */
typedef enum B_CONSTRAINT_OB_TYPE {
	CONSTRAINT_OBTYPE_OBJECT = 1,	/*	string is ""				*/
	CONSTRAINT_OBTYPE_BONE,			/*	string is bone-name		*/
	CONSTRAINT_OBTYPE_VERT,			/*	string is vertex-group name 	*/
	CONSTRAINT_OBTYPE_CV			/* 	string is vertex-group name - is not available until curves get vgroups */
} B_CONSTRAINT_OB_TYPE;



/* Python Script Constraint */
typedef struct bPythonConstraint {	
	struct Text *text;		/* text-buffer (containing script) to execute */
	IDProperty *prop;		/* 'id-properties' used to store custom properties for constraint */
	
	int flag;				/* general settings/state indicators accessed by bitmapping */
	int tarnum;				/* number of targets - usually only 1-3 are needed */
	
	ListBase targets;		/* a list of targets that this constraint has (bConstraintTarget-s) */
	
	Object *tar;			/* target from previous implementation (version-patch sets this to NULL on file-load) */
	char subtarget[32];		/* subtarger from previous implentation (version-patch sets this to "" on file-load) */
} bPythonConstraint;


/* Inverse-Kinematics (IK) constraint
   This constraint supports a variety of mode determine by the type field 
   according to B_CONSTRAINT_IK_TYPE.
   Some fields are used by all types, some are specific to some types
   This is indicated in the comments for each field
 */
typedef struct bKinematicConstraint {
	Object		*tar;			/* All: target object in case constraint needs a target */
	short		iterations;		/* All: Maximum number of iterations to try */
	short		flag;			/* All & CopyPose: some options Like CONSTRAINT_IK_TIP */
	short		rootbone;		/* All: index to rootbone, if zero go all the way to mother bone */
	short		max_rootbone;	/* CopyPose: for auto-ik, maximum length of chain */
	char		subtarget[32];	/* All: String to specify sub-object target */
	Object		*poletar;			/* All: Pole vector target */
	char		polesubtarget[32];	/* All: Pole vector sub-object target */
	float		poleangle;			/* All: Pole vector rest angle */
	float		weight;			/* All: Weight of constraint in IK tree */
	float		orientweight;	/* CopyPose: Amount of rotation a target applies on chain */
	float		grabtarget[3];	/* CopyPose: for target-less IK */
	short		type;			/* subtype of IK constraint: B_CONSTRAINT_IK_TYPE */
	short 		mode;			/* Distance: how to limit in relation to clamping sphere: LIMITDIST_.. */
	float 		dist;			/* Distance: distance (radius of clamping sphere) from target */
} bKinematicConstraint;

typedef enum B_CONSTRAINT_IK_TYPE {
	CONSTRAINT_IK_COPYPOSE = 0,		/* 'standard' IK constraint: match position and/or orientation of target */
	CONSTRAINT_IK_DISTANCE			/* maintain distance with target */
} B_CONSTRAINT_IK_TYPE;


/* Spline IK Constraint 
 * Aligns 'n' bones to the curvature defined by the curve,
 * with the chain ending on the bone that owns this constraint, 
 * and starting on the nth parent.
 */
typedef struct bSplineIKConstraint {
		/* target(s) */
	Object 		*tar;		/* curve object (with follow path enabled) which drives the bone chain */
	
		/* binding details */
	float 		*points;	/* array of numpoints items, denoting parametric positions along curve that joints should follow */
	short 		numpoints;	/* number of points to bound in points array */
	short		chainlen;	/* number of bones ('n') that are in the chain */
	
		/* settings */	
	short flag;				/* general settings for constraint */
	short xzScaleMode;		/* method used for determining the x & z scaling of the bones */
} bSplineIKConstraint;


/* Single-target subobject constraints ---------------------  */
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
	float		offset;
	int			flag;
	short		sticky, stuck, pad1, pad2; /* for backward compatability */
	float		cache[3];
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
	short		type;	/* what transform 'channel' drives the result */
	short		local;	/* was used in versions prior to the Constraints recode */
	int			start;
	int			end;
	float		min;
	float		max;
	int         pad;
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

/* Damped Tracking constraint */
typedef struct bDampTrackConstraint {
	Object		*tar;
	int			trackflag;
	int			pad;
	char		subtarget[32];
} bDampTrackConstraint;

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
	int			flag2;			/* for legacy reasons, this is flag2. used for any extra settings */
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
	short		flag2;
} bRotLimitConstraint;

/* Limit Scaling Constraint */
typedef struct bSizeLimitConstraint {
	float 		xmin, xmax;
	float		ymin, ymax;
	float 		zmin, zmax;
	short 		flag;
	short		flag2;
} bSizeLimitConstraint;

/* Limit Distance Constraint */
typedef struct bDistLimitConstraint {
	Object 		*tar;
	char 		subtarget[32];
	
	float 		dist;			/* distance (radius of clamping sphere) from target */
	float		soft;			/* distance from clamping-sphere to start applying 'fade' */
	
	short		flag;			/* settings */
	short 		mode;			/* how to limit in relation to clamping sphere */
	int 		pad;
} bDistLimitConstraint;

/* ShrinkWrap Constraint */
typedef struct bShrinkwrapConstraint {
	Object		*target;
	float		dist;			/* distance to kept from target */
	short		shrinkType;		/* shrink type (look on MOD shrinkwrap for values) */
	char		projAxis;		/* axis to project over UP_X, UP_Y, UP_Z */
	char 		pad[9];
} bShrinkwrapConstraint;


/* ------------------------------------------ */

/* bConstraint->type 
 * 	- Do not ever change the order of these, or else files could get
 * 	  broken as their correct value cannot be resolved
 */
typedef enum B_CONSTAINT_TYPES {
	CONSTRAINT_TYPE_NULL = 0,			/* Invalid/legacy constraint */
	CONSTRAINT_TYPE_CHILDOF,			/* Unimplemented non longer :) - during constraints recode, Aligorith */
	CONSTRAINT_TYPE_TRACKTO,
	CONSTRAINT_TYPE_KINEMATIC,
	CONSTRAINT_TYPE_FOLLOWPATH,
	CONSTRAINT_TYPE_ROTLIMIT,			/* Unimplemented no longer :) - Aligorith */
	CONSTRAINT_TYPE_LOCLIMIT,			/* Unimplemented no longer :) - Aligorith */
	CONSTRAINT_TYPE_SIZELIMIT,			/* Unimplemented no longer :) - Aligorith */
	CONSTRAINT_TYPE_ROTLIKE,	
	CONSTRAINT_TYPE_LOCLIKE,	
	CONSTRAINT_TYPE_SIZELIKE,
	CONSTRAINT_TYPE_PYTHON,				/* Unimplemented no longer :) - Aligorith. Scripts */
	CONSTRAINT_TYPE_ACTION,
	CONSTRAINT_TYPE_LOCKTRACK,			/* New Tracking constraint that locks an axis in place - theeth */
	CONSTRAINT_TYPE_DISTLIMIT,			/* limit distance */
	CONSTRAINT_TYPE_STRETCHTO, 			/* claiming this to be mine :) is in tuhopuu bjornmose */ 
	CONSTRAINT_TYPE_MINMAX,  			/* floor constraint */
	CONSTRAINT_TYPE_RIGIDBODYJOINT,		/* rigidbody constraint */
	CONSTRAINT_TYPE_CLAMPTO, 			/* clampto constraint */	
	CONSTRAINT_TYPE_TRANSFORM,			/* transformation (loc/rot/size -> loc/rot/size) constraint */	
	CONSTRAINT_TYPE_SHRINKWRAP,			/* shrinkwrap (loc/rot) constraint */
	CONSTRAINT_TYPE_DAMPTRACK,			/* New Tracking constraint that minimises twisting */
	CONSTRAINT_TYPE_SPLINEIK,			/* Spline-IK - Align 'n' bones to a curve */
	
	/* NOTE: no constraints are allowed to be added after this */
	NUM_CONSTRAINT_TYPES
} B_CONSTRAINT_TYPES; 

/* bConstraint->flag */
/* flags 0x2 (1<<1) and 0x8 (1<<3) were used in past */
/* flag 0x20 (1<<5) was used to indicate that a constraint was evaluated using a 'local' hack for posebones only  */
typedef enum B_CONSTRAINT_FLAG {
		/* expand for UI */
	CONSTRAINT_EXPAND =		(1<<0), 
		/* pre-check for illegal object name or bone name */
	CONSTRAINT_DISABLE = 	(1<<2), 
		/* to indicate which Ipo should be shown, maybe for 3d access later too */	
	CONSTRAINT_ACTIVE = 	(1<<4), 
		/* to indicate that the owner's space should only be changed into ownspace, but not out of it */
	CONSTRAINT_SPACEONCE = 	(1<<6),
		/* influence ipo is on constraint itself, not in action channel */
	CONSTRAINT_OWN_IPO	= (1<<7),
		/* indicates that constraint was added locally (i.e.  didn't come from the proxy-lib) */
	CONSTRAINT_PROXY_LOCAL = (1<<8),
		/* indicates that constraint is temporarily disabled (only used in GE) */
	CONSTRAINT_OFF = (1<<9)
} B_CONSTRAINT_FLAG;

/* bConstraint->ownspace/tarspace */
typedef enum B_CONSTRAINT_SPACETYPES {
		/* default for all - worldspace */
	CONSTRAINT_SPACE_WORLD = 0,
		/* for objects (relative to parent/without parent influence), 
		 * for bones (along normals of bone, without parent/restpositions) 
		 */
	CONSTRAINT_SPACE_LOCAL, /* = 1 */
		/* for posechannels - pose space  */
	CONSTRAINT_SPACE_POSE, /* = 2 */
 		/* for posechannels - local with parent  */
	CONSTRAINT_SPACE_PARLOCAL, /* = 3 */
		/* for files from between 2.43-2.46 (should have been parlocal) */
	CONSTRAINT_SPACE_INVALID, /* = 4. do not exchange for anything! */
} B_CONSTRAINT_SPACETYPES;

/* bConstraintChannel.flag */
typedef enum B_CONSTRAINTCHANNEL_FLAG {
	CONSTRAINT_CHANNEL_SELECT =		(1<<0),
	CONSTRAINT_CHANNEL_PROTECTED =	(1<<1)
} B_CONSTRAINTCHANNEL_FLAG;

/* -------------------------------------- */

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
	/* LOCLIKE_TIP is a depreceated option... use headtail=1.0f instead */
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

/* FollowPath flags */
#define FOLLOWPATH_FOLLOW	0x01
#define FOLLOWPATH_STATIC	0x02
#define FOLLOWPATH_RADIUS	0x04

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

/* ClampTo Constraint ->flag2 */
#define CLAMPTO_CYCLIC	1

/* bKinematicConstraint->flag */
#define CONSTRAINT_IK_TIP		1
#define CONSTRAINT_IK_ROT		2
	/* targetless */
#define CONSTRAINT_IK_AUTO		4
	/* autoik */
#define CONSTRAINT_IK_TEMP		8
#define CONSTRAINT_IK_STRETCH	16
#define CONSTRAINT_IK_POS		32
#define CONSTRAINT_IK_SETANGLE	64
#define CONSTRAINT_IK_GETANGLE	128
	/* limit axis */
#define CONSTRAINT_IK_NO_POS_X	256
#define CONSTRAINT_IK_NO_POS_Y	512
#define CONSTRAINT_IK_NO_POS_Z	1024
#define CONSTRAINT_IK_NO_ROT_X	2048
#define CONSTRAINT_IK_NO_ROT_Y	4096
#define CONSTRAINT_IK_NO_ROT_Z	8192
	/* axis relative to target */
#define CONSTRAINT_IK_TARGETAXIS	16384

/* bSplineIKConstraint->flag */
	/* chain has been attached to spline */
#define CONSTRAINT_SPLINEIK_BOUND			(1<<0)
	/* root of chain is not influenced by the constraint */
#define CONSTRAINT_SPLINEIK_NO_ROOT			(1<<1)
	/* bones in the chain should not scale to fit the curve */
#define CONSTRAINT_SPLINEIK_SCALE_LIMITED	(1<<2)
	/* evenly distribute the bones along the path regardless of length */
#define CONSTRAINT_SPLINEIK_EVENSPLITS		(1<<3)	

/* bSplineIKConstraint->xzScaleMode */
	/* no x/z scaling */
#define CONSTRAINT_SPLINEIK_XZS_NONE			0
	/* bones in the chain should take their x/z scales from the curve radius */
#define CONSTRAINT_SPLINEIK_XZS_RADIUS			1
	/* bones in the chain should take their x/z scales from the original scaling */
#define CONSTRAINT_SPLINEIK_XZS_ORIGINAL		2

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
	/* for all Limit constraints - allow to be used during transform? */
#define LIMIT_TRANSFORM 0x02

/* distance limit constraint */
	/* bDistLimitConstraint->flag */
#define LIMITDIST_USESOFT		(1<<0)

	/* bDistLimitConstraint->mode */
#define LIMITDIST_INSIDE		0
#define LIMITDIST_OUTSIDE		1
#define LIMITDIST_ONSURFACE		2
	
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
#define CONSTRAINT_DISABLE_LINKED_COLLISION 0x80

/* important: these defines need to match up with PHY_DynamicTypes headerfile */
#define CONSTRAINT_RB_BALL		1
#define CONSTRAINT_RB_HINGE		2
#define CONSTRAINT_RB_CONETWIST 4
#define CONSTRAINT_RB_VEHICLE	11
#define CONSTRAINT_RB_GENERIC6DOF 12

#endif
