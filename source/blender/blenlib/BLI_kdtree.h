/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Janne Karhu
 *                 Brecht Van Lommel
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef __BLI_KDTREE_H__
#define __BLI_KDTREE_H__

/** \file BLI_kdtree.h
 *  \ingroup bli
 *  \brief A kd-tree for nearest neighbor search.
 *  \author Janne Karhu
 *  \author Brecht van Lommel
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
        KDTree *tree, const float co[3],
        KDTreeNearest *r_nearest) ATTR_NONNULL(1, 2);

#define BLI_kdtree_find_nearest_n(tree, co, r_nearest, n) \
        BLI_kdtree_find_nearest_n__normal(tree, co, NULL, r_nearest, n)
#define BLI_kdtree_range_search(tree, co, r_nearest, range) \
        BLI_kdtree_range_search__normal(tree, co, NULL, r_nearest, range)

/* Normal use is deprecated */
/* remove __normal functions when last users drop */
int BLI_kdtree_find_nearest_n__normal(
        KDTree *tree, const float co[3], const float nor[3],
        KDTreeNearest *r_nearest,
        unsigned int n) ATTR_NONNULL(1, 2, 4);
int BLI_kdtree_range_search__normal(
        KDTree *tree, const float co[3], const float nor[3],
        KDTreeNearest **r_nearest,
        float range) ATTR_NONNULL(1, 2, 4) ATTR_WARN_UNUSED_RESULT;

#endif  /* __BLI_KDTREE_H__ */
