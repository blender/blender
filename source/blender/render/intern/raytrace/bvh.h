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

/** \file blender/render/intern/raytrace/bvh.h
 *  \ingroup render
 */


#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "raycounter.h"
#include "rayintersection.h"
#include "rayobject.h"
#include "rayobject_hint.h"
#include "rayobject_rtbuild.h"

#include <assert.h>

#ifdef __SSE__
#include <xmmintrin.h>
#endif

#ifndef __BVH_H__
#define __BVH_H__

#ifdef __SSE__
inline int test_bb_group4(__m128 *bb_group, const Isect *isec)
{
	const __m128 tmin0 = _mm_setzero_ps();
	const __m128 tmax0 = _mm_set_ps1(isec->dist);

	float start[3], idot_axis[3];
	copy_v3_v3(start, isec->start);
	copy_v3_v3(idot_axis, isec->idot_axis);

	const __m128 tmin1 = _mm_max_ps(tmin0, _mm_mul_ps( _mm_sub_ps( bb_group[isec->bv_index[0]], _mm_set_ps1(start[0]) ), _mm_set_ps1(idot_axis[0])) );
	const __m128 tmax1 = _mm_min_ps(tmax0, _mm_mul_ps( _mm_sub_ps( bb_group[isec->bv_index[1]], _mm_set_ps1(start[0]) ), _mm_set_ps1(idot_axis[0])) );
	const __m128 tmin2 = _mm_max_ps(tmin1, _mm_mul_ps( _mm_sub_ps( bb_group[isec->bv_index[2]], _mm_set_ps1(start[1]) ), _mm_set_ps1(idot_axis[1])) );
	const __m128 tmax2 = _mm_min_ps(tmax1, _mm_mul_ps( _mm_sub_ps( bb_group[isec->bv_index[3]], _mm_set_ps1(start[1]) ), _mm_set_ps1(idot_axis[1])) );
	const __m128 tmin3 = _mm_max_ps(tmin2, _mm_mul_ps( _mm_sub_ps( bb_group[isec->bv_index[4]], _mm_set_ps1(start[2]) ), _mm_set_ps1(idot_axis[2])) );
	const __m128 tmax3 = _mm_min_ps(tmax2, _mm_mul_ps( _mm_sub_ps( bb_group[isec->bv_index[5]], _mm_set_ps1(start[2]) ), _mm_set_ps1(idot_axis[2])) );
	
	return _mm_movemask_ps(_mm_cmpge_ps(tmax3, tmin3));
}
#endif

/*
 * Determines the distance that the ray must travel to hit the bounding volume of the given node
 * Based on Tactical Optimization of Ray/Box Intersection, by Graham Fyffe
 *  [http://tog.acm.org/resources/RTNews/html/rtnv21n1.html#art9]
 */
static int rayobject_bb_intersect_test(const Isect *isec, const float *_bb)
{
	const float *bb = _bb;
	
	float t1x = (bb[isec->bv_index[0]] - isec->start[0]) * isec->idot_axis[0];
	float t2x = (bb[isec->bv_index[1]] - isec->start[0]) * isec->idot_axis[0];
	float t1y = (bb[isec->bv_index[2]] - isec->start[1]) * isec->idot_axis[1];
	float t2y = (bb[isec->bv_index[3]] - isec->start[1]) * isec->idot_axis[1];
	float t1z = (bb[isec->bv_index[4]] - isec->start[2]) * isec->idot_axis[2];
	float t2z = (bb[isec->bv_index[5]] - isec->start[2]) * isec->idot_axis[2];

	RE_RC_COUNT(isec->raycounter->bb.test);
	
	if(t1x > t2y || t2x < t1y || t1x > t2z || t2x < t1z || t1y > t2z || t2y < t1z) return 0;
	if(t2x < 0.0 || t2y < 0.0 || t2z < 0.0) return 0;
	if(t1x > isec->dist || t1y > isec->dist || t1z > isec->dist) return 0;
	RE_RC_COUNT(isec->raycounter->bb.hit);	

	return 1;
}

/* bvh tree generics */
template<class Tree> static int bvh_intersect(Tree *obj, Isect *isec);

template<class Tree> static void bvh_add(Tree *obj, RayObject *ob)
{
	rtbuild_add( obj->builder, ob );
}

template<class Node>
inline bool is_leaf(Node *node)
{
	return !RE_rayobject_isAligned(node);
}

template<class Tree> static void bvh_done(Tree *obj);

template<class Tree>
static void bvh_free(Tree *obj)
{
	if(obj->builder)
		rtbuild_free(obj->builder);

	if(obj->node_arena)
		BLI_memarena_free(obj->node_arena);

	MEM_freeN(obj);
}

template<class Tree>
static void bvh_bb(Tree *obj, float *min, float *max)
{
	if(obj->root)
		bvh_node_merge_bb(obj->root, min, max);
}


template<class Tree>
static float bvh_cost(Tree *obj)
{
	assert(obj->cost >= 0.0);
	return obj->cost;
}



/* bvh tree nodes generics */
template<class Node> static inline int bvh_node_hit_test(Node *node, Isect *isec)
{
	return rayobject_bb_intersect_test(isec, (const float*)node->bb);
}


template<class Node>
static inline void bvh_node_merge_bb(Node *node, float *min, float *max)
{
	if(is_leaf(node))
	{
		RE_rayobject_merge_bb( (RayObject*)node, min, max);
	}
	else
	{
		DO_MIN(node->bb  , min);
		DO_MAX(node->bb+3, max);
	}
}



/*
 * recursively transverse a BVH looking for a rayhit using a local stack
 */
template<class Node> static inline void bvh_node_push_childs(Node *node, Isect *isec, Node **stack, int &stack_pos);

template<class Node,int MAX_STACK_SIZE,bool TEST_ROOT,bool SHADOW>
static int bvh_node_stack_raycast(Node *root, Isect *isec)
{
	Node *stack[MAX_STACK_SIZE];
	int hit = 0, stack_pos = 0;
		
	if(!TEST_ROOT && !is_leaf(root))
		bvh_node_push_childs(root, isec, stack, stack_pos);
	else
		stack[stack_pos++] = root;

	while(stack_pos)
	{
		Node *node = stack[--stack_pos];
		if(!is_leaf(node))
		{
			if(bvh_node_hit_test(node,isec))
			{
				bvh_node_push_childs(node, isec, stack, stack_pos);
				assert(stack_pos <= MAX_STACK_SIZE);
			}
		}
		else
		{
			hit |= RE_rayobject_intersect( (RayObject*)node, isec);
			if(SHADOW && hit) return hit;
		}
	}
	return hit;
}


#ifdef __SSE__
/*
 * Generic SIMD bvh recursion
 * this was created to be able to use any simd (with the cost of some memmoves)
 * it can take advantage of any SIMD width and doens't needs any special tree care
 */
template<class Node,int MAX_STACK_SIZE,bool TEST_ROOT>
static int bvh_node_stack_raycast_simd(Node *root, Isect *isec)
{
	Node *stack[MAX_STACK_SIZE];

	int hit = 0, stack_pos = 0;
		
	if(!TEST_ROOT)
	{
		if(!is_leaf(root))
		{
			if(!is_leaf(root->child))
				bvh_node_push_childs(root, isec, stack, stack_pos);
			else
				return RE_rayobject_intersect( (RayObject*)root->child, isec);
		}
		else
			return RE_rayobject_intersect( (RayObject*)root, isec);
	}
	else
	{
		if(!is_leaf(root))
			stack[stack_pos++] = root;
		else
			return RE_rayobject_intersect( (RayObject*)root, isec);
	}

	while(true)
	{
		//Use SIMD 4
		if(stack_pos >= 4)
		{
			__m128 t_bb[6];
			Node * t_node[4];
			
			stack_pos -= 4;

			/* prepare the 4BB for SIMD */
			t_node[0] = stack[stack_pos+0]->child;
			t_node[1] = stack[stack_pos+1]->child;
			t_node[2] = stack[stack_pos+2]->child;
			t_node[3] = stack[stack_pos+3]->child;
			
			const float *bb0 = stack[stack_pos+0]->bb;
			const float *bb1 = stack[stack_pos+1]->bb;
			const float *bb2 = stack[stack_pos+2]->bb;
			const float *bb3 = stack[stack_pos+3]->bb;
			
			const __m128 x0y0x1y1 = _mm_shuffle_ps( _mm_load_ps(bb0), _mm_load_ps(bb1), _MM_SHUFFLE(1,0,1,0) );
			const __m128 x2y2x3y3 = _mm_shuffle_ps( _mm_load_ps(bb2), _mm_load_ps(bb3), _MM_SHUFFLE(1,0,1,0) );
			t_bb[0] = _mm_shuffle_ps( x0y0x1y1, x2y2x3y3, _MM_SHUFFLE(2,0,2,0) );
			t_bb[1] = _mm_shuffle_ps( x0y0x1y1, x2y2x3y3, _MM_SHUFFLE(3,1,3,1) );

			const __m128 z0X0z1X1 = _mm_shuffle_ps( _mm_load_ps(bb0), _mm_load_ps(bb1), _MM_SHUFFLE(3,2,3,2) );
			const __m128 z2X2z3X3 = _mm_shuffle_ps( _mm_load_ps(bb2), _mm_load_ps(bb3), _MM_SHUFFLE(3,2,3,2) );
			t_bb[2] = _mm_shuffle_ps( z0X0z1X1, z2X2z3X3, _MM_SHUFFLE(2,0,2,0) );
			t_bb[3] = _mm_shuffle_ps( z0X0z1X1, z2X2z3X3, _MM_SHUFFLE(3,1,3,1) );

			const __m128 Y0Z0Y1Z1 = _mm_shuffle_ps( _mm_load_ps(bb0+4), _mm_load_ps(bb1+4), _MM_SHUFFLE(1,0,1,0) );
			const __m128 Y2Z2Y3Z3 = _mm_shuffle_ps( _mm_load_ps(bb2+4), _mm_load_ps(bb3+4), _MM_SHUFFLE(1,0,1,0) );
			t_bb[4] = _mm_shuffle_ps( Y0Z0Y1Z1, Y2Z2Y3Z3, _MM_SHUFFLE(2,0,2,0) );
			t_bb[5] = _mm_shuffle_ps( Y0Z0Y1Z1, Y2Z2Y3Z3, _MM_SHUFFLE(3,1,3,1) );
/*			
			for(int i=0; i<4; i++)
			{
				Node *t = stack[stack_pos+i];
				assert(!is_leaf(t));
				
				float *bb = ((float*)t_bb)+i;
				bb[4*0] = t->bb[0];
				bb[4*1] = t->bb[1];
				bb[4*2] = t->bb[2];
				bb[4*3] = t->bb[3];
				bb[4*4] = t->bb[4];
				bb[4*5] = t->bb[5];
				t_node[i] = t->child;
			}
*/
			RE_RC_COUNT(isec->raycounter->simd_bb.test);
			int res = test_bb_group4( t_bb, isec );

			for(int i=0; i<4; i++)
			if(res & (1<<i))
			{
				RE_RC_COUNT(isec->raycounter->simd_bb.hit);
				if(!is_leaf(t_node[i]))
				{
					for(Node *t=t_node[i]; t; t=t->sibling)
					{
						assert(stack_pos < MAX_STACK_SIZE);
						stack[stack_pos++] = t;
					}
				}
				else
				{
					hit |= RE_rayobject_intersect( (RayObject*)t_node[i], isec);
					if(hit && isec->mode == RE_RAY_SHADOW) return hit;				
				}	
			}
		}
		else if(stack_pos > 0)
		{	
			Node *node = stack[--stack_pos];
			assert(!is_leaf(node));
			
			if(bvh_node_hit_test(node,isec))
			{
				if(!is_leaf(node->child))
				{
					bvh_node_push_childs(node, isec, stack, stack_pos);
					assert(stack_pos <= MAX_STACK_SIZE);
				}
				else
				{
					hit |= RE_rayobject_intersect( (RayObject*)node->child, isec);
					if(hit && isec->mode == RE_RAY_SHADOW) return hit;
				}
			}
		}
		else break;
	}
	return hit;
}
#endif

/*
 * recursively transverse a BVH looking for a rayhit using system stack
 */
/*
template<class Node>
static int bvh_node_raycast(Node *node, Isect *isec)
{
	int hit = 0;
	if(bvh_test_node(node, isec))
	{
		if(isec->idot_axis[node->split_axis] > 0.0f)
		{
			int i;
			for(i=0; i<BVH_NCHILDS; i++)
				if(!is_leaf(node->child[i]))
				{
					if(node->child[i] == 0) break;
					
					hit |= bvh_node_raycast(node->child[i], isec);
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
				if(!is_leaf(node->child[i]))
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
*/

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
static RayObjectAPI* bvh_get_api(int maxstacksize);


template<class Tree, int DFS_STACK_SIZE>
static inline RayObject *bvh_create_tree(int size)
{
	Tree *obj= (Tree*)MEM_callocN(sizeof(Tree), "BVHTree" );
	assert( RE_rayobject_isAligned(obj) ); /* RayObject API assumes real data to be 4-byte aligned */	
	
	obj->rayobj.api = bvh_get_api<Tree>(DFS_STACK_SIZE);
	obj->root = NULL;
	
	obj->node_arena = NULL;
	obj->builder    = rtbuild_create( size );
	
	return RE_rayobject_unalignRayAPI((RayObject*) obj);
}

#endif
