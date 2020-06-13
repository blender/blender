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
#ifndef __BLI_ARRAY_HH__
#define __BLI_ARRAY_HH__

/** \file
 * \ingroup bli
 *
 * A `blender::Array<T>` is a container for a fixed size array the size of which is NOT known at
 * compile time.
 *
 * If the size is known at compile time, `std::array<T, N>` should be used instead.
 *
 * blender::Array should usually be used instead of blender::Vector whenever the number of elements
 * is known at construction time. Note however, that blender::Array will default construct all
 * elements when initialized with the size-constructor. For trivial types, this does nothing. In
 * all other cases, this adds overhead. If this becomes a problem, a different constructor which
 * does not do default construction can be added.
 *
 * A main benefit of using Array over Vector is that it expresses the intent of the developer
 * better. It indicates that the size of the data structure is not expected to change. Furthermore,
 * you can be more certain that an array does not overallocate.
 *
 * blender::Array supports small object optimization to improve performance when the size turns out
 * to be small at run-time.
 */

#include "BLI_allocator.hh"
#include "BLI_index_range.hh"
#include "BLI_memory_utils.hh"
#include "BLI_span.hh"
#include "BLI_utildefines.h"

namespace blender {

template<
    /**
     * The type of the values stored in the array.
     */
    typename T,
    /**
     * The number of values that can be stored in the array, without doing a heap allocation.
     *
     * When T is large, the small buffer optimization is disabled by default to avoid large
     * unexpected allocations on the stack. It can still be enabled explicitly though.
     */
    uint InlineBufferCapacity = (sizeof(T) < 100) ? 4 : 0,
    /**
     * The allocator used by this array. Should rarely be changed, except when you don't want that
     * MEM_* functions are used internally.
     */
    typename Allocator = GuardedAllocator>
class Array {
 private:
  /** The beginning of the array. It might point into the inline buffer. */
  T *m_data;

  /** Number of elements in the array. */
  uint m_size;

  /** Used for allocations when the inline buffer is too small. */
  Allocator m_allocator;

  /** A placeholder buffer that will remain uninitialized until it is used. */
  AlignedBuffer<sizeof(T) * InlineBufferCapacity, alignof(T)> m_inline_buffer;

 public:
  /**
   * By default an empty array is created.
   */
  Array()
  {
    m_data = this->inline_buffer();
    m_size = 0;
  }

  /**
   * Create a new array that contains copies of all values.
   */
  Array(Span<T> values)
  {
    m_size = values.size();
    m_data = this->get_buffer_for_size(values.size());
    uninitialized_copy_n(values.data(), m_size, m_data);
  }

  /**
   * Create a new array that contains copies of all values.
   */
  Array(const std::initializer_list<T> &values) : Array(Span<T>(values))
  {
  }

  /**
   * Create a new array with the given size. All values will be default constructed. For trivial
   * types like int, default construction does nothing.
   *
   * We might want another version of this in the future, that does not do default construction
   * even for non-trivial types. This should not be the default though, because one can easily mess
   * up when dealing with uninitialized memory.
   */
  explicit Array(uint size)
  {
    m_size = size;
    m_data = this->get_buffer_for_size(size);
    default_construct_n(m_data, size);
  }

  /**
   * Create a new array with the given size. All values will be initialized by copying the given
   * default.
   */
  Array(uint size, const T &value)
  {
    m_size = size;
    m_data = this->get_buffer_for_size(size);
    uninitialized_fill_n(m_data, m_size, value);
  }

  Array(const Array &other) : m_allocator(other.m_allocator)
  {
    m_size = other.size();

    m_data = this->get_buffer_for_size(other.size());
    uninitialized_copy_n(other.data(), m_size, m_data);
  }

  Array(Array &&other) noexcept : m_allocator(other.m_allocator)
  {
    m_size = other.m_size;

    if (!other.uses_inline_buffer()) {
      m_data = other.m_data;
    }
    else {
      m_data = this->get_buffer_for_size(m_size);
      uninitialized_relocate_n(other.m_data, m_size, m_data);
    }

    other.m_data = other.inline_buffer();
    other.m_size = 0;
  }

  ~Array()
  {
    destruct_n(m_data, m_size);
    if (!this->uses_inline_buffer()) {
      m_allocator.deallocate((void *)m_data);
    }
  }

  Array &operator=(const Array &other)
  {
    if (this == &other) {
      return *this;
    }

    this->~Array();
    new (this) Array(other);
    return *this;
  }

  Array &operator=(Array &&other)
  {
    if (this == &other) {
      return *this;
    }

    this->~Array();
    new (this) Array(std::move(other));
    return *this;
  }

  operator Span<T>() const
  {
    return Span<T>(m_data, m_size);
  }

  operator MutableSpan<T>()
  {
    return MutableSpan<T>(m_data, m_size);
  }

  Span<T> as_span() const
  {
    return *this;
  }

  MutableSpan<T> as_mutable_span()
  {
    return *this;
  }

  T &operator[](uint index)
  {
    BLI_assert(index < m_size);
    return m_data[index];
  }

  const T &operator[](uint index) const
  {
    BLI_assert(index < m_size);
    return m_data[index];
  }

  /**
   * Returns the number of elements in the array.
   */
  uint size() const
  {
    return m_size;
  }

  /**
   * Returns true when the number of elements in the array is zero.
   */
  bool is_empty() const
  {
    return m_size == 0;
  }

  /**
   * Copies the value to all indices in the array.
   */
  void fill(const T &value)
  {
    initialized_fill_n(m_data, m_size, value);
  }

  /**
   * Copies the value to the given indices in the array.
   */
  void fill_indices(Span<uint> indices, const T &value)
  {
    MutableSpan<T>(*this).fill_indices(indices, value);
  }

  /**
   * Get a pointer to the beginning of the array.
   */
  const T *data() const
  {
    return m_data;
  }
  T *data()
  {
    return m_data;
  }

  const T *begin() const
  {
    return m_data;
  }

  const T *end() const
  {
    return m_data + m_size;
  }

  T *begin()
  {
    return m_data;
  }

  T *end()
  {
    return m_data + m_size;
  }

  /**
   * Get an index range containing all valid indices for this array.
   */
  IndexRange index_range() const
  {
    return IndexRange(m_size);
  }

  /**
   * Sets the size to zero. This should only be used when you have manually destructed all elements
   * in the array beforehand. Use with care.
   */
  void clear_without_destruct()
  {
    m_size = 0;
  }

  /**
   * Access the allocator used by this array.
   */
  Allocator &allocator()
  {
    return m_allocator;
  }

  /**
   * Get the value of the InlineBufferCapacity template argument. This is the number of elements
   * that can be stored without doing an allocation.
   */
  static uint inline_buffer_capacity()
  {
    return InlineBufferCapacity;
  }

 private:
  T *get_buffer_for_size(uint size)
  {
    if (size <= InlineBufferCapacity) {
      return this->inline_buffer();
    }
    else {
      return this->allocate(size);
    }
  }

  T *inline_buffer() const
  {
    return (T *)m_inline_buffer.ptr();
  }

  T *allocate(uint size)
  {
    return (T *)m_allocator.allocate(size * sizeof(T), alignof(T), AT);
  }

  bool uses_inline_buffer() const
  {
    return m_data == this->inline_buffer();
  }
};

}  // namespace blender

#endif /* __BLI_ARRAY_HH__ */
