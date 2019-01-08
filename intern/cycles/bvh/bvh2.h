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

#ifndef __BVH2_H__
#define __BVH2_H__

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

#define BVH_NODE_SIZE           4
#define BVH_NODE_LEAF_SIZE      1
#define BVH_UNALIGNED_NODE_SIZE 7

/* BVH2
 *
 * Typical BVH with each node having two children.
 */
class BVH2 : public BVH {
protected:
	/* constructor */
	friend class BVH;
	BVH2(const BVHParams& params, const vector<Object*>& objects);

	/* Building process. */
	virtual BVHNode *widen_children_nodes(const BVHNode *root) override;

	/* pack */
	void pack_nodes(const BVHNode *root);

	void pack_leaf(const BVHStackEntry& e,
	               const LeafNode *leaf);
	void pack_inner(const BVHStackEntry& e,
	                const BVHStackEntry& e0,
	                const BVHStackEntry& e1);

	void pack_aligned_inner(const BVHStackEntry& e,
	                        const BVHStackEntry& e0,
	                        const BVHStackEntry& e1);
	void pack_aligned_node(int idx,
	                       const BoundBox& b0,
	                       const BoundBox& b1,
	                       int c0, int c1,
	                       uint visibility0, uint visibility1);

	void pack_unaligned_inner(const BVHStackEntry& e,
	                          const BVHStackEntry& e0,
	                          const BVHStackEntry& e1);
	void pack_unaligned_node(int idx,
	                         const Transform& aligned_space0,
	                         const Transform& aligned_space1,
	                         const BoundBox& b0,
	                         const BoundBox& b1,
	                         int c0, int c1,
	                         uint visibility0, uint visibility1);

	/* refit */
	void refit_nodes();
	void refit_node(int idx, bool leaf, BoundBox& bbox, uint& visibility);
};

CCL_NAMESPACE_END

#endif  /* __BVH2_H__ */
