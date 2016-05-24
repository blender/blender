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
#include "util_task.h"

CCL_NAMESPACE_BEGIN

static const int BVH_SORT_THRESHOLD = 4096;

/* Silly workaround for float extended precision that happens when compiling
 * on x86, due to one float staying in 80 bit precision register and the other
 * not, which causes the strictly weak ordering to break.
 */
#if !defined(__i386__)
#  define NO_EXTENDED_PRECISION
#else
#  define NO_EXTENDED_PRECISION volatile
#endif

struct BVHReferenceCompare {
public:
	int dim;

	explicit BVHReferenceCompare(int dim_)
	{
		dim = dim_;
	}

	/* Compare two references.
	 *
	 * Returns value is similar to return value of strcmp().
	 */
	__forceinline int compare(const BVHReference& ra,
	                          const BVHReference& rb) const
	{
		NO_EXTENDED_PRECISION float ca = ra.bounds().min[dim] + ra.bounds().max[dim];
		NO_EXTENDED_PRECISION float cb = rb.bounds().min[dim] + rb.bounds().max[dim];

		if(ca < cb) return -1;
		else if(ca > cb) return 1;
		else if(ra.prim_object() < rb.prim_object()) return -1;
		else if(ra.prim_object() > rb.prim_object()) return 1;
		else if(ra.prim_index() < rb.prim_index()) return -1;
		else if(ra.prim_index() > rb.prim_index()) return 1;
		else if(ra.prim_type() < rb.prim_type()) return -1;
		else if(ra.prim_type() > rb.prim_type()) return 1;

		return 0;
	}

	bool operator()(const BVHReference& ra, const BVHReference& rb)
	{
		return (compare(ra, rb) < 0);
	}
};

static void bvh_reference_sort_threaded(TaskPool *task_pool,
                                        BVHReference *data,
                                        const int job_start,
                                        const int job_end,
                                        const BVHReferenceCompare& compare);

class BVHSortTask : public Task {
public:
	BVHSortTask(TaskPool *task_pool,
	            BVHReference *data,
	            const int job_start,
	            const int job_end,
	            const BVHReferenceCompare& compare)
	{
		run = function_bind(bvh_reference_sort_threaded,
		                    task_pool,
		                    data,
		                    job_start,
		                    job_end,
		                    compare);
	}
};

/* Multi-threaded reference sort. */
static void bvh_reference_sort_threaded(TaskPool *task_pool,
                                        BVHReference *data,
                                        const int job_start,
                                        const int job_end,
                                        const BVHReferenceCompare& compare)
{
	int start = job_start, end = job_end;
	bool have_work = (start < end);
	while(have_work) {
		const int count = job_end - job_start;
		if(count < BVH_SORT_THRESHOLD) {
			/* Number of reference low enough, faster to finish the job
			 * in one thread rather than to spawn more threads.
			 */
			sort(data+job_start, data+job_end+1, compare);
			break;
		}
		/* Single QSort step.
		 * Use median-of-three method for the pivot point.
		 */
		int left = start, right = end;
		int center = (left + right) >> 1;
		if(compare.compare(data[left], data[center]) > 0) {
			swap(data[left], data[center]);
		}
		if(compare.compare(data[left], data[right]) > 0) {
			swap(data[left], data[right]);
		}
		if(compare.compare(data[center], data[right]) > 0) {
			swap(data[center], data[right]);
		}
		swap(data[center], data[right - 1]);
		BVHReference median = data[right - 1];
		do {
			while(compare.compare(data[left], median) < 0) {
				++left;
			}
			while(compare.compare(data[right], median) > 0) {
				--right;
			}
			if(left <= right) {
				swap(data[left], data[right]);
				++left;
				--right;
			}
		} while(left <= right);
		/* We only create one new task here to reduce downside effects of
		 * latency in TaskScheduler.
		 * So generally current thread keeps working on the left part of the
		 * array, and we create new task for the right side.
		 * However, if there's nothing to be done in the left side of the array
		 * we don't create any tasks and make it so current thread works on the
		 * right side.
		 */
		have_work = false;
		if(left < end) {
			if(start < right) {
				task_pool->push(new BVHSortTask(task_pool,
				                                data,
				                                left, end,
				                                compare), true);
			}
			else {
				start = left;
				have_work = true;
			}
		}
		if(start < right) {
			end = right;
			have_work = true;
		}
	}
}

void bvh_reference_sort(int start, int end, BVHReference *data, int dim)
{
	const int count = end - start;
	BVHReferenceCompare compare(dim);
	if(count < BVH_SORT_THRESHOLD) {
		/* It is important to not use any mutex if array is small enough,
		 * otherwise we end up in situation when we're going to sleep far
		 * too often.
		 */
		sort(data+start, data+end, compare);
	}
	else {
		TaskPool task_pool;
		bvh_reference_sort_threaded(&task_pool, data, start, end - 1, compare);
		task_pool.wait_work();
	}
}

CCL_NAMESPACE_END

