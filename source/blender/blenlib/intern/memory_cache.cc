/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <atomic>
#include <mutex>
#include <optional>

#include "BLI_concurrent_map.hh"
#include "BLI_memory_cache.hh"
#include "BLI_memory_counter.hh"

namespace blender::memory_cache {

struct StoredValue {
  /**
   * The corresponding key. It's stored here, because only a reference to it is used as key in the
   * hash table.
   *
   * This is a shared_ptr instead of unique_ptr so that the entire struct is copy constructible.
   */
  std::shared_ptr<const GenericKey> key;
  /** The user-provided value. */
  std::shared_ptr<CachedValue> value;
  /** A logical time that indicates when the value was last used. Lower values are older. */
  int64_t last_use_time = 0;
};

using CacheMap = ConcurrentMap<std::reference_wrapper<const GenericKey>, StoredValue>;

struct Cache {
  CacheMap map;

  std::atomic<int64_t> logical_time = 0;
  std::atomic<int64_t> approximate_limit = 1024 * 1024 * 1024;
  /**
   * This is derived from `memory` below, but is atomic for safe access when the global mutex is
   * not locked.
   */
  std::atomic<int64_t> size_in_bytes = 0;

  std::mutex global_mutex;
  /** Amount of memory currently used in the cache. */
  MemoryCount memory;
  /**
   * Keys currently cached. This is stored separately from the map, because the map does not allow
   * thread-safe iteration.
   */
  Vector<const GenericKey *> keys;
};

static Cache &get_cache()
{
  static Cache cache;
  return cache;
}

static void try_enforce_limit();

static void set_new_logical_time(const StoredValue &stored_value, const int64_t new_time)
{
  /* Don't want to use `std::atomic` directly in the struct, because that makes it
   * non-movable. Could also use a non-const accessor, but that may degrade performance more.
   * It's not necessary for correctness that the time is exactly the right value. */
  reinterpret_cast<std::atomic<int64_t> *>(const_cast<int64_t *>(&stored_value.last_use_time))
      ->store(new_time, std::memory_order_relaxed);
  static_assert(sizeof(int64_t) == sizeof(std::atomic<int64_t>));
}

std::shared_ptr<CachedValue> get_base(const GenericKey &key,
                                      const FunctionRef<std::unique_ptr<CachedValue>()> compute_fn)
{
  Cache &cache = get_cache();
  /* "Touch" the cached value so that we know that it is still used. This makes it less likely that
   * it is removed. */
  const int64_t new_time = cache.logical_time.fetch_add(1, std::memory_order_relaxed);
  {
    /* Fast path when the value is already cached. */
    CacheMap::ConstAccessor accessor;
    if (cache.map.lookup(accessor, std::ref(key))) {
      set_new_logical_time(accessor->second, new_time);
      return accessor->second.value;
    }
  }

  /* Compute value while no locks are held to avoid potential for dead-locks. Not using a lock also
   * means that the value may be computed more than once, but that's still better than locking all
   * the time. It may be possible to implement something smarter in the future. */
  std::shared_ptr<CachedValue> result = compute_fn();
  /* Result should be valid. Use exception to propagate error if necessary. */
  BLI_assert(result);

  {
    CacheMap::MutableAccessor accessor;
    const bool newly_inserted = cache.map.add(accessor, std::ref(key));
    if (!newly_inserted) {
      /* The value is available already. It was computed unnecessarily. Use the value created by
       * the other thread instead. */
      return accessor->second.value;
    }
    /* We want to store the key in the map, but the reference we got passed in may go out of scope.
     * So make a storable copy of it that we use in the map. */
    accessor->second.key = key.to_storable();
    /* Modifying the key should be fine because the new key is equal to the original key. */
    const_cast<std::reference_wrapper<const GenericKey> &>(accessor->first) = std::ref(
        *accessor->second.key);

    /* Store the value. Don't move, because we still want to return the value from the function. */
    accessor->second.value = result;
    /* Set initial logical time for the new cached entry. */
    set_new_logical_time(accessor->second, new_time);

    {
      /* Update global data of the cache. */
      std::lock_guard lock{cache.global_mutex};
      memory_counter::MemoryCounter memory_counter{cache.memory};
      accessor->second.value->count_memory(memory_counter);
      cache.keys.append(&accessor->first.get());
      cache.size_in_bytes = cache.memory.total_bytes;
    }
  }
  /* Potentially free elements from the cache. Note, even if this would free the value we just
   * added, it would still work correctly, because we already have a shared_ptr to it. */
  try_enforce_limit();
  return result;
}

void set_approximate_size_limit(const int64_t limit_in_bytes)
{
  Cache &cache = get_cache();
  cache.approximate_limit = limit_in_bytes;
  try_enforce_limit();
}

void clear()
{
  memory_cache::remove_if([](const GenericKey &) { return true; });
}

void remove_if(const FunctionRef<bool(const GenericKey &)> predicate)
{
  Cache &cache = get_cache();
  std::lock_guard lock{cache.global_mutex};

  /* Store predicate results to avoid assuming that the predicate is cheap and without side effects
   * that must not happen more than once. */
  Array<bool> predicate_results(cache.keys.size());

  /* Recount memory of all elements that are not removed. */
  cache.memory.reset();
  MemoryCounter memory_counter{cache.memory};

  for (const int64_t i : cache.keys.index_range()) {
    const GenericKey &key = *cache.keys[i];
    const bool ok_to_remove = predicate(key);
    predicate_results[i] = ok_to_remove;

    if (!ok_to_remove) {
      /* The value is kept, so count its memory. */
      CacheMap::ConstAccessor accessor;
      if (cache.map.lookup(accessor, key)) {
        accessor->second.value->count_memory(memory_counter);
        continue;
      }
      BLI_assert_unreachable();
    }
    /* The value should be removed. */
    const bool success = cache.map.remove(key);
    BLI_assert(success);
    UNUSED_VARS_NDEBUG(success);
  }
  /* Remove all removed keys from the vector too. */
  cache.keys.remove_if([&](const GenericKey *&key) {
    const int64_t index = &key - cache.keys.data();
    return predicate_results[index];
  });
  cache.size_in_bytes = cache.memory.total_bytes;
}

static void try_enforce_limit()
{
  Cache &cache = get_cache();
  const int64_t old_size = cache.size_in_bytes.load(std::memory_order_relaxed);
  const int64_t approximate_limit = cache.approximate_limit.load(std::memory_order_relaxed);
  if (old_size < approximate_limit) {
    /* Nothing to do, the current cache size is still within the right limits. */
    return;
  }

  std::lock_guard lock{cache.global_mutex};

  /* Gather all the keys with their latest usage times. */
  Vector<std::pair<int64_t, const GenericKey *>> keys_with_time;
  for (const GenericKey *key : cache.keys) {
    CacheMap::ConstAccessor accessor;
    if (!cache.map.lookup(accessor, *key)) {
      continue;
    }
    keys_with_time.append({accessor->second.last_use_time, key});
  }
  /* Sort the items so that the newest keys come first. */
  std::sort(keys_with_time.begin(), keys_with_time.end());
  std::reverse(keys_with_time.begin(), keys_with_time.end());

  /* Count used memory starting at the most recently touched element. Stop at the element when the
   * amount became larger than the capacity. */
  cache.memory.reset();
  std::optional<int> first_bad_index;
  {
    MemoryCounter memory_counter{cache.memory};
    for (const int i : keys_with_time.index_range()) {
      const GenericKey &key = *keys_with_time[i].second;
      CacheMap::ConstAccessor accessor;
      if (!cache.map.lookup(accessor, key)) {
        continue;
      }
      accessor->second.value->count_memory(memory_counter);
      /* Undershoot a little bit. This typically results in more things being freed that have not
       * been used in a while. The benefit is that we have to do the decision what to free less
       * often than if we were always just freeing the minimum amount necessary. */
      if (cache.memory.total_bytes <= approximate_limit * 0.75) {
        continue;
      }
      first_bad_index = i;
      break;
    }
  }
  if (!first_bad_index) {
    return;
  }

  /* Avoid recounting memory if the last item is not way too large and the overshoot is still ok.
   * The alternative would be to subtract the last item from the counted memory again, but removing
   * from #MemoryCount is not implemented yet. */
  bool need_memory_recount = false;
  if (cache.memory.total_bytes < approximate_limit * 1.1) {
    *first_bad_index += 1;
    if (*first_bad_index == keys_with_time.size()) {
      return;
    }
  }
  else {
    need_memory_recount = true;
  }

  /* Remove elements that don't fit anymore. */
  for (const int i : keys_with_time.index_range().drop_front(*first_bad_index)) {
    const GenericKey &key = *keys_with_time[i].second;
    cache.map.remove(key);
  }

  /* Update keys vector. */
  cache.keys.clear();
  for (const int i : keys_with_time.index_range().take_front(*first_bad_index)) {
    cache.keys.append(keys_with_time[i].second);
  }

  if (need_memory_recount) {
    /* Recount memory another time, because the last count does not accurately represent the actual
     * value. */
    cache.memory.reset();
    MemoryCounter memory_counter{cache.memory};
    for (const int i : keys_with_time.index_range().take_front(*first_bad_index)) {
      const GenericKey &key = *keys_with_time[i].second;
      CacheMap::ConstAccessor accessor;
      if (!cache.map.lookup(accessor, key)) {
        continue;
      }
      accessor->second.value->count_memory(memory_counter);
    }
  }
  cache.size_in_bytes = cache.memory.total_bytes;
}

}  // namespace blender::memory_cache
