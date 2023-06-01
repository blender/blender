/* SPDX-FileCopyrightText: 2021 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_assert.h"

#include <iterator>

namespace blender::compositor {

/* Forward declarations. */
template<typename T> class BufferRangeIterator;

/**
 * A range of buffer elements.
 */
template<typename T> class BufferRange {
 public:
  using Iterator = BufferRangeIterator<T>;
  using ConstIterator = BufferRangeIterator<const T>;

 private:
  T *start_;
  /* Number of elements in the range. */
  int64_t size_;
  /* Buffer element stride. */
  int elem_stride_;

 public:
  constexpr BufferRange() = default;

  /**
   * Create a buffer range of elements from a given element index.
   */
  constexpr BufferRange(T *buffer, int64_t start_elem_index, int64_t size, int elem_stride = 1)
      : start_(buffer + start_elem_index * elem_stride), size_(size), elem_stride_(elem_stride)
  {
  }

  constexpr friend bool operator==(const BufferRange &a, const BufferRange &b)
  {
    return a.start_ == b.start_ && a.size_ == b.size_ && a.elem_stride_ == b.elem_stride_;
  }

  /**
   * Access an element in the range. Index is relative to range start.
   */
  constexpr T *operator[](int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->size());
    return start_ + index * elem_stride_;
  }

  /**
   * Get the number of elements in the range.
   */
  constexpr int64_t size() const
  {
    return size_;
  }

  constexpr Iterator begin()
  {
    return begin_iterator<Iterator>();
  }

  constexpr Iterator end()
  {
    return end_iterator<Iterator>();
  }

  constexpr ConstIterator begin() const
  {
    return begin_iterator<ConstIterator>();
  }

  constexpr ConstIterator end() const
  {
    return end_iterator<ConstIterator>();
  }

 private:
  template<typename TIterator> constexpr TIterator begin_iterator() const
  {
    if (elem_stride_ == 0) {
      /* Iterate a single element. */
      return TIterator(start_, 1);
    }

    return TIterator(start_, elem_stride_);
  }

  template<typename TIterator> constexpr TIterator end_iterator() const
  {
    if (elem_stride_ == 0) {
      /* Iterate a single element. */
      return TIterator(start_ + 1, 1);
    }

    return TIterator(start_ + size_ * elem_stride_, elem_stride_);
  }
};

template<typename T> class BufferRangeIterator {
 public:
  using iterator_category = std::input_iterator_tag;
  using value_type = T *;
  using pointer = T *const *;
  using reference = T *const &;
  using difference_type = std::ptrdiff_t;

 private:
  T *current_;
  int elem_stride_;

 public:
  constexpr BufferRangeIterator() = default;

  constexpr BufferRangeIterator(T *current, int elem_stride = 1)
      : current_(current), elem_stride_(elem_stride)
  {
  }

  constexpr BufferRangeIterator &operator++()
  {
    current_ += elem_stride_;
    return *this;
  }

  constexpr BufferRangeIterator operator++(int) const
  {
    BufferRangeIterator copied_iterator = *this;
    ++copied_iterator;
    return copied_iterator;
  }

  constexpr friend bool operator!=(const BufferRangeIterator &a, const BufferRangeIterator &b)
  {
    return a.current_ != b.current_;
  }

  constexpr friend bool operator==(const BufferRangeIterator &a, const BufferRangeIterator &b)
  {
    return a.current_ == b.current_;
  }

  constexpr T *operator*() const
  {
    return current_;
  }
};

}  // namespace blender::compositor
