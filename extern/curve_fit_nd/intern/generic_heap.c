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

typedef unsigned int uint;

/***/

struct HeapNode {
	void   *ptr;
	double  value;
	uint    index;
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
	uint size;
	uint bufsize;
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

static void heap_swap(Heap *heap, const uint i, const uint j)
{

#if 0
	SWAP(uint,       heap->tree[i]->index, heap->tree[j]->index);
	SWAP(HeapNode *, heap->tree[i],        heap->tree[j]);
#else
	HeapNode **tree = heap->tree;
	union {
		uint      index;
		HeapNode *node;
	} tmp;
	SWAP_TVAL(tmp.index, tree[i]->index, tree[j]->index);
	SWAP_TVAL(tmp.node,  tree[i],        tree[j]);
#endif
}

static void heap_down(Heap *heap, uint i)
{
	/* size won't change in the loop */
	const uint size = heap->size;

	while (1) {
		const uint l = HEAP_LEFT(i);
		const uint r = HEAP_RIGHT(i);
		uint smallest;

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

static void heap_up(Heap *heap, uint i)
{
	while (i > 0) {
		const uint p = HEAP_PARENT(i);

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
Heap *HEAP_new(uint tot_reserve)
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
		uint i;

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
		uint i;

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

void HEAP_insert_or_update(Heap *heap, HeapNode **node_p, double value, void *ptr)
{
	if (*node_p == NULL) {
		*node_p = HEAP_insert(heap, value, ptr);
	}
	else {
		HEAP_node_value_update_ptr(heap, *node_p, value, ptr);
	}
}

bool HEAP_is_empty(const Heap *heap)
{
	return (heap->size == 0);
}

uint HEAP_size(const Heap *heap)
{
	return heap->size;
}

HeapNode *HEAP_top(Heap *heap)
{
	return heap->tree[0];
}

double HEAP_top_value(const Heap *heap)
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
	uint i = node->index;

	assert(heap->size != 0);

	while (i > 0) {
		uint p = HEAP_PARENT(i);

		heap_swap(heap, p, i);
		i = p;
	}

	HEAP_popmin(heap);
}

void HEAP_node_value_update(Heap *heap, HeapNode *node, double value)
{
	assert(heap->size != 0);
	if (node->value == value) {
		return;
	}
	node->value = value;
	/* Can be called in either order, makes no difference. */
	heap_up(heap, node->index);
	heap_down(heap, node->index);
}

void HEAP_node_value_update_ptr(Heap *heap, HeapNode *node, double value, void *ptr)
{
	node->ptr = ptr;
	HEAP_node_value_update(heap, node, value);
}

double HEAP_node_value(const HeapNode *node)
{
	return node->value;
}

void *HEAP_node_ptr(HeapNode *node)
{
	return node->ptr;
}

/** \} */
