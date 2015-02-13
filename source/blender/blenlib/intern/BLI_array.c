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
 * arrays are buffered, using double-buffering (so on each reallocation,
 * the array size is doubled).  supposedly this should give good Big Oh
 * behavior, though it may not be the best in practice.
 */

#include <string.h>

#include "BLI_array.h"

#include "BLI_sys_types.h"

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
