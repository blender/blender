/*
 * Copyright 2011-2013 Blender Foundation
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

#ifndef __UTIL_STATS_H__
#define __UTIL_STATS_H__

#include "util/util_atomic.h"

CCL_NAMESPACE_BEGIN

class Stats {
public:
	enum static_init_t { static_init = 0 };

	Stats() : mem_used(0), mem_peak(0) {}
	explicit Stats(static_init_t) {}

	void mem_alloc(size_t size) {
		atomic_add_and_fetch_z(&mem_used, size);
		atomic_fetch_and_update_max_z(&mem_peak, mem_used);
	}

	void mem_free(size_t size) {
		assert(mem_used >= size);
		atomic_sub_and_fetch_z(&mem_used, size);
	}

	size_t mem_used;
	size_t mem_peak;
};

CCL_NAMESPACE_END

#endif  /* __UTIL_STATS_H__ */
