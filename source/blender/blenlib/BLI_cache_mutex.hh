/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#pragma once

/**
 * A #CacheMutex is used to protect a lazily computed cache from being computed more than once.
 * Using #CacheMutex instead of a "raw mutex" to protect a cache has some benefits:
 * - Avoid common pitfalls like forgetting to use task isolation or a double checked lock.
 * - Cleaner and less redundant code because the same locking patterns don't have to be repeated
 *   everywhere.
 * - One can benefit from potential future improvements to #CacheMutex of which there are a few
 *   mentioned below.
 *
 * The data protected by #CacheMutex is not part of #CacheMutex. Instead, the #CacheMutex and its
 * protected data should generally be placed next to each other.
 *
 * Each #CacheMutex protects exactly one cache, so multiple cache mutexes have to be used when a
 * class has multiple caches. That is contrary to a "custom" solution using `Mutex` where one
 * mutex could protect multiple caches at the cost of higher lock contention.
 *
 * To make sure the cache is up to date, call `CacheMutex::ensure` and pass in the function that
 * computes the cache.
 *
 * To tell the #CacheMutex that the cache is invalidated and to be re-evaluated upon next access
 * use `CacheMutex::tag_dirty`.
 *
 * This example shows how one could implement a lazily computed average vertex position in an
 * imaginary `Mesh` data structure:
 *
 * \code{.cpp}
 * class Mesh {
 *  private:
 *   mutable CacheMutex average_position_cache_mutex_;
 *   mutable float3 average_position_cache_;
 *
 *  public:
 *   const float3 &average_position() const
 *   {
 *     average_position_cache_mutex_.ensure([&]() {
 *       average_position_cache_ = actually_compute_average_position();
 *     });
 *     return average_position_cache_;
 *   }
 *
 *   void tag_positions_changed()
 *   {
 *     average_position_cache_mutex_.tag_dirty();
 *   }
 * };
 * \endcode
 *
 * Possible future improvements:
 * - Avoid task isolation when we know that the cache computation does not use threading.
 * - Try to use a smaller mutex. The mutex does not have to be fair for this use case.
 * - Try to join the cache computation instead of blocking if another thread is computing the cache
 *   already.
 */

#include <atomic>

#include "BLI_function_ref.hh"
#include "BLI_mutex.hh"

namespace blender {

class CacheMutex {
 private:
  Mutex mutex_;
  std::atomic<bool> cache_valid_ = false;

 public:
  /**
   * Make sure the cache exists and is up to date. This calls `compute_cache` once to update the
   * cache (which is stored outside of this class) if it is dirty, otherwise it does nothing.
   *
   * This function is thread-safe under the assumption that the same parameters are passed from
   * every thread.
   */
  void ensure(const FunctionRef<void()> compute_cache)
  {
    /* Handle fast case when the cache is up-to-date. */
    if (cache_valid_.load(std::memory_order_acquire)) {
      return;
    }
    this->ensure_impl(compute_cache);
  }

  /**
   * Reset the cache. The next time #ensure is called, it will recompute that code.
   */
  void tag_dirty()
  {
    cache_valid_.store(false);
  }

  /**
   * Return true if the cache currently does not exist or has been invalidated.
   */
  bool is_dirty() const
  {
    return !this->is_cached();
  }

  /**
   * Return true if the cache exists and is valid.
   */
  bool is_cached() const
  {
    return cache_valid_.load(std::memory_order_relaxed);
  }

 private:
  void ensure_impl(FunctionRef<void()> compute_cache);
};

}  // namespace blender
