/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 NaN Holding BV. All rights reserved. */

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
  BVH_OVERLAP_USE_THREADING = (1 << 0),
  BVH_OVERLAP_RETURN_PAIRS = (1 << 1),
  /* Use a specialized self-overlap traversal to only test and output every
   * pair once, rather than twice in different order as usual. */
  BVH_OVERLAP_SELF = (1 << 2),
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

/**
 * Callback must update nearest in case it finds a nearest result.
 */
typedef void (*BVHTree_NearestPointCallback)(void *userdata,
                                             int index,
                                             const float co[3],
                                             BVHTreeNearest *nearest);

/**
 * Callback must update hit in case it finds a nearest successful hit.
 */
typedef void (*BVHTree_RayCastCallback)(void *userdata,
                                        int index,
                                        const BVHTreeRay *ray,
                                        BVHTreeRayHit *hit);

/**
 * Callback to check if 2 nodes overlap (use thread if intersection results need to be stored).
 */
typedef bool (*BVHTree_OverlapCallback)(void *userdata, int index_a, int index_b, int thread);

/**
 * Callback to range search query.
 */
typedef void (*BVHTree_RangeQuery)(void *userdata, int index, const float co[3], float dist_sq);

/**
 * Callback to find nearest projected.
 */
typedef void (*BVHTree_NearestProjectedCallback)(void *userdata,
                                                 int index,
                                                 const struct DistProjectedAABBPrecalc *precalc,
                                                 const float (*clip_plane)[4],
                                                 int clip_plane_len,
                                                 BVHTreeNearest *nearest);

/* callbacks to BLI_bvhtree_walk_dfs */

/**
 * Return true to traverse into this nodes children, else skip.
 */
typedef bool (*BVHTree_WalkParentCallback)(const BVHTreeAxisRange *bounds, void *userdata);
/**
 * Return true to keep walking, else early-exit the search.
 */
typedef bool (*BVHTree_WalkLeafCallback)(const BVHTreeAxisRange *bounds,
                                         int index,
                                         void *userdata);
/**
 * Return true to search (min, max) else (max, min).
 */
typedef bool (*BVHTree_WalkOrderCallback)(const BVHTreeAxisRange *bounds,
                                          char axis,
                                          void *userdata);

/**
 * \note many callers don't check for `NULL` return.
 */
BVHTree *BLI_bvhtree_new(int maxsize, float epsilon, char tree_type, char axis);
void BLI_bvhtree_free(BVHTree *tree);

/**
 * Construct: first insert points, then call balance.
 */
void BLI_bvhtree_insert(BVHTree *tree, int index, const float co[3], int numpoints);
void BLI_bvhtree_balance(BVHTree *tree);

/**
 * Update: first update points/nodes, then call update_tree to refit the bounding volumes.
 * \note call before #BLI_bvhtree_update_tree().
 */
bool BLI_bvhtree_update_node(
    BVHTree *tree, int index, const float co[3], const float co_moving[3], int numpoints);
/**
 * Call #BLI_bvhtree_update_node() first for every node/point/triangle.
 *
 * Note that this does not rebalance the tree, so if the shape of the mesh changes
 * too much, operations on the tree may become suboptimal.
 */
void BLI_bvhtree_update_tree(BVHTree *tree);

/**
 * Use to check the total number of threads #BLI_bvhtree_overlap will use.
 *
 * \warning Must be the first tree passed to #BLI_bvhtree_overlap!
 */
int BLI_bvhtree_overlap_thread_num(const BVHTree *tree);

/**
 * Collision/overlap: check two trees if they overlap,
 * alloc's *overlap with length of the int return value.
 *
 * \param callback: optional, to test the overlap before adding (must be thread-safe!).
 */
BVHTreeOverlap *BLI_bvhtree_overlap_ex(const BVHTree *tree1,
                                       const BVHTree *tree2,
                                       uint *r_overlap_num,
                                       BVHTree_OverlapCallback callback,
                                       void *userdata,
                                       uint max_interactions,
                                       int flag);
BVHTreeOverlap *BLI_bvhtree_overlap(const BVHTree *tree1,
                                    const BVHTree *tree2,
                                    unsigned int *r_overlap_num,
                                    BVHTree_OverlapCallback callback,
                                    void *userdata);
/** Compute overlaps of the tree with itself. This is faster than BLI_bvhtree_overlap
 *  because it only tests and returns each symmetrical pair once. */
BVHTreeOverlap *BLI_bvhtree_overlap_self(const BVHTree *tree,
                                         unsigned int *r_overlap_num,
                                         BVHTree_OverlapCallback callback,
                                         void *userdata);

int *BLI_bvhtree_intersect_plane(const BVHTree *tree, float plane[4], uint *r_intersect_num);

/**
 * Number of times #BLI_bvhtree_insert has been called.
 * mainly useful for asserts functions to check we added the correct number.
 */
int BLI_bvhtree_get_len(const BVHTree *tree);
/**
 * Maximum number of children that a node can have.
 */
int BLI_bvhtree_get_tree_type(const BVHTree *tree);
float BLI_bvhtree_get_epsilon(const BVHTree *tree);
/**
 * This function returns the bounding box of the BVH tree.
 */
void BLI_bvhtree_get_bounding_box(const BVHTree *tree, float r_bb_min[3], float r_bb_max[3]);

/**
 * Find nearest node to the given coordinates
 * (if nearest is given it will only search nodes where
 * square distance is smaller than nearest->dist).
 */
int BLI_bvhtree_find_nearest_ex(const BVHTree *tree,
                                const float co[3],
                                BVHTreeNearest *nearest,
                                BVHTree_NearestPointCallback callback,
                                void *userdata,
                                int flag);
int BLI_bvhtree_find_nearest(const BVHTree *tree,
                             const float co[3],
                             BVHTreeNearest *nearest,
                             BVHTree_NearestPointCallback callback,
                             void *userdata);

/**
 * Find the first node nearby.
 * Favors speed over quality since it doesn't find the best target node.
 */
int BLI_bvhtree_find_nearest_first(const BVHTree *tree,
                                   const float co[3],
                                   float dist_sq,
                                   BVHTree_NearestPointCallback callback,
                                   void *userdata);

int BLI_bvhtree_ray_cast_ex(const BVHTree *tree,
                            const float co[3],
                            const float dir[3],
                            float radius,
                            BVHTreeRayHit *hit,
                            BVHTree_RayCastCallback callback,
                            void *userdata,
                            int flag);
int BLI_bvhtree_ray_cast(const BVHTree *tree,
                         const float co[3],
                         const float dir[3],
                         float radius,
                         BVHTreeRayHit *hit,
                         BVHTree_RayCastCallback callback,
                         void *userdata);

/**
 * Calls the callback for every ray intersection
 *
 * \note Using a \a callback which resets or never sets the #BVHTreeRayHit index & dist works too,
 * however using this function means existing generic callbacks can be used from custom callbacks
 * without having to handle resetting the hit beforehand.
 * It also avoid redundant argument and return value which aren't meaningful
 * when collecting multiple hits.
 */
void BLI_bvhtree_ray_cast_all_ex(const BVHTree *tree,
                                 const float co[3],
                                 const float dir[3],
                                 float radius,
                                 float hit_dist,
                                 BVHTree_RayCastCallback callback,
                                 void *userdata,
                                 int flag);
void BLI_bvhtree_ray_cast_all(const BVHTree *tree,
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

/**
 * Range query.
 */
int BLI_bvhtree_range_query(const BVHTree *tree,
                            const float co[3],
                            float radius,
                            BVHTree_RangeQuery callback,
                            void *userdata);

int BLI_bvhtree_find_nearest_projected(const BVHTree *tree,
                                       float projmat[4][4],
                                       float winsize[2],
                                       float mval[2],
                                       float clip_planes[6][4],
                                       int clip_plane_len,
                                       BVHTreeNearest *nearest,
                                       BVHTree_NearestProjectedCallback callback,
                                       void *userdata);

/**
 * This is a generic function to perform a depth first search on the #BVHTree
 * where the search order and nodes traversed depend on callbacks passed in.
 *
 * \param tree: Tree to walk.
 * \param walk_parent_cb: Callback on a parents bound-box to test if it should be traversed.
 * \param walk_leaf_cb: Callback to test leaf nodes, callback must store its own result,
 * returning false exits early.
 * \param walk_order_cb: Callback that indicates which direction to search,
 * either from the node with the lower or higher K-DOP axis value.
 * \param userdata: Argument passed to all callbacks.
 */
void BLI_bvhtree_walk_dfs(const BVHTree *tree,
                          BVHTree_WalkParentCallback walk_parent_cb,
                          BVHTree_WalkLeafCallback walk_leaf_cb,
                          BVHTree_WalkOrderCallback walk_order_cb,
                          void *userdata);

/**
 * Expose for BVH callbacks to use.
 */
extern const float bvhtree_kdop_axes[13][3];

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#  include "BLI_function_ref.hh"
#  include "BLI_math_vector.hh"

namespace blender {

using BVHTree_RayCastCallback_CPP =
    FunctionRef<void(int index, const BVHTreeRay &ray, BVHTreeRayHit &hit)>;

inline void BLI_bvhtree_ray_cast_all_cpp(const BVHTree &tree,
                                         const float3 co,
                                         const float3 dir,
                                         float radius,
                                         float hit_dist,
                                         BVHTree_RayCastCallback_CPP fn)
{
  BLI_bvhtree_ray_cast_all(
      &tree,
      co,
      dir,
      radius,
      hit_dist,
      [](void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit) {
        BVHTree_RayCastCallback_CPP fn = *static_cast<BVHTree_RayCastCallback_CPP *>(userdata);
        fn(index, *ray, *hit);
      },
      &fn);
}

using BVHTree_RangeQuery_CPP = FunctionRef<void(int index, const float3 &co, float dist_sq)>;

inline void BLI_bvhtree_range_query_cpp(const BVHTree &tree,
                                        const float3 co,
                                        float radius,
                                        BVHTree_RangeQuery_CPP fn)
{
  BLI_bvhtree_range_query(
      &tree,
      co,
      radius,
      [](void *userdata, const int index, const float co[3], const float dist_sq) {
        BVHTree_RangeQuery_CPP fn = *static_cast<BVHTree_RangeQuery_CPP *>(userdata);
        fn(index, co, dist_sq);
      },
      &fn);
}

}  // namespace blender

#endif
