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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_ARRAY_MALLOCN_H
#define BKE_ARRAY_MALLOCN_H

/** \file BKE_array_mallocn.h
 *  \ingroup bke
 *  \brief little array macro library.
 */

/* example of usage:
 *
 * int *arr = NULL;
 * V_DECLARE(arr);
 * int i;
 *
 * for (i=0; i<10; i++) {
 * 	V_GROW(arr);
 * 	arr[i] = something;
 * }
 * V_FREE(arr);
 *
 * arrays are buffered, using double-buffering (so on each reallocation,
 * the array size is doubled).  supposedly this should give good Big Oh
 * behaviour, though it may not be the best in practice.
 */

#define V_DECLARE(vec) int _##vec##_count=0; void *_##vec##_tmp

/* in the future, I plan on having V_DECLARE allocate stack memory it'll
 * use at first, and switch over to heap when it needs more.  that'll mess
 * up cases where you'd want to use this API to build a dynamic list for
 * non-local use, so all such cases should use this macro.*/
#define V_DYNDECLARE(vec) V_DECLARE(vec)

/*this returns the entire size of the array, including any buffering.*/
#define V_SIZE(vec) ((signed int)((vec)==NULL ? 0 : MEM_allocN_len(vec) / sizeof(*vec)))

/*this returns the logical size of the array, not including buffering.*/
#define V_COUNT(vec) _##vec##_count

/*grow the array by one.  zeroes the new elements.*/
#define V_GROW(vec) \
	V_SIZE(vec) > _##vec##_count ? _##vec##_count++ : \
	((_##vec##_tmp = MEM_callocN(sizeof(*vec)*(_##vec##_count*2+2), #vec " " __FILE__ " ")),\
	(void)(vec && memcpy(_##vec##_tmp, vec, sizeof(*vec) * _##vec##_count)),\
	(void)(vec && (MEM_freeN(vec),1)),\
	(vec = _##vec##_tmp),\
	_##vec##_count++)

#define V_FREE(vec) if (vec) MEM_freeN(vec);

/*resets the logical size of an array to zero, but doesn't
  free the memory.*/
#define V_RESET(vec) _##vec##_count=0

/*set the count of the array*/
#define V_SETCOUNT(vec, count) _##vec##_count = (count)

#endif // BKE_ARRAY_MALLOCN_H
