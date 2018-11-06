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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/BLI_heap_simple.c
 *  \ingroup bli
 *
 * A min-heap / priority queue ADT.
 *
 * Simplified version of the heap that only supports insertion and removal from top.
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_heap_simple.h"
#include "BLI_strict_flags.h"

#define HEAP_PARENT(i) (((i) - 1) >> 1)

/* -------------------------------------------------------------------- */
/** \name FastHeap Internal Structs
 * \{ */

typedef struct FastHeapNode {
	float value;
	void *ptr;
} FastHeapNode;

struct FastHeap {
	uint size;
	uint bufsize;
	FastHeapNode *tree;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name FastHeap Internal Functions
 * \{ */

static void fastheap_down(FastHeap *heap, uint start_i, const FastHeapNode *init)
{
#if 1
	/* The compiler isn't smart enough to realize that all computations
	 * using index here can be modified to work with byte offset. */
	uint8_t * const tree_buf = (uint8_t *)heap->tree;

#define OFFSET(i) (i * (uint)sizeof(FastHeapNode))
#define NODE(offset) (*(FastHeapNode*)(tree_buf + (offset)))
#else
	FastHeapNode *const tree = heap->tree;

#define OFFSET(i) (i)
#define NODE(i) tree[i]
#endif

#define HEAP_LEFT_OFFSET(i) (((i) << 1) + OFFSET(1))

	const uint size = OFFSET(heap->size);

	/* Pull the active node values into locals. This allows spilling
	 * the data from registers instead of literally swapping nodes. */
	float active_val = init->value;
	void *active_ptr = init->ptr;

	/* Prepare the first iteration and spill value. */
	uint i = OFFSET(start_i);

	NODE(i).value = active_val;

	for (;;) {
		const uint l = HEAP_LEFT_OFFSET(i);
		const uint r = l + OFFSET(1); /* right */

		/* Find the child with the smallest value. */
		uint smallest = i;

		if (LIKELY(l < size) && NODE(l).value < active_val) {
			smallest = l;
		}
		if (LIKELY(r < size) && NODE(r).value < NODE(smallest).value) {
			smallest = r;
		}

		if (UNLIKELY(smallest == i)) {
			break;
		}

		/* Move the smallest child into the current node.
		 * Skip padding: for some reason that makes it faster here. */
		NODE(i).value = NODE(smallest).value;
		NODE(i).ptr = NODE(smallest).ptr;

		/* Proceed to next iteration and spill value. */
		i = smallest;
		NODE(i).value = active_val;
	}

	/* Spill the pointer into the final position of the node. */
	NODE(i).ptr = active_ptr;

#undef NODE
#undef OFFSET
#undef HEAP_LEFT_OFFSET
}

static void fastheap_up(FastHeap *heap, uint i, float active_val, void *active_ptr)
{
	FastHeapNode *const tree = heap->tree;

	while (LIKELY(i > 0)) {
		const uint p = HEAP_PARENT(i);

		if (active_val >= tree[p].value) {
			break;
		}

		tree[i] = tree[p];
		i = p;
	}

	tree[i].value = active_val;
	tree[i].ptr = active_ptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public FastHeap API
 * \{ */

/**
 * Creates a new fast heap, which only supports insertion and removal from top.
 *
 * \note Use when the size of the heap is known in advance.
 */
FastHeap *BLI_fastheap_new_ex(uint tot_reserve)
{
	FastHeap *heap = MEM_mallocN(sizeof(FastHeap), __func__);
	/* ensure we have at least one so we can keep doubling it */
	heap->size = 0;
	heap->bufsize = MAX2(1u, tot_reserve);
	heap->tree = MEM_mallocN(heap->bufsize * sizeof(FastHeapNode), "BLIFastHeapTree");
	return heap;
}

FastHeap *BLI_fastheap_new(void)
{
	return BLI_fastheap_new_ex(1);
}

void BLI_fastheap_free(FastHeap *heap, HeapSimpleFreeFP ptrfreefp)
{
	if (ptrfreefp) {
		for (uint i = 0; i < heap->size; i++) {
			ptrfreefp(heap->tree[i].ptr);
		}
	}

	MEM_freeN(heap->tree);
	MEM_freeN(heap);
}

void BLI_fastheap_clear(FastHeap *heap, HeapSimpleFreeFP ptrfreefp)
{
	if (ptrfreefp) {
		for (uint i = 0; i < heap->size; i++) {
			ptrfreefp(heap->tree[i].ptr);
		}
	}

	heap->size = 0;
}

/**
 * Insert heap node with a value (often a 'cost') and pointer into the heap,
 * duplicate values are allowed.
 */
void BLI_fastheap_insert(FastHeap *heap, float value, void *ptr)
{
	if (UNLIKELY(heap->size >= heap->bufsize)) {
		heap->bufsize *= 2;
		heap->tree = MEM_reallocN(heap->tree, heap->bufsize * sizeof(*heap->tree));
	}

	fastheap_up(heap, heap->size++, value, ptr);
}

bool BLI_fastheap_is_empty(const FastHeap *heap)
{
	return (heap->size == 0);
}

uint BLI_fastheap_len(const FastHeap *heap)
{
	return heap->size;
}

/**
 * Return the lowest value of the heap.
 */
float BLI_fastheap_top_value(const FastHeap *heap)
{
	BLI_assert(heap->size != 0);

	return heap->tree[0].value;
}

/**
 * Pop the top node off the heap and return it's pointer.
 */
void *BLI_fastheap_pop_min(FastHeap *heap)
{
	BLI_assert(heap->size != 0);

	void *ptr = heap->tree[0].ptr;

	if (--heap->size) {
		fastheap_down(heap, 0, &heap->tree[heap->size]);
	}

	return ptr;
}

/** \} */
