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
 * This file contains different slot types that are supposed to be used with blender::Set.
 *
 * Every slot type has to be able to hold a value of the Key type and state information.
 * A set slot has three possible states: empty, occupied and removed.
 *
 * Only when a slot is occupied, it stores an instance of type Key.
 *
 * A set slot type has to implement a couple of methods that are explained in SimpleSetSlot.
 * A slot type is assumed to be trivially destructible, when it is not in occupied state. So the
 * destructor might not be called in that case.
 */

#include "BLI_memory_utils.hh"
#include "BLI_string_ref.hh"

namespace blender {

/**
 * The simplest possible set slot. It stores the slot state and the optional key instance in
 * separate variables. Depending on the alignment requirement of the key, many bytes might be
 * wasted.
 */
template<typename Key> class SimpleSetSlot {
 private:
  enum State : uint8_t {
    Empty = 0,
    Occupied = 1,
    Removed = 2,
  };

  State state_;
  TypedBuffer<Key> key_buffer_;

 public:
  /**
   * After the default constructor has run, the slot has to be in the empty state.
   */
  SimpleSetSlot()
  {
    state_ = Empty;
  }

  /**
   * The destructor also has to destruct the key, if the slot is currently occupied.
   */
  ~SimpleSetSlot()
  {
    if (state_ == Occupied) {
      key_buffer_.ref().~Key();
    }
  }

  /**
   * The copy constructor has to copy the state. If the other slot was occupied, a copy of the key
   * has to be made as well.
   */
  SimpleSetSlot(const SimpleSetSlot &other)
  {
    state_ = other.state_;
    if (other.state_ == Occupied) {
      new (&key_buffer_) Key(*other.key_buffer_);
    }
  }

  /**
   * The move constructor has to copy the state. If the other slot was occupied, the key from the
   * other slot has to be moved as well. The other slot stays in the state it was in before. Its
   * optionally stored key remains in a moved-from state.
   */
  SimpleSetSlot(SimpleSetSlot &&other) noexcept(std::is_nothrow_move_constructible_v<Key>)
  {
    state_ = other.state_;
    if (other.state_ == Occupied) {
      new (&key_buffer_) Key(std::move(*other.key_buffer_));
    }
  }

  /**
   * Get a non-const pointer to the position where the key is stored.
   */
  Key *key()
  {
    return key_buffer_;
  }

  /**
   * Get a const pointer to the position where the key is stored.
   */
  const Key *key() const
  {
    return key_buffer_;
  }

  /**
   * Return true if the slot currently contains a key.
   */
  bool is_occupied() const
  {
    return state_ == Occupied;
  }

  /**
   * Return true if the slot is empty, i.e. it does not contain a key and is not in removed state.
   */
  bool is_empty() const
  {
    return state_ == Empty;
  }

  /**
   * Return the hash of the currently stored key. In this simple set slot implementation, we just
   * compute the hash here. Other implementations might store the hash in the slot instead.
   */
  template<typename Hash> uint64_t get_hash(const Hash &hash) const
  {
    BLI_assert(this->is_occupied());
    return hash(*key_buffer_);
  }

  /**
   * Return true, when this slot is occupied and contains a key that compares equal to the given
   * key. The hash is used by other slot implementations to determine inequality faster.
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
   * Change the state of this slot from empty/removed to occupied. The key has to be constructed
   * by calling the constructor with the given key as parameter.
   */
  template<typename ForwardKey> void occupy(ForwardKey &&key, uint64_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    new (&key_buffer_) Key(std::forward<ForwardKey>(key));
    state_ = Occupied;
  }

  /**
   * Change the state of this slot from occupied to removed. The key has to be destructed as well.
   */
  void remove()
  {
    BLI_assert(this->is_occupied());
    key_buffer_.ref().~Key();
    state_ = Removed;
  }
};

/**
 * This set slot implementation stores the hash of the key within the slot. This helps when
 * computing the hash or an equality check is expensive.
 */
template<typename Key> class HashedSetSlot {
 private:
  enum State : uint8_t {
    Empty = 0,
    Occupied = 1,
    Removed = 2,
  };

  uint64_t hash_;
  State state_;
  TypedBuffer<Key> key_buffer_;

 public:
  HashedSetSlot()
  {
    state_ = Empty;
  }

  ~HashedSetSlot()
  {
    if (state_ == Occupied) {
      key_buffer_.ref().~Key();
    }
  }

  HashedSetSlot(const HashedSetSlot &other)
  {
    state_ = other.state_;
    if (other.state_ == Occupied) {
      hash_ = other.hash_;
      new (&key_buffer_) Key(*other.key_buffer_);
    }
  }

  HashedSetSlot(HashedSetSlot &&other) noexcept(std::is_nothrow_move_constructible_v<Key>)
  {
    state_ = other.state_;
    if (other.state_ == Occupied) {
      hash_ = other.hash_;
      new (&key_buffer_) Key(std::move(*other.key_buffer_));
    }
  }

  Key *key()
  {
    return key_buffer_;
  }

  const Key *key() const
  {
    return key_buffer_;
  }

  bool is_occupied() const
  {
    return state_ == Occupied;
  }

  bool is_empty() const
  {
    return state_ == Empty;
  }

  template<typename Hash> uint64_t get_hash(const Hash &UNUSED(hash)) const
  {
    BLI_assert(this->is_occupied());
    return hash_;
  }

  template<typename ForwardKey, typename IsEqual>
  bool contains(const ForwardKey &key, const IsEqual &is_equal, const uint64_t hash) const
  {
    /* `hash_` might be uninitialized here, but that is ok. */
    if (hash_ == hash) {
      if (state_ == Occupied) {
        return is_equal(key, *key_buffer_);
      }
    }
    return false;
  }

  template<typename ForwardKey> void occupy(ForwardKey &&key, const uint64_t hash)
  {
    BLI_assert(!this->is_occupied());
    new (&key_buffer_) Key(std::forward<ForwardKey>(key));
    state_ = Occupied;
    hash_ = hash;
  }

  void remove()
  {
    BLI_assert(this->is_occupied());
    key_buffer_.ref().~Key();
    state_ = Removed;
  }
};

/**
 * An IntrusiveSetSlot uses two special values of the key to indicate whether the slot is empty or
 * removed. This saves some memory in all cases and is more efficient in many cases. The KeyInfo
 * type indicates which specific values are used. An example for a KeyInfo implementation is
 * PointerKeyInfo.
 *
 * The special key values are expected to be trivially destructible.
 */
template<typename Key, typename KeyInfo> class IntrusiveSetSlot {
 private:
  Key key_ = KeyInfo::get_empty();

 public:
  IntrusiveSetSlot() = default;
  ~IntrusiveSetSlot() = default;
  IntrusiveSetSlot(const IntrusiveSetSlot &other) = default;
  IntrusiveSetSlot(IntrusiveSetSlot &&other) noexcept(std::is_nothrow_move_constructible_v<Key>) =
      default;

  Key *key()
  {
    return &key_;
  }

  const Key *key() const
  {
    return &key_;
  }

  bool is_occupied() const
  {
    return KeyInfo::is_not_empty_or_removed(key_);
  }

  bool is_empty() const
  {
    return KeyInfo::is_empty(key_);
  }

  template<typename Hash> uint64_t get_hash(const Hash &hash) const
  {
    BLI_assert(this->is_occupied());
    return hash(key_);
  }

  template<typename ForwardKey, typename IsEqual>
  bool contains(const ForwardKey &key, const IsEqual &is_equal, const uint64_t UNUSED(hash)) const
  {
    BLI_assert(KeyInfo::is_not_empty_or_removed(key));
    return is_equal(key_, key);
  }

  template<typename ForwardKey> void occupy(ForwardKey &&key, const uint64_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(KeyInfo::is_not_empty_or_removed(key));
    key_ = std::forward<ForwardKey>(key);
  }

  void remove()
  {
    BLI_assert(this->is_occupied());
    KeyInfo::remove(key_);
  }
};

/**
 * This exists just to make it more convenient to define which special integer values can be used
 * to indicate an empty and removed value.
 */
template<typename Int, Int EmptyValue, Int RemovedValue>
using IntegerSetSlot = IntrusiveSetSlot<Int, TemplatedKeyInfo<Int, EmptyValue, RemovedValue>>;

template<typename Key> struct DefaultSetSlot;

/**
 * Use SimpleSetSlot by default, because it is the smallest slot type that works for all key types.
 */
template<typename Key> struct DefaultSetSlot {
  using type = SimpleSetSlot<Key>;
};

/**
 * Store the hash of a string in the slot by default. Recomputing the hash or doing string
 * comparisons can be relatively costly.
 */
template<> struct DefaultSetSlot<std::string> {
  using type = HashedSetSlot<std::string>;
};
template<> struct DefaultSetSlot<StringRef> {
  using type = HashedSetSlot<StringRef>;
};
template<> struct DefaultSetSlot<StringRefNull> {
  using type = HashedSetSlot<StringRefNull>;
};

/**
 * Use a special slot type for pointer keys, because we can store whether a slot is empty or
 * removed with special pointer values.
 */
template<typename Key> struct DefaultSetSlot<Key *> {
  using type = IntrusiveSetSlot<Key *, PointerKeyInfo<Key *>>;
};

}  // namespace blender
