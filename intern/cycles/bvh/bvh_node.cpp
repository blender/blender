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

#include "bvh.h"
#include "bvh_build.h"
#include "bvh_node.h"

#include "util_debug.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

/* BVH Node */

int BVHNode::getSubtreeSize(BVH_STAT stat) const
{
	int cnt = 0;

	switch(stat)
	{
		case BVH_STAT_NODE_COUNT:
			cnt = 1;
			break;
		case BVH_STAT_LEAF_COUNT:
			cnt = is_leaf() ? 1 : 0;
			break;
		case BVH_STAT_INNER_COUNT:
			cnt = is_leaf() ? 0 : 1;
			break;
		case BVH_STAT_TRIANGLE_COUNT:
			cnt = is_leaf() ? reinterpret_cast<const LeafNode*>(this)->num_triangles() : 0;
			break;
		case BVH_STAT_CHILDNODE_COUNT:
			cnt = num_children();
			break;
		default:
			assert(0); /* unknown mode */
	}

	if(!is_leaf())
		for(int i=0;i<num_children();i++)
			cnt += get_child(i)->getSubtreeSize(stat);

	return cnt;
}

void BVHNode::deleteSubtree()
{
	for(int i=0;i<num_children();i++)
		if(get_child(i))
			get_child(i)->deleteSubtree();

	delete this;
}

float BVHNode::computeSubtreeSAHCost(const BVHParams& p, float probability) const
{
	float SAH = probability * p.cost(num_children(), num_triangles());

	for(int i=0;i<num_children();i++) {
		BVHNode *child = get_child(i);
		SAH += child->computeSubtreeSAHCost(p, probability * child->m_bounds.safe_area()/m_bounds.safe_area());
	}

	return SAH;
}

uint BVHNode::update_visibility()
{
	if(!is_leaf() && m_visibility == 0) {
		InnerNode *inner = (InnerNode*)this;
		BVHNode *child0 = inner->children[0];
		BVHNode *child1 = inner->children[1];

		m_visibility = child0->update_visibility()|child1->update_visibility();
	}

	return m_visibility;
}

/* Inner Node */

void InnerNode::print(int depth) const
{
	for(int i = 0; i < depth; i++)
		printf("  ");
	
	printf("inner node %p\n", (void*)this);

	if(children[0])
		children[0]->print(depth+1);
	if(children[1])
		children[1]->print(depth+1);
}

void LeafNode::print(int depth) const
{
	for(int i = 0; i < depth; i++)
		printf("  ");
	
	printf("leaf node %d to %d\n", m_lo, m_hi);
}

CCL_NAMESPACE_END

