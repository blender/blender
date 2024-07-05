/* SPDX-FileCopyrightText: 2002-2017 `Jason Evans <jasone@canonware.com>`. All rights reserved.
 * SPDX-FileCopyrightText: 2007-2012 Mozilla Foundation. All rights reserved.
 * SPDX-FileCopyrightText: 2009-2017 Facebook, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause */
#pragma once

/** \file
 * \ingroup intern_mem
 */

#include <cstdlib>

/* BEGIN copied from BLI_asan.h */

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

/* END copied from BLI_asan.h */

MEM_INLINE bool MEM_size_safe_multiply(size_t a, size_t b, size_t *result)
{
  /* A size_t with its high-half bits all set to 1. */
  const size_t high_bits = SIZE_MAX << (sizeof(size_t) * 8 / 2);
  *result = a * b;

  if (UNLIKELY(*result == 0)) {
    return (a == 0 || b == 0);
  }

  /*
   * We got a non-zero size, but we don't know if we overflowed to get
   * there.  To avoid having to do a divide, we'll be clever and note that
   * if both A and B can be represented in N/2 bits, then their product
   * can be represented in N bits (without the possibility of overflow).
   */
  return ((high_bits & (a | b)) == 0 || (*result / b == a));
}

/**
 * Util to trigger an error for the given memory block.
 *
 * When ASAN is available, it will poison the memory block before accessing it, to trigger a
 * detailed ASAN report. Otherwise, it will abort if aborting on assert is set.
 */
#ifdef WITH_ASAN
MEM_INLINE void MEM_trigger_error_on_memory_block(const void *address, const size_t size)
{
  if (address == nullptr) {
#  ifdef WITH_ASSERT_ABORT
    abort();
#  endif
    return;
  }

  /* Trigger ASAN error by poisoning the memory and accessing it. */
  ASAN_POISON_MEMORY_REGION(address, size);
  char *buffer = const_cast<char *>(static_cast<const char *>(address));
  const char c = *buffer;
  *buffer &= 255;
  *buffer = c;

  /* In case ASAN is set to not terminate on error, but abort on assert is requested. */
#  ifdef WITH_ASSERT_ABORT
  abort();
#  endif
  ASAN_UNPOISON_MEMORY_REGION(address, size);
}
#else
MEM_INLINE void MEM_trigger_error_on_memory_block(const void * /* address */,
                                                  const size_t /* size */)
{
#  ifdef WITH_ASSERT_ABORT
  abort();
#  endif
}
#endif
