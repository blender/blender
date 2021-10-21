#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"

#include "atomic_ops.h"  //for one atomic variable exposed to C, curchunk_threaded_shared in BLI_lfmempool_iter
#include <algorithm>
#include <atomic>

#include "MEM_guardedalloc.h"

#include "BLI_mempool_lockfree.h"

/* NOTE: copied from BLO_blend_defs.h, don't use here because we're in BLI. */
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
 * Important that this value is an is _not_  aligned with `sizeof(void *)`.
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

namespace blender {

struct LockFreeElem {
  struct LockFreeElem *next;
  uintptr_t freeword;
};

struct LockFreeChunk {
  struct LockFreeChunk *next, *prev;

  // we are convienently aligned to 16 bytes here
};

static void *data_from_chunk(LockFreeChunk *chunk)
{
  return reinterpret_cast<void *>(chunk + 1);
}

static LockFreeElem *elem_from_chunk(LockFreeChunk *chunk)
{
  return reinterpret_cast<LockFreeElem *>(data_from_chunk(chunk));
}

static LockFreeElem *elem_next(LockFreeElem *elem, int esize)
{
  char *ptr = reinterpret_cast<char *>(elem);
  ptr += esize;
  return reinterpret_cast<LockFreeElem *>(ptr);
}

static_assert(sizeof(std::atomic<void *>) == sizeof(void *), "std:atomic has space overhead!");

struct LockFreePool {
  struct {
    std::atomic<LockFreeChunk *> first;
    std::atomic<LockFreeChunk *> last;
  } chunks;

  std::atomic<int> totchunk;
  std::atomic<int> totused;

  std::atomic<LockFreeElem *> free_elem;

  int esize, psize, csize;

  LockFreePool(int _esize, int psize) : psize(psize)
  {
    esize = std::max(_esize, (int)(sizeof(void *) * 2));

    if (esize & 8) {
      esize += 8 - (esize & 7);
    }

    csize = esize * psize + sizeof(LockFreeChunk);
    totused.store(0);

    free_elem.store(nullptr);
    chunks.first.store(nullptr);
    chunks.last.store(nullptr);
  }

  ~LockFreePool()
  {
    LockFreeChunk *chunk, *next;

    for (chunk = chunks.last; chunk; chunk = next) {
      next = chunk->prev;

      MEM_freeN(reinterpret_cast<void *>(chunk));
    }
  }

  void add_chunk()
  {
    LockFreeChunk *chunk = reinterpret_cast<LockFreeChunk *>(MEM_mallocN(csize, "LockFreeChunk"));
    LockFreeElem *elem = elem_from_chunk(chunk), *last;
    LockFreeElem *first = elem;

    chunk->next = chunk->prev = nullptr;

    for (int i = 0; i < psize; i++, elem = elem_next(elem, esize)) {
      elem->freeword = FREEWORD;

      if (i == psize - 1) {
        last = elem;
        elem->next = nullptr;
      }
      else {
        elem->next = elem_next(elem, esize);
      }
    }

    // last->next = free_elem
    // free_elem = last;

    while (1) {
      last->next = free_elem.load();

      if (free_elem.compare_exchange_strong(last->next, first)) {
        break;
      }
    }

    while (1) {
      chunk->prev = chunks.last.load();

      if (chunks.last.compare_exchange_strong(chunk->prev, chunk)) {
        if (!chunk->prev) {
          // chunks.first is not accessed in threading cases, only when pool
          // is destroyed
          chunks.first.store(chunk);
        }
        else {
          chunk->prev->next = chunk;
        }
        break;
      }
    }
  }

  void *alloc()
  {
    totused++;

    while (1) {
      if (!free_elem.load()) {
        add_chunk();
      }

      LockFreeElem *cur = free_elem.load();

      if (free_elem.compare_exchange_strong(cur, cur->next)) {
        cur->freeword = 0;
        return reinterpret_cast<void *>(cur);
      }
    }
  }

  void free(void *mem)
  {
    totused--;
    LockFreeElem *elem = reinterpret_cast<LockFreeElem *>(mem);

    elem->freeword = FREEWORD;

    while (!free_elem.compare_exchange_strong(elem->next, elem)) {
    }
  }
};

static LockFreePool *cast_pool(BLI_lfmempool *pool)
{
  return reinterpret_cast<LockFreePool *>(pool);
}

extern "C" {

BLI_lfmempool *BLI_lfmempool_create(int esize, int psize)
{
  LockFreePool *pool = OBJECT_GUARDED_NEW(LockFreePool, esize, psize);
  return reinterpret_cast<BLI_lfmempool *>(pool);
}

void BLI_lfmempool_destroy(BLI_lfmempool *pool)
{
  OBJECT_GUARDED_DELETE(cast_pool(pool), LockFreePool);
}

void *BLI_lfmempool_alloc(BLI_lfmempool *pool)
{
  return cast_pool(pool)->alloc();
}

void *BLI_lfmempool_calloc(BLI_lfmempool *_pool)
{
  void *mem = BLI_lfmempool_alloc(_pool);
  LockFreePool *pool = cast_pool(_pool);

  memset(mem, 0, pool->esize);

  return mem;
}

void BLI_lfmempool_free(BLI_lfmempool *pool, void *mem)
{
  return cast_pool(pool)->free(mem);
}

void BLI_lfmempool_iternew(BLI_lfmempool *_pool, BLI_lfmempool_iter *iter)
{
  LockFreePool *pool = cast_pool(_pool);
  iter->pool = _pool;
  iter->chunk = reinterpret_cast<void *>(pool->chunks.first.load());
  iter->i = 0;
  iter->curchunk_threaded_shared = nullptr;
}

static void *chunk_next(void *vchunk)
{
  LockFreeChunk *chunk = reinterpret_cast<LockFreeChunk *>(vchunk);

  return reinterpret_cast<void *>(chunk->next);
}

void *BLI_lfmempool_iterstep_threadsafe(BLI_lfmempool_iter *iter)
{
  if (!iter->chunk) {
    return nullptr;
  }

  LockFreePool *pool = cast_pool(iter->pool);
  LockFreeChunk *chunk = reinterpret_cast<LockFreeChunk *>(iter->chunk);

  char *data = reinterpret_cast<char *>(data_from_chunk(chunk));
  void *ret = reinterpret_cast<void *>(data + pool->esize * iter->i);

  iter->i++;

  if (iter->i >= pool->psize) {
    iter->i = 0;
    /* Begin unique to the `threadsafe` version of this function. */
    for (iter->chunk = *iter->curchunk_threaded_shared;
         (iter->chunk != NULL) && (atomic_cas_ptr((void **)iter->curchunk_threaded_shared,
                                                  iter->chunk,
                                                  chunk_next(iter->chunk)) != iter->chunk);
         iter->chunk = *iter->curchunk_threaded_shared) {
      /* pass. */
    }
    iter->chunk = reinterpret_cast<void *>(chunk->next);
  }

  LockFreeElem *elem = reinterpret_cast<LockFreeElem *>(ret);
  if (elem->freeword == FREEWORD) {
    return BLI_lfmempool_iterstep_threadsafe(iter);
  }

  return ret;
}

void *BLI_lfmempool_iterstep(BLI_lfmempool_iter *iter)
{
  if (!iter->chunk) {
    return nullptr;
  }

  LockFreePool *pool = cast_pool(iter->pool);
  LockFreeChunk *chunk = reinterpret_cast<LockFreeChunk *>(iter->chunk);

  char *data = reinterpret_cast<char *>(data_from_chunk(chunk));
  void *ret = reinterpret_cast<void *>(data + pool->esize * iter->i);

  iter->i++;

  if (iter->i >= pool->psize) {
    iter->i = 0;
    iter->chunk = reinterpret_cast<void *>(chunk->next);
  }

  LockFreeElem *elem = reinterpret_cast<LockFreeElem *>(ret);
  if (elem->freeword == FREEWORD) {
    return BLI_lfmempool_iterstep(iter);
  }

  return ret;
}

void *BLI_lfmempool_findelem(BLI_lfmempool *pool, int index)
{
  BLI_lfmempool_iter iter;
  BLI_lfmempool_iternew(pool, &iter);
  void *item = BLI_lfmempool_iterstep(&iter);

  int i = 0;

  for (; item; item = BLI_lfmempool_iterstep(&iter), i++) {
    if (i == index) {
      return item;
    }
  }

  return nullptr;
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
ParallelLFMempoolTaskData *lfmempool_iter_threadsafe_create(BLI_lfmempool *pool,
                                                            const size_t num_iter)
{
  BLI_assert(pool->flag & BLI_MEMPOOL_ALLOW_ITER);

  ParallelLFMempoolTaskData *iter_arr = (ParallelLFMempoolTaskData *)MEM_mallocN(
      sizeof(*iter_arr) * num_iter, __func__);
  LockFreeChunk **curchunk_threaded_shared = (LockFreeChunk **)MEM_mallocN(sizeof(void *),
                                                                           __func__);

  BLI_lfmempool_iternew(pool, &iter_arr->ts_iter);

  *curchunk_threaded_shared = reinterpret_cast<LockFreeChunk *>(iter_arr->ts_iter.chunk);
  iter_arr->ts_iter.curchunk_threaded_shared = reinterpret_cast<void **>(curchunk_threaded_shared);
  for (size_t i = 1; i < num_iter; i++) {
    iter_arr[i].ts_iter = iter_arr[0].ts_iter;
    *curchunk_threaded_shared = ((*curchunk_threaded_shared) ? (*curchunk_threaded_shared)->next :
                                                               NULL);
    iter_arr[i].ts_iter.chunk = reinterpret_cast<void *>(*curchunk_threaded_shared);
  }

  return iter_arr;
}

void lfmempool_iter_threadsafe_destroy(ParallelLFMempoolTaskData *iter_arr)
{
  BLI_assert(iter_arr->ts_iter.curchunk_threaded_shared != NULL);

  MEM_freeN(iter_arr->ts_iter.curchunk_threaded_shared);
  MEM_freeN(iter_arr);
}
}
}  // namespace blender
