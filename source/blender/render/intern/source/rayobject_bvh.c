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
#include "BLI_kdopbvh.h"
#include "BLI_arithb.h"
#include "RE_raytrace.h"
#include "render_types.h"
#include "rayobject.h"

static int  RayObject_bvh_intersect(RayObject *o, Isect *isec);
static void RayObject_bvh_add(RayObject *o, RayObject *ob);
static void RayObject_bvh_done(RayObject *o);
static void RayObject_bvh_free(RayObject *o);
static void RayObject_bvh_bb(RayObject *o, float *min, float *max);

static RayObjectAPI bvh_api =
{
	RayObject_bvh_intersect,
	RayObject_bvh_add,
	RayObject_bvh_done,
	RayObject_bvh_free,
	RayObject_bvh_bb
};

typedef struct BVHObject
{
	RayObject rayobj;
	BVHTree *bvh;

} BVHObject;


RayObject *RE_rayobject_bvh_create(int size)
{
	BVHObject *obj= (BVHObject*)MEM_callocN(sizeof(BVHObject), "BVHObject");
	assert( RayObject_isAligned(obj) ); /* RayObject API assumes real data to be 4-byte aligned */	
	
	obj->rayobj.api = &bvh_api;
	obj->bvh = BLI_bvhtree_new(size, 0.0, 4, 6);
	
	return RayObject_unalign((RayObject*) obj);
}

static void bvh_callback(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	Isect *isect = (Isect*)userdata;
	RayObject *face = (RayObject*)index;
	
	if(RE_rayobject_intersect(face,isect))
	{
		hit->index = index;
//		hit.distance = TODO
	}
}

static int  RayObject_bvh_intersect(RayObject *o, Isect *isec)
{
	BVHObject *obj = (BVHObject*)o;
	float dir[3];
	VECCOPY( dir, isec->vec );
	Normalize( dir );
	
	//BLI_bvhtree_ray_cast returns -1 on non hit (in case we dont give a Hit structure
	return BLI_bvhtree_ray_cast(obj->bvh, isec->start, dir, 0.0, NULL, bvh_callback, isec) != -1;
}

static void RayObject_bvh_add(RayObject *o, RayObject *ob)
{
	BVHObject *obj = (BVHObject*)o;
	float min_max[6];
	INIT_MINMAX(min_max, min_max+3);
	RE_rayobject_merge_bb(ob, min_max, min_max+3);	
	BLI_bvhtree_insert(obj->bvh, (int)ob, min_max, 2 );
}

static void RayObject_bvh_done(RayObject *o)
{
	BVHObject *obj = (BVHObject*)o;
	BLI_bvhtree_balance(obj->bvh);
}

static void RayObject_bvh_free(RayObject *o)
{
	BVHObject *obj = (BVHObject*)o;

	if(obj->bvh)
		BLI_bvhtree_free(obj->bvh);

	MEM_freeN(obj);
}

static void RayObject_bvh_bb(RayObject *o, float *min, float *max)
{
	assert(0);
}
