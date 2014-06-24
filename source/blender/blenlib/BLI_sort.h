/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Benoit Bolsee,
 *                 Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_SORT_H__
#define __BLI_SORT_H__

/** \file BLI_sort.h
 *  \ingroup bli
 */

#include <stdlib.h>

#ifdef __GLIBC__
#  define BLI_qsort_r qsort_r
#endif

/* Quick sort reentrant */
typedef int (*BLI_sort_cmp_t)(const void *a, const void *b, void *ctx);

void BLI_qsort_r(void *a, size_t n, size_t es, BLI_sort_cmp_t cmp, void *thunk)
#ifdef __GNUC__
__attribute__((nonnull(1, 5)))
#endif
;

#endif  /* __BLI_SORT_H__ */
