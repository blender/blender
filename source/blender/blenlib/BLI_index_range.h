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

#ifndef __BLI_INDEX_RANGE_H__
#define __BLI_INDEX_RANGE_H__

/** \file
 * \ingroup bli
 *
 * Allows passing iterators over ranges of integers without actually allocating an array or passing
 * separate values. A range always has a step of one. If other step sizes are required in some
 * cases, a separate data structure should be used.
 */

#include <algorithm>
#include <cmath>
#include <iostream>

#include "BLI_utildefines.h"

/* Forward declare tbb::blocked_range for conversion operations. */
namespace tbb {
template<typename Value> class blocked_range;
}

namespace BLI {

template<typename T> class ArrayRef;

class IndexRange {
 private:
  uint m_start = 0;
  uint m_size = 0;

 public:
  IndexRange() = default;

  explicit IndexRange(uint size) : m_start(0), m_size(size)
  {
  }

  IndexRange(uint start, uint size) : m_start(start), m_size(size)
  {
  }

  template<typename T>
  IndexRange(const tbb::blocked_range<T> &range) : m_start(range.begin()), m_size(range.size())
  {
  }

  class Iterator {
   private:
    uint m_current;

   public:
    Iterator(uint current) : m_current(current)
    {
    }

    Iterator &operator++()
    {
      m_current++;
      return *this;
    }

    bool operator!=(const Iterator &iterator) const
    {
      return m_current != iterator.m_current;
    }

    uint operator*() const
    {
      return m_current;
    }
  };

  Iterator begin() const
  {
    return Iterator(m_start);
  }

  Iterator end() const
  {
    return Iterator(m_start + m_size);
  }

  /**
   * Access an element in the range.
   */
  uint operator[](uint index) const
  {
    BLI_assert(index < this->size());
    return m_start + index;
  }

  /**
   * Two ranges compare equal when they contain the same numbers.
   */
  friend bool operator==(IndexRange a, IndexRange b)
  {
    return (a.m_size == b.m_size) && (a.m_start == b.m_start || a.m_size == 0);
  }

  /**
   * Get the amount of numbers in the range.
   */
  uint size() const
  {
    return m_size;
  }

  /**
   * Create a new range starting at the end of the current one.
   */
  IndexRange after(uint n) const
  {
    return IndexRange(m_start + m_size, n);
  }

  /**
   * Create a new range that ends at the start of the current one.
   */
  IndexRange before(uint n) const
  {
    return IndexRange(m_start - n, n);
  }

  /**
   * Get the first element in the range.
   * Asserts when the range is empty.
   */
  uint first() const
  {
    BLI_assert(this->size() > 0);
    return m_start;
  }

  /**
   * Get the last element in the range.
   * Asserts when the range is empty.
   */
  uint last() const
  {
    BLI_assert(this->size() > 0);
    return m_start + m_size - 1;
  }

  /**
   * Get the element one after the end. The returned value is undefined when the range is empty.
   */
  uint one_after_last() const
  {
    return m_start + m_size;
  }

  /**
   * Get the first element in the range. The returned value is undefined when the range is empty.
   */
  uint start() const
  {
    return m_start;
  }

  /**
   * Returns true when the range contains a certain number, otherwise false.
   */
  bool contains(uint value) const
  {
    return value >= m_start && value < m_start + m_size;
  }

  IndexRange slice(uint start, uint size) const
  {
    uint new_start = m_start + start;
    BLI_assert(new_start + size <= m_start + m_size || size == 0);
    return IndexRange(new_start, size);
  }

  IndexRange slice(IndexRange range) const
  {
    return this->slice(range.start(), range.size());
  }

  /**
   * Get read-only access to a memory buffer that contains the range as actual numbers.
   */
  ArrayRef<uint> as_array_ref() const;

  friend std::ostream &operator<<(std::ostream &stream, IndexRange range)
  {
    stream << "[" << range.start() << ", " << range.one_after_last() << ")";
    return stream;
  }
};

}  // namespace BLI

#endif /* __BLI_INDEX_RANGE_H__ */
