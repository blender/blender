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

/** \file blender/render/intern/raytrace/vbvh.h
 *  \ingroup render
 */


#include <assert.h>
#include <algorithm>

#include "BLI_memarena.h"

#include "rayobject_rtbuild.h"

/*
 * VBVHNode represents a BVHNode with support for a variable number of childrens
 */
struct VBVHNode {
	float bb[6];

	VBVHNode *child;
	VBVHNode *sibling;
};


/*
 * Push nodes (used on dfs)
 */
template<class Node>
inline static void bvh_node_push_childs(Node *node, Isect *UNUSED(isec), Node **stack, int &stack_pos)
{
	Node *child = node->child;

	if (is_leaf(child)) {
		stack[stack_pos++] = child;
	}
	else {
		while (child) {
			/* Skips BB tests on primitives */
#if 0
			if (is_leaf(child->child)) {
				stack[stack_pos++] = child->child;
			}
			else
#endif
			{
				stack[stack_pos++] = child;
			}

			child = child->sibling;
		}
	}
}


template<class Node>
static int count_childs(Node *parent)
{
	int n = 0;
	for (Node *i = parent->child; i; i = i->sibling) {
		n++;
		if (is_leaf(i))
			break;
	}
		
	return n;
}


template<class Node>
static void append_sibling(Node *node, Node *sibling)
{
	while (node->sibling)
		node = node->sibling;
		
	node->sibling = sibling;
}


/*
 * Builds a binary VBVH from a rtbuild
 */
template<class Node>
struct BuildBinaryVBVH {
	MemArena *arena;
	RayObjectControl *control;

	void test_break()
	{
		if (RE_rayobjectcontrol_test_break(control))
			throw "Stop";
	}

	BuildBinaryVBVH(MemArena *a, RayObjectControl *c)
	{
		arena = a;
		control = c;
	}

	Node *create_node()
	{
		Node *node = (Node *)BLI_memarena_alloc(arena, sizeof(Node) );
		assert(RE_rayobject_isAligned(node));

		node->sibling = NULL;
		node->child   = NULL;

		return node;
	}
	
	int rtbuild_split(RTBuilder *builder)
	{
		return ::rtbuild_heuristic_object_split(builder, 2);
	}
	
	Node *transform(RTBuilder *builder)
	{
		try
		{
			return _transform(builder);
			
		} catch (...)
		{
		}
		return NULL;
	}
	
	Node *_transform(RTBuilder *builder)
	{
		int size = rtbuild_size(builder);

		if (size == 0) {
			return NULL;
		}
		else if (size == 1) {
			Node *node = create_node();
			INIT_MINMAX(node->bb, node->bb + 3);
			rtbuild_merge_bb(builder, node->bb, node->bb + 3);
			node->child = (Node *) rtbuild_get_primitive(builder, 0);
			return node;
		}
		else {
			test_break();
			
			Node *node = create_node();

			Node **child = &node->child;

			int nc = rtbuild_split(builder);
			INIT_MINMAX(node->bb, node->bb + 3);

			assert(nc == 2);
			for (int i = 0; i < nc; i++) {
				RTBuilder tmp;
				rtbuild_get_child(builder, i, &tmp);
				
				*child = _transform(&tmp);
				DO_MIN((*child)->bb, node->bb);
				DO_MAX((*child)->bb + 3, node->bb + 3);
				child = &((*child)->sibling);
			}

			*child = NULL;
			return node;
		}
	}
};

#if 0
template<class Tree, class OldNode>
struct Reorganize_VBVH {
	Tree *tree;
	
	Reorganize_VBVH(Tree *t)
	{
		tree = t;
	}
	
	VBVHNode *create_node()
	{
		VBVHNode *node = (VBVHNode *)BLI_memarena_alloc(tree->node_arena, sizeof(VBVHNode));
		return node;
	}
	
	void copy_bb(VBVHNode *node, OldNode *old)
	{
		std::copy(old->bb, old->bb + 6, node->bb);
	}
	
	VBVHNode *transform(OldNode *old)
	{
		if (is_leaf(old))
			return (VBVHNode *)old;

		VBVHNode *node = create_node();
		VBVHNode **child_ptr = &node->child;
		node->sibling = 0;

		copy_bb(node, old);

		for (OldNode *o_child = old->child; o_child; o_child = o_child->sibling)
		{
			VBVHNode *n_child = transform(o_child);
			*child_ptr = n_child;
			if (is_leaf(n_child)) return node;
			child_ptr = &n_child->sibling;
		}
		*child_ptr = 0;
		
		return node;
	}
};
#endif
