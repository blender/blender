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
 */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BArrayStore;

struct BArrayStore_AtSize {
  struct BArrayStore **stride_table;
  int stride_table_len;
};

BArrayStore *BLI_array_store_at_size_ensure(struct BArrayStore_AtSize *bs_stride,
                                            const int stride,
                                            const int chunk_size);

BArrayStore *BLI_array_store_at_size_get(struct BArrayStore_AtSize *bs_stride, const int stride);

void BLI_array_store_at_size_clear(struct BArrayStore_AtSize *bs_stride);

void BLI_array_store_at_size_calc_memory_usage(struct BArrayStore_AtSize *bs_stride,
                                               size_t *r_size_expanded,
                                               size_t *r_size_compacted);

#ifdef __cplusplus
}
#endif
