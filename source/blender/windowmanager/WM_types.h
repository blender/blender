/**
 * $Id:
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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef WM_TYPES_H
#define WM_TYPES_H

struct bContext;
struct wmEvent;
struct wmWindowManager;

/* exported types for WM */

#include "wm_cursors.h"
#include "wm_event_types.h"

/* ************** wmOperatorType ************************ */

/* flag */
#define OPTYPE_REGISTER		1	/* register operators in stack after finishing */
#define OPTYPE_UNDO			2	/* do undo push after after */
#define OPTYPE_BLOCKING		4	/* let blender grab all input from the WM (X11) */
#define OPTYPE_MACRO		8
#define OPTYPE_GRAB_POINTER	16	/* */

/* context to call operator in for WM_operator_name_call */
/* rna_ui.c contains EnumPropertyItem's of these, keep in sync */
enum {
	/* if there's invoke, call it, otherwise exec */
	WM_OP_INVOKE_DEFAULT,
	WM_OP_INVOKE_REGION_WIN,
	WM_OP_INVOKE_AREA,
	WM_OP_INVOKE_SCREEN,
	/* only call exec */
	WM_OP_EXEC_DEFAULT,
	WM_OP_EXEC_REGION_WIN,
	WM_OP_EXEC_AREA,
	WM_OP_EXEC_SCREEN
};

/* ************** wmKeyMap ************************ */

/* modifier */
#define KM_SHIFT	1
#define KM_CTRL		2
#define KM_ALT		4
#define KM_OSKEY	8
	/* means modifier should be pressed 2nd */
#define KM_SHIFT2	16
#define KM_CTRL2	32
#define KM_ALT2		64
#define KM_OSKEY2	128

/* type: defined in wm_event_types.c */
#define KM_TEXTINPUT	-2

/* val */
#define KM_ANY		-1
#define KM_NOTHING	0
#define KM_PRESS	1
#define KM_RELEASE	2


/* ************** UI Handler ***************** */

#define WM_UI_HANDLER_CONTINUE	0
#define WM_UI_HANDLER_BREAK		1

typedef int (*wmUIHandlerFunc)(struct bContext *C, struct wmEvent *event, void *userdata);
typedef void (*wmUIHandlerRemoveFunc)(struct bContext *C, void *userdata);

/* ************** Notifiers ****************** */

typedef struct wmNotifier {
	struct wmNotifier *next, *prev;
	
	struct wmWindowManager *wm;
	struct wmWindow *window;
	
	int swinid;
	unsigned int category, data, subtype, action;
	
	void *reference;
	
} wmNotifier;


/* 4 levels 

0xFF000000; category
0x00FF0000; data
0x0000FF00; data subtype (unused?)
0x000000FF; action
*/

/* category */
#define NOTE_CATEGORY		0xFF000000
#define	NC_WM				(1<<24)
#define	NC_WINDOW			(2<<24)
#define	NC_SCREEN			(3<<24)
#define	NC_SCENE			(4<<24)
#define	NC_OBJECT			(5<<24)
#define	NC_MATERIAL			(6<<24)
#define	NC_TEXTURE			(7<<24)
#define	NC_LAMP				(8<<24)
#define	NC_GROUP			(9<<24)
#define	NC_IMAGE			(10<<24)
#define	NC_BRUSH			(11<<24)
#define	NC_TEXT				(12<<24)
#define NC_WORLD			(13<<24)
#define NC_ANIMATION		(14<<24)
#define NC_SPACE			(15<<24)
#define NC_NODE				(15<<24)
#define	NC_GEOM				(16<<24)

/* data type, 256 entries is enough, it can overlap */
#define NOTE_DATA			0x00FF0000

	/* NC_WM windowmanager */
#define ND_FILEREAD			(1<<16)
#define ND_FILESAVE			(2<<16)
#define ND_DATACHANGED		(3<<16)

	/* NC_SCREEN screen */
#define ND_SCREENBROWSE		(1<<16)
#define ND_SCREENDELETE		(2<<16)
#define ND_SCREENCAST		(3<<16)
#define ND_ANIMPLAY			(4<<16)
#define ND_GPENCIL			(5<<16)

	/* NC_SCENE Scene */
#define ND_SCENEBROWSE		(1<<16)
#define	ND_MARKERS			(2<<16)
#define	ND_FRAME			(3<<16)
#define	ND_RENDER_OPTIONS	(4<<16)
#define	ND_NODES			(5<<16)
#define	ND_SEQUENCER		(6<<16)
#define ND_OB_ACTIVE		(7<<16)
#define ND_OB_SELECT		(8<<16)
#define ND_MODE				(9<<16)
#define ND_RENDER_RESULT	(10<<16)
#define ND_COMPO_RESULT		(11<<16)
#define ND_KEYINGSET		(12<<16)
#define ND_SCENEDELETE		(13<<16)
#define ND_LAYER			(14<<16)

	/* NC_OBJECT Object */
#define	ND_TRANSFORM		(16<<16)
#define ND_OB_SHADING		(17<<16)
#define ND_POSE				(18<<16)
#define ND_BONE_ACTIVE		(19<<16)
#define ND_BONE_SELECT		(20<<16)
#define ND_DRAW				(21<<16)
#define ND_MODIFIER			(22<<16) /* modifiers edited */
#define ND_KEYS				(23<<16)
#define ND_CONSTRAINT		(24<<16) /* constraints edited */
#define ND_PARTICLE_DATA	(25<<16) /* particles edited */
#define ND_PARTICLE_SELECT	(26<<16) /* particles selecting change */

	/* NC_MATERIAL Material */
#define	ND_SHADING			(30<<16)
#define	ND_SHADING_DRAW		(31<<16)

	/* NC_LAMP Lamp */
#define	ND_LIGHTING			(40<<16)
#define	ND_LIGHTING_DRAW	(41<<16)
#define ND_SKY				(42<<16)

	/* NC_WORLD World */
#define	ND_WORLD_DRAW		(45<<16)

	/* NC_TEXT Text */
#define ND_CURSOR			(50<<16)
#define ND_DISPLAY			(51<<16)
	
	/* NC_ANIMATION Animato */
#define ND_KEYFRAME_SELECT	(70<<16)
#define ND_KEYFRAME_EDIT	(71<<16)
#define ND_KEYFRAME_PROP	(72<<16)
#define ND_ANIMCHAN_SELECT	(73<<16)
#define ND_ANIMCHAN_EDIT	(74<<16)
#define ND_NLA_SELECT		(75<<16)
#define ND_NLA_EDIT			(76<<16)
#define ND_NLA_ACTCHANGE	(77<<16)

	/* NC_GEOM Geometry */
	/* Mesh, Curve, MetaBall, Armature, .. */
#define ND_SELECT			(80<<16)
#define ND_DATA				(81<<16)

	/* NC_SPACE */
#define ND_SPACE_CONSOLE		(1<<16) /* general redraw */
#define ND_SPACE_CONSOLE_REPORT	(2<<16) /* update for reports, could specify type */
#define ND_SPACE_INFO			(2<<16)
#define ND_SPACE_IMAGE			(3<<16)
#define ND_SPACE_FILE_PARAMS	(4<<16)
#define ND_SPACE_FILE_LIST		(5<<16)
#define ND_SPACE_NODE			(6<<16)
#define ND_SPACE_OUTLINER		(7<<16)
#define ND_SPACE_VIEW3D			(8<<16)
#define ND_SPACE_PROPERTIES		(9<<16)
#define ND_SPACE_TEXT			(10<<16)
#define ND_SPACE_TIME			(11<<16)
#define ND_SPACE_GRAPH			(12<<16)
#define ND_SPACE_DOPESHEET		(13<<16)
#define ND_SPACE_NLA			(14<<16)
#define ND_SPACE_SEQUENCER		(15<<16)
#define ND_SPACE_NODE_VIEW		(16<<16)

/* subtype, 256 entries too */
#define NOTE_SUBTYPE		0x0000FF00

/* subtype scene mode */
#define NS_MODE_OBJECT		(1<<8)

#define NS_EDITMODE_MESH	(2<<8)
#define NS_EDITMODE_CURVE	(3<<8)
#define NS_EDITMODE_SURFACE	(4<<8)
#define NS_EDITMODE_TEXT	(5<<8)
#define NS_EDITMODE_MBALL	(6<<8)
#define NS_EDITMODE_LATTICE	(7<<8)
#define NS_EDITMODE_ARMATURE	(8<<8)
#define NS_MODE_POSE		(9<<8)
#define NS_MODE_PARTICLE	(10<<8)


/* action classification */
#define NOTE_ACTION			(0x000000FF)
#define NA_EDITED			1
#define NA_EVALUATED		2
#define NA_ADDED			3
#define NA_REMOVED			4


/* ************** Gesture Manager data ************** */

/* wmGesture->type */
#define WM_GESTURE_TWEAK		0
#define WM_GESTURE_LINES		1
#define WM_GESTURE_RECT			2
#define WM_GESTURE_CROSS_RECT	3
#define WM_GESTURE_LASSO		4
#define WM_GESTURE_CIRCLE		5

/* wmGesture is registered to window listbase, handled by operator callbacks */
/* tweak gesture is builtin feature */
typedef struct wmGesture {
	struct wmGesture *next, *prev;
	int event_type;	/* event->type */
	int mode;		/* for modal callback */
	int type;		/* gesture type define */
	int swinid;		/* initial subwindow id where it started */
	int points;		/* optional, amount of points stored */
	
	void *customdata;
	/* customdata for border is a recti */
	/* customdata for circle is recti, (xmin, ymin) is center, xmax radius */
	/* customdata for lasso is short array */
} wmGesture;

/* ************** custom wmEvent data ************** */
typedef struct wmTabletData {
	int Active;			/* 0=EVT_TABLET_NONE, 1=EVT_TABLET_STYLUS, 2=EVT_TABLET_ERASER */
	float Pressure;		/* range 0.0 (not touching) to 1.0 (full pressure) */
	float Xtilt;		/* range 0.0 (upright) to 1.0 (tilted fully against the tablet surface) */
	float Ytilt;		/* as above */
} wmTabletData;

typedef struct wmTimer {
	struct wmTimer *next, *prev;
	
	struct wmWindow *win;	/* window this timer is attached to (optional) */

	double timestep;		/* set by timer user */
	int event_type;			/* set by timer user, goes to event system */
	void *customdata;		/* set by timer user, to allow custom values */
	
	double duration;		/* total running time in seconds */
	double delta;			/* time since previous step in seconds */
	
	double ltime;			/* internal, last time timer was activated */
	double ntime;			/* internal, next time we want to activate the timer */
	double stime;			/* internal, when the timer started */
	int sleep;				/* internal, put timers to sleep when needed */
} wmTimer;


/* **************** Paint Cursor ******************* */

typedef void (*wmPaintCursorDraw)(struct bContext *C, int, int, void *customdata);


/* ****************** Messages ********************* */

enum {
	WM_LOG_DEBUG				= 0,
	WM_LOG_INFO					= 1000,
	WM_LOG_WARNING				= 2000,
	WM_ERROR_UNDEFINED			= 3000,
	WM_ERROR_INVALID_INPUT		= 3001,
	WM_ERROR_INVALID_CONTEXT	= 3002,
	WM_ERROR_OUT_OF_MEMORY		= 3003
};

typedef struct wmReport {
	struct wmReport *next, *prev;
	int type;
	const char *typestr;
	char *message;
} wmReport;

/* *************** migrated stuff, clean later? ******************************** */

typedef struct RecentFile {
	struct RecentFile *next, *prev;
	char *filename;
} RecentFile;


#endif /* WM_TYPES_H */

