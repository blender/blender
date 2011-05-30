/*
 * $Id$
 *
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/moviecache.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_mempool.h"

#include "BKE_moviecache.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

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

	int totseg, *points;	/* for visual statistics optimization */
} MovieCache;

typedef struct MovieCacheKey {
	MovieCache *cache_owner;
	void *userkey;
} MovieCacheKey;

typedef struct MovieCacheItem {
	MovieCache *cache_owner;
	ImBuf *ibuf;
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

	IMB_freeImBuf(item->ibuf);

	BLI_mempool_free(item->cache_owner->items_pool, item);
}

static MovieCacheKey *get_lru_key(MovieCache *cache)
{
	GHashIterator *iter;
	MovieCacheKey *lru_key= NULL;
	MovieCacheItem *lru_item= NULL;

	iter= BLI_ghashIterator_new(cache->hash);
	while(!BLI_ghashIterator_isDone(iter)) {
		MovieCacheKey *key= BLI_ghashIterator_getKey(iter);
		MovieCacheItem *item= BLI_ghashIterator_getValue(iter);

		if(lru_item==NULL || item->last_access<lru_item->last_access) {
			lru_key= key;
			lru_item= item;
		}

		BLI_ghashIterator_step(iter);
	}

	BLI_ghashIterator_free(iter);

	return lru_key;
}

static int compare_int(const void *av, const void *bv)
{
	const int *a= (int *)av;
	const int *b= (int *)bv;
	return *a-*b;
}

struct MovieCache *BKE_moviecache_create(int keysize, GHashHashFP hashfp, GHashCmpFP cmpfp,
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

	return cache;
}

void BKE_moviecache_put(MovieCache *cache, void *userkey, ImBuf *ibuf)
{
	MovieCacheKey *key;
	MovieCacheItem *item;

	/* TODO: implement better limiters */
	if(BLI_ghash_size(cache->hash) > 250) {
		MovieCacheKey *lru_key= get_lru_key(cache);
		BLI_ghash_remove(cache->hash, lru_key, moviecache_keyfree, moviecache_valfree);
	}

	IMB_refImBuf(ibuf);

	key= BLI_mempool_alloc(cache->keys_pool);
	key->cache_owner= cache;
	key->userkey= BLI_mempool_alloc(cache->userkeys_pool);;
	memcpy(key->userkey, userkey, cache->keysize);

	item= BLI_mempool_alloc(cache->items_pool);
	item->ibuf= ibuf;
	item->cache_owner= cache;
	item->last_access= cache->curtime++;

	BLI_ghash_remove(cache->hash, key, moviecache_keyfree, moviecache_valfree);
	BLI_ghash_insert(cache->hash, key, item);

	if(cache->points) {
		MEM_freeN(cache->points);
		cache->points= NULL;
	}
}

ImBuf* BKE_moviecache_get(MovieCache *cache, void *userkey)
{
	MovieCacheKey key;
	MovieCacheItem *item;

	key.cache_owner= cache;
	key.userkey= userkey;
	item= (MovieCacheItem*)BLI_ghash_lookup(cache->hash, &key);

	if(item) {
		item->last_access= cache->curtime++;

		if(item->ibuf) {
			IMB_refImBuf(item->ibuf);
			return item->ibuf;
		}
	}

	return NULL;
}

void BKE_moviecache_free(MovieCache *cache)
{
	BLI_ghash_free(cache->hash, moviecache_keyfree, moviecache_valfree);

	BLI_mempool_destroy(cache->keys_pool);
	BLI_mempool_destroy(cache->items_pool);
	BLI_mempool_destroy(cache->userkeys_pool);

	if(cache->points)
		MEM_freeN(cache->points);

	MEM_freeN(cache);
}

/* get segments of cached frames. useful for debugging cache policies */
void BKE_moviecache_get_cache_segments(MovieCache *cache, int *totseg_r, int **points_r)
{
	*totseg_r= 0;
	*points_r= NULL;

	if(!cache->getdatafp)
		return;

	if(cache->points) {
		*totseg_r= cache->totseg;
		*points_r= cache->points;
	} else if(cache) {
		int totframe= BLI_ghash_size(cache->hash);
		int *frames= MEM_callocN(totframe*sizeof(int), "movieclip cache frames");
		int a, totseg= 0;
		GHashIterator *iter;

		iter= BLI_ghashIterator_new(cache->hash);
		a= 0;
		while(!BLI_ghashIterator_isDone(iter)) {
			MovieCacheKey *key= BLI_ghashIterator_getKey(iter);
			int framenr;

			cache->getdatafp(key->userkey, &framenr);

			frames[a++]= framenr;

			BLI_ghashIterator_step(iter);
		}

		BLI_ghashIterator_free(iter);

		qsort(frames, totframe, sizeof(int), compare_int);

		/* count */
		for(a= 0; a<totframe; a++) {
			if(a && frames[a]-frames[a-1]!=1)
				totseg++;

			if(a==totframe-1)
				totseg++;
		}

		if(totseg) {
			int b, *points;

			points= MEM_callocN(2*sizeof(int)*totseg, "movieclip cache segments");

			/* fill */
			for(a= 0, b= 0; a<totframe; a++) {
				if(a==0)
					points[b++]= frames[a];

				if(a && frames[a]-frames[a-1]!=1) {
					points[b++]= frames[a-1];
					points[b++]= frames[a];
				}

				if(a==totframe-1)
					points[b++]= frames[a];
			}

			MEM_freeN(frames);

			*totseg_r= totseg;
			*points_r= points;

			cache->totseg= totseg;
			cache->points= points;
		}
	}
}