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

#ifndef __BLI_KDTREE_H__
#define __BLI_KDTREE_H__

/** \file \ingroup bli
 *  \brief A kd-tree for nearest neighbor search.
 */

#include "BLI_compiler_attrs.h"

struct KDTree;
typedef struct KDTree KDTree;

typedef struct KDTreeNearest {
	int index;
	float dist;
	float co[3];
} KDTreeNearest;

KDTree *BLI_kdtree_new(unsigned int maxsize);
void BLI_kdtree_free(KDTree *tree);
void BLI_kdtree_balance(KDTree *tree) ATTR_NONNULL(1);

void BLI_kdtree_insert(
        KDTree *tree, int index,
        const float co[3]) ATTR_NONNULL(1, 3);
int BLI_kdtree_find_nearest(
        const KDTree *tree, const float co[3],
        KDTreeNearest *r_nearest) ATTR_NONNULL(1, 2);

#define BLI_kdtree_find_nearest_n(tree, co, r_nearest, n) \
        BLI_kdtree_find_nearest_n__normal(tree, co, NULL, r_nearest, n)
#define BLI_kdtree_range_search(tree, co, r_nearest, range) \
        BLI_kdtree_range_search__normal(tree, co, NULL, r_nearest, range)

int BLI_kdtree_find_nearest_cb(
        const KDTree *tree, const float co[3],
        int (*filter_cb)(void *user_data, int index, const float co[3], float dist_sq), void *user_data,
        KDTreeNearest *r_nearest);
void BLI_kdtree_range_search_cb(
        const KDTree *tree, const float co[3], float range,
        bool (*search_cb)(void *user_data, int index, const float co[3], float dist_sq), void *user_data);

int BLI_kdtree_calc_duplicates_fast(
        const KDTree *tree, const float range, bool use_index_order,
        int *doubles);

/* Normal use is deprecated */
/* remove __normal functions when last users drop */
int BLI_kdtree_find_nearest_n__normal(
        const KDTree *tree, const float co[3], const float nor[3],
        KDTreeNearest *r_nearest,
        unsigned int n) ATTR_NONNULL(1, 2, 4);
int BLI_kdtree_range_search__normal(
        const KDTree *tree, const float co[3], const float nor[3],
        KDTreeNearest **r_nearest,
        float range) ATTR_NONNULL(1, 2, 4) ATTR_WARN_UNUSED_RESULT;

#endif  /* __BLI_KDTREE_H__ */
