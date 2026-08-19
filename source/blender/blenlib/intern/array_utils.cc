/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <algorithm>
#include <cmath>
#include <functional>

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_threads.hh"

#include "PRF_profile.hh"

#include "atomic_ops.h"

namespace blender::array_utils {

void copy(const GVArray &src, GMutableSpan dst, const exec_mode::Mode mode)
{
  PRF_scope_with_name("array_utils::copy", ProfileCategory::Default);
  BLI_assert(src.type() == dst.type());
  BLI_assert(src.size() == dst.size());
  if (!mode.is_parallel) {
    src.materialize(src.index_range(), dst.data());
  }
  else {
    const int64_t grain_size = calc_copy_grain_size(mode, src.type().size);
    threading::parallel_for(src.index_range(), grain_size, [&](const IndexRange range) {
      src.materialize(range, dst.data());
    });
  }
}

void copy(const GVArray &src,
          const IndexMask &selection,
          GMutableSpan dst,
          const exec_mode::Mode mode)
{
  PRF_scope_with_name("array_utils::copy", ProfileCategory::Default);
  BLI_assert(src.type() == dst.type());
  BLI_assert(src.size() >= selection.min_array_size());
  BLI_assert(dst.size() >= selection.min_array_size());
  if (!mode.is_parallel) {
    src.materialize(selection, dst.data());
  }
  else {
    const int64_t grain_size = calc_copy_grain_size(mode, src.type().size);
    threading::parallel_for(selection.index_range(), grain_size, [&](const IndexRange range) {
      src.materialize(selection.slice(range), dst.data());
    });
  }
}

void gather(const GVArray &src,
            const IndexMask &indices,
            GMutableSpan dst,
            const exec_mode::Mode mode)
{
  PRF_scope_with_name("array_utils::gather", ProfileCategory::Default);
  BLI_assert(src.type() == dst.type());
  BLI_assert(indices.size() == dst.size());
  if (!mode.is_parallel) {
    src.materialize_compressed(indices, dst.data());
  }
  else {
    const int64_t grain_size = calc_copy_grain_size(mode, src.type().size);
    threading::parallel_for(indices.index_range(), grain_size, [&](const IndexRange range) {
      src.materialize_compressed(indices.slice(range), dst.slice(range).data());
    });
  }
}

void gather(const GSpan src,
            const IndexMask &indices,
            GMutableSpan dst,
            const exec_mode::Mode mode)
{
  gather(GVArray::from_span(src), indices, dst, mode);
}

void copy_group_to_group(const OffsetIndices<int> src_offsets,
                         const OffsetIndices<int> dst_offsets,
                         const IndexMask &selection,
                         const GSpan src,
                         GMutableSpan dst)
{
  /* Each group might be large, so a threaded copy might make sense here too. */
  selection.foreach_index(
      [&](const int i) { dst.slice(dst_offsets[i]).copy_from(src.slice(src_offsets[i])); },
      exec_mode::grain_size(512));
}

static void count_indices_serial(const Span<int> indices, MutableSpan<int> counts)
{
  for (const int i : indices) {
    counts[i]++;
  }
}

static void count_indices_atomics(const Span<int> indices, MutableSpan<int> counts)
{
  threading::parallel_for(indices.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : indices.slice(range)) {
      atomic_add_and_fetch_int32(&counts[i], 1);
    }
  });
}

static void count_indices_thread_local(const Span<int> indices,
                                       MutableSpan<int> counts,
                                       const int64_t max_threads)
{
  /* Count into thread local buffers, parallelized with chunks of indices. The
   * number of threads is limited to avoid using more memory and memory bandwidth
   * than helpful. */
  const int64_t groups_num = counts.size();
  threading::EnumerableThreadSpecific<Array<int>> counts_by_thread(
      [&]() { return Array<int>(groups_num, 0); });
  threading::max_threads_task(max_threads, [&]() {
    threading::parallel_for(indices.index_range(), 4096, [&](const IndexRange range) {
      Array<int> &local_counts = counts_by_thread.local();
      for (const int i : indices.slice(range)) {
        local_counts[i]++;
      }
    });
  });

  /* Sum counts for all threads. */
  threading::parallel_for(IndexRange(groups_num), 4096, [&](const IndexRange range) {
    for (const Array<int> &local_counts : counts_by_thread) {
      for (const int64_t i : range) {
        counts[i] += local_counts[i];
      }
    }
  });
}

void count_indices(const Span<int> indices, MutableSpan<int> counts)
{
  PRF_scope_with_name("array_utils::count_indices", ProfileCategory::Default);

  const int64_t indices_num = indices.size();
  const int64_t groups_num = std::max<int64_t>(counts.size(), 1);

  /* Thread local is fastest when the number of groups is small enough that the
   * counters can stay in the per core caches. */
  constexpr int64_t max_thread_local_groups = 1 << 18;
  if (groups_num <= max_thread_local_groups) {
    /* More threads add memory overhead, limit by groups per index so it's worth
     * using dedicated memory for every thread. */
    constexpr int64_t min_groups_per_index = 10;
    const int64_t max_threads = std::clamp<int64_t>(
        std::sqrt(min_groups_per_index * indices_num / groups_num), 1, BLI_system_thread_count());

    /* Heuristic for when there are enough indices for thread local to work.
     * Fixed minimum, enough indices per thread and not too much group overhead. */
    constexpr int64_t min_parallel_indices = 1 << 16;
    const int64_t min_thread_local_indices = min_parallel_indices + indices_num / max_threads +
                                             max_threads * groups_num / min_groups_per_index;

    if (indices_num < min_thread_local_indices) {
      count_indices_serial(indices, counts);
    }
    else {
      count_indices_thread_local(indices, counts, max_threads);
    }
  }
  else {
    /* Atomics are faster than serial when there are enough indices to justify the overhead
     * and we hopefully don't get too much contention. */
    constexpr int64_t min_atomic_indices = 1 << 19;
    if (indices_num < min_atomic_indices) {
      count_indices_serial(indices, counts);
    }
    else {
      count_indices_atomics(indices, counts);
    }
  }
}

void invert_booleans(MutableSpan<bool> span)
{
  PRF_scope_with_name("array_utils::invert_booleans", ProfileCategory::Default);
  threading::parallel_for(span.index_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      span[i] = !span[i];
    }
  });
}

void invert_booleans(MutableSpan<bool> span, const IndexMask &mask)
{
  PRF_scope_with_name("array_utils::invert_booleans", ProfileCategory::Default);
  mask.foreach_index_optimized<int64_t>([&](const int64_t i) { span[i] = !span[i]; });
}

static bool all_equal(const Span<bool> span, const bool test)
{
  return std::all_of(span.begin(), span.end(), [&](const bool value) { return value == test; });
}

static bool all_equal(const VArray<bool> &varray, const IndexRange range, const bool test)
{
  return std::all_of(
      range.begin(), range.end(), [&](const int64_t i) { return varray[i] == test; });
}

BooleanMix booleans_mix_calc(const VArray<bool> &varray, const IndexRange range_to_check)
{
  if (varray.is_empty()) {
    return BooleanMix::None;
  }
  PRF_scope_with_name("array_utils::booleans_mix_calc", ProfileCategory::Default);
  const CommonVArrayInfo info = varray.common_info();
  if (info.type == CommonVArrayInfo::Type::Single) {
    return *static_cast<const bool *>(info.data) ? BooleanMix::AllTrue : BooleanMix::AllFalse;
  }
  if (info.type == CommonVArrayInfo::Type::Span) {
    const Span<bool> span(static_cast<const bool *>(info.data), varray.size());
    return threading::parallel_reduce(
        range_to_check,
        4096,
        BooleanMix::None,
        [&](const IndexRange range, const BooleanMix init) {
          if (init == BooleanMix::Mixed) {
            return init;
          }
          const Span<bool> slice = span.slice(range);
          const bool compare = (init == BooleanMix::None) ? slice.first() :
                                                            (init == BooleanMix::AllTrue);
          if (all_equal(slice, compare)) {
            return compare ? BooleanMix::AllTrue : BooleanMix::AllFalse;
          }
          return BooleanMix::Mixed;
        },
        [&](BooleanMix a, BooleanMix b) { return (a == b) ? a : BooleanMix::Mixed; });
  }
  return threading::parallel_reduce(
      range_to_check,
      2048,
      BooleanMix::None,
      [&](const IndexRange range, const BooleanMix init) {
        if (init == BooleanMix::Mixed) {
          return init;
        }
        /* Alternatively, this could use #materialize to retrieve many values at once. */
        const bool compare = (init == BooleanMix::None) ? varray[range.first()] :
                                                          (init == BooleanMix::AllTrue);
        if (all_equal(varray, range, compare)) {
          return compare ? BooleanMix::AllTrue : BooleanMix::AllFalse;
        }
        return BooleanMix::Mixed;
      },
      [&](BooleanMix a, BooleanMix b) { return (a == b) ? a : BooleanMix::Mixed; });
}

int64_t count_booleans(const VArray<bool> &varray, const IndexMask &mask)
{
  if (varray.is_empty() || mask.is_empty()) {
    return 0;
  }
  PRF_scope_with_name("array_utils::count_booleans", ProfileCategory::Default);
  /* Check if mask is full. */
  if (varray.size() == mask.size()) {
    const CommonVArrayInfo info = varray.common_info();
    if (info.type == CommonVArrayInfo::Type::Single) {
      return *static_cast<const bool *>(info.data) ? varray.size() : 0;
    }
    if (info.type == CommonVArrayInfo::Type::Span) {
      const Span<bool> span(static_cast<const bool *>(info.data), varray.size());
      return threading::parallel_reduce(
          varray.index_range(),
          4096,
          0,
          [&](const IndexRange range, const int64_t init) {
            const Span<bool> slice = span.slice(range);
            return init + std::count(slice.begin(), slice.end(), true);
          },
          std::plus<>());
    }
    return threading::parallel_reduce(
        varray.index_range(),
        2048,
        0,
        [&](const IndexRange range, const int64_t init) {
          int64_t value = init;
          /* Alternatively, this could use #materialize to retrieve many values at once. */
          for (const int64_t i : range) {
            value += int64_t(varray[i]);
          }
          return value;
        },
        std::plus<>());
  }
  const CommonVArrayInfo info = varray.common_info();
  if (info.type == CommonVArrayInfo::Type::Single) {
    return *static_cast<const bool *>(info.data) ? mask.size() : 0;
  }
  int64_t value = 0;
  mask.foreach_segment([&](const IndexMaskSegment segment) {
    for (const int64_t i : segment) {
      value += int64_t(varray[i]);
    }
  });
  return value;
}

bool contains(const VArray<bool> &varray, const IndexMask &indices_to_check, const bool value)
{
  PRF_scope_with_name("array_utils::contains", ProfileCategory::Default);
  const CommonVArrayInfo info = varray.common_info();
  if (info.type == CommonVArrayInfo::Type::Single) {
    return *static_cast<const bool *>(info.data) == value;
  }
  if (info.type == CommonVArrayInfo::Type::Span) {
    const Span<bool> span(static_cast<const bool *>(info.data), varray.size());
    return threading::parallel_reduce(
        indices_to_check.index_range(),
        4096,
        false,
        [&](const IndexRange range, const bool init) {
          if (init) {
            return init;
          }
          const IndexMask sliced_mask = indices_to_check.slice(range);
          if (std::optional<IndexRange> range = sliced_mask.to_range()) {
            return span.slice(*range).contains(value);
          }
          for (const int64_t segment_i : IndexRange(sliced_mask.segments_num())) {
            const IndexMaskSegment segment = sliced_mask.segment(segment_i);
            for (const int i : segment) {
              if (span[i] == value) {
                return true;
              }
            }
          }
          return false;
        },
        std::logical_or());
  }
  return threading::parallel_reduce(
      indices_to_check.index_range(),
      2048,
      false,
      [&](const IndexRange range, const bool init) {
        if (init) {
          return init;
        }
        constexpr int64_t MaxChunkSize = 512;
        const int64_t slice_end = range.one_after_last();
        for (int64_t start = range.start(); start < slice_end; start += MaxChunkSize) {
          const int64_t end = std::min<int64_t>(start + MaxChunkSize, slice_end);
          const int64_t size = end - start;
          const IndexMask sliced_mask = indices_to_check.slice(start, size);
          std::array<bool, MaxChunkSize> values;
          std::array<bool, MaxChunkSize>::iterator values_end = values.begin() + size;
          varray.materialize_compressed(sliced_mask, values);
          if (std::find(values.begin(), values_end, value) != values_end) {
            return true;
          }
        }
        return false;
      },
      std::logical_or());
}

IndexMask indices_non_negative(const IndexMask &universe,
                               const Span<int> values,
                               LinearAllocator<> &memory)
{
  PRF_scope_with_name("array_utils::indices_non_negative", ProfileCategory::Default);
  return IndexMask::from_predicate(
      universe, memory, [&](const int i) { return values[i] >= 0; }, exec_mode::grain_size(4096));
}

IndexMask indices_in_range(const IndexMask &universe,
                           const Span<int> values,
                           const IndexRange range,
                           LinearAllocator<> &memory)
{
  PRF_scope_with_name("array_utils::indices_in_range", ProfileCategory::Default);
  return IndexMask::from_predicate(
      universe,
      memory,
      [&](const int i) { return range.contains(values[i]); },
      exec_mode::grain_size(4096));
}

int64_t count_booleans(const VArray<bool> &varray)
{
  return count_booleans(varray, IndexMask(varray.size()));
}

bool indices_are_range(Span<int> indices, IndexRange range)
{
  if (indices.size() != range.size()) {
    return false;
  }
  PRF_scope_with_name("array_utils::indices_are_range", ProfileCategory::Default);
  return threading::parallel_reduce(
      range.index_range(),
      4096,
      true,
      [&](const IndexRange part, const bool is_range) {
        const Span<int> local_indices = indices.slice(part);
        const IndexRange local_range = range.slice(part);
        return is_range &&
               std::equal(local_indices.begin(), local_indices.end(), local_range.begin());
      },
      std::logical_and<>());
}

}  // namespace blender::array_utils
