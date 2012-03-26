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

/** \file blender/render/intern/raytrace/svbvh.h
 *  \ingroup render
 */


#ifdef __SSE__
 
#ifndef __SVBVH_H__
#define __SVBVH_H__

#include "bvh.h"
#include "BLI_memarena.h"
#include "BKE_global.h"
#include <stdio.h>
#include <algorithm>

struct SVBVHNode
{
	float child_bb[24];
	SVBVHNode *child[4];
	int nchilds;
};

static int svbvh_bb_intersect_test_simd4(const Isect *isec, const __m128 *bb_group)
{
	const __m128 tmin0 = _mm_setzero_ps();
	const __m128 tmax0 = _mm_set_ps1(isec->dist);

	const __m128 start0 = _mm_set_ps1(isec->start[0]);
	const __m128 start1 = _mm_set_ps1(isec->start[1]);
	const __m128 start2 = _mm_set_ps1(isec->start[2]);
	const __m128 sub0 = _mm_sub_ps(bb_group[isec->bv_index[0]], start0);
	const __m128 sub1 = _mm_sub_ps(bb_group[isec->bv_index[1]], start0);
	const __m128 sub2 = _mm_sub_ps(bb_group[isec->bv_index[2]], start1);
	const __m128 sub3 = _mm_sub_ps(bb_group[isec->bv_index[3]], start1);
	const __m128 sub4 = _mm_sub_ps(bb_group[isec->bv_index[4]], start2);
	const __m128 sub5 = _mm_sub_ps(bb_group[isec->bv_index[5]], start2);
	const __m128 idot_axis0 = _mm_set_ps1(isec->idot_axis[0]);
	const __m128 idot_axis1 = _mm_set_ps1(isec->idot_axis[1]);
	const __m128 idot_axis2 = _mm_set_ps1(isec->idot_axis[2]);
	const __m128 mul0 = _mm_mul_ps(sub0, idot_axis0);
	const __m128 mul1 = _mm_mul_ps(sub1, idot_axis0);
	const __m128 mul2 = _mm_mul_ps(sub2, idot_axis1);
	const __m128 mul3 = _mm_mul_ps(sub3, idot_axis1);
	const __m128 mul4 = _mm_mul_ps(sub4, idot_axis2);
	const __m128 mul5 = _mm_mul_ps(sub5, idot_axis2);
	const __m128 tmin1 = _mm_max_ps(tmin0, mul0);
	const __m128 tmax1 = _mm_min_ps(tmax0, mul1);
	const __m128 tmin2 = _mm_max_ps(tmin1, mul2);
	const __m128 tmax2 = _mm_min_ps(tmax1, mul3);
	const __m128 tmin3 = _mm_max_ps(tmin2, mul4);
	const __m128 tmax3 = _mm_min_ps(tmax2, mul5);
	
	return _mm_movemask_ps(_mm_cmpge_ps(tmax3, tmin3));
}

static int svbvh_bb_intersect_test(const Isect *isec, const float *_bb)
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

static bool svbvh_node_is_leaf(const SVBVHNode *node)
{
	return !RE_rayobject_isAligned(node);
}

template<int MAX_STACK_SIZE, bool SHADOW>
static int svbvh_node_stack_raycast(SVBVHNode *root, Isect *isec)
{
	SVBVHNode *stack[MAX_STACK_SIZE], *node;
	int hit = 0, stack_pos = 0;

	stack[stack_pos++] = root;

	while(stack_pos)
	{
		node = stack[--stack_pos];

		if(!svbvh_node_is_leaf(node))
		{
			int nchilds= node->nchilds;

			if(nchilds == 4) {
				float *child_bb= node->child_bb;
				int res = svbvh_bb_intersect_test_simd4(isec, ((__m128*) (child_bb)));
				SVBVHNode **child= node->child;

				RE_RC_COUNT(isec->raycounter->simd_bb.test);

				if(res & 1) { stack[stack_pos++] = child[0]; RE_RC_COUNT(isec->raycounter->simd_bb.hit); }
				if(res & 2) { stack[stack_pos++] = child[1]; RE_RC_COUNT(isec->raycounter->simd_bb.hit); }
				if(res & 4) { stack[stack_pos++] = child[2]; RE_RC_COUNT(isec->raycounter->simd_bb.hit); }
				if(res & 8) { stack[stack_pos++] = child[3]; RE_RC_COUNT(isec->raycounter->simd_bb.hit); }
			}
			else {
				float *child_bb= node->child_bb;
				SVBVHNode **child= node->child;
				int i;

				for(i=0; i<nchilds; i++)
					if(svbvh_bb_intersect_test(isec, (float*)child_bb+6*i))
						stack[stack_pos++] = child[i];
			}
		}
		else
		{
			hit |= RE_rayobject_intersect((RayObject*)node, isec);
			if(SHADOW && hit) break;
		}
	}

	return hit;
}


template<>
inline void bvh_node_merge_bb<SVBVHNode>(SVBVHNode *node, float *min, float *max)
{
	if(is_leaf(node))
	{
		RE_rayobject_merge_bb((RayObject*)node, min, max);
	}
	else
	{
		int i=0;
		while(i+4 <= node->nchilds)
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



/*
 * Builds a SVBVH tree form a VBVHTree
 */
template<class OldNode>
struct Reorganize_SVBVH
{
	MemArena *arena;

	float childs_per_node;
	int nodes_with_childs[16];
	int useless_bb;
	int nodes;

	Reorganize_SVBVH(MemArena *a)
	{
		arena = a;
		nodes = 0;
		childs_per_node = 0;
		useless_bb = 0;
		
		for(int i=0; i<16; i++)
			nodes_with_childs[i] = 0;
	}
	
	~Reorganize_SVBVH()
	{
		if(G.f & G_DEBUG) {
			printf("%f childs per node\n", childs_per_node / nodes);
			printf("%d childs BB are useless\n", useless_bb);
			for(int i=0; i<16; i++)
				printf("%i childs per node: %d/%d = %f\n", i, nodes_with_childs[i], nodes,  nodes_with_childs[i]/float(nodes));
		}
	}
	
	SVBVHNode *create_node(int nchilds)
	{
		SVBVHNode *node = (SVBVHNode*)BLI_memarena_alloc(arena, sizeof(SVBVHNode));
		node->nchilds = nchilds;

		return node;
	}
	
	void copy_bb(float *bb, const float *old_bb)
	{
		std::copy(old_bb, old_bb+6, bb);
	}
	
	void prepare_for_simd(SVBVHNode *node)
	{
		int i=0;
		while(i+4 <= node->nchilds)
		{
			float vec_tmp[4*6];
			float *res = node->child_bb+6*i;
			std::copy(res, res+6*4, vec_tmp);
			
			for(int j=0; j<6; j++)
			{
				res[4*j+0] = vec_tmp[6*0+j];
				res[4*j+1] = vec_tmp[6*1+j];
				res[4*j+2] = vec_tmp[6*2+j];
				res[4*j+3] = vec_tmp[6*3+j];
			}

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
			node->child[alloc_childs] = NULL;
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
				RE_rayobject_merge_bb((RayObject*)o_child, bb, bb+3);
				copy_bb(node->child_bb+i*6, bb);
				break;
			}
			else
			{
				copy_bb(node->child_bb+i*6, o_child->bb);
			}
		}
		assert(i == 0);

		prepare_for_simd(node);
		
		return node;
	}	
};

#endif

#endif //__SSE__

