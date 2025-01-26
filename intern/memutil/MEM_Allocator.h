/* SPDX-FileCopyrightText: 2006-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_memutil
 */

#ifndef __MEM_ALLOCATOR_H__
#define __MEM_ALLOCATOR_H__

#include "guardedalloc/MEM_guardedalloc.h"
#include <cstddef>

template<typename _Tp> struct MEM_Allocator {
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = _Tp *;
  using const_pointer = const _Tp *;
  using reference = _Tp &;
  using const_reference = const _Tp &;
  using value_type = _Tp;

  template<typename _Tp1> struct rebind {
    using other = MEM_Allocator<_Tp1>;
  };

  MEM_Allocator() noexcept = default;
  MEM_Allocator(const MEM_Allocator & /*other*/) noexcept = default;

  template<typename _Tp1> MEM_Allocator(const MEM_Allocator<_Tp1> /*other*/) noexcept {}

  ~MEM_Allocator() noexcept = default;

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
  _Tp *allocate(size_type __n, const void * /*unused*/ = nullptr)
  {
    _Tp *__ret = NULL;
    if (__n) {
      __ret = static_cast<_Tp *>(MEM_mallocN(__n * sizeof(_Tp), "STL MEM_Allocator"));
    }
    return __ret;
  }

  // __p is not permitted to be a null pointer.
  void deallocate(pointer __p, size_type /*unused*/)
  {
    MEM_freeN(__p);
  }

  size_type max_size() const noexcept
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
