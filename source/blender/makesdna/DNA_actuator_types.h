/**
 * blenlib/DNA_actuator_types.h (mar-2001 nzc)
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef DNA_ACTUATOR_TYPES_H
#define DNA_ACTUATOR_TYPES_H

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
	struct bAction *act;	/* Pointer to action */				
	short	type, flag;		/* Playback type */					
	int	sta, end;		/* Start & End frames */			
	char	name[32];		/* For property-driven playback */	
	char	frameProp[32];	/* Set this property to the actions current frame */
	short	blendin;		/* Number of frames of blending */
	short	priority;		/* Execution priority */
	short	end_reset;	/* Ending the actuator (negative pulse) wont reset the the action to its starting frame */
	short	strideaxis;		/* Displacement axis */
	float	stridelength;	/* Displacement incurred by cycle */
} bActionActuator;												

typedef struct bSoundActuator {
	short flag, sndnr;
	int sta, end;
	short pad1[2];
	struct bSound *sound;
	short type, makecopy;
	short copymade, pad2[1];
} bSoundActuator;

typedef struct bCDActuator {
	short flag, sndnr;
	int sta, end;
	short type, track;
	float volume;
} bCDActuator;

typedef struct bEditObjectActuator {
	int time;
	short type, flag;
	struct Object *ob;
	struct Mesh *me;
	char name[32];
	float linVelocity[3]; /* initial lin. velocity on creation */
	short localflag; /* flag for the lin. vel: apply locally   */
	short dyn_operation;
} bEditObjectActuator;

typedef struct bSceneActuator {
	short type, flag;
	int pad;
	struct Scene *scene;
	struct Object *camera;
} bSceneActuator;

typedef struct bPropertyActuator {
	int flag, type;
	char name[32], value[32];
	struct Object *ob;
} bPropertyActuator;

typedef struct bObjectActuator {
	short flag, type, otype;
	short damping;
	float forceloc[3], forcerot[3];
	float loc[3], rot[3];
	float dloc[3], drot[3];
	float linearvelocity[3], angularvelocity[3];
} bObjectActuator;

typedef struct bIpoActuator {
	short flag, type;
	int sta, end;
	char name[32];
	
	short pad1, cur, butsta, butend;
	
} bIpoActuator;

typedef struct bCameraActuator {
	struct Object *ob;
	float height, min, max;
	float fac;
	short flag, axis;
	float visifac;
} bCameraActuator ;

typedef struct bConstraintActuator {
	short type, mode;
	short flag, damp;
	short time, rotdamp;
	int pad;
	float minloc[3], maxloc[3];
	float minrot[3], maxrot[3];
	char matprop[32];
} bConstraintActuator;

typedef struct bGroupActuator {
	short flag, type;
	int sta, end;
	char name[32];		/* property or groupkey */
	
	short pad[3], cur, butsta, butend;/* not referenced, can remove? */
	struct Group *group;		/* only during game */
	
} bGroupActuator;

/* I added a few extra fields here, to facilitate conversions                */
typedef struct bRandomActuator {
	int  seed;
	int   distribution;
	int int_arg_1;
	int int_arg_2;
	float float_arg_1;
	float float_arg_2;
	char  propname[32];
} bRandomActuator;

typedef struct bMessageActuator {
	/**
	 * Send to all objects with this propertyname. Empty to broadcast.
	 */
	char toPropName[32];

	/**
	 * (Possible future use) pointer to a single destination object.
	 */
	struct Object *toObject;

	/**
	 * Message Subject to send.
	 */
	char subject[32];

	/**
	 * bodyType is either 'User defined text' or PropName
	 */
	short bodyType, pad1;
	int pad2;

	/**
	 * Either User Defined Text or our PropName to send value of
	 */
	char body[32];
} bMessageActuator;

typedef struct bGameActuator {
	short flag, type;
	int sta, end;
	char filename[64];
	char loadaniname[64];
} bGameActuator;

typedef struct bVisibilityActuator {
	/** bit 0: Is this object visible? */
	int flag;
} bVisibilityActuator;

typedef struct bTwoDFilterActuator{
	char pad[4];
	/* Tells what type of 2D Filter */
	short type;
	/* (flag == 0) means 2D filter is activate and
	   (flag != 0) means 2D filter is inactive */
	short flag;
	int   int_arg;
	/* a float argument */
	float float_arg;
	struct Text *text;
}bTwoDFilterActuator;

typedef struct bParentActuator {
	char pad[4];
	int type;
	struct Object *ob;
} bParentActuator;

typedef struct bStateActuator {
	int type;			/* 0=Set, 1=Add, 2=Rem, 3=Chg */
	unsigned int mask;	/* the bits to change */
} bStateActuator;

typedef struct bActuator {
	struct bActuator *next, *prev, *mynew;
	short type;
	/**
	 * Tells what type of actuator data <data> holds. 
	 */
	short flag;
	short otype, go;
	char name[32];

	/**
	 * Data must point to an object actuator type struct.
	 */
	void *data;

	/**
	 * For ipo's and props: to find out which object the actuator
	 * belongs to */
	struct Object *ob;		
	
} bActuator;

typedef struct FreeCamera {
	float mass, accelleration;
	float maxspeed, maxrotspeed,  maxtiltspeed;
	int flag;
	float rotdamp, tiltdamp, speeddamp, pad;
} FreeCamera;

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

/* objectactuator->type */
#define ACT_OBJECT_NORMAL	0
#define ACT_OBJECT_SERVO	1

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
#define ACT_CD			16
#define ACT_GAME		17
#define ACT_VISIBILITY          18
#define ACT_2DFILTER	19
#define ACT_PARENT      20
#define ACT_SHAPEACTION 21
#define ACT_STATE		22

/* actuator flag */
#define ACT_SHOW		1
#define ACT_DEL			2
#define ACT_NEW			4
#define ACT_LINKED		8	
#define ACT_VISIBLE		16	

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

/* ipoactuator->type */
#define ACT_IPO_PLAY		0
#define ACT_IPO_PINGPONG	1
#define ACT_IPO_FLIPPER		2
#define ACT_IPO_LOOP_STOP	3
#define ACT_IPO_LOOP_END	4
#define ACT_IPO_KEY2KEY		5
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

/* ipoactuator->flag for k2k */
#define ACT_K2K_PREV		1
#define ACT_K2K_CYCLIC		2
#define ACT_K2K_PINGPONG	4
#define ACT_K2K_HOLD		8

/* property actuator->type */
#define ACT_PROP_ASSIGN		0
#define ACT_PROP_ADD		1
#define ACT_PROP_COPY		2

/* constraint flag */
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

/* editObjectActuator->type */
#define ACT_EDOB_ADD_OBJECT		0
#define ACT_EDOB_END_OBJECT		1
#define ACT_EDOB_REPLACE_MESH		2
#define ACT_EDOB_TRACK_TO		3
#define ACT_EDOB_DYNAMICS		4



/* editObjectActuator->flag */
#define ACT_TRACK_3D			1

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

/* cdactuator->type */
#define ACT_CD_PLAY_ALL		0
#define ACT_CD_PLAY_TRACK	1
#define ACT_CD_LOOP_TRACK	2
#define ACT_CD_VOLUME		3
#define ACT_CD_STOP			4
#define ACT_CD_PAUSE		5
#define ACT_CD_RESUME		6

/* gameactuator->type */
#define ACT_GAME_LOAD		0
#define ACT_GAME_START		1
#define ACT_GAME_RESTART	2
#define ACT_GAME_QUIT		3

/* visibilityact->flag */
/* Set means the object will become invisible */
#define ACT_VISIBILITY_INVISIBLE       (1 << 0)

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
#endif


