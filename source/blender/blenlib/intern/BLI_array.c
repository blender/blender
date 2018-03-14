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
 * This is an array library, used to manage array (re)allocation.
 *
 * \note This is primarily accessed via macros,
 * functions are used to implement some of the internals.
 *
 * Example usage:
 *
 * \code{.c}
 * int *arr = NULL;
 * BLI_array_declare(arr);
 * int i;
 *
 * for (i = 0; i < 10; i++) {
 *     BLI_array_grow_one(arr);
 *     arr[i] = something;
 * }
 * BLI_array_free(arr);
 * \endcode
 *
 * Arrays are over allocated, so each reallocation the array size is doubled.
 * In situations where contiguous array access isn't needed,
 * other solutions for allocation are available.
 * Consider using on of: BLI_memarena.c, BLI_mempool.c, BLi_stack.c
 */

#include <string.h>

#include "BLI_array.h"

#include "BLI_sys_types.h"

#include "MEM_guardedalloc.h"

/**
 * This function is only to be called via macros.
 *
 * \note The caller must adjust \a arr_len
 */
void _bli_array_grow_func(
        void **arr_p, const void *arr_static,
        const int sizeof_arr_p, const int arr_len, const int num,
        const char *alloc_str)
{
	void *arr = *arr_p;
	void *arr_tmp;

	arr_tmp = MEM_mallocN(
	        sizeof_arr_p *
	        ((num < arr_len) ?
	         (arr_len * 2 + 2) : (arr_len + num)), alloc_str);

	if (arr) {
		memcpy(arr_tmp, arr, sizeof_arr_p * arr_len);

		if (arr != arr_static) {
			MEM_freeN(arr);
		}
	}

	*arr_p = arr_tmp;

	/* caller must do */
#if 0
	arr_len += num;
#endif
}
