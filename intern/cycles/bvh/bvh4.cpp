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

#include "bvh/bvh4.h"

#include "render/mesh.h"
#include "render/object.h"

#include "bvh/bvh_node.h"
#include "bvh/bvh_unaligned.h"

CCL_NAMESPACE_BEGIN

/* Can we avoid this somehow or make more generic?
 *
 * Perhaps we can merge nodes in actual tree and make our
 * life easier all over the place.
 */
static bool node_qbvh_is_unaligned(const BVHNode *node)
{
	const BVHNode *node0 = node->get_child(0),
	              *node1 = node->get_child(1);
	bool has_unaligned = false;
	if(node0->is_leaf()) {
		has_unaligned |= node0->is_unaligned;
	}
	else {
		has_unaligned |= node0->get_child(0)->is_unaligned;
		has_unaligned |= node0->get_child(1)->is_unaligned;
	}
	if(node1->is_leaf()) {
		has_unaligned |= node1->is_unaligned;
	}
	else {
		has_unaligned |= node1->get_child(0)->is_unaligned;
		has_unaligned |= node1->get_child(1)->is_unaligned;
	}
	return has_unaligned;
}

BVH4::BVH4(const BVHParams& params_, const vector<Object*>& objects_)
: BVH(params_, objects_)
{
	params.bvh_layout = BVH_LAYOUT_BVH4;
}

void BVH4::pack_leaf(const BVHStackEntry& e, const LeafNode *leaf)
{
	float4 data[BVH_QNODE_LEAF_SIZE];
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

	memcpy(&pack.leaf_nodes[e.idx], data, sizeof(float4)*BVH_QNODE_LEAF_SIZE);
}

void BVH4::pack_inner(const BVHStackEntry& e,
                      const BVHStackEntry *en,
                      int num)
{
	bool has_unaligned = false;
	/* Check whether we have to create unaligned node or all nodes are aligned
	 * and we can cut some corner here.
	 */
	if(params.use_unaligned_nodes) {
		for(int i = 0; i < num; i++) {
			if(en[i].node->is_unaligned) {
				has_unaligned = true;
				break;
			}
		}
	}
	if(has_unaligned) {
		/* There's no unaligned children, pack into AABB node. */
		pack_unaligned_inner(e, en, num);
	}
	else {
		/* Create unaligned node with orientation transform for each of the
		 * children.
		 */
		pack_aligned_inner(e, en, num);
	}
}

void BVH4::pack_aligned_inner(const BVHStackEntry& e,
                              const BVHStackEntry *en,
                              int num)
{
	BoundBox bounds[4];
	int child[4];
	for(int i = 0; i < num; ++i) {
		bounds[i] = en[i].node->bounds;
		child[i] = en[i].encodeIdx();
	}
	pack_aligned_node(e.idx,
	                  bounds,
	                  child,
	                  e.node->visibility,
	                  e.node->time_from,
	                  e.node->time_to,
	                  num);
}

void BVH4::pack_aligned_node(int idx,
                             const BoundBox *bounds,
                             const int *child,
                             const uint visibility,
                             const float time_from,
                             const float time_to,
                             const int num)
{
	float4 data[BVH_QNODE_SIZE];
	memset(data, 0, sizeof(data));

	data[0].x = __uint_as_float(visibility & ~PATH_RAY_NODE_UNALIGNED);
	data[0].y = time_from;
	data[0].z = time_to;

	for(int i = 0; i < num; i++) {
		float3 bb_min = bounds[i].min;
		float3 bb_max = bounds[i].max;

		data[1][i] = bb_min.x;
		data[2][i] = bb_max.x;
		data[3][i] = bb_min.y;
		data[4][i] = bb_max.y;
		data[5][i] = bb_min.z;
		data[6][i] = bb_max.z;

		data[7][i] = __int_as_float(child[i]);
	}

	for(int i = num; i < 4; i++) {
		/* We store BB which would never be recorded as intersection
		 * so kernel might safely assume there are always 4 child nodes.
		 */
		data[1][i] = FLT_MAX;
		data[2][i] = -FLT_MAX;

		data[3][i] = FLT_MAX;
		data[4][i] = -FLT_MAX;

		data[5][i] = FLT_MAX;
		data[6][i] = -FLT_MAX;

		data[7][i] = __int_as_float(0);
	}

	memcpy(&pack.nodes[idx], data, sizeof(float4)*BVH_QNODE_SIZE);
}

void BVH4::pack_unaligned_inner(const BVHStackEntry& e,
                                const BVHStackEntry *en,
                                int num)
{
	Transform aligned_space[4];
	BoundBox bounds[4];
	int child[4];
	for(int i = 0; i < num; ++i) {
		aligned_space[i] = en[i].node->get_aligned_space();
		bounds[i] = en[i].node->bounds;
		child[i] = en[i].encodeIdx();
	}
	pack_unaligned_node(e.idx,
	                    aligned_space,
	                    bounds,
	                    child,
	                    e.node->visibility,
	                    e.node->time_from,
	                    e.node->time_to,
	                    num);
}

void BVH4::pack_unaligned_node(int idx,
                               const Transform *aligned_space,
                               const BoundBox *bounds,
                               const int *child,
                               const uint visibility,
                               const float time_from,
                               const float time_to,
                               const int num)
{
	float4 data[BVH_UNALIGNED_QNODE_SIZE];
	memset(data, 0, sizeof(data));

	data[0].x = __uint_as_float(visibility | PATH_RAY_NODE_UNALIGNED);
	data[0].y = time_from;
	data[0].z = time_to;

	for(int i = 0; i < num; i++) {
		Transform space = BVHUnaligned::compute_node_transform(
		        bounds[i],
		        aligned_space[i]);

		data[1][i] = space.x.x;
		data[2][i] = space.x.y;
		data[3][i] = space.x.z;

		data[4][i] = space.y.x;
		data[5][i] = space.y.y;
		data[6][i] = space.y.z;

		data[7][i] = space.z.x;
		data[8][i] = space.z.y;
		data[9][i] = space.z.z;

		data[10][i] = space.x.w;
		data[11][i] = space.y.w;
		data[12][i] = space.z.w;

		data[13][i] = __int_as_float(child[i]);
	}

	for(int i = num; i < 4; i++) {
		/* We store BB which would never be recorded as intersection
		 * so kernel might safely assume there are always 4 child nodes.
		 */

		data[1][i] = NAN;
		data[2][i] = NAN;
		data[3][i] = NAN;

		data[4][i] = NAN;
		data[5][i] = NAN;
		data[6][i] = NAN;

		data[7][i] = NAN;
		data[8][i] = NAN;
		data[9][i] = NAN;

		data[10][i] = NAN;
		data[11][i] = NAN;
		data[12][i] = NAN;

		data[13][i] = __int_as_float(0);
	}

	memcpy(&pack.nodes[idx], data, sizeof(float4)*BVH_UNALIGNED_QNODE_SIZE);
}

/* Quad SIMD Nodes */

void BVH4::pack_nodes(const BVHNode *root)
{
	/* Calculate size of the arrays required. */
	const size_t num_nodes = root->getSubtreeSize(BVH_STAT_QNODE_COUNT);
	const size_t num_leaf_nodes = root->getSubtreeSize(BVH_STAT_LEAF_COUNT);
	assert(num_leaf_nodes <= num_nodes);
	const size_t num_inner_nodes = num_nodes - num_leaf_nodes;
	size_t node_size;
	if(params.use_unaligned_nodes) {
		const size_t num_unaligned_nodes =
		        root->getSubtreeSize(BVH_STAT_UNALIGNED_INNER_QNODE_COUNT);
		node_size = (num_unaligned_nodes * BVH_UNALIGNED_QNODE_SIZE) +
		            (num_inner_nodes - num_unaligned_nodes) * BVH_QNODE_SIZE;
	}
	else {
		node_size = num_inner_nodes * BVH_QNODE_SIZE;
	}
	/* Resize arrays. */
	pack.nodes.clear();
	pack.leaf_nodes.clear();
	/* For top level BVH, first merge existing BVH's so we know the offsets. */
	if(params.top_level) {
		pack_instances(node_size, num_leaf_nodes*BVH_QNODE_LEAF_SIZE);
	}
	else {
		pack.nodes.resize(node_size);
		pack.leaf_nodes.resize(num_leaf_nodes*BVH_QNODE_LEAF_SIZE);
	}

	int nextNodeIdx = 0, nextLeafNodeIdx = 0;

	vector<BVHStackEntry> stack;
	stack.reserve(BVHParams::MAX_DEPTH*2);
	if(root->is_leaf()) {
		stack.push_back(BVHStackEntry(root, nextLeafNodeIdx++));
	}
	else {
		stack.push_back(BVHStackEntry(root, nextNodeIdx));
		nextNodeIdx += node_qbvh_is_unaligned(root)
		                       ? BVH_UNALIGNED_QNODE_SIZE
		                       : BVH_QNODE_SIZE;
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
			/* Inner node. */
			const BVHNode *node = e.node;
			const BVHNode *node0 = node->get_child(0);
			const BVHNode *node1 = node->get_child(1);
			/* Collect nodes. */
			const BVHNode *nodes[4];
			int numnodes = 0;
			if(node0->is_leaf()) {
				nodes[numnodes++] = node0;
			}
			else {
				nodes[numnodes++] = node0->get_child(0);
				nodes[numnodes++] = node0->get_child(1);
			}
			if(node1->is_leaf()) {
				nodes[numnodes++] = node1;
			}
			else {
				nodes[numnodes++] = node1->get_child(0);
				nodes[numnodes++] = node1->get_child(1);
			}
			/* Push entries on the stack. */
			for(int i = 0; i < numnodes; ++i) {
				int idx;
				if(nodes[i]->is_leaf()) {
					idx = nextLeafNodeIdx++;
				}
				else {
					idx = nextNodeIdx;
					nextNodeIdx += node_qbvh_is_unaligned(nodes[i])
					                       ? BVH_UNALIGNED_QNODE_SIZE
					                       : BVH_QNODE_SIZE;
				}
				stack.push_back(BVHStackEntry(nodes[i], idx));
			}
			/* Set node. */
			pack_inner(e, &stack[stack.size()-numnodes], numnodes);
		}
	}
	assert(node_size == nextNodeIdx);
	/* Root index to start traversal at, to handle case of single leaf node. */
	pack.root_index = (root->is_leaf())? -1: 0;
}

void BVH4::refit_nodes()
{
	assert(!params.top_level);

	BoundBox bbox = BoundBox::empty;
	uint visibility = 0;
	refit_node(0, (pack.root_index == -1)? true: false, bbox, visibility);
}

void BVH4::refit_node(int idx, bool leaf, BoundBox& bbox, uint& visibility)
{
	if(leaf) {
		/* Refit leaf node. */
		int4 *data = &pack.leaf_nodes[idx];
		int4 c = data[0];

		BVH::refit_primitives(c.x, c.y, bbox, visibility);

		/* TODO(sergey): This is actually a copy of pack_leaf(),
		 * but this chunk of code only knows actual data and has
		 * no idea about BVHNode.
		 *
		 * Would be nice to de-duplicate code, but trying to make
		 * making code more general ends up in much nastier code
		 * in my opinion so far.
		 *
		 * Same applies to the inner nodes case below.
		 */
		float4 leaf_data[BVH_QNODE_LEAF_SIZE];
		leaf_data[0].x = __int_as_float(c.x);
		leaf_data[0].y = __int_as_float(c.y);
		leaf_data[0].z = __uint_as_float(visibility);
		leaf_data[0].w = __uint_as_float(c.w);
		memcpy(&pack.leaf_nodes[idx], leaf_data, sizeof(float4)*BVH_QNODE_LEAF_SIZE);
	}
	else {
		int4 *data = &pack.nodes[idx];
		bool is_unaligned = (data[0].x & PATH_RAY_NODE_UNALIGNED) != 0;
		int4 c;
		if(is_unaligned) {
			c = data[13];
		}
		else {
			c = data[7];
		}
		/* Refit inner node, set bbox from children. */
		BoundBox child_bbox[4] = {BoundBox::empty,
		                          BoundBox::empty,
		                          BoundBox::empty,
		                          BoundBox::empty};
		uint child_visibility[4] = {0};
		int num_nodes = 0;

		for(int i = 0; i < 4; ++i) {
			if(c[i] != 0) {
				refit_node((c[i] < 0)? -c[i]-1: c[i], (c[i] < 0),
				           child_bbox[i], child_visibility[i]);
				++num_nodes;
				bbox.grow(child_bbox[i]);
				visibility |= child_visibility[i];
			}
		}

		if(is_unaligned) {
			Transform aligned_space[4] = {transform_identity(),
			                              transform_identity(),
			                              transform_identity(),
			                              transform_identity()};
			pack_unaligned_node(idx,
			                    aligned_space,
			                    child_bbox,
			                    &c[0],
			                    visibility,
			                    0.0f,
			                    1.0f,
			                    4);
		}
		else {
			pack_aligned_node(idx,
			                  child_bbox,
			                  &c[0],
			                  visibility,
			                  0.0f,
			                  1.0f,
			                  4);
		}
	}
}

CCL_NAMESPACE_END
