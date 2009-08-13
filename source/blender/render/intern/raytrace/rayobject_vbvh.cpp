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
#define RE_USE_HINT	(0)
static int tot_pushup   = 0;
static int tot_pushdown = 0;
static int tot_hints    = 0;


extern "C"
{
#include <assert.h>
#include "MEM_guardedalloc.h"
#include "BKE_utildefines.h"
#include "BLI_arithb.h"
#include "BLI_memarena.h"
#include "RE_raytrace.h"
#include "rayobject_rtbuild.h"
#include "rayobject.h"
};

#include "rayobject_hint.h"
#include "reorganize.h"
#include "bvh.h"
#include "svbvh.h"
#include <queue>


#define RE_DO_HINTS	(0)
#define RAY_BB_TEST_COST (0.2f)
#define DFS_STACK_SIZE	256
//#define DYNAMIC_ALLOC_BB


//#define rtbuild_split	rtbuild_mean_split_largest_axis		/* objects mean split on the longest axis, childs BB are allowed to overlap */
//#define rtbuild_split	rtbuild_median_split_largest_axis	/* space median split on the longest axis, childs BB are allowed to overlap */
#define rtbuild_split	rtbuild_heuristic_object_split		/* split objects using heuristic */

struct VBVHNode
{
#ifdef DYNAMIC_ALLOC_BB
	float *bb;
#else
	float	bb[6];
#endif

	VBVHNode *child;
	VBVHNode *sibling;
};

struct VBVHTree
{
	RayObject rayobj;

	SVBVHNode *root;

	MemArena *node_arena;

	float cost;
	RTBuilder *builder;
};




template<class Tree,class OldNode>
struct Reorganize_VBVH
{
	Tree *tree;
	
	Reorganize_VBVH(Tree *t)
	{
		tree = t;
	}
	
	VBVHNode *create_node()
	{
		VBVHNode *node = (VBVHNode*)BLI_memarena_alloc(tree->node_arena, sizeof(VBVHNode));
		return node;
	}
	
	void copy_bb(VBVHNode *node, OldNode *old)
	{
		std::copy( old->bb, old->bb+6, node->bb );
	}
	
	VBVHNode *transform(OldNode *old)
	{
		if(is_leaf(old))
			return (VBVHNode*)old;

		VBVHNode *node = create_node();
		VBVHNode **child_ptr = &node->child;
		node->sibling = 0;

		copy_bb(node,old);

		for(OldNode *o_child = old->child; o_child; o_child = o_child->sibling)
		{
			VBVHNode *n_child = transform(o_child);
			*child_ptr = n_child;
			if(is_leaf(n_child)) return node;
			child_ptr = &n_child->sibling;
		}
		*child_ptr = 0;
		
		return node;
	}	
};


/*
 * Push nodes (used on dfs)
 */
template<class Node>
inline static void bvh_node_push_childs(Node *node, Isect *isec, Node **stack, int &stack_pos)
{
	Node *child = node->child;

	if(is_leaf(child))
	{
		stack[stack_pos++] = child;
	}
	else
	{
		while(child)
		{
			//Skips BB tests on primitives
/*
			if(is_leaf(child->child))
				stack[stack_pos++] = child->child;
			else
*/
				stack[stack_pos++] = child;
				
			child = child->sibling;
		}
	}
}

/*
 * BVH done
 */
static VBVHNode *bvh_new_node(VBVHTree *tree)
{
	VBVHNode *node = (VBVHNode*)BLI_memarena_alloc(tree->node_arena, sizeof(VBVHNode));
	
	if( (((intptr_t)node) & (0x0f)) != 0 )
	{
		puts("WRONG!");
		printf("%08x\n", (intptr_t)node);
	}
	node->sibling = NULL;
	node->child   = NULL;

#ifdef DYNAMIC_ALLOC_BB
	node->bb = (float*)BLI_memarena_alloc(tree->node_arena, 6*sizeof(float));
#endif
	assert(RayObject_isAligned(node));
	return node;
}



template<class Node>
int count_childs(Node *parent)
{
	int n = 0;
	for(Node *i = parent->child; i; i = i->sibling)
	{
		n++;
		if(is_leaf(i))
			break;
	}
		
	return n;
}

template<class Node>
void append_sibling(Node *node, Node *sibling)
{
	while(node->sibling)
		node = node->sibling;
		
	node->sibling = sibling;
}


template<class Tree, class Node, class Builder>
Node *bvh_rearrange(Tree *tree, Builder *builder)
{
	
	int size = rtbuild_size(builder);
	if(size == 1)
	{
		Node *node = bvh_new_node(tree);
		INIT_MINMAX(node->bb, node->bb+3);
		rtbuild_merge_bb(builder, node->bb, node->bb+3);		
		node->child = (VBVHNode*) rtbuild_get_primitive( builder, 0 );
		return node;
	}
	else
	{
		Node *node = bvh_new_node(tree);

		INIT_MINMAX(node->bb, node->bb+3);
		rtbuild_merge_bb(builder, node->bb, node->bb+3);
		
		Node **child = &node->child;

		int nc = rtbuild_split(builder, 2);
		assert(nc == 2);
		for(int i=0; i<nc; i++)
		{
			Builder tmp;
			rtbuild_get_child(builder, i, &tmp);
			
			*child = bvh_rearrange<Tree,Node,Builder>(tree, &tmp);
			child = &((*child)->sibling);
		}

		*child = 0;
		return node;
	}
}

template<>
void bvh_done<VBVHTree>(VBVHTree *obj)
{
	rtbuild_done(obj->builder);
	
	int needed_nodes = (rtbuild_size(obj->builder)+1)*2;
	if(needed_nodes > BLI_MEMARENA_STD_BUFSIZE)
		needed_nodes = BLI_MEMARENA_STD_BUFSIZE;

	MemArena *arena1 = BLI_memarena_new(needed_nodes);
	BLI_memarena_use_malloc(arena1);
	BLI_memarena_use_align(arena1, 16);
	obj->node_arena = arena1;
	
	VBVHNode *root = bvh_rearrange<VBVHTree,VBVHNode,RTBuilder>( obj, obj->builder );
	reorganize(root);
	remove_useless(root, &root);
	printf("refit: %f\n", bvh_refit(root) );
	
	pushup(root);
	pushdown(root);
	pushup_simd<VBVHNode,4>(root);

	//Memory re-organize
	if(0)
	{
		MemArena *arena2 = BLI_memarena_new(needed_nodes);
		BLI_memarena_use_malloc(arena2);
		BLI_memarena_use_align(arena2, 16);
		obj->node_arena = arena2;
		root = Reorganize_VBVH<VBVHTree,VBVHNode>(obj).transform(root);
	
		BLI_memarena_free(arena1);
	}

	if(1)
	{
		MemArena *arena2 = BLI_memarena_new(needed_nodes);
		BLI_memarena_use_malloc(arena2);
		BLI_memarena_use_align(arena2, 16);
		obj->node_arena = arena2;
		obj->root = Reorganize_SVBVH<VBVHTree,VBVHNode>(obj).transform(root);
	
		BLI_memarena_free(arena1);
	}
/*
	{
		obj->root = root;	
	}
*/

	obj->cost = 1.0;
	
	rtbuild_free( obj->builder );
	obj->builder = NULL;
}

template<int StackSize>
int intersect(VBVHTree *obj, Isect* isec)
{
/*
	if(RE_DO_HINTS && isec->hint)
	{
		LCTSHint *lcts = (LCTSHint*)isec->hint;
		isec->hint = 0;
		
		int hit = 0;
		for(int i=0; i<lcts->size; i++)
		{
			VBVHNode *node = (VBVHNode*)lcts->stack[i];
			if(RayObject_isAligned(node))
				hit |= bvh_node_stack_raycast<VBVHNode,StackSize,true>(node, isec);
			else
				hit |= RE_rayobject_intersect( (RayObject*)node, isec );
			
			if(hit && isec->mode == RE_RAY_SHADOW)
				break;
		}
		isec->hint = (RayHint*)lcts;
		return hit;
	}
	else
*/
	{
		if(RayObject_isAligned(obj->root))
			return bvh_node_stack_raycast<SVBVHNode,StackSize,false>( obj->root, isec);
		else
			return RE_rayobject_intersect( (RayObject*) obj->root, isec );
	}
}

template<class Node,class HintObject>
void bvh_dfs_make_hint(Node *node, LCTSHint *hint, int reserve_space, HintObject *hintObject);

template<class Node,class HintObject>
void bvh_dfs_make_hint_push_siblings(Node *node, LCTSHint *hint, int reserve_space, HintObject *hintObject)
{
	if(!RayObject_isAligned(node))
		hint->stack[hint->size++] = (RayObject*)node;
	else
	{
		if(node->sibling)
			bvh_dfs_make_hint_push_siblings(node->sibling, hint, reserve_space+1, hintObject);

		bvh_dfs_make_hint(node, hint, reserve_space, hintObject);
	}	
}

template<class Node,class HintObject>
void bvh_dfs_make_hint(Node *node, LCTSHint *hint, int reserve_space, HintObject *hintObject)
{
	assert( hint->size + reserve_space + 1 <= RE_RAY_LCTS_MAX_SIZE );
	
	if(is_leaf(node))
	{
		hint->stack[hint->size++] = (RayObject*)node;
	}
	else
	{
		int childs = count_childs(node);
		if(hint->size + reserve_space + childs <= RE_RAY_LCTS_MAX_SIZE)
		{
			int result = hint_test_bb(hintObject, node->bb, node->bb+3);
			if(result == HINT_RECURSE)
			{
				/* We are 100% sure the ray will be pass inside this node */
				bvh_dfs_make_hint_push_siblings(node->child, hint, reserve_space, hintObject);
			}
			else if(result == HINT_ACCEPT)
			{
				hint->stack[hint->size++] = (RayObject*)node;
			}
		}
		else
		{
			hint->stack[hint->size++] = (RayObject*)node;
		}
	}
}

template<class Tree>
void bvh_hint_bb(Tree *tree, LCTSHint *hint, float *min, float *max)
{
/*
	if(RE_USE_HINT)
	{
		HintBB bb;
		VECCOPY(bb.bb, min);
		VECCOPY(bb.bb+3, max);

		hint->size = 0;
		bvh_dfs_make_hint( tree->root, hint, 0, &bb );
		tot_hints++;
	}
	else
*/
	{
	 	hint->size = 0;
	 	hint->stack[hint->size++] = (RayObject*)tree->root;
	}
}

void bfree(VBVHTree *tree)
{
	if(tot_pushup + tot_pushdown + tot_hints + tot_moves)
	{
		printf("tot pushups: %d\n", tot_pushup);
		printf("tot pushdowns: %d\n", tot_pushdown);
		printf("tot moves: %d\n", tot_moves);
		printf("tot hints created: %d\n", tot_hints);
		tot_pushup = 0;
		tot_pushdown = 0;
		tot_hints = 0;
		tot_moves = 0;
	}
	bvh_free(tree);
}

/* the cast to pointer function is needed to workarround gcc bug: http://gcc.gnu.org/bugzilla/show_bug.cgi?id=11407 */
template<class Tree,int STACK_SIZE>
static RayObjectAPI make_api()
{
	static RayObjectAPI api = 
	{
		(RE_rayobject_raycast_callback) ((int(*)(Tree*,Isect*)) &intersect<STACK_SIZE>),
		(RE_rayobject_add_callback)     ((void(*)(Tree*,RayObject*)) &bvh_add<Tree>),
		(RE_rayobject_done_callback)    ((void(*)(Tree*))       &bvh_done<Tree>),
//		(RE_rayobject_free_callback)    ((void(*)(Tree*))       &bvh_free<Tree>),
		(RE_rayobject_free_callback)    ((void(*)(Tree*))       &bfree),
		(RE_rayobject_merge_bb_callback)((void(*)(Tree*,float*,float*)) &bvh_bb<Tree>),
		(RE_rayobject_cost_callback)	((float(*)(Tree*))      &bvh_cost<Tree>),
		(RE_rayobject_hint_bb_callback)	((void(*)(Tree*,LCTSHint*,float*,float*)) &bvh_hint_bb<Tree>)
	};
	
	return api;
}

template<class Tree>
static RayObjectAPI* get_api(int maxstacksize)
{
	static RayObjectAPI bvh_api256 = make_api<Tree,1024>();
	
	if(maxstacksize <= 1024) return &bvh_api256;
	assert(maxstacksize <= 256);
	return 0;
}

RayObject *RE_rayobject_vbvh_create(int size)
{
	VBVHTree *obj= (VBVHTree*)MEM_callocN(sizeof(VBVHTree), "VBVHTree");
	assert( RayObject_isAligned(obj) ); /* RayObject API assumes real data to be 4-byte aligned */	
	
	obj->rayobj.api = get_api<VBVHTree>(DFS_STACK_SIZE);
	obj->root = NULL;
	
	obj->node_arena = NULL;
	obj->builder    = rtbuild_create( size );
	
	return RayObject_unalignRayAPI((RayObject*) obj);
}



/* SVBVH */
template<class HintObject>
void bvh_dfs_make_hint(VBVHNode *node, LCTSHint *hint, int reserve_space, HintObject *hintObject)
{
	return;
}
/*
RayObject *RE_rayobject_svbvh_create(int size)
{
	SVBVHTree *obj= (SVBVHTree*)MEM_callocN(sizeof(SVBVHTree), "SVBVHTree");
	assert( RayObject_isAligned(obj) ); // RayObject API assumes real data to be 4-byte aligned
	
	obj->rayobj.api = get_api<SVBVHTree>(DFS_STACK_SIZE);
	obj->root = NULL;
	
	obj->node_arena = NULL;
	obj->builder    = rtbuild_create( size );
	
	return RayObject_unalignRayAPI((RayObject*) obj);
}
*/