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

/** \file blender/blenlib/intern/bitmap.c
 *  \ingroup bli
 *
 * Utility functions for variable size bitmasks.
 */

#include <string.h>
#include <limits.h>

#include "BLI_utildefines.h"
#include "BLI_bitmap.h"

/** Set or clear all bits in the bitmap. */
void BLI_bitmap_set_all(BLI_bitmap *bitmap, bool set, size_t bits)
{
	memset(bitmap, set ? UCHAR_MAX : 0, BLI_BITMAP_SIZE(bits));
}

/** Invert all bits in the bitmap. */
void BLI_bitmap_flip_all(BLI_bitmap *bitmap, size_t bits)
{
	size_t num_blocks = _BITMAP_NUM_BLOCKS(bits);
	for (size_t i = 0; i < num_blocks; i++) {
		bitmap[i] ^= ~(BLI_bitmap)0;
	}
}

/** Copy all bits from one bitmap to another. */
void BLI_bitmap_copy_all(BLI_bitmap *dst, const BLI_bitmap *src, size_t bits)
{
	memcpy(dst, src, BLI_BITMAP_SIZE(bits));
}

/** Combine two bitmaps with boolean AND. */
void BLI_bitmap_and_all(BLI_bitmap *dst, const BLI_bitmap *src, size_t bits)
{
	size_t num_blocks = _BITMAP_NUM_BLOCKS(bits);
	for (size_t i = 0; i < num_blocks; i++) {
		dst[i] &= src[i];
	}
}

/** Combine two bitmaps with boolean OR. */
void BLI_bitmap_or_all(BLI_bitmap *dst, const BLI_bitmap *src, size_t bits)
{
	size_t num_blocks = _BITMAP_NUM_BLOCKS(bits);
	for (size_t i = 0; i < num_blocks; i++) {
		dst[i] |= src[i];
	}
}
