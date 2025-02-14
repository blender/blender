/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cstddef>

CCL_NAMESPACE_BEGIN

/* Minimum alignment needed by all CPU native data types (SSE, AVX). */
#define MIN_ALIGNMENT_CPU_DATA_TYPES 16  // NOLINT

/* Allocate block of size bytes at least aligned to a given value. */
void *util_aligned_malloc(const size_t size, const int alignment);

/* Free memory allocated by util_aligned_malloc. */
void util_aligned_free(void *ptr, const size_t size);

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
    util_aligned_free(t, sizeof(T));
  }
}

CCL_NAMESPACE_END
