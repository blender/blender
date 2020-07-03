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

#ifndef __BLI_INDEX_RANGE_HH__
#define __BLI_INDEX_RANGE_HH__

/** \file
 * \ingroup bli
 *
 * A `blender::IndexRange` wraps an interval of non-negative integers. It can be used to reference
 * consecutive elements in an array. Furthermore, it can make for loops more convenient and less
 * error prone, especially when using nested loops.
 *
 * I'd argue that the second loop is more readable and less error prone than the first one. That is
 * not necessarily always the case, but often it is.
 *
 *  for (uint i = 0; i < 10; i++) {
 *    for (uint j = 0; j < 20; j++) {
 *       for (uint k = 0; k < 30; k++) {
 *
 *  for (uint i : IndexRange(10)) {
 *    for (uint j : IndexRange(20)) {
 *      for (uint k : IndexRange(30)) {
 *
 * Some containers like blender::Vector have an index_range() method. This will return the
 * IndexRange that contains all indices that can be used to access the container. This is
 * particularly useful when you want to iterate over the indices and the elements (much like
 * Python's enumerate(), just worse). Again, I think the second example here is better:
 *
 *  for (uint i = 0; i < my_vector_with_a_long_name.size(); i++) {
 *    do_something(i, my_vector_with_a_long_name[i]);
 *
 *  for (uint i : my_vector_with_a_long_name.index_range()) {
 *    do_something(i, my_vector_with_a_long_name[i]);
 *
 * Ideally this could be could be even closer to Python's enumerate(). We might get that in the
 * future with newer C++ versions.
 *
 * One other important feature is the as_span method. This method returns an Span<uint>
 * that contains the interval as individual numbers.
 */

#include <algorithm>
#include <cmath>
#include <iostream>

#include "BLI_utildefines.h"

/* Forward declare tbb::blocked_range for conversion operations. */
namespace tbb {
template<typename Value> class blocked_range;
}

namespace blender {

template<typename T> class Span;

class IndexRange {
 private:
  uint start_ = 0;
  uint size_ = 0;

 public:
  IndexRange() = default;

  explicit IndexRange(uint size) : start_(0), size_(size)
  {
  }

  IndexRange(uint start, uint size) : start_(start), size_(size)
  {
  }

  template<typename T>
  IndexRange(const tbb::blocked_range<T> &range) : start_(range.begin()), size_(range.size())
  {
  }

  class Iterator {
   private:
    uint current_;

   public:
    Iterator(uint current) : current_(current)
    {
    }

    Iterator &operator++()
    {
      current_++;
      return *this;
    }

    bool operator!=(const Iterator &iterator) const
    {
      return current_ != iterator.current_;
    }

    uint operator*() const
    {
      return current_;
    }
  };

  Iterator begin() const
  {
    return Iterator(start_);
  }

  Iterator end() const
  {
    return Iterator(start_ + size_);
  }

  /**
   * Access an element in the range.
   */
  uint operator[](uint index) const
  {
    BLI_assert(index < this->size());
    return start_ + index;
  }

  /**
   * Two ranges compare equal when they contain the same numbers.
   */
  friend bool operator==(IndexRange a, IndexRange b)
  {
    return (a.size_ == b.size_) && (a.start_ == b.start_ || a.size_ == 0);
  }

  /**
   * Get the amount of numbers in the range.
   */
  uint size() const
  {
    return size_;
  }

  /**
   * Create a new range starting at the end of the current one.
   */
  IndexRange after(uint n) const
  {
    return IndexRange(start_ + size_, n);
  }

  /**
   * Create a new range that ends at the start of the current one.
   */
  IndexRange before(uint n) const
  {
    return IndexRange(start_ - n, n);
  }

  /**
   * Get the first element in the range.
   * Asserts when the range is empty.
   */
  uint first() const
  {
    BLI_assert(this->size() > 0);
    return start_;
  }

  /**
   * Get the last element in the range.
   * Asserts when the range is empty.
   */
  uint last() const
  {
    BLI_assert(this->size() > 0);
    return start_ + size_ - 1;
  }

  /**
   * Get the element one after the end. The returned value is undefined when the range is empty.
   */
  uint one_after_last() const
  {
    return start_ + size_;
  }

  /**
   * Get the first element in the range. The returned value is undefined when the range is empty.
   */
  uint start() const
  {
    return start_;
  }

  /**
   * Returns true when the range contains a certain number, otherwise false.
   */
  bool contains(uint value) const
  {
    return value >= start_ && value < start_ + size_;
  }

  /**
   * Returns a new range, that contains a sub-interval of the current one.
   */
  IndexRange slice(uint start, uint size) const
  {
    uint new_start = start_ + start;
    BLI_assert(new_start + size <= start_ + size_ || size == 0);
    return IndexRange(new_start, size);
  }
  IndexRange slice(IndexRange range) const
  {
    return this->slice(range.start(), range.size());
  }

  /**
   * Get read-only access to a memory buffer that contains the range as actual numbers.
   */
  Span<uint> as_span() const;

  friend std::ostream &operator<<(std::ostream &stream, IndexRange range)
  {
    stream << "[" << range.start() << ", " << range.one_after_last() << ")";
    return stream;
  }
};

}  // namespace blender

#endif /* __BLI_INDEX_RANGE_HH__ */
