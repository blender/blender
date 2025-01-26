/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_math_geom.h"

#include "BKE_attribute.hh"
#include "BKE_bvhutils.hh"
#include "BKE_editmesh.hh"
#include "BKE_mesh.hh"

namespace blender::bke {

/* -------------------------------------------------------------------- */
/** \name Local Callbacks
 * \{ */

/* Math stuff for ray casting on mesh faces and for nearest surface */

float bvhtree_ray_tri_intersection(const BVHTreeRay *ray,
                                   const float /*m_dist*/,
                                   const float v0[3],
                                   const float v1[3],
                                   const float v2[3])
{
  float dist;

#ifdef USE_KDOPBVH_WATERTIGHT
  if (isect_ray_tri_watertight_v3(ray->origin, ray->isect_precalc, v0, v1, v2, &dist, nullptr))
#else
  if (isect_ray_tri_epsilon_v3(
          ray->origin, ray->direction, v0, v1, v2, &dist, nullptr, FLT_EPSILON))
#endif
  {
    return dist;
  }

  return FLT_MAX;
}

float bvhtree_sphereray_tri_intersection(const BVHTreeRay *ray,
                                         float radius,
                                         const float m_dist,
                                         const float v0[3],
                                         const float v1[3],
                                         const float v2[3])
{

  float idist;
  float p1[3];
  float hit_point[3];

  madd_v3_v3v3fl(p1, ray->origin, ray->direction, m_dist);
  if (isect_sweeping_sphere_tri_v3(ray->origin, p1, radius, v0, v1, v2, &idist, hit_point)) {
    return idist * m_dist;
  }

  return FLT_MAX;
}

/*
 * BVH from meshes callbacks
 */

/**
 * Callback to BVH-tree nearest point.
 * The tree must have been built using #bvhtree_from_mesh_faces.
 *
 * \param userdata: Must be a #BVHMeshCallbackUserdata built from the same mesh as the tree.
 */
static void mesh_faces_nearest_point(void *userdata,
                                     int index,
                                     const float co[3],
                                     BVHTreeNearest *nearest)
{
  const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
  const MFace *face = data->face + index;

  const float *t0, *t1, *t2, *t3;
  t0 = data->vert_positions[face->v1];
  t1 = data->vert_positions[face->v2];
  t2 = data->vert_positions[face->v3];
  t3 = face->v4 ? &data->vert_positions[face->v4].x : nullptr;

  do {
    float nearest_tmp[3], dist_sq;

    closest_on_tri_to_point_v3(nearest_tmp, co, t0, t1, t2);
    dist_sq = len_squared_v3v3(co, nearest_tmp);

    if (dist_sq < nearest->dist_sq) {
      nearest->index = index;
      nearest->dist_sq = dist_sq;
      copy_v3_v3(nearest->co, nearest_tmp);
      normal_tri_v3(nearest->no, t0, t1, t2);
    }

    t1 = t2;
    t2 = t3;
    t3 = nullptr;

  } while (t2);
}
/* copy of function above */
static void mesh_corner_tris_nearest_point(void *userdata,
                                           int index,
                                           const float co[3],
                                           BVHTreeNearest *nearest)
{
  const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
  const int3 &tri = data->corner_tris[index];
  const float *vtri_co[3] = {
      data->vert_positions[data->corner_verts[tri[0]]],
      data->vert_positions[data->corner_verts[tri[1]]],
      data->vert_positions[data->corner_verts[tri[2]]],
  };
  float nearest_tmp[3], dist_sq;

  closest_on_tri_to_point_v3(nearest_tmp, co, UNPACK3(vtri_co));
  dist_sq = len_squared_v3v3(co, nearest_tmp);

  if (dist_sq < nearest->dist_sq) {
    nearest->index = index;
    nearest->dist_sq = dist_sq;
    copy_v3_v3(nearest->co, nearest_tmp);
    normal_tri_v3(nearest->no, UNPACK3(vtri_co));
  }
}

/**
 * Callback to BVH-tree ray-cast.
 * The tree must have been built using bvhtree_from_mesh_faces.
 *
 * \param userdata: Must be a #BVHMeshCallbackUserdata built from the same mesh as the tree.
 */
static void mesh_faces_spherecast(void *userdata,
                                  int index,
                                  const BVHTreeRay *ray,
                                  BVHTreeRayHit *hit)
{
  const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
  const MFace *face = &data->face[index];

  const float *t0, *t1, *t2, *t3;
  t0 = data->vert_positions[face->v1];
  t1 = data->vert_positions[face->v2];
  t2 = data->vert_positions[face->v3];
  t3 = face->v4 ? &data->vert_positions[face->v4].x : nullptr;

  do {
    float dist;
    if (ray->radius == 0.0f) {
      dist = bvhtree_ray_tri_intersection(ray, hit->dist, t0, t1, t2);
    }
    else {
      dist = bvhtree_sphereray_tri_intersection(ray, ray->radius, hit->dist, t0, t1, t2);
    }

    if (dist >= 0 && dist < hit->dist) {
      hit->index = index;
      hit->dist = dist;
      madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);

      normal_tri_v3(hit->no, t0, t1, t2);
    }

    t1 = t2;
    t2 = t3;
    t3 = nullptr;

  } while (t2);
}
/* copy of function above */
static void mesh_corner_tris_spherecast(void *userdata,
                                        int index,
                                        const BVHTreeRay *ray,
                                        BVHTreeRayHit *hit)
{
  const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
  const Span<float3> positions = data->vert_positions;
  const int3 &tri = data->corner_tris[index];
  const float *vtri_co[3] = {
      positions[data->corner_verts[tri[0]]],
      positions[data->corner_verts[tri[1]]],
      positions[data->corner_verts[tri[2]]],
  };
  float dist;

  if (ray->radius == 0.0f) {
    dist = bvhtree_ray_tri_intersection(ray, hit->dist, UNPACK3(vtri_co));
  }
  else {
    dist = bvhtree_sphereray_tri_intersection(ray, ray->radius, hit->dist, UNPACK3(vtri_co));
  }

  if (dist >= 0 && dist < hit->dist) {
    hit->index = index;
    hit->dist = dist;
    madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);

    normal_tri_v3(hit->no, UNPACK3(vtri_co));
  }
}

/**
 * Callback to BVH-tree nearest point.
 * The tree must have been built using #bvhtree_from_mesh_edges.
 *
 * \param userdata: Must be a #BVHMeshCallbackUserdata built from the same mesh as the tree.
 */
static void mesh_edges_nearest_point(void *userdata,
                                     int index,
                                     const float co[3],
                                     BVHTreeNearest *nearest)
{
  const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
  const Span<float3> positions = data->vert_positions;
  const int2 edge = data->edges[index];
  float nearest_tmp[3], dist_sq;

  const float *t0, *t1;
  t0 = positions[edge[0]];
  t1 = positions[edge[1]];

  closest_to_line_segment_v3(nearest_tmp, co, t0, t1);
  dist_sq = len_squared_v3v3(nearest_tmp, co);

  if (dist_sq < nearest->dist_sq) {
    nearest->index = index;
    nearest->dist_sq = dist_sq;
    copy_v3_v3(nearest->co, nearest_tmp);
    sub_v3_v3v3(nearest->no, t0, t1);
    normalize_v3(nearest->no);
  }
}

/* Helper, does all the point-sphere-cast work actually. */
static void mesh_verts_spherecast_do(int index,
                                     const float v[3],
                                     const BVHTreeRay *ray,
                                     BVHTreeRayHit *hit)
{
  const float *r1;
  float r2[3], i1[3];
  r1 = ray->origin;
  add_v3_v3v3(r2, r1, ray->direction);

  closest_to_line_segment_v3(i1, v, r1, r2);

  /* No hit if closest point is 'behind' the origin of the ray, or too far away from it. */
  if (dot_v3v3v3(r1, i1, r2) >= 0.0f) {
    const float dist = len_v3v3(r1, i1);
    if (dist < hit->dist) {
      hit->index = index;
      hit->dist = dist;
      copy_v3_v3(hit->co, i1);
    }
  }
}

/**
 * Callback to BVH-tree ray-cast.
 * The tree must have been built using bvhtree_from_mesh_verts.
 *
 * \param userdata: Must be a #BVHMeshCallbackUserdata built from the same mesh as the tree.
 */
static void mesh_verts_spherecast(void *userdata,
                                  int index,
                                  const BVHTreeRay *ray,
                                  BVHTreeRayHit *hit)
{
  const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
  const float *v = data->vert_positions[index];

  mesh_verts_spherecast_do(index, v, ray, hit);
}

/**
 * Callback to BVH-tree ray-cast.
 * The tree must have been built using bvhtree_from_mesh_edges.
 *
 * \param userdata: Must be a #BVHMeshCallbackUserdata built from the same mesh as the tree.
 */
static void mesh_edges_spherecast(void *userdata,
                                  int index,
                                  const BVHTreeRay *ray,
                                  BVHTreeRayHit *hit)
{
  const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
  const Span<float3> positions = data->vert_positions;
  const int2 edge = data->edges[index];

  const float radius_sq = square_f(ray->radius);
  const float *v1, *v2, *r1;
  float r2[3], i1[3], i2[3];
  v1 = positions[edge[0]];
  v2 = positions[edge[1]];

  /* In case we get a zero-length edge, handle it as a point! */
  if (equals_v3v3(v1, v2)) {
    mesh_verts_spherecast_do(index, v1, ray, hit);
    return;
  }

  r1 = ray->origin;
  add_v3_v3v3(r2, r1, ray->direction);

  if (isect_line_line_v3(v1, v2, r1, r2, i1, i2)) {
    /* No hit if intersection point is 'behind' the origin of the ray, or too far away from it. */
    if (dot_v3v3v3(r1, i2, r2) >= 0.0f) {
      const float dist = len_v3v3(r1, i2);
      if (dist < hit->dist) {
        const float e_fac = line_point_factor_v3(i1, v1, v2);
        if (e_fac < 0.0f) {
          copy_v3_v3(i1, v1);
        }
        else if (e_fac > 1.0f) {
          copy_v3_v3(i1, v2);
        }
        /* Ensure ray is really close enough from edge! */
        if (len_squared_v3v3(i1, i2) <= radius_sq) {
          hit->index = index;
          hit->dist = dist;
          copy_v3_v3(hit->co, i2);
        }
      }
    }
  }
}

/** \} */

/*
 * BVH builders
 */

/* -------------------------------------------------------------------- */
/** \name Common Utils
 * \{ */

static BVHTreeFromMesh create_verts_tree_data(const BVHTree *tree, const Span<float3> positions)
{
  BVHTreeFromMesh data{};
  data.tree = tree;
  data.vert_positions = positions;
  /* a nullptr nearest callback works fine
   * remember the min distance to point is the same as the min distance to BV of point */
  data.nearest_callback = nullptr;
  data.raycast_callback = mesh_verts_spherecast;
  return data;
}

static BVHTreeFromMesh create_verts_tree_data(std::unique_ptr<BVHTree, BVHTreeDeleter> tree,
                                              const Span<float3> positions)
{
  BVHTreeFromMesh data = create_verts_tree_data(tree.get(), positions);
  data.owned_tree = std::move(tree);
  return data;
}

static BVHTreeFromMesh create_edges_tree_data(const BVHTree *tree,
                                              const Span<float3> positions,
                                              const Span<int2> edges)
{
  BVHTreeFromMesh data{};
  data.tree = tree;
  data.vert_positions = positions;
  data.edges = edges;
  data.nearest_callback = mesh_edges_nearest_point;
  data.raycast_callback = mesh_edges_spherecast;
  return data;
}

static BVHTreeFromMesh create_edges_tree_data(std::unique_ptr<BVHTree, BVHTreeDeleter> tree,
                                              const Span<float3> positions,
                                              const Span<int2> edges)
{
  BVHTreeFromMesh data = create_edges_tree_data(tree.get(), positions, edges);
  data.owned_tree = std::move(tree);
  return data;
}

static BVHTreeFromMesh create_legacy_faces_tree_data(const BVHTree *tree,
                                                     const Span<float3> positions,
                                                     const MFace *face)
{
  BVHTreeFromMesh data{};
  data.tree = tree;
  data.vert_positions = positions;
  data.face = face;
  data.nearest_callback = mesh_faces_nearest_point;
  data.raycast_callback = mesh_faces_spherecast;
  return data;
}

static BVHTreeFromMesh create_tris_tree_data(const BVHTree *tree,
                                             const Span<float3> positions,
                                             const Span<int> corner_verts,
                                             const Span<int3> corner_tris)
{
  BVHTreeFromMesh data{};
  data.tree = tree;
  data.vert_positions = positions;
  data.corner_verts = corner_verts;
  data.corner_tris = corner_tris;
  data.nearest_callback = mesh_corner_tris_nearest_point;
  data.raycast_callback = mesh_corner_tris_spherecast;
  return data;
}

static BVHTreeFromMesh create_tris_tree_data(std::unique_ptr<BVHTree, BVHTreeDeleter> tree,
                                             const Span<float3> positions,
                                             const Span<int> corner_verts,
                                             const Span<int3> corner_tris)
{
  BVHTreeFromMesh data = create_tris_tree_data(tree.get(), positions, corner_verts, corner_tris);
  data.owned_tree = std::move(tree);
  return data;
}

static std::unique_ptr<BVHTree, BVHTreeDeleter> bvhtree_new_common(int elems_num)
{
  if (elems_num == 0) {
    return nullptr;
  }
  return std::unique_ptr<BVHTree, BVHTreeDeleter>(BLI_bvhtree_new(elems_num, 0.0f, 2, 6));
}

static std::unique_ptr<BVHTree, BVHTreeDeleter> create_tree_from_verts(
    const Span<float3> positions, const IndexMask &verts_mask)
{
  std::unique_ptr<BVHTree, BVHTreeDeleter> tree = bvhtree_new_common(verts_mask.size());
  if (!tree) {
    return nullptr;
  }
  verts_mask.foreach_index(
      [&](const int i) { BLI_bvhtree_insert(tree.get(), i, positions[i], 1); });
  BLI_bvhtree_balance(tree.get());
  return tree;
}

BVHTreeFromMesh bvhtree_from_mesh_verts_ex(const Span<float3> vert_positions,
                                           const IndexMask &verts_mask)
{
  return create_verts_tree_data(create_tree_from_verts(vert_positions, verts_mask),
                                vert_positions);
}

static std::unique_ptr<BVHTree, BVHTreeDeleter> create_tree_from_edges(
    const Span<float3> positions, const Span<int2> edges, const IndexMask &edges_mask)
{
  std::unique_ptr<BVHTree, BVHTreeDeleter> tree = bvhtree_new_common(edges_mask.size());
  if (!tree) {
    return nullptr;
  }
  edges_mask.foreach_index([&](const int edge_i) {
    const int2 &edge = edges[edge_i];
    float co[2][3];
    copy_v3_v3(co[0], positions[edge[0]]);
    copy_v3_v3(co[1], positions[edge[1]]);
    BLI_bvhtree_insert(tree.get(), edge_i, co[0], 2);
  });
  BLI_bvhtree_balance(tree.get());
  return tree;
}

BVHTreeFromMesh bvhtree_from_mesh_edges_ex(const Span<float3> vert_positions,
                                           const Span<int2> edges,
                                           const IndexMask &edges_mask)
{
  return create_edges_tree_data(
      create_tree_from_edges(vert_positions, edges, edges_mask), vert_positions, edges);
}

static std::unique_ptr<BVHTree, BVHTreeDeleter> create_tree_from_legacy_faces(
    const Span<float3> positions, const Span<MFace> faces)
{
  std::unique_ptr<BVHTree, BVHTreeDeleter> tree = bvhtree_new_common(faces.size());
  if (!tree) {
    return nullptr;
  }
  for (const int i : faces.index_range()) {
    float co[4][3];
    copy_v3_v3(co[0], positions[faces[i].v1]);
    copy_v3_v3(co[1], positions[faces[i].v2]);
    copy_v3_v3(co[2], positions[faces[i].v3]);
    if (faces[i].v4) {
      copy_v3_v3(co[3], positions[faces[i].v4]);
    }
    BLI_bvhtree_insert(tree.get(), i, co[0], faces[i].v4 ? 4 : 3);
  }
  BLI_bvhtree_balance(tree.get());
  return tree;
}

static std::unique_ptr<BVHTree, BVHTreeDeleter> create_tree_from_tris(const Span<float3> positions,
                                                                      const Span<int> corner_verts,
                                                                      const Span<int3> corner_tris)
{
  std::unique_ptr<BVHTree, BVHTreeDeleter> tree = bvhtree_new_common(corner_tris.size());
  if (!tree) {
    return {};
  }
  for (const int tri : corner_tris.index_range()) {
    float co[3][3];
    copy_v3_v3(co[0], positions[corner_verts[corner_tris[tri][0]]]);
    copy_v3_v3(co[1], positions[corner_verts[corner_tris[tri][1]]]);
    copy_v3_v3(co[2], positions[corner_verts[corner_tris[tri][2]]]);
    BLI_bvhtree_insert(tree.get(), tri, co[0], 3);
  }
  BLI_bvhtree_balance(tree.get());
  return tree;
}

static std::unique_ptr<BVHTree, BVHTreeDeleter> create_tree_from_tris(
    const Span<float3> positions,
    const OffsetIndices<int> faces,
    const Span<int> corner_verts,
    const Span<int3> corner_tris,
    const IndexMask &faces_mask)
{
  if (faces_mask.size() == faces.size()) {
    /* Avoid accessing face offsets if the selection is full. */
    return create_tree_from_tris(positions, corner_verts, corner_tris);
  }

  int tris_num = 0;
  faces_mask.foreach_index_optimized<int>(
      [&](const int i) { tris_num += mesh::face_triangles_num(faces[i].size()); });

  std::unique_ptr<BVHTree, BVHTreeDeleter> tree = bvhtree_new_common(tris_num);
  if (!tree) {
    return {};
  }
  faces_mask.foreach_index([&](const int face) {
    for (const int tri : mesh::face_triangles_range(faces, face)) {
      float co[3][3];
      copy_v3_v3(co[0], positions[corner_verts[corner_tris[tri][0]]]);
      copy_v3_v3(co[1], positions[corner_verts[corner_tris[tri][1]]]);
      copy_v3_v3(co[2], positions[corner_verts[corner_tris[tri][2]]]);
      BLI_bvhtree_insert(tree.get(), tri, co[0], 3);
    }
  });
  BLI_bvhtree_balance(tree.get());
  return tree;
}

BVHTreeFromMesh bvhtree_from_mesh_corner_tris_ex(const Span<float3> vert_positions,
                                                 const OffsetIndices<int> faces,
                                                 const Span<int> corner_verts,
                                                 const Span<int3> corner_tris,
                                                 const IndexMask &faces_mask)
{
  return create_tris_tree_data(
      create_tree_from_tris(vert_positions, faces, corner_verts, corner_tris, faces_mask),
      vert_positions,
      corner_verts,
      corner_tris);
}

static BitVector<> loose_verts_no_hidden_mask_get(const Mesh &mesh)
{
  int count = mesh.verts_num;
  BitVector<> verts_mask(count, true);

  const AttributeAccessor attributes = mesh.attributes();
  const Span<int2> edges = mesh.edges();
  const VArray<bool> hide_edge = *attributes.lookup_or_default(
      ".hide_edge", AttrDomain::Edge, false);
  const VArray<bool> hide_vert = *attributes.lookup_or_default(
      ".hide_vert", AttrDomain::Point, false);

  for (const int i : edges.index_range()) {
    if (hide_edge[i]) {
      continue;
    }
    for (const int vert : {edges[i][0], edges[i][1]}) {
      if (verts_mask[vert]) {
        verts_mask[vert].reset();
        count--;
      }
    }
  }

  if (count) {
    for (const int vert : verts_mask.index_range()) {
      if (verts_mask[vert] && hide_vert[vert]) {
        verts_mask[vert].reset();
      }
    }
  }

  return verts_mask;
}

static BitVector<> loose_edges_no_hidden_mask_get(const Mesh &mesh)
{
  int count = mesh.edges_num;
  BitVector<> edge_mask(count, true);

  const AttributeAccessor attributes = mesh.attributes();
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_edges = mesh.corner_edges();
  const VArray<bool> hide_poly = *attributes.lookup_or_default(
      ".hide_poly", AttrDomain::Face, false);
  const VArray<bool> hide_edge = *attributes.lookup_or_default(
      ".hide_edge", AttrDomain::Edge, false);

  for (const int i : faces.index_range()) {
    if (hide_poly[i]) {
      continue;
    }
    for (const int edge : corner_edges.slice(faces[i])) {
      if (edge_mask[edge]) {
        edge_mask[edge].reset();
        count--;
      }
    }
  }

  if (count) {
    for (const int edge : edge_mask.index_range()) {
      if (edge_mask[edge] && hide_edge[edge]) {
        edge_mask[edge].reset();
      }
    }
  }

  return edge_mask;
}

}  // namespace blender::bke

blender::bke::BVHTreeFromMesh Mesh::bvh_loose_verts() const
{
  using namespace blender;
  using namespace blender::bke;
  const Span<float3> positions = this->vert_positions();
  this->runtime->bvh_cache_loose_verts.ensure([&](std::unique_ptr<BVHTree, BVHTreeDeleter> &data) {
    const LooseVertCache &loose_verts = this->loose_verts();
    IndexMaskMemory memory;
    const IndexMask mask = IndexMask::from_bits(loose_verts.is_loose_bits, memory);
    data = create_tree_from_verts(positions, mask);
  });
  return create_verts_tree_data(this->runtime->bvh_cache_loose_verts.data().get(), positions);
}

blender::bke::BVHTreeFromMesh Mesh::bvh_loose_no_hidden_verts() const
{
  using namespace blender;
  using namespace blender::bke;
  const Span<float3> positions = this->vert_positions();
  this->runtime->bvh_cache_loose_verts_no_hidden.ensure(
      [&](std::unique_ptr<BVHTree, BVHTreeDeleter> &data) {
        const BitVector<> mask = loose_verts_no_hidden_mask_get(*this);
        IndexMaskMemory memory;
        data = create_tree_from_verts(positions, IndexMask::from_bits(mask, memory));
      });
  return create_verts_tree_data(this->runtime->bvh_cache_loose_verts_no_hidden.data().get(),
                                positions);
}

blender::bke::BVHTreeFromMesh Mesh::bvh_verts() const
{
  using namespace blender;
  using namespace blender::bke;
  const Span<float3> positions = this->vert_positions();
  this->runtime->bvh_cache_verts.ensure([&](std::unique_ptr<BVHTree, BVHTreeDeleter> &data) {
    data = create_tree_from_verts(positions, positions.index_range());
  });
  return create_verts_tree_data(this->runtime->bvh_cache_verts.data().get(), positions);
}

blender::bke::BVHTreeFromMesh Mesh::bvh_loose_edges() const
{
  using namespace blender;
  using namespace blender::bke;
  const Span<float3> positions = this->vert_positions();
  const Span<int2> edges = this->edges();
  this->runtime->bvh_cache_loose_edges.ensure([&](std::unique_ptr<BVHTree, BVHTreeDeleter> &data) {
    const LooseEdgeCache &loose_edges = this->loose_edges();
    IndexMaskMemory memory;
    const IndexMask mask = IndexMask::from_bits(loose_edges.is_loose_bits, memory);
    data = create_tree_from_edges(positions, edges, mask);
  });
  return create_edges_tree_data(
      this->runtime->bvh_cache_loose_edges.data().get(), positions, edges);
}

blender::bke::BVHTreeFromMesh Mesh::bvh_loose_no_hidden_edges() const
{
  using namespace blender;
  using namespace blender::bke;
  const Span<float3> positions = this->vert_positions();
  const Span<int2> edges = this->edges();
  this->runtime->bvh_cache_loose_edges_no_hidden.ensure(
      [&](std::unique_ptr<BVHTree, BVHTreeDeleter> &data) {
        const BitVector<> mask = loose_edges_no_hidden_mask_get(*this);
        IndexMaskMemory memory;
        data = create_tree_from_edges(positions, edges, IndexMask::from_bits(mask, memory));
      });
  return create_edges_tree_data(
      this->runtime->bvh_cache_loose_edges_no_hidden.data().get(), positions, edges);
}

blender::bke::BVHTreeFromMesh Mesh::bvh_edges() const
{
  using namespace blender;
  using namespace blender::bke;
  const Span<float3> positions = this->vert_positions();
  const Span<int2> edges = this->edges();
  this->runtime->bvh_cache_edges.ensure([&](std::unique_ptr<BVHTree, BVHTreeDeleter> &data) {
    data = create_tree_from_edges(positions, edges, edges.index_range());
  });
  return create_edges_tree_data(this->runtime->bvh_cache_edges.data().get(), positions, edges);
}

blender::bke::BVHTreeFromMesh Mesh::bvh_legacy_faces() const
{
  using namespace blender;
  using namespace blender::bke;
  BLI_assert(!(this->totface_legacy == 0 && this->faces_num != 0));
  Span legacy_faces{
      static_cast<const MFace *>(CustomData_get_layer(&this->fdata_legacy, CD_MFACE)),
      this->totface_legacy};
  const Span<float3> positions = this->vert_positions();
  this->runtime->bvh_cache_faces.ensure([&](std::unique_ptr<BVHTree, BVHTreeDeleter> &data) {
    data = create_tree_from_legacy_faces(positions, legacy_faces);
  });
  return create_legacy_faces_tree_data(
      this->runtime->bvh_cache_faces.data().get(), positions, legacy_faces.data());
}

blender::bke::BVHTreeFromMesh Mesh::bvh_corner_tris_no_hidden() const
{
  using namespace blender;
  using namespace blender::bke;
  const Span<float3> positions = this->vert_positions();
  const Span<int> corner_verts = this->corner_verts();
  const Span<int3> corner_tris = this->corner_tris();
  const AttributeAccessor attributes = this->attributes();
  const VArray hide_poly = *attributes.lookup<bool>(".hide_poly", AttrDomain::Face);
  if (!hide_poly) {
    return this->bvh_corner_tris();
  }
  this->runtime->bvh_cache_corner_tris_no_hidden.ensure(
      [&](std::unique_ptr<BVHTree, BVHTreeDeleter> &data) {
        const OffsetIndices<int> faces = this->faces();
        IndexMaskMemory memory;
        const IndexMask visible_faces = IndexMask::from_bools_inverse(
            faces.index_range(), VArraySpan(hide_poly), memory);
        data = create_tree_from_tris(positions, faces, corner_verts, corner_tris, visible_faces);
      });
  return create_tris_tree_data(this->runtime->bvh_cache_corner_tris_no_hidden.data().get(),
                               positions,
                               corner_verts,
                               corner_tris);
}

blender::bke::BVHTreeFromMesh Mesh::bvh_corner_tris() const
{
  using namespace blender;
  using namespace blender::bke;
  const Span<float3> positions = this->vert_positions();
  const Span<int> corner_verts = this->corner_verts();
  const Span<int3> corner_tris = this->corner_tris();
  this->runtime->bvh_cache_corner_tris.ensure([&](std::unique_ptr<BVHTree, BVHTreeDeleter> &data) {
    data = create_tree_from_tris(positions, corner_verts, corner_tris);
  });
  return create_tris_tree_data(
      this->runtime->bvh_cache_corner_tris.data().get(), positions, corner_verts, corner_tris);
}

namespace blender::bke {

BVHTreeFromMesh bvhtree_from_mesh_tris_init(const Mesh &mesh, const IndexMask &faces_mask)
{
  if (faces_mask.size() == mesh.faces_num) {
    return mesh.bvh_corner_tris();
  }
  return bvhtree_from_mesh_corner_tris_ex(
      mesh.vert_positions(), mesh.faces(), mesh.corner_verts(), mesh.corner_tris(), faces_mask);
}

BVHTreeFromMesh bvhtree_from_mesh_edges_init(const Mesh &mesh, const IndexMask &edges_mask)
{
  if (edges_mask.size() == mesh.edges_num) {
    return mesh.bvh_edges();
  }
  return bvhtree_from_mesh_edges_ex(mesh.vert_positions(), mesh.edges(), edges_mask);
}

BVHTreeFromMesh bvhtree_from_mesh_verts_init(const Mesh &mesh, const IndexMask &verts_mask)
{
  if (verts_mask.size() == mesh.verts_num) {
    return mesh.bvh_verts();
  }
  return bvhtree_from_mesh_verts_ex(mesh.vert_positions(), verts_mask);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Point Cloud BVH Building
 * \{ */

BVHTreeFromPointCloud bvhtree_from_pointcloud_get(const PointCloud &pointcloud,
                                                  const IndexMask &points_mask)
{
  const Span<float3> positions = pointcloud.positions();
  std::unique_ptr<BVHTree, BVHTreeDeleter> tree = create_tree_from_verts(positions, points_mask);
  if (!tree) {
    return {};
  }
  BVHTreeFromPointCloud data{};
  data.tree = std::move(tree);
  data.nearest_callback = nullptr;
  data.coords = (const float(*)[3])positions.data();
  return data;
}

/** \} */

}  // namespace blender::bke
