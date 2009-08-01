/**
 * A kd-tree for nearest neighbour search.
 * 
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: none of this file.
 *
 * Contributor(s): Janne Karhu
 *                 Brecht Van Lommel
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef BLI_KDTREE_H
#define BLI_KDTREE_H

struct KDTree;
typedef struct KDTree KDTree;

typedef struct KDTreeNearest {
	int index;
	float dist;
	float co[3];
} KDTreeNearest;

/* Creates or free a kdtree */
KDTree* BLI_kdtree_new(int maxsize);
void BLI_kdtree_free(KDTree *tree);

/* Construction: first insert points, then call balance. Normal is optional. */
void BLI_kdtree_insert(KDTree *tree, int index, float *co, float *nor);
void BLI_kdtree_balance(KDTree *tree);

/* Find nearest returns index, and -1 if no node is found.
 * Find n nearest returns number of points found, with results in nearest.
 * Normal is optional, but if given will limit results to points in normal direction from co. */
int	BLI_kdtree_find_nearest(KDTree *tree, float *co, float *nor, KDTreeNearest *nearest);
int	BLI_kdtree_find_n_nearest(KDTree *tree, int n, float *co, float *nor, KDTreeNearest *nearest);

/* Range search returns number of points found, with results in nearest */
/* Normal is optional, but if given will limit results to points in normal direction from co. */
/* Remember to free nearest after use! */
int BLI_kdtree_range_search(KDTree *tree, float range, float *co, float *nor, KDTreeNearest **nearest);
#endif

