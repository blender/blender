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
 * This implements the disjoint set data structure with path compression and union by rank.
 */

#include "BLI_array.hh"

namespace blender {

class DisjointSet {
 private:
  Array<int64_t> parents_;
  Array<int64_t> ranks_;

 public:
  /**
   * Create a new disjoint set with the given size. Initially, every element is in a separate set.
   */
  DisjointSet(int64_t size) : parents_(size), ranks_(size, 0)
  {
    BLI_assert(size >= 0);
    for (int64_t i = 0; i < size; i++) {
      parents_[i] = i;
    }
  }

  /**
   * Join the sets containing elements x and y. Nothing happens when they have been in the same set
   * before.
   */
  void join(int64_t x, int64_t y)
  {
    int64_t root1 = this->find_root(x);
    int64_t root2 = this->find_root(y);

    /* x and y are in the same set already. */
    if (root1 == root2) {
      return;
    }

    /* Implement union by rank heuristic. */
    if (ranks_[root1] < ranks_[root2]) {
      std::swap(root1, root2);
    }
    parents_[root2] = root1;

    if (ranks_[root1] == ranks_[root2]) {
      ranks_[root1]++;
    }
  }

  /**
   * Return true when x and y are in the same set.
   */
  bool in_same_set(int64_t x, int64_t y)
  {
    int64_t root1 = this->find_root(x);
    int64_t root2 = this->find_root(y);
    return root1 == root2;
  }

  /**
   * Find the element that represents the set containing x currently.
   */
  int64_t find_root(int64_t x)
  {
    /* Find root by following parents. */
    int64_t root = x;
    while (parents_[root] != root) {
      root = parents_[root];
    }

    /* Compress path. */
    while (parents_[x] != root) {
      int64_t parent = parents_[x];
      parents_[x] = root;
      x = parent;
    }

    return root;
  }
};

}  // namespace blender
