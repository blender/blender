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

#define BVH_NCHILDS	4
typedef struct BVHTree BVHTree;

static int  bvh_intersect(BVHTree *obj, Isect *isec);
static void bvh_add(BVHTree *o, RayObject *ob);
static void bvh_done(BVHTree *o);
static void bvh_free(BVHTree *o);
static void bvh_bb(BVHTree *o, float *min, float *max);

static RayObjectAPI bvh_api =
{
	(RE_rayobject_raycast_callback) bvh_intersect,
	(RE_rayobject_add_callback)     bvh_add,
	(RE_rayobject_done_callback)    bvh_done,
	(RE_rayobject_free_callback)    bvh_free,
	(RE_rayobject_merge_bb_callback)bvh_bb
};

typedef struct BVHNode BVHNode;
struct BVHNode
{
	BVHNode *child[BVH_NCHILDS];
	float	*bb; //[6]; //[2][3];
	char split_axis;
};

struct BVHTree
{
	RayObject rayobj;

	BVHNode *alloc, *next_node, *root;
	float *bb_alloc, *bb_next;
	RTBuilder *builder;

};


RayObject *RE_rayobject_bvh_create(int size)
{
	BVHTree *obj= (BVHTree*)MEM_callocN(sizeof(BVHTree), "BVHTree");
	assert( RayObject_isAligned(obj) ); /* RayObject API assumes real data to be 4-byte aligned */	
	
	obj->rayobj.api = &bvh_api;
	obj->builder = rtbuild_create( size );
	obj->root = NULL;
	
	return RayObject_unalignRayAPI((RayObject*) obj);
}

static void bvh_free(BVHTree *obj)
{
	if(obj->builder)
		rtbuild_free(obj->builder);

	if(obj->alloc)
		MEM_freeN(obj->alloc);

	if(obj->bb_alloc)
		MEM_freeN(obj->bb_alloc);

	MEM_freeN(obj);
}


static void bvh_merge_bb(BVHNode *node, float *min, float *max)
{
	if(RayObject_isAligned(node))
	{
		//TODO only half operations needed
		DO_MINMAX(node->bb  , min, max);
		DO_MINMAX(node->bb+3, min, max);
	}
	else
	{
		RE_rayobject_merge_bb( (RayObject*)node, min, max);
	}
}

static void bvh_bb(BVHTree *obj, float *min, float *max)
{
	bvh_merge_bb(obj->root, min, max);
}

/*
 * Tree transverse
 */
static int dfs_raycast(BVHNode *node, Isect *isec)
{
	int hit = 0;
	if(RE_rayobject_bb_intersect(isec, (const float*)node->bb) != FLT_MAX)
	{
		if(isec->idot_axis[node->split_axis] > 0.0f)
		{
			int i;
			for(i=0; i<BVH_NCHILDS; i++)
				if(RayObject_isAligned(node->child[i]))
				{
					if(node->child[i] == 0) break;
					
					hit |= dfs_raycast(node->child[i], isec);
					if(hit && isec->mode == RE_RAY_SHADOW) return hit;
				}
				else
				{
					hit |= RE_rayobject_intersect( (RayObject*)node->child[i], isec);
					if(hit && isec->mode == RE_RAY_SHADOW) return hit;
				}
		}
		else
		{
			int i;
			for(i=BVH_NCHILDS-1; i>=0; i--)
				if(RayObject_isAligned(node->child[i]))
				{
					hit |= dfs_raycast(node->child[i], isec);
					if(hit && isec->mode == RE_RAY_SHADOW) return hit;
				}
				else
				{
					hit |= RE_rayobject_intersect( (RayObject*)node->child[i], isec);
					if(hit && isec->mode == RE_RAY_SHADOW) return hit;
				}
		}
	}
	return hit;
}

static int bvh_intersect(BVHTree *obj, Isect *isec)
{
	if(RayObject_isAligned(obj->root))
		return dfs_raycast(obj->root, isec);
	else
		return RE_rayobject_intersect( (RayObject*)obj->root, isec);
}


/*
 * Builds a BVH tree from builder object
 */
static void bvh_add(BVHTree *obj, RayObject *ob)
{
	rtbuild_add( obj->builder, ob );
}

static BVHNode *bvh_new_node(BVHTree *tree, int nid)
{
	BVHNode *node = tree->alloc + nid - 1;
	if(node+1 > tree->next_node)
		tree->next_node = node+1;
		
	node->bb = tree->bb_next;
	tree->bb_next += 6;
	
	return node;
}

static int child_id(int pid, int nchild)
{
	//N child of node A = A * K + (2 - K) + N, (0 <= N < K)
	return pid*BVH_NCHILDS+(2-BVH_NCHILDS)+nchild;
}

static BVHNode *bvh_rearrange(BVHTree *tree, RTBuilder *builder, int nid)
{
	if(rtbuild_size(builder) == 1)
	{
//		return (BVHNode*)builder->begin[0];
//
//
		int i;
		BVHNode *parent = bvh_new_node(tree, nid);
		
		INIT_MINMAX(parent->bb, parent->bb+3);

		for(i=0; i<1; i++)
		{
			parent->child[i] = (BVHNode*)builder->begin[i];
			bvh_merge_bb(parent->child[i], parent->bb, parent->bb+3);
		}
		for(; i<BVH_NCHILDS; i++)
			parent->child[i] = 0;

		return parent;
	}
	else
	{
		int i;
		int nc = rtbuild_mean_split_largest_axis(builder, BVH_NCHILDS);
		RTBuilder tmp;
	
		BVHNode *parent = bvh_new_node(tree, nid);

		INIT_MINMAX(parent->bb, parent->bb+3);
		parent->split_axis = builder->split_axis;
		for(i=0; i<nc; i++)
		{
			parent->child[i] = bvh_rearrange( tree, rtbuild_get_child(builder, i, &tmp), child_id(nid,i) );
			bvh_merge_bb(parent->child[i], parent->bb, parent->bb+3);
		}
		for(; i<BVH_NCHILDS; i++)
			parent->child[i] = 0;

		return parent;
	}
}
	
static void bvh_done(BVHTree *obj)
{
	int needed_nodes;
	assert(obj->root == NULL && obj->next_node == NULL && obj->builder);

	needed_nodes = (rtbuild_size(obj->builder)+1)*2;
	assert(needed_nodes > 0);
	obj->alloc = (BVHNode*)MEM_mallocN( sizeof(BVHNode)*needed_nodes, "BVHTree.Nodes");
	obj->next_node = obj->alloc;

	obj->bb_alloc = (float*)MEM_mallocN( sizeof(float)*6*needed_nodes, "BVHTree.NodesBB");
	obj->bb_next  = obj->bb_alloc;
	
	obj->root = bvh_rearrange( obj, obj->builder, 1 );

	assert(obj->alloc+needed_nodes >= obj->next_node);
	
	printf("BVH: Used %d nodes\n", obj->next_node-obj->alloc);
	

	rtbuild_free( obj->builder );
	obj->builder = NULL;
}

