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
  int elem_size;
  /** First unused element index. */
  int elem_next;
  /** Last "touched" element. */
  int elem_last;
  /** Offset in a chunk of the next elem. */
  int elem_next_ofs;
  /** Max offset in a chunk. */
  int chunk_max_ofs;
  /** Id of the chunk used for the next allocation. */
  int chunk_next;
  /** Chunck size in bytes. */
  int chunk_size;
  /** Number of allocated chunck. */
  int chunk_len;
};

BLI_memblock *BLI_memblock_create(uint elem_size)
{
  BLI_assert(elem_size < BLI_MEM_BLOCK_CHUNK_SIZE);

  BLI_memblock *mblk = MEM_mallocN(sizeof(BLI_memblock), "BLI_memblock");
  mblk->elem_size = (int)elem_size;
  mblk->elem_next = 0;
  mblk->elem_last = -1;
  mblk->chunk_size = BLI_MEM_BLOCK_CHUNK_SIZE;
  mblk->chunk_len = CHUNK_LIST_SIZE;
  mblk->chunk_list = MEM_callocN(sizeof(void *) * (uint)mblk->chunk_len, "chunk list");
  mblk->chunk_list[0] = MEM_callocN((uint)mblk->chunk_size, "BLI_memblock chunk");
  mblk->chunk_max_ofs = (mblk->chunk_size / mblk->elem_size) * mblk->elem_size;
  mblk->elem_next_ofs = 0;
  mblk->chunk_next = 0;
  return mblk;
}

void BLI_memblock_destroy(BLI_memblock *mblk, MemblockValFreeFP free_callback)
{
  BLI_memblock_clear(mblk, free_callback);

  for (int i = 0; i < mblk->chunk_len; i++) {
    MEM_SAFE_FREE(mblk->chunk_list[i]);
  }
  MEM_SAFE_FREE(mblk->chunk_list);
  MEM_freeN(mblk);
}

/* Reset elem count to 0 but keep as much memory allocated needed for at least the previous elem
 * count. */
void BLI_memblock_clear(BLI_memblock *mblk, MemblockValFreeFP free_callback)
{
  int elem_per_chunk = mblk->chunk_size / mblk->elem_size;
  int last_used_chunk = mblk->elem_next / elem_per_chunk;

  if (free_callback) {
    for (int i = mblk->elem_last; i >= mblk->elem_next; i--) {
      int chunk_idx = i / elem_per_chunk;
      int elem_idx = i - elem_per_chunk * chunk_idx;
      void *val = (char *)(mblk->chunk_list[chunk_idx]) + mblk->elem_size * elem_idx;
      free_callback(val);
    }
  }

  for (int i = last_used_chunk + 1; i < mblk->chunk_len; i++) {
    MEM_SAFE_FREE(mblk->chunk_list[i]);
  }

  if (UNLIKELY(last_used_chunk + 1 < mblk->chunk_len - CHUNK_LIST_SIZE)) {
    mblk->chunk_len -= CHUNK_LIST_SIZE;
    mblk->chunk_list = MEM_recallocN(mblk->chunk_list, sizeof(void *) * (uint)mblk->chunk_len);
  }

  mblk->elem_last = mblk->elem_next - 1;
  mblk->elem_next = 0;
  mblk->elem_next_ofs = 0;
  mblk->chunk_next = 0;
}

void *BLI_memblock_alloc(BLI_memblock *mblk)
{
  /* Bookeeping. */
  if (mblk->elem_last < mblk->elem_next) {
    mblk->elem_last = mblk->elem_next;
  }
  mblk->elem_next++;

  void *ptr = (char *)(mblk->chunk_list[mblk->chunk_next]) + mblk->elem_next_ofs;

  mblk->elem_next_ofs += mblk->elem_size;

  if (mblk->elem_next_ofs == mblk->chunk_max_ofs) {
    mblk->elem_next_ofs = 0;
    mblk->chunk_next++;

    if (UNLIKELY(mblk->chunk_next >= mblk->chunk_len)) {
      mblk->chunk_len += CHUNK_LIST_SIZE;
      mblk->chunk_list = MEM_recallocN(mblk->chunk_list, sizeof(void *) * (uint)mblk->chunk_len);
    }

    if (UNLIKELY(mblk->chunk_list[mblk->chunk_next] == NULL)) {
      mblk->chunk_list[mblk->chunk_next] = MEM_callocN((uint)mblk->chunk_size,
                                                       "BLI_memblock chunk");
    }
  }
  return ptr;
}

void BLI_memblock_iternew(BLI_memblock *mblk, BLI_memblock_iter *iter)
{
  /* Small copy of the memblock used for better cache coherence. */
  iter->chunk_list = mblk->chunk_list;
  iter->end_index = mblk->elem_next;
  iter->cur_index = 0;
  iter->chunk_idx = 0;
  iter->elem_ofs = 0;
  iter->elem_size = mblk->elem_size;
  iter->chunk_max_ofs = mblk->chunk_max_ofs;
}

void *BLI_memblock_iterstep(BLI_memblock_iter *iter)
{
  if (iter->cur_index == iter->end_index) {
    return NULL;
  }

  iter->cur_index++;

  void *ptr = (char *)(iter->chunk_list[iter->chunk_idx]) + iter->elem_ofs;

  iter->elem_ofs += iter->elem_size;

  if (iter->elem_ofs == iter->chunk_max_ofs) {
    iter->elem_ofs = 0;
    iter->chunk_idx++;
  }
  return ptr;
}
