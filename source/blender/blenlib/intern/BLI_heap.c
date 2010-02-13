/**
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: none of this file.
 *
 * Contributor(s): Brecht Van Lommel
 *
 * ***** END GPL LICENSE BLOCK *****
 * A heap / priority queue ADT.
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "BLI_memarena.h"
#include "BLI_heap.h"

/***/

struct HeapNode {
	void *ptr;
	float value;
	int index;
};

struct Heap {
	unsigned int size;
	unsigned int bufsize;
	MemArena *arena;
	HeapNode *freenodes;
	HeapNode *nodes;
	HeapNode **tree;
};

#define SWAP(type, a, b) \
	{ type sw_ap; sw_ap=(a); (a)=(b); (b)=sw_ap; }
#define HEAP_PARENT(i) ((i-1)>>1)
#define HEAP_LEFT(i)   ((i<<1)+1)
#define HEAP_RIGHT(i)  ((i<<1)+2)
#define HEAP_COMPARE(a, b) (a->value < b->value)
#define HEAP_EQUALS(a, b) (a->value == b->value)
#define HEAP_SWAP(heap, i, j) \
	{ SWAP(int, heap->tree[i]->index, heap->tree[j]->index); \
	  SWAP(HeapNode*, heap->tree[i], heap->tree[j]);  }

/***/

Heap *BLI_heap_new()
{
	Heap *heap = (Heap*)MEM_callocN(sizeof(Heap), "BLIHeap");
	heap->bufsize = 1;
	heap->tree = (HeapNode**)MEM_mallocN(sizeof(HeapNode*), "BLIHeapTree");
	heap->arena = BLI_memarena_new(1<<16);

	return heap;
}

void BLI_heap_free(Heap *heap, HeapFreeFP ptrfreefp)
{
	int i;

	if (ptrfreefp)
		for (i = 0; i < heap->size; i++)
			ptrfreefp(heap->tree[i]->ptr);
	
	MEM_freeN(heap->tree);
	BLI_memarena_free(heap->arena);
	MEM_freeN(heap);
}

static void BLI_heap_down(Heap *heap, int i)
{
	while (1) {
		int size = heap->size, smallest;
		int l = HEAP_LEFT(i);
		int r = HEAP_RIGHT(i);

		smallest = ((l < size) && HEAP_COMPARE(heap->tree[l], heap->tree[i]))? l: i;

		if ((r < size) && HEAP_COMPARE(heap->tree[r], heap->tree[smallest]))
			smallest = r;
		
		if (smallest == i)
			break;

		HEAP_SWAP(heap, i, smallest);
		i = smallest;
	}
}

static void BLI_heap_up(Heap *heap, int i)
{
	while (i > 0) {
		int p = HEAP_PARENT(i);

		if (HEAP_COMPARE(heap->tree[p], heap->tree[i]))
			break;

		HEAP_SWAP(heap, p, i);
		i = p;
	}
}

HeapNode *BLI_heap_insert(Heap *heap, float value, void *ptr)
{
	HeapNode *node;

	if ((heap->size + 1) > heap->bufsize) {
		int newsize = heap->bufsize*2;
		HeapNode **newtree;

		newtree = (HeapNode**)MEM_mallocN(newsize*sizeof(*newtree), "BLIHeapTree");
		memcpy(newtree, heap->tree, sizeof(HeapNode*)*heap->size);
		MEM_freeN(heap->tree);

		heap->tree = newtree;
		heap->bufsize = newsize;
	}

	if (heap->freenodes) {
		node = heap->freenodes;
		heap->freenodes = (HeapNode*)(((HeapNode*)heap->freenodes)->ptr);
	}
	else
		node = (HeapNode*)BLI_memarena_alloc(heap->arena, sizeof *node);

	node->value = value;
	node->ptr = ptr;
	node->index = heap->size;

	heap->tree[node->index] = node;

	heap->size++;

	BLI_heap_up(heap, heap->size-1);

	return node;
}

int BLI_heap_empty(Heap *heap)
{
	return (heap->size == 0);
}

int BLI_heap_size(Heap *heap)
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

	heap->tree[0]->ptr = heap->freenodes;
	heap->freenodes = heap->tree[0];

	if (heap->size == 1)
		heap->size--;
	else {
		HEAP_SWAP(heap, 0, heap->size-1);
		heap->size--;

		BLI_heap_down(heap, 0);
	}

	return ptr;
}

void BLI_heap_remove(Heap *heap, HeapNode *node)
{
	int i = node->index;

	while (i > 0) {
		int p = HEAP_PARENT(i);

		HEAP_SWAP(heap, p, i);
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

