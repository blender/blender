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

#ifndef __BLI_STACK_HH__
#define __BLI_STACK_HH__

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

  uint capacity() const
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
     *
     * When T is large, the small buffer optimization is disabled by default to avoid large
     * unexpected allocations on the stack. It can still be enabled explicitely though.
     */
    uint InlineBufferCapacity = (sizeof(T) < 100) ? 4 : 0,
    /**
     * The allocator used by this stack. Should rarely be changed, except when you don't want that
     * MEM_* is used internally.
     */
    typename Allocator = GuardedAllocator>
class Stack {
 private:
  using Chunk = StackChunk<T>;

  /**
   * Points to one element after top-most value in the stack.
   *
   * Invariant:
   *  If m_size == 0
   *    then: m_top == m_inline_chunk.begin
   *    else: &peek() == m_top - 1;
   */
  T *m_top;

  /** Points to the chunk that references the memory pointed to by m_top. */
  Chunk *m_top_chunk;

  /**
   * Number of elements in the entire stack. The sum of initialized element counts in the chunks.
   */
  uint m_size;

  /** The buffer used to implement small object optimization. */
  AlignedBuffer<sizeof(T) * InlineBufferCapacity, alignof(T)> m_inline_buffer;

  /**
   * A chunk referencing the inline buffer. This is always the bottom-most chunk.
   * So m_inline_chunk.below == nullptr.
   */
  Chunk m_inline_chunk;

  /** Used for allocations when the inline buffer is not large enough. */
  Allocator m_allocator;

 public:
  /**
   * Initialize an empty stack. No heap allocation is done.
   */
  Stack(Allocator allocator = {}) : m_allocator(allocator)
  {
    T *inline_buffer = this->inline_buffer();

    m_inline_chunk.below = nullptr;
    m_inline_chunk.above = nullptr;
    m_inline_chunk.begin = inline_buffer;
    m_inline_chunk.capacity_end = inline_buffer + InlineBufferCapacity;

    m_top = inline_buffer;
    m_top_chunk = &m_inline_chunk;
    m_size = 0;
  }

  /**
   * Create a new stack that contains the given elements. The values are pushed to the stack in
   * the order they are in the array.
   */
  Stack(Span<T> values) : Stack()
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
  Stack(const std::initializer_list<T> &values) : Stack(Span<T>(values))
  {
  }

  Stack(const Stack &other) : Stack(other.m_allocator)
  {
    for (const Chunk *chunk = &other.m_inline_chunk; chunk; chunk = chunk->above) {
      const T *begin = chunk->begin;
      const T *end = (chunk == other.m_top_chunk) ? other.m_top : chunk->capacity_end;
      this->push_multiple(Span<T>(begin, end - begin));
    }
  }

  Stack(Stack &&other) noexcept : Stack(other.m_allocator)
  {
    uninitialized_relocate_n(other.inline_buffer(),
                             std::min(other.m_size, InlineBufferCapacity),
                             this->inline_buffer());

    m_inline_chunk.above = other.m_inline_chunk.above;
    m_size = other.m_size;

    if (m_size <= InlineBufferCapacity) {
      m_top_chunk = &m_inline_chunk;
      m_top = this->inline_buffer() + m_size;
    }
    else {
      m_top_chunk = other.m_top_chunk;
      m_top = other.m_top;
    }

    other.m_size = 0;
    other.m_inline_chunk.above = nullptr;
    other.m_top_chunk = &other.m_inline_chunk;
    other.m_top = other.m_top_chunk->begin;
  }

  ~Stack()
  {
    this->destruct_all_elements();
    Chunk *above_chunk;
    for (Chunk *chunk = m_inline_chunk.above; chunk; chunk = above_chunk) {
      above_chunk = chunk->above;
      m_allocator.deallocate(chunk);
    }
  }

  Stack &operator=(const Stack &stack)
  {
    if (this == &stack) {
      return *this;
    }

    this->~Stack();
    new (this) Stack(stack);

    return *this;
  }

  Stack &operator=(Stack &&stack)
  {
    if (this == &stack) {
      return *this;
    }

    this->~Stack();
    new (this) Stack(std::move(stack));

    return *this;
  }

  /**
   * Add a new element to the top of the stack.
   */
  void push(const T &value)
  {
    if (m_top == m_top_chunk->capacity_end) {
      this->activate_next_chunk(1);
    }
    new (m_top) T(value);
    m_top++;
    m_size++;
  }
  void push(T &&value)
  {
    if (m_top == m_top_chunk->capacity_end) {
      this->activate_next_chunk(1);
    }
    new (m_top) T(std::move(value));
    m_top++;
    m_size++;
  }

  /**
   * Remove and return the top-most element from the stack. This invokes undefined behavior when
   * the stack is empty.
   */
  T pop()
  {
    BLI_assert(m_size > 0);
    m_top--;
    T value = std::move(*m_top);
    m_top->~T();
    m_size--;

    if (m_top == m_top_chunk->begin) {
      if (m_top_chunk->below != nullptr) {
        m_top_chunk = m_top_chunk->below;
        m_top = m_top_chunk->capacity_end;
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
    BLI_assert(m_size > 0);
    BLI_assert(m_top > m_top_chunk->begin);
    return *(m_top - 1);
  }
  const T &peek() const
  {
    BLI_assert(m_size > 0);
    BLI_assert(m_top > m_top_chunk->begin);
    return *(m_top - 1);
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
      if (m_top == m_top_chunk->capacity_end) {
        this->activate_next_chunk(remaining_values.size());
      }

      uint remaining_capacity = m_top_chunk->capacity_end - m_top;
      uint amount = std::min(remaining_values.size(), remaining_capacity);
      uninitialized_copy_n(remaining_values.data(), amount, m_top);
      m_top += amount;

      remaining_values = remaining_values.drop_front(amount);
    }

    m_size += values.size();
  }

  /**
   * Returns true when the size is zero.
   */
  bool is_empty() const
  {
    return m_size == 0;
  }

  /**
   * Returns the number of elements in the stack.
   */
  uint size() const
  {
    return m_size;
  }

  /**
   * Removes all elements from the stack. The memory is not freed, so it is more efficient to reuse
   * the stack than to create a new one.
   */
  void clear()
  {
    this->destruct_all_elements();
    m_top_chunk = &m_inline_chunk;
    m_top = m_top_chunk->begin;
  }

 private:
  T *inline_buffer() const
  {
    return (T *)m_inline_buffer.ptr();
  }

  /**
   * Changes m_top_chunk to point to a new chunk that is above the current one. The new chunk might
   * be smaller than the given size_hint. This happens when a chunk that has been allocated before
   * is reused. The size of the new chunk will be at least one.
   *
   * This invokes undefined behavior when the currently active chunk is not full.
   */
  void activate_next_chunk(uint size_hint)
  {
    BLI_assert(m_top == m_top_chunk->capacity_end);
    if (m_top_chunk->above == nullptr) {
      uint new_capacity = std::max(size_hint, m_top_chunk->capacity() * 2 + 10);

      /* Do a single memory allocation for the Chunk and the array it references. */
      void *buffer = m_allocator.allocate(
          sizeof(Chunk) + sizeof(T) * new_capacity + alignof(T), alignof(Chunk), AT);
      void *chunk_buffer = buffer;
      void *data_buffer = (void *)(((uintptr_t)buffer + sizeof(Chunk) + alignof(T) - 1) &
                                   ~(alignof(T) - 1));

      Chunk *new_chunk = new (chunk_buffer) Chunk();
      new_chunk->begin = (T *)data_buffer;
      new_chunk->capacity_end = new_chunk->begin + new_capacity;
      new_chunk->above = nullptr;
      new_chunk->below = m_top_chunk;
      m_top_chunk->above = new_chunk;
    }
    m_top_chunk = m_top_chunk->above;
    m_top = m_top_chunk->begin;
  }

  void destruct_all_elements()
  {
    for (T *value = m_top_chunk->begin; value != m_top; value++) {
      value->~T();
    }
    for (Chunk *chunk = m_top_chunk->below; chunk; chunk = chunk->below) {
      for (T *value = chunk->begin; value != chunk->capacity_end; value++) {
        value->~T();
      }
    }
  }
};

} /* namespace blender */

#endif /* __BLI_STACK_HH__ */
