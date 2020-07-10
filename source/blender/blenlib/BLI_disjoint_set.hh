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

#ifndef __BLI_DISJOINT_SET_HH__
#define __BLI_DISJOINT_SET_HH__

/** \file
 * \ingroup bli
 *
 * This implements the disjoint set data structure with path compression and union by rank.
 */

#include "BLI_array.hh"

namespace blender {

class DisjointSet {
 private:
  Array<uint> parents_;
  Array<uint> ranks_;

 public:
  /**
   * Create a new disjoint set with the given size. Initially, every element is in a separate set.
   */
  DisjointSet(uint size) : parents_(size), ranks_(size, 0)
  {
    for (uint i = 0; i < size; i++) {
      parents_[i] = i;
    }
  }

  /**
   * Join the sets containing elements x and y. Nothing happens when they have been in the same set
   * before.
   */
  void join(uint x, uint y)
  {
    uint root1 = this->find_root(x);
    uint root2 = this->find_root(y);

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
  bool in_same_set(uint x, uint y)
  {
    uint root1 = this->find_root(x);
    uint root2 = this->find_root(y);
    return root1 == root2;
  }

  /**
   * Find the element that represents the set containing x currently.
   */
  uint find_root(uint x)
  {
    /* Find root by following parents. */
    uint root = x;
    while (parents_[root] != root) {
      root = parents_[root];
    }

    /* Compress path. */
    while (parents_[x] != root) {
      uint parent = parents_[x];
      parents_[x] = root;
      x = parent;
    }

    return root;
  }
};

}  // namespace blender

#endif /* __BLI_DISJOINT_SET_HH__ */
