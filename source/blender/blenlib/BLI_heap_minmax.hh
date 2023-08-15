/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 * \brief A min-heap / priority queue ADT
 */

#include "BLI_index_range.hh"
#include "BLI_memory_utils.hh"
#include "BLI_vector.hh"

using blender::IndexRange;
using blender::Vector;

#include <algorithm>

//#define BLI_MINMAX_HEAP_VALIDATE

namespace blender {
template<typename Value, typename NodeType> class HeapValueIter {
  Span<NodeType> nodes_;
  int i_ = 0;

 public:
  HeapValueIter(Vector<NodeType> &nodes) : nodes_(nodes) {}
  HeapValueIter(Span<NodeType> &nodes) : nodes_(nodes) {}
  HeapValueIter(const HeapValueIter &b) : nodes_(b.nodes_), i_(b.i_) {}

  bool operator==(const HeapValueIter &b)
  {
    return b.i_ == i_;
  }

  bool operator!=(const HeapValueIter &b)
  {
    return b.i_ != i_;
  }

  Value operator*()
  {
    return nodes_[i_].value;
  }

  HeapValueIter &operator++()
  {
    i_++;

    return *this;
  }

  HeapValueIter begin()
  {
    return HeapValueIter(nodes_);
  }

  HeapValueIter end()
  {
    HeapValueIter ret(nodes_);
    ret.i_ = std::max(int(nodes_.size()) - 1, 0);

    return ret;
  }
};

template<typename Value = void *,
         int64_t InlineBufferCapacity = default_inline_buffer_capacity(sizeof(Value))>

class MinMaxHeap {
  struct MinMaxHeapNode {
    Value value;
    float weight;

    int child1 = -1, child2 = -1, parent = -1;
  };

 public:
  MinMaxHeap(int reserved = 0)
  {
    if (reserved > 0) {
      nodes.reserve(reserved);
    }
  }

  ~MinMaxHeap() {}

  bool valid_recurse(int i)
  {
    bool ret = true;

    MinMaxHeapNode &node = nodes[i];
    int level = i & 1;

    if (node.child1 != -1) {
      MinMaxHeapNode &child = nodes[node.child1];
      if (child.weight != node.weight && (child.weight < node.weight) ^ level) {
        printf("error: node %d[%f] is less than node %d[%f]\n",
               i,
               node.weight,
               node.child1,
               child.weight);
      }

      ret = ret && valid_recurse(node.child1);
    }

    if (node.child2 != -1) {
      MinMaxHeapNode &child = nodes[node.child2];

      if (child.weight != node.weight && (child.weight < node.weight) ^ level) {
        printf("error: node %d[%f] is less than node %d[%f]\n",
               i,
               node.weight,
               node.child2,
               child.weight);
      }

      ret = ret && valid_recurse(node.child2);
    }

    return ret;
  }

  bool is_valid()
  {

    if (nodes.size() == 0) {
      return true;
    }

    return valid_recurse(0);
  }

  HeapValueIter<Value, MinMaxHeapNode> values()
  {
    return HeapValueIter<Value, MinMaxHeapNode>(nodes);
  }

  MinMaxHeapNode *insert(float weight, Value value)
  {
    MinMaxHeapNode *node = heap_make_node();

    node->value = value;
    node->weight = weight;

    if (nodes.size() == 1) {
      return node;
    }

    int i = node - nodes.data();

    node->parent = (i - 1) >> 1;

    MinMaxHeapNode *parent = &nodes[node->parent];
    if (parent->child1 == -1) {
      parent->child1 = i;
    }
    else {
      parent->child2 = i;
    }

    MinMaxHeapNode *ret = heap_push_up(node);

#ifdef BLI_MINMAX_HEAP_VALIDATE
    if (!is_valid()) {
      printf("invalid heap!\n");
    }
#endif

    return ret;
  }

  void insert_or_update(MinMaxHeapNode **node_p, float weight, Value value)
  {
    MinMaxHeapNode *node = *node_p;

    if (!node) {
      *node_p = insert(weight, value);
      return;
    }

    node = heap_push_down(node);
    node = heap_push_up(node);

    *node_p = node;
  }

  bool empty() const
  {
    return nodes.size() == 0;
  }

  unsigned int len() const
  {
    return nodes.size();
  }

  float min_weight()
  {
    return nodes[0].weight;
  }

  float max_weight()
  {
    return max().weight;
  }

  Value pop_min()
  {
    if (nodes.size() == 1) {
      return nodes.pop_last().value;
    }

#ifdef BLI_MINMAX_HEAP_VALIDATE
    if (!is_valid()) {
      printf("invalid heap!\n");
    }
#endif

    Value ret = nodes[0].value;
    MinMaxHeapNode last = heap_pop_last();

    nodes[0].weight = last.weight;
    nodes[0].value = last.value;

    heap_push_down(&nodes[0]);

#ifdef BLI_MINMAX_HEAP_VALIDATE
    if (!is_valid()) {
      printf("invalid heap!\n");
    }
#endif

    return ret;
  }

  Value pop_max(float *r_w = nullptr)
  {
    if (nodes.size() == 1) {
      if (r_w) {
        *r_w = nodes[0].weight;
      }
      return nodes.pop_last().value;
    }

    MinMaxHeapNode &node = max();

#ifdef BLI_MINMAX_HEAP_VALIDATE
    if (!is_valid()) {
      printf("invalid heap!\n");
    }
#endif

    Value ret = node.value;
    if (r_w) {
      *r_w = node.weight;
    }

    MinMaxHeapNode last = heap_pop_last();

    node.weight = last.weight;
    node.value = last.value;

    heap_push_down(&node);

#ifdef BLI_MINMAX_HEAP_VALIDATE
    if (!is_valid()) {
      printf("invalid heap!\n");
    }
#endif

    return ret;
  }

  MinMaxHeapNode *node_weight_update(MinMaxHeapNode *node, float weight)
  {
    node->weight = weight;

    node = heap_push_down(node);
    node = heap_push_up(node);

    return node;
  }

  MinMaxHeapNode *node_weight_update_value(MinMaxHeapNode *node, float weight, Value value)
  {
    node->weight = weight;
    node->value = value;

    node = heap_push_down(node);
    node = heap_push_up(node);

    return node;
  }

 private:
  MinMaxHeapNode &min()
  {
    return nodes[0];
  }

  MinMaxHeapNode &max()
  {
    if (nodes.size() == 1) {
      return nodes[0];
    }

    MinMaxHeapNode &root = nodes[0];
    if (root.child1 != -1 && root.child2 != -1) {
      if (nodes[nodes[0].child1].weight > nodes[nodes[0].child2].weight) {
        return nodes[nodes[0].child1];
      }

      return nodes[nodes[0].child2];
    }
    else if (root.child1 != -1) {
      return nodes[nodes[0].child1];
    }
    else {
      return nodes[nodes[0].child2];
    }
  }

  MinMaxHeapNode *heap_make_node()
  {
    nodes.resize(nodes.size() + 1);
    return &nodes[nodes.size() - 1];
  }

  MinMaxHeapNode *heap_descent_min(MinMaxHeapNode *node)
  {
    if (node->child1 != -1 && node->child2 != -1) {
      MinMaxHeapNode *n1 = &nodes[node->child1];
      MinMaxHeapNode *n2 = &nodes[node->child2];

      n1 = heap_descent_min2(n1);
      n2 = heap_descent_min2(n2);

      return n1->value < n2->value ? n1 : n2;
    }
    else if (node->child1 != -1) {
      return heap_descent_min2(&nodes[node->child1]);
    }
    else if (node->child2 != -1) {
      return heap_descent_min2(&nodes[node->child2]);
    }

    return nullptr;
  }

  MinMaxHeapNode *heap_descent_min2(MinMaxHeapNode *n)
  {
    if (n->child1 != -1 && n->child2 != -1) {
      MinMaxHeapNode *n1 = &nodes[n->child1];
      MinMaxHeapNode *n2 = &nodes[n->child2];

      return n1->weight < n2->weight ? n1 : n2;
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

      return n1->weight > n2->weight ? n1 : n2;
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

      n1 = heap_descent_max2(n1);
      n2 = heap_descent_max2(n2);

      return n1->weight > n2->weight ? n1 : n2;
    }
    else if (node->child1 != -1) {
      return heap_descent_max2(&nodes[node->child1]);
    }
    else if (node->child2 != -1) {
      return heap_descent_max2(&nodes[node->child2]);
    }

    return nullptr;
  }

  MinMaxHeapNode *heap_push_down_min(MinMaxHeapNode *node)
  {
    MinMaxHeapNode *ret = node;
    MinMaxHeapNode *node2 = heap_descent_min(node);

    if (!node2) {
      return node;
    }

    /* Is node2 a grandchild? */
    if (node2->parent != node - nodes.data()) {
      MinMaxHeapNode *parent = &nodes[node2->parent];

      if (node2->weight < node->weight) {
        std::swap(node2->weight, node->weight);
        std::swap(node2->value, node->value);
        ret = node2;

        if (node2->weight > parent->weight) {
          std::swap(node2->weight, parent->weight);
          std::swap(node2->value, parent->value);
          ret = parent;
        }

        ret = heap_push_down(node2);
      }
    }
    else if (node2->weight < node->weight) {
      std::swap(node2->weight, node->weight);
      std::swap(node2->value, node->value);
      ret = node2;
    }

    return ret;
  }

  MinMaxHeapNode *heap_push_down_max(MinMaxHeapNode *node)
  {
    MinMaxHeapNode *ret = node;
    MinMaxHeapNode *node2 = heap_descent_max(node);

    if (!node2) {
      return node;
    }

    /* Is node2 a grandchild? */
    if (node2->parent != node - nodes.data()) {
      MinMaxHeapNode *parent = &nodes[node2->parent];

      if (node2->weight > node->weight) {
        std::swap(node2->weight, node->weight);
        std::swap(node2->value, node->value);
        ret = node2;

        if (node2->weight < parent->weight) {
          std::swap(node2->weight, parent->weight);
          std::swap(node2->value, parent->value);
          ret = parent;
        }

        ret = heap_push_down(node2);
      }
    }
    else if (node2->weight > node->weight) {
      std::swap(node2->weight, node->weight);
      std::swap(node2->value, node->value);
      ret = node2;
    }

    return ret;
  }

  int heap_get_level(const MinMaxHeapNode *node)
  {
    int i = 0;

    while (node->parent != -1) {
      node = &nodes[node->parent];
      i++;
    }

    return i;
  }

  MinMaxHeapNode *heap_push_down(MinMaxHeapNode *node)
  {
    int i = heap_get_level(node);

    if (i & 1) {
      return heap_push_down_max(node);
    }
    else {
      return heap_push_down_min(node);
    }
  }

  MinMaxHeapNode *heap_push_up_min(MinMaxHeapNode *node)
  {
    while (node->parent != -1) {
      MinMaxHeapNode *parent = &nodes[node->parent];

      if (parent->parent != -1 && node->weight < nodes[parent->parent].weight) {
        parent = &nodes[parent->parent];

        std::swap(node->weight, parent->weight);
        std::swap(node->value, parent->value);

        node = parent;
      }
      else {
        break;
      }
    }

    return node;
  }

  MinMaxHeapNode *heap_push_up_max(MinMaxHeapNode *node)
  {
    while (node->parent != -1) {
      MinMaxHeapNode *parent = &nodes[node->parent];

      if (parent->parent != -1 && node->weight > nodes[parent->parent].weight) {
        parent = &nodes[parent->parent];

        std::swap(node->weight, parent->weight);
        std::swap(node->value, parent->value);

        node = parent;
      }
      else {
        break;
      }
    }

    return node;
  }

  MinMaxHeapNode *heap_push_up(MinMaxHeapNode *node)
  {
    int i = heap_get_level(node);

    if ((i & 1) == 0) {
      MinMaxHeapNode *parent = &nodes[node->parent];

      if (node->weight > parent->weight) {
        std::swap(node->weight, parent->weight);
        std::swap(node->value, parent->value);

        return heap_push_up_max(parent);
      }
      else {
        return heap_push_up_min(node);
      }
    }
    else {
      MinMaxHeapNode *parent = &nodes[node->parent];

      if (node->weight < parent->weight) {
        std::swap(node->weight, parent->weight);
        std::swap(node->value, parent->value);

        return heap_push_up_min(parent);
      }
      else {
        return heap_push_up_max(node);
      }
    }

    return node;
  }

  MinMaxHeapNode heap_pop_last()
  {
    int index = nodes.size() - 1;

    MinMaxHeapNode last = nodes[index];
    if (last.parent != -1) {
      MinMaxHeapNode &parent = nodes[last.parent];
      if (parent.child1 == index) {
        parent.child1 = -1;
      }
      else {
        parent.child2 = -1;
      }
    }

    nodes.pop_last();
    return last;
  }

  Vector<MinMaxHeapNode, InlineBufferCapacity> nodes;
};
}  // namespace blender
