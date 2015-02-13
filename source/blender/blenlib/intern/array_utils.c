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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/array_utils.c
 *  \ingroup bli
 *  \brief Generic array manipulation API.
 */
#include <string.h>
#include <stdlib.h>

#include "BLI_array_utils.h"

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"
#include "BLI_alloca.h"


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
