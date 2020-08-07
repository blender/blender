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
 */

#pragma once

/** \file
 * \ingroup bli
 *
 * A `blender::VectorAdaptor` is a container with a fixed maximum size and does not own the
 * underlying memory. When an adaptor is constructed, you have to provide it with an uninitialized
 * array that will be filled when elements are added to the vector. The vector adaptor is not able
 * to grow. Therefore, it is undefined behavior to add more elements than fit into the provided
 * buffer.
 */

#include "BLI_span.hh"

namespace blender {

template<typename T> class VectorAdaptor {
 private:
  T *begin_;
  T *end_;
  T *capacity_end_;

 public:
  VectorAdaptor() : begin_(nullptr), end_(nullptr), capacity_end_(nullptr)
  {
  }

  VectorAdaptor(T *data, int64_t capacity, int64_t size = 0)
      : begin_(data), end_(data + size), capacity_end_(data + capacity)
  {
  }

  VectorAdaptor(MutableSpan<T> span) : VectorAdaptor(span.data(), span.size(), 0)
  {
  }

  void append(const T &value)
  {
    BLI_assert(end_ < capacity_end_);
    new (end_) T(value);
    end_++;
  }

  void append(T &&value)
  {
    BLI_assert(end_ < capacity_end_);
    new (end_) T(std::move(value));
    end_++;
  }

  void append_n_times(const T &value, int64_t n)
  {
    BLI_assert(end_ + n <= capacity_end_);
    uninitialized_fill_n(end_, n, value);
    end_ += n;
  }

  void extend(Span<T> values)
  {
    BLI_assert(end_ + values.size() <= capacity_end_);
    uninitialized_copy_n(values.data(), values.size(), end_);
    end_ += values.size();
  }

  int64_t capacity() const
  {
    return capacity_end_ - begin_;
  }

  int64_t size() const
  {
    return end_ - begin_;
  }

  bool is_empty() const
  {
    return begin_ == end_;
  }

  bool is_full() const
  {
    return end_ == capacity_end_;
  }
};

}  // namespace blender
