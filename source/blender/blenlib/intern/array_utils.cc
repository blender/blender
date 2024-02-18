/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <functional>

#include "BLI_array_utils.hh"
#include "BLI_threads.h"

#include "atomic_ops.h"

namespace blender::array_utils {

void copy(const GVArray &src, GMutableSpan dst, const int64_t grain_size)
{
  BLI_assert(src.type() == dst.type());
  BLI_assert(src.size() == dst.size());
  threading::parallel_for(src.index_range(), grain_size, [&](const IndexRange range) {
    src.materialize_to_uninitialized(range, dst.data());
  });
}

void copy(const GVArray &src,
          const IndexMask &selection,
          GMutableSpan dst,
          const int64_t grain_size)
{
  BLI_assert(src.type() == dst.type());
  BLI_assert(src.size() >= selection.min_array_size());
  BLI_assert(dst.size() >= selection.min_array_size());
  threading::parallel_for(selection.index_range(), grain_size, [&](const IndexRange range) {
    src.materialize_to_uninitialized(selection.slice(range), dst.data());
  });
}

void gather(const GVArray &src,
            const IndexMask &indices,
            GMutableSpan dst,
            const int64_t grain_size)
{
  BLI_assert(src.type() == dst.type());
  BLI_assert(indices.size() == dst.size());
  threading::parallel_for(indices.index_range(), grain_size, [&](const IndexRange range) {
    src.materialize_compressed_to_uninitialized(indices.slice(range), dst.slice(range).data());
  });
}

void gather(const GSpan src, const IndexMask &indices, GMutableSpan dst, const int64_t grain_size)
{
  gather(GVArray::ForSpan(src), indices, dst, grain_size);
}

void copy_group_to_group(const OffsetIndices<int> src_offsets,
                         const OffsetIndices<int> dst_offsets,
                         const IndexMask &selection,
                         const GSpan src,
                         GMutableSpan dst)
{
  /* Each group might be large, so a threaded copy might make sense here too. */
  selection.foreach_index(GrainSize(512), [&](const int i) {
    dst.slice(dst_offsets[i]).copy_from(src.slice(src_offsets[i]));
  });
}

void count_indices(const Span<int> indices, MutableSpan<int> counts)
{
  if (indices.size() < 8192 || BLI_system_thread_count() < 4) {
    for (const int i : indices) {
      counts[i]++;
    }
  }
  else {
    threading::parallel_for(indices.index_range(), 4096, [&](const IndexRange range) {
      for (const int i : indices.slice(range)) {
        atomic_add_and_fetch_int32(&counts[i], 1);
      }
    });
  }
}

void invert_booleans(MutableSpan<bool> span)
{
  threading::parallel_for(span.index_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      span[i] = !span[i];
    }
  });
}

void invert_booleans(MutableSpan<bool> span, const IndexMask &mask)
{
  mask.foreach_index_optimized<int64_t>([&](const int64_t i) { span[i] = !span[i]; });
}

BooleanMix booleans_mix_calc(const VArray<bool> &varray, const IndexRange range_to_check)
{
  if (varray.is_empty()) {
    return BooleanMix::None;
  }
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
          const bool first = slice.first();
          for (const bool value : slice.drop_front(1)) {
            if (value != first) {
              return BooleanMix::Mixed;
            }
          }
          return first ? BooleanMix::AllTrue : BooleanMix::AllFalse;
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
        const bool first = varray[range.first()];
        for (const int64_t i : range.drop_front(1)) {
          if (varray[i] != first) {
            return BooleanMix::Mixed;
          }
        }
        return first ? BooleanMix::AllTrue : BooleanMix::AllFalse;
      },
      [&](BooleanMix a, BooleanMix b) { return (a == b) ? a : BooleanMix::Mixed; });
}

int64_t count_booleans(const VArray<bool> &varray, const IndexMask &mask)
{
  if (varray.is_empty() || mask.is_empty()) {
    return 0;
  }
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
          std::plus<int64_t>());
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
        std::plus<int64_t>());
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

int64_t count_booleans(const VArray<bool> &varray)
{
  return count_booleans(varray, IndexMask(varray.size()));
}

bool indices_are_range(Span<int> indices, IndexRange range)
{
  if (indices.size() != range.size()) {
    return false;
  }
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
      std::logical_and<bool>());
}

}  // namespace blender::array_utils
