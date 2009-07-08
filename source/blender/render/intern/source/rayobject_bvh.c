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
#include "BLI_memarena.h"
#include "RE_raytrace.h"
#include "rayobject_rtbuild.h"
#include "rayobject.h"

#define DFS_STACK_SIZE	64
#define DYNAMIC_ALLOC

//#define SPLIT_OVERLAP_MEAN_LONGEST_AXIS		/* objects mean split on the longest axis, childs BB are allowed to overlap */
//#define SPLIT_OVERLAP_MEDIAN_LONGEST_AXIS	/* space median split on the longest axis, childs BB are allowed to overlap */
#define SPLIT_OBJECTS_SAH					/* split objects using heuristic */

#define BVH_NCHILDS	2
typedef struct BVHTree BVHTree;

static int  bvh_intersect(BVHTree *obj, Isect *isec);
static int  bvh_intersect_stack(BVHTree *obj, Isect *isec);
static void bvh_add(BVHTree *o, RayObject *ob);
static void bvh_done(BVHTree *o);
static void bvh_free(BVHTree *o);
static void bvh_bb(BVHTree *o, float *min, float *max);

static RayObjectAPI bvh_api =
{
#ifdef DFS_STACK_SIZE
	(RE_rayobject_raycast_callback) bvh_intersect_stack,
#else
	(RE_rayobject_raycast_callback) bvh_intersect,
#endif
	(RE_rayobject_add_callback)     bvh_add,
	(RE_rayobject_done_callback)    bvh_done,
	(RE_rayobject_free_callback)    bvh_free,
	(RE_rayobject_merge_bb_callback)bvh_bb
};

typedef struct BVHNode BVHNode;
struct BVHNode
{
	BVHNode *child[BVH_NCHILDS];
#ifdef DYNAMIC_ALLOC
	float	bb[6];
#else
	float	*bb; //[6]; //[2][3];
#endif
	int split_axis;
};

struct BVHTree
{
	RayObject rayobj;

	BVHNode *root;

#ifdef DYNAMIC_ALLOC
	MemArena *node_arena;
#else
	BVHNode *node_alloc, *node_next;
	float *bb_alloc, *bb_next;
#endif
	RTBuilder *builder;

};


RayObject *RE_rayobject_bvh_create(int size)
{
	BVHTree *obj= (BVHTree*)MEM_callocN(sizeof(BVHTree), "BVHTree");
	assert( RayObject_isAligned(obj) ); /* RayObject API assumes real data to be 4-byte aligned */	
	
	obj->rayobj.api = &bvh_api;
	obj->root = NULL;
	
#ifdef DYNAMIC_ALLOC
	obj->node_arena = NULL;
#else
	obj->node_alloc = obj->node_next = NULL;
	obj->bb_alloc   = obj->bb_next = NULL;
#endif
	obj->builder    = rtbuild_create( size );
	
	return RayObject_unalignRayAPI((RayObject*) obj);
}

static void bvh_free(BVHTree *obj)
{
	if(obj->builder)
		rtbuild_free(obj->builder);

#ifdef DYNAMIC_ALLOC
	if(obj->node_arena)
		BLI_memarena_free(obj->node_arena);
#else
	if(obj->node_alloc)
		MEM_freeN(obj->node_alloc);

	if(obj->bb_alloc)
		MEM_freeN(obj->bb_alloc);
#endif

	MEM_freeN(obj);
}


static void bvh_merge_bb(BVHNode *node, float *min, float *max)
{
	if(RayObject_isAligned(node))
	{
		DO_MIN(node->bb  , min);
		DO_MAX(node->bb+3, max);
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
static int dfs_raycast_stack(BVHNode *root, Isect *isec)
{
	BVHNode *stack[DFS_STACK_SIZE];
	int hit = 0, stack_pos = 0;
	
	stack[stack_pos++] = root;
	
	while(stack_pos)
	{
		BVHNode *node = stack[--stack_pos];
		if(RayObject_isAligned(node))
		{
			if(RE_rayobject_bb_intersect(isec, (const float*)node->bb) != FLT_MAX)
			{
				//push nodes in reverse visit order
				if(isec->idot_axis[node->split_axis] < 0.0f)
				{
					int i;
					for(i=0; i<BVH_NCHILDS; i++)
						if(node->child[i] == 0) break;
						else stack[stack_pos++] = node->child[i];
				}
				else
				{
					int i;	
					for(i=0; i<BVH_NCHILDS; i++)
						if(node->child[i] != 0) stack[stack_pos++] = node->child[i];
						else break;
				}
				assert(stack_pos <= DFS_STACK_SIZE);
			}
		}
		else
		{
			hit |= RE_rayobject_intersect( (RayObject*)node, isec);
			if(hit && isec->mode == RE_RAY_SHADOW) return hit;
		}
	}
	return hit;
}

static int bvh_intersect_stack(BVHTree *obj, Isect *isec)
{
	if(RayObject_isAligned(obj->root))
		return dfs_raycast_stack(obj->root, isec);
	else
		return RE_rayobject_intersect( (RayObject*)obj->root, isec);
}

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
					if(node->child[i])
					{
						hit |= dfs_raycast(node->child[i], isec);
						if(hit && isec->mode == RE_RAY_SHADOW) return hit;
					}
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
#ifdef DYNAMIC_ALLOC
	BVHNode *node = BLI_memarena_alloc(tree->node_arena, sizeof(BVHNode));
	return node;
#else
	BVHNode *node = tree->node_alloc + nid - 1;
	assert(RayObject_isAligned(node));
	if(node+1 > tree->node_next)
		tree->node_next = node+1;
		
	node->bb = tree->bb_alloc + (nid - 1)*6;
	tree->bb_next += 6;
	
	return node;
#endif
}

static int child_id(int pid, int nchild)
{
	//N child of node A = A * K + (2 - K) + N, (0 <= N < K)
	return pid*BVH_NCHILDS+(2-BVH_NCHILDS)+nchild;
}

static BVHNode *bvh_rearrange(BVHTree *tree, RTBuilder *builder, int nid)
{
	if(rtbuild_size(builder) == 0)
		return 0;

	if(rtbuild_size(builder) == 1)
	{
		RayObject *child = builder->begin[0];

		if(RayObject_isRayFace(child))
		{
			int i;
			BVHNode *parent = bvh_new_node(tree, nid);
			parent->split_axis = 0;

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
			assert(!RayObject_isAligned(child));
			//Its a sub-raytrace structure, assume it has it own raycast
			//methods and adding a Bounding Box arround is unnecessary
			return (BVHNode*)child;
		}
	}
	else
	{
		int i;
		RTBuilder tmp;
		BVHNode *parent = bvh_new_node(tree, nid);
		int nc; 

#ifdef SPLIT_OVERLAP_MEAN_LONGEST_AXIS
		nc = rtbuild_mean_split_largest_axis(builder, BVH_NCHILDS);
#elif defined(SPLIT_OVERLAP_MEDIAN_LONGEST_AXIS)
		nc = rtbuild_median_split_largest_axis(builder, BVH_NCHILDS);
#elif defined(SPLIT_OBJECTS_SAH)
		nc = rtbuild_heuristic_object_split(builder, BVH_NCHILDS);
#else
		assert(0);
#endif	

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

/*
static void bvh_info(BVHTree *obj)
{
	printf("BVH: Used %d nodes\n", obj->node_next - obj->node_alloc);
}
*/
	
static void bvh_done(BVHTree *obj)
{


#ifdef DYNAMIC_ALLOC
	int needed_nodes = (rtbuild_size(obj->builder)+1)*2;
	if(needed_nodes > BLI_MEMARENA_STD_BUFSIZE)
		needed_nodes = BLI_MEMARENA_STD_BUFSIZE;

	obj->node_arena = BLI_memarena_new(needed_nodes);
	BLI_memarena_use_malloc(obj->node_arena);

#else
	int needed_nodes;

	//TODO exact calculate needed nodes
	needed_nodes = (rtbuild_size(obj->builder)+1)*2;
	assert(needed_nodes > 0);

	BVHNode *node = BLI_memarena_alloc(tree->node_arena, sizeof(BVHNode));
	return node;
	obj->node_alloc = (BVHNode*)MEM_mallocN( sizeof(BVHNode)*needed_nodes, "BVHTree.Nodes");
	obj->node_next  = obj->node_alloc;

	obj->bb_alloc = (float*)MEM_mallocN( sizeof(float)*6*needed_nodes, "BVHTree.NodesBB");
	obj->bb_next  = obj->bb_alloc;
#endif
	
	obj->root = bvh_rearrange( obj, obj->builder, 1 );
	
#ifndef DYNAMIC_ALLOC
	assert(obj->node_alloc+needed_nodes >= obj->node_next);
#endif

	rtbuild_free( obj->builder );
	obj->builder = NULL;
}

