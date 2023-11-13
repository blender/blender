/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * This implements the disjoint set data structure with path compression and union by rank.
 */

#include "BLI_array.hh"
#include "BLI_index_range.hh"

namespace blender {

template<typename T = int64_t> class DisjointSet {
 private:
  Array<T> parents_;
  Array<T> ranks_;

 public:
  /**
   * Create a new disjoint set with the given size. Initially, every element is in a separate set.
   */
  DisjointSet(const int64_t size) : parents_(size), ranks_(size, 0)
  {
    BLI_assert(size >= 0);
    for (const int64_t i : IndexRange(size)) {
      parents_[i] = T(i);
    }
  }

  /**
   * Join the sets containing elements x and y. Nothing happens when they have been in the same set
   * before.
   */
  void join(const T x, const T y)
  {
    T root1 = this->find_root(x);
    T root2 = this->find_root(y);

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
  bool in_same_set(const T x, const T y)
  {
    T root1 = this->find_root(x);
    T root2 = this->find_root(y);
    return root1 == root2;
  }

  /**
   * Find the element that represents the set containing x currently.
   */
  T find_root(const T x)
  {
    /* Find root by following parents. */
    T root = x;
    while (parents_[root] != root) {
      root = parents_[root];
    }

    /* Compress path. */
    T to_root = x;
    while (parents_[to_root] != root) {
      const T parent = parents_[to_root];
      parents_[to_root] = root;
      to_root = parent;
    }

    return root;
  }
};

}  // namespace blender
