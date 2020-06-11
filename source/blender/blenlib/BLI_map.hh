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

#ifndef __BLI_MAP_HH__
#define __BLI_MAP_HH__

/** \file
 * \ingroup bli
 *
 * A `blender::Map<Key, Value>` is an unordered associative container that stores key-value pairs.
 * The keys have to be unique. It is designed to be a more convenient and efficient replacement for
 * `std::unordered_map`. All core operations (add, lookup, remove and contains) can be done in O(1)
 * amortized expected time.
 *
 * Your default choice for a hash map in Blender should be `blender::Map`.
 *
 * blender::Map is implemented using open addressing in a slot array with a power-of-two size.
 * Every slot is in one of three states: empty, occupied or removed. If a slot is occupied, it
 * contains a Key and Value instance.
 *
 * Benchmarking and comparing hash tables is hard, because many factors influence the result. The
 * performance of a hash table depends on the combination of the hash function, probing strategy,
 * max load factor, data types, slot type and data distribution. This implementation is designed to
 * be relatively fast by default in all cases. However, it also offers many customization points
 * that allow it to be optimized for a specific use case.
 *
 * A rudimentary benchmark can be found in BLI_map_test.cc. The results of that benchmark are there
 * as well. The numbers show that in this specific case blender::Map outperforms std::unordered_map
 * consistently by a good amount.
 *
 * Some noteworthy information:
 * - Key and Value must be movable types.
 * - Pointers to keys and values might be invalidated when the map is changed or moved.
 * - The hash function can be customized. See BLI_hash.hh for details.
 * - The probing strategy can be customized. See BLI_probing_strategies.hh for details.
 * - The slot type can be customized. See BLI_map_slots.hh for details.
 * - Small buffer optimization is enabled by default, if Key and Value are not too large.
 * - The methods `add_new` and `remove_contained` should be used instead of `add` and `remove`
 *   whenever appropriate. Assumptions and intention are described better this way.
 * - There are multiple methods to add and lookup keys for different use cases.
 * - You cannot use a range-for loop on the map directly. Instead use the keys(), values() and
 *   items() iterators. If your map is non-const, you can also change the values through those
 *   iterators (but not the keys).
 * - Lookups can be performed using types other than Key without conversion. For that use the
 *   methods ending with `_as`. The template parameters Hash and IsEqual have to support the other
 *   key type. This can greatly improve performance when the map uses strings as keys.
 * - The default constructor is cheap, even when a large InlineBufferCapacity is used. A large
 *   slot array will only be initialized when the first element is added.
 * - The `print_stats` method can be used to get information about the distribution of keys and
 *   memory usage of the map.
 * - The method names don't follow the std::unordered_map names in many cases. Searching for such
 *   names in this file will usually let you discover the new name.
 * - There is a StdUnorderedMapWrapper class, that wraps std::unordered_map and gives it the same
 *   interface as blender::Map. This is useful for benchmarking.
 */

#include <unordered_map>

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_hash_tables.hh"
#include "BLI_map_slots.hh"
#include "BLI_optional.hh"
#include "BLI_probing_strategies.hh"

namespace blender {

template<
    /**
     * Type of the keys stored in the map. Keys have to be movable. Furthermore, the hash and
     * is-equal functions have to support it.
     */
    typename Key,
    /**
     * Type of the value that is stored per key. It has to be movable as well.
     */
    typename Value,
    /**
     * The minimum number of elements that can be stored in this Map without doing a heap
     * allocation. This is useful when you expect to have many small maps. However, keep in mind
     * that (unlike vector) initializing a map has a O(n) cost in the number of slots.
     *
     * When Key or Value are large, the small buffer optimization is disabled by default to avoid
     * large unexpected allocations on the stack. It can still be enabled explicitely though.
     */
    uint32_t InlineBufferCapacity = (sizeof(Key) + sizeof(Value) < 100) ? 4 : 0,
    /**
     * The strategy used to deal with collistions. They are defined in BLI_probing_strategies.hh.
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
     * able to hold a key, a value and information about whether the slot is empty, occupied or
     * removed. Using a non-standard slot type can improve performance or reduce the memory
     * footprint for some types. Slot types are defined in BLI_map_slots.hh
     */
    typename Slot = typename DefaultMapSlot<Key, Value>::type,
    /**
     * The allocator used by this map. Should rarely be changed, except when you don't want that
     * MEM_* is used internally.
     */
    typename Allocator = GuardedAllocator>
class Map {
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
#define MAP_SLOT_PROBING_BEGIN(HASH, R_SLOT) \
  SLOT_PROBING_BEGIN (ProbingStrategy, HASH, m_slot_mask, SLOT_INDEX) \
    auto &R_SLOT = m_slots[SLOT_INDEX];
#define MAP_SLOT_PROBING_END() SLOT_PROBING_END()

 public:
  /**
   * Initialize an empty map. This is a cheap operation no matter how large the inline buffer is.
   * This is necessary to avoid a high cost when no elements are added at all. An optimized grow
   * operation is performed on the first insertion.
   */
  Map()
      : m_removed_slots(0),
        m_occupied_and_removed_slots(0),
        m_usable_slots(0),
        m_slot_mask(0),
        m_hash(),
        m_is_equal(),
        m_slots(1)
  {
  }

  ~Map() = default;

  Map(const Map &other) = default;

  Map(Map &&other) noexcept
      : m_removed_slots(other.m_removed_slots),
        m_occupied_and_removed_slots(other.m_occupied_and_removed_slots),
        m_usable_slots(other.m_usable_slots),
        m_slot_mask(other.m_slot_mask),
        m_hash(std::move(other.m_hash)),
        m_is_equal(std::move(other.m_is_equal)),
        m_slots(std::move(other.m_slots))
  {
    other.~Map();
    new (&other) Map();
  }

  Map &operator=(const Map &other)
  {
    if (this == &other) {
      return *this;
    }

    this->~Map();
    new (this) Map(other);

    return *this;
  }

  Map &operator=(Map &&other)
  {
    if (this == &other) {
      return *this;
    }

    this->~Map();
    new (this) Map(std::move(other));

    return *this;
  }

  /**
   * Insert a new key-value-pair into the map. This invokes undefined behavior when the key is in
   * the map already.
   */
  void add_new(const Key &key, const Value &value)
  {
    this->add_new__impl(key, value, m_hash(key));
  }
  void add_new(const Key &key, Value &&value)
  {
    this->add_new__impl(key, std::move(value), m_hash(key));
  }
  void add_new(Key &&key, const Value &value)
  {
    this->add_new__impl(std::move(key), value, m_hash(key));
  }
  void add_new(Key &&key, Value &&value)
  {
    this->add_new__impl(std::move(key), std::move(value), m_hash(key));
  }

  /**
   * Add a key-value-pair to the map. If the map contains the key already, nothing is changed.
   * If you want to replace the currently stored value, use `add_overwrite`.
   * Returns true when the key has been newly added.
   *
   * This is similar to std::unordered_map::insert.
   */
  bool add(const Key &key, const Value &value)
  {
    return this->add_as(key, value);
  }
  bool add(const Key &key, Value &&value)
  {
    return this->add_as(key, std::move(value));
  }
  bool add(Key &&key, const Value &value)
  {
    return this->add_as(std::move(key), value);
  }
  bool add(Key &&key, Value &&value)
  {
    return this->add_as(std::move(key), std::move(value));
  }

  /**
   * Same as `add`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey, typename ForwardValue>
  bool add_as(ForwardKey &&key, ForwardValue &&value)
  {
    return this->add__impl(
        std::forward<ForwardKey>(key), std::forward<ForwardValue>(value), m_hash(key));
  }

  /**
   * Adds a key-value-pair to the map. If the map contained the key already, the corresponding
   * value will be replaced.
   * Returns true when the key has been newly added.
   *
   * This is similar to std::unordered_map::insert_or_assign.
   */
  bool add_overwrite(const Key &key, const Value &value)
  {
    return this->add_overwrite_as(key, value);
  }
  bool add_overwrite(const Key &key, Value &&value)
  {
    return this->add_overwrite_as(key, std::move(value));
  }
  bool add_overwrite(Key &&key, const Value &value)
  {
    return this->add_overwrite_as(std::move(key), value);
  }
  bool add_overwrite(Key &&key, Value &&value)
  {
    return this->add_overwrite_as(std::move(key), std::move(value));
  }

  /**
   * Same as `add_overwrite`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey, typename ForwardValue>
  bool add_overwrite_as(ForwardKey &&key, ForwardValue &&value)
  {
    return this->add_overwrite__impl(
        std::forward<ForwardKey>(key), std::forward<ForwardValue>(value), m_hash(key));
  }

  /**
   * Returns true if there is a key in the map that compares equal to the given key.
   *
   * This is similar to std::unordered_map::contains.
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
   * Deletes the key-value-pair with the given key. Returns true when the key was contained and is
   * now removed, otherwise false.
   *
   * This is similar to std::unordered_map::erase.
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
   * Deletes the key-value-pair with the given key. This invokes undefined behavior when the key is
   * not in the map.
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
   * Get the value that is stored for the given key and remove it from the map. This invokes
   * undefined behavior when the key is not in the map.
   */
  Value pop(const Key &key)
  {
    return this->pop_as(key);
  }

  /**
   * Same as `pop`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey> Value pop_as(const ForwardKey &key)
  {
    return this->pop__impl(key, m_hash(key));
  }

  /**
   * Get the value that is stored for the given key and remove it from the map. If the key is not
   * in the map, a value-less optional is returned.
   */
  Optional<Value> pop_try(const Key &key)
  {
    return this->pop_try_as(key);
  }

  /**
   * Same as `pop_try`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey> Optional<Value> pop_try_as(const ForwardKey &key)
  {
    return this->pop_try__impl(key, m_hash(key));
  }

  /**
   * Get the value that corresponds to the given key and remove it from the map. If the key is not
   * in the map, return the given default value instead.
   */
  Value pop_default(const Key &key, const Value &default_value)
  {
    return this->pop_default_as(key, default_value);
  }
  Value pop_default(const Key &key, Value &&default_value)
  {
    return this->pop_default_as(key, std::move(default_value));
  }

  /**
   * Same as `pop_default`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey, typename ForwardValue>
  Value pop_default_as(const ForwardKey &key, ForwardValue &&default_value)
  {
    return this->pop_default__impl(key, std::forward<ForwardValue>(default_value), m_hash(key));
  }

  /**
   * This method can be used to implement more complex custom behavior without having to do
   * multiple lookups
   *
   * When the key did not yet exist in the map, the create_value function is called. Otherwise the
   * modify_value function is called.
   *
   * Both functions are expected to take a single parameter of type `Value *`. In create_value,
   * this pointer will point to uninitialized memory that has to be initialized by the function. In
   * modify_value, it will point to an already initialized value.
   *
   * The function returns whatever is returned from the create_value or modify_value callback.
   * Therefore, both callbacks have to have the same return type.
   *
   * In this example an integer is stored for every key. The initial value is five and we want to
   * increase it every time the same key is used.
   *   map.add_or_modify(key,
   *                     [](int *value) { *value = 5; },
   *                     [](int *value) { (*value)++; });
   */
  template<typename CreateValueF, typename ModifyValueF>
  auto add_or_modify(const Key &key,
                     const CreateValueF &create_value,
                     const ModifyValueF &modify_value) -> decltype(create_value(nullptr))
  {
    return this->add_or_modify_as(key, create_value, modify_value);
  }
  template<typename CreateValueF, typename ModifyValueF>
  auto add_or_modify(Key &&key, const CreateValueF &create_value, const ModifyValueF &modify_value)
      -> decltype(create_value(nullptr))
  {
    return this->add_or_modify_as(std::move(key), create_value, modify_value);
  }

  /**
   * Same as `add_or_modify`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey, typename CreateValueF, typename ModifyValueF>
  auto add_or_modify_as(ForwardKey &&key,
                        const CreateValueF &create_value,
                        const ModifyValueF &modify_value) -> decltype(create_value(nullptr))
  {
    return this->add_or_modify__impl(
        std::forward<Key>(key), create_value, modify_value, m_hash(key));
  }

  /**
   * Returns a pointer to the value that corresponds to the given key. If the key is not in the
   * map, nullptr is returned.
   *
   * This is similar to std::unordered_map::find.
   */
  const Value *lookup_ptr(const Key &key) const
  {
    return this->lookup_ptr_as(key);
  }
  Value *lookup_ptr(const Key &key)
  {
    return this->lookup_ptr_as(key);
  }

  /**
   * Same as `lookup_ptr`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey> const Value *lookup_ptr_as(const ForwardKey &key) const
  {
    return this->lookup_ptr__impl(key, m_hash(key));
  }
  template<typename ForwardKey> Value *lookup_ptr_as(const ForwardKey &key)
  {
    return const_cast<Value *>(this->lookup_ptr__impl(key, m_hash(key)));
  }

  /**
   * Returns a reference to the value that corresponds to the given key. This invokes undefined
   * behavior when the key is not in the map.
   */
  const Value &lookup(const Key &key) const
  {
    return this->lookup_as(key);
  }
  Value &lookup(const Key &key)
  {
    return this->lookup_as(key);
  }

  /**
   * Same as `lookup`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey> const Value &lookup_as(const ForwardKey &key) const
  {
    const Value *ptr = this->lookup_ptr_as(key);
    BLI_assert(ptr != nullptr);
    return *ptr;
  }
  template<typename ForwardKey> Value &lookup_as(const ForwardKey &key)
  {
    Value *ptr = this->lookup_ptr_as(key);
    BLI_assert(ptr != nullptr);
    return *ptr;
  }

  /**
   * Returns a copy of the value that corresponds to the given key. If the key is not in the
   * map, the provided default_value is returned.
   */
  Value lookup_default(const Key &key, const Value &default_value) const
  {
    return this->lookup_default_as(key, default_value);
  }

  /**
   * Same as `lookup_default`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey, typename ForwardValue>
  Value lookup_default_as(const ForwardKey &key, ForwardValue &&default_value) const
  {
    const Value *ptr = this->lookup_ptr_as(key);
    if (ptr != nullptr) {
      return *ptr;
    }
    else {
      return std::forward<ForwardValue>(default_value);
    }
  }

  /**
   * Returns a reference to the value corresponding to the given key. If the key is not in the map,
   * a new key-value-pair is added and a reference to the value in the map is returned.
   */
  Value &lookup_or_add(const Key &key, const Value &value)
  {
    return this->lookup_or_add_as(key, value);
  }
  Value &lookup_or_add(const Key &key, Value &&value)
  {
    return this->lookup_or_add_as(key, std::move(value));
  }
  Value &lookup_or_add(Key &&key, const Value &value)
  {
    return this->lookup_or_add_as(std::move(key), value);
  }
  Value &lookup_or_add(Key &&key, Value &&value)
  {
    return this->lookup_or_add_as(std::move(key), std::move(value));
  }

  /**
   * Same as `lookup_or_add`, but accepts other key types that are supported by the hash
   * function.
   */
  template<typename ForwardKey, typename ForwardValue>
  Value &lookup_or_add_as(ForwardKey &&key, ForwardValue &&value)
  {
    return this->lookup_or_add__impl(
        std::forward<ForwardKey>(key), std::forward<ForwardValue>(value), m_hash(key));
  }

  /**
   * Returns a reference to the value that corresponds to the given key. If the key is not yet in
   * the map, it will be newly added.
   *
   * The create_value callback is only called when the key did not exist yet. It is expected to
   * take no parameters and return the value to be inserted.
   */
  template<typename CreateValueF>
  Value &lookup_or_add_cb(const Key &key, const CreateValueF &create_value)
  {
    return this->lookup_or_add_cb_as(key, create_value);
  }
  template<typename CreateValueF>
  Value &lookup_or_add_cb(Key &&key, const CreateValueF &create_value)
  {
    return this->lookup_or_add_cb_as(std::move(key), create_value);
  }

  /**
   * Same as `lookup_or_add_cb`, but accepts other key types that are supported by the hash
   * function.
   */
  template<typename ForwardKey, typename CreateValueF>
  Value &lookup_or_add_cb_as(ForwardKey &&key, const CreateValueF &create_value)
  {
    return this->lookup_or_add_cb__impl(std::forward<ForwardKey>(key), create_value, m_hash(key));
  }

  /**
   * Returns a reference to the value that corresponds to the given key. If the key is not yet in
   * the map, it will be newly added. The newly added value will be default constructed.
   */
  Value &lookup_or_add_default(const Key &key)
  {
    return this->lookup_or_add_default_as(key);
  }
  Value &lookup_or_add_default(Key &&key)
  {
    return this->lookup_or_add_default_as(std::move(key));
  }

  /**
   * Same as `lookup_or_add_default`, but accepts other key types that are supported by the hash
   * function.
   */
  template<typename ForwardKey> Value &lookup_or_add_default_as(ForwardKey &&key)
  {
    return this->lookup_or_add_cb_as(std::forward<ForwardKey>(key), []() { return Value(); });
  }

  /**
   * Calls the provided callback for every key-value-pair in the map. The callback is expected
   * to take a `const Key &` as first and a `const Value &` as second parameter.
   */
  template<typename FuncT> void foreach_item(const FuncT &func) const
  {
    uint32_t size = this->size();
    for (uint32_t i = 0; i < size; i++) {
      const Slot &slot = m_slots[i];
      if (slot.is_occupied()) {
        const Key &key = *slot.key();
        const Value &value = *slot.value();
        func(key, value);
      }
    }
  }

  /**
   * A utility iterator that reduces the amount of code when implementing the actual iterators.
   * This uses the "curiously recurring template pattern" (CRTP).
   */
  template<typename SubIterator> struct BaseIterator {
    Slot *m_slots;
    uint32_t m_total_slots;
    uint32_t m_current_slot;

    BaseIterator(const Slot *slots, uint32_t total_slots, uint32_t current_slot)
        : m_slots(const_cast<Slot *>(slots)),
          m_total_slots(total_slots),
          m_current_slot(current_slot)
    {
    }

    BaseIterator &operator++()
    {
      while (++m_current_slot < m_total_slots) {
        if (m_slots[m_current_slot].is_occupied()) {
          break;
        }
      }
      return *this;
    }

    friend bool operator!=(const BaseIterator &a, const BaseIterator &b)
    {
      BLI_assert(a.m_slots == b.m_slots);
      BLI_assert(a.m_total_slots == b.m_total_slots);
      return a.m_current_slot != b.m_current_slot;
    }

    SubIterator begin() const
    {
      for (uint32_t i = 0; i < m_total_slots; i++) {
        if (m_slots[i].is_occupied()) {
          return SubIterator(m_slots, m_total_slots, i);
        }
      }
      return this->end();
    }

    SubIterator end() const
    {
      return SubIterator(m_slots, m_total_slots, m_total_slots);
    }

    Slot &current_slot() const
    {
      return m_slots[m_current_slot];
    }
  };

  class KeyIterator final : public BaseIterator<KeyIterator> {
   public:
    KeyIterator(const Slot *slots, uint32_t total_slots, uint32_t current_slot)
        : BaseIterator<KeyIterator>(slots, total_slots, current_slot)
    {
    }

    const Key &operator*() const
    {
      return *this->current_slot().key();
    }
  };

  class ValueIterator final : public BaseIterator<ValueIterator> {
   public:
    ValueIterator(const Slot *slots, uint32_t total_slots, uint32_t current_slot)
        : BaseIterator<ValueIterator>(slots, total_slots, current_slot)
    {
    }

    const Value &operator*() const
    {
      return *this->current_slot().value();
    }
  };

  class MutableValueIterator final : public BaseIterator<MutableValueIterator> {
   public:
    MutableValueIterator(const Slot *slots, uint32_t total_slots, uint32_t current_slot)
        : BaseIterator<MutableValueIterator>(slots, total_slots, current_slot)
    {
    }

    Value &operator*()
    {
      return *this->current_slot().value();
    }
  };

  struct Item {
    const Key &key;
    const Value &value;
  };

  struct MutableItem {
    const Key &key;
    Value &value;

    operator Item() const
    {
      return Item{key, value};
    }
  };

  class ItemIterator final : public BaseIterator<ItemIterator> {
   public:
    ItemIterator(const Slot *slots, uint32_t total_slots, uint32_t current_slot)
        : BaseIterator<ItemIterator>(slots, total_slots, current_slot)
    {
    }

    Item operator*() const
    {
      const Slot &slot = this->current_slot();
      return {*slot.key(), *slot.value()};
    }
  };

  class MutableItemIterator final : public BaseIterator<MutableItemIterator> {
   public:
    MutableItemIterator(const Slot *slots, uint32_t total_slots, uint32_t current_slot)
        : BaseIterator<MutableItemIterator>(slots, total_slots, current_slot)
    {
    }

    MutableItem operator*() const
    {
      Slot &slot = this->current_slot();
      return {*slot.key(), *slot.value()};
    }
  };

  /**
   * Allows writing a range-for loop that iterates over all keys. The iterator is invalidated, when
   * the map is changed.
   */
  KeyIterator keys() const
  {
    return KeyIterator(m_slots.data(), m_slots.size(), 0);
  }

  /**
   * Returns an iterator over all values in the map. The iterator is invalidated, when the map is
   * changed.
   */
  ValueIterator values() const
  {
    return ValueIterator(m_slots.data(), m_slots.size(), 0);
  }

  /**
   * Returns an iterator over all values in the map and allows you to change the values. The
   * iterator is invalidated, when the map is changed.
   */
  MutableValueIterator values()
  {
    return MutableValueIterator(m_slots.data(), m_slots.size(), 0);
  }

  /**
   * Returns an iterator over all key-value-pairs in the map. The key-value-pairs are stored in
   * a temporary struct with a .key and a .value field.The iterator is invalidated, when the map is
   * changed.
   */
  ItemIterator items() const
  {
    return ItemIterator(m_slots.data(), m_slots.size(), 0);
  }

  /**
   * Returns an iterator over all key-value-pairs in the map. The key-value-pairs are stored in
   * a temporary struct with a .key and a .value field. The iterator is invalidated, when the map
   * is changed.
   *
   * This iterator also allows you to modify the value (but not the key).
   */
  MutableItemIterator items()
  {
    return MutableItemIterator(m_slots.data(), m_slots.size(), 0);
  }

  /**
   * Print common statistics like size and collision count. This is useful for debugging purposes.
   */
  void print_stats(StringRef name = "") const
  {
    HashTableStats stats(*this, this->keys());
    stats.print(name);
  }

  /**
   * Return the number of key-value-pairs that are stored in the map.
   */
  uint32_t size() const
  {
    return m_occupied_and_removed_slots - m_removed_slots;
  }

  /**
   * Returns true if there are no elements in the map.
   *
   * This is similar to std::unordered_map::empty.
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
   * Returns the approximate memory requirements of the map in bytes. This becomes more exact the
   * larger the map becomes.
   */
  uint32_t size_in_bytes() const
  {
    return (uint32_t)(sizeof(Slot) * m_slots.size());
  }

  /**
   * Potentially resize the map such that the specified number of elements can be added without
   * another grow operation.
   */
  void reserve(uint32_t n)
  {
    if (m_usable_slots < n) {
      this->realloc_and_reinsert(n);
    }
  }

  /**
   * Removes all key-value-pairs from the map.
   */
  void clear()
  {
    this->~Map();
    new (this) Map();
  }

  /**
   * Get the number of collisions that the probing strategy has to go through to find the key or
   * determine that it is not in the map.
   */
  uint32_t count_collisions(const Key &key) const
  {
    return this->count_collisions__impl(key, m_hash(key));
  }

 private:
  BLI_NOINLINE void realloc_and_reinsert(uint32_t min_usable_slots)
  {
    uint32_t total_slots, usable_slots;
    m_max_load_factor.compute_total_and_usable_slots(
        SlotArray::inline_buffer_capacity(), min_usable_slots, &total_slots, &usable_slots);
    uint32_t new_slot_mask = total_slots - 1;

    /**
     * Optimize the case when the map was empty beforehand. We can avoid some copies here.
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
    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        return false;
      }
      if (slot.contains(key, m_is_equal, hash)) {
        return true;
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey, typename ForwardValue>
  void add_new__impl(ForwardKey &&key, ForwardValue &&value, uint32_t hash)
  {
    BLI_assert(!this->contains_as(key));

    this->ensure_can_add();
    m_occupied_and_removed_slots++;

    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        slot.occupy(std::forward<ForwardKey>(key), std::forward<ForwardValue>(value), hash);
        return;
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey, typename ForwardValue>
  bool add__impl(ForwardKey &&key, ForwardValue &&value, uint32_t hash)
  {
    this->ensure_can_add();

    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        slot.occupy(std::forward<ForwardKey>(key), std::forward<ForwardValue>(value), hash);
        m_occupied_and_removed_slots++;
        return true;
      }
      if (slot.contains(key, m_is_equal, hash)) {
        return false;
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey> bool remove__impl(const ForwardKey &key, uint32_t hash)
  {
    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, m_is_equal, hash)) {
        slot.remove();
        m_removed_slots++;
        return true;
      }
      if (slot.is_empty()) {
        return false;
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey> void remove_contained__impl(const ForwardKey &key, uint32_t hash)
  {
    BLI_assert(this->contains_as(key));

    m_removed_slots++;

    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, m_is_equal, hash)) {
        slot.remove();
        return;
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey> Value pop__impl(const ForwardKey &key, uint32_t hash)
  {
    BLI_assert(this->contains_as(key));

    m_removed_slots++;

    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, m_is_equal, hash)) {
        Value value = std::move(*slot.value());
        slot.remove();
        return value;
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey> Optional<Value> pop_try__impl(const ForwardKey &key, uint32_t hash)
  {
    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, m_is_equal, hash)) {
        Optional<Value> value = std::move(*slot.value());
        slot.remove();
        m_removed_slots++;
        return value;
      }
      if (slot.is_empty()) {
        return {};
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey, typename ForwardValue>
  Value pop_default__impl(const ForwardKey &key, ForwardValue &&default_value, uint32_t hash)
  {
    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, m_is_equal, hash)) {
        Value value = std::move(*slot.value());
        slot.remove();
        m_removed_slots++;
        return value;
      }
      if (slot.is_empty()) {
        return std::forward<ForwardValue>(default_value);
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey, typename CreateValueF, typename ModifyValueF>
  auto add_or_modify__impl(ForwardKey &&key,
                           const CreateValueF &create_value,
                           const ModifyValueF &modify_value,
                           uint32_t hash) -> decltype(create_value(nullptr))
  {
    using CreateReturnT = decltype(create_value(nullptr));
    using ModifyReturnT = decltype(modify_value(nullptr));
    BLI_STATIC_ASSERT((std::is_same<CreateReturnT, ModifyReturnT>::value),
                      "Both callbacks should return the same type.");

    this->ensure_can_add();

    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        m_occupied_and_removed_slots++;
        slot.occupy_without_value(std::forward<ForwardKey>(key), hash);
        Value *value_ptr = slot.value();
        return create_value(value_ptr);
      }
      if (slot.contains(key, m_is_equal, hash)) {
        Value *value_ptr = slot.value();
        return modify_value(value_ptr);
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey, typename CreateValueF>
  Value &lookup_or_add_cb__impl(ForwardKey &&key, const CreateValueF &create_value, uint32_t hash)
  {
    this->ensure_can_add();

    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        slot.occupy(std::forward<ForwardKey>(key), create_value(), hash);
        m_occupied_and_removed_slots++;
        return *slot.value();
      }
      if (slot.contains(key, m_is_equal, hash)) {
        return *slot.value();
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey, typename ForwardValue>
  Value &lookup_or_add__impl(ForwardKey &&key, ForwardValue &&value, uint32_t hash)
  {
    this->ensure_can_add();

    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        slot.occupy(std::forward<ForwardKey>(key), std::forward<ForwardValue>(value), hash);
        m_occupied_and_removed_slots++;
        return *slot.value();
      }
      if (slot.contains(key, m_is_equal, hash)) {
        return *slot.value();
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey, typename ForwardValue>
  bool add_overwrite__impl(ForwardKey &&key, ForwardValue &&value, uint32_t hash)
  {
    auto create_func = [&](Value *ptr) {
      new ((void *)ptr) Value(std::forward<ForwardValue>(value));
      return true;
    };
    auto modify_func = [&](Value *ptr) {
      *ptr = std::forward<ForwardValue>(value);
      return false;
    };
    return this->add_or_modify__impl(
        std::forward<ForwardKey>(key), create_func, modify_func, hash);
  }

  template<typename ForwardKey>
  const Value *lookup_ptr__impl(const ForwardKey &key, uint32_t hash) const
  {
    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        return nullptr;
      }
      if (slot.contains(key, m_is_equal, hash)) {
        return slot.value();
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey>
  uint32_t count_collisions__impl(const ForwardKey &key, uint32_t hash) const
  {
    uint32_t collisions = 0;

    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, m_is_equal, hash)) {
        return collisions;
      }
      if (slot.is_empty()) {
        return collisions;
      }
      collisions++;
    }
    MAP_SLOT_PROBING_END();
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
 * A wrapper for std::unordered_map with the API of blender::Map. This can be used for
 * benchmarking.
 */
template<typename Key, typename Value> class StdUnorderedMapWrapper {
 private:
  using MapType = std::unordered_map<Key, Value, blender::DefaultHash<Key>>;
  MapType m_map;

 public:
  uint32_t size() const
  {
    return (uint32_t)m_map.size();
  }

  bool is_empty() const
  {
    return m_map.empty();
  }

  void reserve(uint32_t n)
  {
    m_map.reserve(n);
  }

  template<typename ForwardKey, typename ForwardValue>
  void add_new(ForwardKey &&key, ForwardValue &&value)
  {
    m_map.insert({std::forward<ForwardKey>(key), std::forward<ForwardValue>(value)});
  }

  template<typename ForwardKey, typename ForwardValue>
  bool add(ForwardKey &&key, ForwardValue &&value)
  {
    return m_map.insert({std::forward<ForwardKey>(key), std::forward<ForwardValue>(value)}).second;
  }

  bool contains(const Key &key) const
  {
    return m_map.find(key) != m_map.end();
  }

  bool remove(const Key &key)
  {
    return (bool)m_map.erase(key);
  }

  Value &lookup(const Key &key)
  {
    return m_map.find(key)->second;
  }

  const Value &lookup(const Key &key) const
  {
    return m_map.find(key)->second;
  }

  void clear()
  {
    m_map.clear();
  }

  void print_stats(StringRef UNUSED(name) = "") const
  {
  }
};

}  // namespace blender

#endif /* __BLI_MAP_HH__ */
