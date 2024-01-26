/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 *
 * Contains functions that help dealing with arrays that are stored in DNA. Due to the constraints
 * of DNA, all structs are trivial from the language's point of view (`std::is_trivial_v`).
 * However, semantically, these types may have non-trivial copy-constructors and destructors.
 */

#include "MEM_guardedalloc.h"

#include "BLI_index_range.hh"
#include "BLI_utildefines.h"

namespace blender::dna::array {

/**
 * Removes an element from the array and shifts the elements after it towards the front.
 */
template<typename T>
inline void remove_index(
    T **items, int *items_num, int *active_index, const int index, void (*destruct_item)(T *))
{
  static_assert(std::is_trivial_v<T>);
  BLI_assert(index >= 0);
  BLI_assert(index < *items_num);

  const int old_items_num = *items_num;
  const int new_items_num = old_items_num - 1;

  T *old_items = *items;
  T *new_items = MEM_cnew_array<T>(new_items_num, __func__);

  std::copy_n(old_items, index, new_items);
  std::copy_n(old_items + index + 1, old_items_num - index - 1, new_items + index);

  destruct_item(&old_items[index]);
  MEM_freeN(old_items);

  *items = new_items;
  *items_num = new_items_num;

  if (active_index) {
    const int old_active_index = active_index ? *active_index : 0;
    const int new_active_index = std::max(
        0, old_active_index == new_items_num ? new_items_num - 1 : old_active_index);
    *active_index = new_active_index;
  }
}

/**
 * Removes all elements from an array and frees it.
 */
template<typename T>
inline void clear(T **items, int *items_num, int *active_index, void (*destruct_item)(T *))
{
  static_assert(std::is_trivial_v<T>);
  for (const int i : IndexRange(*items_num)) {
    destruct_item(&(*items)[i]);
  }
  MEM_SAFE_FREE(*items);
  *items_num = 0;
  if (active_index) {
    *active_index = 0;
  }
}

/**
 * Moves one element from one index to another, moving other elements if necessary.
 */
template<typename T>
inline void move_index(T *items, const int items_num, const int from_index, const int to_index)
{
  static_assert(std::is_trivial_v<T>);
  BLI_assert(from_index >= 0);
  BLI_assert(from_index < items_num);
  BLI_assert(to_index >= 0);
  BLI_assert(to_index < items_num);
  UNUSED_VARS_NDEBUG(items_num);

  if (from_index == to_index) {
    return;
  }

  if (from_index < to_index) {
    const T tmp = items[from_index];
    for (int i = from_index; i < to_index; i++) {
      items[i] = items[i + 1];
    }
    items[to_index] = tmp;
  }
  else if (from_index > to_index) {
    const T tmp = items[from_index];
    for (int i = from_index; i > to_index; i--) {
      items[i] = items[i - 1];
    }
    items[to_index] = tmp;
  }
}

}  // namespace blender::dna::array
