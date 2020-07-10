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
 * all other cases, this adds overhead.
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
  T *data_;

  /** Number of elements in the array. */
  uint size_;

  /** Used for allocations when the inline buffer is too small. */
  Allocator allocator_;

  /** A placeholder buffer that will remain uninitialized until it is used. */
  TypedBuffer<T, InlineBufferCapacity> inline_buffer_;

 public:
  /**
   * By default an empty array is created.
   */
  Array()
  {
    data_ = inline_buffer_;
    size_ = 0;
  }

  /**
   * Create a new array that contains copies of all values.
   */
  template<typename U, typename std::enable_if_t<std::is_convertible_v<U, T>> * = nullptr>
  Array(Span<U> values, Allocator allocator = {}) : allocator_(allocator)
  {
    size_ = values.size();
    data_ = this->get_buffer_for_size(values.size());
    uninitialized_convert_n<U, T>(values.data(), size_, data_);
  }

  /**
   * Create a new array that contains copies of all values.
   */
  template<typename U, typename std::enable_if_t<std::is_convertible_v<U, T>> * = nullptr>
  Array(const std::initializer_list<U> &values) : Array(Span<U>(values))
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
    size_ = size;
    data_ = this->get_buffer_for_size(size);
    default_construct_n(data_, size);
  }

  /**
   * Create a new array with the given size. All values will be initialized by copying the given
   * default.
   */
  Array(uint size, const T &value)
  {
    size_ = size;
    data_ = this->get_buffer_for_size(size);
    uninitialized_fill_n(data_, size_, value);
  }

  /**
   * Create a new array with uninitialized elements. The caller is responsible for constructing the
   * elements. Moving, copying or destructing an Array with uninitialized elements invokes
   * undefined behavior.
   *
   * This should be used very rarely. Note, that the normal size-constructor also does not
   * initialize the elements when T is trivially constructible. Therefore, it only makes sense to
   * use this with non trivially constructible types.
   *
   * Usage:
   *  Array<std::string> my_strings(10, NoInitialization());
   */
  Array(uint size, NoInitialization)
  {
    size_ = size;
    data_ = this->get_buffer_for_size(size);
  }

  Array(const Array &other) : Array(other.as_span(), other.allocator_)
  {
  }

  Array(Array &&other) noexcept : allocator_(other.allocator_)
  {
    size_ = other.size_;

    if (!other.uses_inline_buffer()) {
      data_ = other.data_;
    }
    else {
      data_ = this->get_buffer_for_size(size_);
      uninitialized_relocate_n(other.data_, size_, data_);
    }

    other.data_ = other.inline_buffer_;
    other.size_ = 0;
  }

  ~Array()
  {
    destruct_n(data_, size_);
    if (!this->uses_inline_buffer()) {
      allocator_.deallocate((void *)data_);
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

  T &operator[](uint index)
  {
    BLI_assert(index < size_);
    return data_[index];
  }

  const T &operator[](uint index) const
  {
    BLI_assert(index < size_);
    return data_[index];
  }

  operator Span<T>() const
  {
    return Span<T>(data_, size_);
  }

  operator MutableSpan<T>()
  {
    return MutableSpan<T>(data_, size_);
  }

  template<typename U, typename std::enable_if_t<is_convertible_pointer_v<T, U>> * = nullptr>
  operator Span<U>() const
  {
    return Span<U>(data_, size_);
  }

  template<typename U, typename std::enable_if_t<is_convertible_pointer_v<T, U>> * = nullptr>
  operator MutableSpan<U>()
  {
    return MutableSpan<U>(data_, size_);
  }

  Span<T> as_span() const
  {
    return *this;
  }

  MutableSpan<T> as_mutable_span()
  {
    return *this;
  }

  /**
   * Returns the number of elements in the array.
   */
  uint size() const
  {
    return size_;
  }

  /**
   * Returns true when the number of elements in the array is zero.
   */
  bool is_empty() const
  {
    return size_ == 0;
  }

  /**
   * Copies the value to all indices in the array.
   */
  void fill(const T &value)
  {
    initialized_fill_n(data_, size_, value);
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
    return data_;
  }
  T *data()
  {
    return data_;
  }

  const T *begin() const
  {
    return data_;
  }

  const T *end() const
  {
    return data_ + size_;
  }

  T *begin()
  {
    return data_;
  }

  T *end()
  {
    return data_ + size_;
  }

  /**
   * Get an index range containing all valid indices for this array.
   */
  IndexRange index_range() const
  {
    return IndexRange(size_);
  }

  /**
   * Sets the size to zero. This should only be used when you have manually destructed all elements
   * in the array beforehand. Use with care.
   */
  void clear_without_destruct()
  {
    size_ = 0;
  }

  /**
   * Access the allocator used by this array.
   */
  Allocator &allocator()
  {
    return allocator_;
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
      return inline_buffer_;
    }
    else {
      return this->allocate(size);
    }
  }

  T *allocate(uint size)
  {
    return (T *)allocator_.allocate(size * sizeof(T), alignof(T), AT);
  }

  bool uses_inline_buffer() const
  {
    return data_ == inline_buffer_;
  }
};

}  // namespace blender

#endif /* __BLI_ARRAY_HH__ */
