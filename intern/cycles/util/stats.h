/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_STATS_H__
#define __UTIL_STATS_H__

#include "util/atomic.h"
#include "util/profiling.h"

CCL_NAMESPACE_BEGIN

class Stats {
 public:
  enum static_init_t { static_init = 0 };

  Stats() : mem_used(0), mem_peak(0) {}
  explicit Stats(static_init_t) {}

  void mem_alloc(size_t size)
  {
    atomic_add_and_fetch_z(&mem_used, size);
    atomic_fetch_and_update_max_z(&mem_peak, mem_used);
  }

  void mem_free(size_t size)
  {
    assert(mem_used >= size);
    atomic_sub_and_fetch_z(&mem_used, size);
  }

  size_t mem_used;
  size_t mem_peak;
};

CCL_NAMESPACE_END

#endif /* __UTIL_STATS_H__ */
