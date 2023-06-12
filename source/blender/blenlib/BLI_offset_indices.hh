/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <algorithm>

#include "BLI_index_mask.hh"
#include "BLI_index_range.hh"
#include "BLI_span.hh"

namespace blender::offset_indices {

/**
 * References an array of ascending indices. A pair of consecutive indices encode an index range.
 * Another common way to store the same kind of data is to store the start and size of every range
 * separately. Using offsets instead halves the memory consumption. The downside is that the
 * array has to be one element longer than the total number of ranges. The extra element is
 * necessary to be able to get the last index range without requiring an extra branch for the case.
 *
 * This class is a thin wrapper around such an array that makes it easy to retrieve the index range
 * at a specific index.
 */
template<typename T> class OffsetIndices {
 private:
  static_assert(std::is_integral_v<T>);

  Span<T> offsets_;

 public:
  OffsetIndices() = default;
  OffsetIndices(const Span<T> offsets) : offsets_(offsets)
  {
    BLI_assert(offsets_.size() < 2 || std::is_sorted(offsets_.begin(), offsets_.end()));
  }

  /** Return the total number of elements in the referenced arrays. */
  T total_size() const
  {
    return offsets_.size() > 1 ? offsets_.last() : 0;
  }

  /**
   * Return the number of ranges encoded by the offsets, not including the last value used
   * internally.
   */
  int64_t size() const
  {
    return std::max<int64_t>(offsets_.size() - 1, 0);
  }

  bool is_empty() const
  {
    return this->size() == 0;
  }

  IndexRange index_range() const
  {
    return IndexRange(this->size());
  }

  IndexRange operator[](const int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < offsets_.size() - 1);
    const int64_t begin = offsets_[index];
    const int64_t end = offsets_[index + 1];
    const int64_t size = end - begin;
    return IndexRange(begin, size);
  }

  IndexRange operator[](const IndexRange indices) const
  {
    const int64_t begin = offsets_[indices.start()];
    const int64_t end = offsets_[indices.one_after_last()];
    const int64_t size = end - begin;
    return IndexRange(begin, size);
  }

  /**
   * Return a subset of the offsets describing the specified range of source elements.
   * This is a slice into the source ranges rather than the indexed elements described by the
   * offset values.
   */
  OffsetIndices slice(const IndexRange range) const
  {
    BLI_assert(offsets_.index_range().drop_back(1).contains(range.last()));
    return OffsetIndices(offsets_.slice(range.start(), range.one_after_last()));
  }

  const T *data() const
  {
    return offsets_.data();
  }
};

/**
 * References many separate spans in a larger contiguous array. This gives a more efficient way to
 * store many grouped arrays, without requiring many small allocations, giving the general benefits
 * of using contiguous memory.
 *
 * \note If the offsets are shared between many #GroupedSpan objects, it will still
 * be more efficient to retrieve the #IndexRange only once and slice each span.
 */
template<typename T> struct GroupedSpan {
  OffsetIndices<int> offsets;
  Span<T> data;

  GroupedSpan() = default;
  GroupedSpan(OffsetIndices<int> offsets, Span<T> data) : offsets(offsets), data(data)
  {
    BLI_assert(this->offsets.total_size() == this->data.size());
  }

  Span<T> operator[](const int64_t index) const
  {
    return this->data.slice(this->offsets[index]);
  }

  int64_t size() const
  {
    return this->offsets.size();
  }

  IndexRange index_range() const
  {
    return this->offsets.index_range();
  }

  bool is_empty() const
  {
    return this->data.size() == 0;
  }
};

/**
 * Turn an array of sizes into the offset at each index including all previous sizes.
 */
OffsetIndices<int> accumulate_counts_to_offsets(MutableSpan<int> counts_to_offsets,
                                                int start_offset = 0);

/** Copy the number of indices in every group in the mask to the corresponding index. */
void copy_group_sizes(OffsetIndices<int> offsets, const IndexMask &mask, MutableSpan<int> sizes);

/** Gather the number of indices in each indexed group to sizes. */
void gather_group_sizes(OffsetIndices<int> offsets, const IndexMask &mask, MutableSpan<int> sizes);

/** Build new offsets that contains only the groups chosen by \a selection. */
OffsetIndices<int> gather_selected_offsets(OffsetIndices<int> src_offsets,
                                           const IndexMask &selection,
                                           MutableSpan<int> dst_offsets);
/**
 * Create a map from indexed elements to the source indices, in other words from the larger array
 * to the smaller array.
 */
void build_reverse_map(OffsetIndices<int> offsets, MutableSpan<int> r_map);

/**
 * Build offsets to group the elements of \a indices pointing to the same index.
 */
void build_reverse_offsets(Span<int> indices, MutableSpan<int> r_map);

}  // namespace blender::offset_indices

namespace blender {
using offset_indices::GroupedSpan;
using offset_indices::OffsetIndices;
}  // namespace blender
