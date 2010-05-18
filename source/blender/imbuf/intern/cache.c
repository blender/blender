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
 * ***** END GPL LICENSE BLOCK *****
 */

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_memarena.h"
#include "BLI_threads.h"

#include "BKE_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_filetype.h"

#include "imbuf.h"

/* We use a two level cache here. A per-thread cache with limited number of
   tiles. This can be accessed without locking and so is hoped to lead to most
   tile access being lock-free. The global cache is shared between all threads
   and requires slow locking to access, and contains all tiles.
   
   The per-thread cache should be big enough that one might hope to not fall
   back to the global cache every pixel, but not to big to keep too many tiles
   locked and using memory. */

#define IB_THREAD_CACHE_SIZE	100

typedef struct ImGlobalTile {
	struct ImGlobalTile *next, *prev;

	ImBuf *ibuf;
	int tx, ty;
	int refcount;
	volatile int loading;
} ImGlobalTile;

typedef struct ImThreadTile {
	struct ImThreadTile *next, *prev;

	ImBuf *ibuf;
	int tx, ty;

	ImGlobalTile *global;
} ImThreadTile;

typedef struct ImThreadTileCache {
	ListBase tiles;
	ListBase unused;
	GHash *tilehash;
} ImThreadTileCache;

typedef struct ImGlobalTileCache {
	ListBase tiles;
	ListBase unused;
	GHash *tilehash;

	MemArena *memarena;
	uintptr_t totmem, maxmem;

	ImThreadTileCache thread_cache[BLENDER_MAX_THREADS+1];
	int totthread;

	ThreadMutex mutex;
} ImGlobalTileCache;

static ImGlobalTileCache GLOBAL_CACHE;

/***************************** Hash Functions ********************************/

static unsigned int imb_global_tile_hash(void *gtile_p)
{
	ImGlobalTile *gtile= gtile_p;

	return ((unsigned int)(intptr_t)gtile->ibuf)*769 + gtile->tx*53 + gtile->ty*97;
}

static int imb_global_tile_cmp(void *a_p, void *b_p)
{
	ImGlobalTile *a= a_p;
	ImGlobalTile *b= b_p;

	if(a->ibuf == b->ibuf && a->tx == b->tx && a->ty == b->ty) return 0;
	else if(a->ibuf < b->ibuf || a->tx < b->tx || a->ty < b->ty) return -1;
	else return 1;
}

static unsigned int imb_thread_tile_hash(void *ttile_p)
{
	ImThreadTile *ttile= ttile_p;

	return ((unsigned int)(intptr_t)ttile->ibuf)*769 + ttile->tx*53 + ttile->ty*97;
}

static int imb_thread_tile_cmp(void *a_p, void *b_p)
{
	ImThreadTile *a= a_p;
	ImThreadTile *b= b_p;

	if(a->ibuf == b->ibuf && a->tx == b->tx && a->ty == b->ty) return 0;
	else if(a->ibuf < b->ibuf || a->tx < b->tx || a->ty < b->ty) return -1;
	else return 1;
}

/******************************** Load/Unload ********************************/

static void imb_global_cache_tile_load(ImGlobalTile *gtile)
{
	ImBuf *ibuf= gtile->ibuf;
	int toffs= ibuf->xtiles*gtile->ty + gtile->tx;
	unsigned int *rect;

	rect = MEM_callocN(sizeof(unsigned int)*ibuf->tilex*ibuf->tiley, "imb_tile");
	imb_loadtile(ibuf, gtile->tx, gtile->ty, rect);
	ibuf->tiles[toffs]= rect;
}

static void imb_global_cache_tile_unload(ImGlobalTile *gtile)
{
	ImBuf *ibuf= gtile->ibuf;
	int toffs= ibuf->xtiles*gtile->ty + gtile->tx;

	MEM_freeN(ibuf->tiles[toffs]);
	ibuf->tiles[toffs]= NULL;

	GLOBAL_CACHE.totmem -= sizeof(unsigned int)*ibuf->tilex*ibuf->tiley;
}

/* external free */
void imb_tile_cache_tile_free(ImBuf *ibuf, int tx, int ty)
{
	ImGlobalTile *gtile, lookuptile;

	BLI_mutex_lock(&GLOBAL_CACHE.mutex);

	lookuptile.ibuf = ibuf;
	lookuptile.tx = tx;
	lookuptile.ty = ty;
	gtile= BLI_ghash_lookup(GLOBAL_CACHE.tilehash, &lookuptile);

	if(gtile) {
		/* in case another thread is loading this */
		while(gtile->loading)
			;

		BLI_ghash_remove(GLOBAL_CACHE.tilehash, gtile, NULL, NULL);
		BLI_remlink(&GLOBAL_CACHE.tiles, gtile);
		BLI_addtail(&GLOBAL_CACHE.unused, gtile);
	}

	BLI_mutex_unlock(&GLOBAL_CACHE.mutex);
}

/******************************* Init/Exit ***********************************/

static void imb_thread_cache_init(ImThreadTileCache *cache)
{
	ImThreadTile *ttile;
	int a;

	memset(cache, 0, sizeof(ImThreadTileCache));

	cache->tilehash= BLI_ghash_new(imb_thread_tile_hash, imb_thread_tile_cmp, "imb_thread_cache_init gh");

	/* pre-allocate all thread local tiles in unused list */
	for(a=0; a<IB_THREAD_CACHE_SIZE; a++) {
		ttile= BLI_memarena_alloc(GLOBAL_CACHE.memarena, sizeof(ImThreadTile));
		BLI_addtail(&cache->unused, ttile);
	}
}

static void imb_thread_cache_exit(ImThreadTileCache *cache)
{
	BLI_ghash_free(cache->tilehash, NULL, NULL);
}

void imb_tile_cache_init(void)
{
	memset(&GLOBAL_CACHE, 0, sizeof(ImGlobalTileCache));

	BLI_mutex_init(&GLOBAL_CACHE.mutex);

	/* initialize for one thread, for places that access textures
	   outside of rendering (displace modifier, painting, ..) */
	IMB_tile_cache_params(0, 0);
}

void imb_tile_cache_exit(void)
{
	ImGlobalTile *gtile;
	int a;

	for(gtile=GLOBAL_CACHE.tiles.first; gtile; gtile=gtile->next)
		imb_global_cache_tile_unload(gtile);

	for(a=0; a<GLOBAL_CACHE.totthread; a++)
		imb_thread_cache_exit(&GLOBAL_CACHE.thread_cache[a]);

	if(GLOBAL_CACHE.memarena)
		BLI_memarena_free(GLOBAL_CACHE.memarena);

	if(GLOBAL_CACHE.tilehash)
		BLI_ghash_free(GLOBAL_CACHE.tilehash, NULL, NULL);

	BLI_mutex_end(&GLOBAL_CACHE.mutex);
}

/* presumed to be called when no threads are running */
void IMB_tile_cache_params(int totthread, int maxmem)
{
	int a;

	/* always one cache for non-threaded access */
	totthread++;

	/* lazy initialize cache */
	if(GLOBAL_CACHE.totthread == totthread && GLOBAL_CACHE.maxmem == maxmem)
		return;

	imb_tile_cache_exit();

	memset(&GLOBAL_CACHE, 0, sizeof(ImGlobalTileCache));

	GLOBAL_CACHE.tilehash= BLI_ghash_new(imb_global_tile_hash, imb_global_tile_cmp, "tile_cache_params gh");

	GLOBAL_CACHE.memarena= BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "ImTileCache arena");
	BLI_memarena_use_calloc(GLOBAL_CACHE.memarena);

	GLOBAL_CACHE.maxmem= maxmem*1024*1024;

	GLOBAL_CACHE.totthread= totthread;
	for(a=0; a<totthread; a++)
		imb_thread_cache_init(&GLOBAL_CACHE.thread_cache[a]);

	BLI_mutex_init(&GLOBAL_CACHE.mutex);
}

/***************************** Global Cache **********************************/

static ImGlobalTile *imb_global_cache_get_tile(ImBuf *ibuf, int tx, int ty, ImGlobalTile *replacetile)
{
	ImGlobalTile *gtile, lookuptile;

	BLI_mutex_lock(&GLOBAL_CACHE.mutex);

	if(replacetile)
		replacetile->refcount--;

	/* find tile in global cache */
	lookuptile.ibuf = ibuf;
	lookuptile.tx = tx;
	lookuptile.ty = ty;
	gtile= BLI_ghash_lookup(GLOBAL_CACHE.tilehash, &lookuptile);
	
	if(gtile) {
		/* found tile. however it may be in the process of being loaded
		   by another thread, in that case we do stupid busy loop waiting
		   for the other thread to load the tile */
		gtile->refcount++;

		BLI_mutex_unlock(&GLOBAL_CACHE.mutex);

		while(gtile->loading)
			;
	}
	else {
		/* not found, let's load it from disk */

		/* first check if we hit the memory limit */
		if(GLOBAL_CACHE.maxmem && GLOBAL_CACHE.totmem > GLOBAL_CACHE.maxmem) {
			/* find an existing tile to unload */
			for(gtile=GLOBAL_CACHE.tiles.last; gtile; gtile=gtile->prev)
				if(gtile->refcount == 0 && gtile->loading == 0)
					break;
		}

		if(gtile) {
			/* found a tile to unload */
			imb_global_cache_tile_unload(gtile);
			BLI_ghash_remove(GLOBAL_CACHE.tilehash, gtile, NULL, NULL);
			BLI_remlink(&GLOBAL_CACHE.tiles, gtile);
		}
		else {
			/* allocate a new tile or reuse unused */
			if(GLOBAL_CACHE.unused.first) {
				gtile= GLOBAL_CACHE.unused.first;
				BLI_remlink(&GLOBAL_CACHE.unused, gtile);
			}
			else
				gtile= BLI_memarena_alloc(GLOBAL_CACHE.memarena, sizeof(ImGlobalTile));
		}

		/* setup new tile */
		gtile->ibuf= ibuf;
		gtile->tx= tx;
		gtile->ty= ty;
		gtile->refcount= 1;
		gtile->loading= 1;

		BLI_ghash_insert(GLOBAL_CACHE.tilehash, gtile, gtile);
		BLI_addhead(&GLOBAL_CACHE.tiles, gtile);

		/* mark as being loaded and unlock to allow other threads to load too */
		GLOBAL_CACHE.totmem += sizeof(unsigned int)*ibuf->tilex*ibuf->tiley;

		BLI_mutex_unlock(&GLOBAL_CACHE.mutex);

		/* load from disk */
		imb_global_cache_tile_load(gtile);

		/* mark as done loading */
		gtile->loading= 0;
	}

	return gtile;
}

/***************************** Per-Thread Cache ******************************/

static unsigned int *imb_thread_cache_get_tile(ImThreadTileCache *cache, ImBuf *ibuf, int tx, int ty)
{
	ImThreadTile *ttile, lookuptile;
	ImGlobalTile *gtile, *replacetile;
	int toffs= ibuf->xtiles*ty + tx;

	/* test if it is already in our thread local cache */
	if((ttile=cache->tiles.first)) {
		/* check last used tile before going to hash */
		if(ttile->ibuf == ibuf && ttile->tx == tx && ttile->ty == ty)
			return ibuf->tiles[toffs];

		/* find tile in hash */
		lookuptile.ibuf = ibuf;
		lookuptile.tx = tx;
		lookuptile.ty = ty;

		if((ttile=BLI_ghash_lookup(cache->tilehash, &lookuptile))) {
			BLI_remlink(&cache->tiles, ttile);
			BLI_addhead(&cache->tiles, ttile);

			return ibuf->tiles[toffs];
		}
	}

	/* not found, have to do slow lookup in global cache */
	if(cache->unused.first == NULL) {
		ttile= cache->tiles.last;
		replacetile= ttile->global;
		BLI_remlink(&cache->tiles, ttile);
		BLI_ghash_remove(cache->tilehash, ttile, NULL, NULL);
	}
	else {
		ttile= cache->unused.first;
		replacetile= NULL;
		BLI_remlink(&cache->unused, ttile);
	}

	BLI_addhead(&cache->tiles, ttile);
	BLI_ghash_insert(cache->tilehash, ttile, ttile);

	gtile= imb_global_cache_get_tile(ibuf, tx, ty, replacetile);

	ttile->ibuf= gtile->ibuf;
	ttile->tx= gtile->tx;
	ttile->ty= gtile->ty;
	ttile->global= gtile;

	return ibuf->tiles[toffs];
}

unsigned int *IMB_gettile(ImBuf *ibuf, int tx, int ty, int thread)
{
	return imb_thread_cache_get_tile(&GLOBAL_CACHE.thread_cache[thread+1], ibuf, tx, ty);
}

void IMB_tiles_to_rect(ImBuf *ibuf)
{
	ImBuf *mipbuf;
	ImGlobalTile *gtile;
	unsigned int *to, *from;
	int a, tx, ty, y, w, h;

	for(a=0; a<ibuf->miptot; a++) {
		mipbuf= IMB_getmipmap(ibuf, a);

		/* don't call imb_addrectImBuf, it frees all mipmaps */
		if(!mipbuf->rect) {
			if((mipbuf->rect = MEM_mapallocN(ibuf->x*ibuf->y*sizeof(unsigned int), "imb_addrectImBuf"))) {
				mipbuf->mall |= IB_rect;
				mipbuf->flags |= IB_rect;
			}
			else
				break;
		}

		for(ty=0; ty<mipbuf->ytiles; ty++) {
			for(tx=0; tx<mipbuf->xtiles; tx++) {
				/* acquire tile through cache, this assumes cache is initialized,
				   which it is always now but it's a weak assumption ... */
				gtile= imb_global_cache_get_tile(mipbuf, tx, ty, NULL);

				/* setup pointers */
				from= mipbuf->tiles[mipbuf->xtiles*ty + tx];
				to= mipbuf->rect + mipbuf->x*ty*mipbuf->tiley + tx*mipbuf->tilex;

				/* exception in tile width/height for tiles at end of image */
				w= (tx == mipbuf->xtiles-1)? mipbuf->x - tx*mipbuf->tilex: mipbuf->tilex;
				h= (ty == mipbuf->ytiles-1)? mipbuf->y - ty*mipbuf->tiley: mipbuf->tiley;

				for(y=0; y<h; y++) {
					memcpy(to, from, sizeof(unsigned int)*w);
					from += mipbuf->tilex;
					to += mipbuf->x;
				}

				/* decrease refcount for tile again */
				BLI_mutex_lock(&GLOBAL_CACHE.mutex);
				gtile->refcount--;
				BLI_mutex_unlock(&GLOBAL_CACHE.mutex);
			}
		}
	}
}

