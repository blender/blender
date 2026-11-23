/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>

CCL_NAMESPACE_BEGIN

class Stats {
 public:
  void mem_alloc(const size_t size)
  {
    const size_t used = mem_used.fetch_add(size) + size;

    /* Atomic max. */
    size_t peak = mem_peak.load();
    while (peak < used && !mem_peak.compare_exchange_weak(peak, used)) {
    }
  }

  void mem_free(const size_t size)
  {
    assert(mem_used.load() >= size);
    mem_used.fetch_sub(size);
  }

  std::atomic<size_t> mem_used = 0;
  std::atomic<size_t> mem_peak = 0;
};

CCL_NAMESPACE_END
