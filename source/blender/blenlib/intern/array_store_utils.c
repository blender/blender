/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 * \brief Helper functions for BLI_array_store API.
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_array_store.h"
#include "BLI_array_store_utils.h" /* own include */

#include "BLI_math_base.h"

BArrayStore *BLI_array_store_at_size_ensure(struct BArrayStore_AtSize *bs_stride,
                                            const int stride,
                                            const int chunk_size)
{
  if (bs_stride->stride_table_len < stride) {
    bs_stride->stride_table_len = stride;
    bs_stride->stride_table = MEM_recallocN(bs_stride->stride_table,
                                            sizeof(*bs_stride->stride_table) * stride);
  }
  BArrayStore **bs_p = &bs_stride->stride_table[stride - 1];

  if ((*bs_p) == NULL) {
    /* calculate best chunk-count to fit a power of two */
    uint chunk_count = chunk_size;
    {
      uint size = chunk_count * stride;
      size = power_of_2_max_u(size);
      size = MEM_SIZE_OPTIMAL(size);
      chunk_count = size / stride;
    }

    (*bs_p) = BLI_array_store_create(stride, chunk_count);
  }
  return *bs_p;
}

BArrayStore *BLI_array_store_at_size_get(struct BArrayStore_AtSize *bs_stride, const int stride)
{
  BLI_assert(stride > 0 && stride <= bs_stride->stride_table_len);
  return bs_stride->stride_table[stride - 1];
}

void BLI_array_store_at_size_clear(struct BArrayStore_AtSize *bs_stride)
{
  for (int i = 0; i < bs_stride->stride_table_len; i += 1) {
    if (bs_stride->stride_table[i]) {
      BLI_array_store_destroy(bs_stride->stride_table[i]);
    }
  }

  /* It's possible this table was never used. */
  MEM_SAFE_FREE(bs_stride->stride_table);
  bs_stride->stride_table_len = 0;
}

void BLI_array_store_at_size_calc_memory_usage(struct BArrayStore_AtSize *bs_stride,
                                               size_t *r_size_expanded,
                                               size_t *r_size_compacted)
{
  size_t size_compacted = 0;
  size_t size_expanded = 0;
  for (int i = 0; i < bs_stride->stride_table_len; i++) {
    BArrayStore *bs = bs_stride->stride_table[i];
    if (bs) {
      size_compacted += BLI_array_store_calc_size_compacted_get(bs);
      size_expanded += BLI_array_store_calc_size_expanded_get(bs);
    }
  }

  *r_size_expanded = size_expanded;
  *r_size_compacted = size_compacted;
}
