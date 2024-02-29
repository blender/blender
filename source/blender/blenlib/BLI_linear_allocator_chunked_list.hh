/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <array>

#include "BLI_linear_allocator.hh"
#include "BLI_struct_equality_utils.hh"
#include "BLI_utility_mixins.hh"

namespace blender::linear_allocator {

/**
 * The list is a linked list of segments containing multiple elements. The capacity of each segment
 * is a template parameter because that removes the need to store it for every segment.
 */
template<typename T, int64_t Capacity> struct ChunkedListSegment {
  /** Pointer to the next segment in the list. */
  ChunkedListSegment *next = nullptr;
  /**
   * Number of constructed elements in this segment. The constructed elements are always at the
   * beginning of the array below.
   */
  int64_t size = 0;
  /**
   * The memory that actually contains the values in the end. The values are constructed and
   * destructed by higher level code.
   */
  std::array<TypedBuffer<T>, Capacity> values;
};

/**
 * This is a special purpose container data structure that can be used to efficiently gather many
 * elements into many (small) lists for later retrieval. Insertion order is *not* maintained.
 *
 * To use this data structure, one has to have a separate #LinearAllocator which is passed to the
 * `append` function. This allows the same allocator to be used by many lists. Passing it into the
 * append function also removes the need to store the allocator pointer in every list.
 *
 * It is an improvement over #Vector because it does not require any reallocations. #VectorList
 * could also be used to overcome the reallocation issue.
 *
 * This data structure is also an improvement over #VectorList because:
 * - It has a much lower memory footprint when empty.
 * - Allows using a #LinearAllocator for all allocations, without storing the pointer to it in
 *   every vector.
 * - It wastes less memory due to over-allocations.
 */
template<typename T, int64_t SegmentCapacity = 4> class ChunkedList : NonCopyable {
 private:
  using Segment = ChunkedListSegment<T, SegmentCapacity>;
  Segment *current_segment_ = nullptr;

 public:
  ChunkedList() = default;

  ChunkedList(ChunkedList &&other)
  {
    current_segment_ = other.current_segment_;
    other.current_segment_ = nullptr;
  }

  ~ChunkedList()
  {
    /* This code assumes that the #ChunkedListSegment does not have to be destructed if the
     * contained type is trivially destructible. */
    static_assert(std::is_trivially_destructible_v<ChunkedListSegment<int, 4>>);
    if constexpr (!std::is_trivially_destructible_v<T>) {
      for (Segment *segment = current_segment_; segment; segment = segment->next) {
        for (const int64_t i : IndexRange(segment->size)) {
          T &value = *segment->values[i];
          std::destroy_at(&value);
        }
      }
    }
  }

  ChunkedList &operator=(ChunkedList &&other)
  {
    if (this == &other) {
      return *this;
    }
    std::destroy_at(this);
    new (this) ChunkedList(std::move(other));
    return *this;
  }

  /**
   * Add an element to the list. The insertion order is not maintained. The given allocator is used
   * to allocate any extra memory that may be needed.
   */
  void append(LinearAllocator<> &allocator, const T &value)
  {
    this->append_as(allocator, value);
  }

  void append(LinearAllocator<> &allocator, T &&value)
  {
    this->append_as(allocator, std::move(value));
  }

  template<typename... Args> void append_as(LinearAllocator<> &allocator, Args &&...args)
  {
    if (current_segment_ == nullptr || current_segment_->size == SegmentCapacity) {
      /* Allocate a new segment if necessary. */
      static_assert(std::is_trivially_destructible_v<Segment>);
      Segment *new_segment = allocator.construct<Segment>().release();
      new_segment->next = current_segment_;
      current_segment_ = new_segment;
    }
    T *value = &*current_segment_->values[current_segment_->size++];
    new (value) T(std::forward<Args>(args)...);
  }

  class ConstIterator {
   private:
    const Segment *segment_ = nullptr;
    int64_t index_ = 0;

   public:
    ConstIterator(const Segment *segment, int64_t index = 0) : segment_(segment), index_(index) {}

    ConstIterator &operator++()
    {
      index_++;
      if (index_ == segment_->size) {
        segment_ = segment_->next;
        index_ = 0;
      }
      return *this;
    }

    const T &operator*() const
    {
      return *segment_->values[index_];
    }

    BLI_STRUCT_EQUALITY_OPERATORS_2(ConstIterator, segment_, index_)
  };

  class MutableIterator {
   private:
    Segment *segment_ = nullptr;
    int64_t index_ = 0;

   public:
    MutableIterator(Segment *segment, int64_t index = 0) : segment_(segment), index_(index) {}

    MutableIterator &operator++()
    {
      index_++;
      if (index_ == segment_->size) {
        segment_ = segment_->next;
        index_ = 0;
      }
      return *this;
    }

    T &operator*()
    {
      return *segment_->values[index_];
    }

    BLI_STRUCT_EQUALITY_OPERATORS_2(MutableIterator, segment_, index_)
  };

  ConstIterator begin() const
  {
    return ConstIterator(current_segment_, 0);
  }

  ConstIterator end() const
  {
    return ConstIterator(nullptr, 0);
  }

  MutableIterator begin()
  {
    return MutableIterator(current_segment_, 0);
  }

  MutableIterator end()
  {
    return MutableIterator(nullptr, 0);
  }
};

}  // namespace blender::linear_allocator
