/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * A min-heap / priority queue ADT.
 */

#include <algorithm>
#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_heap.h"
#include "BLI_utildefines.h"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

/***/

struct HeapNode {
  float value;
  uint index;
  void *ptr;
};

struct HeapNode_Chunk {
  HeapNode_Chunk *prev;
  uint size;
  uint bufsize;
  HeapNode buf[0];
};

/**
 * Number of nodes to include per #HeapNode_Chunk when no reserved size is passed,
 * or we allocate past the reserved number.
 *
 * \note Optimize number for 64kb allocations.
 * \note keep type in sync with nodes_num in heap_node_alloc_chunk.
 */
#define HEAP_CHUNK_DEFAULT_NUM \
  uint(MEM_SIZE_OPTIMAL((1 << 16) - sizeof(HeapNode_Chunk)) / sizeof(HeapNode))

struct Heap {
  uint size;
  uint bufsize;
  HeapNode **tree;

  struct {
    /* Always keep at least one chunk (never nullptr) */
    HeapNode_Chunk *chunk;
    /* when nullptr, allocate a new chunk */
    HeapNode *free;
  } nodes;
};

/* -------------------------------------------------------------------- */
/** \name Internal Functions
 * \{ */

#define HEAP_PARENT(i) (((i) - 1) >> 1)
#define HEAP_LEFT(i) (((i) << 1) + 1)
#define HEAP_RIGHT(i) (((i) << 1) + 2)
#define HEAP_COMPARE(a, b) ((a)->value < (b)->value)

#if 0 /* UNUSED */
#  define HEAP_EQUALS(a, b) ((a)->value == (b)->value)
#endif

BLI_INLINE void heap_swap(Heap *heap, const uint i, const uint j)
{
  HeapNode **tree = heap->tree;
  HeapNode *pi = tree[i], *pj = tree[j];
  pi->index = j;
  tree[j] = pi;
  pj->index = i;
  tree[i] = pj;
}

static void heap_down(Heap *heap, uint i)
{
  /* size won't change in the loop */
  HeapNode **const tree = heap->tree;
  const uint size = heap->size;

  while (true) {
    const uint l = HEAP_LEFT(i);
    const uint r = HEAP_RIGHT(i);
    uint smallest = i;

    if (LIKELY(l < size) && HEAP_COMPARE(tree[l], tree[smallest])) {
      smallest = l;
    }
    if (LIKELY(r < size) && HEAP_COMPARE(tree[r], tree[smallest])) {
      smallest = r;
    }

    if (UNLIKELY(smallest == i)) {
      break;
    }

    heap_swap(heap, i, smallest);
    i = smallest;
  }
}

static void heap_up(Heap *heap, uint i)
{
  HeapNode **const tree = heap->tree;

  while (LIKELY(i > 0)) {
    const uint p = HEAP_PARENT(i);

    if (HEAP_COMPARE(tree[p], tree[i])) {
      break;
    }
    heap_swap(heap, p, i);
    i = p;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Memory Management
 * \{ */

static HeapNode_Chunk *heap_node_alloc_chunk(uint nodes_num, HeapNode_Chunk *chunk_prev)
{
  HeapNode_Chunk *chunk = static_cast<HeapNode_Chunk *>(
      MEM_mallocN(sizeof(HeapNode_Chunk) + (sizeof(HeapNode) * nodes_num), __func__));
  chunk->prev = chunk_prev;
  chunk->bufsize = nodes_num;
  chunk->size = 0;
  return chunk;
}

static HeapNode *heap_node_alloc(Heap *heap)
{
  HeapNode *node;

  if (heap->nodes.free) {
    node = heap->nodes.free;
    heap->nodes.free = static_cast<HeapNode *>(heap->nodes.free->ptr);
  }
  else {
    HeapNode_Chunk *chunk = heap->nodes.chunk;
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

/* -------------------------------------------------------------------- */
/** \name Public Heap API
 * \{ */

Heap *BLI_heap_new_ex(uint reserve_num)
{
  Heap *heap = MEM_callocN<Heap>(__func__);
  /* ensure we have at least one so we can keep doubling it */
  heap->size = 0;
  heap->bufsize = std::max(1u, reserve_num);
  heap->tree = MEM_calloc_arrayN<HeapNode *>(heap->bufsize, "BLIHeapTree");

  heap->nodes.chunk = heap_node_alloc_chunk(
      (reserve_num > 1) ? reserve_num : HEAP_CHUNK_DEFAULT_NUM, nullptr);
  heap->nodes.free = nullptr;

  return heap;
}

Heap *BLI_heap_new()
{
  return BLI_heap_new_ex(1);
}

void BLI_heap_free(Heap *heap, HeapFreeFP ptrfreefp)
{
  if (ptrfreefp) {
    uint i;

    for (i = 0; i < heap->size; i++) {
      ptrfreefp(heap->tree[i]->ptr);
    }
  }

  HeapNode_Chunk *chunk = heap->nodes.chunk;
  do {
    HeapNode_Chunk *chunk_prev;
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
    uint i;

    for (i = 0; i < heap->size; i++) {
      ptrfreefp(heap->tree[i]->ptr);
    }
  }
  heap->size = 0;

  /* Remove all except the last chunk */
  while (heap->nodes.chunk->prev) {
    HeapNode_Chunk *chunk_prev = heap->nodes.chunk->prev;
    MEM_freeN(heap->nodes.chunk);
    heap->nodes.chunk = chunk_prev;
  }
  heap->nodes.chunk->size = 0;
  heap->nodes.free = nullptr;
}

HeapNode *BLI_heap_insert(Heap *heap, float value, void *ptr)
{
  HeapNode *node;

  if (UNLIKELY(heap->size >= heap->bufsize)) {
    heap->bufsize *= 2;
    heap->tree = static_cast<HeapNode **>(
        MEM_reallocN(heap->tree, heap->bufsize * sizeof(*heap->tree)));
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

void BLI_heap_insert_or_update(Heap *heap, HeapNode **node_p, float value, void *ptr)
{
  if (*node_p == nullptr) {
    *node_p = BLI_heap_insert(heap, value, ptr);
  }
  else {
    BLI_heap_node_value_update_ptr(heap, *node_p, value, ptr);
  }
}

bool BLI_heap_is_empty(const Heap *heap)
{
  return (heap->size == 0);
}

uint BLI_heap_len(const Heap *heap)
{
  return heap->size;
}

HeapNode *BLI_heap_top(const Heap *heap)
{
  return heap->tree[0];
}

float BLI_heap_top_value(const Heap *heap)
{
  BLI_assert(heap->size != 0);

  return heap->tree[0]->value;
}

void *BLI_heap_pop_min(Heap *heap)
{
  BLI_assert(heap->size != 0);

  void *ptr = heap->tree[0]->ptr;

  heap_node_free(heap, heap->tree[0]);

  if (--heap->size) {
    heap_swap(heap, 0, heap->size);
    heap_down(heap, 0);
  }

  return ptr;
}

void BLI_heap_remove(Heap *heap, HeapNode *node)
{
  BLI_assert(heap->size != 0);

  uint i = node->index;

  while (i > 0) {
    uint p = HEAP_PARENT(i);
    heap_swap(heap, p, i);
    i = p;
  }

  BLI_heap_pop_min(heap);
}

void BLI_heap_node_value_update(Heap *heap, HeapNode *node, float value)
{
  if (value < node->value) {
    node->value = value;
    heap_up(heap, node->index);
  }
  else if (value > node->value) {
    node->value = value;
    heap_down(heap, node->index);
  }
}

void BLI_heap_node_value_update_ptr(Heap *heap, HeapNode *node, float value, void *ptr)
{
  node->ptr = ptr; /* only difference */
  if (value < node->value) {
    node->value = value;
    heap_up(heap, node->index);
  }
  else if (value > node->value) {
    node->value = value;
    heap_down(heap, node->index);
  }
}

float BLI_heap_node_value(const HeapNode *heap)
{
  return heap->value;
}

void *BLI_heap_node_ptr(const HeapNode *heap)
{
  return heap->ptr;
}

static bool heap_is_minheap(const Heap *heap, uint root)
{
  if (root < heap->size) {
    if (heap->tree[root]->index != root) {
      return false;
    }
    const uint l = HEAP_LEFT(root);
    if (l < heap->size) {
      if (HEAP_COMPARE(heap->tree[l], heap->tree[root]) || !heap_is_minheap(heap, l)) {
        return false;
      }
    }
    const uint r = HEAP_RIGHT(root);
    if (r < heap->size) {
      if (HEAP_COMPARE(heap->tree[r], heap->tree[root]) || !heap_is_minheap(heap, r)) {
        return false;
      }
    }
  }
  return true;
}
bool BLI_heap_is_valid(const Heap *heap)
{
  return heap_is_minheap(heap, 0);
}

/** \} */
