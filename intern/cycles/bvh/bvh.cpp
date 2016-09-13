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

#include "mesh.h"
#include "object.h"
#include "scene.h"
#include "curves.h"

#include "bvh.h"
#include "bvh_build.h"
#include "bvh_node.h"
#include "bvh_params.h"
#include "bvh_unaligned.h"

#include "util_debug.h"
#include "util_foreach.h"
#include "util_logging.h"
#include "util_map.h"
#include "util_progress.h"
#include "util_system.h"
#include "util_types.h"
#include "util_math.h"

CCL_NAMESPACE_BEGIN

/* Pack Utility */

struct BVHStackEntry
{
	const BVHNode *node;
	int idx;

	BVHStackEntry(const BVHNode* n = 0, int i = 0)
	: node(n), idx(i)
	{
	}

	int encodeIdx() const
	{
		return (node->is_leaf())? ~idx: idx;
	}
};

/* BVH */

BVH::BVH(const BVHParams& params_, const vector<Object*>& objects_)
: params(params_), objects(objects_)
{
}

BVH *BVH::create(const BVHParams& params, const vector<Object*>& objects)
{
	if(params.use_qbvh)
		return new QBVH(params, objects);
	else
		return new RegularBVH(params, objects);
}

/* Building */

void BVH::build(Progress& progress)
{
	progress.set_substatus("Building BVH");

	/* build nodes */
	BVHBuild bvh_build(objects,
	                   pack.prim_type,
	                   pack.prim_index,
	                   pack.prim_object,
	                   params,
	                   progress);
	BVHNode *root = bvh_build.run();

	if(progress.get_cancel()) {
		if(root) root->deleteSubtree();
		return;
	}

	/* pack triangles */
	progress.set_substatus("Packing BVH triangles and strands");
	pack_primitives();

	if(progress.get_cancel()) {
		root->deleteSubtree();
		return;
	}

	/* pack nodes */
	progress.set_substatus("Packing BVH nodes");
	pack_nodes(root);

	/* free build nodes */
	root->deleteSubtree();
}

/* Refitting */

void BVH::refit(Progress& progress)
{
	progress.set_substatus("Packing BVH primitives");
	pack_primitives();

	if(progress.get_cancel()) return;

	progress.set_substatus("Refitting BVH nodes");
	refit_nodes();
}

/* Triangles */

void BVH::pack_triangle(int idx, float4 tri_verts[3])
{
	int tob = pack.prim_object[idx];
	assert(tob >= 0 && tob < objects.size());
	const Mesh *mesh = objects[tob]->mesh;

	int tidx = pack.prim_index[idx];
	Mesh::Triangle t = mesh->get_triangle(tidx);
	const float3 *vpos = &mesh->verts[0];
	float3 v0 = vpos[t.v[0]];
	float3 v1 = vpos[t.v[1]];
	float3 v2 = vpos[t.v[2]];

	tri_verts[0] = float3_to_float4(v0);
	tri_verts[1] = float3_to_float4(v1);
	tri_verts[2] = float3_to_float4(v2);
}

void BVH::pack_primitives()
{
	const size_t tidx_size = pack.prim_index.size();
	size_t num_prim_triangles = 0;
	/* Count number of triangles primitives in BVH. */
	for(unsigned int i = 0; i < tidx_size; i++) {
		if((pack.prim_index[i] != -1)) {
			if ((pack.prim_type[i] & PRIMITIVE_ALL_TRIANGLE) != 0) {
				++num_prim_triangles;
			}
		}
	}
	/* Reserve size for arrays. */
	pack.prim_tri_index.clear();
	pack.prim_tri_index.resize(tidx_size);
	pack.prim_tri_verts.clear();
	pack.prim_tri_verts.resize(num_prim_triangles * 3);
	pack.prim_visibility.clear();
	pack.prim_visibility.resize(tidx_size);
	/* Fill in all the arrays. */
	size_t prim_triangle_index = 0;
	for(unsigned int i = 0; i < tidx_size; i++) {
		if(pack.prim_index[i] != -1) {
			int tob = pack.prim_object[i];
			Object *ob = objects[tob];

			if((pack.prim_type[i] & PRIMITIVE_ALL_TRIANGLE) != 0) {
				pack_triangle(i, (float4*)&pack.prim_tri_verts[3 * prim_triangle_index]);
				pack.prim_tri_index[i] = 3 * prim_triangle_index;
				++prim_triangle_index;
			}
			else {
				pack.prim_tri_index[i] = -1;
			}

			pack.prim_visibility[i] = ob->visibility;

			if(pack.prim_type[i] & PRIMITIVE_ALL_CURVE)
				pack.prim_visibility[i] |= PATH_RAY_CURVE;
		}
		else {
			pack.prim_tri_index[i] = -1;
			pack.prim_visibility[i] = 0;
		}
	}
}

/* Pack Instances */

void BVH::pack_instances(size_t nodes_size, size_t leaf_nodes_size)
{
	/* The BVH's for instances are built separately, but for traversal all
	 * BVH's are stored in global arrays. This function merges them into the
	 * top level BVH, adjusting indexes and offsets where appropriate.
	 */
	const bool use_qbvh = params.use_qbvh;

	/* Adjust primitive index to point to the triangle in the global array, for
	 * meshes with transform applied and already in the top level BVH.
	 */
	for(size_t i = 0; i < pack.prim_index.size(); i++)
		if(pack.prim_index[i] != -1) {
			if(pack.prim_type[i] & PRIMITIVE_ALL_CURVE)
				pack.prim_index[i] += objects[pack.prim_object[i]]->mesh->curve_offset;
			else
				pack.prim_index[i] += objects[pack.prim_object[i]]->mesh->tri_offset;
		}

	/* track offsets of instanced BVH data in global array */
	size_t prim_offset = pack.prim_index.size();
	size_t nodes_offset = nodes_size;
	size_t nodes_leaf_offset = leaf_nodes_size;

	/* clear array that gives the node indexes for instanced objects */
	pack.object_node.clear();

	/* reserve */
	size_t prim_index_size = pack.prim_index.size();
	size_t prim_tri_verts_size = pack.prim_tri_verts.size();

	size_t pack_prim_index_offset = prim_index_size;
	size_t pack_prim_tri_verts_offset = prim_tri_verts_size;
	size_t pack_nodes_offset = nodes_size;
	size_t pack_leaf_nodes_offset = leaf_nodes_size;
	size_t object_offset = 0;

	map<Mesh*, int> mesh_map;

	foreach(Object *ob, objects) {
		Mesh *mesh = ob->mesh;
		BVH *bvh = mesh->bvh;

		if(mesh->need_build_bvh()) {
			if(mesh_map.find(mesh) == mesh_map.end()) {
				prim_index_size += bvh->pack.prim_index.size();
				prim_tri_verts_size += bvh->pack.prim_tri_verts.size();
				nodes_size += bvh->pack.nodes.size();
				leaf_nodes_size += bvh->pack.leaf_nodes.size();

				mesh_map[mesh] = 1;
			}
		}
	}

	mesh_map.clear();

	pack.prim_index.resize(prim_index_size);
	pack.prim_type.resize(prim_index_size);
	pack.prim_object.resize(prim_index_size);
	pack.prim_visibility.resize(prim_index_size);
	pack.prim_tri_verts.resize(prim_tri_verts_size);
	pack.prim_tri_index.resize(prim_index_size);
	pack.nodes.resize(nodes_size);
	pack.leaf_nodes.resize(leaf_nodes_size);
	pack.object_node.resize(objects.size());

	int *pack_prim_index = (pack.prim_index.size())? &pack.prim_index[0]: NULL;
	int *pack_prim_type = (pack.prim_type.size())? &pack.prim_type[0]: NULL;
	int *pack_prim_object = (pack.prim_object.size())? &pack.prim_object[0]: NULL;
	uint *pack_prim_visibility = (pack.prim_visibility.size())? &pack.prim_visibility[0]: NULL;
	float4 *pack_prim_tri_verts = (pack.prim_tri_verts.size())? &pack.prim_tri_verts[0]: NULL;
	uint *pack_prim_tri_index = (pack.prim_tri_index.size())? &pack.prim_tri_index[0]: NULL;
	int4 *pack_nodes = (pack.nodes.size())? &pack.nodes[0]: NULL;
	int4 *pack_leaf_nodes = (pack.leaf_nodes.size())? &pack.leaf_nodes[0]: NULL;

	/* merge */
	foreach(Object *ob, objects) {
		Mesh *mesh = ob->mesh;

		/* We assume that if mesh doesn't need own BVH it was already included
		 * into a top-level BVH and no packing here is needed.
		 */
		if(!mesh->need_build_bvh()) {
			pack.object_node[object_offset++] = 0;
			continue;
		}

		/* if mesh already added once, don't add it again, but used set
		 * node offset for this object */
		map<Mesh*, int>::iterator it = mesh_map.find(mesh);

		if(mesh_map.find(mesh) != mesh_map.end()) {
			int noffset = it->second;
			pack.object_node[object_offset++] = noffset;
			continue;
		}

		BVH *bvh = mesh->bvh;

		int noffset = nodes_offset;
		int noffset_leaf = nodes_leaf_offset;
		int mesh_tri_offset = mesh->tri_offset;
		int mesh_curve_offset = mesh->curve_offset;

		/* fill in node indexes for instances */
		if(bvh->pack.root_index == -1)
			pack.object_node[object_offset++] = -noffset_leaf-1;
		else
			pack.object_node[object_offset++] = noffset;

		mesh_map[mesh] = pack.object_node[object_offset-1];

		/* merge primitive, object and triangle indexes */
		if(bvh->pack.prim_index.size()) {
			size_t bvh_prim_index_size = bvh->pack.prim_index.size();
			int *bvh_prim_index = &bvh->pack.prim_index[0];
			int *bvh_prim_type = &bvh->pack.prim_type[0];
			uint *bvh_prim_visibility = &bvh->pack.prim_visibility[0];
			uint *bvh_prim_tri_index = &bvh->pack.prim_tri_index[0];

			for(size_t i = 0; i < bvh_prim_index_size; i++) {
				if(bvh->pack.prim_type[i] & PRIMITIVE_ALL_CURVE) {
					pack_prim_index[pack_prim_index_offset] = bvh_prim_index[i] + mesh_curve_offset;
					pack_prim_tri_index[pack_prim_index_offset] = -1;
				}
				else {
					pack_prim_index[pack_prim_index_offset] = bvh_prim_index[i] + mesh_tri_offset;
					pack_prim_tri_index[pack_prim_index_offset] =
					        bvh_prim_tri_index[i] + pack_prim_tri_verts_offset;
				}

				pack_prim_type[pack_prim_index_offset] = bvh_prim_type[i];
				pack_prim_visibility[pack_prim_index_offset] = bvh_prim_visibility[i];
				pack_prim_object[pack_prim_index_offset] = 0;  // unused for instances
				pack_prim_index_offset++;
			}
		}

		/* Merge triangle vertices data. */
		if(bvh->pack.prim_tri_verts.size()) {
			const size_t prim_tri_size = bvh->pack.prim_tri_verts.size();
			memcpy(pack_prim_tri_verts + pack_prim_tri_verts_offset,
			       &bvh->pack.prim_tri_verts[0],
			       prim_tri_size*sizeof(float4));
			pack_prim_tri_verts_offset += prim_tri_size;
		}

		/* merge nodes */
		if(bvh->pack.leaf_nodes.size()) {
			int4 *leaf_nodes_offset = &bvh->pack.leaf_nodes[0];
			size_t leaf_nodes_offset_size = bvh->pack.leaf_nodes.size();
			for(size_t i = 0, j = 0;
			    i < leaf_nodes_offset_size;
			    i+= BVH_NODE_LEAF_SIZE, j++)
			{
				int4 data = leaf_nodes_offset[i];
				data.x += prim_offset;
				data.y += prim_offset;
				pack_leaf_nodes[pack_leaf_nodes_offset] = data;
				for(int j = 1; j < BVH_NODE_LEAF_SIZE; ++j) {
					pack_leaf_nodes[pack_leaf_nodes_offset + j] = leaf_nodes_offset[i + j];
				}
				pack_leaf_nodes_offset += BVH_NODE_LEAF_SIZE;
			}
		}

		if(bvh->pack.nodes.size()) {
			int4 *bvh_nodes = &bvh->pack.nodes[0];
			size_t bvh_nodes_size = bvh->pack.nodes.size();

			for(size_t i = 0, j = 0; i < bvh_nodes_size; j++) {
				size_t nsize, nsize_bbox;
				if(bvh_nodes[i].x & PATH_RAY_NODE_UNALIGNED) {
					nsize = use_qbvh
					            ? BVH_UNALIGNED_QNODE_SIZE
					            : BVH_UNALIGNED_NODE_SIZE;
					nsize_bbox = (use_qbvh)? 13: 0;
				}
				else {
					nsize = (use_qbvh)? BVH_QNODE_SIZE: BVH_NODE_SIZE;
					nsize_bbox = (use_qbvh)? 7: 0;
				}

				memcpy(pack_nodes + pack_nodes_offset,
				       bvh_nodes + i,
				       nsize_bbox*sizeof(int4));

				/* Modify offsets into arrays */
				int4 data = bvh_nodes[i + nsize_bbox];

				data.z += (data.z < 0)? -noffset_leaf: noffset;
				data.w += (data.w < 0)? -noffset_leaf: noffset;

				if(use_qbvh) {
					data.x += (data.x < 0)? -noffset_leaf: noffset;
					data.y += (data.y < 0)? -noffset_leaf: noffset;
				}

				pack_nodes[pack_nodes_offset + nsize_bbox] = data;

				/* Usually this copies nothing, but we better
				 * be prepared for possible node size extension.
				 */
				memcpy(&pack_nodes[pack_nodes_offset + nsize_bbox+1],
				       &bvh_nodes[i + nsize_bbox+1],
				       sizeof(int4) * (nsize - (nsize_bbox+1)));

				pack_nodes_offset += nsize;
				i += nsize;
			}
		}

		nodes_offset += bvh->pack.nodes.size();
		nodes_leaf_offset += bvh->pack.leaf_nodes.size();
		prim_offset += bvh->pack.prim_index.size();
	}
}

/* Regular BVH */

static bool node_bvh_is_unaligned(const BVHNode *node)
{
	const BVHNode *node0 = node->get_child(0),
	              *node1 = node->get_child(1);
	return node0->is_unaligned() || node1->is_unaligned();
}

RegularBVH::RegularBVH(const BVHParams& params_, const vector<Object*>& objects_)
: BVH(params_, objects_)
{
}

void RegularBVH::pack_leaf(const BVHStackEntry& e,
                           const LeafNode *leaf)
{
	assert(e.idx + BVH_NODE_LEAF_SIZE <= pack.leaf_nodes.size());
	float4 data[BVH_NODE_LEAF_SIZE];
	memset(data, 0, sizeof(data));
	if(leaf->num_triangles() == 1 && pack.prim_index[leaf->m_lo] == -1) {
		/* object */
		data[0].x = __int_as_float(~(leaf->m_lo));
		data[0].y = __int_as_float(0);
	}
	else {
		/* triangle */
		data[0].x = __int_as_float(leaf->m_lo);
		data[0].y = __int_as_float(leaf->m_hi);
	}
	data[0].z = __uint_as_float(leaf->m_visibility);
	if(leaf->num_triangles() != 0) {
		data[0].w = __uint_as_float(pack.prim_type[leaf->m_lo]);
	}

	memcpy(&pack.leaf_nodes[e.idx], data, sizeof(float4)*BVH_NODE_LEAF_SIZE);
}

void RegularBVH::pack_inner(const BVHStackEntry& e,
                            const BVHStackEntry& e0,
                            const BVHStackEntry& e1)
{
	if (e0.node->is_unaligned() || e1.node->is_unaligned()) {
		pack_unaligned_inner(e, e0, e1);
	} else {
		pack_aligned_inner(e, e0, e1);
	}
}

void RegularBVH::pack_aligned_inner(const BVHStackEntry& e,
                                    const BVHStackEntry& e0,
                                    const BVHStackEntry& e1)
{
	pack_aligned_node(e.idx,
	                  e0.node->m_bounds, e1.node->m_bounds,
	                  e0.encodeIdx(), e1.encodeIdx(),
	                  e0.node->m_visibility, e1.node->m_visibility);
}

void RegularBVH::pack_aligned_node(int idx,
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

void RegularBVH::pack_unaligned_inner(const BVHStackEntry& e,
                                      const BVHStackEntry& e0,
                                      const BVHStackEntry& e1)
{
	pack_unaligned_node(e.idx,
	                    e0.node->get_aligned_space(),
	                    e1.node->get_aligned_space(),
	                    e0.node->m_bounds,
	                    e1.node->m_bounds,
	                    e0.encodeIdx(), e1.encodeIdx(),
	                    e0.node->m_visibility, e1.node->m_visibility);
}

void RegularBVH::pack_unaligned_node(int idx,
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

void RegularBVH::pack_nodes(const BVHNode *root)
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
		nextNodeIdx += node_bvh_is_unaligned(root)
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
			for (int i = 0; i < 2; ++i) {
				if (e.node->get_child(i)->is_leaf()) {
					idx[i] = nextLeafNodeIdx++;
				}
				else {
					idx[i] = nextNodeIdx;
					nextNodeIdx += node_bvh_is_unaligned(e.node->get_child(i))
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

void RegularBVH::refit_nodes()
{
	assert(!params.top_level);

	BoundBox bbox = BoundBox::empty;
	uint visibility = 0;
	refit_node(0, (pack.root_index == -1)? true: false, bbox, visibility);
}

void RegularBVH::refit_node(int idx, bool leaf, BoundBox& bbox, uint& visibility)
{
	if(leaf) {
		assert(idx + BVH_NODE_LEAF_SIZE <= pack.leaf_nodes.size());
		const int4 *data = &pack.leaf_nodes[idx];
		const int c0 = data[0].x;
		const int c1 = data[0].y;
		/* refit leaf node */
		for(int prim = c0; prim < c1; prim++) {
			int pidx = pack.prim_index[prim];
			int tob = pack.prim_object[prim];
			Object *ob = objects[tob];

			if(pidx == -1) {
				/* object instance */
				bbox.grow(ob->bounds);
			}
			else {
				/* primitives */
				const Mesh *mesh = ob->mesh;

				if(pack.prim_type[prim] & PRIMITIVE_ALL_CURVE) {
					/* curves */
					int str_offset = (params.top_level)? mesh->curve_offset: 0;
					Mesh::Curve curve = mesh->get_curve(pidx - str_offset);
					int k = PRIMITIVE_UNPACK_SEGMENT(pack.prim_type[prim]);

					curve.bounds_grow(k, &mesh->curve_keys[0], &mesh->curve_radius[0], bbox);

					visibility |= PATH_RAY_CURVE;

					/* motion curves */
					if(mesh->use_motion_blur) {
						Attribute *attr = mesh->curve_attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

						if(attr) {
							size_t mesh_size = mesh->curve_keys.size();
							size_t steps = mesh->motion_steps - 1;
							float3 *key_steps = attr->data_float3();

							for(size_t i = 0; i < steps; i++)
								curve.bounds_grow(k, key_steps + i*mesh_size, &mesh->curve_radius[0], bbox);
						}
					}
				}
				else {
					/* triangles */
					int tri_offset = (params.top_level)? mesh->tri_offset: 0;
					Mesh::Triangle triangle = mesh->get_triangle(pidx - tri_offset);
					const float3 *vpos = &mesh->verts[0];

					triangle.bounds_grow(vpos, bbox);

					/* motion triangles */
					if(mesh->use_motion_blur) {
						Attribute *attr = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

						if(attr) {
							size_t mesh_size = mesh->verts.size();
							size_t steps = mesh->motion_steps - 1;
							float3 *vert_steps = attr->data_float3();

							for(size_t i = 0; i < steps; i++)
								triangle.bounds_grow(vert_steps + i*mesh_size, bbox);
						}
					}
				}
			}

			visibility |= ob->visibility;
		}

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

/* QBVH */

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
		has_unaligned |= node0->is_unaligned();
	}
	else {
		has_unaligned |= node0->get_child(0)->is_unaligned();
		has_unaligned |= node0->get_child(1)->is_unaligned();
	}
	if(node1->is_leaf()) {
		has_unaligned |= node1->is_unaligned();
	}
	else {
		has_unaligned |= node1->get_child(0)->is_unaligned();
		has_unaligned |= node1->get_child(1)->is_unaligned();
	}
	return has_unaligned;
}

QBVH::QBVH(const BVHParams& params_, const vector<Object*>& objects_)
: BVH(params_, objects_)
{
	params.use_qbvh = true;
}

void QBVH::pack_leaf(const BVHStackEntry& e, const LeafNode *leaf)
{
	float4 data[BVH_QNODE_LEAF_SIZE];
	memset(data, 0, sizeof(data));
	if(leaf->num_triangles() == 1 && pack.prim_index[leaf->m_lo] == -1) {
		/* object */
		data[0].x = __int_as_float(~(leaf->m_lo));
		data[0].y = __int_as_float(0);
	}
	else {
		/* triangle */
		data[0].x = __int_as_float(leaf->m_lo);
		data[0].y = __int_as_float(leaf->m_hi);
	}
	data[0].z = __uint_as_float(leaf->m_visibility);
	if(leaf->num_triangles() != 0) {
		data[0].w = __uint_as_float(pack.prim_type[leaf->m_lo]);
	}

	memcpy(&pack.leaf_nodes[e.idx], data, sizeof(float4)*BVH_QNODE_LEAF_SIZE);
}

void QBVH::pack_inner(const BVHStackEntry& e,
                      const BVHStackEntry *en,
                      int num)
{
	bool has_unaligned = false;
	/* Check whether we have to create unaligned node or all nodes are aligned
	 * and we can cut some corner here.
	 */
	if(params.use_unaligned_nodes) {
		for(int i = 0; i < num; i++) {
			if(en[i].node->is_unaligned()) {
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

void QBVH::pack_aligned_inner(const BVHStackEntry& e,
                              const BVHStackEntry *en,
                              int num)
{
	float4 data[BVH_QNODE_SIZE];
	memset(data, 0, sizeof(data));

	data[0].x = __uint_as_float(e.node->m_visibility & ~PATH_RAY_NODE_UNALIGNED);
	for(int i = 0; i < num; i++) {
		float3 bb_min = en[i].node->m_bounds.min;
		float3 bb_max = en[i].node->m_bounds.max;

		data[1][i] = bb_min.x;
		data[2][i] = bb_max.x;
		data[3][i] = bb_min.y;
		data[4][i] = bb_max.y;
		data[5][i] = bb_min.z;
		data[6][i] = bb_max.z;

		data[7][i] = __int_as_float(en[i].encodeIdx());
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

	memcpy(&pack.nodes[e.idx], data, sizeof(float4)*BVH_QNODE_SIZE);
}

void QBVH::pack_unaligned_inner(const BVHStackEntry& e,
                                const BVHStackEntry *en,
                                int num)
{
	float4 data[BVH_UNALIGNED_QNODE_SIZE];
	memset(data, 0, sizeof(data));

	data[0].x = __uint_as_float(e.node->m_visibility | PATH_RAY_NODE_UNALIGNED);

	for(int i = 0; i < num; i++) {
		Transform space = BVHUnaligned::compute_node_transform(
		        en[i].node->m_bounds,
		        en[i].node->get_aligned_space());

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

		data[13][i] = __int_as_float(en[i].encodeIdx());
	}

	for(int i = num; i < 4; i++) {
		/* We store BB which would never be recorded as intersection
		 * so kernel might safely assume there are always 4 child nodes.
		 */

		data[1][i] = 1.0f;
		data[2][i] = 0.0f;
		data[3][i] = 0.0f;

		data[4][i] = 0.0f;
		data[5][i] = 0.0f;
		data[6][i] = 0.0f;

		data[7][i] = 0.0f;
		data[8][i] = 0.0f;
		data[9][i] = 0.0f;

		data[10][i] = -FLT_MAX;
		data[11][i] = -FLT_MAX;
		data[12][i] = -FLT_MAX;

		data[13][i] = __int_as_float(0);
	}

	memcpy(&pack.nodes[e.idx], data, sizeof(float4)*BVH_UNALIGNED_QNODE_SIZE);
}

/* Quad SIMD Nodes */

void QBVH::pack_nodes(const BVHNode *root)
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

void QBVH::refit_nodes()
{
	assert(!params.top_level);

	BoundBox bbox = BoundBox::empty;
	uint visibility = 0;
	refit_node(0, (pack.root_index == -1)? true: false, bbox, visibility);
}

void QBVH::refit_node(int idx, bool leaf, BoundBox& bbox, uint& visibility)
{
	if(leaf) {
		int4 *data = &pack.leaf_nodes[idx];
		int4 c = data[0];
		/* Refit leaf node. */
		for(int prim = c.x; prim < c.y; prim++) {
			int pidx = pack.prim_index[prim];
			int tob = pack.prim_object[prim];
			Object *ob = objects[tob];

			if(pidx == -1) {
				/* Object instance. */
				bbox.grow(ob->bounds);
			}
			else {
				/* Primitives. */
				const Mesh *mesh = ob->mesh;

				if(pack.prim_type[prim] & PRIMITIVE_ALL_CURVE) {
					/* Curves. */
					int str_offset = (params.top_level)? mesh->curve_offset: 0;
					Mesh::Curve curve = mesh->get_curve(pidx - str_offset);
					int k = PRIMITIVE_UNPACK_SEGMENT(pack.prim_type[prim]);

					curve.bounds_grow(k, &mesh->curve_keys[0], &mesh->curve_radius[0], bbox);

					visibility |= PATH_RAY_CURVE;

					/* Motion curves. */
					if(mesh->use_motion_blur) {
						Attribute *attr = mesh->curve_attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

						if(attr) {
							size_t mesh_size = mesh->curve_keys.size();
							size_t steps = mesh->motion_steps - 1;
							float3 *key_steps = attr->data_float3();

							for(size_t i = 0; i < steps; i++)
								curve.bounds_grow(k, key_steps + i*mesh_size, &mesh->curve_radius[0], bbox);
						}
					}
				}
				else {
					/* Triangles. */
					int tri_offset = (params.top_level)? mesh->tri_offset: 0;
					Mesh::Triangle triangle = mesh->get_triangle(pidx - tri_offset);
					const float3 *vpos = &mesh->verts[0];

					triangle.bounds_grow(vpos, bbox);

					/* Motion triangles. */
					if(mesh->use_motion_blur) {
						Attribute *attr = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

						if(attr) {
							size_t mesh_size = mesh->verts.size();
							size_t steps = mesh->motion_steps - 1;
							float3 *vert_steps = attr->data_float3();

							for(size_t i = 0; i < steps; i++)
								triangle.bounds_grow(vert_steps + i*mesh_size, bbox);
						}
					}
				}
			}

			visibility |= ob->visibility;
		}

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

		/* TODO(sergey): To be de-duplicated with pack_inner(),
		 * but for that need some sort of pack_node(). which operates with
		 * direct data, not stack element.
		 */
		if(is_unaligned) {
			Transform aligned_space = transform_identity();
			float4 inner_data[BVH_UNALIGNED_QNODE_SIZE];
			inner_data[0] = make_float4(
			        __int_as_float(visibility | PATH_RAY_NODE_UNALIGNED),
			        0.0f,
			        0.0f,
			        0.0f);
			for(int i = 0; i < 4; ++i) {
				Transform space = BVHUnaligned::compute_node_transform(
				        child_bbox[i],
				        aligned_space);
				inner_data[1][i] = space.x.x;
				inner_data[2][i] = space.x.y;
				inner_data[3][i] = space.x.z;

				inner_data[4][i] = space.y.x;
				inner_data[5][i] = space.y.y;
				inner_data[6][i] = space.y.z;

				inner_data[7][i] = space.z.x;
				inner_data[8][i] = space.z.y;
				inner_data[9][i] = space.z.z;

				inner_data[10][i] = space.x.w;
				inner_data[11][i] = space.y.w;
				inner_data[12][i] = space.z.w;

				inner_data[13][i] = __int_as_float(c[i]);
			}
			memcpy(&pack.nodes[idx], inner_data, sizeof(float4)*BVH_UNALIGNED_QNODE_SIZE);
		}
		else {
			float4 inner_data[BVH_QNODE_SIZE];
			inner_data[0] = make_float4(
			        __int_as_float(visibility & ~PATH_RAY_NODE_UNALIGNED),
			        0.0f,
			        0.0f,
			        0.0f);
			for(int i = 0; i < 4; ++i) {
				float3 bb_min = child_bbox[i].min;
				float3 bb_max = child_bbox[i].max;
				inner_data[1][i] = bb_min.x;
				inner_data[2][i] = bb_max.x;
				inner_data[3][i] = bb_min.y;
				inner_data[4][i] = bb_max.y;
				inner_data[5][i] = bb_min.z;
				inner_data[6][i] = bb_max.z;
				inner_data[7][i] = __int_as_float(c[i]);
			}
			memcpy(&pack.nodes[idx], inner_data, sizeof(float4)*BVH_QNODE_SIZE);
		}
	}
}

CCL_NAMESPACE_END
