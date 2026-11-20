/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * Dead simple, fast memory allocator for allocating many elements of the same size.
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "BLI_utildefines.hh"
#include "BLI_vector.hh"

#include "BLI_memblock.hh" /* own include */

#include "MEM_guardedalloc.h"

#include "BLI_strict_flags.hh" /* IWYU pragma: keep. Keep last. */

namespace blender {

#define CHUNK_LIST_SIZE 16

struct BLI_memblock {
  /** Allocated chunks, unused trailing items may be null. */
  Vector<void *> chunk_list;

  /** Element size in bytes. */
  int elem_size = 0;
  /** First unused element index. */
  int elem_next = 0;
  /** Last "touched" element. */
  int elem_last = -1;
  /** Offset in a chunk of the next elem. */
  int elem_next_ofs = 0;
  /** Max offset in a chunk. */
  int chunk_max_ofs = 0;
  /** Id of the chunk used for the next allocation. */
  int chunk_next = 0;
  /** Chunk size in bytes. */
  int chunk_size = 0;
};

BLI_memblock *BLI_memblock_create_ex(uint elem_size, uint chunk_size)
{
  BLI_assert(elem_size < chunk_size);

  BLI_memblock *mblk = MEM_new<BLI_memblock>("BLI_memblock");
  mblk->elem_size = int(elem_size);
  mblk->elem_next = 0;
  mblk->elem_last = -1;
  mblk->chunk_size = int(chunk_size);
  mblk->chunk_list.resize(CHUNK_LIST_SIZE, nullptr);
  mblk->chunk_list[0] = MEM_new_uninitialized_aligned(
      size_t(mblk->chunk_size), 32, "BLI_memblock chunk");
  memset(mblk->chunk_list[0], 0x0, uint(mblk->chunk_size));
  mblk->chunk_max_ofs = (mblk->chunk_size / mblk->elem_size) * mblk->elem_size;
  mblk->elem_next_ofs = 0;
  mblk->chunk_next = 0;
  return mblk;
}

void BLI_memblock_destroy(BLI_memblock *mblk, MemblockValFreeFP free_callback)
{
  int elem_per_chunk = mblk->chunk_size / mblk->elem_size;

  if (free_callback) {
    for (int i = 0; i <= mblk->elem_last; i++) {
      int chunk_idx = i / elem_per_chunk;
      int elem_idx = i - elem_per_chunk * chunk_idx;
      void *val = static_cast<char *>(mblk->chunk_list[chunk_idx]) + mblk->elem_size * elem_idx;
      free_callback(val);
    }
  }

  for (void *chunk : mblk->chunk_list) {
    MEM_SAFE_DELETE_VOID(chunk);
  }
  MEM_delete(mblk);
}

void BLI_memblock_clear(BLI_memblock *mblk, MemblockValFreeFP free_callback)
{
  int elem_per_chunk = mblk->chunk_size / mblk->elem_size;
  int last_used_chunk = mblk->elem_next / elem_per_chunk;

  if (free_callback) {
    for (int i = mblk->elem_last; i >= mblk->elem_next; i--) {
      int chunk_idx = i / elem_per_chunk;
      int elem_idx = i - elem_per_chunk * chunk_idx;
      void *val = static_cast<char *>(mblk->chunk_list[chunk_idx]) + mblk->elem_size * elem_idx;
      free_callback(val);
    }
  }

  const int chunk_len = int(mblk->chunk_list.size());
  for (int i = last_used_chunk + 1; i < chunk_len; i++) {
    MEM_SAFE_DELETE_VOID(mblk->chunk_list[i]);
  }

  if (last_used_chunk + 1 < chunk_len - CHUNK_LIST_SIZE) [[unlikely]] {
    mblk->chunk_list.resize(chunk_len - CHUNK_LIST_SIZE);
  }

  mblk->elem_last = mblk->elem_next - 1;
  mblk->elem_next = 0;
  mblk->elem_next_ofs = 0;
  mblk->chunk_next = 0;
}

void *BLI_memblock_alloc(BLI_memblock *mblk)
{
  /* Bookkeeping. */
  mblk->elem_last = std::max(mblk->elem_last, mblk->elem_next);
  mblk->elem_next++;

  void *ptr = static_cast<char *>(mblk->chunk_list[mblk->chunk_next]) + mblk->elem_next_ofs;

  mblk->elem_next_ofs += mblk->elem_size;

  if (mblk->elem_next_ofs == mblk->chunk_max_ofs) {
    mblk->elem_next_ofs = 0;
    mblk->chunk_next++;

    if (mblk->chunk_next >= mblk->chunk_list.size()) [[unlikely]] {
      mblk->chunk_list.resize(mblk->chunk_list.size() + CHUNK_LIST_SIZE, nullptr);
    }

    if (mblk->chunk_list[mblk->chunk_next] == nullptr) [[unlikely]] {
      mblk->chunk_list[mblk->chunk_next] = MEM_new_uninitialized_aligned(
          uint(mblk->chunk_size), 32, "BLI_memblock chunk");
      memset(mblk->chunk_list[mblk->chunk_next], 0x0, uint(mblk->chunk_size));
    }
  }
  return ptr;
}

void BLI_memblock_iternew(BLI_memblock *mblk, BLI_memblock_iter *iter)
{
  /* Small copy of the memblock used for better cache coherence. */
  iter->chunk_list = mblk->chunk_list.data();
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
    return nullptr;
  }

  iter->cur_index++;

  void *ptr = static_cast<char *>(iter->chunk_list[iter->chunk_idx]) + iter->elem_ofs;

  iter->elem_ofs += iter->elem_size;

  if (iter->elem_ofs == iter->chunk_max_ofs) {
    iter->elem_ofs = 0;
    iter->chunk_idx++;
  }
  return ptr;
}

void *BLI_memblock_elem_get(BLI_memblock *mblk, int chunk, int elem)
{
  BLI_assert(chunk < mblk->chunk_list.size());
  int elem_per_chunk = mblk->chunk_size / mblk->elem_size;
  chunk += elem / elem_per_chunk;
  elem = elem % elem_per_chunk;
  return static_cast<char *>(mblk->chunk_list[chunk]) + mblk->elem_size * elem;
}

}  // namespace blender
