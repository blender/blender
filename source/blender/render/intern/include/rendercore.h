/*
 * rendercore_ext.h
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

#ifndef RENDERCORE_H
#define RENDERCORE_H 

#include "render_types.h"


/* vector defines */

#define CROSS(dest, a, b)		{ dest[0]= a[1] * b[2] - a[2] * b[1]; dest[1]= a[2] * b[0] - a[0] * b[2]; dest[2]= a[0] * b[1] - a[1] * b[0]; }
#define VECMUL(dest, f)			{ dest[0]*= f; dest[1]*= f; dest[2]*= f; }

struct HaloRen;
struct ShadeInput;
struct ShadeResult;
struct World;
struct RenderPart;
struct RenderLayer;
struct ObjectRen;
struct ListBase;

/* ------------------------------------------------------------------------- */

typedef struct PixStr
{
	struct PixStr *next;
	int obi, facenr, z, maskz;
	unsigned short mask;
	short shadfac;
} PixStr;

typedef struct PixStrMain
{
	struct PixStrMain *next, *prev;
	struct PixStr *ps;
	int counter;
} PixStrMain;

/* ------------------------------------------------------------------------- */


void	calc_view_vector(float *view, float x, float y);
float   mistfactor(float zcor, float *co);	/* dist and height, return alpha */

void	renderspothalo(struct ShadeInput *shi, float *col, float alpha);
void	add_halo_flare(Render *re);

void calc_renderco_zbuf(float *co, float *view, int z);
void calc_renderco_ortho(float *co, float x, float y, int z);

int count_mask(unsigned short mask);

void zbufshade(void);
void zbufshadeDA(void);	/* Delta Accum Pixel Struct */

void zbufshade_tile(struct RenderPart *pa);
void zbufshadeDA_tile(struct RenderPart *pa);

void zbufshade_sss_tile(struct RenderPart *pa);

int get_sample_layers(struct RenderPart *pa, struct RenderLayer *rl, struct RenderLayer **rlpp);


/* -------- ray.c ------- */

extern void freeraytree(Render *re);
extern void makeraytree(Render *re);
RayObject* makeraytree_object(Render *re, ObjectInstanceRen *obi);

extern void ray_shadow(ShadeInput *, LampRen *, float *);
extern void ray_trace(ShadeInput *, ShadeResult *);
extern void ray_ao(ShadeInput *, float *);
extern void init_jitter_plane(LampRen *lar);
extern void init_ao_sphere(struct World *wrld);
extern void init_render_qmcsampler(Render *re);
extern void free_render_qmcsampler(Render *re);

#endif /* RENDER_EXT_H */

