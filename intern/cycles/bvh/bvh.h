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

#ifndef __BVH_H__
#define __BVH_H__

#include "bvh_params.h"

#include "util_string.h"
#include "util_types.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class BVHNode;
struct BVHStackEntry;
class BVHParams;
class BoundBox;
class CacheData;
class LeafNode;
class Object;
class Progress;

#define BVH_NODE_SIZE	4
#define BVH_QNODE_SIZE	8
#define BVH_ALIGN		4096
#define TRI_NODE_SIZE	3

/* Packed BVH
 *
 * BVH stored as it will be used for traversal on the rendering device. */

struct PackedBVH {
	/* BVH nodes storage, one node is 4x int4, and contains two bounding boxes,
	   and child, triangle or object indexes dependening on the node type */
	array<int4> nodes; 
	/* object index to BVH node index mapping for instances */
	array<int> object_node; 
	/* precomputed triangle intersection data, one triangle is 4x float4 */
	array<float4> tri_woop; 
	/* visibility visibilitys for primitives */
	array<uint> prim_visibility;
	/* mapping from BVH primitive index to true primitive index, as primitives
	   may be duplicated due to spatial splits. -1 for instances. */
	array<int> prim_index;
	/* mapping from BVH primitive index, to the object id of that primitive. */
	array<int> prim_object;
	/* quick array to lookup if a node is a leaf, not used for traversal, only
	   for instance BVH merging  */
	array<int> is_leaf;

	/* index of the root node. */
	int root_index;

	/* surface area heuristic, for building top level BVH */
	float SAH;

	PackedBVH()
	{
		root_index = 0;
		SAH = 0.0f;
	}
};

/* BVH */

class BVH
{
public:
	PackedBVH pack;
	BVHParams params;
	vector<Object*> objects;
	string cache_filename;

	static BVH *create(const BVHParams& params, const vector<Object*>& objects);
	virtual ~BVH() {}

	void build(Progress& progress);
	void refit(Progress& progress);

	void clear_cache_except();

protected:
	BVH(const BVHParams& params, const vector<Object*>& objects);

	/* cache */
	bool cache_read(CacheData& key);
	void cache_write(CacheData& key);

	/* triangles */
	void pack_triangles();
	void pack_triangle(int idx, float4 woop[3]);

	/* merge instance BVH's */
	void pack_instances(size_t nodes_size);

	/* for subclasses to implement */
	virtual void pack_nodes(const array<int>& prims, const BVHNode *root) = 0;
	virtual void refit_nodes() = 0;
};

/* Regular BVH
 *
 * Typical BVH with each node having two children. */

class RegularBVH : public BVH {
protected:
	/* constructor */
	friend class BVH;
	RegularBVH(const BVHParams& params, const vector<Object*>& objects);

	/* pack */
	void pack_nodes(const array<int>& prims, const BVHNode *root);
	void pack_leaf(const BVHStackEntry& e, const LeafNode *leaf);
	void pack_inner(const BVHStackEntry& e, const BVHStackEntry& e0, const BVHStackEntry& e1);
	void pack_node(int idx, const BoundBox& b0, const BoundBox& b1, int c0, int c1, uint visibility0, uint visibility1);

	/* refit */
	void refit_nodes();
	void refit_node(int idx, bool leaf, BoundBox& bbox, uint& visibility);
};

/* QBVH
 *
 * Quad BVH, with each node having four children, to use with SIMD instructions. */

class QBVH : public BVH {
protected:
	/* constructor */
	friend class BVH;
	QBVH(const BVHParams& params, const vector<Object*>& objects);

	/* pack */
	void pack_nodes(const array<int>& prims, const BVHNode *root);
	void pack_leaf(const BVHStackEntry& e, const LeafNode *leaf);
	void pack_inner(const BVHStackEntry& e, const BVHStackEntry *en, int num);

	/* refit */
	void refit_nodes();
};

CCL_NAMESPACE_END

#endif /* __BVH_H__ */

