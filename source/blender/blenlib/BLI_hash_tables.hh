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

#ifndef __BLI_OPEN_ADDRESSING_HH__
#define __BLI_OPEN_ADDRESSING_HH__

/** \file
 * \ingroup bli
 *
 * This file contains code that can be shared between different hash table implementations.
 */

#include <cmath>

#include "BLI_allocator.hh"
#include "BLI_array.hh"
#include "BLI_math_base.h"
#include "BLI_memory_utils.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Constexpr Utility Functions
 *
 * Those should eventually be deduplicated with functions in BLI_math_base.h.
 * \{ */

inline constexpr int is_power_of_2_i_constexpr(int n)
{
  return (n & (n - 1)) == 0;
}

inline constexpr uint32_t log2_floor_u_constexpr(uint32_t x)
{
  return x <= 1 ? 0 : 1 + log2_floor_u_constexpr(x >> 1);
}

inline constexpr uint32_t log2_ceil_u_constexpr(uint32_t x)
{
  return (is_power_of_2_i_constexpr((int)x)) ? log2_floor_u_constexpr(x) :
                                               log2_floor_u_constexpr(x) + 1;
}

inline constexpr uint32_t power_of_2_max_u_constexpr(uint32_t x)
{
  return 1 << log2_ceil_u_constexpr(x);
}

template<typename IntT> inline constexpr IntT ceil_division(IntT x, IntT y)
{
  BLI_STATIC_ASSERT(!std::is_signed<IntT>::value, "");
  return x / y + ((x % y) != 0);
}

template<typename IntT> inline constexpr IntT floor_division(IntT x, IntT y)
{
  BLI_STATIC_ASSERT(!std::is_signed<IntT>::value, "");
  return x / y;
}

inline constexpr uint32_t ceil_division_by_fraction(uint32_t x,
                                                    uint32_t numerator,
                                                    uint32_t denominator)
{
  return (uint32_t)ceil_division((uint64_t)x * (uint64_t)denominator, (uint64_t)numerator);
}

inline constexpr uint32_t floor_multiplication_with_fraction(uint32_t x,
                                                             uint32_t numerator,
                                                             uint32_t denominator)
{
  return (uint32_t)((uint64_t)x * (uint64_t)numerator / (uint64_t)denominator);
}

inline constexpr uint32_t total_slot_amount_for_usable_slots(uint32_t min_usable_slots,
                                                             uint32_t max_load_factor_numerator,
                                                             uint32_t max_load_factor_denominator)
{
  return power_of_2_max_u_constexpr(ceil_division_by_fraction(
      min_usable_slots, max_load_factor_numerator, max_load_factor_denominator));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Load Factor
 *
 * This is an abstraction for a fractional load factor. The hash table using this class is assumed
 * to use arrays with a size that is a power of two.
 *
 * \{ */

class LoadFactor {
 private:
  uint8_t m_numerator;
  uint8_t m_denominator;

 public:
  LoadFactor(uint8_t numerator, uint8_t denominator)
      : m_numerator(numerator), m_denominator(denominator)
  {
    BLI_assert(numerator > 0);
    BLI_assert(numerator < denominator);
  }

  void compute_total_and_usable_slots(uint32_t min_total_slots,
                                      uint32_t min_usable_slots,
                                      uint32_t *r_total_slots,
                                      uint32_t *r_usable_slots) const
  {
    BLI_assert(is_power_of_2_i((int)min_total_slots));

    uint32_t total_slots = this->compute_total_slots(min_usable_slots, m_numerator, m_denominator);
    total_slots = std::max(total_slots, min_total_slots);
    uint32_t usable_slots = floor_multiplication_with_fraction(
        total_slots, m_numerator, m_denominator);
    BLI_assert(min_usable_slots <= usable_slots);

    *r_total_slots = total_slots;
    *r_usable_slots = usable_slots;
  }

  static constexpr uint32_t compute_total_slots(uint32_t min_usable_slots,
                                                uint8_t numerator,
                                                uint8_t denominator)
  {
    return total_slot_amount_for_usable_slots(min_usable_slots, numerator, denominator);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Intrusive Key Info
 *
 * A hash table slot has to maintain state about whether the slot is empty, occupied or removed.
 * Usually, this state information is stored in its own variable. While it only needs two bits in
 * theory, in practice often 4 or 8 bytes are used, due to alignment requirements.
 *
 * One solution to deal with this problem is to embed the state information in the key. That means,
 * two values of the key type are selected to indicate whether the slot is empty or removed.
 *
 * The classes below tell a slot implementation which special key values it can use. They can be
 * used as KeyInfo in slot types like IntrusiveSetSlot and IntrusiveMapSlot.
 *
 * A KeyInfo type has to implement a couple of static methods that are descriped in
 * TemplatedKeyInfo.
 *
 * \{ */

/**
 * The template arguments EmptyValue and RemovedValue define which special are used. This can be
 * used when a hash table has integer keys and there are two specific integers that will never be
 * used as keys.
 */
template<typename Key, Key EmptyValue, Key RemovedValue> struct TemplatedKeyInfo {
  /**
   * Get the value that indicates that the slot is empty. This is used to indicate new slots.
   */
  static Key get_empty()
  {
    return EmptyValue;
  }

  /**
   * Modify the given key so that it represents a removed slot.
   */
  static void remove(Key &key)
  {
    key = RemovedValue;
  }

  /**
   * Return true, when the given key indicates that the slot is empty.
   */
  static bool is_empty(const Key &key)
  {
    return key == EmptyValue;
  }

  /**
   * Return true, when the given key indicates that the slot is removed.
   */
  static bool is_removed(const Key &key)
  {
    return key == RemovedValue;
  }

  /**
   * Return true, when the key is valid, i.e. it can be contained in an occupied slot.
   */
  static bool is_not_empty_or_removed(const Key &key)
  {
    return key != EmptyValue && key != RemovedValue;
  }
};

/**
 * 0xffff...ffff indicates an empty slot.
 * 0xffff...fffe indicates a removed slot.
 *
 * Those specific values are used, because with them a single comparison is enough to check whether
 * a slot is occupied. The keys 0x0000...0000 and 0x0000...0001 also satisfy this constraint.
 * However, nullptr is much more likely to be used as valid key.
 */
template<typename Pointer> struct PointerKeyInfo {
  static Pointer get_empty()
  {
    return (Pointer)UINTPTR_MAX;
  }

  static void remove(Pointer &pointer)
  {
    pointer = (Pointer)(UINTPTR_MAX - 1);
  }

  static bool is_empty(Pointer pointer)
  {
    return (uintptr_t)pointer == UINTPTR_MAX;
  }

  static bool is_removed(Pointer pointer)
  {
    return (uintptr_t)pointer == UINTPTR_MAX - 1;
  }

  static bool is_not_empty_or_removed(Pointer pointer)
  {
    return (uintptr_t)pointer < UINTPTR_MAX - 1;
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hash Table Stats
 *
 * A utility class that makes it easier for hash table implementations to provide statistics to the
 * developer. These statistics can be helpful when trying to figure out why a hash table is slow.
 *
 * To use this utility, a hash table has to implement various methods, that are mentioned below.
 *
 * \{ */

class HashTableStats {
 private:
  Vector<uint32_t> m_keys_by_collision_count;
  uint32_t m_total_collisions;
  float m_average_collisions;
  uint32_t m_size;
  uint32_t m_capacity;
  uint32_t m_removed_amount;
  float m_load_factor;
  float m_removed_load_factor;
  uint32_t m_size_per_element;
  uint32_t m_size_in_bytes;
  const void *m_address;

 public:
  /**
   * Requires that the hash table has the following methods:
   * - count_collisions(key) -> uint32_t
   * - size() -> uint32_t
   * - capacity() -> uint32_t
   * - removed_amount() -> uint32_t
   * - size_per_element() -> uint32_t
   * - size_in_bytes() -> uint32_t
   */
  template<typename HashTable, typename Keys>
  HashTableStats(const HashTable &hash_table, const Keys &keys)
  {
    m_total_collisions = 0;
    m_size = hash_table.size();
    m_capacity = hash_table.capacity();
    m_removed_amount = hash_table.removed_amount();
    m_size_per_element = hash_table.size_per_element();
    m_size_in_bytes = hash_table.size_in_bytes();
    m_address = (const void *)&hash_table;

    for (const auto &key : keys) {
      uint32_t collisions = hash_table.count_collisions(key);
      if (m_keys_by_collision_count.size() <= collisions) {
        m_keys_by_collision_count.append_n_times(
            0, collisions - m_keys_by_collision_count.size() + 1);
      }
      m_keys_by_collision_count[collisions]++;
      m_total_collisions += collisions;
    }

    m_average_collisions = (m_size == 0) ? 0 : (float)m_total_collisions / (float)m_size;
    m_load_factor = (float)m_size / (float)m_capacity;
    m_removed_load_factor = (float)m_removed_amount / (float)m_capacity;
  }

  void print(StringRef name = "")
  {
    std::cout << "Hash Table Stats: " << name << "\n";
    std::cout << "  Address: " << m_address << "\n";
    std::cout << "  Total Slots: " << m_capacity << "\n";
    std::cout << "  Occupied Slots:  " << m_size << " (" << m_load_factor * 100.0f << " %)\n";
    std::cout << "  Removed Slots: " << m_removed_amount << " (" << m_removed_load_factor * 100.0f
              << " %)\n";

    char memory_size_str[15];
    BLI_str_format_byte_unit(memory_size_str, m_size_in_bytes, true);
    std::cout << "  Size: ~" << memory_size_str << "\n";
    std::cout << "  Size per Slot: " << m_size_per_element << " bytes\n";

    std::cout << "  Average Collisions: " << m_average_collisions << "\n";
    for (uint32_t collision_count : m_keys_by_collision_count.index_range()) {
      std::cout << "  " << collision_count
                << " Collisions: " << m_keys_by_collision_count[collision_count] << "\n";
    }
  }
};

/** \} */

/**
 * This struct provides an equality operator that returns true for all objects that compare equal
 * when one would use the `==` operator. This is different from std::equal_to<T>, because that
 * requires the parameters to be of type T. Our hash tables support lookups using other types
 * without conversion, therefore DefaultEquality needs to be more generic.
 */
struct DefaultEquality {
  template<typename T1, typename T2> bool operator()(const T1 &a, const T2 &b) const
  {
    return a == b;
  }
};

}  // namespace blender

#endif /* __BLI_OPEN_ADDRESSING_HH__ */
