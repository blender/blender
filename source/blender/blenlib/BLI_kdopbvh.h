/**
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

typedef struct BVHTreeNearest
{
	int index;			/* the index of the nearest found (untouched if none is found within a dist radius from the given coordinates) */
	float co[3];		/* nearest coordinates (untouched it none is found within a dist radius from the given coordinates) */
	float no[3];		/* normal at nearest coordinates (untouched it none is found within a dist radius from the given coordinates) */
	float dist;			/* squared distance to search arround */
} BVHTreeNearest;

typedef struct BVHTreeRay
{
	float origin[3];	/* ray origin */
	float direction[3];	/* ray direction */
} BVHTreeRay;

typedef struct BVHTreeRayHit
{
	int index;			/* index of the tree node (untouched if no hit is found) */
	float co[3];		/* coordinates of the hit point */
	float no[3];		/* normal on hit point */
	float dist;			/* distance to the hit point */
} BVHTreeRayHit;

/* callback must update nearest in case it finds a nearest result */
typedef void (*BVHTree_NearestPointCallback) (void *userdata, int index, const float *co, BVHTreeNearest *nearest);

/* callback must update hit in case it finds a nearest successful hit */
typedef void (*BVHTree_RayCastCallback) (void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit);


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

/* find nearest node to the given coordinates (if nearest is given it will only search nodes where square distance is smaller than nearest->dist) */
int BLI_bvhtree_find_nearest(BVHTree *tree, const float *co, BVHTreeNearest *nearest, BVHTree_NearestPointCallback callback, void *userdata);

int BLI_bvhtree_ray_cast(BVHTree *tree, const float *co, const float *dir, BVHTreeRayHit *hit, BVHTree_RayCastCallback callback, void *userdata);

#endif // BLI_KDOPBVH_H

