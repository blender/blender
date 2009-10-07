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
and to be more of a complete array API.  The way arrays are
exposed to client code as normal C arrays is very useful though, imho.
it does require some use of macros, however.  

anyway, it's used a bit too heavily to simply rewrite as a 
more "correct" solution without macros entirely.  I originally wrote this
to be very easy to use, without the normal pain of most array libraries.
This was especially helpful when it came to the massive refactors necessary for
bmesh, and really helped to speed the process up. - joeedh
  
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

#define BLI_array_declare(arr) int _##arr##_count=0; void *_##arr##_tmp

/*this returns the entire size of the array, including any buffering.*/
#define BLI_array_totalsize(arr) ((signed int)((arr)==NULL ? 0 : MEM_allocN_len(arr) / sizeof(*arr)))

/*this returns the logical size of the array, not including buffering.*/
#define BLI_array_count(arr) _##arr##_count

/*grow the array by one.  zeroes the new elements.*/
#define BLI_array_growone(arr) \
	BLI_array_totalsize(arr) > _##arr##_count ? _##arr##_count++ : \
	((_##arr##_tmp = MEM_callocN(sizeof(*arr)*(_##arr##_count*2+2), #arr " " __FILE__ " ")),\
	(arr && memcpy(_##arr##_tmp, arr, sizeof(*arr) * _##arr##_count)),\
	(arr && (MEM_freeN(arr),1)),\
	(arr = _##arr##_tmp),\
	_##arr##_count++)

/*appends an item to the array and returns a pointer to the item in the array.
  item is not a pointer, but actual data value.*/
#define BLI_array_append(arr, item) (BLI_array_growone(arr), arr[_##arr##_count] = item, (arr+_##arr##_count))

/*grow an array by a specified number of items.*/
#define BLI_array_growitems(arr, num) {int _i; for (_i=0; _i<(num); _i++) {BLI_array_growone(arr);}}
#define BLI_array_free(arr) if (arr) MEM_freeN(arr)

/*resets the logical size of an array to zero, but doesn't
  free the memory.*/
#define BLI_array_empty(arr) _##arr##_count=0

/*set the count of the array, doesn't actually increase the allocated array
  size.  don't use this unless you know what your doing.*/
#define BLI_array_set_length(arr, count) _##arr##_count = (count)
