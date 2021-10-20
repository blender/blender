#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <atomic>

#include "MEM_guardedalloc.h"

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

typedef struct BLI_lfmempool BLI_lfmempool;

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

  LockFreePool(int esize, int psize) : esize(esize), psize(psize)
  {
    esize = std::max(esize, (int)(sizeof(void *) * 2));
    csize = esize * psize + sizeof(LockFreeChunk);
  }

  ~LockFreePool()
  {
    LockFreeChunk *chunk, *next;

    for (chunk = chunks.first; chunk; chunk = next) {
      next = chunk->next;

      OBJECT_GUARDED_DELETE(chunk, LockFreeChunk);
    }
  }

  void add_chunk()
  {
    LockFreeChunk *chunk = OBJECT_GUARDED_NEW(LockFreeChunk);
    LockFreeElem *elem = elem_from_chunk(chunk), *last;

    chunk->next = chunk->prev = nullptr;

    for (int i = 0; i < psize; i++, elem = elem_next(elem, esize)) {
      elem->next = i == psize - 1 ? nullptr : elem_next(elem, esize);
      elem->freeword = FREEWORD;

      if (i == psize - 1) {
        last = elem;
      }
    }

    // last->next = free_elem
    // free_elem = last;

    while (1) {
      last->next = free_elem.load();

      if (free_elem.compare_exchange_strong(last->next, last)) {
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
        break;
      }
    }
  }

  void *alloc()
  {
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

typedef struct BLI_lfmempool_iter {
  void *chunk;
  BLI_lfmempool *pool;
  int i;
} BLI_lfmempool_iter;

void BLI_lfmempool_destroy(BLI_lfmempool *pool)
{
  OBJECT_GUARDED_DELETE(cast_pool(pool), LockFreePool);
}

void *BLI_lfmempool_alloc(BLI_lfmempool *pool)
{
  return cast_pool(pool)->alloc();
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
}
}  // namespace blender
