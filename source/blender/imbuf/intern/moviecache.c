/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin,
 *                 Peter Schlaile
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/moviecache.c
 *  \ingroup bke
 */

#undef DEBUG_MESSAGES

#include <stdlib.h> /* for qsort */
#include <memory.h>

#include "MEM_guardedalloc.h"
#include "MEM_CacheLimiterC-Api.h"

#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_mempool.h"
#include "BLI_threads.h"

#include "IMB_moviecache.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#ifdef DEBUG_MESSAGES
#  if defined __GNUC__ || defined __sun
#    define PRINT(format, args ...) printf(format, ##args)
#  else
#    define PRINT(format, ...) printf(__VA_ARGS__)
#  endif
#else
#  define PRINT(format, ...)
#endif

static MEM_CacheLimiterC *limitor = NULL;
static pthread_mutex_t limitor_lock = BLI_MUTEX_INITIALIZER;

typedef struct MovieCache {
	char name[64];

	GHash *hash;
	GHashHashFP hashfp;
	GHashCmpFP cmpfp;
	MovieCacheGetKeyDataFP getdatafp;

	MovieCacheGetPriorityDataFP getprioritydatafp;
	MovieCacheGetItemPriorityFP getitempriorityfp;
	MovieCachePriorityDeleterFP prioritydeleterfp;

	struct BLI_mempool *keys_pool;
	struct BLI_mempool *items_pool;
	struct BLI_mempool *userkeys_pool;

	int keysize;

	void *last_userkey;

	int totseg, *points, proxy, render_flags;  /* for visual statistics optimization */
	int pad;
} MovieCache;

typedef struct MovieCacheKey {
	MovieCache *cache_owner;
	void *userkey;
} MovieCacheKey;

typedef struct MovieCacheItem {
	MovieCache *cache_owner;
	ImBuf *ibuf;
	MEM_CacheLimiterHandleC *c_handle;
	void *priority_data;
} MovieCacheItem;

static unsigned int moviecache_hashhash(const void *keyv)
{
	MovieCacheKey *key = (MovieCacheKey *)keyv;

	return key->cache_owner->hashfp(key->userkey);
}

static int moviecache_hashcmp(const void *av, const void *bv)
{
	const MovieCacheKey *a = (MovieCacheKey *)av;
	const MovieCacheKey *b = (MovieCacheKey *)bv;

	return a->cache_owner->cmpfp(a->userkey, b->userkey);
}

static void moviecache_keyfree(void *val)
{
	MovieCacheKey *key = (MovieCacheKey *)val;

	BLI_mempool_free(key->cache_owner->userkeys_pool, key->userkey);

	BLI_mempool_free(key->cache_owner->keys_pool, key);
}

static void moviecache_valfree(void *val)
{
	MovieCacheItem *item = (MovieCacheItem *)val;
	MovieCache *cache = item->cache_owner;

	PRINT("%s: cache '%s' free item %p buffer %p\n", __func__, cache->name, item, item->ibuf);

	if (item->ibuf) {
		MEM_CacheLimiter_unmanage(item->c_handle);
		IMB_freeImBuf(item->ibuf);
	}

	if (item->priority_data && cache->prioritydeleterfp) {
		cache->prioritydeleterfp(item->priority_data);
	}

	BLI_mempool_free(item->cache_owner->items_pool, item);
}

static void check_unused_keys(MovieCache *cache)
{
	GHashIterator *iter;

	iter = BLI_ghashIterator_new(cache->hash);
	while (!BLI_ghashIterator_done(iter)) {
		MovieCacheKey *key = BLI_ghashIterator_getKey(iter);
		MovieCacheItem *item = BLI_ghashIterator_getValue(iter);
		int remove = 0;

		BLI_ghashIterator_step(iter);

		remove = !item->ibuf;

		if (remove) {
			PRINT("%s: cache '%s' remove item %p without buffer\n", __func__, cache->name, item);
		}

		if (remove)
			BLI_ghash_remove(cache->hash, key, moviecache_keyfree, moviecache_valfree);
	}

	BLI_ghashIterator_free(iter);
}

static int compare_int(const void *av, const void *bv)
{
	const int *a = (int *)av;
	const int *b = (int *)bv;
	return *a - *b;
}

static void IMB_moviecache_destructor(void *p)
{
	MovieCacheItem *item = (MovieCacheItem *)p;

	if (item && item->ibuf) {
		MovieCache *cache = item->cache_owner;

		PRINT("%s: cache '%s' destroy item %p buffer %p\n", __func__, cache->name, item, item->ibuf);

		IMB_freeImBuf(item->ibuf);

		item->ibuf = NULL;
		item->c_handle = NULL;

		/* force cached segments to be updated */
		if (cache->points) {
			MEM_freeN(cache->points);
			cache->points = NULL;
		}
	}
}

/* approximate size of ImBuf in memory */
static size_t IMB_get_size_in_memory(ImBuf *ibuf)
{
	int a;
	size_t size = 0, channel_size = 0;

	/* Persistent images should have no affect on how "normal"
	 * images are cached.
	 *
	 * This is a bit arbitrary, but would make it so only movies
	 * and sequences are memory limited, keeping textures in the
	 * memory in order to avoid constant file reload on viewport
	 * update.
	 */
	if (ibuf->userflags & IB_PERSISTENT) {
		return 0;
	}

	size += sizeof(ImBuf);

	if (ibuf->rect)
		channel_size += sizeof(char);

	if (ibuf->rect_float)
		channel_size += sizeof(float);

	size += channel_size * ibuf->x * ibuf->y * ibuf->channels;

	if (ibuf->miptot) {
		for (a = 0; a < ibuf->miptot; a++) {
			if (ibuf->mipmap[a])
				size += IMB_get_size_in_memory(ibuf->mipmap[a]);
		}
	}

	if (ibuf->tiles) {
		size += sizeof(unsigned int) * ibuf->ytiles * ibuf->xtiles;
	}

	return size;
}

static size_t get_item_size(void *p)
{
	size_t size = sizeof(MovieCacheItem);
	MovieCacheItem *item = (MovieCacheItem *) p;

	if (item->ibuf)
		size += IMB_get_size_in_memory(item->ibuf);

	return size;
}

static int get_item_priority(void *item_v, int default_priority)
{
	MovieCacheItem *item = (MovieCacheItem *) item_v;
	MovieCache *cache = item->cache_owner;
	int priority;

	if (!cache->getitempriorityfp) {
		PRINT("%s: cache '%s' item %p use default priority %d\n", __func__, cache-> name, item, default_priority);

		return default_priority;
	}

	priority = cache->getitempriorityfp(cache->last_userkey, item->priority_data);

	PRINT("%s: cache '%s' item %p priority %d\n", __func__, cache-> name, item, priority);

	return priority;
}

static bool get_item_destroyable(void *item_v)
{
	MovieCacheItem *item = (MovieCacheItem *) item_v;
	/* IB_BITMAPDIRTY means image was modified from inside blender and
	 * changes are not saved to disk.
	 *
	 * Such buffers are never to be freed.
	 */
	if ((item->ibuf->userflags & IB_BITMAPDIRTY) ||
	    (item->ibuf->userflags & IB_PERSISTENT))
	{
		return false;
	}
	return true;
}

void IMB_moviecache_init(void)
{
	limitor = new_MEM_CacheLimiter(IMB_moviecache_destructor, get_item_size);

	MEM_CacheLimiter_ItemPriority_Func_set(limitor, get_item_priority);
	MEM_CacheLimiter_ItemDestroyable_Func_set(limitor, get_item_destroyable);
}

void IMB_moviecache_destruct(void)
{
	if (limitor)
		delete_MEM_CacheLimiter(limitor);
}

MovieCache *IMB_moviecache_create(const char *name, int keysize, GHashHashFP hashfp, GHashCmpFP cmpfp)
{
	MovieCache *cache;

	PRINT("%s: cache '%s' create\n", __func__, name);

	cache = MEM_callocN(sizeof(MovieCache), "MovieCache");

	BLI_strncpy(cache->name, name, sizeof(cache->name));

	cache->keys_pool = BLI_mempool_create(sizeof(MovieCacheKey), 0, 64, BLI_MEMPOOL_NOP);
	cache->items_pool = BLI_mempool_create(sizeof(MovieCacheItem), 0, 64, BLI_MEMPOOL_NOP);
	cache->userkeys_pool = BLI_mempool_create(keysize, 0, 64, BLI_MEMPOOL_NOP);
	cache->hash = BLI_ghash_new(moviecache_hashhash, moviecache_hashcmp, "MovieClip ImBuf cache hash");

	cache->keysize = keysize;
	cache->hashfp = hashfp;
	cache->cmpfp = cmpfp;
	cache->proxy = -1;

	return cache;
}

void IMB_moviecache_set_getdata_callback(MovieCache *cache, MovieCacheGetKeyDataFP getdatafp)
{
	cache->getdatafp = getdatafp;
}

void IMB_moviecache_set_priority_callback(struct MovieCache *cache, MovieCacheGetPriorityDataFP getprioritydatafp,
                                          MovieCacheGetItemPriorityFP getitempriorityfp,
                                          MovieCachePriorityDeleterFP prioritydeleterfp)
{
	cache->last_userkey = MEM_mallocN(cache->keysize, "movie cache last user key");

	cache->getprioritydatafp = getprioritydatafp;
	cache->getitempriorityfp = getitempriorityfp;
	cache->prioritydeleterfp = prioritydeleterfp;
}

static void do_moviecache_put(MovieCache *cache, void *userkey, ImBuf *ibuf, bool need_lock)
{
	MovieCacheKey *key;
	MovieCacheItem *item;

	if (!limitor)
		IMB_moviecache_init();

	IMB_refImBuf(ibuf);

	key = BLI_mempool_alloc(cache->keys_pool);
	key->cache_owner = cache;
	key->userkey = BLI_mempool_alloc(cache->userkeys_pool);
	memcpy(key->userkey, userkey, cache->keysize);

	item = BLI_mempool_alloc(cache->items_pool);

	PRINT("%s: cache '%s' put %p, item %p\n", __func__, cache-> name, ibuf, item);

	item->ibuf = ibuf;
	item->cache_owner = cache;
	item->c_handle = NULL;
	item->priority_data = NULL;

	if (cache->getprioritydatafp) {
		item->priority_data = cache->getprioritydatafp(userkey);
	}

	BLI_ghash_remove(cache->hash, key, moviecache_keyfree, moviecache_valfree);
	BLI_ghash_insert(cache->hash, key, item);

	if (cache->last_userkey) {
		memcpy(cache->last_userkey, userkey, cache->keysize);
	}

	if (need_lock)
		BLI_mutex_lock(&limitor_lock);

	item->c_handle = MEM_CacheLimiter_insert(limitor, item);

	MEM_CacheLimiter_ref(item->c_handle);
	MEM_CacheLimiter_enforce_limits(limitor);
	MEM_CacheLimiter_unref(item->c_handle);

	if (need_lock)
		BLI_mutex_unlock(&limitor_lock);

	/* cache limiter can't remove unused keys which points to destoryed values */
	check_unused_keys(cache);

	if (cache->points) {
		MEM_freeN(cache->points);
		cache->points = NULL;
	}
}

void IMB_moviecache_put(MovieCache *cache, void *userkey, ImBuf *ibuf)
{
	do_moviecache_put(cache, userkey, ibuf, true);
}

bool IMB_moviecache_put_if_possible(MovieCache *cache, void *userkey, ImBuf *ibuf)
{
	size_t mem_in_use, mem_limit, elem_size;
	bool result = false;

	elem_size = IMB_get_size_in_memory(ibuf);
	mem_limit = MEM_CacheLimiter_get_maximum();

	BLI_mutex_lock(&limitor_lock);
	mem_in_use = MEM_CacheLimiter_get_memory_in_use(limitor);

	if (mem_in_use + elem_size <= mem_limit) {
		do_moviecache_put(cache, userkey, ibuf, false);
		result = true;
	}

	BLI_mutex_unlock(&limitor_lock);

	return result;
}

ImBuf *IMB_moviecache_get(MovieCache *cache, void *userkey)
{
	MovieCacheKey key;
	MovieCacheItem *item;

	key.cache_owner = cache;
	key.userkey = userkey;
	item = (MovieCacheItem *)BLI_ghash_lookup(cache->hash, &key);

	if (item) {
		if (item->ibuf) {
			BLI_mutex_lock(&limitor_lock);
			MEM_CacheLimiter_touch(item->c_handle);
			BLI_mutex_unlock(&limitor_lock);

			IMB_refImBuf(item->ibuf);

			return item->ibuf;
		}
	}

	return NULL;
}

bool IMB_moviecache_has_frame(MovieCache *cache, void *userkey)
{
	MovieCacheKey key;
	MovieCacheItem *item;

	key.cache_owner = cache;
	key.userkey = userkey;
	item = (MovieCacheItem *)BLI_ghash_lookup(cache->hash, &key);

	return item != NULL;
}

void IMB_moviecache_free(MovieCache *cache)
{
	PRINT("%s: cache '%s' free\n", __func__, cache->name);

	BLI_ghash_free(cache->hash, moviecache_keyfree, moviecache_valfree);

	BLI_mempool_destroy(cache->keys_pool);
	BLI_mempool_destroy(cache->items_pool);
	BLI_mempool_destroy(cache->userkeys_pool);

	if (cache->points)
		MEM_freeN(cache->points);

	if (cache->last_userkey)
		MEM_freeN(cache->last_userkey);

	MEM_freeN(cache);
}

void IMB_moviecache_cleanup(MovieCache *cache, bool (cleanup_check_cb) (ImBuf *ibuf, void *userkey, void *userdata), void *userdata)
{
	GHashIterator *iter;

	check_unused_keys(cache);

	iter = BLI_ghashIterator_new(cache->hash);
	while (!BLI_ghashIterator_done(iter)) {
		MovieCacheKey *key = BLI_ghashIterator_getKey(iter);
		MovieCacheItem *item = BLI_ghashIterator_getValue(iter);

		BLI_ghashIterator_step(iter);

		if (cleanup_check_cb(item->ibuf, key->userkey, userdata)) {
			PRINT("%s: cache '%s' remove item %p\n", __func__, cache->name, item);

			BLI_ghash_remove(cache->hash, key, moviecache_keyfree, moviecache_valfree);
		}
	}

	BLI_ghashIterator_free(iter);
}

/* get segments of cached frames. useful for debugging cache policies */
void IMB_moviecache_get_cache_segments(MovieCache *cache, int proxy, int render_flags, int *totseg_r, int **points_r)
{
	*totseg_r = 0;
	*points_r = NULL;

	if (!cache->getdatafp)
		return;

	if (cache->proxy != proxy || cache->render_flags != render_flags) {
		if (cache->points)
			MEM_freeN(cache->points);

		cache->points = NULL;
	}

	if (cache->points) {
		*totseg_r = cache->totseg;
		*points_r = cache->points;
	}
	else {
		int totframe = BLI_ghash_size(cache->hash);
		int *frames = MEM_callocN(totframe * sizeof(int), "movieclip cache frames");
		int a, totseg = 0;
		GHashIterator *iter;

		iter = BLI_ghashIterator_new(cache->hash);
		a = 0;
		while (!BLI_ghashIterator_done(iter)) {
			MovieCacheKey *key = BLI_ghashIterator_getKey(iter);
			MovieCacheItem *item = BLI_ghashIterator_getValue(iter);
			int framenr, curproxy, curflags;

			if (item->ibuf) {
				cache->getdatafp(key->userkey, &framenr, &curproxy, &curflags);

				if (curproxy == proxy && curflags == render_flags)
					frames[a++] = framenr;
			}

			BLI_ghashIterator_step(iter);
		}

		BLI_ghashIterator_free(iter);

		qsort(frames, totframe, sizeof(int), compare_int);

		/* count */
		for (a = 0; a < totframe; a++) {
			if (a && frames[a] - frames[a - 1] != 1)
				totseg++;

			if (a == totframe - 1)
				totseg++;
		}

		if (totseg) {
			int b, *points;

			points = MEM_callocN(2 * sizeof(int) * totseg, "movieclip cache segments");

			/* fill */
			for (a = 0, b = 0; a < totframe; a++) {
				if (a == 0)
					points[b++] = frames[a];

				if (a && frames[a] - frames[a - 1] != 1) {
					points[b++] = frames[a - 1];
					points[b++] = frames[a];
				}

				if (a == totframe - 1)
					points[b++] = frames[a];
			}

			*totseg_r = totseg;
			*points_r = points;

			cache->totseg = totseg;
			cache->points = points;
			cache->proxy = proxy;
			cache->render_flags = render_flags;
		}

		MEM_freeN(frames);
	}
}

struct MovieCacheIter *IMB_moviecacheIter_new(MovieCache *cache)
{
	GHashIterator *iter;

	check_unused_keys(cache);
	iter = BLI_ghashIterator_new(cache->hash);

	return (struct MovieCacheIter *) iter;
}

void IMB_moviecacheIter_free(struct MovieCacheIter *iter)
{
	BLI_ghashIterator_free((GHashIterator *) iter);
}

bool IMB_moviecacheIter_done(struct MovieCacheIter *iter)
{
	return BLI_ghashIterator_done((GHashIterator *) iter);
}

void IMB_moviecacheIter_step(struct MovieCacheIter *iter)
{
	BLI_ghashIterator_step((GHashIterator *) iter);
}

ImBuf *IMB_moviecacheIter_getImBuf(struct MovieCacheIter *iter)
{
	MovieCacheItem *item = BLI_ghashIterator_getValue((GHashIterator *) iter);
	return item->ibuf;
}

void *IMB_moviecacheIter_getUserKey(struct MovieCacheIter *iter)
{
	MovieCacheKey *key = BLI_ghashIterator_getKey((GHashIterator *) iter);
	return key->userkey;
}
