/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

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
 *  for (int64_t i = 0; i < 10; i++) {
 *    for (int64_t j = 0; j < 20; j++) {
 *       for (int64_t k = 0; k < 30; k++) {
 *
 *  for (int64_t i : IndexRange(10)) {
 *    for (int64_t j : IndexRange(20)) {
 *      for (int64_t k : IndexRange(30)) {
 *
 * Some containers like blender::Vector have an index_range() method. This will return the
 * IndexRange that contains all indices that can be used to access the container. This is
 * particularly useful when you want to iterate over the indices and the elements (much like
 * Python's enumerate(), just worse). Again, I think the second example here is better:
 *
 *  for (int64_t i = 0; i < my_vector_with_a_long_name.size(); i++) {
 *    do_something(i, my_vector_with_a_long_name[i]);
 *
 *  for (int64_t i : my_vector_with_a_long_name.index_range()) {
 *    do_something(i, my_vector_with_a_long_name[i]);
 *
 * Ideally this could be could be even closer to Python's enumerate(). We might get that in the
 * future with newer C++ versions.
 */

#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>

#include "BLI_utildefines.h"

namespace blender {

template<typename T> class Span;

class IndexRange {
 private:
  int64_t start_ = 0;
  int64_t size_ = 0;

 public:
  constexpr IndexRange() = default;

  constexpr explicit IndexRange(int64_t size) : start_(0), size_(size)
  {
    BLI_assert(size >= 0);
  }

  constexpr IndexRange(int64_t start, int64_t size) : start_(start), size_(size)
  {
    BLI_assert(start >= 0);
    BLI_assert(size >= 0);
  }

  class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = int64_t;
    using pointer = const int64_t *;
    using reference = const int64_t &;
    using difference_type = std::ptrdiff_t;

   private:
    int64_t current_;

   public:
    constexpr explicit Iterator(int64_t current) : current_(current) {}

    constexpr Iterator &operator++()
    {
      current_++;
      return *this;
    }

    constexpr Iterator operator++(int)
    {
      Iterator copied_iterator = *this;
      ++(*this);
      return copied_iterator;
    }

    constexpr friend bool operator!=(const Iterator &a, const Iterator &b)
    {
      return a.current_ != b.current_;
    }

    constexpr friend bool operator==(const Iterator &a, const Iterator &b)
    {
      return a.current_ == b.current_;
    }

    constexpr friend int64_t operator-(const Iterator &a, const Iterator &b)
    {
      return a.current_ - b.current_;
    }

    constexpr int64_t operator*() const
    {
      return current_;
    }
  };

  constexpr Iterator begin() const
  {
    return Iterator(start_);
  }

  constexpr Iterator end() const
  {
    return Iterator(start_ + size_);
  }

  /**
   * Access an element in the range.
   */
  constexpr int64_t operator[](int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->size());
    return start_ + index;
  }

  /**
   * Two ranges compare equal when they contain the same numbers.
   */
  constexpr friend bool operator==(IndexRange a, IndexRange b)
  {
    return (a.size_ == b.size_) && (a.start_ == b.start_ || a.size_ == 0);
  }
  constexpr friend bool operator!=(IndexRange a, IndexRange b)
  {
    return !(a == b);
  }

  /**
   * Get the amount of numbers in the range.
   */
  constexpr int64_t size() const
  {
    return size_;
  }

  constexpr IndexRange index_range() const
  {
    return IndexRange(size_);
  }

  /**
   * Returns true if the size is zero.
   */
  constexpr bool is_empty() const
  {
    return size_ == 0;
  }

  /**
   * Create a new range starting at the end of the current one.
   */
  constexpr IndexRange after(int64_t n) const
  {
    BLI_assert(n >= 0);
    return IndexRange(start_ + size_, n);
  }

  /**
   * Create a new range that ends at the start of the current one.
   */
  constexpr IndexRange before(int64_t n) const
  {
    BLI_assert(n >= 0);
    return IndexRange(start_ - n, n);
  }

  /**
   * Get the first element in the range.
   * Asserts when the range is empty.
   */
  constexpr int64_t first() const
  {
    BLI_assert(this->size() > 0);
    return start_;
  }

  /**
   * Get the nth last element in the range.
   * Asserts when the range is empty or when n is negative.
   */
  constexpr int64_t last(const int64_t n = 0) const
  {
    BLI_assert(n >= 0);
    BLI_assert(n < size_);
    BLI_assert(this->size() > 0);
    return start_ + size_ - 1 - n;
  }

  /**
   * Get the element one before the beginning. The returned value is undefined when the range is
   * empty, and the range must start after zero already.
   */
  constexpr int64_t one_before_start() const
  {
    BLI_assert(start_ > 0);
    return start_ - 1;
  }

  /**
   * Get the element one after the end. The returned value is undefined when the range is empty.
   */
  constexpr int64_t one_after_last() const
  {
    return start_ + size_;
  }

  /**
   * Get the first element in the range. The returned value is undefined when the range is empty.
   */
  constexpr int64_t start() const
  {
    return start_;
  }

  /**
   * Returns true when the range contains a certain number, otherwise false.
   */
  constexpr bool contains(int64_t value) const
  {
    return value >= start_ && value < start_ + size_;
  }

  /**
   * Returns a new range, that contains a sub-interval of the current one.
   */
  constexpr IndexRange slice(int64_t start, int64_t size) const
  {
    BLI_assert(start >= 0);
    BLI_assert(size >= 0);
    int64_t new_start = start_ + start;
    BLI_assert(new_start + size <= start_ + size_ || size == 0);
    return IndexRange(new_start, size);
  }
  constexpr IndexRange slice(IndexRange range) const
  {
    return this->slice(range.start(), range.size());
  }

  /**
   * Returns a new IndexRange that contains the intersection of the current one with the given
   * range. Returns empty range if there are no overlapping indices. The returned range is always
   * a valid slice of this range.
   */
  constexpr IndexRange intersect(IndexRange other) const
  {
    const int64_t old_end = start_ + size_;
    const int64_t new_start = std::min(old_end, std::max(start_, other.start_));
    const int64_t new_end = std::max(new_start, std::min(old_end, other.start_ + other.size_));
    return IndexRange(new_start, new_end - new_start);
  }

  /**
   * Returns a new IndexRange with n elements removed from the beginning of the range.
   * This invokes undefined behavior when n is negative.
   */
  constexpr IndexRange drop_front(int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::max<int64_t>(0, size_ - n);
    return IndexRange(start_ + n, new_size);
  }

  /**
   * Returns a new IndexRange with n elements removed from the end of the range.
   * This invokes undefined behavior when n is negative.
   */
  constexpr IndexRange drop_back(int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::max<int64_t>(0, size_ - n);
    return IndexRange(start_, new_size);
  }

  /**
   * Returns a new IndexRange that only contains the first n elements. This invokes undefined
   * behavior when n is negative.
   */
  constexpr IndexRange take_front(int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::min<int64_t>(size_, n);
    return IndexRange(start_, new_size);
  }

  /**
   * Returns a new IndexRange that only contains the last n elements. This invokes undefined
   * behavior when n is negative.
   */
  constexpr IndexRange take_back(int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::min<int64_t>(size_, n);
    return IndexRange(start_ + size_ - new_size, new_size);
  }

  /**
   * Move the range forward or backward within the larger array. The amount may be negative,
   * but its absolute value cannot be greater than the existing start of the range.
   */
  constexpr IndexRange shift(int64_t n) const
  {
    return IndexRange(start_ + n, size_);
  }

  friend std::ostream &operator<<(std::ostream &stream, IndexRange range)
  {
    stream << "[" << range.start() << ", " << range.one_after_last() << ")";
    return stream;
  }
};

struct AlignedIndexRanges {
  IndexRange prefix;
  IndexRange aligned;
  IndexRange suffix;
};

/**
 * Split a range into three parts so that the boundaries of the middle part are aligned to some
 * power of two.
 *
 * This can be used when an algorithm can be optimized on aligned indices/memory. The algorithm
 * then needs a slow path for the beginning and end, and a fast path for the aligned elements.
 */
AlignedIndexRanges split_index_range_by_alignment(const IndexRange range, const int64_t alignment);

}  // namespace blender
