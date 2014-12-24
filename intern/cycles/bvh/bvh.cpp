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

#include "util_cache.h"
#include "util_debug.h"
#include "util_foreach.h"
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

/* Cache */

bool BVH::cache_read(CacheData& key)
{
	key.add(system_cpu_bits());
	key.add(&params, sizeof(params));

	foreach(Object *ob, objects) {
		Mesh *mesh = ob->mesh;

		key.add(mesh->verts);
		key.add(mesh->triangles);
		key.add(mesh->curve_keys);
		key.add(mesh->curves);
		key.add(&ob->bounds, sizeof(ob->bounds));
		key.add(&ob->visibility, sizeof(ob->visibility));
		key.add(&mesh->transform_applied, sizeof(bool));

		if(mesh->use_motion_blur) {
			Attribute *attr = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
			if(attr)
				key.add(attr->buffer);

			attr = mesh->curve_attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
			if(attr)
				key.add(attr->buffer);
		}
	}

	CacheData value;

	if(Cache::global.lookup(key, value)) {
		cache_filename = key.get_filename();

		if(!(value.read(pack.root_index) &&
		     value.read(pack.SAH) &&
		     value.read(pack.nodes) &&
		     value.read(pack.object_node) &&
		     value.read(pack.tri_woop) &&
		     value.read(pack.prim_type) &&
		     value.read(pack.prim_visibility) &&
		     value.read(pack.prim_index) &&
		     value.read(pack.prim_object) &&
		     value.read(pack.is_leaf)))
		{
			/* Clear the pack if load failed. */
			pack.root_index = 0;
			pack.SAH = 0.0f;
			pack.nodes.clear();
			pack.object_node.clear();
			pack.tri_woop.clear();
			pack.prim_type.clear();
			pack.prim_visibility.clear();
			pack.prim_index.clear();
			pack.prim_object.clear();
			pack.is_leaf.clear();
			return false;
		}
		return true;
	}

	return false;
}

void BVH::cache_write(CacheData& key)
{
	CacheData value;

	value.add(pack.root_index);
	value.add(pack.SAH);

	value.add(pack.nodes);
	value.add(pack.object_node);
	value.add(pack.tri_woop);
	value.add(pack.prim_type);
	value.add(pack.prim_visibility);
	value.add(pack.prim_index);
	value.add(pack.prim_object);
	value.add(pack.is_leaf);

	Cache::global.insert(key, value);

	cache_filename = key.get_filename();
}

void BVH::clear_cache_except()
{
	set<string> except;

	if(!cache_filename.empty())
		except.insert(cache_filename);

	foreach(Object *ob, objects) {
		Mesh *mesh = ob->mesh;
		BVH *bvh = mesh->bvh;

		if(bvh && !bvh->cache_filename.empty())
			except.insert(bvh->cache_filename);
	}

	Cache::global.clear_except("bvh", except);
}

/* Building */

void BVH::build(Progress& progress)
{
	progress.set_substatus("Building BVH");

	/* cache read */
	CacheData key("bvh");

	if(params.use_cache) {
		progress.set_substatus("Looking in BVH cache");

		if(cache_read(key))
			return;
	}

	/* build nodes */
	vector<int> prim_type;
	vector<int> prim_index;
	vector<int> prim_object;

	BVHBuild bvh_build(objects, prim_type, prim_index, prim_object, params, progress);
	BVHNode *root = bvh_build.run();

	if(progress.get_cancel()) {
		if(root) root->deleteSubtree();
		return;
	}

	/* todo: get rid of this copy */
	pack.prim_type = prim_type;
	pack.prim_index = prim_index;
	pack.prim_object = prim_object;

	/* compute SAH */
	if(!params.top_level)
		pack.SAH = root->computeSubtreeSAHCost(params);

	if(progress.get_cancel()) {
		root->deleteSubtree();
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
	array<int> tmp_prim_object = pack.prim_object;
	pack_nodes(tmp_prim_object, root);
	
	/* free build nodes */
	root->deleteSubtree();

	if(progress.get_cancel()) return;

	/* cache write */
	if(params.use_cache) {
		progress.set_substatus("Writing BVH cache");
		cache_write(key);

		/* clear other bvh files from cache */
		if(params.top_level)
			clear_cache_except();
	}
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

void BVH::pack_triangle(int idx, float4 woop[3])
{
	int tob = pack.prim_object[idx];
	const Mesh *mesh = objects[tob]->mesh;

	if(mesh->has_motion_blur())
		return;

	int tidx = pack.prim_index[idx];
	const int *vidx = mesh->triangles[tidx].v;
	const float3* vpos = &mesh->verts[0];
	float3 v0 = vpos[vidx[0]];
	float3 v1 = vpos[vidx[1]];
	float3 v2 = vpos[vidx[2]];

	woop[0] = float3_to_float4(v0);
	woop[1] = float3_to_float4(v1);
	woop[2] = float3_to_float4(v2);
}

/* Curves*/

void BVH::pack_primitives()
{
	int nsize = TRI_NODE_SIZE;
	size_t tidx_size = pack.prim_index.size();

	pack.tri_woop.clear();
	pack.tri_woop.resize(tidx_size * nsize);
	pack.prim_visibility.clear();
	pack.prim_visibility.resize(tidx_size);

	for(unsigned int i = 0; i < tidx_size; i++) {
		if(pack.prim_index[i] != -1) {
			float4 woop[3];

			if(pack.prim_type[i] & PRIMITIVE_ALL_TRIANGLE)
				pack_triangle(i, woop);
			
			memcpy(&pack.tri_woop[i * nsize], woop, sizeof(float4)*3);

			int tob = pack.prim_object[i];
			Object *ob = objects[tob];
			pack.prim_visibility[i] = ob->visibility;

			if(pack.prim_type[i] & PRIMITIVE_ALL_CURVE)
				pack.prim_visibility[i] |= PATH_RAY_CURVE;
		}
		else {
			memset(&pack.tri_woop[i * nsize], 0, sizeof(float4)*3);
			pack.prim_visibility[i] = 0;
		}
	}
}

/* Pack Instances */

void BVH::pack_instances(size_t nodes_size)
{
	/* The BVH's for instances are built separately, but for traversal all
	 * BVH's are stored in global arrays. This function merges them into the
	 * top level BVH, adjusting indexes and offsets where appropriate. */
	bool use_qbvh = params.use_qbvh;
	size_t nsize = (use_qbvh)? BVH_QNODE_SIZE: BVH_NODE_SIZE;

	/* adjust primitive index to point to the triangle in the global array, for
	 * meshes with transform applied and already in the top level BVH */
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

	/* clear array that gives the node indexes for instanced objects */
	pack.object_node.clear();

	/* reserve */
	size_t prim_index_size = pack.prim_index.size();
	size_t tri_woop_size = pack.tri_woop.size();

	size_t pack_prim_index_offset = prim_index_size;
	size_t pack_tri_woop_offset = tri_woop_size;
	size_t pack_nodes_offset = nodes_size;
	size_t object_offset = 0;

	map<Mesh*, int> mesh_map;

	foreach(Object *ob, objects) {
		Mesh *mesh = ob->mesh;
		BVH *bvh = mesh->bvh;

		if(!mesh->transform_applied) {
			if(mesh_map.find(mesh) == mesh_map.end()) {
				prim_index_size += bvh->pack.prim_index.size();
				tri_woop_size += bvh->pack.tri_woop.size();
				nodes_size += bvh->pack.nodes.size();

				mesh_map[mesh] = 1;
			}
		}
	}

	mesh_map.clear();

	pack.prim_index.resize(prim_index_size);
	pack.prim_type.resize(prim_index_size);
	pack.prim_object.resize(prim_index_size);
	pack.prim_visibility.resize(prim_index_size);
	pack.tri_woop.resize(tri_woop_size);
	pack.nodes.resize(nodes_size);
	pack.object_node.resize(objects.size());

	int *pack_prim_index = (pack.prim_index.size())? &pack.prim_index[0]: NULL;
	int *pack_prim_type = (pack.prim_type.size())? &pack.prim_type[0]: NULL;
	int *pack_prim_object = (pack.prim_object.size())? &pack.prim_object[0]: NULL;
	uint *pack_prim_visibility = (pack.prim_visibility.size())? &pack.prim_visibility[0]: NULL;
	float4 *pack_tri_woop = (pack.tri_woop.size())? &pack.tri_woop[0]: NULL;
	int4 *pack_nodes = (pack.nodes.size())? &pack.nodes[0]: NULL;

	/* merge */
	foreach(Object *ob, objects) {
		Mesh *mesh = ob->mesh;

		/* if mesh transform is applied, that means it's already in the top
		 * level BVH, and we don't need to merge it in */
		if(mesh->transform_applied) {
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

		int noffset = nodes_offset/nsize;
		int mesh_tri_offset = mesh->tri_offset;
		int mesh_curve_offset = mesh->curve_offset;

		/* fill in node indexes for instances */
		if((bvh->pack.is_leaf.size() != 0) && bvh->pack.is_leaf[0])
			pack.object_node[object_offset++] = -noffset-1;
		else
			pack.object_node[object_offset++] = noffset;

		mesh_map[mesh] = pack.object_node[object_offset-1];

		/* merge primitive and object indexes */
		if(bvh->pack.prim_index.size()) {
			size_t bvh_prim_index_size = bvh->pack.prim_index.size();
			int *bvh_prim_index = &bvh->pack.prim_index[0];
			int *bvh_prim_type = &bvh->pack.prim_type[0];
			uint *bvh_prim_visibility = &bvh->pack.prim_visibility[0];

			for(size_t i = 0; i < bvh_prim_index_size; i++) {
				if(bvh->pack.prim_type[i] & PRIMITIVE_ALL_CURVE)
					pack_prim_index[pack_prim_index_offset] = bvh_prim_index[i] + mesh_curve_offset;
				else
					pack_prim_index[pack_prim_index_offset] = bvh_prim_index[i] + mesh_tri_offset;

				pack_prim_type[pack_prim_index_offset] = bvh_prim_type[i];
				pack_prim_visibility[pack_prim_index_offset] = bvh_prim_visibility[i];
				pack_prim_object[pack_prim_index_offset] = 0;  // unused for instances
				pack_prim_index_offset++;
			}
		}

		/* merge triangle intersection data */
		if(bvh->pack.tri_woop.size()) {
			memcpy(pack_tri_woop + pack_tri_woop_offset, &bvh->pack.tri_woop[0],
				bvh->pack.tri_woop.size()*sizeof(float4));
			pack_tri_woop_offset += bvh->pack.tri_woop.size();
		}

		/* merge nodes */
		if(bvh->pack.nodes.size()) {
			/* For QBVH we're packing ann child bbox into 6 float4,
			 * and for regulat BVH they're packed into 3 float4.
			 */
			size_t nsize_bbox = (use_qbvh)? 6: 3;
			int4 *bvh_nodes = &bvh->pack.nodes[0];
			size_t bvh_nodes_size = bvh->pack.nodes.size(); 
			int *bvh_is_leaf = (bvh->pack.is_leaf.size() != 0) ? &bvh->pack.is_leaf[0] : NULL;

			for(size_t i = 0, j = 0; i < bvh_nodes_size; i+=nsize, j++) {
				memcpy(pack_nodes + pack_nodes_offset, bvh_nodes + i, nsize_bbox*sizeof(int4));

				/* modify offsets into arrays */
				int4 data = bvh_nodes[i + nsize_bbox];

				if(bvh_is_leaf && bvh_is_leaf[j]) {
					data.x += prim_offset;
					data.y += prim_offset;
				}
				else {
					data.x += (data.x < 0)? -noffset: noffset;
					data.y += (data.y < 0)? -noffset: noffset;

					if(use_qbvh) {
						data.z += (data.z < 0)? -noffset: noffset;
						data.w += (data.w < 0)? -noffset: noffset;
					}
				}

				pack_nodes[pack_nodes_offset + nsize_bbox] = data;

				/* Usually this is gonna to copy nothing, but we'd better to
				 * beprepared for possible node size extension.
				 */
				memcpy(&pack_nodes[pack_nodes_offset + nsize_bbox+1],
				       &bvh_nodes[i + nsize_bbox+1],
				       sizeof(int4) * (nsize - (nsize_bbox+1)));

				pack_nodes_offset += nsize;
			}
		}

		nodes_offset += bvh->pack.nodes.size();
		prim_offset += bvh->pack.prim_index.size();
	}
}

/* Regular BVH */

RegularBVH::RegularBVH(const BVHParams& params_, const vector<Object*>& objects_)
: BVH(params_, objects_)
{
}

void RegularBVH::pack_leaf(const BVHStackEntry& e, const LeafNode *leaf)
{
	if(leaf->num_triangles() == 1 && pack.prim_index[leaf->m_lo] == -1)
		/* object */
		pack_node(e.idx, leaf->m_bounds, leaf->m_bounds, ~(leaf->m_lo), 0, leaf->m_visibility, leaf->m_visibility);
	else
		/* triangle */
		pack_node(e.idx, leaf->m_bounds, leaf->m_bounds, leaf->m_lo, leaf->m_hi, leaf->m_visibility, leaf->m_visibility);
}

void RegularBVH::pack_inner(const BVHStackEntry& e, const BVHStackEntry& e0, const BVHStackEntry& e1)
{
	pack_node(e.idx, e0.node->m_bounds, e1.node->m_bounds, e0.encodeIdx(), e1.encodeIdx(), e0.node->m_visibility, e1.node->m_visibility);
}

void RegularBVH::pack_node(int idx, const BoundBox& b0, const BoundBox& b1, int c0, int c1, uint visibility0, uint visibility1)
{
	int4 data[BVH_NODE_SIZE] =
	{
		make_int4(__float_as_int(b0.min.x), __float_as_int(b1.min.x), __float_as_int(b0.max.x), __float_as_int(b1.max.x)),
		make_int4(__float_as_int(b0.min.y), __float_as_int(b1.min.y), __float_as_int(b0.max.y), __float_as_int(b1.max.y)),
		make_int4(__float_as_int(b0.min.z), __float_as_int(b1.min.z), __float_as_int(b0.max.z), __float_as_int(b1.max.z)),
		make_int4(c0, c1, visibility0, visibility1)
	};

	memcpy(&pack.nodes[idx * BVH_NODE_SIZE], data, sizeof(int4)*BVH_NODE_SIZE);
}

void RegularBVH::pack_nodes(const array<int>& prims, const BVHNode *root)
{
	size_t node_size = root->getSubtreeSize(BVH_STAT_NODE_COUNT);

	/* resize arrays */
	pack.nodes.clear();
	pack.is_leaf.clear();
	pack.is_leaf.resize(node_size);

	/* for top level BVH, first merge existing BVH's so we know the offsets */
	if(params.top_level)
		pack_instances(node_size*BVH_NODE_SIZE);
	else
		pack.nodes.resize(node_size*BVH_NODE_SIZE);

	int nextNodeIdx = 0;

	vector<BVHStackEntry> stack;
	stack.reserve(BVHParams::MAX_DEPTH*2);
	stack.push_back(BVHStackEntry(root, nextNodeIdx++));

	while(stack.size()) {
		BVHStackEntry e = stack.back();
		stack.pop_back();

		pack.is_leaf[e.idx] = e.node->is_leaf();

		if(e.node->is_leaf()) {
			/* leaf node */
			const LeafNode* leaf = reinterpret_cast<const LeafNode*>(e.node);
			pack_leaf(e, leaf);
		}
		else {
			/* innner node */
			stack.push_back(BVHStackEntry(e.node->get_child(0), nextNodeIdx++));
			stack.push_back(BVHStackEntry(e.node->get_child(1), nextNodeIdx++));

			pack_inner(e, stack[stack.size()-2], stack[stack.size()-1]);
		}
	}

	/* root index to start traversal at, to handle case of single leaf node */
	pack.root_index = (pack.is_leaf[0])? -1: 0;
}

void RegularBVH::refit_nodes()
{
	assert(!params.top_level);

	BoundBox bbox = BoundBox::empty;
	uint visibility = 0;
	refit_node(0, (pack.is_leaf[0])? true: false, bbox, visibility);
}

void RegularBVH::refit_node(int idx, bool leaf, BoundBox& bbox, uint& visibility)
{
	int4 *data = &pack.nodes[idx*BVH_NODE_SIZE];

	int c0 = data[3].x;
	int c1 = data[3].y;

	if(leaf) {
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
					const Mesh::Curve& curve = mesh->curves[pidx - str_offset];
					int k = PRIMITIVE_UNPACK_SEGMENT(pack.prim_type[prim]);

					curve.bounds_grow(k, &mesh->curve_keys[0], bbox);

					visibility |= PATH_RAY_CURVE;

					/* motion curves */
					if(mesh->use_motion_blur) {
						Attribute *attr = mesh->curve_attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

						if(attr) {
							size_t mesh_size = mesh->curve_keys.size();
							size_t steps = mesh->motion_steps - 1;
							float4 *key_steps = attr->data_float4();

							for (size_t i = 0; i < steps; i++)
								curve.bounds_grow(k, key_steps + i*mesh_size, bbox);
						}
					}
				}
				else {
					/* triangles */
					int tri_offset = (params.top_level)? mesh->tri_offset: 0;
					const Mesh::Triangle& triangle = mesh->triangles[pidx - tri_offset];
					const float3 *vpos = &mesh->verts[0];

					triangle.bounds_grow(vpos, bbox);

					/* motion triangles */
					if(mesh->use_motion_blur) {
						Attribute *attr = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

						if(attr) {
							size_t mesh_size = mesh->verts.size();
							size_t steps = mesh->motion_steps - 1;
							float3 *vert_steps = attr->data_float3();

							for (size_t i = 0; i < steps; i++)
								triangle.bounds_grow(vert_steps + i*mesh_size, bbox);
						}
					}
				}
			}

			visibility |= ob->visibility;
		}

		pack_node(idx, bbox, bbox, c0, c1, visibility, visibility);
	}
	else {
		/* refit inner node, set bbox from children */
		BoundBox bbox0 = BoundBox::empty, bbox1 = BoundBox::empty;
		uint visibility0 = 0, visibility1 = 0;

		refit_node((c0 < 0)? -c0-1: c0, (c0 < 0), bbox0, visibility0);
		refit_node((c1 < 0)? -c1-1: c1, (c1 < 0), bbox1, visibility1);

		pack_node(idx, bbox0, bbox1, c0, c1, visibility0, visibility1);

		bbox.grow(bbox0);
		bbox.grow(bbox1);
		visibility = visibility0|visibility1;
	}
}

/* QBVH */

QBVH::QBVH(const BVHParams& params_, const vector<Object*>& objects_)
: BVH(params_, objects_)
{
	params.use_qbvh = true;

	/* todo: use visibility */
}

void QBVH::pack_leaf(const BVHStackEntry& e, const LeafNode *leaf)
{
	float4 data[BVH_QNODE_SIZE];

	memset(data, 0, sizeof(data));

	if(leaf->num_triangles() == 1 && pack.prim_index[leaf->m_lo] == -1) {
		/* object */
		data[6].x = __int_as_float(~(leaf->m_lo));
		data[6].y = __int_as_float(0);
	}
	else {
		/* triangle */
		data[6].x = __int_as_float(leaf->m_lo);
		data[6].y = __int_as_float(leaf->m_hi);
	}
	data[6].z = __uint_as_float(leaf->m_visibility);

	memcpy(&pack.nodes[e.idx * BVH_QNODE_SIZE], data, sizeof(float4)*BVH_QNODE_SIZE);
}

void QBVH::pack_inner(const BVHStackEntry& e, const BVHStackEntry *en, int num)
{
	float4 data[BVH_QNODE_SIZE];

	for(int i = 0; i < num; i++) {
		float3 bb_min = en[i].node->m_bounds.min;
		float3 bb_max = en[i].node->m_bounds.max;

		data[0][i] = bb_min.x;
		data[1][i] = bb_max.x;
		data[2][i] = bb_min.y;
		data[3][i] = bb_max.y;
		data[4][i] = bb_min.z;
		data[5][i] = bb_max.z;

		data[6][i] = __int_as_float(en[i].encodeIdx());
	}

	for(int i = num; i < 4; i++) {
		/* We store BB which would never be recorded as intersection
		 * so kernel might safely assume there are always 4 child nodes.
		 */
		data[0][i] = FLT_MAX;
		data[1][i] = -FLT_MAX;

		data[2][i] = FLT_MAX;
		data[3][i] = -FLT_MAX;

		data[4][i] = FLT_MAX;
		data[5][i] = -FLT_MAX;

		data[6][i] = __int_as_float(0);
	}

	memcpy(&pack.nodes[e.idx * BVH_QNODE_SIZE], data, sizeof(float4)*BVH_QNODE_SIZE);
}

/* Quad SIMD Nodes */

void QBVH::pack_nodes(const array<int>& prims, const BVHNode *root)
{
	size_t node_size = root->getSubtreeSize(BVH_STAT_QNODE_COUNT);

	/* resize arrays */
	pack.nodes.clear();
	pack.is_leaf.clear();
	pack.is_leaf.resize(node_size);

	/* for top level BVH, first merge existing BVH's so we know the offsets */
	if(params.top_level)
		pack_instances(node_size*BVH_QNODE_SIZE);
	else
		pack.nodes.resize(node_size*BVH_QNODE_SIZE);

	int nextNodeIdx = 0;

	vector<BVHStackEntry> stack;
	stack.reserve(BVHParams::MAX_DEPTH*2);
	stack.push_back(BVHStackEntry(root, nextNodeIdx++));

	while(stack.size()) {
		BVHStackEntry e = stack.back();
		stack.pop_back();

		pack.is_leaf[e.idx] = e.node->is_leaf();

		if(e.node->is_leaf()) {
			/* leaf node */
			const LeafNode* leaf = reinterpret_cast<const LeafNode*>(e.node);
			pack_leaf(e, leaf);
		}
		else {
			/* inner node */
			const BVHNode *node = e.node;
			const BVHNode *node0 = node->get_child(0);
			const BVHNode *node1 = node->get_child(1);

			/* collect nodes */
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

			/* push entries on the stack */
			for(int i = 0; i < numnodes; i++)
				stack.push_back(BVHStackEntry(nodes[i], nextNodeIdx++));

			/* set node */
			pack_inner(e, &stack[stack.size()-numnodes], numnodes);
		}
	}

	/* root index to start traversal at, to handle case of single leaf node */
	pack.root_index = (pack.is_leaf[0])? -1: 0;
}

void QBVH::refit_nodes()
{
	assert(!params.top_level);

	BoundBox bbox = BoundBox::empty;
	uint visibility = 0;
	refit_node(0, (pack.is_leaf[0])? true: false, bbox, visibility);
}

void QBVH::refit_node(int idx, bool leaf, BoundBox& bbox, uint& visibility)
{
	int4 *data = &pack.nodes[idx*BVH_QNODE_SIZE];
	int4 c = data[6];
	if(leaf) {
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
					const Mesh::Curve& curve = mesh->curves[pidx - str_offset];
					int k = PRIMITIVE_UNPACK_SEGMENT(pack.prim_type[prim]);

					curve.bounds_grow(k, &mesh->curve_keys[0], bbox);

					visibility |= PATH_RAY_CURVE;

					/* Motion curves. */
					if(mesh->use_motion_blur) {
						Attribute *attr = mesh->curve_attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

						if(attr) {
							size_t mesh_size = mesh->curve_keys.size();
							size_t steps = mesh->motion_steps - 1;
							float4 *key_steps = attr->data_float4();

							for (size_t i = 0; i < steps; i++)
								curve.bounds_grow(k, key_steps + i*mesh_size, bbox);
						}
					}
				}
				else {
					/* Triangles. */
					int tri_offset = (params.top_level)? mesh->tri_offset: 0;
					const Mesh::Triangle& triangle = mesh->triangles[pidx - tri_offset];
					const float3 *vpos = &mesh->verts[0];

					triangle.bounds_grow(vpos, bbox);

					/* Motion triangles. */
					if(mesh->use_motion_blur) {
						Attribute *attr = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

						if(attr) {
							size_t mesh_size = mesh->verts.size();
							size_t steps = mesh->motion_steps - 1;
							float3 *vert_steps = attr->data_float3();

							for (size_t i = 0; i < steps; i++)
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
		 * making code more geenral ends up in much nastier code
		 * in my opinion so far.
		 *
		 * Same applies to the inner nodes case below.
		 */
		float4 leaf_data[BVH_QNODE_SIZE];
		memset(leaf_data, 0, sizeof(leaf_data));
		leaf_data[6].x = __int_as_float(c.x);
		leaf_data[6].y = __int_as_float(c.y);
		leaf_data[6].z = __uint_as_float(visibility);
		memcpy(&pack.nodes[idx * BVH_QNODE_SIZE],
		       leaf_data,
		       sizeof(float4)*BVH_QNODE_SIZE);
	}
	else {
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

		float4 inner_data[BVH_QNODE_SIZE];
		for(int i = 0; i < 4; ++i) {
			float3 bb_min = child_bbox[i].min;
			float3 bb_max = child_bbox[i].max;
			inner_data[0][i] = bb_min.x;
			inner_data[1][i] = bb_max.x;
			inner_data[2][i] = bb_min.y;
			inner_data[3][i] = bb_max.y;
			inner_data[4][i] = bb_min.z;
			inner_data[5][i] = bb_max.z;
			inner_data[6][i] = __int_as_float(c[i]);
		}
		memcpy(&pack.nodes[idx * BVH_QNODE_SIZE],
		       inner_data,
		       sizeof(float4)*BVH_QNODE_SIZE);
	}
}

CCL_NAMESPACE_END
