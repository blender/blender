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

#include "bvh/bvh2.h"

#include "render/mesh.h"
#include "render/object.h"

#include "bvh/bvh_node.h"
#include "bvh/bvh_unaligned.h"

CCL_NAMESPACE_BEGIN

BVH2::BVH2(const BVHParams& params_, const vector<Object*>& objects_)
: BVH(params_, objects_)
{
}

void BVH2::pack_leaf(const BVHStackEntry& e,
                     const LeafNode *leaf)
{
	assert(e.idx + BVH_NODE_LEAF_SIZE <= pack.leaf_nodes.size());
	float4 data[BVH_NODE_LEAF_SIZE];
	memset(data, 0, sizeof(data));
	if(leaf->num_triangles() == 1 && pack.prim_index[leaf->lo] == -1) {
		/* object */
		data[0].x = __int_as_float(~(leaf->lo));
		data[0].y = __int_as_float(0);
	}
	else {
		/* triangle */
		data[0].x = __int_as_float(leaf->lo);
		data[0].y = __int_as_float(leaf->hi);
	}
	data[0].z = __uint_as_float(leaf->visibility);
	if(leaf->num_triangles() != 0) {
		data[0].w = __uint_as_float(pack.prim_type[leaf->lo]);
	}

	memcpy(&pack.leaf_nodes[e.idx], data, sizeof(float4)*BVH_NODE_LEAF_SIZE);
}

void BVH2::pack_inner(const BVHStackEntry& e,
                      const BVHStackEntry& e0,
                      const BVHStackEntry& e1)
{
	if(e0.node->is_unaligned || e1.node->is_unaligned) {
		pack_unaligned_inner(e, e0, e1);
	} else {
		pack_aligned_inner(e, e0, e1);
	}
}

void BVH2::pack_aligned_inner(const BVHStackEntry& e,
                              const BVHStackEntry& e0,
                              const BVHStackEntry& e1)
{
	pack_aligned_node(e.idx,
	                  e0.node->bounds, e1.node->bounds,
	                  e0.encodeIdx(), e1.encodeIdx(),
	                  e0.node->visibility, e1.node->visibility);
}

void BVH2::pack_aligned_node(int idx,
                             const BoundBox& b0,
                             const BoundBox& b1,
                             int c0, int c1,
                             uint visibility0, uint visibility1)
{
	assert(idx + BVH_NODE_SIZE <= pack.nodes.size());
	assert(c0 < 0 || c0 < pack.nodes.size());
	assert(c1 < 0 || c1 < pack.nodes.size());

	int4 data[BVH_NODE_SIZE] = {
		make_int4(visibility0 & ~PATH_RAY_NODE_UNALIGNED,
		          visibility1 & ~PATH_RAY_NODE_UNALIGNED,
		          c0, c1),
		make_int4(__float_as_int(b0.min.x),
		          __float_as_int(b1.min.x),
		          __float_as_int(b0.max.x),
		          __float_as_int(b1.max.x)),
		make_int4(__float_as_int(b0.min.y),
		          __float_as_int(b1.min.y),
		          __float_as_int(b0.max.y),
		          __float_as_int(b1.max.y)),
		make_int4(__float_as_int(b0.min.z),
		          __float_as_int(b1.min.z),
		          __float_as_int(b0.max.z),
		          __float_as_int(b1.max.z)),
	};

	memcpy(&pack.nodes[idx], data, sizeof(int4)*BVH_NODE_SIZE);
}

void BVH2::pack_unaligned_inner(const BVHStackEntry& e,
                                const BVHStackEntry& e0,
                                const BVHStackEntry& e1)
{
	pack_unaligned_node(e.idx,
	                    e0.node->get_aligned_space(),
	                    e1.node->get_aligned_space(),
	                    e0.node->bounds,
	                    e1.node->bounds,
	                    e0.encodeIdx(), e1.encodeIdx(),
	                    e0.node->visibility, e1.node->visibility);
}

void BVH2::pack_unaligned_node(int idx,
                               const Transform& aligned_space0,
                               const Transform& aligned_space1,
                               const BoundBox& bounds0,
                               const BoundBox& bounds1,
                               int c0, int c1,
                               uint visibility0, uint visibility1)
{
	assert(idx + BVH_UNALIGNED_NODE_SIZE <= pack.nodes.size());
	assert(c0 < 0 || c0 < pack.nodes.size());
	assert(c1 < 0 || c1 < pack.nodes.size());

	float4 data[BVH_UNALIGNED_NODE_SIZE];
	Transform space0 = BVHUnaligned::compute_node_transform(bounds0,
	                                                        aligned_space0);
	Transform space1 = BVHUnaligned::compute_node_transform(bounds1,
	                                                        aligned_space1);
	data[0] = make_float4(__int_as_float(visibility0 | PATH_RAY_NODE_UNALIGNED),
	                      __int_as_float(visibility1 | PATH_RAY_NODE_UNALIGNED),
	                      __int_as_float(c0),
	                      __int_as_float(c1));

	data[1] = space0.x;
	data[2] = space0.y;
	data[3] = space0.z;
	data[4] = space1.x;
	data[5] = space1.y;
	data[6] = space1.z;

	memcpy(&pack.nodes[idx], data, sizeof(float4)*BVH_UNALIGNED_NODE_SIZE);
}

void BVH2::pack_nodes(const BVHNode *root)
{
	const size_t num_nodes = root->getSubtreeSize(BVH_STAT_NODE_COUNT);
	const size_t num_leaf_nodes = root->getSubtreeSize(BVH_STAT_LEAF_COUNT);
	assert(num_leaf_nodes <= num_nodes);
	const size_t num_inner_nodes = num_nodes - num_leaf_nodes;
	size_t node_size;
	if(params.use_unaligned_nodes) {
		const size_t num_unaligned_nodes =
		        root->getSubtreeSize(BVH_STAT_UNALIGNED_INNER_COUNT);
		node_size = (num_unaligned_nodes * BVH_UNALIGNED_NODE_SIZE) +
		            (num_inner_nodes - num_unaligned_nodes) * BVH_NODE_SIZE;
	}
	else {
		node_size = num_inner_nodes * BVH_NODE_SIZE;
	}
	/* Resize arrays */
	pack.nodes.clear();
	pack.leaf_nodes.clear();
	/* For top level BVH, first merge existing BVH's so we know the offsets. */
	if(params.top_level) {
		pack_instances(node_size, num_leaf_nodes*BVH_NODE_LEAF_SIZE);
	}
	else {
		pack.nodes.resize(node_size);
		pack.leaf_nodes.resize(num_leaf_nodes*BVH_NODE_LEAF_SIZE);
	}

	int nextNodeIdx = 0, nextLeafNodeIdx = 0;

	vector<BVHStackEntry> stack;
	stack.reserve(BVHParams::MAX_DEPTH*2);
	if(root->is_leaf()) {
		stack.push_back(BVHStackEntry(root, nextLeafNodeIdx++));
	}
	else {
		stack.push_back(BVHStackEntry(root, nextNodeIdx));
		nextNodeIdx += node_is_unaligned(root, bvh2)
		                       ? BVH_UNALIGNED_NODE_SIZE
		                       : BVH_NODE_SIZE;
	}

	while(stack.size()) {
		BVHStackEntry e = stack.back();
		stack.pop_back();

		if(e.node->is_leaf()) {
			/* leaf node */
			const LeafNode *leaf = reinterpret_cast<const LeafNode*>(e.node);
			pack_leaf(e, leaf);
		}
		else {
			/* innner node */
			int idx[2];
			for(int i = 0; i < 2; ++i) {
				if(e.node->get_child(i)->is_leaf()) {
					idx[i] = nextLeafNodeIdx++;
				}
				else {
					idx[i] = nextNodeIdx;
					nextNodeIdx += node_is_unaligned(e.node->get_child(i), bvh2)
					                       ? BVH_UNALIGNED_NODE_SIZE
					                       : BVH_NODE_SIZE;
				}
			}

			stack.push_back(BVHStackEntry(e.node->get_child(0), idx[0]));
			stack.push_back(BVHStackEntry(e.node->get_child(1), idx[1]));

			pack_inner(e, stack[stack.size()-2], stack[stack.size()-1]);
		}
	}
	assert(node_size == nextNodeIdx);
	/* root index to start traversal at, to handle case of single leaf node */
	pack.root_index = (root->is_leaf())? -1: 0;
}

void BVH2::refit_nodes()
{
	assert(!params.top_level);

	BoundBox bbox = BoundBox::empty;
	uint visibility = 0;
	refit_node(0, (pack.root_index == -1)? true: false, bbox, visibility);
}

void BVH2::refit_node(int idx, bool leaf, BoundBox& bbox, uint& visibility)
{
	if(leaf) {
		/* refit leaf node */
		assert(idx + BVH_NODE_LEAF_SIZE <= pack.leaf_nodes.size());
		const int4 *data = &pack.leaf_nodes[idx];
		const int c0 = data[0].x;
		const int c1 = data[0].y;

		BVH::refit_primitives(c0, c1, bbox, visibility);

		/* TODO(sergey): De-duplicate with pack_leaf(). */
		float4 leaf_data[BVH_NODE_LEAF_SIZE];
		leaf_data[0].x = __int_as_float(c0);
		leaf_data[0].y = __int_as_float(c1);
		leaf_data[0].z = __uint_as_float(visibility);
		leaf_data[0].w = __uint_as_float(data[0].w);
		memcpy(&pack.leaf_nodes[idx], leaf_data, sizeof(float4)*BVH_NODE_LEAF_SIZE);
	}
	else {
		assert(idx + BVH_NODE_SIZE <= pack.nodes.size());

		const int4 *data = &pack.nodes[idx];
		const bool is_unaligned = (data[0].x & PATH_RAY_NODE_UNALIGNED) != 0;
		const int c0 = data[0].z;
		const int c1 = data[0].w;
		/* refit inner node, set bbox from children */
		BoundBox bbox0 = BoundBox::empty, bbox1 = BoundBox::empty;
		uint visibility0 = 0, visibility1 = 0;

		refit_node((c0 < 0)? -c0-1: c0, (c0 < 0), bbox0, visibility0);
		refit_node((c1 < 0)? -c1-1: c1, (c1 < 0), bbox1, visibility1);

		if(is_unaligned) {
			Transform aligned_space = transform_identity();
			pack_unaligned_node(idx,
			                    aligned_space, aligned_space,
			                    bbox0, bbox1,
			                    c0, c1,
			                    visibility0,
			                    visibility1);
		}
		else {
			pack_aligned_node(idx,
			                  bbox0, bbox1,
			                  c0, c1,
			                  visibility0,
			                  visibility1);
		}

		bbox.grow(bbox0);
		bbox.grow(bbox1);
		visibility = visibility0|visibility1;
	}
}

CCL_NAMESPACE_END
