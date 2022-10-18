
#pragma once

/** \file
 * \ingroup bli
 * \brief A min-heap / priority queue ADT
 */

#include "BLI_index_range.hh"
#include "BLI_math.h"
#include "BLI_vector.hh"

using blender::IndexRange;
using blender::Vector;

namespace blender {
template<typename T> class MinMaxHeapNode {
 public:
 private:
  T *value;
  float value;
  int child1 = -1, child2 = -1, parent = -1;
};

template<typename T> class MinMaxHeap {
 public:
  MinMaxHeap(int reserved = 0)
  {
    nodes.reserve(reserved);
  }

  ~MinMaxHeap()
  {
  }

 private:
  MinMaxHeapNode *heap_make_node()
  {
    nodes.append(MinMaxHeapNode());
    return &nodes[nodes.size() - 1];
  }

  MinMaxHeapNode *heap_descent_min2(MinMaxHeapNode *n)
  {
    if (n->child1 != -1 && n->child2 != -1) {
      MinMaxHeapNode *n1 = &nodes[n->child1];
      MinMaxHeapNode *n2 = &nodes[n->child2];

      return n1->value < n2->value ? n1 : n2;
    }
    else if (n->child1 != -1) {
      return &nodes[n->child1];
    }
    else if (n->child2 != -1) {
      return &nodes[n->child2];
    }

    return n;
  }

  MinMaxHeapNode *heap_descent_max2(MinMaxHeapNode *n)
  {
    if (n->child1 != -1 && n->child2 != -1) {
      MinMaxHeapNode *n1 = &nodes[n->child1];
      MinMaxHeapNode *n2 = &nodes[n->child2];

      return n1->value > n2->value ? n1 : n2;
    }
    else if (n->child1 != -1) {
      return &nodes[n->child1];
    }
    else if (n->child2 != -1) {
      return &nodes[n->child2];
    }

    return n;
  }

  /* find node grandchild */
  MinMaxHeapNode *heap_descent_max(MinMaxHeapNode *node)
  {
    if (node->child1 != -1 && node->child2 != -1) {
      MinMaxHeapNode *n1 = &nodes[node->child1];
      MinMaxHeapNode *n2 = &nodes[node->child2];

      n1 = heap_descent_max2(heap, n1);
      n2 = heap_descent_max2(heap, n2);

      return n1->value > n2->value ? n1 : n2;
    }
    else if (node->child1 != -1) {
      return heap_descent_max2(&nodes[node->child1]);
    }
    else if (node->child2 != -1) {
      return heap_descent_max2(&nodes[node->child2]);
    }

    return NULL;
  }

  Vector<MinMaxHeapNode<T>> nodes;
};
}  // namespace blender
