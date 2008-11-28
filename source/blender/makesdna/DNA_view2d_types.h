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
	rctf tot, cur;
	rcti vert, hor, mask;
	
	float min[2], max[2];
	float minzoom, maxzoom;
	
	short scroll, keeptot;			/* scroll - scrollbars to display (bitflag); keeptot - 'tot' rect  */
	short keepaspect, keepzoom;
	short oldwinx, oldwiny;
	
	int flag;						/* settings */
	
	float cursor[2]; 				/* only used in the UV view for now (for 2D-cursor) */
	short around;					/* pivot point for transforms (rotate and scale) */
	char pad[6];
} View2D;

/* ---------------------------------- */

/* v2d->keepzoom */
#define V2D_KEEPZOOM	0x0001	
#define V2D_LOCKZOOM_X	0x0100
#define V2D_LOCKZOOM_Y	0x0200

/* event codes for locking function */
#define V2D_LOCK_COPY		1
#define V2D_LOCK_REDRAW		2

/* v2d->flag */
#define V2D_VIEWLOCK	(1<<0)

/* scrollbar thickness */
	/* height */
#define SCROLLH	16
	/* width */
#define SCROLLB	16

#endif

