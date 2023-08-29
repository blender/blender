/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_STACK_ALLOCATOR_H__
#define __UTIL_STACK_ALLOCATOR_H__

#include <cstddef>
#include <memory>

CCL_NAMESPACE_BEGIN

/* Stack allocator for the use with STL. */
template<int SIZE, typename T> class ccl_try_align(16) StackAllocator
{
 public:
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  typedef T *pointer;
  typedef const T *const_pointer;
  typedef T &reference;
  typedef const T &const_reference;
  typedef T value_type;

  /* Allocator construction/destruction. */

  StackAllocator() : pointer_(0), use_stack_(true) {}

  StackAllocator(const StackAllocator &) : pointer_(0), use_stack_(true) {}

  template<class U>
  StackAllocator(const StackAllocator<SIZE, U> &) : pointer_(0), use_stack_(false)
  {
  }

  /* Memory allocation/deallocation. */

  T *allocate(size_t n, const void *hint = 0)
  {
    (void)hint;
    if (n == 0) {
      return NULL;
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
      if (mem == NULL) {
        throw std::bad_alloc();
      }
      return mem;
    }
    T *mem = &data_[pointer_];
    pointer_ += n;
    return mem;
  }

  void deallocate(T * p, size_t n)
  {
    if (p == NULL) {
      return;
    }
    if (p < data_ || p >= data_ + SIZE) {
      util_guarded_mem_free(n * sizeof(T));
#ifdef WITH_BLENDER_GUARDEDALLOC
      MEM_freeN(p);
#else
      free(p);
#endif
      return;
    }
    /* We don't support memory free for the stack allocator. */
  }

  /* Address of an reference. */

  T *address(T & x) const
  {
    return &x;
  }

  const T *address(const T &x) const
  {
    return &x;
  }

  /* Object construction/destruction. */

  void construct(T * p, const T &val)
  {
    if (p != NULL) {
      new ((T *)p) T(val);
    }
  }

  void destroy(T * p)
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
    typedef StackAllocator<SIZE, U> other;
  };

  /* Operators */

  template<class U> inline StackAllocator &operator=(const StackAllocator<SIZE, U> &)
  {
    return *this;
  }

  StackAllocator<SIZE, T> &operator=(const StackAllocator &)
  {
    return *this;
  }

  inline bool operator==(StackAllocator const & /*other*/) const
  {
    return true;
  }

  inline bool operator!=(StackAllocator const &other) const
  {
    return !operator==(other);
  }

 private:
  int pointer_;
  bool use_stack_;
  T data_[SIZE];
};

CCL_NAMESPACE_END

#endif /* __UTIL_STACK_ALLOCATOR_H__ */
