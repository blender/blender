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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): André Pinto.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/include/rayobject.h
 *  \ingroup render
 */


#ifndef __RAYOBJECT_H__
#define __RAYOBJECT_H__

#ifdef __cplusplus
extern "C" {
#endif

struct Isect;
struct ObjectInstanceRen;
struct RayHint;
struct VlakRen;

/* RayObject
 * Can be a face/triangle, bvh tree, object instance, etc. This is the
 * public API used by the renderer, see rayobject_internal.h for the
 * internal implementation details.
 * */
typedef struct RayObject RayObject;

/* Intersection, see rayintersection.h */

int RE_rayobject_raycast(RayObject *r, struct Isect *i);

/* Acceleration Structures */

RayObject* RE_rayobject_octree_create(int ocres, int size);
RayObject* RE_rayobject_instance_create(RayObject *target, float transform[][4], void *ob, void *target_ob);
RayObject* RE_rayobject_empty_create(void);

RayObject* RE_rayobject_blibvh_create(int size);	/* BLI_kdopbvh.c   */
RayObject* RE_rayobject_vbvh_create(int size);		/* raytrace/rayobject_vbvh.c */
RayObject* RE_rayobject_svbvh_create(int size);		/* raytrace/rayobject_svbvh.c */
RayObject* RE_rayobject_qbvh_create(int size);		/* raytrace/rayobject_qbvh.c */

/* Building */

void RE_rayobject_add(RayObject *r, RayObject *);
void RE_rayobject_done(RayObject *r);
void RE_rayobject_free(RayObject *r);

void RE_rayobject_set_control(RayObject *r, void *data, int (*test_break)(void *data));

/* RayObject representing faces, all data is locally available instead
 * of referring to some external data structure, for possibly faster
 * intersection tests. */

typedef struct RayFace {
	float v1[4], v2[4], v3[4], v4[3];
	int quad;
	void *ob;
	void *face;
} RayFace;

#define RE_rayface_isQuad(a) ((a)->quad)

RayObject* RE_rayface_from_vlak(RayFace *face, struct ObjectInstanceRen *obi, struct VlakRen *vlr);

/* RayObject representing faces directly from a given VlakRen structure. Thus
 * allowing to save memory, but making code triangle intersection dependent on
 * render structures. */

typedef struct VlakPrimitive {
	struct ObjectInstanceRen *ob;
	struct VlakRen *face;
} VlakPrimitive;

RayObject* RE_vlakprimitive_from_vlak(VlakPrimitive *face, struct ObjectInstanceRen *obi, struct VlakRen *vlr);

/* Bounding Box */

/* extend min/max coords so that the rayobject is inside them */
void RE_rayobject_merge_bb(RayObject *ob, float *min, float *max);

/* initializes an hint for optimizing raycast where it is know that a ray will pass by the given BB often the origin point */
void RE_rayobject_hint_bb(RayObject *r, struct RayHint *hint, float *min, float *max);

/* initializes an hint for optimizing raycast where it is know that a ray will be contained inside the given cone*/
/* void RE_rayobject_hint_cone(RayObject *r, struct RayHint *hint, float *); */

/* Internals */

#include "../raytrace/rayobject_internal.h"

#ifdef __cplusplus
}
#endif

#endif

