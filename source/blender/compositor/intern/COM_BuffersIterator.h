/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2021, Blender Foundation.
 */

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
    const T *out_end_;
    const T *out_row_end_;
    int out_elem_stride_;
    int out_row_stride_;
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
    /**
     * Current output element.
     */
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
      if (out == out_row_end_) {
        out += out_rows_gap_;
        out_row_end_ += out_row_stride_;
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
   * \param buffer_width: Number of elements in an output buffer row.
   * \param area: Rectangle area to be iterated in all buffers.
   * \param elem_stride: Output buffer element stride.
   */
  BuffersIteratorBuilder(T *output, int buffer_width, const rcti &area, int elem_stride = 1)
      : area_(area), is_built_(false)
  {
    iterator_.out_elem_stride_ = elem_stride;
    iterator_.out_row_stride_ = buffer_width * elem_stride;
    iterator_.out_rows_gap_ = iterator_.out_row_stride_ - BLI_rcti_size_x(&area) * elem_stride;
    iterator_.out = output + (intptr_t)area.ymin * iterator_.out_row_stride_ +
                    (intptr_t)area.xmin * elem_stride;
    iterator_.out_row_end_ = iterator_.out + (intptr_t)BLI_rcti_size_x(&area) * elem_stride;
    iterator_.out_end_ = iterator_.out_row_end_ +
                         (intptr_t)iterator_.out_row_stride_ * (BLI_rcti_size_y(&area) - 1);
  }

  /**
   * Create a buffers iterator builder to iterate given output buffer with no offsets.
   */
  BuffersIteratorBuilder(T *output, int buffer_width, int buffer_height, int elem_stride = 1)
      : BuffersIteratorBuilder(
            output, buffer_width, {0, buffer_width, 0, buffer_height}, elem_stride)
  {
  }

  /**
   * Add an input buffer to be iterated. Its coordinates must be correlated with the output.
   */
  void add_input(const T *input, int buffer_width, int elem_stride = 1)
  {
    BLI_assert(!is_built_);
    typename Iterator::In in;
    in.elem_stride = elem_stride;
    in.rows_gap = buffer_width * elem_stride - BLI_rcti_size_x(&area_) * elem_stride;
    in.in = input + area_.ymin * buffer_width * elem_stride + area_.xmin * elem_stride;
    iterator_.ins_.append(std::move(in));
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
