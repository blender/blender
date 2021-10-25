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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/WM_types.h
 *  \ingroup wm
 */

#ifndef __WM_TYPES_H__
#define __WM_TYPES_H__

/**
 * Overview of WM structs
 * ======================
 *
 * <pre>
 * > wmWindowManager    (window manager stores a list of windows)
 * > > wmWindow         (window has an active screen)
 * > > > bScreen        (link to ScrAreas via 'areabase')
 * > > > > ScrArea      (stores multiple spaces via space links via 'spacedata')
 * > > > > > SpaceLink  (base struct for space data for all different space types)
 * > > > > ScrArea      (stores multiple regions via 'regionbase')
 * > > > > > ARegion
 * </pre>
 *
 * Window Layout
 * =============
 *
 * <pre>
 * wmWindow -> bScreen
 * +----------------------------------------------------------+
 * |+-----------------------------------------+-------------+ |
 * ||ScrArea (links to 3D view)               |ScrArea      | |
 * ||+-------++----------+-------------------+|(links to    | |
 * |||ARegion||          |ARegion (quad view)|| properties) | |
 * |||(tools)||          |                   ||             | |
 * |||       ||          |                   ||             | |
 * |||       ||          |                   ||             | |
 * |||       ||          |                   ||             | |
 * |||       |+----------+-------------------+|             | |
 * |||       ||          |                   ||             | |
 * |||       ||          |                   ||             | |
 * |||       ||          |                   ||             | |
 * |||       ||          |                   ||             | |
 * |||       ||          |                   ||             | |
 * ||+-------++----------+-------------------+|             | |
 * |+-----------------------------------------+-------------+ |
 * +----------------------------------------------------------+
 * </pre>
 *
 * Space Data
 * ==========
 *
 * <pre>
 * ScrArea's store a list of space data (SpaceLinks), each of unique type.
 * The first one is the displayed in the UI, others are added as needed.
 *
 * +----------------------------+  <-- sa->spacedata.first;
 * |                            |
 * |                            |---+  <-- other inactive SpaceLink's stored.
 * |                            |   |
 * |                            |   |---+
 * |                            |   |   |
 * |                            |   |   |
 * |                            |   |   |
 * |                            |   |   |
 * +----------------------------+   |   |
 *    |                             |   |
 *    +-----------------------------+   |
 *       |                              |
 *       +------------------------------+
 * </pre>
 *
 * A common way to get the space from the ScrArea:
 * \code{.c}
 * if (sa->spacetype == SPACE_VIEW3D) {
 *     View3D *v3d = sa->spacedata.first;
 *     ...
 * }
 * \endcode
 */

#ifdef __cplusplus
extern "C" {
#endif

struct bContext;
struct wmEvent;
struct wmWindowManager;
struct wmOperator;
struct ImBuf;

#include "RNA_types.h"
#include "DNA_listBase.h"
#include "BLI_compiler_attrs.h"

/* exported types for WM */
#include "wm_cursors.h"
#include "wm_event_types.h"

/* ************** wmOperatorType ************************ */

/* flag */
enum {
	OPTYPE_REGISTER     = (1 << 0),  /* register operators in stack after finishing */
	OPTYPE_UNDO         = (1 << 1),  /* do undo push after after */
	OPTYPE_BLOCKING     = (1 << 2),  /* let blender grab all input from the WM (X11) */
	OPTYPE_MACRO        = (1 << 3),
	OPTYPE_GRAB_CURSOR  = (1 << 4),  /* grabs the cursor and optionally enables continuous cursor wrapping */
	OPTYPE_PRESET       = (1 << 5),  /* show preset menu */

	/* some operators are mainly for internal use
	 * and don't make sense to be accessed from the
	 * search menu, even if poll() returns true.
	 * currently only used for the search toolbox */
	OPTYPE_INTERNAL     = (1 << 6),

	OPTYPE_LOCK_BYPASS  = (1 << 7),  /* Allow operator to run when interface is locked */
	OPTYPE_UNDO_GROUPED = (1 << 8),  /* Special type of undo which doesn't store itself multiple times */
};

/* context to call operator in for WM_operator_name_call */
/* rna_ui.c contains EnumPropertyItem's of these, keep in sync */
enum {
	/* if there's invoke, call it, otherwise exec */
	WM_OP_INVOKE_DEFAULT,
	WM_OP_INVOKE_REGION_WIN,
	WM_OP_INVOKE_REGION_CHANNELS,
	WM_OP_INVOKE_REGION_PREVIEW,
	WM_OP_INVOKE_AREA,
	WM_OP_INVOKE_SCREEN,
	/* only call exec */
	WM_OP_EXEC_DEFAULT,
	WM_OP_EXEC_REGION_WIN,
	WM_OP_EXEC_REGION_CHANNELS,
	WM_OP_EXEC_REGION_PREVIEW,
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

/* KM_MOD_ flags for wmKeyMapItem and wmEvent.alt/shift/oskey/ctrl  */
/* note that KM_ANY and KM_NOTHING are used with these defines too */
#define KM_MOD_FIRST  1
#define KM_MOD_SECOND 2

/* type: defined in wm_event_types.c */
#define KM_TEXTINPUT	-2

/* val */
#define KM_ANY		-1
#define KM_NOTHING	0
#define KM_PRESS	1
#define KM_RELEASE	2
#define KM_CLICK	3
#define KM_DBL_CLICK	4


/* ************** UI Handler ***************** */

#define WM_UI_HANDLER_CONTINUE	0
#define WM_UI_HANDLER_BREAK		1

/* ************** Notifiers ****************** */

typedef struct wmNotifier {
	struct wmNotifier *next, *prev;
	
	struct wmWindowManager *wm;
	struct wmWindow *window;
	
	int swinid;			/* can't rely on this, notifiers can be added without context, swinid of 0 */
	unsigned int category, data, subtype, action;
	
	void *reference;
	
} wmNotifier;


/* 4 levels
 *
 * 0xFF000000; category
 * 0x00FF0000; data
 * 0x0000FF00; data subtype (unused?)
 * 0x000000FF; action
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
#define	NC_GEOM				(16<<24)
#define NC_NODE				(17<<24)
#define NC_ID				(18<<24)
#define NC_LOGIC			(19<<24)
#define NC_MOVIECLIP			(20<<24)
#define NC_MASK				(21<<24)
#define NC_GPENCIL			(22<<24)
#define NC_LINESTYLE			(23<<24)
#define NC_CAMERA			(24<<24)

/* data type, 256 entries is enough, it can overlap */
#define NOTE_DATA			0x00FF0000

	/* NC_WM windowmanager */
#define ND_FILEREAD			(1<<16)
#define ND_FILESAVE			(2<<16)
#define ND_DATACHANGED		(3<<16)
#define ND_HISTORY			(4<<16)
#define ND_JOB				(5<<16)
#define ND_UNDO				(6<<16)

	/* NC_SCREEN screen */
#define ND_SCREENBROWSE		(1<<16)
#define ND_SCREENDELETE		(2<<16)
#define ND_SCREENCAST		(3<<16)
#define ND_ANIMPLAY			(4<<16)
#define ND_GPENCIL			(5<<16)
#define ND_EDITOR_CHANGED	(6<<16) /*sent to new editors after switching to them*/
#define ND_SCREENSET		(7<<16)
#define ND_SKETCH			(8<<16)

	/* NC_SCENE Scene */
#define ND_SCENEBROWSE		(1<<16)
#define	ND_MARKERS			(2<<16)
#define	ND_FRAME			(3<<16)
#define	ND_RENDER_OPTIONS	(4<<16)
#define	ND_NODES			(5<<16)
#define	ND_SEQUENCER		(6<<16)
#define ND_OB_ACTIVE		(7<<16)
#define ND_OB_SELECT		(8<<16)
#define ND_OB_VISIBLE		(9<<16)
#define ND_OB_RENDER		(10<<16)
#define ND_MODE				(11<<16)
#define ND_RENDER_RESULT	(12<<16)
#define ND_COMPO_RESULT		(13<<16)
#define ND_KEYINGSET		(14<<16)
#define ND_TOOLSETTINGS		(15<<16)
#define ND_LAYER			(16<<16)
#define ND_FRAME_RANGE		(17<<16)
#define ND_TRANSFORM_DONE	(18<<16)
#define ND_WORLD			(92<<16)
#define ND_LAYER_CONTENT	(101<<16)

	/* NC_OBJECT Object */
#define	ND_TRANSFORM		(18<<16)
#define ND_OB_SHADING		(19<<16)
#define ND_POSE				(20<<16)
#define ND_BONE_ACTIVE		(21<<16)
#define ND_BONE_SELECT		(22<<16)
#define ND_DRAW				(23<<16)
#define ND_MODIFIER			(24<<16)
#define ND_KEYS				(25<<16)
#define ND_CONSTRAINT		(26<<16)
#define ND_PARTICLE			(27<<16)
#define ND_POINTCACHE		(28<<16)
#define ND_PARENT			(29<<16)
#define ND_LOD				(30<<16)
#define ND_DRAW_RENDER_VIEWPORT	(31<<16)  /* for camera & sequencer viewport update, also /w NC_SCENE */

	/* NC_MATERIAL Material */
#define	ND_SHADING			(30<<16)
#define	ND_SHADING_DRAW		(31<<16)
#define	ND_SHADING_LINKS	(32<<16)
#define	ND_SHADING_PREVIEW	(33<<16)

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
#define ND_KEYFRAME			(70<<16)
#define ND_KEYFRAME_PROP	(71<<16)
#define ND_ANIMCHAN			(72<<16)
#define ND_NLA				(73<<16)
#define ND_NLA_ACTCHANGE	(74<<16)
#define ND_FCURVES_ORDER	(75<<16)

	/* NC_GPENCIL */
#define ND_GPENCIL_EDITMODE	(85<<16)

	/* NC_GEOM Geometry */
	/* Mesh, Curve, MetaBall, Armature, .. */
#define ND_SELECT			(90<<16)
#define ND_DATA				(91<<16)
#define ND_VERTEX_GROUP		(92<<16)

	/* NC_NODE Nodes */

	/* NC_SPACE */
#define ND_SPACE_CONSOLE		(1<<16) /* general redraw */
#define ND_SPACE_INFO_REPORT	(2<<16) /* update for reports, could specify type */
#define ND_SPACE_INFO			(3<<16)
#define ND_SPACE_IMAGE			(4<<16)
#define ND_SPACE_FILE_PARAMS	(5<<16)
#define ND_SPACE_FILE_LIST		(6<<16)
#define ND_SPACE_NODE			(7<<16)
#define ND_SPACE_OUTLINER		(8<<16)
#define ND_SPACE_VIEW3D			(9<<16)
#define ND_SPACE_PROPERTIES		(10<<16)
#define ND_SPACE_TEXT			(11<<16)
#define ND_SPACE_TIME			(12<<16)
#define ND_SPACE_GRAPH			(13<<16)
#define ND_SPACE_DOPESHEET		(14<<16)
#define ND_SPACE_NLA			(15<<16)
#define ND_SPACE_SEQUENCER		(16<<16)
#define ND_SPACE_NODE_VIEW		(17<<16)
#define ND_SPACE_CHANGED		(18<<16) /*sent to a new editor type after it's replaced an old one*/
#define ND_SPACE_CLIP			(19<<16)
#define ND_SPACE_FILE_PREVIEW   (20<<16)

/* subtype, 256 entries too */
#define NOTE_SUBTYPE		0x0000FF00

/* subtype scene mode */
#define NS_MODE_OBJECT			(1<<8)

#define NS_EDITMODE_MESH		(2<<8)
#define NS_EDITMODE_CURVE		(3<<8)
#define NS_EDITMODE_SURFACE		(4<<8)
#define NS_EDITMODE_TEXT		(5<<8)
#define NS_EDITMODE_MBALL		(6<<8)
#define NS_EDITMODE_LATTICE		(7<<8)
#define NS_EDITMODE_ARMATURE	(8<<8)
#define NS_MODE_POSE			(9<<8)
#define NS_MODE_PARTICLE		(10<<8)

/* subtype 3d view editing */
#define NS_VIEW3D_GPU			(16<<8)

/* action classification */
#define NOTE_ACTION			(0x000000FF)
#define NA_EDITED			1
#define NA_EVALUATED		2
#define NA_ADDED			3
#define NA_REMOVED			4
#define NA_RENAME			5
#define NA_SELECTED			6
#define NA_PAINTING			7

/* ************** Gesture Manager data ************** */

/* wmGesture->type */
#define WM_GESTURE_TWEAK		0
#define WM_GESTURE_LINES		1
#define WM_GESTURE_RECT			2
#define WM_GESTURE_CROSS_RECT	3
#define WM_GESTURE_LASSO		4
#define WM_GESTURE_CIRCLE		5
#define WM_GESTURE_STRAIGHTLINE	6

/* wmGesture is registered to window listbase, handled by operator callbacks */
/* tweak gesture is builtin feature */
typedef struct wmGesture {
	struct wmGesture *next, *prev;
	int event_type;	/* event->type */
	int mode;		/* for modal callback */
	int type;		/* gesture type define */
	int swinid;		/* initial subwindow id where it started */
	int points;		/* optional, amount of points stored */
	int size;		/* optional, maximum amount of points stored */
	
	void *customdata;
	/* customdata for border is a recti */
	/* customdata for circle is recti, (xmin, ymin) is center, xmax radius */
	/* customdata for lasso is short array */
	/* customdata for straight line is a recti: (xmin,ymin) is start, (xmax, ymax) is end */

	/* free pointer to use for operator allocs (if set, its freed on exit)*/
	void *userdata;
} wmGesture;

/* ************** wmEvent ************************ */

/* each event should have full modifier state */
/* event comes from eventmanager and from keymap */
typedef struct wmEvent {
	struct wmEvent *next, *prev;
	
	short type;			/* event code itself (short, is also in keymap) */
	short val;			/* press, release, scrollvalue */
	int x, y;			/* mouse pointer position, screen coord */
	int mval[2];		/* region mouse position, name convention pre 2.5 :) */
	char utf8_buf[6];	/* from, ghost if utf8 is enabled for the platform,
						 * BLI_str_utf8_size() must _always_ be valid, check
						 * when assigning s we don't need to check on every access after */
	char ascii;			/* from ghost, fallback if utf8 isn't set */
	char pad;

	/* previous state, used for double click and the 'click' */
	short prevtype;
	short prevval;
	int prevx, prevy;
	double prevclicktime;
	int prevclickx, prevclicky;
	
	/* modifier states */
	short shift, ctrl, alt, oskey;	/* oskey is apple or windowskey, value denotes order of pressed */
	short keymodifier;				/* rawkey modifier */
	
	/* set in case a KM_PRESS went by unhandled */
	short check_click;
	
	/* keymap item, set by handler (weak?) */
	const char *keymap_idname;

	/* tablet info, only use when the tablet is active */
	const struct wmTabletData *tablet_data;

	/* custom data */
	short custom;		/* custom data type, stylus, 6dof, see wm_event_types.h */
	short customdatafree;
	int pad2;
	void *customdata;	/* ascii, unicode, mouse coords, angles, vectors, dragdrop info */
	
} wmEvent;

/* ************** custom wmEvent data ************** */
typedef struct wmTabletData {
	int Active;			/* 0=EVT_TABLET_NONE, 1=EVT_TABLET_STYLUS, 2=EVT_TABLET_ERASER */
	float Pressure;		/* range 0.0 (not touching) to 1.0 (full pressure) */
	float Xtilt;		/* range 0.0 (upright) to 1.0 (tilted fully against the tablet surface) */
	float Ytilt;		/* as above */
} wmTabletData;

typedef enum {  /* motion progress, for modal handlers */
	P_NOT_STARTED,
	P_STARTING,    /* <-- */
	P_IN_PROGRESS, /* <-- only these are sent for NDOF motion*/
	P_FINISHING,   /* <-- */
	P_FINISHED
} wmProgress;

#ifdef WITH_INPUT_NDOF
typedef struct wmNDOFMotionData {
	/* awfully similar to GHOST_TEventNDOFMotionData... */
	/* Each component normally ranges from -1 to +1, but can exceed that.
	 * These use blender standard view coordinates, with positive rotations being CCW about the axis. */
	float tvec[3]; /* translation */
	float rvec[3]; /* rotation: */
	/* axis = (rx,ry,rz).normalized */
	/* amount = (rx,ry,rz).magnitude [in revolutions, 1.0 = 360 deg] */
	float dt; /* time since previous NDOF Motion event */
	wmProgress progress; /* is this the first event, the last, or one of many in between? */
} wmNDOFMotionData;
#endif /* WITH_INPUT_NDOF */

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

typedef struct wmOperatorType {
	const char *name;		/* text for ui, undo */
	const char *idname;		/* unique identifier */
	const char *translation_context;
	const char *description;	/* tooltips and python docs */
	const char *undo_group;	/* identifier to group operators together */

	/* this callback executes the operator without any interactive input,
	 * parameters may be provided through operator properties. cannot use
	 * any interface code or input device state.
	 * - see defines below for return values */
	int (*exec)(struct bContext *, struct wmOperator *) ATTR_WARN_UNUSED_RESULT;

	/* this callback executes on a running operator whenever as property
	 * is changed. It can correct its own properties or report errors for
	 * invalid settings in exceptional cases.
	 * Boolean return value, True denotes a change has been made and to redraw */
	bool (*check)(struct bContext *, struct wmOperator *);

	/* for modal temporary operators, initially invoke is called. then
	 * any further events are handled in modal. if the operation is
	 * canceled due to some external reason, cancel is called
	 * - see defines below for return values */
	int (*invoke)(struct bContext *, struct wmOperator *, const struct wmEvent *) ATTR_WARN_UNUSED_RESULT;

	/* Called when a modal operator is canceled (not used often).
	 * Internal cleanup can be done here if needed. */
	void (*cancel)(struct bContext *, struct wmOperator *);

	/* Modal is used for operators which continuously run, eg:
	 * fly mode, knife tool, circle select are all examples of modal operators.
	 * Modal operators can handle events which would normally access other operators,
	 * they keep running until they don't return `OPERATOR_RUNNING_MODAL`. */
	int (*modal)(struct bContext *, struct wmOperator *, const struct wmEvent *) ATTR_WARN_UNUSED_RESULT;

	/* verify if the operator can be executed in the current context, note
	 * that the operator might still fail to execute even if this return true */
	int (*poll)(struct bContext *) ATTR_WARN_UNUSED_RESULT;

	/* optional panel for redo and repeat, autogenerated if not set */
	void (*ui)(struct bContext *, struct wmOperator *);

	/* rna for properties */
	struct StructRNA *srna;

	/* previous settings - for initializing on re-use */
	struct IDProperty *last_properties;

	/* Default rna property to use for generic invoke functions.
	 * menus, enum search... etc. Example: Enum 'type' for a Delete menu */
	PropertyRNA *prop;

	/* struct wmOperatorTypeMacro */
	ListBase macro;

	/* pointer to modal keymap, do not free! */
	struct wmKeyMap *modalkeymap;

	/* python needs the operator type as well */
	int (*pyop_poll)(struct bContext *, struct wmOperatorType *ot) ATTR_WARN_UNUSED_RESULT;

	/* RNA integration */
	ExtensionRNA ext;

	/* Flag last for padding */
	short flag;

} wmOperatorType;

#ifdef WITH_INPUT_IME
/* *********** Input Method Editor (IME) *********** */

/* similar to GHOST_TEventImeData */
typedef struct wmIMEData {
	size_t result_len, composite_len;

	char *str_result;           /* utf8 encoding */
	char *str_composite;        /* utf8 encoding */

	int cursor_pos;             /* cursor position in the IME composition. */
	int sel_start;              /* beginning of the selection */
	int sel_end;                /* end of the selection */

	bool is_ime_composing;
} wmIMEData;
#endif

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
	const char *typestr;
	char *message;
	int type;
} wmReport;

/* *************** Drag and drop *************** */

#define WM_DRAG_ID		0
#define WM_DRAG_RNA		1
#define WM_DRAG_PATH	2
#define WM_DRAG_NAME	3
#define WM_DRAG_VALUE	4
#define WM_DRAG_COLOR	5

typedef enum wmDragFlags {
	WM_DRAG_NOP         = 0,
	WM_DRAG_FREE_DATA   = 1,
} wmDragFlags;

/* note: structs need not exported? */

typedef struct wmDrag {
	struct wmDrag *next, *prev;
	
	int icon, type;					/* type, see WM_DRAG defines above */
	void *poin;
	char path[1024]; /* FILE_MAX */
	double value;
	
	struct ImBuf *imb;						/* if no icon but imbuf should be drawn around cursor */
	float scale;
	int sx, sy;
	
	char opname[200]; /* if set, draws operator name*/
	unsigned int flags;
} wmDrag;

/* dropboxes are like keymaps, part of the screen/area/region definition */
/* allocation and free is on startup and exit */
typedef struct wmDropBox {
	struct wmDropBox *next, *prev;
	
	/* test if the dropbox is active, then can print optype name */
	int (*poll)(struct bContext *, struct wmDrag *, const wmEvent *);

	/* before exec, this copies drag info to wmDrop properties */
	void (*copy)(struct wmDrag *, struct wmDropBox *);
	
	/* if poll survives, operator is called */
	wmOperatorType *ot;				/* not saved in file, so can be pointer */

	struct IDProperty *properties;	/* operator properties, assigned to ptr->data and can be written to a file */
	struct PointerRNA *ptr;			/* rna pointer to access properties */

	short opcontext;				/* default invoke */

} wmDropBox;

/* *************** migrated stuff, clean later? ************** */

typedef struct RecentFile {
	struct RecentFile *next, *prev;
	char *filepath;
} RecentFile;


#ifdef __cplusplus
}
#endif

#endif /* __WM_TYPES_H__ */

