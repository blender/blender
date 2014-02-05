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
 * The Original Code is Copyright (C) 2006 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Daniel Genrich, Andre Pinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#ifndef __BLI_KDOPBVH_H__
#define __BLI_KDOPBVH_H__

/** \file BLI_kdopbvh.h
 *  \ingroup bli
 *  \author Daniel Genrich
 *  \author Andre Pinto
 */

#ifdef __cplusplus
extern "C" { 
#endif

struct BVHTree;
typedef struct BVHTree BVHTree;

typedef struct BVHTreeOverlap {
	int indexA;
	int indexB;
} BVHTreeOverlap;

/* flags */
#define BVH_ONQUAD (1 << 0)

typedef struct BVHTreeNearest {
	int index;          /* the index of the nearest found (untouched if none is found within a dist radius from the given coordinates) */
	float co[3];        /* nearest coordinates (untouched it none is found within a dist radius from the given coordinates) */
	float no[3];        /* normal at nearest coordinates (untouched it none is found within a dist radius from the given coordinates) */
	float dist_sq;      /* squared distance to search arround */
	int flags;
} BVHTreeNearest;

typedef struct BVHTreeRay {
	float origin[3];    /* ray origin */
	float direction[3]; /* ray direction */
	float radius;       /* radius around ray */
} BVHTreeRay;

typedef struct BVHTreeRayHit {
	int index;          /* index of the tree node (untouched if no hit is found) */
	float co[3];        /* coordinates of the hit point */
	float no[3];        /* normal on hit point */
	float dist;         /* distance to the hit point */
	int flags;
} BVHTreeRayHit;

/* callback must update nearest in case it finds a nearest result */
typedef void (*BVHTree_NearestPointCallback)(void *userdata, int index, const float co[3], BVHTreeNearest *nearest);

/* callback must update hit in case it finds a nearest successful hit */
typedef void (*BVHTree_RayCastCallback)(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit);

/* callback to range search query */
typedef void (*BVHTree_RangeQuery)(void *userdata, int index, float dist_sq);

BVHTree *BLI_bvhtree_new(int maxsize, float epsilon, char tree_type, char axis);
void BLI_bvhtree_free(BVHTree *tree);

/* construct: first insert points, then call balance */
void BLI_bvhtree_insert(BVHTree *tree, int index, const float co[3], int numpoints);
void BLI_bvhtree_balance(BVHTree *tree);

/* update: first update points/nodes, then call update_tree to refit the bounding volumes */
int BLI_bvhtree_update_node(BVHTree *tree, int index, const float co[3], const float co_moving[3], int numpoints);
void BLI_bvhtree_update_tree(BVHTree *tree);

/* collision/overlap: check two trees if they overlap, alloc's *overlap with length of the int return value */
BVHTreeOverlap *BLI_bvhtree_overlap(BVHTree *tree1, BVHTree *tree2, unsigned int *result);

float BLI_bvhtree_getepsilon(const BVHTree *tree);

/* find nearest node to the given coordinates
 * (if nearest is given it will only search nodes where square distance is smaller than nearest->dist) */
int BLI_bvhtree_find_nearest(BVHTree *tree, const float co[3], BVHTreeNearest *nearest,
                             BVHTree_NearestPointCallback callback, void *userdata);

int BLI_bvhtree_ray_cast(BVHTree *tree, const float co[3], const float dir[3], float radius, BVHTreeRayHit *hit,
                         BVHTree_RayCastCallback callback, void *userdata);

float BLI_bvhtree_bb_raycast(const float bv[6], const float light_start[3], const float light_end[3], float pos[3]);

/* range query */
int BLI_bvhtree_range_query(BVHTree *tree, const float co[3], float radius,
                            BVHTree_RangeQuery callback, void *userdata);

#ifdef __cplusplus
}
#endif

#endif  /* __BLI_KDOPBVH_H__ */
