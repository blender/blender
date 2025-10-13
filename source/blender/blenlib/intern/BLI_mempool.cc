/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * Simple, fast memory allocator for allocating many elements of the same size.
 *
 * Supports:
 *
 * - Freeing chunks.
 * - Iterating over allocated chunks
 *   (optionally when using the #BLI_MEMPOOL_ALLOW_ITER flag).
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "atomic_ops.h"

#include "BLI_utildefines.h"

#include "BLI_asan.h"
#include "BLI_math_base.h"
#include "BLI_mempool.h"         /* own include */
#include "BLI_mempool_private.h" /* own include */

#ifdef WITH_ASAN
#  include "BLI_threads.h"
#endif

#include "MEM_guardedalloc.h"

#ifdef WITH_MEM_VALGRIND
#  include "valgrind/memcheck.h"
#endif

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

#ifdef WITH_ASAN
#  define POISON_REDZONE_SIZE 32
#else
#  define POISON_REDZONE_SIZE 0
#endif

/* NOTE: copied from BLO_core_bhead.hh, don't use here because we're in BLI. */
/* NOTE: this is endianness-sensitive. */
#define MAKE_ID(a, b, c, d) (int(d) << 24 | int(c) << 16 | (b) << 8 | (a))
#define MAKE_ID_8(a, b, c, d, e, f, g, h) \
  (int64_t(h) << 56 | int64_t(g) << 48 | int64_t(f) << 40 | int64_t(e) << 32 | int64_t(d) << 24 | \
   int64_t(c) << 16 | int64_t(b) << 8 | (a))

/**
 * Important that this value is not aligned with `sizeof(void *)`.
 * So having a pointer to 2/4/8... aligned memory is enough to ensure
 * the `freeword` will never be used.
 * To be safe, use a word that's the same in both directions.
 */
#define FREEWORD \
  ((sizeof(void *) > sizeof(int32_t)) ? MAKE_ID_8('e', 'e', 'r', 'f', 'f', 'r', 'e', 'e') : \
                                        MAKE_ID('e', 'f', 'f', 'e'))

/**
 * The 'used' word just needs to be set to something besides FREEWORD.
 */
#define USEDWORD MAKE_ID('u', 's', 'e', 'd')

/* optimize pool size */
#define USE_CHUNK_POW2

#ifndef NDEBUG
static bool mempool_debug_memset = false;
#endif

/**
 * A free element from #BLI_mempool_chunk. Data is cast to this type and stored in
 * #BLI_mempool.free as a single linked list, each item #BLI_mempool.esize large.
 *
 * Each element represents a block which BLI_mempool_alloc may return.
 */
struct BLI_freenode {
  BLI_freenode *next;
  /** Used to identify this as a freed node. */
  intptr_t freeword;
};

/**
 * A chunk of memory in the mempool stored in
 * #BLI_mempool.chunks as a double linked list.
 */
struct BLI_mempool_chunk {
  BLI_mempool_chunk *next;
};

/**
 * The mempool, stores and tracks memory \a chunks and elements within those chunks \a free.
 */
struct BLI_mempool {
#ifdef WITH_ASAN
  /** Serialize access to memory-pools when debugging with ASAN. */
  ThreadMutex mutex;
#endif
  /** Single linked list of allocated chunks. */
  BLI_mempool_chunk *chunks;
  /** Keep a pointer to the last, so we can append new chunks there
   * this is needed for iteration so we can loop over chunks in the order added. */
  BLI_mempool_chunk *chunk_tail;

  /** Element size in bytes. */
  uint esize;
  /** Chunk size in bytes. */
  uint csize;
  /** Number of elements per chunk. */
  uint pchunk;
  uint flag;

  /** Free element list. Interleaved into chunk data. */
  BLI_freenode *free;
  /** Use to know how many chunks to keep for #BLI_mempool_clear. */
  uint maxchunks;
  /** Number of elements currently in use. */
  uint totused;
};

#define MEMPOOL_ELEM_SIZE_MIN (sizeof(void *) * 2)

#define CHUNK_DATA(chunk) \
  ((BLI_freenode *)(CHECK_TYPE_INLINE(chunk, BLI_mempool_chunk *), (void *)((chunk) + 1)))

#define NODE_STEP_NEXT(node) ((BLI_freenode *)((char *)(node) + esize))
#define NODE_STEP_PREV(node) ((BLI_freenode *)((char *)(node) - esize))

/** Extra bytes implicitly used for every chunk alloc. */
#define CHUNK_OVERHEAD uint(MEM_SIZE_OVERHEAD + sizeof(BLI_mempool_chunk))

static void mempool_asan_unlock(BLI_mempool *pool)
{
#ifdef WITH_ASAN
  BLI_mutex_unlock(&pool->mutex);
#else
  UNUSED_VARS(pool);
#endif
}

static void mempool_asan_lock(BLI_mempool *pool)
{
#ifdef WITH_ASAN
  BLI_mutex_lock(&pool->mutex);
#else
  UNUSED_VARS(pool);
#endif
}

BLI_INLINE BLI_mempool_chunk *mempool_chunk_find(BLI_mempool_chunk *head, uint index)
{
  while (index-- && head) {
    head = head->next;
  }
  return head;
}

/**
 * \return the number of chunks to allocate based on how many elements are needed.
 *
 * \note for small pools 1 is a good default, the elements need to be initialized,
 * adding overhead on creation which is redundant if they aren't used.
 */
BLI_INLINE uint mempool_maxchunks(const uint elem_num, const uint pchunk)
{
  return (elem_num <= pchunk) ? 1 : ((elem_num / pchunk) + 1);
}

static BLI_mempool_chunk *mempool_chunk_alloc(const BLI_mempool *pool)
{
  return static_cast<BLI_mempool_chunk *>(
      MEM_mallocN(sizeof(BLI_mempool_chunk) + size_t(pool->csize), "mempool chunk"));
}

/**
 * Initialize a chunk and add into \a pool->chunks
 *
 * \param pool: The pool to add the chunk into.
 * \param mpchunk: The new uninitialized chunk (can be malloc'd)
 * \param last_tail: The last element of the previous chunk
 * (used when building free chunks initially)
 * \return The last chunk,
 */
static BLI_freenode *mempool_chunk_add(BLI_mempool *pool,
                                       BLI_mempool_chunk *mpchunk,
                                       BLI_freenode *last_tail)
{
  const uint esize = pool->esize;
  BLI_freenode *curnode = CHUNK_DATA(mpchunk);
  uint j;

  /* append */
  if (pool->chunk_tail) {
    pool->chunk_tail->next = mpchunk;
  }
  else {
    BLI_assert(pool->chunks == nullptr);
    pool->chunks = mpchunk;
  }

  mpchunk->next = nullptr;
  pool->chunk_tail = mpchunk;

  if (UNLIKELY(pool->free == nullptr)) {
    pool->free = curnode;
  }

  /* loop through the allocated data, building the pointer structures */
  j = pool->pchunk;
  if (pool->flag & BLI_MEMPOOL_ALLOW_ITER) {
    while (j--) {
      BLI_freenode *next;

      BLI_asan_unpoison(curnode, pool->esize - POISON_REDZONE_SIZE);
#ifdef WITH_MEM_VALGRIND
      VALGRIND_MAKE_MEM_DEFINED(curnode, pool->esize - POISON_REDZONE_SIZE);
#endif
      curnode->next = next = NODE_STEP_NEXT(curnode);
      curnode->freeword = FREEWORD;

      BLI_asan_poison(curnode, pool->esize);
#ifdef WITH_MEM_VALGRIND
      VALGRIND_MAKE_MEM_UNDEFINED(curnode, pool->esize);
#endif
      curnode = next;
    }
  }
  else {
    while (j--) {
      BLI_freenode *next;

      BLI_asan_unpoison(curnode, pool->esize - POISON_REDZONE_SIZE);
#ifdef WITH_MEM_VALGRIND
      VALGRIND_MAKE_MEM_DEFINED(curnode, pool->esize - POISON_REDZONE_SIZE);
#endif
      curnode->next = next = NODE_STEP_NEXT(curnode);
      BLI_asan_poison(curnode, pool->esize);
#ifdef WITH_MEM_VALGRIND
      VALGRIND_MAKE_MEM_UNDEFINED(curnode, pool->esize);
#endif

      curnode = next;
    }
  }

  /* terminate the list (rewind one)
   * will be overwritten if 'curnode' gets passed in again as 'last_tail' */

  if (POISON_REDZONE_SIZE > 0) {
    BLI_asan_unpoison(curnode, pool->esize - POISON_REDZONE_SIZE);
    BLI_asan_poison(curnode, pool->esize);
#ifdef WITH_MEM_VALGRIND
    VALGRIND_MAKE_MEM_DEFINED(curnode, pool->esize - POISON_REDZONE_SIZE);
    VALGRIND_MAKE_MEM_UNDEFINED(curnode, pool->esize);
#endif
  }

  curnode = NODE_STEP_PREV(curnode);

  BLI_asan_unpoison(curnode, pool->esize - POISON_REDZONE_SIZE);
#ifdef WITH_MEM_VALGRIND
  VALGRIND_MAKE_MEM_DEFINED(curnode, pool->esize - POISON_REDZONE_SIZE);
#endif

  curnode->next = nullptr;
  BLI_asan_poison(curnode, pool->esize);
#ifdef WITH_MEM_VALGRIND
  VALGRIND_MAKE_MEM_UNDEFINED(curnode, pool->esize);
#endif

  /* final pointer in the previously allocated chunk is wrong */
  if (last_tail) {
    BLI_asan_unpoison(last_tail, pool->esize - POISON_REDZONE_SIZE);
#ifdef WITH_MEM_VALGRIND
    VALGRIND_MAKE_MEM_DEFINED(last_tail, pool->esize - POISON_REDZONE_SIZE);
#endif
    last_tail->next = CHUNK_DATA(mpchunk);
    BLI_asan_poison(last_tail, pool->esize);
#ifdef WITH_MEM_VALGRIND
    VALGRIND_MAKE_MEM_UNDEFINED(last_tail, pool->esize);
#endif
  }

  return curnode;
}

static void mempool_chunk_free(BLI_mempool_chunk *mpchunk, BLI_mempool *pool)
{
#ifdef WITH_ASAN
  BLI_asan_unpoison(mpchunk, sizeof(BLI_mempool_chunk) + pool->esize * pool->csize);
#else
  UNUSED_VARS(pool);
#endif
#ifdef WITH_MEM_VALGRIND
  VALGRIND_MAKE_MEM_DEFINED(mpchunk, sizeof(BLI_mempool_chunk) + pool->esize * pool->csize);
#endif
  MEM_freeN(mpchunk);
}

static void mempool_chunk_free_all(BLI_mempool_chunk *mpchunk, BLI_mempool *pool)
{
  BLI_mempool_chunk *mpchunk_next;

  for (; mpchunk; mpchunk = mpchunk_next) {
    mpchunk_next = mpchunk->next;
    mempool_chunk_free(mpchunk, pool);
  }
}

BLI_mempool *BLI_mempool_create(uint esize, uint elem_num, uint pchunk, uint flag)
{
  BLI_mempool *pool;
  BLI_freenode *last_tail = nullptr;
  uint i, maxchunks;

  /* allocate the pool structure */
  pool = MEM_callocN<BLI_mempool>("memory pool");

#ifdef WITH_ASAN
  BLI_mutex_init(&pool->mutex);
#endif

  /* set the elem size */
  esize = std::max(esize, uint(MEMPOOL_ELEM_SIZE_MIN));

  if (flag & BLI_MEMPOOL_ALLOW_ITER) {
    esize = std::max(esize, uint(sizeof(BLI_freenode)));
  }

  esize += POISON_REDZONE_SIZE;

  maxchunks = mempool_maxchunks(elem_num, pchunk);

  pool->chunks = nullptr;
  pool->chunk_tail = nullptr;
  pool->esize = esize;

  /* Optimize chunk size to powers of 2, accounting for slop-space. */
#ifdef USE_CHUNK_POW2
  {
    BLI_assert(power_of_2_max_u(pchunk * esize) > CHUNK_OVERHEAD);
    pchunk = (power_of_2_max_u(pchunk * esize) - CHUNK_OVERHEAD) / esize;
  }
#endif

  pool->csize = esize * pchunk;

  /* Ensure this is a power of 2, minus the rounding by element size. */
#if defined(USE_CHUNK_POW2) && !defined(NDEBUG)
  {
    uint final_size = uint(MEM_SIZE_OVERHEAD) + uint(sizeof(BLI_mempool_chunk)) + pool->csize;
    BLI_assert((uint(power_of_2_max_u(final_size)) - final_size) < pool->esize);
  }
#endif

  pool->pchunk = pchunk;
  pool->flag = flag;
  pool->free = nullptr; /* mempool_chunk_add assigns */
  pool->maxchunks = maxchunks;
  pool->totused = 0;

  if (elem_num) {
    /* Allocate the actual chunks. */
    for (i = 0; i < maxchunks; i++) {
      BLI_mempool_chunk *mpchunk = mempool_chunk_alloc(pool);
      last_tail = mempool_chunk_add(pool, mpchunk, last_tail);
    }
  }

#ifdef WITH_MEM_VALGRIND
  VALGRIND_CREATE_MEMPOOL(pool, 0, false);
#endif

  return pool;
}

void *BLI_mempool_alloc(BLI_mempool *pool)
{
  BLI_freenode *free_pop;

  if (UNLIKELY(pool->free == nullptr)) {
    /* Need to allocate a new chunk. */
    BLI_mempool_chunk *mpchunk = mempool_chunk_alloc(pool);
    mempool_chunk_add(pool, mpchunk, nullptr);
  }

  free_pop = pool->free;

  BLI_asan_unpoison(free_pop, pool->esize - POISON_REDZONE_SIZE);
#ifdef WITH_MEM_VALGRIND
  VALGRIND_MEMPOOL_ALLOC(pool, free_pop, pool->esize - POISON_REDZONE_SIZE);
  /* Mark as define, then undefine immediately before returning so:
   * - `free_pop->next` can be read without reading "undefined" memory.
   * - `freeword` can be set without causing the memory to be considered "defined".
   *
   * These could be handled on a more granular level - dealing with defining & underlining these
   * members explicitly but that requires more involved calls,
   * adding overhead for no real benefit. */
  VALGRIND_MAKE_MEM_DEFINED(free_pop, pool->esize - POISON_REDZONE_SIZE);
#endif

  BLI_assert(pool->chunk_tail->next == nullptr);

  if (pool->flag & BLI_MEMPOOL_ALLOW_ITER) {
    free_pop->freeword = USEDWORD;
  }

  pool->free = free_pop->next;
  pool->totused++;

#ifdef WITH_MEM_VALGRIND
  VALGRIND_MAKE_MEM_UNDEFINED(free_pop, pool->esize - POISON_REDZONE_SIZE);
#endif

  return (void *)free_pop;
}

void *BLI_mempool_calloc(BLI_mempool *pool)
{
  void *retval = BLI_mempool_alloc(pool);

  memset(retval, 0, size_t(pool->esize) - POISON_REDZONE_SIZE);

  return retval;
}

void BLI_mempool_free(BLI_mempool *pool, void *addr)
{
  BLI_freenode *newhead = static_cast<BLI_freenode *>(addr);

#ifndef NDEBUG
  {
    BLI_mempool_chunk *chunk;
    bool found = false;
    for (chunk = pool->chunks; chunk; chunk = chunk->next) {
      if (ARRAY_HAS_ITEM((char *)addr, (char *)CHUNK_DATA(chunk), pool->csize)) {
        found = true;
        break;
      }
    }
    if (!found) {
      BLI_assert_msg(0, "Attempt to free data which is not in pool.\n");
    }
  }

  /* Enable for debugging. */
  if (UNLIKELY(mempool_debug_memset)) {
    memset(addr, 255, pool->esize - POISON_REDZONE_SIZE);
  }
#endif

  if (pool->flag & BLI_MEMPOOL_ALLOW_ITER) {
#ifndef NDEBUG
    /* This will detect double free's. */
    BLI_assert(newhead->freeword != FREEWORD);
#endif
    newhead->freeword = FREEWORD;
  }

  newhead->next = pool->free;
  pool->free = newhead;

  BLI_asan_poison(newhead, pool->esize);

  pool->totused--;

#ifdef WITH_MEM_VALGRIND
  VALGRIND_MEMPOOL_FREE(pool, addr);
#endif

  /* Nothing is in use; free all the chunks except the first. */
  if (UNLIKELY(pool->totused == 0) && (pool->chunks->next)) {
    const uint esize = pool->esize;
    BLI_freenode *curnode;
    uint j;
    BLI_mempool_chunk *first;

    first = pool->chunks;
    mempool_chunk_free_all(first->next, pool);
    first->next = nullptr;
    pool->chunk_tail = first;

    /* Temporary allocation so VALGRIND doesn't complain when setting freed blocks 'next'. */
#ifdef WITH_MEM_VALGRIND
    VALGRIND_MEMPOOL_ALLOC(pool, CHUNK_DATA(first), pool->csize);
#endif

    curnode = CHUNK_DATA(first);
    pool->free = curnode;

    j = pool->pchunk;
    while (j--) {
      BLI_asan_unpoison(curnode, pool->esize - POISON_REDZONE_SIZE);
      BLI_freenode *next = curnode->next = NODE_STEP_NEXT(curnode);
      BLI_asan_poison(curnode, pool->esize);
      curnode = next;
    }

    BLI_asan_unpoison(curnode, pool->esize - POISON_REDZONE_SIZE);
    BLI_freenode *prev = NODE_STEP_PREV(curnode);
    BLI_asan_poison(curnode, pool->esize);

    curnode = prev;

    BLI_asan_unpoison(curnode, pool->esize - POISON_REDZONE_SIZE);
    curnode->next = nullptr; /* terminate the list */
    BLI_asan_poison(curnode, pool->esize);

#ifdef WITH_MEM_VALGRIND
    VALGRIND_MEMPOOL_FREE(pool, CHUNK_DATA(first));
#endif
  }
}

int BLI_mempool_len(const BLI_mempool *pool)
{
  int ret = int(pool->totused);

  return ret;
}

void *BLI_mempool_findelem(BLI_mempool *pool, uint index)
{
  mempool_asan_lock(pool);

  BLI_assert(pool->flag & BLI_MEMPOOL_ALLOW_ITER);

  if (index < pool->totused) {
    /* We could have some faster mem chunk stepping code inline. */
    BLI_mempool_iter iter;
    void *elem;
    BLI_mempool_iternew(pool, &iter);
    for (elem = BLI_mempool_iterstep(&iter); index-- != 0; elem = BLI_mempool_iterstep(&iter)) {
      /* pass */
    }

    mempool_asan_unlock(pool);
    return elem;
  }

  mempool_asan_unlock(pool);
  return nullptr;
}

void BLI_mempool_as_array(BLI_mempool *pool, void *data)
{
  const uint esize = pool->esize - uint(POISON_REDZONE_SIZE);
  BLI_mempool_iter iter;
  const char *elem;
  char *p = static_cast<char *>(data);

  BLI_assert(pool->flag & BLI_MEMPOOL_ALLOW_ITER);

  mempool_asan_lock(pool);
  BLI_mempool_iternew(pool, &iter);
  while ((elem = static_cast<const char *>(BLI_mempool_iterstep(&iter)))) {
    memcpy(p, elem, size_t(esize));
    p = reinterpret_cast<char *>(NODE_STEP_NEXT(p));
  }
  mempool_asan_unlock(pool);
}

void *BLI_mempool_as_arrayN(BLI_mempool *pool, const char *allocstr)
{
  char *data = static_cast<char *>(
      MEM_malloc_arrayN(size_t(pool->totused), pool->esize, allocstr));
  BLI_mempool_as_array(pool, data);
  return data;
}

void BLI_mempool_iternew(BLI_mempool *pool, BLI_mempool_iter *iter)
{
  BLI_assert(pool->flag & BLI_MEMPOOL_ALLOW_ITER);

  iter->pool = pool;
  iter->curchunk = pool->chunks;
  iter->curindex = 0;
}

static void mempool_threadsafe_iternew(BLI_mempool *pool, BLI_mempool_threadsafe_iter *ts_iter)
{
  BLI_mempool_iternew(pool, &ts_iter->iter);
  ts_iter->curchunk_threaded_shared = nullptr;
}

ParallelMempoolTaskData *mempool_iter_threadsafe_create(BLI_mempool *pool, const size_t iter_num)
{
  BLI_assert(pool->flag & BLI_MEMPOOL_ALLOW_ITER);

  ParallelMempoolTaskData *iter_arr = MEM_calloc_arrayN<ParallelMempoolTaskData>(iter_num,
                                                                                 __func__);
  BLI_mempool_chunk **curchunk_threaded_shared = MEM_callocN<BLI_mempool_chunk *>(__func__);

  mempool_threadsafe_iternew(pool, &iter_arr->ts_iter);

  *curchunk_threaded_shared = iter_arr->ts_iter.iter.curchunk;
  iter_arr->ts_iter.curchunk_threaded_shared = curchunk_threaded_shared;
  for (size_t i = 1; i < iter_num; i++) {
    iter_arr[i].ts_iter = iter_arr[0].ts_iter;
    *curchunk_threaded_shared = iter_arr[i].ts_iter.iter.curchunk =
        ((*curchunk_threaded_shared) ? (*curchunk_threaded_shared)->next : nullptr);
  }

  return iter_arr;
}

void mempool_iter_threadsafe_destroy(ParallelMempoolTaskData *iter_arr)
{
  BLI_assert(iter_arr->ts_iter.curchunk_threaded_shared != nullptr);

  MEM_freeN(iter_arr->ts_iter.curchunk_threaded_shared);
  MEM_freeN(iter_arr);
}

#if 0
/* unoptimized, more readable */

static void *bli_mempool_iternext(BLI_mempool_iter *iter)
{
  void *ret = nullptr;

  if (iter->curchunk == nullptr || !iter->pool->totused) {
    return ret;
  }

  ret = ((char *)CHUNK_DATA(iter->curchunk)) + (iter->pool->esize * iter->curindex);

  iter->curindex++;

  if (iter->curindex == iter->pool->pchunk) {
    iter->curindex = 0;
    iter->curchunk = iter->curchunk->next;
  }

  return ret;
}

void *BLI_mempool_iterstep(BLI_mempool_iter *iter)
{
  BLI_freenode *ret;

  do {
    ret = bli_mempool_iternext(iter);
  } while (ret && ret->freeword == FREEWORD);

  return ret;
}

#else /* Optimized version of code above. */

void *BLI_mempool_iterstep(BLI_mempool_iter *iter)
{
  if (UNLIKELY(iter->curchunk == nullptr)) {
    return nullptr;
  }

  const uint esize = iter->pool->esize;
  BLI_freenode *curnode = POINTER_OFFSET(CHUNK_DATA(iter->curchunk), (esize * iter->curindex));
  BLI_freenode *ret;
  do {
    ret = curnode;

    BLI_asan_unpoison(ret, iter->pool->esize - POISON_REDZONE_SIZE);
#  ifdef WITH_MEM_VALGRIND
    VALGRIND_MAKE_MEM_DEFINED(ret, iter->pool->esize - POISON_REDZONE_SIZE);
#  endif

    if (++iter->curindex != iter->pool->pchunk) {
      curnode = POINTER_OFFSET(curnode, esize);
    }
    else {
      iter->curindex = 0;
      iter->curchunk = iter->curchunk->next;
      if (UNLIKELY(iter->curchunk == nullptr)) {
        BLI_asan_unpoison(ret, iter->pool->esize - POISON_REDZONE_SIZE);
#  ifdef WITH_MEM_VALGRIND
        VALGRIND_MAKE_MEM_DEFINED(ret, iter->pool->esize - POISON_REDZONE_SIZE);
#  endif
        void *ret2 = (ret->freeword == FREEWORD) ? nullptr : ret;

        if (ret->freeword == FREEWORD) {
          BLI_asan_poison(ret, iter->pool->esize);
#  ifdef WITH_MEM_VALGRIND
          VALGRIND_MAKE_MEM_UNDEFINED(ret, iter->pool->esize);
#  endif
        }

        return ret2;
      }
      curnode = CHUNK_DATA(iter->curchunk);
    }
  } while (ret->freeword == FREEWORD);

  return ret;
}

void *mempool_iter_threadsafe_step(BLI_mempool_threadsafe_iter *ts_iter)
{
  BLI_mempool_iter *iter = &ts_iter->iter;
  if (UNLIKELY(iter->curchunk == nullptr)) {
    return nullptr;
  }

  mempool_asan_lock(iter->pool);

  const uint esize = iter->pool->esize;
  BLI_freenode *curnode = POINTER_OFFSET(CHUNK_DATA(iter->curchunk), (esize * iter->curindex));
  BLI_freenode *ret;
  do {
    ret = curnode;

    BLI_asan_unpoison(ret, esize - POISON_REDZONE_SIZE);
#  ifdef WITH_MEM_VALGRIND
    VALGRIND_MAKE_MEM_DEFINED(ret, iter->pool->esize);
#  endif

    if (++iter->curindex != iter->pool->pchunk) {
      curnode = POINTER_OFFSET(curnode, esize);
    }
    else {
      iter->curindex = 0;

      /* Begin unique to the `threadsafe` version of this function. */
      for (iter->curchunk = *ts_iter->curchunk_threaded_shared;
           (iter->curchunk != nullptr) &&
           (atomic_cas_ptr((void **)ts_iter->curchunk_threaded_shared,
                           iter->curchunk,
                           iter->curchunk->next) != iter->curchunk);
           iter->curchunk = *ts_iter->curchunk_threaded_shared)
      {
        /* pass. */
      }
      if (UNLIKELY(iter->curchunk == nullptr)) {
        if (ret->freeword == FREEWORD) {
          BLI_asan_poison(ret, esize);
#  ifdef WITH_MEM_VALGRIND
          VALGRIND_MAKE_MEM_UNDEFINED(ret, iter->pool->esize);
#  endif
          mempool_asan_unlock(iter->pool);
          return nullptr;
        }
        mempool_asan_unlock(iter->pool);
        return ret;
      }
      /* End `threadsafe` exception. */

      iter->curchunk = iter->curchunk->next;
      if (UNLIKELY(iter->curchunk == nullptr)) {
        if (ret->freeword == FREEWORD) {
          BLI_asan_poison(ret, iter->pool->esize);
#  ifdef WITH_MEM_VALGRIND
          VALGRIND_MAKE_MEM_UNDEFINED(ret, iter->pool->esize);
#  endif
          mempool_asan_unlock(iter->pool);
          return nullptr;
        }
        mempool_asan_unlock(iter->pool);
        return ret;
      }

      curnode = CHUNK_DATA(iter->curchunk);
    }

    if (ret->freeword == FREEWORD) {
      BLI_asan_poison(ret, iter->pool->esize);
#  ifdef WITH_MEM_VALGRIND
      VALGRIND_MAKE_MEM_UNDEFINED(ret, iter->pool->esize);
#  endif
    }
    else {
      break;
    }
  } while (true);

  mempool_asan_unlock(iter->pool);
  return ret;
}

#endif

void BLI_mempool_clear_ex(BLI_mempool *pool, const int elem_num_reserve)
{
  BLI_mempool_chunk *mpchunk;
  BLI_mempool_chunk *mpchunk_next;
  uint maxchunks;

  BLI_mempool_chunk *chunks_temp;
  BLI_freenode *last_tail = nullptr;

#ifdef WITH_MEM_VALGRIND
  VALGRIND_DESTROY_MEMPOOL(pool);
  VALGRIND_CREATE_MEMPOOL(pool, 0, false);
#endif

  if (elem_num_reserve == -1) {
    maxchunks = pool->maxchunks;
  }
  else {
    maxchunks = mempool_maxchunks(uint(elem_num_reserve), pool->pchunk);
  }

  /* Free all after 'pool->maxchunks'. */
  mpchunk = mempool_chunk_find(pool->chunks, maxchunks - 1);
  if (mpchunk && mpchunk->next) {
    /* terminate */
    mpchunk_next = mpchunk->next;
    mpchunk->next = nullptr;
    mpchunk = mpchunk_next;

    do {
      mpchunk_next = mpchunk->next;
      mempool_chunk_free(mpchunk, pool);
    } while ((mpchunk = mpchunk_next));
  }

  /* re-initialize */
  pool->free = nullptr;
  pool->totused = 0;
  chunks_temp = pool->chunks;
  pool->chunks = nullptr;
  pool->chunk_tail = nullptr;

  while ((mpchunk = chunks_temp)) {
    chunks_temp = mpchunk->next;
    last_tail = mempool_chunk_add(pool, mpchunk, last_tail);
  }
}

void BLI_mempool_clear(BLI_mempool *pool)
{
  BLI_mempool_clear_ex(pool, -1);
}

void BLI_mempool_destroy(BLI_mempool *pool)
{
  mempool_chunk_free_all(pool->chunks, pool);

#ifdef WITH_MEM_VALGRIND
  VALGRIND_DESTROY_MEMPOOL(pool);
#endif

  MEM_freeN(pool);
}

#ifndef NDEBUG
void BLI_mempool_set_memory_debug()
{
  mempool_debug_memset = true;
}
#endif
