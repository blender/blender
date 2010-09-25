/**
 * Simple fast memory allocator
 * 
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef BLI_MEMPOOL_H
#define BLI_MEMPOOL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "BKE_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_blenlib.h"
#include <string.h>

#ifndef BLI_MEMPOOL_INTERN
//struct BLI_mempool;
//struct BLI_mempool_chunk;
//typedef struct BLI_mempool BLI_mempool;
#endif

typedef struct BLI_freenode{
	struct BLI_freenode *next;
	int freeword; /*used to identify this as a freed node*/
}BLI_freenode;

typedef struct BLI_mempool_chunk{
	struct BLI_mempool_chunk *next, *prev;
	void *data;
}BLI_mempool_chunk;

typedef struct BLI_mempool{
	struct ListBase chunks;
	int esize, csize, pchunk;		/*size of elements and chunks in bytes and number of elements per chunk*/
	BLI_freenode *free;		/*free element list. Interleaved into chunk datas.*/
	int totalloc, totused; /*total number of elements allocated in total, and currently in use*/
	int use_sysmalloc, allow_iter;
}BLI_mempool;

/*allow_iter allows iteration on this mempool.  note: this requires that the
  first four bytes of the elements never contain the character string
  'free'.  use with care.*/

BLI_mempool *BLI_mempool_create(int esize, int tote, int pchunk,
								int use_sysmalloc, int allow_iter);
//void *BLI_mempool_alloc(BLI_mempool *pool);
void *BLI_mempool_calloc(BLI_mempool *pool);
void BLI_mempool_free(BLI_mempool *pool, void *addr);
void BLI_mempool_destroy(BLI_mempool *pool);
int BLI_mempool_count(BLI_mempool *pool);

/** iteration stuff.  note: this may easy to produce bugs with **/
/*private structure*/
typedef struct BLI_mempool_iter {
	BLI_mempool *pool;
	struct BLI_mempool_chunk *curchunk;
	int curindex;
} BLI_mempool_iter;

/*allow iteration on this mempool.  note: this requires that the
  first four bytes of the elements never contain the character string
  'free'.  use with care.*/
void BLI_mempool_allow_iter(BLI_mempool *pool);
void BLI_mempool_iternew(BLI_mempool *pool, BLI_mempool_iter *iter);
void *BLI_mempool_iterstep(BLI_mempool_iter *iter);

/************ inlined stuff ***********/
#define FREEWORD MAKE_ID('f', 'r', 'e', 'e')
#include "MEM_guardedalloc.h"

BM_INLINE void *BLI_mempool_alloc(BLI_mempool *pool) {
	void *retval=NULL;
	BLI_freenode *curnode=NULL;
	char *addr=NULL;
	int j;
	
	if (!pool) return NULL;
	
	pool->totused++;

	if(!(pool->free)){
		/*need to allocate a new chunk*/
		BLI_mempool_chunk *mpchunk = pool->use_sysmalloc ? (BLI_mempool_chunk*)malloc(sizeof(BLI_mempool_chunk)) :  (BLI_mempool_chunk*)MEM_mallocN(sizeof(BLI_mempool_chunk), "BLI_Mempool Chunk");
		mpchunk->next = mpchunk->prev = NULL;
		mpchunk->data = pool->use_sysmalloc ? malloc(pool->csize) : MEM_mallocN(pool->csize, "BLI_Mempool Chunk Data");
		BLI_addtail(&(pool->chunks), mpchunk);

		pool->free = (BLI_freenode*)mpchunk->data; /*start of the list*/
		if (pool->allow_iter)
			pool->free->freeword = FREEWORD;
		for(addr = (char*)mpchunk->data, j=0; j < pool->pchunk; j++){
			curnode = ((BLI_freenode*)addr);
			addr += pool->esize;
			curnode->next = (BLI_freenode*)addr;

			if (pool->allow_iter) {
				curnode->freeword = FREEWORD;
				if (j != pool->pchunk-1)
					curnode->next->freeword = FREEWORD;
			}
		}
		curnode->next = NULL; /*terminate the list*/

		pool->totalloc += pool->pchunk;
	}

	retval = pool->free;
	if (pool->allow_iter)
		pool->free->freeword = 0x7FFFFFFF;

	pool->free = pool->free->next;
	//memset(retval, 0, pool->esize);
	return retval;
}

#ifdef __cplusplus
}
#endif

#endif
