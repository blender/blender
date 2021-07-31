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

template<typename Src1, typename Src2, typename Dst1, typename Dst2>
void initialize_pointer_pair(Src1 &&src1, Src2 &&src2, Dst1 *dst1, Dst2 *dst2)
{
  new ((void *)dst1) Dst1(std::forward<Src1>(src1));
  try {
    new ((void *)dst2) Dst2(std::forward<Src2>(src2));
  }
  catch (...) {
    dst1->~Dst1();
    throw;
  }
}

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

  State state_;
  TypedBuffer<Key> key_buffer_;
  TypedBuffer<Value> value_buffer_;

 public:
  /**
   * After the default constructor has run, the slot has to be in the empty state.
   */
  SimpleMapSlot()
  {
    state_ = Empty;
  }

  /**
   * The destructor also has to destruct the key and value, if the slot is currently occupied.
   */
  ~SimpleMapSlot()
  {
    if (state_ == Occupied) {
      key_buffer_.ref().~Key();
      value_buffer_.ref().~Value();
    }
  }

  /**
   * The copy constructor has to copy the state. If the other slot was occupied, a copy of the key
   * and value have to be made as well.
   */
  SimpleMapSlot(const SimpleMapSlot &other)
  {
    state_ = other.state_;
    if (other.state_ == Occupied) {
      initialize_pointer_pair(other.key_buffer_.ref(),
                              other.value_buffer_.ref(),
                              key_buffer_.ptr(),
                              value_buffer_.ptr());
    }
  }

  /**
   * The move constructor has to copy the state. If the other slot was occupied, the key and value
   * from the other have to moved as well. The other slot stays in the state it was in before. Its
   * optionally stored key and value remain in a moved-from state.
   */
  SimpleMapSlot(SimpleMapSlot &&other) noexcept(
      std::is_nothrow_move_constructible_v<Key> &&std::is_nothrow_move_constructible_v<Value>)
  {
    state_ = other.state_;
    if (other.state_ == Occupied) {
      initialize_pointer_pair(std::move(other.key_buffer_.ref()),
                              std::move(other.value_buffer_.ref()),
                              key_buffer_.ptr(),
                              value_buffer_.ptr());
    }
  }

  /**
   * Returns a non-const pointer to the position where the key is stored.
   */
  Key *key()
  {
    return key_buffer_;
  }

  /**
   * Returns a const pointer to the position where the key is stored.
   */
  const Key *key() const
  {
    return key_buffer_;
  }

  /**
   * Returns a non-const pointer to the position where the value is stored.
   */
  Value *value()
  {
    return value_buffer_;
  }

  /**
   * Returns a const pointer to the position where the value is stored.
   */
  const Value *value() const
  {
    return value_buffer_;
  }

  /**
   * Returns true if the slot currently contains a key and a value.
   */
  bool is_occupied() const
  {
    return state_ == Occupied;
  }

  /**
   * Returns true if the slot is empty, i.e. it does not contain a key and is not in removed state.
   */
  bool is_empty() const
  {
    return state_ == Empty;
  }

  /**
   * Returns the hash of the currently stored key. In this simple map slot implementation, we just
   * computed the hash here. Other implementations might store the hash in the slot instead.
   */
  template<typename Hash> uint64_t get_hash(const Hash &hash)
  {
    BLI_assert(this->is_occupied());
    return hash(*key_buffer_);
  }

  /**
   * Returns true, when this slot is occupied and contains a key that compares equal to the given
   * key. The hash can be used by other slot implementations to determine inequality faster.
   */
  template<typename ForwardKey, typename IsEqual>
  bool contains(const ForwardKey &key, const IsEqual &is_equal, uint64_t UNUSED(hash)) const
  {
    if (state_ == Occupied) {
      return is_equal(key, *key_buffer_);
    }
    return false;
  }

  /**
   * Change the state of this slot from empty/removed to occupied. The key/value has to be
   * constructed by calling the constructor with the given key/value as parameter.
   */
  template<typename ForwardKey, typename... ForwardValue>
  void occupy(ForwardKey &&key, uint64_t hash, ForwardValue &&...value)
  {
    BLI_assert(!this->is_occupied());
    new (&value_buffer_) Value(std::forward<ForwardValue>(value)...);
    this->occupy_no_value(std::forward<ForwardKey>(key), hash);
    state_ = Occupied;
  }

  /**
   * Change the state of this slot from empty/removed to occupied. The value is assumed to be
   * constructed already.
   */
  template<typename ForwardKey> void occupy_no_value(ForwardKey &&key, uint64_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    try {
      new (&key_buffer_) Key(std::forward<ForwardKey>(key));
    }
    catch (...) {
      /* The value is assumed to be constructed already, so it has to be destructed as well. */
      value_buffer_.ref().~Value();
      throw;
    }
    state_ = Occupied;
  }

  /**
   * Change the state of this slot from occupied to removed. The key and value have to be
   * destructed as well.
   */
  void remove()
  {
    BLI_assert(this->is_occupied());
    key_buffer_.ref().~Key();
    value_buffer_.ref().~Value();
    state_ = Removed;
  }
};

/**
 * An IntrusiveMapSlot uses two special values of the key to indicate whether the slot is empty
 * or removed. This saves some memory in all cases and is more efficient in many cases. The
 * KeyInfo type indicates which specific values are used. An example for a KeyInfo
 * implementation is PointerKeyInfo.
 *
 * The special key values are expected to be trivially destructible.
 */
template<typename Key, typename Value, typename KeyInfo> class IntrusiveMapSlot {
 private:
  Key key_ = KeyInfo::get_empty();
  TypedBuffer<Value> value_buffer_;

 public:
  IntrusiveMapSlot() = default;

  ~IntrusiveMapSlot()
  {
    if (KeyInfo::is_not_empty_or_removed(key_)) {
      value_buffer_.ref().~Value();
    }
  }

  IntrusiveMapSlot(const IntrusiveMapSlot &other) : key_(other.key_)
  {
    if (KeyInfo::is_not_empty_or_removed(key_)) {
      new (&value_buffer_) Value(*other.value_buffer_);
    }
  }

  IntrusiveMapSlot(IntrusiveMapSlot &&other) noexcept : key_(other.key_)
  {
    if (KeyInfo::is_not_empty_or_removed(key_)) {
      new (&value_buffer_) Value(std::move(*other.value_buffer_));
    }
  }

  Key *key()
  {
    return &key_;
  }

  const Key *key() const
  {
    return &key_;
  }

  Value *value()
  {
    return value_buffer_;
  }

  const Value *value() const
  {
    return value_buffer_;
  }

  bool is_occupied() const
  {
    return KeyInfo::is_not_empty_or_removed(key_);
  }

  bool is_empty() const
  {
    return KeyInfo::is_empty(key_);
  }

  template<typename Hash> uint64_t get_hash(const Hash &hash)
  {
    BLI_assert(this->is_occupied());
    return hash(key_);
  }

  template<typename ForwardKey, typename IsEqual>
  bool contains(const ForwardKey &key, const IsEqual &is_equal, uint64_t UNUSED(hash)) const
  {
    BLI_assert(KeyInfo::is_not_empty_or_removed(key));
    return is_equal(key, key_);
  }

  template<typename ForwardKey, typename... ForwardValue>
  void occupy(ForwardKey &&key, uint64_t hash, ForwardValue &&...value)
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(KeyInfo::is_not_empty_or_removed(key));
    new (&value_buffer_) Value(std::forward<ForwardValue>(value)...);
    this->occupy_no_value(std::forward<ForwardKey>(key), hash);
  }

  template<typename ForwardKey> void occupy_no_value(ForwardKey &&key, uint64_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(KeyInfo::is_not_empty_or_removed(key));
    try {
      key_ = std::forward<ForwardKey>(key);
    }
    catch (...) {
      value_buffer_.ref().~Value();
      throw;
    }
  }

  void remove()
  {
    BLI_assert(this->is_occupied());
    value_buffer_.ref().~Value();
    KeyInfo::remove(key_);
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
