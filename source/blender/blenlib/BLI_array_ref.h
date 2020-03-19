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

#ifndef __BLI_ARRAY_REF_H__
#define __BLI_ARRAY_REF_H__

/** \file
 * \ingroup bli
 *
 * These classes offer a convenient way to work with continuous chunks of memory of a certain type.
 * We differentiate #ArrayRef and #MutableArrayRef. The elements in the former are const while the
 * elements in the other are not.
 *
 * Passing array references as parameters has multiple benefits:
 *   - Less templates are used because the function does not have to work with different
 *     container types.
 *   - It encourages an Struct-of-Arrays data layout which is often beneficial when
 *     writing high performance code. Also it makes it easier to reuse code.
 *   - Array references offer convenient ways of slicing and other operations.
 *
 * The instances of #ArrayRef and #MutableArrayRef are very small and should be passed by value.
 * Since array references do not own any memory, it is generally not save to store them.
 */

#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <vector>

#include "BLI_index_range.h"
#include "BLI_memory_utils_cxx.h"
#include "BLI_utildefines.h"

namespace BLI {

/**
 * References an array of data. The elements in the array should not be changed.
 */
template<typename T> class ArrayRef {
 private:
  const T *m_start = nullptr;
  uint m_size = 0;

 public:
  /**
   * Create a reference to an empty array.
   * The pointer is allowed to be nullptr.
   */
  ArrayRef() = default;

  ArrayRef(const T *start, uint size) : m_start(start), m_size(size)
  {
  }

  ArrayRef(const std::initializer_list<T> &list) : ArrayRef(list.begin(), list.size())
  {
  }

  ArrayRef(const std::vector<T> &vector) : ArrayRef(vector.data(), vector.size())
  {
  }

  template<std::size_t N> ArrayRef(const std::array<T, N> &array) : ArrayRef(array.data(), N)
  {
  }

  /**
   * ArrayRef<T *> -> ArrayRef<const T *>
   * ArrayRef<Derived *> -> ArrayRef<Base *>
   */
  template<typename U,
           typename std::enable_if<std::is_convertible<U *, T>::value>::type * = nullptr>
  ArrayRef(ArrayRef<U *> array) : ArrayRef((T *)array.begin(), array.size())
  {
  }

  /**
   * Return a continuous part of the array.
   * Asserts that the slice stays within the array.
   */
  ArrayRef slice(uint start, uint size) const
  {
    BLI_assert(start + size <= this->size() || size == 0);
    return ArrayRef(m_start + start, size);
  }

  ArrayRef slice(IndexRange range) const
  {
    return this->slice(range.start(), range.size());
  }

  /**
   * Return a new ArrayRef with n elements removed from the beginning.
   * Asserts that the array contains enough elements.
   */
  ArrayRef drop_front(uint n = 1) const
  {
    BLI_assert(n <= this->size());
    return this->slice(n, this->size() - n);
  }

  /**
   * Return a new ArrayRef with n elements removed from the beginning.
   * Asserts that the array contains enough elements.
   */
  ArrayRef drop_back(uint n = 1) const
  {
    BLI_assert(n <= this->size());
    return this->slice(0, this->size() - n);
  }

  /**
   * Return a new ArrayRef that only contains the first n elements.
   * Asserts that the array contains enough elements.
   */
  ArrayRef take_front(uint n) const
  {
    BLI_assert(n <= this->size());
    return this->slice(0, n);
  }

  /**
   * Return a new ArrayRef that only contains the last n elements.
   * Asserts that the array contains enough elements.
   */
  ArrayRef take_back(uint n) const
  {
    BLI_assert(n <= this->size());
    return this->slice(this->size() - n, n);
  }

  /**
   * Copy the values in this array to another array.
   */
  void copy_to(T *ptr) const
  {
    BLI::copy_n(m_start, m_size, ptr);
  }

  const T *begin() const
  {
    return m_start;
  }

  const T *end() const
  {
    return m_start + m_size;
  }

  /**
   * Access an element in the array.
   * Asserts that the index is in the bounds of the array.
   */
  const T &operator[](uint index) const
  {
    BLI_assert(index < m_size);
    return m_start[index];
  }

  /**
   * Return the number of elements in the referenced array.
   */
  uint size() const
  {
    return m_size;
  }

  /**
   * Return the number of bytes referenced by this ArrayRef.
   */
  uint byte_size() const
  {
    return sizeof(T) * m_size;
  }

  /**
   * Does a linear search to see of the value is in the array.
   * Return true if it is, otherwise false.
   */
  bool contains(const T &value) const
  {
    for (const T &element : *this) {
      if (element == value) {
        return true;
      }
    }
    return false;
  }

  /**
   * Does a constant time check to see if the pointer is within the referenced array.
   * Return true if it is, otherwise false.
   */
  bool contains_ptr(const T *ptr) const
  {
    return (this->begin() <= ptr) && (ptr < this->end());
  }

  /**
   * Does a linear search to count how often the value is in the array.
   * Returns the number of occurrences.
   */
  uint count(const T &value) const
  {
    uint counter = 0;
    for (const T &element : *this) {
      if (element == value) {
        counter++;
      }
    }
    return counter;
  }

  /**
   * Return a reference to the first element in the array.
   * Asserts that the array is not empty.
   */
  const T &first() const
  {
    BLI_assert(m_size > 0);
    return m_start[0];
  }

  /**
   * Return a reference to the last element in the array.
   * Asserts that the array is not empty.
   */
  const T &last() const
  {
    BLI_assert(m_size > 0);
    return m_start[m_size - 1];
  }

  /**
   * Get element at the given index. If the index is out of range, return the fallback value.
   */
  T get(uint index, const T &fallback) const
  {
    if (index < m_size) {
      return m_start[index];
    }
    return fallback;
  }

  /**
   * Check if the array contains duplicates. Does a linear search for every element. So the total
   * running time is O(n^2). Only use this for small arrays.
   */
  bool has_duplicates__linear_search() const
  {
    /* The size should really be smaller than that. If it is not, the calling code should be
     * changed. */
    BLI_assert(m_size < 1000);

    for (uint i = 0; i < m_size; i++) {
      const T &value = m_start[i];
      for (uint j = i + 1; j < m_size; j++) {
        if (value == m_start[j]) {
          return true;
        }
      }
    }
    return false;
  }

  bool intersects__linear_search(ArrayRef other) const
  {
    /* The size should really be smaller than that. If it is not, the calling code should be
     * changed. */
    BLI_assert(m_size < 1000);

    for (uint i = 0; i < m_size; i++) {
      const T &value = m_start[i];
      if (other.contains(value)) {
        return true;
      }
    }
    return false;
  }

  uint first_index(const T &search_value) const
  {
    int index = this->first_index_try(search_value);
    BLI_assert(index >= 0);
    return (uint)index;
  }

  int first_index_try(const T &search_value) const
  {
    for (uint i = 0; i < m_size; i++) {
      if (m_start[i] == search_value) {
        return i;
      }
    }
    return -1;
  }

  template<typename PredicateT> bool any(const PredicateT predicate)
  {
    for (uint i = 0; i < m_size; i++) {
      if (predicate(m_start[i])) {
        return true;
      }
    }
    return false;
  }

  /**
   * Utility to make it more convenient to iterate over all indices that can be used with this
   * array.
   */
  IndexRange index_range() const
  {
    return IndexRange(m_size);
  }

  /**
   * Get a new array ref to the same underlying memory buffer. No conversions are done.
   */
  template<typename NewT> ArrayRef<NewT> cast() const
  {
    BLI_assert((m_size * sizeof(T)) % sizeof(NewT) == 0);
    uint new_size = m_size * sizeof(T) / sizeof(NewT);
    return ArrayRef<NewT>(reinterpret_cast<const NewT *>(m_start), new_size);
  }

  /**
   * A debug utility to print the content of the array ref. Every element will be printed on a
   * separate line using the given callback.
   */
  template<typename PrintLineF> void print_as_lines(std::string name, PrintLineF print_line) const
  {
    std::cout << "ArrayRef: " << name << " \tSize:" << m_size << '\n';
    for (const T &value : *this) {
      std::cout << "  ";
      print_line(value);
      std::cout << '\n';
    }
  }

  void print_as_lines(std::string name) const
  {
    this->print_as_lines(name, [](const T &value) { std::cout << value; });
  }
};

/**
 * Mostly the same as ArrayRef, except that one can change the array elements via this reference.
 */
template<typename T> class MutableArrayRef {
 private:
  T *m_start;
  uint m_size;

 public:
  MutableArrayRef() = default;

  MutableArrayRef(T *start, uint size) : m_start(start), m_size(size)
  {
  }

  MutableArrayRef(std::initializer_list<T> &list) : MutableArrayRef(list.begin(), list.size())
  {
  }

  MutableArrayRef(std::vector<T> &vector) : MutableArrayRef(vector.data(), vector.size())
  {
  }

  template<std::size_t N>
  MutableArrayRef(std::array<T, N> &array) : MutableArrayRef(array.data(), N)
  {
  }

  operator ArrayRef<T>() const
  {
    return ArrayRef<T>(m_start, m_size);
  }

  /**
   * Get the number of elements in the array.
   */
  uint size() const
  {
    return m_size;
  }

  /**
   * Replace all elements in the referenced array with the given value.
   */
  void fill(const T &element)
  {
    std::fill_n(m_start, m_size, element);
  }

  /**
   * Replace a subset of all elements with the given value.
   */
  void fill_indices(ArrayRef<uint> indices, const T &element)
  {
    for (uint i : indices) {
      m_start[i] = element;
    }
  }

  /**
   * Copy the values from another array into the references array.
   */
  void copy_from(const T *ptr)
  {
    BLI::copy_n(ptr, m_size, m_start);
  }

  void copy_from(ArrayRef<T> other)
  {
    BLI_assert(this->size() == other.size());
    this->copy_from(other.begin());
  }

  T *begin() const
  {
    return m_start;
  }

  T *end() const
  {
    return m_start + m_size;
  }

  T &operator[](uint index) const
  {
    BLI_assert(index < this->size());
    return m_start[index];
  }

  /**
   * Return a continuous part of the array.
   * Asserts that the slice stays in the array bounds.
   */
  MutableArrayRef slice(uint start, uint length) const
  {
    BLI_assert(start + length <= this->size());
    return MutableArrayRef(m_start + start, length);
  }

  /**
   * Return a new MutableArrayRef with n elements removed from the beginning.
   */
  MutableArrayRef drop_front(uint n = 1) const
  {
    BLI_assert(n <= this->size());
    return this->slice(n, this->size() - n);
  }

  /**
   * Return a new MutableArrayRef with n elements removed from the beginning.
   */
  MutableArrayRef drop_back(uint n = 1) const
  {
    BLI_assert(n <= this->size());
    return this->slice(0, this->size() - n);
  }

  /**
   * Return a new MutableArrayRef that only contains the first n elements.
   */
  MutableArrayRef take_front(uint n) const
  {
    BLI_assert(n <= this->size());
    return this->slice(0, n);
  }

  /**
   * Return a new MutableArrayRef that only contains the last n elements.
   */
  MutableArrayRef take_back(uint n) const
  {
    BLI_assert(n <= this->size());
    return this->slice(this->size() - n, n);
  }

  ArrayRef<T> as_ref() const
  {
    return ArrayRef<T>(m_start, m_size);
  }

  IndexRange index_range() const
  {
    return IndexRange(m_size);
  }

  const T &last() const
  {
    BLI_assert(m_size > 0);
    return m_start[m_size - 1];
  }
};

/**
 * Shorthand to make use of automatic template parameter deduction.
 */
template<typename T> ArrayRef<T> ref_c_array(const T *array, uint size)
{
  return ArrayRef<T>(array, size);
}

template<typename T1, typename T2> void assert_same_size(const T1 &v1, const T2 &v2)
{
  UNUSED_VARS_NDEBUG(v1, v2);
#ifdef DEBUG
  uint size = v1.size();
  BLI_assert(size == v1.size());
  BLI_assert(size == v2.size());
#endif
}

template<typename T1, typename T2, typename T3>
void assert_same_size(const T1 &v1, const T2 &v2, const T3 &v3)
{
  UNUSED_VARS_NDEBUG(v1, v2, v3);
#ifdef DEBUG
  uint size = v1.size();
  BLI_assert(size == v1.size());
  BLI_assert(size == v2.size());
  BLI_assert(size == v3.size());
#endif
}

} /* namespace BLI */

#endif /* __BLI_ARRAY_REF_H__ */
