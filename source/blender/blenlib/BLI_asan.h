/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* Clang defines this. */
#ifndef __has_feature
#  define __has_feature(x) 0
#endif

#if (defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)) && \
    (!defined(_MSC_VER) || _MSC_VER > 1929) /* MSVC 2019 and below doesn't ship ASAN headers. */
#  include "sanitizer/asan_interface.h"
#  define WITH_ASAN
#else
/* Ensure return value is used. Just using UNUSED_VARS results in a warning. */
#  define ASAN_POISON_MEMORY_REGION(addr, size) (void)(0 && ((size) != 0 && (addr) != NULL))
#  define ASAN_UNPOISON_MEMORY_REGION(addr, size) (void)(0 && ((size) != 0 && (addr) != NULL))
#endif

/**
 * Mark a region of memory as "freed". When using address sanitizer, accessing the given memory
 * region will cause an use-after-poison error. This can be used to find errors when dealing with
 * uninitialized memory in custom containers.
 */
#define BLI_asan_poison(addr, size) ASAN_POISON_MEMORY_REGION(addr, size)

/**
 * Mark a region of memory as usable again.
 */
#define BLI_asan_unpoison(addr, size) ASAN_UNPOISON_MEMORY_REGION(addr, size)

#if (defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer))
#  include "MEM_guardedalloc.h"

static void *BLI_asan_safe_malloc(size_t size, const char *tag)
{
  // align size at 16 bytes
  size += 15 - (size & 15);

  // add safe padding
  size += 32;

  void *ret = MEM_mallocN(size, tag);

  int *iptr = (int *)ret;
  *iptr = (int)size;

  char *ptr = (char *)ret;

  ptr[4] = 't';
  ptr[5] = 'a';
  ptr[6] = 'g';
  ptr[7] = '1';

  BLI_asan_poison(ptr, 16);
  BLI_asan_poison(ptr + size - 16, 16);

  ret = (void *)(ptr + 16);

  return ret;
}

static void BLI_asan_safe_free(void *mem)
{
  if (!mem) {
    return;
  }

  mem = (void *)(((char *)mem) - 16);

  BLI_asan_unpoison(mem, 16);
  int *iptr = (int *)mem;
  volatile char *ptr = (char *)mem;

  if (ptr[4] != 't' || ptr[5] != 'a' || ptr[6] != 'g' || ptr[7] != '1') {
    BLI_asan_poison(mem, 16);
    *ptr = 1;  // deliberately trigger asan fault
  }

  BLI_asan_unpoison(ptr + iptr[0] - 16, 16);
  MEM_freeN((void *)ptr);
}
#else
#  define BLI_asan_safe_malloc(size, tag) MEM_mallocN(size, tag)
#  define BLI_asan_safe_free(mem) MEM_SAFE_FREE(mem)
#endif
