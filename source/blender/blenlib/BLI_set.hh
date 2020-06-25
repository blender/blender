/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __BLI_SET_HH__
#define __BLI_SET_HH__

/** \file
 * \ingroup bli
 *
 * A `blender::Set<Key>` is an unordered container for unique elements of type `Key`. It is
 * designed to be a more convenient and efficient replacement for `std::unordered_set`. All core
 * operations (add, remove and contains) can be done in O(1) amortized expected time.
 *
 * In most cases, your default choice for a hash set in Blender should be `blender::Set`.
 *
 * blender::Set is implemented using open addressing in a slot array with a power-of-two size.
 * Every slot is in one of three states: empty, occupied or removed. If a slot is occupied, it
 * contains an instance of the key type.
 *
 * Bench-marking and comparing hash tables is hard, because many factors influence the result. The
 * performance of a hash table depends on the combination of the hash function, probing strategy,
 * max load factor, key type, slot type and the data distribution. This implementation is designed
 * to be relatively fast by default in all cases. However, it also offers many customization
 * points that allow it to be optimized for a specific use case.
 *
 * A rudimentary benchmark can be found in BLI_set_test.cc. The results of that benchmark are
 * there as well. The numbers show that in this specific case blender::Set outperforms
 * std::unordered_set consistently by a good amount.
 *
 * Some noteworthy information:
 * - Key must be a movable type.
 * - Pointers to keys might be invalidated when the set is changed or moved.
 * - The hash function can be customized. See BLI_hash.hh for details.
 * - The probing strategy can be customized. See BLI_probing_stragies.hh for details.
 * - The slot type can be customized. See BLI_set_slots.hh for details.
 * - Small buffer optimization is enabled by default, if the key is not too large.
 * - The methods `add_new` and `remove_contained` should be used instead of `add` and `remove`
 *   whenever appropriate. Assumptions and intention are described better this way.
 * - Look-ups can be performed using types other than Key without conversion. For that use the
 *   methods ending with `_as`. The template parameters Hash and #IsEqual have to support the other
 *   key type. This can greatly improve performance when the set contains strings.
 * - The default constructor is cheap, even when a large #InlineBufferCapacity is used. A large
 *   slot array will only be initialized when the first key is added.
 * - The `print_stats` method can be used to get information about the distribution of keys and
 *   memory usage of the set.
 * - The method names don't follow the std::unordered_set names in many cases. Searching for such
 *   names in this file will usually let you discover the new name.
 * - There is a #StdUnorderedSetWrapper class, that wraps std::unordered_set and gives it the same
 *   interface as blender::Set. This is useful for bench-marking.
 *
 * Possible Improvements:
 * - Use a branch-less loop over slots in grow function (measured ~10% performance improvement when
 *   the distribution of occupied slots is sufficiently random).
 * - Support max load factor customization.
 * - Improve performance with large data sets through software prefetching. I got fairly
 *   significant improvements in simple tests (~30% faster). It still needs to be investigated how
 *   to make a nice interface for this functionality.
 */

#include <unordered_set>

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_hash_tables.hh"
#include "BLI_probing_strategies.hh"
#include "BLI_set_slots.hh"

namespace blender {

template<
    /** Type of the elements that are stored in this set. It has to be movable. Furthermore, the
     * hash and is-equal functions have to support it.
     */
    typename Key,
    /**
     * The minimum number of elements that can be stored in this Set without doing a heap
     * allocation. This is useful when you expect to have many small sets. However, keep in mind
     * that (unlike vector) initializing a set has a O(n) cost in the number of slots.
     *
     * When Key is large, the small buffer optimization is disabled by default to avoid large
     * unexpected allocations on the stack. It can still be enabled explicitly though.
     */
    uint32_t InlineBufferCapacity = (sizeof(Key) < 100) ? 4 : 0,
    /**
     * The strategy used to deal with collisions. They are defined in BLI_probing_strategies.hh.
     */
    typename ProbingStrategy = DefaultProbingStrategy,
    /**
     * The hash function used to hash the keys. There is a default for many types. See BLI_hash.hh
     * for examples on how to define a custom hash function.
     */
    typename Hash = DefaultHash<Key>,
    /**
     * The equality operator used to compare keys. By default it will simply compare keys using the
     * `==` operator.
     */
    typename IsEqual = DefaultEquality,
    /**
     * This is what will actually be stored in the hash table array. At a minimum a slot has to
     * be able to hold a key and information about whether the slot is empty, occupied or removed.
     * Using a non-standard slot type can improve performance or reduce the memory footprint. For
     * example, a hash can be stored in the slot, to make inequality checks more efficient. Some
     * types have special values that can represent an empty or removed state, eliminating the need
     * for an additional variable. Also see BLI_set_slots.hh.
     */
    typename Slot = typename DefaultSetSlot<Key>::type,
    /**
     * The allocator used by this set. Should rarely be changed, except when you don't want that
     * MEM_* is used internally.
     */
    typename Allocator = GuardedAllocator>
class Set {
 private:
  /**
   * Slots are either empty, occupied or removed. The number of occupied slots can be computed by
   * subtracting the removed slots from the occupied-and-removed slots.
   */
  uint32_t m_removed_slots;
  uint32_t m_occupied_and_removed_slots;

  /**
   * The maximum number of slots that can be used (either occupied or removed) until the set has to
   * grow. This is the total number of slots times the max load factor.
   */
  uint32_t m_usable_slots;

  /**
   * The number of slots minus one. This is a bit mask that can be used to turn any integer into a
   * valid slot index efficiently.
   */
  uint32_t m_slot_mask;

  /** This is called to hash incoming keys. */
  Hash m_hash;

  /** This is called to check equality of two keys. */
  IsEqual m_is_equal;

  /** The max load factor is 1/2 = 50% by default. */
#define LOAD_FACTOR 1, 2
  LoadFactor m_max_load_factor = LoadFactor(LOAD_FACTOR);
  using SlotArray =
      Array<Slot, LoadFactor::compute_total_slots(InlineBufferCapacity, LOAD_FACTOR), Allocator>;
#undef LOAD_FACTOR

  /**
   * This is the array that contains the actual slots. There is always at least one empty slot and
   * the size of the array is a power of two.
   */
  SlotArray m_slots;

  /** Iterate over a slot index sequence for a given hash. */
#define SET_SLOT_PROBING_BEGIN(HASH, R_SLOT) \
  SLOT_PROBING_BEGIN (ProbingStrategy, HASH, m_slot_mask, SLOT_INDEX) \
    auto &R_SLOT = m_slots[SLOT_INDEX];
#define SET_SLOT_PROBING_END() SLOT_PROBING_END()

 public:
  /**
   * Initialize an empty set. This is a cheap operation no matter how large the inline buffer
   * is. This is necessary to avoid a high cost when no elements are added at all. An optimized
   * grow operation is performed on the first insertion.
   */
  Set()
      : m_removed_slots(0),
        m_occupied_and_removed_slots(0),
        m_usable_slots(0),
        m_slot_mask(0),
        m_slots(1)
  {
  }

  ~Set() = default;

  /**
   * Construct a set that contains the given keys. Duplicates will be removed automatically.
   */
  Set(const std::initializer_list<Key> &list) : Set()
  {
    this->add_multiple(list);
  }

  Set(const Set &other) = default;

  Set(Set &&other) noexcept
      : m_removed_slots(other.m_removed_slots),
        m_occupied_and_removed_slots(other.m_occupied_and_removed_slots),
        m_usable_slots(other.m_usable_slots),
        m_slot_mask(other.m_slot_mask),
        m_hash(std::move(other.m_hash)),
        m_is_equal(std::move(other.m_is_equal)),
        m_slots(std::move(other.m_slots))
  {
    other.~Set();
    new (&other) Set();
  }

  Set &operator=(const Set &other)
  {
    if (this == &other) {
      return *this;
    }

    this->~Set();
    new (this) Set(other);

    return *this;
  }

  Set &operator=(Set &&other)
  {
    if (this == &other) {
      return *this;
    }

    this->~Set();
    new (this) Set(std::move(other));

    return *this;
  }

  /**
   * Add a new key to the set. This invokes undefined behavior when the key is in the set already.
   * When you know for certain that a key is not in the set yet, use this method for better
   * performance. This also expresses the intent better.
   */
  void add_new(const Key &key)
  {
    this->add_new__impl(key, m_hash(key));
  }
  void add_new(Key &&key)
  {
    this->add_new__impl(std::move(key), m_hash(key));
  }

  /**
   * Add a key to the set. If the key exists in the set already, nothing is done. The return value
   * is true if the key was newly added to the set.
   *
   * This is similar to std::unordered_set::insert.
   */
  bool add(const Key &key)
  {
    return this->add_as(key);
  }
  bool add(Key &&key)
  {
    return this->add_as(std::move(key));
  }

  /**
   * Same as `add`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey> bool add_as(ForwardKey &&key)
  {
    return this->add__impl(std::forward<ForwardKey>(key), m_hash(key));
  }

  /**
   * Convenience function to add many keys to the set at once. Duplicates are removed
   * automatically.
   *
   * We might be able to make this faster than sequentially adding all keys, but that is not
   * implemented yet.
   */
  void add_multiple(Span<Key> keys)
  {
    for (const Key &key : keys) {
      this->add(key);
    }
  }

  /**
   * Convenience function to add many new keys to the set at once. The keys must not exist in the
   * set before and there must not be duplicates in the array.
   */
  void add_multiple_new(Span<Key> keys)
  {
    for (const Key &key : keys) {
      this->add_new(key);
    }
  }

  /**
   * Returns true if the key is in the set.
   *
   * This is similar to std::unordered_set::find() != std::unordered_set::end().
   */
  bool contains(const Key &key) const
  {
    return this->contains_as(key);
  }

  /**
   * Same as `contains`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey> bool contains_as(const ForwardKey &key) const
  {
    return this->contains__impl(key, m_hash(key));
  }

  /**
   * Deletes the key from the set. Returns true when the key did exist beforehand, otherwise false.
   *
   * This is similar to std::unordered_set::erase.
   */
  bool remove(const Key &key)
  {
    return this->remove_as(key);
  }

  /**
   * Same as `remove`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey> bool remove_as(const ForwardKey &key)
  {
    return this->remove__impl(key, m_hash(key));
  }

  /**
   * Deletes the key from the set. This invokes undefined behavior when the key is not in the map.
   */
  void remove_contained(const Key &key)
  {
    this->remove_contained_as(key);
  }

  /**
   * Same as `remove_contained`, but accepts other key types that are supported by the hash
   * function.
   */
  template<typename ForwardKey> void remove_contained_as(const ForwardKey &key)
  {
    this->remove_contained__impl(key, m_hash(key));
  }

  /**
   * An iterator that can iterate over all keys in the set. The iterator is invalidated when the
   * set is moved or when it is grown.
   *
   * Keys returned by this iterator are always const. They should not change, because this might
   * also change their hash.
   */
  class Iterator {
   private:
    const Slot *m_slots;
    uint32_t m_total_slots;
    uint32_t m_current_slot;

   public:
    Iterator(const Slot *slots, uint32_t total_slots, uint32_t current_slot)
        : m_slots(slots), m_total_slots(total_slots), m_current_slot(current_slot)
    {
    }

    Iterator &operator++()
    {
      while (++m_current_slot < m_total_slots) {
        if (m_slots[m_current_slot].is_occupied()) {
          break;
        }
      }
      return *this;
    }

    const Key &operator*() const
    {
      return *m_slots[m_current_slot].key();
    }

    friend bool operator!=(const Iterator &a, const Iterator &b)
    {
      BLI_assert(a.m_slots == b.m_slots);
      BLI_assert(a.m_total_slots == b.m_total_slots);
      return a.m_current_slot != b.m_current_slot;
    }
  };

  Iterator begin() const
  {
    for (uint32_t i = 0; i < m_slots.size(); i++) {
      if (m_slots[i].is_occupied()) {
        return Iterator(m_slots.data(), m_slots.size(), i);
      }
    }
    return this->end();
  }

  Iterator end() const
  {
    return Iterator(m_slots.data(), m_slots.size(), m_slots.size());
  }

  /**
   * Print common statistics like size and collision count. This is useful for debugging purposes.
   */
  void print_stats(StringRef name = "") const
  {
    HashTableStats stats(*this, *this);
    stats.print(name);
  }

  /**
   * Get the number of collisions that the probing strategy has to go through to find the key or
   * determine that it is not in the set.
   */
  uint32_t count_collisions(const Key &key) const
  {
    return this->count_collisions__impl(key, m_hash(key));
  }

  /**
   * Remove all elements from the set.
   */
  void clear()
  {
    this->~Set();
    new (this) Set();
  }

  /**
   * Creates a new slot array and reinserts all keys inside of that. This method can be used to get
   * rid of removed slots. Also this is useful for benchmarking the grow function.
   */
  void rehash()
  {
    this->realloc_and_reinsert(this->size());
  }

  /**
   * Returns the number of keys stored in the set.
   */
  uint32_t size() const
  {
    return m_occupied_and_removed_slots - m_removed_slots;
  }

  /**
   * Returns true if no keys are stored.
   */
  bool is_empty() const
  {
    return m_occupied_and_removed_slots == m_removed_slots;
  }

  /**
   * Returns the number of available slots. This is mostly for debugging purposes.
   */
  uint32_t capacity() const
  {
    return m_slots.size();
  }

  /**
   * Returns the amount of removed slots in the set. This is mostly for debugging purposes.
   */
  uint32_t removed_amount() const
  {
    return m_removed_slots;
  }

  /**
   * Returns the bytes required per element. This is mostly for debugging purposes.
   */
  uint32_t size_per_element() const
  {
    return sizeof(Slot);
  }

  /**
   * Returns the approximate memory requirements of the set in bytes. This is more correct for
   * larger sets.
   */
  uint32_t size_in_bytes() const
  {
    return sizeof(Slot) * m_slots.size();
  }

  /**
   * Potentially resize the set such that it can hold the specified number of keys without another
   * grow operation.
   */
  void reserve(uint32_t n)
  {
    if (m_usable_slots < n) {
      this->realloc_and_reinsert(n);
    }
  }

  /**
   * Returns true if there is a key that exists in both sets.
   */
  static bool Intersects(const Set &a, const Set &b)
  {
    /* Make sure we iterate over the shorter set. */
    if (a.size() > b.size()) {
      return Intersects(b, a);
    }

    for (const Key &key : a) {
      if (b.contains(key)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Returns true if no key from a is also in b and vice versa.
   */
  static bool Disjoint(const Set &a, const Set &b)
  {
    return !Intersects(a, b);
  }

 private:
  BLI_NOINLINE void realloc_and_reinsert(uint32_t min_usable_slots)
  {
    uint32_t total_slots, usable_slots;
    m_max_load_factor.compute_total_and_usable_slots(
        SlotArray::inline_buffer_capacity(), min_usable_slots, &total_slots, &usable_slots);
    uint32_t new_slot_mask = total_slots - 1;

    /**
     * Optimize the case when the set was empty beforehand. We can avoid some copies here.
     */
    if (this->size() == 0) {
      m_slots.~Array();
      new (&m_slots) SlotArray(total_slots);
      m_removed_slots = 0;
      m_occupied_and_removed_slots = 0;
      m_usable_slots = usable_slots;
      m_slot_mask = new_slot_mask;
      return;
    }

    /* The grown array that we insert the keys into. */
    SlotArray new_slots(total_slots);

    for (Slot &slot : m_slots) {
      if (slot.is_occupied()) {
        this->add_after_grow_and_destruct_old(slot, new_slots, new_slot_mask);
      }
    }

    /* All occupied slots have been destructed already and empty/removed slots are assumed to be
     * trivially destructible. */
    m_slots.clear_without_destruct();
    m_slots = std::move(new_slots);
    m_occupied_and_removed_slots -= m_removed_slots;
    m_usable_slots = usable_slots;
    m_removed_slots = 0;
    m_slot_mask = new_slot_mask;
  }

  void add_after_grow_and_destruct_old(Slot &old_slot,
                                       SlotArray &new_slots,
                                       uint32_t new_slot_mask)
  {
    uint32_t hash = old_slot.get_hash(Hash());

    SLOT_PROBING_BEGIN (ProbingStrategy, hash, new_slot_mask, slot_index) {
      Slot &slot = new_slots[slot_index];
      if (slot.is_empty()) {
        slot.relocate_occupied_here(old_slot, hash);
        return;
      }
    }
    SLOT_PROBING_END();
  }

  template<typename ForwardKey> bool contains__impl(const ForwardKey &key, uint32_t hash) const
  {
    SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        return false;
      }
      if (slot.contains(key, m_is_equal, hash)) {
        return true;
      }
    }
    SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey> void add_new__impl(ForwardKey &&key, uint32_t hash)
  {
    BLI_assert(!this->contains_as(key));

    this->ensure_can_add();

    SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        slot.occupy(std::forward<ForwardKey>(key), hash);
        m_occupied_and_removed_slots++;
        return;
      }
    }
    SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey> bool add__impl(ForwardKey &&key, uint32_t hash)
  {
    this->ensure_can_add();

    SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        slot.occupy(std::forward<ForwardKey>(key), hash);
        m_occupied_and_removed_slots++;
        return true;
      }
      if (slot.contains(key, m_is_equal, hash)) {
        return false;
      }
    }
    SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey> bool remove__impl(const ForwardKey &key, uint32_t hash)
  {
    SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, m_is_equal, hash)) {
        slot.remove();
        m_removed_slots++;
        return true;
      }
      if (slot.is_empty()) {
        return false;
      }
    }
    SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey> void remove_contained__impl(const ForwardKey &key, uint32_t hash)
  {
    BLI_assert(this->contains_as(key));
    m_removed_slots++;

    SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, m_is_equal, hash)) {
        slot.remove();
        return;
      }
    }
    SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey>
  uint32_t count_collisions__impl(const ForwardKey &key, uint32_t hash) const
  {
    uint32_t collisions = 0;

    SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, m_is_equal, hash)) {
        return collisions;
      }
      if (slot.is_empty()) {
        return collisions;
      }
      collisions++;
    }
    SET_SLOT_PROBING_END();
  }

  void ensure_can_add()
  {
    if (m_occupied_and_removed_slots >= m_usable_slots) {
      this->realloc_and_reinsert(this->size() + 1);
      BLI_assert(m_occupied_and_removed_slots < m_usable_slots);
    }
  }
};

/**
 * A wrapper for std::unordered_set with the API of blender::Set. This can be used for
 * benchmarking.
 */
template<typename Key> class StdUnorderedSetWrapper {
 private:
  using SetType = std::unordered_set<Key, blender::DefaultHash<Key>>;
  SetType m_set;

 public:
  uint32_t size() const
  {
    return (uint32_t)m_set.size();
  }

  bool is_empty() const
  {
    return m_set.empty();
  }

  void reserve(uint32_t n)
  {
    m_set.reserve(n);
  }

  void add_new(const Key &key)
  {
    m_set.insert(key);
  }
  void add_new(Key &&key)
  {
    m_set.insert(std::move(key));
  }

  bool add(const Key &key)
  {
    return m_set.insert(key).second;
  }
  bool add(Key &&key)
  {
    return m_set.insert(std::move(key)).second;
  }

  void add_multiple(Span<Key> keys)
  {
    for (const Key &key : keys) {
      m_set.insert(key);
    }
  }

  bool contains(const Key &key) const
  {
    return m_set.find(key) != m_set.end();
  }

  bool remove(const Key &key)
  {
    return (bool)m_set.erase(key);
  }

  void remove_contained(const Key &key)
  {
    return m_set.erase(key);
  }

  void clear()
  {
    m_set.clear();
  }

  typename SetType::iterator begin() const
  {
    return m_set.begin();
  }

  typename SetType::iterator end() const
  {
    return m_set.end();
  }
};

}  // namespace blender

#endif /* __BLI_SET_HH__ */
