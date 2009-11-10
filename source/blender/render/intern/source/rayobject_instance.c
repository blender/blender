/**
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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): André Pinto.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <assert.h>

#include "MEM_guardedalloc.h"
#include "BKE_utildefines.h"
#include "BLI_math.h"
#include "RE_raytrace.h"
#include "rayobject.h"

#define RE_COST_INSTANCE (1.0f)

static int  RE_rayobject_instance_intersect(RayObject *o, Isect *isec);
static void RE_rayobject_instance_free(RayObject *o);
static void RE_rayobject_instance_bb(RayObject *o, float *min, float *max);
static float RE_rayobject_instance_cost(RayObject *o);

static void RE_rayobject_instance_hint_bb(RayObject *o, RayHint *hint, float *min, float *max)
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

typedef struct InstanceRayObject
{
	RayObject rayobj;
	RayObject *target;

	void *ob; //Object represented by this instance
	void *target_ob; //Object represented by the inner RayObject, needed to handle self-intersection
	
	float global2target[4][4];
	float target2global[4][4];
	
} InstanceRayObject;


RayObject *RE_rayobject_instance_create(RayObject *target, float transform[][4], void *ob, void *target_ob)
{
	InstanceRayObject *obj= (InstanceRayObject*)MEM_callocN(sizeof(InstanceRayObject), "InstanceRayObject");
	assert( RE_rayobject_isAligned(obj) ); /* RayObject API assumes real data to be 4-byte aligned */	
	
	obj->rayobj.api = &instance_api;
	obj->target = target;
	obj->ob = ob;
	obj->target_ob = target_ob;
	
	copy_m4_m4(obj->target2global, transform);
	invert_m4_m4(obj->global2target, obj->target2global);
	
	return RE_rayobject_unalignRayAPI((RayObject*) obj);
}

static int  RE_rayobject_instance_intersect(RayObject *o, Isect *isec)
{
	//TODO
	// *there is probably a faster way to convert between coordinates
	
	InstanceRayObject *obj = (InstanceRayObject*)o;
	int res;
	float start[3], vec[3], labda, dist;
	int changed = 0, i;
	
	//TODO - this is disabling self intersection on instances
	if(isec->orig.ob == obj->ob && obj->ob)
	{
		changed = 1;
		isec->orig.ob = obj->target_ob;
	}
	
	
	VECCOPY( start, isec->start );
	VECCOPY( vec  , isec->vec   );
	labda = isec->labda;
	dist  = isec->dist;

	//Transform to target coordinates system
	VECADD( isec->vec, isec->vec, isec->start );	

	mul_m4_v3(obj->global2target, isec->start);
	mul_m4_v3(obj->global2target, isec->vec  );

	isec->dist = len_v3v3( isec->start, isec->vec );
	VECSUB( isec->vec, isec->vec, isec->start );
	
	isec->labda *= isec->dist / dist;
	
	//Update idot_axis and bv_index
	for(i=0; i<3; i++)
	{
		isec->idot_axis[i]		= 1.0f / isec->vec[i];
		
		isec->bv_index[2*i]		= isec->idot_axis[i] < 0.0 ? 1 : 0;
		isec->bv_index[2*i+1]	= 1 - isec->bv_index[2*i];
		
		isec->bv_index[2*i]		= i+3*isec->bv_index[2*i];
		isec->bv_index[2*i+1]	= i+3*isec->bv_index[2*i+1];
	}

	//Raycast
	res = RE_rayobject_intersect(obj->target, isec);

	//Restore coordinate space coords
	if(res == 0)
	{
		isec->labda = labda;
	}
	else
	{
		isec->labda *= dist / isec->dist;
		isec->hit.ob = obj->ob;
	}
	isec->dist = dist;
	VECCOPY( isec->start, start );
	VECCOPY( isec->vec,   vec );
	
	if(changed)
		isec->orig.ob = obj->ob;

	//Update idot_axis and bv_index
	for(i=0; i<3; i++)
	{
		isec->idot_axis[i]		= 1.0f / isec->vec[i];
		
		isec->bv_index[2*i]		= isec->idot_axis[i] < 0.0 ? 1 : 0;
		isec->bv_index[2*i+1]	= 1 - isec->bv_index[2*i];
		
		isec->bv_index[2*i]		= i+3*isec->bv_index[2*i];
		isec->bv_index[2*i+1]	= i+3*isec->bv_index[2*i+1];
	}
		
	return res;
}

static void RE_rayobject_instance_free(RayObject *o)
{
	InstanceRayObject *obj = (InstanceRayObject*)o;
	MEM_freeN(obj);
}

static float RE_rayobject_instance_cost(RayObject *o)
{
	InstanceRayObject *obj = (InstanceRayObject*)o;
	return RE_rayobject_cost(obj->target) + RE_COST_INSTANCE;
}

static void RE_rayobject_instance_bb(RayObject *o, float *min, float *max)
{
	//TODO:
	// *better bb.. calculated without rotations of bb
	// *maybe cache that better-fitted-BB at the InstanceRayObject
	InstanceRayObject *obj = (InstanceRayObject*)o;

	float m[3], M[3], t[3];
	int i, j;
	INIT_MINMAX(m, M);
	RE_rayobject_merge_bb(obj->target, m, M);

	//There must be a faster way than rotating all the 8 vertexs of the BB
	for(i=0; i<8; i++)
	{
		for(j=0; j<3; j++) t[j] = i&(1<<j) ? M[j] : m[j];
		mul_m4_v3(obj->target2global, t);
		DO_MINMAX(t, min, max);
	}
}
