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

#include <stdlib.h> /* for qsort */
#include <memory.h>

#include "MEM_guardedalloc.h"
#include "MEM_CacheLimiterC-Api.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_mempool.h"

#include "IMB_moviecache.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

static MEM_CacheLimiterC *limitor= NULL;

typedef struct MovieCache {
	GHash *hash;
	GHashHashFP hashfp;
	GHashCmpFP cmpfp;
	MovieCacheGetKeyDataFP getdatafp;

	struct BLI_mempool *keys_pool;
	struct BLI_mempool *items_pool;
	struct BLI_mempool *userkeys_pool;

	int keysize;
	unsigned long curtime;

	int totseg, *points, proxy, render_flags;	/* for visual statistics optimization */
	int pad;
} MovieCache;

typedef struct MovieCacheKey {
	MovieCache *cache_owner;
	void *userkey;
} MovieCacheKey;

typedef struct MovieCacheItem {
	MovieCache *cache_owner;
	ImBuf *ibuf;
	MEM_CacheLimiterHandleC * c_handle;
	unsigned long last_access;
} MovieCacheItem;

static unsigned int moviecache_hashhash(const void *keyv)
{
	MovieCacheKey *key= (MovieCacheKey*)keyv;

	return key->cache_owner->hashfp(key->userkey);
}

static int moviecache_hashcmp(const void *av, const void *bv)
{
	const MovieCacheKey *a= (MovieCacheKey*)av;
	const MovieCacheKey *b= (MovieCacheKey*)bv;

	return a->cache_owner->cmpfp(a->userkey, b->userkey);
}

static void moviecache_keyfree(void *val)
{
	MovieCacheKey *key= (MovieCacheKey*)val;

	BLI_mempool_free(key->cache_owner->keys_pool, key);
}

static void moviecache_valfree(void *val)
{
	MovieCacheItem *item= (MovieCacheItem*)val;

	if (item->ibuf) {
		MEM_CacheLimiter_unmanage(item->c_handle);
		IMB_freeImBuf(item->ibuf);
	}

	BLI_mempool_free(item->cache_owner->items_pool, item);
}

static void check_unused_keys(MovieCache *cache)
{
	GHashIterator *iter;

	iter= BLI_ghashIterator_new(cache->hash);
	while (!BLI_ghashIterator_isDone(iter)) {
		MovieCacheKey *key= BLI_ghashIterator_getKey(iter);
		MovieCacheItem *item= BLI_ghashIterator_getValue(iter);

		BLI_ghashIterator_step(iter);

		if (!item->ibuf)
			BLI_ghash_remove(cache->hash, key, moviecache_keyfree, moviecache_valfree);
	}

	BLI_ghashIterator_free(iter);
}

static int compare_int(const void *av, const void *bv)
{
	const int *a= (int *)av;
	const int *b= (int *)bv;
	return *a-*b;
}

static void IMB_moviecache_destructor(void *p)
{
	MovieCacheItem *item= (MovieCacheItem *) p;

	if (item && item->ibuf) {
		IMB_freeImBuf(item->ibuf);

		item->ibuf= NULL;
		item->c_handle= NULL;
	}
}

/* approximate size of ImBuf in memory */
static size_t IMB_get_size_in_memory(ImBuf *ibuf)
{
	int a;
	size_t size= 0, channel_size= 0;

	size+= sizeof(ImBuf);

	if (ibuf->rect)
		channel_size+= sizeof(char);

	if (ibuf->rect_float)
		channel_size+= sizeof(float);

	size+= channel_size*ibuf->x*ibuf->y*ibuf->channels;

	if (ibuf->miptot) {
		for (a= 0; a<ibuf->miptot; a++) {
			if (ibuf->mipmap[a])
				size+= IMB_get_size_in_memory(ibuf->mipmap[a]);
		}
	}

	if (ibuf->tiles) {
		size+= sizeof(unsigned int)*ibuf->ytiles*ibuf->xtiles;
	}

	return size;
}

static size_t get_item_size (void *p)
{
	size_t size= sizeof(MovieCacheItem);
	MovieCacheItem *item= (MovieCacheItem *) p;

	if (item->ibuf)
		size+= IMB_get_size_in_memory(item->ibuf);

	return size;
}

void IMB_moviecache_init(void)
{
	limitor= new_MEM_CacheLimiter(IMB_moviecache_destructor, get_item_size);
}

void IMB_moviecache_destruct(void)
{
	if (limitor)
		delete_MEM_CacheLimiter(limitor);
}

struct MovieCache *IMB_moviecache_create(int keysize, GHashHashFP hashfp, GHashCmpFP cmpfp,
		MovieCacheGetKeyDataFP getdatafp)
{
	MovieCache *cache;

	cache= MEM_callocN(sizeof(MovieCache), "MovieCache");
	cache->keys_pool= BLI_mempool_create(sizeof(MovieCacheKey), 64, 64, 0);
	cache->items_pool= BLI_mempool_create(sizeof(MovieCacheItem), 64, 64, 0);
	cache->userkeys_pool= BLI_mempool_create(keysize, 64, 64, 0);
	cache->hash= BLI_ghash_new(moviecache_hashhash, moviecache_hashcmp, "MovieClip ImBuf cache hash");

	cache->keysize= keysize;
	cache->hashfp= hashfp;
	cache->cmpfp= cmpfp;
	cache->getdatafp= getdatafp;
	cache->proxy= -1;

	return cache;
}

void IMB_moviecache_put(MovieCache *cache, void *userkey, ImBuf *ibuf)
{
	MovieCacheKey *key;
	MovieCacheItem *item;

	if (!limitor)
		IMB_moviecache_init();

	IMB_refImBuf(ibuf);

	key= BLI_mempool_alloc(cache->keys_pool);
	key->cache_owner= cache;
	key->userkey= BLI_mempool_alloc(cache->userkeys_pool);
	memcpy(key->userkey, userkey, cache->keysize);

	item= BLI_mempool_alloc(cache->items_pool);
	item->ibuf= ibuf;
	item->cache_owner= cache;
	item->last_access= cache->curtime++;
	item->c_handle= NULL;

	BLI_ghash_remove(cache->hash, key, moviecache_keyfree, moviecache_valfree);
	BLI_ghash_insert(cache->hash, key, item);

	item->c_handle= MEM_CacheLimiter_insert(limitor, item);

	MEM_CacheLimiter_ref(item->c_handle);
	MEM_CacheLimiter_enforce_limits(limitor);
	MEM_CacheLimiter_unref(item->c_handle);

	/* cache limiter can't remove unused keys which points to destoryed values */
	check_unused_keys(cache);

	if (cache->points) {
		MEM_freeN(cache->points);
		cache->points= NULL;
	}
}

ImBuf* IMB_moviecache_get(MovieCache *cache, void *userkey)
{
	MovieCacheKey key;
	MovieCacheItem *item;

	key.cache_owner= cache;
	key.userkey= userkey;
	item= (MovieCacheItem*)BLI_ghash_lookup(cache->hash, &key);

	if (item) {
		item->last_access= cache->curtime++;

		if (item->ibuf) {
			MEM_CacheLimiter_touch(item->c_handle);
			IMB_refImBuf(item->ibuf);

			return item->ibuf;
		}
	}

	return NULL;
}

void IMB_moviecache_free(MovieCache *cache)
{
	BLI_ghash_free(cache->hash, moviecache_keyfree, moviecache_valfree);

	BLI_mempool_destroy(cache->keys_pool);
	BLI_mempool_destroy(cache->items_pool);
	BLI_mempool_destroy(cache->userkeys_pool);

	if (cache->points)
		MEM_freeN(cache->points);

	MEM_freeN(cache);
}

/* get segments of cached frames. useful for debugging cache policies */
void IMB_moviecache_get_cache_segments(MovieCache *cache, int proxy, int render_flags, int *totseg_r, int **points_r)
{
	*totseg_r= 0;
	*points_r= NULL;

	if (!cache->getdatafp)
		return;

	if (cache->proxy!=proxy || cache->render_flags!=render_flags) {
		if (cache->points)
			MEM_freeN(cache->points);

		cache->points= NULL;
	}

	if (cache->points) {
		*totseg_r= cache->totseg;
		*points_r= cache->points;
	}
	else {
		int totframe= BLI_ghash_size(cache->hash);
		int *frames= MEM_callocN(totframe*sizeof(int), "movieclip cache frames");
		int a, totseg= 0;
		GHashIterator *iter;

		iter= BLI_ghashIterator_new(cache->hash);
		a= 0;
		while (!BLI_ghashIterator_isDone(iter)) {
			MovieCacheKey *key= BLI_ghashIterator_getKey(iter);
			MovieCacheItem *item= BLI_ghashIterator_getValue(iter);
			int framenr, curproxy, curflags;

			if (item->ibuf) {
				cache->getdatafp(key->userkey, &framenr, &curproxy, &curflags);

				if (curproxy==proxy && curflags==render_flags)
					frames[a++]= framenr;
			}

			BLI_ghashIterator_step(iter);
		}

		BLI_ghashIterator_free(iter);

		qsort(frames, totframe, sizeof(int), compare_int);

		/* count */
		for (a= 0; a<totframe; a++) {
			if (a && frames[a]-frames[a-1]!=1)
				totseg++;

			if (a==totframe-1)
				totseg++;
		}

		if (totseg) {
			int b, *points;

			points= MEM_callocN(2*sizeof(int)*totseg, "movieclip cache segments");

			/* fill */
			for (a= 0, b= 0; a<totframe; a++) {
				if (a==0)
					points[b++]= frames[a];

				if (a && frames[a]-frames[a-1]!=1) {
					points[b++]= frames[a-1];
					points[b++]= frames[a];
				}

				if (a==totframe-1)
					points[b++]= frames[a];
			}

			*totseg_r= totseg;
			*points_r= points;

			cache->totseg= totseg;
			cache->points= points;
			cache->proxy= proxy;
			cache->render_flags= render_flags;
		}

		MEM_freeN(frames);
	}
}
