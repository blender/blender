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
#include <xmmintrin.h>

inline int test_bb_group4(__m128 *bb_group, Isect *isec)
{
/*
	const float *bb = _bb;
	__m128 tmin={0}, tmax = {isec->labda};

	tmin = _mm_max_ps(tmin, _mm_mul_ps( _mm_sub_ps( bb_group[isec->bv_index[0]], isec->sse_start[0] ), isec->sse_idot_axis[0]) );
	tmax = _mm_min_ps(tmax, _mm_mul_ps( _mm_sub_ps( bb_group[isec->bv_index[1]], isec->sse_start[0] ), isec->sse_idot_axis[0]) );	
	tmin = _mm_max_ps(tmin, _mm_mul_ps( _mm_sub_ps( bb_group[isec->bv_index[2]], isec->sse_start[1] ), isec->sse_idot_axis[1]) );
	tmax = _mm_min_ps(tmax, _mm_mul_ps( _mm_sub_ps( bb_group[isec->bv_index[3]], isec->sse_start[1] ), isec->sse_idot_axis[1]) );
	tmin = _mm_max_ps(tmin, _mm_mul_ps( _mm_sub_ps( bb_group[isec->bv_index[4]], isec->sse_start[2] ), isec->sse_idot_axis[2]) );
	tmax = _mm_min_ps(tmax, _mm_mul_ps( _mm_sub_ps( bb_group[isec->bv_index[5]], isec->sse_start[2] ), isec->sse_idot_axis[2]) );
	*/
	return 4; //_mm_movemask_ps(_mm_cmpge_ps(tmax, tmin));
}


/* bvh tree generics */
template<class Tree> static int bvh_intersect(Tree *obj, Isect *isec);

template<class Tree> static void bvh_add(Tree *obj, RayObject *ob)
{
	rtbuild_add( obj->builder, ob );
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
	return RE_rayobject_bb_intersect_test(isec, (const float*)node->bb);
}


template<class Node>
static void bvh_node_merge_bb(Node *node, float *min, float *max)
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



/*
 * recursivly transverse a BVH looking for a rayhit using a local stack
 */
template<class Node> static inline void bvh_node_push_childs(Node *node, Isect *isec, Node **stack, int &stack_pos);

template<class Node,int MAX_STACK_SIZE,bool TEST_ROOT>
static int bvh_node_stack_raycast(Node *root, Isect *isec)
{
	Node *stack[MAX_STACK_SIZE];
	int hit = 0, stack_pos = 0;
		
	if(!TEST_ROOT && RayObject_isAligned(root))
		bvh_node_push_childs(root, isec, stack, stack_pos);
	else
		stack[stack_pos++] = root;

	while(stack_pos)
	{
		Node *node = stack[--stack_pos];
		if(RayObject_isAligned(node))
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
			if(hit && isec->mode == RE_RAY_SHADOW) return hit;
		}
	}
	return hit;
}


/*
 * Generic SIMD bvh recursion
 * this was created to be able to use any simd (with the cost of some memmoves)
 * it can take advantage of any SIMD width and doens't needs any special tree care
 */
template<class Node,int MAX_STACK_SIZE,bool TEST_ROOT>
static int bvh_node_stack_raycast_simd(Node *root, Isect *isec)
{
	Node *stack[MAX_STACK_SIZE];
	__m128 t_bb[6];
	Node * t_node[4];

	int hit = 0, stack_pos = 0;
		
	if(!TEST_ROOT)
	{
		if(RayObject_isAligned(root))
		{
			if(RayObject_isAligned(root->child))
				bvh_node_push_childs(root, isec, stack, stack_pos);
			else
				return RE_rayobject_intersect( (RayObject*)root->child, isec);
		}
		else
			return RE_rayobject_intersect( (RayObject*)root, isec);
	}
	else
	{
		if(RayObject_isAligned(root))
			stack[stack_pos++] = root;
		else
			return RE_rayobject_intersect( (RayObject*)root, isec);
	}

	while(true)
	{
		//Use SIMD 4
		if(0 && stack_pos >= 4)
		{
			stack_pos -= 4;
			for(int i=0; i<4; i++)
			{
				Node *t = stack[stack_pos+i];
				assert(RayObject_isAligned(t));
				
				float *bb = ((float*)t_bb)+i;
				bb[4*0] = t->bb[0];
				bb[4*1] = t->bb[1];
				bb[4*2] = t->bb[2];
				bb[4*3] = t->bb[3];
				bb[4*4] = t->bb[4];
				bb[4*5] = t->bb[5];
				t_node[i] = t->child;
			}
			int res = test_bb_group4( t_bb, isec );

			for(int i=0; i<4; i++)
			if(res & (1<<i))
			{
				if(RayObject_isAligned(t_node[i]))
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
			assert(RayObject_isAligned(node));
			
			if(bvh_node_hit_test(node,isec))
			{
				if(RayObject_isAligned(node->child))
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
				if(RayObject_isAligned(node->child[i]))
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
*/
