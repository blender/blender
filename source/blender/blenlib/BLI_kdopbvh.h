/**
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
 * The Original Code is Copyright (C) 2006 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Daniel Genrich, Andre Pinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#ifndef BLI_KDOPBVH_H
#define BLI_KDOPBVH_H

#include <float.h>

struct BVHTree;
typedef struct BVHTree BVHTree;

typedef struct BVHTreeOverlap {
	int indexA;
	int indexB;
} BVHTreeOverlap;

BVHTree *BLI_bvhtree_new(int maxsize, float epsilon, char tree_type, char axis);
void BLI_bvhtree_free(BVHTree *tree);

/* construct: first insert points, then call balance */
int BLI_bvhtree_insert(BVHTree *tree, int index, float *co, int numpoints);
void BLI_bvhtree_balance(BVHTree *tree);

/* update: first update points/nodes, then call update_tree to refit the bounding volumes */
int BLI_bvhtree_update_node(BVHTree *tree, int index, float *co, float *co_moving, int numpoints);
void BLI_bvhtree_update_tree(BVHTree *tree);

/* collision/overlap: check two trees if they overlap, alloc's *overlap with length of the int return value */
BVHTreeOverlap *BLI_bvhtree_overlap(BVHTree *tree1, BVHTree *tree2, int *result);

float BLI_bvhtree_getepsilon(BVHTree *tree);

#endif // BLI_KDOPBVH_H

