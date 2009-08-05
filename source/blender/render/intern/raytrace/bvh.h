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
/* 
template<int SIZE>
struct BBGroup
{
	float bb[6][SIZE];
};


static inline int test_bb_group(BBGroup<4> *bb_group, Isect *isec)
{
	const float *bb = _bb;
	__m128 tmin={0}, tmax = {isec->labda};

	tmin = _mm_max_ps(tmin, _mm_mul_ps( _mm_sub_ps( group->bb[isec->bv_index[0]], isec->sse_start[0] ), isec->sse_idot_axis[0]) );
	tmax = _mm_min_ps(tmax, _mm_mul_ps( _mm_sub_ps( group->bb[isec->bv_index[1]], isec->sse_start[0] ), isec->sse_idot_axis[0]) );	
	tmin = _mm_max_ps(tmin, _mm_mul_ps( _mm_sub_ps( group->bb[isec->bv_index[2]], isec->sse_start[1] ), isec->sse_idot_axis[1]) );
	tmax = _mm_min_ps(tmax, _mm_mul_ps( _mm_sub_ps( group->bb[isec->bv_index[3]], isec->sse_start[1] ), isec->sse_idot_axis[1]) );
	tmin = _mm_max_ps(tmin, _mm_mul_ps( _mm_sub_ps( group->bb[isec->bv_index[4]], isec->sse_start[2] ), isec->sse_idot_axis[2]) );
	tmax = _mm_min_ps(tmax, _mm_mul_ps( _mm_sub_ps( group->bb[isec->bv_index[5]], isec->sse_start[2] ), isec->sse_idot_axis[2]) );
	
	return _mm_movemask_ps(_mm_cmpge_ps(tmax, tmin));
}

static inline int test_bb_group(BBGroup<1> *bb_group, Isect *isec)
{
	float t1x = (bb[isec->bv_index[0]] - isec->start[0]) * isec->idot_axis[0];
	float t2x = (bb[isec->bv_index[1]] - isec->start[0]) * isec->idot_axis[0];
	float t1y = (bb[isec->bv_index[2]] - isec->start[1]) * isec->idot_axis[1];
	float t2y = (bb[isec->bv_index[3]] - isec->start[1]) * isec->idot_axis[1];
	float t1z = (bb[isec->bv_index[4]] - isec->start[2]) * isec->idot_axis[2];
	float t2z = (bb[isec->bv_index[5]] - isec->start[2]) * isec->idot_axis[2];

	RE_RC_COUNT(isec->raycounter->bb.test);
	if(t1x > t2y || t2x < t1y || t1x > t2z || t2x < t1z || t1y > t2z || t2y < t1z) return 0;
	if(t2x < 0.0 || t2y < 0.0 || t2z < 0.0) return 0;
	if(t1x > isec->labda || t1y > isec->labda || t1z > isec->labda) return 0;

	RE_RC_COUNT(isec->raycounter->bb.hit);
	return 1;
}
*/

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
