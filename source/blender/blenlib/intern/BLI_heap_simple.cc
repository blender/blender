/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * A min-heap / priority queue ADT.
 *
 * Simplified version of the heap that only supports insertion and removal from top.
 *
 * See BLI_heap.c for a more full featured heap implementation.
 */

#include <algorithm>
#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_heap_simple.h"
#include "BLI_utildefines.h"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

#define HEAP_PARENT(i) (((i) - 1) >> 1)

/* -------------------------------------------------------------------- */
/** \name HeapSimple Internal Structs
 * \{ */

struct HeapSimpleNode {
  float value;
  void *ptr;
};

struct HeapSimple {
  uint size;
  uint bufsize;
  HeapSimpleNode *tree;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name HeapSimple Internal Functions
 * \{ */

static void heapsimple_down(HeapSimple *heap, uint start_i, const HeapSimpleNode *init)
{
#if 1
  /* The compiler isn't smart enough to realize that all computations
   * using index here can be modified to work with byte offset. */
  uint8_t *const tree_buf = (uint8_t *)heap->tree;

#  define OFFSET(i) (i * uint(sizeof(HeapSimpleNode)))
#  define NODE(offset) (*(HeapSimpleNode *)(tree_buf + (offset)))
#else
  HeapSimpleNode *const tree = heap->tree;

#  define OFFSET(i) (i)
#  define NODE(i) tree[i]
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

static void heapsimple_up(HeapSimple *heap, uint i, float active_val, void *active_ptr)
{
  HeapSimpleNode *const tree = heap->tree;

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
/** \name Public HeapSimple API
 * \{ */

HeapSimple *BLI_heapsimple_new_ex(uint reserve_num)
{
  HeapSimple *heap = MEM_callocN<HeapSimple>(__func__);
  /* ensure we have at least one so we can keep doubling it */
  heap->size = 0;
  heap->bufsize = std::max(1u, reserve_num);
  heap->tree = MEM_calloc_arrayN<HeapSimpleNode>(heap->bufsize, "BLIHeapSimpleTree");
  return heap;
}

HeapSimple *BLI_heapsimple_new()
{
  return BLI_heapsimple_new_ex(1);
}

void BLI_heapsimple_free(HeapSimple *heap, HeapSimpleFreeFP ptrfreefp)
{
  if (ptrfreefp) {
    for (uint i = 0; i < heap->size; i++) {
      ptrfreefp(heap->tree[i].ptr);
    }
  }

  MEM_freeN(heap->tree);
  MEM_freeN(heap);
}

void BLI_heapsimple_clear(HeapSimple *heap, HeapSimpleFreeFP ptrfreefp)
{
  if (ptrfreefp) {
    for (uint i = 0; i < heap->size; i++) {
      ptrfreefp(heap->tree[i].ptr);
    }
  }

  heap->size = 0;
}

void BLI_heapsimple_insert(HeapSimple *heap, float value, void *ptr)
{
  if (UNLIKELY(heap->size >= heap->bufsize)) {
    heap->bufsize *= 2;
    heap->tree = static_cast<HeapSimpleNode *>(
        MEM_reallocN(heap->tree, heap->bufsize * sizeof(*heap->tree)));
  }

  heapsimple_up(heap, heap->size++, value, ptr);
}

bool BLI_heapsimple_is_empty(const HeapSimple *heap)
{
  return (heap->size == 0);
}

uint BLI_heapsimple_len(const HeapSimple *heap)
{
  return heap->size;
}

float BLI_heapsimple_top_value(const HeapSimple *heap)
{
  BLI_assert(heap->size != 0);

  return heap->tree[0].value;
}

void *BLI_heapsimple_pop_min(HeapSimple *heap)
{
  BLI_assert(heap->size != 0);

  void *ptr = heap->tree[0].ptr;

  if (--heap->size) {
    heapsimple_down(heap, 0, &heap->tree[heap->size]);
  }

  return ptr;
}

/** \} */
