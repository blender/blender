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
 * Simple, fast memory allocator for allocating many elements of the same size.
 *
 * Supports:
 *
 * - Freeing chunks.
 * - Iterating over allocated chunks
 *   (optionally when using the #BLI_MEMPOOL_ALLOW_ITER flag).
 */

#include <stdlib.h>
#include <string.h>

#include "atomic_ops.h"

#include "BLI_utildefines.h"

#include "BLI_mempool.h" /* own include */

#include "MEM_guardedalloc.h"

#include "BLI_strict_flags.h" /* keep last */

#ifdef WITH_MEM_VALGRIND
#  include "valgrind/memcheck.h"
#endif

/* note: copied from BLO_blend_defs.h, don't use here because we're in BLI */
#ifdef __BIG_ENDIAN__
/* Big Endian */
#  define MAKE_ID(a, b, c, d) ((int)(a) << 24 | (int)(b) << 16 | (c) << 8 | (d))
#  define MAKE_ID_8(a, b, c, d, e, f, g, h) \
    ((int64_t)(a) << 56 | (int64_t)(b) << 48 | (int64_t)(c) << 40 | (int64_t)(d) << 32 | \
     (int64_t)(e) << 24 | (int64_t)(f) << 16 | (int64_t)(g) << 8 | (h))
#else
/* Little Endian */
#  define MAKE_ID(a, b, c, d) ((int)(d) << 24 | (int)(c) << 16 | (b) << 8 | (a))
#  define MAKE_ID_8(a, b, c, d, e, f, g, h) \
    ((int64_t)(h) << 56 | (int64_t)(g) << 48 | (int64_t)(f) << 40 | (int64_t)(e) << 32 | \
     (int64_t)(d) << 24 | (int64_t)(c) << 16 | (int64_t)(b) << 8 | (a))
#endif

/**
 * Important that this value is an is _not_  aligned with ``sizeof(void *)``.
 * So having a pointer to 2/4/8... aligned memory is enough to ensure
 * the freeword will never be used.
 * To be safe, use a word that's the same in both directions.
 */
#define FREEWORD \
  ((sizeof(void *) > sizeof(int32_t)) ? MAKE_ID_8('e', 'e', 'r', 'f', 'f', 'r', 'e', 'e') : \
                                        MAKE_ID('e', 'f', 'f', 'e'))

/**
 * The 'used' word just needs to be set to something besides FREEWORD.
 */
#define USEDWORD MAKE_ID('u', 's', 'e', 'd')

/* Currently totalloc isn't used. */
// #define USE_TOTALLOC

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
typedef struct BLI_freenode {
  struct BLI_freenode *next;
  /** Used to identify this as a freed node. */
  intptr_t freeword;
} BLI_freenode;

/**
 * A chunk of memory in the mempool stored in
 * #BLI_mempool.chunks as a double linked list.
 */
typedef struct BLI_mempool_chunk {
  struct BLI_mempool_chunk *next;
} BLI_mempool_chunk;

/**
 * The mempool, stores and tracks memory \a chunks and elements within those chunks \a free.
 */
struct BLI_mempool {
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
  /* keeps aligned to 16 bits */

  /** Free element list. Interleaved into chunk datas. */
  BLI_freenode *free;
  /** Use to know how many chunks to keep for #BLI_mempool_clear. */
  uint maxchunks;
  /** Number of elements currently in use. */
  uint totused;
#ifdef USE_TOTALLOC
  /** Number of elements allocated in total. */
  uint totalloc;
#endif
};

#define MEMPOOL_ELEM_SIZE_MIN (sizeof(void *) * 2)

#define CHUNK_DATA(chunk) (CHECK_TYPE_INLINE(chunk, BLI_mempool_chunk *), (void *)((chunk) + 1))

#define NODE_STEP_NEXT(node) ((void *)((char *)(node) + esize))
#define NODE_STEP_PREV(node) ((void *)((char *)(node)-esize))

/** Extra bytes implicitly used for every chunk alloc. */
#define CHUNK_OVERHEAD (uint)(MEM_SIZE_OVERHEAD + sizeof(BLI_mempool_chunk))

#ifdef USE_CHUNK_POW2
static uint power_of_2_max_u(uint x)
{
  x -= 1;
  x = x | (x >> 1);
  x = x | (x >> 2);
  x = x | (x >> 4);
  x = x | (x >> 8);
  x = x | (x >> 16);
  return x + 1;
}
#endif

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
BLI_INLINE uint mempool_maxchunks(const uint totelem, const uint pchunk)
{
  return (totelem <= pchunk) ? 1 : ((totelem / pchunk) + 1);
}

static BLI_mempool_chunk *mempool_chunk_alloc(BLI_mempool *pool)
{
  return MEM_mallocN(sizeof(BLI_mempool_chunk) + (size_t)pool->csize, "BLI_Mempool Chunk");
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
    BLI_assert(pool->chunks == NULL);
    pool->chunks = mpchunk;
  }

  mpchunk->next = NULL;
  pool->chunk_tail = mpchunk;

  if (UNLIKELY(pool->free == NULL)) {
    pool->free = curnode;
  }

  /* loop through the allocated data, building the pointer structures */
  j = pool->pchunk;
  if (pool->flag & BLI_MEMPOOL_ALLOW_ITER) {
    while (j--) {
      curnode->next = NODE_STEP_NEXT(curnode);
      curnode->freeword = FREEWORD;
      curnode = curnode->next;
    }
  }
  else {
    while (j--) {
      curnode->next = NODE_STEP_NEXT(curnode);
      curnode = curnode->next;
    }
  }

  /* terminate the list (rewind one)
   * will be overwritten if 'curnode' gets passed in again as 'last_tail' */
  curnode = NODE_STEP_PREV(curnode);
  curnode->next = NULL;

#ifdef USE_TOTALLOC
  pool->totalloc += pool->pchunk;
#endif

  /* final pointer in the previously allocated chunk is wrong */
  if (last_tail) {
    last_tail->next = CHUNK_DATA(mpchunk);
  }

  return curnode;
}

static void mempool_chunk_free(BLI_mempool_chunk *mpchunk)
{
  MEM_freeN(mpchunk);
}

static void mempool_chunk_free_all(BLI_mempool_chunk *mpchunk)
{
  BLI_mempool_chunk *mpchunk_next;

  for (; mpchunk; mpchunk = mpchunk_next) {
    mpchunk_next = mpchunk->next;
    mempool_chunk_free(mpchunk);
  }
}

BLI_mempool *BLI_mempool_create(uint esize, uint totelem, uint pchunk, uint flag)
{
  BLI_mempool *pool;
  BLI_freenode *last_tail = NULL;
  uint i, maxchunks;

  /* allocate the pool structure */
  pool = MEM_mallocN(sizeof(BLI_mempool), "memory pool");

  /* set the elem size */
  if (esize < (int)MEMPOOL_ELEM_SIZE_MIN) {
    esize = (int)MEMPOOL_ELEM_SIZE_MIN;
  }

  if (flag & BLI_MEMPOOL_ALLOW_ITER) {
    esize = MAX2(esize, (uint)sizeof(BLI_freenode));
  }

  maxchunks = mempool_maxchunks(totelem, pchunk);

  pool->chunks = NULL;
  pool->chunk_tail = NULL;
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
    uint final_size = (uint)MEM_SIZE_OVERHEAD + (uint)sizeof(BLI_mempool_chunk) + pool->csize;
    BLI_assert(((uint)power_of_2_max_u(final_size) - final_size) < pool->esize);
  }
#endif

  pool->pchunk = pchunk;
  pool->flag = flag;
  pool->free = NULL; /* mempool_chunk_add assigns */
  pool->maxchunks = maxchunks;
#ifdef USE_TOTALLOC
  pool->totalloc = 0;
#endif
  pool->totused = 0;

  if (totelem) {
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

  if (UNLIKELY(pool->free == NULL)) {
    /* Need to allocate a new chunk. */
    BLI_mempool_chunk *mpchunk = mempool_chunk_alloc(pool);
    mempool_chunk_add(pool, mpchunk, NULL);
  }

  free_pop = pool->free;

  BLI_assert(pool->chunk_tail->next == NULL);

  if (pool->flag & BLI_MEMPOOL_ALLOW_ITER) {
    free_pop->freeword = USEDWORD;
  }

  pool->free = free_pop->next;
  pool->totused++;

#ifdef WITH_MEM_VALGRIND
  VALGRIND_MEMPOOL_ALLOC(pool, free_pop, pool->esize);
#endif

  return (void *)free_pop;
}

void *BLI_mempool_calloc(BLI_mempool *pool)
{
  void *retval = BLI_mempool_alloc(pool);
  memset(retval, 0, (size_t)pool->esize);
  return retval;
}

/**
 * Free an element from the mempool.
 *
 * \note doesn't protect against double frees, take care!
 */
void BLI_mempool_free(BLI_mempool *pool, void *addr)
{
  BLI_freenode *newhead = addr;

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
      BLI_assert(!"Attempt to free data which is not in pool.\n");
    }
  }

  /* Enable for debugging. */
  if (UNLIKELY(mempool_debug_memset)) {
    memset(addr, 255, pool->esize);
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
    mempool_chunk_free_all(first->next);
    first->next = NULL;
    pool->chunk_tail = first;

#ifdef USE_TOTALLOC
    pool->totalloc = pool->pchunk;
#endif

    /* Temp alloc so valgrind doesn't complain when setting free'd blocks 'next'. */
#ifdef WITH_MEM_VALGRIND
    VALGRIND_MEMPOOL_ALLOC(pool, CHUNK_DATA(first), pool->csize);
#endif

    curnode = CHUNK_DATA(first);
    pool->free = curnode;

    j = pool->pchunk;
    while (j--) {
      curnode->next = NODE_STEP_NEXT(curnode);
      curnode = curnode->next;
    }
    curnode = NODE_STEP_PREV(curnode);
    curnode->next = NULL; /* terminate the list */

#ifdef WITH_MEM_VALGRIND
    VALGRIND_MEMPOOL_FREE(pool, CHUNK_DATA(first));
#endif
  }
}

int BLI_mempool_len(BLI_mempool *pool)
{
  return (int)pool->totused;
}

void *BLI_mempool_findelem(BLI_mempool *pool, uint index)
{
  BLI_assert(pool->flag & BLI_MEMPOOL_ALLOW_ITER);

  if (index < pool->totused) {
    /* We could have some faster mem chunk stepping code inline. */
    BLI_mempool_iter iter;
    void *elem;
    BLI_mempool_iternew(pool, &iter);
    for (elem = BLI_mempool_iterstep(&iter); index-- != 0; elem = BLI_mempool_iterstep(&iter)) {
      /* pass */
    }
    return elem;
  }

  return NULL;
}

/**
 * Fill in \a data with pointers to each element of the mempool,
 * to create lookup table.
 *
 * \param pool: Pool to create a table from.
 * \param data: array of pointers at least the size of 'pool->totused'
 */
void BLI_mempool_as_table(BLI_mempool *pool, void **data)
{
  BLI_mempool_iter iter;
  void *elem;
  void **p = data;
  BLI_assert(pool->flag & BLI_MEMPOOL_ALLOW_ITER);
  BLI_mempool_iternew(pool, &iter);
  while ((elem = BLI_mempool_iterstep(&iter))) {
    *p++ = elem;
  }
  BLI_assert((uint)(p - data) == pool->totused);
}

/**
 * A version of #BLI_mempool_as_table that allocates and returns the data.
 */
void **BLI_mempool_as_tableN(BLI_mempool *pool, const char *allocstr)
{
  void **data = MEM_mallocN((size_t)pool->totused * sizeof(void *), allocstr);
  BLI_mempool_as_table(pool, data);
  return data;
}

/**
 * Fill in \a data with the contents of the mempool.
 */
void BLI_mempool_as_array(BLI_mempool *pool, void *data)
{
  const uint esize = pool->esize;
  BLI_mempool_iter iter;
  char *elem, *p = data;
  BLI_assert(pool->flag & BLI_MEMPOOL_ALLOW_ITER);
  BLI_mempool_iternew(pool, &iter);
  while ((elem = BLI_mempool_iterstep(&iter))) {
    memcpy(p, elem, (size_t)esize);
    p = NODE_STEP_NEXT(p);
  }
  BLI_assert((uint)(p - (char *)data) == pool->totused * esize);
}

/**
 * A version of #BLI_mempool_as_array that allocates and returns the data.
 */
void *BLI_mempool_as_arrayN(BLI_mempool *pool, const char *allocstr)
{
  char *data = MEM_malloc_arrayN(pool->totused, pool->esize, allocstr);
  BLI_mempool_as_array(pool, data);
  return data;
}

/**
 * Initialize a new mempool iterator, #BLI_MEMPOOL_ALLOW_ITER flag must be set.
 */
void BLI_mempool_iternew(BLI_mempool *pool, BLI_mempool_iter *iter)
{
  BLI_assert(pool->flag & BLI_MEMPOOL_ALLOW_ITER);

  iter->pool = pool;
  iter->curchunk = pool->chunks;
  iter->curindex = 0;

  iter->curchunk_threaded_shared = NULL;
}

/**
 * Initialize an array of mempool iterators, #BLI_MEMPOOL_ALLOW_ITER flag must be set.
 *
 * This is used in threaded code, to generate as much iterators as needed
 * (each task should have its own),
 * such that each iterator goes over its own single chunk,
 * and only getting the next chunk to iterate over has to be
 * protected against concurrency (which can be done in a lockless way).
 *
 * To be used when creating a task for each single item in the pool is totally overkill.
 *
 * See BLI_task_parallel_mempool implementation for detailed usage example.
 */
BLI_mempool_iter *BLI_mempool_iter_threadsafe_create(BLI_mempool *pool, const size_t num_iter)
{
  BLI_assert(pool->flag & BLI_MEMPOOL_ALLOW_ITER);

  BLI_mempool_iter *iter_arr = MEM_mallocN(sizeof(*iter_arr) * num_iter, __func__);
  BLI_mempool_chunk **curchunk_threaded_shared = MEM_mallocN(sizeof(void *), __func__);

  BLI_mempool_iternew(pool, iter_arr);

  *curchunk_threaded_shared = iter_arr->curchunk;
  iter_arr->curchunk_threaded_shared = curchunk_threaded_shared;

  for (size_t i = 1; i < num_iter; i++) {
    iter_arr[i] = iter_arr[0];
    *curchunk_threaded_shared = iter_arr[i].curchunk = ((*curchunk_threaded_shared) ?
                                                            (*curchunk_threaded_shared)->next :
                                                            NULL);
  }

  return iter_arr;
}

void BLI_mempool_iter_threadsafe_free(BLI_mempool_iter *iter_arr)
{
  BLI_assert(iter_arr->curchunk_threaded_shared != NULL);

  MEM_freeN(iter_arr->curchunk_threaded_shared);
  MEM_freeN(iter_arr);
}

#if 0
/* unoptimized, more readable */

static void *bli_mempool_iternext(BLI_mempool_iter *iter)
{
  void *ret = NULL;

  if (iter->curchunk == NULL || !iter->pool->totused) {
    return ret;
  }

  ret = ((char *)CHUNK_DATA(iter->curchunk)) + (iter->pool->esize * iter->curindex);

  iter->curindex++;

  if (iter->curindex == iter->pool->pchunk) {
    iter->curindex = 0;
    if (iter->curchunk_threaded_shared) {
      while (1) {
        iter->curchunk = *iter->curchunk_threaded_shared;
        if (iter->curchunk == NULL) {
          return ret;
        }
        if (atomic_cas_ptr((void **)iter->curchunk_threaded_shared,
                           iter->curchunk,
                           iter->curchunk->next) == iter->curchunk) {
          break;
        }
      }
    }
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

#else

/* optimized version of code above */

/**
 * Step over the iterator, returning the mempool item or NULL.
 */
void *BLI_mempool_iterstep(BLI_mempool_iter *iter)
{
  if (UNLIKELY(iter->curchunk == NULL)) {
    return NULL;
  }

  const uint esize = iter->pool->esize;
  BLI_freenode *curnode = POINTER_OFFSET(CHUNK_DATA(iter->curchunk), (esize * iter->curindex));
  BLI_freenode *ret;
  do {
    ret = curnode;

    if (++iter->curindex != iter->pool->pchunk) {
      curnode = POINTER_OFFSET(curnode, esize);
    }
    else {
      iter->curindex = 0;
      if (iter->curchunk_threaded_shared) {
        for (iter->curchunk = *iter->curchunk_threaded_shared;
             (iter->curchunk != NULL) && (atomic_cas_ptr((void **)iter->curchunk_threaded_shared,
                                                         iter->curchunk,
                                                         iter->curchunk->next) != iter->curchunk);
             iter->curchunk = *iter->curchunk_threaded_shared) {
          /* pass. */
        }

        if (UNLIKELY(iter->curchunk == NULL)) {
          return (ret->freeword == FREEWORD) ? NULL : ret;
        }
      }
      iter->curchunk = iter->curchunk->next;
      if (UNLIKELY(iter->curchunk == NULL)) {
        return (ret->freeword == FREEWORD) ? NULL : ret;
      }
      curnode = CHUNK_DATA(iter->curchunk);
    }
  } while (ret->freeword == FREEWORD);

  return ret;
}

#endif

/**
 * Empty the pool, as if it were just created.
 *
 * \param pool: The pool to clear.
 * \param totelem_reserve: Optionally reserve how many items should be kept from clearing.
 */
void BLI_mempool_clear_ex(BLI_mempool *pool, const int totelem_reserve)
{
  BLI_mempool_chunk *mpchunk;
  BLI_mempool_chunk *mpchunk_next;
  uint maxchunks;

  BLI_mempool_chunk *chunks_temp;
  BLI_freenode *last_tail = NULL;

#ifdef WITH_MEM_VALGRIND
  VALGRIND_DESTROY_MEMPOOL(pool);
  VALGRIND_CREATE_MEMPOOL(pool, 0, false);
#endif

  if (totelem_reserve == -1) {
    maxchunks = pool->maxchunks;
  }
  else {
    maxchunks = mempool_maxchunks((uint)totelem_reserve, pool->pchunk);
  }

  /* Free all after 'pool->maxchunks'. */
  mpchunk = mempool_chunk_find(pool->chunks, maxchunks - 1);
  if (mpchunk && mpchunk->next) {
    /* terminate */
    mpchunk_next = mpchunk->next;
    mpchunk->next = NULL;
    mpchunk = mpchunk_next;

    do {
      mpchunk_next = mpchunk->next;
      mempool_chunk_free(mpchunk);
    } while ((mpchunk = mpchunk_next));
  }

  /* re-initialize */
  pool->free = NULL;
  pool->totused = 0;
#ifdef USE_TOTALLOC
  pool->totalloc = 0;
#endif

  chunks_temp = pool->chunks;
  pool->chunks = NULL;
  pool->chunk_tail = NULL;

  while ((mpchunk = chunks_temp)) {
    chunks_temp = mpchunk->next;
    last_tail = mempool_chunk_add(pool, mpchunk, last_tail);
  }
}

/**
 * Wrap #BLI_mempool_clear_ex with no reserve set.
 */
void BLI_mempool_clear(BLI_mempool *pool)
{
  BLI_mempool_clear_ex(pool, -1);
}

/**
 * Free the mempool its self (and all elements).
 */
void BLI_mempool_destroy(BLI_mempool *pool)
{
  mempool_chunk_free_all(pool->chunks);

#ifdef WITH_MEM_VALGRIND
  VALGRIND_DESTROY_MEMPOOL(pool);
#endif

  MEM_freeN(pool);
}

#ifndef NDEBUG
void BLI_mempool_set_memory_debug(void)
{
  mempool_debug_memset = true;
}
#endif
