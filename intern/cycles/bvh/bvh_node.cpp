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

#include "bvh/bvh_node.h"

#include "bvh/bvh.h"
#include "bvh/bvh_build.h"

#include "util/util_vector.h"

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
		case BVH_STAT_QNODE_COUNT:
			cnt = 1;
			for(int i = 0; i < num_children(); i++) {
				BVHNode *node = get_child(i);
				if(node->is_leaf()) {
					cnt += 1;
				}
				else {
					for(int j = 0; j < node->num_children(); j++) {
						cnt += node->get_child(j)->getSubtreeSize(stat);
					}
				}
			}
			return cnt;
		case BVH_STAT_ALIGNED_COUNT:
			if(!is_unaligned) {
				cnt = 1;
			}
			break;
		case BVH_STAT_UNALIGNED_COUNT:
			if(is_unaligned) {
				cnt = 1;
			}
			break;
		case BVH_STAT_ALIGNED_INNER_COUNT:
			if(!is_leaf()) {
				bool has_unaligned = false;
				for(int j = 0; j < num_children(); j++) {
					has_unaligned |= get_child(j)->is_unaligned;
				}
				cnt += has_unaligned? 0: 1;
			}
			break;
		case BVH_STAT_UNALIGNED_INNER_COUNT:
			if(!is_leaf()) {
				bool has_unaligned = false;
				for(int j = 0; j < num_children(); j++) {
					has_unaligned |= get_child(j)->is_unaligned;
				}
				cnt += has_unaligned? 1: 0;
			}
			break;
		case BVH_STAT_ALIGNED_INNER_QNODE_COUNT:
			{
				bool has_unaligned = false;
				for(int i = 0; i < num_children(); i++) {
					BVHNode *node = get_child(i);
					if(node->is_leaf()) {
						has_unaligned |= node->is_unaligned;
					}
					else {
						for(int j = 0; j < node->num_children(); j++) {
							cnt += node->get_child(j)->getSubtreeSize(stat);
							has_unaligned |= node->get_child(j)->is_unaligned;
						}
					}
				}
				cnt += has_unaligned? 0: 1;
			}
			return cnt;
		case BVH_STAT_UNALIGNED_INNER_QNODE_COUNT:
			{
				bool has_unaligned = false;
				for(int i = 0; i < num_children(); i++) {
					BVHNode *node = get_child(i);
					if(node->is_leaf()) {
						has_unaligned |= node->is_unaligned;
					}
					else {
						for(int j = 0; j < node->num_children(); j++) {
							cnt += node->get_child(j)->getSubtreeSize(stat);
							has_unaligned |= node->get_child(j)->is_unaligned;
						}
					}
				}
				cnt += has_unaligned? 1: 0;
			}
			return cnt;
		case BVH_STAT_ALIGNED_LEAF_COUNT:
			cnt = (is_leaf() && !is_unaligned) ? 1 : 0;
			break;
		case BVH_STAT_UNALIGNED_LEAF_COUNT:
			cnt = (is_leaf() && is_unaligned) ? 1 : 0;
			break;
		case BVH_STAT_DEPTH:
			if(is_leaf()) {
				cnt = 1;
			}
			else {
				for(int i = 0; i < num_children(); i++) {
					cnt = max(cnt, get_child(i)->getSubtreeSize(stat));
				}
				cnt += 1;
			}
			return cnt;
		default:
			assert(0); /* unknown mode */
	}

	if(!is_leaf())
		for(int i = 0; i < num_children(); i++)
			cnt += get_child(i)->getSubtreeSize(stat);

	return cnt;
}

void BVHNode::deleteSubtree()
{
	for(int i = 0; i < num_children(); i++)
		if(get_child(i))
			get_child(i)->deleteSubtree();

	delete this;
}

float BVHNode::computeSubtreeSAHCost(const BVHParams& p, float probability) const
{
	float SAH = probability * p.cost(num_children(), num_triangles());

	for(int i = 0; i < num_children(); i++) {
		BVHNode *child = get_child(i);
		SAH += child->computeSubtreeSAHCost(p, probability * child->bounds.safe_area()/bounds.safe_area());
	}

	return SAH;
}

uint BVHNode::update_visibility()
{
	if(!is_leaf() && visibility == 0) {
		InnerNode *inner = (InnerNode*)this;
		BVHNode *child0 = inner->children[0];
		BVHNode *child1 = inner->children[1];

		visibility = child0->update_visibility()|child1->update_visibility();
	}

	return visibility;
}

void BVHNode::update_time()
{
	if(!is_leaf()) {
		InnerNode *inner = (InnerNode*)this;
		BVHNode *child0 = inner->children[0];
		BVHNode *child1 = inner->children[1];
		child0->update_time();
		child1->update_time();
		time_from = min(child0->time_from, child1->time_from);
		time_to =  max(child0->time_to, child1->time_to);
	}
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

	printf("leaf node %d to %d\n", lo, hi);
}

CCL_NAMESPACE_END
