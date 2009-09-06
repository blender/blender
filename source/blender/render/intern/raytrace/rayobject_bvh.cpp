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

#include "RE_raytrace.h"
#include "rayobject_rtbuild.h"
#include "rayobject.h"
#include "MEM_guardedalloc.h"
#include "BKE_utildefines.h"
#include "BLI_arithb.h"
#include "BLI_memarena.h"
#include "bvh.h"

#define BVH_NCHILDS 2
#define RAY_BB_TEST_COST (0.2f)
#define DFS_STACK_SIZE	64
#define DYNAMIC_ALLOC

//#define rtbuild_split	rtbuild_mean_split_largest_axis		/* objects mean split on the longest axis, childs BB are allowed to overlap */
//#define rtbuild_split	rtbuild_median_split_largest_axis	/* space median split on the longest axis, childs BB are allowed to overlap */
#define rtbuild_split	rtbuild_heuristic_object_split		/* split objects using heuristic */

struct BVHNode
{
	BVHNode *child[BVH_NCHILDS];
	float	bb[6];
	int split_axis;
};

struct BVHTree
{
	RayObject rayobj;

	BVHNode *root;

	MemArena *node_arena;

	float cost;
	RTBuilder *builder;
};

/*
 * Push nodes (used on dfs)
 */
template<class Node>
inline static void bvh_node_push_childs(Node *node, Isect *isec, Node **stack, int &stack_pos)
{
	//push nodes in reverse visit order
	if(isec->idot_axis[node->split_axis] < 0.0f)
	{
		int i;
		for(i=0; i<BVH_NCHILDS; i++)
			if(node->child[i] == 0)
				break;
			else
				stack[stack_pos++] = node->child[i];
	}
	else
	{
		int i;	
		for(i=BVH_NCHILDS-1; i>=0; i--)
			if(node->child[i] != 0)
				stack[stack_pos++] = node->child[i];
	}
}

/*
 * BVH done
 */
static BVHNode *bvh_new_node(BVHTree *tree, int nid)
{
	BVHNode *node = (BVHNode*)BLI_memarena_alloc(tree->node_arena, sizeof(BVHNode));
	return node;
}

static int child_id(int pid, int nchild)
{
	//N child of node A = A * K + (2 - K) + N, (0 <= N < K)
    return pid*BVH_NCHILDS+(2-BVH_NCHILDS)+nchild;
}
        

static BVHNode *bvh_rearrange(BVHTree *tree, RTBuilder *builder, int nid, float *cost)
{
	*cost = 0;
	if(rtbuild_size(builder) == 0)
		return 0;

	if(rtbuild_size(builder) == 1)
	{
		RayObject *child = rtbuild_get_primitive( builder, 0 );

		if(RE_rayobject_isRayFace(child))
		{
			int i;
			BVHNode *parent = bvh_new_node(tree, nid);
			parent->split_axis = 0;

			INIT_MINMAX(parent->bb, parent->bb+3);

			for(i=0; i<1; i++)
			{
				parent->child[i] = (BVHNode*)rtbuild_get_primitive( builder, i );
				bvh_node_merge_bb(parent->child[i], parent->bb, parent->bb+3);
			}
			for(; i<BVH_NCHILDS; i++)
				parent->child[i] = 0;

			*cost = RE_rayobject_cost(child)+RAY_BB_TEST_COST;
			return parent;
		}
		else
		{
			assert(!RE_rayobject_isAligned(child));
			//Its a sub-raytrace structure, assume it has it own raycast
			//methods and adding a Bounding Box arround is unnecessary

			*cost = RE_rayobject_cost(child);
			return (BVHNode*)child;
		}
	}
	else
	{
		int i;
		RTBuilder tmp;
		BVHNode *parent = bvh_new_node(tree, nid);
		int nc = rtbuild_split(builder, BVH_NCHILDS); 


		INIT_MINMAX(parent->bb, parent->bb+3);
		parent->split_axis = builder->split_axis;
		for(i=0; i<nc; i++)
		{
			float cbb[6];
			float tcost;
			parent->child[i] = bvh_rearrange( tree, rtbuild_get_child(builder, i, &tmp), child_id(nid,i), &tcost );
			
			INIT_MINMAX(cbb, cbb+3);
			bvh_node_merge_bb(parent->child[i], cbb, cbb+3);
			DO_MIN(cbb,   parent->bb);
			DO_MAX(cbb+3, parent->bb+3);
			
			*cost += tcost*bb_area(cbb, cbb+3);
		}
		for(; i<BVH_NCHILDS; i++)
			parent->child[i] = 0;

		*cost /= bb_area(parent->bb, parent->bb+3);
		*cost += nc*RAY_BB_TEST_COST;
		return parent;
	}

	assert(false);
}

template<>
void bvh_done<BVHTree>(BVHTree *obj)
{
	int needed_nodes = (rtbuild_size(obj->builder)+1)*2;
	if(needed_nodes > BLI_MEMARENA_STD_BUFSIZE)
		needed_nodes = BLI_MEMARENA_STD_BUFSIZE;

	obj->node_arena = BLI_memarena_new(needed_nodes);
	BLI_memarena_use_malloc(obj->node_arena);

	
	obj->root = bvh_rearrange( obj, obj->builder, 1, &obj->cost );
	
	rtbuild_free( obj->builder );
	obj->builder = NULL;
}

template<>
int bvh_intersect<BVHTree>(BVHTree *obj, Isect* isec)
{
	if(RE_rayobject_isAligned(obj->root))
		return bvh_node_stack_raycast<BVHNode,64,true>(obj->root, isec);
	else
		return RE_rayobject_intersect( (RayObject*) obj->root, isec );
}


/* the cast to pointer function is needed to workarround gcc bug: http://gcc.gnu.org/bugzilla/show_bug.cgi?id=11407 */
static RayObjectAPI bvh_api =
{
	(RE_rayobject_raycast_callback) ((int(*)(BVHTree*,Isect*)) &bvh_intersect<BVHTree>),
	(RE_rayobject_add_callback)     ((void(*)(BVHTree*,RayObject*)) &bvh_add<BVHTree>),
	(RE_rayobject_done_callback)    ((void(*)(BVHTree*))       &bvh_done<BVHTree>),
	(RE_rayobject_free_callback)    ((void(*)(BVHTree*))       &bvh_free<BVHTree>),
	(RE_rayobject_merge_bb_callback)((void(*)(BVHTree*,float*,float*)) &bvh_bb<BVHTree>),
	(RE_rayobject_cost_callback)	((float(*)(BVHTree*))      &bvh_cost<BVHTree>)
};


RayObject *RE_rayobject_bvh_create(int size)
{
	BVHTree *obj= (BVHTree*)MEM_callocN(sizeof(BVHTree), "BVHTree");
	assert( RE_rayobject_isAligned(obj) ); /* RayObject API assumes real data to be 4-byte aligned */	
	
	obj->rayobj.api = &bvh_api;
	obj->root = NULL;
	
	obj->node_arena = NULL;
	obj->builder    = rtbuild_create( size );
	
	return RE_rayobject_unalignRayAPI((RayObject*) obj);
}
