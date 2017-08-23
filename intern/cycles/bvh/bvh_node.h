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

#include "util/util_boundbox.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

enum BVH_STAT {
	BVH_STAT_NODE_COUNT,
	BVH_STAT_INNER_COUNT,
	BVH_STAT_LEAF_COUNT,
	BVH_STAT_TRIANGLE_COUNT,
	BVH_STAT_CHILDNODE_COUNT,
	BVH_STAT_QNODE_COUNT,
	BVH_STAT_ALIGNED_COUNT,
	BVH_STAT_UNALIGNED_COUNT,
	BVH_STAT_ALIGNED_INNER_COUNT,
	BVH_STAT_UNALIGNED_INNER_COUNT,
	BVH_STAT_ALIGNED_INNER_QNODE_COUNT,
	BVH_STAT_UNALIGNED_INNER_QNODE_COUNT,
	BVH_STAT_ALIGNED_LEAF_COUNT,
	BVH_STAT_UNALIGNED_LEAF_COUNT,
	BVH_STAT_DEPTH,
};

class BVHParams;

class BVHNode
{
public:
	BVHNode() : is_unaligned(false),
	            aligned_space(NULL),
	            time_from(0.0f),
	            time_to(1.0f)
	{
	}

	virtual ~BVHNode()
	{
		delete aligned_space;
	}

	virtual bool is_leaf() const = 0;
	virtual int num_children() const = 0;
	virtual BVHNode *get_child(int i) const = 0;
	virtual int num_triangles() const { return 0; }
	virtual void print(int depth = 0) const = 0;

	inline void set_aligned_space(const Transform& aligned_space)
	{
		is_unaligned = true;
		if(this->aligned_space == NULL) {
			this->aligned_space = new Transform(aligned_space);
		}
		else {
			*this->aligned_space = aligned_space;
		}
	}

	inline Transform get_aligned_space() const
	{
		if(aligned_space == NULL) {
			return transform_identity();
		}
		return *aligned_space;
	}

	// Subtree functions
	int getSubtreeSize(BVH_STAT stat=BVH_STAT_NODE_COUNT) const;
	float computeSubtreeSAHCost(const BVHParams& p, float probability = 1.0f) const;
	void deleteSubtree();

	uint update_visibility();
	void update_time();

	// Properties.
	BoundBox bounds;
	uint visibility;

	bool is_unaligned;

	/* TODO(sergey): Can be stored as 3x3 matrix, but better to have some
	 * utilities and type defines in util_transform first.
	 */
	Transform *aligned_space;

	float time_from, time_to;
};

class InnerNode : public BVHNode
{
public:
	InnerNode(const BoundBox& bounds,
	          BVHNode* child0,
	          BVHNode* child1)
	{
		this->bounds = bounds;
		children[0] = child0;
		children[1] = child1;

		if(child0 && child1)
			visibility = child0->visibility|child1->visibility;
		else
			visibility = 0; /* happens on build cancel */
	}

	explicit InnerNode(const BoundBox& bounds)
	{
		this->bounds = bounds;
		visibility = 0;
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
	: lo(lo),
	  hi(hi)
	{
		this->bounds = bounds;
		this->visibility = visibility;
	}

	LeafNode(const LeafNode& s)
	: BVHNode()
	{
		*this = s;
	}

	bool is_leaf() const { return true; }
	int num_children() const { return 0; }
	BVHNode *get_child(int) const { return NULL; }
	int num_triangles() const { return hi - lo; }
	void print(int depth) const;

	int lo;
	int hi;
};

CCL_NAMESPACE_END

#endif /* __BVH_NODE_H__ */
