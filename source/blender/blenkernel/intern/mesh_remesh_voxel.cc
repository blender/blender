/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_index_range.hh"
#include "BLI_math_vector.h"
#include "BLI_span.hh"
#include "BLI_task.hh"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_bvhutils.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_remesh_voxel.hh" /* own include */
#include "BKE_mesh_sample.hh"
#include "BKE_modifier.hh"
#include "BKE_report.hh"

#include "bmesh.hh"
#include "bmesh_tools.hh"

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/MeshToVolume.h>
#  include <openvdb/tools/VolumeToMesh.h>
#endif

#ifdef WITH_QUADRIFLOW
#  include "quadriflow_capi.hpp"
#endif

using blender::Array;
using blender::float3;
using blender::IndexRange;
using blender::int3;
using blender::MutableSpan;
using blender::Span;

#ifdef WITH_QUADRIFLOW
static Mesh *remesh_quadriflow(const Mesh *input_mesh,
                               int target_faces,
                               int seed,
                               bool preserve_sharp,
                               bool preserve_boundary,
                               bool adaptive_scale,
                               void (*update_cb)(void *, float progress, int *cancel),
                               void *update_cb_data)
{
  using namespace blender;
  using namespace blender::bke;
  const Span<float3> input_positions = input_mesh->vert_positions();
  const Span<int> input_corner_verts = input_mesh->corner_verts();
  const Span<int3> corner_tris = input_mesh->corner_tris();

  /* Gather the required data for export to the internal quadriflow mesh format. */
  Array<int3> vert_tris(corner_tris.size());
  mesh::vert_tris_from_corner_tris(input_corner_verts, corner_tris, vert_tris);

  /* Fill out the required input data */
  QuadriflowRemeshData qrd;

  qrd.totfaces = corner_tris.size();
  qrd.totverts = input_positions.size();
  qrd.verts = input_positions.cast<float>().data();
  qrd.faces = vert_tris.as_span().cast<int>().data();
  qrd.target_faces = target_faces;

  qrd.preserve_sharp = preserve_sharp;
  qrd.preserve_boundary = preserve_boundary;
  qrd.adaptive_scale = adaptive_scale;
  qrd.minimum_cost_flow = false;
  qrd.aggresive_sat = false;
  qrd.rng_seed = seed;

  qrd.out_faces = nullptr;

  /* Run the remesher */
  QFLOW_quadriflow_remesh(&qrd, update_cb, update_cb_data);

  if (qrd.out_faces == nullptr) {
    /* The remeshing was canceled */
    return nullptr;
  }

  if (qrd.out_totfaces == 0) {
    /* Meshing failed */
    MEM_freeN(qrd.out_faces);
    MEM_freeN(qrd.out_verts);
    return nullptr;
  }

  /* Construct the new output mesh */
  Mesh *mesh = BKE_mesh_new_nomain(qrd.out_totverts, 0, qrd.out_totfaces, qrd.out_totfaces * 4);
  BKE_mesh_copy_parameters(mesh, input_mesh);
  MutableSpan<int> face_offsets = mesh->face_offsets_for_write();
  MutableSpan<int> corner_verts = mesh->corner_verts_for_write();

  blender::offset_indices::fill_constant_group_size(4, 0, face_offsets);

  mesh->vert_positions_for_write().copy_from(
      Span(reinterpret_cast<float3 *>(qrd.out_verts), qrd.out_totverts));

  for (const int i : IndexRange(qrd.out_totfaces)) {
    const int loopstart = i * 4;
    corner_verts[loopstart] = qrd.out_faces[loopstart];
    corner_verts[loopstart + 1] = qrd.out_faces[loopstart + 1];
    corner_verts[loopstart + 2] = qrd.out_faces[loopstart + 2];
    corner_verts[loopstart + 3] = qrd.out_faces[loopstart + 3];
  }

  mesh_calc_edges(*mesh, false, false);

  MEM_freeN(qrd.out_faces);
  MEM_freeN(qrd.out_verts);

  return mesh;
}
#endif

Mesh *BKE_mesh_remesh_quadriflow(const Mesh *mesh,
                                 int target_faces,
                                 int seed,
                                 bool preserve_sharp,
                                 bool preserve_boundary,
                                 bool adaptive_scale,
                                 void (*update_cb)(void *, float progress, int *cancel),
                                 void *update_cb_data)
{
#ifdef WITH_QUADRIFLOW
  if (target_faces <= 0) {
    target_faces = -1;
  }
  return remesh_quadriflow(mesh,
                           target_faces,
                           seed,
                           preserve_sharp,
                           preserve_boundary,
                           adaptive_scale,
                           update_cb,
                           update_cb_data);
#else
  UNUSED_VARS(mesh,
              target_faces,
              seed,
              preserve_sharp,
              preserve_boundary,
              adaptive_scale,
              update_cb,
              update_cb_data);
  return nullptr;
#endif
}

#ifdef WITH_OPENVDB
static openvdb::FloatGrid::Ptr remesh_voxel_level_set_create(
    const Mesh *mesh, openvdb::math::Transform::Ptr transform)
{
  const Span<float3> positions = mesh->vert_positions();
  const Span<int> corner_verts = mesh->corner_verts();
  const Span<int3> corner_tris = mesh->corner_tris();

  std::vector<openvdb::Vec3s> points(mesh->verts_num);
  std::vector<openvdb::Vec3I> triangles(corner_tris.size());

  for (const int i : IndexRange(mesh->verts_num)) {
    const float3 &co = positions[i];
    points[i] = openvdb::Vec3s(co.x, co.y, co.z);
  }

  for (const int i : IndexRange(corner_tris.size())) {
    const int3 &tri = corner_tris[i];
    triangles[i] = openvdb::Vec3I(
        corner_verts[tri[0]], corner_verts[tri[1]], corner_verts[tri[2]]);
  }

  openvdb::FloatGrid::Ptr grid = openvdb::tools::meshToLevelSet<openvdb::FloatGrid>(
      *transform, points, triangles, 1.0f);

  return grid;
}

static Mesh *remesh_voxel_volume_to_mesh(const openvdb::FloatGrid::Ptr level_set_grid,
                                         const float isovalue,
                                         const float adaptivity,
                                         const bool relax_disoriented_triangles)
{
  using namespace blender;
  using namespace blender::bke;
  std::vector<openvdb::Vec3s> vertices;
  std::vector<openvdb::Vec4I> quads;
  std::vector<openvdb::Vec3I> tris;
  openvdb::tools::volumeToMesh<openvdb::FloatGrid>(
      *level_set_grid, vertices, tris, quads, isovalue, adaptivity, relax_disoriented_triangles);

  Mesh *mesh = BKE_mesh_new_nomain(
      vertices.size(), 0, quads.size() + tris.size(), quads.size() * 4 + tris.size() * 3);
  MutableSpan<float3> vert_positions = mesh->vert_positions_for_write();
  MutableSpan<int> face_offsets = mesh->face_offsets_for_write();
  MutableSpan<int> mesh_corner_verts = mesh->corner_verts_for_write();

  const int triangle_loop_start = quads.size() * 4;
  if (!face_offsets.is_empty()) {
    blender::offset_indices::fill_constant_group_size(4, 0, face_offsets.take_front(quads.size()));
    blender::offset_indices::fill_constant_group_size(
        3, triangle_loop_start, face_offsets.drop_front(quads.size()));
  }

  for (const int i : vert_positions.index_range()) {
    vert_positions[i] = float3(vertices[i].x(), vertices[i].y(), vertices[i].z());
  }

  for (const int i : IndexRange(quads.size())) {
    const int loopstart = i * 4;
    mesh_corner_verts[loopstart] = quads[i][0];
    mesh_corner_verts[loopstart + 1] = quads[i][3];
    mesh_corner_verts[loopstart + 2] = quads[i][2];
    mesh_corner_verts[loopstart + 3] = quads[i][1];
  }

  for (const int i : IndexRange(tris.size())) {
    const int loopstart = triangle_loop_start + i * 3;
    mesh_corner_verts[loopstart] = tris[i][2];
    mesh_corner_verts[loopstart + 1] = tris[i][1];
    mesh_corner_verts[loopstart + 2] = tris[i][0];
  }

  mesh_calc_edges(*mesh, false, false);

  return mesh;
}
#endif

Mesh *BKE_mesh_remesh_voxel(const Mesh *mesh,
                            const float voxel_size,
                            const float adaptivity,
                            const float isovalue,
                            const Object *object,
                            ModifierData *modifier_data)
{
#ifdef WITH_OPENVDB
  openvdb::math::Transform::Ptr transform;
  try {
    transform = openvdb::math::Transform::createLinearTransform(voxel_size);
  }
  catch (const openvdb::ArithmeticError & /*e*/) {
    /* OpenVDB internally has a limit of 3e-15 for the matrix's determinant and throws
     * ArithmeticError if the provided value is too low.
     * See #136637 for more details. */
    BKE_modifier_set_error(
        object, modifier_data, "Voxel size of %f too small to be solved", voxel_size);
    return nullptr;
  }
  openvdb::FloatGrid::Ptr level_set = remesh_voxel_level_set_create(mesh, transform);
  Mesh *result = remesh_voxel_volume_to_mesh(level_set, isovalue, adaptivity, false);
  BKE_mesh_copy_parameters(result, mesh);
  return result;
#else
  UNUSED_VARS(mesh, voxel_size, adaptivity, isovalue, object, modifier_data);
  return nullptr;
#endif
}

Mesh *BKE_mesh_remesh_voxel(const Mesh *mesh,
                            const float voxel_size,
                            const float adaptivity,
                            const float isovalue,
                            ReportList *reports)
{
#ifdef WITH_OPENVDB
  openvdb::math::Transform::Ptr transform;
  try {
    transform = openvdb::math::Transform::createLinearTransform(voxel_size);
  }
  catch (const openvdb::ArithmeticError & /*e*/) {
    /* OpenVDB internally has a limit of 3e-15 for the matrix's determinant and throws
     * ArithmeticError if the provided value is too low.
     * See #136637 for more details. */
    BKE_reportf(reports, RPT_ERROR, "Voxel size of %f too small to be solved", voxel_size);
    return nullptr;
  }
  openvdb::FloatGrid::Ptr level_set = remesh_voxel_level_set_create(mesh, transform);
  Mesh *result = remesh_voxel_volume_to_mesh(level_set, isovalue, adaptivity, false);
  BKE_mesh_copy_parameters(result, mesh);
  return result;
#else
  UNUSED_VARS(mesh, voxel_size, adaptivity, isovalue, reports);
  return nullptr;
#endif
}

namespace blender::bke {

static void calc_edge_centers(const Span<float3> positions,
                              const Span<int2> edges,
                              MutableSpan<float3> edge_centers)
{
  for (const int i : edges.index_range()) {
    edge_centers[i] = math::midpoint(positions[edges[i][0]], positions[edges[i][1]]);
  }
}

static void calc_face_centers(const Span<float3> positions,
                              const OffsetIndices<int> faces,
                              const Span<int> corner_verts,
                              MutableSpan<float3> face_centers)
{
  for (const int i : faces.index_range()) {
    face_centers[i] = mesh::face_center_calc(positions, corner_verts.slice(faces[i]));
  }
}

static void find_nearest_tris(const Span<float3> positions,
                              BVHTreeFromMesh &bvhtree,
                              MutableSpan<int> tris)
{
  for (const int i : positions.index_range()) {
    BVHTreeNearest nearest;
    nearest.index = -1;
    nearest.dist_sq = FLT_MAX;
    BLI_bvhtree_find_nearest(
        bvhtree.tree, positions[i], &nearest, bvhtree.nearest_callback, &bvhtree);
    tris[i] = nearest.index;
  }
}

static void find_nearest_tris_parallel(const Span<float3> positions,
                                       BVHTreeFromMesh &bvhtree,
                                       MutableSpan<int> tris)
{
  threading::parallel_for(tris.index_range(), 512, [&](const IndexRange range) {
    find_nearest_tris(positions.slice(range), bvhtree, tris.slice(range));
  });
}

static void find_nearest_verts(const Span<float3> positions,
                               const Span<int> corner_verts,
                               const Span<int3> src_corner_tris,
                               const Span<float3> dst_positions,
                               const Span<int> nearest_vert_tris,
                               MutableSpan<int> nearest_verts)
{
  threading::parallel_for(dst_positions.index_range(), 512, [&](const IndexRange range) {
    for (const int dst_vert : range) {
      const float3 &dst_position = dst_positions[dst_vert];
      const int3 &src_tri = src_corner_tris[nearest_vert_tris[dst_vert]];

      std::array<float, 3> distances;
      for (const int i : IndexRange(3)) {
        const int src_vert = corner_verts[src_tri[i]];
        distances[i] = math::distance_squared(positions[src_vert], dst_position);
      }

      const int min = std::min_element(distances.begin(), distances.end()) - distances.begin();
      nearest_verts[dst_vert] = corner_verts[src_tri[min]];
    }
  });
}

static void find_nearest_faces(const Span<int> src_tri_faces,
                               const Span<float3> dst_positions,
                               const OffsetIndices<int> dst_faces,
                               const Span<int> dst_corner_verts,
                               BVHTreeFromMesh &bvhtree,
                               MutableSpan<int> nearest_faces)
{
  struct TLS {
    Vector<float3> face_centers;
    Vector<int> tri_indices;
  };
  threading::EnumerableThreadSpecific<TLS> all_tls;
  threading::parallel_for(dst_faces.index_range(), 512, [&](const IndexRange range) {
    threading::isolate_task([&] {
      TLS &tls = all_tls.local();
      Vector<float3> &face_centers = tls.face_centers;
      face_centers.reinitialize(range.size());
      calc_face_centers(dst_positions, dst_faces.slice(range), dst_corner_verts, face_centers);

      Vector<int> &tri_indices = tls.tri_indices;
      tri_indices.reinitialize(range.size());
      find_nearest_tris(face_centers, bvhtree, tri_indices);

      array_utils::gather(src_tri_faces, tri_indices.as_span(), nearest_faces.slice(range));
    });
  });
}

static void find_nearest_corners(const Span<float3> src_positions,
                                 const OffsetIndices<int> src_faces,
                                 const Span<int> src_corner_verts,
                                 const Span<int> src_tri_faces,
                                 const Span<float3> dst_positions,
                                 const Span<int> dst_corner_verts,
                                 const Span<int> nearest_vert_tris,
                                 MutableSpan<int> nearest_corners)
{
  threading::parallel_for(nearest_corners.index_range(), 512, [&](const IndexRange range) {
    Vector<float, 64> distances;
    for (const int dst_corner : range) {
      const int dst_vert = dst_corner_verts[dst_corner];
      const float3 &dst_position = dst_positions[dst_vert];

      const int src_tri = nearest_vert_tris[dst_vert];
      const IndexRange src_face = src_faces[src_tri_faces[src_tri]];
      const Span<int> src_face_verts = src_corner_verts.slice(src_face);

      /* Find the corner in the face that's closest in the closest face. */
      distances.reinitialize(src_face_verts.size());
      for (const int i : src_face_verts.index_range()) {
        const int src_vert = src_face_verts[i];
        distances[i] = math::distance_squared(src_positions[src_vert], dst_position);
      }

      const int min = std::min_element(distances.begin(), distances.end()) - distances.begin();
      nearest_corners[dst_corner] = src_face[min];
    }
  });
}

static void find_nearest_edges(const Span<float3> src_positions,
                               const Span<int2> src_edges,
                               const OffsetIndices<int> src_faces,
                               const Span<int> src_corner_edges,
                               const Span<int> src_tri_faces,
                               const Span<float3> dst_positions,
                               const Span<int2> dst_edges,
                               BVHTreeFromMesh &bvhtree,
                               MutableSpan<int> nearest_edges)
{
  struct TLS {
    Vector<float3> edge_centers;
    Vector<int> tri_indices;
    Vector<int> face_indices;
    Vector<float> distances;
  };
  threading::EnumerableThreadSpecific<TLS> all_tls;
  threading::parallel_for(nearest_edges.index_range(), 512, [&](const IndexRange range) {
    threading::isolate_task([&] {
      TLS &tls = all_tls.local();
      Vector<float3> &edge_centers = tls.edge_centers;
      edge_centers.reinitialize(range.size());
      calc_edge_centers(dst_positions, dst_edges.slice(range), edge_centers);

      Vector<int> &tri_indices = tls.tri_indices;
      tri_indices.reinitialize(range.size());
      find_nearest_tris_parallel(edge_centers, bvhtree, tri_indices);

      Vector<int> &face_indices = tls.face_indices;
      face_indices.reinitialize(range.size());
      array_utils::gather(src_tri_faces, tri_indices.as_span(), face_indices.as_mutable_span());

      /* Find the source edge that's closest to the destination edge in the nearest face. Search
       * through the whole face instead of just the triangle because the triangle has edges that
       * might not be actual mesh edges. */
      Vector<float, 64> distances;
      for (const int i : range.index_range()) {
        const int dst_edge = range[i];
        const float3 &dst_position = edge_centers[i];

        const int src_face = face_indices[i];
        const Span<int> src_face_edges = src_corner_edges.slice(src_faces[src_face]);

        distances.reinitialize(src_face_edges.size());
        for (const int i : src_face_edges.index_range()) {
          const int2 src_edge = src_edges[src_face_edges[i]];
          const float3 src_center = math::midpoint(src_positions[src_edge[0]],
                                                   src_positions[src_edge[1]]);
          distances[i] = math::distance_squared(src_center, dst_position);
        }

        const int min = std::min_element(distances.begin(), distances.end()) - distances.begin();
        nearest_edges[dst_edge] = src_face_edges[min];
      }
    });
  });
}

static void gather_attributes(const Span<StringRef> ids,
                              const AttributeAccessor src_attributes,
                              const AttrDomain domain,
                              const Span<int> index_map,
                              MutableAttributeAccessor dst_attributes)
{
  for (const StringRef id : ids) {
    const GVArraySpan src = *src_attributes.lookup(id, domain);
    const AttrType type = cpp_type_to_attribute_type(src.type());
    GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(id, domain, type);
    attribute_math::gather(src, index_map, dst.span);
    dst.finish();
  }
}

void mesh_remesh_reproject_attributes(const Mesh &src, Mesh &dst)
{
  /* Gather attributes to transfer for each domain. This makes it possible to skip
   * building index maps and even the main BVH tree if there are no attributes. */
  const AttributeAccessor src_attributes = src.attributes();
  Vector<StringRef> point_ids;
  Vector<StringRef> edge_ids;
  Vector<StringRef> face_ids;
  Vector<StringRef> corner_ids;
  src_attributes.foreach_attribute([&](const AttributeIter &iter) {
    if (ELEM(iter.name, "position", ".edge_verts", ".corner_vert", ".corner_edge")) {
      return;
    }
    switch (iter.domain) {
      case AttrDomain::Point:
        point_ids.append(iter.name);
        break;
      case AttrDomain::Edge:
        edge_ids.append(iter.name);
        break;
      case AttrDomain::Face:
        face_ids.append(iter.name);
        break;
      case AttrDomain::Corner:
        corner_ids.append(iter.name);
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
  });

  if (point_ids.is_empty() && edge_ids.is_empty() && face_ids.is_empty() && corner_ids.is_empty())
  {
    return;
  }

  const Span<float3> src_positions = src.vert_positions();
  const OffsetIndices src_faces = src.faces();
  const Span<int> src_corner_verts = src.corner_verts();
  const Span<int3> src_corner_tris = src.corner_tris();

  /* The main idea in the following code is to trade some complexity in sampling for the benefit of
   * only using and building a single BVH tree. Since sculpt mode doesn't generally deal with loose
   * vertices and edges, we use the standard "triangles" BVH which won't contain them. Also, only
   * relying on a single BVH should reduce memory usage, and work better if the BVH and #pbvh::Tree
   * are ever merged.
   *
   * One key decision is separating building transfer index maps from actually transferring any
   * attribute data. This is important to keep attribute storage independent from the specifics of
   * the decisions made here, which mainly results in easier refactoring, more generic code, and
   * possibly improved performance from lower cache usage in the "complex" sampling part of the
   * algorithm and the copying itself. */
  BVHTreeFromMesh bvhtree = src.bvh_corner_tris();

  const Span<float3> dst_positions = dst.vert_positions();
  const OffsetIndices dst_faces = dst.faces();
  const Span<int> dst_corner_verts = dst.corner_verts();

  MutableAttributeAccessor dst_attributes = dst.attributes_for_write();

  if (!point_ids.is_empty() || !corner_ids.is_empty()) {
    Array<int> vert_nearest_tris(dst_positions.size());
    find_nearest_tris_parallel(dst_positions, bvhtree, vert_nearest_tris);

    if (!point_ids.is_empty()) {
      Array<int> map(dst.verts_num);
      find_nearest_verts(
          src_positions, src_corner_verts, src_corner_tris, dst_positions, vert_nearest_tris, map);
      gather_attributes(point_ids, src_attributes, AttrDomain::Point, map, dst_attributes);
    }

    if (!corner_ids.is_empty()) {
      const Span<int> src_tri_faces = src.corner_tri_faces();
      Array<int> map(dst.corners_num);
      find_nearest_corners(src_positions,
                           src_faces,
                           src_corner_verts,
                           src_tri_faces,
                           dst_positions,
                           dst_corner_verts,
                           vert_nearest_tris,
                           map);
      gather_attributes(corner_ids, src_attributes, AttrDomain::Corner, map, dst_attributes);
    }
  }

  if (!edge_ids.is_empty()) {
    const Span<int2> src_edges = src.edges();
    const Span<int> src_corner_edges = src.corner_edges();
    const Span<int> src_tri_faces = src.corner_tri_faces();
    const Span<int2> dst_edges = dst.edges();
    Array<int> map(dst.edges_num);
    find_nearest_edges(src_positions,
                       src_edges,
                       src_faces,
                       src_corner_edges,
                       src_tri_faces,
                       dst_positions,
                       dst_edges,
                       bvhtree,
                       map);
    gather_attributes(edge_ids, src_attributes, AttrDomain::Edge, map, dst_attributes);
  }

  if (!face_ids.is_empty()) {
    const Span<int> src_tri_faces = src.corner_tri_faces();
    Array<int> map(dst.faces_num);
    find_nearest_faces(src_tri_faces, dst_positions, dst_faces, dst_corner_verts, bvhtree, map);
    gather_attributes(face_ids, src_attributes, AttrDomain::Face, map, dst_attributes);
  }

  if (src.active_color_attribute) {
    BKE_id_attributes_active_color_set(&dst.id, src.active_color_attribute);
  }
  if (src.default_color_attribute) {
    BKE_id_attributes_default_color_set(&dst.id, src.default_color_attribute);
  }
}

}  // namespace blender::bke

Mesh *BKE_mesh_remesh_voxel_fix_poles(const Mesh *mesh)
{
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh);

  BMeshCreateParams bmesh_create_params{};
  bmesh_create_params.use_toolflags = true;
  BMesh *bm = BM_mesh_create(&allocsize, &bmesh_create_params);

  BMeshFromMeshParams bmesh_from_mesh_params{};
  bmesh_from_mesh_params.calc_face_normal = true;
  bmesh_from_mesh_params.calc_vert_normal = true;
  BM_mesh_bm_from_me(bm, mesh, &bmesh_from_mesh_params);

  BMVert *v;
  BMEdge *ed, *ed_next;
  BMFace *f, *f_next;
  BMIter iter_a, iter_b;

  /* Merge 3 edge poles vertices that exist in the same face */
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  BM_ITER_MESH_MUTABLE (f, f_next, &iter_a, bm, BM_FACES_OF_MESH) {
    BMVert *v1, *v2;
    v1 = nullptr;
    v2 = nullptr;
    BM_ITER_ELEM (v, &iter_b, f, BM_VERTS_OF_FACE) {
      if (BM_vert_edge_count(v) == 3) {
        if (v1) {
          v2 = v;
        }
        else {
          v1 = v;
        }
      }
    }
    if (v1 && v2 && (v1 != v2) && !BM_edge_exists(v1, v2)) {
      BM_face_kill(bm, f);
      BMEdge *e = BM_edge_create(bm, v1, v2, nullptr, BM_CREATE_NOP);
      BM_elem_flag_set(e, BM_ELEM_TAG, true);
    }
  }

  BM_ITER_MESH_MUTABLE (ed, ed_next, &iter_a, bm, BM_EDGES_OF_MESH) {
    if (BM_elem_flag_test(ed, BM_ELEM_TAG)) {
      float co[3];
      mid_v3_v3v3(co, ed->v1->co, ed->v2->co);
      BMVert *vc = BM_edge_collapse(bm, ed, ed->v1, true, true);
      copy_v3_v3(vc->co, co);
    }
  }

  /* Delete faces with a 3 edge pole in all their vertices */
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  BM_ITER_MESH (f, &iter_a, bm, BM_FACES_OF_MESH) {
    bool dissolve = true;
    BM_ITER_ELEM (v, &iter_b, f, BM_VERTS_OF_FACE) {
      if (BM_vert_edge_count(v) != 3) {
        dissolve = false;
      }
    }
    if (dissolve) {
      BM_ITER_ELEM (v, &iter_b, f, BM_VERTS_OF_FACE) {
        BM_elem_flag_set(v, BM_ELEM_TAG, true);
      }
    }
  }
  BM_mesh_delete_hflag_context(bm, BM_ELEM_TAG, DEL_VERTS);

  BM_ITER_MESH (ed, &iter_a, bm, BM_EDGES_OF_MESH) {
    if (BM_edge_face_count(ed) != 2) {
      BM_elem_flag_set(ed, BM_ELEM_TAG, true);
    }
  }
  BM_mesh_edgenet(bm, false, true);

  /* Smooth the result */
  for (int i = 0; i < 4; i++) {
    BM_ITER_MESH (v, &iter_a, bm, BM_VERTS_OF_MESH) {
      float co[3];
      zero_v3(co);
      BM_ITER_ELEM (ed, &iter_b, v, BM_EDGES_OF_VERT) {
        BMVert *vert = BM_edge_other_vert(ed, v);
        add_v3_v3(co, vert->co);
      }
      mul_v3_fl(co, 1.0f / float(BM_vert_edge_count(v)));
      mid_v3_v3v3(v->co, v->co, co);
    }
  }

  BM_mesh_normals_update(bm);

  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT, false);
  BM_mesh_elem_hflag_enable_all(bm, BM_FACE, BM_ELEM_TAG, false);
  BMO_op_callf(bm,
               (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
               "recalc_face_normals faces=%hf",
               BM_ELEM_TAG);
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

  BMeshToMeshParams bmesh_to_mesh_params{};
  bmesh_to_mesh_params.calc_object_remap = false;
  Mesh *result = BKE_mesh_from_bmesh_nomain(bm, &bmesh_to_mesh_params, mesh);

  BM_mesh_free(bm);
  return result;
}
