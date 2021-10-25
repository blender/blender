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

#ifndef __RENDERCORE_H__
#define __RENDERCORE_H__ 

/** \file blender/render/intern/include/rendercore.h
 *  \ingroup render
 */

#include "render_types.h"

#include "RE_engine.h"

#include "DNA_node_types.h"

#include "NOD_composite.h"

struct ShadeInput;
struct ShadeResult;
struct World;
struct RenderPart;
struct RenderLayer;
struct RayObject;

/* ------------------------------------------------------------------------- */

typedef struct PixStr {
	struct PixStr *next;
	int obi, facenr, z, maskz;
	unsigned short mask;
	short shadfac;
} PixStr;

typedef struct PixStrMain {
	struct PixStrMain *next, *prev;
	struct PixStr *ps;
	int counter;
} PixStrMain;

/* ------------------------------------------------------------------------- */


void	calc_view_vector(float view[3], float x, float y);
float   mistfactor(float zcor, const float co[3]); /* dist and height, return alpha */

void	renderspothalo(struct ShadeInput *shi, float col[4], float alpha);
void	add_halo_flare(Render *re);

void calc_renderco_zbuf(float co[3], const float view[3], int z);
void calc_renderco_ortho(float co[3], float x, float y, int z);

int count_mask(unsigned short mask);

void zbufshade_tile(struct RenderPart *pa);
void zbufshadeDA_tile(struct RenderPart *pa);

void zbufshade_sss_tile(struct RenderPart *pa);

int get_sample_layers(struct RenderPart *pa, struct RenderLayer *rl, struct RenderLayer **rlpp);

void render_internal_update_passes(struct RenderEngine *engine, struct Scene *scene, struct SceneRenderLayer *srl);


/* -------- ray.c ------- */

struct RayObject *RE_rayobject_create(int type, int size, int octree_resolution);

extern void freeraytree(Render *re);
extern void makeraytree(Render *re);
struct RayObject* makeraytree_object(Render *re, ObjectInstanceRen *obi);

extern void ray_shadow(ShadeInput *shi, LampRen *lar, float shadfac[4]);
extern void ray_trace(ShadeInput *shi, ShadeResult *);
extern void ray_ao(ShadeInput *shi, float ao[3], float env[3]);
extern void init_jitter_plane(LampRen *lar);
extern void init_ao_sphere(Render *re, struct World *wrld);
extern void init_render_qmcsampler(Render *re);
extern void free_render_qmcsampler(Render *re);

#endif  /* __RENDERCORE_H__ */
