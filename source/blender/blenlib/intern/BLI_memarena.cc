/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 * \brief Efficient memory allocation for many small chunks.
 * \section aboutmemarena Memory Arena
 *
 * Memory arena's are commonly used when the program
 * needs to quickly allocate lots of little bits of data,
 * which are all freed at the same moment.
 *
 * \note Memory can't be freed during the arenas lifetime.
 */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_asan.h"
#include "BLI_memarena.h"
#include "BLI_utildefines.h"

#ifdef WITH_MEM_VALGRIND
#  include "valgrind/memcheck.h"
#else
#  define VALGRIND_CREATE_MEMPOOL(pool, rzB, is_zeroed) UNUSED_VARS(pool, rzB, is_zeroed)
#  define VALGRIND_DESTROY_MEMPOOL(pool) UNUSED_VARS(pool)
#  define VALGRIND_MEMPOOL_ALLOC(pool, addr, size) UNUSED_VARS(pool, addr, size)
#  define VALGRIND_MOVE_MEMPOOL(pool_a, pool_b) UNUSED_VARS(pool_a, pool_b)
#endif

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

struct MemBuf {
  MemBuf *next;
  uchar data[0];
};

struct MemArena {
  uchar *curbuf;
  const char *name;
  MemBuf *bufs;

  size_t bufsize, cursize;
  size_t align;

  bool use_calloc;
};

static void memarena_buf_free_all(MemBuf *mb)
{
  while (mb != nullptr) {
    MemBuf *mb_next = mb->next;

    /* Unpoison memory because #MEM_freeN might overwrite it. */
    BLI_asan_unpoison(mb, uint(MEM_allocN_len(mb)));

    MEM_freeN(mb);
    mb = mb_next;
  }
}

MemArena *BLI_memarena_new(const size_t bufsize, const char *name)
{
  MemArena *ma = MEM_callocN<MemArena>("memarena");
  ma->bufsize = bufsize;
  ma->align = 8;
  ma->name = name;

  VALGRIND_CREATE_MEMPOOL(ma, 0, false);

  return ma;
}

void BLI_memarena_use_calloc(MemArena *ma)
{
  ma->use_calloc = true;
}

void BLI_memarena_use_malloc(MemArena *ma)
{
  ma->use_calloc = false;
}

void BLI_memarena_use_align(MemArena *ma, const size_t align)
{
  /* Align must be a power of two. */
  BLI_assert((align & (align - 1)) == 0);

  ma->align = align;
}

void BLI_memarena_free(MemArena *ma)
{
  memarena_buf_free_all(ma->bufs);

  VALGRIND_DESTROY_MEMPOOL(ma);

  MEM_freeN(ma);
}

/** Pad num up by \a amt (must be power of two). */
#define PADUP(num, amt) (((num) + ((amt) - 1)) & ~((amt) - 1))

/** Align alloc'ed memory (needed if `align > 8`). */
static void memarena_curbuf_align(MemArena *ma)
{
  uchar *tmp;

  tmp = (uchar *)PADUP(intptr_t(ma->curbuf), int(ma->align));
  ma->cursize -= size_t(tmp - ma->curbuf);
  ma->curbuf = tmp;
}

void *BLI_memarena_alloc(MemArena *ma, size_t size)
{
  void *ptr;

  /* Ensure proper alignment by rounding size up to multiple of 8. */
  size = PADUP(size, ma->align);

  if (UNLIKELY(size > ma->cursize)) {
    if (size > ma->bufsize - (ma->align - 1)) {
      ma->cursize = PADUP(size + 1, ma->align);
    }
    else {
      ma->cursize = ma->bufsize;
    }

    MemBuf *mb;
    if (ma->use_calloc) {
      mb = static_cast<MemBuf *>(MEM_callocN(sizeof(*mb) + ma->cursize, ma->name));
    }
    else {
      mb = static_cast<MemBuf *>(MEM_mallocN(sizeof(*mb) + ma->cursize, ma->name));
    }
    ma->curbuf = mb->data;
    mb->next = ma->bufs;
    ma->bufs = mb;

    BLI_asan_poison(ma->curbuf, ma->cursize);

    memarena_curbuf_align(ma);
  }

  ptr = ma->curbuf;
  ma->curbuf += size;
  ma->cursize -= size;

  VALGRIND_MEMPOOL_ALLOC(ma, ptr, size);

  BLI_asan_unpoison(ptr, size);

  return ptr;
}

void *BLI_memarena_calloc(MemArena *ma, size_t size)
{
  void *ptr;

  /* No need to use this function call if we're calloc'ing by default. */
  BLI_assert(ma->use_calloc == false);

  ptr = BLI_memarena_alloc(ma, size);
  BLI_assert(ptr != nullptr);
  memset(ptr, 0, size);

  return ptr;
}

void BLI_memarena_merge(MemArena *ma_dst, MemArena *ma_src)
{
  /* Memory arenas must be compatible. */
  BLI_assert(ma_dst != ma_src);
  BLI_assert(ma_dst->align == ma_src->align);
  BLI_assert(ma_dst->use_calloc == ma_src->use_calloc);
  BLI_assert(ma_dst->bufsize == ma_src->bufsize);

  if (ma_src->bufs == nullptr) {
    return;
  }

  if (UNLIKELY(ma_dst->bufs == nullptr)) {
    BLI_assert(ma_dst->curbuf == nullptr);
    ma_dst->bufs = ma_src->bufs;
    ma_dst->curbuf = ma_src->curbuf;
    ma_dst->cursize = ma_src->cursize;
  }
  else {
    /* Keep the 'ma_dst->curbuf' for simplicity.
     * Insert buffers after the first. */
    if (ma_dst->bufs->next != nullptr) {
      /* Loop over `ma_src` instead of `ma_dst` since it's likely the destination is larger
       * when used for accumulating from multiple sources. */
      MemBuf *mb_src = ma_src->bufs;
      while (mb_src->next) {
        mb_src = mb_src->next;
      }
      mb_src->next = ma_dst->bufs->next;
    }
    ma_dst->bufs->next = ma_src->bufs;
  }

  ma_src->bufs = nullptr;
  ma_src->curbuf = nullptr;
  ma_src->cursize = 0;

  VALGRIND_MOVE_MEMPOOL(ma_src, ma_dst);
  VALGRIND_CREATE_MEMPOOL(ma_src, 0, false);
}

void BLI_memarena_clear(MemArena *ma)
{
  if (ma->bufs) {
    if (ma->bufs->next) {
      memarena_buf_free_all(ma->bufs->next);
      ma->bufs->next = nullptr;
    }

    const uchar *curbuf_prev = ma->curbuf;
    ma->curbuf = ma->bufs->data;
    memarena_curbuf_align(ma);

    /* restore to original size */
    const size_t curbuf_used = size_t(curbuf_prev - ma->curbuf);
    ma->cursize += curbuf_used;

    if (ma->use_calloc) {
      memset(ma->curbuf, 0, curbuf_used);
    }
    BLI_asan_poison(ma->curbuf, ma->cursize);
  }

  VALGRIND_DESTROY_MEMPOOL(ma);
  VALGRIND_CREATE_MEMPOOL(ma, 0, false);
}
