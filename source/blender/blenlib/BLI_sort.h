/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Quick sort (re-entrant). */
typedef int (*BLI_sort_cmp_t)(const void *a, const void *b, void *ctx);

void BLI_qsort_r(void *a, size_t n, size_t es, BLI_sort_cmp_t cmp, void *thunk)
#ifdef __GNUC__
    __attribute__((nonnull(1, 5)))
#endif
    ;

#ifdef __cplusplus
}
#endif
