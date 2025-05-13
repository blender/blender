/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#ifdef WITH_TBB
/* Quiet top level deprecation message, unrelated to API usage here. */
#  if defined(WIN32) && !defined(NOMINMAX)
/* TBB includes Windows.h which will define min/max macros causing issues
 * when we try to use std::min and std::max later on. */
#    define NOMINMAX
#    define TBB_MIN_MAX_CLEANUP
#  endif
#  include <tbb/concurrent_hash_map.h>
#  ifdef WIN32
/* We cannot keep this defined, since other parts of the code deal with this on their own, leading
 * to multiple define warnings unless we un-define this, however we can only undefine this if we
 * were the ones that made the definition earlier. */
#    ifdef TBB_MIN_MAX_CLEANUP
#      undef NOMINMAX
#    endif
#  endif
#else
#  include "BLI_mutex.hh"
#  include "BLI_set.hh"
#endif

#include "BLI_hash.hh"
#include "BLI_hash_tables.hh"

namespace blender {

/**
 * A #ConcurrentMap allows adding, removing and looking up values from multiple threads
 * concurrently. It has higher memory and performance overhead than a simple #Map when not used
 * concurrently though.
 *
 * For thread-safety, one always has the use an accessor to retrieve or update values. This makes
 * sure that only one thread can modify a value at a time. Multiple threads may read from the same
 * key at the same time though.
 *
 * \note #ConcurrentMap does not support iteration over all values.
 *
 * This is a thin wrapper around #tbb::concurrent_hash_map that also has a fallback implementation
 * if TBB is not available. The fallback implementation is not optimized for performance. It mainly
 * intends to be a simple implementation that can compile whenever the TBB variant can compile.
 */
template<typename Key,
         typename Value,
         typename Hash = DefaultHash<Key>,
         typename IsEqual = DefaultEquality<Key>>
class ConcurrentMap {
 public:
  using size_type = int64_t;

  /* Sometimes TBB requires the value to be constructible. */
  static_assert(std::is_copy_constructible_v<Value>);

#ifdef WITH_TBB
 private:
  struct Hasher {
    template<typename T> size_t hash(const T &value) const
    {
      return Hash{}(value);
    }

    template<typename T1, typename T2> bool equal(const T1 &a, const T2 &b) const
    {
      return IsEqual{}(a, b);
    }
  };

  using TBBMap = tbb::concurrent_hash_map<Key, Value, Hasher>;
  TBBMap map_;

 public:
  using MutableAccessor = typename TBBMap::accessor;
  using ConstAccessor = typename TBBMap::const_accessor;

  /**
   * Try to find the key-value-pair for the given key and get write-access to it. Only one thread
   * may have write access to it at a time. The looked up value can be accessed through the
   * accessor.
   *
   * \return True if the lookup was successful.
   */
  bool lookup(MutableAccessor &accessor, const Key &key)
  {
    return map_.find(accessor, key);
  }

  /**
   * Same as above, but only retrieves read-access which multiple threads can have at the same
   * time.
   */
  bool lookup(ConstAccessor &accessor, const Key &key)
  {
    return map_.find(accessor, key);
  }

  /**
   * Add the key to the map if it does not exist yet. The value is default initialized and can be
   * updated through the accessor.
   */
  bool add(MutableAccessor &accessor, const Key &key)
  {
    return map_.insert(accessor, key);
  }

  bool add(ConstAccessor &accessor, const Key &key)
  {
    return map_.insert(accessor, key);
  }

  /**
   * Remove the key-value-pair that corresponds to this key. This waits until no one else is using
   * it anymore.
   */
  bool remove(const Key &key)
  {
    return map_.erase(key);
  }

#else
 private:
  /**
   * In the fallback implementation, we actually use a #Set, because the API expects the key and
   * value to be stored in a `std::pair`. #Set can support this use case too.
   */
  struct SetKey {
    std::pair<Key, Value> item;

    SetKey(Key key) : item(std::move(key), Value()) {}

    uint64_t hash() const
    {
      return Hash{}(this->item.first);
    }

    static uint64_t hash_as(const Key &key)
    {
      return Hash{}(key);
    }

    friend bool operator==(const SetKey &a, const SetKey &b)
    {
      return IsEqual{}(a.item.first, b.item.first);
    }

    friend bool operator==(const Key &a, const SetKey &b)
    {
      return IsEqual{}(a, b.item.first);
    }

    friend bool operator==(const SetKey &a, const Key &b)
    {
      return IsEqual{}(a.item.first, b);
    }
  };

  using UsedSet = Set<SetKey>;

  struct Accessor {
    std::unique_lock<Mutex> mutex;
    std::pair<Key, Value> *data = nullptr;

    std::pair<Key, Value> *operator->()
    {
      return this->data;
    }
  };

  Mutex mutex_;
  UsedSet set_;

 public:
  using MutableAccessor = Accessor;
  using ConstAccessor = Accessor;

  bool lookup(Accessor &accessor, const Key &key)
  {
    accessor.mutex = std::unique_lock(mutex_);
    SetKey *stored_key = const_cast<SetKey *>(set_.lookup_key_ptr_as(key));
    if (!stored_key) {
      return false;
    }
    accessor.data = &stored_key->item;
    return true;
  }

  bool add(Accessor &accessor, const Key &key)
  {
    accessor.mutex = std::unique_lock(mutex_);
    const bool newly_added = !set_.contains_as(key);
    SetKey &stored_key = const_cast<SetKey &>(set_.lookup_key_or_add_as(key));
    accessor.data = &stored_key.item;
    return newly_added;
  }

  bool remove(const Key &key)
  {
    std::unique_lock lock(mutex_);
    return set_.remove_as(key);
  }

#endif
};

}  // namespace blender
