/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 * \brief A KD-tree for nearest neighbor search.
 */

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#define _BLI_CONCAT_AUX(MACRO_ARG1, MACRO_ARG2) MACRO_ARG1##MACRO_ARG2
#define _BLI_CONCAT(MACRO_ARG1, MACRO_ARG2) _BLI_CONCAT_AUX(MACRO_ARG1, MACRO_ARG2)
#define BLI_kdtree_nd_(id) _BLI_CONCAT(KDTREE_PREFIX_ID, _##id)

/* For auto-complete / `clangd`. */
#ifndef KD_DIMS
#  define KD_DIMS 0
#endif

struct KDTree;
typedef struct KDTree KDTree;

typedef struct KDTreeNearest {
  int index;
  float dist;
  float co[KD_DIMS];
} KDTreeNearest;

/**
 * \param nodes_len_capacity: The maximum length this KD-tree may hold.
 */
KDTree *BLI_kdtree_nd_(new)(unsigned int nodes_len_capacity);
void BLI_kdtree_nd_(free)(KDTree *tree);
void BLI_kdtree_nd_(balance)(KDTree *tree) ATTR_NONNULL(1);

void BLI_kdtree_nd_(insert)(KDTree *tree, int index, const float co[KD_DIMS]) ATTR_NONNULL(1, 3);
int BLI_kdtree_nd_(find_nearest)(const KDTree *tree,
                                 const float co[KD_DIMS],
                                 KDTreeNearest *r_nearest) ATTR_NONNULL(1, 2);

int BLI_kdtree_nd_(find_nearest_n)(const KDTree *tree,
                                   const float co[KD_DIMS],
                                   KDTreeNearest *r_nearest,
                                   uint nearest_len_capacity) ATTR_NONNULL(1, 2, 3);

int BLI_kdtree_nd_(range_search)(const KDTree *tree,
                                 const float co[KD_DIMS],
                                 KDTreeNearest **r_nearest,
                                 float range) ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;

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
    KDTreeNearest *r_nearest);
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
    void *user_data);

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
                                         float range,
                                         bool use_index_order,
                                         int *duplicates);

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
int BLI_kdtree_nd_(calc_duplicates_cb)(const KDTree *tree,
                                       const float range,
                                       int *duplicates,
                                       bool has_self_index,
                                       int (*deduplicate_cb)(void *user_data,
                                                             const int *cluster,
                                                             int cluster_num),
                                       void *user_data);

/**
 * Remove exact duplicates (run before balancing).
 *
 * Keep the first element added when duplicates are found.
 */
int BLI_kdtree_nd_(deduplicate)(KDTree *tree);

/**
 * Find \a nearest_len_capacity nearest returns number of points found, with results in nearest.
 *
 * \param r_nearest: An array of nearest, sized at least \a nearest_len_capacity.
 */
int BLI_kdtree_nd_(find_nearest_n_with_len_squared_cb)(
    const KDTree *tree,
    const float co[KD_DIMS],
    KDTreeNearest *r_nearest,
    uint nearest_len_capacity,
    float (*len_sq_fn)(const float co_search[KD_DIMS],
                       const float co_test[KD_DIMS],
                       const void *user_data),
    const void *user_data) ATTR_NONNULL(1, 2, 3);
/**
 * Range search returns number of points nearest_len, with results in nearest
 *
 * \param r_nearest: Allocated array of nearest nearest_len (caller is responsible for freeing).
 */
int BLI_kdtree_nd_(range_search_with_len_squared_cb)(
    const KDTree *tree,
    const float co[KD_DIMS],
    KDTreeNearest **r_nearest,
    float range,
    float (*len_sq_fn)(const float co_search[KD_DIMS],
                       const float co_test[KD_DIMS],
                       const void *user_data),
    const void *user_data) ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;

template<typename Fn>
inline void BLI_kdtree_nd_(range_search_cb_cpp)(const KDTree *tree,
                                                const float co[KD_DIMS],
                                                const float distance,
                                                const Fn &fn)
{
  BLI_kdtree_nd_(range_search_cb)(
      tree,
      co,
      distance,
      [](void *user_data, const int index, const float *co, const float dist_sq) {
        const Fn &fn = *static_cast<const Fn *>(user_data);
        return fn(index, co, dist_sq);
      },
      const_cast<Fn *>(&fn));
}

template<typename Fn>
inline int BLI_kdtree_nd_(find_nearest_cb_cpp)(const KDTree *tree,
                                               const float co[KD_DIMS],
                                               KDTreeNearest *r_nearest,
                                               Fn &&fn)
{
  return BLI_kdtree_nd_(find_nearest_cb)(
      tree,
      co,
      [](void *user_data, const int index, const float *co, const float dist_sq) {
        Fn &fn = *static_cast<Fn *>(user_data);
        return fn(index, co, dist_sq);
      },
      &fn,
      r_nearest);
}

template<typename Fn>
inline int BLI_kdtree_nd_(calc_duplicates_cb_cpp)(const KDTree *tree,
                                                  const float distance,
                                                  int *duplicates,
                                                  const bool has_self_index,
                                                  const Fn &fn)
{
  return BLI_kdtree_nd_(calc_duplicates_cb)(
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

#undef _BLI_CONCAT_AUX
#undef _BLI_CONCAT
#undef BLI_kdtree_nd_
