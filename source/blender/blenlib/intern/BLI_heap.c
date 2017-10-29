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
 * Contributor(s): Brecht Van Lommel
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/BLI_heap.c
 *  \ingroup bli
 *
 * A heap / priority queue ADT.
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_heap.h"
#include "BLI_strict_flags.h"

/***/

struct HeapNode {
	void        *ptr;
	float        value;
	unsigned int index;
};

struct HeapNode_Chunk {
	struct HeapNode_Chunk *prev;
	unsigned int    size;
	unsigned int    bufsize;
	struct HeapNode buf[0];
};

/**
 * Number of nodes to include per #HeapNode_Chunk when no reserved size is passed,
 * or we allocate past the reserved number.
 *
 * \note Optimize number for 64kb allocs.
 * \note keep type in sync with tot_nodes in heap_node_alloc_chunk.
 */
#define HEAP_CHUNK_DEFAULT_NUM \
	((unsigned int)((MEM_SIZE_OPTIMAL((1 << 16) - sizeof(struct HeapNode_Chunk))) / sizeof(HeapNode)))

struct Heap {
	unsigned int size;
	unsigned int bufsize;
	HeapNode **tree;

	struct {
		/* Always keep at least one chunk (never NULL) */
		struct HeapNode_Chunk *chunk;
		/* when NULL, allocate a new chunk */
		HeapNode *free;
	} nodes;
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

BLI_INLINE void heap_swap(Heap *heap, const unsigned int i, const unsigned int j)
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


/** \name Internal Memory Management
 * \{ */

static struct HeapNode_Chunk *heap_node_alloc_chunk(
        unsigned int tot_nodes, struct HeapNode_Chunk *chunk_prev)
{
	struct HeapNode_Chunk *chunk = MEM_mallocN(
	        sizeof(struct HeapNode_Chunk) + (sizeof(HeapNode) * tot_nodes), __func__);
	chunk->prev = chunk_prev;
	chunk->bufsize = tot_nodes;
	chunk->size = 0;
	return chunk;
}

static struct HeapNode *heap_node_alloc(Heap *heap)
{
	HeapNode *node;

	if (heap->nodes.free) {
		node = heap->nodes.free;
		heap->nodes.free = heap->nodes.free->ptr;
	}
	else {
		struct HeapNode_Chunk *chunk = heap->nodes.chunk;
		if (UNLIKELY(chunk->size == chunk->bufsize)) {
			chunk = heap->nodes.chunk = heap_node_alloc_chunk(HEAP_CHUNK_DEFAULT_NUM, chunk);
		}
		node = &chunk->buf[chunk->size++];
	}

	return node;
}

static void heap_node_free(Heap *heap, HeapNode *node)
{
	node->ptr = heap->nodes.free;
	heap->nodes.free = node;
}

/** \} */


/** \name Public Heap API
 * \{ */

/* use when the size of the heap is known in advance */
Heap *BLI_heap_new_ex(unsigned int tot_reserve)
{
	Heap *heap = MEM_mallocN(sizeof(Heap), __func__);
	/* ensure we have at least one so we can keep doubling it */
	heap->size = 0;
	heap->bufsize = MAX2(1u, tot_reserve);
	heap->tree = MEM_mallocN(heap->bufsize * sizeof(HeapNode *), "BLIHeapTree");

	heap->nodes.chunk = heap_node_alloc_chunk((tot_reserve > 1) ? tot_reserve : HEAP_CHUNK_DEFAULT_NUM, NULL);
	heap->nodes.free = NULL;

	return heap;
}

Heap *BLI_heap_new(void)
{
	return BLI_heap_new_ex(1);
}

void BLI_heap_free(Heap *heap, HeapFreeFP ptrfreefp)
{
	if (ptrfreefp) {
		unsigned int i;

		for (i = 0; i < heap->size; i++) {
			ptrfreefp(heap->tree[i]->ptr);
		}
	}

	struct HeapNode_Chunk *chunk = heap->nodes.chunk;
	do {
		struct HeapNode_Chunk *chunk_prev;
		chunk_prev = chunk->prev;
		MEM_freeN(chunk);
		chunk = chunk_prev;
	} while (chunk);

	MEM_freeN(heap->tree);
	MEM_freeN(heap);
}

void BLI_heap_clear(Heap *heap, HeapFreeFP ptrfreefp)
{
	if (ptrfreefp) {
		unsigned int i;

		for (i = 0; i < heap->size; i++) {
			ptrfreefp(heap->tree[i]->ptr);
		}
	}
	heap->size = 0;

	/* Remove all except the last chunk */
	while (heap->nodes.chunk->prev) {
		struct HeapNode_Chunk *chunk_prev = heap->nodes.chunk->prev;
		MEM_freeN(heap->nodes.chunk);
		heap->nodes.chunk = chunk_prev;
	}
	heap->nodes.chunk->size = 0;
	heap->nodes.free = NULL;
}

HeapNode *BLI_heap_insert(Heap *heap, float value, void *ptr)
{
	HeapNode *node;

	if (UNLIKELY(heap->size >= heap->bufsize)) {
		heap->bufsize *= 2;
		heap->tree = MEM_reallocN(heap->tree, heap->bufsize * sizeof(*heap->tree));
	}

	node = heap_node_alloc(heap);

	node->ptr = ptr;
	node->value = value;
	node->index = heap->size;

	heap->tree[node->index] = node;

	heap->size++;

	heap_up(heap, node->index);

	return node;
}

/**
 * Convenience function since this is a common pattern.
 */
void BLI_heap_insert_or_update(Heap *heap, HeapNode **node_p, float value, void *ptr)
{
	if (*node_p == NULL) {
		*node_p = BLI_heap_insert(heap, value, ptr);
	}
	else {
		BLI_heap_node_value_update_ptr(heap, *node_p, value, ptr);
	}
}


bool BLI_heap_is_empty(Heap *heap)
{
	return (heap->size == 0);
}

unsigned int BLI_heap_size(Heap *heap)
{
	return heap->size;
}

HeapNode *BLI_heap_top(Heap *heap)
{
	return heap->tree[0];
}

void *BLI_heap_popmin(Heap *heap)
{
	void *ptr = heap->tree[0]->ptr;

	BLI_assert(heap->size != 0);

	heap_node_free(heap, heap->tree[0]);

	if (--heap->size) {
		heap_swap(heap, 0, heap->size);
		heap_down(heap, 0);
	}

	return ptr;
}

void BLI_heap_remove(Heap *heap, HeapNode *node)
{
	unsigned int i = node->index;

	BLI_assert(heap->size != 0);

	while (i > 0) {
		unsigned int p = HEAP_PARENT(i);

		heap_swap(heap, p, i);
		i = p;
	}

	BLI_heap_popmin(heap);
}

/**
 * Can be used to avoid #BLI_heap_remove, #BLI_heap_insert calls,
 * balancing the tree still has a performance cost,
 * but is often much less than remove/insert, difference is most noticable with large heaps.
 */
void BLI_heap_node_value_update(Heap *heap, HeapNode *node, float value)
{
	if (value == node->value) {
		return;
	}
	node->value = value;
	/* Can be called in either order, makes no difference. */
	heap_up(heap, node->index);
	heap_down(heap, node->index);
}

void BLI_heap_node_value_update_ptr(Heap *heap, HeapNode *node, float value, void *ptr)
{
	node->ptr = ptr;
	BLI_heap_node_value_update(heap, node, value);
}

float BLI_heap_node_value(HeapNode *node)
{
	return node->value;
}

void *BLI_heap_node_ptr(HeapNode *node)
{
	return node->ptr;
}

/** \} */
