/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_MANIFOLD
#  include <algorithm>
#  include <iostream>

#  include "BLI_array.hh"
#  include "BLI_array_utils.hh"
#  include "BLI_map.hh"
#  include "BLI_math_geom.h"
#  include "BLI_math_matrix.h"
#  include "BLI_math_matrix.hh"
#  include "BLI_math_matrix_types.hh"
#  include "BLI_math_vector.hh"
#  include "BLI_math_vector_types.hh"
#  include "BLI_offset_indices.hh"
#  include "BLI_span.hh"
#  include "BLI_task.hh"
#  include "BLI_vector.hh"

// #define DEBUG_TIME
#  ifdef DEBUG_TIME
#    include "BLI_timeit.hh"
#  endif

#  include "BKE_attribute.hh"
#  include "BKE_attribute_math.hh"
#  include "BKE_deform.hh"
#  include "BKE_geometry_set.hh"
#  include "BKE_instances.hh"
#  include "BKE_lib_id.hh"
#  include "BKE_mesh.hh"

#  include "GEO_realize_instances.hh"

#  include "mesh_boolean_intern.hh"
#  include "mesh_boolean_manifold.hh"

#  include "manifold/manifold.h"

using manifold::Manifold;
using manifold::MeshGL;

/* Using this for now for debug printing of materials. Can remove later. */
#  include "DNA_material_types.h"

namespace blender::geometry::boolean {

/* Some debug output functions. */

template<typename T> static void dump_span(Span<T> span, const std::string &name)
{
  std::cout << name << ":";
  for (const int i : span.index_range()) {
    if (i % 10 == 0) {
      std::cout << "\n[" << i << "] ";
    }
    std::cout << span[i] << " ";
  }
  std::cout << "\n";
}

template<typename T>
static void dump_span_with_stride(Span<T> span, int stride, const std::string &name)
{
  std::cout << name << ":";
  for (const int i : span.index_range()) {
    if (i % 10 == 0) {
      std::cout << "\n[" << i << "] ";
    }
    std::cout << span[i] << " ";
    if (stride > 1 && (i % stride) == stride - 1) {
      std::cout << "/ ";
    }
  }
  std::cout << "\n";
}

template<typename T>
static void dump_vector(const std::vector<T> &vec, int stride, const std::string &name)
{
  std::cout << name << ":";
  for (int i = 0; i < vec.size(); i++) {
    if (i % 10 == 0) {
      std::cout << "\n[" << i << "] ";
    }
    std::cout << vec[i] << " ";
    if (stride > 1 && (i % stride) == stride - 1) {
      std::cout << "/ ";
    }
  }
  std::cout << "\n";
}

template<typename T>
static void dump_vector_values(const std::string indent,
                               const std::string &assign_to,
                               const std::vector<T> &vec)
{
  std::cout << indent << assign_to << " = { ";
  for (int i = 0; i < vec.size(); i++) {
    if (i > 0 && (i % 10) == 0) {
      std::cout << "\n" << indent << indent;
    }
    std::cout << vec[i];
    if (i == vec.size() - 1) {
      std::cout << " };\n";
    }
    else {
      std::cout << ", ";
    }
  }
}

static void dump_meshgl(const MeshGL &mgl, const std::string &name)
{
  std::cout << "\nMeshGL " << name << ":\n"
            << "num verts = " << mgl.NumVert() << "\nnum triangles = " << mgl.NumTri() << "\n"
            << "\n";
  dump_vector(mgl.vertProperties, mgl.numProp, "vertProperties");
  dump_vector(mgl.triVerts, 3, "triVerts");
  dump_vector(mgl.faceID, 1, "faceID");
  if (!mgl.mergeFromVert.empty()) {
    dump_vector(mgl.mergeFromVert, 1, "mergeFromVert");
    dump_vector(mgl.mergeToVert, 1, "mergeToVert");
  }
  dump_vector(mgl.runIndex, 1, "runIndex");
  dump_vector(mgl.runOriginalID, 1, "runOrigiinalID");
}

[[maybe_unused]] static void dump_meshgl_for_debug(const MeshGL &mgl)
{
  std::string indent = "    ";
  std::cout << indent << "MeshGL m;\n";
  std::cout << indent << "m.numProp = " << mgl.numProp << ";\n";
  dump_vector_values(indent, "m.vertProperties", mgl.vertProperties);
  dump_vector_values(indent, "m.triVerts", mgl.triVerts);
  if (!mgl.mergeFromVert.empty()) {
    dump_vector_values(indent, "m.mergeFromVert", mgl.mergeFromVert);
    dump_vector_values(indent, "m.mergeToVert", mgl.mergeToVert);
  }
  dump_vector_values(indent, "m.runIndex", mgl.runIndex);
  dump_vector_values(indent, "m.runOriginalID", mgl.runOriginalID);
  dump_vector_values(indent, "m.faceID", mgl.faceID);
  BLI_assert(mgl.runTransform.size() == 0);
  BLI_assert(mgl.halfedgeTangent.size() == 0);
  if (mgl.tolerance != 0) {
    std::cout << indent << "m.tolerance = " << mgl.tolerance << ";\n";
  }
}

static const char *domain_names[] = {
    "point", "edge", "face", "corner", "curve", "instance", "layer"};

static void dump_mesh(const Mesh *mesh, const std::string &name)
{
  std::cout << "\nMesh " << name << ":\n"
            << "verts_num = " << mesh->verts_num << "\nfaces_num = " << mesh->faces_num
            << "\nedges_num = " << mesh->edges_num << "\ncorners_num = " << mesh->corners_num
            << "\n";
  dump_span(mesh->vert_positions(), "verts");
  dump_span(mesh->edges(), "edges");
  dump_span(mesh->corner_verts(), "corner_verts");
  dump_span(mesh->corner_edges(), "corner_edges");
  dump_span(mesh->face_offsets(), "face_offsets");
  std::cout << "triangulation:\n";
  dump_span(mesh->corner_tris(), "corner_tris");
  dump_span(mesh->corner_tri_faces(), "corner_tri_faces");
  std::cout << "attributes:\n";
  bke::AttributeAccessor attrs = mesh->attributes();
  attrs.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (ELEM(iter.name, "position", ".edge_verts", ".corner_vert", ".corner_edge")) {
      return;
    }
    const int di = int8_t(iter.domain);
    const char *domain = (di >= 0 && di < ATTR_DOMAIN_NUM) ? domain_names[di] : "?";
    std::string label = std::string(domain) + ": " + iter.name;
    switch (iter.data_type) {
      case bke::AttrType::Float: {
        VArraySpan<float> floatspan(*attrs.lookup<float>(iter.name));
        dump_span(floatspan, label);
      } break;
      case bke::AttrType::Int32:
      case bke::AttrType::Bool: {
        const VArraySpan<int> intspan(*attrs.lookup<int>(iter.name));
        dump_span(intspan, label);
      } break;
      case bke::AttrType::Float3: {
        const VArraySpan<float3> float3span(*attrs.lookup<float3>(iter.name));
        dump_span(float3span, label);
      } break;
      case bke::AttrType::Float2: {
        const VArraySpan<float2> float2span(*attrs.lookup<float2>(iter.name));
        dump_span(float2span, label);
      } break;
      default:
        std::cout << label << " attribute not dumped\n";
        break;
    }
  });
  std::cout << "materials:\n";
  for (int i = 0; i < mesh->totcol; i++) {
    std::cout << "[" << i << "]: " << (mesh->mat[i] ? mesh->mat[i]->id.name + 2 : "none") << "\n";
  }
}

MeshOffsets::MeshOffsets(Span<const Mesh *> meshes)
{
  const int meshes_num = meshes.size();
  this->vert_start.reinitialize(meshes_num + 1);
  this->face_start.reinitialize(meshes_num + 1);
  this->edge_start.reinitialize(meshes_num + 1);
  this->corner_start.reinitialize(meshes_num + 1);
  for (const int i : meshes.index_range()) {
    this->vert_start[i] = meshes[i]->verts_num;
    this->face_start[i] = meshes[i]->faces_num;
    this->edge_start[i] = meshes[i]->edges_num;
    this->corner_start[i] = meshes[i]->corners_num;
  }
  this->vert_offsets = offset_indices::accumulate_counts_to_offsets(this->vert_start);
  this->face_offsets = offset_indices::accumulate_counts_to_offsets(this->face_start);
  this->edge_offsets = offset_indices::accumulate_counts_to_offsets(this->edge_start);
  this->corner_offsets = offset_indices::accumulate_counts_to_offsets(this->corner_start);
}

/**
 * Create and return the Manifold library's internal #Manifold class instance
 * to represent the subset \a joined_mesh which came from the input
 * mesh with index \a mesh_index.  We can tell which elements are in the
 * subset using \a mesh_offsets.
 * This is done using Manifold's #MeshGL struct, which has linearized
 * vector of x, y, z coordinates in its #vertProperties,
 * where the index divided by 3 is the input "vertex index".
 * It also has a linearized list of the triples of vertex indices that
 * give the triangulation of the mesh faces, where the index divided
 * by 3 is the "triangle index".
 * The #faceID vector is indexed by triangle index, and gives the
 * original mesh face index in the joined mesh..
 * It also sets up #runIndex and #runOriginalID so that when we
 * access OriginalId's in the output, they will be \a mesh_index.
 */
static void get_manifold(Manifold &manifold,
                         const Span<const Mesh *> meshes,
                         int mesh_index,
                         const MeshOffsets &mesh_offsets)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "get_manifold for mesh " << mesh_index << "\n";
  }
  /* Use the original mesh for simplicity for some things, and use the joined mesh for data that
   * could have been affected by a transform input. A potential optimization would be retrieving
   * the data from an original mesh instead if the corresponding transform was the identity. */
  const Mesh &mesh = *meshes[mesh_index];
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int3> corner_tris = mesh.corner_tris();

  MeshGL meshgl;

  constexpr int props_num = 3;
  meshgl.numProp = props_num;
  meshgl.vertProperties.resize(size_t(mesh.verts_num) * props_num);
  array_utils::copy(mesh.vert_positions(), MutableSpan(meshgl.vertProperties).cast<float3>());

  /* Using separate a OriginalID for each input face will prevent co-planar
   * faces from being merged.  We need this until the fix introduced in
   * Manifold at version 3.1.0. */
  constexpr bool use_runids = false;
  if (use_runids) {
    meshgl.runIndex.resize(mesh.faces_num);
    meshgl.runOriginalID.resize(mesh.faces_num);
  }

  const int face_start = mesh_offsets.face_start[mesh_index];

  meshgl.faceID.resize(corner_tris.size());
  /* Inlined copy of #corner_tris_calc_face_indices with an offset added to the face index. */
  MutableSpan face_ids = MutableSpan(meshgl.faceID);
  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const IndexRange face = faces[i];
      const int start = poly_to_tri_count(int(i), int(face.start()));
      const int num = bke::mesh::face_triangles_num(int(face.size()));
      face_ids.slice(start, num).fill(uint32_t(i + face_start));
      if (use_runids) {
        meshgl.runOriginalID[i] = face_start + i;
        meshgl.runIndex[i] = start * 3;
      }
    }
  });

  meshgl.triVerts.resize(corner_tris.size() * 3);
  MutableSpan vert_tris = MutableSpan(meshgl.triVerts).cast<int3>();
  bke::mesh::vert_tris_from_corner_tris(corner_verts, corner_tris, vert_tris);

  if (!use_runids) {
    meshgl.runIndex.resize(2);
    meshgl.runOriginalID.resize(1);
    meshgl.runIndex[0] = 0;
    meshgl.runIndex[1] = corner_tris.size() * 3;
    meshgl.runOriginalID[0] = mesh_index;
  }
  if (dbg_level > 0) {
    dump_meshgl(meshgl, "converted result for mesh " + std::to_string(mesh_index));
    if (dbg_level > 1) {
      dump_meshgl_for_debug(meshgl);
    }
  }
  {
#  ifdef DEBUG_TIME
    timeit::ScopedTimer mtimer("manifold constructor from meshgl");
#  endif
    manifold = Manifold(meshgl);
  }
}

/**
 * Get all the Manifold data structures for each Mesh subset of \a joined_mesh that is indicated
 * by a range of offsets in \a mesh_offsets. ß
 */
static void get_manifolds(MutableSpan<Manifold> manifolds,
                          const Span<const Mesh *> meshes,
                          const Span<float4x4> transforms,
                          const MeshOffsets &mesh_offsets)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "GET_MANIFOLDS\n";
    std::cout << "\nMesh Offset (starts):\n";
    dump_span(mesh_offsets.vert_start.as_span(), "vert");
    dump_span(mesh_offsets.face_start.as_span(), "face");
    dump_span(mesh_offsets.edge_start.as_span(), "edge");
    dump_span(mesh_offsets.corner_start.as_span(), "corner");
  }
  const int meshes_num = manifolds.size();

  /* Transforming the original input meshes is a simple way to reuse the #Mesh::corner_tris() cache
   * for un-transformed meshes. This should reduce memory usage and help to avoid unnecessary cache
   * re-computations. */
  Array<const Mesh *> transformed_meshes(meshes_num);
  for (const int i : meshes.index_range()) {
    if (math::is_identity(transforms[i])) {
      transformed_meshes[i] = meshes[i];
    }
    else {
      Mesh *transformed_mesh = BKE_mesh_copy_for_eval(*meshes[i]);
      bke::mesh_transform(*transformed_mesh, transforms[i], false);
      transformed_meshes[i] = transformed_mesh;
    }
  }

  if (dbg_level > 0) {
    for (const int mesh_index : IndexRange(meshes_num)) {
      get_manifold(manifolds[mesh_index], transformed_meshes, mesh_index, mesh_offsets);
    }
  }
  else {
    threading::parallel_for_each(IndexRange(meshes_num), [&](int mesh_index) {
      get_manifold(manifolds[mesh_index], transformed_meshes, mesh_index, mesh_offsets);
    });
  }

  for (const int i : transformed_meshes.index_range()) {
    if (transformed_meshes[i] != meshes[i]) {
      BKE_id_free(nullptr, const_cast<Mesh *>(transformed_meshes[i]));
    }
  }
}

constexpr int inline_outface_size = 8;

struct OutFace {
  /* Vertex ids in meshgl indexing space. */
  Vector<int, inline_outface_size> verts;
  /* The faceID input to manifold, i.e. original face id in combined input mesh indexing space.
   */
  int face_id;

  /* Find the first index (should be only one) of verts that contains v, else -1. */
  int find_vert_index(int v) const
  {
    return verts.first_index_of_try(v);
  }
};

/** Data needed to build the final output Mesh. */
struct MeshAssembly {
  /* Vertex positions, linearized (use vertpos_stride to multiply index). */
  MutableSpan<float> vertpos;
  int vertpos_stride = 3;
  /* How many vertices were in the combined input meshes. */
  int input_verts_num;
  /* How many vertices are in the output (i.e., in vertpos). */
  int output_verts_num;
  /* The new output faces. */
  Vector<OutFace> new_faces;
  /* If we have to delete vertices, this map will have non-zero size and
   * will map the MeshGL vertex index to final vertex index.
   * Also, if this mapping happens, the vertpos array will be modified
   * accordingly.
   */
  Vector<int> old_to_new_vert_map;

  float3 vert_position(const int v) const
  {
    const int start = vertpos_stride * v;
    return float3(vertpos[start], vertpos[start + 1], vertpos[start + 2]);
  }

  int mapped_vert(const int v) const
  {
    if (!new_faces.is_empty()) {
      return old_to_new_vert_map[v];
    }
    return v;
  }
};

/**
 * Arrays to find, for each index of a given type in the output mesh,
 * what is the corresponding index of a representative element in the joined mesh.
 * if there is no representative, a -1 is used.
 * These are created lazily - if their current length is zero, then need to be created.
 */
class OutToInMaps {
  Array<int> vertex_map_;
  Array<int> face_map_;
  Array<int> edge_map_;
  Array<int> corner_map_;

  const MeshAssembly *mesh_assembly_;
  const Mesh *joined_mesh_;
  const Mesh *output_mesh_;
  const MeshOffsets *mesh_offsets_;

 public:
  OutToInMaps(const MeshAssembly *mesh_assembly,
              const Mesh *joined_mesh,
              const Mesh *output_mesh,
              const MeshOffsets *mesh_offsets)
      : mesh_assembly_(mesh_assembly),
        joined_mesh_(joined_mesh),
        output_mesh_(output_mesh),
        mesh_offsets_(mesh_offsets)
  {
  }

  Span<int> ensure_vertex_map();
  Span<int> ensure_face_map();
  Span<int> ensure_edge_map();
  Span<int> ensure_corner_map();
};

Span<int> OutToInMaps::ensure_face_map()
{
  if (!face_map_.is_empty()) {
    return face_map_;
  }
  /* The MeshAssembly's new_faces should map one to one with output faces. */
#  ifdef DEBUG_TIME
  timeit::ScopedTimer timer("filling face map");
#  endif
  face_map_.reinitialize(output_mesh_->faces_num);
  BLI_assert(mesh_assembly_->new_faces.size() == face_map_.size());
  constexpr int grain_size = 50000;
  threading::parallel_for(
      mesh_assembly_->new_faces.index_range(), grain_size, [&](const IndexRange range) {
        for (const int i : range) {
          face_map_[i] = mesh_assembly_->new_faces[i].face_id;
        }
      });
  return face_map_;
}

Span<int> OutToInMaps::ensure_vertex_map()
{
  if (!vertex_map_.is_empty()) {
    return vertex_map_;
  }
  /* There may be better ways, but for now we discover the output to input
   * vertex mapping by going through the output faces, and for each, looking
   * through the vertices of the corresponding input face for matches.
   */
  const Span<int> face_map = this->ensure_face_map();
#  ifdef DEBUG_TIME
  timeit::ScopedTimer timer("filling vertex map");
#  endif
  vertex_map_ = Array<int>(output_mesh_->verts_num, -1);
  /* To parallelize this, need to deal with the fact that this will
   * have different threads wanting to write vertex_map, and also want
   * determinism of which one wins if there is more than one possibility.
   */
  const OffsetIndices<int> in_faces = joined_mesh_->faces();
  const OffsetIndices<int> out_faces = output_mesh_->faces();
  const Span<int> in_corner_verts = joined_mesh_->corner_verts();
  const Span<int> out_corner_verts = output_mesh_->corner_verts();
  const Span<float3> out_vert_positions = output_mesh_->vert_positions();
  const Span<float3> in_vert_positions = joined_mesh_->vert_positions();
  for (const int out_face_index : IndexRange(output_mesh_->faces_num)) {
    const int in_face_index = face_map[out_face_index];
    const IndexRange in_face = in_faces[in_face_index];
    const IndexRange out_face = out_faces[out_face_index];
    const Span<int> in_face_verts = in_corner_verts.slice(in_face);
    for (const int out_v : out_corner_verts.slice(out_face)) {
      if (vertex_map_[out_v] != -1) {
        continue;
      }
      float3 out_pos = out_vert_positions[out_v];
      const auto *it = std::find_if(in_face_verts.begin(), in_face_verts.end(), [&](int in_v) {
        return out_pos == in_vert_positions[in_v];
      });
      if (it != in_face_verts.end()) {
        int in_v = in_face_verts[std::distance(in_face_verts.begin(), it)];
        vertex_map_[out_v] = in_v;
      }
    }
  }
  return vertex_map_;
}

Span<int> OutToInMaps::ensure_corner_map()
{
  if (!corner_map_.is_empty()) {
    return corner_map_;
  }
  /* There may be better ways, but for now we discover the output to input
   * corner mapping by going through the output faces, and for each, looking
   * through the corners of the corresponding input face for matches of the
   * vertex involved.
   */
  const Span<int> face_map = this->ensure_face_map();
  const Span<int> vert_map = this->ensure_vertex_map();
#  ifdef DEBUG_TIME
  timeit::ScopedTimer timer("filling corner map");
#  endif
  corner_map_ = Array<int>(output_mesh_->corners_num, -1);
  const OffsetIndices<int> in_faces = joined_mesh_->faces();
  const OffsetIndices<int> out_faces = output_mesh_->faces();
  const Span<int> in_corner_verts = joined_mesh_->corner_verts();
  const Span<int> out_corner_verts = output_mesh_->corner_verts();
  constexpr int grain_size = 10000;
  threading::parallel_for(
      IndexRange(output_mesh_->faces_num), grain_size, [&](const IndexRange range) {
        for (const int out_face_index : range) {
          const int in_face_index = face_map[out_face_index];
          const IndexRange in_face = in_faces[in_face_index];
          for (const int out_c : out_faces[out_face_index]) {
            BLI_assert(corner_map_[out_c] == -1);
            const int out_v = out_corner_verts[out_c];
            const int in_v = vert_map[out_v];
            if (in_v == -1) {
              continue;
            }
            const int in_face_i = in_corner_verts.slice(in_face).first_index_try(in_v);
            if (in_face_i != -1) {
              const int in_c = in_face[in_face_i];
              corner_map_[out_c] = in_c;
            }
          }
        }
      });
  return corner_map_;
}

static bool same_dir(const float3 &p1, const float3 &p2, const float3 &q1, const float3 &q2)
{
  float3 p = p1 - p2;
  float3 q = q1 - q2;
  float pq = math::length(p) * math::length(q);
  if (pq == 0.0f) {
    return true;
  }
  float abs_cos_pq = math::abs(math::dot(p, q) / pq);
  return (math::abs(abs_cos_pq - 1.0f) <= 1e-5f);
}

/**
 * What mesh_id corresponds to a given face_id, assuming that the face_id
 * is in one of the ranges of mesh_offsets.face_offsets.
 */
static inline int mesh_id_for_face(const int face_id, const MeshOffsets &mesh_offsets)
{
  for (const int mesh_id : mesh_offsets.face_offsets.index_range()) {
    if (mesh_offsets.face_offsets[mesh_id].contains(face_id)) {
      return mesh_id;
    }
  }
  return -1;
}

/**
 * What is the vertex index range for the face \a face_id, assuming that face_id is one of the
 * ranges of mesh_offsets.face_offsets.
 */
static IndexRange vertex_range_for_face(const int face_id, const MeshOffsets &mesh_offsets)
{
  const int mesh_id = mesh_id_for_face(face_id, mesh_offsets);
  if (mesh_id == -1) {
    return IndexRange();
  }
  return IndexRange::from_begin_end(mesh_offsets.vert_start[mesh_id],
                                    mesh_offsets.vert_start[mesh_id + 1]);
}

Span<int> OutToInMaps::ensure_edge_map()
{
  constexpr int dbg_level = 0;
  if (!edge_map_.is_empty()) {
    return edge_map_;
  }
  if (dbg_level > 0) {
    std::cout << "\nensure_edge_map\n";
    if (dbg_level > 1) {
      dump_mesh(joined_mesh_, "joined_mesh");
      dump_mesh(output_mesh_, "output_mesh");
    }
  }
  /* There may be better ways to get the edge map, but for now
   * we go through the output faces, and for each edge, see if
   * there is an input edge in the corresponding input face that
   * has one or the other end in common, and if only one end is
   * in common, is in approximately the same direction.
   * We can assume that the output and input are manifold.
   * So if there is an edge that starts or ends at a corner in
   * the corresponding input face, then we need only look for the
   * "starts at" case, because if it is "ends at" in this face, it
   * should be "starts at" in the matching face.
   */
  const Span<int> face_map = this->ensure_face_map();
  const Span<int> vert_map = this->ensure_vertex_map();
  const Span<int> corner_map = this->ensure_corner_map();
  /* To parallelize this, would need a way to figure out that
   * this is the "canonical" edge representative so that only
   * one thread tries to write this. Or could use atomic operations.
   */
#  ifdef DEBUG_TIME
  timeit::ScopedTimer timer("filling edge map");
#  endif
  edge_map_ = Array<int>(output_mesh_->edges_num, -1);
  const Span<int> out_corner_edges = output_mesh_->corner_edges();
  const Span<int> out_corner_verts = output_mesh_->corner_verts();
  const Span<int2> out_edges = output_mesh_->edges();
  const Span<float3> out_positions = output_mesh_->vert_positions();
  const Span<int> in_corner_edges = joined_mesh_->corner_edges();
  const Span<int> in_corner_verts = joined_mesh_->corner_verts();
  const Span<int2> in_edges = joined_mesh_->edges();
  const Span<float3> in_positions = joined_mesh_->vert_positions();
  const OffsetIndices<int> in_faces = joined_mesh_->faces();
  const OffsetIndices<int> out_faces = output_mesh_->faces();
  Array<bool> done_edge(output_mesh_->edges_num, false);
  for (const int out_face_index : IndexRange(output_mesh_->faces_num)) {
    const int in_face_index = face_map[out_face_index];
    const IndexRange in_face = in_faces[in_face_index];
    const IndexRange in_face_vert_range = vertex_range_for_face(in_face_index, *mesh_offsets_);
    if (dbg_level > 0) {
      std::cout << "process out_face = " << out_face_index << ", in_face = " << in_face_index
                << "\n";
    }
    for (const int out_c : out_faces[out_face_index]) {
      const int in_c = corner_map[out_c];
      if (dbg_level > 0) {
        std::cout << "  out_c = " << out_c << ", in_c = " << in_c << "\n";
      }
      if (in_c == -1) {
        /* No possible "starts at" match here. */
        continue;
      }
      const int out_e = out_corner_edges[out_c];
      if (dbg_level > 0) {
        std::cout << "  out_e = " << out_e << ", done = " << done_edge[out_e] << "\n";
      }
      if (done_edge[out_e]) {
        continue;
      }
      const int out_v = out_corner_verts[out_c];
      const int in_e = in_corner_edges[in_c];
      const int in_v = in_corner_verts[in_c];
      /* Because of corner mapping, the output vertex should map to the input one. */
      BLI_assert(vert_map[out_v] == in_v);
      int2 out_e_v = out_edges[out_e];
      if (out_e_v[0] != out_v) {
        out_e_v = {out_e_v[1], out_e_v[0]};
      }
      int2 in_e_v = in_edges[in_e];
      if (in_e_v[0] != in_v) {
        in_e_v = {in_e_v[1], in_e_v[0]};
      }
      if (dbg_level > 0) {
        std::cout << "  out_v = " << out_v << ", in_e = " << in_e << ", in_v = " << in_v << "\n";
        std::cout << "  out_e_v = " << out_e_v << ", in_e_v = " << in_e_v << "\n";
        std::cout << "  vertex_map(out_e_v) = " << int2(vert_map[out_e_v[0]], vert_map[out_e_v[1]])
                  << "\n";
      }
      /* Here out_e_v should hold the output vertices in out_e, with the first
       * one being out_v, the vertex at corner out_c.
       * Similarly for in_e_v, with the first one being in_v.
       */
      BLI_assert(vert_map[out_e_v[0]] == in_e_v[0]);
      int edge_rep = -1;
      if (vert_map[out_e_v[1]] == in_e_v[1]) {
        /* Here both ends of the edges match. */
        if (dbg_level > 0) {
          std::cout << "  case 1, edge_rep = in_e = " << in_e << "\n";
        }
        edge_rep = in_e;
      }
      else if (!in_face_vert_range.contains(vert_map[out_e_v[1]])) {
        /* Here the "ends at" vertex of the output edge is a new vertex or in a different mesh.
         * Does the edge at least go in the same direction as in_e?
         */
        if (same_dir(out_positions[out_e_v[0]],
                     out_positions[out_e_v[1]],
                     in_positions[in_e_v[0]],
                     in_positions[in_e_v[1]]))
        {
          if (dbg_level > 0) {
            std::cout << "  case 2, edge_rep = in_e = " << in_e << "\n";
          }
          edge_rep = in_e;
        }
      }
      /* It is possible that the output face and corresponding
       * input face have opposite winding. So do all of the previous
       * again with the previous edge of input face but same edge of
       * output face.
       */
      if (edge_rep == -1) {
        const int in_c_prev = bke::mesh::face_corner_prev(in_face, in_c);
        const int in_e_prev = in_corner_edges[in_c_prev];
        const int in_v_prev = in_corner_verts[in_c_prev];
        int2 in_e_v_prev = in_edges[in_e_prev];
        if (in_e_v_prev[0] != in_v_prev) {
          in_e_v_prev = {in_e_v_prev[1], in_e_v_prev[0]};
        }
        if (dbg_level > 0) {
          std::cout << "  in_c_prev = " << in_c_prev << ", in_e_prev = " << in_e_prev
                    << ", in_v_prev = " << in_v_prev << "\n";
          std::cout << "  in_e_v_prev = " << in_e_v_prev << "\n";
        }
        if (vert_map[out_e_v[0]] == in_e_v_prev[1]) {
          if (vert_map[out_e_v[1]] == in_e_v_prev[0]) {
            if (dbg_level > 0) {
              std::cout << "  case 3, edge_rep = in_e_prev = " << in_e_prev << "\n";
            }
            edge_rep = in_e_prev;
          }
          else if (vert_map[out_e_v[1]] == -1) {
            if (same_dir(out_positions[out_e_v[0]],
                         out_positions[out_e_v[1]],
                         in_positions[in_e_v_prev[0]],
                         in_positions[in_e_v_prev[1]]))
            {
              if (dbg_level > 0) {
                std::cout << "  case 4, edge_rep = in_e_prev = " << in_e_prev << "\n";
              }
              edge_rep = in_e_prev;
            }
          }
        }
      }
      if (edge_rep != -1) {
        if (dbg_level > 0) {
          std::cout << "  found: set edge_map[" << out_e << "] = " << edge_rep << "\n";
        }
        edge_map_[out_e] = edge_rep;
        done_edge[out_e] = true;
      }
    }
  }
  return edge_map_;
}

/** Most input faces should map to face_group_inline or fewer output triangles. */
constexpr int face_group_inline = 4;

/**
 * Return an array of length \a input_faces_num, where the i'th entry
 * is a Vector of the \a mgl triangles that derive from the i'th input
 * face (where i is an index in the concatenated input mesh face space.
 */
static Array<Vector<int, face_group_inline>> get_face_groups(const MeshGL &mgl,
                                                             int input_faces_num)
{
#  ifdef DEBUG_TIME
  timeit::ScopedTimer timer("get_face_groups");
#  endif
  constexpr int dbg_level = 0;
  Array<Vector<int, face_group_inline>> fg(input_faces_num);
  const int tris_num = mgl.NumTri();
  BLI_assert(mgl.faceID.size() == tris_num);
  for (const int t : IndexRange(tris_num)) {
    const int faceid = mgl.faceID[t];
    fg[faceid].append(t);
  }
  if (dbg_level > 0) {
    std::cout << "face_groups\n";
    for (const int i : fg.index_range()) {
      std::cout << "orig face " << i;
      dump_span(fg[i].as_span(), "");
    }
  }
  return fg;
}

#  if 0
/* TODO: later */
/**
 * Return 1 if \a group is just the same as the original face \a face_index
 * in \a mesh.
 * Return 2 if it is the same but with the normal reversed.
 * Return 0 otherwise.
 */
static uchar check_original_face(const Vector<int, face_group_inline> &group,
                                 const MeshGL &mgl,
                                 const Mesh *mesh,
                                 int face_index)
{
  BLI_assert(0 <= face_index && face_index < mesh->faces_num);
  const IndexRange orig_face = mesh->faces()[face_index];
  /* The face can't be original if the number of triangles isn't equal
   * to the original face size minus 2. */
  int orig_face_size = orig_face.size();
  if (orig_face_size != group.size() + 2) {
    return 0;
  }
  Span<int> orig_face_verts = mesh->corner_verts().slice(mesh->faces()[face_index]);
  /* edge_value[i] will be 1 if that edge is identical to an output edge,
   * and -1 if it is the reverse of an output edge. */
  Array<uchar, 20> edge_value(orig_face_size, 0);
  int stride = mgl.numProp;
  for (const int t : group) {
    /* face_vert_index[i] will hold the position in input face face_index
     * where the i'th vertex of triangle t is (assuming that no position
     * is exactly repeated in an output face). -1 if there is no such. */
    Array<int, 3> face_vert_index(3);
    for (const int i : IndexRange(3)) {
      int v = mgl.triVerts[3 * t + i];
      int prop_offset = v * stride;
      float3 pos(mgl.vertProperties[prop_offset],
                 mgl.vertProperties[prop_offset + 1],
                 mgl.vertProperties[prop_offset + 2]);
      auto it = std::find_if(orig_face_verts.begin(), orig_face_verts.end(), [&](int orig_v) {
        return pos == mesh->vert_positions()[orig_v];
      });
      face_vert_index[i] = it == orig_face_verts.end() ?
                               -1 :
                               std::distance(orig_face_verts.begin(), it);
    }
    /* Now we can tell which original edges are covered by t. */
    for (const int i : IndexRange(3)) {
      const int a = face_vert_index[i];
      const int b = face_vert_index[(i + 1) % 3];
      if (a != -1 && b != -1) {
        if ((a + 1) % orig_face_size == b) {
          edge_value[a] = 1;
        }
        else if ((b + 1) % orig_face_size == a) {
          edge_value[b] = -1;
        }
      }
    }
  }
  if (std::all_of(edge_value.begin(), edge_value.end(), [](int x) { return x == 1; })) {
    return 1;
  }
  else if (std::all_of(edge_value.begin(), edge_value.end(), [](int x) { return x == -1; })) {
    return 2;
  }
  return 0;
}
#  endif

static OutFace make_out_face(const MeshGL &mgl, int tri_index, int orig_face)
{
  OutFace ans;
  ans.verts = Vector<int, inline_outface_size>(3);
  const int k = 3 * tri_index;
  ans.verts[0] = mgl.triVerts[k];
  ans.verts[1] = mgl.triVerts[k + 1];
  ans.verts[2] = mgl.triVerts[k + 2];
  ans.face_id = orig_face;
  return ans;
}

/**
 * For face merging, there is this indexing space:
 * "group edge" index:  linearized indices of edges in the
 * triangles in the group.
 * A SharedEdge has two such indices, with the assertion that
 * they have the same vertices (but in opposite order).
 */
struct SharedEdge {
  /* First shared edge ("group edge" indexing). */
  int e1;
  /* Second shared edge. */
  int e2;
  /* First vertex for e1 (second for e2). */
  int v1;
  /* Second vertex for e1 (first for e2). */
  int v2;

  SharedEdge(int e1, int e2, int v1, int v2) : e1(e1), e2(e2), v1(v1), v2(v2) {}

  /* Return the indices (in the linearized triangle space of an OutFace group)
   * corresponding to e1 and e2. */
  int2 outface_group_face_indices() const
  {
    return int2(e1 / 3, e2 / 3);
  }
};

/** Canonical SharedEdge has v1 < v2. */
static inline SharedEdge canon_shared_edge(int e1, int e2, int v1, int v2)
{
  if (v1 < v2) {
    return SharedEdge(e1, e2, v1, v2);
  }
  return SharedEdge(e2, e1, v2, v1);
}

/**
 * Special case of get_shared_edges when there are two faces.
 * Return the version of SharedEdge where 0 <= e1 < 3 and 3 <= e2 < 6.
 * If there is no shared edge, return SharedEdge(-1, -1, -1, -1).
 */
static SharedEdge get_shared_edge_from_pair(const OutFace &tri1, const OutFace &tri2)
{
  /* There should be at most one shared edge between the tri1 and tri2. Find it. */
  SharedEdge shared_edge(-1, -1, -1, -1);
  for (const int i1 : IndexRange(3)) {
    for (const int i2 : IndexRange(3)) {
      const int v1 = tri1.verts[i1];
      const int v2 = tri2.verts[i2];
      if (v1 == v2) {
        const int v1_next = tri1.verts[(i1 + 1) % 3];
        const int v2_prev = tri2.verts[(i2 + 2) % 3];
        if (v1_next == v2_prev) {
          shared_edge = SharedEdge(i1, 3 + ((i2 + 2) % 3), v1, v1_next);
          break;
        }
        const int v1_prev = tri1.verts[(i1 + 2) % 3];
        const int v2_next = tri2.verts[(i2 + 1) % 3];
        if (v1_prev == v2_next) {
          shared_edge = SharedEdge((i1 + 2) % 3, 3 + i2, v1_prev, v1);
          break;
        }
      }
    }
    if (shared_edge.e1 != -1) {
      break;
    }
  }
  return shared_edge;
}

/**
 * Given a span of OutFaces, all triangles, find as many SharedEdge's as possible.
 * A SharedEdge is one where it is in two triangles but with the vertices in opposite order.
 * The edge ids are given as indexes into all the edges of \a faces in order.
 */
static Vector<SharedEdge> get_shared_edges(Span<OutFace> faces)
{
  Vector<SharedEdge> ans;
  /* Map from two vert indices making an edge to where that edge appears
   * in list of group edges. */
  Map<int2, int> edge_verts_to_tri;
  for (const int face_index : faces.index_range()) {
    const OutFace &f = faces[face_index];
    for (const int i : IndexRange(3)) {
      int v1 = f.verts[i];
      int v2 = f.verts[(i + 1) % 3];
      int this_e = face_index * 3 + i;
      edge_verts_to_tri.add_new(int2(v1, v2), this_e);
      int other_e = edge_verts_to_tri.lookup_default(int2(v2, v1), -1);
      if (other_e != -1) {
        ans.append(canon_shared_edge(this_e, other_e, v1, v2));
      }
    }
  }
  return ans;
}

/**
 * Return true if the splice of faces \a f1 and \a f2 forms a legal face (no repeated verts).
 * The splice will be between vertices \a v1 and \a v2, which are assumed to not be
 * repeated in the other face (since incoming faces are assumed legal).
 */
static bool is_legal_merge(const OutFace &f1, const OutFace &f2, int v1, int v2)
{
  /* For now, just look for each non-splice-involved vertex of each face to see if
   * it is in the other face.
   * TODO: if the faces are big, sort both together and look for repeats after sorting.
   */
  for (const int v : f1.verts) {
    if (!ELEM(v, v1, v2)) {
      if (f2.find_vert_index(v) != -1) {
        return false;
      }
    }
  }
  for (const int v : f2.verts) {
    if (!ELEM(v, v1, v2)) {
      if (f1.find_vert_index(v) != -1) {
        return false;
      }
    }
  }
  return true;
}

/**
 * Try merging OutFaces \a f1 and \a f2, which should have a \a se as a shared edge.
 * Assume the shared edge has v1,v2 in CCW order in f1, and in the opposite order in f2.
 * This involves splicing the two faces together and checking that there
 * is no repeated vertex if this is done.
 * If the merge is successful, update f1 to be the merged face and return true,
 * else leave the faces alone and return false.
 */
static bool try_merge_out_face_pair(OutFace &f1, const OutFace &f2, const SharedEdge &se)
{

  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "try_merge_out_face_pair\n";
    dump_span(f1.verts.as_span(), "f1");
    dump_span(f2.verts.as_span(), "f2");
    std::cout << "shared edge: "
              << "(e" << se.e1 << ",e" << se.e2 << ";v" << se.v1 << ",v" << se.v2 << ")\n";
  }
  const int f1_len = f1.verts.size();
  const int f2_len = f2.verts.size();
  const int v1 = se.v1;
  const int v2 = se.v2;
  /* Find i1, the index of the earlier of v1 and v2 in f1,
   * and i2, the index of the earlier of v1 and v2 in f2. */
  const int i1 = f1.find_vert_index(v1);
  BLI_assert(i1 != -1);
  const int i1_next = (i1 + 1) % f1_len;
  const int i2 = f2.find_vert_index(v2);
  BLI_assert(i2 != -1);
  const int i2_next = (i2 + 1) % f2_len;
  BLI_assert(f1.verts[i1] == v1 && f1.verts[i1_next] == v2);
  BLI_assert(f2.verts[i2] == v2 && f2.verts[i2_next] == v1);
  const bool can_merge = is_legal_merge(f1, f2, v1, v2);
  if (dbg_level > 0) {
    std::cout << "i1 = " << i1 << ", i2 = " << i2 << ", can_merge = " << can_merge << "\n";
  }
  if (!can_merge) {
    return false;
  }
  /* The merged face is the concatenation of these slices
   * (giving inclusive indices, with implied wrap-around at end of faces):
   * f1 : [0, i1]
   * f2 : [i2_next+1, i2_prev]
   * f1 : [i1_next, f1_len-1]
   */
  const int i2_prev = (i2 + f2_len - 1) % f2_len;
  const int i2_next_next = (i2_next + 1) % f2_len;
  const auto *f2_start_it = f2.verts.begin() + i2_next_next;
  const auto *f2_end_it = f2.verts.begin() + i2_prev + 1;
  if (f2_end_it > f2_start_it) {
    f1.verts.insert(i1_next, f2_start_it, f2_end_it);
  }
  else {
    const int n1 = std::distance(f2_start_it, f2.verts.end());
    if (n1 > 0) {
      f1.verts.insert(i1_next, f2_start_it, f2.verts.end());
    }
    if (n1 < f2_len - 2) {
      f1.verts.insert(i1_next + n1, f2.verts.begin(), f2_end_it);
    }
  }
  if (dbg_level > 0) {
    dump_span(f1.verts.as_span(), "merge result");
  }
  return true;
}

/** Special case (for speed) merge_out_faces when faces has two triangles. */
static void merge_out_face_pair(Vector<OutFace> &faces)
{
  constexpr int dbg_level = 0;
  BLI_assert(faces.size() == 2);
  OutFace &tri1 = faces[0];
  OutFace &tri2 = faces[1];
  if (dbg_level > 0) {
    std::cout << "\nmerge_out_face_pair for faceid " << faces[0].face_id << "\n";
    dump_span(tri1.verts.as_span(), "tri1");
    dump_span(tri2.verts.as_span(), "tri2");
  }
  const SharedEdge shared_edge = get_shared_edge_from_pair(tri1, tri2);
  if (shared_edge.e1 == -1) {
    /* No shared edge, so no merging possible. */
    return;
  }
  const int va = shared_edge.v1;
  const int vb = shared_edge.v2;
  const int e1 = shared_edge.e1;
  const int e2 = shared_edge.e2;
  if (dbg_level > 0) {
    std::cout << "shared_edge = e" << e1 << ", e" << e2 << "; " << va << ", " << vb << "\n";
  }
  BLI_assert(e1 < 3 && e2 >= 3);
  /* Say tri1 has verts starting at pos e1 called a, b, c.
   * Then tri2 has verts starting at pos e2-3 called b, a, d.
   * So the quad we want is b, c, a, d.
   */
  const int vc = tri1.verts[(e1 + 2) % 3];
  const int vd = tri2.verts[(e2 - 3 + 2) % 3];
  BLI_assert(tri1.verts[e1] == va && tri1.verts[(e1 + 1) % 3] == vb && tri2.verts[e2 - 3] == vb &&
             tri2.verts[(e2 - 3 + 1) % 3] == va);
  if (vc == vd) {
    /* This can't happen geometrically, but maybe in extreme cases... */
    return;
  }
  tri1.verts.resize(4);
  tri1.verts[0] = vb;
  tri1.verts[1] = vc;
  tri1.verts[2] = va;
  tri1.verts[3] = vd;
  if (dbg_level > 0) {
    dump_span(tri1.verts.as_span(), "merged quad");
  }
  faces.resize(1);
}

/**
 * Give a group of #OutFace's that are all from a same original mesh face,
 * remove as many dissolvable edges as possible while still keeping the faces legal.
 * A face is legal if it has no repeated vertices and has size at least 3.
 */
static void merge_out_faces(Vector<OutFace> &faces)
{
  constexpr int dbg_level = 0;
  if (faces.size() <= 1) {
    return;
  }
  if (faces.size() == 2) {
    merge_out_face_pair(faces);
    return;
  }
  if (dbg_level > 0) {
    std::cout << "\nmerge_out_faces for faceid " << faces[0].face_id << "\n";
    for (const int i : faces.index_range()) {
      const OutFace &f = faces[i];
      dump_span(f.verts.as_span(), std::to_string(i));
    }
  }
  Vector<SharedEdge> shared_edges = get_shared_edges(faces);
  if (dbg_level > 0) {
    std::cout << "shared edges:\n";
    for (const SharedEdge &se : shared_edges) {
      std::cout << "(e" << se.e1 << ",e" << se.e2 << ";v" << se.v1 << ",v" << se.v2 << ")";
    }
    std::cout << "\n";
    // dump_span(shared_edges.as_span(), "shared edges");
  }
  if (shared_edges.is_empty()) {
    return;
  }
  /* `shared_edge_valid[i]` is true if both edges in shared_edges[i] are still alive. */
  Array<bool> shared_edge_valid(shared_edges.size(), true);
  /* If `merged_to_faces[i]` is not -1, then argument faces[i] has been merged to that other face.
   */
  Array<int> merged_to(faces.size(), -1);
  /* Local function to follow merged_to mappings as far as possible. */
  auto final_merged_to = [&](int f_orig) {
    BLI_assert(f_orig != -1);
    int f_mapped = f_orig;
    do {
      if (merged_to[f_mapped] != -1) {
        f_mapped = merged_to[f_mapped];
      }
    } while (merged_to[f_mapped] != -1);
    return f_mapped;
  };
  /* TODO: sort shared_edges by decreasing length. */
  for (const int i : shared_edges.index_range()) {
    if (!shared_edge_valid[i]) {
      continue;
    }
    const SharedEdge se = shared_edges[i];
    const int2 orig_faces = se.outface_group_face_indices();
    const int2 cur_faces = int2(final_merged_to(orig_faces[0]), final_merged_to(orig_faces[1]));
    const int f1 = cur_faces[0];
    const int f2 = cur_faces[1];
    if (f1 == -1 || f2 == -2) {
      continue;
    }
    if (dbg_level > 0) {
      std::cout << "try merge of faces " << f1 << " and " << f2 << "\n";
    }
    if (try_merge_out_face_pair(faces[f1], faces[f2], se)) {
      if (dbg_level > 0) {
        std::cout << "successful merge\n";
        dump_span(faces[f1].verts.as_span(), "new f1");
      }
      merged_to[f2] = f1;
    }
  }
  /* Now compress the surviving faces. */
  int move_from = 0;
  int move_to = 0;
  const int orig_faces_num = faces.size();
  while (move_from < orig_faces_num) {
    /* Don't move faces that have been merged elsewhere. */
    while (move_from < orig_faces_num && merged_to[move_from] != -1) {
      move_from++;
    }
    if (move_from >= orig_faces_num) {
      break;
    }
    if (move_to < move_from) {
      faces[move_to] = faces[move_from];
    }
    move_to++;
    move_from++;
  }
  if (move_to < orig_faces_num) {
    faces.resize(move_to);
  }
  if (dbg_level > 0) {
    std::cout << "final faces:\n";
    for (const int i : faces.index_range()) {
      dump_span(faces[i].verts.as_span(), std::to_string(i));
    }
  }
}

/** Return true if the points p0, p1, p2 are approximately in a straight line. */
static inline bool approx_in_line(const float3 &p0, const float3 &p1, const float3 &p2)
{
  float cos_ang = math::dot(math::normalize(p1 - p0), math::normalize(p2 - p1));
  return math::abs(cos_ang - 1.0) < 1e-4;
}

/**
 * Find redundant valence-2 vertices in the output faces #ma.new_faces and dissolve them.
 * A vertex is redundant if it is valence-2 in both shared faces and if it is between
 * two neighbors in approximately a straight line.
 * These can be the result of triangulation edges intersecting the result geometry,
 * and then being dissolved by merge_out_faces.
 * TODO: don't do this if the vertex was original.
 * (To do that we need the mapping from input to output verts to be passed as an argument,
 * and at th moment, we don't do that mapping yet -- and would have to redo it if we end up
 * dissolving vert.)
 */
static void dissolve_valence2_verts(MeshAssembly &ma)
{
  const int vnum = ma.output_verts_num;
  Array<bool> dissolve(vnum, false);
  /* We'll remember up to two vertex neighbors for each vertex. */
  Array<std::pair<int, int>> neighbors(ma.output_verts_num, std::pair<int, int>(-1, -1));
  /* First, tentatively set dissolve based on neighbors. Alignment will be checked later. */
  for (const int f : ma.new_faces.index_range()) {
    const OutFace &face = ma.new_faces[f];
    const int fsize = face.verts.size();
    for (const int i : IndexRange(fsize)) {
      const int vprev = face.verts[(i - 1 + fsize) % fsize];
      const int v = face.verts[i];
      const int vnext = face.verts[(i + 1) % fsize];
      std::pair<int, int> &v_nbrs = neighbors[v];
      if (v_nbrs.first == -1) {
        /* First time we've seen v. Set the tentative neighbors. */
        v_nbrs.first = vprev;
        v_nbrs.second = vnext;
        /* Don't want to dissolve any vert of a triangle, even if triangle is degenerate. */
        dissolve[v] = fsize <= 3 ? false : true;
      }
      else {
        /* Some previous face had v. Disable dissolve unless if neighbors are the same, reversed,
         * or if this face is a triangle.
         */
        if (fsize == 3 || !(vprev == v_nbrs.second && vnext == v_nbrs.first)) {
          dissolve[v] = false;
        }
      }
    }
  }
  /* We can't dissolve so many verts in a face that it leaves less than a triangle.
   * This should be rare, since the above logic will prevent dissolving a vert from a triangle,
   * but it is possible that two or more verts are to be dissolved from a quad or ngon.
   * Do a pass to remove the possibility of dissolving anything from such faces. */
  for (const int f : ma.new_faces.index_range()) {
    const OutFace &face = ma.new_faces[f];
    const int fsize = face.verts.size();
    int num_dissolved = 0;
    for (const int i : IndexRange(fsize)) {
      if (dissolve[face.verts[i]]) {
        num_dissolved++;
      }
    }
    if (fsize - num_dissolved < 3) {
      for (const int i : IndexRange(fsize)) {
        dissolve[face.verts[i]] = false;
      }
    }
  }
  /* Now, for tentative dissolves, check "in a straight line" condition. */
  const int grain_size = 15000;
  bool any_dissolve = false;
  threading::parallel_for(IndexRange(vnum), grain_size, [&](const IndexRange range) {
    bool range_any_dissolve = false;
    for (const int v : range) {
      if (dissolve[v]) {
        std::pair<int, int> &v_nbrs = neighbors[v];
        BLI_assert(v_nbrs.first != -1 && v_nbrs.second != -1);
        const float3 p0 = ma.vert_position(v_nbrs.first);
        const float3 p1 = ma.vert_position(v);
        const float3 p2 = ma.vert_position(v_nbrs.second);
        if (!approx_in_line(p0, p1, p2)) {
          dissolve[v] = false;
        }
        else {
          range_any_dissolve = true;
        }
      }
    }
    if (range_any_dissolve) {
      /* No need for atomics here as this is a single byte. */
      any_dissolve = true;
    }
  });
  if (!any_dissolve) {
    return;
  }

  /* We need to compress out the dissolved vertices out of `ma.vertpos`,
   * remap all the faces to account for that compression,
   * and rebuild any faces containing those compressed verts.
   * The compressing part is a bit like #mesh_copy_selection. */
  IndexMaskMemory memory;
  IndexMask keep = IndexMask::from_bools_inverse(
      dissolve.index_range(), dissolve.as_span(), memory);
  const int new_vnum = keep.size();
  ma.old_to_new_vert_map.reinitialize(vnum);
  ma.old_to_new_vert_map.fill(-1);
  index_mask::build_reverse_map<int>(keep, ma.old_to_new_vert_map);

  /* Compress `vertpos` in place. Is there a parallel way to do this? */
  float *vpos_data = ma.vertpos.data();
  BLI_assert(ma.vertpos_stride == 3);
  for (const int old_v : IndexRange(vnum)) {
    const int new_v = ma.old_to_new_vert_map[old_v];
    BLI_assert(new_v <= old_v);
    if (new_v >= 0) {
      std::copy_n(vpos_data + 3 * old_v, 3, vpos_data + 3 * new_v);
    }
  }
  ma.vertpos = ma.vertpos.take_front(new_vnum * ma.vertpos_stride);
  ma.output_verts_num = new_vnum;

  /* Remap verts and compress dissolved verts in output faces. */
  threading::parallel_for(ma.new_faces.index_range(), 10000, [&](IndexRange range) {
    for (const int f : range) {
      OutFace &face = ma.new_faces[f];
      int i_to = 0;
      for (const int i_from : face.verts.index_range()) {
        const int mapped_v_from = ma.mapped_vert(face.verts[i_from]);
        if (mapped_v_from >= 0) {
          face.verts[i_to++] = mapped_v_from;
        }
      }
      if (i_to < face.verts.size()) {
        BLI_assert(i_to >= 3);
        face.verts.resize(i_to);
      }
    }
  });
}

/**
 * Build the MeshAssembly corresponding to \a mgl.
 * This involves:
 *  (1) Pointing at output vertices.
 *  (2) Making a map from output vertices to input vertices (using -1 if no match).
 *  (3) Making initial face_groups, where each face group is all the output triangles that
 *      were part of the same input face.
 *  (4) For each face group, remove as many shared edges as possible.
 */
static MeshAssembly assemble_mesh_from_meshgl(MeshGL &mgl, const MeshOffsets &mesh_offsets)
{
#  ifdef DEBUG_TIME
  timeit::ScopedTimer timer("calculating assemble_mesh_from_meshgl");
#  endif
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "assemble_mesh_from_meshgl\n";
  }
  MeshAssembly ma;
  ma.vertpos = MutableSpan<float>(&*mgl.vertProperties.begin(), mgl.vertProperties.size());
  ma.vertpos_stride = mgl.numProp;
  ma.input_verts_num = mesh_offsets.vert_start.last();
  ma.output_verts_num = ma.vertpos.size() / ma.vertpos_stride;
  const int input_faces_num = mesh_offsets.face_start.last();

  /* For each offset input mesh face, what mgl triangles have it as id? */
  Array<Vector<int, face_group_inline>> face_groups = get_face_groups(mgl, input_faces_num);
  if (dbg_level > 1) {
    std::cout << "groups:\n";
    for (const int i : face_groups.index_range()) {
      std::cout << "orig (offset) face " << i << ": ";
      dump_span(face_groups[i].as_span(), "");
    }
  }
  {
#  ifdef DEBUG_TIME
    timeit::ScopedTimer timer("face merging");
#  endif
    Vector<Vector<OutFace>> new_groups(face_groups.size());
    const int grain_size = 15000;
    threading::parallel_for(face_groups.index_range(), grain_size, [&](const IndexRange range) {
      for (const int gid : range) {
        const Span<int> group = face_groups[gid].as_span();
        Vector<OutFace> &group_faces = new_groups[gid] = Vector<OutFace, 4>(group.size());
        for (const int i : group_faces.index_range()) {
          int tri_index = group[i];
          group_faces[i] = make_out_face(mgl, tri_index, gid);
        }
        merge_out_faces(group_faces);
      }
    });
#  ifdef DEBUG_TIME
    timeit::ScopedTimer xtimer("copying groups at end");
#  endif
    for (const int i : new_groups.index_range()) {
      ma.new_faces.extend(new_groups[i].as_span());
    }
  }
  {
#  ifdef DEBUG_TIME
    timeit::ScopedTimer timer("valence-2-vertex dissolving");
#  endif
    dissolve_valence2_verts(ma);
    if (ma.old_to_new_vert_map.size() > 0) {
      /* We compressed ma.vertpos in place, but that really means
       * we compressed mgl.VertProperties, so we need to change its size. */
      mgl.vertProperties.resize(ma.vertpos.size());
    }
  }
  if (dbg_level > 0) {
    std::cout << "mesh_assembly result:\n";
    std::cout << "input_verts_num = " << ma.input_verts_num
              << ", output_verts_num = " << ma.output_verts_num << "\n";
    dump_span_with_stride(ma.vertpos.as_span(), ma.vertpos_stride, "vertpos");
    std::cout << "new_faces:\n";
    for (const int i : ma.new_faces.index_range()) {
      std::cout << i << ": face_id = " << ma.new_faces[i].face_id << "\nverts ";
      dump_span(ma.new_faces[i].verts.as_span(), "");
    }
  }
  return ma;
}

template<typename T>
void copy_attribute_using_map(const Span<T> src, const Span<int> out_to_in_map, MutableSpan<T> dst)
{
  const int grain_size = 20000;
  threading::parallel_for(out_to_in_map.index_range(), grain_size, [&](const IndexRange range) {
    for (const int out_elem : range) {
      const int in_elem = out_to_in_map[out_elem];
      if (in_elem == -1) {
        dst[out_elem] = T();
      }
      else {
        dst[out_elem] = src[in_elem];
      }
    }
  });
}

void copy_attribute_using_map(const GSpan src, const Span<int> out_to_in_map, GMutableSpan dst)
{
  const CPPType &type = dst.type();
  bke::attribute_math::convert_to_static_type(type, [&](auto dummy) {
    using T = decltype(dummy);
    copy_attribute_using_map(src.typed<T>(), out_to_in_map, dst.typed<T>());
  });
}

void interpolate_corner_attributes(bke::MutableAttributeAccessor output_attrs,
                                   const bke::AttributeAccessor input_attrs,
                                   Mesh *output_mesh,
                                   const Mesh *input_mesh,
                                   const Span<int> out_to_in_corner_map,
                                   const Span<int> out_to_in_face_map)
{
#  ifdef DEBUG_TIME
  timeit::ScopedTimer timer("interpolate corner attributes");
#  endif
  /* Make parallel arrays of things needed access and write all corner attributes to interpolate.
   */
  Vector<bke::GSpanAttributeWriter> writers;
  Vector<bke::GAttributeReader> readers;
  Vector<GVArraySpan> srcs;
  Vector<GMutableSpan> dsts;
  /* For each index of `srcs` and `dsts`, we need to know if it is a "normal"-like attribute. */
  Vector<bool> is_normal_attribute;
  input_attrs.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain != bke::AttrDomain::Corner || ELEM(iter.name, ".corner_vert", ".corner_edge"))
    {
      return;
    }
    const bke::GAttributeReader reader = input_attrs.lookup_or_default(
        iter.name, iter.domain, iter.data_type);
    if (!reader) {
      return;
    }
    writers.append(
        output_attrs.lookup_or_add_for_write_span(iter.name, iter.domain, iter.data_type));
    readers.append(input_attrs.lookup_or_default(iter.name, iter.domain, iter.data_type));
    srcs.append(*readers.last());
    dsts.append(writers.last().span);
    is_normal_attribute.append(iter.name == "custom_normal");
  });

  if (writers.is_empty()) {
    return;
  }

  /* Loop per source face, as there is an expensive weight calculation that needs to be done per
   * face. */
  const OffsetIndices<int> output_faces = output_mesh->faces();
  const OffsetIndices<int> input_faces = input_mesh->faces();
  const Span<int> input_corner_verts = input_mesh->corner_verts();
  const Span<float3> input_vert_positions = input_mesh->vert_positions();
  const Span<int> output_corner_verts = output_mesh->corner_verts();
  const Span<float3> output_vert_positions = output_mesh->vert_positions();
  const int grain_size = 256;
  threading::parallel_for(
      out_to_in_face_map.index_range(), grain_size, [&](const IndexRange range) {
        Vector<float, 20> weights;
        Vector<float2, 20> cos_2d;
        for (const int out_face_index : range) {
          const int in_face_index = out_to_in_face_map[out_face_index];
          const IndexRange in_face = input_faces[in_face_index];
          /* Are there any corners needing interpolation in this face?
           * The corners needing interpolation are those whose out_to_in_corner_map entry is -1.
           */
          IndexRange out_face = output_faces[out_face_index];
          if (std::none_of(out_face.begin(), out_face.end(), [&](int c) {
                return out_to_in_corner_map[c] == -1;
              }))
          {
            for (const int attr_index : dsts.index_range()) {
              const GSpan src = srcs[attr_index];
              GMutableSpan dst = dsts[attr_index];
              const CPPType &type = dst.type();
              for (const int dst_corner : out_face) {
                type.copy_construct(src[out_to_in_corner_map[dst_corner]], dst[dst_corner]);
              }
            }
            continue;
          }

          /* At least one output corner did not map to an input corner. */

          /* First get coordinates of input face projected onto 2d, and make sure that
           * weights has the right size. */
          const Span<int> in_face_verts = input_corner_verts.slice(in_face);
          const int in_face_size = in_face.size();
          const Span<int> out_face_verts = output_corner_verts.slice(out_face);
          weights.resize(in_face_size);
          cos_2d.resize(in_face_size);
          float (*cos_2d_p)[2] = reinterpret_cast<float (*)[2]>(cos_2d.data());
          const float3 axis_dominant = bke::mesh::face_normal_calc(input_vert_positions,
                                                                   in_face_verts);
          float3x3 axis_mat;
          axis_dominant_v3_to_m3(axis_mat.ptr(), axis_dominant);
          /* We also need to know if the output face has a flipped normal compared
           * to the corresponding input face (used if we have custom normals).
           */
          const float3 out_face_normal = bke::mesh::face_normal_calc(output_vert_positions,
                                                                     out_face_verts);
          const bool face_is_flipped = math::dot(axis_dominant, out_face_normal) < 0.0;
          for (const int i : in_face_verts.index_range()) {
            const float3 &co = input_vert_positions[in_face_verts[i]];
            cos_2d[i] = (axis_mat * co).xy();
          }
          /* Now the loop to actually interpolate attributes of the new-vertex corners of the
           * output face. */
          for (const int out_c : out_face) {
            const int in_c = out_to_in_corner_map[out_c];
            if (in_c != -1) {
              for (const int attr_index : dsts.index_range()) {
                const GSpan src = srcs[attr_index];
                GMutableSpan dst = dsts[attr_index];
                const CPPType &type = dst.type();
                type.copy_construct(src[in_c], dst[out_c]);
              }
              continue;
            }
            const int out_v = output_corner_verts[out_c];
            float2 co;
            mul_v2_m3v3(co, axis_mat.ptr(), output_vert_positions[out_v]);
            interp_weights_poly_v2(weights.data(), cos_2d_p, in_face_size, co);

            for (const int attr_index : dsts.index_range()) {
              const GSpan src = srcs[attr_index];
              GMutableSpan dst = dsts[attr_index];
              const bool need_flip = face_is_flipped && is_normal_attribute[attr_index];
              const CPPType &type = dst.type();
              bke::attribute_math::convert_to_static_type(type, [&](auto dummy) {
                using T = decltype(dummy);
                const Span<T> src_typed = src.typed<T>();
                MutableSpan<T> dst_typed = dst.typed<T>();
                bke::attribute_math::DefaultMixer<T> mixer{MutableSpan(&dst_typed[out_c], 1)};
                for (const int i : in_face.index_range()) {
                  mixer.mix_in(0, src_typed[in_face[i]], weights[i]);
                }
                mixer.finalize();
                if (need_flip) {
                  /* The joined mesh has converted custom normals to float3. */
                  if (type.is<float3>()) {
                    dst.typed<float3>()[out_c] = -dst.typed<float3>()[out_c];
                  }
                }
              });
            }
          }
        }
      });
  for (bke::GSpanAttributeWriter &writer : writers) {
    writer.finish();
  }
}

void set_material_from_map(const Span<int> out_to_in_map,
                           const Span<Array<short>> material_remaps,
                           const Span<const Mesh *> meshes,
                           const MeshOffsets &mesh_offsets,
                           const MutableSpan<int> dst)
{
  BLI_assert(material_remaps.size() > 0);
  Array<VArray<int>> material_varrays(meshes.size());
  for (const int i : meshes.index_range()) {
    bke::AttributeAccessor input_attrs = meshes[i]->attributes();
    material_varrays[i] = *input_attrs.lookup_or_default<int>(
        "material_index", bke::AttrDomain::Face, 0);
  }
  threading::parallel_for(out_to_in_map.index_range(), 8192, [&](const IndexRange range) {
    for (const int out_f : range) {
      const int in_f = out_to_in_map[out_f];
      const int mesh_id = mesh_id_for_face(in_f, mesh_offsets);
      const int in_f_local = in_f - mesh_offsets.face_start[mesh_id];
      const int orig = material_varrays[mesh_id][in_f_local];
      const Array<short> &map = material_remaps[mesh_id];
      dst[out_f] = (orig >= 0 && orig < map.size()) ? map[orig] : orig;
    }
  });
}

/**
 * Find the edges that are the result of intersecting one mesh with another,
 * and add their indices to \a r_intersecting_edges.
 */
static void get_intersecting_edges(Vector<int> *r_intersecting_edges,
                                   const Mesh *mesh,
                                   OutToInMaps &out_to_in,
                                   const MeshOffsets &mesh_offsets)
{
/* In a manifold mesh, every edge is adjacent to exactly two faces.
 * Find them, and when we have a pair, check to see if those faces came
 * from separate input meshes, and add the edge to r_intersecting_Edges if so. */
#  ifdef DEBUG_TIME
  timeit::ScopedTimer timer("get_intersecting_edges");
#  endif
  const OffsetIndices<int> faces = mesh->faces();
  const Span<int> corner_edges = mesh->corner_edges();
  const Span<int> face_map = out_to_in.ensure_face_map();
  Array<int> edge_first_face(mesh->edges_num, -1);
  for (int face_i : faces.index_range()) {
    for (const int edge_i : corner_edges.slice(faces[face_i])) {
      int face2_i = edge_first_face[edge_i];
      if (face2_i == -1) {
        edge_first_face[edge_i] = face_i;
      }
      else {
        int in_face_i = face_map[face_i];
        int in_face2_i = face_map[face2_i];
        int m1 = mesh_id_for_face(in_face_i, mesh_offsets);
        int m2 = mesh_id_for_face(in_face2_i, mesh_offsets);
        BLI_assert(m1 != -1 && m2 != -1);
        if (m1 != m2) {
          r_intersecting_edges->append(edge_i);
        }
      }
    }
  }
}

/**
 * Return true if \a mesh is a plane. If it is, fill in *r_normal to be
 * the plane's normal, and *r_origin_offset to be the vector that goes
 * from the origin to the plane in the normal direction.
 */
static bool is_plane(const Mesh *mesh,
                     const float4x4 &transform,
                     float3 *r_normal,
                     float *r_origin_offset)
{
  if (mesh->faces_num != 1 && mesh->verts_num != 4) {
    return false;
  }
  float3 vpos[4];
  const Span<float3> positions = mesh->vert_positions();
  const Span<int> f_corners = mesh->corner_verts().slice(mesh->faces()[0]);
  for (int i = 0; i < 4; i++) {
    mul_v3_m4v3(vpos[i], transform.ptr(), positions[f_corners[i]]);
  }
  float3 norm1 = math::normal_tri(vpos[0], vpos[1], vpos[2]);
  float3 norm2 = math::normal_tri(vpos[0], vpos[2], vpos[3]);
  if (math::almost_equal_relative(norm1, norm2, 1e-5f)) {
    *r_normal = norm1;
    *r_origin_offset = math::dot(norm1, vpos[0]);
    return true;
  }
  return false;
}

/**
 * Handle special case of one manifold mesh, which has been converted to
 * \a manifold 0, and one plane, which has normalized normal \a normal
 * and distance from origin \a origin_offset.
 * If there is an error, set *r_error appropriately.
 */
static MeshGL mesh_trim_manifold(Manifold &manifold0,
                                 float3 normal,
                                 float origin_offset,
                                 const MeshOffsets &mesh_offsets,
                                 BooleanError *r_error)
{
  Manifold man_result = manifold0.TrimByPlane(manifold::vec3(normal[0], normal[1], normal[2]),
                                              double(origin_offset));
  MeshGL meshgl = man_result.GetMeshGL();
  if (man_result.Status() != Manifold::Error::NoError) {
    if (man_result.Status() == Manifold::Error::ResultTooLarge) {
      *r_error = BooleanError::ResultTooBig;
    }
    else if (man_result.Status() == Manifold::Error::NotManifold) {
      *r_error = BooleanError::NonManifold;
    }
    else {
      *r_error = BooleanError::UnknownError;
    }
    return meshgl;
  }
  /* This meshgl_result has a non-standard (but non-zero) original ID for the
   * plane faces, and faceIDs that make no sense for them. Fix this.
   * But only do this if the result is not empty. */
  if (meshgl.vertProperties.size() > 0) {
    BLI_assert(meshgl.runOriginalID.size() == 2 && meshgl.runOriginalID[1] > 0);
    meshgl.runOriginalID[1] = 1;
    BLI_assert(meshgl.runIndex.size() == 3);
    int plane_face_start = meshgl.runIndex[1] / 3;
    int plane_face_end = meshgl.runIndex[2] / 3;
    for (int i = plane_face_start; i < plane_face_end; i++) {
      meshgl.faceID[i] = mesh_offsets.face_offsets[1][0];
    }
  }
  return meshgl;
}

/**
 * Convert the meshgl that is the result of the boolean back into a
 * Blender Mesh.
 * If \a r_intersecting_edges is not null, fill it with the edge indices
 * of edges that separate two different meshes of the input.
 */
static Mesh *meshgl_to_mesh(MeshGL &mgl,
                            const Mesh *joined_mesh,
                            Span<const Mesh *> meshes,
                            const Span<Array<short>> material_remaps,
                            const MeshOffsets &mesh_offsets,
                            Vector<int> *r_intersecting_edges)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "MESHGL_TO_MESH\n";
  }
#  ifdef DEBUG_TIME
  timeit::ScopedTimer timer("meshgl to mesh from joined_mesh");
#  endif
  BLI_assert(mgl.mergeFromVert.empty());

  if (mgl.vertProperties.empty() || mgl.triVerts.empty()) {
    Mesh *mesh = BKE_mesh_new_nomain(0, 0, 0, 0);
    BKE_mesh_copy_parameters_for_eval(mesh, joined_mesh);
    return mesh;
  }

  MeshAssembly ma = assemble_mesh_from_meshgl(mgl, mesh_offsets);
  const int verts_num = ma.output_verts_num;
  const int faces_num = ma.new_faces.size();

  /* Make a new Mesh, now that we know the number of vertices and faces. Corners will be counted
   * using the mesh's face offsets, and we will use Blender's parallelized function to calculate
   * edges later. */
  Mesh *mesh = bke::mesh_new_no_attributes(verts_num, 0, faces_num, 0);
  BKE_mesh_copy_parameters_for_eval(mesh, joined_mesh);

  /* First the face offsets store the size of each result face, then we accumulate them to form the
   * final offsets. */
  MutableSpan<int> face_offsets = mesh->face_offsets_for_write();
  threading::parallel_for(IndexRange(faces_num), 10'000, [&](const IndexRange range) {
    for (const int face : range) {
      face_offsets[face] = ma.new_faces[face].verts.size();
    }
  });
  const OffsetIndices<int> faces = offset_indices::accumulate_counts_to_offsets(face_offsets);
  mesh->corners_num = faces.total_size();

  bke::MutableAttributeAccessor output_attrs = mesh->attributes_for_write();

  /* Write corner vertex references. */
  {
#  ifdef DEBUG_TIME
    timeit::ScopedTimer timer_c("calculate faces");
#  endif
    output_attrs.add<int>(".corner_vert", bke::AttrDomain::Corner, bke::AttributeInitConstruct());
    MutableSpan<int> corner_verts = mesh->corner_verts_for_write();
    threading::parallel_for(IndexRange(faces_num), 10'000, [&](const IndexRange range) {
      for (const int face : range) {
        corner_verts.slice(faces[face]).copy_from(ma.new_faces[face].verts);
      }
    });
  }

  /* Set the vertex positions, using implicit sharing to avoid copying any data. */
  {
#  ifdef DEBUG_TIME
    timeit::ScopedTimer timer_c("set positions");
#  endif
    BLI_assert(!output_attrs.contains("position"));
    BLI_assert(mgl.numProp == 3);
    auto *sharing_info = new ImplicitSharedValue<std::vector<float>>(
        std::move(mgl.vertProperties));
    const bke::AttributeInitShared init(sharing_info->data.data(), *sharing_info);
    output_attrs.add<float3>("position", bke::AttrDomain::Point, init);
    sharing_info->remove_user_and_delete_if_last();
  }

  {
#  ifdef DEBUG_TIME
    timeit::ScopedTimer timer_e("calculating edges");
#  endif
    bke::mesh_calc_edges(*mesh, false, false);
  }

  BLI_assert(BKE_mesh_is_valid(mesh));

  OutToInMaps out_to_in(&ma, joined_mesh, mesh, &mesh_offsets);

  {
#  ifdef DEBUG_TIME
    timeit::ScopedTimer timer_a("copying and interpolating attributes");
#  endif

    /* Copy attributes from joined_mesh to elements they are mapped to
     * in the new mesh. For most attributes, if there is no input element
     * mapping to it, the attribute value is left at default.
     * But for corner attributes (most importantly, UV maps), missing
     * values are interpolated in their containing face.
     * We'll do corner interpolation in a separate pass so as to do
     * such attributes at once for a given face.
     */
    bke::AttributeAccessor join_attrs = joined_mesh->attributes();

    join_attrs.foreach_attribute([&](const bke::AttributeIter &iter) {
      if (ELEM(iter.name, "position", ".edge_verts", ".corner_vert", ".corner_edge")) {
        return;
      }
      Span<int> out_to_in_map;
      bool do_copy = true;
      bool do_material_remap = false;
      switch (iter.domain) {
        case bke::AttrDomain::Point: {
          out_to_in_map = out_to_in.ensure_vertex_map();
          break;
        }
        case bke::AttrDomain::Face: {
          out_to_in_map = out_to_in.ensure_face_map();
          /* If #material_remaps is non-empty, we need to use that map to set the
           * face "material_index" property instead of taking it from the joined mesh.
           * This should only happen if the user wants something other than the default
           * "transfer the materials" mode, which has already happened in the joined mesh.
           */
          do_material_remap = !material_remaps.is_empty() && iter.name == "material_index";
          break;
        }
        case bke::AttrDomain::Edge: {
          out_to_in_map = out_to_in.ensure_edge_map();
          break;
        }
        case bke::AttrDomain::Corner: {
          /* Handled separately below. */
          break;
        }
        default: {
          BLI_assert_unreachable();
          do_copy = false;
          break;
        }
      }
      if (do_copy) {
        if (dbg_level > 0) {
          std::cout << "copy_attribute_using_map, name = " << iter.name << "\n";
        }
        bke::GSpanAttributeWriter dst = output_attrs.lookup_or_add_for_write_only_span(
            iter.name, iter.domain, iter.data_type);
        if (do_material_remap) {
          set_material_from_map(
              out_to_in_map, material_remaps, meshes, mesh_offsets, dst.span.typed<int>());
        }
        else {
          copy_attribute_using_map(GVArraySpan(*iter.get()), out_to_in_map, dst.span);
        }
        dst.finish();
      }
    });

    interpolate_corner_attributes(output_attrs,
                                  join_attrs,
                                  mesh,
                                  joined_mesh,
                                  out_to_in.ensure_corner_map(),
                                  out_to_in.ensure_face_map());

    if (r_intersecting_edges != nullptr) {
      get_intersecting_edges(r_intersecting_edges, mesh, out_to_in, mesh_offsets);
    }
  }

  mesh->tag_loose_verts_none();
  mesh->tag_overlapping_none();

  BLI_assert(BKE_mesh_is_valid(mesh));

  return mesh;
}

bke::GeometrySet join_meshes_with_transforms(const Span<const Mesh *> meshes,
                                             const Span<float4x4> transforms)
{
#  ifdef DEBUG_TIME
  timeit::ScopedTimer jtimer(__func__);
#  endif
  bke::Instances instances;
  instances.resize(meshes.size());
  instances.transforms_for_write().copy_from(transforms);
  MutableSpan<int> handles = instances.reference_handles_for_write();

  Map<const Mesh *, int> handle_by_mesh;
  for (const int i : meshes.index_range()) {
    handles[i] = handle_by_mesh.lookup_or_add_cb(meshes[i], [&]() {
      bke::GeometrySet geometry = bke::GeometrySet::from_mesh(
          const_cast<Mesh *>(meshes[i]), bke::GeometryOwnershipType::ReadOnly);
      return instances.add_new_reference(std::move(geometry));
    });
  }
  return geometry::realize_instances(
             bke::GeometrySet::from_instances(&instances, bke::GeometryOwnershipType::Editable),
             geometry::RealizeInstancesOptions())
      .geometry;
}

Mesh *mesh_boolean_manifold(Span<const Mesh *> meshes,
                            const Span<float4x4> transforms,
                            const Span<Array<short>> material_remaps,
                            const BooleanOpParameters op_params,
                            Vector<int> *r_intersecting_edges,
                            BooleanError *r_error)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "\nMESH_BOOLEAN_MANIFOLD with " << meshes.size() << " args\n";
  }
  *r_error = BooleanError::NoError;
  try {
#  ifdef DEBUG_TIME
    timeit::ScopedTimer timer("MANIFOLD BOOLEAN");
#  endif

    const int meshes_num = meshes.size();

    bke::GeometrySet joined_meshes_set = join_meshes_with_transforms(meshes, transforms);
    const Mesh *joined_mesh = joined_meshes_set.get_mesh();
    if (joined_mesh == nullptr) {
      return nullptr;
    }

    const MeshOffsets mesh_offsets(meshes);
    std::vector<Manifold> manifolds(meshes_num);
    get_manifolds(manifolds, meshes, transforms, mesh_offsets);

    MeshGL meshgl_result;
    Operation op = op_params.boolean_mode;
    if (std::any_of(manifolds.begin(), manifolds.end(), [](const Manifold &m) {
          return m.Status() != Manifold::Error::NoError;
        }))
    {
      /* Check special case of subtracting a plane, which Manifold can handle. */
      float3 normal;
      float origin_offset;
      if (meshes_num == 2 && op == Operation::Difference &&
          manifolds[0].Status() == Manifold::Error::NoError &&
          is_plane(meshes[1], transforms[1], &normal, &origin_offset))
      {
#  ifdef DEBUG_TIME
        timeit::ScopedTimer timer_trim("DOING BOOLEAN SLICE, GETTING MESH_GL RESULT");
#  endif
        meshgl_result = mesh_trim_manifold(
            manifolds[0], normal, origin_offset, mesh_offsets, r_error);
        if (*r_error != BooleanError::NoError) {
          return nullptr;
        }
      }
      else {
        if (std::any_of(manifolds.begin(), manifolds.end(), [](const Manifold &m) {
              return m.Status() == Manifold::Error::NotManifold;
            }))
        {
          *r_error = BooleanError::NonManifold;
        }
        else {
          *r_error = BooleanError::UnknownError;
        }
        return nullptr;
      }
    }
    else {
      manifold::OpType mop = op == Operation::Intersect ?
                                 manifold::OpType::Intersect :
                                 (op == Operation::Union ? manifold::OpType::Add :
                                                           manifold::OpType::Subtract);
#  ifdef DEBUG_TIME
      timeit::ScopedTimer timer_bool("DOING BOOLEAN, GETTING MESH_GL RESULT");
#  endif
      Manifold man_result = Manifold::BatchBoolean(manifolds, mop);
      meshgl_result = man_result.GetMeshGL();
      /* Have to wait until after converting to MeshGL to check status. */
      if (man_result.Status() != Manifold::Error::NoError) {
        if (man_result.Status() == Manifold::Error::ResultTooLarge) {
          *r_error = BooleanError::ResultTooBig;
        }
        else {
          *r_error = BooleanError::UnknownError;
        }
        if (dbg_level > 0) {
          std::cout << "manifold boolean returned with error status\n";
        }
        return nullptr;
      }
    }
    if (dbg_level > 0) {
      std::cout << "boolean result has " << meshgl_result.NumTri() << " tris\n";
      dump_meshgl(meshgl_result, "boolean result meshgl");
    }
    Mesh *mesh_result;
    {
#  ifdef DEBUG_TIME
      timeit::ScopedTimer timer_out("MESHGL RESULT TO MESH");
#  endif
      mesh_result = meshgl_to_mesh(
          meshgl_result, joined_mesh, meshes, material_remaps, mesh_offsets, r_intersecting_edges);
    }
    return mesh_result;
  }
  catch (const std::exception &e) {
    std::cout << "mesh_boolean_manifold: exception: " << e.what() << "\n";
  }
  catch (...) {
    std::cout << "mesh_boolean_manifold: unknown exception\n";
  }
  *r_error = BooleanError::UnknownError;
  return nullptr;
}

}  // namespace blender::geometry::boolean
#endif  // WITH_MANIFOLD
