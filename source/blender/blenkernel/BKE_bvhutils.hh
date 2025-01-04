/* SPDX-FileCopyrightText: 2006 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 *
 * This header encapsulates necessary code to build a BVH.
 */

#include "BLI_index_mask_fwd.hh"
#include "BLI_kdopbvh.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_span.hh"

struct BVHTree;
struct MFace;
struct Mesh;
struct PointCloud;

namespace blender::bke {

/**
 * Struct that stores basic information about a #BVHTree built from a mesh.
 */
struct BVHTreeFromMesh {
  const BVHTree *tree = nullptr;

  /** Default callbacks to BVH nearest and ray-cast. */
  BVHTree_NearestPointCallback nearest_callback;
  BVHTree_RayCastCallback raycast_callback;

  /* Vertex array, so that callbacks have instant access to data. */
  Span<float3> vert_positions;
  Span<int2> edges;
  Span<int> corner_verts;
  Span<int3> corner_tris;

  const MFace *face = nullptr;

  std::unique_ptr<BVHTree, BVHTreeDeleter> owned_tree;
};

/**
 * Builds a BVH-tree where nodes are the given vertices.
 */
BVHTreeFromMesh bvhtree_from_mesh_verts_ex(Span<float3> vert_positions,
                                           const IndexMask &verts_mask);

/**
 * Builds a BVH-tree where nodes are the given edges.
 */
BVHTreeFromMesh bvhtree_from_mesh_edges_ex(Span<float3> vert_positions,
                                           Span<int2> edges,
                                           const IndexMask &edges_mask);

/**
 * Builds a BVH-tree where nodes are the triangle faces (#Mesh::corner_tris()) of the given mesh.
 */
BVHTreeFromMesh bvhtree_from_mesh_corner_tris_ex(Span<float3> vert_positions,
                                                 OffsetIndices<int> faces,
                                                 Span<int> corner_verts,
                                                 Span<int3> corner_tris,
                                                 const IndexMask &faces_mask);

/**
 * Build a BVH-tree from the triangles in the mesh that correspond to the faces in the given mask.
 */
BVHTreeFromMesh bvhtree_from_mesh_tris_init(const Mesh &mesh, const IndexMask &faces_mask);

/**
 * Build a BVH-tree containing the given edges.
 */
BVHTreeFromMesh bvhtree_from_mesh_edges_init(const Mesh &mesh, const IndexMask &edges_mask);

/**
 * Build a BVH-tree containing the given vertices.
 */
BVHTreeFromMesh bvhtree_from_mesh_verts_init(const Mesh &mesh, const IndexMask &verts_mask);

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
  std::unique_ptr<BVHTree, BVHTreeDeleter> tree;

  BVHTree_NearestPointCallback nearest_callback;

  const float (*coords)[3];
};

BVHTreeFromPointCloud bvhtree_from_pointcloud_get(const PointCloud &pointcloud,
                                                  const IndexMask &points_mask);

}  // namespace blender::bke
