/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#pragma once

#include <numeric>

#include "BLI_generic_span.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_base.h"
#include "BLI_offset_indices.hh"
#include "BLI_task.hh"
#include "BLI_virtual_array.hh"

namespace blender::array_utils {

/**
 * Fill the destination span by copying all values from the `src` array. Threaded based on
 * grain-size.
 */
void copy(const GVArray &src, GMutableSpan dst, int64_t grain_size = 4096);
template<typename T>
inline void copy(const VArray<T> &src, MutableSpan<T> dst, const int64_t grain_size = 4096)
{
  BLI_assert(src.size() == dst.size());
  threading::parallel_for(src.index_range(), grain_size, [&](const IndexRange range) {
    src.materialize_to_uninitialized(range, dst);
  });
}

/**
 * Fill the destination span by copying all values from the `src` array. Threaded based on
 * grain-size.
 */
template<typename T>
inline void copy(const Span<T> src, MutableSpan<T> dst, const int64_t grain_size = 4096)
{
  BLI_assert(src.size() == dst.size());
  threading::parallel_for(src.index_range(), grain_size, [&](const IndexRange range) {
    dst.slice(range).copy_from(src.slice(range));
  });
}

/**
 * Fill the destination span by copying masked values from the `src` array. Threaded based on
 * grain-size.
 */
void copy(const GVArray &src,
          const IndexMask &selection,
          GMutableSpan dst,
          int64_t grain_size = 4096);

/**
 * Fill the destination span by copying values from the `src` array. Threaded based on
 * grain-size.
 */
template<typename T>
inline void copy(const Span<T> src,
                 const IndexMask &selection,
                 MutableSpan<T> dst,
                 const int64_t grain_size = 4096)
{
  BLI_assert(src.size() == dst.size());
  selection.foreach_index_optimized<int64_t>(GrainSize(grain_size),
                                             [&](const int64_t i) { dst[i] = src[i]; });
}

template<typename T> T compute_sum(const Span<T> data)
{
  /* Explicitly splitting work into chunks for a couple of reasons:
   * - Improve numerical stability. While there are even more stable algorithms (e.g. Kahan
   *   summation), they also add more complexity to the hot code path. So far, this simple approach
   *   seems to solve the common issues people run into.
   * - Support computing the sum using multiple threads.
   * - Ensure deterministic results even with floating point numbers.
   */
  constexpr int64_t chunk_size = 1024;
  const int64_t chunks_num = divide_ceil_ul(data.size(), chunk_size);
  Array<T> partial_sums(chunks_num);
  threading::parallel_for(partial_sums.index_range(), 1, [&](const IndexRange range) {
    for (const int64_t i : range) {
      const int64_t start = i * chunk_size;
      const Span<T> chunk = data.slice_safe(start, chunk_size);
      const T partial_sum = std::accumulate(chunk.begin(), chunk.end(), T());
      partial_sums[i] = partial_sum;
    }
  });
  return std::accumulate(partial_sums.begin(), partial_sums.end(), T());
}

/**
 * Fill the specified indices of the destination with the values in the source span.
 */
template<typename T, typename IndexT>
inline void scatter(const Span<T> src,
                    const Span<IndexT> indices,
                    MutableSpan<T> dst,
                    const int64_t grain_size = 4096)
{
  BLI_assert(indices.size() == src.size());
  threading::parallel_for(indices.index_range(), grain_size, [&](const IndexRange range) {
    for (const int64_t i : range) {
      dst[indices[i]] = src[i];
    }
  });
}

template<typename T>
inline void scatter(const Span<T> src,
                    const IndexMask &indices,
                    MutableSpan<T> dst,
                    const int64_t grain_size = 4096)
{
  BLI_assert(indices.size() == src.size());
  BLI_assert(indices.min_array_size() <= dst.size());
  indices.foreach_index_optimized<int64_t>(
      GrainSize(grain_size),
      [&](const int64_t index, const int64_t pos) { dst[index] = src[pos]; });
}

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
void gather(const GVArray &src,
            const IndexMask &indices,
            GMutableSpan dst,
            int64_t grain_size = 4096);

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
void gather(GSpan src, const IndexMask &indices, GMutableSpan dst, int64_t grain_size = 4096);

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
template<typename T>
inline void gather(const VArray<T> &src,
                   const IndexMask &indices,
                   MutableSpan<T> dst,
                   const int64_t grain_size = 4096)
{
  BLI_assert(indices.size() == dst.size());
  threading::parallel_for(indices.index_range(), grain_size, [&](const IndexRange range) {
    src.materialize_compressed_to_uninitialized(indices.slice(range), dst.slice(range));
  });
}

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
template<typename T>
inline void gather(const Span<T> src,
                   const IndexMask &indices,
                   MutableSpan<T> dst,
                   const int64_t grain_size = 4096)
{
  BLI_assert(indices.size() == dst.size());
  indices.foreach_segment(GrainSize(grain_size),
                          [&](const IndexMaskSegment segment, const int64_t segment_pos) {
                            for (const int64_t i : segment.index_range()) {
                              dst[segment_pos + i] = src[segment[i]];
                            }
                          });
}

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
template<typename T, typename IndexT>
inline void gather(const Span<T> src,
                   const Span<IndexT> indices,
                   MutableSpan<T> dst,
                   const int64_t grain_size = 4096)
{
  BLI_assert(indices.size() == dst.size());
  threading::parallel_for(indices.index_range(), grain_size, [&](const IndexRange range) {
    for (const int64_t i : range) {
      dst[i] = src[indices[i]];
    }
  });
}

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
template<typename T, typename IndexT>
inline void gather(const VArray<T> &src,
                   const Span<IndexT> indices,
                   MutableSpan<T> dst,
                   const int64_t grain_size = 4096)
{
  BLI_assert(indices.size() == dst.size());
  devirtualize_varray(src, [&](const auto &src) {
    threading::parallel_for(indices.index_range(), grain_size, [&](const IndexRange range) {
      for (const int64_t i : range) {
        dst[i] = src[indices[i]];
      }
    });
  });
}

template<typename T>
inline void gather_group_to_group(const OffsetIndices<int> src_offsets,
                                  const OffsetIndices<int> dst_offsets,
                                  const IndexMask &selection,
                                  const Span<T> src,
                                  MutableSpan<T> dst)
{
  selection.foreach_index(GrainSize(512), [&](const int64_t src_i, const int64_t dst_i) {
    dst.slice(dst_offsets[dst_i]).copy_from(src.slice(src_offsets[src_i]));
  });
}

template<typename T>
inline void gather_group_to_group(const OffsetIndices<int> src_offsets,
                                  const OffsetIndices<int> dst_offsets,
                                  const IndexMask &selection,
                                  const VArray<T> src,
                                  MutableSpan<T> dst)
{
  selection.foreach_index(GrainSize(512), [&](const int64_t src_i, const int64_t dst_i) {
    src.materialize_compressed(src_offsets[src_i], dst.slice(dst_offsets[dst_i]));
  });
}

template<typename T>
inline void gather_to_groups(const OffsetIndices<int> dst_offsets,
                             const IndexMask &src_selection,
                             const Span<T> src,
                             MutableSpan<T> dst)
{
  src_selection.foreach_index(GrainSize(1024), [&](const int src_i, const int dst_i) {
    dst.slice(dst_offsets[dst_i]).fill(src[src_i]);
  });
}

/**
 * Copy the \a src data from the groups defined by \a src_offsets to the groups in \a dst defined
 * by \a dst_offsets. Groups to use are masked by \a selection, and it is assumed that the
 * corresponding groups have the same size.
 */
void copy_group_to_group(OffsetIndices<int> src_offsets,
                         OffsetIndices<int> dst_offsets,
                         const IndexMask &selection,
                         GSpan src,
                         GMutableSpan dst);
template<typename T>
void copy_group_to_group(OffsetIndices<int> src_offsets,
                         OffsetIndices<int> dst_offsets,
                         const IndexMask &selection,
                         Span<T> src,
                         MutableSpan<T> dst)
{
  copy_group_to_group(src_offsets, dst_offsets, selection, GSpan(src), GMutableSpan(dst));
}

/**
 * Count the number of occurrences of each index.
 * \param indices: The indices to count.
 * \param counts: The number of occurrences of each index. Typically initialized to zero.
 * Must be large enough to contain the maximum index.
 *
 * \note The memory referenced by the two spans must not overlap.
 */
void count_indices(Span<int> indices, MutableSpan<int> counts);

void invert_booleans(MutableSpan<bool> span);
void invert_booleans(MutableSpan<bool> span, const IndexMask &mask);

int64_t count_booleans(const VArray<bool> &varray);
int64_t count_booleans(const VArray<bool> &varray, const IndexMask &mask);

enum class BooleanMix {
  None,
  AllFalse,
  AllTrue,
  Mixed,
};
BooleanMix booleans_mix_calc(const VArray<bool> &varray, IndexRange range_to_check);
inline BooleanMix booleans_mix_calc(const VArray<bool> &varray)
{
  return booleans_mix_calc(varray, varray.index_range());
}

/** Check if the value exists in the array. */
bool contains(const VArray<bool> &varray, const IndexMask &indices_to_check, bool value);

/**
 * Finds all the index ranges for which consecutive values in \a span equal \a value.
 */
template<typename T> inline Vector<IndexRange> find_all_ranges(const Span<T> span, const T &value)
{
  if (span.is_empty()) {
    return Vector<IndexRange>();
  }
  Vector<IndexRange> ranges;
  int64_t length = (span.first() == value) ? 1 : 0;
  for (const int64_t i : span.index_range().drop_front(1)) {
    if (span[i - 1] == value && span[i] != value) {
      ranges.append(IndexRange::from_end_size(i, length));
      length = 0;
    }
    else if (span[i] == value) {
      length++;
    }
  }
  if (length > 0) {
    ranges.append(IndexRange::from_end_size(span.size(), length));
  }
  return ranges;
}

/**
 * Fill the span with increasing indices: 0, 1, 2, ...
 * Optionally, the start value can be provided.
 */
template<typename T> inline void fill_index_range(MutableSpan<T> span, const T start = 0)
{
  std::iota(span.begin(), span.end(), start);
}

template<typename T>
bool indexed_data_equal(const Span<T> all_values, const Span<int> indices, const Span<T> values)
{
  BLI_assert(indices.size() == values.size());
  for (const int i : indices.index_range()) {
    if (all_values[indices[i]] != values[i]) {
      return false;
    }
  }
  return true;
}

bool indices_are_range(Span<int> indices, IndexRange range);

}  // namespace blender::array_utils
