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
 * This class offers a useful abstraction for other containers that implement hash tables using
 * open addressing. It handles the following aspects:
 *   - Allocation and deallocation of the open addressing array.
 *   - Optional small object optimization.
 *   - Keeps track of how many elements and dummies are in the table.
 *
 * The nice thing about this abstraction is that it does not get in the way of any performance
 * optimizations. The data that is actually stored in the table is still fully defined by the
 * actual hash table implementation.
 */

#include <cmath>

#include "BLI_allocator.hh"
#include "BLI_math_base.h"
#include "BLI_memory_utils.hh"
#include "BLI_utildefines.h"

namespace BLI {

/** \name Constexpr utility functions.
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

inline constexpr uint8_t compute_item_exponent(uint32_t min_usable_slots,
                                               uint32_t slots_per_item,
                                               uint32_t max_load_factor_numerator,
                                               uint32_t max_load_factor_denominator)
{
  // uint64_t min_total_slots = ceil_division((uint64_t)min_usable_slots *
  //                                              (uint64_t)max_load_factor_denominator,
  //                                          (uint64_t)max_load_factor_numerator);
  // uint32_t min_total_items = (uint32_t)ceil_division(min_total_slots, (uint64_t)slots_per_item);
  // uint8_t item_exponent = (uint8_t)log2_ceil_u_constexpr(min_total_items);
  // return item_exponent;

  return (uint8_t)log2_ceil_u_constexpr((uint32_t)ceil_division(
      ceil_division((uint64_t)min_usable_slots * (uint64_t)max_load_factor_denominator,
                    (uint64_t)max_load_factor_numerator),
      (uint64_t)slots_per_item));
}

/** \} */

template<typename Item,
         uint32_t MinUsableSlotsInSmallStorage = 1,
         typename Allocator = GuardedAllocator>
class OpenAddressingArray {
 private:
  static constexpr uint32_t s_max_load_factor_numerator = 1;
  static constexpr uint32_t s_max_load_factor_denominator = 2;
  static constexpr uint32_t s_slots_per_item = Item::slots_per_item;

  static constexpr uint8_t s_small_storage_item_exponent = compute_item_exponent(
      MinUsableSlotsInSmallStorage,
      s_slots_per_item,
      s_max_load_factor_numerator,
      s_max_load_factor_denominator);
  static constexpr uint32_t s_items_in_small_storage = 1u << s_small_storage_item_exponent;

  /* Invariants:
   *   2^m_item_exponent = m_item_amount
   *   m_item_amount * s_slots_per_item = m_slots_total
   *   m_slot_mask = m_slots_total - 1
   *   m_slots_set_or_dummy < m_slots_total
   */

  /* Array containing the actual hash table. Might be a pointer to the inlined storage. */
  Item *m_items;
  /* Number of items in the hash table. Must be a power of two. */
  uint32_t m_item_amount;
  /* Exponent of the current item amount. */
  uint8_t m_item_exponent;
  /* Number of elements that could be stored in the table when the load factor is 1. */
  uint32_t m_slots_total;
  /* Number of elements that are not empty. */
  uint32_t m_slots_set_or_dummy;
  /* Number of dummy entries. */
  uint32_t m_slots_dummy;
  /* Max number of slots that can be non-empty according to the load factor. */
  uint32_t m_slots_usable;
  /* Can be used to map a hash value into the range of valid slot indices. */
  uint32_t m_slot_mask;
  Allocator m_allocator;
  AlignedBuffer<(uint)sizeof(Item) * s_items_in_small_storage, (uint)alignof(Item)>
      m_local_storage;

 public:
  explicit OpenAddressingArray(uint8_t item_exponent = s_small_storage_item_exponent)
  {
    m_item_exponent = item_exponent;
    m_item_amount = 1u << item_exponent;
    m_slots_total = m_item_amount * s_slots_per_item;
    m_slot_mask = m_slots_total - 1;
    m_slots_set_or_dummy = 0;
    m_slots_dummy = 0;
    m_slots_usable = (uint32_t)floor_division((uint64_t)m_slots_total *
                                                  (uint64_t)s_max_load_factor_numerator,
                                              (uint64_t)s_max_load_factor_denominator);

    if (m_item_amount <= s_items_in_small_storage) {
      m_items = this->small_storage();
    }
    else {
      m_items = (Item *)m_allocator.allocate_aligned(
          (uint32_t)sizeof(Item) * m_item_amount, std::alignment_of<Item>::value, __func__);
    }

    for (uint32_t i = 0; i < m_item_amount; i++) {
      new (m_items + i) Item();
    }
  }

  ~OpenAddressingArray()
  {
    if (m_items != nullptr) {
      for (uint32_t i = 0; i < m_item_amount; i++) {
        m_items[i].~Item();
      }
      if (!this->is_in_small_storage()) {
        m_allocator.deallocate((void *)m_items);
      }
    }
  }

  OpenAddressingArray(const OpenAddressingArray &other)
  {
    m_slots_total = other.m_slots_total;
    m_slots_set_or_dummy = other.m_slots_set_or_dummy;
    m_slots_dummy = other.m_slots_dummy;
    m_slots_usable = other.m_slots_usable;
    m_slot_mask = other.m_slot_mask;
    m_item_amount = other.m_item_amount;
    m_item_exponent = other.m_item_exponent;

    if (m_item_amount <= s_items_in_small_storage) {
      m_items = this->small_storage();
    }
    else {
      m_items = (Item *)m_allocator.allocate_aligned(
          sizeof(Item) * m_item_amount, std::alignment_of<Item>::value, __func__);
    }

    uninitialized_copy_n(other.m_items, m_item_amount, m_items);
  }

  OpenAddressingArray(OpenAddressingArray &&other) noexcept
  {
    m_slots_total = other.m_slots_total;
    m_slots_set_or_dummy = other.m_slots_set_or_dummy;
    m_slots_dummy = other.m_slots_dummy;
    m_slots_usable = other.m_slots_usable;
    m_slot_mask = other.m_slot_mask;
    m_item_amount = other.m_item_amount;
    m_item_exponent = other.m_item_exponent;
    if (other.is_in_small_storage()) {
      m_items = this->small_storage();
      uninitialized_relocate_n(other.m_items, m_item_amount, m_items);
    }
    else {
      m_items = other.m_items;
    }

    other.m_items = nullptr;
    other.~OpenAddressingArray();
    new (&other) OpenAddressingArray();
  }

  OpenAddressingArray &operator=(const OpenAddressingArray &other)
  {
    if (this == &other) {
      return *this;
    }
    this->~OpenAddressingArray();
    new (this) OpenAddressingArray(other);
    return *this;
  }

  OpenAddressingArray &operator=(OpenAddressingArray &&other)
  {
    if (this == &other) {
      return *this;
    }
    this->~OpenAddressingArray();
    new (this) OpenAddressingArray(std::move(other));
    return *this;
  }

  Allocator &allocator()
  {
    return m_allocator;
  }

  /* Prepare a new array that can hold a minimum of min_usable_slots elements. All entries are
   * empty. */
  OpenAddressingArray init_reserved(uint32_t min_usable_slots) const
  {
    uint8_t item_exponent = compute_item_exponent(min_usable_slots,
                                                  s_slots_per_item,
                                                  s_max_load_factor_numerator,
                                                  s_max_load_factor_denominator);
    OpenAddressingArray grown(item_exponent);
    grown.m_slots_set_or_dummy = this->slots_set();
    return grown;
  }

  /**
   * Amount of items in the array times the number of slots per item.
   */
  uint32_t slots_total() const
  {
    return m_slots_total;
  }

  /**
   * Amount of slots that are initialized with some value that is not empty or dummy.
   */
  uint32_t slots_set() const
  {
    return m_slots_set_or_dummy - m_slots_dummy;
  }

  /**
   * Amount of slots that can be used before the array should grow.
   */
  uint32_t slots_usable() const
  {
    return m_slots_usable;
  }

  /**
   * Update the counters after one empty element is used for a newly added element.
   */
  void update__empty_to_set()
  {
    m_slots_set_or_dummy++;
  }

  /**
   * Update the counters after one previously dummy element becomes set.
   */
  void update__dummy_to_set()
  {
    m_slots_dummy--;
  }

  /**
   * Update the counters after one previously set element becomes a dummy.
   */
  void update__set_to_dummy()
  {
    m_slots_dummy++;
  }

  /**
   * Access the current slot mask for this array.
   */
  uint32_t slot_mask() const
  {
    return m_slot_mask;
  }

  /**
   * Access the item for a specific item index.
   * Note: The item index is not necessarily the slot index.
   */
  const Item &item(uint32_t item_index) const
  {
    return m_items[item_index];
  }

  Item &item(uint32_t item_index)
  {
    return m_items[item_index];
  }

  uint8_t item_exponent() const
  {
    return m_item_exponent;
  }

  uint32_t item_amount() const
  {
    return m_item_amount;
  }

  bool should_grow() const
  {
    return m_slots_set_or_dummy >= m_slots_usable;
  }

  Item *begin()
  {
    return m_items;
  }

  Item *end()
  {
    return m_items + m_item_amount;
  }

  const Item *begin() const
  {
    return m_items;
  }

  const Item *end() const
  {
    return m_items + m_item_amount;
  }

 private:
  Item *small_storage() const
  {
    return reinterpret_cast<Item *>((char *)m_local_storage.ptr());
  }

  bool is_in_small_storage() const
  {
    return m_items == this->small_storage();
  }
};

}  // namespace BLI

#endif /* __BLI_OPEN_ADDRESSING_HH__ */
