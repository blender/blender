#include <assert.h>

#include <algorithm>
#include "rayobject_rtbuild.h"
#include "BLI_memarena.h"


/*
 * VBVHNode represents a BVHNode with support for a variable number of childrens
 */
struct VBVHNode
{
	float	bb[6];

	VBVHNode *child;
	VBVHNode *sibling;
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


/*
 * Builds a binary VBVH from a rtbuild
 */
struct BuildBinaryVBVH
{
	MemArena *arena;

	BuildBinaryVBVH(MemArena *a)
	{
		arena = a;
	}

	VBVHNode *create_node()
	{
		VBVHNode *node = (VBVHNode*)BLI_memarena_alloc( arena, sizeof(VBVHNode) );
		assert( RE_rayobject_isAligned(node) );

		node->sibling = NULL;
		node->child   = NULL;

		return node;
	}
	
	int rtbuild_split(RTBuilder *builder)
	{
		return ::rtbuild_heuristic_object_split(builder, 2);
	}
	
	VBVHNode *transform(RTBuilder *builder)
	{
		
		int size = rtbuild_size(builder);
		if(size == 1)
		{
			VBVHNode *node = create_node();
			INIT_MINMAX(node->bb, node->bb+3);
			rtbuild_merge_bb(builder, node->bb, node->bb+3);		
			node->child = (VBVHNode*) rtbuild_get_primitive( builder, 0 );
			return node;
		}
		else
		{
			VBVHNode *node = create_node();

			INIT_MINMAX(node->bb, node->bb+3);
			rtbuild_merge_bb(builder, node->bb, node->bb+3);
			
			VBVHNode **child = &node->child;

			int nc = rtbuild_split(builder);
			assert(nc == 2);
			for(int i=0; i<nc; i++)
			{
				RTBuilder tmp;
				rtbuild_get_child(builder, i, &tmp);
				
				*child = transform(&tmp);
				child = &((*child)->sibling);
			}

			*child = 0;
			return node;
		}
	}
};

/*
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
*/