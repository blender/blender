/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_rect.h"
#include "BLI_vector.hh"

namespace blender::compositor {

/**
 * Builds an iterator for simultaneously iterating an area of elements in an output buffer and any
 * number of input buffers. It's not a standard C++ iterator and it does not support neither
 * deference, equality or postfix increment operators.
 */
template<typename T> class BuffersIteratorBuilder {
 public:
  class Iterator {
    int x_start_;
    int x_end_;
    const T *out_end_;
    int out_elem_stride_;
    /* Stride between an output row end and the next row start. */
    int out_rows_gap_;

    struct In {
      int elem_stride;
      int rows_gap;
      const T *in;
    };
    Vector<In, 6> ins_;

    friend class BuffersIteratorBuilder;

   public:
    int x;
    int y;
    /** Current output element. */
    T *out;

   public:
    /**
     * Get current element from an input.
     */
    const T *in(int input_index) const
    {
      BLI_assert(input_index < ins_.size());
      return ins_[input_index].in;
    }

    int get_num_inputs() const
    {
      return ins_.size();
    }

    /**
     * Has the end of the area been reached.
     */
    bool is_end() const
    {
      return out >= out_end_;
    }

    /**
     * Go to the next element in the area.
     */
    void next()
    {
      out += out_elem_stride_;
      for (In &in : ins_) {
        in.in += in.elem_stride;
      }
      x++;
      if (x == x_end_) {
        x = x_start_;
        y++;
        out += out_rows_gap_;
        for (In &in : ins_) {
          in.in += in.rows_gap;
        }
      }
    }

    Iterator &operator++()
    {
      this->next();
      return *this;
    }
  };

 private:
  Iterator iterator_;
  rcti area_;
  bool is_built_;

 public:
  /**
   * Create a buffers iterator builder to iterate given output buffer area.
   * \param output: Output buffer.
   * \param buffer_area: Whole output buffer area (may have offset position).
   * \param iterated_area: Area to be iterated in all buffers.
   * \param elem_stride: Output buffer element stride.
   */
  BuffersIteratorBuilder(T *output,
                         const rcti &buffer_area,
                         const rcti &iterated_area,
                         int elem_stride = 1)
      : area_(iterated_area), is_built_(false)
  {
    BLI_assert(BLI_rcti_inside_rcti(&buffer_area, &iterated_area));
    iterator_.x = iterated_area.xmin;
    iterator_.y = iterated_area.ymin;
    iterator_.x_start_ = iterated_area.xmin;
    iterator_.x_end_ = iterated_area.xmax;

    iterator_.out_elem_stride_ = elem_stride;
    const int buffer_width = BLI_rcti_size_x(&buffer_area);
    intptr_t out_row_stride = buffer_width * elem_stride;
    iterator_.out_rows_gap_ = out_row_stride - BLI_rcti_size_x(&iterated_area) * elem_stride;
    const int out_start_x = iterated_area.xmin - buffer_area.xmin;
    const int out_start_y = iterated_area.ymin - buffer_area.ymin;
    iterator_.out = output + (intptr_t)out_start_y * out_row_stride +
                    (intptr_t)out_start_x * elem_stride;
    const T *out_row_end_ = iterator_.out +
                            (intptr_t)BLI_rcti_size_x(&iterated_area) * elem_stride;
    iterator_.out_end_ = out_row_end_ +
                         (intptr_t)out_row_stride * (BLI_rcti_size_y(&iterated_area) - 1);
  }

  /**
   * Create a buffers iterator builder to iterate given output buffer with no offsets.
   */
  BuffersIteratorBuilder(T *output, int buffer_width, int buffer_height, int elem_stride = 1)
      : BuffersIteratorBuilder(output,
                               {0, buffer_width, 0, buffer_height},
                               {0, buffer_width, 0, buffer_height},
                               elem_stride)
  {
  }

  /**
   * Add an input buffer to be iterated. It must contain iterated area.
   */
  void add_input(const T *input, const rcti &buffer_area, int elem_stride = 1)
  {
    BLI_assert(!is_built_);
    BLI_assert(BLI_rcti_inside_rcti(&buffer_area, &area_));
    typename Iterator::In in;
    in.elem_stride = elem_stride;
    const int buffer_width = BLI_rcti_size_x(&buffer_area);
    in.rows_gap = buffer_width * elem_stride - BLI_rcti_size_x(&area_) * elem_stride;
    const int in_start_x = area_.xmin - buffer_area.xmin;
    const int in_start_y = area_.ymin - buffer_area.ymin;
    in.in = input + in_start_y * buffer_width * elem_stride + in_start_x * elem_stride;
    iterator_.ins_.append(std::move(in));
  }

  /**
   * Add an input buffer to be iterated with no offsets. It must contain iterated area.
   */
  void add_input(const T *input, int buffer_width, int elem_stride = 1)
  {
    rcti buffer_area;
    BLI_rcti_init(&buffer_area, 0, buffer_width, 0, area_.ymax);
    add_input(input, buffer_area, elem_stride);
  }

  /**
   * Build the iterator.
   */
  BuffersIteratorBuilder::Iterator build()
  {
    is_built_ = true;
    return iterator_;
  }
};

template<typename T> using BuffersIterator = typename BuffersIteratorBuilder<T>::Iterator;

}  // namespace blender::compositor
