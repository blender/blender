/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#pragma once

#include <algorithm>
#include <cmath>

#include "BLI_math_bits.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

namespace blender {

/**
 * A VectorList is a vector of vectors.
 *
 * VectorList can be used when:
 *
 * 1) Don't know up front the number of elements that will be added to the list.
 * Use `array` or `vector.reserve` when known up front.
 *
 * 2) Number of reads/writes doesn't require sequential access
 * of the whole list. A vector ensures memory is sequential which is fast when reading, writing can
 * have overhead when the reserved memory is full.
 *
 * When a VectorList reserved memory is full it will allocate memory for the new items, breaking
 * the sequential access. Within each allocated memory block the elements are ordered sequentially.
 *
 * Indexing has some overhead compared to a Vector or an Array, but it still has constant time
 * access.
 */
template<typename T, int64_t CapacityStart = 32, int64_t CapacityMax = 4096> class VectorList {
  using SelfT = VectorList<T, CapacityStart, CapacityMax>;
  using VectorT = Vector<T, 0>;

  static_assert(is_power_of_2(CapacityStart));
  static_assert(is_power_of_2(CapacityMax));
  static_assert(CapacityStart <= CapacityMax);

  /** Contains the individual vectors. There must always be at least one vector. */
  Vector<VectorT> vectors_;
  /** Number of vectors in use. */
  int64_t used_vectors_ = 0;
  /** Total element count across all vectors_. */
  int64_t size_ = 0;

 public:
  VectorList()
  {
    this->append_vector();
    used_vectors_ = 1;
  }

  VectorList(VectorList &&other) noexcept
  {
    vectors_ = std::move(other.vectors_);
    used_vectors_ = other.used_vectors_;
    size_ = other.size_;
    other.clear_and_shrink();
  }

  VectorList &operator=(VectorList &&other)
  {
    return move_assign_container(*this, std::move(other));
  }

  /** Insert a new element at the end of the VectorList. */
  void append(const T &value)
  {
    this->append_as(value);
  }

  /** Insert a new element at the end of the VectorList. */
  void append(T &&value)
  {
    this->append_as(std::move(value));
  }

  /** This is similar to `std::vector::emplace_back`. */
  template<typename ForwardT> void append_as(ForwardT &&value)
  {
    VectorT &vector = this->ensure_space_for_one();
    vector.append_unchecked_as(std::forward<ForwardT>(value));
    size_++;
  }

  /**
   * Return a reference to the first element in the VectorList.
   * This invokes undefined behavior when the VectorList is empty.
   */
  T &first()
  {
    BLI_assert(size() > 0);
    return vectors_.first().first();
  }

  /**
   * Return a reference to the last element in the VectorList.
   * This invokes undefined behavior when the VectorList is empty.
   */
  T &last()
  {
    BLI_assert(size() > 0);
    return vectors_[used_vectors_ - 1].last();
  }

  /** Return how many values are currently stored in the VectorList. */
  int64_t size() const
  {
    return size_;
  }

  /**
   * Returns true when the VectorList contains no elements, otherwise false.
   *
   * This is the same as std::vector::empty.
   */
  bool is_empty() const
  {
    return size_ == 0;
  }

  /** Afterwards the VectorList has 0 elements, but will still have memory to be refilled again. */
  void clear()
  {
    for (VectorT &vector : vectors_) {
      vector.clear();
    }
    used_vectors_ = 1;
    size_ = 0;
  }

  /** Afterwards the VectorList has 0 elements and the Vectors allocated memory will be freed. */
  void clear_and_shrink()
  {
    vectors_.clear();
    this->append_vector();
    used_vectors_ = 1;
    size_ = 0;
  }

  /**
   * Get the value at the given index.
   * This invokes undefined behavior when the index is out of bounds.
   */
  const T &operator[](int64_t index) const
  {
    std::pair<int64_t, int64_t> index_pair = this->global_index_to_index_pair(index);
    return vectors_[index_pair.first][index_pair.second];
  }

  /**
   * Get the value at the given index.
   * This invokes undefined behavior when the index is out of bounds.
   */
  T &operator[](int64_t index)
  {
    std::pair<int64_t, int64_t> index_pair = this->global_index_to_index_pair(index);
    return vectors_[index_pair.first][index_pair.second];
  }

 private:
  /**
   * Convert a global index into a Vector index and Element index pair.
   * We use the fact that vector sizes increase geometrically to compute this in constant time.
   * https://en.wikipedia.org/wiki/Geometric_progression
   */
  std::pair<int64_t, int64_t> global_index_to_index_pair(int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->size());

    auto log2 = [](int64_t value) -> int64_t {
      return 31 - bitscan_reverse_uint(uint32_t(value));
    };
    auto geometric_sum = [](int64_t index) -> int64_t {
      return CapacityStart * ((2 << index) - 1);
    };
    auto index_from_sum = [log2](int64_t sum) -> int64_t {
      return log2((sum / CapacityStart) + 1);
    };

    static const int64_t start_log2 = log2(CapacityStart);
    static const int64_t end_log2 = log2(CapacityMax);
    /* The number of vectors until CapacityMax size is reached. */
    static const int64_t geometric_steps = end_log2 - start_log2 + 1;
    /* The number of elements until CapacityMax size is reached. */
    static const int64_t geometric_total = geometric_sum(geometric_steps - 1);

    int64_t index_a, index_b;
    if (index < geometric_total) {
      index_a = index_from_sum(index);
      index_b = index_a > 0 ? index - geometric_sum(index_a - 1) : index;
    }
    else {
      int64_t linear_start = index - geometric_total;
      index_a = geometric_steps + linear_start / CapacityMax;
      index_b = linear_start % CapacityMax;
    }
    return {index_a, index_b};
  }

  VectorT &ensure_space_for_one()
  {
    if (vectors_[used_vectors_ - 1].is_at_capacity()) {
      if (used_vectors_ == vectors_.size()) {
        this->append_vector();
      }
      used_vectors_++;
    }
    return vectors_[used_vectors_ - 1];
  }

  void append_vector()
  {
    const int64_t new_vector_capacity = this->get_next_vector_capacity();
    vectors_.append({});
    vectors_.last().reserve(new_vector_capacity);
  }

  int64_t get_next_vector_capacity()
  {
    if (vectors_.is_empty()) {
      return CapacityStart;
    }
    return std::min(vectors_.last().capacity() * 2, CapacityMax);
  }

  template<typename IterableT, typename ElemT> struct Iterator {
    IterableT &vector_list;
    int64_t index_a = 0;
    int64_t index_b = 0;

    Iterator(IterableT &vector_list, int64_t index_a = 0, int64_t index_b = 0)
        : vector_list(vector_list), index_a(index_a), index_b(index_b)
    {
    }

    ElemT &operator*() const
    {
      return vector_list.vectors_[index_a][index_b];
    }

    Iterator &operator++()
    {
      if (vector_list.vectors_[index_a].size() == index_b + 1) {
        if (index_a + 1 == vector_list.used_vectors_) {
          /* Reached the end. */
          index_b++;
        }
        else {
          index_a++;
          index_b = 0;
        }
      }
      else {
        index_b++;
      }
      return *this;
    }

    bool operator==(const Iterator &other) const
    {
      BLI_assert(&other.vector_list == &vector_list);
      return other.index_a == index_a && other.index_b == index_b;
    }

    bool operator!=(const Iterator &other) const
    {
      return !(other == *this);
    }
  };

  using MutIterator = Iterator<SelfT, T>;
  using ConstIterator = Iterator<const SelfT, const T>;

 public:
  MutIterator begin()
  {
    return MutIterator(*this, 0, 0);
  }
  MutIterator end()
  {
    return MutIterator(*this, used_vectors_ - 1, vectors_[used_vectors_ - 1].size());
  }

  ConstIterator begin() const
  {
    return ConstIterator(*this, 0, 0);
  }
  ConstIterator end() const
  {
    return ConstIterator(*this, used_vectors_ - 1, vectors_[used_vectors_ - 1].size());
  }
};

}  // namespace blender
