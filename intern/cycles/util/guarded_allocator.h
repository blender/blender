/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cstddef>
#include <cstdlib>

#ifdef WITH_BLENDER_GUARDEDALLOC
#  include "../../guardedalloc/MEM_guardedalloc.h"
#endif

CCL_NAMESPACE_BEGIN

/* Internal use only. */
void util_guarded_mem_alloc(const size_t n);
void util_guarded_mem_free(const size_t n);

/* Guarded allocator for the use with STL. */
template<typename T> class GuardedAllocator {
 public:
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using value_type = T;

  GuardedAllocator() = default;
  GuardedAllocator(const GuardedAllocator & /*unused*/) = default;

  T *allocate(const size_t n, const void *hint = nullptr)
  {
    (void)hint;
    size_t size = n * sizeof(T);
    util_guarded_mem_alloc(size);
    if (n == 0) {
      return nullptr;
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
    if (mem == nullptr) {
      throw std::bad_alloc();
    }
    return mem;
  }

  void deallocate(T *p, const size_t n)
  {
    util_guarded_mem_free(n * sizeof(T));
    if (p != nullptr) {
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

  GuardedAllocator<T> &operator=(const GuardedAllocator & /*unused*/) = default;

  size_t max_size() const
  {
    return size_t(-1);
  }

  template<class U> struct rebind {
    using other = GuardedAllocator<U>;
  };

  template<class U> GuardedAllocator(const GuardedAllocator<U> & /*unused*/) {}

  template<class U> GuardedAllocator &operator=(const GuardedAllocator<U> & /*unused*/)
  {
    return *this;
  }

  bool operator==(const GuardedAllocator & /*other*/) const
  {
    return true;
  }
  bool operator!=(const GuardedAllocator &other) const
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
