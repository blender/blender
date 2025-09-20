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

int BLI_kdtree_nd_(find_nearest_cb)(
    const KDTree *tree,
    const float co[KD_DIMS],
    int (*filter_cb)(void *user_data, int index, const float co[KD_DIMS], float dist_sq),
    void *user_data,
    KDTreeNearest *r_nearest);
void BLI_kdtree_nd_(range_search_cb)(
    const KDTree *tree,
    const float co[KD_DIMS],
    float range,
    bool (*search_cb)(void *user_data, int index, const float co[KD_DIMS], float dist_sq),
    void *user_data);

int BLI_kdtree_nd_(calc_duplicates_fast)(const KDTree *tree,
                                         float range,
                                         bool use_index_order,
                                         int *duplicates);

/**
 * De-duplicate utility where the callback can evaluate duplicates and select the target
 * which other indices are merged into.
 *
 * \param tree: A tree, all indices *must* be unique.
 * \param deduplicate_cb: A function which receives duplicate indices,
 * it must choose the the "target" index to keep which is returned.
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
                                       int (*deduplicate_cb)(void *user_data,
                                                             const int *cluster,
                                                             int cluster_num),
                                       void *user_data);

int BLI_kdtree_nd_(deduplicate)(KDTree *tree);

/** Versions of find/range search that take a squared distance callback to support bias. */
int BLI_kdtree_nd_(find_nearest_n_with_len_squared_cb)(
    const KDTree *tree,
    const float co[KD_DIMS],
    KDTreeNearest *r_nearest,
    uint nearest_len_capacity,
    float (*len_sq_fn)(const float co_search[KD_DIMS],
                       const float co_test[KD_DIMS],
                       const void *user_data),
    const void *user_data) ATTR_NONNULL(1, 2, 3);
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
                                                float distance,
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
                                                  float distance,
                                                  int *duplicates,
                                                  const Fn &fn)
{
  return BLI_kdtree_nd_(calc_duplicates_cb)(
      tree,
      distance,
      duplicates,
      [](void *user_data, const int *cluster, int cluster_num) -> int {
        const Fn &fn = *static_cast<const Fn *>(user_data);
        return fn(cluster, cluster_num);
      },
      const_cast<Fn *>(&fn));
}

#undef _BLI_CONCAT_AUX
#undef _BLI_CONCAT
#undef BLI_kdtree_nd_
