
#pragma once

/** \file
 * \ingroup bli
 * \brief A min-heap / priority queue ADT
 */

#include "BLI_index_range.hh"
#include "BLI_math.h"
#include "BLI_memory_utils.hh"
#include "BLI_vector.hh"

using blender::IndexRange;
using blender::Vector;

#include <algorithm>

namespace blender {
template<typename Value = void *,
         int64_t InlineBufferCapacity = default_inline_buffer_capacity(sizeof(Value))>

#if 1
class MinMaxHeap {
  struct MinMaxHeapNode {
    Value value;
    float weight;

    int child1 = -1, child2 = -1, parent = -1;
  };

 public:
  MinMaxHeap(int reserved = 0) {}
  ATTR_NO_OPT MinMaxHeapNode *insert(float weight, Value value)
  {
    nodes.resize(nodes.size() + 1);
    MinMaxHeapNode *node = &nodes.last();

    node->weight = weight;
    node->value = value;

    return node;
  }

  MinMaxHeapNode *max()
  {
    MinMaxHeapNode *max_node = nullptr;
    float max = FLT_MIN;

    for (MinMaxHeapNode &node : nodes) {
      if (node.weight > max) {
        max_node = &node;
        max = node.weight;
      }
    }

    return max_node;
  }
  MinMaxHeapNode *min()
  {
    MinMaxHeapNode *min_node = nullptr;
    float min = FLT_MAX;

    for (MinMaxHeapNode &node : nodes) {
      if (node.weight < min) {
        min_node = &node;
        min = node.weight;
      }
    }

    return min_node;
  }

  float min_weight()
  {
    return min()->weight;
  }

  float max_weight()
  {
    return max()->weight;
  }

  ATTR_NO_OPT void pop_node(MinMaxHeapNode *node)
  {
    int i = node - nodes.data();

    nodes[i] = nodes[nodes.size() - 1];
    nodes.pop_last();
  }

  Value pop_min(float *r_w = nullptr)
  {
    MinMaxHeapNode *node = min();
    if (r_w) {
      *r_w = node->weight;
    }

    Value ret = node->value;
    pop_node(node);

    return ret;
  }

  Value pop_max(float *r_w = nullptr)
  {
    MinMaxHeapNode *node = max();
    if (r_w) {
      *r_w = node->weight;
    }

    Value ret = node->value;
    pop_node(node);

    return ret;
  }

  int len()
  {
    return nodes.size();
  }

  bool empty()
  {
    return nodes.size() == 0;
  }

 private:
  Vector<MinMaxHeapNode> nodes;
};

#else
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

    return heap_push_up(node);
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
    return max()->weight;
  }

  Value pop_min()
  {
    if (nodes.size() == 1) {
      return nodes.pop_last().value;
    }

#  ifdef BLI_MINMAX_HEAP_VALIDATE
    if (!is_valid()) {
      printf("invalid heap!\n");
    }
#  endif

    Value ret = nodes[0].value;
    MinMaxHeapNode *last = heap_pop_last();

    std::swap(last->weight, nodes[0].weight);
    std::swap(last->value, nodes[0].value);

    heap_push_down(&nodes[0]);

#  ifdef BLI_MINMAX_HEAP_VALIDATE
    if (!is_valid()) {
      printf("invalid heap!\n");
    }
#  endif

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

    MinMaxHeapNode *node = max();

#  ifdef BLI_MINMAX_HEAP_VALIDATE
    if (!is_valid()) {
      printf("invalid heap!\n");
    }
#  endif

    Value ret = node->value;
    if (r_w) {
      *r_w = node->weight;
    }

    MinMaxHeapNode *last = heap_pop_last();

    node->weight = last->weight;
    node->value = last->value;

    node = heap_push_down(node);

#  ifdef BLI_MINMAX_HEAP_VALIDATE
    if (!is_valid()) {
      printf("invalid heap!\n");
    }
#  endif

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
  MinMaxHeapNode *min()
  {
    return &nodes[0];
  }

  MinMaxHeapNode *max()
  {
    if (nodes.size() == 1) {
      return &nodes[0];
    }

    MinMaxHeapNode &root = nodes[0];
    if (root.child1 != -1 && root.child2 != -1) {
      if (nodes[nodes[0].child1].weight > nodes[nodes[0].child2].weight) {
        return &nodes[nodes[0].child1];
      }

      return &nodes[nodes[0].child2];
    }
    else if (root.child1 != -1) {
      return &nodes[nodes[0].child1];
    }
    else {
      return &nodes[nodes[0].child2];
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
    MinMaxHeapNode *ret = nullptr;

    while (node->child1 >= 0 || node->child2 >= 0) {
      MinMaxHeapNode *node2 = heap_descent_min(node);

      if (!node2) {
        break;
      }

      if (node2->weight < node->weight) {
        std::swap(node2->weight, node->weight);
        std::swap(node2->value, node->value);

        if (node2->parent != node - nodes.data()) {
          MinMaxHeapNode *parent = &nodes[node2->parent];

          if (node2->weight > parent->weight) {
            std::swap(node2->weight, parent->weight);
            std::swap(node2->value, parent->value);

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

  MinMaxHeapNode *heap_push_down_max(MinMaxHeapNode *node)
  {
    MinMaxHeapNode *ret = nullptr;

    while (node->child1 >= 0 || node->child2 >= 0) {
      MinMaxHeapNode *node2 = heap_descent_max(node);

      if (node2->weight > node->weight) {
        std::swap(node2->weight, node->weight);
        std::swap(node2->value, node->value);

        if (node2->parent != node - nodes.data()) {
          MinMaxHeapNode *parent = &nodes[node2->parent];

          if (node2->weight < parent->weight) {
            std::swap(node2->weight, parent->weight);
            std::swap(node2->value, parent->value);

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

  MinMaxHeapNode *heap_pop_last()
  {
    MinMaxHeapNode *last = &nodes[nodes.size() - 1];

    if (last->parent) {
      MinMaxHeapNode *parent = &nodes[last->parent];
      if (parent->child1 == nodes.size() - 1) {
        parent->child1 = -1;
      }
      else {
        parent->child2 = -1;
      }
    }

    nodes.pop_last();

    return last;
  }

  Vector<MinMaxHeapNode, InlineBufferCapacity> nodes;
};
#endif
}  // namespace blender
