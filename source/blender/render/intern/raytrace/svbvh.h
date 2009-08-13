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
#ifndef RE_RAYTRACE_SVBVH_H
#define RE_RAYTRACE_SVBVH_H

#define SVBVH_SIMD 1

#include "bvh.h"
#include <stdio.h>

struct SVBVHNode
{
	int nchilds;

	//Array of bb, array of childs
	float *child_bb;
	SVBVHNode **child;
};

template<>
inline int bvh_node_hit_test<SVBVHNode>(SVBVHNode *node, Isect *isec)
{
	return 1;
}

template<>
inline void bvh_node_push_childs<SVBVHNode>(SVBVHNode *node, Isect *isec, SVBVHNode **stack, int &stack_pos)
{
	if(SVBVH_SIMD)
	{
		int i=0;
		while(i+4 <= node->nchilds)
		{
			int res = test_bb_group4( (__m128*) (node->child_bb+6*i), isec );
			RE_RC_COUNT(isec->raycounter->bb.test);
			RE_RC_COUNT(isec->raycounter->bb.test);
			RE_RC_COUNT(isec->raycounter->bb.test);
			RE_RC_COUNT(isec->raycounter->bb.test);
			
			if(res & 1) { stack[stack_pos++] = node->child[i+0]; RE_RC_COUNT(isec->raycounter->bb.hit); }
			if(res & 2) { stack[stack_pos++] = node->child[i+1]; RE_RC_COUNT(isec->raycounter->bb.hit); }
			if(res & 4) { stack[stack_pos++] = node->child[i+2]; RE_RC_COUNT(isec->raycounter->bb.hit); }
			if(res & 8) { stack[stack_pos++] = node->child[i+3]; RE_RC_COUNT(isec->raycounter->bb.hit); }
			
			i += 4;
		}
		while(i < node->nchilds)
		{
			if(RE_rayobject_bb_intersect_test(isec, (const float*)node->child_bb+6*i))
				stack[stack_pos++] = node->child[i];
			i++;
		}
	}
	else
	{
		for(int i=0; i<node->nchilds; i++)
		{
			if(RE_rayobject_bb_intersect_test(isec, (const float*)node->child_bb+6*i))
				stack[stack_pos++] = node->child[i];
		}
	}
}

template<>
void bvh_node_merge_bb<SVBVHNode>(SVBVHNode *node, float *min, float *max)
{
	if(is_leaf(node))
	{
		RE_rayobject_merge_bb( (RayObject*)node, min, max);
	}
	else
	{
		int i=0;
		while(SVBVH_SIMD && i+4 <= node->nchilds)
		{
			float *res = node->child_bb + 6*i;
			for(int j=0; j<3; j++)
			{
				min[j] = MIN2(min[j], res[4*j+0]);
				min[j] = MIN2(min[j], res[4*j+1]);
				min[j] = MIN2(min[j], res[4*j+2]);
				min[j] = MIN2(min[j], res[4*j+3]);
			}
			for(int j=0; j<3; j++)
			{
				max[j] = MAX2(max[j], res[4*(j+3)+0]);
				max[j] = MAX2(max[j], res[4*(j+3)+1]);
				max[j] = MAX2(max[j], res[4*(j+3)+2]);
				max[j] = MAX2(max[j], res[4*(j+3)+3]);
			}
			
			i += 4;
		}

		for(; i<node->nchilds; i++)
		{
			DO_MIN(node->child_bb+6*i  , min);
			DO_MAX(node->child_bb+3+6*i, max);
		}
	}
}

struct SVBVHTree
{
	RayObject rayobj;

	SVBVHNode *root;

	MemArena *node_arena;

	float cost;
	RTBuilder *builder;
};



template<class Tree,class OldNode>
struct Reorganize_SVBVH
{
	Tree *tree;

	float childs_per_node;
	int nodes_with_childs[16];
	int useless_bb;
	int nodes;

	Reorganize_SVBVH(Tree *t)
	{
		tree = t;
		nodes = 0;
		childs_per_node = 0;
		useless_bb = 0;
		
		for(int i=0; i<16; i++)
			nodes_with_childs[i] = 0;
	}
	
	~Reorganize_SVBVH()
	{
		printf("%f childs per node\n", childs_per_node / nodes);
		printf("%d childs BB are useless\n", useless_bb);
		for(int i=0; i<16; i++)
			printf("%i childs per node: %d/%d = %f\n", i, nodes_with_childs[i], nodes,  nodes_with_childs[i]/float(nodes));
	}
	
	SVBVHNode *create_node(int nchilds)
	{
		SVBVHNode *node = (SVBVHNode*)BLI_memarena_alloc(tree->node_arena, sizeof(SVBVHNode));
		node->nchilds = nchilds;
		node->child_bb   = (float*)BLI_memarena_alloc(tree->node_arena, sizeof(float)*6*nchilds);
		node->child= (SVBVHNode**)BLI_memarena_alloc(tree->node_arena, sizeof(SVBVHNode*)*nchilds);

		return node;
	}
	
	void copy_bb(float *bb, const float *old_bb)
	{
		std::copy( old_bb, old_bb+6, bb );
	}
	
	void prepare_for_simd(SVBVHNode *node)
	{
		int i=0;
		while(i+4 <= node->nchilds)
		{
			float vec_tmp[4*6];
			float *res = node->child_bb+6*i;
			std::copy( res, res+6*4, vec_tmp);
			
			for(int j=0; j<6; j++)
			{
				res[4*j+0] = vec_tmp[6*0+j];
				res[4*j+1] = vec_tmp[6*1+j];
				res[4*j+2] = vec_tmp[6*2+j];
				res[4*j+3] = vec_tmp[6*3+j];
			}
/*
			const float *bb0 = vec_tmp+6*(i+0);
			const float *bb1 = vec_tmp+6*(i+1);
			const float *bb2 = vec_tmp+6*(i+2);
			const float *bb3 = vec_tmp+6*(i+3);

			//memmoves could be memory alligned
			const __m128 x0y0x1y1 = _mm_shuffle_ps( _mm_loadu_ps(bb0), _mm_loadu_ps(bb1), _MM_SHUFFLE(1,0,1,0) );
			const __m128 x2y2x3y3 = _mm_shuffle_ps( _mm_loadu_ps(bb2), _mm_loadu_ps(bb3), _MM_SHUFFLE(1,0,1,0) );
			_mm_store_ps( node->child_bb+6*i+4*0, _mm_shuffle_ps( x0y0x1y1, x2y2x3y3, _MM_SHUFFLE(2,0,2,0) ) );
			_mm_store_ps( node->child_bb+6*i+4*1, _mm_shuffle_ps( x0y0x1y1, x2y2x3y3, _MM_SHUFFLE(3,1,3,1) ) );

			const __m128 z0X0z1X1 = _mm_shuffle_ps( _mm_loadu_ps(bb0), _mm_loadu_ps(bb1), _MM_SHUFFLE(3,2,3,2) );
			const __m128 z2X2z3X3 = _mm_shuffle_ps( _mm_loadu_ps(bb2), _mm_loadu_ps(bb3), _MM_SHUFFLE(3,2,3,2) );
			_mm_store_ps( node->child_bb+6*i+4*2, _mm_shuffle_ps( z0X0z1X1, z2X2z3X3, _MM_SHUFFLE(2,0,2,0) ) );
			_mm_store_ps( node->child_bb+6*i+4*3, _mm_shuffle_ps( z0X0z1X1, z2X2z3X3, _MM_SHUFFLE(3,1,3,1) ) );

			const __m128 Y0Z0Y1Z1 = _mm_shuffle_ps( _mm_loadu_ps(bb0+4), _mm_loadu_ps(bb1+4), _MM_SHUFFLE(1,0,1,0) );
			const __m128 Y2Z2Y3Z3 = _mm_shuffle_ps( _mm_loadu_ps(bb2+4), _mm_loadu_ps(bb3+4), _MM_SHUFFLE(1,0,1,0) );
			_mm_store_ps( node->child_bb+6*i+4*4, _mm_shuffle_ps( Y0Z0Y1Z1, Y2Z2Y3Z3, _MM_SHUFFLE(2,0,2,0) ) );
			_mm_store_ps( node->child_bb+6*i+4*5, _mm_shuffle_ps( Y0Z0Y1Z1, Y2Z2Y3Z3, _MM_SHUFFLE(3,1,3,1) ) );
 */
			
			i += 4;
		}
	}

	/* amt must be power of two */
	inline int padup(int num, int amt)
	{
		return ((num+(amt-1))&~(amt-1));
	}
	
	SVBVHNode *transform(OldNode *old)
	{
		if(is_leaf(old))
			return (SVBVHNode*)old;
		if(is_leaf(old->child))
			return (SVBVHNode*)old->child;

		int nchilds = count_childs(old);
		int alloc_childs = nchilds;
		if(nchilds % 4 > 2)
			alloc_childs = padup(nchilds, 4);
		
		SVBVHNode *node = create_node(alloc_childs);

		childs_per_node += nchilds;
		nodes++;
		if(nchilds < 16)
			nodes_with_childs[nchilds]++;
		
		useless_bb += alloc_childs-nchilds;
		while(alloc_childs > nchilds)
		{
			const static float def_bb[6] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MIN, FLT_MIN, FLT_MIN };
			alloc_childs--;
			node->child[alloc_childs] = 0;
			copy_bb(node->child_bb+alloc_childs*6, def_bb);
		}
		
		int i=nchilds;
		for(OldNode *o_child = old->child; o_child; o_child = o_child->sibling)
		{
			i--;
			node->child[i] = transform(o_child);
			if(is_leaf(o_child))
			{
				float bb[6];
				INIT_MINMAX(bb, bb+3);
				RE_rayobject_merge_bb( (RayObject*)o_child, bb, bb+3);
				copy_bb(node->child_bb+i*6, bb);
				break;
			}
			else
			{
				copy_bb(node->child_bb+i*6, o_child->bb);
			}
		}
		assert( i == 0 );
		

		if(SVBVH_SIMD)
			prepare_for_simd(node);
		
		return node;
	}	
};

#endif
