/* SPDX-FileCopyrightText: 2006-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_memutil
 */

#ifndef __MEM_ALLOCATOR_H__
#define __MEM_ALLOCATOR_H__

#include "guardedalloc/MEM_guardedalloc.h"
#include <stddef.h>

template<typename _Tp> struct MEM_Allocator {
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  typedef _Tp *pointer;
  typedef const _Tp *const_pointer;
  typedef _Tp &reference;
  typedef const _Tp &const_reference;
  typedef _Tp value_type;

  template<typename _Tp1> struct rebind {
    typedef MEM_Allocator<_Tp1> other;
  };

  MEM_Allocator() throw() {}
  MEM_Allocator(const MEM_Allocator &) throw() {}

  template<typename _Tp1> MEM_Allocator(const MEM_Allocator<_Tp1>) throw() {}

  ~MEM_Allocator() throw() {}

  pointer address(reference __x) const
  {
    return &__x;
  }

  const_pointer address(const_reference __x) const
  {
    return &__x;
  }

  /* NOTE: `__n` is permitted to be 0.
   * The C++ standard says nothing about what the return value is when `__n == 0`. */
  _Tp *allocate(size_type __n, const void * = 0)
  {
    _Tp *__ret = NULL;
    if (__n)
      __ret = static_cast<_Tp *>(MEM_mallocN(__n * sizeof(_Tp), "STL MEM_Allocator"));
    return __ret;
  }

  // __p is not permitted to be a null pointer.
  void deallocate(pointer __p, size_type)
  {
    MEM_freeN(__p);
  }

  size_type max_size() const throw()
  {
    return size_t(-1) / sizeof(_Tp);
  }

  void construct(pointer __p, const _Tp &__val)
  {
    new (__p) _Tp(__val);
  }

  void destroy(pointer __p)
  {
    __p->~_Tp();
  }
};

#endif  // __MEM_ALLOCATOR_H__
