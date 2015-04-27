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

#ifndef __BLI_ARRAY_UTILS_H__
#define __BLI_ARRAY_UTILS_H__

/** \file BLI_array_utils.h
 *  \ingroup bli
 *  \brief Generic array manipulation API.
 */

void _bli_array_reverse(void *arr, unsigned int arr_len, size_t arr_stride);
#define BLI_array_reverse(arr, arr_len) \
	_bli_array_reverse(arr, arr_len, sizeof(*(arr)))

void _bli_array_wrap(void *arr, unsigned int arr_len, size_t arr_stride, int dir);
#define BLI_array_wrap(arr, arr_len, dir) \
	_bli_array_wrap(arr, arr_len, sizeof(*(arr)), dir)

void _bli_array_permute(
        void *arr, const unsigned int arr_len, const size_t arr_stride,
        const unsigned int *index, void *arr_temp);
#define BLI_array_permute(arr, arr_len, order) \
	_bli_array_permute(arr, arr_len, sizeof(*(arr)), order, NULL)
#define BLI_array_permute_ex(arr, arr_len, index, arr_temp) \
	_bli_array_permute(arr, arr_len, sizeof(*(arr)), order, arr_temp)

int _bli_array_findindex(const void *arr, unsigned int arr_len, size_t arr_stride, const void *p);
#define BLI_array_findindex(arr, arr_len, p) \
	_bli_array_findindex(arr, arr_len, sizeof(*(arr)), p)

#endif  /* __BLI_ARRAY_UTILS_H__ */
