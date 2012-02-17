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
 * Full recode: 2004-2006 Blender Foundation
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/** \file blender/render/intern/include/zbuf.h
 *  \ingroup render
 */


#ifndef __ZBUF_H__
#define __ZBUF_H__

struct RenderPart;
struct RenderLayer;
struct LampRen;
struct VlakRen;
struct ListBase;
struct ZSpan;
struct APixstrand;
struct APixstr;
struct StrandShadeCache;

void fillrect(int *rect, int x, int y, int val);

/**
 * Converts a world coordinate into a homogenous coordinate in view
 * coordinates. 
 */
void projectvert(const float v1[3], float winmat[][4], float adr[4]);
void projectverto(const float v1[3], float winmat[][4], float adr[4]);
int testclip(const float v[3]);

void zbuffer_shadow(struct Render *re, float winmat[][4], struct LampRen *lar, int *rectz, int size, float jitx, float jity);
void zbuffer_abuf_shadow(struct Render *re, struct LampRen *lar, float winmat[][4], struct APixstr *APixbuf, struct APixstrand *apixbuf, struct ListBase *apsmbase, int size, int samples, float (*jit)[2]);
void zbuffer_solid(struct RenderPart *pa, struct RenderLayer *rl, void (*fillfunc)(struct RenderPart*, struct ZSpan*, int, void*), void *data);

unsigned short *zbuffer_transp_shade(struct RenderPart *pa, struct RenderLayer *rl, float *pass, struct ListBase *psmlist);
void zbuffer_sss(RenderPart *pa, unsigned int lay, void *handle, void (*func)(void*, int, int, int, int, int));
int zbuffer_strands_abuf(struct Render *re, struct RenderPart *pa, struct APixstrand *apixbuf, struct ListBase *apsmbase, unsigned int lay, int negzmask, float winmat[][4], int winx, int winy, int sample, float (*jit)[2], float clipcrop, int shadow, struct StrandShadeCache *cache);

typedef struct APixstr {
	unsigned short mask[4];		/* jitter mask */
	int z[4];					/* distance    */
	int p[4];					/* index       */
	int obi[4];					/* object instance */
	short shadfac[4];			/* optimize storage for irregular shadow */
	struct APixstr *next;
} APixstr;

typedef struct APixstrand {
	unsigned short mask[4];		/* jitter mask */
	int z[4];					/* distance    */
	int p[4];					/* index       */
	int obi[4];					/* object instance */
	int seg[4];					/* for strands, segment number */
	float u[4], v[4];			/* for strands, u,v coordinate in segment */
	struct APixstrand *next;
} APixstrand;

typedef struct APixstrMain
{
	struct APixstrMain *next, *prev;
	void *ps;
} APixstrMain;

/* span fill in method, is also used to localize data for zbuffering */
typedef struct ZSpan {
	int rectx, recty;						/* range for clipping */
	
	int miny1, maxy1, miny2, maxy2;			/* actual filled in range */
	float *minp1, *maxp1, *minp2, *maxp2;	/* vertex pointers detect min/max range in */
	float *span1, *span2;
	
	float zmulx, zmuly, zofsx, zofsy;		/* transform from hoco to zbuf co */
	
	int *rectz, *arectz;					/* zbuffers, arectz is for transparant */
	int *rectz1;							/* seconday z buffer for shadowbuffer (2nd closest z) */
	int *rectp;								/* polygon index buffer */
	int *recto;								/* object buffer */
	int *rectmask;							/* negative zmask buffer */
	APixstr *apixbuf, *curpstr;				/* apixbuf for transparent */
	APixstrand *curpstrand;					/* same for strands */
	struct ListBase *apsmbase;
	
	int polygon_offset;						/* offset in Z */
	float shad_alpha;						/* copy from material, used by irregular shadbuf */
	int mask, apsmcounter;					/* in use by apixbuf */
	int apstrandmcounter;

	float clipcrop;							/* for shadow, was in R global before */

	void *sss_handle;						/* used by sss */
	void (*sss_func)(void *, int, int, int, int, int);
	
	void (*zbuffunc)(struct ZSpan *, int, int, float *, float *, float *, float *);
	void (*zbuflinefunc)(struct ZSpan *, int, int, float *, float *);
	
} ZSpan;

/* exported to shadbuf.c */
void zbufclip4(struct ZSpan *zspan, int obi, int zvlnr, float *f1, float *f2, float *f3, float *f4, int c1, int c2, int c3, int c4);
void zbuf_free_span(struct ZSpan *zspan);
void freepsA(struct ListBase *lb);

/* to rendercore.c */
void zspan_scanconvert(struct ZSpan *zpan, void *handle, float *v1, float *v2, float *v3, void (*func)(void *, int, int, float, float) );

/* exported to edge render... */
void zbufclip(struct ZSpan *zspan, int obi, int zvlnr, float *f1, float *f2, float *f3, int c1, int c2, int c3);
void zbuf_alloc_span(struct ZSpan *zspan, int rectx, int recty, float clipcrop);
void zbufclipwire(struct ZSpan *zspan, int obi, int zvlnr, int ec, float *ho1, float *ho2, float *ho3, float *ho4, int c1, int c2, int c3, int c4);

/* exported to shadeinput.c */
void zbuf_make_winmat(Render *re, float winmat[][4]);
void zbuf_render_project(float winmat[][4], const float co[3], float ho[4]);

#endif

