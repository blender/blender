/**
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

#ifndef RENDER_TYPES_H
#define RENDER_TYPES_H "$Id$"

#include "DNA_scene_types.h"
#include "DNA_world_types.h"
#include "DNA_object_types.h"

#define TABLEINITSIZE 1024
#define LAMPINITSIZE 256

/* This is needed to not let VC choke on near and far... old
 * proprietary MS extensions... */
#ifdef WIN32
#undef near
#undef far
#define near clipsta
#define far clipend
#endif

/* ------------------------------------------------------------------------- */

typedef struct RE_Render
{
	float co[3];
	float lo[3], gl[3], uv[3], ref[3], orn[3], winco[3], sticky[3], vcol[3], rad[3];
	float itot, i, ic, rgb, norm;
	float vn[3], view[3], *vno, refcol[4];

	float grvec[3], inprz, inprh;
	float imat[3][3];

	float viewmat[4][4], viewinv[4][4];
	float persmat[4][4], persinv[4][4];
	float winmat[4][4];
	
	short flag, osatex, osa, rt;
	/**
	 * Screen sizes and positions, in pixels
	 */
	short xstart, xend, ystart, yend, afmx, afmy;
	short rectx;  /* Picture width - 1, normally xend - xstart. */  
	short recty;  /* picture height - 1, normally yend - ystart. */

	/**
	 * Distances and sizes in world coordinates nearvar, farvar were
	 * near and far, but VC in cpp mode chokes on it :( */
	float near;    /* near clip distance */
	float far;     /* far clip distance */
	float ycor, zcor, pixsize, viewfac;


	/* These three need to be 'handlerized'. Not an easy task... */
/*  	RE_RenderDataHandle r; */
	RenderData r;
	World wrld;
	ListBase parts;
	
	int totvlak, totvert, tothalo, totlamp;

	/* internal: these two are a sort of cache for the render pipe */
	struct VlakRen *vlr;
	int vlaknr;
	
	/* external */
	struct Material *mat, *matren;
	/* internal, fortunately */
	struct LampRen **la;
	struct VlakRen **blovl;
	struct VertRen **blove;
	struct HaloRen **bloha;
	
	unsigned int *rectaccu;
	unsigned int *rectz; /* z buffer: distance buffer */
	unsigned int *rectf1, *rectf2;
	unsigned int *rectot; /* z buffer: face index buffer, recycled as colour buffer! */
	unsigned int *rectspare; /*  */
	/* for 8 byte systems! */
	long *rectdaps;
	
	short win, winpos, winx, winy, winxof, winyof;
	short winpop, displaymode, sparex, sparey;

	/* Not sure what these do... But they're pointers, so good for handlerization */
	struct Image *backbuf, *frontbuf;
	/* backbuf is an image that drawn as background */
	
} RE_Render;

/* ------------------------------------------------------------------------- */

/** 
 * Part as in part-rendering. An image rendered in parts is rendered
 * to a list of parts, with x,y size, and a pointer to the render
 * output stored per part. Internal!
 */
typedef struct Part
{
	struct Part *next, *prev;
	unsigned int *rect;
	short x, y;
} Part;

typedef struct ShadBuf {
	short samp, shadhalostep;
	float persmat[4][4];
	float viewmat[4][4];
	float winmat[4][4];
	float *jit;
	float d,far,pixsize,soft;
	int co[3];
	int size,bias;
	unsigned long *zbuf;
	char *cbuf;
} ShadBuf;

/* ------------------------------------------------------------------------- */

typedef struct VertRen
{
	float co[3];
	float n[3];
	float ho[4];
	float rad[3];			/* result radio rendering */
	float *orco;
	float *sticky;
	void *svert;			/* smooth vert, only used during initrender */
	short clip, texofs;		/* texofs= flag */
	float accum;			/* accum for radio weighting */
} VertRen;

/* ------------------------------------------------------------------------- */

struct halosort {
	struct HaloRen *har;
	unsigned int z;
};

/* ------------------------------------------------------------------------- */
struct Material;
struct MFace;
struct TFace;

typedef struct RadFace {
	float unshot[3], totrad[3];
	float norm[3], cent[3], area;
	int flag;
} RadFace;

typedef struct VlakRen
{
	struct VertRen *v1, *v2, *v3, *v4;
	float n[3], len;
	struct Material *mat;
	struct MFace *mface;
	struct TFace *tface;
	unsigned int *vcol;
	char snproj, puno;
	char flag, ec;
	unsigned int lay;
	unsigned int raycount;
	RadFace *radface;
	Object *ob;
} VlakRen;


typedef struct HaloRen
{	
    float alfa, xs, ys, rad, radsq, sin, cos, co[3], no[3];
    unsigned int zs, zd;
    unsigned int zBufDist;/* depth in the z-buffer coordinate system */
    short miny, maxy;
    short hard, b, g, r;
    char starpoints, add, type, tex;
    char linec, ringc, seed;
	short flarec; /* used to be a char. why ?*/
    float hasize;
    int pixels;
    unsigned int lay;
    struct Material *mat;
} HaloRen;

struct LampRen;
struct MTex;

/**
 * For each lamp in a scene, a LampRen is created. It determines the
 * properties of a lightsource.
 */
typedef struct LampRen
{
	float xs, ys, dist;
	float co[3];
	short type, mode;
	float r, g, b;
	float energy, haint;
	int lay;
	float spotsi,spotbl;
	float vec[3];
	float xsp, ysp, distkw, inpr;
	float halokw, halo;
	float ld1,ld2;

	/* copied from Lamp, to decouple more rendering stuff */
	/** Size of the shadowbuffer */
	short bufsize;
	/** Number of samples for the shadows */
	short samp;
	/** Softness factor for shadow */
	float soft;
	/** shadow plus halo: detail level */
	short shadhalostep;
	/** Near clip of the lamp */
	float clipsta;
	/** Far clip of the lamp */
	float clipend;
	/** A small depth offset to prevent self-shadowing. */
	float bias;
	
	float ray_soft;
	short ray_samp;
	
	/** If the lamp casts shadows, one of these is filled. For the old
     * renderer, shb is used, for the new pipeline the shadowBufOb,
     * which should be a shadowbuffer handle. */
	struct ShadBuf *shb;
	void* shadowBufOb;

	float imat[3][3];
	float spottexfac;
	float sh_invcampos[3], sh_zfac;	/* sh_= spothalo */
	
	struct LampRen *org;
	struct MTex *mtex[8];
} LampRen;

#endif /* RENDER_TYPES_H */

