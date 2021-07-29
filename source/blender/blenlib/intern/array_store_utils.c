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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/array_store_utils.c
 *  \ingroup bli
 *  \brief Helper functions for BLI_array_store API.
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_array_store.h"
#include "BLI_array_store_utils.h"  /* own include */

#include "BLI_math_base.h"

BArrayStore *BLI_array_store_at_size_ensure(
        struct BArrayStore_AtSize *bs_stride,
        const int stride, const int chunk_size)
{
	if (bs_stride->stride_table_len < stride) {
		bs_stride->stride_table_len = stride;
		bs_stride->stride_table = MEM_recallocN(bs_stride->stride_table, sizeof(*bs_stride->stride_table) * stride);
	}
	BArrayStore **bs_p = &bs_stride->stride_table[stride - 1];

	if ((*bs_p) == NULL) {
#if 0
		unsigned int chunk_count = chunk_size;
#else
		/* calculate best chunk-count to fit a power of two */
		unsigned int chunk_count = chunk_size;
		{
			unsigned int size = chunk_count * stride;
			size = power_of_2_max_u(size);
			size = MEM_SIZE_OPTIMAL(size);
			chunk_count = size / stride;
		}
#endif

		(*bs_p) = BLI_array_store_create(stride, chunk_count);
	}
	return *bs_p;
}

BArrayStore *BLI_array_store_at_size_get(
        struct BArrayStore_AtSize *bs_stride,
        const int stride)
{
	BLI_assert(stride > 0 && stride <= bs_stride->stride_table_len);
	return bs_stride->stride_table[stride - 1];
}

void BLI_array_store_at_size_clear(
        struct BArrayStore_AtSize *bs_stride)
{
	for (int i = 0; i < bs_stride->stride_table_len; i += 1) {
		if (bs_stride->stride_table[i]) {
			BLI_array_store_destroy(bs_stride->stride_table[i]);
		}
	}

	MEM_freeN(bs_stride->stride_table);
	bs_stride->stride_table = NULL;
	bs_stride->stride_table_len = 0;
}


void BLI_array_store_at_size_calc_memory_usage(
        struct BArrayStore_AtSize *bs_stride,
        size_t *r_size_expanded, size_t *r_size_compacted)
{
	size_t size_compacted = 0;
	size_t size_expanded  = 0;
	for (int i = 0; i < bs_stride->stride_table_len; i++) {
		BArrayStore *bs = bs_stride->stride_table[i];
		if (bs) {
			size_compacted += BLI_array_store_calc_size_compacted_get(bs);
			size_expanded  += BLI_array_store_calc_size_expanded_get(bs);
		}
	}

	*r_size_expanded = size_expanded;
	*r_size_compacted = size_compacted;
}
