/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Algorithms to work with Set<T> as defined in BLI_set.hh.
 */

namespace blender::set {

/**
 * Return the intersection of 'a' and 'b' as a new set.
 *
 * A new set is returned to ensure its backing storage is as small as possible.
 */
template<typename Set> [[nodiscard]] Set intersect(const Set &a, const Set &b)
{
  if (a.size() > b.size()) {
    /* Iterate over the smallest set, it's more efficient. */
    return intersect(b, a);
  }
  Set intersection;
  for (const auto &item : a) {
    if (b.contains(item)) {
      intersection.add(item);
    }
  }
  return intersection;
}

/**
 * Remove all keys from 'a' that do not exist in 'b'.
 *
 * After the operation completes, 'a' contains the intersection of 'a' and 'b'.
 *
 * \note This may result in an inefficient set 'a', as its backing storage will not be shrunk down
 * to its minimum size.
 */
template<typename Set> void intersect_inplace(Set &a, const Set &b)
{
  if (a.size() > b.size() * 2 + 10) {
    /* If 'a' is significantly larger than 'b', it's more efficient to construct a new set in place
     * and overwrite 'a' with it.
     *
     * The |a| > 2|b|+10 check for 'significantly larger' is fairly arbitrary, and is there to
     * avoid constructing new sets when there is only a small difference in size between the two.
     */
    a = intersect(b, a);
    return;
  }

  a.remove_if([&](const auto &item) { return !b.contains(item); });
}

}  // namespace blender::set
