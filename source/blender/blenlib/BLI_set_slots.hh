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

#ifndef __BLI_SET_SLOTS_HH__
#define __BLI_SET_SLOTS_HH__

/** \file
 * \ingroup bli
 *
 * This file contains different slot types that are supposed to be used with BLI::Set.
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

namespace BLI {

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

  State m_state;
  AlignedBuffer<sizeof(Key), alignof(Key)> m_buffer;

 public:
  /**
   * After the default constructor has run, the slot has to be in the empty state.
   */
  SimpleSetSlot()
  {
    m_state = Empty;
  }

  /**
   * The destructor also has to destruct the key, if the slot is currently occupied.
   */
  ~SimpleSetSlot()
  {
    if (m_state == Occupied) {
      this->key()->~Key();
    }
  }

  /**
   * The copy constructor has to copy the state. If the other slot was occupied, a copy of the key
   * has to be made as well.
   */
  SimpleSetSlot(const SimpleSetSlot &other)
  {
    m_state = other.m_state;
    if (other.m_state == Occupied) {
      new (this->key()) Key(*other.key());
    }
  }

  /**
   * The move constructor has to copy the state. If the other slot was occupied, the key from the
   * other slot has to be moved as well. The other slot stays in the state it was in before. Its
   * optionally stored key remains in a moved-from state.
   */
  SimpleSetSlot(SimpleSetSlot &&other) noexcept
  {
    m_state = other.m_state;
    if (other.m_state == Occupied) {
      new (this->key()) Key(std::move(*other.key()));
    }
  }

  /**
   * Get a non-const pointer to the position where the key is stored.
   */
  Key *key()
  {
    return (Key *)m_buffer.ptr();
  }

  /**
   * Get a const pointer to the position where the key is stored.
   */
  const Key *key() const
  {
    return (const Key *)m_buffer.ptr();
  }

  /**
   * Return true if the slot currently contains a key.
   */
  bool is_occupied() const
  {
    return m_state == Occupied;
  }

  /**
   * Return true if the slot is empty, i.e. it does not contain a key and is not in removed state.
   */
  bool is_empty() const
  {
    return m_state == Empty;
  }

  /**
   * Return the hash of the currently stored key. In this simple set slot implementation, we just
   * compute the hash here. Other implementations might store the hash in the slot instead.
   */
  template<typename Hash> uint32_t get_hash(const Hash &hash) const
  {
    BLI_assert(this->is_occupied());
    return hash(*this->key());
  }

  /**
   * Move the other slot into this slot and destruct it. We do destruction here, because this way
   * we can avoid a comparison with the state, since we know the slot is occupied.
   */
  void relocate_occupied_here(SimpleSetSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(other.is_occupied());
    m_state = Occupied;
    new (this->key()) Key(std::move(*other.key()));
    other.key()->~Key();
  }

  /**
   * Return true, when this slot is occupied and contains a key that compares equal to the given
   * key. The hash is used by other slot implementations to determine inequality faster.
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
   * Change the state of this slot from empty/removed to occupied. The key has to be constructed
   * by calling the constructor with the given key as parameter.
   */
  template<typename ForwardKey> void occupy(ForwardKey &&key, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    m_state = Occupied;
    new (this->key()) Key(std::forward<ForwardKey>(key));
  }

  /**
   * Change the state of this slot from occupied to removed. The key has to be destructed as well.
   */
  void remove()
  {
    BLI_assert(this->is_occupied());
    m_state = Removed;
    this->key()->~Key();
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

  uint32_t m_hash;
  State m_state;
  AlignedBuffer<sizeof(Key), alignof(Key)> m_buffer;

 public:
  HashedSetSlot()
  {
    m_state = Empty;
  }

  ~HashedSetSlot()
  {
    if (m_state == Occupied) {
      this->key()->~Key();
    }
  }

  HashedSetSlot(const HashedSetSlot &other)
  {
    m_state = other.m_state;
    if (other.m_state == Occupied) {
      m_hash = other.m_hash;
      new (this->key()) Key(*other.key());
    }
  }

  HashedSetSlot(HashedSetSlot &&other) noexcept
  {
    m_state = other.m_state;
    if (other.m_state == Occupied) {
      m_hash = other.m_hash;
      new (this->key()) Key(std::move(*other.key()));
    }
  }

  Key *key()
  {
    return (Key *)m_buffer.ptr();
  }

  const Key *key() const
  {
    return (const Key *)m_buffer.ptr();
  }

  bool is_occupied() const
  {
    return m_state == Occupied;
  }

  bool is_empty() const
  {
    return m_state == Empty;
  }

  template<typename Hash> uint32_t get_hash(const Hash &UNUSED(hash)) const
  {
    BLI_assert(this->is_occupied());
    return m_hash;
  }

  void relocate_occupied_here(HashedSetSlot &other, uint32_t hash)
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(other.is_occupied());
    m_state = Occupied;
    m_hash = hash;
    new (this->key()) Key(std::move(*other.key()));
    other.key()->~Key();
  }

  template<typename ForwardKey, typename IsEqual>
  bool contains(const ForwardKey &key, const IsEqual &is_equal, uint32_t hash) const
  {
    /* m_hash might be uninitialized here, but that is ok. */
    if (m_hash == hash) {
      if (m_state == Occupied) {
        return is_equal(key, *this->key());
      }
    }
    return false;
  }

  template<typename ForwardKey> void occupy(ForwardKey &&key, uint32_t hash)
  {
    BLI_assert(!this->is_occupied());
    m_state = Occupied;
    m_hash = hash;
    new (this->key()) Key(std::forward<ForwardKey>(key));
  }

  void remove()
  {
    BLI_assert(this->is_occupied());
    m_state = Removed;
    this->key()->~Key();
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
  Key m_key = KeyInfo::get_empty();

 public:
  IntrusiveSetSlot() = default;
  ~IntrusiveSetSlot() = default;
  IntrusiveSetSlot(const IntrusiveSetSlot &other) = default;
  IntrusiveSetSlot(IntrusiveSetSlot &&other) noexcept = default;

  Key *key()
  {
    return &m_key;
  }

  const Key *key() const
  {
    return &m_key;
  }

  bool is_occupied() const
  {
    return KeyInfo::is_not_empty_or_removed(m_key);
  }

  bool is_empty() const
  {
    return KeyInfo::is_empty(m_key);
  }

  template<typename Hash> uint32_t get_hash(const Hash &hash) const
  {
    BLI_assert(this->is_occupied());
    return hash(m_key);
  }

  void relocate_occupied_here(IntrusiveSetSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(other.is_occupied());
    m_key = std::move(other.m_key);
    other.m_key.~Key();
  }

  template<typename ForwardKey, typename IsEqual>
  bool contains(const ForwardKey &key, const IsEqual &is_equal, uint32_t UNUSED(hash)) const
  {
    BLI_assert(KeyInfo::is_not_empty_or_removed(key));
    return is_equal(m_key, key);
  }

  template<typename ForwardKey> void occupy(ForwardKey &&key, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(KeyInfo::is_not_empty_or_removed(key));

    m_key = std::forward<ForwardKey>(key);
  }

  void remove()
  {
    BLI_assert(this->is_occupied());
    KeyInfo::remove(m_key);
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

}  // namespace BLI

#endif /* __BLI_SET_SLOTS_HH__ */
