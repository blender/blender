/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * A `blender::Pool` allows fast allocation and deallocation of many elements of the same type.
 *
 * It is compatible with types that are not movable.
 *
 * Freed elements memory will be reused by next allocations.
 * Elements are allocated in chunks to reduce memory fragmentation and avoid reallocation.
 */

#pragma once

#include "BLI_stack.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

namespace blender {

template<typename T, int64_t ChunkLen = 64> class Pool : NonCopyable {
 private:
  using Chunk = TypedBuffer<T, ChunkLen>;

  /** Allocated item buffer. */
  Vector<std::unique_ptr<Chunk>> values_;
  /** List of freed elements to be use for the next allocations. A Stack is best here to avoid
   * overhead when growing the free list. It also offers better cache performance than a queue
   * since last added entries will be reused first. */
  Stack<T *, 0> free_list_;

 public:
  ~Pool()
  {
    /* All elements need to be freed before freeing the pool. */
    BLI_assert(this->size() == 0);
  }

  /**
   * Construct an object inside this pool's memory.
   */
  template<typename... ForwardT> T &construct(ForwardT &&...value)
  {
    if (free_list_.is_empty()) {
      values_.append(std::make_unique<Chunk>());
      T *chunk_start = values_.last()->ptr();
      for (auto i : IndexRange(ChunkLen)) {
        free_list_.push(chunk_start + i);
      }
    }
    T *ptr = free_list_.pop();
    new (ptr) T(std::forward<ForwardT>(value)...);
    return *ptr;
  }

  /**
   * Destroy the given element inside this memory pool. Memory will be reused by next element
   * construction. This invokes undefined behavior if the item is not from this pool.
   */
  void destruct(T &value)
  {
    value.~T();
    free_list_.push(&value);
  }

  /**
   * Return the number of constructed elements in this pool.
   */
  int64_t size() const
  {
    return values_.size() * ChunkLen - free_list_.size();
  }

  /**
   * Returns true when the pool contains no elements, otherwise false.
   */
  bool is_empty() const
  {
    return this->size() == 0;
  }
};

}  // namespace blender
