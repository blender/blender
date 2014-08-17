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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Eagar,
 *                 Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/BLI_array.c
 *  \ingroup bli
 *  \brief A (mainly) macro array library.
 *
 * This library needs to be changed to not use macros quite so heavily,
 * and to be more of a complete array API.  The way arrays are
 * exposed to client code as normal C arrays is very useful though, imho.
 * it does require some use of macros, however.
 *
 * anyway, it's used a bit too heavily to simply rewrite as a
 * more "correct" solution without macros entirely.  I originally wrote this
 * to be very easy to use, without the normal pain of most array libraries.
 * This was especially helpful when it came to the massive refactors necessary
 * for bmesh, and really helped to speed the process up. - joeedh
 *
 * little array macro library.  example of usage:
 *
 * int *arr = NULL;
 * BLI_array_declare(arr);
 * int i;
 *
 * for (i = 0; i < 10; i++) {
 *     BLI_array_grow_one(arr);
 *     arr[i] = something;
 * }
 * BLI_array_free(arr);
 *
 * arrays are buffered, using double-buffering (so on each reallocation,
 * the array size is doubled).  supposedly this should give good Big Oh
 * behavior, though it may not be the best in practice.
 */

#include <string.h>
#include <stdlib.h>

#include "BLI_array.h"

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"
#include "BLI_alloca.h"

#include "MEM_guardedalloc.h"

/**
 * This function is only to be called via macros.
 *
 * \note The caller must adjust \a arr_count
 */
void _bli_array_grow_func(void **arr_p, const void *arr_static,
                          const int sizeof_arr_p, const int arr_count, const int num,
                          const char *alloc_str)
{
	void *arr = *arr_p;
	void *arr_tmp;

	arr_tmp = MEM_mallocN(sizeof_arr_p *
	                      ((num < arr_count) ?
	                      (arr_count * 2 + 2) : (arr_count + num)), alloc_str);

	if (arr) {
		memcpy(arr_tmp, arr, sizeof_arr_p * arr_count);

		if (arr != arr_static) {
			MEM_freeN(arr);
		}
	}

	*arr_p = arr_tmp;

	/* caller must do */
#if 0
	arr_count += num;
#endif
}

void _bli_array_reverse(void *arr_v, unsigned int arr_len, size_t arr_stride)
{
	const unsigned int arr_half_stride = (arr_len / 2) * arr_stride;
	unsigned int i, i_end;
	char *arr = arr_v;
	char *buf = BLI_array_alloca(buf, arr_stride);

	for (i = 0, i_end = (arr_len - 1) * arr_stride;
	     i < arr_half_stride;
	     i += arr_stride, i_end -= arr_stride)
	{
		memcpy(buf, &arr[i], arr_stride);
		memcpy(&arr[i], &arr[i_end], arr_stride);
		memcpy(&arr[i_end], buf, arr_stride);
	}
}

void _bli_array_wrap(void *arr_v, unsigned int arr_len, size_t arr_stride, int dir)
{
	char *arr = arr_v;
	char *buf = BLI_array_alloca(buf, arr_stride);

	if (dir == -1) {
		memcpy(buf, arr, arr_stride);
		memmove(arr, arr + arr_stride, arr_stride * (arr_len - 1));
		memcpy(arr + (arr_stride * (arr_len - 1)), buf, arr_stride);
	}
	else if (dir == 1) {
		memcpy(buf, arr + (arr_stride * (arr_len - 1)), arr_stride);
		memmove(arr + arr_stride, arr, arr_stride * (arr_len - 1));
		memcpy(arr, buf, arr_stride);
	}
	else {
		BLI_assert(0);
	}
}

/**
 * \note Not efficient, use for error checks/asserts.
 */
int _bli_array_findindex(const void *arr, unsigned int arr_len, size_t arr_stride, const void *p)
{
	const char *arr_step = (const char *)arr;
	unsigned int i;
	for (i = 0; i < arr_len; i++, arr_step += arr_stride) {
		if (memcmp(arr_step, p, arr_stride) == 0) {
			return (int)i;
		}
	}
	return -1;
}
