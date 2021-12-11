/*
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
 */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int BLI_bitmap;

/* warning: the bitmap does not keep track of its own size or check
 * for out-of-bounds access */

/* internal use */
/* 2^5 = 32 (bits) */
#define _BITMAP_POWER 5
/* 0b11111 */
#define _BITMAP_MASK 31

/**
 * Number of blocks needed to hold '_tot' bits.
 */
#define _BITMAP_NUM_BLOCKS(_tot) (((_tot) >> _BITMAP_POWER) + 1)

/**
 * Size (in bytes) used to hold '_tot' bits.
 */
#define BLI_BITMAP_SIZE(_tot) ((size_t)(_BITMAP_NUM_BLOCKS(_tot)) * sizeof(BLI_bitmap))

/**
 * Allocate memory for a bitmap with '_tot' bits; free with MEM_freeN().
 */
#define BLI_BITMAP_NEW(_tot, _alloc_string) \
  ((BLI_bitmap *)MEM_callocN(BLI_BITMAP_SIZE(_tot), _alloc_string))

/**
 * Allocate a bitmap on the stack.
 */
#define BLI_BITMAP_NEW_ALLOCA(_tot) \
  ((BLI_bitmap *)memset(alloca(BLI_BITMAP_SIZE(_tot)), 0, BLI_BITMAP_SIZE(_tot)))

/**
 * Allocate using given MemArena.
 */
#define BLI_BITMAP_NEW_MEMARENA(_mem, _tot) \
  (CHECK_TYPE_INLINE(_mem, MemArena *), \
   ((BLI_bitmap *)BLI_memarena_calloc(_mem, BLI_BITMAP_SIZE(_tot))))

/**
 * Get the value of a single bit at '_index'.
 */
#define BLI_BITMAP_TEST(_bitmap, _index) \
  (CHECK_TYPE_ANY(_bitmap, BLI_bitmap *, const BLI_bitmap *), \
   ((_bitmap)[(_index) >> _BITMAP_POWER] & (1u << ((_index)&_BITMAP_MASK))))

#define BLI_BITMAP_TEST_AND_SET_ATOMIC(_bitmap, _index) \
  (CHECK_TYPE_ANY(_bitmap, BLI_bitmap *, const BLI_bitmap *), \
   (atomic_fetch_and_or_uint32((uint32_t *)&(_bitmap)[(_index) >> _BITMAP_POWER], \
                               (1u << ((_index)&_BITMAP_MASK))) & \
    (1u << ((_index)&_BITMAP_MASK))))

#define BLI_BITMAP_TEST_BOOL(_bitmap, _index) \
  (CHECK_TYPE_ANY(_bitmap, BLI_bitmap *, const BLI_bitmap *), \
   (BLI_BITMAP_TEST(_bitmap, _index) != 0))

/**
 * Set the value of a single bit at '_index'.
 */
#define BLI_BITMAP_ENABLE(_bitmap, _index) \
  (CHECK_TYPE_ANY(_bitmap, BLI_bitmap *, const BLI_bitmap *), \
   ((_bitmap)[(_index) >> _BITMAP_POWER] |= (1u << ((_index)&_BITMAP_MASK))))

/**
 * Clear the value of a single bit at '_index'.
 */
#define BLI_BITMAP_DISABLE(_bitmap, _index) \
  (CHECK_TYPE_ANY(_bitmap, BLI_bitmap *, const BLI_bitmap *), \
   ((_bitmap)[(_index) >> _BITMAP_POWER] &= ~(1u << ((_index)&_BITMAP_MASK))))

/**
 * Flip the value of a single bit at '_index'.
 */
#define BLI_BITMAP_FLIP(_bitmap, _index) \
  (CHECK_TYPE_ANY(_bitmap, BLI_bitmap *, const BLI_bitmap *), \
   ((_bitmap)[(_index) >> _BITMAP_POWER] ^= (1u << ((_index)&_BITMAP_MASK))))

/**
 * Set or clear the value of a single bit at '_index'.
 */
#define BLI_BITMAP_SET(_bitmap, _index, _set) \
  { \
    CHECK_TYPE(_bitmap, BLI_bitmap *); \
    if (_set) { \
      BLI_BITMAP_ENABLE(_bitmap, _index); \
    } \
    else { \
      BLI_BITMAP_DISABLE(_bitmap, _index); \
    } \
  } \
  (void)0

/**
 * Resize bitmap to have space for '_tot' bits.
 */
#define BLI_BITMAP_RESIZE(_bitmap, _tot) \
  { \
    CHECK_TYPE(_bitmap, BLI_bitmap *); \
    (_bitmap) = MEM_recallocN(_bitmap, BLI_BITMAP_SIZE(_tot)); \
  } \
  (void)0

/**
 * Set or clear all bits in the bitmap.
 */
void BLI_bitmap_set_all(BLI_bitmap *bitmap, bool set, size_t bits);
/**
 * Invert all bits in the bitmap.
 */
void BLI_bitmap_flip_all(BLI_bitmap *bitmap, size_t bits);
/**
 * Copy all bits from one bitmap to another.
 */
void BLI_bitmap_copy_all(BLI_bitmap *dst, const BLI_bitmap *src, size_t bits);
/**
 * Combine two bitmaps with boolean AND.
 */
void BLI_bitmap_and_all(BLI_bitmap *dst, const BLI_bitmap *src, size_t bits);
/**
 * Combine two bitmaps with boolean OR.
 */
void BLI_bitmap_or_all(BLI_bitmap *dst, const BLI_bitmap *src, size_t bits);

#ifdef __cplusplus
}
#endif
