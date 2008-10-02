/**
 * blenlib/DNA_curve_types.h (mar-2001 nzc)
 *
 * Curve stuff.
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
#ifndef DNA_CURVE_TYPES_H
#define DNA_CURVE_TYPES_H

#include "DNA_listBase.h"
#include "DNA_vec_types.h"
#include "DNA_ID.h"

#define MAXTEXTBOX 256  /* used in readfile.c and editfont.c */

struct BoundBox;
struct Object;
struct Ipo;
struct Key;
struct Material;
struct VFont;

/* These two Lines with # tell makesdna this struct can be excluded. */
#
#
typedef struct Path {
	int len;
	float *data;
	float totdist;
} Path;

/* These two Lines with # tell makesdna this struct can be excluded. */
#
#
typedef struct BevList {
	struct BevList *next, *prev;
	int nr, flag;
	short poly, gat;
} BevList;

/* These two Lines with # tell makesdna this struct can be excluded. */
#
#
typedef struct BevPoint {
	float x, y, z, alfa, radius, sina, cosa, mat[3][3];
	short f1, f2;
} BevPoint;

/* Keyframes on IPO curves and Points on Bezier Curves/Paths are generally BezTriples */
/* note: alfa location in struct is abused by Key system */
/* vec in BezTriple looks like this:
	vec[0][0]=x location of handle 1
	vec[0][1]=y location of handle 1
	vec[0][2]=z location of handle 1 (not used for IpoCurve Points(2d))
	vec[1][0]=x location of control point
	vec[1][1]=y location of control point
	vec[1][2]=z location of control point
	vec[2][0]=x location of handle 2
	vec[2][1]=y location of handle 2
	vec[2][2]=z location of handle 2 (not used for IpoCurve Points(2d))
*/
typedef struct BezTriple {
	float vec[3][3];
	float alfa, weight, radius;	/* alfa: tilt in 3D View, weight: used for softbody goal weight, radius: for bevel tapering */
	short h1, h2; 				/* h1, h2: the handle type of the two handles */
	char f1, f2, f3, hide;		/* f1, f2, f3: used for selection status,  hide: used to indicate whether BezTriple is hidden */
} BezTriple;

/* note; alfa location in struct is abused by Key system */
typedef struct BPoint {
	float vec[4];
	float alfa, weight;		/* alfa: tilt in 3D View, weight: used for softbody goal weight */
	short f1, hide;			/* f1: selection status,  hide: is point hidden or not */
	float radius, pad;		/* user-set radius per point for bevelling etc */
} BPoint;

typedef struct Nurb {
	struct Nurb *next, *prev;	/* multiple nurbs per curve object are allowed */
	short type;
	short mat_nr;		/* index into material list */
	short hide, flag;
	short pntsu, pntsv;		/* number of points in the U or V directions */
	short resolu, resolv;	/* tesselation resolution in the U or V directions */
	short orderu, orderv;
	short flagu, flagv;
	
	float *knotsu, *knotsv;
	BPoint *bp;
	BezTriple *bezt;

	short tilt_interp;	/* KEY_LINEAR, KEY_CARDINAL, KEY_BSPLINE */
	short radius_interp;
	
	int charidx;
} Nurb;

typedef struct CharInfo {
	short kern;
	short mat_nr;
	char flag;
	char pad;
	short pad2;
} CharInfo;

typedef struct TextBox {
	float x, y, w, h;
} TextBox;

typedef struct Curve {
	ID id;
	
	struct BoundBox *bb;
	
	ListBase nurb;
	ListBase disp;
	struct Object *bevobj, *taperobj, *textoncurve;
	struct Ipo *ipo;
	Path *path;
	struct Key *key;
	struct Material **mat;
	
	ListBase bev;
	
	/* texture space, copied as one block in editobject.c */
	float loc[3];
	float size[3];
	float rot[3];

	int texflag;

	short pathlen, totcol;
	short flag, bevresol;
	float width, ext1, ext2;
	
	/* default */
	short resolu, resolv;
	short resolu_ren, resolv_ren;
	int pad2;
	
	/* font part */
	short len, lines, pos, spacemode;
	float spacing, linedist, shear, fsize, wordspace, ulpos, ulheight;
	float xof, yof;
	float linewidth;

	char *str;
	char family[24];
	struct VFont *vfont;
	struct VFont *vfontb;
	struct VFont *vfonti;
	struct VFont *vfontbi;

	int sepchar;
	
	int totbox, actbox, pad;
	struct TextBox *tb;	
	
	int selstart, selend;	
	
	struct CharInfo *strinfo;	
	struct CharInfo curinfo;	
} Curve;

/* **************** CURVE ********************* */

/* texflag */
#define CU_AUTOSPACE	1

/* flag */
#define CU_3D			1
#define CU_FRONT		2
#define CU_BACK			4
#define CU_PATH			8
#define CU_FOLLOW		16
#define CU_UV_ORCO		32
#define CU_NOPUNOFLIP	64
#define CU_STRETCH		128
#define CU_OFFS_PATHDIST	256
#define CU_FAST			512 /* Font: no filling inside editmode */
#define CU_RETOPO               1024

/* spacemode */
#define CU_LEFT			0
#define CU_MIDDLE		1
#define CU_RIGHT		2
#define CU_JUSTIFY		3
#define CU_FLUSH		4

/* flag (nurb) */
#define CU_SMOOTH		1

/* type (nurb) */
#define CU_POLY			0
#define CU_BEZIER		1
#define CU_BSPLINE		2
#define CU_CARDINAL		3
#define CU_NURBS		4
#define CU_2D			8

/* flagu flagv (nurb) */
#define CU_CYCLIC		1

/* h1 h2 (beztriple) */
#define HD_FREE			0
#define HD_AUTO			1
#define HD_VECT			2
#define HD_ALIGN		3
#define HD_AUTO_ANIM	4

/* *************** CHARINFO **************** */

/* flag */
#define CU_STYLE		(1+2)
#define CU_BOLD			1
#define CU_ITALIC		2
#define CU_UNDERLINE	4
#define CU_WRAP			8	/* wordwrap occured here */

#endif

