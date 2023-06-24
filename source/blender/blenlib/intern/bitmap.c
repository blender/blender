/* SPDX-FileCopyrightText: 2012 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * Utility functions for variable size bit-masks.
 */

#include <limits.h>
#include <string.h>

#include "BLI_bitmap.h"
#include "BLI_math_bits.h"
#include "BLI_utildefines.h"

void BLI_bitmap_set_all(BLI_bitmap *bitmap, bool set, size_t bits)
{
  memset(bitmap, set ? UCHAR_MAX : 0, BLI_BITMAP_SIZE(bits));
}

void BLI_bitmap_flip_all(BLI_bitmap *bitmap, size_t bits)
{
  size_t blocks_num = _BITMAP_NUM_BLOCKS(bits);
  for (size_t i = 0; i < blocks_num; i++) {
    bitmap[i] ^= ~(BLI_bitmap)0;
  }
}

void BLI_bitmap_copy_all(BLI_bitmap *dst, const BLI_bitmap *src, size_t bits)
{
  memcpy(dst, src, BLI_BITMAP_SIZE(bits));
}

void BLI_bitmap_and_all(BLI_bitmap *dst, const BLI_bitmap *src, size_t bits)
{
  size_t blocks_num = _BITMAP_NUM_BLOCKS(bits);
  for (size_t i = 0; i < blocks_num; i++) {
    dst[i] &= src[i];
  }
}

void BLI_bitmap_or_all(BLI_bitmap *dst, const BLI_bitmap *src, size_t bits)
{
  size_t blocks_num = _BITMAP_NUM_BLOCKS(bits);
  for (size_t i = 0; i < blocks_num; i++) {
    dst[i] |= src[i];
  }
}

int BLI_bitmap_find_first_unset(const BLI_bitmap *bitmap, const size_t bits)
{
  const size_t blocks_num = _BITMAP_NUM_BLOCKS(bits);
  int result = -1;
  /* Skip over completely set blocks. */
  int index = 0;
  while (index < blocks_num && bitmap[index] == ~0u) {
    index++;
  }
  if (index < blocks_num) {
    /* Found a partially used block: find the lowest unused bit. */
    const uint m = ~bitmap[index];
    BLI_assert(m != 0);
    const uint bit_index = bitscan_forward_uint(m);
    result = bit_index + (index << _BITMAP_POWER);
  }
  return result;
}
