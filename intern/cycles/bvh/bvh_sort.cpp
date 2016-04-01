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

/* silly workaround for float extended precision that happens when compiling
 * on x86, due to one float staying in 80 bit precision register and the other
 * not, which causes the strictly weak ordering to break */
#if !defined(__i386__)
#define NO_EXTENDED_PRECISION
#else
#define NO_EXTENDED_PRECISION volatile
#endif

struct BVHReferenceCompare {
public:
	int dim;

	BVHReferenceCompare(int dim_)
	{
		dim = dim_;
	}

	bool operator()(const BVHReference& ra, const BVHReference& rb)
	{
		NO_EXTENDED_PRECISION float ca = ra.bounds().min[dim] + ra.bounds().max[dim];
		NO_EXTENDED_PRECISION float cb = rb.bounds().min[dim] + rb.bounds().max[dim];

		if(ca < cb) return true;
		else if(ca > cb) return false;
		else if(ra.prim_object() < rb.prim_object()) return true;
		else if(ra.prim_object() > rb.prim_object()) return false;
		else if(ra.prim_index() < rb.prim_index()) return true;
		else if(ra.prim_index() > rb.prim_index()) return false;
		else if(ra.prim_type() < rb.prim_type()) return true;
		else if(ra.prim_type() > rb.prim_type()) return false;

		return false;
	}
};

void bvh_reference_sort(int start, int end, BVHReference *data, int dim)
{
	BVHReferenceCompare compare(dim);
	sort(data+start, data+end, compare);
}

CCL_NAMESPACE_END

