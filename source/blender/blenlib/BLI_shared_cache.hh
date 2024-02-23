/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_cache_mutex.hh"

namespace blender {

/**
 * A `SharedCache` is meant to share lazily computed data between equivalent objects. It allows
 * saving unnecessary computation by making a calculated value accessible from any object that
 * shares the cache. Unlike `CacheMutex`, the cached data is embedded inside of this object.
 *
 * When data is copied (copy-on-evaluation before changing a mesh, for example), the cache is
 * shared, allowing its calculation on either the source or original to make the result available
 * on both objects. As soon as either object is changed in a way that invalidates the cache, the
 * data is "un-shared", and they will no-longer influence each other.
 *
 * One important use case is a typical copy-on-evaluation update loop of a persistent geometry
 * data-block in `Main`. Even if bounds are only calculated on the evaluated *copied* geometry, if
 * nothing changes them, they only need to be calculated on the first evaluation, because the same
 * evaluated bounds are also accessible from the original geometry.
 *
 * The cache is implemented with a shared pointer, so it is relatively cheap, but to avoid
 * unnecessary overhead it should only be used for relatively expensive computations.
 */
template<typename T> class SharedCache {
  struct CacheData {
    CacheMutex mutex;
    T data;
    CacheData() = default;
    CacheData(const T &data) : data(data) {}
  };
  std::shared_ptr<CacheData> cache_;

 public:
  SharedCache()
  {
    /* The cache should be allocated to trigger sharing of the cached data as early as possible. */
    cache_ = std::make_shared<CacheData>();
  }

  /** Tag the data for recomputation and stop sharing the cache with other objects. */
  void tag_dirty()
  {
    if (cache_.unique()) {
      cache_->mutex.tag_dirty();
    }
    else {
      cache_ = std::make_shared<CacheData>();
    }
  }

  /**
   * If the cache is dirty, trigger its computation with the provided function which should set
   * the proper data.
   */
  void ensure(FunctionRef<void(T &data)> compute_cache)
  {
    cache_->mutex.ensure([&]() { compute_cache(this->cache_->data); });
  }

  /**
   * Represents a combination of "tag dirty" and "update cache for new data." Existing cached
   * values are kept available (copied from shared data if necessary). This can be helpful when
   * the recalculation is only expected to make a small change to the cached data, since using
   * #tag_dirty() and #ensure() separately may require rebuilding the cache from scratch.
   */
  void update(FunctionRef<void(T &data)> compute_cache)
  {
    if (cache_.unique()) {
      cache_->mutex.tag_dirty();
    }
    else {
      cache_ = std::make_shared<CacheData>(cache_->data);
    }
    cache_->mutex.ensure([&]() { compute_cache(this->cache_->data); });
  }

  /** Retrieve the cached data. */
  const T &data() const
  {
    BLI_assert(cache_->mutex.is_cached());
    return cache_->data;
  }

  /**
   * Return true if the cache currently does not exist or has been invalidated.
   */
  bool is_dirty() const
  {
    return cache_->mutex.is_dirty();
  }

  /**
   * Return true if the cache exists and is valid.
   */
  bool is_cached() const
  {
    return cache_->mutex.is_cached();
  }
};

}  // namespace blender
