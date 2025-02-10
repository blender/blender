/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "util/aligned_malloc.h"
#include "util/guarded_allocator.h"

#ifdef WITH_BLENDER_GUARDEDALLOC
#  include "../../guardedalloc/MEM_guardedalloc.h"
#endif

#include <cassert>

/* Adopted from Libmv. */

#if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__)
/* Needed for memalign on Linux and _aligned_alloc on Windows. */
#  ifdef FREE_WINDOWS
/* Make sure _aligned_malloc is included. */
#    ifdef __MSVCRT_VERSION__
#      undef __MSVCRT_VERSION__
#    endif
#    define __MSVCRT_VERSION__ 0x0700
#  endif /* FREE_WINDOWS */
#  include <malloc.h>
#else
/* Apple's `malloc` is 16-byte aligned, and does not have `malloc.h`, so include
 * `stdilb` instead.
 */
#  include <cstdlib>
#endif

CCL_NAMESPACE_BEGIN

void *util_aligned_malloc(const size_t size, const int alignment)
{
  void *mem = nullptr;
#ifdef WITH_BLENDER_GUARDEDALLOC
  mem = MEM_mallocN_aligned(size, alignment, "Cycles Aligned Alloc");
#elif defined(_WIN32)
  mem = _aligned_malloc(size, alignment);
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
  if (posix_memalign(&mem, alignment, size)) {
    /* Non-zero means allocation error
     * either no allocation or bad alignment value. */
    mem = nullptr;
  }
#else /* This is for Linux. */
  mem = memalign(alignment, size);
#endif
  if (mem) {
    util_guarded_mem_alloc(size);
  }
  return mem;
}

void util_aligned_free(void *ptr, const size_t size)
{
  if (ptr) {
    util_guarded_mem_free(size);
  }
#if defined(WITH_BLENDER_GUARDEDALLOC)
  if (ptr != nullptr) {
    MEM_freeN(ptr);
  }
#elif defined(_WIN32)
  _aligned_free(ptr);
#else
  free(ptr);
#endif
}

CCL_NAMESPACE_END
