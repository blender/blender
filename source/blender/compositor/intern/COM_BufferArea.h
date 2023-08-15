/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_assert.h"
#include "BLI_rect.h"
#include <iterator>

namespace blender::compositor {

/* Forward declarations. */
template<typename T> class BufferAreaIterator;

/**
 * A rectangle area of buffer elements.
 */
template<typename T> class BufferArea : rcti {
 public:
  using Iterator = BufferAreaIterator<T>;
  using ConstIterator = BufferAreaIterator<const T>;

 private:
  T *buffer_;
  /* Number of elements in a buffer row. */
  int buffer_width_;
  /* Buffer element stride. */
  int elem_stride_;

 public:
  constexpr BufferArea() = default;

  /**
   * Create a buffer area containing given rectangle area.
   */
  constexpr BufferArea(T *buffer, int buffer_width, const rcti &area, int elem_stride = 1)
      : rcti(area), buffer_(buffer), buffer_width_(buffer_width), elem_stride_(elem_stride)
  {
  }

  /**
   * Create a buffer area containing whole buffer with no offsets.
   */
  constexpr BufferArea(T *buffer, int buffer_width, int buffer_height, int elem_stride = 1)
      : buffer_(buffer), buffer_width_(buffer_width), elem_stride_(elem_stride)
  {
    BLI_rcti_init(this, 0, buffer_width, 0, buffer_height);
  }

  constexpr friend bool operator==(const BufferArea &a, const BufferArea &b)
  {
    return a.buffer_ == b.buffer_ && BLI_rcti_compare(&a, &b) && a.elem_stride_ == b.elem_stride_;
  }

  constexpr const rcti &get_rect() const
  {
    return *this;
  }

  /**
   * Number of elements in a row.
   */
  constexpr int width() const
  {
    return BLI_rcti_size_x(this);
  }

  /**
   * Number of elements in a column.
   */
  constexpr int height() const
  {
    return BLI_rcti_size_y(this);
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
    T *end_ptr = get_end_ptr();
    if (elem_stride_ == 0) {
      /* Iterate a single element. */
      return TIterator(buffer_, end_ptr, 1, 1, 1);
    }

    T *begin_ptr = buffer_ + (intptr_t)this->ymin * buffer_width_ * elem_stride_ +
                   (intptr_t)this->xmin * elem_stride_;
    return TIterator(begin_ptr, end_ptr, buffer_width_, BLI_rcti_size_x(this), elem_stride_);
  }

  template<typename TIterator> constexpr TIterator end_iterator() const
  {
    T *end_ptr = get_end_ptr();
    if (elem_stride_ == 0) {
      /* Iterate a single element. */
      return TIterator(end_ptr, end_ptr, 1, 1, 1);
    }

    return TIterator(end_ptr, end_ptr, buffer_width_, BLI_rcti_size_x(this), elem_stride_);
  }

  T *get_end_ptr() const
  {
    if (elem_stride_ == 0) {
      return buffer_ + 1;
    }
    return buffer_ + (intptr_t)(this->ymax - 1) * buffer_width_ * elem_stride_ +
           (intptr_t)this->xmax * elem_stride_;
  }
};

template<typename T> class BufferAreaIterator {
 public:
  using iterator_category = std::input_iterator_tag;
  using value_type = T *;
  using pointer = T *const *;
  using reference = T *const &;
  using difference_type = std::ptrdiff_t;

 private:
  int elem_stride_;
  int row_stride_;
  /* Stride between a row end and the next row start. */
  int rows_gap_;
  T *current_;
  const T *row_end_;
  const T *end_;

 public:
  constexpr BufferAreaIterator() = default;

  constexpr BufferAreaIterator(
      T *current, const T *end, int buffer_width, int area_width, int elem_stride = 1)
      : elem_stride_(elem_stride),
        row_stride_(buffer_width * elem_stride),
        rows_gap_(row_stride_ - area_width * elem_stride),
        current_(current),
        row_end_(current + area_width * elem_stride),
        end_(end)
  {
  }

  constexpr BufferAreaIterator &operator++()
  {
    current_ += elem_stride_;
    BLI_assert(current_ <= row_end_);
    if (current_ == row_end_) {
      BLI_assert(current_ <= end_);
      if (current_ == end_) {
        return *this;
      }
      current_ += rows_gap_;
      row_end_ += row_stride_;
    }
    return *this;
  }

  constexpr BufferAreaIterator operator++(int) const
  {
    BufferAreaIterator copied_iterator = *this;
    ++copied_iterator;
    return copied_iterator;
  }

  constexpr friend bool operator!=(const BufferAreaIterator &a, const BufferAreaIterator &b)
  {
    return a.current_ != b.current_;
  }

  constexpr friend bool operator==(const BufferAreaIterator &a, const BufferAreaIterator &b)
  {
    return a.current_ == b.current_;
  }

  constexpr T *operator*() const
  {
    return current_;
  }
};

}  // namespace blender::compositor
