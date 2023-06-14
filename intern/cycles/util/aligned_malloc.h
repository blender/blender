/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_ALIGNED_MALLOC_H__
#define __UTIL_ALIGNED_MALLOC_H__

#include "util/types.h"

CCL_NAMESPACE_BEGIN

/* Minimum alignment needed by all CPU native data types (SSE, AVX). */
#define MIN_ALIGNMENT_CPU_DATA_TYPES 16

/* Allocate block of size bytes at least aligned to a given value. */
void *util_aligned_malloc(size_t size, int alignment);

/* Free memory allocated by util_aligned_malloc. */
void util_aligned_free(void *ptr);

/* Aligned new operator. */
template<typename T, typename... Args> T *util_aligned_new(Args... args)
{
  void *mem = util_aligned_malloc(sizeof(T), alignof(T));
  return new (mem) T(args...);
}

template<typename T> void util_aligned_delete(T *t)
{
  if (t) {
    t->~T();
    util_aligned_free(t);
  }
}

CCL_NAMESPACE_END

#endif /* __UTIL_ALIGNED_MALLOC_H__ */
