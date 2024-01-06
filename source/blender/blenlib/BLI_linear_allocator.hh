/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

/**
 * If enabled, #LinearAllocator keeps track of how much memory it owns and how much it has
 * allocated.
 */
// #define BLI_DEBUG_LINEAR_ALLOCATOR_SIZE

template<typename Allocator = GuardedAllocator> class LinearAllocator : NonCopyable, NonMovable {
 private:
  BLI_NO_UNIQUE_ADDRESS Allocator allocator_;
  Vector<void *, 2> owned_buffers_;

  uintptr_t current_begin_;
  uintptr_t current_end_;

  /* Buffers larger than that are not packed together with smaller allocations to avoid wasting
   * memory. */
  constexpr static inline int64_t large_buffer_threshold = 4096;

 public:
#ifdef BLI_DEBUG_LINEAR_ALLOCATOR_SIZE
  int64_t user_requested_size_ = 0;
  int64_t owned_allocation_size_ = 0;
#endif

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
    BLI_assert(is_power_of_2(alignment));

    const uintptr_t alignment_mask = alignment - 1;
    const uintptr_t potential_allocation_begin = (current_begin_ + alignment_mask) &
                                                 ~alignment_mask;
    const uintptr_t potential_allocation_end = potential_allocation_begin + size;

    if (potential_allocation_end <= current_end_) {
#ifdef BLI_DEBUG_LINEAR_ALLOCATOR_SIZE
      user_requested_size_ += size;
#endif
      current_begin_ = potential_allocation_end;
      return reinterpret_cast<void *>(potential_allocation_begin);
    }
    if (size <= large_buffer_threshold) {
      this->allocate_new_buffer(size + alignment, alignment);
      return this->allocate(size, alignment);
    }
#ifdef BLI_DEBUG_LINEAR_ALLOCATOR_SIZE
    user_requested_size_ += size;
#endif
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

    MutableSpan<void *> pointers(static_cast<void **>(pointer_buffer), element_amount);
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
  void provide_buffer(void *buffer, const int64_t size)
  {
    BLI_assert(owned_buffers_.is_empty());
    current_begin_ = uintptr_t(buffer);
    current_end_ = current_begin_ + size;
  }

  template<size_t Size, size_t Alignment>
  void provide_buffer(AlignedBuffer<Size, Alignment> &aligned_buffer)
  {
    this->provide_buffer(aligned_buffer.ptr(), Size);
  }

  /**
   * This allocator takes ownership of the buffers owned by `other`. Therefor, when `other` is
   * destructed, memory allocated using it is not freed.
   *
   * Note that the caller is responsible for making sure that buffers passed into #provide_buffer
   * of `other` live at least as long as this allocator.
   */
  void transfer_ownership_from(LinearAllocator<> &other)
  {
    owned_buffers_.extend(other.owned_buffers_);
#ifdef BLI_DEBUG_LINEAR_ALLOCATOR_SIZE
    user_requested_size_ += other.user_requested_size_;
    owned_allocation_size_ += other.owned_allocation_size_;
#endif
    other.owned_buffers_.clear();
    std::destroy_at(&other);
    new (&other) LinearAllocator<>();
  }

 private:
  void allocate_new_buffer(int64_t min_allocation_size, int64_t min_alignment)
  {
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

    void *buffer = this->allocated_owned(size_in_bytes, min_alignment);
    current_begin_ = uintptr_t(buffer);
    current_end_ = current_begin_ + size_in_bytes;
  }

  void *allocator_large_buffer(const int64_t size, const int64_t alignment)
  {
    return this->allocated_owned(size, alignment);
  }

  void *allocated_owned(const int64_t size, const int64_t alignment)
  {
    void *buffer = allocator_.allocate(size, alignment, __func__);
    owned_buffers_.append(buffer);
#ifdef BLI_DEBUG_LINEAR_ALLOCATOR_SIZE
    owned_allocation_size_ += size;
#endif
    return buffer;
  }
};

}  // namespace blender
