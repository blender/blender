/* SPDX-FileCopyrightText: 2012-2021 Meta Platforms, Inc. and affiliates.
 * SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Simplified version of Folly's AtomicHashArray
 * (https://github.com/facebook/folly/blob/main/folly/AtomicHashArray.h).
 *
 * Notable changes:
 * - Standalone and header-only.
 * - Behaves like a set, not like a map: There's no value type anymore, only keys.
 * - Capacity check logic have been removed, the code assumes you know the required size in
 * advance.
 * - Custom allocator support has been removed.
 * - Erase has been removed.
 * - Find has been removed.
 */

/** \file
 * \ingroup mikktspace
 */

#pragma once

#ifdef _MSC_VER
#  include <intrin.h>
#endif

#include <atomic>
#include <type_traits>

namespace mikk {

struct AtomicHashSetLinearProbeFcn {
  inline size_t operator()(size_t idx, size_t /* numProbes */, size_t capacity) const
  {
    idx += 1;  // linear probing

    // Avoid modulus because it's slow
    return LIKELY(idx < capacity) ? idx : (idx - capacity);
  }
};

struct AtomicHashSetQuadraticProbeFcn {
  inline size_t operator()(size_t idx, size_t numProbes, size_t capacity) const
  {
    idx += numProbes;  // quadratic probing

    // Avoid modulus because it's slow
    return LIKELY(idx < capacity) ? idx : (idx - capacity);
  }
};

template<class KeyT,
         bool isAtomic,
         class KeyHash = std::hash<KeyT>,
         class KeyEqual = std::equal_to<KeyT>,
         class ProbeFcn = AtomicHashSetLinearProbeFcn>
class AtomicHashSet {
  static_assert((std::is_convertible<KeyT, int32_t>::value ||
                 std::is_convertible<KeyT, int64_t>::value ||
                 std::is_convertible<KeyT, const void *>::value),
                "You are trying to use AtomicHashSet with disallowed key "
                "types.  You must use atomically compare-and-swappable integer "
                "keys, or a different container class.");

 public:
  const size_t capacity_;
  const KeyT kEmptyKey_;

  KeyHash hasher_;
  KeyEqual equalityChecker_;

 private:
  size_t kAnchorMask_;
  /* When using a single thread, we can avoid overhead by not bothering with atomic cells. */
  typedef typename std::conditional<isAtomic, std::atomic<KeyT>, KeyT>::type cell_type;
  std::vector<cell_type> cells_;

 public:
  struct Config {
    KeyT emptyKey;
    double maxLoadFactor;
    double growthFactor;
    size_t capacity;  // if positive, overrides maxLoadFactor

    //  Cannot have constexpr ctor because some compilers rightly complain.
    Config() : emptyKey((KeyT)-1), maxLoadFactor(0.8), growthFactor(-1), capacity(0) {}
  };

  /* Instead of a mess of arguments, we take a max size and a Config struct to
   * simulate named ctor parameters.  The Config struct has sensible defaults
   * for everything, but is overloaded - if you specify a positive capacity,
   * that will be used directly instead of computing it based on maxLoadFactor.
   */
  AtomicHashSet(size_t maxSize,
                KeyHash hasher = KeyHash(),
                KeyEqual equalityChecker = KeyEqual(),
                const Config &c = Config())
      : capacity_(size_t(double(maxSize) / c.maxLoadFactor) + 1),
        kEmptyKey_(c.emptyKey),
        hasher_(hasher),
        equalityChecker_(equalityChecker),
        cells_(capacity_)
  {
    /* Get next power of two. Could be done more effiently with builtin_clz, but this is not
     * performance-critical. */
    kAnchorMask_ = 1;
    while (kAnchorMask_ < capacity_)
      kAnchorMask_ *= 2;
    /* Get mask for lower bits. */
    kAnchorMask_ -= 1;

    /* Not great, but the best we can do to support both atomic and non-atomic cells
     * since std::atomic doesn't have a copy constructor so cells_(capacity_, kEmptyKey_)
     * in the initializer list won't work. */
    std::fill((KeyT *)cells_.data(), (KeyT *)cells_.data() + capacity_, kEmptyKey_);
  }

  AtomicHashSet(const AtomicHashSet &) = delete;
  AtomicHashSet &operator=(const AtomicHashSet &) = delete;

  ~AtomicHashSet() = default;

  /* Sequential specialization. */
  bool tryUpdateCell(KeyT *cell, KeyT &existingKey, KeyT newKey)
  {
    if (*cell == existingKey) {
      *cell = newKey;
      return true;
    }
    existingKey = *cell;
    return false;
  }

  /* Atomic specialization. */
  bool tryUpdateCell(std::atomic<KeyT> *cell, KeyT &existingKey, KeyT newKey)
  {
    return cell->compare_exchange_strong(existingKey, newKey, std::memory_order_acq_rel);
  }

  std::pair<KeyT, bool> emplace(KeyT key)
  {
    size_t idx = keyToAnchorIdx(key);
    size_t numProbes = 0;
    for (;;) {
      cell_type *cell = &cells_[idx];
      KeyT existingKey = kEmptyKey_;
      /* Try to replace empty cell with our key. */
      if (tryUpdateCell(cell, existingKey, key)) {
        /* Cell was empty, we're done. */
        return std::make_pair(key, true);
      }

      /* Cell was not empty, check if the existing key is equal. */
      if (equalityChecker_(existingKey, key)) {
        /* Found equal element, we're done. */
        return std::make_pair(existingKey, false);
      }

      /* Continue to next cell according to probe strategy. */
      ++numProbes;
      if (UNLIKELY(numProbes >= capacity_)) {
        // probed every cell...fail
        assert(false);
        return std::make_pair(kEmptyKey_, false);
      }

      idx = ProbeFcn()(idx, numProbes, capacity_);
    }
  }

 private:
  inline size_t keyToAnchorIdx(const KeyT k) const
  {
    const size_t hashVal = hasher_(k);
    const size_t probe = hashVal & kAnchorMask_;
    return LIKELY(probe < capacity_) ? probe : hashVal % capacity_;
  }

};  // AtomicHashSet

}  // namespace mikk
