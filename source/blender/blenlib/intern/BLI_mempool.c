/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/*
	Simple, fast memory allocator for allocating many elements of the same size.
*/

#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"
#include "DNA_listBase.h"
#include "BLI_linklist.h"
#include <string.h> 

typedef struct BLI_freenode{
	struct BLI_freenode *next;
}BLI_freenode;

typedef struct BLI_mempool_chunk{
	struct BLI_mempool_chunk *next, *prev;
	void *data;
}BLI_mempool_chunk;

typedef struct BLI_mempool{
	struct ListBase chunks;
	int esize, csize, pchunk;		/*size of elements and chunks in bytes and number of elements per chunk*/
	struct BLI_freenode	*free;		/*free element list. Interleaved into chunk datas.*/
}BLI_mempool;

BLI_mempool *BLI_mempool_create(int esize, int tote, int pchunk)
{	BLI_mempool  *pool = NULL;
	BLI_freenode *lasttail = NULL, *curnode = NULL;
	int i,j, maxchunks;
	char *addr;

	/*allocate the pool structure*/
	pool = MEM_mallocN(sizeof(BLI_mempool),"memory pool");
	pool->esize = esize;
	pool->pchunk = pchunk;	
	pool->csize = esize * pchunk;
	pool->chunks.first = pool->chunks.last = NULL;
	
	maxchunks = tote / pchunk;
	
	/*allocate the actual chunks*/
	for(i=0; i < maxchunks; i++){
		BLI_mempool_chunk *mpchunk = MEM_mallocN(sizeof(BLI_mempool_chunk), "BLI_Mempool Chunk");
		mpchunk->next = mpchunk->prev = NULL;
		mpchunk->data = MEM_mallocN(pool->csize, "BLI Mempool Chunk Data");
		BLI_addtail(&(pool->chunks), mpchunk);
		
		if(i==0) pool->free = mpchunk->data; /*start of the list*/
		/*loop through the allocated data, building the pointer structures*/
		for(addr = mpchunk->data, j=0; j < pool->pchunk; j++){
			curnode = ((BLI_freenode*)addr);
			addr += pool->esize;
			curnode->next = (BLI_freenode*)addr;
		}
		/*final pointer in the previously allocated chunk is wrong.*/
		if(lasttail) lasttail->next = mpchunk->data;
		/*set the end of this chunks memoryy to the new tail for next iteration*/
		lasttail = curnode;
	}
	/*terminate the list*/
	curnode->next = NULL;
	return pool;
}

void *BLI_mempool_alloc(BLI_mempool *pool){
	void *retval=NULL;
	BLI_freenode *curnode=NULL;
	char *addr=NULL;
	int j;

	if(!(pool->free)){
		/*need to allocate a new chunk*/
		BLI_mempool_chunk *mpchunk = MEM_mallocN(sizeof(BLI_mempool_chunk), "BLI_Mempool Chunk");
		mpchunk->next = mpchunk->prev = NULL;
		mpchunk->data = MEM_mallocN(pool->csize, "BLI_Mempool Chunk Data");
		BLI_addtail(&(pool->chunks), mpchunk);

		pool->free = mpchunk->data; /*start of the list*/
		for(addr = mpchunk->data, j=0; j < pool->pchunk; j++){
			curnode = ((BLI_freenode*)addr);
			addr += pool->esize;
			curnode->next = (BLI_freenode*)addr;
		}
		curnode->next = NULL; /*terminate the list*/
	}

	retval = pool->free;
	pool->free = pool->free->next;
	//memset(retval, 0, pool->esize);
	return retval;
}
void BLI_mempool_free(BLI_mempool *pool, void *addr){ //doesnt protect against double frees, dont be stupid!
	BLI_freenode *newhead = addr;
	newhead->next = pool->free;
	pool->free = newhead;
}
void BLI_mempool_destroy(BLI_mempool *pool)
{
	BLI_mempool_chunk *mpchunk=NULL;
	for(mpchunk = pool->chunks.first; mpchunk; mpchunk = mpchunk->next) MEM_freeN(mpchunk->data);
	BLI_freelistN(&(pool->chunks));
	MEM_freeN(pool);
}