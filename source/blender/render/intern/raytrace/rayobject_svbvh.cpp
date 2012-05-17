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

/** \file blender/render/intern/raytrace/rayobject_svbvh.cpp
 *  \ingroup render
 */


#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "vbvh.h"
#include "svbvh.h"
#include "reorganize.h"

#ifdef __SSE__

#define DFS_STACK_SIZE  256

struct SVBVHTree {
	RayObject rayobj;

	SVBVHNode *root;
	MemArena *node_arena;

	float cost;
	RTBuilder *builder;
};

/*
 * Cost to test N childs
 */
struct PackCost {
	float operator()(int n)
	{
		return (n / 4) + ((n % 4) > 2 ? 1 : n % 4);
	}
};


template<>
void bvh_done<SVBVHTree>(SVBVHTree *obj)
{
	rtbuild_done(obj->builder, &obj->rayobj.control);
	
	//TODO find a away to exactly calculate the needed memory
	MemArena *arena1 = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "svbvh arena");
	BLI_memarena_use_malloc(arena1);

	MemArena *arena2 = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "svbvh arena2");
	BLI_memarena_use_malloc(arena2);
	BLI_memarena_use_align(arena2, 16);

	//Build and optimize the tree
	if (0) {
		VBVHNode *root = BuildBinaryVBVH<VBVHNode>(arena1, &obj->rayobj.control).transform(obj->builder);

		if (RE_rayobjectcontrol_test_break(&obj->rayobj.control)) {
			BLI_memarena_free(arena1);
			BLI_memarena_free(arena2);
			return;
		}
		
		reorganize(root);
		remove_useless(root, &root);
		bvh_refit(root);

		pushup(root);
		pushdown(root);
		pushup_simd<VBVHNode, 4>(root);

		obj->root = Reorganize_SVBVH<VBVHNode>(arena2).transform(root);
	}
	else {
		//Finds the optimal packing of this tree using a given cost model
		//TODO this uses quite a lot of memory, find ways to reduce memory usage during building
		OVBVHNode *root = BuildBinaryVBVH<OVBVHNode>(arena1, &obj->rayobj.control).transform(obj->builder);

		if (RE_rayobjectcontrol_test_break(&obj->rayobj.control)) {
			BLI_memarena_free(arena1);
			BLI_memarena_free(arena2);
			return;
		}

		if (root) {
			VBVH_optimalPackSIMD<OVBVHNode, PackCost>(PackCost()).transform(root);
			obj->root = Reorganize_SVBVH<OVBVHNode>(arena2).transform(root);
		}
		else
			obj->root = NULL;
	}
	
	//Free data
	BLI_memarena_free(arena1);
	
	obj->node_arena = arena2;
	obj->cost = 1.0;

	rtbuild_free(obj->builder);
	obj->builder = NULL;
}

template<int StackSize>
int intersect(SVBVHTree *obj, Isect *isec)
{
	//TODO renable hint support
	if (RE_rayobject_isAligned(obj->root)) {
		if (isec->mode == RE_RAY_SHADOW)
			return svbvh_node_stack_raycast<StackSize, true>(obj->root, isec);
		else
			return svbvh_node_stack_raycast<StackSize, false>(obj->root, isec);
	}
	else
		return RE_rayobject_intersect( (RayObject *) obj->root, isec);
}

template<class Tree>
void bvh_hint_bb(Tree *tree, LCTSHint *hint, float *UNUSED(min), float *UNUSED(max))
{
	//TODO renable hint support
	{
		hint->size = 0;
		hint->stack[hint->size++] = (RayObject *)tree->root;
	}
}
/* the cast to pointer function is needed to workarround gcc bug: http://gcc.gnu.org/bugzilla/show_bug.cgi?id=11407 */
template<class Tree, int STACK_SIZE>
RayObjectAPI make_api()
{
	static RayObjectAPI api = 
	{
		(RE_rayobject_raycast_callback) ((int   (*)(Tree *, Isect *)) & intersect<STACK_SIZE>),
		(RE_rayobject_add_callback)     ((void  (*)(Tree *, RayObject *)) & bvh_add<Tree>),
		(RE_rayobject_done_callback)    ((void  (*)(Tree *))       & bvh_done<Tree>),
		(RE_rayobject_free_callback)    ((void  (*)(Tree *))       & bvh_free<Tree>),
		(RE_rayobject_merge_bb_callback)((void  (*)(Tree *, float *, float *)) & bvh_bb<Tree>),
		(RE_rayobject_cost_callback)    ((float (*)(Tree *))      & bvh_cost<Tree>),
		(RE_rayobject_hint_bb_callback) ((void  (*)(Tree *, LCTSHint *, float *, float *)) & bvh_hint_bb<Tree>)
	};
	
	return api;
}

template<class Tree>
RayObjectAPI *bvh_get_api(int maxstacksize)
{
	static RayObjectAPI bvh_api256 = make_api<Tree, 1024>();
	
	if (maxstacksize <= 1024) return &bvh_api256;
	assert(maxstacksize <= 256);
	return NULL;
}

RayObject *RE_rayobject_svbvh_create(int size)
{
	return bvh_create_tree<SVBVHTree, DFS_STACK_SIZE>(size);
}

#else

RayObject *RE_rayobject_svbvh_create(int size)
{
	puts("WARNING: SSE disabled at compile time\n");
	return NULL;
}

#endif
