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

/** \file generic_heap.c
 *  \ingroup curve_fit
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "generic_heap.h"

/* swap with a temp value */
#define SWAP_TVAL(tval, a, b)  {  \
	(tval) = (a);                 \
	(a) = (b);                    \
	(b) = (tval);                 \
} (void)0

#ifdef __GNUC__
#  define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#  define UNLIKELY(x)     (x)
#endif


/***/

struct HeapNode {
	void        *ptr;
	double       value;
	unsigned int index;
};

/* heap_* pool allocator */
#define TPOOL_IMPL_PREFIX  heap
#define TPOOL_ALLOC_TYPE   HeapNode
#define TPOOL_STRUCT       HeapMemPool
#include "generic_alloc_impl.h"
#undef TPOOL_IMPL_PREFIX
#undef TPOOL_ALLOC_TYPE
#undef TPOOL_STRUCT

struct Heap {
	unsigned int size;
	unsigned int bufsize;
	HeapNode **tree;

	struct HeapMemPool pool;
};

/** \name Internal Functions
 * \{ */

#define HEAP_PARENT(i) (((i) - 1) >> 1)
#define HEAP_LEFT(i)   (((i) << 1) + 1)
#define HEAP_RIGHT(i)  (((i) << 1) + 2)
#define HEAP_COMPARE(a, b) ((a)->value < (b)->value)

#if 0  /* UNUSED */
#define HEAP_EQUALS(a, b) ((a)->value == (b)->value)
#endif

static void heap_swap(Heap *heap, const unsigned int i, const unsigned int j)
{

#if 0
	SWAP(unsigned int,  heap->tree[i]->index, heap->tree[j]->index);
	SWAP(HeapNode *,    heap->tree[i],        heap->tree[j]);
#else
	HeapNode **tree = heap->tree;
	union {
		unsigned int  index;
		HeapNode     *node;
	} tmp;
	SWAP_TVAL(tmp.index, tree[i]->index, tree[j]->index);
	SWAP_TVAL(tmp.node,  tree[i],        tree[j]);
#endif
}

static void heap_down(Heap *heap, unsigned int i)
{
	/* size won't change in the loop */
	const unsigned int size = heap->size;

	while (1) {
		const unsigned int l = HEAP_LEFT(i);
		const unsigned int r = HEAP_RIGHT(i);
		unsigned int smallest;

		smallest = ((l < size) && HEAP_COMPARE(heap->tree[l], heap->tree[i])) ? l : i;

		if ((r < size) && HEAP_COMPARE(heap->tree[r], heap->tree[smallest])) {
			smallest = r;
		}

		if (smallest == i) {
			break;
		}

		heap_swap(heap, i, smallest);
		i = smallest;
	}
}

static void heap_up(Heap *heap, unsigned int i)
{
	while (i > 0) {
		const unsigned int p = HEAP_PARENT(i);

		if (HEAP_COMPARE(heap->tree[p], heap->tree[i])) {
			break;
		}
		heap_swap(heap, p, i);
		i = p;
	}
}

/** \} */


/** \name Public Heap API
 * \{ */

/* use when the size of the heap is known in advance */
Heap *HEAP_new(unsigned int tot_reserve)
{
	Heap *heap = malloc(sizeof(Heap));
	/* ensure we have at least one so we can keep doubling it */
	heap->size = 0;
	heap->bufsize = tot_reserve ? tot_reserve : 1;
	heap->tree = malloc(heap->bufsize * sizeof(HeapNode *));

	heap_pool_create(&heap->pool, tot_reserve);

	return heap;
}

void HEAP_free(Heap *heap, HeapFreeFP ptrfreefp)
{
	if (ptrfreefp) {
		unsigned int i;

		for (i = 0; i < heap->size; i++) {
			ptrfreefp(heap->tree[i]->ptr);
		}
	}

	heap_pool_destroy(&heap->pool);

	free(heap->tree);
	free(heap);
}

void HEAP_clear(Heap *heap, HeapFreeFP ptrfreefp)
{
	if (ptrfreefp) {
		unsigned int i;

		for (i = 0; i < heap->size; i++) {
			ptrfreefp(heap->tree[i]->ptr);
		}
	}
	heap->size = 0;

	heap_pool_clear(&heap->pool);
}

HeapNode *HEAP_insert(Heap *heap, double value, void *ptr)
{
	HeapNode *node;

	if (UNLIKELY(heap->size >= heap->bufsize)) {
		heap->bufsize *= 2;
		heap->tree = realloc(heap->tree, heap->bufsize * sizeof(*heap->tree));
	}

	node = heap_pool_elem_alloc(&heap->pool);

	node->ptr = ptr;
	node->value = value;
	node->index = heap->size;

	heap->tree[node->index] = node;

	heap->size++;

	heap_up(heap, node->index);

	return node;
}

bool HEAP_is_empty(Heap *heap)
{
	return (heap->size == 0);
}

unsigned int HEAP_size(Heap *heap)
{
	return heap->size;
}

HeapNode *HEAP_top(Heap *heap)
{
	return heap->tree[0];
}

double HEAP_top_value(Heap *heap)
{
	return heap->tree[0]->value;
}

void *HEAP_popmin(Heap *heap)
{
	void *ptr = heap->tree[0]->ptr;

	assert(heap->size != 0);

	heap_pool_elem_free(&heap->pool, heap->tree[0]);

	if (--heap->size) {
		heap_swap(heap, 0, heap->size);
		heap_down(heap, 0);
	}

	return ptr;
}

void HEAP_remove(Heap *heap, HeapNode *node)
{
	unsigned int i = node->index;

	assert(heap->size != 0);

	while (i > 0) {
		unsigned int p = HEAP_PARENT(i);

		heap_swap(heap, p, i);
		i = p;
	}

	HEAP_popmin(heap);
}

double HEAP_node_value(HeapNode *node)
{
	return node->value;
}

void *HEAP_node_ptr(HeapNode *node)
{
	return node->ptr;
}
