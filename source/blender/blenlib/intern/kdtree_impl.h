/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_kdtree_impl.h"
#include "BLI_math_base.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include <algorithm>
#include <cstring>

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

#define _BLI_KDTREE_CONCAT_AUX(MACRO_ARG1, MACRO_ARG2) MACRO_ARG1##MACRO_ARG2
#define _BLI_KDTREE_CONCAT(MACRO_ARG1, MACRO_ARG2) _BLI_KDTREE_CONCAT_AUX(MACRO_ARG1, MACRO_ARG2)
#define BLI_kdtree_nd_(id) _BLI_KDTREE_CONCAT(KDTREE_PREFIX_ID, _##id)

/* All these struct names are #defines with unique names, to avoid violating the one definition
 * rule. Otherwise `MEM_malloc_array<KDTreeNode>` can get defined once for multiple dimensions,
 * with different node sizes. */

struct KDTreeNode_head {
  uint left, right;
  float co[KD_DIMS];
  int index;
};

struct KDTreeNode {
  uint left, right;
  float co[KD_DIMS];
  int index;
  uint d; /* range is only (0..KD_DIMS - 1) */
};

struct KDTree {
  KDTreeNode *nodes;
  uint nodes_len;
  uint root;
  int max_node_index;
#ifndef NDEBUG
  bool is_balanced;        /* ensure we call balance first */
  uint nodes_len_capacity; /* max size of the tree */
#endif
};

#define KD_STACK_INIT 100     /* initial size for array (on the stack) */
#define KD_NEAR_ALLOC_INC 100 /* alloc increment for collecting nearest */
#define KD_FOUND_ALLOC_INC 50 /* alloc increment for collecting nearest */

#define KD_NODE_UNSET ((uint) - 1)

/**
 * When set we know all values are unbalanced,
 * otherwise clear them when re-balancing: see #62210.
 */
#define KD_NODE_ROOT_IS_INIT ((uint) - 2)

/* -------------------------------------------------------------------- */
/** \name Local Math API
 * \{ */

static void copy_vn_vn(float v0[KD_DIMS], const float v1[KD_DIMS])
{
  for (uint j = 0; j < KD_DIMS; j++) {
    v0[j] = v1[j];
  }
}

static float len_squared_vnvn(const float v0[KD_DIMS], const float v1[KD_DIMS])
{
  float d = 0.0f;
  for (uint j = 0; j < KD_DIMS; j++) {
    d += square_f(v0[j] - v1[j]);
  }
  return d;
}

static float len_squared_vnvn_cb(const float co_kdtree[KD_DIMS],
                                 const float co_search[KD_DIMS],
                                 const void * /*user_data*/)
{
  return len_squared_vnvn(co_kdtree, co_search);
}

/** \} */

/**
 * Creates or free a kdtree
 */
KDTree *BLI_kdtree_nd_(new)(uint nodes_len_capacity)
{
  KDTree *tree;

  tree = MEM_callocN<KDTree>("KDTree");
  tree->nodes = MEM_malloc_arrayN<KDTreeNode>(nodes_len_capacity, "KDTreeNode");
  tree->nodes_len = 0;
  tree->root = KD_NODE_ROOT_IS_INIT;
  tree->max_node_index = -1;

#ifndef NDEBUG
  tree->is_balanced = false;
  tree->nodes_len_capacity = nodes_len_capacity;
#endif

  return tree;
}

void BLI_kdtree_nd_(free)(KDTree *tree)
{
  if (tree) {
    MEM_freeN(tree->nodes);
    MEM_freeN(tree);
  }
}

/**
 * Construction: first insert points, then call balance. Normal is optional.
 */
void BLI_kdtree_nd_(insert)(KDTree *tree, int index, const float co[KD_DIMS])
{
  KDTreeNode *node = &tree->nodes[tree->nodes_len++];

#ifndef NDEBUG
  BLI_assert(tree->nodes_len <= tree->nodes_len_capacity);
#endif

  /* NOTE: array isn't calloc'd,
   * need to initialize all struct members */

  node->left = node->right = KD_NODE_UNSET;
  copy_vn_vn(node->co, co);
  node->index = index;
  node->d = 0;
  tree->max_node_index = std::max(tree->max_node_index, index);

#ifndef NDEBUG
  tree->is_balanced = false;
#endif
}

static uint kdtree_balance(KDTreeNode *nodes, uint nodes_len, uint axis, const uint ofs)
{
  KDTreeNode *node;
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

      SWAP(KDTreeNode_head, *(KDTreeNode_head *)&nodes[i], *(KDTreeNode_head *)&nodes[j]);
    }

    SWAP(KDTreeNode_head, *(KDTreeNode_head *)&nodes[i], *(KDTreeNode_head *)&nodes[right]);
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
  axis = (axis + 1) % KD_DIMS;
  node->left = kdtree_balance(nodes, median, axis, ofs);
  node->right = kdtree_balance(
      nodes + median + 1, (nodes_len - (median + 1)), axis, (median + 1) + ofs);

  return median + ofs;
}

void BLI_kdtree_nd_(balance)(KDTree *tree)
{
  if (tree->root != KD_NODE_ROOT_IS_INIT) {
    for (uint i = 0; i < tree->nodes_len; i++) {
      tree->nodes[i].left = KD_NODE_UNSET;
      tree->nodes[i].right = KD_NODE_UNSET;
    }
  }

  tree->root = kdtree_balance(tree->nodes, tree->nodes_len, 0, 0);

#ifndef NDEBUG
  tree->is_balanced = true;
#endif
}

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

/**
 * Find nearest returns index, and -1 if no node is found.
 */
int BLI_kdtree_nd_(find_nearest)(const KDTree *tree,
                                 const float co[KD_DIMS],
                                 KDTreeNearest *r_nearest)
{
  const KDTreeNode *nodes = tree->nodes;
  const KDTreeNode *root, *min_node;
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
  min_dist = len_squared_vnvn(root->co, co);

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
    const KDTreeNode *node = &nodes[stack[cur]];

    cur_dist = node->co[node->d] - co[node->d];

    if (cur_dist < 0.0f) {
      cur_dist = -cur_dist * cur_dist;

      if (-cur_dist < min_dist) {
        cur_dist = len_squared_vnvn(node->co, co);
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
        cur_dist = len_squared_vnvn(node->co, co);
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
    if (UNLIKELY(cur + KD_DIMS > stack_len_capacity)) {
      stack = realloc_nodes(stack, &stack_len_capacity, stack_default != stack);
    }
  }

  if (r_nearest) {
    r_nearest->index = min_node->index;
    r_nearest->dist = sqrtf(min_dist);
    copy_vn_vn(r_nearest->co, min_node->co);
  }

  if (stack != stack_default) {
    MEM_freeN(stack);
  }

  return min_node->index;
}

int BLI_kdtree_nd_(find_nearest_cb)(
    const KDTree *tree,
    const float co[KD_DIMS],
    int (*filter_cb)(void *user_data, int index, const float co[KD_DIMS], float dist_sq),
    void *user_data,
    KDTreeNearest *r_nearest)
{
  const KDTreeNode *nodes = tree->nodes;
  const KDTreeNode *min_node = nullptr;

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

#define NODE_TEST_NEAREST(node) \
  { \
    const float dist_sq = len_squared_vnvn((node)->co, co); \
    if (dist_sq < min_dist) { \
      const int result = filter_cb(user_data, (node)->index, (node)->co, dist_sq); \
      if (result == 1) { \
        min_dist = dist_sq; \
        min_node = node; \
      } \
      else if (result == 0) { \
        /* pass */ \
      } \
      else { \
        BLI_assert(result == -1); \
        goto finally; \
      } \
    } \
  } \
  ((void)0)

  stack[cur++] = tree->root;

  while (cur--) {
    const KDTreeNode *node = &nodes[stack[cur]];

    cur_dist = node->co[node->d] - co[node->d];

    if (cur_dist < 0.0f) {
      cur_dist = -cur_dist * cur_dist;

      if (-cur_dist < min_dist) {
        NODE_TEST_NEAREST(node);

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
        NODE_TEST_NEAREST(node);

        if (node->right != KD_NODE_UNSET) {
          stack[cur++] = node->right;
        }
      }
      if (node->left != KD_NODE_UNSET) {
        stack[cur++] = node->left;
      }
    }
    if (UNLIKELY(cur + KD_DIMS > stack_len_capacity)) {
      stack = realloc_nodes(stack, &stack_len_capacity, stack_default != stack);
    }
  }

#undef NODE_TEST_NEAREST

finally:
  if (stack != stack_default) {
    MEM_freeN(stack);
  }

  if (min_node) {
    if (r_nearest) {
      r_nearest->index = min_node->index;
      r_nearest->dist = sqrtf(min_dist);
      copy_vn_vn(r_nearest->co, min_node->co);
    }

    return min_node->index;
  }
  return -1;
}

static void nearest_ordered_insert(KDTreeNearest *nearest,
                                   uint *nearest_len,
                                   const uint nearest_len_capacity,
                                   const int index,
                                   const float dist,
                                   const float co[KD_DIMS])
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
  copy_vn_vn(nearest[i].co, co);
}

int BLI_kdtree_nd_(find_nearest_n_with_len_squared_cb)(
    const KDTree *tree,
    const float co[KD_DIMS],
    KDTreeNearest r_nearest[],
    const uint nearest_len_capacity,
    float (*len_sq_fn)(const float co_search[KD_DIMS],
                       const float co_test[KD_DIMS],
                       const void *user_data),
    const void *user_data)
{
  const KDTreeNode *nodes = tree->nodes;
  const KDTreeNode *root;
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
    len_sq_fn = len_squared_vnvn_cb;
    BLI_assert(user_data == nullptr);
  }

  stack = stack_default;
  stack_len_capacity = int(ARRAY_SIZE(stack_default));

  root = &nodes[tree->root];

  cur_dist = len_sq_fn(co, root->co, user_data);
  nearest_ordered_insert(
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
    const KDTreeNode *node = &nodes[stack[cur]];

    cur_dist = node->co[node->d] - co[node->d];

    if (cur_dist < 0.0f) {
      cur_dist = -cur_dist * cur_dist;

      if (nearest_len < nearest_len_capacity || -cur_dist < r_nearest[nearest_len - 1].dist) {
        cur_dist = len_sq_fn(co, node->co, user_data);

        if (nearest_len < nearest_len_capacity || cur_dist < r_nearest[nearest_len - 1].dist) {
          nearest_ordered_insert(
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
          nearest_ordered_insert(
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
    if (UNLIKELY(cur + KD_DIMS > stack_len_capacity)) {
      stack = realloc_nodes(stack, &stack_len_capacity, stack_default != stack);
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

int BLI_kdtree_nd_(find_nearest_n)(const KDTree *tree,
                                   const float co[KD_DIMS],
                                   KDTreeNearest r_nearest[],
                                   uint nearest_len_capacity)
{
  return BLI_kdtree_nd_(find_nearest_n_with_len_squared_cb)(
      tree, co, r_nearest, nearest_len_capacity, nullptr, nullptr);
}

static int nearest_cmp_dist(const void *a, const void *b)
{
  const KDTreeNearest *kda = static_cast<const KDTreeNearest *>(a);
  const KDTreeNearest *kdb = static_cast<const KDTreeNearest *>(b);

  if (kda->dist < kdb->dist) {
    return -1;
  }
  if (kda->dist > kdb->dist) {
    return 1;
  }
  return 0;
}
static void nearest_add_in_range(KDTreeNearest **r_nearest,
                                 uint nearest_index,
                                 uint *nearest_len_capacity,
                                 const int index,
                                 const float dist,
                                 const float co[KD_DIMS])
{
  KDTreeNearest *to;

  if (UNLIKELY(nearest_index >= *nearest_len_capacity)) {
    *r_nearest = static_cast<KDTreeNearest *>(MEM_reallocN_id(
        *r_nearest, (*nearest_len_capacity += KD_FOUND_ALLOC_INC) * sizeof(KDTreeNode), __func__));
  }

  to = (*r_nearest) + nearest_index;

  to->index = index;
  to->dist = sqrtf(dist);
  copy_vn_vn(to->co, co);
}

int BLI_kdtree_nd_(range_search_with_len_squared_cb)(
    const KDTree *tree,
    const float co[KD_DIMS],
    KDTreeNearest **r_nearest,
    const float range,
    float (*len_sq_fn)(const float co_search[KD_DIMS],
                       const float co_test[KD_DIMS],
                       const void *user_data),
    const void *user_data)
{
  const KDTreeNode *nodes = tree->nodes;
  uint *stack, stack_default[KD_STACK_INIT];
  KDTreeNearest *nearest = nullptr;
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
    len_sq_fn = len_squared_vnvn_cb;
    BLI_assert(user_data == nullptr);
  }

  stack = stack_default;
  stack_len_capacity = int(ARRAY_SIZE(stack_default));

  stack[cur++] = tree->root;

  while (cur--) {
    const KDTreeNode *node = &nodes[stack[cur]];

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
        nearest_add_in_range(
            &nearest, nearest_len++, &nearest_len_capacity, node->index, dist_sq, node->co);
      }

      if (node->left != KD_NODE_UNSET) {
        stack[cur++] = node->left;
      }
      if (node->right != KD_NODE_UNSET) {
        stack[cur++] = node->right;
      }
    }

    if (UNLIKELY(cur + KD_DIMS > stack_len_capacity)) {
      stack = realloc_nodes(stack, &stack_len_capacity, stack_default != stack);
    }
  }

  if (stack != stack_default) {
    MEM_freeN(stack);
  }

  if (nearest_len) {
    qsort(nearest, nearest_len, sizeof(KDTreeNearest), nearest_cmp_dist);
  }

  *r_nearest = nearest;

  return (int)nearest_len;
}

int BLI_kdtree_nd_(range_search)(const KDTree *tree,
                                 const float co[KD_DIMS],
                                 KDTreeNearest **r_nearest,
                                 float range)
{
  return BLI_kdtree_nd_(range_search_with_len_squared_cb)(
      tree, co, r_nearest, range, nullptr, nullptr);
}

void BLI_kdtree_nd_(range_search_cb)(
    const KDTree *tree,
    const float co[KD_DIMS],
    float range,
    bool (*search_cb)(void *user_data, int index, const float co[KD_DIMS], float dist_sq),
    void *user_data)
{
  const KDTreeNode *nodes = tree->nodes;

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
    const KDTreeNode *node = &nodes[stack[cur]];

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
      dist_sq = len_squared_vnvn(node->co, co);
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

    if (UNLIKELY(cur + KD_DIMS > stack_len_capacity)) {
      stack = realloc_nodes(stack, &stack_len_capacity, stack_default != stack);
    }
  }

finally:
  if (stack != stack_default) {
    MEM_freeN(stack);
  }
}

/**
 * Use when we want to loop over nodes ordered by index.
 * Requires indices to be aligned with nodes.
 */
static blender::Vector<int> kdtree_order(const KDTree *tree)
{
  const KDTreeNode *nodes = tree->nodes;
  blender::Vector<int> order(tree->max_node_index + 1, -1);
  for (uint i = 0; i < tree->nodes_len; i++) {
    order[nodes[i].index] = (int)i;
  }
  return order;
}

/* -------------------------------------------------------------------- */
/** \name BLI_kdtree_3d_calc_duplicates_fast
 * \{ */

struct DeDuplicateParams {
  /* Static */
  const KDTreeNode *nodes;
  float range;
  float range_sq;
  int *duplicates;
  int *duplicates_found;

  /* Per Search */
  float search_co[KD_DIMS];
  int search;
};

static void deduplicate_recursive(const DeDuplicateParams *p, uint i)
{
  const KDTreeNode *node = &p->nodes[i];
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
      if (len_squared_vnvn(node->co, p->search_co) <= p->range_sq) {
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

int BLI_kdtree_nd_(calc_duplicates_fast)(const KDTree *tree,
                                         const float range,
                                         const bool use_index_order,
                                         int *duplicates)
{
  int found = 0;

  DeDuplicateParams p = {};
  p.nodes = tree->nodes;
  p.range = range;
  p.range_sq = square_f(range);
  p.duplicates = duplicates;
  p.duplicates_found = &found;

  if (use_index_order) {
    blender::Vector<int> order = kdtree_order(tree);
    for (int i = 0; i < tree->max_node_index + 1; i++) {
      const int node_index = order[i];
      if (node_index == -1) {
        continue;
      }
      const int index = i;
      if (ELEM(duplicates[index], -1, index)) {
        p.search = index;
        copy_vn_vn(p.search_co, tree->nodes[node_index].co);
        int found_prev = found;
        deduplicate_recursive(&p, tree->root);
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
        copy_vn_vn(p.search_co, tree->nodes[node_index].co);
        int found_prev = found;
        deduplicate_recursive(&p, tree->root);
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

/* -------------------------------------------------------------------- */
/** \name BLI_kdtree_3d_calc_duplicates_cb
 * \{ */

int BLI_kdtree_nd_(calc_duplicates_cb)(const KDTree *tree,
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

      BLI_kdtree_nd_(range_search_cb_cpp)(tree, search_co, range, accumulate_neighbors_fn);
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

    BLI_kdtree_nd_(range_search_cb_cpp)(tree, search_co, range, accumulate_neighbors_fn);
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

/* -------------------------------------------------------------------- */
/** \name BLI_kdtree_3d_deduplicate
 * \{ */

static int kdtree_cmp_bool(const bool a, const bool b)
{
  if (a == b) {
    return 0;
  }
  return b ? -1 : 1;
}

static int kdtree_node_cmp_deduplicate(const void *n0_p, const void *n1_p)
{
  const KDTreeNode *n0 = static_cast<const KDTreeNode *>(n0_p);
  const KDTreeNode *n1 = static_cast<const KDTreeNode *>(n1_p);
  for (uint j = 0; j < KD_DIMS; j++) {
    if (n0->co[j] < n1->co[j]) {
      return -1;
    }
    if (n0->co[j] > n1->co[j]) {
      return 1;
    }
  }

  if (n0->d != KD_DIMS && n1->d != KD_DIMS) {
    /* Two nodes share identical `co`
     * Both are still valid.
     * Cast away `const` and tag one of them as invalid. */
    ((KDTreeNode *)n1)->d = KD_DIMS;
  }

  /* Keep sorting until each unique value has one and only one valid node. */
  return kdtree_cmp_bool(n0->d == KD_DIMS, n1->d == KD_DIMS);
}

int BLI_kdtree_nd_(deduplicate)(KDTree *tree)
{
#ifndef NDEBUG
  tree->is_balanced = false;
#endif
  qsort(tree->nodes, (size_t)tree->nodes_len, sizeof(*tree->nodes), kdtree_node_cmp_deduplicate);
  uint j = 0;
  for (uint i = 0; i < tree->nodes_len; i++) {
    if (tree->nodes[i].d != KD_DIMS) {
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
