/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_function_ref.hh"
#include "BLI_generic_key.hh"
#include "BLI_memory_counter_fwd.hh"

namespace blender::memory_cache {

/**
 * A value that is stored in the cache. It may be freed automatically when the cache is full. This
 * is expected to be subclassed by users of the memory cache.
 */
class CachedValue {
 public:
  virtual ~CachedValue() = default;

  /**
   * Gather the memory used by this value. This allows the cache system to determine when it is
   * full.
   */
  virtual void count_memory(MemoryCounter &memory) const = 0;
};

/**
 * Returns the value that corresponds to the given key. If it's not cached yet, #compute_fn is
 * called and its result is cached for the next time.
 *
 * If the cache is full, older values may be freed.
 */
template<typename T>
std::shared_ptr<const T> get(const GenericKey &key, FunctionRef<std::unique_ptr<T>()> compute_fn);

/**
 * A non-templated version of the main entry point above.
 */
std::shared_ptr<CachedValue> get_base(const GenericKey &key,
                                      FunctionRef<std::unique_ptr<CachedValue>()> compute_fn);

/**
 * Set how much memory the cache is allowed to use. This is only an approximation because counting
 * the memory is not 100% accurate, and for some types the memory usage may even change over time.
 */
void set_approximate_size_limit(int64_t limit_in_bytes);

/**
 * Remove all elements from the cache. Note that this does not guarantee that no elements are in
 * the cache after the function returned. This is because another thread may have added a new
 * element right after the clearing.
 */
void clear();

/* -------------------------------------------------------------------- */
/** \name Inline Functions
 * \{ */

template<typename T>
inline std::shared_ptr<const T> get(const GenericKey &key,
                                    FunctionRef<std::unique_ptr<T>()> compute_fn)
{
  return std::dynamic_pointer_cast<const T>(get_base(key, compute_fn));
}

/** \} */

}  // namespace blender::memory_cache
