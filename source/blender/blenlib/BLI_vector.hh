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
 * A `blender::Vector<T>` is a dynamically growing contiguous array for values of type T. It is
 * designed to be a more convenient and efficient replacement for `std::vector`. Note that the term
 * "vector" has nothing to do with a vector from computer graphics here.
 *
 * A vector supports efficient insertion and removal at the end (O(1) amortized). Removal in other
 * places takes O(n) time, because all elements afterwards have to be moved. If the order of
 * elements is not important, `remove_and_reorder` can be used instead of `remove` for better
 * performance.
 *
 * The improved efficiency is mainly achieved by supporting small buffer optimization. As long as
 * the number of elements in the vector does not become larger than InlineBufferCapacity, no memory
 * allocation is done. As a consequence, iterators are invalidated when a blender::Vector is moved
 * (iterators of std::vector remain valid when the vector is moved).
 *
 * `blender::Vector` should be your default choice for a vector data structure in Blender.
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>

#include "BLI_allocator.hh"
#include "BLI_index_range.hh"
#include "BLI_listbase_wrapper.hh"
#include "BLI_math_base.h"
#include "BLI_memory_utils.hh"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

namespace blender {

template<
    /**
     * Type of the values stored in this vector. It has to be movable.
     */
    typename T,
    /**
     * The number of values that can be stored in this vector, without doing a heap allocation.
     * Sometimes it makes sense to increase this value a lot. The memory in the inline buffer is
     * not initialized when it is not needed.
     *
     * When T is large, the small buffer optimization is disabled by default to avoid large
     * unexpected allocations on the stack. It can still be enabled explicitly though.
     */
    int64_t InlineBufferCapacity = default_inline_buffer_capacity(sizeof(T)),
    /**
     * The allocator used by this vector. Should rarely be changed, except when you don't want that
     * MEM_* is used internally.
     */
    typename Allocator = GuardedAllocator>
class Vector {
 public:
  using value_type = T;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using iterator = T *;
  using const_iterator = const T *;
  using size_type = int64_t;

 private:
  /**
   * Use pointers instead of storing the size explicitly. This reduces the number of instructions
   * in `append`.
   *
   * The pointers might point to the memory in the inline buffer.
   */
  T *begin_;
  T *end_;
  T *capacity_end_;

  /** Used for allocations when the inline buffer is too small. */
  Allocator allocator_;

  /** A placeholder buffer that will remain uninitialized until it is used. */
  TypedBuffer<T, InlineBufferCapacity> inline_buffer_;

  /**
   * Store the size of the vector explicitly in debug builds. Otherwise you'd always have to call
   * the `size` function or do the math to compute it from the pointers manually. This is rather
   * annoying. Knowing the size of a vector is often quite essential when debugging some code.
   */
#ifndef NDEBUG
  int64_t debug_size_;
#  define UPDATE_VECTOR_SIZE(ptr) \
    (ptr)->debug_size_ = static_cast<int64_t>((ptr)->end_ - (ptr)->begin_)
#else
#  define UPDATE_VECTOR_SIZE(ptr) ((void)0)
#endif

  /**
   * Be a friend with other vector instantiations. This is necessary to implement some memory
   * management logic.
   */
  template<typename OtherT, int64_t OtherInlineBufferCapacity, typename OtherAllocator>
  friend class Vector;

 public:
  /**
   * Create an empty vector.
   * This does not do any memory allocation.
   */
  Vector(Allocator allocator = {}) noexcept : allocator_(allocator)
  {
    begin_ = inline_buffer_;
    end_ = begin_;
    capacity_end_ = begin_ + InlineBufferCapacity;
    UPDATE_VECTOR_SIZE(this);
  }

  Vector(NoExceptConstructor, Allocator allocator = {}) noexcept : Vector(allocator)
  {
  }

  /**
   * Create a vector with a specific size.
   * The elements will be default constructed.
   * If T is trivially constructible, the elements in the vector are not touched.
   */
  explicit Vector(int64_t size, Allocator allocator = {})
      : Vector(NoExceptConstructor(), allocator)
  {
    this->resize(size);
  }

  /**
   * Create a vector filled with a specific value.
   */
  Vector(int64_t size, const T &value, Allocator allocator = {})
      : Vector(NoExceptConstructor(), allocator)
  {
    this->resize(size, value);
  }

  /**
   * Create a vector from a span. The values in the vector are copy constructed.
   */
  template<typename U, typename std::enable_if_t<std::is_convertible_v<U, T>> * = nullptr>
  Vector(Span<U> values, Allocator allocator = {}) : Vector(NoExceptConstructor(), allocator)
  {
    const int64_t size = values.size();
    this->reserve(size);
    uninitialized_convert_n<U, T>(values.data(), size, begin_);
    this->increase_size_by_unchecked(size);
  }

  /**
   * Create a vector that contains copies of the values in the initialized list.
   *
   * This allows you to write code like:
   * Vector<int> vec = {3, 4, 5};
   */
  template<typename U, typename std::enable_if_t<std::is_convertible_v<U, T>> * = nullptr>
  Vector(const std::initializer_list<U> &values) : Vector(Span<U>(values))
  {
  }

  Vector(const std::initializer_list<T> &values) : Vector(Span<T>(values))
  {
  }

  template<typename U,
           size_t N,
           typename std::enable_if_t<std::is_convertible_v<U, T>> * = nullptr>
  Vector(const std::array<U, N> &values) : Vector(Span(values))
  {
  }

  template<typename InputIt,
           /* This constructor should not be called with e.g. Vector(3, 10), because that is
            * expected to produce the vector (10, 10, 10). */
           typename std::enable_if_t<!std::is_convertible_v<InputIt, int>> * = nullptr>
  Vector(InputIt first, InputIt last, Allocator allocator = {})
      : Vector(NoExceptConstructor(), allocator)
  {
    for (InputIt current = first; current != last; ++current) {
      this->append(*current);
    }
  }

  /**
   * Create a vector from a ListBase. The caller has to make sure that the values in the linked
   * list have the correct type.
   *
   * Example Usage:
   *  Vector<ModifierData *> modifiers(ob->modifiers);
   */
  Vector(const ListBase &values, Allocator allocator = {})
      : Vector(NoExceptConstructor(), allocator)
  {
    LISTBASE_FOREACH (T, value, &values) {
      this->append(value);
    }
  }

  /**
   * Create a copy of another vector. The other vector will not be changed. If the other vector has
   * less than InlineBufferCapacity elements, no allocation will be made.
   */
  Vector(const Vector &other) : Vector(other.as_span(), other.allocator_)
  {
  }

  /**
   * Create a copy of a vector with a different InlineBufferCapacity. This needs to be handled
   * separately, so that the other one is a valid copy constructor.
   */
  template<int64_t OtherInlineBufferCapacity>
  Vector(const Vector<T, OtherInlineBufferCapacity, Allocator> &other)
      : Vector(other.as_span(), other.allocator_)
  {
  }

  /**
   * Steal the elements from another vector. This does not do an allocation. The other vector will
   * have zero elements afterwards.
   */
  template<int64_t OtherInlineBufferCapacity>
  Vector(Vector<T, OtherInlineBufferCapacity, Allocator> &&other) noexcept(
      std::is_nothrow_move_constructible_v<T>)
      : Vector(NoExceptConstructor(), other.allocator_)
  {
    const int64_t size = other.size();

    if (other.is_inline()) {
      if (size <= InlineBufferCapacity) {
        /* Copy between inline buffers. */
        uninitialized_relocate_n(other.begin_, size, begin_);
        end_ = begin_ + size;
      }
      else {
        /* Copy from inline buffer to newly allocated buffer. */
        const int64_t capacity = size;
        begin_ = static_cast<T *>(
            allocator_.allocate(sizeof(T) * static_cast<size_t>(capacity), alignof(T), AT));
        capacity_end_ = begin_ + capacity;
        uninitialized_relocate_n(other.begin_, size, begin_);
        end_ = begin_ + size;
      }
    }
    else {
      /* Steal the pointer. */
      begin_ = other.begin_;
      end_ = other.end_;
      capacity_end_ = other.capacity_end_;
    }

    other.begin_ = other.inline_buffer_;
    other.end_ = other.begin_;
    other.capacity_end_ = other.begin_ + OtherInlineBufferCapacity;
    UPDATE_VECTOR_SIZE(this);
    UPDATE_VECTOR_SIZE(&other);
  }

  ~Vector()
  {
    destruct_n(begin_, this->size());
    if (!this->is_inline()) {
      allocator_.deallocate(begin_);
    }
  }

  Vector &operator=(const Vector &other)
  {
    return copy_assign_container(*this, other);
  }

  Vector &operator=(Vector &&other)
  {
    return move_assign_container(*this, std::move(other));
  }

  /**
   * Get the value at the given index. This invokes undefined behavior when the index is out of
   * bounds.
   */
  const T &operator[](int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->size());
    return begin_[index];
  }

  T &operator[](int64_t index)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->size());
    return begin_[index];
  }

  operator Span<T>() const
  {
    return Span<T>(begin_, this->size());
  }

  operator MutableSpan<T>()
  {
    return MutableSpan<T>(begin_, this->size());
  }

  template<typename U, typename std::enable_if_t<is_span_convertible_pointer_v<T, U>> * = nullptr>
  operator Span<U>() const
  {
    return Span<U>(begin_, this->size());
  }

  template<typename U, typename std::enable_if_t<is_span_convertible_pointer_v<T, U>> * = nullptr>
  operator MutableSpan<U>()
  {
    return MutableSpan<U>(begin_, this->size());
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
   * Make sure that enough memory is allocated to hold min_capacity elements.
   * This won't necessarily make an allocation when min_capacity is small.
   * The actual size of the vector does not change.
   */
  void reserve(const int64_t min_capacity)
  {
    if (min_capacity > this->capacity()) {
      this->realloc_to_at_least(min_capacity);
    }
  }

  /**
   * Change the size of the vector so that it contains new_size elements.
   * If new_size is smaller than the old size, the elements at the end of the vector are
   * destructed. If new_size is larger than the old size, the new elements at the end are default
   * constructed. If T is trivially constructible, the memory is not touched by this function.
   */
  void resize(const int64_t new_size)
  {
    BLI_assert(new_size >= 0);
    const int64_t old_size = this->size();
    if (new_size > old_size) {
      this->reserve(new_size);
      default_construct_n(begin_ + old_size, new_size - old_size);
    }
    else {
      destruct_n(begin_ + new_size, old_size - new_size);
    }
    end_ = begin_ + new_size;
    UPDATE_VECTOR_SIZE(this);
  }

  /**
   * Change the size of the vector so that it contains new_size elements.
   * If new_size is smaller than the old size, the elements at the end of the vector are
   * destructed. If new_size is larger than the old size, the new elements will be copy constructed
   * from the given value.
   */
  void resize(const int64_t new_size, const T &value)
  {
    BLI_assert(new_size >= 0);
    const int64_t old_size = this->size();
    if (new_size > old_size) {
      this->reserve(new_size);
      uninitialized_fill_n(begin_ + old_size, new_size - old_size, value);
    }
    else {
      destruct_n(begin_ + new_size, old_size - new_size);
    }
    end_ = begin_ + new_size;
    UPDATE_VECTOR_SIZE(this);
  }

  /**
   * Afterwards the vector has 0 elements, but will still have
   * memory to be refilled again.
   */
  void clear()
  {
    destruct_n(begin_, this->size());
    end_ = begin_;
    UPDATE_VECTOR_SIZE(this);
  }

  /**
   * Afterwards the vector has 0 elements and any allocated memory
   * will be freed.
   */
  void clear_and_make_inline()
  {
    destruct_n(begin_, this->size());
    if (!this->is_inline()) {
      allocator_.deallocate(begin_);
    }

    begin_ = inline_buffer_;
    end_ = begin_;
    capacity_end_ = begin_ + InlineBufferCapacity;
    UPDATE_VECTOR_SIZE(this);
  }

  /**
   * Insert a new element at the end of the vector.
   * This might cause a reallocation with the capacity is exceeded.
   *
   * This is similar to std::vector::push_back.
   */
  void append(const T &value)
  {
    this->append_as(value);
  }
  void append(T &&value)
  {
    this->append_as(std::move(value));
  }
  /* This is similar to `std::vector::emplace_back`. */
  template<typename... ForwardValue> void append_as(ForwardValue &&...value)
  {
    this->ensure_space_for_one();
    this->append_unchecked_as(std::forward<ForwardValue>(value)...);
  }

  /**
   * Append the value to the vector and return the index that can be used to access the newly
   * added value.
   */
  int64_t append_and_get_index(const T &value)
  {
    const int64_t index = this->size();
    this->append(value);
    return index;
  }

  /**
   * Append the value if it is not yet in the vector. This has to do a linear search to check if
   * the value is in the vector. Therefore, this should only be called when it is known that the
   * vector is small.
   */
  void append_non_duplicates(const T &value)
  {
    if (!this->contains(value)) {
      this->append(value);
    }
  }

  /**
   * Append the value and assume that vector has enough memory reserved. This invokes undefined
   * behavior when not enough capacity has been reserved beforehand. Only use this in performance
   * critical code.
   */
  void append_unchecked(const T &value)
  {
    this->append_unchecked_as(value);
  }
  void append_unchecked(T &&value)
  {
    this->append_unchecked_as(std::move(value));
  }
  template<typename... ForwardT> void append_unchecked_as(ForwardT &&...value)
  {
    BLI_assert(end_ < capacity_end_);
    new (end_) T(std::forward<ForwardT>(value)...);
    end_++;
    UPDATE_VECTOR_SIZE(this);
  }

  /**
   * Insert the same element n times at the end of the vector.
   * This might result in a reallocation internally.
   */
  void append_n_times(const T &value, const int64_t n)
  {
    BLI_assert(n >= 0);
    this->reserve(this->size() + n);
    uninitialized_fill_n(end_, n, value);
    this->increase_size_by_unchecked(n);
  }

  /**
   * Enlarges the size of the internal buffer that is considered to be initialized.
   * This invokes undefined behavior when the new size is larger than the capacity.
   * The method can be useful when you want to call constructors in the vector yourself.
   * This should only be done in very rare cases and has to be justified every time.
   */
  void increase_size_by_unchecked(const int64_t n) noexcept
  {
    BLI_assert(end_ + n <= capacity_end_);
    end_ += n;
    UPDATE_VECTOR_SIZE(this);
  }

  /**
   * Copy the elements of another array to the end of this vector.
   *
   * This can be used to emulate parts of std::vector::insert.
   */
  void extend(Span<T> array)
  {
    this->extend(array.data(), array.size());
  }
  void extend(const T *start, int64_t amount)
  {
    this->reserve(this->size() + amount);
    this->extend_unchecked(start, amount);
  }

  /**
   * Adds all elements from the array that are not already in the vector. This is an expensive
   * operation when the vector is large, but can be very cheap when it is known that the vector is
   * small.
   */
  void extend_non_duplicates(Span<T> array)
  {
    for (const T &value : array) {
      this->append_non_duplicates(value);
    }
  }

  /**
   * Extend the vector without bounds checking. It is assumed that enough memory has been reserved
   * beforehand. Only use this in performance critical code.
   */
  void extend_unchecked(Span<T> array)
  {
    this->extend_unchecked(array.data(), array.size());
  }
  void extend_unchecked(const T *start, int64_t amount)
  {
    BLI_assert(amount >= 0);
    BLI_assert(begin_ + amount <= capacity_end_);
    uninitialized_copy_n(start, amount, end_);
    end_ += amount;
    UPDATE_VECTOR_SIZE(this);
  }

  template<typename InputIt> void extend(InputIt first, InputIt last)
  {
    this->insert(this->end(), first, last);
  }

  /**
   * Insert elements into the vector at the specified position. This has a running time of O(n)
   * where n is the number of values that have to be moved. Undefined behavior is invoked when the
   * insert position is out of bounds.
   */
  void insert(const int64_t insert_index, const T &value)
  {
    this->insert(insert_index, Span<T>(&value, 1));
  }
  void insert(const int64_t insert_index, T &&value)
  {
    this->insert(
        insert_index, std::make_move_iterator(&value), std::make_move_iterator(&value + 1));
  }
  void insert(const int64_t insert_index, Span<T> array)
  {
    this->insert(begin_ + insert_index, array.begin(), array.end());
  }
  template<typename InputIt> void insert(const T *insert_position, InputIt first, InputIt last)
  {
    const int64_t insert_index = insert_position - begin_;
    this->insert(insert_index, first, last);
  }
  template<typename InputIt> void insert(const int64_t insert_index, InputIt first, InputIt last)
  {
    BLI_assert(insert_index >= 0);
    BLI_assert(insert_index <= this->size());

    const int64_t insert_amount = std::distance(first, last);
    const int64_t old_size = this->size();
    const int64_t new_size = old_size + insert_amount;
    const int64_t move_amount = old_size - insert_index;

    this->reserve(new_size);
    for (int64_t i = 0; i < move_amount; i++) {
      const int64_t src_index = insert_index + move_amount - i - 1;
      const int64_t dst_index = new_size - i - 1;
      try {
        new (static_cast<void *>(begin_ + dst_index)) T(std::move(begin_[src_index]));
      }
      catch (...) {
        /* Destruct all values that have been moved already. */
        destruct_n(begin_ + dst_index + 1, i);
        end_ = begin_ + src_index + 1;
        UPDATE_VECTOR_SIZE(this);
        throw;
      }
      begin_[src_index].~T();
    }

    try {
      std::uninitialized_copy_n(first, insert_amount, begin_ + insert_index);
    }
    catch (...) {
      /* Destruct all values that have been moved. */
      destruct_n(begin_ + new_size - move_amount, move_amount);
      end_ = begin_ + insert_index;
      UPDATE_VECTOR_SIZE(this);
      throw;
    }
    end_ = begin_ + new_size;
    UPDATE_VECTOR_SIZE(this);
  }

  /**
   * Insert values at the beginning of the vector. The has to move all the other elements, so it
   * has a linear running time.
   */
  void prepend(const T &&value)
  {
    this->insert(0, value);
  }
  void prepend(T &&value)
  {
    this->insert(0, std::move(value));
  }
  void prepend(Span<T> values)
  {
    this->insert(0, values);
  }
  template<typename InputIt> void prepend(InputIt first, InputIt last)
  {
    this->insert(0, first, last);
  }

  /**
   * Return a reference to the last element in the vector.
   * This invokes undefined behavior when the vector is empty.
   */
  const T &last() const
  {
    BLI_assert(this->size() > 0);
    return *(end_ - 1);
  }
  T &last()
  {
    BLI_assert(this->size() > 0);
    return *(end_ - 1);
  }

  /**
   * Return a reference to the first element in the vector.
   * This invokes undefined behavior when the vector is empty.
   */
  const T &first() const
  {
    BLI_assert(this->size() > 0);
    return *begin_;
  }
  T &first()
  {
    BLI_assert(this->size() > 0);
    return *begin_;
  }

  /**
   * Return how many values are currently stored in the vector.
   */
  int64_t size() const
  {
    const int64_t current_size = static_cast<int64_t>(end_ - begin_);
    BLI_assert(debug_size_ == current_size);
    return current_size;
  }

  /**
   * Returns true when the vector contains no elements, otherwise false.
   *
   * This is the same as std::vector::empty.
   */
  bool is_empty() const
  {
    return begin_ == end_;
  }

  /**
   * Destructs the last element and decreases the size by one. This invokes undefined behavior when
   * the vector is empty.
   */
  void remove_last()
  {
    BLI_assert(!this->is_empty());
    end_--;
    end_->~T();
    UPDATE_VECTOR_SIZE(this);
  }

  /**
   * Remove the last element from the vector and return it. This invokes undefined behavior when
   * the vector is empty.
   *
   * This is similar to std::vector::pop_back.
   */
  T pop_last()
  {
    BLI_assert(!this->is_empty());
    T value = std::move(*(end_ - 1));
    end_--;
    end_->~T();
    UPDATE_VECTOR_SIZE(this);
    return value;
  }

  /**
   * Delete any element in the vector. The empty space will be filled by the previously last
   * element. This takes O(1) time.
   */
  void remove_and_reorder(const int64_t index)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->size());
    T *element_to_remove = begin_ + index;
    T *last_element = end_ - 1;
    if (element_to_remove < last_element) {
      *element_to_remove = std::move(*last_element);
    }
    end_ = last_element;
    last_element->~T();
    UPDATE_VECTOR_SIZE(this);
  }

  /**
   * Finds the first occurrence of the value, removes it and copies the last element to the hole in
   * the vector. This takes O(n) time.
   */
  void remove_first_occurrence_and_reorder(const T &value)
  {
    const int64_t index = this->first_index_of(value);
    this->remove_and_reorder(index);
  }

  /**
   * Remove the element at the given index and move all values coming after it one towards the
   * front. This takes O(n) time. If the order is not important, remove_and_reorder should be used
   * instead.
   *
   * This is similar to std::vector::erase.
   */
  void remove(const int64_t index)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->size());
    const int64_t last_index = this->size() - 1;
    for (int64_t i = index; i < last_index; i++) {
      begin_[i] = std::move(begin_[i + 1]);
    }
    begin_[last_index].~T();
    end_--;
    UPDATE_VECTOR_SIZE(this);
  }

  /**
   * Remove a contiguous chunk of elements and move all values coming after it towards the front.
   * This takes O(n) time.
   *
   * This is similar to std::vector::erase.
   */
  void remove(const int64_t start_index, const int64_t amount)
  {
    const int64_t old_size = this->size();
    BLI_assert(start_index >= 0);
    BLI_assert(amount >= 0);
    BLI_assert(start_index + amount <= old_size);
    const int64_t move_amount = old_size - start_index - amount;
    for (int64_t i = 0; i < move_amount; i++) {
      begin_[start_index + i] = std::move(begin_[start_index + amount + i]);
    }
    destruct_n(end_ - amount, amount);
    end_ -= amount;
    UPDATE_VECTOR_SIZE(this);
  }

  /**
   * Do a linear search to find the value in the vector.
   * When found, return the first index, otherwise return -1.
   */
  int64_t first_index_of_try(const T &value) const
  {
    for (const T *current = begin_; current != end_; current++) {
      if (*current == value) {
        return static_cast<int64_t>(current - begin_);
      }
    }
    return -1;
  }

  /**
   * Do a linear search to find the value in the vector and return the found index. This invokes
   * undefined behavior when the value is not in the vector.
   */
  int64_t first_index_of(const T &value) const
  {
    const int64_t index = this->first_index_of_try(value);
    BLI_assert(index >= 0);
    return index;
  }

  /**
   * Do a linear search to see of the value is in the vector.
   * Return true when it exists, otherwise false.
   */
  bool contains(const T &value) const
  {
    return this->first_index_of_try(value) != -1;
  }

  /**
   * Copies the given value to every element in the vector.
   */
  void fill(const T &value) const
  {
    initialized_fill_n(begin_, this->size(), value);
  }

  /**
   * Get access to the underlying array.
   */
  T *data()
  {
    return begin_;
  }

  /**
   * Get access to the underlying array.
   */
  const T *data() const
  {
    return begin_;
  }

  T *begin()
  {
    return begin_;
  }
  T *end()
  {
    return end_;
  }

  const T *begin() const
  {
    return begin_;
  }
  const T *end() const
  {
    return end_;
  }

  std::reverse_iterator<T *> rbegin()
  {
    return std::reverse_iterator<T *>(this->end());
  }
  std::reverse_iterator<T *> rend()
  {
    return std::reverse_iterator<T *>(this->begin());
  }

  std::reverse_iterator<const T *> rbegin() const
  {
    return std::reverse_iterator<T *>(this->end());
  }
  std::reverse_iterator<const T *> rend() const
  {
    return std::reverse_iterator<T *>(this->begin());
  }

  /**
   * Get the current capacity of the vector, i.e. the maximum number of elements the vector can
   * hold, before it has to reallocate.
   */
  int64_t capacity() const
  {
    return static_cast<int64_t>(capacity_end_ - begin_);
  }

  /**
   * Get an index range that makes looping over all indices more convenient and less error prone.
   * Obviously, this should only be used when you actually need the index in the loop.
   *
   * Example:
   *  for (int64_t i : myvector.index_range()) {
   *    do_something(i, my_vector[i]);
   *  }
   */
  IndexRange index_range() const
  {
    return IndexRange(this->size());
  }

  friend bool operator==(const Vector &a, const Vector &b)
  {
    return a.as_span() == b.as_span();
  }

  friend bool operator!=(const Vector &a, const Vector &b)
  {
    return !(a == b);
  }

  /**
   * Print some debug information about the vector.
   */
  void print_stats(StringRef name = "") const
  {
    std::cout << "Vector Stats: " << name << "\n";
    std::cout << "  Address: " << this << "\n";
    std::cout << "  Elements: " << this->size() << "\n";
    std::cout << "  Capacity: " << (capacity_end_ - begin_) << "\n";
    std::cout << "  Inline Capacity: " << InlineBufferCapacity << "\n";

    char memory_size_str[15];
    BLI_str_format_byte_unit(memory_size_str, sizeof(*this), true);
    std::cout << "  Size on Stack: " << memory_size_str << "\n";
  }

 private:
  bool is_inline() const
  {
    return begin_ == inline_buffer_;
  }

  void ensure_space_for_one()
  {
    if (UNLIKELY(end_ >= capacity_end_)) {
      this->realloc_to_at_least(this->size() + 1);
    }
  }

  BLI_NOINLINE void realloc_to_at_least(const int64_t min_capacity)
  {
    if (this->capacity() >= min_capacity) {
      return;
    }

    /* At least double the size of the previous allocation. Otherwise consecutive calls to grow can
     * cause a reallocation every time even though min_capacity only increments. */
    const int64_t min_new_capacity = this->capacity() * 2;

    const int64_t new_capacity = std::max(min_capacity, min_new_capacity);
    const int64_t size = this->size();

    T *new_array = static_cast<T *>(
        allocator_.allocate(static_cast<size_t>(new_capacity) * sizeof(T), alignof(T), AT));
    try {
      uninitialized_relocate_n(begin_, size, new_array);
    }
    catch (...) {
      allocator_.deallocate(new_array);
      throw;
    }

    if (!this->is_inline()) {
      allocator_.deallocate(begin_);
    }

    begin_ = new_array;
    end_ = begin_ + size;
    capacity_end_ = begin_ + new_capacity;
  }
};

#undef UPDATE_VECTOR_SIZE

/**
 * Same as a normal Vector, but does not use Blender's guarded allocator. This is useful when
 * allocating memory with static storage duration.
 */
template<typename T, int64_t InlineBufferCapacity = default_inline_buffer_capacity(sizeof(T))>
using RawVector = Vector<T, InlineBufferCapacity, RawAllocator>;

} /* namespace blender */
