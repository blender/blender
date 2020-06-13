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

#ifndef __BLI_MAP_SLOTS_HH__
#define __BLI_MAP_SLOTS_HH__

/** \file
 * \ingroup bli
 *
 * This file contains slot types that are supposed to be used with blender::Map.
 *
 * Every slot type has to be able to hold a value of type Key, a value of type Value and state
 * information. A map slot has three possible states: empty, occupied and removed.
 *
 * When a slot is occupied, it stores instances of type Key and Value.
 *
 * A map slot type has to implement a couple of methods that are explained in SimpleMapSlot.
 * A slot type is assumed to be trivially destructible, when it is not in occupied state. So the
 * destructor might not be called in that case.
 *
 * Possible Improvements:
 * - Implement slot type that stores the hash.
 */

#include "BLI_memory_utils.hh"

namespace blender {

/**
 * The simplest possible map slot. It stores the slot state and the optional key and value
 * instances in separate variables. Depending on the alignment requirement of the key and value,
 * many bytes might be wasted.
 */
template<typename Key, typename Value> class SimpleMapSlot {
 private:
  enum State : uint8_t {
    Empty = 0,
    Occupied = 1,
    Removed = 2,
  };

  State m_state;
  AlignedBuffer<sizeof(Key), alignof(Key)> m_key_buffer;
  AlignedBuffer<sizeof(Value), alignof(Value)> m_value_buffer;

 public:
  /**
   * After the default constructor has run, the slot has to be in the empty state.
   */
  SimpleMapSlot()
  {
    m_state = Empty;
  }

  /**
   * The destructor also has to destruct the key and value, if the slot is currently occupied.
   */
  ~SimpleMapSlot()
  {
    if (m_state == Occupied) {
      this->key()->~Key();
      this->value()->~Value();
    }
  }

  /**
   * The copy constructor has to copy the state. If the other slot was occupied, a copy of the key
   * and value have to be made as well.
   */
  SimpleMapSlot(const SimpleMapSlot &other)
  {
    m_state = other.m_state;
    if (other.m_state == Occupied) {
      new ((void *)this->key()) Key(*other.key());
      new ((void *)this->value()) Value(*other.value());
    }
  }

  /**
   * The move constructor has to copy the state. If the other slot was occupied, the key and value
   * from the other have to moved as well. The other slot stays in the state it was in before. Its
   * optionally stored key and value remain in a moved-from state.
   */
  SimpleMapSlot(SimpleMapSlot &&other) noexcept
  {
    m_state = other.m_state;
    if (other.m_state == Occupied) {
      new ((void *)this->key()) Key(std::move(*other.key()));
      new ((void *)this->value()) Value(std::move(*other.value()));
    }
  }

  /**
   * Returns a non-const pointer to the position where the key is stored.
   */
  Key *key()
  {
    return (Key *)m_key_buffer.ptr();
  }

  /**
   * Returns a const pointer to the position where the key is stored.
   */
  const Key *key() const
  {
    return (const Key *)m_key_buffer.ptr();
  }

  /**
   * Returns a non-const pointer to the position where the value is stored.
   */
  Value *value()
  {
    return (Value *)m_value_buffer.ptr();
  }

  /**
   * Returns a const pointer to the position where the value is stored.
   */
  const Value *value() const
  {
    return (const Value *)m_value_buffer.ptr();
  }

  /**
   * Returns true if the slot currently contains a key and a value.
   */
  bool is_occupied() const
  {
    return m_state == Occupied;
  }

  /**
   * Returns true if the slot is empty, i.e. it does not contain a key and is not in removed state.
   */
  bool is_empty() const
  {
    return m_state == Empty;
  }

  /**
   * Returns the hash of the currently stored key. In this simple map slot implementation, we just
   * computed the hash here. Other implementations might store the hash in the slot instead.
   */
  template<typename Hash> uint32_t get_hash(const Hash &hash)
  {
    BLI_assert(this->is_occupied());
    return hash(*this->key());
  }

  /**
   * Move the other slot into this slot and destruct it. We do destruction here, because this way
   * we can avoid a comparison with the state, since we know the slot is occupied.
   */
  void relocate_occupied_here(SimpleMapSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(other.is_occupied());
    m_state = Occupied;
    new ((void *)this->key()) Key(std::move(*other.key()));
    new ((void *)this->value()) Value(std::move(*other.value()));
    other.key()->~Key();
    other.value()->~Value();
  }

  /**
   * Returns true, when this slot is occupied and contains a key that compares equal to the given
   * key. The hash can be used by other slot implementations to determine inequality faster.
   */
  template<typename ForwardKey, typename IsEqual>
  bool contains(const ForwardKey &key, const IsEqual &is_equal, uint32_t UNUSED(hash)) const
  {
    if (m_state == Occupied) {
      return is_equal(key, *this->key());
    }
    return false;
  }

  /**
   * Change the state of this slot from empty/removed to occupied. The key/value has to be
   * constructed by calling the constructor with the given key/value as parameter.
   */
  template<typename ForwardKey, typename ForwardValue>
  void occupy(ForwardKey &&key, ForwardValue &&value, uint32_t hash)
  {
    BLI_assert(!this->is_occupied());
    this->occupy_without_value(std::forward<ForwardKey>(key), hash);
    new ((void *)this->value()) Value(std::forward<ForwardValue>(value));
  }

  /**
   * Change the state of this slot from empty/removed to occupied, but leave the value
   * uninitialized. The caller is responsible to construct the value afterwards.
   */
  template<typename ForwardKey> void occupy_without_value(ForwardKey &&key, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    m_state = Occupied;
    new ((void *)this->key()) Key(std::forward<ForwardKey>(key));
  }

  /**
   * Change the state of this slot from occupied to removed. The key and value have to be
   * destructed as well.
   */
  void remove()
  {
    BLI_assert(this->is_occupied());
    m_state = Removed;
    this->key()->~Key();
    this->value()->~Value();
  }
};

/**
 * An IntrusiveMapSlot uses two special values of the key to indicate whether the slot is empty or
 * removed. This saves some memory in all cases and is more efficient in many cases. The KeyInfo
 * type indicates which specific values are used. An example for a KeyInfo implementation is
 * PointerKeyInfo.
 *
 * The special key values are expected to be trivially destructible.
 */
template<typename Key, typename Value, typename KeyInfo> class IntrusiveMapSlot {
 private:
  Key m_key = KeyInfo::get_empty();
  AlignedBuffer<sizeof(Value), alignof(Value)> m_value_buffer;

 public:
  IntrusiveMapSlot() = default;

  ~IntrusiveMapSlot()
  {
    if (KeyInfo::is_not_empty_or_removed(m_key)) {
      this->value()->~Value();
    }
  }

  IntrusiveMapSlot(const IntrusiveMapSlot &other) : m_key(other.m_key)
  {
    if (KeyInfo::is_not_empty_or_removed(m_key)) {
      new ((void *)this->value()) Value(*other.value());
    }
  }

  IntrusiveMapSlot(IntrusiveMapSlot &&other) noexcept : m_key(other.m_key)
  {
    if (KeyInfo::is_not_empty_or_removed(m_key)) {
      new ((void *)this->value()) Value(std::move(*other.value()));
    }
  }

  Key *key()
  {
    return &m_key;
  }

  const Key *key() const
  {
    return &m_key;
  }

  Value *value()
  {
    return (Value *)m_value_buffer.ptr();
  }

  const Value *value() const
  {
    return (const Value *)m_value_buffer.ptr();
  }

  bool is_occupied() const
  {
    return KeyInfo::is_not_empty_or_removed(m_key);
  }

  bool is_empty() const
  {
    return KeyInfo::is_empty(m_key);
  }

  template<typename Hash> uint32_t get_hash(const Hash &hash)
  {
    BLI_assert(this->is_occupied());
    return hash(*this->key());
  }

  void relocate_occupied_here(IntrusiveMapSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(other.is_occupied());
    m_key = std::move(other.m_key);
    new ((void *)this->value()) Value(std::move(*other.value()));
    other.m_key.~Key();
    other.value()->~Value();
  }

  template<typename ForwardKey, typename IsEqual>
  bool contains(const ForwardKey &key, const IsEqual &is_equal, uint32_t UNUSED(hash)) const
  {
    BLI_assert(KeyInfo::is_not_empty_or_removed(key));
    return is_equal(key, m_key);
  }

  template<typename ForwardKey, typename ForwardValue>
  void occupy(ForwardKey &&key, ForwardValue &&value, uint32_t hash)
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(KeyInfo::is_not_empty_or_removed(key));
    this->occupy_without_value(std::forward<ForwardKey>(key), hash);
    new ((void *)this->value()) Value(std::forward<ForwardValue>(value));
  }

  template<typename ForwardKey> void occupy_without_value(ForwardKey &&key, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(KeyInfo::is_not_empty_or_removed(key));
    m_key = std::forward<ForwardKey>(key);
  }

  void remove()
  {
    BLI_assert(this->is_occupied());
    KeyInfo::remove(m_key);
    this->value()->~Value();
  }
};

template<typename Key, typename Value> struct DefaultMapSlot;

/**
 * Use SimpleMapSlot by default, because it is the smallest slot type, that works for all keys.
 */
template<typename Key, typename Value> struct DefaultMapSlot {
  using type = SimpleMapSlot<Key, Value>;
};

/**
 * Use a special slot type for pointer keys, because we can store whether a slot is empty or
 * removed with special pointer values.
 */
template<typename Key, typename Value> struct DefaultMapSlot<Key *, Value> {
  using type = IntrusiveMapSlot<Key *, Value, PointerKeyInfo<Key *>>;
};

}  // namespace blender

#endif /* __BLI_MAP_SLOTS_HH__ */
