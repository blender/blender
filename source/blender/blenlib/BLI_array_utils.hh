/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#pragma once

#include <numeric>

#include "BLI_bytes_type.hh"
#include "BLI_execution_mode.hh"
#include "BLI_generic_span.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_base_c.hh"
#include "BLI_offset_indices.hh"
#include "BLI_task.hh"
#include "BLI_virtual_array.hh"

#include "PRF_profile.hh"

namespace blender::array_utils {

constexpr int64_t calc_copy_grain_size(const exec_mode::Tag auto mode, const int64_t type_size)
{
  /* The grain size should roughly depend on the work being done per index, which roughly
   * corresponds with the size of the type being copied. */
  return mode.grain_size(std::max<int64_t>(1, 32768 / type_size));
}

constexpr int64_t calc_copy_grain_size(const exec_mode::Mode mode, const int64_t type_size)
{
  return mode.grain_size(std::max<int64_t>(1, 32768 / type_size));
}

/** Similar to #Mode::grain_size(int), but also returns a tag type. */
template<exec_mode::Tag Mode>
constexpr auto exec_mode_tag_for_copy(const Mode mode, const int64_t type_size)
{
  if constexpr (mode.is_parallel) {
    return exec_mode::ParallelGrainSize{mode.grain_size(std::max<int64_t>(1, 32768 / type_size))};
  }
  else {
    return Mode{mode};
  }
}

namespace detail {

template<typename T, typename IndexT>
BLI_NOINLINE_NOCLONE void scatter_to_indices(const T *__restrict src,
                                             const IndexT *__restrict indices,
                                             const int64_t size,
                                             T *__restrict dst)
{
  for (const int64_t i : IndexRange(size)) {
    dst[indices[i]] = src[i];
  }
}

template<typename T>
inline void copy_impl(const Span<T> src,
                      const IndexMask &selection,
                      MutableSpan<T> dst,
                      const exec_mode::Mode mode)
{
  if (!mode.is_parallel) {
    index_mask::copy_assign(src, selection, dst);
    return;
  }
  threading::parallel_for(
      selection.index_range(),
      calc_copy_grain_size(mode, int64_t(sizeof(T))),
      [&](const IndexRange range) { index_mask::copy_assign(src, selection.slice(range), dst); });
}

template<typename T, typename IndexT>
inline void scatter_impl(const Span<T> src,
                         const Span<IndexT> indices,
                         MutableSpan<T> dst,
                         const exec_mode::Mode mode)
{
  if (!mode.is_parallel) {
    scatter_to_indices(src.data(), indices.data(), indices.size(), dst.data());
    return;
  }
  threading::parallel_for(indices.index_range(),
                          calc_copy_grain_size(mode, int64_t(sizeof(T))),
                          [&](const IndexRange range) {
                            scatter_to_indices(src.slice(range).data(),
                                               indices.slice(range).data(),
                                               range.size(),
                                               dst.data());
                          });
}

template<typename T>
inline void scatter_impl(const Span<T> src,
                         const IndexMask &indices,
                         MutableSpan<T> dst,
                         const exec_mode::Mode mode)
{
  if (!mode.is_parallel) {
    index_mask::scatter_assign(src, indices, dst);
    return;
  }
  threading::parallel_for(indices.index_range(),
                          calc_copy_grain_size(mode, int64_t(sizeof(T))),
                          [&](const IndexRange range) {
                            index_mask::scatter_assign(
                                src.slice(range), indices.slice(range), dst);
                          });
}

template<typename T, typename IndexT>
inline void gather_impl(const Span<T> src,
                        const Span<IndexT> indices,
                        const IndexMask &dst_mask,
                        MutableSpan<T> dst,
                        const exec_mode::Mode mode)
{
  PRF_scope(ProfileCategory::Default);
  if (!mode.is_parallel) {
    index_mask::gather_assign(src, indices, dst_mask, dst);
    return;
  }
  threading::parallel_for(dst_mask.index_range(),
                          calc_copy_grain_size(mode, int64_t(sizeof(T))),
                          [&](const IndexRange range) {
                            index_mask::gather_assign(src, indices, dst_mask.slice(range), dst);
                          });
}

template<typename T>
BLI_NOINLINE_NOCLONE void gather_to_groups_impl(const OffsetIndices<int> dst_offsets,
                                                const IndexMask &src_selection,
                                                const Span<T> src,
                                                MutableSpan<T> dst)
{
  PRF_scope(ProfileCategory::Default);
  src_selection.foreach_index(
      [&](const int src_i, const int dst_i) { dst.slice(dst_offsets[dst_i]).fill(src[src_i]); },
      exec_mode::grain_size(1024));
}

/**
 * The trivial versions of functions that copy contiguous ranges of values can be even simpler by
 * using memcpy.
 */

void copy_bytes(const void *src, void *dst, int64_t size, int64_t type_size, exec_mode::Mode mode);
void gather_group_to_group_bytes(OffsetIndices<int> src_offsets,
                                 OffsetIndices<int> dst_offsets,
                                 const IndexMask &selection,
                                 const void *src,
                                 void *dst,
                                 int64_t type_size);
void gather_group_to_group_bytes(OffsetIndices<int> src_offsets,
                                 OffsetIndices<int> dst_offsets,
                                 Span<int> selection,
                                 const void *src,
                                 void *dst,
                                 int64_t type_size);
void copy_group_to_group_bytes(OffsetIndices<int> src_offsets,
                               OffsetIndices<int> dst_offsets,
                               const IndexMask &selection,
                               const void *src,
                               void *dst,
                               int64_t type_size);

}  // namespace detail

/**
 * Fill the destination span by copying all values from the `src` array.
 */
void copy(const GVArray &src, GMutableSpan dst, exec_mode::Mode mode = exec_mode::parallel);
template<typename T, exec_mode::Tag Mode = exec_mode::Parallel>
inline void copy(const VArray<T> &src, MutableSpan<T> dst, const Mode mode = {})
{
  BLI_assert(src.size() == dst.size());
  if constexpr (!mode.is_parallel) {
    src.materialize_compressed(dst.index_range(), dst);
  }
  else {
    const int64_t grain_size = calc_copy_grain_size(mode, sizeof(T));
    threading::parallel_for(src.index_range(), grain_size, [&](const IndexRange range) {
      src.materialize(range, dst);
    });
  }
}

/**
 * Fill the destination span by copying all values from the `src` array.
 */
template<typename T, exec_mode::Tag Mode = exec_mode::Parallel>
inline void copy(const Span<T> src, MutableSpan<T> dst, const Mode mode = {})
{
  BLI_assert(src.size() == dst.size());
  if constexpr (std::is_trivially_copyable_v<T>) {
    detail::copy_bytes(src.data(), dst.data(), src.size(), sizeof(T), mode);
  }
  else if constexpr (!mode.is_parallel) {
    dst.copy_from(src);
  }
  else {
    const int64_t grain_size = calc_copy_grain_size(mode, sizeof(T));
    threading::parallel_for(src.index_range(), grain_size, [&](const IndexRange range) {
      dst.slice(range).copy_from(src.slice(range));
    });
  }
}

/**
 * Fill the destination span by copying masked values from the `src` array.
 */
void copy(const GVArray &src,
          const IndexMask &selection,
          GMutableSpan dst,
          exec_mode::Mode mode = exec_mode::parallel);

/**
 * Fill the destination span by copying values from the `src` array.
 */
template<typename T, exec_mode::Tag Mode = exec_mode::Parallel>
inline void copy(const Span<T> src,
                 const IndexMask &selection,
                 MutableSpan<T> dst,
                 const Mode mode = {})
{
  BLI_assert(src.size() == dst.size());
  using U = bytes_type::to_copy_type<T>;
  detail::copy_impl(src.template cast<U>(), selection, dst.template cast<U>(), mode);
}

/** Copy integers from a between two spans while adding an offset. */
void copy_ints_with_offset(Span<int> src,
                           int offset,
                           MutableSpan<int> dst,
                           int64_t grain_size = 4096);

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
template<typename T, typename IndexT, exec_mode::Tag Mode = exec_mode::Parallel>
inline void scatter(const Span<T> src,
                    const Span<IndexT> indices,
                    MutableSpan<T> dst,
                    const Mode mode = {})
{
  BLI_assert(indices.size() == src.size());
  using U = bytes_type::to_copy_type<T>;
  detail::scatter_impl(src.template cast<U>(), indices, dst.template cast<U>(), mode);
}

template<typename T, exec_mode::Tag Mode = exec_mode::Parallel>
inline void scatter(const Span<T> src,
                    const IndexMask &indices,
                    MutableSpan<T> dst,
                    const Mode mode = {})
{
  BLI_assert(indices.size() == src.size());
  BLI_assert(indices.min_array_size() <= dst.size());
  using U = bytes_type::to_copy_type<T>;
  detail::scatter_impl(src.template cast<U>(), indices, dst.template cast<U>(), mode);
}

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
void gather(const GVArray &src,
            const IndexMask &indices,
            GMutableSpan dst,
            exec_mode::Mode mode = exec_mode::parallel);

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
void gather(GSpan src,
            const IndexMask &indices,
            GMutableSpan dst,
            exec_mode::Mode mode = exec_mode::parallel);

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
template<typename T, exec_mode::Tag Mode = exec_mode::Parallel>
inline void gather(const VArray<T> &src,
                   const IndexMask &indices,
                   MutableSpan<T> dst,
                   const Mode mode = {})
{
  PRF_scope_with_name("array_utils::gather", ProfileCategory::Default);
  BLI_assert(indices.size() >= dst.size());
  if constexpr (!mode.is_parallel) {
    src.materialize_compressed(indices, dst);
  }
  else {
    const int64_t grain_size = calc_copy_grain_size(mode, sizeof(T));
    threading::parallel_for(indices.index_range(), grain_size, [&](const IndexRange range) {
      src.materialize_compressed(indices.slice(range), dst.slice(range));
    });
  }
}

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
template<typename T, typename IndexT, exec_mode::Tag Mode = exec_mode::Parallel>
inline void gather(const Span<T> src,
                   const Span<IndexT> indices,
                   const IndexMask &dst_mask,
                   MutableSpan<T> dst,
                   const Mode mode = {})
{
  BLI_assert(indices.size() >= dst.size());
  using U = bytes_type::to_copy_type<T>;
  detail::gather_impl(src.template cast<U>(), indices, dst_mask, dst.template cast<U>(), mode);
}

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
template<typename T, typename IndexT, exec_mode::Tag Mode = exec_mode::Parallel>
inline void gather(const Span<T> src,
                   const Span<IndexT> indices,
                   MutableSpan<T> dst,
                   const Mode mode = {})
{
  gather(src, indices, IndexMask(dst.size()), dst, mode);
}

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
template<typename T, typename IndexT, exec_mode::Tag Mode = exec_mode::Parallel>
inline void gather(const VArray<T> &src,
                   const Span<IndexT> indices,
                   const IndexMask &dst_mask,
                   MutableSpan<T> dst,
                   const Mode mode = {})
{
  PRF_scope_with_name("array_utils::gather", ProfileCategory::Default);
  BLI_assert(indices.size() >= dst_mask.min_array_size());
  const CommonVArrayInfo info = src.common_info();
  switch (info.type) {
    case CommonVArrayInfo::Type::Any: {
      dst_mask.foreach_index_optimized<int64_t>([&](const int64_t i) { dst[i] = src[indices[i]]; },
                                                exec_mode_tag_for_copy(mode, sizeof(T)));
      break;
    }
    case CommonVArrayInfo::Type::Span: {
      const Span span(static_cast<const T *>(info.data), src.size());
      gather(span, indices, dst_mask, dst, mode);
      break;
    }
    case CommonVArrayInfo::Type::Single: {
      index_mask::masked_fill(dst, *static_cast<const T *>(info.data), dst_mask);
      break;
    }
  }
}

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
template<typename T, typename IndexT, exec_mode::Tag Mode = exec_mode::Parallel>
inline void gather(const VArray<T> &src,
                   const Span<IndexT> indices,
                   MutableSpan<T> dst,
                   const Mode mode = {})
{
  gather(src, indices, dst.index_range(), dst, mode);
}

template<typename T>
inline void gather_group_to_group(const OffsetIndices<int> src_offsets,
                                  const OffsetIndices<int> dst_offsets,
                                  const IndexMask &selection,
                                  const Span<T> src,
                                  MutableSpan<T> dst)
{
  if constexpr (std::is_trivially_copyable_v<T>) {
    detail::gather_group_to_group_bytes(
        src_offsets, dst_offsets, selection, src.data(), dst.data(), sizeof(T));
  }
  else {
    selection.foreach_index(
        [&](const int64_t src_i, const int64_t dst_i) {
          dst.slice(dst_offsets[dst_i]).copy_from(src.slice(src_offsets[src_i]));
        },
        exec_mode::grain_size(512));
  }
}

template<typename T>
inline void gather_group_to_group(const OffsetIndices<int> src_offsets,
                                  const OffsetIndices<int> dst_offsets,
                                  const Span<int> selection,
                                  const Span<T> src,
                                  MutableSpan<T> dst)
{
  if constexpr (std::is_trivially_copyable_v<T>) {
    detail::gather_group_to_group_bytes(
        src_offsets, dst_offsets, selection, src.data(), dst.data(), sizeof(T));
  }
  else {
    threading::parallel_for(selection.index_range(), 512, [&](const IndexRange range) {
      for (const int dst_i : range) {
        dst.slice(dst_offsets[dst_i]).copy_from(src.slice(src_offsets[selection[dst_i]]));
      }
    });
  }
}

template<typename T>
inline void gather_group_to_group(const OffsetIndices<int> src_offsets,
                                  const OffsetIndices<int> dst_offsets,
                                  const IndexMask &selection,
                                  const VArray<T> src,
                                  MutableSpan<T> dst)
{
  selection.foreach_index(
      [&](const int64_t src_i, const int64_t dst_i) {
        src.materialize_compressed(src_offsets[src_i], dst.slice(dst_offsets[dst_i]));
      },
      exec_mode::grain_size(512));
}

template<typename T>
inline void gather_to_groups(const OffsetIndices<int> dst_offsets,
                             const IndexMask &src_selection,
                             const Span<T> src,
                             MutableSpan<T> dst)
{
  using U = bytes_type::to_copy_type<T>;
  detail::gather_to_groups_impl(
      dst_offsets, src_selection, src.template cast<U>(), dst.template cast<U>());
}

void gather(GSpan src, Span<int> indices, const IndexMask &dst_mask, GMutableSpan dst);
void gather(GSpan src, Span<int> indices, GMutableSpan dst);
void gather(const GVArray &src, Span<int> indices, const IndexMask &dst_mask, GMutableSpan dst);
void gather(const GVArray &src, Span<int> indices, GMutableSpan dst);

/** Generic version of the #gather_group_to_group template above. */
void gather_group_to_group(OffsetIndices<int> src_offsets,
                           OffsetIndices<int> dst_offsets,
                           const IndexMask &selection,
                           GSpan src,
                           GMutableSpan dst);

/** Generic version of the #gather_to_groups template above. */
void gather_to_groups(OffsetIndices<int> dst_offsets,
                      const IndexMask &src_selection,
                      GSpan src,
                      GMutableSpan dst);

/** Copy each range in \a src_ranges to the group in \a dst with the same index. */
void gather_ranges_to_groups(Span<IndexRange> src_ranges,
                             OffsetIndices<int> dst_offsets,
                             GSpan src,
                             GMutableSpan dst);

/**
 * Move elements from [src_begin, src_end) to a range starting at \a dst_begin.
 * \a dst_begin must be <= \a src_begin.
 */
void shift_left(GMutableSpan data, int src_begin, int src_end, int dst_begin);

/**
 * Move elements from [src_begin, src_end) to a range ending at \a dst_end.
 * \a dst_end must be >= \a src_end.
 */
void shift_right(GMutableSpan data, int src_begin, int src_end, int dst_end);

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
  if constexpr (std::is_trivially_copyable_v<T>) {
    detail::copy_group_to_group_bytes(
        src_offsets, dst_offsets, selection, src.data(), dst.data(), sizeof(T));
  }
  else {
    copy_group_to_group(src_offsets, dst_offsets, selection, GSpan(src), GMutableSpan(dst));
  }
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

/**
 * Find a compact index for every unique arbitrary ID, in the order that the IDs first appear.
 *
 * \param mask: The elements whose IDs are processed.
 * \param r_group_indices: The group index of every element, for every index in the mask.
 * \param r_first_indices: Optional. Filled with the first element with each ID, for every group,
 * which can be useful to choose a representative element for each group.
 * \return The number of groups.
 */
int group_ids_to_indices(Span<int> ids,
                         const IndexMask &mask,
                         MutableSpan<int> r_group_indices,
                         Vector<int> *r_first_indices = nullptr);

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

/** Return indices in the mask that are non-negative. */
IndexMask indices_non_negative(const IndexMask &universe,
                               Span<int> values,
                               LinearAllocator<> &memory);
/** Return indices in the mask that are not negative and less than the given size. */
IndexMask indices_in_range(const IndexMask &universe,
                           Span<int> values,
                           IndexRange range,
                           LinearAllocator<> &memory);

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

template<typename T, exec_mode::Tag Mode = exec_mode::Parallel>
inline void fill_index_range(const IndexMask &mask, MutableSpan<T> span, const Mode mode = {})
{
  mask.foreach_index_optimized<T>([&](const T index) { span[index] = index; }, mode);
}

template<typename T, exec_mode::Tag Mode = exec_mode::Parallel>
inline void fill_index_range(const IndexMask &mask,
                             MutableSpan<T> span,
                             const T start,
                             const Mode mode = {})
{
  if (start == 0) {
    fill_index_range(mask, span, mode);
  }
  else {
    mask.foreach_index_optimized<T>([&](const T index) { span[index] = start + index; }, mode);
  }
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

/**
 * Returns the index of the (first) maximum element in the virtual array or std::nullopt if the
 * array is empty.
 */
template<typename T>
inline std::optional<int64_t> max_element_index(const Span<T> &span,
                                                const int64_t grain_size = 8192)
{
  const T *max_it = threading::parallel_reduce(
      span.index_range(),
      grain_size,
      span.begin(),
      [&](const IndexRange range, const T *init_max) {
        const Span<T> sub_span = span.slice(range);
        const T *max_elem = std::max_element(sub_span.begin(), sub_span.end());
        if (*max_elem < *init_max) {
          return init_max;
        }
        return max_elem;
      },
      [&](const T *a, const T *b) {
        if (*a < *b) {
          return a;
        }
        return b;
      });
  return std::distance(span.begin(), max_it);
}
template<typename T>
inline std::optional<int64_t> max_element_index(const VArray<T> &array,
                                                const int64_t grain_size = 8192)
{
  if (!array || array.is_empty()) {
    return std::nullopt;
  }
  if (array.is_single()) {
    return array.first();
  }
  if (array.is_span()) {
    return max_element_index(array.get_internal_span(), grain_size);
  }
  return threading::parallel_reduce(
      array.index_range(),
      grain_size,
      array.first(),
      [&](const IndexRange range, const int64_t init_i) {
        int64_t max_index = init_i;
        T max_elem = array[max_index];
        for (const int i : range) {
          if (max_elem < array[i]) {
            max_index = i;
            max_elem = array[i];
          }
        }
        return max_index;
      },
      [&](const int64_t index_a, const int64_t index_b) {
        if (array[index_a] < array[index_b]) {
          return index_b;
        }
        return index_a;
      });
}

}  // namespace blender::array_utils
