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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_actuator_types.h
 *  \ingroup DNA
 *
 * #bActuator type is specifically for use by Object logic-bricks in the game-engine.
 */

#ifndef __DNA_ACTUATOR_TYPES_H__
#define __DNA_ACTUATOR_TYPES_H__

struct Object;
struct Mesh;
struct Scene;
struct Group;
struct Text;

/* ****************** ACTUATORS ********************* */

/* unused now, moved to editobjectactuator in 2.02. Still needed for dna */
typedef struct bAddObjectActuator {
	int time, pad;
	struct Object *ob;
} bAddObjectActuator;

typedef struct bActionActuator {
	struct bAction *act;    /* Pointer to action */
	short   type, flag;     /* Playback type */  // not in use
	float   sta, end;       /* Start & End frames */
	char    name[64];       /* For property-driven playback, MAX_NAME */
	char    frameProp[64];  /* Set this property to the actions current frame, MAX_NAME */
	short   blendin;        /* Number of frames of blending */
	short   priority;       /* Execution priority */
	short   layer;          /* Animation layer */
	short   end_reset;      /* Ending the actuator (negative pulse) wont reset the action to its starting frame */
	short   strideaxis;     /* Displacement axis */
	short   blend_mode;		/* Layer blending mode */
	float   stridelength;   /* Displacement incurred by cycle */ // not in use
	float   layer_weight;   /* How much of the previous layer to use for blending. (<0 = disable, 0 = add mode) */
} bActionActuator;

typedef struct Sound3D {
	float min_gain;
	float max_gain;
	float reference_distance;
	float max_distance;
	float rolloff_factor;
	float cone_inner_angle;
	float cone_outer_angle;
	float cone_outer_gain;
} Sound3D;

typedef struct bSoundActuator {
	short flag, sndnr;
	int pad1, pad2;
	short pad3[2];
	float volume, pitch;
	struct bSound *sound;
	struct Sound3D sound3D;
	short type, pad4;
	short pad5, pad6[1];
} bSoundActuator;

typedef struct bEditObjectActuator {
	int time;
	short type, flag;
	struct Object *ob;
	struct Mesh *me;
	char name[64];	/* MAX_NAME */
	float linVelocity[3]; /* initial lin. velocity on creation */
	float angVelocity[3]; /* initial ang. velocity on creation */
	float mass;
	short localflag; /* flag for the lin & ang. vel: apply locally   */
	short dyn_operation;
	short upflag, trackflag; /* flag for up axis and track axis */
	int pad;
} bEditObjectActuator;

typedef struct bSceneActuator {
	short type, pad1;
	int pad;
	struct Scene *scene;
	struct Object *camera;
} bSceneActuator;

typedef struct bPropertyActuator {
	int pad, type;
	char name[64], value[64];	/* MAX_NAME */
	struct Object *ob;
} bPropertyActuator;

typedef struct bObjectActuator {
	short flag, type, otype;
	short damping;
	float forceloc[3], forcerot[3];
	float pad[3], pad1[3];
	float dloc[3], drot[3]; /* angle in radians */
	float linearvelocity[3], angularvelocity[3];
	struct Object *reference;
} bObjectActuator;

/* deprecated, handled by bActionActuator now */
typedef struct bIpoActuator {
	short flag, type;
	float sta, end;
	char name[64];		/* MAX_NAME */
	char frameProp[64];	/* Set this property to the actions current frame, MAX_NAME */
	
	short pad1, pad2, pad3, pad4;
	
} bIpoActuator;

typedef struct bCameraActuator {
	struct Object *ob;
	float height, min, max;
	float damping;
	short pad1, axis;
	float pad2;
} bCameraActuator;

typedef struct bConstraintActuator {
	short type, mode;
	short flag, damp;
	short time, rotdamp;
	int pad;
	float minloc[3], maxloc[3];
	float minrot[3], maxrot[3];
	char matprop[64];	/* MAX_NAME */
} bConstraintActuator;

typedef struct bGroupActuator {
	short flag, type;
	int sta, end;
	char name[64];		/* property or groupkey, MAX_NAME */
	
	short pad[3], cur, butsta, butend;/* not referenced, can remove? */
	/* struct Group *group;		not used, remove */
	
} bGroupActuator;

/* I added a few extra fields here, to facilitate conversions                */
typedef struct bRandomActuator {
	int  seed;
	int   distribution;
	int int_arg_1;
	int int_arg_2;
	float float_arg_1;
	float float_arg_2;
	char  propname[64];	/* MAX_NAME */
} bRandomActuator;

typedef struct bMessageActuator {
	char toPropName[64];	/* Send to all objects with this propertyname. Empty to broadcast. MAX_NAME. */
	struct Object *toObject;/* (Possible future use) pointer to a single destination object. */
	char subject[64];		/* Message Subject to send. MAX_NAME. */
	short bodyType, pad1;	/* bodyType is either 'User defined text' or PropName */
	int pad2;
	char body[64];			/* Either User Defined Text or our PropName to send value of, MAX_NAME */
} bMessageActuator;

typedef struct bGameActuator {
	short flag, type;
	int sta, end;
	char filename[64];
	char loadaniname[64];
} bGameActuator;

typedef struct bVisibilityActuator {
	/** bit 0: Is this object visible? 
	 ** bit 1: Apply recursively  
	 ** bit 2: Is this object an occluder? */
	int flag;
} bVisibilityActuator;

typedef struct bTwoDFilterActuator {
	char pad[4];
	/* Tells what type of 2D Filter */
	short type;
	/* (flag == 0) means 2D filter is activate and
	 * (flag != 0) means 2D filter is inactive */
	short flag;
	int   int_arg;
	/* a float argument */
	float float_arg;
	struct Text *text;
} bTwoDFilterActuator;

typedef struct bParentActuator {
	char pad[2];
	short flag;
	int type;
	struct Object *ob;
} bParentActuator;

typedef struct bStateActuator {
	int type;			/* 0=Set, 1=Add, 2=Rem, 3=Chg */
	unsigned int mask;	/* the bits to change */
} bStateActuator;

typedef struct bArmatureActuator {
	char posechannel[64];	/* MAX_NAME */
	char constraint[64];	/* MAX_NAME */
	int type;		/* 0=run, 1=enable, 2=disable, 3=set target, 4=set weight */
	float weight;
	float influence;
	float pad;
	struct Object *target;
	struct Object *subtarget;
} bArmatureActuator;

typedef struct bSteeringActuator {
	char pad[5];
	char flag;
	short facingaxis;
	int type;		/* 0=seek, 1=flee, 2=path following */
	float dist;
	float velocity;
	float acceleration;
	float turnspeed;
	int updateTime;
	struct Object *target;
	struct Object *navmesh;
} bSteeringActuator;

typedef struct bMouseActuator {
	short type; /* 0=Visibility, 1=Look */
	short flag;

	int object_axis[2];
	float threshold[2];
	float sensitivity[2];
	float limit_x[2];
	float limit_y[2];
} bMouseActuator;


typedef struct bActuator {
	struct bActuator *next, *prev, *mynew;
	short type;
	/**
	 * Tells what type of actuator data \ref data holds. 
	 */
	short flag;
	short otype, go;
	char name[64];	/* MAX_NAME */

	/**
	 * data must point to an object actuator type struct.
	 */
	void *data;

	/**
	 * For ipo's and props: to find out which object the actuator
	 * belongs to */
	struct Object *ob;

} bActuator;

/* objectactuator->flag */
#define ACT_FORCE_LOCAL			1
#define ACT_TORQUE_LOCAL		2
#define ACT_SERVO_LIMIT_X		2
#define ACT_DLOC_LOCAL			4
#define ACT_SERVO_LIMIT_Y		4
#define ACT_DROT_LOCAL			8
#define ACT_SERVO_LIMIT_Z		8
#define ACT_LIN_VEL_LOCAL		16
#define ACT_ANG_VEL_LOCAL		32
//#define ACT_ADD_LIN_VEL_LOCAL	64
#define ACT_ADD_LIN_VEL			64
#define ACT_ADD_CHAR_LOC		128
#define ACT_CHAR_JUMP			256

/* objectactuator->type */
#define ACT_OBJECT_NORMAL		0
#define ACT_OBJECT_SERVO		1
#define ACT_OBJECT_CHARACTER	2

/* actuator->type */
#define ACT_OBJECT		0
#define ACT_IPO			1
#define ACT_LAMP		2
#define ACT_CAMERA		3
#define ACT_MATERIAL	4
#define ACT_SOUND		5
#define ACT_PROPERTY	6
	/* these two obsolete since 2.02 */
#define ACT_ADD_OBJECT	7
#define ACT_END_OBJECT	8

#define ACT_CONSTRAINT	9
#define ACT_EDIT_OBJECT	10
#define ACT_SCENE		11
#define ACT_GROUP		12
#define ACT_RANDOM      13
#define ACT_MESSAGE     14
#define ACT_ACTION		15	/* __ NLA */
#define ACT_GAME		17
#define ACT_VISIBILITY          18
#define ACT_2DFILTER	19
#define ACT_PARENT      20
#define ACT_SHAPEACTION 21
#define ACT_STATE		22
#define ACT_ARMATURE	23
#define ACT_STEERING    24
#define ACT_MOUSE		25

/* actuator flag */
#define ACT_SHOW		1
#define ACT_DEL			2
#define ACT_NEW			4
#define ACT_LINKED		8	
#define ACT_VISIBLE		16	
#define ACT_PIN			32
#define ACT_DEACTIVATE  64

/* link codes */
#define LINK_SENSOR		0
#define LINK_CONTROLLER	1
#define LINK_ACTUATOR	2

/* keyboardsensor->type */
#define SENS_ALL_KEYS	1

/* actionactuator->type */
#define ACT_ACTION_PLAY			0
#define ACT_ACTION_PINGPONG		1
#define ACT_ACTION_FLIPPER		2
#define ACT_ACTION_LOOP_STOP	3
#define ACT_ACTION_LOOP_END		4
#define ACT_ACTION_KEY2KEY		5
#define ACT_ACTION_FROM_PROP	6
#define ACT_ACTION_MOTION		7

/* actionactuator->blend_mode */
#define ACT_ACTION_BLEND		0
#define ACT_ACTION_ADD			1

/* ipoactuator->type */
/* used for conversion from 2.01 */
#define ACT_IPO_FROM_PROP	6

/* groupactuator->type */
#define ACT_GROUP_PLAY		0
#define ACT_GROUP_PINGPONG	1
#define ACT_GROUP_FLIPPER	2
#define ACT_GROUP_LOOP_STOP	3
#define ACT_GROUP_LOOP_END	4
#define ACT_GROUP_FROM_PROP	5
#define ACT_GROUP_SET		6

/* ipoactuator->flag */
#define ACT_IPOFORCE        (1 << 0)
#define ACT_IPOEND          (1 << 1)
#define ACT_IPOLOCAL		(1 << 2)
#define ACT_IPOCHILD        (1 << 4)	
#define ACT_IPOADD			(1 << 5)

/* property actuator->type */
#define ACT_PROP_ASSIGN		0
#define ACT_PROP_ADD		1
#define ACT_PROP_COPY		2
#define ACT_PROP_TOGGLE		3
#define ACT_PROP_LEVEL		4

/* constraint flag */
#define ACT_CONST_NONE		0
#define ACT_CONST_LOCX		1
#define ACT_CONST_LOCY		2
#define ACT_CONST_LOCZ		4
#define ACT_CONST_ROTX		8
#define ACT_CONST_ROTY		16
#define ACT_CONST_ROTZ		32
#define ACT_CONST_NORMAL	64
#define ACT_CONST_MATERIAL	128
#define ACT_CONST_PERMANENT 256
#define ACT_CONST_DISTANCE	512
#define ACT_CONST_LOCAL     1024
#define ACT_CONST_DOROTFH	2048

/* constraint mode */
#define ACT_CONST_DIRPX		1
#define ACT_CONST_DIRPY		2
#define ACT_CONST_DIRPZ		4
#define ACT_CONST_DIRNX		8
#define ACT_CONST_DIRNY		16
#define ACT_CONST_DIRNZ		32

/* constraint type */
#define ACT_CONST_TYPE_LOC	0
#define ACT_CONST_TYPE_DIST	1
#define ACT_CONST_TYPE_ORI	2
#define ACT_CONST_TYPE_FH   3

/* editObjectActuator->type */
#define ACT_EDOB_ADD_OBJECT		0
#define ACT_EDOB_END_OBJECT		1
#define ACT_EDOB_REPLACE_MESH		2
#define ACT_EDOB_TRACK_TO		3
#define ACT_EDOB_DYNAMICS		4

/* editObjectActuator->localflag */
#define ACT_EDOB_LOCAL_LINV		2
#define ACT_EDOB_LOCAL_ANGV		4

/* editObjectActuator->flag */
#define ACT_TRACK_3D			1

/* editObjectActuator->upflag */
#define ACT_TRACK_UP_X			0
#define ACT_TRACK_UP_Y			1
#define ACT_TRACK_UP_Z			2

/* editObjectActuator->trackflag */
#define ACT_TRACK_TRAXIS_X			0
#define ACT_TRACK_TRAXIS_Y			1
#define ACT_TRACK_TRAXIS_Z			2
#define ACT_TRACK_TRAXIS_NEGX		3
#define ACT_TRACK_TRAXIS_NEGY		4
#define ACT_TRACK_TRAXIS_NEGZ		5

/* editObjectActuator->flag for replace mesh actuator */
#define ACT_EDOB_REPLACE_MESH_NOGFX		2 /* use for replace mesh actuator */
#define ACT_EDOB_REPLACE_MESH_PHYS		4

/* editObjectActuator->dyn_operation */
#define ACT_EDOB_RESTORE_DYN	0
#define ACT_EDOB_SUSPEND_DYN	1
#define ACT_EDOB_ENABLE_RB		2
#define ACT_EDOB_DISABLE_RB		3
#define ACT_EDOB_SET_MASS		4


/* SceneActuator->type */
#define ACT_SCENE_RESTART		0
#define ACT_SCENE_SET			1
#define ACT_SCENE_CAMERA		2
#define ACT_SCENE_ADD_FRONT		3
#define ACT_SCENE_ADD_BACK		4
#define ACT_SCENE_REMOVE		5
#define ACT_SCENE_SUSPEND		6
#define ACT_SCENE_RESUME		7


/* randomAct->distribution */
#define ACT_RANDOM_BOOL_CONST                  0
#define ACT_RANDOM_BOOL_UNIFORM                1
#define ACT_RANDOM_BOOL_BERNOUILLI             2
#define ACT_RANDOM_INT_CONST                   3
#define ACT_RANDOM_INT_UNIFORM                 4
#define ACT_RANDOM_INT_POISSON		           5
#define ACT_RANDOM_FLOAT_CONST                 6
#define ACT_RANDOM_FLOAT_UNIFORM               7
#define ACT_RANDOM_FLOAT_NORMAL                8
#define ACT_RANDOM_FLOAT_NEGATIVE_EXPONENTIAL  9

/* SoundActuator->flag */
#define ACT_SND_3D_SOUND		1

/*  SoundActuator->type */
#define ACT_SND_PLAY_STOP_SOUND		0
#define ACT_SND_PLAY_END_SOUND		1
#define ACT_SND_LOOP_STOP_SOUND		2
#define ACT_SND_LOOP_END_SOUND		3
#define ACT_SND_LOOP_BIDIRECTIONAL_SOUND		4
#define ACT_SND_LOOP_BIDIRECTIONAL_STOP_SOUND	5

/* messageactuator->type */
#define ACT_MESG_MESG		0
#define ACT_MESG_PROP		1

/* gameactuator->type */
#define ACT_GAME_LOAD		0
#define ACT_GAME_START		1
#define ACT_GAME_RESTART	2
#define ACT_GAME_QUIT		3
#define ACT_GAME_SAVECFG	4
#define ACT_GAME_LOADCFG	5
#define ACT_GAME_SCREENSHOT	6

/* visibilityact->flag */
/* Set means the object will become invisible */
#define ACT_VISIBILITY_INVISIBLE       (1 << 0)
#define ACT_VISIBILITY_RECURSIVE       (1 << 1)
#define ACT_VISIBILITY_OCCLUSION       (1 << 2)

/* twodfilter->type */
#define ACT_2DFILTER_ENABLED			-2
#define ACT_2DFILTER_DISABLED			-1
#define ACT_2DFILTER_NOFILTER			0
#define ACT_2DFILTER_MOTIONBLUR			1
#define ACT_2DFILTER_BLUR				2
#define ACT_2DFILTER_SHARPEN			3
#define ACT_2DFILTER_DILATION			4
#define ACT_2DFILTER_EROSION			5
#define ACT_2DFILTER_LAPLACIAN			6
#define ACT_2DFILTER_SOBEL				7
#define ACT_2DFILTER_PREWITT			8
#define ACT_2DFILTER_GRAYSCALE			9
#define ACT_2DFILTER_SEPIA				10
#define ACT_2DFILTER_INVERT				11
#define ACT_2DFILTER_CUSTOMFILTER		12
#define ACT_2DFILTER_NUMBER_OF_FILTERS	13

/* parentactuator->type */
#define ACT_PARENT_SET      0
#define ACT_PARENT_REMOVE   1

/* parentactuator->flag */
#define ACT_PARENT_COMPOUND	1
#define ACT_PARENT_GHOST	2

/* armatureactuator->type */
#define ACT_ARM_RUN			0
#define ACT_ARM_ENABLE		1
#define ACT_ARM_DISABLE		2
#define ACT_ARM_SETTARGET	3
#define ACT_ARM_SETWEIGHT	4
#define ACT_ARM_SETINFLUENCE	5
/* update this define if more types are added */
#define ACT_ARM_MAXTYPE		5

/* stateactuator->type */
#define ACT_STATE_SET		0
#define ACT_STATE_ADD		1
#define ACT_STATE_REMOVE	2
#define ACT_STATE_CHANGE	3

/* steeringactuator->type */
#define ACT_STEERING_SEEK   0
#define ACT_STEERING_FLEE   1
#define ACT_STEERING_PATHFOLLOWING   2
/* steeringactuator->flag */
#define ACT_STEERING_SELFTERMINATED   1
#define ACT_STEERING_ENABLEVISUALIZATION   2
#define ACT_STEERING_AUTOMATICFACING   4
#define ACT_STEERING_NORMALUP  8
#define ACT_STEERING_LOCKZVEL  16

/* mouseactuator->type */
#define ACT_MOUSE_VISIBILITY	0
#define ACT_MOUSE_LOOK			1

/* mouseactuator->flag */
#define ACT_MOUSE_VISIBLE	(1 << 0)
#define ACT_MOUSE_USE_AXIS_X	(1 << 1)
#define ACT_MOUSE_USE_AXIS_Y	(1 << 2)
#define ACT_MOUSE_RESET_X	(1 << 3)
#define ACT_MOUSE_RESET_Y	(1 << 4)
#define ACT_MOUSE_LOCAL_X	(1 << 5)
#define ACT_MOUSE_LOCAL_Y	(1 << 6)

/* mouseactuator->object_axis */
#define ACT_MOUSE_OBJECT_AXIS_X	0
#define ACT_MOUSE_OBJECT_AXIS_Y	1
#define ACT_MOUSE_OBJECT_AXIS_Z	2

#endif  /* __DNA_ACTUATOR_TYPES_H__ */
