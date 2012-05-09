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
 
#include "bvh_build.h"
#include "bvh_sort.h"

#include "util_algorithm.h"
#include "util_debug.h"

CCL_NAMESPACE_BEGIN

struct BVHReferenceCompare {
public:
	int dim;

	BVHReferenceCompare(int dim_)
	{
		dim = dim_;
	}

	bool operator()(const BVHReference& ra, const BVHReference& rb)
	{
		float ca = ra.bounds().min[dim] + ra.bounds().max[dim];
		float cb = rb.bounds().min[dim] + rb.bounds().max[dim];

		if(ca < cb) return true;
		else if(ca > cb) return false;
		else if(ra.prim_object() < rb.prim_object()) return true;
		else if(ra.prim_object() > rb.prim_object()) return false;
		else if(ra.prim_index() < rb.prim_index()) return true;
		else if(ra.prim_index() > rb.prim_index()) return false;

		return false;
	}
};

void bvh_reference_sort(int start, int end, BVHReference *data, int dim)
{
	sort(data+start, data+end, BVHReferenceCompare(dim));
}

CCL_NAMESPACE_END

