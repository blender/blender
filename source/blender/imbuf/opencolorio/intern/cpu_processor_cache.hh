/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <functional>
#include <memory>
#include <mutex>

#include "BLI_mutex.hh"

#include "OCIO_cpu_processor.hh"

namespace blender::ocio {

class CPUProcessorCache {
  /* TODO(sergey): Figure out how this can be made per-cache.
   *
   * The issue here is that the cache might be part of an object which is used in a Vector(). Or,
   * even simpler: Vector<CPUProcessorCache>.
   *
   * If the mutex is per-object then this doesn't work as the mutex deletes the move constructor.
   */
  static inline Mutex mutex_;

  mutable bool processor_created_ = false;
  mutable std::unique_ptr<const CPUProcessor> cpu_processor_;

 public:
  CPUProcessorCache() = default;
  CPUProcessorCache(const CPUProcessorCache &other) = delete;
  CPUProcessorCache(CPUProcessorCache &&other) noexcept = default;

  ~CPUProcessorCache() = default;

  CPUProcessorCache &operator=(const CPUProcessorCache &other) = delete;
  CPUProcessorCache &operator=(CPUProcessorCache &&other) = default;

  /**
   * Get cached processor, or create the new one using create_processor() and cache it.
   *
   * If the create_processor() returns nullptr it is cached as nullptr.
   */
  const CPUProcessor *get(
      const std::function<std::unique_ptr<CPUProcessor>()> &create_processor) const
  {
    std::lock_guard lock(mutex_);

    if (!processor_created_) {
      cpu_processor_ = create_processor();
      processor_created_ = true;
    }

    return cpu_processor_.get();
  }
};

}  // namespace blender::ocio
