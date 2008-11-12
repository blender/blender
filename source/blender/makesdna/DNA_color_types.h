/**
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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef DNA_COLOR_TYPES_H
#define DNA_COLOR_TYPES_H

#include "DNA_vec_types.h"

/* general defines for kernel functions */
#define CM_RESOL 32
#define CM_TABLE 256
#define CM_TABLEDIV		(1.0f/256.0f)

#define CM_TOT	4

typedef struct CurveMapPoint {
	float x, y;
	short flag, shorty;		/* shorty for result lookup */
} CurveMapPoint;

/* curvepoint->flag */
#define CUMA_SELECT		1
#define CUMA_VECTOR		2

typedef struct CurveMap {
	short totpoint, flag;
	
	float range;					/* quick multiply value for reading table */
	float mintable, maxtable;		/* the x-axis range for the table */
	float ext_in[2], ext_out[2];	/* for extrapolated curves, the direction vector */
	CurveMapPoint *curve;			/* actual curve */
	CurveMapPoint *table;			/* display and evaluate table */
	CurveMapPoint *premultable;		/* for RGB curves, premulled table */
} CurveMap;

/* cuma->flag */
#define CUMA_EXTEND_EXTRAPOLATE	1

typedef struct CurveMapping {
	int flag, cur;					/* cur; for buttons, to show active curve */
	
	rctf curr, clipr;				/* current rect, clip rect (is default rect too) */
	
	CurveMap cm[4];					/* max 4 builtin curves per mapping struct now */
	float black[3], white[3];		/* black/white point (black[0] abused for current frame) */
	float bwmul[3];					/* black/white point multiply value, for speed */
	
	float sample[3];				/* sample values, if flag set it draws line and intersection */
} CurveMapping;

/* cumapping->flag */
#define CUMA_DO_CLIP			1
#define CUMA_PREMULLED			2
#define CUMA_DRAW_CFRA			4
#define CUMA_DRAW_SAMPLE		8

#endif

