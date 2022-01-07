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
 * \ingroup bke
 */

#include "BLI_bitmap.h"
#include "BLI_kdopbvh.h"
#include "BLI_threads.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This header encapsulates necessary code to build a BVH
 */

struct BMEditMesh;
struct MFace;
struct MVert;
struct Mesh;
struct PointCloud;

struct BVHCache;

/**
 * Struct that stores basic information about a BVHTree built from a edit-mesh.
 */
typedef struct BVHTreeFromEditMesh {
  struct BVHTree *tree;

  /** Default callbacks to BVH nearest and ray-cast. */
  BVHTree_NearestPointCallback nearest_callback;
  BVHTree_RayCastCallback raycast_callback;

  struct BMEditMesh *em;

  /* Private data */
  bool cached;

} BVHTreeFromEditMesh;

/**
 * Struct that stores basic information about a #BVHTree built from a mesh.
 */
typedef struct BVHTreeFromMesh {
  struct BVHTree *tree;

  /** Default callbacks to BVH nearest and ray-cast. */
  BVHTree_NearestPointCallback nearest_callback;
  BVHTree_RayCastCallback raycast_callback;

  /* Vertex array, so that callbacks have instant access to data. */
  const struct MVert *vert;
  const struct MEdge *edge; /* only used for #BVHTreeFromMeshEdges */
  const struct MFace *face;
  const struct MLoop *loop;
  const struct MLoopTri *looptri;
  bool vert_allocated;
  bool edge_allocated;
  bool face_allocated;
  bool loop_allocated;
  bool looptri_allocated;

  /* Private data */
  bool cached;

} BVHTreeFromMesh;

typedef enum BVHCacheType {
  BVHTREE_FROM_VERTS,
  BVHTREE_FROM_EDGES,
  BVHTREE_FROM_FACES,
  BVHTREE_FROM_LOOPTRI,
  BVHTREE_FROM_LOOPTRI_NO_HIDDEN,

  BVHTREE_FROM_LOOSEVERTS,
  BVHTREE_FROM_LOOSEEDGES,

  BVHTREE_FROM_EM_VERTS,
  BVHTREE_FROM_EM_EDGES,
  BVHTREE_FROM_EM_LOOPTRI,

  /* Keep `BVHTREE_MAX_ITEM` as last item. */
  BVHTREE_MAX_ITEM,
} BVHCacheType;

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
    BVHTreeFromEditMesh *data, struct BMEditMesh *em, float epsilon, int tree_type, int axis);

/**
 * Builds a BVH-tree where nodes are the vertices of the given `em`.
 */
BVHTree *bvhtree_from_editmesh_verts_ex(BVHTreeFromEditMesh *data,
                                        struct BMEditMesh *em,
                                        const BLI_bitmap *mask,
                                        int verts_num_active,
                                        float epsilon,
                                        int tree_type,
                                        int axis,
                                        const BVHCacheType bvh_cache_type,
                                        struct BVHCache **bvh_cache_p,
                                        ThreadMutex *mesh_eval_mutex);

/**
 * Builds a BVH-tree where nodes are the given vertices (NOTE: does not copy given `vert`!).
 * \param vert_allocated: if true, vert freeing will be done when freeing data.
 * \param verts_mask: if not null, true elements give which vert to add to BVH-tree.
 * \param verts_num_active: if >= 0, number of active verts to add to BVH-tree
 * (else will be computed from mask).
 */
BVHTree *bvhtree_from_mesh_verts_ex(struct BVHTreeFromMesh *data,
                                    const struct MVert *vert,
                                    int verts_num,
                                    bool vert_allocated,
                                    const BLI_bitmap *verts_mask,
                                    int verts_num_active,
                                    float epsilon,
                                    int tree_type,
                                    int axis,
                                    const BVHCacheType bvh_cache_type,
                                    struct BVHCache **bvh_cache_p,
                                    ThreadMutex *mesh_eval_mutex);

BVHTree *bvhtree_from_editmesh_edges(
    BVHTreeFromEditMesh *data, struct BMEditMesh *em, float epsilon, int tree_type, int axis);

/**
 * Builds a BVH-tree where nodes are the edges of the given `em`.
 */
BVHTree *bvhtree_from_editmesh_edges_ex(BVHTreeFromEditMesh *data,
                                        struct BMEditMesh *em,
                                        const BLI_bitmap *edges_mask,
                                        int edges_num_active,
                                        float epsilon,
                                        int tree_type,
                                        int axis,
                                        const BVHCacheType bvh_cache_type,
                                        struct BVHCache **bvh_cache_p,
                                        ThreadMutex *mesh_eval_mutex);

/**
 * Builds a BVH-tree where nodes are the given edges.
 * \param vert, vert_allocated: if true, elem freeing will be done when freeing data.
 * \param edge, edge_allocated: if true, elem freeing will be done when freeing data.
 * \param edges_mask: if not null, true elements give which vert to add to BVH-tree.
 * \param edges_num_active: if >= 0, number of active edges to add to BVH-tree
 * (else will be computed from mask).
 */
BVHTree *bvhtree_from_mesh_edges_ex(struct BVHTreeFromMesh *data,
                                    const struct MVert *vert,
                                    bool vert_allocated,
                                    const struct MEdge *edge,
                                    int edges_num,
                                    bool edge_allocated,
                                    const BLI_bitmap *edges_mask,
                                    int edges_num_active,
                                    float epsilon,
                                    int tree_type,
                                    int axis,
                                    const BVHCacheType bvh_cache_type,
                                    struct BVHCache **bvh_cache_p,
                                    ThreadMutex *mesh_eval_mutex);

/**
 * Builds a BVH-tree where nodes are the given tessellated faces
 * (NOTE: does not copy given mfaces!).
 * \param vert_allocated: if true, vert freeing will be done when freeing data.
 * \param face_allocated: if true, face freeing will be done when freeing data.
 * \param faces_mask: if not null, true elements give which faces to add to BVH-tree.
 * \param faces_num_active: if >= 0, number of active faces to add to BVH-tree
 * (else will be computed from mask).
 */
BVHTree *bvhtree_from_mesh_faces_ex(struct BVHTreeFromMesh *data,
                                    const struct MVert *vert,
                                    bool vert_allocated,
                                    const struct MFace *face,
                                    int numFaces,
                                    bool face_allocated,
                                    const BLI_bitmap *faces_mask,
                                    int faces_num_active,
                                    float epsilon,
                                    int tree_type,
                                    int axis,
                                    const BVHCacheType bvh_cache_type,
                                    struct BVHCache **bvh_cache_p,
                                    ThreadMutex *mesh_eval_mutex);

BVHTree *bvhtree_from_editmesh_looptri(
    BVHTreeFromEditMesh *data, struct BMEditMesh *em, float epsilon, int tree_type, int axis);

/**
 * Builds a BVH-tree where nodes are the `looptri` faces of the given `bm`.
 */
BVHTree *bvhtree_from_editmesh_looptri_ex(BVHTreeFromEditMesh *data,
                                          struct BMEditMesh *em,
                                          const BLI_bitmap *mask,
                                          int looptri_num_active,
                                          float epsilon,
                                          int tree_type,
                                          int axis,
                                          const BVHCacheType bvh_cache_type,
                                          struct BVHCache **bvh_cache_p,
                                          ThreadMutex *mesh_eval_mutex);

/**
 * Builds a BVH-tree where nodes are the looptri faces of the given mesh.
 *
 * \note for edit-mesh this is currently a duplicate of #bvhtree_from_mesh_faces_ex
 */
BVHTree *bvhtree_from_mesh_looptri_ex(struct BVHTreeFromMesh *data,
                                      const struct MVert *vert,
                                      bool vert_allocated,
                                      const struct MLoop *mloop,
                                      bool loop_allocated,
                                      const struct MLoopTri *looptri,
                                      int looptri_num,
                                      bool looptri_allocated,
                                      const BLI_bitmap *mask,
                                      int looptri_num_active,
                                      float epsilon,
                                      int tree_type,
                                      int axis,
                                      const BVHCacheType bvh_cache_type,
                                      struct BVHCache **bvh_cache_p,
                                      ThreadMutex *mesh_eval_mutex);

/**
 * Builds or queries a BVH-cache for the cache BVH-tree of the request type.
 *
 * \note This function only fills a cache, and therefore the mesh argument can
 * be considered logically const. Concurrent access is protected by a mutex.
 */
BVHTree *BKE_bvhtree_from_mesh_get(struct BVHTreeFromMesh *data,
                                   const struct Mesh *mesh,
                                   const BVHCacheType bvh_cache_type,
                                   int tree_type);

/**
 * Builds or queries a BVH-cache for the cache BVH-tree of the request type.
 */
BVHTree *BKE_bvhtree_from_editmesh_get(BVHTreeFromEditMesh *data,
                                       struct BMEditMesh *em,
                                       int tree_type,
                                       const BVHCacheType bvh_cache_type,
                                       struct BVHCache **bvh_cache_p,
                                       ThreadMutex *mesh_eval_mutex);

/**
 * Frees data allocated by a call to `bvhtree_from_editmesh_*`.
 */
void free_bvhtree_from_editmesh(struct BVHTreeFromEditMesh *data);
/**
 * Frees data allocated by a call to `bvhtree_from_mesh_*`.
 */
void free_bvhtree_from_mesh(struct BVHTreeFromMesh *data);

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

typedef struct BVHTreeFromPointCloud {
  struct BVHTree *tree;

  BVHTree_NearestPointCallback nearest_callback;

  const float (*coords)[3];
} BVHTreeFromPointCloud;

BVHTree *BKE_bvhtree_from_pointcloud_get(struct BVHTreeFromPointCloud *data,
                                         const struct PointCloud *pointcloud,
                                         int tree_type);

void free_bvhtree_from_pointcloud(struct BVHTreeFromPointCloud *data);

/**
 * BVHCache
 */

/* Using local coordinates */

bool bvhcache_has_tree(const struct BVHCache *bvh_cache, const BVHTree *tree);
struct BVHCache *bvhcache_init(void);
/**
 * Frees a BVH-cache.
 */
void bvhcache_free(struct BVHCache *bvh_cache);

#ifdef __cplusplus
}
#endif
