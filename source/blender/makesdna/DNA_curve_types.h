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

/** \file DNA_curve_types.h
 *  \ingroup DNA
 */

#ifndef DNA_CURVE_TYPES_H
#define DNA_CURVE_TYPES_H

#include "DNA_defs.h"
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
struct AnimData;
struct SelBox;
struct EditFont;
struct GHash;

/* These two Lines with # tell makesdna this struct can be excluded. */
#
#
typedef struct PathPoint {
	float vec[4]; /* grr, cant get rid of tilt yet */
	float quat[4];
	float radius, weight;
} PathPoint;

/* These two Lines with # tell makesdna this struct can be excluded. */
#
#
typedef struct Path {
	struct PathPoint *data;
	int len;
	float totdist;
} Path;

/* These two Lines with # tell makesdna this struct can be excluded. */
#
#
typedef struct BevList {
	struct BevList *next, *prev;
	int nr, dupe_nr;
	short poly, hole;
} BevList;

/* These two Lines with # tell makesdna this struct can be excluded. */
#
#
typedef struct BevPoint {
	float vec[3], alfa, radius, weight;
	float sina, cosa;				/* 2D Only */
	float dir[3], tan[3], quat[4];	/* 3D Only */
	short split_tag, dupe_tag;
} BevPoint;

/* Keyframes on F-Curves (allows code reuse of Bezier eval code) and 
 * Points on Bezier Curves/Paths are generally BezTriples 
 */
/* note: alfa location in struct is abused by Key system */
/* vec in BezTriple looks like this:
	vec[0][0]=x location of handle 1
	vec[0][1]=y location of handle 1
	vec[0][2]=z location of handle 1 (not used for FCurve Points(2d))
	vec[1][0]=x location of control point
	vec[1][1]=y location of control point
	vec[1][2]=z location of control point
	vec[2][0]=x location of handle 2
	vec[2][1]=y location of handle 2
	vec[2][2]=z location of handle 2 (not used for FCurve Points(2d))
*/
typedef struct BezTriple {
	float vec[3][3];
	float alfa, weight, radius;	/* alfa: tilt in 3D View, weight: used for softbody goal weight, radius: for bevel tapering */
	short ipo;					/* ipo: interpolation mode for segment from this BezTriple to the next */
	char h1, h2; 				/* h1, h2: the handle type of the two handles */
	char f1, f2, f3;			/* f1, f2, f3: used for selection status */
	char hide;					/* hide: used to indicate whether BezTriple is hidden (3D), type of keyframe (eBezTriple_KeyframeTypes) */
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
	short mat_nr; /* index start at 1, unlike mesh & nurbs */
	char flag;
	char pad;
	short pad2;
} CharInfo;

typedef struct TextBox {
	float x, y, w, h;
} TextBox;

typedef struct EditNurb {
	/* base of nurbs' list (old Curve->editnurb) */
	ListBase nurbs;

	/* index data for shape keys */
	struct GHash *keyindex;

	/* shape key being edited */
	int shapenr;

	char pad[4];
} EditNurb;

typedef struct Curve {
	ID id;
	struct AnimData *adt;		/* animation data (must be immediately after id for utilities to use it) */ 
	
	struct BoundBox *bb;
	
	ListBase nurb;		/* actual data, called splines in rna */
	ListBase disp;		/* undeformed display list, used mostly for texture space calculation */
	
	EditNurb *editnurb;	/* edited data, not in file, use pointer so we can check for it */
	
	struct Object *bevobj, *taperobj, *textoncurve;
	struct Ipo *ipo    DNA_DEPRECATED;  /* old animation system, deprecated for 2.5 */
	Path *path;
	struct Key *key;
	struct Material **mat;
	
	ListBase bev;
	
	/* texture space, copied as one block in editobject.c */
	float loc[3];
	float size[3];
	float rot[3];

	short type;	/* creation-time type of curve datablock */

	short texflag; /* keep a short because of give_obdata_texspace() */
	short drawflag, twist_mode;
	float twist_smooth, smallcaps_scale;

	int pathlen;
	short pad, totcol;
	short flag, bevresol;
	float width, ext1, ext2;
	
	/* default */
	short resolu, resolv;
	short resolu_ren, resolv_ren;

	/* edit, index in nurb list */
	int actnu;
	/* edit, last selected point */
	void *lastsel;
	
	/* font part */
	short len, lines, pos, spacemode;
	float spacing, linedist, shear, fsize, wordspace, ulpos, ulheight;
	float xof, yof;
	float linewidth;

	char *str;
	struct SelBox *selboxes;
	struct EditFont *editfont;
	
	char family[24];
	struct VFont *vfont;
	struct VFont *vfontb;
	struct VFont *vfonti;
	struct VFont *vfontbi;

	int sepchar;
	
	float ctime;			/* current evaltime - for use by Objects parented to curves */
	int totbox, actbox;
	struct TextBox *tb;	
	
	int selstart, selend;	
	
	struct CharInfo *strinfo;	
	struct CharInfo curinfo;
} Curve;

/* **************** CURVE ********************* */

/* texflag */
#define CU_AUTOSPACE	1

/* drawflag */
#define CU_HIDE_HANDLES	(1 << 0)
#define CU_HIDE_NORMALS	(1 << 1)

/* flag */
#define CU_3D			1
#define CU_FRONT		2
#define CU_BACK			4
#define CU_PATH			8
#define CU_FOLLOW		16
#define CU_UV_ORCO		32
#define CU_DEFORM_BOUNDS_OFF 64 
#define CU_STRETCH		128
#define CU_OFFS_PATHDIST	256
#define CU_FAST			512 /* Font: no filling inside editmode */
/* #define CU_RETOPO               1024 */ /* DEPRECATED */
#define CU_DS_EXPAND	2048
#define CU_PATH_RADIUS	4096 /* make use of the path radius if this is enabled (default for new curves) */
#define CU_DEFORM_FILL	8192 /* fill 2d curve after deformation */

/* twist mode */
#define CU_TWIST_Z_UP			0
// #define CU_TWIST_Y_UP			1 // not used yet
// #define CU_TWIST_X_UP			2
#define CU_TWIST_MINIMUM		3
#define CU_TWIST_TANGENT		4

/* spacemode */
#define CU_LEFT			0
#define CU_MIDDLE		1
#define CU_RIGHT		2
#define CU_JUSTIFY		3
#define CU_FLUSH		4

/* flag (nurb) */
#define CU_SMOOTH		1
#define CU_2D			8 /* moved from type since 2.4x */

/* type (nurb) */
#define CU_POLY			0
#define CU_BEZIER		1
#define CU_BSPLINE		2
#define CU_CARDINAL		3
#define CU_NURBS		4
#define CU_TYPE			(CU_POLY|CU_BEZIER|CU_BSPLINE|CU_CARDINAL|CU_NURBS)

		/* only for adding */
#define CU_PRIMITIVE	0xF00

		/* 2 or 4 points */
#define CU_PRIM_CURVE	0x100
		/* 8 points circle */
#define CU_PRIM_CIRCLE	0x200
		/* 4x4 patch Nurb */
#define CU_PRIM_PATCH	0x300
#define CU_PRIM_TUBE	0x400
#define CU_PRIM_SPHERE	0x500
#define CU_PRIM_DONUT	0x600
		/* 5 points,  5th order straight line (for anim path) */
#define CU_PRIM_PATH	0x700


/* flagu flagv (nurb) */
#define CU_NURB_CYCLIC		1
#define CU_NURB_ENDPOINT	2
#define CU_NURB_BEZIER		4

/* *************** BEZTRIPLE **************** */

/* h1 h2 (beztriple) */
typedef enum eBezTriple_Handle {
	HD_FREE	= 0,
	HD_AUTO,
	HD_VECT,
	HD_ALIGN,
	HD_AUTO_ANIM 	/* auto-clamped handles for animation */
} eBezTriple_Handle;

/* interpolation modes (used only for BezTriple->ipo) */
typedef enum eBezTriple_Interpolation {
	BEZT_IPO_CONST = 0,	/* constant interpolation */
	BEZT_IPO_LIN,		/* linear interpolation */
	BEZT_IPO_BEZ		/* bezier interpolation */
} eBezTriple_Interpolation;

/* types of keyframe (used only for BezTriple->hide when BezTriple is used in F-Curves) */
typedef enum eBezTriple_KeyframeType {
	BEZT_KEYTYPE_KEYFRAME = 0,	/* default - 'proper' Keyframe */
	BEZT_KEYTYPE_EXTREME,		/* 'extreme' keyframe */
	BEZT_KEYTYPE_BREAKDOWN,		/* 'breakdown' keyframe */
	BEZT_KEYTYPE_JITTER,		/* 'jitter' keyframe (for adding 'filler' secondary motion) */
} eBezTriple_KeyframeType;

/* checks if the given BezTriple is selected */
#define BEZSELECTED(bezt) (((bezt)->f2 & SELECT) || ((bezt)->f1 & SELECT) || ((bezt)->f3 & SELECT))
#define BEZSELECTED_HIDDENHANDLES(cu, bezt)   (((cu)->drawflag & CU_HIDE_HANDLES) ? (bezt)->f2 & SELECT : BEZSELECTED(bezt))

/* *************** CHARINFO **************** */

/* flag */
/* note: CU_CHINFO_WRAP and CU_CHINFO_SMALLCAPS_TEST are set dynamically */
#define CU_CHINFO_BOLD			(1<<0)
#define CU_CHINFO_ITALIC		(1<<1)
#define CU_CHINFO_UNDERLINE	(1<<2)
#define CU_CHINFO_WRAP			(1<<3)	/* wordwrap occurred here */
#define CU_CHINFO_SMALLCAPS	(1<<4)
#define CU_CHINFO_SMALLCAPS_CHECK (1<<5) /* set at runtime, checks if case switching is needed */

/* mixed with KEY_LINEAR but define here since only curve supports */
#define KEY_CU_EASE			3


#endif

