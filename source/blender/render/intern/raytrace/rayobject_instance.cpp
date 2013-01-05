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

/** \file blender/render/intern/raytrace/rayobject_instance.cpp
 *  \ingroup render
 */


#include <assert.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "rayintersection.h"
#include "rayobject.h"

#define RE_COST_INSTANCE (1.0f)

static int  RE_rayobject_instance_intersect(RayObject *o, Isect *isec);
static void RE_rayobject_instance_free(RayObject *o);
static void RE_rayobject_instance_bb(RayObject *o, float *min, float *max);
static float RE_rayobject_instance_cost(RayObject *o);

static void RE_rayobject_instance_hint_bb(RayObject *UNUSED(o), RayHint *UNUSED(hint),
                                          float *UNUSED(min), float *UNUSED(max))
{}

static RayObjectAPI instance_api =
{
	RE_rayobject_instance_intersect,
	NULL, //static void RE_rayobject_instance_add(RayObject *o, RayObject *ob);
	NULL, //static void RE_rayobject_instance_done(RayObject *o);
	RE_rayobject_instance_free,
	RE_rayobject_instance_bb,
	RE_rayobject_instance_cost,
	RE_rayobject_instance_hint_bb
};

typedef struct InstanceRayObject {
	RayObject rayobj;
	RayObject *target;

	void *ob; //Object represented by this instance
	void *target_ob; //Object represented by the inner RayObject, needed to handle self-intersection
	
	float global2target[4][4];
	float target2global[4][4];
	
} InstanceRayObject;


RayObject *RE_rayobject_instance_create(RayObject *target, float transform[4][4], void *ob, void *target_ob)
{
	InstanceRayObject *obj = (InstanceRayObject *)MEM_callocN(sizeof(InstanceRayObject), "InstanceRayObject");
	assert(RE_rayobject_isAligned(obj) );  /* RayObject API assumes real data to be 4-byte aligned */

	obj->rayobj.api = &instance_api;
	obj->target = target;
	obj->ob = ob;
	obj->target_ob = target_ob;

	copy_m4_m4(obj->target2global, transform);
	invert_m4_m4(obj->global2target, obj->target2global);

	return RE_rayobject_unalignRayAPI((RayObject *) obj);
}

static int  RE_rayobject_instance_intersect(RayObject *o, Isect *isec)
{
	InstanceRayObject *obj = (InstanceRayObject *)o;
	float start[3], dir[3], idot_axis[3], dist;
	int changed = 0, i, res;

	// TODO - this is disabling self intersection on instances
	if (isec->orig.ob == obj->ob && obj->ob) {
		changed = 1;
		isec->orig.ob = obj->target_ob;
	}

	// backup old values
	copy_v3_v3(start, isec->start);
	copy_v3_v3(dir, isec->dir);
	copy_v3_v3(idot_axis, isec->idot_axis);
	dist = isec->dist;

	// transform to target coordinates system
	mul_m4_v3(obj->global2target, isec->start);
	mul_mat3_m4_v3(obj->global2target, isec->dir);
	isec->dist *= normalize_v3(isec->dir);

	// update idot_axis and bv_index
	for (i = 0; i < 3; i++) {
		isec->idot_axis[i]        = 1.0f / isec->dir[i];

		isec->bv_index[2 * i]     = isec->idot_axis[i] < 0.0f ? 1 : 0;
		isec->bv_index[2 * i + 1] = 1 - isec->bv_index[2 * i];

		isec->bv_index[2 * i]     = i + 3 * isec->bv_index[2 * i];
		isec->bv_index[2 * i + 1] = i + 3 * isec->bv_index[2 * i + 1];
	}

	// raycast
	res = RE_rayobject_intersect(obj->target, isec);

	// map dist into original coordinate space
	if (res == 0) {
		isec->dist = dist;
	}
	else {
		// note we don't just multiply dist, because of possible
		// non-uniform scaling in the transform matrix
		float vec[3];

		mul_v3_v3fl(vec, isec->dir, isec->dist);
		mul_mat3_m4_v3(obj->target2global, vec);

		isec->dist = len_v3(vec);
		isec->hit.ob = obj->ob;

#ifdef RT_USE_LAST_HIT
		// TODO support for last hit optimization in instances that can jump
		// directly to the last hit face.
		// For now it jumps directly to the last-hit instance root node.
		isec->last_hit = RE_rayobject_unalignRayAPI((RayObject *) obj);
#endif
	}

	// restore values
	copy_v3_v3(isec->start, start);
	copy_v3_v3(isec->dir, dir);
	copy_v3_v3(isec->idot_axis, idot_axis);

	if (changed)
		isec->orig.ob = obj->ob;

	// restore bv_index
	for (i = 0; i < 3; i++) {
		isec->bv_index[2 * i]     = isec->idot_axis[i] < 0.0f ? 1 : 0;
		isec->bv_index[2 * i + 1] = 1 - isec->bv_index[2 * i];

		isec->bv_index[2 * i]     = i + 3 * isec->bv_index[2 * i];
		isec->bv_index[2 * i + 1] = i + 3 * isec->bv_index[2 * i + 1];
	}

	return res;
}

static void RE_rayobject_instance_free(RayObject *o)
{
	InstanceRayObject *obj = (InstanceRayObject *)o;
	MEM_freeN(obj);
}

static float RE_rayobject_instance_cost(RayObject *o)
{
	InstanceRayObject *obj = (InstanceRayObject *)o;
	return RE_rayobject_cost(obj->target) + RE_COST_INSTANCE;
}

static void RE_rayobject_instance_bb(RayObject *o, float *min, float *max)
{
	//TODO:
	// *better bb.. calculated without rotations of bb
	// *maybe cache that better-fitted-BB at the InstanceRayObject
	InstanceRayObject *obj = (InstanceRayObject *)o;

	float m[3], M[3], t[3];
	int i, j;
	INIT_MINMAX(m, M);
	RE_rayobject_merge_bb(obj->target, m, M);

	//There must be a faster way than rotating all the 8 vertexs of the BB
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 3; j++) t[j] = i & (1 << j) ? M[j] : m[j];
		mul_m4_v3(obj->target2global, t);
		DO_MINMAX(t, min, max);
	}
}

