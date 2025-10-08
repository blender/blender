/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_cache_mutex.hh"
#include "BLI_task.hh"

namespace blender {

void CacheMutex::ensure_impl(const FunctionRef<void()> compute_cache)
{
  if (cache_valid_.load(std::memory_order_acquire)) {
    return;
  }
  std::scoped_lock lock{mutex_};
  /* Double checked lock. */
  if (cache_valid_.load(std::memory_order_relaxed)) {
    return;
  }
  /* Use task isolation because a mutex is locked and the cache computation might use
   * multi-threading. */
  threading::isolate_task(compute_cache);

  cache_valid_.store(true, std::memory_order_release);
}

}  // namespace blender
