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
 * An `blender::Span<T>` references an array that is owned by someone else. It is just a
 * pointer and a size. Since the memory is not owned, Span should not be used to transfer
 * ownership. The array cannot be modified through the Span. However, if T is a non-const
 * pointer, the pointed-to elements can be modified.
 *
 * There is also `blender::MutableSpan<T>`. It is mostly the same as Span, but allows the
 * array to be modified.
 *
 * A (Mutable)Span can refer to data owned by many different data structures including
 * blender::Vector, blender::Array, blender::VectorSet, std::vector, std::array, std::string,
 * std::initializer_list and c-style array.
 *
 * `blender::Span` is very similar to `std::span` (C++20). However, there are a few differences:
 * - `blender::Span` is const by default. This is to avoid making things mutable when they don't
 *   have to be. To get a non-const span, you need to use `blender::MutableSpan`. Below is a list
 *   of const-behavior-equivalent pairs of data structures:
 *   - std::span<int>                <==>  blender::MutableSpan<int>
 *   - std::span<const int>          <==>  blender::Span<int>
 *   - std::span<int *>              <==>  blender::MutableSpan<int *>
 *   - std::span<const int *>        <==>  blender::MutableSpan<const int *>
 *   - std::span<int * const>        <==>  blender::Span<int *>
 *   - std::span<const int * const>  <==>  blender::Span<const int *>
 * - `blender::Span` always has a dynamic extent, while `std::span` can have a size that is
 *   determined at compile time. I did not have a use case for that yet. If we need it, we can
 *   decide to add this functionality to `blender::Span` or introduce a new type like
 *   `blender::FixedSpan<T, N>`.
 *
 * `blender::Span<T>` should be your default choice when you have to pass a read-only array
 * into a function. It is better than passing a `const Vector &`, because then the function only
 * works for vectors and not for e.g. arrays. Using Span as function parameter makes it usable
 * in more contexts, better expresses the intent and does not sacrifice performance. It is also
 * better than passing a raw pointer and size separately, because it is more convenient and safe.
 *
 * `blender::MutableSpan<T>` can be used when a function is supposed to return an array, the
 * size of which is known before the function is called. One advantage of this approach is that the
 * caller is responsible for allocation and deallocation. Furthermore, the function can focus on
 * its task, without having to worry about memory allocation. Alternatively, a function could
 * return an Array or Vector.
 *
 * NOTE: When a function has a MutableSpan<T> output parameter and T is not a trivial type,
 * then the function has to specify whether the referenced array is expected to be initialized or
 * not.
 *
 * Since the arrays are only referenced, it is generally unsafe to store an Span. When you
 * store one, you should know who owns the memory.
 *
 * Instances of Span and MutableSpan are small and should be passed by value.
 */

#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <vector>

#include "BLI_index_range.hh"
#include "BLI_memory_utils.hh"
#include "BLI_utildefines.h"

namespace blender {

/**
 * References an array of type T that is owned by someone else. The data in the array cannot be
 * modified.
 */
template<typename T> class Span {
 public:
  using value_type = T;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using iterator = const T *;
  using size_type = int64_t;

 protected:
  const T *data_ = nullptr;
  int64_t size_ = 0;

 public:
  /**
   * Create a reference to an empty array.
   */
  constexpr Span() = default;

  constexpr Span(const T *start, int64_t size) : data_(start), size_(size)
  {
    BLI_assert(size >= 0);
  }

  template<typename U, typename std::enable_if_t<is_span_convertible_pointer_v<U, T>> * = nullptr>
  constexpr Span(const U *start, int64_t size) : data_(static_cast<const T *>(start)), size_(size)
  {
    BLI_assert(size >= 0);
  }

  /**
   * Reference an initializer_list. Note that the data in the initializer_list is only valid until
   * the expression containing it is fully computed.
   *
   * Do:
   *  call_function_with_array({1, 2, 3, 4});
   *
   * Don't:
   *  Span<int> span = {1, 2, 3, 4};
   *  call_function_with_array(span);
   */
  constexpr Span(const std::initializer_list<T> &list)
      : Span(list.begin(), static_cast<int64_t>(list.size()))
  {
  }

  constexpr Span(const std::vector<T> &vector)
      : Span(vector.data(), static_cast<int64_t>(vector.size()))
  {
  }

  template<std::size_t N> constexpr Span(const std::array<T, N> &array) : Span(array.data(), N)
  {
  }

  /**
   * Support implicit conversions like the one below:
   *   Span<T *> -> Span<const T *>
   */
  template<typename U, typename std::enable_if_t<is_span_convertible_pointer_v<U, T>> * = nullptr>
  constexpr Span(Span<U> span) : data_(static_cast<const T *>(span.data())), size_(span.size())
  {
  }

  /**
   * Returns a contiguous part of the array. This invokes undefined behavior when the start or size
   * is negative.
   */
  constexpr Span slice(int64_t start, int64_t size) const
  {
    BLI_assert(start >= 0);
    BLI_assert(size >= 0);
    const int64_t new_size = std::max<int64_t>(0, std::min(size, size_ - start));
    return Span(data_ + start, new_size);
  }

  constexpr Span slice(IndexRange range) const
  {
    return this->slice(range.start(), range.size());
  }

  /**
   * Returns a new Span with n elements removed from the beginning. This invokes undefined
   * behavior when n is negative.
   */
  constexpr Span drop_front(int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::max<int64_t>(0, size_ - n);
    return Span(data_ + n, new_size);
  }

  /**
   * Returns a new Span with n elements removed from the beginning. This invokes undefined
   * behavior when n is negative.
   */
  constexpr Span drop_back(int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::max<int64_t>(0, size_ - n);
    return Span(data_, new_size);
  }

  /**
   * Returns a new Span that only contains the first n elements. This invokes undefined
   * behavior when n is negative.
   */
  constexpr Span take_front(int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::min<int64_t>(size_, n);
    return Span(data_, new_size);
  }

  /**
   * Returns a new Span that only contains the last n elements. This invokes undefined
   * behavior when n is negative.
   */
  constexpr Span take_back(int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::min<int64_t>(size_, n);
    return Span(data_ + size_ - new_size, new_size);
  }

  /**
   * Returns the pointer to the beginning of the referenced array. This may be nullptr when the
   * size is zero.
   */
  constexpr const T *data() const
  {
    return data_;
  }

  constexpr const T *begin() const
  {
    return data_;
  }
  constexpr const T *end() const
  {
    return data_ + size_;
  }

  constexpr std::reverse_iterator<const T *> rbegin() const
  {
    return std::reverse_iterator<const T *>(this->end());
  }
  constexpr std::reverse_iterator<const T *> rend() const
  {
    return std::reverse_iterator<const T *>(this->begin());
  }

  /**
   * Access an element in the array. This invokes undefined behavior when the index is out of
   * bounds.
   */
  constexpr const T &operator[](int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    return data_[index];
  }

  /**
   * Returns the number of elements in the referenced array.
   */
  constexpr int64_t size() const
  {
    return size_;
  }

  /**
   * Returns true if the size is zero.
   */
  constexpr bool is_empty() const
  {
    return size_ == 0;
  }

  /**
   * Returns the number of bytes referenced by this Span.
   */
  constexpr int64_t size_in_bytes() const
  {
    return sizeof(T) * size_;
  }

  /**
   * Does a linear search to see of the value is in the array.
   * Returns true if it is, otherwise false.
   */
  constexpr bool contains(const T &value) const
  {
    for (const T &element : *this) {
      if (element == value) {
        return true;
      }
    }
    return false;
  }

  /**
   * Does a constant time check to see if the pointer points to a value in the referenced array.
   * Return true if it is, otherwise false.
   */
  constexpr bool contains_ptr(const T *ptr) const
  {
    return (this->begin() <= ptr) && (ptr < this->end());
  }

  /**
   * Does a linear search to count how often the value is in the array.
   * Returns the number of occurrences.
   */
  constexpr int64_t count(const T &value) const
  {
    int64_t counter = 0;
    for (const T &element : *this) {
      if (element == value) {
        counter++;
      }
    }
    return counter;
  }

  /**
   * Return a reference to the first element in the array. This invokes undefined behavior when the
   * array is empty.
   */
  constexpr const T &first() const
  {
    BLI_assert(size_ > 0);
    return data_[0];
  }

  /**
   * Returns a reference to the last element in the array. This invokes undefined behavior when the
   * array is empty.
   */
  constexpr const T &last() const
  {
    BLI_assert(size_ > 0);
    return data_[size_ - 1];
  }

  /**
   * Returns the element at the given index. If the index is out of range, return the fallback
   * value.
   */
  constexpr T get(int64_t index, const T &fallback) const
  {
    if (index < size_ && index >= 0) {
      return data_[index];
    }
    return fallback;
  }

  /**
   * Check if the array contains duplicates. Does a linear search for every element. So the total
   * running time is O(n^2). Only use this for small arrays.
   */
  constexpr bool has_duplicates__linear_search() const
  {
    /* The size should really be smaller than that. If it is not, the calling code should be
     * changed. */
    BLI_assert(size_ < 1000);

    for (int64_t i = 0; i < size_; i++) {
      const T &value = data_[i];
      for (int64_t j = i + 1; j < size_; j++) {
        if (value == data_[j]) {
          return true;
        }
      }
    }
    return false;
  }

  /**
   * Returns true when this and the other array have an element in common. This should only be
   * called on small arrays, because it has a running time of O(n*m) where n and m are the sizes of
   * the arrays.
   */
  constexpr bool intersects__linear_search(Span other) const
  {
    /* The size should really be smaller than that. If it is not, the calling code should be
     * changed. */
    BLI_assert(size_ < 1000);

    for (int64_t i = 0; i < size_; i++) {
      const T &value = data_[i];
      if (other.contains(value)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Returns the index of the first occurrence of the given value. This invokes undefined behavior
   * when the value is not in the array.
   */
  constexpr int64_t first_index(const T &search_value) const
  {
    const int64_t index = this->first_index_try(search_value);
    BLI_assert(index >= 0);
    return index;
  }

  /**
   * Returns the index of the first occurrence of the given value or -1 if it does not exist.
   */
  constexpr int64_t first_index_try(const T &search_value) const
  {
    for (int64_t i = 0; i < size_; i++) {
      if (data_[i] == search_value) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Utility to make it more convenient to iterate over all indices that can be used with this
   * array.
   */
  constexpr IndexRange index_range() const
  {
    return IndexRange(size_);
  }

  /**
   * Returns a new Span to the same underlying memory buffer. No conversions are done.
   */
  template<typename NewT> Span<NewT> constexpr cast() const
  {
    BLI_assert((size_ * sizeof(T)) % sizeof(NewT) == 0);
    int64_t new_size = size_ * sizeof(T) / sizeof(NewT);
    return Span<NewT>(reinterpret_cast<const NewT *>(data_), new_size);
  }

  friend bool operator==(const Span<T> a, const Span<T> b)
  {
    if (a.size() != b.size()) {
      return false;
    }
    return std::equal(a.begin(), a.end(), b.begin());
  }

  friend bool operator!=(const Span<T> a, const Span<T> b)
  {
    return !(a == b);
  }

  /**
   * A debug utility to print the content of the Span. Every element will be printed on a
   * separate line using the given callback.
   */
  template<typename PrintLineF> void print_as_lines(std::string name, PrintLineF print_line) const
  {
    std::cout << "Span: " << name << " \tSize:" << size_ << '\n';
    for (const T &value : *this) {
      std::cout << "  ";
      print_line(value);
      std::cout << '\n';
    }
  }

  /**
   * A debug utility to print the content of the span. Every element be printed on a separate
   * line.
   */
  void print_as_lines(std::string name) const
  {
    this->print_as_lines(name, [](const T &value) { std::cout << value; });
  }
};

/**
 * Mostly the same as Span, except that one can change the array elements through a
 * MutableSpan.
 */
template<typename T> class MutableSpan {
 public:
  using value_type = T;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using iterator = T *;
  using size_type = int64_t;

 protected:
  T *data_ = nullptr;
  int64_t size_ = 0;

 public:
  constexpr MutableSpan() = default;

  constexpr MutableSpan(T *start, const int64_t size) : data_(start), size_(size)
  {
  }

  constexpr MutableSpan(std::vector<T> &vector) : MutableSpan(vector.data(), vector.size())
  {
  }

  template<std::size_t N>
  constexpr MutableSpan(std::array<T, N> &array) : MutableSpan(array.data(), N)
  {
  }

  /**
   * Support implicit conversions like the one below:
   *   MutableSpan<T *> -> MutableSpan<const T *>
   */
  template<typename U, typename std::enable_if_t<is_span_convertible_pointer_v<U, T>> * = nullptr>
  constexpr MutableSpan(MutableSpan<U> span)
      : data_(static_cast<T *>(span.data())), size_(span.size())
  {
  }

  constexpr operator Span<T>() const
  {
    return Span<T>(data_, size_);
  }

  template<typename U, typename std::enable_if_t<is_span_convertible_pointer_v<T, U>> * = nullptr>
  constexpr operator Span<U>() const
  {
    return Span<U>(static_cast<const U *>(data_), size_);
  }

  /**
   * Returns the number of elements in the array.
   */
  constexpr int64_t size() const
  {
    return size_;
  }

  /**
   * Returns true if the size is zero.
   */
  constexpr bool is_empty() const
  {
    return size_ == 0;
  }

  /**
   * Replace all elements in the referenced array with the given value.
   */
  constexpr void fill(const T &value)
  {
    initialized_fill_n(data_, size_, value);
  }

  /**
   * Replace a subset of all elements with the given value. This invokes undefined behavior when
   * one of the indices is out of bounds.
   */
  constexpr void fill_indices(Span<int64_t> indices, const T &value)
  {
    for (int64_t i : indices) {
      BLI_assert(i < size_);
      data_[i] = value;
    }
  }

  /**
   * Returns a pointer to the beginning of the referenced array. This may be nullptr, when the size
   * is zero.
   */
  constexpr T *data() const
  {
    return data_;
  }

  constexpr T *begin() const
  {
    return data_;
  }
  constexpr T *end() const
  {
    return data_ + size_;
  }

  constexpr std::reverse_iterator<T *> rbegin() const
  {
    return std::reverse_iterator<T *>(this->end());
  }
  constexpr std::reverse_iterator<T *> rend() const
  {
    return std::reverse_iterator<T *>(this->begin());
  }

  constexpr T &operator[](const int64_t index) const
  {
    BLI_assert(index < this->size());
    return data_[index];
  }

  /**
   * Returns a contiguous part of the array. This invokes undefined behavior when the start or size
   * is negative.
   */
  constexpr MutableSpan slice(const int64_t start, const int64_t size) const
  {
    BLI_assert(start >= 0);
    BLI_assert(size >= 0);
    const int64_t new_size = std::max<int64_t>(0, std::min(size, size_ - start));
    return MutableSpan(data_ + start, new_size);
  }

  /**
   * Returns a new MutableSpan with n elements removed from the beginning. This invokes
   * undefined behavior when n is negative.
   */
  constexpr MutableSpan drop_front(const int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::max<int64_t>(0, size_ - n);
    return MutableSpan(data_ + n, new_size);
  }

  /**
   * Returns a new MutableSpan with n elements removed from the end. This invokes undefined
   * behavior when n is negative.
   */
  constexpr MutableSpan drop_back(const int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::max<int64_t>(0, size_ - n);
    return MutableSpan(data_, new_size);
  }

  /**
   * Returns a new MutableSpan that only contains the first n elements. This invokes undefined
   * behavior when n is negative.
   */
  constexpr MutableSpan take_front(const int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::min<int64_t>(size_, n);
    return MutableSpan(data_, new_size);
  }

  /**
   * Return a new MutableSpan that only contains the last n elements. This invokes undefined
   * behavior when n is negative.
   */
  constexpr MutableSpan take_back(const int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::min<int64_t>(size_, n);
    return MutableSpan(data_ + size_ - new_size, new_size);
  }

  /**
   * Reverse the data in the MutableSpan.
   */
  constexpr void reverse()
  {
    for (const int i : IndexRange(size_ / 2)) {
      std::swap(data_[size_ - 1 - i], data_[i]);
    }
  }

  /**
   * Returns an (immutable) Span that references the same array. This is usually not needed,
   * due to implicit conversions. However, sometimes automatic type deduction needs some help.
   */
  constexpr Span<T> as_span() const
  {
    return Span<T>(data_, size_);
  }

  /**
   * Utility to make it more convenient to iterate over all indices that can be used with this
   * array.
   */
  constexpr IndexRange index_range() const
  {
    return IndexRange(size_);
  }

  /**
   * Return a reference to the first element in the array. This invokes undefined behavior when the
   * array is empty.
   */
  constexpr T &first() const
  {
    BLI_assert(size_ > 0);
    return data_[0];
  }

  /**
   * Returns a reference to the last element. This invokes undefined behavior when the array is
   * empty.
   */
  constexpr T &last() const
  {
    BLI_assert(size_ > 0);
    return data_[size_ - 1];
  }

  /**
   * Does a linear search to count how often the value is in the array.
   * Returns the number of occurrences.
   */
  constexpr int64_t count(const T &value) const
  {
    int64_t counter = 0;
    for (const T &element : *this) {
      if (element == value) {
        counter++;
      }
    }
    return counter;
  }

  /**
   * Copy all values from another span into this span. This invokes undefined behavior when the
   * destination contains uninitialized data and T is not trivially copy constructible.
   * The size of both spans is expected to be the same.
   */
  constexpr void copy_from(Span<T> values)
  {
    BLI_assert(size_ == values.size());
    initialized_copy_n(values.data(), size_, data_);
  }

  /**
   * Returns a new span to the same underlying memory buffer. No conversions are done.
   * The caller is responsible for making sure that the type cast is valid.
   */
  template<typename NewT> constexpr MutableSpan<NewT> cast() const
  {
    BLI_assert((size_ * sizeof(T)) % sizeof(NewT) == 0);
    int64_t new_size = size_ * sizeof(T) / sizeof(NewT);
    return MutableSpan<NewT>((NewT *)data_, new_size);
  }
};

} /* namespace blender */
