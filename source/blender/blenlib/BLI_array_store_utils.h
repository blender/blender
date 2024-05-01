/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
                                            int stride,
                                            int chunk_size);

BArrayStore *BLI_array_store_at_size_get(struct BArrayStore_AtSize *bs_stride, int stride);

void BLI_array_store_at_size_clear(struct BArrayStore_AtSize *bs_stride);

void BLI_array_store_at_size_calc_memory_usage(const struct BArrayStore_AtSize *bs_stride,
                                               size_t *r_size_expanded,
                                               size_t *r_size_compacted);

#ifdef __cplusplus
}
#endif
