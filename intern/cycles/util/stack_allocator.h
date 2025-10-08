/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cstddef>

#include "util/defines.h"
#include "util/guarded_allocator.h"

CCL_NAMESPACE_BEGIN

/* Stack allocator for the use with STL. */
template<int SIZE, typename T> class ccl_try_align(16) StackAllocator {
 public:
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using value_type = T;

  /* Allocator construction/destruction. */

  StackAllocator() : pointer_(0), use_stack_(true) {}

  StackAllocator(const StackAllocator & /*unused*/) : pointer_(0), use_stack_(true) {}

  template<class U>
  StackAllocator(const StackAllocator<SIZE, U> & /*unused*/) : pointer_(0), use_stack_(false)
  {
  }

  /* Memory allocation/deallocation. */

  T *allocate(const size_t n, const void *hint = nullptr)
  {
    (void)hint;
    if (n == 0) {
      return nullptr;
    }
    if (pointer_ + n >= SIZE || use_stack_ == false) {
      size_t size = n * sizeof(T);
      util_guarded_mem_alloc(size);
      T *mem;
#ifdef WITH_BLENDER_GUARDEDALLOC
      mem = (T *)MEM_mallocN_aligned(size, 16, "Cycles Alloc");
#else
      mem = (T *)malloc(size);
#endif
      if (mem == nullptr) {
        throw std::bad_alloc();
      }
      return mem;
    }
    T *mem = &data_[pointer_];
    pointer_ += n;
    return mem;
  }

  void deallocate(T *p, const size_t n)
  {
    if (p == nullptr) {
      return;
    }
    if (p < data_ || p >= data_ + SIZE) {
      util_guarded_mem_free(n * sizeof(T));
#ifdef WITH_BLENDER_GUARDEDALLOC
      MEM_freeN(static_cast<void *>(p));
#else
      free(p);
#endif
      return;
    }
    /* We don't support memory free for the stack allocator. */
  }

  /* Address of an reference. */

  T *address(T &x) const
  {
    return &x;
  }

  const T *address(const T &x) const
  {
    return &x;
  }

  /* Object construction/destruction. */

  void construct(T *p, const T &val)
  {
    if (p != nullptr) {
      new (p) T(val);
    }
  }

  void destroy(T *p)
  {
    p->~T();
  }

  /* Maximum allocation size. */

  size_t max_size() const
  {
    return size_t(-1);
  }

  /* Rebind to other type of allocator. */

  template<class U> struct rebind {
    using other = StackAllocator<SIZE, U>;
  };

  /* Operators */

  template<class U> StackAllocator &operator=(const StackAllocator<SIZE, U> & /*unused*/)
  {
    return *this;
  }

  StackAllocator<SIZE, T> &operator=(const StackAllocator & /*unused*/)
  {
    return *this;
  }

  bool operator==(const StackAllocator & /*other*/) const
  {
    return true;
  }

  bool operator!=(const StackAllocator &other) const
  {
    return !operator==(other);
  }

 private:
  int pointer_;
  bool use_stack_;
  T data_[SIZE];
};

CCL_NAMESPACE_END
