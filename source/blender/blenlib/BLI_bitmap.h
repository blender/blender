/* SPDX-FileCopyrightText: 2012 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int BLI_bitmap;

/* WARNING: the bitmap does not keep track of its own size or check
 * for out-of-bounds access */

/* internal use */
/* 2^5 = 32 (bits) */
#define _BITMAP_POWER 5
/* 0b11111 */
#define _BITMAP_MASK 31

/**
 * Number of blocks needed to hold '_num' bits.
 */
#define _BITMAP_NUM_BLOCKS(_num) (((_num) + _BITMAP_MASK) >> _BITMAP_POWER)

/**
 * Size (in bytes) used to hold '_num' bits.
 */
#define BLI_BITMAP_SIZE(_num) ((size_t)(_BITMAP_NUM_BLOCKS(_num)) * sizeof(BLI_bitmap))

/**
 * Allocate memory for a bitmap with '_num' bits; free with MEM_freeN().
 */
#define BLI_BITMAP_NEW(_num, _alloc_string) \
  ((BLI_bitmap *)MEM_callocN(BLI_BITMAP_SIZE(_num), _alloc_string))

/**
 * Allocate a bitmap on the stack.
 */
#define BLI_BITMAP_NEW_ALLOCA(_num) \
  ((BLI_bitmap *)memset(alloca(BLI_BITMAP_SIZE(_num)), 0, BLI_BITMAP_SIZE(_num)))

/**
 * Allocate using given MemArena.
 */
#define BLI_BITMAP_NEW_MEMARENA(_mem, _num) \
  (CHECK_TYPE_INLINE(_mem, MemArena *), \
   ((BLI_bitmap *)BLI_memarena_calloc(_mem, BLI_BITMAP_SIZE(_num))))

/**
 * Declares a bitmap as a variable.
 */
#define BLI_BITMAP_DECLARE(_name, _num) BLI_bitmap _name[_BITMAP_NUM_BLOCKS(_num)] = {}

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
 * Resize bitmap to have space for '_num' bits.
 */
#define BLI_BITMAP_RESIZE(_bitmap, _num) \
  { \
    CHECK_TYPE(_bitmap, BLI_bitmap *); \
    (_bitmap) = MEM_recallocN(_bitmap, BLI_BITMAP_SIZE(_num)); \
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

/**
 * Find index of the lowest unset bit.
 * Returns -1 if all the bits are set.
 */
int BLI_bitmap_find_first_unset(const BLI_bitmap *bitmap, size_t bits);

#ifdef __cplusplus
}
#endif
