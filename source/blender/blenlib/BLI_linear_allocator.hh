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

#ifndef __BLI_LINEAR_ALLOCATOR_HH__
#define __BLI_LINEAR_ALLOCATOR_HH__

#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

namespace BLI {

template<typename Allocator = GuardedAllocator> class LinearAllocator : NonCopyable, NonMovable {
 private:
  Allocator m_allocator;
  Vector<void *> m_owned_buffers;
  Vector<ArrayRef<char>> m_unused_borrowed_buffers;

  uintptr_t m_current_begin;
  uintptr_t m_current_end;
  uint m_next_min_alloc_size;

#ifdef DEBUG
  uint m_debug_allocated_amount = 0;
#endif

 public:
  LinearAllocator()
  {
    m_current_begin = 0;
    m_current_end = 0;
    m_next_min_alloc_size = 64;
  }

  ~LinearAllocator()
  {
    for (void *ptr : m_owned_buffers) {
      m_allocator.deallocate(ptr);
    }
  }

  void provide_buffer(void *buffer, uint size)
  {
    m_unused_borrowed_buffers.append(ArrayRef<char>((char *)buffer, size));
  }

  template<uint Size, uint Alignment>
  void provide_buffer(AlignedBuffer<Size, Alignment> &aligned_buffer)
  {
    this->provide_buffer(aligned_buffer.ptr(), Size);
  }

  template<typename T> T *allocate()
  {
    return (T *)this->allocate(sizeof(T), alignof(T));
  }

  template<typename T> MutableArrayRef<T> allocate_array(uint length)
  {
    return MutableArrayRef<T>((T *)this->allocate(sizeof(T) * length), length);
  }

  void *allocate(uint size, uint alignment = 4)
  {
    BLI_assert(alignment >= 1);
    BLI_assert(is_power_of_2_i(alignment));

#ifdef DEBUG
    m_debug_allocated_amount += size;
#endif

    uintptr_t alignment_mask = alignment - 1;
    uintptr_t potential_allocation_begin = (m_current_begin + alignment_mask) & ~alignment_mask;
    uintptr_t potential_allocation_end = potential_allocation_begin + size;

    if (potential_allocation_end <= m_current_end) {
      m_current_begin = potential_allocation_end;
      return (void *)potential_allocation_begin;
    }
    else {
      this->allocate_new_buffer(size + alignment);
      return this->allocate(size, alignment);
    }
  };

  StringRefNull copy_string(StringRef str)
  {
    uint alloc_size = str.size() + 1;
    char *buffer = (char *)this->allocate(alloc_size, 1);
    str.copy(buffer, alloc_size);
    return StringRefNull((const char *)buffer);
  }

  template<typename T, typename... Args> T *construct(Args &&... args)
  {
    void *buffer = this->allocate(sizeof(T), alignof(T));
    T *value = new (buffer) T(std::forward<Args>(args)...);
    return value;
  }

  template<typename T, typename... Args>
  ArrayRef<T *> construct_elements_and_pointer_array(uint n, Args &&... args)
  {
    void *pointer_buffer = this->allocate(n * sizeof(T *), alignof(T *));
    void *element_buffer = this->allocate(n * sizeof(T), alignof(T));

    MutableArrayRef<T *> pointers((T **)pointer_buffer, n);
    T *elements = (T *)element_buffer;

    for (uint i : IndexRange(n)) {
      pointers[i] = elements + i;
    }
    for (uint i : IndexRange(n)) {
      new (elements + i) T(std::forward<Args>(args)...);
    }

    return pointers;
  }

  template<typename T> MutableArrayRef<T> construct_array_copy(ArrayRef<T> source)
  {
    T *buffer = (T *)this->allocate(source.byte_size(), alignof(T));
    uninitialized_copy_n(source.begin(), source.size(), buffer);
    return MutableArrayRef<T>(buffer, source.size());
  }

 private:
  void allocate_new_buffer(uint min_allocation_size)
  {
    for (uint i : m_unused_borrowed_buffers.index_range()) {
      ArrayRef<char> buffer = m_unused_borrowed_buffers[i];
      if (buffer.size() >= min_allocation_size) {
        m_unused_borrowed_buffers.remove_and_reorder(i);
        m_current_begin = (uintptr_t)buffer.begin();
        m_current_end = (uintptr_t)buffer.end();
        return;
      }
    }

    uint size_in_bytes = power_of_2_min_u(std::max(min_allocation_size, m_next_min_alloc_size));
    m_next_min_alloc_size = size_in_bytes * 2;

    void *buffer = m_allocator.allocate(size_in_bytes, __func__);
    m_owned_buffers.append(buffer);
    m_current_begin = (uintptr_t)buffer;
    m_current_end = m_current_begin + size_in_bytes;
  }
};

}  // namespace BLI

#endif /* __BLI_LINEAR_ALLOCATOR_HH__ */
