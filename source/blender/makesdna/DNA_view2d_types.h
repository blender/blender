/**
 * blenlib/DNA_view2d_types.h (mar-2001 nzc)
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
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef DNA_VIEW2D_TYPES_H
#define DNA_VIEW2D_TYPES_H

#include "DNA_vec_types.h"

/* ---------------------------------- */

/* View 2D data - stored per region */
typedef struct View2D {
	rctf tot, cur;					/* tot - area that data can be drawn in; cur - region of tot that is visible in viewport */
	rcti vert, hor;					/* vert - vertical scrollbar region; hor - horizontal scrollbar region */
	rcti mask;						/* mask - region (in screenspace) within which 'cur' can be viewed */
	
	float min[2], max[2];			/* min/max sizes? */
	float minzoom, maxzoom;			/* self explanatory. allowable zoom factor range */
	
	short scroll;					/* scroll - scrollbars to display (bitflag) */
	short keeptot;					/* keeptot - 'tot' rect  */
	short keepaspect, keepzoom;		/* axes that zoomimg cannot occur on, and need to maintain aspect ratio */
	short keepofs;					/* keepofs - axes that translation is not allowed to occur on */
	
	short flag;						/* settings */
	
	short oldwinx, oldwiny;			/* storage of previous winx/winy values encountered by UI_view2d_enforce_status(), for keepaspect */
	
	float cursor[2]; 				/* only used in the UV view for now (for 2D-cursor) */
	short around;					/* pivot point for transforms (rotate and scale) */
	char pad[6];
} View2D;

/* ---------------------------------- */

/* v2d->keepzoom */
#define V2D_KEEPZOOM	0x0001	
#define V2D_LOCKZOOM_X	0x0100
#define V2D_LOCKZOOM_Y	0x0200

/* v2d->keepofs */
#define V2D_LOCKOFS_X	(1<<1)
#define V2D_LOCKOFS_Y	(1<<2)

/* event codes for locking function */
#define V2D_LOCK_COPY		1
#define V2D_LOCK_REDRAW		2

/* v2d->flag */
#define V2D_VIEWLOCK	(1<<0)

/* scrollbar thickness */
#define V2D_SCROLL_HEIGHT	16
#define V2D_SCROLL_WIDTH	16

/* scrollbar flags for View2D */
	/* left scrollbar */
#define V2D_SCROLL_LEFT 		(1<<0)		
#define V2D_SCROLL_RIGHT 		(1<<1)
#define V2D_SCROLL_VERTICAL 	(V2D_SCROLL_LEFT|V2D_SCROLL_RIGHT)
	/* horizontal scrollbar */
#define V2D_SCROLL_TOP 		(1<<2)
#define V2D_SCROLL_BOTTOM 		(1<<3)
#define V2D_SCROLL_HORIZONTAL  	(V2D_SCROLL_TOP|V2D_SCROLL_BOTTOM)
	/* special hacks for outliner hscroll - prevent hanging older versions of Blender */
#define V2D_SCROLL_BOTTOM_O   	(1<<4)
#define V2D_SCROLL_HORIZONTAL_O 	(V2D_SCROLL_BOTTOM|V2D_SCROLL_BOTTOM_O)
	/* scale markings - vertical */
#define V2D_SCROLL_SCALE_LEFT	(1<<5)
#define V2D_SCROLL_SCALE_RIGHT	(1<<6)
#define V2D_SCROLL_SCALE_VERTICAL	(V2D_SCROLL_SCALE_LEFT|V2D_SCROLL_SCALE_RIGHT)
	/* scale markings - horizontal */
#define V2D_SCROLL_SCALE_BOTTOM	(1<<7)
#define V2D_SCROLL_SCALE_TOP	(1<<8)	
#define V2D_SCROLL_SCALE_HORIZONTAL	(V2D_SCROLL_SCALE_BOTTOM|V2D_SCROLL_SCALE_TOP)

#endif

