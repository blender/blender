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

#ifndef __BVH4_H__
#define __BVH4_H__

#include "bvh/bvh.h"
#include "bvh/bvh_params.h"

#include "util/util_types.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class BVHNode;
struct BVHStackEntry;
class BVHParams;
class BoundBox;
class LeafNode;
class Object;
class Progress;

#define BVH_QNODE_SIZE           8
#define BVH_QNODE_LEAF_SIZE      1
#define BVH_UNALIGNED_QNODE_SIZE 14

/* BVH4
 *
 * Quad BVH, with each node having four children, to use with SIMD instructions.
 */
class BVH4 : public BVH {
protected:
	/* constructor */
	friend class BVH;
	BVH4(const BVHParams& params, const vector<Object*>& objects);

	/* Building process. */
	virtual BVHNode *widen_children_nodes(const BVHNode *root) override;

	/* pack */
	void pack_nodes(const BVHNode *root) override;

	void pack_leaf(const BVHStackEntry& e, const LeafNode *leaf);
	void pack_inner(const BVHStackEntry& e, const BVHStackEntry *en, int num);

	void pack_aligned_inner(const BVHStackEntry& e,
	                        const BVHStackEntry *en,
	                        int num);
	void pack_aligned_node(int idx,
	                       const BoundBox *bounds,
	                       const int *child,
	                       const uint visibility,
	                       const float time_from,
	                       const float time_to,
	                       const int num);

	void pack_unaligned_inner(const BVHStackEntry& e,
	                          const BVHStackEntry *en,
	                          int num);
	void pack_unaligned_node(int idx,
	                         const Transform *aligned_space,
	                         const BoundBox *bounds,
	                         const int *child,
	                         const uint visibility,
	                         const float time_from,
	                         const float time_to,
	                         const int num);

	/* refit */
	void refit_nodes() override;
	void refit_node(int idx, bool leaf, BoundBox& bbox, uint& visibility);
};

CCL_NAMESPACE_END

#endif  /* __BVH4_H__ */
