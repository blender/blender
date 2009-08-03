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
#include <stdio.h>

#include "MEM_guardedalloc.h"
#include "BKE_utildefines.h"
#include "BLI_arithb.h"
#include "RE_raytrace.h"
#include "rayobject_rtbuild.h"
#include "rayobject.h"

#define BIH_NCHILDS	4
typedef struct BIHTree BIHTree;

static int  bih_intersect(BIHTree *obj, Isect *isec);
static void bih_add(BIHTree *o, RayObject *ob);
static void bih_done(BIHTree *o);
static void bih_free(BIHTree *o);
static void bih_bb(BIHTree *o, float *min, float *max);

static RayObjectAPI bih_api =
{
	(RE_rayobject_raycast_callback) bih_intersect,
	(RE_rayobject_add_callback)     bih_add,
	(RE_rayobject_done_callback)    bih_done,
	(RE_rayobject_free_callback)    bih_free,
	(RE_rayobject_merge_bb_callback)bih_bb
};

typedef struct BIHNode BIHNode;
struct BIHNode
{
	BIHNode *child[BIH_NCHILDS];
	float bi[BIH_NCHILDS][2];
	int split_axis;
};

struct BIHTree
{
	RayObject rayobj;

	BIHNode *root;

	BIHNode *node_alloc, *node_next;
	RTBuilder *builder;

	float bb[2][3];
};


RayObject *RE_rayobject_bih_create(int size)
{
	BIHTree *obj= (BIHTree*)MEM_callocN(sizeof(BIHTree), "BIHTree");
	assert( RayObject_isAligned(obj) ); /* RayObject API assumes real data to be 4-byte aligned */	
	
	obj->rayobj.api = &bih_api;
	obj->root = NULL;
	
	obj->node_alloc = obj->node_next = NULL;
	obj->builder    = rtbuild_create( size );
	
	return RayObject_unalignRayAPI((RayObject*) obj);
}

static void bih_free(BIHTree *obj)
{
	if(obj->builder)
		rtbuild_free(obj->builder);

	if(obj->node_alloc)
		MEM_freeN(obj->node_alloc);

	MEM_freeN(obj);
}

static void bih_bb(BIHTree *obj, float *min, float *max)
{
	DO_MIN(obj->bb[0], min);
	DO_MAX(obj->bb[1], max);
}

/*
 * Tree transverse
 */
static int dfs_raycast(const BIHNode *const node, Isect *isec, float tmin, float tmax)
{
	int i;
	int hit = 0;

	const int *const offset = isec->bv_index + node->split_axis*2;

	//TODO diving heuristic
	for(i=0; i<BIH_NCHILDS; i++)
	{

		float t1 = (node->bi[i][offset[0]] - isec->start[node->split_axis]) * isec->idot_axis[node->split_axis];
		float t2 = (node->bi[i][offset[1]] - isec->start[node->split_axis]) * isec->idot_axis[node->split_axis];

		if(t1 < tmin) t1 = tmin; //t1 = MAX2(t1, tmin);
		if(t2 > tmax) t2 = tmax; //t2 = MIN2(t2, tmax);

		if(t1 <= t2)
		{
				if(RayObject_isAligned(node->child[i]))
				{
					if(node->child[i] == 0) break;
					
					hit |= dfs_raycast(node->child[i], isec, t1, t2);
					if(hit && isec->mode == RE_RAY_SHADOW) return hit;
				}
				else
				{
					hit |= RE_rayobject_intersect( (RayObject*)node->child[i], isec);
					if(hit && isec->mode == RE_RAY_SHADOW) return hit;
				}

				if(tmax > isec->labda)
					tmax = isec->labda;
		}
	}

	return hit;
}

static int bih_intersect(BIHTree *obj, Isect *isec)
{
	if(RayObject_isAligned(obj->root))
		return dfs_raycast(obj->root, isec, 0, isec->labda);
	else
		return RE_rayobject_intersect( (RayObject*)obj->root, isec);
}


/*
 * Builds a BIH tree from builder object
 */
static void bih_add(BIHTree *obj, RayObject *ob)
{
	rtbuild_add( obj->builder, ob );
}

static BIHNode *bih_new_node(BIHTree *tree, int nid)
{
	BIHNode *node = tree->node_alloc + nid - 1;
	assert(RayObject_isAligned(node));
	if(node+1 > tree->node_next)
		tree->node_next = node+1;
		
	return node;
}

static int child_id(int pid, int nchild)
{
	//N child of node A = A * K + (2 - K) + N, (0 <= N < K)
	return pid*BIH_NCHILDS+(2-BIH_NCHILDS)+nchild;
}

static BIHNode *bih_rearrange(BIHTree *tree, RTBuilder *builder, int nid, float *bb)
{
	if(rtbuild_size(builder) == 1)
	{
		RayObject *child = rtbuild_get_primitive( builder, 0 );
		assert(!RayObject_isAligned(child));

		INIT_MINMAX(bb, bb+3);
		RE_rayobject_merge_bb( (RayObject*)child, bb, bb+3);

		return (BIHNode*)child;
	}
	else
	{
		int i;
		int nc = rtbuild_mean_split_largest_axis(builder, BIH_NCHILDS);
		RTBuilder tmp;
	
		BIHNode *parent = bih_new_node(tree, nid);

		INIT_MINMAX(bb, bb+3);
		parent->split_axis = builder->split_axis;
		for(i=0; i<nc; i++)
		{
			float cbb[6];
			parent->child[i] = bih_rearrange( tree, rtbuild_get_child(builder, i, &tmp), child_id(nid,i), cbb );

			parent->bi[i][0] = cbb[parent->split_axis];
			parent->bi[i][1] = cbb[parent->split_axis+3];

			DO_MIN(cbb  , bb);
			DO_MAX(cbb+3, bb+3);
		}
		for(; i<BIH_NCHILDS; i++)
		{
			parent->bi[i][0] =  1.0;
			parent->bi[i][1] = -1.0;
			parent->child[i] = 0;
		}

		return parent;
	}
}

static void bih_done(BIHTree *obj)
{
	int needed_nodes;
	assert(obj->root == NULL && obj->node_alloc == NULL && obj->builder);

	//TODO exact calculate needed nodes
	needed_nodes = (rtbuild_size(obj->builder)+1)*2;
	assert(needed_nodes > 0);

	obj->node_alloc = (BIHNode*)MEM_mallocN( sizeof(BIHNode)*needed_nodes, "BIHTree.Nodes");
	obj->node_next  = obj->node_alloc;

	obj->root = bih_rearrange( obj, obj->builder, 1, (float*)obj->bb );

	rtbuild_free( obj->builder );
	obj->builder = NULL;
	
	assert(obj->node_alloc+needed_nodes >= obj->node_next);
}

