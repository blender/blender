/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <atomic>

#include "BLI_array.hh"

namespace blender {

/**
 * Same as `DisjointSet` but is thread safe (at slightly higher cost for the single threaded case).
 *
 * The implementation is based on the following paper:
 * "Wait-free Parallel Algorithms for the Union-Find Problem"
 * by Richard J. Anderson and Heather Woll.
 *
 * It's also inspired by this implementation: https://github.com/wjakob/dset.
 */
class AtomicDisjointSet {
 private:
  /* Can generally used relaxed memory order with this algorithm. */
  static constexpr auto relaxed = std::memory_order_relaxed;

  struct Item {
    int parent;
    int rank;
  };

  /**
   * An #Item per element. It's important that the entire item is in a single atomic, so that it
   * can be updated atomically. */
  mutable Array<std::atomic<Item>> items_;

 public:
  /**
   * Create a new disjoing set with the given set. Initially, every element is in a separate set.
   */
  AtomicDisjointSet(const int size);

  /**
   * Join the sets containing elements x and y. Nothing happens when they were in the same set
   * before.
   */
  void join(int x, int y)
  {
    while (true) {
      x = this->find_root(x);
      y = this->find_root(y);

      if (x == y) {
        /* They are in the same set already. */
        return;
      }

      Item x_item = items_[x].load(relaxed);
      Item y_item = items_[y].load(relaxed);

      if (
          /* Implement union by rank heuristic. */
          x_item.rank > y_item.rank
          /* If the rank is the same, make a consistent decision. */
          || (x_item.rank == y_item.rank && x < y))
      {
        std::swap(x_item, y_item);
        std::swap(x, y);
      }

      /* Update parent of item x. */
      const Item x_item_new{y, x_item.rank};
      if (!items_[x].compare_exchange_strong(x_item, x_item_new, relaxed)) {
        /* Another thread has updated item x, start again. */
        continue;
      }

      if (x_item.rank == y_item.rank) {
        /* Increase rank of item y. This may fail when another thread has updated item y in the
         * meantime. That may lead to worse behavior with the union by rank heurist, but seems to
         * be ok in practice. */
        const Item y_item_new{y, y_item.rank + 1};
        items_[y].compare_exchange_weak(y_item, y_item_new, relaxed);
      }
    }
  }

  /**
   * Return true when x and y are in the same set.
   */
  bool in_same_set(int x, int y) const
  {
    while (true) {
      x = this->find_root(x);
      y = this->find_root(y);
      if (x == y) {
        return true;
      }
      if (items_[x].load(relaxed).parent == x) {
        return false;
      }
    }
  }

  /**
   * Find the element that represents the set containing x currently.
   */
  int find_root(int x) const
  {
    while (true) {
      const Item item = items_[x].load(relaxed);
      if (x == item.parent) {
        return x;
      }
      const int new_parent = items_[item.parent].load(relaxed).parent;
      if (item.parent != new_parent) {
        /* This halves the path for faster future lookups. That fail but that does not change
         * correctness. */
        Item expected = item;
        const Item desired{new_parent, item.rank};
        items_[x].compare_exchange_weak(expected, desired, relaxed);
      }
      x = new_parent;
    }
  }

  /**
   * True when x represents a set.
   */
  bool is_root(const int x) const
  {
    const Item item = items_[x].load(relaxed);
    return item.parent == x;
  }

  /**
   * Get an identifier for each id. This is deterministic and does not depend on the order of
   * joins. The ids are ordered by their first occurrence. Consequently, `result[0]` is always zero
   * (unless there are no elements).
   */
  void calc_reduced_ids(MutableSpan<int> result) const;

  /**
   * Count the number of disjoint sets.
   */
  int count_sets() const;
};

}  // namespace blender
