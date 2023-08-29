/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 * \brief A min-heap / priority queue ADT
 */

#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Heap;
struct HeapNode;
typedef struct Heap Heap;
typedef struct HeapNode HeapNode;

typedef void (*HeapFreeFP)(void *ptr);

/**
 * Creates a new heap. Removed nodes are recycled, so memory usage will not shrink.
 *
 * \note Use when the size of the heap is known in advance.
 */
Heap *BLI_heap_new_ex(unsigned int reserve_num) ATTR_WARN_UNUSED_RESULT;
Heap *BLI_heap_new(void) ATTR_WARN_UNUSED_RESULT;
void BLI_heap_clear(Heap *heap, HeapFreeFP ptrfreefp) ATTR_NONNULL(1);
void BLI_heap_free(Heap *heap, HeapFreeFP ptrfreefp) ATTR_NONNULL(1);
/**
 * Insert heap node with a value (often a 'cost') and pointer into the heap,
 * duplicate values are allowed.
 */
HeapNode *BLI_heap_insert(Heap *heap, float value, void *ptr) ATTR_NONNULL(1);
/**
 * Convenience function since this is a common pattern.
 */
void BLI_heap_insert_or_update(Heap *heap, HeapNode **node_p, float value, void *ptr)
    ATTR_NONNULL(1, 2);
void BLI_heap_remove(Heap *heap, HeapNode *node) ATTR_NONNULL(1, 2);
bool BLI_heap_is_empty(const Heap *heap) ATTR_NONNULL(1);
unsigned int BLI_heap_len(const Heap *heap) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Return the top node of the heap.
 * This is the node with the lowest value.
 */
HeapNode *BLI_heap_top(const Heap *heap) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Return the value of top node of the heap.
 * This is the node with the lowest value.
 */
float BLI_heap_top_value(const Heap *heap) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Pop the top node off the heap and return its pointer.
 */
void *BLI_heap_pop_min(Heap *heap) ATTR_NONNULL(1);
/**
 * Can be used to avoid #BLI_heap_remove, #BLI_heap_insert calls,
 * balancing the tree still has a performance cost,
 * but is often much less than remove/insert, difference is most noticeable with large heaps.
 */
void BLI_heap_node_value_update(Heap *heap, HeapNode *node, float value) ATTR_NONNULL(1, 2);
void BLI_heap_node_value_update_ptr(Heap *heap, HeapNode *node, float value, void *ptr)
    ATTR_NONNULL(1, 2);

/**
 * Return the value or pointer of a heap node.
 */
float BLI_heap_node_value(const HeapNode *heap) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void *BLI_heap_node_ptr(const HeapNode *heap) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Only for checking internal errors (gtest).
 */
bool BLI_heap_is_valid(const Heap *heap);

#ifdef __cplusplus
}
#endif
