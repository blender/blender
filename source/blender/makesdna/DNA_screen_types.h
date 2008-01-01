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
struct wmSubWindow;

typedef struct bScreen {
	ID id;
	
	ListBase vertbase, edgebase, areabase;
	struct Scene *scene;
	short startx, endx, starty, endy;	/* framebuffer coords */
	short sizex, sizey;
	short scenenr, screennr;			/* only for pupmenu */
	short full, winid;					/* winid from WM, starts with 1 */
	int pad;
	short handler[8];					/* similar to space handler now */
	
	struct wmSubWindow *mainwin;		/* screensize subwindow, for screenedges */
	struct wmSubWindow *subwinactive;	/* active subwindow */
	
	ListBase handlers;					/* wmEventHandler */
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
	float winmat[4][4];
	rcti totrct, headrct, winrct;

	int pad;
	short headertype;		/* 0=no header, 1= down, 2= up */
	char spacetype, butspacetype;	/* SPACE_...  */
	short winx, winy;		/* size */
	char head_swap, head_equal;
	char win_swap, win_equal;
	
	short headbutlen, headbutofs;
	short cursor, flag;

	ScriptLink scriptlink;

	ListBase spacedata;
	ListBase uiblocks;
	ListBase panels;
	ListBase regionbase;	/* ARegion */
	ListBase handlers;		/* wmEventHandler */
} ScrArea;

typedef struct ARegion {
	struct ARegion *next, *prev;
	
	rcti winrct;
	struct wmSubWindow *subwin;
	
	ListBase handlers;
	
} ARegion;

#define MAXWIN		128

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

#endif

