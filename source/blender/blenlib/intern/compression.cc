/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_compression.hh"

namespace blender {

void filter_transpose_delta(const uint8_t *src, uint8_t *dst, size_t items_num, size_t item_size)
{
  for (size_t ib = 0; ib < item_size; ib++) {
    uint8_t prev = 0;
    const uint8_t *src_ptr = src + ib;
    size_t it = 0;
    for (; it < items_num; it++) {
      uint8_t v = *src_ptr;
      *dst = v - prev;
      prev = v;
      src_ptr += item_size;
      dst += 1;
    }
  }
}

void unfilter_transpose_delta(const uint8_t *src, uint8_t *dst, size_t items_num, size_t item_size)
{
  for (size_t ib = 0; ib < item_size; ib++) {
    uint8_t prev = 0;
    uint8_t *dst_ptr = dst + ib;
    for (size_t it = 0; it < items_num; it++) {
      uint8_t v = *src + prev;
      prev = v;
      *dst_ptr = v;
      src += 1;
      dst_ptr += item_size;
    }
  }
}

}  // namespace blender
