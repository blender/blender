/**
 * blenlib/DNA_view3d_types.h (mar-2001 nzc)
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
#ifndef DNA_VIEW3D_TYPES_H
#define DNA_VIEW3D_TYPES_H

struct ViewDepths;
struct Object;
struct Image;
struct Tex;
struct SpaceLink;
struct Base;
struct BoundBox;
struct RenderInfo;
struct RetopoViewData;
struct bGPdata;

/* This is needed to not let VC choke on near and far... old
 * proprietary MS extensions... */
#ifdef WIN32
#undef near
#undef far
#define near clipsta
#define far clipend
#endif

#include "DNA_listBase.h"
#include "DNA_image_types.h"

/* ******************************** */

/* The near/far thing is a Win EXCEPTION. Thus, leave near/far in the
 * code, and patch for windows. */
 
/* Background Picture in 3D-View */
typedef struct BGpic {
    struct Image *ima;
	struct ImageUser iuser;
    float xof, yof, size, zoom, blend;
    short xim, yim;
} BGpic;

/* ********************************* */

/* 3D ViewPort Struct */
typedef struct View3D {
	struct SpaceLink *next, *prev;
	int spacetype;
	float blockscale;
	struct ScrArea *area;
	
	short blockhandler[8];

	float viewmat[4][4];
	float viewinv[4][4];
	float persmat[4][4];
	float persinv[4][4];
	
	float winmat1[4][4];  // persp(1) storage, for swap matrices
	float viewmat1[4][4];
	
	float viewquat[4], dist, zfac, pad0;	/* zfac is initgrabz() result */

	short persp;
	short view;

	struct Object *camera, *ob_centre;
	struct BGpic *bgpic;
	struct View3D *localvd;
	struct RenderInfo *ri;
	struct RetopoViewData *retopo_view_data;
	struct ViewDepths *depths;
	
	char ob_centre_bone[32];		/* optional string for armature bone to define center */
	
	/**
	 * The drawing mode for the 3d display. Set to OB_WIRE, OB_SOLID,
	 * OB_SHADED or OB_TEXTURE */
	short drawtype;
	short localview;
	int lay, layact;
	short scenelock, around, camzoom;
	
	char pivot_last, pad1; /* pivot_last is for rotating around the last edited element */
	
	float lens, grid, gridview, pixsize, near, far;
	float camdx, camdy;		/* camera view offsets, 1.0 = viewplane moves entire width/height */
	float ofs[3], cursor[3];

	short gridlines, viewbut;
	short gridflag;
	short modeselect, menunr, texnr;
	
	/* transform widget info */
	short twtype, twmode, twflag, twdrawflag;
	float twmat[4][4];
	
	/* user defined clipping planes */
	float clip[4][4];
	
	struct BoundBox *clipbb;
	
	/* afterdraw, for xray & transparent */
	struct ListBase afterdraw;
	/* drawflags, denoting state */
	short zbuf, transp, xray;

	short flag, flag2;
	
	short gridsubdiv;	/* Number of subdivisions in the grid between each highlighted grid line */
	
	short pad3;
	
	char ndofmode;	/* mode of transform for 6DOF devices -1 not found, 0 normal, 1 fly, 2 ob transform */
	char ndoffilter;		/*filter for 6DOF devices 0 normal, 1 dominant */
	
	void *properties_storage;	/* Nkey panel stores stuff here, not in file */
	struct bGPdata *gpd;		/* Grease-Pencil Data (annotation layers) */
} View3D;


/* View3D->flag (short) */
#define V3D_MODE			(16+32+64+128+256+512)
#define V3D_DISPIMAGE		1
#define V3D_DISPBGPIC		2
#define V3D_HIDE_HELPLINES	4
#define V3D_NEEDBACKBUFDRAW	8
#define V3D_EDITMODE		16
#define V3D_VERTEXPAINT		32
#define V3D_FACESELECT		64
#define V3D_POSEMODE		128
#define V3D_TEXTUREPAINT	256
#define V3D_WEIGHTPAINT		512
#define V3D_ALIGN			1024
#define V3D_SELECT_OUTLINE	2048
#define V3D_ZBUF_SELECT		4096
#define V3D_GLOBAL_STATS	8192
#define V3D_CLIPPING		16384
#define V3D_DRAW_CENTERS	32768

/* View3d->flag2 (short) */
#define V3D_MODE2			(32)
#define V3D_OPP_DIRECTION_NAME	1
#define V3D_FLYMODE				2
#define V3D_DEPRECATED			4 /* V3D_TRANSFORM_SNAP, moved to a scene setting */
#define V3D_SOLID_TEX			8
#define V3D_DISPGP				16

/* View3D->around */
#define V3D_CENTER		 0
#define V3D_CENTROID	 3
#define V3D_CURSOR		 1
#define V3D_LOCAL		 2
#define V3D_ACTIVE		 4


/* View3d->persp */
#define V3D_ORTHO				0
#define V3D_PERSP				1
#define V3D_CAMOB				2

/* View3d->gridflag */
#define V3D_SHOW_FLOOR			1
#define V3D_SHOW_X				2
#define V3D_SHOW_Y				4
#define V3D_SHOW_Z				8

/* View3d->twtype (bits, we can combine them) */
#define V3D_MANIP_TRANSLATE		1
#define V3D_MANIP_ROTATE		2
#define V3D_MANIP_SCALE			4

/* View3d->twmode */
#define V3D_MANIP_GLOBAL		0
#define V3D_MANIP_LOCAL			1
#define V3D_MANIP_NORMAL		2
#define V3D_MANIP_VIEW			3
#define V3D_MANIP_CUSTOM		4 /* anything of value 4 or higher is custom */

/* View3d->twflag */
   /* USE = user setting, DRAW = based on selection */
#define V3D_USE_MANIPULATOR		1
#define V3D_DRAW_MANIPULATOR	2
#define V3D_CALC_MANIPULATOR	4


#endif


