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
#include <queue>

#define BVHNode VBVHNode
#define BVHTree VBVHTree


#define RE_DO_HINTS	(0)
#define RAY_BB_TEST_COST (0.2f)
#define DFS_STACK_SIZE	256
//#define DYNAMIC_ALLOC_BB

//#define rtbuild_split	rtbuild_mean_split_largest_axis		/* objects mean split on the longest axis, childs BB are allowed to overlap */
//#define rtbuild_split	rtbuild_median_split_largest_axis	/* space median split on the longest axis, childs BB are allowed to overlap */
#define rtbuild_split	rtbuild_heuristic_object_split		/* split objects using heuristic */

struct BVHNode
{
#ifdef DYNAMIC_ALLOC_BB
	float *bb;
#else
	float	bb[6];
#endif

	BVHNode *child;
	BVHNode *sibling;
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
	Node *child = node->child;

	if(!RayObject_isAligned(child))
	{
		stack[stack_pos++] = child;
	}
	else
	{
		while(child)
		{
			//Skips BB tests on primitives
/*
			if(!RayObject_isAligned(child->child))
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
static BVHNode *bvh_new_node(BVHTree *tree)
{
	BVHNode *node = (BVHNode*)BLI_memarena_alloc(tree->node_arena, sizeof(BVHNode));
	
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

template<class Builder>
float rtbuild_area(Builder *builder)
{
	float min[3], max[3];
	INIT_MINMAX(min, max);
	rtbuild_merge_bb(builder, min, max);
	return bb_area(min, max);	
}

template<class Node>
void bvh_update_bb(Node *node)
{
	INIT_MINMAX(node->bb, node->bb+3);
	Node *child = node->child;
	
	while(child)
	{
		bvh_node_merge_bb(child, node->bb, node->bb+3);
		if(RayObject_isAligned(child))
			child = child->sibling;
		else
			child = 0;
	}
}


static int tot_pushup   = 0;
static int tot_pushdown = 0;
static int tot_hints    = 0;

template<class Node>
void pushdown(Node *parent)
{
	Node **s_child = &parent->child;
	Node * child = parent->child;
	
	while(child && RayObject_isAligned(child))
	{
		Node *next = child->sibling;
		Node **next_s_child = &child->sibling;
		
		//assert(bb_fits_inside(parent->bb, parent->bb+3, child->bb, child->bb+3));
		
		for(Node *i = parent->child; RayObject_isAligned(i) && i; i = i->sibling)
		if(child != i && bb_fits_inside(i->bb, i->bb+3, child->bb, child->bb+3) && RayObject_isAligned(i->child))
		{
//			todo optimize (should the one with the smallest area?)
//			float ia = bb_area(i->bb, i->bb+3)
//			if(child->i)
			*s_child = child->sibling;
			child->sibling = i->child;
			i->child = child;
			next_s_child = s_child;
			
			tot_pushdown++;
			break;
		}
		child = next;
		s_child = next_s_child;
	}
	
	for(Node *i = parent->child; RayObject_isAligned(i) && i; i = i->sibling)
		pushdown( i );	
}

template<class Node>
int count_childs(Node *parent)
{
	int n = 0;
	for(Node *i = parent->child; i; i = i->sibling)
	{
		n++;
		if(!RayObject_isAligned(i))
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

template<class Node>
void pushup(Node *parent)
{
	float p_area = bb_area(parent->bb, parent->bb+3);
	Node **prev = &parent->child;
	for(Node *child = parent->child; RayObject_isAligned(child) && child; )
	{
		float c_area = bb_area(child->bb, child->bb+3) ;
		int nchilds = count_childs(child);
		float original_cost = (c_area / p_area)*nchilds + 1;
		float flatten_cost = nchilds;
		if(flatten_cost < original_cost && nchilds >= 2)
		{
			append_sibling(child, child->child);
			child = child->sibling;
			*prev = child;

//			*prev = child->child;
//			append_sibling( *prev, child->sibling );
//			child = *prev;
			tot_pushup++;
		}
		else
		{
			*prev = child;
			prev = &(*prev)->sibling;
			child = *prev;
		}		
	}
	
	for(Node *child = parent->child; RayObject_isAligned(child) && child; child = child->sibling)
		pushup(child);
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
		node->child = (BVHNode*) rtbuild_get_primitive( builder, 0 );
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

template<class Node>
float bvh_refit(Node *node)
{
	if(!RayObject_isAligned(node)) return 0;	
	if(!RayObject_isAligned(node->child)) return 0;
	
	float total = 0;
	
	for(Node *child = node->child; child; child = child->sibling)
		total += bvh_refit(child);
		
	float old_area = bb_area(node->bb, node->bb+3);
	INIT_MINMAX(node->bb, node->bb+3);
	for(Node *child = node->child; child; child = child->sibling)
	{
		DO_MIN(child->bb, node->bb);
		DO_MAX(child->bb+3, node->bb+3);
	}
	total += old_area - bb_area(node->bb, node->bb+3);
	return total;
}

template<>
void bvh_done<BVHTree>(BVHTree *obj)
{
	rtbuild_done(obj->builder);
	
	int needed_nodes = (rtbuild_size(obj->builder)+1)*2;
	if(needed_nodes > BLI_MEMARENA_STD_BUFSIZE)
		needed_nodes = BLI_MEMARENA_STD_BUFSIZE;

	obj->node_arena = BLI_memarena_new(needed_nodes);
	BLI_memarena_use_malloc(obj->node_arena);
	BLI_memarena_use_align(obj->node_arena, 16);

	
	obj->root = bvh_rearrange<BVHTree,BVHNode,RTBuilder>( obj, obj->builder );
	reorganize(obj->root);
	remove_useless(obj->root, &obj->root);
	printf("refit: %f\n", bvh_refit(obj->root) );
	pushup(obj->root);
	pushdown(obj->root);
//	obj->root = memory_rearrange(obj->root);
	obj->cost = 1.0;
	
	rtbuild_free( obj->builder );
	obj->builder = NULL;
}

template<int StackSize>
int intersect(BVHTree *obj, Isect* isec)
{
	if(RE_DO_HINTS && isec->hint)
	{
		LCTSHint *lcts = (LCTSHint*)isec->hint;
		isec->hint = 0;
		
		int hit = 0;
		for(int i=0; i<lcts->size; i++)
		{
			BVHNode *node = (BVHNode*)lcts->stack[i];
			if(RayObject_isAligned(node))
				hit |= bvh_node_stack_raycast_simd<BVHNode,StackSize,true>(node, isec);
			else
				hit |= RE_rayobject_intersect( (RayObject*)node, isec );
			
			if(hit && isec->mode == RE_RAY_SHADOW)
				break;
		}
		isec->hint = (RayHint*)lcts;
		return hit;
	}
	else
	{
		if(RayObject_isAligned(obj->root))
			return bvh_node_stack_raycast_simd<BVHNode,StackSize,false>(obj->root, isec);
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
	
	if(!RayObject_isAligned(node))
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
	if(RE_DO_HINTS)
	{
		HintBB bb;
		VECCOPY(bb.bb, min);
		VECCOPY(bb.bb+3, max);
	
		hint->size = 0;
		bvh_dfs_make_hint( tree->root, hint, 0, &bb );
		tot_hints++;
	}
	else
	{
		hint->size = 0;
		hint->stack[hint->size++] = (RayObject*)tree->root;
		tot_hints++;
	}
}

void bfree(BVHTree *tree)
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
template<int STACK_SIZE>
static RayObjectAPI make_api()
{
	static RayObjectAPI api = 
	{
		(RE_rayobject_raycast_callback) ((int(*)(BVHTree*,Isect*)) &intersect<STACK_SIZE>),
		(RE_rayobject_add_callback)     ((void(*)(BVHTree*,RayObject*)) &bvh_add<BVHTree>),
		(RE_rayobject_done_callback)    ((void(*)(BVHTree*))       &bvh_done<BVHTree>),
//		(RE_rayobject_free_callback)    ((void(*)(BVHTree*))       &bvh_free<BVHTree>),
		(RE_rayobject_free_callback)    ((void(*)(BVHTree*))       &bfree),
		(RE_rayobject_merge_bb_callback)((void(*)(BVHTree*,float*,float*)) &bvh_bb<BVHTree>),
		(RE_rayobject_cost_callback)	((float(*)(BVHTree*))      &bvh_cost<BVHTree>),
		(RE_rayobject_hint_bb_callback)	((void(*)(BVHTree*,LCTSHint*,float*,float*)) &bvh_hint_bb<BVHTree>)
	};
	
	return api;
}

static RayObjectAPI* get_api(int maxstacksize)
{
//	static RayObjectAPI bvh_api16  = make_api<16>();
//	static RayObjectAPI bvh_api32  = make_api<32>();
//	static RayObjectAPI bvh_api64  = make_api<64>();
	static RayObjectAPI bvh_api128 = make_api<128>();
	static RayObjectAPI bvh_api256 = make_api<256>();
	
//	if(maxstacksize <= 16 ) return &bvh_api16;
//	if(maxstacksize <= 32 ) return &bvh_api32;
//	if(maxstacksize <= 64 ) return &bvh_api64;
	if(maxstacksize <= 128) return &bvh_api128;
	if(maxstacksize <= 256) return &bvh_api256;
	assert(maxstacksize <= 256);
	return 0;
}

RayObject *RE_rayobject_vbvh_create(int size)
{
	BVHTree *obj= (BVHTree*)MEM_callocN(sizeof(BVHTree), "BVHTree");
	assert( RayObject_isAligned(obj) ); /* RayObject API assumes real data to be 4-byte aligned */	
	
	obj->rayobj.api = get_api(DFS_STACK_SIZE);
	obj->root = NULL;
	
	obj->node_arena = NULL;
	obj->builder    = rtbuild_create( size );
	
	return RayObject_unalignRayAPI((RayObject*) obj);
}
