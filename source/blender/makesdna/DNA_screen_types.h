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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef DNA_SCREEN_TYPES_H
#define DNA_SCREEN_TYPES_H

#include "DNA_listBase.h"
#include "DNA_view2d_types.h"
#include "DNA_vec_types.h"

#include "DNA_ID.h"

struct SpaceType;
struct SpaceLink;
struct ARegion;
struct ARegionType;
struct PanelType;
struct HeaderType;
struct Scene;
struct uiLayout;
struct wmTimer;

typedef struct bScreen {
	ID id;
	
	ListBase vertbase;					/* screens have vertices/edges to define areas */
	ListBase edgebase;
	ListBase areabase;
	ListBase regionbase;				/* screen level regions (menus), runtime only */
	
	struct Scene *scene;
	struct Scene *newscene;				/* temporary when switching */
	
	short full;							/* fade out? */
	short winid;						/* winid from WM, starts with 1 */
	short do_draw;						/* notifier for drawing edges */
	short do_refresh;					/* notifier for scale screen, changed screen, etc */
	short do_draw_gesture;				/* notifier for gesture draw. */
	short do_draw_paintcursor;			/* notifier for paint cursor draw. */
	short do_draw_drag;					/* notifier for dragging draw. */
	short swap;							/* indicator to survive swap-exchange systems */
	
	short mainwin;						/* screensize subwindow, for screenedges and global menus */
	short subwinactive;					/* active subwindow */
	
	int pad2;
	
	struct wmTimer *animtimer;			/* if set, screen has timer handler added in window */
	void *context;						/* context callback */
	
	short handler[8];					/* similar to space handler */
} bScreen;

typedef struct ScrVert {
	struct ScrVert *next, *prev, *newv;
	vec2s vec;
	int flag;
} ScrVert;

typedef struct ScrEdge {
	struct ScrEdge *next, *prev;
	ScrVert *v1, *v2;
	short border;			/* 1 when at edge of screen */
	short flag;
	int pad;
} ScrEdge;

typedef struct Panel {		/* the part from uiBlock that needs saved in file */
	struct Panel *next, *prev;

	struct PanelType *type;			/* runtime */
	struct uiLayout *layout;		/* runtime for drawing */

	char panelname[64], tabname[64];	/* defined as UI_MAX_NAME_STR */
	char drawname[64];					/* panelname is identifier for restoring location */
	short ofsx, ofsy, sizex, sizey;
	short labelofs, pad;
	short flag, runtime_flag;
	short control;
	short snap;
	int sortorder;			/* panels are aligned according to increasing sortorder */
	struct Panel *paneltab;		/* this panel is tabbed in *paneltab */
	void *activedata;			/* runtime for panel manipulation */

	int list_scroll, list_size;
	int list_last_len, list_grip_size;
	char list_search[64];
} Panel;

typedef struct ScrArea {
	struct ScrArea *next, *prev;
	
	ScrVert *v1, *v2, *v3, *v4;
	bScreen *full;			/* if area==full, this is the parent */

	rcti totrct;			/* rect bound by v1 v2 v3 v4 */

	char spacetype, butspacetype;	/* SPACE_..., butspacetype is button arg  */
	short winx, winy;				/* size */
	
	short headertype;				/* OLD! 0=no header, 1= down, 2= up */
	short pad;
	short do_refresh;				/* private, for spacetype refresh callback */
	short cursor, flag;
	
	struct SpaceType *type;		/* callbacks for this space type */
	
	ListBase spacedata;		/* SpaceLink */
	ListBase regionbase;	/* ARegion */
	ListBase handlers;		/* wmEventHandler */
	
	ListBase actionzones;	/* AZone */
} ScrArea;

typedef struct ARegion {
	struct ARegion *next, *prev;
	
	View2D v2d;					/* 2D-View scrolling/zoom info (most regions are 2d anyways) */
	rcti winrct;				/* coordinates of region */
	rcti drawrct;				/* runtime for partial redraw, same or smaller than winrct */
	short winx, winy;			/* size */
	
	short swinid;
	short regiontype;			/* window, header, etc. identifier for drawing */
	short alignment;			/* how it should split */
	short flag;					/* hide, ... */
	
	float fsize;				/* current split size in float (unused) */
	short sizex, sizey;			/* current split size in pixels (if zero it uses regiontype) */
	
	short do_draw;				/* private, cached notifier events */
	short swap;					/* private, indicator to survive swap-exchange */
	
	struct ARegionType *type;	/* callbacks for this region type */
	
	ListBase uiblocks;			/* uiBlock */
	ListBase panels;			/* Panel */
	ListBase handlers;			/* wmEventHandler */
	
	char *headerstr;			/* use this string to draw info */
	void *regiondata;			/* XXX 2.50, need spacedata equivalent? */
} ARegion;

/* swap */
#define WIN_BACK_OK		1
#define WIN_FRONT_OK	2
#define WIN_EQUAL		3

/* area->flag */
#define HEADER_NO_PULLDOWN		1
#define AREA_FLAG_DRAWJOINTO	2
#define AREA_FLAG_DRAWJOINFROM	4
#define AREA_TEMP_INFO			8

/* If you change EDGEWIDTH, also do the global arrat edcol[]  */
#define EDGEWIDTH	1
#define AREAGRID	4
#define AREAMINX	32
#define HEADERY		26
#define AREAMINY	(HEADERY+EDGEWIDTH)

#define HEADERDOWN	1
#define HEADERTOP	2

#define SCREENNORMAL    0
#define SCREENFULL      1
#define SCREENAUTOPLAY  2
#define SCREENTEMP		3


/* Panel->snap - for snapping to screen edges */
#define PNL_SNAP_NONE		0
#define PNL_SNAP_TOP		1
#define PNL_SNAP_RIGHT		2
#define PNL_SNAP_BOTTOM	4
#define PNL_SNAP_LEFT		8

#define PNL_SNAP_DIST		9.0

/* paneltype flag */
#define PNL_DEFAULT_CLOSED		1
#define PNL_NO_HEADER			2

/* screen handlers */
#define SCREEN_MAXHANDLER		8

#define SCREEN_HANDLER_ANIM		1
#define SCREEN_HANDLER_PYTHON   2
#define SCREEN_HANDLER_VERSE	3

/* regiontype, first two are the default set */
enum {
	RGN_TYPE_WINDOW = 0,
	RGN_TYPE_HEADER,
	RGN_TYPE_CHANNELS,
	RGN_TYPE_TEMPORARY,
	RGN_TYPE_UI,
	RGN_TYPE_TOOLS,
	RGN_TYPE_TOOL_PROPS,
	RGN_TYPE_PREVIEW
};

/* region alignment */
#define RGN_ALIGN_NONE		0
#define RGN_ALIGN_TOP		1
#define RGN_ALIGN_BOTTOM	2
#define RGN_ALIGN_LEFT		3
#define RGN_ALIGN_RIGHT		4
#define RGN_ALIGN_HSPLIT	5
#define RGN_ALIGN_VSPLIT	6
#define RGN_ALIGN_FLOAT		7
#define RGN_ALIGN_QSPLIT	8
#define RGN_OVERLAP_TOP		9
#define RGN_OVERLAP_BOTTOM	10
#define RGN_OVERLAP_LEFT	11
#define RGN_OVERLAP_RIGHT	12

#define RGN_SPLIT_PREV		32

/* region flag */
#define RGN_FLAG_HIDDEN		1
#define RGN_FLAG_TOO_SMALL	2

#endif

