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
 * \brief A KD-tree for nearest neighbor search.
 */

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#define _BLI_CONCAT_AUX(MACRO_ARG1, MACRO_ARG2) MACRO_ARG1##MACRO_ARG2
#define _BLI_CONCAT(MACRO_ARG1, MACRO_ARG2) _BLI_CONCAT_AUX(MACRO_ARG1, MACRO_ARG2)
#define BLI_kdtree_nd_(id) _BLI_CONCAT(KDTREE_PREFIX_ID, _##id)

struct KDTree;
typedef struct KDTree KDTree;

typedef struct KDTreeNearest {
  int index;
  float dist;
  float co[KD_DIMS];
} KDTreeNearest;

KDTree *BLI_kdtree_nd_(new)(unsigned int maxsize);
void BLI_kdtree_nd_(free)(KDTree *tree);
void BLI_kdtree_nd_(balance)(KDTree *tree) ATTR_NONNULL(1);

void BLI_kdtree_nd_(insert)(KDTree *tree, int index, const float co[KD_DIMS]) ATTR_NONNULL(1, 3);
int BLI_kdtree_nd_(find_nearest)(const KDTree *tree,
                                 const float co[KD_DIMS],
                                 KDTreeNearest *r_nearest) ATTR_NONNULL(1, 2);

int BLI_kdtree_nd_(find_nearest_n)(const KDTree *tree,
                                   const float co[KD_DIMS],
                                   KDTreeNearest *r_nearest,
                                   const uint nearest_len_capacity) ATTR_NONNULL(1, 2, 3);

int BLI_kdtree_nd_(range_search)(const KDTree *tree,
                                 const float co[KD_DIMS],
                                 KDTreeNearest **r_nearest,
                                 const float range) ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;

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
                                         const float range,
                                         bool use_index_order,
                                         int *doubles);

int BLI_kdtree_nd_(deduplicate)(KDTree *tree);

/* Versions of find/range search that take a squared distance callback to support bias. */
int BLI_kdtree_nd_(find_nearest_n_with_len_squared_cb)(
    const KDTree *tree,
    const float co[KD_DIMS],
    KDTreeNearest *r_nearest,
    const uint nearest_len_capacity,
    float (*len_sq_fn)(const float co_search[KD_DIMS],
                       const float co_test[KD_DIMS],
                       const void *user_data),
    const void *user_data) ATTR_NONNULL(1, 2, 3);
int BLI_kdtree_nd_(range_search_with_len_squared_cb)(
    const KDTree *tree,
    const float co[KD_DIMS],
    KDTreeNearest **r_nearest,
    const float range,
    float (*len_sq_fn)(const float co_search[KD_DIMS],
                       const float co_test[KD_DIMS],
                       const void *user_data),
    const void *user_data) ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;

#undef _BLI_CONCAT_AUX
#undef _BLI_CONCAT
#undef BLI_kdtree_nd_
