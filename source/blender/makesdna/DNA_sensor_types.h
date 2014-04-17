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

/** \file DNA_sensor_types.h
 *  \ingroup DNA
 *  \since mar-2001
 *  \author nzc
 *
 * #bSensor type is specifically for use by Object logic-bricks in the game-engine.
 */

#ifndef __DNA_SENSOR_TYPES_H__
#define __DNA_SENSOR_TYPES_H__

struct Object;
struct Material;

/* ****************** SENSORS ********************* */

typedef struct bNearSensor {
	char name[64];	/* MAX_NAME */
	float dist, resetdist;
	int lastval, pad;
} bNearSensor;

/**
 * Defines the settings of a mouse sensor.
 */
typedef struct bMouseSensor {
	/**
	 * The type of key this sensor listens to. 
	 */
	short type;
	short flag;
	short pad1;
	short pad2;
} bMouseSensor;

/* DEPRECATED */
typedef struct bTouchSensor {
	char name[64];	/* MAX_NAME */
	struct Material *ma;
	float dist, pad;
} bTouchSensor;

typedef struct bKeyboardSensor {
	short key, qual;
	short type, qual2;
	/**
	 * Name of the target property
	 */
	char targetName[64];	/* MAX_NAME */
	/**
	 * Name of the toggle property
	 */
	char toggleName[64];	/* MAX_NAME */
} bKeyboardSensor;

typedef struct bPropertySensor {
	int type;
	int pad;
	char name[64];	/* MAX_NAME */
	char value[64];
	char maxvalue[64];
} bPropertySensor;

typedef struct bActuatorSensor {
	int type;
	int pad;
	char name[64];	/* MAX_NAME */
} bActuatorSensor;

typedef struct bDelaySensor {
	short delay;
	short duration;
	short flag;
	short pad;
} bDelaySensor;

typedef struct bCollisionSensor {
	char name[64];          /* property name. MAX_NAME */
	char materialName[64];  /* material      */
	// struct Material *ma; // XXX remove materialName
	short damptimer, damp;
	short mode;             /* flag to choose material or property */
	short pad2;
} bCollisionSensor;

typedef struct bRadarSensor {
	char name[64];	/* MAX_NAME */
	float angle;
	float range;
	short flag, axis;
} bRadarSensor;

typedef struct bRandomSensor {
	char name[64];	/* MAX_NAME */
	int seed;
	int delay;
} bRandomSensor;

typedef struct bRaySensor {
	char name[64];	/* MAX_NAME */
	float range;
	char propname[64];
	char matname[64];
	//struct Material *ma; // XXX remove materialName
	short mode;
	short pad1;
	int axisflag;
} bRaySensor;

typedef struct bArmatureSensor {
	char posechannel[64];	/* MAX_NAME */
	char constraint[64];	/* MAX_NAME */
	int  type;
	float value;
} bArmatureSensor;

typedef struct bMessageSensor {
	/**
	 * (Possible future use) pointer to a single sender object
	 */
	struct Object *fromObject;

	/**
	 * Can be used to filter on subjects like this
	 */
	char subject[64];

	/**
	 * (Possible future use) body to filter on
	 */
	char body[64];
} bMessageSensor;

typedef struct bSensor {
	struct bSensor *next, *prev;
	/* pulse and freq are the bool toggle and frame count for pulse mode */
	short type, otype, flag, pulse;
	short freq, totlinks, pad1, pad2;
	char name[64];	/* MAX_NAME */
	void *data;
	
	struct bController **links;
	
	struct Object *ob;

	/* just add here, to avoid align errors... */
	short invert; /* Whether or not to invert the output. */
	short level;  /* Whether the sensor is level base (edge by default) */
	short tap;
	short pad;
} bSensor;

typedef struct bJoystickSensor {
	char name[64];	/* MAX_NAME */
	char type;
	char joyindex;
	short flag;
	short axis;
	short axis_single;
	int axisf;
	int button;
	int hat;
	int hatf;
	int precision;
} bJoystickSensor;

/* bMouseSensor->type: uses blender event defines */

/* bMouseSensor->flag: only pulse for now */
#define SENS_MOUSE_FOCUS_PULSE	1

/* propertysensor->type */
#define SENS_PROP_EQUAL		0
#define SENS_PROP_NEQUAL	1
#define SENS_PROP_INTERVAL	2
#define SENS_PROP_CHANGED	3
#define SENS_PROP_EXPRESSION	4

/* raysensor->axisflag */
/* flip x and y to make y default!!! */
#define SENS_RAY_X_AXIS     1
#define SENS_RAY_Y_AXIS     0
#define SENS_RAY_Z_AXIS     2
#define SENS_RAY_NEG_X_AXIS     3
#define SENS_RAY_NEG_Y_AXIS     4
#define SENS_RAY_NEG_Z_AXIS     5
//#define SENS_RAY_NEGATIVE_AXIS     1

/* bRadarSensor->axis */
#define SENS_RADAR_X_AXIS     0
#define SENS_RADAR_Y_AXIS     1
#define SENS_RADAR_Z_AXIS     2
#define SENS_RADAR_NEG_X_AXIS     3
#define SENS_RADAR_NEG_Y_AXIS     4
#define SENS_RADAR_NEG_Z_AXIS     5

/* bMessageSensor->type */
#define SENS_MESG_MESG		0
#define SENS_MESG_PROP		1

/* bArmatureSensor->type */
#define SENS_ARM_STATE_CHANGED		0
#define SENS_ARM_LIN_ERROR_BELOW	1
#define SENS_ARM_LIN_ERROR_ABOVE	2
#define SENS_ARM_ROT_ERROR_BELOW	3
#define SENS_ARM_ROT_ERROR_ABOVE	4
/* update this when adding new type */
#define SENS_ARM_MAXTYPE			4

/* sensor->type */
#define SENS_ALWAYS		0
#define SENS_TOUCH		1  /* DEPRECATED */
#define SENS_NEAR		2
#define SENS_KEYBOARD	3
#define SENS_PROPERTY	4
#define SENS_MOUSE		5
#define SENS_COLLISION	6
#define SENS_RADAR		7
#define SENS_RANDOM     8
#define SENS_RAY        9
#define SENS_MESSAGE   10
#define SENS_JOYSTICK  11
#define SENS_ACTUATOR  12
#define SENS_DELAY     13
#define SENS_ARMATURE  14
/* sensor->flag */
#define SENS_SHOW		1
#define SENS_DEL		2
#define SENS_NEW		4
#define SENS_NOT		8
#define SENS_VISIBLE	16
#define SENS_PIN		32
#define SENS_DEACTIVATE	64

/* sensor->pulse */
#define SENS_PULSE_CONT 	0
#define SENS_PULSE_REPEAT	1
//#define SENS_PULSE_ONCE 	2
#define SENS_NEG_PULSE_MODE 4

/* sensor->suppress */
#define SENS_SUPPRESS_POSITIVE (1 << 0)
#define SENS_SUPPRESS_NEGATIVE (1 << 1)

/* collision, ray sensor modes: */
/* A little bit fake: when property is active, the first bit is
 * reset. Bite me :) So we don't actually use it, so we comment it out
 * ... The reason for this is that we need to be backward compatible,
 * and have a proper default value for this thing.
 * */
#define SENS_COLLISION_PROPERTY 0
#define SENS_COLLISION_MATERIAL 1
#define SENS_COLLISION_PULSE 2

/* ray specific mode */
/* X-Ray means that the ray will traverse objects that don't have the property/material */
#define SENS_RAY_PROPERTY		0
#define SENS_RAY_MATERIAL		1
#define SENS_RAY_XRAY			2

/* Some stuff for the mouse sensor Type: */
#define BL_SENS_MOUSE_LEFT_BUTTON    1
#define BL_SENS_MOUSE_MIDDLE_BUTTON  2
#define BL_SENS_MOUSE_RIGHT_BUTTON   4
#define BL_SENS_MOUSE_WHEEL_UP       5
#define BL_SENS_MOUSE_WHEEL_DOWN     6
#define BL_SENS_MOUSE_MOVEMENT       8
#define BL_SENS_MOUSE_MOUSEOVER      16
#define BL_SENS_MOUSE_MOUSEOVER_ANY	 32

/* Joystick sensor - sorted by axis types */
#define SENS_JOY_ANY_EVENT		1

#define SENS_JOY_BUTTON		0			/* axis type */
#define SENS_JOY_BUTTON_PRESSED	0
#define SENS_JOY_BUTTON_RELEASED	1

#define SENS_JOY_AXIS			1		/* axis type */
#define SENS_JOY_X_AXIS		0
#define SENS_JOY_Y_AXIS		1
#define SENS_JOY_NEG_X_AXIS     	2
#define SENS_JOY_NEG_Y_AXIS     	3
#define SENS_JOY_PRECISION		4

#define SENS_JOY_HAT			2		/* axis type */
#define SENS_JOY_HAT_DIR		0
#define SENS_JOY_HAT_UP			1
#define SENS_JOY_HAT_RIGHT		2
#define SENS_JOY_HAT_DOWN		4
#define SENS_JOY_HAT_LEFT		8

#define SENS_JOY_HAT_UP_RIGHT	SENS_JOY_HAT_UP | SENS_JOY_HAT_RIGHT
#define SENS_JOY_HAT_DOWN_RIGHT	SENS_JOY_HAT_DOWN | SENS_JOY_HAT_RIGHT
#define SENS_JOY_HAT_UP_LEFT	SENS_JOY_HAT_UP | SENS_JOY_HAT_LEFT
#define SENS_JOY_HAT_DOWN_LEFT	SENS_JOY_HAT_DOWN | SENS_JOY_HAT_LEFT


#define SENS_JOY_AXIS_SINGLE	3		/* axis type */


#define SENS_DELAY_REPEAT		1
// should match JOYINDEX_MAX in SCA_JoystickDefines.h */
#define SENS_JOY_MAXINDEX		8

#endif  /* __DNA_SENSOR_TYPES_H__ */
