/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <stdlib.h>

/* glibc 2.8+ */
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 8))
#  define BLI_qsort_r qsort_r
#endif

/** Quick sort (re-entrant). */
typedef int (*BLI_sort_cmp_t)(const void *a, const void *b, void *ctx);

void BLI_qsort_r(void *a, size_t n, size_t es, BLI_sort_cmp_t cmp, void *thunk)
#ifdef __GNUC__
    __attribute__((nonnull(1, 5)))
#endif
    ;
