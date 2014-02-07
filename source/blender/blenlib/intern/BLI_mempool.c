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
 * The Original Code is Copyright (C) 2008 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffery Bantle
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/BLI_mempool.c
 *  \ingroup bli
 *
 * Simple, fast memory allocator for allocating many elements of the same size.
 */

#include <string.h>
#include <stdlib.h>

#include "BLI_utildefines.h"
#include "BLI_listbase.h"

#include "BLI_mempool.h" /* own include */

#include "DNA_listBase.h"

#include "MEM_guardedalloc.h"

#include "BLI_strict_flags.h"  /* keep last */

#ifdef WITH_MEM_VALGRIND
#  include "valgrind/memcheck.h"
#endif

/* note: copied from BLO_blend_defs.h, don't use here because we're in BLI */
#ifdef __BIG_ENDIAN__
/* Big Endian */
#  define MAKE_ID(a, b, c, d) ( (int)(a) << 24 | (int)(b) << 16 | (c) << 8 | (d) )
#else
/* Little Endian */
#  define MAKE_ID(a, b, c, d) ( (int)(d) << 24 | (int)(c) << 16 | (b) << 8 | (a) )
#endif

#define FREEWORD MAKE_ID('f', 'r', 'e', 'e')

/* currently totalloc isnt used */
// #define USE_TOTALLOC

/* when undefined, merge the allocs for BLI_mempool_chunk and its data */
// #define USE_DATA_PTR

#ifndef NDEBUG
static bool mempool_debug_memset = false;
#endif

/**
 * A free element from #BLI_mempool_chunk. Data is cast to this type and stored in
 * #BLI_mempool.free as a single linked list, each item #BLI_mempool.esize large.
 *
 * Each element represents a block which BLI_mempool_alloc may return.
 */
typedef struct BLI_freenode {
	struct BLI_freenode *next;
	int freeword; /* used to identify this as a freed node */
} BLI_freenode;

/**
 * A chunk of memory in the mempool stored in
 * #BLI_mempool.chunks as a double linked list.
 */
typedef struct BLI_mempool_chunk {
	struct BLI_mempool_chunk *next, *prev;
#ifdef USE_DATA_PTR
	void *_data;
#endif
} BLI_mempool_chunk;

/**
 * The mempool, stores and tracks memory \a chunks and elements within those chunks \a free.
 */
struct BLI_mempool {
	struct ListBase chunks;
	unsigned int esize;         /* element size in bytes */
	unsigned int csize;         /* chunk size in bytes */
	unsigned int pchunk;        /* number of elements per chunk */
	unsigned int flag;
	/* keeps aligned to 16 bits */

	BLI_freenode *free;         /* free element list. Interleaved into chunk datas. */
	unsigned int maxchunks;     /* use to know how many chunks to keep for BLI_mempool_clear */
	unsigned int totused;       /* number of elements currently in use */
#ifdef USE_TOTALLOC
	unsigned int totalloc;          /* number of elements allocated in total */
#endif
};

#define MEMPOOL_ELEM_SIZE_MIN (sizeof(void *) * 2)

#ifdef USE_DATA_PTR
#  define CHUNK_DATA(chunk) (chunk)->_data
#else
#  define CHUNK_DATA(chunk) (CHECK_TYPE_INLINE(chunk, BLI_mempool_chunk *), (void *)((chunk) + 1))
#endif

/**
 * \return the number of chunks to allocate based on how many elements are needed.
 */
BLI_INLINE unsigned int mempool_maxchunks(const unsigned int totelem, const unsigned int pchunk)
{
	return totelem / pchunk + 1;
}

static BLI_mempool_chunk *mempool_chunk_alloc(BLI_mempool *pool)
{
	BLI_mempool_chunk *mpchunk;
#ifdef USE_DATA_PTR
	if (pool->flag & BLI_MEMPOOL_SYSMALLOC) {
		mpchunk = malloc(sizeof(BLI_mempool_chunk));
		CHUNK_DATA(mpchunk) = malloc((size_t)pool->csize);
	}
	else {
		mpchunk = MEM_mallocN(sizeof(BLI_mempool_chunk), "BLI_Mempool Chunk");
		CHUNK_DATA(mpchunk) = MEM_mallocN((size_t)pool->csize, "BLI Mempool Chunk Data");
	}
#else
	if (pool->flag & BLI_MEMPOOL_SYSMALLOC) {
		mpchunk = malloc(sizeof(BLI_mempool_chunk) + (size_t)pool->csize);
	}
	else {
		mpchunk = MEM_mallocN(sizeof(BLI_mempool_chunk) + (size_t)pool->csize, "BLI_Mempool Chunk");
	}
#endif

	return mpchunk;
}

/**
 * Initialize a chunk and add into \a pool->chunks
 *
 * \param pool  The pool to add the chunk into.
 * \param mpchunk  The new uninitialized chunk (can be malloc'd)
 * \param lasttail  The last element of the previous chunk
 * (used when building free chunks initially)
 * \return The last chunk,
 */
static BLI_freenode *mempool_chunk_add(BLI_mempool *pool, BLI_mempool_chunk *mpchunk,
                                       BLI_freenode *lasttail)
{
	BLI_freenode *curnode = NULL;
	const unsigned int pchunk_last = pool->pchunk - 1;
	char *addr;
	unsigned int j;

	mpchunk->next = mpchunk->prev = NULL;
	BLI_addtail(&(pool->chunks), mpchunk);

	if (pool->free == NULL) {
		pool->free = CHUNK_DATA(mpchunk); /* start of the list */
		if (pool->flag & BLI_MEMPOOL_ALLOW_ITER) {
			pool->free->freeword = FREEWORD;
		}
	}

	/* loop through the allocated data, building the pointer structures */
	for (addr = CHUNK_DATA(mpchunk), j = 0; j != pchunk_last; j++) {
		curnode = ((BLI_freenode *)addr);
		addr += pool->esize;
		curnode->next = (BLI_freenode *)addr;
		if (pool->flag & BLI_MEMPOOL_ALLOW_ITER) {
			if (j != pchunk_last)
				curnode->next->freeword = FREEWORD;
			curnode->freeword = FREEWORD;
		}
	}

	/* terminate the list,
	 * will be overwritten if 'curnode' gets passed in again as 'lasttail' */
	curnode->next = NULL;

#ifdef USE_TOTALLOC
	pool->totalloc += pool->pchunk;
#endif

	/* final pointer in the previously allocated chunk is wrong */
	if (lasttail) {
		lasttail->next = CHUNK_DATA(mpchunk);
		if (pool->flag & BLI_MEMPOOL_ALLOW_ITER) {
			lasttail->freeword = FREEWORD;
		}
	}

	return curnode;
}

static void mempool_chunk_free(BLI_mempool_chunk *mpchunk, const unsigned int flag)
{
	if (flag & BLI_MEMPOOL_SYSMALLOC) {
#ifdef USE_DATA_PTR
		free(CHUNK_DATA(mpchunk));
#endif
		free(mpchunk);
	}
	else {
#ifdef USE_DATA_PTR
		MEM_freeN(CHUNK_DATA(mpchunk));
#endif
		MEM_freeN(mpchunk);
	}
}

static void mempool_chunk_free_all(ListBase *chunks, const unsigned int flag)
{
	BLI_mempool_chunk *mpchunk, *mpchunk_next;

	for (mpchunk = chunks->first; mpchunk; mpchunk = mpchunk_next) {
		mpchunk_next = mpchunk->next;
		mempool_chunk_free(mpchunk, flag);
	}
	BLI_listbase_clear(chunks);
}

BLI_mempool *BLI_mempool_create(unsigned int esize, unsigned int totelem,
                                unsigned int pchunk, unsigned int flag)
{
	BLI_mempool *pool = NULL;
	BLI_freenode *lasttail = NULL;
	unsigned int i, maxchunks;

	/* allocate the pool structure */
	if (flag & BLI_MEMPOOL_SYSMALLOC) {
		pool = malloc(sizeof(BLI_mempool));
	}
	else {
		pool = MEM_mallocN(sizeof(BLI_mempool), "memory pool");
	}

	/* set the elem size */
	if (esize < (int)MEMPOOL_ELEM_SIZE_MIN) {
		esize = (int)MEMPOOL_ELEM_SIZE_MIN;
	}

	if (flag & BLI_MEMPOOL_ALLOW_ITER) {
		pool->esize = MAX2(esize, (unsigned int)sizeof(BLI_freenode));
	}
	else {
		pool->esize = esize;
	}

	maxchunks = mempool_maxchunks(totelem, pchunk);

	pool->flag = flag;
	pool->pchunk = pchunk;
	pool->csize = esize * pchunk;
	BLI_listbase_clear(&pool->chunks);
	pool->free = NULL;  /* mempool_chunk_add assigns */
	pool->maxchunks = maxchunks;
#ifdef USE_TOTALLOC
	pool->totalloc = 0;
#endif
	pool->totused = 0;

	/* allocate the actual chunks */
	for (i = 0; i < maxchunks; i++) {
		BLI_mempool_chunk *mpchunk = mempool_chunk_alloc(pool);
		lasttail = mempool_chunk_add(pool, mpchunk, lasttail);
	}

#ifdef WITH_MEM_VALGRIND
	VALGRIND_CREATE_MEMPOOL(pool, 0, false);
#endif

	return pool;
}

void *BLI_mempool_alloc(BLI_mempool *pool)
{
	void *retval = NULL;

	pool->totused++;

	if (UNLIKELY(pool->free == NULL)) {
		/* need to allocate a new chunk */
		BLI_mempool_chunk *mpchunk = mempool_chunk_alloc(pool);
		mempool_chunk_add(pool, mpchunk, NULL);
	}

	retval = pool->free;

	if (pool->flag & BLI_MEMPOOL_ALLOW_ITER) {
		pool->free->freeword = 0x7FFFFFFF;
	}

	pool->free = pool->free->next;

#ifdef WITH_MEM_VALGRIND
	VALGRIND_MEMPOOL_ALLOC(pool, retval, pool->esize);
#endif

	return retval;
}

void *BLI_mempool_calloc(BLI_mempool *pool)
{
	void *retval = BLI_mempool_alloc(pool);
	memset(retval, 0, (size_t)pool->esize);
	return retval;
}

/**
 * Free an element from the mempool.
 *
 * \note doesnt protect against double frees, don't be stupid!
 */
void BLI_mempool_free(BLI_mempool *pool, void *addr)
{
	BLI_freenode *newhead = addr;

#ifndef NDEBUG
	{
		BLI_mempool_chunk *chunk;
		bool found = false;
		for (chunk = pool->chunks.first; chunk; chunk = chunk->next) {
			if (ARRAY_HAS_ITEM((char *)addr, (char *)CHUNK_DATA(chunk), pool->csize)) {
				found = true;
				break;
			}
		}
		if (!found) {
			BLI_assert(!"Attempt to free data which is not in pool.\n");
		}
	}

	/* enable for debugging */
	if (UNLIKELY(mempool_debug_memset)) {
		memset(addr, 255, pool->esize);
	}
#endif

	if (pool->flag & BLI_MEMPOOL_ALLOW_ITER) {
#ifndef NDEBUG
		/* this will detect double free's */
		BLI_assert(newhead->freeword != FREEWORD);
#endif
		newhead->freeword = FREEWORD;
	}

	newhead->next = pool->free;
	pool->free = newhead;

	pool->totused--;

#ifdef WITH_MEM_VALGRIND
	VALGRIND_MEMPOOL_FREE(pool, addr);
#endif

	/* nothing is in use; free all the chunks except the first */
	if (UNLIKELY(pool->totused == 0)) {
		BLI_freenode *curnode = NULL;
		char *tmpaddr = NULL;
		unsigned int i;
		BLI_mempool_chunk *first;

		first = BLI_pophead(&pool->chunks);
		mempool_chunk_free_all(&pool->chunks, pool->flag);
		BLI_addtail(&pool->chunks, first);
#ifdef USE_TOTALLOC
		pool->totalloc = pool->pchunk;
#endif

		/* temp alloc so valgrind doesn't complain when setting free'd blocks 'next' */
#ifdef WITH_MEM_VALGRIND
		VALGRIND_MEMPOOL_ALLOC(pool, CHUNK_DATA(first), pool->csize);
#endif
		pool->free = CHUNK_DATA(first); /* start of the list */
		for (tmpaddr = CHUNK_DATA(first), i = 0; i < pool->pchunk; i++) {
			curnode = ((BLI_freenode *)tmpaddr);
			tmpaddr += pool->esize;
			curnode->next = (BLI_freenode *)tmpaddr;
		}
		curnode->next = NULL; /* terminate the list */

#ifdef WITH_MEM_VALGRIND
		VALGRIND_MEMPOOL_FREE(pool, CHUNK_DATA(first));
#endif
	}
}

int BLI_mempool_count(BLI_mempool *pool)
{
	return (int)pool->totused;
}

void *BLI_mempool_findelem(BLI_mempool *pool, unsigned int index)
{
	BLI_assert(pool->flag & BLI_MEMPOOL_ALLOW_ITER);

	if (index < pool->totused) {
		/* we could have some faster mem chunk stepping code inline */
		BLI_mempool_iter iter;
		void *elem;
		BLI_mempool_iternew(pool, &iter);
		for (elem = BLI_mempool_iterstep(&iter); index-- != 0; elem = BLI_mempool_iterstep(&iter)) {
			/* do nothing */
		}
		return elem;
	}

	return NULL;
}

/**
 * Fill in \a data with pointers to each element of the mempool,
 * to create lookup table.
 *
 * \param pool Pool to create a table from.
 * \param data array of pointers at least the size of 'pool->totused'
 */
void BLI_mempool_as_table(BLI_mempool *pool, void **data)
{
	BLI_mempool_iter iter;
	void *elem;
	void **p = data;
	BLI_assert(pool->flag & BLI_MEMPOOL_ALLOW_ITER);
	BLI_mempool_iternew(pool, &iter);
	while ((elem = BLI_mempool_iterstep(&iter))) {
		*p++ = elem;
	}
	BLI_assert((unsigned int)(p - data) == pool->totused);
}

/**
 * A version of #BLI_mempool_as_table that allocates and returns the data.
 */
void **BLI_mempool_as_tableN(BLI_mempool *pool, const char *allocstr)
{
	void **data = MEM_mallocN((size_t)pool->totused * sizeof(void *), allocstr);
	BLI_mempool_as_table(pool, data);
	return data;
}

/**
 * Fill in \a data with the contents of the mempool.
 */
void BLI_mempool_as_array(BLI_mempool *pool, void *data)
{
	BLI_mempool_iter iter;
	char *elem, *p = data;
	BLI_assert(pool->flag & BLI_MEMPOOL_ALLOW_ITER);
	BLI_mempool_iternew(pool, &iter);
	while ((elem = BLI_mempool_iterstep(&iter))) {
		memcpy(p, elem, (size_t)pool->esize);
		p += pool->esize;
	}
	BLI_assert((unsigned int)(p - (char *)data) == pool->totused * pool->esize);
}

/**
 * A version of #BLI_mempool_as_array that allocates and returns the data.
 */
void *BLI_mempool_as_arrayN(BLI_mempool *pool, const char *allocstr)
{
	char *data = MEM_mallocN((size_t)(pool->totused * pool->esize), allocstr);
	BLI_mempool_as_array(pool, data);
	return data;
}

/**
 * Create a new mempool iterator, \a BLI_MEMPOOL_ALLOW_ITER flag must be set.
 */
void BLI_mempool_iternew(BLI_mempool *pool, BLI_mempool_iter *iter)
{
	BLI_assert(pool->flag & BLI_MEMPOOL_ALLOW_ITER);

	iter->pool = pool;
	iter->curchunk = pool->chunks.first;
	iter->curindex = 0;
}

#if 0
/* unoptimized, more readable */

static void *bli_mempool_iternext(BLI_mempool_iter *iter)
{
	void *ret = NULL;

	if (!iter->curchunk || !iter->pool->totused) return NULL;

	ret = ((char *)CHUNK_DATA(iter->curchunk)) + (iter->pool->esize * iter->curindex);

	iter->curindex++;

	if (iter->curindex == iter->pool->pchunk) {
		iter->curchunk = iter->curchunk->next;
		iter->curindex = 0;
	}

	return ret;
}

void *BLI_mempool_iterstep(BLI_mempool_iter *iter)
{
	BLI_freenode *ret;

	do {
		ret = bli_mempool_iternext(iter);
	} while (ret && ret->freeword == FREEWORD);

	return ret;
}

#else

/* optimized version of code above */

/**
 * Step over the iterator, returning the mempool item or NULL.
 */
void *BLI_mempool_iterstep(BLI_mempool_iter *iter)
{
	BLI_freenode *ret;

	do {
		if (LIKELY(iter->curchunk)) {
			ret = (BLI_freenode *)(((char *)CHUNK_DATA(iter->curchunk)) + (iter->pool->esize * iter->curindex));
		}
		else {
			return NULL;
		}

		if (UNLIKELY(++iter->curindex == iter->pool->pchunk)) {
			iter->curindex = 0;
			iter->curchunk = iter->curchunk->next;
		}
	} while (ret->freeword == FREEWORD);

	return ret;
}

#endif

/**
 * Empty the pool, as if it were just created.
 *
 * \param pool The pool to clear.
 * \param totelem_reserve  Optionally reserve how many items should be kept from clearing.
 */
void BLI_mempool_clear_ex(BLI_mempool *pool, const int totelem_reserve)
{
	BLI_mempool_chunk *mpchunk;
	BLI_mempool_chunk *mpchunk_next;
	unsigned int maxchunks;

	ListBase chunks_temp;
	BLI_freenode *lasttail = NULL;

#ifdef WITH_MEM_VALGRIND
	VALGRIND_DESTROY_MEMPOOL(pool);
	VALGRIND_CREATE_MEMPOOL(pool, 0, false);
#endif

	if (totelem_reserve == -1) {
		maxchunks = pool->maxchunks;
	}
	else {
		maxchunks = mempool_maxchunks((unsigned int)totelem_reserve, pool->pchunk);
	}

	/* free all after pool->maxchunks  */

	for (mpchunk = BLI_findlink(&pool->chunks, (int)maxchunks); mpchunk; mpchunk = mpchunk_next) {
		mpchunk_next = mpchunk->next;
		BLI_remlink(&pool->chunks, mpchunk);
		mempool_chunk_free(mpchunk, pool->flag);
	}

	/* re-initialize */
	pool->free = NULL;
	pool->totused = 0;
#ifdef USE_TOTALLOC
	pool->totalloc = 0;
#endif

	chunks_temp = pool->chunks;
	BLI_listbase_clear(&pool->chunks);

	while ((mpchunk = BLI_pophead(&chunks_temp))) {
		lasttail = mempool_chunk_add(pool, mpchunk, lasttail);
	}
}

/**
 * Wrap #BLI_mempool_clear_ex with no reserve set.
 */
void BLI_mempool_clear(BLI_mempool *pool)
{
	BLI_mempool_clear_ex(pool, -1);
}

/**
 * Free the mempool its self (and all elements).
 */
void BLI_mempool_destroy(BLI_mempool *pool)
{
	mempool_chunk_free_all(&pool->chunks, pool->flag);

#ifdef WITH_MEM_VALGRIND
	VALGRIND_DESTROY_MEMPOOL(pool);
#endif

	if (pool->flag & BLI_MEMPOOL_SYSMALLOC) {
		free(pool);
	}
	else {
		MEM_freeN(pool);
	}
}

#ifndef NDEBUG
void BLI_mempool_set_memory_debug(void)
{
	mempool_debug_memset = true;
}
#endif
