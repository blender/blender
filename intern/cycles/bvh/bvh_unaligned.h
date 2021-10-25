/*
 * Copyright 2011-2016 Blender Foundation
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

#ifndef __BVH_UNALIGNED_H__
#define __BVH_UNALIGNED_H__

#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class BoundBox;
class BVHObjectBinning;
class BVHRange;
class BVHReference;
struct Transform;
class Object;

/* Helper class to perform calculations needed for unaligned nodes. */
class BVHUnaligned {
public:
	BVHUnaligned(const vector<Object*>& objects);

	/* Calculate alignment for the oriented node for a given range. */
	Transform compute_aligned_space(
	        const BVHObjectBinning& range,
	        const BVHReference *references) const;
	Transform compute_aligned_space(
	        const BVHRange& range,
	        const BVHReference *references) const;

	/* Calculate alignment for the oriented node for a given reference.
	 *
	 * Return true when space was calculated successfully.
	 */
	bool compute_aligned_space(const BVHReference& ref,
	                           Transform *aligned_space) const;

	/* Calculate primitive's bounding box in given space. */
	BoundBox compute_aligned_prim_boundbox(
	        const BVHReference& prim,
	        const Transform& aligned_space) const;

	/* Calculate bounding box in given space. */
	BoundBox compute_aligned_boundbox(
	        const BVHObjectBinning& range,
	        const BVHReference *references,
	        const Transform& aligned_space,
	        BoundBox *cent_bounds = NULL) const;
	BoundBox compute_aligned_boundbox(
	        const BVHRange& range,
	        const BVHReference *references,
	        const Transform& aligned_space,
	        BoundBox *cent_bounds = NULL) const;

	/* Calculate affine transform for node packing.
	 * Bounds will be in the range of 0..1.
	 */
	static Transform compute_node_transform(const BoundBox& bounds,
	                                        const Transform& aligned_space);
protected:
	/* List of objects BVH is being created for. */
	const vector<Object*>& objects_;
};

CCL_NAMESPACE_END

#endif /* __BVH_UNALIGNED_H__ */
