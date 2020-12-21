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
 *
 * The Original Code is Copyright (C) 2006 by NaN Holding BV.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BVHTree;
struct DistProjectedAABBPrecalc;

typedef struct BVHTree BVHTree;
#define USE_KDOPBVH_WATERTIGHT

typedef struct BVHTreeAxisRange {
  union {
    struct {
      float min, max;
    };
    /* alternate access */
    float range[2];
  };
} BVHTreeAxisRange;

typedef struct BVHTreeOverlap {
  int indexA;
  int indexB;
} BVHTreeOverlap;

typedef struct BVHTreeNearest {
  /** The index of the nearest found
   * (untouched if none is found within a dist radius from the given coordinates) */
  int index;
  /** Nearest coordinates
   * (untouched it none is found within a dist radius from the given coordinates). */
  float co[3];
  /** Normal at nearest coordinates
   * (untouched it none is found within a dist radius from the given coordinates). */
  float no[3];
  /** squared distance to search around */
  float dist_sq;
  int flags;
} BVHTreeNearest;

typedef struct BVHTreeRay {
  /** ray origin */
  float origin[3];
  /** ray direction */
  float direction[3];
  /** radius around ray */
  float radius;
#ifdef USE_KDOPBVH_WATERTIGHT
  struct IsectRayPrecalc *isect_precalc;
#endif
} BVHTreeRay;

typedef struct BVHTreeRayHit {
  /** Index of the tree node (untouched if no hit is found). */
  int index;
  /** Coordinates of the hit point. */
  float co[3];
  /** Normal on hit point. */
  float no[3];
  /** Distance to the hit point. */
  float dist;
} BVHTreeRayHit;

enum {
  /* Use a priority queue to process nodes in the optimal order (for slow callbacks) */
  BVH_OVERLAP_USE_THREADING = (1 << 0),
  BVH_OVERLAP_RETURN_PAIRS = (1 << 1),
};
enum {
  /* Use a priority queue to process nodes in the optimal order (for slow callbacks) */
  BVH_NEAREST_OPTIMAL_ORDER = (1 << 0),
};
enum {
  /* calculate IsectRayPrecalc data */
  BVH_RAYCAST_WATERTIGHT = (1 << 0),
};
#define BVH_RAYCAST_DEFAULT (BVH_RAYCAST_WATERTIGHT)
#define BVH_RAYCAST_DIST_MAX (FLT_MAX / 2.0f)

/* callback must update nearest in case it finds a nearest result */
typedef void (*BVHTree_NearestPointCallback)(void *userdata,
                                             int index,
                                             const float co[3],
                                             BVHTreeNearest *nearest);

/* callback must update hit in case it finds a nearest successful hit */
typedef void (*BVHTree_RayCastCallback)(void *userdata,
                                        int index,
                                        const BVHTreeRay *ray,
                                        BVHTreeRayHit *hit);

/* callback to check if 2 nodes overlap (use thread if intersection results need to be stored) */
typedef bool (*BVHTree_OverlapCallback)(void *userdata, int index_a, int index_b, int thread);

/* callback to range search query */
typedef void (*BVHTree_RangeQuery)(void *userdata, int index, const float co[3], float dist_sq);

/* callback to find nearest projected */
typedef void (*BVHTree_NearestProjectedCallback)(void *userdata,
                                                 int index,
                                                 const struct DistProjectedAABBPrecalc *precalc,
                                                 const float (*clip_plane)[4],
                                                 const int clip_plane_len,
                                                 BVHTreeNearest *nearest);

/* callbacks to BLI_bvhtree_walk_dfs */
/* return true to traverse into this nodes children, else skip. */
typedef bool (*BVHTree_WalkParentCallback)(const BVHTreeAxisRange *bounds, void *userdata);
/* return true to keep walking, else early-exit the search. */
typedef bool (*BVHTree_WalkLeafCallback)(const BVHTreeAxisRange *bounds,
                                         int index,
                                         void *userdata);
/* return true to search (min, max) else (max, min). */
typedef bool (*BVHTree_WalkOrderCallback)(const BVHTreeAxisRange *bounds,
                                          char axis,
                                          void *userdata);

BVHTree *BLI_bvhtree_new(int maxsize, float epsilon, char tree_type, char axis);
void BLI_bvhtree_free(BVHTree *tree);

/* construct: first insert points, then call balance */
void BLI_bvhtree_insert(BVHTree *tree, int index, const float co[3], int numpoints);
void BLI_bvhtree_balance(BVHTree *tree);

/* update: first update points/nodes, then call update_tree to refit the bounding volumes */
bool BLI_bvhtree_update_node(
    BVHTree *tree, int index, const float co[3], const float co_moving[3], int numpoints);
void BLI_bvhtree_update_tree(BVHTree *tree);

int BLI_bvhtree_overlap_thread_num(const BVHTree *tree);

/* collision/overlap: check two trees if they overlap,
 * alloc's *overlap with length of the int return value */
BVHTreeOverlap *BLI_bvhtree_overlap_ex(
    const BVHTree *tree1,
    const BVHTree *tree2,
    uint *r_overlap_tot,
    /* optional callback to test the overlap before adding (must be thread-safe!) */
    BVHTree_OverlapCallback callback,
    void *userdata,
    const uint max_interactions,
    const int flag);
BVHTreeOverlap *BLI_bvhtree_overlap(const BVHTree *tree1,
                                    const BVHTree *tree2,
                                    unsigned int *r_overlap_tot,
                                    BVHTree_OverlapCallback callback,
                                    void *userdata);

int *BLI_bvhtree_intersect_plane(BVHTree *tree, float plane[4], uint *r_intersect_tot);

int BLI_bvhtree_get_len(const BVHTree *tree);
int BLI_bvhtree_get_tree_type(const BVHTree *tree);
float BLI_bvhtree_get_epsilon(const BVHTree *tree);
void BLI_bvhtree_get_bounding_box(BVHTree *tree, float r_bb_min[3], float r_bb_max[3]);

/* find nearest node to the given coordinates
 * (if nearest is given it will only search nodes where
 * square distance is smaller than nearest->dist) */
int BLI_bvhtree_find_nearest_ex(BVHTree *tree,
                                const float co[3],
                                BVHTreeNearest *nearest,
                                BVHTree_NearestPointCallback callback,
                                void *userdata,
                                int flag);
int BLI_bvhtree_find_nearest(BVHTree *tree,
                             const float co[3],
                             BVHTreeNearest *nearest,
                             BVHTree_NearestPointCallback callback,
                             void *userdata);

int BLI_bvhtree_find_nearest_first(BVHTree *tree,
                                   const float co[3],
                                   const float dist_sq,
                                   BVHTree_NearestPointCallback callback,
                                   void *userdata);

int BLI_bvhtree_ray_cast_ex(BVHTree *tree,
                            const float co[3],
                            const float dir[3],
                            float radius,
                            BVHTreeRayHit *hit,
                            BVHTree_RayCastCallback callback,
                            void *userdata,
                            int flag);
int BLI_bvhtree_ray_cast(BVHTree *tree,
                         const float co[3],
                         const float dir[3],
                         float radius,
                         BVHTreeRayHit *hit,
                         BVHTree_RayCastCallback callback,
                         void *userdata);

void BLI_bvhtree_ray_cast_all_ex(BVHTree *tree,
                                 const float co[3],
                                 const float dir[3],
                                 float radius,
                                 float hit_dist,
                                 BVHTree_RayCastCallback callback,
                                 void *userdata,
                                 int flag);
void BLI_bvhtree_ray_cast_all(BVHTree *tree,
                              const float co[3],
                              const float dir[3],
                              float radius,
                              float hit_dist,
                              BVHTree_RayCastCallback callback,
                              void *userdata);

float BLI_bvhtree_bb_raycast(const float bv[6],
                             const float light_start[3],
                             const float light_end[3],
                             float pos[3]);

/* range query */
int BLI_bvhtree_range_query(
    BVHTree *tree, const float co[3], float radius, BVHTree_RangeQuery callback, void *userdata);

int BLI_bvhtree_find_nearest_projected(BVHTree *tree,
                                       float projmat[4][4],
                                       float winsize[2],
                                       float mval[2],
                                       float clip_planes[6][4],
                                       int clip_plane_len,
                                       BVHTreeNearest *nearest,
                                       BVHTree_NearestProjectedCallback callback,
                                       void *userdata);

void BLI_bvhtree_walk_dfs(BVHTree *tree,
                          BVHTree_WalkParentCallback walk_parent_cb,
                          BVHTree_WalkLeafCallback walk_leaf_cb,
                          BVHTree_WalkOrderCallback walk_order_cb,
                          void *userdata);

/* expose for bvh callbacks to use */
extern const float bvhtree_kdop_axes[13][3];

#ifdef __cplusplus
}
#endif
