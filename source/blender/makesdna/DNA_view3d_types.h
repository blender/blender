/**
 * blenlib/DNA_view3d_types.h (mar-2001 nzc)
 *
 * $Id$ 
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef DNA_VIEW3D_TYPES_H
#define DNA_VIEW3D_TYPES_H

struct Object;
struct Image;
struct Tex;
struct SpaceLink;

/* This is needed to not let VC choke on near and far... old
 * proprietary MS extensions... */
#ifdef WIN32
#undef near
#undef far
#define near clipsta
#define far clipend
#endif

/* The near/far thing is a Win EXCEPTION. Thus, leave near/far in the
 * code, and patch for windows. */

typedef struct BGpic {
    struct Image *ima;
	struct Tex *tex;
    float xof, yof, size, zoom, blend;
    short xim, yim;
	unsigned int *rect;
} BGpic;

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
	
	float viewquat[4], dist;

	/**
	 * 0 - ortho
	 * 1 - do 3d perspective
	 * 2 - use the camera
	 */
	short persp;
	short view;

	struct Object *camera;
	struct BGpic *bgpic;
	struct View3D *localvd;
	
	/**
	 * The drawing mode for the 3d display. Set to OB_WIRE, OB_SOLID,
	 * OB_SHADED or OB_TEXTURED */
	short drawtype;
	short localview;
	int lay, layact;
	short scenelock, around, camzoom, flag;
	
	float lens, grid, near, far;
	float ofs[3], cursor[3];
	
	short mx, my;	/* have to remain together, because used as single pointer */
	short mxo, myo;

	short gridlines, viewbut;
	short gridflag;
	short modeselect, menunr, texnr;
} View3D;

/* View3D->flag */
#define V3D_MODE			(16+32+64+128+256+512)
#define V3D_DISPIMAGE		1
#define V3D_DISPBGPIC		2
#define V3D_SETUPBUTS		4
#define V3D_NEEDBACKBUFDRAW	8
#define V3D_EDITMODE		16
#define V3D_VERTEXPAINT		32
#define V3D_FACESELECT		64
#define V3D_POSEMODE		128
#define V3D_TEXTUREPAINT	256
#define V3D_WEIGHTPAINT		512

/* View3D->around */
#define V3D_CENTRE		0
#define V3D_CENTROID	3
#define V3D_CURSOR		1
#define V3D_LOCAL		2

/* View3d->persp */
#define V3D_PERSP_ORTHO          0
#define V3D_PERSP_DO_3D_PERSP    1
#define V3D_PERSP_USE_THE_CAMERA 2

/* View3d->gridflag */
#define V3D_SHOW_FLOOR			1
#define V3D_SHOW_X				2
#define V3D_SHOW_Y				4
#define V3D_SHOW_Z				8

#endif

