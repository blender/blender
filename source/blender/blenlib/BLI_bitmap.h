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
 * The Original Code is Copyright (C) 2012 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_BITMAP_H__
#define __BLI_BITMAP_H__

typedef unsigned int* BLI_bitmap;

/* warning: the bitmap does not keep track of its own size or check
 * for out-of-bounds access */

/* internal use */
/* 2^5 = 32 (bits) */
#define BLI_BITMAP_POWER 5
/* 0b11111 */
#define BLI_BITMAP_MASK 31

/* number of blocks needed to hold '_tot' bits */
#define BLI_BITMAP_NUM_BLOCKS(_tot) \
	(((_tot) >> BLI_BITMAP_POWER) + 1)

/* size (in bytes) used to hold '_tot' bits */
#define BLI_BITMAP_SIZE(_tot) \
	(BLI_BITMAP_NUM_BLOCKS(_tot) * sizeof(unsigned int))

/* allocate memory for a bitmap with '_tot' bits; free
 *  with MEM_freeN() */
#define BLI_BITMAP_NEW(_tot, _alloc_string) \
	((BLI_bitmap)MEM_callocN(BLI_BITMAP_SIZE(_tot), \
							 _alloc_string))

/* get the value of a single bit at '_index' */
#define BLI_BITMAP_GET(_bitmap, _index) \
	((_bitmap)[(_index) >> BLI_BITMAP_POWER] & \
	 (1 << ((_index) & BLI_BITMAP_MASK)))

/* set the value of a single bit at '_index' */
#define BLI_BITMAP_SET(_bitmap, _index) \
	((_bitmap)[(_index) >> BLI_BITMAP_POWER] |= \
	 (1 << ((_index) & BLI_BITMAP_MASK)))

/* clear the value of a single bit at '_index' */
#define BLI_BITMAP_CLEAR(_bitmap, _index) \
	((_bitmap)[(_index) >> BLI_BITMAP_POWER] &= \
	 ~(1 << ((_index) & BLI_BITMAP_MASK)))

/* set or clear the value of a single bit at '_index' */
#define BLI_BITMAP_MODIFY(_bitmap, _index, _set) \
	do { \
		if (_set) \
			BLI_BITMAP_SET(_bitmap, _index); \
		else \
			BLI_BITMAP_CLEAR(_bitmap, _index); \
	} while(0)

/* resize bitmap to have space for '_tot' bits */
#define BLI_BITMAP_RESIZE(_bitmap, _tot) \
	(_bitmap) = MEM_reallocN(_bitmap, BLI_BITMAP_SIZE(_tot))

#endif
