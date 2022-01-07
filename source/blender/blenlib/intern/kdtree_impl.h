/*
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
 */

/** \file
 * \ingroup bli
 */

#include "MEM_guardedalloc.h"

#include "BLI_kdtree_impl.h"
#include "BLI_math.h"
#include "BLI_strict_flags.h"
#include "BLI_utildefines.h"

#define _CONCAT_AUX(MACRO_ARG1, MACRO_ARG2) MACRO_ARG1##MACRO_ARG2
#define _CONCAT(MACRO_ARG1, MACRO_ARG2) _CONCAT_AUX(MACRO_ARG1, MACRO_ARG2)
#define BLI_kdtree_nd_(id) _CONCAT(KDTREE_PREFIX_ID, _##id)

typedef struct KDTreeNode_head {
  uint left, right;
  float co[KD_DIMS];
  int index;
} KDTreeNode_head;

typedef struct KDTreeNode {
  uint left, right;
  float co[KD_DIMS];
  int index;
  uint d; /* range is only (0..KD_DIMS - 1) */
} KDTreeNode;

struct KDTree {
  KDTreeNode *nodes;
  uint nodes_len;
  uint root;
#ifdef DEBUG
  bool is_balanced;        /* ensure we call balance first */
  uint nodes_len_capacity; /* max size of the tree */
#endif
};

#define KD_STACK_INIT 100     /* initial size for array (on the stack) */
#define KD_NEAR_ALLOC_INC 100 /* alloc increment for collecting nearest */
#define KD_FOUND_ALLOC_INC 50 /* alloc increment for collecting nearest */

#define KD_NODE_UNSET ((uint)-1)

/**
 * When set we know all values are unbalanced,
 * otherwise clear them when re-balancing: see T62210.
 */
#define KD_NODE_ROOT_IS_INIT ((uint)-2)

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
                                 const void *UNUSED(user_data))
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

  tree = MEM_mallocN(sizeof(KDTree), "KDTree");
  tree->nodes = MEM_mallocN(sizeof(KDTreeNode) * nodes_len_capacity, "KDTreeNode");
  tree->nodes_len = 0;
  tree->root = KD_NODE_ROOT_IS_INIT;

#ifdef DEBUG
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

#ifdef DEBUG
  BLI_assert(tree->nodes_len <= tree->nodes_len_capacity);
#endif

  /* NOTE: array isn't calloc'd,
   * need to initialize all struct members */

  node->left = node->right = KD_NODE_UNSET;
  copy_vn_vn(node->co, co);
  node->index = index;
  node->d = 0;

#ifdef DEBUG
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
  else if (nodes_len == 1) {
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

    while (1) {
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

#ifdef DEBUG
  tree->is_balanced = true;
#endif
}

static uint *realloc_nodes(uint *stack, uint *stack_len_capacity, const bool is_alloc)
{
  uint *stack_new = MEM_mallocN((*stack_len_capacity + KD_NEAR_ALLOC_INC) * sizeof(uint),
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

#ifdef DEBUG
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

/**
 * A version of #BLI_kdtree_3d_find_nearest which runs a callback
 * to filter out values.
 *
 * \param filter_cb: Filter find results,
 * Return codes: (1: accept, 0: skip, -1: immediate exit).
 */
int BLI_kdtree_nd_(find_nearest_cb)(
    const KDTree *tree,
    const float co[KD_DIMS],
    int (*filter_cb)(void *user_data, int index, const float co[KD_DIMS], float dist_sq),
    void *user_data,
    KDTreeNearest *r_nearest)
{
  const KDTreeNode *nodes = tree->nodes;
  const KDTreeNode *min_node = NULL;

  uint *stack, stack_default[KD_STACK_INIT];
  float min_dist = FLT_MAX, cur_dist;
  uint stack_len_capacity, cur = 0;

#ifdef DEBUG
  BLI_assert(tree->is_balanced == true);
#endif

  if (UNLIKELY(tree->root == KD_NODE_UNSET)) {
    return -1;
  }

  stack = stack_default;
  stack_len_capacity = ARRAY_SIZE(stack_default);

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
  else {
    return -1;
  }
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
    else {
      nearest[i] = nearest[i - 1];
    }
  }

  nearest[i].index = index;
  nearest[i].dist = dist;
  copy_vn_vn(nearest[i].co, co);
}

/**
 * Find \a nearest_len_capacity nearest returns number of points found, with results in nearest.
 *
 * \param r_nearest: An array of nearest, sized at least \a nearest_len_capacity.
 */
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

#ifdef DEBUG
  BLI_assert(tree->is_balanced == true);
#endif

  if (UNLIKELY((tree->root == KD_NODE_UNSET) || nearest_len_capacity == 0)) {
    return 0;
  }

  if (len_sq_fn == NULL) {
    len_sq_fn = len_squared_vnvn_cb;
    BLI_assert(user_data == NULL);
  }

  stack = stack_default;
  stack_len_capacity = ARRAY_SIZE(stack_default);

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
      tree, co, r_nearest, nearest_len_capacity, NULL, NULL);
}

static int nearest_cmp_dist(const void *a, const void *b)
{
  const KDTreeNearest *kda = a;
  const KDTreeNearest *kdb = b;

  if (kda->dist < kdb->dist) {
    return -1;
  }
  else if (kda->dist > kdb->dist) {
    return 1;
  }
  else {
    return 0;
  }
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
    *r_nearest = MEM_reallocN_id(
        *r_nearest, (*nearest_len_capacity += KD_FOUND_ALLOC_INC) * sizeof(KDTreeNode), __func__);
  }

  to = (*r_nearest) + nearest_index;

  to->index = index;
  to->dist = sqrtf(dist);
  copy_vn_vn(to->co, co);
}

/**
 * Range search returns number of points nearest_len, with results in nearest
 *
 * \param r_nearest: Allocated array of nearest nearest_len (caller is responsible for freeing).
 */
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
  KDTreeNearest *nearest = NULL;
  const float range_sq = range * range;
  float dist_sq;
  uint stack_len_capacity, cur = 0;
  uint nearest_len = 0, nearest_len_capacity = 0;

#ifdef DEBUG
  BLI_assert(tree->is_balanced == true);
#endif

  if (UNLIKELY(tree->root == KD_NODE_UNSET)) {
    return 0;
  }

  if (len_sq_fn == NULL) {
    len_sq_fn = len_squared_vnvn_cb;
    BLI_assert(user_data == NULL);
  }

  stack = stack_default;
  stack_len_capacity = ARRAY_SIZE(stack_default);

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
  return BLI_kdtree_nd_(range_search_with_len_squared_cb)(tree, co, r_nearest, range, NULL, NULL);
}

/**
 * A version of #BLI_kdtree_3d_range_search which runs a callback
 * instead of allocating an array.
 *
 * \param search_cb: Called for every node found in \a range,
 * false return value performs an early exit.
 *
 * \note the order of calls isn't sorted based on distance.
 */
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

#ifdef DEBUG
  BLI_assert(tree->is_balanced == true);
#endif

  if (UNLIKELY(tree->root == KD_NODE_UNSET)) {
    return;
  }

  stack = stack_default;
  stack_len_capacity = ARRAY_SIZE(stack_default);

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
static uint *kdtree_order(const KDTree *tree)
{
  const KDTreeNode *nodes = tree->nodes;
  uint *order = MEM_mallocN(sizeof(uint) * tree->nodes_len, __func__);
  for (uint i = 0; i < tree->nodes_len; i++) {
    order[nodes[i].index] = i;
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

static void deduplicate_recursive(const struct DeDuplicateParams *p, uint i)
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
int BLI_kdtree_nd_(calc_duplicates_fast)(const KDTree *tree,
                                         const float range,
                                         bool use_index_order,
                                         int *duplicates)
{
  int found = 0;
  struct DeDuplicateParams p = {
      .nodes = tree->nodes,
      .range = range,
      .range_sq = square_f(range),
      .duplicates = duplicates,
      .duplicates_found = &found,
  };

  if (use_index_order) {
    uint *order = kdtree_order(tree);
    for (uint i = 0; i < tree->nodes_len; i++) {
      const uint node_index = order[i];
      const int index = (int)i;
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
    MEM_freeN(order);
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
/** \name BLI_kdtree_3d_deduplicate
 * \{ */

static int kdtree_node_cmp_deduplicate(const void *n0_p, const void *n1_p)
{
  const KDTreeNode *n0 = n0_p;
  const KDTreeNode *n1 = n1_p;
  for (uint j = 0; j < KD_DIMS; j++) {
    if (n0->co[j] < n1->co[j]) {
      return -1;
    }
    else if (n0->co[j] > n1->co[j]) {
      return 1;
    }
  }
  /* Sort by pointer so the first added will be used.
   * assignment below ignores const correctness,
   * however the values aren't used for sorting and are to be discarded. */
  if (n0 < n1) {
    ((KDTreeNode *)n1)->d = KD_DIMS; /* tag invalid */
    return -1;
  }
  else {
    ((KDTreeNode *)n0)->d = KD_DIMS; /* tag invalid */
    return 1;
  }
}

/**
 * Remove exact duplicates (run before balancing).
 *
 * Keep the first element added when duplicates are found.
 */
int BLI_kdtree_nd_(deduplicate)(KDTree *tree)
{
#ifdef DEBUG
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
