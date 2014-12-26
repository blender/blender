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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): André Pinto.
 *
 * ***** END GPL LICENSE BLOCK *****
 * RE_raytrace.h: ray tracing api, can be used independently from the renderer. 
 */

/** \file blender/render/intern/include/rayintersection.h
 *  \ingroup render
 */


#ifndef __RAYINTERSECTION_H__
#define __RAYINTERSECTION_H__

#ifdef __cplusplus
extern "C" {
#endif

struct RayObject;

/* Ray Hints */

#define RE_RAY_LCTS_MAX_SIZE	256
#define RT_USE_LAST_HIT			/* last shadow hit is reused before raycasting on whole tree */
//#define RT_USE_HINT			/* last hit object is reused before raycasting on whole tree */

typedef struct LCTSHint {
	int size;
	struct RayObject *stack[RE_RAY_LCTS_MAX_SIZE];
} LCTSHint;

typedef struct RayHint {
	union { LCTSHint lcts; } data;
} RayHint;

/* Ray Intersection */

typedef struct Isect {
	/* ray start, direction (normalized vector), and max distance. on hit,
	 * the distance is modified to be the distance to the hit point. */
	float start[3];
	float dir[3];
	float dist;

	/* for envmap and incremental view update renders */
	float origstart[3];
	float origdir[3];
	
	/* precomputed values to accelerate bounding box intersection */
	int bv_index[6];
	float idot_axis[3];

	/* intersection options */
	int mode;				/* RE_RAY_SHADOW, RE_RAY_MIRROR, RE_RAY_SHADOW_TRA */
	int lay;				/* -1 default, set for layer lamps */
	int skip;				/* skip flags */
	int check;				/* check flags */
	void *userdata;			/* used by bake check */

	/* hit information */
	float u, v;
	int isect;				/* which half of quad */
	
	struct {
		void *ob;
		void *face;
	} hit, orig;
	
	/* last hit optimization */
	struct RayObject *last_hit;

	/* hints */
#ifdef RT_USE_HINT
	RayTraceHint *hint, *hit_hint;
#endif
	RayHint *hint;
	
	/* ray counter */
#ifdef RE_RAYCOUNTER
	RayCounter *raycounter;
#endif
} Isect;

/* ray types */
#define RE_RAY_SHADOW 0
#define RE_RAY_MIRROR 1
#define RE_RAY_SHADOW_TRA 2

/* skip options */
#define RE_SKIP_CULLFACE                (1 << 0)
/* if using this flag then *face should be a pointer to a VlakRen */
#define RE_SKIP_VLR_NEIGHBOUR           (1 << 1)

/* check options */
#define RE_CHECK_VLR_NONE               0
#define RE_CHECK_VLR_RENDER             1
#define RE_CHECK_VLR_NON_SOLID_MATERIAL 2
#define RE_CHECK_VLR_BAKE               3

/* arbitrary, but can't use e.g. FLT_MAX because of precision issues */
#define RE_RAYTRACE_MAXDIST	1e15f
#define RE_RAYTRACE_EPSILON 0.0f

#ifdef __cplusplus
}
#endif

#endif /* __RAYINTERSECTION_H__ */

