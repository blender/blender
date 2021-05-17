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

#pragma once

/** \file
 * \ingroup bli
 *
 * A `blender::VectorSet<Key>` is an ordered container for elements of type `Key`. It has the same
 * interface as `blender::Set` with the following extensions:
 * - The insertion order of keys is maintained as long as no elements are removed.
 * - The keys are stored in a contiguous array.
 *
 * All core operations (add, remove and contains) can be done in O(1) amortized expected time.
 *
 * Using a VectorSet instead of a normal Set can be beneficial in any of the following
 * circumstances:
 * - The insertion order is important.
 * - Iteration over all keys has to be fast.
 * - The keys in the set are supposed to be passed to a function that does not have to know that
 *   the keys are stored in a set. With a VectorSet, one can get a Span containing all keys
 *   without additional copies.
 *
 * blender::VectorSet is implemented using open addressing in a slot array with a power-of-two
 * size. Other than in blender::Set, a slot does not contain the key though. Instead it only
 * contains an index into an array of keys that is stored separately.
 *
 * Some noteworthy information:
 * - Key must be a movable type.
 * - Pointers to keys might be invalidated, when the vector set is changed or moved.
 * - The hash function can be customized. See BLI_hash.hh for details.
 * - The probing strategy can be customized. See BLI_probing_strategies.hh for details.
 * - The slot type can be customized. See BLI_vector_set_slots.hh for details.
 * - The methods `add_new` and `remove_contained` should be used instead of `add` and `remove`
 *   whenever appropriate. Assumptions and intention are described better this way.
 * - Using a range-for loop over a vector set, is as efficient as iterating over an array (because
 *   it is the same thing).
 * - Lookups can be performed using types other than Key without conversion. For that use the
 *   methods ending with `_as`. The template parameters Hash and IsEqual have to support the other
 *   key type. This can greatly improve performance when the strings are used as keys.
 * - The default constructor is cheap.
 * - The `print_stats` method can be used to get information about the distribution of keys and
 *   memory usage.
 *
 * Possible Improvements:
 * - Small buffer optimization for the keys.
 */

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_hash_tables.hh"
#include "BLI_probing_strategies.hh"
#include "BLI_vector_set_slots.hh"

namespace blender {

template<
    /**
     * Type of the elements that are stored in this set. It has to be movable. Furthermore, the
     * hash and is-equal functions have to support it.
     */
    typename Key,
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
     * This is what will actually be stored in the hash table array. At a minimum a slot has to be
     * able to hold an array index and information about whether the slot is empty, occupied or
     * removed. Using a non-standard slot type can improve performance for some types.
     * Also see BLI_vector_set_slots.hh.
     */
    typename Slot = typename DefaultVectorSetSlot<Key>::type,
    /**
     * The allocator used by this set. Should rarely be changed, except when you don't want that
     * MEM_* etc. is used internally.
     */
    typename Allocator = GuardedAllocator>
class VectorSet {
 public:
  using value_type = Key;
  using pointer = Key *;
  using const_pointer = const Key *;
  using reference = Key &;
  using const_reference = const Key &;
  using iterator = Key *;
  using const_iterator = const Key *;
  using size_type = int64_t;

 private:
  /**
   * Slots are either empty, occupied or removed. The number of occupied slots can be computed by
   * subtracting the removed slots from the occupied-and-removed slots.
   */
  int64_t removed_slots_;
  int64_t occupied_and_removed_slots_;

  /**
   * The maximum number of slots that can be used (either occupied or removed) until the set has to
   * grow. This is the total number of slots times the max load factor.
   */
  int64_t usable_slots_;

  /**
   * The number of slots minus one. This is a bit mask that can be used to turn any integer into a
   * valid slot index efficiently.
   */
  uint64_t slot_mask_;

  /** This is called to hash incoming keys. */
  Hash hash_;

  /** This is called to check equality of two keys. */
  IsEqual is_equal_;

  /** The max load factor is 1/2 = 50% by default. */
#define LOAD_FACTOR 1, 2
  LoadFactor max_load_factor_ = LoadFactor(LOAD_FACTOR);
  using SlotArray = Array<Slot, LoadFactor::compute_total_slots(4, LOAD_FACTOR), Allocator>;
#undef LOAD_FACTOR

  /**
   * This is the array that contains the actual slots. There is always at least one empty slot and
   * the size of the array is a power of two.
   */
  SlotArray slots_;

  /**
   * Pointer to an array that contains all keys. The keys are sorted by insertion order as long as
   * no keys are removed. The first set->size() elements in this array are initialized. The
   * capacity of the array is usable_slots_.
   */
  Key *keys_ = nullptr;

  /** Iterate over a slot index sequence for a given hash. */
#define VECTOR_SET_SLOT_PROBING_BEGIN(HASH, R_SLOT) \
  SLOT_PROBING_BEGIN (ProbingStrategy, HASH, slot_mask_, SLOT_INDEX) \
    auto &R_SLOT = slots_[SLOT_INDEX];
#define VECTOR_SET_SLOT_PROBING_END() SLOT_PROBING_END()

 public:
  /**
   * Initialize an empty vector set. This is a cheap operation and won't do an allocation. This is
   * necessary to avoid a high cost when no elements are added at all. An optimized grow operation
   * is performed on the first insertion.
   */
  VectorSet(Allocator allocator = {}) noexcept
      : removed_slots_(0),
        occupied_and_removed_slots_(0),
        usable_slots_(0),
        slot_mask_(0),
        slots_(1, allocator),
        keys_(nullptr)
  {
  }

  VectorSet(NoExceptConstructor, Allocator allocator = {}) : VectorSet(allocator)
  {
  }

  VectorSet(Span<Key> keys, Allocator allocator = {}) : VectorSet(NoExceptConstructor(), allocator)
  {
    this->add_multiple(keys);
  }

  /**
   * Construct a vector set that contains the given keys. Duplicates will be removed automatically.
   */
  VectorSet(const std::initializer_list<Key> &keys, Allocator allocator = {})
      : VectorSet(Span(keys), allocator)
  {
  }

  ~VectorSet()
  {
    destruct_n(keys_, this->size());
    if (keys_ != nullptr) {
      this->deallocate_keys_array(keys_);
    }
  }

  VectorSet(const VectorSet &other) : slots_(other.slots_)
  {
    keys_ = this->allocate_keys_array(other.usable_slots_);
    try {
      uninitialized_copy_n(other.keys_, other.size(), keys_);
    }
    catch (...) {
      this->deallocate_keys_array(keys_);
      throw;
    }

    removed_slots_ = other.removed_slots_;
    occupied_and_removed_slots_ = other.occupied_and_removed_slots_;
    usable_slots_ = other.usable_slots_;
    slot_mask_ = other.slot_mask_;
    hash_ = other.hash_;
    is_equal_ = other.is_equal_;
  }

  VectorSet(VectorSet &&other) noexcept
      : removed_slots_(other.removed_slots_),
        occupied_and_removed_slots_(other.occupied_and_removed_slots_),
        usable_slots_(other.usable_slots_),
        slot_mask_(other.slot_mask_),
        slots_(std::move(other.slots_)),
        keys_(other.keys_)
  {
    other.removed_slots_ = 0;
    other.occupied_and_removed_slots_ = 0;
    other.usable_slots_ = 0;
    other.slot_mask_ = 0;
    other.slots_ = SlotArray(1);
    other.keys_ = nullptr;
  }

  VectorSet &operator=(const VectorSet &other)
  {
    return copy_assign_container(*this, other);
  }

  VectorSet &operator=(VectorSet &&other)
  {
    return move_assign_container(*this, std::move(other));
  }

  /**
   * Get the key stored at the given position in the vector.
   */
  const Key &operator[](const int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index <= this->size());
    return keys_[index];
  }

  operator Span<Key>() const
  {
    return Span<Key>(keys_, this->size());
  }

  /**
   * Get an Span referencing the keys vector. The referenced memory buffer is only valid as
   * long as the vector set is not changed.
   *
   * The keys must not be changed, because this would change their hash value.
   */
  Span<Key> as_span() const
  {
    return *this;
  }

  /**
   * Add a new key to the vector set. This invokes undefined behavior when the key is in the set
   * already. When you know for certain that a key is not in the set yet, use this method for
   * better performance. This also expresses the intent better.
   */
  void add_new(const Key &key)
  {
    this->add_new__impl(key, hash_(key));
  }
  void add_new(Key &&key)
  {
    this->add_new__impl(std::move(key), hash_(key));
  }

  /**
   * Add a key to the vector set. If the key exists in the set already, nothing is done. The return
   * value is true if the key was newly added.
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
  template<typename ForwardKey> bool add_as(ForwardKey &&key)
  {
    return this->add__impl(std::forward<ForwardKey>(key), hash_(key));
  }

  /**
   * Convenience function to add many keys to the vector set at once. Duplicates are removed
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
   * Returns true if the key is in the vector set.
   *
   * This is similar to std::unordered_set::find() != std::unordered_set::end().
   */
  bool contains(const Key &key) const
  {
    return this->contains_as(key);
  }
  template<typename ForwardKey> bool contains_as(const ForwardKey &key) const
  {
    return this->contains__impl(key, hash_(key));
  }

  /**
   * Deletes the key from the set. Returns true when the key existed in the set and is now removed.
   * This might change the order of elements in the vector.
   *
   * This is similar to std::unordered_set::erase.
   */
  bool remove(const Key &key)
  {
    return this->remove_as(key);
  }
  template<typename ForwardKey> bool remove_as(const ForwardKey &key)
  {
    return this->remove__impl(key, hash_(key));
  }

  /**
   * Deletes the key from the set. This invokes undefined behavior when the key is not in the set.
   * It might change the order of elements in the vector.
   */
  void remove_contained(const Key &key)
  {
    this->remove_contained_as(key);
  }
  template<typename ForwardKey> void remove_contained_as(const ForwardKey &key)
  {
    this->remove_contained__impl(key, hash_(key));
  }

  /**
   * Delete and return a key from the set. This will remove the last element in the vector. The
   * order of the remaining elements in the set is not changed.
   */
  Key pop()
  {
    return this->pop__impl();
  }

  /**
   * Return the location of the key in the vector. It is assumed, that the key is in the vector
   * set. If this is not necessarily the case, use `index_of_try`.
   */
  int64_t index_of(const Key &key) const
  {
    return this->index_of_as(key);
  }
  template<typename ForwardKey> int64_t index_of_as(const ForwardKey &key) const
  {
    return this->index_of__impl(key, hash_(key));
  }

  /**
   * Return the location of the key in the vector. If the key is not in the set, -1 is returned.
   * If you know for sure that the key is in the set, it is better to use `index_of` instead.
   */
  int64_t index_of_try(const Key &key) const
  {
    return this->index_of_try_as(key);
  }
  template<typename ForwardKey> int64_t index_of_try_as(const ForwardKey &key) const
  {
    return this->index_of_try__impl(key, hash_(key));
  }

  /**
   * Return the index of the key in the vector. If the key is not in the set, add it and return its
   * index.
   */
  int64_t index_of_or_add(const Key &key)
  {
    return this->index_of_or_add_as(key);
  }
  int64_t index_of_or_add(Key &&key)
  {
    return this->index_of_or_add_as(std::move(key));
  }
  template<typename ForwardKey> int64_t index_of_or_add_as(ForwardKey &&key)
  {
    return this->index_of_or_add__impl(std::forward<ForwardKey>(key), hash_(key));
  }

  /**
   * Returns the key that is stored in the vector set that compares equal to the given key. This
   * invokes undefined behavior when the key is not in the set.
   */
  const Key &lookup_key(const Key &key) const
  {
    return this->lookup_key_as(key);
  }
  template<typename ForwardKey> const Key &lookup_key_as(const ForwardKey &key) const
  {
    const Key *key_ptr = this->lookup_key_ptr_as(key);
    BLI_assert(key_ptr != nullptr);
    return *key_ptr;
  }

  /**
   * Returns a pointer to the key that is stored in the vector set that compares equal to the given
   * key. If the key is not in the set, null is returned.
   */
  const Key *lookup_key_ptr(const Key &key) const
  {
    return this->lookup_key_ptr_as(key);
  }
  template<typename ForwardKey> const Key *lookup_key_ptr_as(const ForwardKey &key) const
  {
    const int64_t index = this->index_of_try__impl(key, hash_(key));
    if (index >= 0) {
      return keys_ + index;
    }
    return nullptr;
  }

  /**
   * Get a pointer to the beginning of the array containing all keys.
   */
  const Key *data() const
  {
    return keys_;
  }

  const Key *begin() const
  {
    return keys_;
  }

  const Key *end() const
  {
    return keys_ + this->size();
  }

  /**
   * Print common statistics like size and collision count. This is useful for debugging purposes.
   */
  void print_stats(StringRef name = "") const
  {
    HashTableStats stats(*this, this->as_span());
    stats.print(name);
  }

  /**
   * Returns the number of keys stored in the vector set.
   */
  int64_t size() const
  {
    return occupied_and_removed_slots_ - removed_slots_;
  }

  /**
   * Returns true if no keys are stored.
   */
  bool is_empty() const
  {
    return occupied_and_removed_slots_ == removed_slots_;
  }

  /**
   * Returns the number of available slots. This is mostly for debugging purposes.
   */
  int64_t capacity() const
  {
    return slots_.size();
  }

  /**
   * Returns the amount of removed slots in the set. This is mostly for debugging purposes.
   */
  int64_t removed_amount() const
  {
    return removed_slots_;
  }

  /**
   * Returns the bytes required per element. This is mostly for debugging purposes.
   */
  int64_t size_per_element() const
  {
    return sizeof(Slot) + sizeof(Key);
  }

  /**
   * Returns the approximate memory requirements of the set in bytes. This is more correct for
   * larger sets.
   */
  int64_t size_in_bytes() const
  {
    return static_cast<int64_t>(sizeof(Slot) * slots_.size() + sizeof(Key) * usable_slots_);
  }

  /**
   * Potentially resize the vector set such that it can hold n elements without doing another grow.
   */
  void reserve(const int64_t n)
  {
    if (usable_slots_ < n) {
      this->realloc_and_reinsert(n);
    }
  }

  /**
   * Remove all keys from the vector set.
   */
  void clear()
  {
    this->noexcept_reset();
  }

  /**
   * Get the number of collisions that the probing strategy has to go through to find the key or
   * determine that it is not in the set.
   */
  int64_t count_collisions(const Key &key) const
  {
    return this->count_collisions__impl(key, hash_(key));
  }

 private:
  BLI_NOINLINE void realloc_and_reinsert(const int64_t min_usable_slots)
  {
    int64_t total_slots, usable_slots;
    max_load_factor_.compute_total_and_usable_slots(
        SlotArray::inline_buffer_capacity(), min_usable_slots, &total_slots, &usable_slots);
    BLI_assert(total_slots >= 1);
    const uint64_t new_slot_mask = static_cast<uint64_t>(total_slots) - 1;

    /* Optimize the case when the set was empty beforehand. We can avoid some copies here. */
    if (this->size() == 0) {
      try {
        slots_.reinitialize(total_slots);
        keys_ = this->allocate_keys_array(usable_slots);
      }
      catch (...) {
        this->noexcept_reset();
        throw;
      }
      removed_slots_ = 0;
      occupied_and_removed_slots_ = 0;
      usable_slots_ = usable_slots;
      slot_mask_ = new_slot_mask;
      return;
    }

    SlotArray new_slots(total_slots);

    try {
      for (Slot &slot : slots_) {
        if (slot.is_occupied()) {
          this->add_after_grow(slot, new_slots, new_slot_mask);
          slot.remove();
        }
      }
      slots_ = std::move(new_slots);
    }
    catch (...) {
      this->noexcept_reset();
      throw;
    }

    Key *new_keys = this->allocate_keys_array(usable_slots);
    try {
      uninitialized_relocate_n(keys_, this->size(), new_keys);
    }
    catch (...) {
      this->deallocate_keys_array(new_keys);
      this->noexcept_reset();
      throw;
    }
    this->deallocate_keys_array(keys_);

    keys_ = new_keys;
    occupied_and_removed_slots_ -= removed_slots_;
    usable_slots_ = usable_slots;
    removed_slots_ = 0;
    slot_mask_ = new_slot_mask;
  }

  void add_after_grow(Slot &old_slot, SlotArray &new_slots, const uint64_t new_slot_mask)
  {
    const Key &key = keys_[old_slot.index()];
    const uint64_t hash = old_slot.get_hash(key, Hash());

    SLOT_PROBING_BEGIN (ProbingStrategy, hash, new_slot_mask, slot_index) {
      Slot &slot = new_slots[slot_index];
      if (slot.is_empty()) {
        slot.occupy(old_slot.index(), hash);
        return;
      }
    }
    SLOT_PROBING_END();
  }

  void noexcept_reset() noexcept
  {
    Allocator allocator = slots_.allocator();
    this->~VectorSet();
    new (this) VectorSet(NoExceptConstructor(), allocator);
  }

  template<typename ForwardKey>
  bool contains__impl(const ForwardKey &key, const uint64_t hash) const
  {
    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        return false;
      }
      if (slot.contains(key, is_equal_, hash, keys_)) {
        return true;
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey> void add_new__impl(ForwardKey &&key, const uint64_t hash)
  {
    BLI_assert(!this->contains_as(key));

    this->ensure_can_add();

    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        int64_t index = this->size();
        new (keys_ + index) Key(std::forward<ForwardKey>(key));
        slot.occupy(index, hash);
        occupied_and_removed_slots_++;
        return;
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey> bool add__impl(ForwardKey &&key, const uint64_t hash)
  {
    this->ensure_can_add();

    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        int64_t index = this->size();
        new (keys_ + index) Key(std::forward<ForwardKey>(key));
        slot.occupy(index, hash);
        occupied_and_removed_slots_++;
        return true;
      }
      if (slot.contains(key, is_equal_, hash, keys_)) {
        return false;
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey>
  int64_t index_of__impl(const ForwardKey &key, const uint64_t hash) const
  {
    BLI_assert(this->contains_as(key));

    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, is_equal_, hash, keys_)) {
        return slot.index();
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey>
  int64_t index_of_try__impl(const ForwardKey &key, const uint64_t hash) const
  {
    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, is_equal_, hash, keys_)) {
        return slot.index();
      }
      if (slot.is_empty()) {
        return -1;
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey>
  int64_t index_of_or_add__impl(ForwardKey &&key, const uint64_t hash)
  {
    this->ensure_can_add();

    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, is_equal_, hash, keys_)) {
        return slot.index();
      }
      if (slot.is_empty()) {
        const int64_t index = this->size();
        new (keys_ + index) Key(std::forward<ForwardKey>(key));
        slot.occupy(index, hash);
        occupied_and_removed_slots_++;
        return index;
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  Key pop__impl()
  {
    BLI_assert(this->size() > 0);

    const int64_t index_to_pop = this->size() - 1;
    Key key = std::move(keys_[index_to_pop]);
    keys_[index_to_pop].~Key();
    const uint64_t hash = hash_(key);

    removed_slots_++;

    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.has_index(index_to_pop)) {
        slot.remove();
        return key;
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey> bool remove__impl(const ForwardKey &key, const uint64_t hash)
  {
    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, is_equal_, hash, keys_)) {
        this->remove_key_internal(slot);
        return true;
      }
      if (slot.is_empty()) {
        return false;
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey>
  void remove_contained__impl(const ForwardKey &key, const uint64_t hash)
  {
    BLI_assert(this->contains_as(key));

    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, is_equal_, hash, keys_)) {
        this->remove_key_internal(slot);
        return;
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  void remove_key_internal(Slot &slot)
  {
    int64_t index_to_remove = slot.index();
    int64_t size = this->size();
    int64_t last_element_index = size - 1;

    if (index_to_remove < last_element_index) {
      keys_[index_to_remove] = std::move(keys_[last_element_index]);
      this->update_slot_index(keys_[index_to_remove], last_element_index, index_to_remove);
    }

    keys_[last_element_index].~Key();
    slot.remove();
    removed_slots_++;
    return;
  }

  void update_slot_index(const Key &key, const int64_t old_index, const int64_t new_index)
  {
    uint64_t hash = hash_(key);
    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.has_index(old_index)) {
        slot.update_index(new_index);
        return;
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey>
  int64_t count_collisions__impl(const ForwardKey &key, const uint64_t hash) const
  {
    int64_t collisions = 0;

    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, is_equal_, hash, keys_)) {
        return collisions;
      }
      if (slot.is_empty()) {
        return collisions;
      }
      collisions++;
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  void ensure_can_add()
  {
    if (occupied_and_removed_slots_ >= usable_slots_) {
      this->realloc_and_reinsert(this->size() + 1);
      BLI_assert(occupied_and_removed_slots_ < usable_slots_);
    }
  }

  Key *allocate_keys_array(const int64_t size)
  {
    return static_cast<Key *>(
        slots_.allocator().allocate(sizeof(Key) * static_cast<size_t>(size), alignof(Key), AT));
  }

  void deallocate_keys_array(Key *keys)
  {
    slots_.allocator().deallocate(keys);
  }
};

/**
 * Same as a normal VectorSet, but does not use Blender's guarded allocator. This is useful when
 * allocating memory with static storage duration.
 */
template<typename Key,
         typename ProbingStrategy = DefaultProbingStrategy,
         typename Hash = DefaultHash<Key>,
         typename IsEqual = DefaultEquality,
         typename Slot = typename DefaultVectorSetSlot<Key>::type>
using RawVectorSet = VectorSet<Key, ProbingStrategy, Hash, IsEqual, Slot, RawAllocator>;

}  // namespace blender
