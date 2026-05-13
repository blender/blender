/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#undef DEBUG_MESSAGES

#include <cstdlib> /* for qsort */
#include <memory.h>
#include <mutex>

#include "MEM_CacheLimiterC-Api.h"
#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_mempool.h"
#include "BLI_string.h"

#include "IMB_cache.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

namespace blender {

#ifdef DEBUG_MESSAGES
#  if defined __GNUC__
#    define PRINT(format, args...) printf(format, ##args)
#  else
#    define PRINT(format, ...) printf(__VA_ARGS__)
#  endif
#else
#  define PRINT(format, ...)
#endif

static MEM_CacheLimiterC *limitor = nullptr;

/* Image buffers managed by a ImBufCache might be using their own caches (used by color
 * management). In practice this means that, for example, freeing ImBufCache used by MovieClip
 * will request freeing ImBufCache owned by ImBuf. Freeing ImBufCache needs to be thread-safe,
 * so regular mutex will not work here, hence the recursive lock. */
static std::recursive_mutex limitor_lock;

struct ImBufCache {
  char name[64];

  GHash *hash;
  GHashHashFP hashfp;
  GHashCmpFP cmpfp;
  ImBufCacheGetKeyDataFP getdatafp;

  ImBufCacheGetPriorityDataFP getprioritydatafp;
  ImBufCacheGetItemPriorityFP getitempriorityfp;
  ImBufCachePriorityDeleterFP prioritydeleterfp;

  BLI_mempool *keys_pool;
  BLI_mempool *items_pool;
  BLI_mempool *userkeys_pool;

  int keysize;

  void *last_userkey;

  int totseg, *points, proxy, render_flags; /* for visual statistics optimization */
  int pad;
};

struct ImBufCacheKey {
  ImBufCache *cache_owner;
  void *userkey;
};

struct ImBufCacheItem {
  ImBufCache *cache_owner;
  ImBuf *ibuf;
  MEM_CacheLimiterHandleC *c_handle;
  void *priority_data;
  /* Indicates that #ibuf is null, because there was an error during load. */
  bool added_empty;
};

static uint imbufcache_hashhash(const void *keyv)
{
  const ImBufCacheKey *key = static_cast<const ImBufCacheKey *>(keyv);

  return key->cache_owner->hashfp(key->userkey);
}

static bool imbufcache_hashcmp(const void *av, const void *bv)
{
  const ImBufCacheKey *a = static_cast<const ImBufCacheKey *>(av);
  const ImBufCacheKey *b = static_cast<const ImBufCacheKey *>(bv);

  return a->cache_owner->cmpfp(a->userkey, b->userkey);
}

static void imbufcache_keyfree(void *val)
{
  ImBufCacheKey *key = static_cast<ImBufCacheKey *>(val);

  BLI_mempool_free(key->cache_owner->userkeys_pool, key->userkey);

  BLI_mempool_free(key->cache_owner->keys_pool, key);
}

static void imbufcache_valfree(void *val)
{
  ImBufCacheItem *item = static_cast<ImBufCacheItem *>(val);
  ImBufCache *cache = item->cache_owner;

  PRINT("%s: cache '%s' free item %p buffer %p\n", __func__, cache->name, item, item->ibuf);

  if (item->c_handle) {
    limitor_lock.lock();
    MEM_CacheLimiter_unmanage(item->c_handle);
    limitor_lock.unlock();
  }

  if (item->ibuf) {
    IMB_freeImBuf(item->ibuf);
  }

  if (item->priority_data && cache->prioritydeleterfp) {
    cache->prioritydeleterfp(item->priority_data);
  }

  BLI_mempool_free(item->cache_owner->items_pool, item);
}

static void check_unused_keys(ImBufCache *cache)
{
  GHashIterator gh_iter;

  BLI_ghashIterator_init(&gh_iter, cache->hash);

  while (!BLI_ghashIterator_done(&gh_iter)) {
    const ImBufCacheKey *key = static_cast<const ImBufCacheKey *>(
        BLI_ghashIterator_getKey(&gh_iter));
    const ImBufCacheItem *item = static_cast<const ImBufCacheItem *>(
        BLI_ghashIterator_getValue(&gh_iter));

    BLI_ghashIterator_step(&gh_iter);

    if (item->added_empty) {
      /* Don't remove entries that have been added empty. Those indicate that the image couldn't be
       * loaded correctly. */
      continue;
    }

    bool remove = !item->ibuf;

    if (remove) {
      PRINT("%s: cache '%s' remove item %p without buffer\n", __func__, cache->name, item);
    }

    if (remove) {
      BLI_ghash_remove(cache->hash, key, imbufcache_keyfree, imbufcache_valfree);
    }
  }
}

static int compare_int(const void *av, const void *bv)
{
  const int *a = static_cast<int *>(const_cast<void *>(av));
  const int *b = static_cast<int *>(const_cast<void *>(bv));
  return *a - *b;
}

static void imbufcache_destructor(void *p)
{
  ImBufCacheItem *item = static_cast<ImBufCacheItem *>(p);

  if (item && item->ibuf) {
    ImBufCache *cache = item->cache_owner;

    PRINT("%s: cache '%s' destroy item %p buffer %p\n", __func__, cache->name, item, item->ibuf);

    IMB_freeImBuf(item->ibuf);

    item->ibuf = nullptr;
    item->c_handle = nullptr;

    /* force cached segments to be updated */
    MEM_SAFE_DELETE(cache->points);
  }
}

static size_t get_size_in_memory(ImBuf *ibuf)
{
  /* Keep textures in the memory to avoid constant file reload on viewport update. */
  if (ibuf->userflags & IB_PERSISTENT) {
    return 0;
  }

  return IMB_get_size_in_memory(ibuf);
}
static size_t get_item_size(void *p)
{
  size_t size = sizeof(ImBufCacheItem);
  ImBufCacheItem *item = static_cast<ImBufCacheItem *>(p);

  if (item->ibuf) {
    size += get_size_in_memory(item->ibuf);
  }

  return size;
}

static int get_item_priority(void *item_v, int default_priority)
{
  ImBufCacheItem *item = static_cast<ImBufCacheItem *>(item_v);
  ImBufCache *cache = item->cache_owner;
  int priority;

  if (!cache->getitempriorityfp) {
    PRINT("%s: cache '%s' item %p use default priority %d\n",
          __func__,
          cache->name,
          item,
          default_priority);

    return default_priority;
  }

  priority = cache->getitempriorityfp(cache->last_userkey, item->priority_data);

  PRINT("%s: cache '%s' item %p priority %d\n", __func__, cache->name, item, priority);

  return priority;
}

static bool get_item_destroyable(void *item_v)
{
  ImBufCacheItem *item = static_cast<ImBufCacheItem *>(item_v);
  if (item->ibuf == nullptr) {
    return true;
  }
  /* IB_BITMAPDIRTY means image was modified from inside blender and
   * changes are not saved to disk.
   *
   * Such buffers are never to be freed.
   */
  if ((item->ibuf->userflags & IB_BITMAPDIRTY) || (item->ibuf->userflags & IB_PERSISTENT)) {
    return false;
  }
  return true;
}

void IMB_cache_init()
{
  limitor = new_MEM_CacheLimiter(imbufcache_destructor, get_item_size);

  MEM_CacheLimiter_ItemPriority_Func_set(limitor, get_item_priority);
  MEM_CacheLimiter_ItemDestroyable_Func_set(limitor, get_item_destroyable);
}

void IMB_cache_destruct()
{
  if (limitor) {
    delete_MEM_CacheLimiter(limitor);
    limitor = nullptr;
  }
}

ImBufCache *IMB_cache_create(const char *name, int keysize, GHashHashFP hashfp, GHashCmpFP cmpfp)
{
  ImBufCache *cache;

  PRINT("%s: cache '%s' create\n", __func__, name);

  cache = MEM_new_zeroed<ImBufCache>("ImBufCache");

  STRNCPY(cache->name, name);

  cache->keys_pool = BLI_mempool_create(sizeof(ImBufCacheKey), 0, 64, BLI_MEMPOOL_NOP);
  cache->items_pool = BLI_mempool_create(sizeof(ImBufCacheItem), 0, 64, BLI_MEMPOOL_NOP);
  cache->userkeys_pool = BLI_mempool_create(keysize, 0, 64, BLI_MEMPOOL_NOP);
  cache->hash = BLI_ghash_new(
      imbufcache_hashhash, imbufcache_hashcmp, "MovieClip ImBuf cache hash");

  cache->keysize = keysize;
  cache->hashfp = hashfp;
  cache->cmpfp = cmpfp;
  cache->proxy = -1;

  return cache;
}

void IMB_cache_set_getdata_callback(ImBufCache *cache, ImBufCacheGetKeyDataFP getdatafp)
{
  cache->getdatafp = getdatafp;
}

void IMB_cache_set_priority_callback(ImBufCache *cache,
                                     ImBufCacheGetPriorityDataFP getprioritydatafp,
                                     ImBufCacheGetItemPriorityFP getitempriorityfp,
                                     ImBufCachePriorityDeleterFP prioritydeleterfp)
{
  cache->last_userkey = MEM_new_uninitialized(cache->keysize, "movie cache last user key");

  cache->getprioritydatafp = getprioritydatafp;
  cache->getitempriorityfp = getitempriorityfp;
  cache->prioritydeleterfp = prioritydeleterfp;
}

static void do_imbufcache_put(ImBufCache *cache, void *userkey, ImBuf *ibuf, bool need_lock)
{
  ImBufCacheKey *key;
  ImBufCacheItem *item;

  if (!limitor) {
    IMB_cache_init();
  }

  if (ibuf != nullptr) {
    IMB_refImBuf(ibuf);
  }

  key = static_cast<ImBufCacheKey *>(BLI_mempool_alloc(cache->keys_pool));
  key->cache_owner = cache;
  key->userkey = BLI_mempool_alloc(cache->userkeys_pool);
  memcpy(key->userkey, userkey, cache->keysize);

  item = static_cast<ImBufCacheItem *>(BLI_mempool_alloc(cache->items_pool));

  PRINT("%s: cache '%s' put %p, item %p\n", __func__, cache->name, ibuf, item);

  item->ibuf = ibuf;
  item->cache_owner = cache;
  item->c_handle = nullptr;
  item->priority_data = nullptr;
  item->added_empty = ibuf == nullptr;

  if (cache->getprioritydatafp) {
    item->priority_data = cache->getprioritydatafp(userkey);
  }

  BLI_ghash_reinsert(cache->hash, key, item, imbufcache_keyfree, imbufcache_valfree);

  if (cache->last_userkey) {
    memcpy(cache->last_userkey, userkey, cache->keysize);
  }

  if (need_lock) {
    limitor_lock.lock();
  }

  item->c_handle = MEM_CacheLimiter_insert(limitor, item);

  MEM_CacheLimiter_ref(item->c_handle);
  MEM_CacheLimiter_enforce_limits(limitor);
  MEM_CacheLimiter_unref(item->c_handle);

  if (need_lock) {
    limitor_lock.unlock();
  }

  /* cache limiter can't remove unused keys which points to destroyed values */
  check_unused_keys(cache);

  MEM_SAFE_DELETE(cache->points);
}

void IMB_cache_put(ImBufCache *cache, void *userkey, ImBuf *ibuf)
{
  do_imbufcache_put(cache, userkey, ibuf, true);
}

bool IMB_cache_put_if_possible(ImBufCache *cache, void *userkey, ImBuf *ibuf)
{
  size_t mem_in_use, mem_limit, elem_size;
  bool result = false;

  elem_size = (ibuf == nullptr) ? 0 : get_size_in_memory(ibuf);
  mem_limit = MEM_CacheLimiter_get_maximum();

  limitor_lock.lock();
  mem_in_use = MEM_CacheLimiter_get_memory_in_use(limitor);

  if (mem_in_use + elem_size <= mem_limit) {
    do_imbufcache_put(cache, userkey, ibuf, false);
    result = true;
  }

  limitor_lock.unlock();

  return result;
}

void IMB_cache_remove(ImBufCache *cache, void *userkey)
{
  ImBufCacheKey key;
  key.cache_owner = cache;
  key.userkey = userkey;
  BLI_ghash_remove(cache->hash, &key, imbufcache_keyfree, imbufcache_valfree);
}

ImBuf *IMB_cache_get(ImBufCache *cache, void *userkey, bool *r_is_cached_empty)
{
  ImBufCacheKey key;
  ImBufCacheItem *item;

  key.cache_owner = cache;
  key.userkey = userkey;
  item = static_cast<ImBufCacheItem *>(BLI_ghash_lookup(cache->hash, &key));

  if (r_is_cached_empty) {
    *r_is_cached_empty = false;
  }

  if (item) {
    if (item->ibuf) {
      std::lock_guard lock(limitor_lock);
      /* Check again, the condition might have changed before we acquired the lock. */
      if (item->ibuf) {
        MEM_CacheLimiter_touch(item->c_handle);
        IMB_refImBuf(item->ibuf);
        return item->ibuf;
      }
    }
    if (r_is_cached_empty && item->added_empty) {
      *r_is_cached_empty = true;
    }
  }

  return nullptr;
}

bool IMB_cache_has_frame(ImBufCache *cache, void *userkey)
{
  ImBufCacheKey key;
  ImBufCacheItem *item;

  key.cache_owner = cache;
  key.userkey = userkey;
  item = static_cast<ImBufCacheItem *>(BLI_ghash_lookup(cache->hash, &key));

  return item != nullptr;
}

void IMB_cache_free(ImBufCache *cache)
{
  PRINT("%s: cache '%s' free\n", __func__, cache->name);

  BLI_ghash_free(cache->hash, imbufcache_keyfree, imbufcache_valfree);

  BLI_mempool_destroy(cache->keys_pool);
  BLI_mempool_destroy(cache->items_pool);
  BLI_mempool_destroy(cache->userkeys_pool);

  if (cache->points) {
    MEM_delete(cache->points);
  }

  if (cache->last_userkey) {
    MEM_delete_void(cache->last_userkey);
  }

  MEM_delete(cache);
}

void IMB_cache_cleanup(ImBufCache *cache,
                       bool(cleanup_check_cb)(ImBuf *ibuf, void *userkey, void *userdata),
                       void *userdata)
{
  GHashIterator gh_iter;

  check_unused_keys(cache);

  BLI_ghashIterator_init(&gh_iter, cache->hash);

  while (!BLI_ghashIterator_done(&gh_iter)) {
    ImBufCacheKey *key = static_cast<ImBufCacheKey *>(BLI_ghashIterator_getKey(&gh_iter));
    ImBufCacheItem *item = static_cast<ImBufCacheItem *>(BLI_ghashIterator_getValue(&gh_iter));

    BLI_ghashIterator_step(&gh_iter);

    if (cleanup_check_cb(item->ibuf, key->userkey, userdata)) {
      PRINT("%s: cache '%s' remove item %p\n", __func__, cache->name, item);

      BLI_ghash_remove(cache->hash, key, imbufcache_keyfree, imbufcache_valfree);
    }
  }
}

void IMB_cache_get_cache_segments(
    ImBufCache *cache, int proxy, int render_flags, int *r_totseg, int **r_points)
{
  *r_totseg = 0;
  *r_points = nullptr;

  if (!cache->getdatafp) {
    return;
  }

  if (cache->proxy != proxy || cache->render_flags != render_flags) {
    MEM_SAFE_DELETE(cache->points);
  }

  if (cache->points) {
    *r_totseg = cache->totseg;
    *r_points = cache->points;
  }
  else {
    int totframe = BLI_ghash_len(cache->hash);
    int *frames = MEM_new_array_zeroed<int>(totframe, "movieclip cache frames");
    int a, totseg = 0;
    GHashIterator gh_iter;

    a = 0;
    GHASH_ITER (gh_iter, cache->hash) {
      ImBufCacheKey *key = static_cast<ImBufCacheKey *>(BLI_ghashIterator_getKey(&gh_iter));
      ImBufCacheItem *item = static_cast<ImBufCacheItem *>(BLI_ghashIterator_getValue(&gh_iter));
      int framenr, curproxy, curflags;

      if (item->ibuf) {
        cache->getdatafp(key->userkey, &framenr, &curproxy, &curflags);

        if (curproxy == proxy && curflags == render_flags) {
          frames[a++] = framenr;
        }
      }
    }

    qsort(frames, totframe, sizeof(int), compare_int);

    /* count */
    for (a = 0; a < totframe; a++) {
      if (a && frames[a] - frames[a - 1] != 1) {
        totseg++;
      }

      if (a == totframe - 1) {
        totseg++;
      }
    }

    if (totseg) {
      int b, *points;

      points = MEM_new_array_zeroed<int>(2 * size_t(totseg), "movieclip cache segments");

      /* fill */
      for (a = 0, b = 0; a < totframe; a++) {
        if (a == 0) {
          points[b++] = frames[a];
        }

        if (a && frames[a] - frames[a - 1] != 1) {
          points[b++] = frames[a - 1];
          points[b++] = frames[a];
        }

        if (a == totframe - 1) {
          points[b++] = frames[a];
        }
      }

      *r_totseg = totseg;
      *r_points = points;

      cache->totseg = totseg;
      cache->points = points;
      cache->proxy = proxy;
      cache->render_flags = render_flags;
    }

    MEM_delete(frames);
  }
}

ImBufCacheIter *IMB_cacheIter_new(ImBufCache *cache)
{
  GHashIterator *iter;

  check_unused_keys(cache);
  iter = BLI_ghashIterator_new(cache->hash);

  return reinterpret_cast<ImBufCacheIter *>(iter);
}

void IMB_cacheIter_free(ImBufCacheIter *iter)
{
  BLI_ghashIterator_free(reinterpret_cast<GHashIterator *>(iter));
}

bool IMB_cacheIter_done(ImBufCacheIter *iter)
{
  return BLI_ghashIterator_done(reinterpret_cast<GHashIterator *>(iter));
}

void IMB_cacheIter_step(ImBufCacheIter *iter)
{
  BLI_ghashIterator_step(reinterpret_cast<GHashIterator *>(iter));
}

ImBuf *IMB_cacheIter_getImBuf(ImBufCacheIter *iter)
{
  ImBufCacheItem *item = static_cast<ImBufCacheItem *>(
      BLI_ghashIterator_getValue(reinterpret_cast<GHashIterator *>(iter)));
  return item->ibuf;
}

void *IMB_cacheIter_getUserKey(ImBufCacheIter *iter)
{
  ImBufCacheKey *key = static_cast<ImBufCacheKey *>(
      BLI_ghashIterator_getKey(reinterpret_cast<GHashIterator *>(iter)));
  return key->userkey;
}

}  // namespace blender
