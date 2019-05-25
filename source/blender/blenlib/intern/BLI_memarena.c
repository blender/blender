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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

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

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_memarena.h"
#include "BLI_strict_flags.h"

#ifdef WITH_MEM_VALGRIND
#  include "valgrind/memcheck.h"
#else
#  define VALGRIND_CREATE_MEMPOOL(pool, rzB, is_zeroed) UNUSED_VARS(pool, rzB, is_zeroed)
#  define VALGRIND_DESTROY_MEMPOOL(pool) UNUSED_VARS(pool)
#  define VALGRIND_MEMPOOL_ALLOC(pool, addr, size) UNUSED_VARS(pool, addr, size)
#endif

/* Clang defines this. */
#ifndef __has_feature
#  define __has_feature(x) 0
#endif
#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
#  include "sanitizer/asan_interface.h"
#else
/* Ensure return value is used. */
#  define ASAN_POISON_MEMORY_REGION(addr, size) (void)(0 && ((size) != 0 && (addr) != NULL))
#  define ASAN_UNPOISON_MEMORY_REGION(addr, size) (void)(0 && ((size) != 0 && (addr) != NULL))
#endif

struct MemBuf {
  struct MemBuf *next;
  uchar data[0];
};

struct MemArena {
  unsigned char *curbuf;
  const char *name;
  struct MemBuf *bufs;

  size_t bufsize, cursize;
  size_t align;

  bool use_calloc;
};

static void memarena_buf_free_all(struct MemBuf *mb)
{
  while (mb != NULL) {
    struct MemBuf *mb_next = mb->next;

    /* Unpoison memory because MEM_freeN might overwrite it. */
    ASAN_UNPOISON_MEMORY_REGION(mb, (uint)MEM_allocN_len(mb));

    MEM_freeN(mb);
    mb = mb_next;
  }
}

MemArena *BLI_memarena_new(const size_t bufsize, const char *name)
{
  MemArena *ma = MEM_callocN(sizeof(*ma), "memarena");
  ma->bufsize = bufsize;
  ma->align = 8;
  ma->name = name;

  VALGRIND_CREATE_MEMPOOL(ma, 0, false);

  return ma;
}

void BLI_memarena_use_calloc(MemArena *ma)
{
  ma->use_calloc = 1;
}

void BLI_memarena_use_malloc(MemArena *ma)
{
  ma->use_calloc = 0;
}

void BLI_memarena_use_align(struct MemArena *ma, const size_t align)
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
#define PADUP(num, amt) (((num) + ((amt)-1)) & ~((amt)-1))

/** Align alloc'ed memory (needed if `align > 8`). */
static void memarena_curbuf_align(MemArena *ma)
{
  unsigned char *tmp;

  tmp = (unsigned char *)PADUP((intptr_t)ma->curbuf, (int)ma->align);
  ma->cursize -= (size_t)(tmp - ma->curbuf);
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

    struct MemBuf *mb = (ma->use_calloc ? MEM_callocN : MEM_mallocN)(sizeof(*mb) + ma->cursize,
                                                                     ma->name);
    ma->curbuf = mb->data;
    mb->next = ma->bufs;
    ma->bufs = mb;

    ASAN_POISON_MEMORY_REGION(ma->curbuf, ma->cursize);

    memarena_curbuf_align(ma);
  }

  ptr = ma->curbuf;
  ma->curbuf += size;
  ma->cursize -= size;

  VALGRIND_MEMPOOL_ALLOC(ma, ptr, size);

  ASAN_UNPOISON_MEMORY_REGION(ptr, size);

  return ptr;
}

void *BLI_memarena_calloc(MemArena *ma, size_t size)
{
  void *ptr;

  /* No need to use this function call if we're calloc'ing by default. */
  BLI_assert(ma->use_calloc == false);

  ptr = BLI_memarena_alloc(ma, size);
  memset(ptr, 0, size);

  return ptr;
}

/**
 * Clear for reuse, avoids re-allocation when an arena may
 * otherwise be free'd and recreated.
 */
void BLI_memarena_clear(MemArena *ma)
{
  if (ma->bufs) {
    unsigned char *curbuf_prev;
    size_t curbuf_used;

    if (ma->bufs->next) {
      memarena_buf_free_all(ma->bufs->next);
      ma->bufs->next = NULL;
    }

    curbuf_prev = ma->curbuf;
    ma->curbuf = ma->bufs->data;
    memarena_curbuf_align(ma);

    /* restore to original size */
    curbuf_used = (size_t)(curbuf_prev - ma->curbuf);
    ma->cursize += curbuf_used;

    if (ma->use_calloc) {
      memset(ma->curbuf, 0, curbuf_used);
    }
    ASAN_POISON_MEMORY_REGION(ma->curbuf, ma->cursize);
  }

  VALGRIND_DESTROY_MEMPOOL(ma);
  VALGRIND_CREATE_MEMPOOL(ma, 0, false);
}
