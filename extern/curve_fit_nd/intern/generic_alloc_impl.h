/*
 * Copyright (c) 2016, Blender Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file generic_alloc_impl.c
 *  \ingroup curve_fit
 *
 * Simple Memory Chunking Allocator
 * ================================
 *
 * Defines need to be set:
 * - #TPOOL_IMPL_PREFIX: Prefix to use for the API.
 * - #TPOOL_ALLOC_TYPE: Struct type this pool handles.
 * - #TPOOL_STRUCT: Name for pool struct name.
 * - #TPOOL_CHUNK_SIZE: Chunk size (optional), use 64kb when not defined.
 *
 * \note #TPOOL_ALLOC_TYPE must be at least ``sizeof(void *)``.
 *
 * Defines the API, uses #TPOOL_IMPL_PREFIX to prefix each function.
 *
 * - *_pool_create()
 * - *_pool_destroy()
 * - *_pool_clear()
 *
 * - *_pool_elem_alloc()
 * - *_pool_elem_calloc()
 * - *_pool_elem_free()
 */

/* check we're not building directly */
#if !defined(TPOOL_IMPL_PREFIX) || \
    !defined(TPOOL_ALLOC_TYPE) || \
    !defined(TPOOL_STRUCT)
#  error "This file can't be compiled directly, include in another source file"
#endif

#define _CONCAT_AUX(MACRO_ARG1, MACRO_ARG2) MACRO_ARG1 ## MACRO_ARG2
#define _CONCAT(MACRO_ARG1, MACRO_ARG2) _CONCAT_AUX(MACRO_ARG1, MACRO_ARG2)
#define _TPOOL_PREFIX(id) _CONCAT(TPOOL_IMPL_PREFIX, _##id)

/* local identifiers */
#define pool_create		_TPOOL_PREFIX(pool_create)
#define pool_destroy	_TPOOL_PREFIX(pool_destroy)
#define pool_clear		_TPOOL_PREFIX(pool_clear)

#define pool_elem_alloc		_TPOOL_PREFIX(pool_elem_alloc)
#define pool_elem_calloc	_TPOOL_PREFIX(pool_elem_calloc)
#define pool_elem_free		_TPOOL_PREFIX(pool_elem_free)

/* private identifiers (only for this file, undefine after) */
#define pool_alloc_chunk	_TPOOL_PREFIX(pool_alloc_chunk)
#define TPoolChunk			_TPOOL_PREFIX(TPoolChunk)
#define TPoolChunkElemFree	_TPOOL_PREFIX(TPoolChunkElemFree)

#ifndef TPOOL_CHUNK_SIZE
#define  TPOOL_CHUNK_SIZE (1 << 16)  /* 64kb */
#define _TPOOL_CHUNK_SIZE_UNDEF
#endif

#ifndef UNLIKELY
#  ifdef __GNUC__
#    define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#  else
#    define UNLIKELY(x)     (x)
#  endif
#endif

#ifdef __GNUC__
#  define MAYBE_UNUSED __attribute__((unused))
#else
#  define MAYBE_UNUSED
#endif


struct TPoolChunk {
	struct TPoolChunk *prev;
	unsigned int    size;
	unsigned int    bufsize;
	TPOOL_ALLOC_TYPE buf[0];
};

struct TPoolChunkElemFree {
	struct TPoolChunkElemFree *next;
};

struct TPOOL_STRUCT {
	/* Always keep at least one chunk (never NULL) */
	struct TPoolChunk *chunk;
	/* when NULL, allocate a new chunk */
	struct TPoolChunkElemFree *free;
};

/**
 * Number of elems to include per #TPoolChunk when no reserved size is passed,
 * or we allocate past the reserved number.
 *
 * \note Optimize number for 64kb allocs.
 */
#define _TPOOL_CHUNK_DEFAULT_NUM \
	(((1 << 16) - sizeof(struct TPoolChunk)) / sizeof(TPOOL_ALLOC_TYPE))


/** \name Internal Memory Management
 * \{ */

static struct TPoolChunk *pool_alloc_chunk(
        unsigned int tot_elems, struct TPoolChunk *chunk_prev)
{
	struct TPoolChunk *chunk = malloc(
	        sizeof(struct TPoolChunk) + (sizeof(TPOOL_ALLOC_TYPE) * tot_elems));
	chunk->prev = chunk_prev;
	chunk->bufsize = tot_elems;
	chunk->size = 0;
	return chunk;
}

static TPOOL_ALLOC_TYPE *pool_elem_alloc(struct TPOOL_STRUCT *pool)
{
	TPOOL_ALLOC_TYPE *elem;

	if (pool->free) {
		elem = (TPOOL_ALLOC_TYPE *)pool->free;
		pool->free = pool->free->next;
	}
	else {
		struct TPoolChunk *chunk = pool->chunk;
		if (UNLIKELY(chunk->size == chunk->bufsize)) {
			chunk = pool->chunk = pool_alloc_chunk(_TPOOL_CHUNK_DEFAULT_NUM, chunk);
		}
		elem = &chunk->buf[chunk->size++];
	}

	return elem;
}

MAYBE_UNUSED
static TPOOL_ALLOC_TYPE *pool_elem_calloc(struct TPOOL_STRUCT *pool)
{
	TPOOL_ALLOC_TYPE *elem = pool_elem_alloc(pool);
	memset(elem, 0, sizeof(*elem));
	return elem;
}

static void pool_elem_free(struct TPOOL_STRUCT *pool, TPOOL_ALLOC_TYPE *elem)
{
	struct TPoolChunkElemFree *elem_free = (struct TPoolChunkElemFree *)elem;
	elem_free->next = pool->free;
	pool->free = elem_free;
}

static void pool_create(struct TPOOL_STRUCT *pool, unsigned int tot_reserve)
{
	pool->chunk = pool_alloc_chunk((tot_reserve > 1) ? tot_reserve : _TPOOL_CHUNK_DEFAULT_NUM, NULL);
	pool->free = NULL;
}

MAYBE_UNUSED
static void pool_clear(struct TPOOL_STRUCT *pool)
{
	/* Remove all except the last chunk */
	while (pool->chunk->prev) {
		struct TPoolChunk *chunk_prev = pool->chunk->prev;
		free(pool->chunk);
		pool->chunk = chunk_prev;
	}
	pool->chunk->size = 0;
	pool->free = NULL;
}

static void pool_destroy(struct TPOOL_STRUCT *pool)
{
	struct TPoolChunk *chunk = pool->chunk;
	do {
		struct TPoolChunk *chunk_prev;
		chunk_prev = chunk->prev;
		free(chunk);
		chunk = chunk_prev;
	} while (chunk);

	pool->chunk = NULL;
	pool->free = NULL;
}

/** \} */

#undef _TPOOL_CHUNK_DEFAULT_NUM
#undef _CONCAT_AUX
#undef _CONCAT
#undef _TPOOL_PREFIX

#undef TPoolChunk
#undef TPoolChunkElemFree

#ifdef _TPOOL_CHUNK_SIZE_UNDEF
#  undef  TPOOL_CHUNK_SIZE
#  undef _TPOOL_CHUNK_SIZE_UNDEF
#endif
