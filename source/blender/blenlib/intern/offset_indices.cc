/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_array_utils.hh"
#include "BLI_offset_indices.hh"
#include "BLI_task.hh"

#include "atomic_ops.h"

namespace blender::offset_indices {

OffsetIndices<int> accumulate_counts_to_offsets(MutableSpan<int> counts_to_offsets,
                                                const int start_offset)
{
  int offset = start_offset;
  int64_t offset_i64 = start_offset;

  for (const int i : counts_to_offsets.index_range().drop_back(1)) {
    const int count = counts_to_offsets[i];
    BLI_assert(count >= 0);
    counts_to_offsets[i] = offset;
    offset += count;
#ifndef NDEBUG
    offset_i64 += count;
#endif
  }
  counts_to_offsets.last() = offset;

  BLI_assert_msg(offset == offset_i64, "Integer overflow occurred");
  UNUSED_VARS_NDEBUG(offset_i64);

  return OffsetIndices<int>(counts_to_offsets);
}

std::optional<OffsetIndices<int>> accumulate_counts_to_offsets_with_overflow_check(
    MutableSpan<int> counts_to_offsets, int start_offset)
{
  /* This variant was measured to be about ~8% slower than the version without overflow check.
   * Since this function is often a serial bottleneck, we use a separate code path for when an
   * overflow check is requested. */
  int64_t offset = start_offset;
  for (const int i : counts_to_offsets.index_range().drop_back(1)) {
    const int count = counts_to_offsets[i];
    BLI_assert(count >= 0);
    counts_to_offsets[i] = offset;
    offset += count;
  }
  counts_to_offsets.last() = offset;
  const bool has_overflow = offset >= std::numeric_limits<int>::max();
  if (has_overflow) {
    return std::nullopt;
  }
  return OffsetIndices<int>(counts_to_offsets);
}

void fill_constant_group_size(const int size, const int start_offset, MutableSpan<int> offsets)
{
  threading::memory_bandwidth_bound_task(offsets.size_in_bytes(), [&]() {
    threading::parallel_for(offsets.index_range(), 1024, [&](const IndexRange range) {
      for (const int64_t i : range) {
        offsets[i] = size * i + start_offset;
      }
    });
  });
}

void copy_group_sizes(const OffsetIndices<int> offsets,
                      const IndexMask &mask,
                      MutableSpan<int> sizes)
{
  mask.foreach_index_optimized<int64_t>([&](const int64_t i) { sizes[i] = offsets[i].size(); },
                                        exec_mode::grain_size(4096));
}

void gather_group_sizes(const OffsetIndices<int> offsets,
                        const IndexMask &mask,
                        MutableSpan<int> sizes)
{
  mask.foreach_index_optimized<int64_t>(
      [&](const int64_t i, const int64_t pos) { sizes[pos] = offsets[i].size(); },
      exec_mode::grain_size(4096));
}

void gather_group_sizes(const OffsetIndices<int> offsets,
                        const Span<int> indices,
                        MutableSpan<int> sizes)
{
  threading::memory_bandwidth_bound_task(
      sizes.size_in_bytes() + offsets.data().size_in_bytes() + indices.size_in_bytes(), [&]() {
        threading::parallel_for(indices.index_range(), 4096, [&](const IndexRange range) {
          for (const int i : range) {
            sizes[i] = offsets[indices[i]].size();
          }
        });
      });
}

int sum_group_sizes(const OffsetIndices<int> offsets, const Span<int> indices)
{
  int count = 0;
  for (const int i : indices) {
    count += offsets[i].size();
  }
  return count;
}

int sum_group_sizes(const OffsetIndices<int> offsets, const IndexMask &mask)
{
  int count = 0;
  mask.foreach_segment_optimized([&](const auto segment) {
    if constexpr (std::is_same_v<std::decay_t<decltype(segment)>, IndexRange>) {
      count += offsets[segment].size();
    }
    else {
      for (const int64_t i : segment) {
        count += offsets[i].size();
      }
    }
  });
  return count;
}

OffsetIndices<int> gather_selected_offsets(const OffsetIndices<int> src_offsets,
                                           const IndexMask &selection,
                                           const int start_offset,
                                           MutableSpan<int> dst_offsets)
{
  if (selection.is_empty()) {
    return {};
  }
  int offset = start_offset;
  selection.foreach_index_optimized<int>([&](const int i, const int pos) {
    dst_offsets[pos] = offset;
    offset += src_offsets[i].size();
  });
  dst_offsets.last() = offset;
  return OffsetIndices<int>(dst_offsets);
}

void build_reverse_map(OffsetIndices<int> offsets, MutableSpan<int> r_map)
{
  threading::parallel_for(offsets.index_range(), 1024, [&](const IndexRange range) {
    for (const int64_t i : range) {
      r_map.slice(offsets[i]).fill(i);
    }
  });
}

OffsetIndices<int> build_reverse_offsets(const Span<int> indices, MutableSpan<int> offsets)
{
  BLI_assert(std::all_of(offsets.begin(), offsets.end(), [](int value) { return value == 0; }));
  array_utils::count_indices(indices, offsets);
  return offset_indices::accumulate_counts_to_offsets(offsets);
}

void sort_small_groups(const OffsetIndices<int> groups, MutableSpan<int> indices)
{
  threading::parallel_for(groups.index_range(), 1024, [&](const IndexRange range) {
    for (const int64_t index : range) {
      MutableSpan<int> group = indices.slice(groups[index]);
      std::ranges::sort(group);
    }
  });
}

void reverse_indices_in_groups(const Span<int> group_indices,
                               const OffsetIndices<int> offsets,
                               MutableSpan<int> results,
                               const bool sort)
{
  if (group_indices.is_empty()) {
    return;
  }
  BLI_assert(results.size() == group_indices.size());
  BLI_assert(results.size() == offsets.total_size());
  BLI_assert(*std::max_element(group_indices.begin(), group_indices.end()) < offsets.size());
  BLI_assert(*std::min_element(group_indices.begin(), group_indices.end()) >= 0);

  /* `counts` keeps track of how many elements have been added to each group, and is incremented
   * atomically by many threads in parallel. `calloc` can be measurably faster than a parallel fill
   * of zero. Alternatively the offsets could be copied and incremented directly, but the cost of
   * the copy is slightly higher than the cost of `calloc`. */
  Array<int> counts(offsets.size(), 0);
  threading::parallel_for(group_indices.index_range(), 1024, [&](const IndexRange range) {
    for (const int64_t i : range) {
      const int group_index = group_indices[i];
      const int index_in_group = atomic_fetch_and_add_int32(&counts[group_index], 1);
      results[offsets[group_index][index_in_group]] = int(i);
    }
  });
  if (sort) {
    sort_small_groups(offsets, results);
  }
}

GroupedSpan<int> build_groups_from_indices(const Span<int> indices,
                                           const int groups_num,
                                           Array<int> &offset_data,
                                           Array<int> &index_data)
{
  offset_data = Array<int>(groups_num + 1, 0);
  const OffsetIndices offsets = build_reverse_offsets(indices, offset_data.as_mutable_span());
  index_data.reinitialize(offsets.total_size());
  reverse_indices_in_groups(indices, offsets, index_data);
  return {OffsetIndices<int>(offsets), index_data};
}

}  // namespace blender::offset_indices
