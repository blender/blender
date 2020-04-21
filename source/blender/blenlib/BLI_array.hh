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
 * This is a container that contains a fixed size array. Note however, the size of the array is not
 * a template argument. Instead it can be specified at the construction time.
 */

#include "BLI_allocator.hh"
#include "BLI_array_ref.hh"
#include "BLI_index_range.hh"
#include "BLI_memory_utils.hh"
#include "BLI_utildefines.h"

namespace BLI {

template<typename T, uint N = 4, typename Allocator = GuardedAllocator> class Array {
 private:
  T *m_data;
  uint m_size;
  Allocator m_allocator;
  AlignedBuffer<sizeof(T) * N, alignof(T)> m_inline_storage;

 public:
  Array()
  {
    m_data = this->inline_storage();
    m_size = 0;
  }

  Array(ArrayRef<T> values)
  {
    m_size = values.size();
    m_data = this->get_buffer_for_size(values.size());
    uninitialized_copy_n(values.begin(), m_size, m_data);
  }

  Array(const std::initializer_list<T> &values) : Array(ArrayRef<T>(values))
  {
  }

  explicit Array(uint size)
  {
    m_size = size;
    m_data = this->get_buffer_for_size(size);

    for (uint i = 0; i < m_size; i++) {
      new (m_data + i) T();
    }
  }

  Array(uint size, const T &value)
  {
    m_size = size;
    m_data = this->get_buffer_for_size(size);
    uninitialized_fill_n(m_data, m_size, value);
  }

  Array(const Array &other)
  {
    m_size = other.size();
    m_allocator = other.m_allocator;

    m_data = this->get_buffer_for_size(other.size());
    copy_n(other.begin(), m_size, m_data);
  }

  Array(Array &&other) noexcept
  {
    m_size = other.m_size;
    m_allocator = other.m_allocator;

    if (!other.uses_inline_storage()) {
      m_data = other.m_data;
    }
    else {
      m_data = this->get_buffer_for_size(m_size);
      uninitialized_relocate_n(other.m_data, m_size, m_data);
    }

    other.m_data = other.inline_storage();
    other.m_size = 0;
  }

  ~Array()
  {
    destruct_n(m_data, m_size);
    if (!this->uses_inline_storage()) {
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

  operator ArrayRef<T>() const
  {
    return ArrayRef<T>(m_data, m_size);
  }

  operator MutableArrayRef<T>()
  {
    return MutableArrayRef<T>(m_data, m_size);
  }

  ArrayRef<T> as_ref() const
  {
    return *this;
  }

  MutableArrayRef<T> as_mutable_ref()
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

  uint size() const
  {
    return m_size;
  }

  void fill(const T &value)
  {
    MutableArrayRef<T>(*this).fill(value);
  }

  void fill_indices(ArrayRef<uint> indices, const T &value)
  {
    MutableArrayRef<T>(*this).fill_indices(indices, value);
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

  IndexRange index_range() const
  {
    return IndexRange(m_size);
  }

 private:
  T *get_buffer_for_size(uint size)
  {
    if (size <= N) {
      return this->inline_storage();
    }
    else {
      return this->allocate(size);
    }
  }

  T *inline_storage() const
  {
    return (T *)m_inline_storage.ptr();
  }

  T *allocate(uint size)
  {
    return (T *)m_allocator.allocate_aligned(
        size * sizeof(T), std::alignment_of<T>::value, __func__);
  }

  bool uses_inline_storage() const
  {
    return m_data == this->inline_storage();
  }
};

}  // namespace BLI

#endif /* __BLI_ARRAY_HH__ */
