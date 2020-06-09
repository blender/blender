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

#ifndef __BLI_VECTOR_SET_SLOTS_HH__
#define __BLI_VECTOR_SET_SLOTS_HH__

/** \file
 * \ingroup bli
 *
 * This file contains slot types that are supposed to be used with blender::VectorSet.
 *
 * Every slot type has to be able to hold an integer index and state information.
 * A vector set slot has three possible states: empty, occupied and removed.
 *
 * A vector slot type has to implement a couple of methods that are explained in
 * SimpleVectorSetSlot.
 * A vector slot type is assumed to be trivially destructible, when it is in empty or removed
 * state.
 *
 * Possible Improvements:
 * - Implement a slot type that stores the hash.
 * - Implement a slot type that stores the key. That means that the key would be stored in two
 *   places: the key vector and the slot itself. Maybe storing the key in the slot as well, can
 *   result in better performance, due to better cache utilization.
 */

#include "BLI_sys_types.h"

namespace blender {

/**
 * The simplest possible vector set slot. It stores the index and state in a signed integer. If the
 * value is negative, it represents empty or occupied state. Otherwise it represents the index.
 */
template<typename Key> class SimpleVectorSetSlot {
 private:
#define s_is_empty -1
#define s_is_removed -2

  /**
   * After the default constructor has run, the slot has to be in the empty state.
   */
  int32_t m_state = s_is_empty;

 public:
  /**
   * Return true if this slot contains an index to a key.
   */
  bool is_occupied() const
  {
    return m_state >= 0;
  }

  /**
   * Return true if the slot is empty, i.e. it does not contain an index.
   */
  bool is_empty() const
  {
    return m_state == s_is_empty;
  }

  /**
   * Return the stored index. It is assumed that the slot is occupied.
   */
  uint32_t index() const
  {
    BLI_assert(this->is_occupied());
    return (uint32_t)m_state;
  }

  /**
   * Return true if the slot contains the given key, i.e. its index points to a key that compares
   * equal to it. The hash can be used by other implementations to determine inequality faster.
   */
  template<typename ForwardKey, typename IsEqual>
  bool contains(const ForwardKey &key,
                const IsEqual &is_equal,
                uint32_t UNUSED(hash),
                const Key *keys) const
  {
    if (m_state >= 0) {
      return is_equal(key, keys[m_state]);
    }
    return false;
  }

  /**
   * Move the other slot into this slot and destruct it. We do destruction here, because this way
   * we can avoid a comparison with the state, since we know the slot is occupied. For this
   * specific slot implementation, this does not make a difference.
   */
  void relocate_occupied_here(SimpleVectorSetSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(other.is_occupied());
    m_state = other.m_state;
  }

  /**
   * Change the state of this slot from empty/removed to occupied. The hash can be used by other
   * slot implementations.
   */
  void occupy(uint32_t index, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    m_state = (int32_t)index;
  }

  /**
   * The key has changed its position in the vector, so the index has to be updated. This method
   * can assume that the slot is currently occupied.
   */
  void update_index(uint32_t index)
  {
    BLI_assert(this->is_occupied());
    m_state = (int32_t)index;
  }

  /**
   * Change the state of this slot from occupied to removed.
   */
  void remove()
  {
    BLI_assert(this->is_occupied());
    m_state = s_is_removed;
  }

  /**
   * Return true if this slot is currently occupied and its corresponding key has the given index.
   */
  bool has_index(uint32_t index) const
  {
    return (uint32_t)m_state == index;
  }

  /**
   * Return the hash of the currently stored key. In this simple set slot implementation, we just
   * compute the hash here. Other implementations might store the hash in the slot instead.
   */
  template<typename Hash> uint32_t get_hash(const Key &key, const Hash &hash) const
  {
    BLI_assert(this->is_occupied());
    return hash(key);
  }

#undef s_is_empty
#undef s_is_removed
};

template<typename Key> struct DefaultVectorSetSlot;

template<typename Key> struct DefaultVectorSetSlot {
  using type = SimpleVectorSetSlot<Key>;
};

}  // namespace blender

#endif /* __BLI_VECTOR_SET_SLOTS_HH__ */
