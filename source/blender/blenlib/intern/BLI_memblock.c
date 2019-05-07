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
 * The Original Code is Copyright (C) 2008 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bli
 *
 * Dead simple, fast memory allocator for allocating many elements of the same size.
 *
 */

#include <string.h>
#include <stdlib.h>

#include "atomic_ops.h"

#include "BLI_utildefines.h"

#include "BLI_memblock.h" /* own include */

#include "MEM_guardedalloc.h"

#include "BLI_strict_flags.h" /* keep last */

#define BLI_MEM_BLOCK_CHUNK_SIZE (1 << 15) /* 32KiB */
#define CHUNK_LIST_SIZE 16

struct BLI_memblock {
  void **chunk_list;

  /** Element size in bytes. */
  uint elem_size;
  /** First unused element index. */
  uint elem_next;
  /** Chunck size in bytes. */
  uint chunk_size;
  /** Number of allocated chunck. */
  uint chunk_len;
};

BLI_memblock *BLI_memblock_create(uint elem_size)
{
  BLI_assert(elem_size < BLI_MEM_BLOCK_CHUNK_SIZE);

  BLI_memblock *mblk = MEM_mallocN(sizeof(BLI_memblock), "BLI_memblock");
  mblk->elem_size = elem_size;
  mblk->elem_next = 0;
  mblk->chunk_size = BLI_MEM_BLOCK_CHUNK_SIZE;
  mblk->chunk_len = CHUNK_LIST_SIZE;
  mblk->chunk_list = MEM_callocN(sizeof(void *) * mblk->chunk_len, "BLI_memblock chunk list");
  return mblk;
}

void BLI_memblock_destroy(BLI_memblock *mblk)
{
  for (uint i = 0; i < mblk->chunk_len; i++) {
    MEM_SAFE_FREE(mblk->chunk_list[i]);
  }
  MEM_SAFE_FREE(mblk->chunk_list);
  MEM_freeN(mblk);
}

/* Reset elem count to 0 but keep as much memory allocated needed for at least the previous elem
 * count. */
void BLI_memblock_clear(BLI_memblock *mblk)
{
  uint elem_per_chunk = mblk->chunk_size / mblk->elem_size;
  uint last_used_chunk = (mblk->elem_next - 1) / elem_per_chunk;

  for (uint i = last_used_chunk + 1; i < mblk->chunk_len; i++) {
    MEM_SAFE_FREE(mblk->chunk_list[i]);
  }

  if (UNLIKELY(last_used_chunk + 1 < mblk->chunk_len - CHUNK_LIST_SIZE)) {
    mblk->chunk_len -= CHUNK_LIST_SIZE;
    mblk->chunk_list = MEM_recallocN(mblk->chunk_list, sizeof(void *) * mblk->chunk_len);
  }

  mblk->elem_next = 0;
}

void *BLI_memblock_alloc(BLI_memblock *mblk)
{
  uint elem_per_chunk = mblk->chunk_size / mblk->elem_size;
  uint chunk_idx = mblk->elem_next / elem_per_chunk;
  uint elem_idx = mblk->elem_next - elem_per_chunk * chunk_idx;
  mblk->elem_next++;

  if (UNLIKELY(chunk_idx >= mblk->chunk_len)) {
    mblk->chunk_len += CHUNK_LIST_SIZE;
    mblk->chunk_list = MEM_recallocN(mblk->chunk_list, sizeof(void *) * mblk->chunk_len);
  }

  if (UNLIKELY(mblk->chunk_list[chunk_idx] == NULL)) {
    mblk->chunk_list[chunk_idx] = MEM_mallocN(mblk->chunk_size, "BLI_memblock chunk");
  }

  return (char *)(mblk->chunk_list[chunk_idx]) + mblk->elem_size * elem_idx;
}

void BLI_memblock_iternew(BLI_memblock *mblk, BLI_memblock_iter *iter)
{
  iter->mblk = mblk;
  iter->current_index = 0;
  iter->elem_per_chunk = mblk->chunk_size / mblk->elem_size;
}

void *BLI_memblock_iterstep(BLI_memblock_iter *iter)
{
  if (iter->current_index >= iter->mblk->elem_next) {
    return NULL;
  }

  uint chunk_idx = iter->current_index / iter->elem_per_chunk;
  uint elem_idx = iter->current_index - iter->elem_per_chunk * chunk_idx;
  iter->current_index++;

  return (char *)(iter->mblk->chunk_list[chunk_idx]) + iter->mblk->elem_size * elem_idx;
}
