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
	int totalloc, totused; /*total number of elements allocated in total, and currently in use*/
	int use_sysmalloc;
}BLI_mempool;

BLI_mempool *BLI_mempool_create(int esize, int tote, int pchunk, int use_sysmalloc)
{	BLI_mempool  *pool = NULL;
	BLI_freenode *lasttail = NULL, *curnode = NULL;
	int i,j, maxchunks;
	char *addr;
	
	if (esize < sizeof(void*))
		esize = sizeof(void*);
	
	/*allocate the pool structure*/
	pool = use_sysmalloc ? malloc(sizeof(BLI_mempool)) : MEM_mallocN(sizeof(BLI_mempool), "memory pool");
	pool->esize = esize;
	pool->use_sysmalloc = use_sysmalloc;
	pool->pchunk = pchunk;	
	pool->csize = esize * pchunk;
	pool->chunks.first = pool->chunks.last = NULL;
	
	maxchunks = tote / pchunk + 1;
	
	/*allocate the actual chunks*/
	for(i=0; i < maxchunks; i++){
		BLI_mempool_chunk *mpchunk = use_sysmalloc ? malloc(sizeof(BLI_mempool_chunk)) : MEM_mallocN(sizeof(BLI_mempool_chunk), "BLI_Mempool Chunk");
		mpchunk->next = mpchunk->prev = NULL;
		mpchunk->data = use_sysmalloc ? malloc(pool->csize) : MEM_mallocN(pool->csize, "BLI Mempool Chunk Data");
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

		pool->totalloc += pool->pchunk;
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

	pool->totused++;

	if(!(pool->free)){
		/*need to allocate a new chunk*/
		BLI_mempool_chunk *mpchunk = pool->use_sysmalloc ? malloc(sizeof(BLI_mempool_chunk)) :  MEM_mallocN(sizeof(BLI_mempool_chunk), "BLI_Mempool Chunk");
		mpchunk->next = mpchunk->prev = NULL;
		mpchunk->data = pool->use_sysmalloc ? malloc(pool->csize) : MEM_mallocN(pool->csize, "BLI_Mempool Chunk Data");
		BLI_addtail(&(pool->chunks), mpchunk);

		pool->free = mpchunk->data; /*start of the list*/
		for(addr = mpchunk->data, j=0; j < pool->pchunk; j++){
			curnode = ((BLI_freenode*)addr);
			addr += pool->esize;
			curnode->next = (BLI_freenode*)addr;
		}
		curnode->next = NULL; /*terminate the list*/

		pool->totalloc += pool->pchunk;
	}

	retval = pool->free;
	pool->free = pool->free->next;
	//memset(retval, 0, pool->esize);
	return retval;
}

void *BLI_mempool_calloc(BLI_mempool *pool){
	void *retval=NULL;
	retval = BLI_mempool_alloc(pool);
	memset(retval, 0, pool->esize);
	return retval;
}


void BLI_mempool_free(BLI_mempool *pool, void *addr){ //doesnt protect against double frees, dont be stupid!
	BLI_freenode *newhead = addr;
	BLI_freenode *curnode=NULL;
	char *tmpaddr=NULL;
	int i;

	newhead->next = pool->free;
	pool->free = newhead;

	pool->totused--;

	/*nothing is in use; free all the chunks except the first*/
	if (pool->totused == 0) {
		BLI_mempool_chunk *mpchunk=NULL, *first;

		first = pool->chunks.first;
		BLI_remlink(&pool->chunks, first);

		for(mpchunk = pool->chunks.first; mpchunk; mpchunk = mpchunk->next) 
			pool->use_sysmalloc ? free(mpchunk->data) : MEM_freeN(mpchunk->data);
		
		pool->use_sysmalloc ? BLI_freelist(&(pool->chunks)) : BLI_freelistN(&(pool->chunks));
		
		BLI_addtail(&pool->chunks, first);
		pool->totalloc = pool->pchunk;

		pool->free = first->data; /*start of the list*/
		for(tmpaddr = first->data, i=0; i < pool->pchunk; i++){
			curnode = ((BLI_freenode*)tmpaddr);
			tmpaddr += pool->esize;
			curnode->next = (BLI_freenode*)tmpaddr;
		}
		curnode->next = NULL; /*terminate the list*/
	}
}

void BLI_mempool_destroy(BLI_mempool *pool)
{
	BLI_mempool_chunk *mpchunk=NULL;
	for(mpchunk = pool->chunks.first; mpchunk; mpchunk = mpchunk->next)
		pool->use_sysmalloc ? free(mpchunk->data) : MEM_freeN(mpchunk->data);
	
	pool->use_sysmalloc ? BLI_freelist(&(pool->chunks)) : BLI_freelistN(&(pool->chunks));	
	pool->use_sysmalloc ? free(pool) : MEM_freeN(pool);
}
