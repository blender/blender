/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_atomic_disjoint_set.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_map.hh"
#include "BLI_sort.hh"
#include "BLI_task.hh"

namespace blender {

AtomicDisjointSet::AtomicDisjointSet(const int size) : items_(size)
{
  threading::parallel_for(IndexRange(size), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      items_[i].store(Item{i, 0}, relaxed);
    }
  });
}

static void update_first_occurence(Map<int, int> &map, const int root, const int index)
{
  map.add_or_modify(
      root,
      [&](int *first_occurence) { *first_occurence = index; },
      [&](int *first_occurence) {
        if (index < *first_occurence) {
          *first_occurence = index;
        }
      });
}

void AtomicDisjointSet::calc_reduced_ids(MutableSpan<int> result) const
{
  BLI_assert(result.size() == items_.size());

  const int size = result.size();

  /* Find the root for element. With multi-threading, this root is not deterministic. So
   * some postprocessing has to be done to make it deterministic. */
  threading::EnumerableThreadSpecific<Map<int, int>> first_occurence_by_root_per_thread;
  threading::parallel_for(IndexRange(size), 1024, [&](const IndexRange range) {
    Map<int, int> &first_occurence_by_root = first_occurence_by_root_per_thread.local();
    for (const int i : range) {
      const int root = this->find_root(i);
      result[i] = root;
      update_first_occurence(first_occurence_by_root, root, i);
    }
  });

  /* Build a map that contains the first element index that has a certain root. */
  Map<int, int> &combined_map = first_occurence_by_root_per_thread.local();
  for (const Map<int, int> &other_map : first_occurence_by_root_per_thread) {
    if (&combined_map == &other_map) {
      continue;
    }
    for (const auto item : other_map.items()) {
      update_first_occurence(combined_map, item.key, item.value);
    }
  }

  struct RootOccurence {
    int root;
    int first_occurence;
  };

  /* Sort roots by first occurence. This removes the non-determinism above. */
  Vector<RootOccurence, 16> root_occurences;
  root_occurences.reserve(combined_map.size());
  for (const auto item : combined_map.items()) {
    root_occurences.append({item.key, item.value});
  }
  parallel_sort(root_occurences.begin(),
                root_occurences.end(),
                [](const RootOccurence &a, const RootOccurence &b) {
                  return a.first_occurence < b.first_occurence;
                });

  /* Remap original root values with deterministic values. */
  Map<int, int> id_by_root;
  id_by_root.reserve(root_occurences.size());
  for (const int i : root_occurences.index_range()) {
    id_by_root.add_new(root_occurences[i].root, i);
  }
  threading::parallel_for(IndexRange(size), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      result[i] = id_by_root.lookup(result[i]);
    }
  });
}

int AtomicDisjointSet::count_sets() const
{
  return threading::parallel_reduce<int>(
      items_.index_range(),
      1024,
      0,
      [&](const IndexRange range, int count) {
        for (const int i : range) {
          if (this->is_root(i)) {
            count++;
          }
        }
        return count;
      },
      [](const int a, const int b) { return a + b; });
}

}  // namespace blender
