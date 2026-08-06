/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <algorithm>

#include "BLI_array_utils.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_base_c.hh"
#include "BLI_offset_indices.hh"
#include "BLI_sort.hh"
#include "BLI_task.hh"
#include "BLI_task_size_hints.hh"
#include "BLI_threads.hh"

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

void sort_groups(const OffsetIndices<int> groups, MutableSpan<int> indices)
{
  threading::parallel_for(
      groups.index_range(),
      1024,
      [&](const IndexRange range) {
        for (const int64_t index : range) {
          MutableSpan<int> group = indices.slice(groups[index]);
          parallel_sort(group);
        }
      },
      threading::accumulated_task_sizes(
          [&](const IndexRange range) { return groups[range].size(); }));
}

/**
 * Number of indices per chunk when counting. An offset is stored for every group in every
 * chunk, so the number of chunks is limited to a small multiple of the number of threads.
 * More chunks than threads still helps load balancing.
 */
static int64_t counting_chunk_size(const int64_t indices_num)
{
  const int64_t max_chunks = BLI_system_thread_count() * 4;
  const int64_t min_counting_chunk_size = 1 << 15;
  return std::max(min_counting_chunk_size, int64_t(divide_ceil_ul(indices_num, max_chunks)));
}

/**
 * Fill the groups by first counting, for every chunk of indices, how many of its indices fall
 * into each group. Every chunk then knows where it can write in each group, so no atomics are
 * needed and the indices in each group end up sorted.
 *
 * This needs an offset for each group in each chunk, so it is only suitable while the number of
 * groups is small relative to the number of indices.
 */
template<typename ValueSourceForeach>
static void reverse_indices_in_groups_counting(const Span<int> group_indices,
                                               const OffsetIndices<int> offsets,
                                               const ValueSourceForeach &value_source_foreach,
                                               MutableSpan<int> results)
{
  const int64_t groups_num = offsets.size();
  const int64_t chunk_size = counting_chunk_size(group_indices.size());
  const int64_t chunks_num = divide_ceil_ul(group_indices.size(), chunk_size);

  /* Count how many indices of each chunk fall into each group. */
  Array<int> chunk_offsets(chunks_num * groups_num);
  threading::parallel_for(IndexRange(chunks_num), 1, [&](const IndexRange range) {
    for (const int64_t chunk : range) {
      MutableSpan<int> counts = chunk_offsets.as_mutable_span().slice(chunk * groups_num,
                                                                      groups_num);
      counts.fill(0);
      for (const int group : group_indices.slice_safe(chunk * chunk_size, chunk_size)) {
        counts[group]++;
      }
    }
  });

  /* Accumulate into the offset where each chunk starts writing in each group. */
  Array<int> current_offsets(groups_num);
  for (const int64_t group : IndexRange(groups_num)) {
    current_offsets[group] = int(offsets[group].start());
  }
  for (const int64_t chunk : IndexRange(chunks_num)) {
    MutableSpan<int> counts = chunk_offsets.as_mutable_span().slice(chunk * groups_num,
                                                                    groups_num);
    for (const int64_t group : IndexRange(groups_num)) {
      const int count = counts[group];
      counts[group] = current_offsets[group];
      current_offsets[group] += count;
    }
  }

  /* Write the index of every element into its group. */
  threading::parallel_for(IndexRange(chunks_num), 1, [&](const IndexRange range) {
    for (const int64_t chunk : range) {
      MutableSpan<int> group_offsets = chunk_offsets.as_mutable_span().slice(chunk * groups_num,
                                                                             groups_num);
      const int64_t begin = chunk * chunk_size;
      const int64_t size = std::min(chunk_size, group_indices.size() - begin);
      value_source_foreach(IndexRange(begin, size), [&](const int64_t pos, const int value) {
        const int position = group_offsets[group_indices[pos]]++;
        results[position] = value;
      });
    }
  });
}

/**
 * Fill the groups in parallel, using atomics to find the position to write to in each group. The
 * order within each group is non-deterministic.
 */
template<typename ValueSourceForeach>
static void reverse_indices_in_groups_atomic(const Span<int> group_indices,
                                             const OffsetIndices<int> offsets,
                                             const ValueSourceForeach &value_source_foreach,
                                             MutableSpan<int> results)
{
  Array<int> counts(offsets.size(), 0);
  threading::parallel_for(group_indices.index_range(), 1024, [&](const IndexRange range) {
    value_source_foreach(range, [&](const int64_t pos, const int value) {
      const int group_index = group_indices[pos];
      const int index_in_group = atomic_fetch_and_add_int32(&counts[group_index], 1);
      results[offsets[group_index][index_in_group]] = value;
    });
  });
}

/**
 * Fill the groups with the value of every element that maps to them. The values must not decrease
 * with the position in #group_indices, so that sorting them orders each group by position.
 */
template<typename ValueSourceForeach>
static void fill_groups(const Span<int> group_indices,
                        const OffsetIndices<int> offsets,
                        const ValueSourceForeach &value_source_foreach,
                        const bool sort,
                        MutableSpan<int> results)
{
  /* Counting avoids both the atomic contention of filling the groups in parallel and the
   * sorting afterwards, but needs an offset per group for every chunk of indices. Use it while
   * that stays small, when there are many indices and few groups. */
  constexpr int64_t min_parallel_indices = 1 << 16;
  const int64_t chunks_num = divide_ceil_ul(group_indices.size(),
                                            counting_chunk_size(group_indices.size()));
  if (group_indices.size() >= min_parallel_indices && BLI_system_thread_count() >= 4 &&
      chunks_num * offsets.size() <= group_indices.size() / 2)
  {
    reverse_indices_in_groups_counting(group_indices, offsets, value_source_foreach, results);
    return;
  }
  reverse_indices_in_groups_atomic(group_indices, offsets, value_source_foreach, results);
  if (sort) {
    sort_groups(offsets, results);
  }
}

/** Check that every group index has a corresponding group in #offsets. */
static void assert_group_indices_valid(const Span<int> group_indices,
                                       const OffsetIndices<int> offsets)
{
  BLI_assert(*std::max_element(group_indices.begin(), group_indices.end()) < offsets.size());
  BLI_assert(*std::min_element(group_indices.begin(), group_indices.end()) >= 0);
  UNUSED_VARS_NDEBUG(group_indices, offsets);
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
  assert_group_indices_valid(group_indices, offsets);

  /* The value stored for a position is the position itself. */
  const auto position_values = [](const IndexRange range, const auto &fn) {
    for (const int64_t pos : range) {
      fn(pos, int(pos));
    }
  };

  fill_groups(group_indices, offsets, position_values, sort, results);
}

void reverse_indices_in_groups(const Span<int> group_indices,
                               const OffsetIndices<int> offsets,
                               MutableSpan<int> results,
                               const OffsetIndices<int> value_groups)
{
  if (group_indices.is_empty()) {
    return;
  }
  BLI_assert(results.size() == group_indices.size());
  BLI_assert(results.size() == offsets.total_size());
  BLI_assert(value_groups.total_size() == group_indices.size());
  assert_group_indices_valid(group_indices, offsets);

  /* The value stored is the index of the group containing it. */
  const auto grouped_values = [&](const IndexRange range, const auto &fn) {
    const Span<int> value_group_offsets = value_groups.data();
    int64_t group = std::ranges::upper_bound(value_group_offsets, int(range.start())) -
                    value_group_offsets.begin() - 1;
    int64_t pos = range.start();
    while (pos < range.one_after_last()) {
      const int64_t group_end = std::min(range.one_after_last(),
                                         value_groups[group].one_after_last());
      for (; pos < group_end; pos++) {
        fn(pos, int(group));
      }
      group++;
    }
  };

  /* Counting per chunk is not used here, since these relations always have many groups relative
   * to the number of indices. */
  reverse_indices_in_groups_atomic(group_indices, offsets, grouped_values, results);
  sort_groups(offsets, results);
}

void reverse_indices_in_groups(const Span<int> group_indices,
                               const OffsetIndices<int> offsets,
                               MutableSpan<int> results,
                               const int value_group_size)
{
  if (group_indices.is_empty()) {
    return;
  }
  BLI_assert(results.size() == group_indices.size());
  BLI_assert(results.size() == offsets.total_size());
  BLI_assert(group_indices.size() % value_group_size == 0);
  assert_group_indices_valid(group_indices, offsets);

  /* The value stored is the index of the group containing it. */
  const auto uniform_grouped_values = [&](const IndexRange range, const auto &fn) {
    int64_t group = range.start() / value_group_size;
    int64_t pos = range.start();
    while (pos < range.one_after_last()) {
      const int64_t group_end = std::min(range.one_after_last(), (group + 1) * value_group_size);
      for (; pos < group_end; pos++) {
        fn(pos, int(group));
      }
      group++;
    }
  };

  reverse_indices_in_groups_atomic(group_indices, offsets, uniform_grouped_values, results);
  sort_groups(offsets, results);
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

GroupedSpan<int> build_groups_from_indices(const Span<int> indices,
                                           const int groups_num,
                                           Array<int> &offset_data,
                                           Array<int> &index_data,
                                           const IndexMask &values)
{
  offset_data = Array<int>(groups_num + 1, 0);
  const OffsetIndices offsets = build_reverse_offsets(indices, offset_data.as_mutable_span());
  index_data.reinitialize(offsets.total_size());
  if (!indices.is_empty()) {
    BLI_assert(values.size() == indices.size());
    assert_group_indices_valid(indices, offsets);

    /* The value stored is the index at that position in the mask. */
    const auto mask_values = [&](const IndexRange range, const auto &fn) {
      values.slice(range).foreach_index_optimized<int>(
          [&](const int index, const int64_t pos) { fn(range.start() + pos, index); });
    };

    fill_groups(indices, offsets, mask_values, true, index_data);
  }
  return {OffsetIndices<int>(offsets), index_data};
}

}  // namespace blender::offset_indices
