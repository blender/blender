/* SPDX-FileCopyrightText: 2006 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 *
 * This header encapsulates necessary code to build a BVH.
 */

#include <mutex>

#include "BLI_bit_vector.hh"
#include "BLI_kdopbvh.h"
#include "BLI_threads.h"

struct BMEditMesh;
struct BVHCache;
struct BVHTree;
struct MFace;
struct Mesh;
struct MLoopTri;
struct PointCloud;
struct vec2i;

/**
 * Struct that stores basic information about a BVHTree built from a edit-mesh.
 */
struct BVHTreeFromEditMesh {
  BVHTree *tree;

  /** Default callbacks to BVH nearest and ray-cast. */
  BVHTree_NearestPointCallback nearest_callback;
  BVHTree_RayCastCallback raycast_callback;

  /* Private data */
  bool cached;
};

/**
 * Struct that stores basic information about a #BVHTree built from a mesh.
 */
struct BVHTreeFromMesh {
  BVHTree *tree;

  /** Default callbacks to BVH nearest and ray-cast. */
  BVHTree_NearestPointCallback nearest_callback;
  BVHTree_RayCastCallback raycast_callback;

  /* Vertex array, so that callbacks have instant access to data. */
  const float (*vert_positions)[3];
  const vec2i *edge;
  const MFace *face;
  const int *corner_verts;
  const MLoopTri *looptri;

  /* Private data */
  bool cached;
};

enum BVHCacheType {
  BVHTREE_FROM_VERTS,
  BVHTREE_FROM_EDGES,
  BVHTREE_FROM_FACES,
  BVHTREE_FROM_LOOPTRI,
  BVHTREE_FROM_LOOPTRI_NO_HIDDEN,

  BVHTREE_FROM_LOOSEVERTS,
  BVHTREE_FROM_LOOSEEDGES,

  BVHTREE_FROM_EM_LOOSEVERTS,
  BVHTREE_FROM_EM_EDGES,
  BVHTREE_FROM_EM_LOOPTRI,

  /* Keep `BVHTREE_MAX_ITEM` as last item. */
  BVHTREE_MAX_ITEM,
};

/**
 * Builds a BVH tree where nodes are the relevant elements of the given mesh.
 * Configures #BVHTreeFromMesh.
 *
 * The tree is build in mesh space coordinates, this means special care must be made on queries
 * so that the coordinates and rays are first translated on the mesh local coordinates.
 * Reason for this is that bvh_from_mesh_* can use a cache in some cases and so it
 * becomes possible to reuse a #BVHTree.
 *
 * #free_bvhtree_from_mesh should be called when the tree is no longer needed.
 */
BVHTree *bvhtree_from_editmesh_verts(
    BVHTreeFromEditMesh *data, BMEditMesh *em, float epsilon, int tree_type, int axis);

/**
 * Builds a BVH-tree where nodes are the vertices of the given `em`.
 */
BVHTree *bvhtree_from_editmesh_verts_ex(BVHTreeFromEditMesh *data,
                                        BMEditMesh *em,
                                        blender::BitSpan mask,
                                        int verts_num_active,
                                        float epsilon,
                                        int tree_type,
                                        int axis);

/**
 * Builds a BVH-tree where nodes are the given vertices (NOTE: does not copy given `vert`!).
 * \param vert_allocated: if true, vert freeing will be done when freeing data.
 * \param verts_mask: if not null, true elements give which vert to add to BVH-tree.
 * \param verts_num_active: if >= 0, number of active verts to add to BVH-tree
 * (else will be computed from mask).
 */
BVHTree *bvhtree_from_mesh_verts_ex(BVHTreeFromMesh *data,
                                    const float (*vert_positions)[3],
                                    int verts_num,
                                    blender::BitSpan verts_mask,
                                    int verts_num_active,
                                    float epsilon,
                                    int tree_type,
                                    int axis);

BVHTree *bvhtree_from_editmesh_edges(
    BVHTreeFromEditMesh *data, BMEditMesh *em, float epsilon, int tree_type, int axis);

/**
 * Builds a BVH-tree where nodes are the edges of the given `em`.
 */
BVHTree *bvhtree_from_editmesh_edges_ex(BVHTreeFromEditMesh *data,
                                        BMEditMesh *em,
                                        blender::BitSpan edges_mask,
                                        int edges_num_active,
                                        float epsilon,
                                        int tree_type,
                                        int axis);

/**
 * Builds a BVH-tree where nodes are the given edges.
 * \param vert, vert_allocated: if true, elem freeing will be done when freeing data.
 * \param edge, edge_allocated: if true, elem freeing will be done when freeing data.
 * \param edges_mask: if not null, true elements give which vert to add to BVH-tree.
 * \param edges_num_active: if >= 0, number of active edges to add to BVH-tree
 * (else will be computed from mask).
 */
BVHTree *bvhtree_from_mesh_edges_ex(BVHTreeFromMesh *data,
                                    const float (*vert_positions)[3],
                                    const blender::int2 *edge,
                                    int edges_num,
                                    blender::BitSpan edges_mask,
                                    int edges_num_active,
                                    float epsilon,
                                    int tree_type,
                                    int axis);

BVHTree *bvhtree_from_editmesh_looptri(
    BVHTreeFromEditMesh *data, BMEditMesh *em, float epsilon, int tree_type, int axis);

/**
 * Builds a BVH-tree where nodes are the `looptri` faces of the given `bm`.
 */
BVHTree *bvhtree_from_editmesh_looptri_ex(BVHTreeFromEditMesh *data,
                                          BMEditMesh *em,
                                          blender::BitSpan mask,
                                          int looptri_num_active,
                                          float epsilon,
                                          int tree_type,
                                          int axis);

/**
 * Builds a BVH-tree where nodes are the looptri faces of the given mesh.
 */
BVHTree *bvhtree_from_mesh_looptri_ex(BVHTreeFromMesh *data,
                                      const float (*vert_positions)[3],
                                      const int *corner_verts,
                                      const MLoopTri *looptri,
                                      int looptri_num,
                                      blender::BitSpan mask,
                                      int looptri_num_active,
                                      float epsilon,
                                      int tree_type,
                                      int axis);

/**
 * Builds or queries a BVH-cache for the cache BVH-tree of the request type.
 *
 * \note This function only fills a cache, and therefore the mesh argument can
 * be considered logically const. Concurrent access is protected by a mutex.
 */
BVHTree *BKE_bvhtree_from_mesh_get(BVHTreeFromMesh *data,
                                   const Mesh *mesh,
                                   BVHCacheType bvh_cache_type,
                                   int tree_type);

/**
 * Builds or queries a BVH-cache for the cache BVH-tree of the request type.
 */
BVHTree *BKE_bvhtree_from_editmesh_get(BVHTreeFromEditMesh *data,
                                       BMEditMesh *em,
                                       int tree_type,
                                       BVHCacheType bvh_cache_type,
                                       BVHCache **bvh_cache_p,
                                       std::mutex *mesh_eval_mutex);

/**
 * Frees data allocated by a call to `bvhtree_from_editmesh_*`.
 */
void free_bvhtree_from_editmesh(BVHTreeFromEditMesh *data);
/**
 * Frees data allocated by a call to `bvhtree_from_mesh_*`.
 */
void free_bvhtree_from_mesh(BVHTreeFromMesh *data);

/**
 * Math functions used by callbacks
 */
float bvhtree_ray_tri_intersection(
    const BVHTreeRay *ray, float m_dist, const float v0[3], const float v1[3], const float v2[3]);
float bvhtree_sphereray_tri_intersection(const BVHTreeRay *ray,
                                         float radius,
                                         float m_dist,
                                         const float v0[3],
                                         const float v1[3],
                                         const float v2[3]);

struct BVHTreeFromPointCloud {
  BVHTree *tree;

  BVHTree_NearestPointCallback nearest_callback;

  const float (*coords)[3];
};

[[nodiscard]] BVHTree *BKE_bvhtree_from_pointcloud_get(BVHTreeFromPointCloud *data,
                                                       const PointCloud *pointcloud,
                                                       int tree_type);

void free_bvhtree_from_pointcloud(BVHTreeFromPointCloud *data);

/**
 * BVHCache
 */

/* Using local coordinates */

bool bvhcache_has_tree(const BVHCache *bvh_cache, const BVHTree *tree);
BVHCache *bvhcache_init();
/**
 * Frees a BVH-cache.
 */
void bvhcache_free(BVHCache *bvh_cache);
