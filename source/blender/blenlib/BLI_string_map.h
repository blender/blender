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

#ifndef __BLI_STRING_MAP_H__
#define __BLI_STRING_MAP_H__

/** \file
 * \ingroup bli
 *
 * This tries to solve the issue that a normal map with std::string as key might do many
 * allocations when the keys are longer than 16 bytes (the usual small string optimization size).
 *
 * For now this still uses std::string, but having this abstraction in place will make it easier to
 * make it more efficient later on. Also, even if we will never implement this optimization, having
 * a special map with string keys can be quite handy. */

#include "BLI_map.h"
#include "BLI_optional.h"
#include "BLI_string_ref.h"
#include "BLI_vector.h"

namespace BLI {

// clang-format off

#define ITER_SLOTS_BEGIN(HASH, ARRAY, OPTIONAL_CONST, R_ITEM, R_OFFSET) \
  uint32_t hash_copy = HASH; \
  uint32_t perturb = HASH; \
  while (true) { \
    uint32_t item_index = (hash_copy & ARRAY.slot_mask()) >> OFFSET_SHIFT; \
    uint8_t R_OFFSET = hash_copy & OFFSET_MASK; \
    uint8_t initial_offset = R_OFFSET; \
    OPTIONAL_CONST Item &R_ITEM = ARRAY.item(item_index); \
    do {

#define ITER_SLOTS_END(R_OFFSET) \
      R_OFFSET = (R_OFFSET + 1) & OFFSET_MASK; \
    } while (R_OFFSET != initial_offset); \
    perturb >>= 5; \
    hash_copy = hash_copy * 5 + 1 + perturb; \
  } ((void)0)

// clang-format on

template<typename T, typename Allocator = GuardedAllocator> class StringMap {
 private:
  static constexpr uint32_t OFFSET_MASK = 3;
  static constexpr uint32_t OFFSET_SHIFT = 2;

  class Item {
   private:
    static constexpr int32_t IS_EMPTY = -1;

    uint32_t m_hashes[4];
    int32_t m_indices[4];
    char m_values[sizeof(T) * 4];

   public:
    static constexpr uint slots_per_item = 4;

    Item()
    {
      for (uint offset = 0; offset < 4; offset++) {
        m_indices[offset] = IS_EMPTY;
      }
    }

    ~Item()
    {
      for (uint offset = 0; offset < 4; offset++) {
        if (this->is_set(offset)) {
          destruct(this->value(offset));
        }
      }
    }

    Item(const Item &other)
    {
      for (uint offset = 0; offset < 4; offset++) {
        m_indices[offset] = other.m_indices[offset];
        if (other.is_set(offset)) {
          m_hashes[offset] = other.m_hashes[offset];
          new (this->value(offset)) T(*other.value(offset));
        }
      }
    }

    Item(Item &&other) noexcept
    {
      for (uint offset = 0; offset < 4; offset++) {
        m_indices[offset] = other.m_indices[offset];
        if (other.is_set(offset)) {
          m_hashes[offset] = other.m_hashes[offset];
          new (this->value(offset)) T(std::move(*other.value(offset)));
        }
      }
    }

    uint32_t index(uint offset) const
    {
      return m_indices[offset];
    }

    uint32_t hash(uint offset) const
    {
      return m_hashes[offset];
    }

    T *value(uint offset) const
    {
      return (T *)POINTER_OFFSET(m_values, offset * sizeof(T));
    }

    bool is_set(uint offset) const
    {
      return m_indices[offset] >= 0;
    }

    bool is_empty(uint offset) const
    {
      return m_indices[offset] == IS_EMPTY;
    }

    bool has_hash(uint offset, uint32_t hash) const
    {
      BLI_assert(this->is_set(offset));
      return m_hashes[offset] == hash;
    }

    bool has_exact_key(uint offset, StringRef key, const Vector<char> &chars) const
    {
      return key == this->get_key(offset, chars);
    }

    StringRefNull get_key(uint offset, const Vector<char> &chars) const
    {
      const char *ptr = chars.begin() + m_indices[offset];
      uint length = *(uint *)ptr;
      const char *start = ptr + sizeof(uint);
      return StringRefNull(start, length);
    }

    template<typename ForwardT>
    void store(uint offset, uint32_t hash, uint32_t index, ForwardT &&value)
    {
      BLI_assert(!this->is_set(offset));
      m_hashes[offset] = hash;
      m_indices[offset] = index;
      new (this->value(offset)) T(std::forward<ForwardT>(value));
    }
  };

  using ArrayType = OpenAddressingArray<Item, 1, Allocator>;
  ArrayType m_array;
  Vector<char> m_chars;

 public:
  StringMap() = default;

  /**
   * Get the number of key-value pairs in the map.
   */
  uint size() const
  {
    return m_array.slots_set();
  }

  /**
   * Add a new element to the map. It is assumed that the key did not exist before.
   */
  void add_new(StringRef key, const T &value)
  {
    this->add_new__impl(key, value);
  }
  void add_new(StringRef key, T &&value)
  {
    this->add_new__impl(key, std::move(value));
  }

  /**
   * Add a new element to the map if the key does not exist yet.
   */
  void add(StringRef key, const T &value)
  {
    if (!this->contains(key)) {
      this->add_new(key, value);
    }
  }
  void add(StringRef key, T &&value)
  {
    if (!this->contains(key)) {
      this->add_new(key, std::move(value));
    }
  }

  /**
   * Return true when the key exists in the map, otherwise false.
   */
  bool contains(StringRef key) const
  {
    uint32_t hash = this->compute_string_hash(key);
    ITER_SLOTS_BEGIN (hash, m_array, const, item, offset) {
      if (item.is_empty(offset)) {
        return false;
      }
      else if (item.has_hash(offset, hash) && item.has_exact_key(offset, key, m_chars)) {
        return true;
      }
    }
    ITER_SLOTS_END(offset);
  }

  /**
   * Get a reference to the value corresponding to a key. It is assumed that the key does exist.
   */
  const T &lookup(StringRef key) const
  {
    BLI_assert(this->contains(key));
    T *found_value = nullptr;
    uint32_t hash = this->compute_string_hash(key);
    ITER_SLOTS_BEGIN (hash, m_array, const, item, offset) {
      if (item.is_empty(offset)) {
        return *found_value;
      }
      else if (item.has_hash(offset, hash)) {
        if (found_value == nullptr) {
          /* Common case: the first slot with the correct hash contains the key.
           * However, still need to iterate until the next empty slot to make sure there is no
           * other key with the exact same hash. */
          /* TODO: Check if we can guarantee that every hash only exists once in some cases. */
          found_value = item.value(offset);
        }
        else if (item.has_exact_key(offset, key, m_chars)) {
          /* Found the hash more than once, now check for actual string equality. */
          return *item.value(offset);
        }
      }
    }
    ITER_SLOTS_END(offset);
  }

  T &lookup(StringRef key)
  {
    return const_cast<T &>(const_cast<const StringMap *>(this)->lookup(key));
  }

  /**
   * Get a pointer to the value corresponding to the key. Return nullptr, if the key does not
   * exist.
   */
  const T *lookup_ptr(StringRef key) const
  {
    uint32_t hash = this->compute_string_hash(key);
    ITER_SLOTS_BEGIN (hash, m_array, const, item, offset) {
      if (item.is_empty(offset)) {
        return nullptr;
      }
      else if (item.has_hash(offset, hash) && item.has_exact_key(offset, key, m_chars)) {
        return item.value(offset);
      }
    }
    ITER_SLOTS_END(offset);
  }

  T *lookup_ptr(StringRef key)
  {
    return const_cast<T *>(const_cast<const StringMap *>(this)->lookup_ptr(key));
  }

  Optional<T> try_lookup(StringRef key) const
  {
    return Optional<T>::FromPointer(this->lookup_ptr(key));
  }

  /**
   * Get a copy of the value corresponding to the key. If the key does not exist, return the
   * default value.
   */
  T lookup_default(StringRef key, const T &default_value) const
  {
    const T *ptr = this->lookup_ptr(key);
    if (ptr != nullptr) {
      return *ptr;
    }
    else {
      return default_value;
    }
  }

  /**
   * Do a linear search over all items to find a key for a value.
   */
  StringRefNull find_key_for_value(const T &value) const
  {
    for (const Item &item : m_array) {
      for (uint offset = 0; offset < 4; offset++) {
        if (item.is_set(offset) && value == *item.value(offset)) {
          return item.get_key(offset, m_chars);
        }
      }
    }
    BLI_assert(false);
    return {};
  }

  /**
   * Run a function for every value in the map.
   */
  template<typename FuncT> void foreach_value(const FuncT &func)
  {
    for (Item &item : m_array) {
      for (uint offset = 0; offset < 4; offset++) {
        if (item.is_set(offset)) {
          func(*item.value(offset));
        }
      }
    }
  }

  /**
   * Run a function for every key in the map.
   */
  template<typename FuncT> void foreach_key(const FuncT &func)
  {
    for (Item &item : m_array) {
      for (uint offset = 0; offset < 4; offset++) {
        if (item.is_set(offset)) {
          StringRefNull key = item.get_key(offset, m_chars);
          func(key);
        }
      }
    }
  }

  /**
   * Run a function for every key-value-pair in the map.
   */
  template<typename FuncT> void foreach_item(const FuncT &func)
  {
    for (Item &item : m_array) {
      for (uint offset = 0; offset < 4; offset++) {
        if (item.is_set(offset)) {
          StringRefNull key = item.get_key(offset, m_chars);
          T &value = *item.value(offset);
          func(key, value);
        }
      }
    }
  }

  template<typename FuncT> void foreach_item(const FuncT &func) const
  {
    for (const Item &item : m_array) {
      for (uint offset = 0; offset < 4; offset++) {
        if (item.is_set(offset)) {
          StringRefNull key = item.get_key(offset, m_chars);
          const T &value = *item.value(offset);
          func(key, value);
        }
      }
    }
  }

 private:
  uint32_t compute_string_hash(StringRef key) const
  {
    /* TODO: check if this can be optimized more because we know the key length already. */
    uint32_t hash = 5381;
    for (char c : key) {
      hash = hash * 33 + c;
    }
    return hash;
  }

  uint32_t save_key_in_array(StringRef key)
  {
    uint index = m_chars.size();
    uint string_size = key.size();
    m_chars.extend(ArrayRef<char>((char *)&string_size, sizeof(uint)));
    m_chars.extend(key);
    m_chars.append('\0');
    return index;
  }

  StringRefNull key_from_index(uint32_t index) const
  {
    const char *ptr = m_chars.begin() + index;
    uint length = *(uint *)ptr;
    const char *start = ptr + sizeof(uint);
    return StringRefNull(start, length);
  }

  void ensure_can_add()
  {
    if (UNLIKELY(m_array.should_grow())) {
      this->grow(this->size() + 1);
    }
  }

  BLI_NOINLINE void grow(uint min_usable_slots)
  {
    ArrayType new_array = m_array.init_reserved(min_usable_slots);
    for (Item &old_item : m_array) {
      for (uint offset = 0; offset < 4; offset++) {
        if (old_item.is_set(offset)) {
          this->add_after_grow(
              *old_item.value(offset), old_item.hash(offset), old_item.index(offset), new_array);
        }
      }
    }
    m_array = std::move(new_array);
  }

  void add_after_grow(T &value, uint32_t hash, uint32_t index, ArrayType &new_array)
  {
    ITER_SLOTS_BEGIN (hash, new_array, , item, offset) {
      if (item.is_empty(offset)) {
        item.store(offset, hash, index, std::move(value));
        return;
      }
    }
    ITER_SLOTS_END(offset);
  }

  template<typename ForwardT> void add_new__impl(StringRef key, ForwardT &&value)
  {
    BLI_assert(!this->contains(key));
    this->ensure_can_add();
    uint32_t hash = this->compute_string_hash(key);
    ITER_SLOTS_BEGIN (hash, m_array, , item, offset) {
      if (item.is_empty(offset)) {
        uint32_t index = this->save_key_in_array(key);
        item.store(offset, hash, index, std::forward<ForwardT>(value));
        m_array.update__empty_to_set();
        return;
      }
    }
    ITER_SLOTS_END(offset);
  }
};

#undef ITER_SLOTS_BEGIN
#undef ITER_SLOTS_END

}  // namespace BLI

#endif /* __BLI_STRING_MAP_H__ */
