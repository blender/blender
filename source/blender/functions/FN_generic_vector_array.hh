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

#ifndef __FN_GENERIC_VECTOR_ARRAY_HH__
#define __FN_GENERIC_VECTOR_ARRAY_HH__

/** \file
 * \ingroup fn
 *
 * A `GVectorArray` is a container for a fixed amount of dynamically growing arrays with a generic
 * type. Its main use case is to store many small vectors with few separate allocations. Using this
 * structure is generally more efficient than allocating each small vector separately.
 *
 * `GVectorArrayRef<T>` is a typed reference to a GVectorArray and makes it easier and safer to
 * work with the class when the type is known at compile time.
 */

#include "FN_array_spans.hh"
#include "FN_cpp_type.hh"

#include "BLI_array.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_utility_mixins.hh"

namespace blender {
namespace fn {

template<typename T> class GVectorArrayRef;

class GVectorArray : NonCopyable, NonMovable {
 private:
  const CPPType &m_type;
  uint m_element_size;
  Array<void *, 1> m_starts;
  Array<uint, 1> m_lengths;
  Array<uint, 1> m_capacities;
  LinearAllocator<> m_allocator;

  template<typename T> friend class GVectorArrayRef;

 public:
  GVectorArray() = delete;

  GVectorArray(const CPPType &type, uint array_size)
      : m_type(type),
        m_element_size(type.size()),
        m_starts(array_size),
        m_lengths(array_size),
        m_capacities(array_size)
  {
    m_starts.fill(nullptr);
    m_lengths.fill(0);
    m_capacities.fill(0);
  }

  ~GVectorArray()
  {
    if (m_type.is_trivially_destructible()) {
      return;
    }

    for (uint i : m_starts.index_range()) {
      m_type.destruct_n(m_starts[i], m_lengths[i]);
    }
  }

  operator GVArraySpan() const
  {
    return GVArraySpan(m_type, m_starts.as_span(), m_lengths);
  }

  bool is_empty() const
  {
    return m_starts.size() == 0;
  }

  uint size() const
  {
    return m_starts.size();
  }

  const CPPType &type() const
  {
    return m_type;
  }

  Span<const void *> starts() const
  {
    return m_starts.as_span();
  }

  Span<uint> lengths() const
  {
    return m_lengths;
  }

  void append(uint index, const void *src)
  {
    uint old_length = m_lengths[index];
    if (old_length == m_capacities[index]) {
      this->grow_at_least_one(index);
    }

    void *dst = POINTER_OFFSET(m_starts[index], m_element_size * old_length);
    m_type.copy_to_uninitialized(src, dst);
    m_lengths[index]++;
  }

  void extend(uint index, GVSpan span)
  {
    BLI_assert(m_type == span.type());
    for (uint i = 0; i < span.size(); i++) {
      this->append(index, span[i]);
    }
  }

  void extend(IndexMask mask, GVArraySpan array_span)
  {
    BLI_assert(m_type == array_span.type());
    BLI_assert(mask.min_array_size() <= array_span.size());
    for (uint i : mask) {
      this->extend(i, array_span[i]);
    }
  }

  GMutableSpan operator[](uint index)
  {
    BLI_assert(index < m_starts.size());
    return GMutableSpan(m_type, m_starts[index], m_lengths[index]);
  }
  template<typename T> GVectorArrayRef<T> typed()
  {
    return GVectorArrayRef<T>(*this);
  }

 private:
  void grow_at_least_one(uint index)
  {
    BLI_assert(m_lengths[index] == m_capacities[index]);
    uint new_capacity = m_lengths[index] * 2 + 1;

    void *new_buffer = m_allocator.allocate(m_element_size * new_capacity, m_type.alignment());
    m_type.relocate_to_uninitialized_n(m_starts[index], new_buffer, m_lengths[index]);

    m_starts[index] = new_buffer;
    m_capacities[index] = new_capacity;
  }
};

template<typename T> class GVectorArrayRef {
 private:
  GVectorArray *m_vector_array;

 public:
  GVectorArrayRef(GVectorArray &vector_array) : m_vector_array(&vector_array)
  {
    BLI_assert(vector_array.m_type == CPPType::get<T>());
  }

  void append(uint index, const T &value)
  {
    m_vector_array->append(index, &value);
  }

  void extend(uint index, Span<T> values)
  {
    m_vector_array->extend(index, values);
  }

  void extend(uint index, VSpan<T> values)
  {
    m_vector_array->extend(index, GVSpan(values));
  }

  MutableSpan<T> operator[](uint index)
  {
    BLI_assert(index < m_vector_array->m_starts.size());
    return MutableSpan<T>((T *)m_vector_array->m_starts[index], m_vector_array->m_lengths[index]);
  }

  uint size() const
  {
    return m_vector_array->size();
  }

  bool is_empty() const
  {
    return m_vector_array->is_empty();
  }
};

}  // namespace fn
}  // namespace blender

#endif /* __FN_GENERIC_VECTOR_ARRAY_HH__ */
