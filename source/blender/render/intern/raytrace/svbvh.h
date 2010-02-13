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
#ifdef __SSE__
 
#ifndef RE_RAYTRACE_SVBVH_H
#define RE_RAYTRACE_SVBVH_H

#include "bvh.h"
#include "BLI_memarena.h"
#include "BKE_global.h"
#include <stdio.h>
#include <algorithm>

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
	int i=0;
	while(i+4 <= node->nchilds)
	{
		int res = test_bb_group4( (__m128*) (node->child_bb+6*i), isec );
		RE_RC_COUNT(isec->raycounter->simd_bb.test);
		
		if(res & 1) { stack[stack_pos++] = node->child[i+0]; RE_RC_COUNT(isec->raycounter->simd_bb.hit); }
		if(res & 2) { stack[stack_pos++] = node->child[i+1]; RE_RC_COUNT(isec->raycounter->simd_bb.hit); }
		if(res & 4) { stack[stack_pos++] = node->child[i+2]; RE_RC_COUNT(isec->raycounter->simd_bb.hit); }
		if(res & 8) { stack[stack_pos++] = node->child[i+3]; RE_RC_COUNT(isec->raycounter->simd_bb.hit); }
		
		i += 4;
	}
	while(i < node->nchilds)
	{
		if(RE_rayobject_bb_intersect_test(isec, (const float*)node->child_bb+6*i))
			stack[stack_pos++] = node->child[i];
		i++;
	}
}

template<>
inline void bvh_node_merge_bb<SVBVHNode>(SVBVHNode *node, float *min, float *max)
{
	if(is_leaf(node))
	{
		RE_rayobject_merge_bb( (RayObject*)node, min, max);
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
		node->child_bb   = (float*)BLI_memarena_alloc(arena, sizeof(float)*6*nchilds);
		node->child= (SVBVHNode**)BLI_memarena_alloc(arena, sizeof(SVBVHNode*)*nchilds);

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

		prepare_for_simd(node);
		
		return node;
	}	
};

#endif

#endif //__SSE__
