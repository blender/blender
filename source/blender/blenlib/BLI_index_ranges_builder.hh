/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <array>

#include "BLI_index_range.hh"
#include "BLI_index_ranges_builder_fwd.hh"
#include "BLI_span.hh"
#include "BLI_utility_mixins.hh"

namespace blender {

/**
 * A data structure that is designed to allow building many index ranges efficiently.
 *
 * One first has to add individual indices or ranges in ascending order. Internally, consecutive
 * indices and ranges are automatically joined.
 *
 * \note This data structure has a pre-defined capacity and can not automatically grow once that
 * capacity is reached. Use #IndexRangesBuilderBuffer to control the capacity.
 */
template<typename T> class IndexRangesBuilder : NonCopyable, NonMovable {
 private:
  /** The current pointer into #data_. It's changed whenever a new range starts. */
  T *c_;
  /** Structure: [-1, start, end, start, end, start, end, ...]. */
  MutableSpan<T> data_;

 public:
  IndexRangesBuilder(MutableSpan<T> data) : data_(data)
  {
    static_assert(std::is_signed_v<T>);
    /* Set the first value to -1 so that when the first index is added, it is detected as the start
     * of a new range. */
    data_[0] = -1;
    c_ = data_.data();
  }

  /** Add a new index. It has to be larger than any previously added index. */
  bool add(const T index)
  {
    return this->add_range(index, index + 1);
  }

  /**
   * Add a range of indices. It has to start after any previously added index.
   * By design, this is branchless and requires O(1) time.
   */
  bool add_range(const T start, const T end)
  {
    /* Indices have to be added in ascending order. */
    BLI_assert(start >= *c_);
    BLI_assert(start >= 0);
    BLI_assert(start < end);

    const bool is_new_range = start > *c_;

    /* Check that the capacity is not overflown. */
    BLI_assert(!is_new_range || this->size() < this->capacity());

    /* This is designed to either append to the last range or start a new range.
     * It is intentionally branchless for more predictable performance on unpredictable data. */
    c_ += is_new_range;
    *c_ = start;
    c_ += is_new_range;
    *c_ = end;

    return is_new_range;
  }

  /** Number of collected ranges. */
  int64_t size() const
  {
    return (c_ - data_.data()) / 2;
  }

  /** How many ranges this container can hold at most. */
  int64_t capacity() const
  {
    return data_.size() / 2;
  }

  /** True if there are no ranges yet. */
  bool is_empty() const
  {
    return c_ == data_.data();
  }

  IndexRange index_range() const
  {
    return IndexRange(this->size());
  }

  /** Get the i-th collected #IndexRange. */
  IndexRange operator[](const int64_t i) const
  {
    const T start = data_[size_t(1) + 2 * size_t(i)];
    const T end = data_[size_t(2) + 2 * size_t(i)];
    return IndexRange::from_begin_end(start, end);
  }

  static constexpr int64_t buffer_size_for_ranges_num(const int64_t ranges_num)
  {
    /* Two values for each range (start, end) and the dummy prefix value. */
    return ranges_num * 2 + 1;
  }
};

template<typename T, int64_t MaxRangesNum> struct IndexRangesBuilderBuffer {
  std::array<T, size_t(IndexRangesBuilder<T>::buffer_size_for_ranges_num(MaxRangesNum))> data;

  operator MutableSpan<T>()
  {
    return this->data;
  }
};

}  // namespace blender
