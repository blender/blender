/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#ifdef WITH_TBB
#  include <tbb/concurrent_unordered_set.h>
#else
#  include <memory>
#  include <utility>

#  include "BLI_mutex.hh"
#  include "BLI_set.hh"
#endif

#include "BLI_hash.hh"
#include "BLI_hash_tables.hh"
#include "BLI_utility_mixins.hh"

namespace blender {

/**
 * A #ConcurrentSet allows adding and looking up keys from multiple threads concurrently. Once a
 * key has been added, a pointer to it remains valid for as long as the set exists (keys are never
 * relocated by growth).
 *
 * This is a thin wrapper around #tbb::concurrent_unordered_set that also has a fallback
 * implementation if TBB is not available. The fallback implementation is not optimized.
 */
template<typename Key, typename Hash = DefaultHash<Key>, typename IsEqual = DefaultEquality<Key>>
class ConcurrentSet : NonCopyable {
#ifdef WITH_TBB
 private:
  /**
   * Adds the #is_transparent marker type that TBB requires to enable heterogeneous lookups (e.g.
   * #find with a type other than #Key), on top of the (already heterogeneous) #IsEqual.
   */
  struct TransparentIsEqual : public IsEqual {
    using is_transparent = void;
  };

  struct Hasher : public Hash {
    using transparent_key_equal = TransparentIsEqual;
  };

  using TBBSet = tbb::concurrent_unordered_set<Key, Hasher>;
  TBBSet set_;

 public:
  /**
   * Look up a key without adding it. Returns null if the key does not exist yet. Does not
   * require creating a #Key first, unlike #lookup_key_or_add.
   */
  template<typename ForwardKey> const Key *lookup_ptr(const ForwardKey &key) const
  {
    typename TBBSet::const_iterator it = set_.find(key);
    return (it == set_.end()) ? nullptr : &*it;
  }

  /**
   * Add the key to the set if it does not exist yet. Either way, a reference to the key that is
   * now stored in the set is returned (which might not be the passed in key, if another thread
   * added an equal key first).
   */
  const Key &lookup_key_or_add(const Key &key)
  {
    return this->lookup_key_or_add_as(key);
  }
  const Key &lookup_key_or_add(Key &&key)
  {
    return this->lookup_key_or_add_as(std::move(key));
  }
  template<typename ForwardKey> const Key &lookup_key_or_add_as(ForwardKey &&key)
  {
    /* Use #emplace instead of #insert, because of a TBB bug that can cause duplicate values to be
     * added to the set via #insert. */
    return *set_.emplace(std::forward<ForwardKey>(key)).first;
  }

  /** Returns the number of keys currently in the set. */
  int64_t size() const
  {
    return int64_t(set_.size());
  }

  /** Potentially grow the set so that it can hold at least #n keys without rehashing again. */
  void reserve(const int64_t n)
  {
    set_.reserve(typename TBBSet::size_type(n));
  }

#else
 private:
  /**
   * The fallback #Set does not guarantee that pointers to its keys remain valid when it grows, so
   * keys are heap-allocated individually and only the (small, freely relocatable) pointers are
   * stored in the set.
   */
  template<typename ForwardKey> struct Unwrap {
    static const ForwardKey &get(const ForwardKey &key)
    {
      return key;
    }
  };
  template<typename T> struct Unwrap<std::unique_ptr<T>> {
    static const T &get(const std::unique_ptr<T> &key)
    {
      return *key;
    }
  };

  struct PtrHash {
    template<typename ForwardKey> uint64_t operator()(const ForwardKey &key) const
    {
      return Hash{}(Unwrap<ForwardKey>::get(key));
    }
  };
  struct PtrIsEqual {
    template<typename ForwardKeyA, typename ForwardKeyB>
    bool operator()(const ForwardKeyA &a, const ForwardKeyB &b) const
    {
      return IsEqual{}(Unwrap<ForwardKeyA>::get(a), Unwrap<ForwardKeyB>::get(b));
    }
  };

  using UsedSet = Set<std::unique_ptr<Key>,
                      default_inline_buffer_capacity(sizeof(std::unique_ptr<Key>)),
                      DefaultProbingStrategy,
                      PtrHash,
                      PtrIsEqual>;

  /* Wrapped in a unique_ptr to allow ConcurrentSet to be moveable. */
  mutable std::unique_ptr<Mutex> mutex_ = std::make_unique<Mutex>();
  UsedSet set_;

 public:
  ConcurrentSet() = default;
  ConcurrentSet(ConcurrentSet &&other) noexcept
      : mutex_(std::exchange(other.mutex_, std::make_unique<Mutex>())), set_(std::move(other.set_))
  {
  }
  ConcurrentSet &operator=(ConcurrentSet &&other) noexcept
  {
    std::destroy_at(this);
    new (this) ConcurrentSet(std::move(other));
    return *this;
  }

  /**
   * Look up a key without adding it. Returns null if the key does not exist yet. Does not
   * require creating a #Key first, unlike #lookup_key_or_add.
   */
  template<typename ForwardKey> const Key *lookup_ptr(const ForwardKey &key) const
  {
    std::lock_guard lock{*mutex_};
    const std::unique_ptr<Key> *stored = set_.lookup_key_ptr_as(key);
    return stored ? stored->get() : nullptr;
  }

  /**
   * Add the key to the set if it does not exist yet. Either way, a reference to the key that is
   * now stored in the set is returned (which might not be the passed in key, if another thread
   * added an equal key first).
   */
  const Key &lookup_key_or_add(const Key &key)
  {
    return this->lookup_key_or_add_as(key);
  }
  const Key &lookup_key_or_add(Key &&key)
  {
    return this->lookup_key_or_add_as(std::move(key));
  }
  template<typename ForwardKey> const Key &lookup_key_or_add_as(ForwardKey &&key)
  {
    std::lock_guard lock{*mutex_};
    return *set_.lookup_key_or_add_as(std::make_unique<Key>(std::forward<ForwardKey>(key)));
  }

  /** Returns the number of keys currently in the set. */
  int64_t size() const
  {
    std::lock_guard lock{*mutex_};
    return set_.size();
  }

  /** Potentially grow the set so that it can hold at least #n keys without rehashing again. */
  void reserve(const int64_t n)
  {
    std::lock_guard lock{*mutex_};
    set_.reserve(n);
  }

#endif
};

}  // namespace blender
