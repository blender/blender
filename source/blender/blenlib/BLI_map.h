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

#ifndef __BLI_MAP_H__
#define __BLI_MAP_H__

/** \file
 * \ingroup bli
 *
 * This file provides a map implementation that uses open addressing with probing.
 *
 * The key and value objects are stored directly in the hash table to avoid indirect memory
 * lookups. Keys and values are stored in groups of four to avoid wasting memory due to padding.
 */

#include "BLI_array_ref.h"
#include "BLI_hash_cxx.h"
#include "BLI_open_addressing.h"

namespace BLI {

// clang-format off

#define ITER_SLOTS_BEGIN(KEY, ARRAY, OPTIONAL_CONST, R_ITEM, R_OFFSET) \
  uint32_t hash = DefaultHash<KeyT>{}(KEY); \
  uint32_t perturb = hash; \
  while (true) { \
    uint32_t item_index = (hash & ARRAY.slot_mask()) >> OFFSET_SHIFT; \
    uint8_t R_OFFSET = hash & OFFSET_MASK; \
    uint8_t initial_offset = R_OFFSET; \
    OPTIONAL_CONST Item &R_ITEM = ARRAY.item(item_index); \
    do {

#define ITER_SLOTS_END(R_OFFSET) \
      R_OFFSET = (R_OFFSET + 1u) & OFFSET_MASK; \
    } while (R_OFFSET != initial_offset); \
    perturb >>= 5; \
    hash = hash * 5 + 1 + perturb; \
  } ((void)0)

// clang-format on

template<typename KeyT, typename ValueT, typename Allocator = GuardedAllocator> class Map {
 private:
  static constexpr uint OFFSET_MASK = 3;
  static constexpr uint OFFSET_SHIFT = 2;

  class Item {
   private:
    static constexpr uint8_t IS_EMPTY = 0;
    static constexpr uint8_t IS_SET = 1;
    static constexpr uint8_t IS_DUMMY = 2;

    uint8_t m_status[4];
    char m_keys[4 * sizeof(KeyT)];
    char m_values[4 * sizeof(ValueT)];

   public:
    static constexpr uint slots_per_item = 4;

    Item()
    {
      for (uint offset = 0; offset < 4; offset++) {
        m_status[offset] = IS_EMPTY;
      }
    }

    ~Item()
    {
      for (uint offset = 0; offset < 4; offset++) {
        if (m_status[offset] == IS_SET) {
          this->key(offset)->~KeyT();
          this->value(offset)->~ValueT();
        }
      }
    }

    Item(const Item &other)
    {
      for (uint offset = 0; offset < 4; offset++) {
        uint8_t status = other.m_status[offset];
        m_status[offset] = status;
        if (status == IS_SET) {
          new (this->key(offset)) KeyT(*other.key(offset));
          new (this->value(offset)) ValueT(*other.value(offset));
        }
      }
    }

    Item(Item &&other) noexcept
    {
      for (uint offset = 0; offset < 4; offset++) {
        uint8_t status = other.m_status[offset];
        m_status[offset] = status;
        if (status == IS_SET) {
          new (this->key(offset)) KeyT(std::move(*other.key(offset)));
          new (this->value(offset)) ValueT(std::move(*other.value(offset)));
        }
      }
    }

    bool has_key(uint offset, const KeyT &key) const
    {
      return m_status[offset] == IS_SET && key == *this->key(offset);
    }

    bool is_set(uint offset) const
    {
      return m_status[offset] == IS_SET;
    }

    bool is_empty(uint offset) const
    {
      return m_status[offset] == IS_EMPTY;
    }

    bool is_dummy(uint offset) const
    {
      return m_status[offset] == IS_DUMMY;
    }

    KeyT *key(uint offset) const
    {
      return (KeyT *)(m_keys + offset * sizeof(KeyT));
    }

    ValueT *value(uint offset) const
    {
      return (ValueT *)(m_values + offset * sizeof(ValueT));
    }

    template<typename ForwardKeyT, typename ForwardValueT>
    void store(uint offset, ForwardKeyT &&key, ForwardValueT &&value)
    {
      BLI_assert(m_status[offset] != IS_SET);
      m_status[offset] = IS_SET;
      new (this->key(offset)) KeyT(std::forward<ForwardKeyT>(key));
      new (this->value(offset)) ValueT(std::forward<ForwardValueT>(value));
    }

    template<typename ForwardKeyT> void store_without_value(uint offset, ForwardKeyT &&key)
    {
      BLI_assert(m_status[offset] != IS_SET);
      m_status[offset] = IS_SET;
      new (this->key(offset)) KeyT(std::forward<ForwardKeyT>(key));
    }

    void set_dummy(uint offset)
    {
      BLI_assert(m_status[offset] == IS_SET);
      m_status[offset] = IS_DUMMY;
      destruct(this->key(offset));
      destruct(this->value(offset));
    }
  };

  using ArrayType = OpenAddressingArray<Item, 1, Allocator>;
  ArrayType m_array;

 public:
  Map() = default;

  /**
   * Allocate memory such that at least min_usable_slots can be added before the map has to grow
   * again.
   */
  void reserve(uint min_usable_slots)
  {
    if (m_array.slots_usable() < min_usable_slots) {
      this->grow(min_usable_slots);
    }
  }

  /**
   * Remove all elements from the map.
   */
  void clear()
  {
    this->~Map();
    new (this) Map();
  }

  /**
   * Insert a new key-value-pair in the map.
   * Asserts when the key existed before.
   */
  void add_new(const KeyT &key, const ValueT &value)
  {
    this->add_new__impl(key, value);
  }
  void add_new(const KeyT &key, ValueT &&value)
  {
    this->add_new__impl(key, std::move(value));
  }
  void add_new(KeyT &&key, const ValueT &value)
  {
    this->add_new__impl(std::move(key), value);
  }
  void add_new(KeyT &&key, ValueT &&value)
  {
    this->add_new__impl(std::move(key), std::move(value));
  }

  /**
   * Insert a new key-value-pair in the map if the key does not exist yet.
   * Returns true when the pair was newly inserted, otherwise false.
   */
  bool add(const KeyT &key, const ValueT &value)
  {
    return this->add__impl(key, value);
  }
  bool add(const KeyT &key, ValueT &&value)
  {
    return this->add__impl(key, std::move(value));
  }
  bool add(KeyT &&key, const ValueT &value)
  {
    return this->add__impl(std::move(key), value);
  }
  bool add(KeyT &&key, ValueT &&value)
  {
    return this->add__impl(std::move(key), std::move(value));
  }

  /**
   * Remove the key from the map.
   * Asserts when the key does not exist in the map.
   */
  void remove(const KeyT &key)
  {
    BLI_assert(this->contains(key));
    ITER_SLOTS_BEGIN (key, m_array, , item, offset) {
      if (item.has_key(offset, key)) {
        item.set_dummy(offset);
        m_array.update__set_to_dummy();
        return;
      }
    }
    ITER_SLOTS_END(offset);
  }

  /**
   * Get the value for the given key and remove it from the map.
   * Asserts when the key does not exist in the map.
   */
  ValueT pop(const KeyT &key)
  {
    BLI_assert(this->contains(key));
    ITER_SLOTS_BEGIN (key, m_array, , item, offset) {
      if (item.has_key(offset, key)) {
        ValueT value = std::move(*item.value(offset));
        item.set_dummy(offset);
        m_array.update__set_to_dummy();
        return value;
      }
    }
    ITER_SLOTS_END(offset);
  }

  /**
   * Returns true when the key exists in the map, otherwise false.
   */
  bool contains(const KeyT &key) const
  {
    ITER_SLOTS_BEGIN (key, m_array, const, item, offset) {
      if (item.is_empty(offset)) {
        return false;
      }
      else if (item.has_key(offset, key)) {
        return true;
      }
    }
    ITER_SLOTS_END(offset);
  }

  /**
   * First, checks if the key exists in the map.
   * If it does exist, call the modify function with a pointer to the corresponding value.
   * If it does not exist, call the create function with a pointer to where the value should be
   * created.
   *
   * Returns whatever is returned from one of the callback functions. Both callbacks have to return
   * the same type.
   *
   * CreateValueF: Takes a pointer to where the value should be created.
   * ModifyValueF: Takes a pointer to the value that should be modified.
   */
  template<typename CreateValueF, typename ModifyValueF>
  auto add_or_modify(const KeyT &key,
                     const CreateValueF &create_value,
                     const ModifyValueF &modify_value) -> decltype(create_value(nullptr))
  {
    return this->add_or_modify__impl(key, create_value, modify_value);
  }
  template<typename CreateValueF, typename ModifyValueF>
  auto add_or_modify(KeyT &&key,
                     const CreateValueF &create_value,
                     const ModifyValueF &modify_value) -> decltype(create_value(nullptr))
  {
    return this->add_or_modify__impl(std::move(key), create_value, modify_value);
  }

  /**
   * Similar to add, but overrides the value for the key when it exists already.
   */
  bool add_override(const KeyT &key, const ValueT &value)
  {
    return this->add_override__impl(key, value);
  }
  bool add_override(const KeyT &key, ValueT &&value)
  {
    return this->add_override__impl(key, std::move(value));
  }
  bool add_override(KeyT &&key, const ValueT &value)
  {
    return this->add_override__impl(std::move(key), value);
  }
  bool add_override(KeyT &&key, ValueT &&value)
  {
    return this->add_override__impl(std::move(key), std::move(value));
  }

  /**
   * Check if the key exists in the map.
   * Return a pointer to the value, when it exists.
   * Otherwise return nullptr.
   */
  const ValueT *lookup_ptr(const KeyT &key) const
  {
    ITER_SLOTS_BEGIN (key, m_array, const, item, offset) {
      if (item.is_empty(offset)) {
        return nullptr;
      }
      else if (item.has_key(offset, key)) {
        return item.value(offset);
      }
    }
    ITER_SLOTS_END(offset);
  }

  /**
   * Lookup the value that corresponds to the key.
   * Asserts when the key does not exist.
   */
  const ValueT &lookup(const KeyT &key) const
  {
    const ValueT *ptr = this->lookup_ptr(key);
    BLI_assert(ptr != nullptr);
    return *ptr;
  }

  ValueT *lookup_ptr(const KeyT &key)
  {
    const Map *const_this = this;
    return const_cast<ValueT *>(const_this->lookup_ptr(key));
  }

  ValueT &lookup(const KeyT &key)
  {
    const Map *const_this = this;
    return const_cast<ValueT &>(const_this->lookup(key));
  }

  /**
   * Check if the key exists in the map.
   * If it does, return a copy of the value.
   * Otherwise, return the default value.
   */
  ValueT lookup_default(const KeyT &key, ValueT default_value) const
  {
    const ValueT *ptr = this->lookup_ptr(key);
    if (ptr != nullptr) {
      return *ptr;
    }
    else {
      return default_value;
    }
  }

  /**
   * Return the value that corresponds to the given key.
   * If it does not exist yet, create and insert it first.
   */
  template<typename CreateValueF>
  ValueT &lookup_or_add(const KeyT &key, const CreateValueF &create_value)
  {
    return this->lookup_or_add__impl(key, create_value);
  }
  template<typename CreateValueF>
  ValueT &lookup_or_add(KeyT &&key, const CreateValueF &create_value)
  {
    return this->lookup_or_add__impl(std::move(key), create_value);
  }

  /**
   * Get the number of elements in the map.
   */
  uint32_t size() const
  {
    return m_array.slots_set();
  }

  template<typename FuncT> void foreach_item(const FuncT &func) const
  {
    for (const Item &item : m_array) {
      for (uint offset = 0; offset < 4; offset++) {
        if (item.is_set(offset)) {
          const KeyT &key = *item.key(offset);
          const ValueT &value = *item.value(offset);
          func(key, value);
        }
      }
    }
  }

  void print_table() const
  {
    std::cout << "Hash Table:\n";
    std::cout << "  Size: " << m_array.slots_set() << '\n';
    std::cout << "  Capacity: " << m_array.slots_total() << '\n';
    uint32_t item_index = 0;
    for (const Item &item : m_array) {
      std::cout << "   Item: " << item_index++ << '\n';
      for (uint offset = 0; offset < 4; offset++) {
        std::cout << "    " << offset << " \t";
        if (item.is_empty(offset)) {
          std::cout << "    <empty>\n";
        }
        else if (item.is_set(offset)) {
          const KeyT &key = *item.key(offset);
          const ValueT &value = *item.value(offset);
          uint32_t collisions = this->count_collisions(key);
          std::cout << "    " << key << " -> " << value << "  \t Collisions: " << collisions
                    << '\n';
        }
        else if (item.is_dummy(offset)) {
          std::cout << "    <dummy>\n";
        }
      }
    }
  }

  template<typename SubIterator> class BaseIterator {
   protected:
    const Map *m_map;
    uint32_t m_slot;

   public:
    BaseIterator(const Map *map, uint32_t slot) : m_map(map), m_slot(slot)
    {
    }

    BaseIterator &operator++()
    {
      m_slot = m_map->next_slot(m_slot + 1);
      return *this;
    }

    friend bool operator==(const BaseIterator &a, const BaseIterator &b)
    {
      BLI_assert(a.m_map == b.m_map);
      return a.m_slot == b.m_slot;
    }

    friend bool operator!=(const BaseIterator &a, const BaseIterator &b)
    {
      return !(a == b);
    }

    SubIterator begin() const
    {
      return SubIterator(m_map, m_map->next_slot(0));
    }

    SubIterator end() const
    {
      return SubIterator(m_map, m_map->m_array.slots_total());
    }
  };

  class KeyIterator final : public BaseIterator<KeyIterator> {
   public:
    KeyIterator(const Map *map, uint32_t slot) : BaseIterator<KeyIterator>(map, slot)
    {
    }

    const KeyT &operator*() const
    {
      uint32_t item_index = this->m_slot >> OFFSET_SHIFT;
      uint offset = this->m_slot & OFFSET_MASK;
      const Item &item = this->m_map->m_array.item(item_index);
      BLI_assert(item.is_set(offset));
      return *item.key(offset);
    }
  };

  class ValueIterator final : public BaseIterator<ValueIterator> {
   public:
    ValueIterator(const Map *map, uint32_t slot) : BaseIterator<ValueIterator>(map, slot)
    {
    }

    ValueT &operator*() const
    {
      uint32_t item_index = this->m_slot >> OFFSET_SHIFT;
      uint offset = this->m_slot & OFFSET_MASK;
      const Item &item = this->m_map->m_array.item(item_index);
      BLI_assert(item.is_set(offset));
      return *item.value(offset);
    }
  };

  class ItemIterator final : public BaseIterator<ItemIterator> {
   public:
    ItemIterator(const Map *map, uint32_t slot) : BaseIterator<ItemIterator>(map, slot)
    {
    }

    struct UserItem {
      const KeyT &key;
      ValueT &value;

      friend std::ostream &operator<<(std::ostream &stream, const Item &item)
      {
        stream << item.key << " -> " << item.value;
        return stream;
      }
    };

    UserItem operator*() const
    {
      uint32_t item_index = this->m_slot >> OFFSET_SHIFT;
      uint offset = this->m_slot & OFFSET_MASK;
      const Item &item = this->m_map->m_array.item(item_index);
      BLI_assert(item.is_set(offset));
      return {*item.key(offset), *item.value(offset)};
    }
  };

  template<typename SubIterator> friend class BaseIterator;

  /**
   * Iterate over all keys in the map.
   */
  KeyIterator keys() const
  {
    return KeyIterator(this, 0);
  }

  /**
   * Iterate over all values in the map.
   */
  ValueIterator values() const
  {
    return ValueIterator(this, 0);
  }

  /**
   * Iterate over all key-value-pairs in the map.
   * They can be accessed with item.key and item.value.
   */
  ItemIterator items() const
  {
    return ItemIterator(this, 0);
  }

 private:
  uint32_t next_slot(uint32_t slot) const
  {
    for (; slot < m_array.slots_total(); slot++) {
      uint32_t item_index = slot >> OFFSET_SHIFT;
      uint offset = slot & OFFSET_MASK;
      const Item &item = m_array.item(item_index);
      if (item.is_set(offset)) {
        return slot;
      }
    }
    return slot;
  }

  uint32_t count_collisions(const KeyT &key) const
  {
    uint32_t collisions = 0;
    ITER_SLOTS_BEGIN (key, m_array, const, item, offset) {
      if (item.is_empty(offset) || item.has_key(offset, key)) {
        return collisions;
      }
      collisions++;
    }
    ITER_SLOTS_END(offset);
  }

  void ensure_can_add()
  {
    if (UNLIKELY(m_array.should_grow())) {
      this->grow(this->size() + 1);
    }
  }

  BLI_NOINLINE void grow(uint32_t min_usable_slots)
  {
    ArrayType new_array = m_array.init_reserved(min_usable_slots);
    for (Item &old_item : m_array) {
      for (uint offset = 0; offset < 4; offset++) {
        if (old_item.is_set(offset)) {
          this->add_after_grow(*old_item.key(offset), *old_item.value(offset), new_array);
        }
      }
    }
    m_array = std::move(new_array);
  }

  void add_after_grow(KeyT &key, ValueT &value, ArrayType &new_array)
  {
    ITER_SLOTS_BEGIN (key, new_array, , item, offset) {
      if (item.is_empty(offset)) {
        item.store(offset, std::move(key), std::move(value));
        return;
      }
    }
    ITER_SLOTS_END(offset);
  }

  template<typename ForwardKeyT, typename ForwardValueT>
  bool add_override__impl(ForwardKeyT &&key, ForwardValueT &&value)
  {
    auto create_func = [&](ValueT *dst) {
      new (dst) ValueT(std::forward<ForwardValueT>(value));
      return true;
    };
    auto modify_func = [&](ValueT *old_value) {
      *old_value = std::forward<ForwardValueT>(value);
      return false;
    };
    return this->add_or_modify(std::forward<ForwardKeyT>(key), create_func, modify_func);
  }

  template<typename ForwardKeyT, typename ForwardValueT>
  bool add__impl(ForwardKeyT &&key, ForwardValueT &&value)
  {
    this->ensure_can_add();

    ITER_SLOTS_BEGIN (key, m_array, , item, offset) {
      if (item.is_empty(offset)) {
        item.store(offset, std::forward<ForwardKeyT>(key), std::forward<ForwardValueT>(value));
        m_array.update__empty_to_set();
        return true;
      }
      else if (item.has_key(offset, key)) {
        return false;
      }
    }
    ITER_SLOTS_END(offset);
  }

  template<typename ForwardKeyT, typename ForwardValueT>
  void add_new__impl(ForwardKeyT &&key, ForwardValueT &&value)
  {
    BLI_assert(!this->contains(key));
    this->ensure_can_add();

    ITER_SLOTS_BEGIN (key, m_array, , item, offset) {
      if (item.is_empty(offset)) {
        item.store(offset, std::forward<ForwardKeyT>(key), std::forward<ForwardValueT>(value));
        m_array.update__empty_to_set();
        return;
      }
    }
    ITER_SLOTS_END(offset);
  }

  template<typename ForwardKeyT, typename CreateValueF, typename ModifyValueF>
  auto add_or_modify__impl(ForwardKeyT &&key,
                           const CreateValueF &create_value,
                           const ModifyValueF &modify_value) -> decltype(create_value(nullptr))
  {
    using CreateReturnT = decltype(create_value(nullptr));
    using ModifyReturnT = decltype(modify_value(nullptr));
    BLI_STATIC_ASSERT((std::is_same<CreateReturnT, ModifyReturnT>::value),
                      "Both callbacks should return the same type.");

    this->ensure_can_add();

    ITER_SLOTS_BEGIN (key, m_array, , item, offset) {
      if (item.is_empty(offset)) {
        m_array.update__empty_to_set();
        item.store_without_value(offset, std::forward<ForwardKeyT>(key));
        ValueT *value_ptr = item.value(offset);
        return create_value(value_ptr);
      }
      else if (item.has_key(offset, key)) {
        ValueT *value_ptr = item.value(offset);
        return modify_value(value_ptr);
      }
    }
    ITER_SLOTS_END(offset);
  }

  template<typename ForwardKeyT, typename CreateValueF>
  ValueT &lookup_or_add__impl(ForwardKeyT &&key, const CreateValueF &create_value)
  {
    this->ensure_can_add();

    ITER_SLOTS_BEGIN (key, m_array, , item, offset) {
      if (item.is_empty(offset)) {
        item.store(offset, std::forward<ForwardKeyT>(key), create_value());
        m_array.update__empty_to_set();
        return *item.value(offset);
      }
      else if (item.has_key(offset, key)) {
        return *item.value(offset);
      }
    }
    ITER_SLOTS_END(offset);
  }
};

#undef ITER_SLOTS_BEGIN
#undef ITER_SLOTS_END

}  // namespace BLI

#endif /* __BLI_MAP_H__ */
