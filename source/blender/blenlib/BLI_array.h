/**
 * Array library
 * 
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/*
this library needs to be changed to not use macros quite so heavily,
and to be more of a complete vector array API.  The way arrays are
exposed to client code as normal C arrays is very useful though, imho.
it does require some use of macros, however.
  
little array macro library.  example of usage:

int *arr = NULL;
BLI_array_declare(arr);
int i;

for (i=0; i<10; i++) {
	BLI_array_growone(arr);
	arr[i] = something;
}
BLI_array_free(arr);

arrays are buffered, using double-buffering (so on each reallocation,
the array size is doubled).  supposedly this should give good Big Oh
behaviour, though it may not be the best in practice.
*/

#define BLI_array_declare(vec) int _##vec##_count=0; void *_##vec##_tmp

/*this returns the entire size of the array, including any buffering.*/
#define BLI_array_totalsize(vec) ((signed int)((vec)==NULL ? 0 : MEM_allocN_len(vec) / sizeof(*vec)))

/*this returns the logical size of the array, not including buffering.*/
#define BLI_array_count(vec) _##vec##_count

/*grow the array by one.  zeroes the new elements.*/
#define BLI_array_growone(vec) \
	BLI_array_totalsize(vec) > _##vec##_count ? _##vec##_count++ : \
	((_##vec##_tmp = MEM_callocN(sizeof(*vec)*(_##vec##_count*2+2), #vec " " __FILE__ " ")),\
	(vec && memcpy(_##vec##_tmp, vec, sizeof(*vec) * _##vec##_count)),\
	(vec && (MEM_freeN(vec),1)),\
	(vec = _##vec##_tmp),\
	_##vec##_count++)

#define BLI_array_free(vec) if (vec) MEM_freeN(vec);

/*resets the logical size of an array to zero, but doesn't
  free the memory.*/
#define BLI_array_empty(vec) _##vec##_count=0

/*set the count of the array*/
#define BLI_array_set_length(vec, count) _##vec##_count = (count)
