/* SPDX-FileCopyrightText: 2012 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * Utility functions for variable size bit-masks.
 */

#include <climits>
#include <cstring>

#include "BLI_bitmap.hh"
#include "BLI_math_bits.hh"

namespace blender {

void BLI_bitmap_set_all(BLI_bitmap *bitmap, bool set, size_t bits)
{
  memset(bitmap, set ? UCHAR_MAX : 0, BLI_BITMAP_SIZE(bits));
}

void BLI_bitmap_copy_all(BLI_bitmap *dst, const BLI_bitmap *src, size_t bits)
{
  memcpy(dst, src, BLI_BITMAP_SIZE(bits));
}

}  // namespace blender
