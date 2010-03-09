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
struct SmoothViewStore;
struct wmTimer;

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
    struct BGpic *next, *prev;

    struct Image *ima;
	struct ImageUser iuser;
    float xof, yof, size, blend;
    short view;
    short flag;
    float pad2;
} BGpic;

/* ********************************* */

typedef struct RegionView3D {
	
	float winmat[4][4];			/* GL_PROJECTION matrix */
	float viewmat[4][4];		/* GL_MODELVIEW matrix */
	float viewinv[4][4];		/* inverse of viewmat */
	float persmat[4][4];		/* viewmat*winmat */
	float persinv[4][4];		/* inverse of persmat */

	/* viewmat/persmat multiplied with object matrix, while drawing and selection */
	float viewmatob[4][4];
	float persmatob[4][4];

	/* transform widget matrix */
	float twmat[4][4];
	
	float viewquat[4], dist, zfac;	/* zfac is initgrabz() result */
	float camdx, camdy;				/* camera view offsets, 1.0 = viewplane moves entire width/height */
	float pixsize;
	float ofs[3];
	short camzoom, viewbut;
	short twdrawflag;
	short pad;
	
	short rflag, viewlock;
	short persp;
	short view;
	
	/* user defined clipping planes */
	float clip[6][4];
	float clip_local[6][4]; /* clip in object space, means we can test for clipping in editmode without first going into worldspace */
	struct BoundBox *clipbb;	
	
	struct bGPdata *gpd;		/* Grease-Pencil Data (annotation layers) */
	
	struct RegionView3D *localvd;
	struct RenderInfo *ri;
	struct RetopoViewData *retopo_view_data;
	struct ViewDepths *depths;
	
	/* animated smooth view */
	struct SmoothViewStore *sms;
	struct wmTimer *smooth_timer;
	
	/* last view */
	float lviewquat[4];
	short lpersp, lview;
	float gridview;
	
	float twangle[3];

	float padf;

} RegionView3D;

/* 3D ViewPort Struct */
typedef struct View3D {
	struct SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;
	short blockhandler[8];
	
	float viewquat[4], dist, pad1;	/* XXX depricated */
	
	int lay_used; /* used while drawing */
	
	short persp;	/* XXX depricated */
	short view;	/* XXX depricated */
	
	struct Object *camera, *ob_centre;

	struct ListBase bgpicbase;
	struct BGpic *bgpic; /* deprecated, use bgpicbase, only kept for do_versions(...) */

	struct View3D *localvd;
	
	char ob_centre_bone[32];		/* optional string for armature bone to define center */
	
	int lay, layact;
	
	/**
	 * The drawing mode for the 3d display. Set to OB_WIRE, OB_SOLID,
	 * OB_SHADED or OB_TEXTURE */
	short drawtype;
	short pad2;
	short scenelock, around, pad3;
	short flag, flag2;
	
	short pivot_last; /* pivot_last is for rotating around the last edited element */
	
	float lens, grid;
	float gridview; /* XXX deprecated, now in RegionView3D */
	float padf, near, far;
	float ofs[3];			/* XXX deprecated */
	float cursor[3];

	short gridlines, pad4;
	short gridflag;
	short gridsubdiv;	/* Number of subdivisions in the grid between each highlighted grid line */
	short modeselect;
	short keyflags;		/* flags for display of keyframes */
	
	/* transform widget info */
	short twtype, twmode, twflag;
	short twdrawflag; /* XXX deprecated */
	
	/* customdata flags from modes */
	unsigned int customdata_mask;
	
	/* afterdraw, for xray & transparent */
	struct ListBase afterdraw;
	
	/* drawflags, denoting state */
	short zbuf, transp, xray;

	char ndofmode;			/* mode of transform for 6DOF devices -1 not found, 0 normal, 1 fly, 2 ob transform */
	char ndoffilter;		/* filter for 6DOF devices 0 normal, 1 dominant */
	
	void *properties_storage;	/* Nkey panel stores stuff here, not in file */
	
	/* XXX depricated? */
	struct bGPdata *gpd;		/* Grease-Pencil Data (annotation layers) */

} View3D;

/* XXX this needs cleaning */

/* View3D->flag (short) */
#define V3D_MODE			(16+32+64+128+256+512)
#define V3D_DISPIMAGE		1
#define V3D_DISPBGPICS		2
#define V3D_HIDE_HELPLINES	4
#define V3D_INVALID_BACKBUF	8
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
#define V3D_DRAW_CENTERS	32768

/* RegionView3d->persp */
#define RV3D_ORTHO				0
#define RV3D_PERSP				1
#define RV3D_CAMOB				2

/* RegionView3d->rflag */
#define RV3D_FLYMODE				2
#define RV3D_CLIPPING				4
#define RV3D_NAVIGATING				8
#define RV3D_GPULIGHT_UPDATE		16

/* RegionView3d->viewlock */
#define RV3D_LOCKED			1
#define RV3D_BOXVIEW		2
#define RV3D_BOXCLIP		4

/* RegionView3d->view */
#define RV3D_VIEW_FRONT		 1
#define RV3D_VIEW_BACK			 2
#define RV3D_VIEW_LEFT			 3
#define RV3D_VIEW_RIGHT		 4
#define RV3D_VIEW_TOP			 5
#define RV3D_VIEW_BOTTOM		 6
#define RV3D_VIEW_PERSPORTHO	 7
#define RV3D_VIEW_CAMERA		 8

/* View3d->flag2 (short) */
#define V3D_SOLID_TEX			8
#define V3D_DISPGP				16

/* View3D->around */
#define V3D_CENTER		 0
#define V3D_CENTROID	 3
#define V3D_CURSOR		 1
#define V3D_LOCAL		 2
#define V3D_ACTIVE		 4

/*View3D types (only used in tools, not actually saved)*/
#define V3D_VIEW_STEPLEFT		 1
#define V3D_VIEW_STEPRIGHT		 2
#define V3D_VIEW_STEPDOWN		 3
#define V3D_VIEW_STEPUP		 4
#define V3D_VIEW_PANLEFT		 5
#define V3D_VIEW_PANRIGHT		 6
#define V3D_VIEW_PANDOWN		 7
#define V3D_VIEW_PANUP			 8

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
#define V3D_MANIP_GIMBAL		4
#define V3D_MANIP_CUSTOM		5 /* anything of value 5 or higher is custom */

/* View3d->twflag */
   /* USE = user setting, DRAW = based on selection */
#define V3D_USE_MANIPULATOR		1
#define V3D_DRAW_MANIPULATOR	2
#define V3D_CALC_MANIPULATOR	4

/* BGPic->flag */
/* may want to use 1 for select ?*/
#define V3D_BGPIC_EXPANDED		2

#endif


