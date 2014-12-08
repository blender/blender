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
#include "BLI_memarena.h"
#include "BLI_heap.h"
#include "BLI_strict_flags.h"

/***/

struct HeapNode {
	void        *ptr;
	float        value;
	unsigned int index;
};

struct Heap {
	unsigned int size;
	unsigned int bufsize;
	MemArena *arena;
	HeapNode *freenodes;
	HeapNode **tree;
};

/* internal functions */

#define HEAP_PARENT(i) ((i - 1) >> 1)
#define HEAP_LEFT(i)   ((i << 1) + 1)
#define HEAP_RIGHT(i)  ((i << 1) + 2)
#define HEAP_COMPARE(a, b) (a->value < b->value)

#if 0  /* UNUSED */
#define HEAP_EQUALS(a, b) (a->value == b->value)
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

		if ((r < size) && HEAP_COMPARE(heap->tree[r], heap->tree[smallest]))
			smallest = r;

		if (smallest == i)
			break;

		heap_swap(heap, i, smallest);
		i = smallest;
	}
}

static void heap_up(Heap *heap, unsigned int i)
{
	while (i > 0) {
		const unsigned int p = HEAP_PARENT(i);

		if (HEAP_COMPARE(heap->tree[p], heap->tree[i]))
			break;

		heap_swap(heap, p, i);
		i = p;
	}
}


/***/

/* use when the size of the heap is known in advance */
Heap *BLI_heap_new_ex(unsigned int tot_reserve)
{
	Heap *heap = (Heap *)MEM_callocN(sizeof(Heap), __func__);
	/* ensure we have at least one so we can keep doubling it */
	heap->bufsize = MAX2(1u, tot_reserve);
	heap->tree = (HeapNode **)MEM_mallocN(heap->bufsize * sizeof(HeapNode *), "BLIHeapTree");
	heap->arena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 16), "heap arena");

	return heap;
}

Heap *BLI_heap_new(void)
{
	return BLI_heap_new_ex(1);
}

void BLI_heap_free(Heap *heap, HeapFreeFP ptrfreefp)
{
	unsigned int i;

	if (ptrfreefp) {
		for (i = 0; i < heap->size; i++) {
			ptrfreefp(heap->tree[i]->ptr);
		}
	}

	MEM_freeN(heap->tree);
	BLI_memarena_free(heap->arena);
	MEM_freeN(heap);
}

HeapNode *BLI_heap_insert(Heap *heap, float value, void *ptr)
{
	HeapNode *node;

	if (UNLIKELY(heap->size >= heap->bufsize)) {
		heap->bufsize *= 2;
		heap->tree = MEM_reallocN(heap->tree, heap->bufsize * sizeof(*heap->tree));
	}

	if (heap->freenodes) {
		node = heap->freenodes;
		heap->freenodes = heap->freenodes->ptr;
	}
	else {
		node = (HeapNode *)BLI_memarena_alloc(heap->arena, sizeof(*node));
	}

	node->value = value;
	node->ptr = ptr;
	node->index = heap->size;

	heap->tree[node->index] = node;

	heap->size++;

	heap_up(heap, node->index);

	return node;
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

	heap->tree[0]->ptr = heap->freenodes;
	heap->freenodes = heap->tree[0];

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

float BLI_heap_node_value(HeapNode *node)
{
	return node->value;
}

void *BLI_heap_node_ptr(HeapNode *node)
{
	return node->ptr;
}

