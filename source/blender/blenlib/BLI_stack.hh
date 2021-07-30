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
 * A `blender::Stack<T>` is a dynamically growing FILO (first-in, last-out) data structure. It is
 * designed to be a more convenient and efficient replacement for `std::stack`.
 *
 * The improved efficiency is mainly achieved by supporting small buffer optimization. As long as
 * the number of elements added to the stack stays below InlineBufferCapacity, no heap allocation
 * is done. Consequently, values stored in the stack have to be movable and they might be moved,
 * when the stack is moved.
 *
 * A Vector can be used to emulate a stack. However, this stack implementation is more efficient
 * when all you have to do is to push and pop elements. That is because a vector guarantees that
 * all elements are in a contiguous array. Therefore, it has to copy all elements to a new buffer
 * when it grows. This stack implementation does not have to copy all previously pushed elements
 * when it grows.
 *
 * blender::Stack is implemented using a double linked list of chunks. Each chunk contains an array
 * of elements. The chunk size increases exponentially with every new chunk that is required. The
 * lowest chunk, i.e. the one that is used for the first few pushed elements, is embedded into the
 * stack.
 */

#include "BLI_allocator.hh"
#include "BLI_memory_utils.hh"
#include "BLI_span.hh"

namespace blender {

/**
 * A StackChunk references a contiguous memory buffer. Multiple StackChunk instances are linked in
 * a double linked list.
 */
template<typename T> struct StackChunk {
  /** The below chunk contains the elements that have been pushed on the stack before. */
  StackChunk *below;
  /** The above chunk contains the elements that have been pushed on the stack afterwards. */
  StackChunk *above;
  /** Pointer to the first element of the referenced buffer. */
  T *begin;
  /** Pointer to one element past the end of the referenced buffer. */
  T *capacity_end;

  int64_t capacity() const
  {
    return capacity_end - begin;
  }
};

template<
    /** Type of the elements that are stored in the stack. */
    typename T,
    /**
     * The number of values that can be stored in this stack, without doing a heap allocation.
     * Sometimes it can make sense to increase this value a lot. The memory in the inline buffer is
     * not initialized when it is not needed.
     */
    int64_t InlineBufferCapacity = default_inline_buffer_capacity(sizeof(T)),
    /**
     * The allocator used by this stack. Should rarely be changed, except when you don't want that
     * MEM_* is used internally.
     */
    typename Allocator = GuardedAllocator>
class Stack {
 public:
  using value_type = T;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using size_type = int64_t;

 private:
  using Chunk = StackChunk<T>;

  /**
   * Points to one element after top-most value in the stack.
   *
   * Invariant:
   *  If size_ == 0
   *    then: top_ == inline_chunk_.begin
   *    else: &peek() == top_ - 1;
   */
  T *top_;

  /** Points to the chunk that references the memory pointed to by top_. */
  Chunk *top_chunk_;

  /**
   * Number of elements in the entire stack. The sum of initialized element counts in the chunks.
   */
  int64_t size_;

  /** The buffer used to implement small object optimization. */
  TypedBuffer<T, InlineBufferCapacity> inline_buffer_;

  /**
   * A chunk referencing the inline buffer. This is always the bottom-most chunk.
   * So inline_chunk_.below == nullptr.
   */
  Chunk inline_chunk_;

  /** Used for allocations when the inline buffer is not large enough. */
  Allocator allocator_;

 public:
  /**
   * Initialize an empty stack. No heap allocation is done.
   */
  Stack(Allocator allocator = {}) noexcept : allocator_(allocator)
  {
    inline_chunk_.below = nullptr;
    inline_chunk_.above = nullptr;
    inline_chunk_.begin = inline_buffer_;
    inline_chunk_.capacity_end = inline_buffer_ + InlineBufferCapacity;

    top_ = inline_buffer_;
    top_chunk_ = &inline_chunk_;
    size_ = 0;
  }

  Stack(NoExceptConstructor, Allocator allocator = {}) noexcept : Stack(allocator)
  {
  }

  /**
   * Create a new stack that contains the given elements. The values are pushed to the stack in
   * the order they are in the array.
   */
  Stack(Span<T> values, Allocator allocator = {}) : Stack(NoExceptConstructor(), allocator)
  {
    this->push_multiple(values);
  }

  /**
   * Create a new stack that contains the given elements. The values are pushed to the stack in the
   * order they are in the array.
   *
   * Example:
   *  Stack<int> stack = {4, 5, 6};
   *  assert(stack.pop() == 6);
   *  assert(stack.pop() == 5);
   */
  Stack(const std::initializer_list<T> &values, Allocator allocator = {})
      : Stack(Span<T>(values), allocator)
  {
  }

  Stack(const Stack &other) : Stack(NoExceptConstructor(), other.allocator_)
  {
    for (const Chunk *chunk = &other.inline_chunk_; chunk; chunk = chunk->above) {
      const T *begin = chunk->begin;
      const T *end = (chunk == other.top_chunk_) ? other.top_ : chunk->capacity_end;
      this->push_multiple(Span<T>(begin, end - begin));
    }
  }

  Stack(Stack &&other) noexcept(std::is_nothrow_move_constructible_v<T>)
      : Stack(NoExceptConstructor(), other.allocator_)
  {
    uninitialized_relocate_n<T>(
        other.inline_buffer_, std::min(other.size_, InlineBufferCapacity), inline_buffer_);

    inline_chunk_.above = other.inline_chunk_.above;
    size_ = other.size_;

    if (inline_chunk_.above != nullptr) {
      inline_chunk_.above->below = &inline_chunk_;
    }

    if (size_ <= InlineBufferCapacity) {
      top_chunk_ = &inline_chunk_;
      top_ = inline_buffer_ + size_;
    }
    else {
      top_chunk_ = other.top_chunk_;
      top_ = other.top_;
    }

    other.size_ = 0;
    other.inline_chunk_.above = nullptr;
    other.top_chunk_ = &other.inline_chunk_;
    other.top_ = other.top_chunk_->begin;
  }

  ~Stack()
  {
    this->destruct_all_elements();
    Chunk *above_chunk;
    for (Chunk *chunk = inline_chunk_.above; chunk; chunk = above_chunk) {
      above_chunk = chunk->above;
      allocator_.deallocate(chunk);
    }
  }

  Stack &operator=(const Stack &other)
  {
    return copy_assign_container(*this, other);
  }

  Stack &operator=(Stack &&other)
  {
    return move_assign_container(*this, std::move(other));
  }

  /**
   * Add a new element to the top of the stack.
   */
  void push(const T &value)
  {
    this->push_as(value);
  }
  void push(T &&value)
  {
    this->push_as(std::move(value));
  }
  /* This is similar to `std::stack::emplace`. */
  template<typename... ForwardT> void push_as(ForwardT &&...value)
  {
    if (top_ == top_chunk_->capacity_end) {
      this->activate_next_chunk(1);
    }
    try {
      new (top_) T(std::forward<ForwardT>(value)...);
      top_++;
      size_++;
    }
    catch (...) {
      this->move_top_pointer_back_to_below_chunk();
      throw;
    }
  }

  /**
   * Remove and return the top-most element from the stack. This invokes undefined behavior when
   * the stack is empty.
   */
  T pop()
  {
    BLI_assert(size_ > 0);
    T value = std::move(*(top_ - 1));
    top_--;
    top_->~T();
    size_--;

    if (top_ == top_chunk_->begin) {
      if (top_chunk_->below != nullptr) {
        top_chunk_ = top_chunk_->below;
        top_ = top_chunk_->capacity_end;
      }
    }
    return value;
  }

  /**
   * Get a reference to the top-most element without removing it from the stack. This invokes
   * undefined behavior when the stack is empty.
   */
  T &peek()
  {
    BLI_assert(size_ > 0);
    BLI_assert(top_ > top_chunk_->begin);
    return *(top_ - 1);
  }
  const T &peek() const
  {
    BLI_assert(size_ > 0);
    BLI_assert(top_ > top_chunk_->begin);
    return *(top_ - 1);
  }

  /**
   * Add multiple elements to the stack. The values are pushed in the order they are in the array.
   * This method is more efficient than pushing multiple elements individually and might cause less
   * heap allocations.
   */
  void push_multiple(Span<T> values)
  {
    Span<T> remaining_values = values;
    while (!remaining_values.is_empty()) {
      if (top_ == top_chunk_->capacity_end) {
        this->activate_next_chunk(remaining_values.size());
      }

      const int64_t remaining_capacity = top_chunk_->capacity_end - top_;
      const int64_t amount = std::min(remaining_values.size(), remaining_capacity);
      try {
        uninitialized_copy_n(remaining_values.data(), amount, top_);
      }
      catch (...) {
        this->move_top_pointer_back_to_below_chunk();
        throw;
      }
      top_ += amount;
      size_ += amount;

      remaining_values = remaining_values.drop_front(amount);
    }
  }

  /**
   * Returns true when the size is zero.
   */
  bool is_empty() const
  {
    return size_ == 0;
  }

  /**
   * Returns the number of elements in the stack.
   */
  int64_t size() const
  {
    return size_;
  }

  /**
   * Removes all elements from the stack. The memory is not freed, so it is more efficient to reuse
   * the stack than to create a new one.
   */
  void clear()
  {
    this->destruct_all_elements();
    top_chunk_ = &inline_chunk_;
    top_ = top_chunk_->begin;
  }

  /* This should only be called by unit tests. */
  bool is_invariant_maintained() const
  {
    if (size_ == 0) {
      return top_ == inline_chunk_.begin;
    }
    return top_ > top_chunk_->begin;
  }

 private:
  /**
   * Changes top_chunk_ to point to a new chunk that is above the current one. The new chunk might
   * be smaller than the given size_hint. This happens when a chunk that has been allocated before
   * is reused. The size of the new chunk will be at least one.
   *
   * This invokes undefined behavior when the currently active chunk is not full.
   */
  void activate_next_chunk(const int64_t size_hint)
  {
    BLI_assert(top_ == top_chunk_->capacity_end);
    if (top_chunk_->above == nullptr) {
      const int64_t new_capacity = std::max(size_hint, top_chunk_->capacity() * 2 + 10);

      /* Do a single memory allocation for the Chunk and the array it references. */
      void *buffer = allocator_.allocate(
          sizeof(Chunk) + sizeof(T) * new_capacity + alignof(T), alignof(Chunk), AT);
      void *chunk_buffer = buffer;
      void *data_buffer = reinterpret_cast<void *>(
          (reinterpret_cast<uintptr_t>(buffer) + sizeof(Chunk) + alignof(T) - 1) &
          ~(alignof(T) - 1));

      Chunk *new_chunk = new (chunk_buffer) Chunk();
      new_chunk->begin = static_cast<T *>(data_buffer);
      new_chunk->capacity_end = new_chunk->begin + new_capacity;
      new_chunk->above = nullptr;
      new_chunk->below = top_chunk_;
      top_chunk_->above = new_chunk;
    }
    top_chunk_ = top_chunk_->above;
    top_ = top_chunk_->begin;
  }

  void move_top_pointer_back_to_below_chunk()
  {
    /* This makes sure that the invariant stays intact after a failed push. */
    if (size_ == 0) {
      top_ = inline_chunk_.begin;
    }
    else if (top_ == top_chunk_->begin) {
      top_chunk_ = top_chunk_->below;
      top_ = top_chunk_->capacity_end;
    }
  }

  void destruct_all_elements()
  {
    for (T *value = top_chunk_->begin; value != top_; value++) {
      value->~T();
    }
    for (Chunk *chunk = top_chunk_->below; chunk; chunk = chunk->below) {
      for (T *value = chunk->begin; value != chunk->capacity_end; value++) {
        value->~T();
      }
    }
  }
};

/**
 * Same as a normal Stack, but does not use Blender's guarded allocator. This is useful when
 * allocating memory with static storage duration.
 */
template<typename T, int64_t InlineBufferCapacity = default_inline_buffer_capacity(sizeof(T))>
using RawStack = Stack<T, InlineBufferCapacity, RawAllocator>;

} /* namespace blender */
