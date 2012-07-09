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

#include "bvh.h"
#include "bvh_build.h"
#include "bvh_node.h"
#include "bvh_params.h"

#include "util_cache.h"
#include "util_debug.h"
#include "util_foreach.h"
#include "util_map.h"
#include "util_progress.h"
#include "util_types.h"

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
	key.add(&params, sizeof(params));

	foreach(Object *ob, objects) {
		key.add(ob->mesh->verts);
		key.add(ob->mesh->triangles);
		key.add(&ob->bounds, sizeof(ob->bounds));
		key.add(&ob->visibility, sizeof(ob->visibility));
		key.add(&ob->mesh->transform_applied, sizeof(bool));
	}

	CacheData value;

	if(Cache::global.lookup(key, value)) {
		cache_filename = key.get_filename();

		value.read(pack.root_index);
		value.read(pack.SAH);

		value.read(pack.nodes);
		value.read(pack.object_node);
		value.read(pack.tri_woop);
		value.read(pack.prim_visibility);
		value.read(pack.prim_index);
		value.read(pack.prim_object);
		value.read(pack.is_leaf);

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
	vector<int> prim_index;
	vector<int> prim_object;

	BVHBuild bvh_build(objects, prim_index, prim_object, params, progress);
	BVHNode *root = bvh_build.run();

	if(progress.get_cancel()) {
		if(root) root->deleteSubtree();
		return;
	}

	/* todo: get rid of this copy */
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
	progress.set_substatus("Packing BVH triangles");
	pack_triangles();

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
	progress.set_substatus("Packing BVH triangles");
	pack_triangles();

	if(progress.get_cancel()) return;

	progress.set_substatus("Refitting BVH nodes");
	refit_nodes();
}

/* Triangles */

void BVH::pack_triangle(int idx, float4 woop[3])
{
	/* create Woop triangle */
	int tob = pack.prim_object[idx];
	const Mesh *mesh = objects[tob]->mesh;
	int tidx = pack.prim_index[idx];
	const int *vidx = mesh->triangles[tidx].v;
	const float3* vpos = &mesh->verts[0];
	float3 v0 = vpos[vidx[0]];
	float3 v1 = vpos[vidx[1]];
	float3 v2 = vpos[vidx[2]];

	float3 r0 = v0 - v2;
	float3 r1 = v1 - v2;
	float3 r2 = cross(r0, r1);

	if(dot(r0, r0) == 0.0f || dot(r1, r1) == 0.0f || dot(r2, r2) == 0.0f) {
		/* degenerate */
		woop[0] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
		woop[1] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
		woop[2] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
	}
	else {
		Transform t = make_transform(
			r0.x, r1.x, r2.x, v2.x,
			r0.y, r1.y, r2.y, v2.y,
			r0.z, r1.z, r2.z, v2.z,
			0.0f, 0.0f, 0.0f, 1.0f);

		t = transform_inverse(t);

		woop[0] = make_float4(t.z.x, t.z.y, t.z.z, -t.z.w);
		woop[1] = make_float4(t.x.x, t.x.y, t.x.z, t.x.w);
		woop[2] = make_float4(t.y.x, t.y.y, t.y.z, t.y.w);
	}
}

void BVH::pack_triangles()
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

			pack_triangle(i, woop);
			memcpy(&pack.tri_woop[i * nsize], woop, sizeof(float4)*3);

			int tob = pack.prim_object[i];
			Object *ob = objects[tob];
			pack.prim_visibility[i] = ob->visibility;
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
		if(pack.prim_index[i] != -1)
			pack.prim_index[i] += objects[pack.prim_object[i]]->mesh->tri_offset;

	/* track offsets of instanced BVH data in global array */
	size_t tri_offset = pack.prim_index.size();
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
				nodes_size += bvh->pack.nodes.size()*nsize;

				mesh_map[mesh] = 1;
			}
		}
	}

	mesh_map.clear();

	pack.prim_index.resize(prim_index_size);
	pack.prim_object.resize(prim_index_size);
	pack.prim_visibility.resize(prim_index_size);
	pack.tri_woop.resize(tri_woop_size);
	pack.nodes.resize(nodes_size);
	pack.object_node.resize(objects.size());

	int *pack_prim_index = (pack.prim_index.size())? &pack.prim_index[0]: NULL;
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

		/* fill in node indexes for instances */
		if(
		   /* XXX, brecht. check this is needed!. it could be a bug elsewhere
		    * /mango/pro/scenes/04_2e/04_2e.blend r2158. on Ian's system 192.168.3.27  - campbell */
		   (bvh->pack.is_leaf.size() != 0) &&

		   /* previously only checked this */
		   bvh->pack.is_leaf[0])
		{
			pack.object_node[object_offset++] = -noffset-1;
		}
		else {
			pack.object_node[object_offset++] = noffset;
		}

		mesh_map[mesh] = pack.object_node[object_offset-1];

		/* merge primitive and object indexes */
		if(bvh->pack.prim_index.size()) {
			size_t bvh_prim_index_size = bvh->pack.prim_index.size();
			int *bvh_prim_index = &bvh->pack.prim_index[0];
			uint *bvh_prim_visibility = &bvh->pack.prim_visibility[0];

			for(size_t i = 0; i < bvh_prim_index_size; i++) {
				pack_prim_index[pack_prim_index_offset] = bvh_prim_index[i] + mesh_tri_offset;
				pack_prim_visibility[pack_prim_index_offset] = bvh_prim_visibility[i];
				pack_prim_object[pack_prim_index_offset] = 0;  // unused for instances
				pack_prim_index_offset++;
			}
		}

		/* merge triangle intersection data */
		if(bvh->pack.tri_woop.size()) {
			memcpy(pack_tri_woop+pack_tri_woop_offset, &bvh->pack.tri_woop[0],
				bvh->pack.tri_woop.size()*sizeof(float4));
			pack_tri_woop_offset += bvh->pack.tri_woop.size();
		}

		/* merge nodes */
		if( bvh->pack.nodes.size()) {
			size_t nsize_bbox = (use_qbvh)? nsize-2: nsize-1;
			int4 *bvh_nodes = &bvh->pack.nodes[0];
			size_t bvh_nodes_size = bvh->pack.nodes.size(); 
			int *bvh_is_leaf = &bvh->pack.is_leaf[0];

			for(size_t i = 0, j = 0; i < bvh_nodes_size; i+=nsize, j++) {
				memcpy(pack_nodes + pack_nodes_offset, bvh_nodes + i, nsize_bbox*sizeof(int4));

				/* modify offsets into arrays */
				int4 data = bvh_nodes[i + nsize_bbox];

				if(bvh_is_leaf[j]) {
					data.x += tri_offset;
					data.y += tri_offset;
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

				if(use_qbvh)
					pack_nodes[pack_nodes_offset + nsize_bbox+1] = bvh_nodes[i + nsize_bbox+1];

				pack_nodes_offset += nsize;
			}
		}

		nodes_offset += bvh->pack.nodes.size();
		tri_offset += bvh->pack.prim_index.size();
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
		make_int4(__float_as_int(b0.min.x), __float_as_int(b0.max.x), __float_as_int(b0.min.y), __float_as_int(b0.max.y)),
		make_int4(__float_as_int(b1.min.x), __float_as_int(b1.max.x), __float_as_int(b1.min.y), __float_as_int(b1.max.y)),
		make_int4(__float_as_int(b0.min.z), __float_as_int(b0.max.z), __float_as_int(b1.min.z), __float_as_int(b1.max.z)),
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
	int4 *data = &pack.nodes[idx*4];

	int c0 = data[3].x;
	int c1 = data[3].y;

	if(leaf) {
		/* refit leaf node */
		for(int tri = c0; tri < c1; tri++) {
			int tidx = pack.prim_index[tri];
			int tob = pack.prim_object[tri];
			Object *ob = objects[tob];

			if(tidx == -1) {
				/* object instance */
				bbox.grow(ob->bounds);
			}
			else {
				/* triangles */
				const Mesh *mesh = ob->mesh;
				int tri_offset = (params.top_level)? mesh->tri_offset: 0;
				const int *vidx = mesh->triangles[tidx - tri_offset].v;
				const float3 *vpos = &mesh->verts[0];

				bbox.grow(vpos[vidx[0]]);
				bbox.grow(vpos[vidx[1]]);
				bbox.grow(vpos[vidx[2]]);
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
		data[7][i] = 0.0f;
	}

	for(int i = num; i < 4; i++) {
		data[0][i] = 0.0f;
		data[1][i] = 0.0f;
		data[2][i] = 0.0f;

		data[3][i] = 0.0f;
		data[4][i] = 0.0f;
		data[5][i] = 0.0f;

		data[6][i] = __int_as_float(0);
		data[7][i] = 0.0f;
	}

	memcpy(&pack.nodes[e.idx * BVH_QNODE_SIZE], data, sizeof(float4)*BVH_QNODE_SIZE);
}

/* Quad SIMD Nodes */

void QBVH::pack_nodes(const array<int>& prims, const BVHNode *root)
{
	size_t node_size = root->getSubtreeSize(BVH_STAT_NODE_COUNT);

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
	assert(0); /* todo */
}

CCL_NAMESPACE_END

