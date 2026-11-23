/* SPDX-FileCopyrightText: 2012 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <atomic>

#include "BLI_utildefines.hh"

namespace blender {

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
 * Allocate memory for a bitmap with '_num' bits; free with MEM_delete().
 */
#define BLI_BITMAP_NEW(_num, _alloc_string) \
  ((BLI_bitmap *)MEM_new_zeroed(BLI_BITMAP_SIZE(_num), _alloc_string))

/**
 * Allocate using given MemArena.
 */
#define BLI_BITMAP_NEW_MEMARENA(_mem, _num) \
  (CHECK_TYPE_INLINE(_mem, MemArena *), \
   ((BLI_bitmap *)BLI_memarena_calloc(_mem, BLI_BITMAP_SIZE(_num))))

/**
 * Get the value of a single bit at '_index'.
 */
#define BLI_BITMAP_TEST(_bitmap, _index) \
  (CHECK_TYPE_ANY(_bitmap, BLI_bitmap *, const BLI_bitmap *), \
   ((_bitmap)[(_index) >> _BITMAP_POWER] & (1u << ((_index) & _BITMAP_MASK))))

#define BLI_BITMAP_TEST_AND_SET_ATOMIC(_bitmap, _index) \
  (CHECK_TYPE_ANY(_bitmap, BLI_bitmap *, const BLI_bitmap *), \
   (std::atomic_ref(*(BLI_bitmap *)&(_bitmap)[(_index) >> _BITMAP_POWER]) \
        .fetch_or(1u << ((_index) & _BITMAP_MASK)) & \
    (1u << ((_index) & _BITMAP_MASK))))

#define BLI_BITMAP_TEST_BOOL(_bitmap, _index) \
  (CHECK_TYPE_ANY(_bitmap, BLI_bitmap *, const BLI_bitmap *), \
   (BLI_BITMAP_TEST(_bitmap, _index) != 0))

/**
 * Set the value of a single bit at '_index'.
 */
#define BLI_BITMAP_ENABLE(_bitmap, _index) \
  (CHECK_TYPE_ANY(_bitmap, BLI_bitmap *, const BLI_bitmap *), \
   ((_bitmap)[(_index) >> _BITMAP_POWER] |= (1u << ((_index) & _BITMAP_MASK))))

/**
 * Clear the value of a single bit at '_index'.
 */
#define BLI_BITMAP_DISABLE(_bitmap, _index) \
  (CHECK_TYPE_ANY(_bitmap, BLI_bitmap *, const BLI_bitmap *), \
   ((_bitmap)[(_index) >> _BITMAP_POWER] &= ~(1u << ((_index) & _BITMAP_MASK))))

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
 * Set or clear all bits in the bitmap.
 */
void BLI_bitmap_set_all(BLI_bitmap *bitmap, bool set, size_t bits);
/**
 * Copy all bits from one bitmap to another.
 */
void BLI_bitmap_copy_all(BLI_bitmap *dst, const BLI_bitmap *src, size_t bits);

}  // namespace blender
