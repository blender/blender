/*
 * Adapted from code copyright 2009-2010 NVIDIA Corporation
 * Modifications Copyright 2011, Blender Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __BVH_NODE_H__
#define __BVH_NODE_H__

#include "util_boundbox.h"
#include "util_debug.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

enum BVH_STAT
{
	BVH_STAT_NODE_COUNT,
	BVH_STAT_INNER_COUNT,
	BVH_STAT_LEAF_COUNT,
	BVH_STAT_TRIANGLE_COUNT,
	BVH_STAT_CHILDNODE_COUNT,
	BVH_STAT_QNODE_COUNT,
};

class BVHParams;

class BVHNode
{
public:
	BVHNode()
	{
	}

	virtual ~BVHNode() {}
	virtual bool is_leaf() const = 0;
	virtual int num_children() const = 0;
	virtual BVHNode *get_child(int i) const = 0;
	virtual int num_triangles() const { return 0; }
	virtual void print(int depth = 0) const = 0;

	BoundBox m_bounds;
	uint m_visibility;

	// Subtree functions
	int getSubtreeSize(BVH_STAT stat=BVH_STAT_NODE_COUNT) const;
	float computeSubtreeSAHCost(const BVHParams& p, float probability = 1.0f) const;
	void deleteSubtree();

	uint update_visibility();
};

class InnerNode : public BVHNode
{
public:
	InnerNode(const BoundBox& bounds, BVHNode* child0, BVHNode* child1)
	{
		m_bounds = bounds;
		children[0] = child0;
		children[1] = child1;

		if(child0 && child1)
			m_visibility = child0->m_visibility|child1->m_visibility;
		else
			m_visibility = 0; /* happens on build cancel */
	}

	InnerNode(const BoundBox& bounds)
	{
		m_bounds = bounds;
		m_visibility = 0;
		children[0] = NULL;
		children[1] = NULL;
	}

	bool is_leaf() const { return false; }
	int num_children() const { return 2; }
	BVHNode *get_child(int i) const{ assert(i>=0 && i<2); return children[i]; }
	void print(int depth) const;

	BVHNode *children[2];
};

class LeafNode : public BVHNode
{
public:
	LeafNode(const BoundBox& bounds, uint visibility, int lo, int hi) 
	{
		m_bounds = bounds;
		m_visibility = visibility;
		m_lo = lo;
		m_hi = hi;
	}

	LeafNode(const LeafNode& s)
	: BVHNode()
	{
		*this = s;
	}

	bool is_leaf() const { return true; }
	int num_children() const { return 0; }
	BVHNode *get_child(int) const { return NULL; }
	int num_triangles() const { return m_hi - m_lo; }
	void print(int depth) const;

	int m_lo;
	int m_hi;
};

CCL_NAMESPACE_END

#endif /* __BVH_NODE_H__ */

