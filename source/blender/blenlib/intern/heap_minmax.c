#include "MEM_guardedalloc.h"

#include "BLI_heap_minmax.h"
#include "BLI_mempool.h"
#include "BLI_utildefines.h"

#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"

typedef struct MinMaxHeapNode {
  void *ptr;
  float value;
  int child1, child2, parent;
} MinMaxHeapNode;

typedef struct MinMaxHeap {
  MinMaxHeapNode *nodes;
  int totnode, reserved;
} MinMaxHeap;

static void heap_reserve(MinMaxHeap *heap, unsigned int n)
{
  if (heap->reserved >= n) {
    return;
  }

  n++;
  n += n >> 1;

  if (heap->nodes) {
    heap->nodes = MEM_recallocN(heap->nodes, sizeof(MinMaxHeapNode) * n);
  }
  else {
    heap->nodes = MEM_calloc_arrayN(n, sizeof(MinMaxHeapNode), "MinMaxHeap nodes");
  }

  heap->reserved = n;
}

/**
 * Creates a new heap. Removed nodes are recycled, so memory usage will not shrink.
 *
 * \note Use when the size of the heap is known in advance.
 */
MinMaxHeap *BLI_mm_heap_new_ex(unsigned int tot_reserve)
{
  MinMaxHeap *heap = MEM_callocN(sizeof(*heap), __func__);
  if (tot_reserve) {
    heap_reserve(heap, tot_reserve);
  }

  return heap;
}

MinMaxHeap *BLI_mm_heap_new()
{
  return BLI_mm_heap_new_ex(256);
}

void BLI_mm_heap_clear(MinMaxHeap *heap, MinMaxHeapFreeFP ptrfreefp)
{
  if (ptrfreefp) {
    for (int i = 0; i < heap->totnode; i++) {
      ptrfreefp(heap->nodes[i].ptr);
    }
  }

  MEM_SAFE_FREE(heap->nodes);
  heap->totnode = 0;
}

void BLI_mm_heap_free(MinMaxHeap *heap, MinMaxHeapFreeFP ptrfreefp)
{
  BLI_mm_heap_clear(heap, ptrfreefp);
  MEM_freeN(heap);
}

static MinMaxHeapNode *heap_make_node(MinMaxHeap *heap)
{
  heap_reserve(heap, heap->totnode + 1);

  MinMaxHeapNode *node = heap->nodes + heap->totnode;
  node->child1 = node->child2 = node->parent = -1;

  heap->totnode++;

  return node;
}

static MinMaxHeapNode *heap_descent_min2(MinMaxHeap *heap, MinMaxHeapNode *n)
{
  if (n->child1 != -1 && n->child2 != -1) {
    MinMaxHeapNode *n1 = heap->nodes + n->child1;
    MinMaxHeapNode *n2 = heap->nodes + n->child2;

    return n1->value < n2->value ? n1 : n2;
  }
  else if (n->child1 != -1) {
    return heap->nodes + n->child1;
  }
  else if (n->child2 != -1) {
    return heap->nodes + n->child2;
  }

  return n;
}

/* find node grandchild */
static MinMaxHeapNode *heap_descent_min(MinMaxHeap *heap, MinMaxHeapNode *node)
{
  if (node->child1 != -1 && node->child2 != -1) {
    MinMaxHeapNode *n1 = heap->nodes + node->child1;
    MinMaxHeapNode *n2 = heap->nodes + node->child2;

    n1 = heap_descent_min2(heap, n1);
    n2 = heap_descent_min2(heap, n2);

    return n1->value < n2->value ? n1 : n2;
  }
  else if (node->child1 != -1) {
    return heap_descent_min2(heap, heap->nodes + node->child1);
  }
  else if (node->child2 != -1) {
    return heap_descent_min2(heap, heap->nodes + node->child2);
  }

  return NULL;
}

static MinMaxHeapNode *heap_descent_max2(MinMaxHeap *heap, MinMaxHeapNode *n)
{
  if (n->child1 != -1 && n->child2 != -1) {
    MinMaxHeapNode *n1 = heap->nodes + n->child1;
    MinMaxHeapNode *n2 = heap->nodes + n->child2;

    return n1->value > n2->value ? n1 : n2;
  }
  else if (n->child1 != -1) {
    return heap->nodes + n->child1;
  }
  else if (n->child2 != -1) {
    return heap->nodes + n->child2;
  }

  return n;
}

/* find node grandchild */
static MinMaxHeapNode *heap_descent_max(MinMaxHeap *heap, MinMaxHeapNode *node)
{
  if (node->child1 != -1 && node->child2 != -1) {
    MinMaxHeapNode *n1 = heap->nodes + node->child1;
    MinMaxHeapNode *n2 = heap->nodes + node->child2;

    n1 = heap_descent_max2(heap, n1);
    n2 = heap_descent_max2(heap, n2);

    return n1->value > n2->value ? n1 : n2;
  }
  else if (node->child1 != -1) {
    return heap_descent_max2(heap, heap->nodes + node->child1);
  }
  else if (node->child2 != -1) {
    return heap_descent_max2(heap, heap->nodes + node->child2);
  }

  return NULL;
}

static MinMaxHeapNode *heap_push_down_min(MinMaxHeap *heap, MinMaxHeapNode *node)
{
  MinMaxHeapNode *ret = NULL;

  while (node->child1 >= 0 || node->child2 >= 0) {
    MinMaxHeapNode *node2 = heap_descent_min(heap, node);

    if (!node2) {
      break;
    }

    if (node2->value < node->value) {
      SWAP(float, node2->value, node->value);
      SWAP(void *, node2->ptr, node->ptr);

      if (node2->parent != node - heap->nodes) {
        MinMaxHeapNode *parent = heap->nodes + node2->parent;

        if (node2->value > parent->value) {
          SWAP(float, node2->value, parent->value);
          SWAP(void *, node2->ptr, parent->ptr);

          /* this is a bit tricky, our return node has now
             moved into the other interleaved heap side */
          if (!ret) {
            ret = parent;
          }
        }
      }

      node = node2;
    }
    else {
      break;
    }
  }

  return ret ? ret : node;
}

static MinMaxHeapNode *heap_push_down_max(MinMaxHeap *heap, MinMaxHeapNode *node)
{
  MinMaxHeapNode *ret = NULL;

  while (node->child1 >= 0 || node->child2 >= 0) {
    MinMaxHeapNode *node2 = heap_descent_max(heap, node);

    if (node2->value > node->value) {
      SWAP(float, node2->value, node->value);
      SWAP(void *, node2->ptr, node->ptr);

      if (node2->parent != node - heap->nodes) {
        MinMaxHeapNode *parent = heap->nodes + node2->parent;

        if (node2->value < parent->value) {
          SWAP(float, node2->value, parent->value);
          SWAP(void *, node2->ptr, parent->ptr);

          /* this is a bit tricky, our return node has now
             moved into the other interleaved heap side */
          if (!ret) {
            ret = parent;
          }
        }
      }

      node = node2;
    }
    else {
      break;
    }
  }

  return ret ? ret : node;
}

static int heap_get_level(const MinMaxHeap *heap, const MinMaxHeapNode *node)
{
  int i = 0;

  while (node->parent != -1) {
    node = heap->nodes + node->parent;
    i++;
  }

  return i;
}

static MinMaxHeapNode *heap_push_down(MinMaxHeap *heap, MinMaxHeapNode *node)
{
  int i = heap_get_level(heap, node);

  if (i & 1) {
    return heap_push_down_max(heap, node);
  }
  else {
    return heap_push_down_min(heap, node);
  }
}

static MinMaxHeapNode *heap_push_up_min(MinMaxHeap *heap, MinMaxHeapNode *node)
{
  while (node->parent != -1) {
    MinMaxHeapNode *parent = heap->nodes + node->parent;

    if (parent->parent != -1 && node->value < heap->nodes[parent->parent].value) {
      parent = heap->nodes + parent->parent;

      SWAP(float, node->value, parent->value);
      SWAP(void *, node->ptr, parent->ptr);

      node = parent;
    }
    else {
      break;
    }
  }

  return node;
}

static MinMaxHeapNode *heap_push_up_max(MinMaxHeap *heap, MinMaxHeapNode *node)
{
  while (node->parent != -1) {
    MinMaxHeapNode *parent = heap->nodes + node->parent;

    if (parent->parent != -1 && node->value > heap->nodes[parent->parent].value) {
      parent = heap->nodes + parent->parent;

      SWAP(float, node->value, parent->value);
      SWAP(void *, node->ptr, parent->ptr);

      node = parent;
    }
    else {
      break;
    }
  }

  return node;
}

static MinMaxHeapNode *heap_push_up(MinMaxHeap *heap, MinMaxHeapNode *node)
{
  int i = heap_get_level(heap, node);

  if ((i & 1) == 0) {
    MinMaxHeapNode *parent = heap->nodes + node->parent;

    if (node->value > parent->value) {
      SWAP(float, node->value, parent->value);
      SWAP(void *, node->ptr, parent->ptr);

      return heap_push_up_max(heap, parent);
    }
    else {
      return heap_push_up_min(heap, node);
    }
  }
  else {
    MinMaxHeapNode *parent = heap->nodes + node->parent;

    if (node->value < parent->value) {
      SWAP(float, node->value, parent->value);
      SWAP(void *, node->ptr, parent->ptr);

      return heap_push_up_min(heap, parent);
    }
    else {
      return heap_push_up_max(heap, node);
    }
  }

  return node;
}

/**
 * Insert heap node with a value (often a 'cost') and pointer into the heap,
 * duplicate values are allowed.
 */
MinMaxHeapNode *BLI_mm_heap_insert(MinMaxHeap *heap, float value, void *ptr)
{
  MinMaxHeapNode *node = heap_make_node(heap);

  node->ptr = ptr;
  node->value = value;

  if (heap->totnode == 1) {
    return node;
  }

  int i = node - heap->nodes;

  node->parent = (i - 1) >> 1;

  MinMaxHeapNode *parent = heap->nodes + node->parent;
  if (parent->child1 == -1) {
    parent->child1 = i;
  }
  else {
    parent->child2 = i;
  }

  return heap_push_up(heap, node);
}

/**
 * Convenience function since this is a common pattern.
 */
void BLI_mm_heap_insert_or_update(MinMaxHeap *heap,
                                  MinMaxHeapNode **node_p,
                                  float value,
                                  void *ptr)
{
  MinMaxHeapNode *node = *node_p;

  if (!node) {
    *node_p = BLI_mm_heap_insert(heap, value, ptr);
    return;
  }

  node = heap_push_down(heap, node);
  node = heap_push_up(heap, node);

  *node_p = node;
}

bool BLI_mm_heap_is_empty(const MinMaxHeap *heap)
{
  return heap->totnode == 0;
}

unsigned int BLI_mm_heap_len(const MinMaxHeap *heap)
{
  return heap->totnode;
}

MinMaxHeapNode *BLI_mm_heap_min(const MinMaxHeap *heap)
{
  return heap->nodes;
}

float BLI_mm_heap_min_value(const MinMaxHeap *heap)
{
  return heap->nodes[0].value;
}

MinMaxHeapNode *BLI_mm_heap_max(const MinMaxHeap *heap)
{
  if (heap->totnode == 1) {
    return heap->nodes;
  }

  if (heap->nodes[0].child1 != -1) {
    return heap->nodes + heap->nodes[0].child1;
  }
  else {
    return heap->nodes + heap->nodes[0].child2;
  }
}

float BLI_mm_heap_max_value(const MinMaxHeap *heap)
{
  return BLI_mm_heap_max(heap)->value;
}

static MinMaxHeapNode *heap_pop_last(MinMaxHeap *heap)
{
  MinMaxHeapNode *last = heap->nodes + heap->totnode - 1;

  if (last->parent) {
    MinMaxHeapNode *parent = heap->nodes + last->parent;
    if (parent->child1 == heap->totnode - 1) {
      parent->child1 = -1;
    }
    else {
      parent->child2 = -1;
    }
  }

  heap->totnode--;

  return last;
}

void *BLI_mm_heap_pop_min(MinMaxHeap *heap)
{
  void *ret = heap->nodes[0].ptr;

  if (heap->totnode == 1) {
    heap->totnode--;
    return ret;
  }

#ifdef BLI_MINMAX_HEAP_VALIDATE
  if (!BLI_mm_heap_is_valid(heap)) {
    printf("invalid heap!\n");
  }
#endif

  MinMaxHeapNode *last = heap_pop_last(heap);

  SWAP(float, last->value, heap->nodes->value);
  SWAP(void *, last->ptr, heap->nodes->ptr);

  heap_push_down(heap, heap->nodes);

#ifdef BLI_MINMAX_HEAP_VALIDATE
  if (!BLI_mm_heap_is_valid(heap)) {
    printf("invalid heap!\n");
  }
#endif

  return ret;
}

void *BLI_mm_heap_pop_max(MinMaxHeap *heap)
{
  MinMaxHeapNode *node = BLI_mm_heap_max(heap);

  void *ret = node->ptr;

  if (heap->totnode == 1) {
    heap->totnode--;
    return ret;
  }

#ifdef BLI_MINMAX_HEAP_VALIDATE
  if (!BLI_mm_heap_is_valid(heap)) {
    printf("invalid heap!\n");
  }
#endif

  MinMaxHeapNode *last = heap_pop_last(heap);

  node->value = last->value;
  node->ptr = last->ptr;

  // node = heap_push_up(heap, node);
  node = heap_push_down(heap, node);
  // heap_push_down(heap, heap->nodes);

#if 0
  if (heap->nodes[0].child2 != -1 && heap->nodes[0].child1 != 0) {
    MinMaxHeapNode *n1 = heap->nodes + heap->nodes[0].child1;
    MinMaxHeapNode *n2 = heap->nodes + heap->nodes[0].child2;

    if (n1->value < n2->value) {
      SWAP(float, n1->value, n2->value);
      SWAP(void *, n1->ptr, n2->ptr);

      heap_push_down(heap, n1);
      heap_push_down(heap, n2);
    }
  }
#endif

#ifdef BLI_MINMAX_HEAP_VALIDATE
  if (!BLI_mm_heap_is_valid(heap)) {
    printf("invalid heap!\n");
  }
#endif

  return ret;
}

MinMaxHeapNode *BLI_mm_heap_node_value_update(MinMaxHeap *heap, MinMaxHeapNode *node, float value)
{
  node->value = value;

  node = heap_push_down(heap, node);
  node = heap_push_up(heap, node);

  return node;
}

MinMaxHeapNode *BLI_mm_heap_node_value_update_ptr(MinMaxHeap *heap,
                                                  MinMaxHeapNode *node,
                                                  float value,
                                                  void *ptr)
{
  node->value = value;
  node->ptr = ptr;

  node = heap_push_down(heap, node);
  node = heap_push_up(heap, node);

  return node;
}

/**
 * Return the value or pointer of a heap node.
 */
float BLI_mm_heap_node_value(const MinMaxHeapNode *node)
{
  return node->value;
}

void *BLI_mm_heap_node_ptr(const MinMaxHeapNode *node)
{
  return node->ptr;
}
/**
 * Only for checking internal errors (gtest).
 */
bool BLI_mm_heap_is_valid(const MinMaxHeap *heap)
{
  for (int i = 0; i < heap->totnode; i++) {
    MinMaxHeapNode *node = heap->nodes + i;

    int level = heap_get_level(heap, node);

    if (i > 0 && (node->parent < 0 || node->parent >= heap->totnode)) {
      return false;
    }

    if (i < 3) {
      continue;
    }

    MinMaxHeapNode *parent = heap->nodes + node->parent;

    if (parent->parent < 0 || parent->parent >= heap->totnode) {
      return false;
    }

    parent = heap->nodes + parent->parent;
    bool test = parent->value < node->value;

    if (parent->value == node->value) {
      continue;
    }

    test = test ^ (level & 1);

    if (!test) {
      return false;
    }
  }

  return true;
}

#include "BLI_rand.h"

void test_mm_heap()
{
  MinMaxHeap *heap = BLI_mm_heap_new();
  RNG *rng = BLI_rng_new(0);
  const int steps = 1024;
  void *ptr = NULL;

  for (int i = 0; i < steps; i++) {
    float f = floorf(BLI_rng_get_float(rng) * 10.0);

    BLI_mm_heap_insert(heap, f, ptr);
  }

  for (int i = 0; i < steps; i++) {
    if (i & 1) {
      BLI_mm_heap_pop_max(heap);
    }
    else {
      BLI_mm_heap_pop_min(heap);
    }

    BLI_mm_heap_is_valid(heap);
  }

  BLI_rng_free(rng);
  BLI_mm_heap_is_valid(heap);
  BLI_mm_heap_free(heap, NULL);
}
