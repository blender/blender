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

/** \file
 * \ingroup bli
 *
 * A linear allocator is the simplest form of an allocator. It never reuses any memory, and
 * therefore does not need a deallocation method. It simply hands out consecutive buffers of
 * memory. When the current buffer is full, it reallocates a new larger buffer and continues.
 */

#pragma once

#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

namespace blender {

template<typename Allocator = GuardedAllocator> class LinearAllocator : NonCopyable, NonMovable {
 private:
  Allocator allocator_;
  Vector<void *> owned_buffers_;
  Vector<Span<char>> unused_borrowed_buffers_;

  uintptr_t current_begin_;
  uintptr_t current_end_;

#ifdef DEBUG
  int64_t debug_allocated_amount_ = 0;
#endif

  /* Buffers larger than that are not packed together with smaller allocations to avoid wasting
   * memory. */
  constexpr static inline int64_t large_buffer_threshold = 4096;

 public:
  LinearAllocator()
  {
    current_begin_ = 0;
    current_end_ = 0;
  }

  ~LinearAllocator()
  {
    for (void *ptr : owned_buffers_) {
      allocator_.deallocate(ptr);
    }
  }

  /**
   * Get a pointer to a memory buffer with the given size an alignment. The memory buffer will be
   * freed when this LinearAllocator is destructed.
   *
   * The alignment has to be a power of 2.
   */
  void *allocate(const int64_t size, const int64_t alignment)
  {
    BLI_assert(size >= 0);
    BLI_assert(alignment >= 1);
    BLI_assert(is_power_of_2_i(alignment));

    const uintptr_t alignment_mask = alignment - 1;
    const uintptr_t potential_allocation_begin = (current_begin_ + alignment_mask) &
                                                 ~alignment_mask;
    const uintptr_t potential_allocation_end = potential_allocation_begin + size;

    if (potential_allocation_end <= current_end_) {
#ifdef DEBUG
      debug_allocated_amount_ += size;
#endif
      current_begin_ = potential_allocation_end;
      return reinterpret_cast<void *>(potential_allocation_begin);
    }
    if (size <= large_buffer_threshold) {
      this->allocate_new_buffer(size + alignment, alignment);
      return this->allocate(size, alignment);
    }
    return this->allocator_large_buffer(size, alignment);
  };

  /**
   * Allocate a memory buffer that can hold an instance of T.
   *
   * This method only allocates memory and does not construct the instance.
   */
  template<typename T> T *allocate()
  {
    return static_cast<T *>(this->allocate(sizeof(T), alignof(T)));
  }

  /**
   * Allocate a memory buffer that can hold T array with the given size.
   *
   * This method only allocates memory and does not construct the instance.
   */
  template<typename T> MutableSpan<T> allocate_array(int64_t size)
  {
    T *array = static_cast<T *>(this->allocate(sizeof(T) * size, alignof(T)));
    return MutableSpan<T>(array, size);
  }

  /**
   * Construct an instance of T in memory provided by this allocator.
   *
   * Arguments passed to this method will be forwarded to the constructor of T.
   *
   * You must not call `delete` on the returned value.
   * Instead, only the destructor has to be called.
   */
  template<typename T, typename... Args> destruct_ptr<T> construct(Args &&...args)
  {
    void *buffer = this->allocate(sizeof(T), alignof(T));
    T *value = new (buffer) T(std::forward<Args>(args)...);
    return destruct_ptr<T>(value);
  }

  /**
   * Construct multiple instances of a type in an array. The constructor of is called with the
   * given arguments. The caller is responsible for calling the destructor (and not `delete`) on
   * the constructed elements.
   */
  template<typename T, typename... Args>
  MutableSpan<T> construct_array(int64_t size, Args &&...args)
  {
    MutableSpan<T> array = this->allocate_array<T>(size);
    for (const int64_t i : IndexRange(size)) {
      new (&array[i]) T(std::forward<Args>(args)...);
    }
    return array;
  }

  /**
   * Copy the given array into a memory buffer provided by this allocator.
   */
  template<typename T> MutableSpan<T> construct_array_copy(Span<T> src)
  {
    if (src.is_empty()) {
      return {};
    }
    MutableSpan<T> dst = this->allocate_array<T>(src.size());
    uninitialized_copy_n(src.data(), src.size(), dst.data());
    return dst;
  }

  /**
   * Copy the given string into a memory buffer provided by this allocator. The returned string is
   * always null terminated.
   */
  StringRefNull copy_string(StringRef str)
  {
    const int64_t alloc_size = str.size() + 1;
    char *buffer = static_cast<char *>(this->allocate(alloc_size, 1));
    str.copy(buffer, alloc_size);
    return StringRefNull(static_cast<const char *>(buffer));
  }

  MutableSpan<void *> allocate_elements_and_pointer_array(int64_t element_amount,
                                                          int64_t element_size,
                                                          int64_t element_alignment)
  {
    void *pointer_buffer = this->allocate(element_amount * sizeof(void *), alignof(void *));
    void *elements_buffer = this->allocate(element_amount * element_size, element_alignment);

    MutableSpan<void *> pointers((void **)pointer_buffer, element_amount);
    void *next_element_buffer = elements_buffer;
    for (int64_t i : IndexRange(element_amount)) {
      pointers[i] = next_element_buffer;
      next_element_buffer = POINTER_OFFSET(next_element_buffer, element_size);
    }

    return pointers;
  }

  template<typename T, typename... Args>
  Span<T *> construct_elements_and_pointer_array(int64_t n, Args &&...args)
  {
    MutableSpan<void *> void_pointers = this->allocate_elements_and_pointer_array(
        n, sizeof(T), alignof(T));
    MutableSpan<T *> pointers = void_pointers.cast<T *>();

    for (int64_t i : IndexRange(n)) {
      new (static_cast<void *>(pointers[i])) T(std::forward<Args>(args)...);
    }

    return pointers;
  }

  /**
   * Tell the allocator to use up the given memory buffer, before allocating new memory from the
   * system.
   */
  void provide_buffer(void *buffer, uint size)
  {
    unused_borrowed_buffers_.append(Span<char>(static_cast<char *>(buffer), size));
  }

  template<size_t Size, size_t Alignment>
  void provide_buffer(AlignedBuffer<Size, Alignment> &aligned_buffer)
  {
    this->provide_buffer(aligned_buffer.ptr(), Size);
  }

 private:
  void allocate_new_buffer(int64_t min_allocation_size, int64_t min_alignment)
  {
    for (int64_t i : unused_borrowed_buffers_.index_range()) {
      Span<char> buffer = unused_borrowed_buffers_[i];
      if (buffer.size() >= min_allocation_size) {
        unused_borrowed_buffers_.remove_and_reorder(i);
        current_begin_ = (uintptr_t)buffer.begin();
        current_end_ = (uintptr_t)buffer.end();
        return;
      }
    }

    /* Possibly allocate more bytes than necessary for the current allocation. This way more small
     * allocations can be packed together. Large buffers are allocated exactly to avoid wasting too
     * much memory. */
    int64_t size_in_bytes = min_allocation_size;
    if (size_in_bytes <= large_buffer_threshold) {
      /* Gradually grow buffer size with each allocation, up to a maximum. */
      const int grow_size = 1 << std::min<int>(owned_buffers_.size() + 6, 20);
      size_in_bytes = std::min(large_buffer_threshold,
                               std::max<int64_t>(size_in_bytes, grow_size));
    }

    void *buffer = allocator_.allocate(size_in_bytes, min_alignment, __func__);
    owned_buffers_.append(buffer);
    current_begin_ = (uintptr_t)buffer;
    current_end_ = current_begin_ + size_in_bytes;
  }

  void *allocator_large_buffer(const int64_t size, const int64_t alignment)
  {
    void *buffer = allocator_.allocate(size, alignment, __func__);
    owned_buffers_.append(buffer);
    return buffer;
  }
};

}  // namespace blender
