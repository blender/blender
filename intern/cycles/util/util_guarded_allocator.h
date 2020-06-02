/*
 * Copyright 2011-2015 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_GUARDED_ALLOCATOR_H__
#define __UTIL_GUARDED_ALLOCATOR_H__

#include <cstddef>
#include <cstdlib>
#include <memory>

#ifdef WITH_BLENDER_GUARDEDALLOC
#  include "../../guardedalloc/MEM_guardedalloc.h"
#endif

CCL_NAMESPACE_BEGIN

/* Internal use only. */
void util_guarded_mem_alloc(size_t n);
void util_guarded_mem_free(size_t n);

/* Guarded allocator for the use with STL. */
template<typename T> class GuardedAllocator {
 public:
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  typedef T *pointer;
  typedef const T *const_pointer;
  typedef T &reference;
  typedef const T &const_reference;
  typedef T value_type;

  GuardedAllocator()
  {
  }
  GuardedAllocator(const GuardedAllocator &)
  {
  }

  T *allocate(size_t n, const void *hint = 0)
  {
    (void)hint;
    size_t size = n * sizeof(T);
    util_guarded_mem_alloc(size);
    if (n == 0) {
      return NULL;
    }
    T *mem;
#ifdef WITH_BLENDER_GUARDEDALLOC
    /* C++ standard requires allocation functions to allocate memory suitably
     * aligned for any standard type. This is 16 bytes for 64 bit platform as
     * far as i concerned. We might over-align on 32bit here, but that should
     * be all safe actually.
     */
    mem = (T *)MEM_mallocN_aligned(size, 16, "Cycles Alloc");
#else
    mem = (T *)malloc(size);
#endif
    if (mem == NULL) {
      throw std::bad_alloc();
    }
    return mem;
  }

  void deallocate(T *p, size_t n)
  {
    util_guarded_mem_free(n * sizeof(T));
    if (p != NULL) {
#ifdef WITH_BLENDER_GUARDEDALLOC
      MEM_freeN(p);
#else
      free(p);
#endif
    }
  }

  T *address(T &x) const
  {
    return &x;
  }

  const T *address(const T &x) const
  {
    return &x;
  }

  GuardedAllocator<T> &operator=(const GuardedAllocator &)
  {
    return *this;
  }

  size_t max_size() const
  {
    return size_t(-1);
  }

  template<class U> struct rebind {
    typedef GuardedAllocator<U> other;
  };

  template<class U> GuardedAllocator(const GuardedAllocator<U> &)
  {
  }

  template<class U> GuardedAllocator &operator=(const GuardedAllocator<U> &)
  {
    return *this;
  }

  inline bool operator==(GuardedAllocator const & /*other*/) const
  {
    return true;
  }
  inline bool operator!=(GuardedAllocator const &other) const
  {
    return !operator==(other);
  }

#ifdef _MSC_VER
  /* Welcome to the black magic here.
   *
   * The issue is that MSVC C++ allocates container proxy on any
   * vector initialization, including static vectors which don't
   * have any data yet. This leads to several issues:
   *
   * - Static objects initialization fiasco (global_stats from
   *   util_stats.h might not be initialized yet).
   * - If main() function changes allocator type (for example,
   *   this might happen with `blender --debug-memory`) nobody
   *   will know how to convert already allocated memory to a new
   *   guarded allocator.
   *
   * Here we work this around by making it so container proxy does
   * not use guarded allocation. A bit fragile, unfortunately.
   */
  template<> struct rebind<std::_Container_proxy> {
    typedef std::allocator<std::_Container_proxy> other;
  };

  operator std::allocator<std::_Container_proxy>() const
  {
    return std::allocator<std::_Container_proxy>();
  }
#endif
};

/* Get memory usage and peak from the guarded STL allocator. */
size_t util_guarded_get_mem_used();
size_t util_guarded_get_mem_peak();

/* Call given function and keep track if it runs out of memory.
 *
 * If it does run out f memory, stop execution and set progress
 * to do a global cancel.
 *
 * It's not fully robust, but good enough to catch obvious issues
 * when running out of memory.
 */
#define MEM_GUARDED_CALL(progress, func, ...) \
  do { \
    try { \
      (func)(__VA_ARGS__); \
    } \
    catch (std::bad_alloc &) { \
      fprintf(stderr, "Error: run out of memory!\n"); \
      fflush(stderr); \
      (progress)->set_error("Out of memory"); \
    } \
  } while (false)

CCL_NAMESPACE_END

#endif /* __UTIL_GUARDED_ALLOCATOR_H__ */
