/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 * \brief A KD-tree for nearest neighbor search.
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_kdtree_types.hh"
#include "BLI_math_base.h"
#include "BLI_vector.hh"

#include <algorithm>

namespace blender {

#define KD_STACK_INIT 100     /* initial size for array (on the stack) */
#define KD_NEAR_ALLOC_INC 100 /* alloc increment for collecting nearest */
#define KD_FOUND_ALLOC_INC 50 /* alloc increment for collecting nearest */

#define KD_NODE_UNSET ((uint) - 1)

/**
 * When set we know all values are unbalanced,
 * otherwise clear them when re-balancing: see #62210.
 */
#define KD_NODE_ROOT_IS_INIT ((uint) - 2)

namespace detail {

/* -------------------------------------------------------------------- */
/** \name Local Math API
 * \{ */

template<int DimsNum> static void copy_vn_vn(float v0[DimsNum], const float v1[DimsNum])
{
  for (uint j = 0; j < DimsNum; j++) {
    v0[j] = v1[j];
  }
}

template<int DimsNum>
static float len_squared_vnvn(const float v0[DimsNum], const float v1[DimsNum])
{
  float d = 0.0f;
  for (uint j = 0; j < DimsNum; j++) {
    d += square_f(v0[j] - v1[j]);
  }
  return d;
}

template<int DimsNum>
static float len_squared_vnvn_cb(const float co_kdtree[DimsNum],
                                 const float co_search[DimsNum],
                                 const void * /*user_data*/)
{
  return len_squared_vnvn<DimsNum>(co_kdtree, co_search);
}

/** \} */

}  // namespace detail

/**
 * Creates or free a kdtree
 * \param nodes_len_capacity: The maximum length this KD-tree may hold.
 */
template<int DimsNum> inline KDTree<DimsNum> *kdtree_new(uint nodes_len_capacity)
{
  KDTree<DimsNum> *tree;

  tree = MEM_callocN<KDTree<DimsNum>>("KDTree");
  tree->nodes = MEM_malloc_arrayN<KDTreeNode<DimsNum>>(nodes_len_capacity, "KDTreeNode<>");
  tree->nodes_len = 0;
  tree->root = KD_NODE_ROOT_IS_INIT;
  tree->max_node_index = -1;

#ifndef NDEBUG
  tree->is_balanced = false;
  tree->nodes_len_capacity = nodes_len_capacity;
#endif

  return tree;
}

template<int DimsNum> inline void kdtree_free(KDTree<DimsNum> *tree)
{
  if (tree) {
    MEM_freeN(tree->nodes);
    MEM_freeN(tree);
  }
}

/**
 * Construction: first insert points, then call balance. Normal is optional.
 */
template<int DimsNum>
inline void kdtree_insert(KDTree<DimsNum> *tree, int index, const float co[DimsNum])
{
  KDTreeNode<DimsNum> *node = &tree->nodes[tree->nodes_len++];

#ifndef NDEBUG
  BLI_assert(tree->nodes_len <= tree->nodes_len_capacity);
#endif

  /* NOTE: array isn't calloc'd,
   * need to initialize all struct members */

  node->left = node->right = KD_NODE_UNSET;
  detail::copy_vn_vn<DimsNum>(node->co, co);
  node->index = index;
  node->d = 0;
  tree->max_node_index = std::max(tree->max_node_index, index);

#ifndef NDEBUG
  tree->is_balanced = false;
#endif
}

namespace detail {

template<int DimsNum>
static uint kdtree_balance(KDTreeNode<DimsNum> *nodes, uint nodes_len, uint axis, const uint ofs)
{
  KDTreeNode<DimsNum> *node;
  float co;
  uint left, right, median, i, j;

  if (nodes_len <= 0) {
    return KD_NODE_UNSET;
  }
  if (nodes_len == 1) {
    return 0 + ofs;
  }

  /* Quick-sort style sorting around median. */
  left = 0;
  right = nodes_len - 1;
  median = nodes_len / 2;

  while (right > left) {
    co = nodes[right].co[axis];
    i = left - 1;
    j = right;

    while (true) {
      while (nodes[++i].co[axis] < co) { /* pass */
      }
      while (nodes[--j].co[axis] > co && j > left) { /* pass */
      }

      if (i >= j) {
        break;
      }

      SWAP(KDTreeNode_head<DimsNum>,
           *(KDTreeNode_head<DimsNum> *)&nodes[i],
           *(KDTreeNode_head<DimsNum> *)&nodes[j]);
    }

    SWAP(KDTreeNode_head<DimsNum>,
         *(KDTreeNode_head<DimsNum> *)&nodes[i],
         *(KDTreeNode_head<DimsNum> *)&nodes[right]);
    if (i >= median) {
      right = i - 1;
    }
    if (i <= median) {
      left = i + 1;
    }
  }

  /* Set node and sort sub-nodes. */
  node = &nodes[median];
  node->d = axis;
  axis = (axis + 1) % DimsNum;
  node->left = kdtree_balance(nodes, median, axis, ofs);
  node->right = kdtree_balance(
      nodes + median + 1, (nodes_len - (median + 1)), axis, (median + 1) + ofs);

  return median + ofs;
}

}  // namespace detail

template<int DimsNum> inline void kdtree_balance(KDTree<DimsNum> *tree)
{
  if (tree->root != KD_NODE_ROOT_IS_INIT) {
    for (uint i = 0; i < tree->nodes_len; i++) {
      tree->nodes[i].left = KD_NODE_UNSET;
      tree->nodes[i].right = KD_NODE_UNSET;
    }
  }

  tree->root = detail::kdtree_balance<DimsNum>(tree->nodes, tree->nodes_len, 0, 0);

#ifndef NDEBUG
  tree->is_balanced = true;
#endif
}

namespace detail {

template<int DimsNum>
static uint *realloc_nodes(uint *stack, uint *stack_len_capacity, const bool is_alloc)
{
  uint *stack_new = MEM_malloc_arrayN<uint>(*stack_len_capacity + KD_NEAR_ALLOC_INC,
                                            "KDTree.treestack");
  memcpy(stack_new, stack, *stack_len_capacity * sizeof(uint));
  // memset(stack_new + *stack_len_capacity, 0, sizeof(uint) * KD_NEAR_ALLOC_INC);
  if (is_alloc) {
    MEM_freeN(stack);
  }
  *stack_len_capacity += KD_NEAR_ALLOC_INC;
  return stack_new;
}

}  // namespace detail

/**
 * Find nearest returns index, and -1 if no node is found.
 */
template<int DimsNum>
inline int kdtree_find_nearest(const KDTree<DimsNum> *tree,
                               const float co[DimsNum],
                               KDTreeNearest<DimsNum> *r_nearest)
{
  const KDTreeNode<DimsNum> *nodes = tree->nodes;
  const KDTreeNode<DimsNum> *root, *min_node;
  uint *stack, stack_default[KD_STACK_INIT];
  float min_dist, cur_dist;
  uint stack_len_capacity, cur = 0;

#ifndef NDEBUG
  BLI_assert(tree->is_balanced == true);
#endif

  if (UNLIKELY(tree->root == KD_NODE_UNSET)) {
    return -1;
  }

  stack = stack_default;
  stack_len_capacity = KD_STACK_INIT;

  root = &nodes[tree->root];
  min_node = root;
  min_dist = detail::len_squared_vnvn<DimsNum>(root->co, co);

  if (co[root->d] < root->co[root->d]) {
    if (root->right != KD_NODE_UNSET) {
      stack[cur++] = root->right;
    }
    if (root->left != KD_NODE_UNSET) {
      stack[cur++] = root->left;
    }
  }
  else {
    if (root->left != KD_NODE_UNSET) {
      stack[cur++] = root->left;
    }
    if (root->right != KD_NODE_UNSET) {
      stack[cur++] = root->right;
    }
  }

  while (cur--) {
    const KDTreeNode<DimsNum> *node = &nodes[stack[cur]];

    cur_dist = node->co[node->d] - co[node->d];

    if (cur_dist < 0.0f) {
      cur_dist = -cur_dist * cur_dist;

      if (-cur_dist < min_dist) {
        cur_dist = detail::len_squared_vnvn<DimsNum>(node->co, co);
        if (cur_dist < min_dist) {
          min_dist = cur_dist;
          min_node = node;
        }
        if (node->left != KD_NODE_UNSET) {
          stack[cur++] = node->left;
        }
      }
      if (node->right != KD_NODE_UNSET) {
        stack[cur++] = node->right;
      }
    }
    else {
      cur_dist = cur_dist * cur_dist;

      if (cur_dist < min_dist) {
        cur_dist = detail::len_squared_vnvn<DimsNum>(node->co, co);
        if (cur_dist < min_dist) {
          min_dist = cur_dist;
          min_node = node;
        }
        if (node->right != KD_NODE_UNSET) {
          stack[cur++] = node->right;
        }
      }
      if (node->left != KD_NODE_UNSET) {
        stack[cur++] = node->left;
      }
    }
    if (UNLIKELY(cur + DimsNum > stack_len_capacity)) {
      stack = detail::realloc_nodes<DimsNum>(stack, &stack_len_capacity, stack_default != stack);
    }
  }

  if (r_nearest) {
    r_nearest->index = min_node->index;
    r_nearest->dist = sqrtf(min_dist);
    detail::copy_vn_vn<DimsNum>(r_nearest->co, min_node->co);
  }

  if (stack != stack_default) {
    MEM_freeN(stack);
  }

  return min_node->index;
}

/**
 * A version of #kdtree_3d_find_nearest which runs a callback
 * to filter out values.
 *
 * \param filter_cb: Filter find results,
 * Return codes: (1: accept, 0: skip, -1: immediate exit).
 */
template<int DimsNum>
inline int kdtree_find_nearest_cb(
    const KDTree<DimsNum> *tree,
    const float co[DimsNum],
    int (*filter_cb)(void *user_data, int index, const float co[DimsNum], float dist_sq),
    void *user_data,
    KDTreeNearest<DimsNum> *r_nearest)
{
  const KDTreeNode<DimsNum> *nodes = tree->nodes;
  const KDTreeNode<DimsNum> *min_node = nullptr;

  uint *stack, stack_default[KD_STACK_INIT];
  float min_dist = FLT_MAX, cur_dist;
  uint stack_len_capacity, cur = 0;

#ifndef NDEBUG
  BLI_assert(tree->is_balanced == true);
#endif

  if (UNLIKELY(tree->root == KD_NODE_UNSET)) {
    return -1;
  }

  stack = stack_default;
  stack_len_capacity = int(ARRAY_SIZE(stack_default));

  const auto node_test_nearest = [&](const KDTreeNode<DimsNum> *node) -> bool {
    const float dist_sq = detail::len_squared_vnvn<DimsNum>((node)->co, co);
    if (dist_sq >= min_dist) {
      return false;
    }
    const int result = filter_cb(user_data, (node)->index, (node)->co, dist_sq);
    if (result == 1) {
      min_dist = dist_sq;
      min_node = node;
      return false;
    }

    if (result == 0) {
      /* pass */
      return false;
    }

    BLI_assert(result == -1);
    return true;
  };

  stack[cur++] = tree->root;

  while (cur--) {
    const KDTreeNode<DimsNum> *node = &nodes[stack[cur]];

    cur_dist = node->co[node->d] - co[node->d];

    if (cur_dist < 0.0f) {
      cur_dist = -cur_dist * cur_dist;

      if (-cur_dist < min_dist) {
        if (node_test_nearest(node)) {
          break;
        }

        if (node->left != KD_NODE_UNSET) {
          stack[cur++] = node->left;
        }
      }
      if (node->right != KD_NODE_UNSET) {
        stack[cur++] = node->right;
      }
    }
    else {
      cur_dist = cur_dist * cur_dist;

      if (cur_dist < min_dist) {
        if (node_test_nearest(node)) {
          break;
        }

        if (node->right != KD_NODE_UNSET) {
          stack[cur++] = node->right;
        }
      }
      if (node->left != KD_NODE_UNSET) {
        stack[cur++] = node->left;
      }
    }
    if (UNLIKELY(cur + DimsNum > stack_len_capacity)) {
      stack = detail::realloc_nodes<DimsNum>(stack, &stack_len_capacity, stack_default != stack);
    }
  }

  if (stack != stack_default) {
    MEM_freeN(stack);
  }

  if (min_node) {
    if (r_nearest) {
      r_nearest->index = min_node->index;
      r_nearest->dist = sqrtf(min_dist);
      detail::copy_vn_vn<DimsNum>(r_nearest->co, min_node->co);
    }

    return min_node->index;
  }
  return -1;
}

namespace detail {

template<int DimsNum>
static void nearest_ordered_insert(KDTreeNearest<DimsNum> *nearest,
                                   uint *nearest_len,
                                   const uint nearest_len_capacity,
                                   const int index,
                                   const float dist,
                                   const float co[DimsNum])
{
  uint i;

  if (*nearest_len < nearest_len_capacity) {
    (*nearest_len)++;
  }

  for (i = *nearest_len - 1; i > 0; i--) {
    if (dist >= nearest[i - 1].dist) {
      break;
    }
    nearest[i] = nearest[i - 1];
  }

  nearest[i].index = index;
  nearest[i].dist = dist;
  detail::copy_vn_vn<DimsNum>(nearest[i].co, co);
}

}  // namespace detail

/**
 * Find \a nearest_len_capacity nearest returns number of points found, with results in nearest.
 *
 * \param r_nearest: An array of nearest, sized at least \a nearest_len_capacity.
 */
template<int DimsNum>
inline int kdtree_find_nearest_n_with_len_squared_cb(
    const KDTree<DimsNum> *tree,
    const float co[DimsNum],
    KDTreeNearest<DimsNum> r_nearest[],
    const uint nearest_len_capacity,
    float (*len_sq_fn)(const float co_search[DimsNum],
                       const float co_test[DimsNum],
                       const void *user_data),
    const void *user_data)
{
  const KDTreeNode<DimsNum> *nodes = tree->nodes;
  const KDTreeNode<DimsNum> *root;
  uint *stack, stack_default[KD_STACK_INIT];
  float cur_dist;
  uint stack_len_capacity, cur = 0;
  uint i, nearest_len = 0;

#ifndef NDEBUG
  BLI_assert(tree->is_balanced == true);
#endif

  if (UNLIKELY((tree->root == KD_NODE_UNSET) || nearest_len_capacity == 0)) {
    return 0;
  }

  if (len_sq_fn == nullptr) {
    len_sq_fn = detail::len_squared_vnvn_cb<DimsNum>;
    BLI_assert(user_data == nullptr);
  }

  stack = stack_default;
  stack_len_capacity = int(ARRAY_SIZE(stack_default));

  root = &nodes[tree->root];

  cur_dist = len_sq_fn(co, root->co, user_data);
  detail::nearest_ordered_insert<DimsNum>(
      r_nearest, &nearest_len, nearest_len_capacity, root->index, cur_dist, root->co);

  if (co[root->d] < root->co[root->d]) {
    if (root->right != KD_NODE_UNSET) {
      stack[cur++] = root->right;
    }
    if (root->left != KD_NODE_UNSET) {
      stack[cur++] = root->left;
    }
  }
  else {
    if (root->left != KD_NODE_UNSET) {
      stack[cur++] = root->left;
    }
    if (root->right != KD_NODE_UNSET) {
      stack[cur++] = root->right;
    }
  }

  while (cur--) {
    const KDTreeNode<DimsNum> *node = &nodes[stack[cur]];

    cur_dist = node->co[node->d] - co[node->d];

    if (cur_dist < 0.0f) {
      cur_dist = -cur_dist * cur_dist;

      if (nearest_len < nearest_len_capacity || -cur_dist < r_nearest[nearest_len - 1].dist) {
        cur_dist = len_sq_fn(co, node->co, user_data);

        if (nearest_len < nearest_len_capacity || cur_dist < r_nearest[nearest_len - 1].dist) {
          detail::nearest_ordered_insert<DimsNum>(
              r_nearest, &nearest_len, nearest_len_capacity, node->index, cur_dist, node->co);
        }

        if (node->left != KD_NODE_UNSET) {
          stack[cur++] = node->left;
        }
      }
      if (node->right != KD_NODE_UNSET) {
        stack[cur++] = node->right;
      }
    }
    else {
      cur_dist = cur_dist * cur_dist;

      if (nearest_len < nearest_len_capacity || cur_dist < r_nearest[nearest_len - 1].dist) {
        cur_dist = len_sq_fn(co, node->co, user_data);
        if (nearest_len < nearest_len_capacity || cur_dist < r_nearest[nearest_len - 1].dist) {
          detail::nearest_ordered_insert<DimsNum>(
              r_nearest, &nearest_len, nearest_len_capacity, node->index, cur_dist, node->co);
        }

        if (node->right != KD_NODE_UNSET) {
          stack[cur++] = node->right;
        }
      }
      if (node->left != KD_NODE_UNSET) {
        stack[cur++] = node->left;
      }
    }
    if (UNLIKELY(cur + DimsNum > stack_len_capacity)) {
      stack = detail::realloc_nodes<DimsNum>(stack, &stack_len_capacity, stack_default != stack);
    }
  }

  for (i = 0; i < nearest_len; i++) {
    r_nearest[i].dist = sqrtf(r_nearest[i].dist);
  }

  if (stack != stack_default) {
    MEM_freeN(stack);
  }

  return (int)nearest_len;
}

template<int DimsNum>
inline int kdtree_find_nearest_n(const KDTree<DimsNum> *tree,
                                 const float co[DimsNum],
                                 KDTreeNearest<DimsNum> r_nearest[],
                                 uint nearest_len_capacity)
{
  return kdtree_find_nearest_n_with_len_squared_cb<DimsNum>(
      tree, co, r_nearest, nearest_len_capacity, nullptr, nullptr);
}

namespace detail {

template<int DimsNum> static int nearest_cmp_dist(const void *a, const void *b)
{
  const KDTreeNearest<DimsNum> *kda = static_cast<const KDTreeNearest<DimsNum> *>(a);
  const KDTreeNearest<DimsNum> *kdb = static_cast<const KDTreeNearest<DimsNum> *>(b);

  if (kda->dist < kdb->dist) {
    return -1;
  }
  if (kda->dist > kdb->dist) {
    return 1;
  }
  return 0;
}

template<int DimsNum>
static void nearest_add_in_range(KDTreeNearest<DimsNum> **r_nearest,
                                 uint nearest_index,
                                 uint *nearest_len_capacity,
                                 const int index,
                                 const float dist,
                                 const float co[DimsNum])
{
  KDTreeNearest<DimsNum> *to;

  if (UNLIKELY(nearest_index >= *nearest_len_capacity)) {
    *r_nearest = static_cast<KDTreeNearest<DimsNum> *>(MEM_reallocN_id(
        *r_nearest,
        (*nearest_len_capacity += KD_FOUND_ALLOC_INC) * sizeof(KDTreeNode<DimsNum>),
        __func__));
  }

  to = (*r_nearest) + nearest_index;

  to->index = index;
  to->dist = sqrtf(dist);
  detail::copy_vn_vn<DimsNum>(to->co, co);
}

}  // namespace detail

/**
 * Range search returns number of points nearest_len, with results in nearest
 *
 * \param r_nearest: Allocated array of nearest nearest_len (caller is responsible for freeing).
 */
template<int DimsNum>
inline int kdtree_range_search_with_len_squared_cb(
    const KDTree<DimsNum> *tree,
    const float co[DimsNum],
    KDTreeNearest<DimsNum> **r_nearest,
    const float range,
    float (*len_sq_fn)(const float co_search[DimsNum],
                       const float co_test[DimsNum],
                       const void *user_data),
    const void *user_data)
{
  const KDTreeNode<DimsNum> *nodes = tree->nodes;
  uint *stack, stack_default[KD_STACK_INIT];
  KDTreeNearest<DimsNum> *nearest = nullptr;
  const float range_sq = range * range;
  float dist_sq;
  uint stack_len_capacity, cur = 0;
  uint nearest_len = 0, nearest_len_capacity = 0;

#ifndef NDEBUG
  BLI_assert(tree->is_balanced == true);
#endif

  if (UNLIKELY(tree->root == KD_NODE_UNSET)) {
    return 0;
  }

  if (len_sq_fn == nullptr) {
    len_sq_fn = detail::len_squared_vnvn_cb<DimsNum>;
    BLI_assert(user_data == nullptr);
  }

  stack = stack_default;
  stack_len_capacity = int(ARRAY_SIZE(stack_default));

  stack[cur++] = tree->root;

  while (cur--) {
    const KDTreeNode<DimsNum> *node = &nodes[stack[cur]];

    if (co[node->d] + range < node->co[node->d]) {
      if (node->left != KD_NODE_UNSET) {
        stack[cur++] = node->left;
      }
    }
    else if (co[node->d] - range > node->co[node->d]) {
      if (node->right != KD_NODE_UNSET) {
        stack[cur++] = node->right;
      }
    }
    else {
      dist_sq = len_sq_fn(co, node->co, user_data);
      if (dist_sq <= range_sq) {
        detail::nearest_add_in_range<DimsNum>(
            &nearest, nearest_len++, &nearest_len_capacity, node->index, dist_sq, node->co);
      }

      if (node->left != KD_NODE_UNSET) {
        stack[cur++] = node->left;
      }
      if (node->right != KD_NODE_UNSET) {
        stack[cur++] = node->right;
      }
    }

    if (UNLIKELY(cur + DimsNum > stack_len_capacity)) {
      stack = detail::realloc_nodes<DimsNum>(stack, &stack_len_capacity, stack_default != stack);
    }
  }

  if (stack != stack_default) {
    MEM_freeN(stack);
  }

  if (nearest_len) {
    qsort(nearest, nearest_len, sizeof(KDTreeNearest<DimsNum>), detail::nearest_cmp_dist<DimsNum>);
  }

  *r_nearest = nearest;

  return (int)nearest_len;
}

template<int DimsNum>
inline int kdtree_range_search(const KDTree<DimsNum> *tree,
                               const float co[DimsNum],
                               KDTreeNearest<DimsNum> **r_nearest,
                               float range)
{
  return kdtree_range_search_with_len_squared_cb<DimsNum>(
      tree, co, r_nearest, range, nullptr, nullptr);
}

/**
 * A version of #kdtree_3d_range_search which runs a callback
 * instead of allocating an array.
 *
 * \param search_cb: Called for every node found in \a range,
 * false return value performs an early exit.
 *
 * \note the order of calls isn't sorted based on distance.
 */
template<int DimsNum>
inline void kdtree_range_search_cb(
    const KDTree<DimsNum> *tree,
    const float co[DimsNum],
    float range,
    bool (*search_cb)(void *user_data, int index, const float co[DimsNum], float dist_sq),
    void *user_data)
{
  const KDTreeNode<DimsNum> *nodes = tree->nodes;

  uint *stack, stack_default[KD_STACK_INIT];
  float range_sq = range * range, dist_sq;
  uint stack_len_capacity, cur = 0;

#ifndef NDEBUG
  BLI_assert(tree->is_balanced == true);
#endif

  if (UNLIKELY(tree->root == KD_NODE_UNSET)) {
    return;
  }

  stack = stack_default;
  stack_len_capacity = int(ARRAY_SIZE(stack_default));

  stack[cur++] = tree->root;

  while (cur--) {
    const KDTreeNode<DimsNum> *node = &nodes[stack[cur]];

    if (co[node->d] + range < node->co[node->d]) {
      if (node->left != KD_NODE_UNSET) {
        stack[cur++] = node->left;
      }
    }
    else if (co[node->d] - range > node->co[node->d]) {
      if (node->right != KD_NODE_UNSET) {
        stack[cur++] = node->right;
      }
    }
    else {
      dist_sq = detail::len_squared_vnvn<DimsNum>(node->co, co);
      if (dist_sq <= range_sq) {
        if (search_cb(user_data, node->index, node->co, dist_sq) == false) {
          goto finally;
        }
      }

      if (node->left != KD_NODE_UNSET) {
        stack[cur++] = node->left;
      }
      if (node->right != KD_NODE_UNSET) {
        stack[cur++] = node->right;
      }
    }

    if (UNLIKELY(cur + DimsNum > stack_len_capacity)) {
      stack = detail::realloc_nodes<DimsNum>(stack, &stack_len_capacity, stack_default != stack);
    }
  }

finally:
  if (stack != stack_default) {
    MEM_freeN(stack);
  }
}

namespace detail {

/**
 * Use when we want to loop over nodes ordered by index.
 * Requires indices to be aligned with nodes.
 */
template<int DimsNum> static blender::Vector<int> kdtree_order(const KDTree<DimsNum> *tree)
{
  const KDTreeNode<DimsNum> *nodes = tree->nodes;
  blender::Vector<int> order(tree->max_node_index + 1, -1);
  for (uint i = 0; i < tree->nodes_len; i++) {
    order[nodes[i].index] = (int)i;
  }
  return order;
}

/* -------------------------------------------------------------------- */
/** \name kdtree_3d_calc_duplicates_fast
 * \{ */

template<int DimsNum> struct DeDuplicateParams {
  /* Static */
  const KDTreeNode<DimsNum> *nodes;
  float range;
  float range_sq;
  int *duplicates;
  int *duplicates_found;

  /* Per Search */
  float search_co[DimsNum];
  int search;
};

template<int DimsNum>
static void deduplicate_recursive(const DeDuplicateParams<DimsNum> *p, uint i)
{
  const KDTreeNode<DimsNum> *node = &p->nodes[i];
  if (p->search_co[node->d] + p->range <= node->co[node->d]) {
    if (node->left != KD_NODE_UNSET) {
      deduplicate_recursive(p, node->left);
    }
  }
  else if (p->search_co[node->d] - p->range >= node->co[node->d]) {
    if (node->right != KD_NODE_UNSET) {
      deduplicate_recursive(p, node->right);
    }
  }
  else {
    if ((p->search != node->index) && (p->duplicates[node->index] == -1)) {
      if (detail::len_squared_vnvn<DimsNum>(node->co, p->search_co) <= p->range_sq) {
        p->duplicates[node->index] = (int)p->search;
        *p->duplicates_found += 1;
      }
    }
    if (node->left != KD_NODE_UNSET) {
      deduplicate_recursive(p, node->left);
    }
    if (node->right != KD_NODE_UNSET) {
      deduplicate_recursive(p, node->right);
    }
  }
}

}  // namespace detail

/**
 * Find duplicate points in \a range.
 * Favors speed over quality since it doesn't find the best target vertex for merging.
 * Nodes are looped over, duplicates are added when found.
 * Nevertheless results are predictable.
 *
 * \param range: Coordinates in this range are candidates to be merged.
 * \param use_index_order: Loop over the coordinates ordered by #KDTreeNode.index
 * At the expense of some performance, this ensures the layout of the tree doesn't influence
 * the iteration order.
 * \param duplicates: An array of int's the length of #KDTree.nodes_len
 * Values initialized to -1 are candidates to me merged.
 * Setting the index to its own position in the array prevents it from being touched,
 * although it can still be used as a target.
 * \returns The number of merges found (includes any merges already in the \a duplicates array).
 *
 * \note Merging is always a single step (target indices won't be marked for merging).
 */
template<int DimsNum>
inline int kdtree_calc_duplicates_fast(const KDTree<DimsNum> *tree,
                                       const float range,
                                       const bool use_index_order,
                                       int *duplicates)
{
  int found = 0;

  detail::DeDuplicateParams<DimsNum> p = {};
  p.nodes = tree->nodes;
  p.range = range;
  p.range_sq = square_f(range);
  p.duplicates = duplicates;
  p.duplicates_found = &found;

  if (use_index_order) {
    blender::Vector<int> order = detail::kdtree_order<DimsNum>(tree);
    for (int i = 0; i < tree->max_node_index + 1; i++) {
      const int node_index = order[i];
      if (node_index == -1) {
        continue;
      }
      const int index = i;
      if (ELEM(duplicates[index], -1, index)) {
        p.search = index;
        detail::copy_vn_vn<DimsNum>(p.search_co, tree->nodes[node_index].co);
        int found_prev = found;
        detail::deduplicate_recursive<DimsNum>(&p, tree->root);
        if (found != found_prev) {
          /* Prevent chains of doubles. */
          duplicates[index] = index;
        }
      }
    }
  }
  else {
    for (uint i = 0; i < tree->nodes_len; i++) {
      const uint node_index = i;
      const int index = p.nodes[node_index].index;
      if (ELEM(duplicates[index], -1, index)) {
        p.search = index;
        detail::copy_vn_vn<DimsNum>(p.search_co, tree->nodes[node_index].co);
        int found_prev = found;
        detail::deduplicate_recursive<DimsNum>(&p, tree->root);
        if (found != found_prev) {
          /* Prevent chains of doubles. */
          duplicates[index] = index;
        }
      }
    }
  }
  return found;
}

/** \} */

template<int DimsNum, typename Fn>
inline void kdtree_range_search_cb_cpp(const KDTree<DimsNum> *tree,
                                       const float co[DimsNum],
                                       const float distance,
                                       const Fn &fn)
{
  kdtree_range_search_cb<DimsNum>(
      tree,
      co,
      distance,
      [](void *user_data, const int index, const float *co, const float dist_sq) {
        const Fn &fn = *static_cast<const Fn *>(user_data);
        return fn(index, co, dist_sq);
      },
      const_cast<Fn *>(&fn));
}

/* -------------------------------------------------------------------- */
/** \name kdtree_3d_calc_duplicates_cb
 * \{ */

/**
 * De-duplicate utility where the callback can evaluate duplicates and select the target
 * which other indices are merged into.
 *
 * \param tree: A tree, all indices *must* be unique.
 * \param has_self_index: When true, account for indices
 * in the `duplicates` array that reference themselves,
 * prioritizing them as targets before de-duplicating the remainder with each other.
 * \param deduplicate_cb: A function which receives duplicate indices,
 * it must choose the "target" index to keep which is returned.
 * The return value is an index in the `cluster` array (a value from `0..cluster_num`).
 * The last item in `cluster` is the index from which the search began.
 *
 * \note ~1.1x-1.5x slower than `calc_duplicates_fast` depending on the distribution of points.
 *
 * \note The duplicate search is performed in an order defined by the tree-nodes index,
 * the index of the input (first to last) for predictability.
 */
template<int DimsNum>
inline int kdtree_calc_duplicates_cb(const KDTree<DimsNum> *tree,
                                     const float range,
                                     int *duplicates,
                                     const bool has_self_index,
                                     int (*duplicates_cb)(void *user_data,
                                                          const int *cluster,
                                                          int cluster_num),
                                     void *user_data)
{
  BLI_assert(tree->is_balanced);
  if (UNLIKELY(tree->root == KD_NODE_UNSET)) {
    return 0;
  }

  /* Use `index_to_node_index` so coordinates are looked up in order first to last. */
  const uint nodes_len = tree->nodes_len;
  blender::Array<int> index_to_node_index(tree->max_node_index + 1);
  for (uint i = 0; i < nodes_len; i++) {
    index_to_node_index[tree->nodes[i].index] = int(i);
  }

  int found = 0;

  /* First pass, handle merging into self-index (if any exist). */
  if (has_self_index) {
    blender::Array<float> duplicates_dist_sq(tree->max_node_index + 1);
    for (uint i = 0; i < nodes_len; i++) {
      const int node_index = tree->nodes[i].index;
      if (node_index != duplicates[node_index]) {
        continue;
      }
      const float *search_co = tree->nodes[index_to_node_index[node_index]].co;
      auto accumulate_neighbors_fn =
          [&duplicates, &node_index, &duplicates_dist_sq, &found](
              int neighbor_index, const float * /*co*/, const float dist_sq) -> bool {
        const int target_index = duplicates[neighbor_index];
        if (target_index == -1) {
          duplicates[neighbor_index] = node_index;
          duplicates_dist_sq[neighbor_index] = dist_sq;
          found += 1;
        }
        /* Don't steal from self references. */
        else if (target_index != neighbor_index) {
          float &dist_sq_best = duplicates_dist_sq[neighbor_index];
          /* Steal the target if it's closer. */
          if ((dist_sq < dist_sq_best) ||
              /* Pick the lowest index as a tie breaker for a deterministic result. */
              ((dist_sq == dist_sq_best) && (node_index < target_index)))
          {
            dist_sq_best = dist_sq;
            duplicates[neighbor_index] = node_index;
          }
        }
        return true;
      };

      kdtree_range_search_cb_cpp<DimsNum>(tree, search_co, range, accumulate_neighbors_fn);
    }
  }

  /* Second pass, de-duplicate clusters that weren't handled in the first pass. */

  /* Could be inline, declare here to avoid re-allocation. */
  blender::Vector<int> cluster;
  for (uint i = 0; i < nodes_len; i++) {
    const int node_index = tree->nodes[i].index;
    if (duplicates[node_index] != -1) {
      continue;
    }

    BLI_assert(cluster.is_empty());
    const float *search_co = tree->nodes[index_to_node_index[node_index]].co;
    auto accumulate_neighbors_fn = [&duplicates, &cluster](int neighbor_index,
                                                           const float * /*co*/,
                                                           const float /*dist_sq*/) -> bool {
      if (duplicates[neighbor_index] == -1) {
        cluster.append(neighbor_index);
      }
      return true;
    };

    kdtree_range_search_cb_cpp<DimsNum>(tree, search_co, range, accumulate_neighbors_fn);
    if (cluster.is_empty()) {
      continue;
    }
    found += int(cluster.size());
    cluster.append(node_index);

    const int cluster_index = duplicates_cb(user_data, cluster.data(), int(cluster.size()));
    BLI_assert(uint(cluster_index) < uint(cluster.size()));
    const int target_index = cluster[cluster_index];
    for (const int cluster_node_index : cluster) {
      duplicates[cluster_node_index] = target_index;
    }
    cluster.clear();
  }

  return found;
}

/** \} */

template<int DimsNum, typename Fn>
inline int kdtree_find_nearest_cb_cpp(const KDTree<DimsNum> *tree,
                                      const float co[DimsNum],
                                      KDTreeNearest<DimsNum> *r_nearest,
                                      Fn &&fn)
{
  return kdtree_find_nearest_cb<DimsNum>(
      tree,
      co,
      [](void *user_data, const int index, const float *co, const float dist_sq) {
        Fn &fn = *static_cast<Fn *>(user_data);
        return fn(index, co, dist_sq);
      },
      &fn,
      r_nearest);
}

template<int DimsNum, typename Fn>
inline int kdtree_calc_duplicates_cb_cpp(const KDTree<DimsNum> *tree,
                                         const float distance,
                                         int *duplicates,
                                         const bool has_self_index,
                                         const Fn &fn)
{
  return kdtree_calc_duplicates_cb<DimsNum>(
      tree,
      distance,
      duplicates,
      has_self_index,
      [](void *user_data, const int *cluster, int cluster_num) -> int {
        const Fn &fn = *static_cast<const Fn *>(user_data);
        return fn(cluster, cluster_num);
      },
      const_cast<Fn *>(&fn));
}

/* -------------------------------------------------------------------- */
/** \name kdtree_3d_deduplicate
 * \{ */

namespace detail {

template<int DimsNum> static int kdtree_cmp_bool(const bool a, const bool b)
{
  if (a == b) {
    return 0;
  }
  return b ? -1 : 1;
}

template<int DimsNum> static int kdtree_node_cmp_deduplicate(const void *n0_p, const void *n1_p)
{
  const KDTreeNode<DimsNum> *n0 = static_cast<const KDTreeNode<DimsNum> *>(n0_p);
  const KDTreeNode<DimsNum> *n1 = static_cast<const KDTreeNode<DimsNum> *>(n1_p);
  for (uint j = 0; j < DimsNum; j++) {
    if (n0->co[j] < n1->co[j]) {
      return -1;
    }
    if (n0->co[j] > n1->co[j]) {
      return 1;
    }
  }

  if (n0->d != DimsNum && n1->d != DimsNum) {
    /* Two nodes share identical `co`
     * Both are still valid.
     * Cast away `const` and tag one of them as invalid. */
    ((KDTreeNode<DimsNum> *)n1)->d = DimsNum;
  }

  /* Keep sorting until each unique value has one and only one valid node. */
  return kdtree_cmp_bool<DimsNum>(n0->d == DimsNum, n1->d == DimsNum);
}

}  // namespace detail

/**
 * Remove exact duplicates (run before balancing).
 *
 * Keep the first element added when duplicates are found.
 */
template<int DimsNum> inline int kdtree_deduplicate(KDTree<DimsNum> *tree)
{
#ifndef NDEBUG
  tree->is_balanced = false;
#endif
  qsort(tree->nodes,
        (size_t)tree->nodes_len,
        sizeof(*tree->nodes),
        detail::kdtree_node_cmp_deduplicate<DimsNum>);
  uint j = 0;
  for (uint i = 0; i < tree->nodes_len; i++) {
    if (tree->nodes[i].d != DimsNum) {
      if (i != j) {
        tree->nodes[j] = tree->nodes[i];
      }
      j++;
    }
  }
  tree->nodes_len = j;
  return (int)tree->nodes_len;
}

/** \} */

#undef KD_STACK_INIT
#undef KD_NEAR_ALLOC_INC
#undef KD_FOUND_ALLOC_INC
#undef KD_NODE_UNSET
#undef KD_NODE_ROOT_IS_INIT

}  //  namespace blender

namespace blender {

constexpr inline auto kdtree_1d_new = kdtree_new<1>;
constexpr inline auto kdtree_2d_new = kdtree_new<2>;
constexpr inline auto kdtree_3d_new = kdtree_new<3>;
constexpr inline auto kdtree_4d_new = kdtree_new<4>;

constexpr inline auto kdtree_1d_free = kdtree_free<1>;
constexpr inline auto kdtree_2d_free = kdtree_free<2>;
constexpr inline auto kdtree_3d_free = kdtree_free<3>;
constexpr inline auto kdtree_4d_free = kdtree_free<4>;

constexpr inline auto kdtree_1d_balance = kdtree_balance<1>;
constexpr inline auto kdtree_2d_balance = kdtree_balance<2>;
constexpr inline auto kdtree_3d_balance = kdtree_balance<3>;
constexpr inline auto kdtree_4d_balance = kdtree_balance<4>;

constexpr inline auto kdtree_1d_insert = kdtree_insert<1>;
constexpr inline auto kdtree_2d_insert = kdtree_insert<2>;
constexpr inline auto kdtree_3d_insert = kdtree_insert<3>;
constexpr inline auto kdtree_4d_insert = kdtree_insert<4>;

constexpr inline auto kdtree_1d_find_nearest = kdtree_find_nearest<1>;
constexpr inline auto kdtree_2d_find_nearest = kdtree_find_nearest<2>;
constexpr inline auto kdtree_3d_find_nearest = kdtree_find_nearest<3>;
constexpr inline auto kdtree_4d_find_nearest = kdtree_find_nearest<4>;

constexpr inline auto kdtree_1d_find_nearest_n = kdtree_find_nearest_n<1>;
constexpr inline auto kdtree_2d_find_nearest_n = kdtree_find_nearest_n<2>;
constexpr inline auto kdtree_3d_find_nearest_n = kdtree_find_nearest_n<3>;
constexpr inline auto kdtree_4d_find_nearest_n = kdtree_find_nearest_n<4>;

constexpr inline auto kdtree_1d_range_search = kdtree_range_search<1>;
constexpr inline auto kdtree_2d_range_search = kdtree_range_search<2>;
constexpr inline auto kdtree_3d_range_search = kdtree_range_search<3>;
constexpr inline auto kdtree_4d_range_search = kdtree_range_search<4>;

constexpr inline auto kdtree_1d_find_nearest_cb = kdtree_find_nearest_cb<1>;
constexpr inline auto kdtree_2d_find_nearest_cb = kdtree_find_nearest_cb<2>;
constexpr inline auto kdtree_3d_find_nearest_cb = kdtree_find_nearest_cb<3>;
constexpr inline auto kdtree_4d_find_nearest_cb = kdtree_find_nearest_cb<4>;

constexpr inline auto kdtree_1d_range_search_cb = kdtree_range_search_cb<1>;
constexpr inline auto kdtree_2d_range_search_cb = kdtree_range_search_cb<2>;
constexpr inline auto kdtree_3d_range_search_cb = kdtree_range_search_cb<3>;
constexpr inline auto kdtree_4d_range_search_cb = kdtree_range_search_cb<4>;

constexpr inline auto kdtree_1d_calc_duplicates_fast = kdtree_calc_duplicates_fast<1>;
constexpr inline auto kdtree_2d_calc_duplicates_fast = kdtree_calc_duplicates_fast<2>;
constexpr inline auto kdtree_3d_calc_duplicates_fast = kdtree_calc_duplicates_fast<3>;
constexpr inline auto kdtree_4d_calc_duplicates_fast = kdtree_calc_duplicates_fast<4>;

constexpr inline auto kdtree_1d_calc_duplicates_cb = kdtree_calc_duplicates_cb<1>;
constexpr inline auto kdtree_2d_calc_duplicates_cb = kdtree_calc_duplicates_cb<2>;
constexpr inline auto kdtree_3d_calc_duplicates_cb = kdtree_calc_duplicates_cb<3>;
constexpr inline auto kdtree_4d_calc_duplicates_cb = kdtree_calc_duplicates_cb<4>;

constexpr inline auto kdtree_1d_deduplicate = kdtree_deduplicate<1>;
constexpr inline auto kdtree_2d_deduplicate = kdtree_deduplicate<2>;
constexpr inline auto kdtree_3d_deduplicate = kdtree_deduplicate<3>;
constexpr inline auto kdtree_4d_deduplicate = kdtree_deduplicate<4>;

constexpr inline auto kdtree_1d_find_nearest_n_with_len_squared_cb =
    kdtree_find_nearest_n_with_len_squared_cb<1>;
constexpr inline auto kdtree_2d_find_nearest_n_with_len_squared_cb =
    kdtree_find_nearest_n_with_len_squared_cb<2>;
constexpr inline auto kdtree_3d_find_nearest_n_with_len_squared_cb =
    kdtree_find_nearest_n_with_len_squared_cb<3>;
constexpr inline auto kdtree_4d_find_nearest_n_with_len_squared_cb =
    kdtree_find_nearest_n_with_len_squared_cb<4>;

constexpr inline auto kdtree_1d_range_search_with_len_squared_cb =
    kdtree_range_search_with_len_squared_cb<1>;
constexpr inline auto kdtree_2d_range_search_with_len_squared_cb =
    kdtree_range_search_with_len_squared_cb<2>;
constexpr inline auto kdtree_3d_range_search_with_len_squared_cb =
    kdtree_range_search_with_len_squared_cb<3>;
constexpr inline auto kdtree_4d_range_search_with_len_squared_cb =
    kdtree_range_search_with_len_squared_cb<4>;

}  // namespace blender
