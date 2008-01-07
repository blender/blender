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
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef DNA_SCREEN_TYPES_H
#define DNA_SCREEN_TYPES_H

#include "DNA_listBase.h"
#include "DNA_vec_types.h"

#include "DNA_ID.h"
#include "DNA_scriptlink_types.h"

struct Scene;
struct SpaceType;
struct SpaceLink;
struct ARegionType;
struct bContext;
struct wmNotifier;
struct wmWindowManager;
		

typedef struct bScreen {
	ID id;
	
	ListBase vertbase, edgebase, areabase;
	struct Scene *scene;
	
	short scenenr, screennr;			/* only for pupmenu */
	
	short full, winid;					/* winid from WM, starts with 1 */
	short do_draw;						/* notifier for drawing edges */
	short do_refresh;					/* notifier for scale screen, changed screen, etc */
	
	short mainwin;						/* screensize subwindow, for screenedges and global menus */
	short subwinactive;					/* active subwindow */
	
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

#ifndef DNA_USHORT_FIX
#define DNA_USHORT_FIX
/**
 * @deprecated This typedef serves to avoid badly typed functions when
 * @deprecated compiling while delivering a proper dna.c. Do not use
 * @deprecated it in any case.
 */
typedef unsigned short dna_ushort_fix;
#endif



typedef struct Panel {		/* the part from uiBlock that needs saved in file */
	struct Panel *next, *prev;

	char panelname[64], tabname[64];	/* defined as UI_MAX_NAME_STR */
	char drawname[64];					/* panelname is identifier for restoring location */
	short ofsx, ofsy, sizex, sizey;
	short flag, active;					/* active= used currently by a uiBlock */
	short control;
	short snap;
	short old_ofsx, old_ofsy;		/* for stow */
	int sortcounter;			/* when sorting panels, it uses this to put new ones in right place */
	struct Panel *paneltab;		/* this panel is tabbed in *paneltab */
} Panel;

typedef struct ScrArea {
	struct ScrArea *next, *prev;
	
	ScrVert *v1, *v2, *v3, *v4;
	bScreen *full;			/* if area==full, this is the parent */

	rcti totrct;			/* rect bound by v1 v2 v3 v4 */
	rcti headrct, winrct;	/* OLD! gets converted to region */

	char spacetype, butspacetype;	/* SPACE_..., butspacetype is button arg  */
	short winx, winy;				/* size */
	
	short headertype;				/* OLD! 0=no header, 1= down, 2= up */
	short headbutlen, headbutofs;	/* OLD! */
	short cursor, flag;

	ScriptLink scriptlink;
	
	struct SpaceType *type;		/* callbacks for this space type */
	
	ListBase spacedata;
	ListBase uiblocks;
	ListBase panels;
	ListBase regionbase;	/* ARegion */
	ListBase handlers;		/* wmEventHandler */
} ScrArea;

typedef struct ARegion {
	struct ARegion *next, *prev;
	
	rcti winrct;
	short swinid;
	short regiontype;			/* window, header, etc. identifier for drawing */
	short alignment;			/* how it should split */
	short size;					/* current split size in pixels */
	short minsize;				/* set by spacedata's region init */
	short flag;					/* hide, ... */
	
	float fsize;				/* current split size in float */
	
	int pad;
	short do_draw, do_refresh;	/* cached notifier events */
	
	struct ARegionType *type;	/* callbacks for this region type */
	
	ListBase handlers;
	
} ARegion;


/* area->flag */
#define HEADER_NO_PULLDOWN	1

/* If you change EDGEWIDTH, also do the global arrat edcol[]  */
#define EDGEWIDTH	1
#define EDGEWIDTH2	0
#define AREAGRID	4
#define AREAMINX	32
#define HEADERY		26
#define AREAMINY	(HEADERY+EDGEWIDTH)

#define HEADERDOWN	1
#define HEADERTOP	2

#define SCREENNORMAL    0
#define SCREENFULL      1
#define SCREENAUTOPLAY  2

/* sa->win_swap */
#define WIN_FRONT_OK	1
#define WIN_BACK_OK		2
#define WIN_EQUAL		3

#define L_SCROLL 1			/* left scrollbar */
#define R_SCROLL 2
#define VERT_SCROLL 3
#define T_SCROLL 4
#define B_SCROLL 8
#define HOR_SCROLL  12
#define B_SCROLLO   16		/* special hack for outliner hscroll - prevent hanging */
#define HOR_SCROLLO 20		/*        in older versions of blender 			     */

/* Panel->snap - for snapping to screen edges */
#define PNL_SNAP_NONE		0
#define PNL_SNAP_TOP		1
#define PNL_SNAP_RIGHT		2
#define PNL_SNAP_BOTTOM	4
#define PNL_SNAP_LEFT		8

#define PNL_SNAP_DIST		9.0

/* screen handlers */
#define SCREEN_MAXHANDLER		8

#define SCREEN_HANDLER_ANIM		1
#define SCREEN_HANDLER_PYTHON   2
#define SCREEN_HANDLER_VERSE	3

/* regiontype, first two are the default set */
#define RGN_TYPE_WINDOW		0
#define RGN_TYPE_HEADER		1

/* region alignment */
#define RGN_ALIGN_NONE		0
#define RGN_ALIGN_TOP		1
#define RGN_ALIGN_BOTTOM	2
#define RGN_ALIGN_LEFT		3
#define RGN_ALIGN_RIGHT		4
#define RGN_ALIGN_HSPLIT	5
#define RGN_ALIGN_VSPLIT	6

/* region flag */
#define RGN_FLAG_HIDDEN		1
#define RGN_FLAG_TOO_SMALL	2


#endif

