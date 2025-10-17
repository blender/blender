/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_array_utils.hh"
#include "BLI_math_geom.h"

#include "BKE_bake_data_block_id.hh"
#include "BKE_bvhutils.hh"
#include "BKE_customdata.hh"
#include "BKE_editmesh_cache.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_shrinkwrap.hh"
#include "BKE_subdiv_ccg.hh"

using blender::float3;
using blender::MutableSpan;
using blender::Span;

/* -------------------------------------------------------------------- */
/** \name Mesh Runtime Struct Utils
 * \{ */

namespace blender::bke {

static void free_mesh_eval(MeshRuntime &mesh_runtime)
{
  if (mesh_runtime.mesh_eval != nullptr) {
    BKE_id_free(nullptr, mesh_runtime.mesh_eval);
    mesh_runtime.mesh_eval = nullptr;
  }
}

static void free_batch_cache(MeshRuntime &mesh_runtime)
{
  if (mesh_runtime.batch_cache) {
    BKE_mesh_batch_cache_free(mesh_runtime.batch_cache);
    mesh_runtime.batch_cache = nullptr;
  }
}

static void free_bvh_caches(MeshRuntime &mesh_runtime)
{
  mesh_runtime.bvh_cache_verts.tag_dirty();
  mesh_runtime.bvh_cache_edges.tag_dirty();
  mesh_runtime.bvh_cache_faces.tag_dirty();
  mesh_runtime.bvh_cache_corner_tris.tag_dirty();
  mesh_runtime.bvh_cache_corner_tris_no_hidden.tag_dirty();
  mesh_runtime.bvh_cache_loose_verts.tag_dirty();
  mesh_runtime.bvh_cache_loose_verts_no_hidden.tag_dirty();
  mesh_runtime.bvh_cache_loose_edges.tag_dirty();
  mesh_runtime.bvh_cache_loose_edges_no_hidden.tag_dirty();
}

MeshRuntime::MeshRuntime() = default;

MeshRuntime::~MeshRuntime()
{
  free_mesh_eval(*this);
  free_batch_cache(*this);
}

static int reset_bits_and_count(MutableBitSpan bits, const Span<int> indices_to_reset)
{
  int count = bits.size();
  for (const int i : indices_to_reset) {
    if (bits[i]) {
      bits[i].reset();
      count--;
    }
  }
  return count;
}

static void bit_vector_with_reset_bits_or_empty(const Span<int> indices_to_reset,
                                                const int indexed_elems_num,
                                                BitVector<> &r_bits,
                                                int &r_count)
{
  r_bits.resize(0);
  r_bits.resize(indexed_elems_num, true);
  r_count = reset_bits_and_count(r_bits, indices_to_reset);
  if (r_count == 0) {
    r_bits.clear_and_shrink();
  }
}

/**
 * If there are no loose edges and no loose vertices, all vertices are used by faces.
 */
static void try_tag_verts_no_face_none(const Mesh &mesh)
{
  if (!mesh.runtime->loose_edges_cache.is_cached() || mesh.loose_edges().count > 0) {
    return;
  }
  if (!mesh.runtime->loose_verts_cache.is_cached() || mesh.loose_verts().count > 0) {
    return;
  }
  mesh.runtime->verts_no_face_cache.ensure([&](LooseVertCache &r_data) {
    r_data.is_loose_bits.clear_and_shrink();
    r_data.count = 0;
  });
}

}  // namespace blender::bke

blender::Span<int> Mesh::corner_to_face_map() const
{
  using namespace blender;
  this->runtime->corner_to_face_map_cache.ensure([&](Array<int> &r_data) {
    const OffsetIndices faces = this->faces();
    r_data = bke::mesh::build_corner_to_face_map(faces);
  });
  return this->runtime->corner_to_face_map_cache.data();
}

blender::OffsetIndices<int> Mesh::vert_to_face_map_offsets() const
{
  using namespace blender;
  this->runtime->vert_to_face_offset_cache.ensure([&](Array<int> &r_data) {
    r_data = Array<int>(this->verts_num + 1, 0);
    offset_indices::build_reverse_offsets(this->corner_verts(), r_data);
  });
  return OffsetIndices<int>(this->runtime->vert_to_face_offset_cache.data());
}

blender::GroupedSpan<int> Mesh::vert_to_face_map() const
{
  using namespace blender;
  const OffsetIndices offsets = this->vert_to_face_map_offsets();
  this->runtime->vert_to_face_map_cache.ensure([&](Array<int> &r_data) {
    r_data.reinitialize(this->corners_num);
    if (this->runtime->vert_to_corner_map_cache.is_cached() &&
        this->runtime->corner_to_face_map_cache.is_cached())
    {
      /* The vertex to face cache can be built from the vertex to face corner
       * and face corner to face maps if they are both already cached. */
      array_utils::gather(this->runtime->corner_to_face_map_cache.data().as_span(),
                          this->runtime->vert_to_corner_map_cache.data().as_span(),
                          r_data.as_mutable_span());
    }
    else {
      bke::mesh::build_vert_to_face_indices(this->faces(), this->corner_verts(), offsets, r_data);
    }
  });
  return {offsets, this->runtime->vert_to_face_map_cache.data()};
}

blender::GroupedSpan<int> Mesh::vert_to_corner_map() const
{
  using namespace blender;
  const OffsetIndices offsets = this->vert_to_face_map_offsets();
  this->runtime->vert_to_corner_map_cache.ensure([&](Array<int> &r_data) {
    r_data = bke::mesh::build_vert_to_corner_indices(this->corner_verts(), offsets);
  });
  return {offsets, this->runtime->vert_to_corner_map_cache.data()};
}

const blender::bke::LooseVertCache &Mesh::loose_verts() const
{
  using namespace blender::bke;
  this->runtime->loose_verts_cache.ensure([&](LooseVertCache &r_data) {
    const Span<int> verts = this->edges().cast<int>();
    bit_vector_with_reset_bits_or_empty(
        verts, this->verts_num, r_data.is_loose_bits, r_data.count);
  });
  return this->runtime->loose_verts_cache.data();
}

const blender::bke::LooseVertCache &Mesh::verts_no_face() const
{
  using namespace blender::bke;
  this->runtime->verts_no_face_cache.ensure([&](LooseVertCache &r_data) {
    const Span<int> verts = this->corner_verts();
    bit_vector_with_reset_bits_or_empty(
        verts, this->verts_num, r_data.is_loose_bits, r_data.count);
  });
  return this->runtime->verts_no_face_cache.data();
}

bool Mesh::no_overlapping_topology() const
{
  return this->flag & ME_NO_OVERLAPPING_TOPOLOGY;
}

const blender::bke::LooseEdgeCache &Mesh::loose_edges() const
{
  using namespace blender::bke;
  this->runtime->loose_edges_cache.ensure([&](LooseEdgeCache &r_data) {
    const Span<int> edges = this->corner_edges();
    bit_vector_with_reset_bits_or_empty(
        edges, this->edges_num, r_data.is_loose_bits, r_data.count);
  });
  return this->runtime->loose_edges_cache.data();
}

void Mesh::tag_loose_verts_none() const
{
  using namespace blender::bke;
  this->runtime->loose_verts_cache.ensure([&](LooseVertCache &r_data) {
    r_data.is_loose_bits.clear_and_shrink();
    r_data.count = 0;
  });
  try_tag_verts_no_face_none(*this);
}

void Mesh::tag_loose_edges_none() const
{
  using namespace blender::bke;
  this->runtime->loose_edges_cache.ensure([&](LooseEdgeCache &r_data) {
    r_data.is_loose_bits.clear_and_shrink();
    r_data.count = 0;
  });
  try_tag_verts_no_face_none(*this);
}

void Mesh::tag_overlapping_none()
{
  using namespace blender::bke;
  this->flag |= ME_NO_OVERLAPPING_TOPOLOGY;
}

namespace blender::bke {

void TrianglesCache::freeze()
{
  this->frozen = true;
  this->dirty_while_frozen = false;
}

void TrianglesCache::unfreeze()
{
  this->frozen = false;
  if (this->dirty_while_frozen) {
    this->data.tag_dirty();
  }
  this->dirty_while_frozen = false;
}

void TrianglesCache::tag_dirty()
{
  if (this->frozen) {
    this->dirty_while_frozen = true;
  }
  else {
    this->data.tag_dirty();
  }
}

}  // namespace blender::bke

blender::Span<blender::int3> Mesh::corner_tris() const
{
  this->runtime->corner_tris_cache.data.ensure([&](blender::Array<blender::int3> &r_data) {
    const Span<float3> positions = this->vert_positions();
    const blender::OffsetIndices faces = this->faces();
    const Span<int> corner_verts = this->corner_verts();

    r_data.reinitialize(poly_to_tri_count(faces.size(), corner_verts.size()));

    if (BKE_mesh_face_normals_are_dirty(this)) {
      blender::bke::mesh::corner_tris_calc(positions, faces, corner_verts, r_data);
    }
    else {
      blender::bke::mesh::corner_tris_calc_with_normals(
          positions, faces, corner_verts, this->face_normals(), r_data);
    }
  });

  return this->runtime->corner_tris_cache.data.data();
}

blender::Span<int> Mesh::corner_tri_faces() const
{
  using namespace blender;
  this->runtime->corner_tri_faces_cache.ensure([&](blender::Array<int> &r_data) {
    const OffsetIndices faces = this->faces();
    r_data.reinitialize(poly_to_tri_count(faces.size(), this->corners_num));
    bke::mesh::corner_tris_calc_face_indices(faces, r_data);
  });
  return this->runtime->corner_tri_faces_cache.data();
}

int BKE_mesh_runtime_corner_tris_len(const Mesh *mesh)
{
  /* Allow returning the size without calculating the cache. */
  return poly_to_tri_count(mesh->faces_num, mesh->corners_num);
}

void BKE_mesh_runtime_ensure_edit_data(Mesh *mesh)
{
  if (!mesh->runtime->edit_data) {
    mesh->runtime->edit_data = std::make_unique<blender::bke::EditMeshData>();
  }
}

void BKE_mesh_runtime_clear_cache(Mesh *mesh)
{
  using namespace blender::bke;
  free_mesh_eval(*mesh->runtime);
  free_batch_cache(*mesh->runtime);
  mesh->runtime->edit_data.reset();
  BKE_mesh_runtime_clear_geometry(mesh);
}

void BKE_mesh_runtime_clear_geometry(Mesh *mesh)
{
  /* Tagging shared caches dirty will free the allocated data if there is only one user. */
  free_bvh_caches(*mesh->runtime);
  mesh->runtime->subdiv_ccg.reset();
  mesh->runtime->bounds_cache.tag_dirty();
  mesh->runtime->vert_to_face_offset_cache.tag_dirty();
  mesh->runtime->vert_to_face_map_cache.tag_dirty();
  mesh->runtime->vert_to_corner_map_cache.tag_dirty();
  mesh->runtime->corner_to_face_map_cache.tag_dirty();
  mesh->runtime->vert_normals_cache.tag_dirty();
  mesh->runtime->vert_normals_true_cache.tag_dirty();
  mesh->runtime->face_normals_cache.tag_dirty();
  mesh->runtime->face_normals_true_cache.tag_dirty();
  mesh->runtime->corner_normals_cache.tag_dirty();
  mesh->runtime->loose_edges_cache.tag_dirty();
  mesh->runtime->loose_verts_cache.tag_dirty();
  mesh->runtime->verts_no_face_cache.tag_dirty();
  mesh->runtime->corner_tris_cache.data.tag_dirty();
  mesh->runtime->corner_tri_faces_cache.tag_dirty();
  mesh->runtime->shrinkwrap_boundary_cache.tag_dirty();
  mesh->runtime->max_material_index.tag_dirty();
  mesh->runtime->subsurf_face_dot_tags.clear_and_shrink();
  mesh->runtime->subsurf_optimal_display_edges.clear_and_shrink();
  mesh->runtime->spatial_groups.reset();
  mesh->flag &= ~(ME_NO_OVERLAPPING_TOPOLOGY | ME_FLAG_UV_SELECT_SYNC_VALID);
}

void Mesh::tag_edges_split()
{
  /* Triangulation didn't change because vertex positions and loop vertex indices didn't change. */
  free_bvh_caches(*this->runtime);
  this->runtime->vert_normals_cache.tag_dirty();
  this->runtime->corner_normals_cache.tag_dirty();
  this->runtime->subdiv_ccg.reset();
  this->runtime->vert_to_face_offset_cache.tag_dirty();
  this->runtime->vert_to_face_map_cache.tag_dirty();
  this->runtime->vert_to_corner_map_cache.tag_dirty();
  if (this->runtime->loose_edges_cache.is_cached() &&
      this->runtime->loose_edges_cache.data().count != 0)
  {
    this->runtime->loose_edges_cache.tag_dirty();
  }
  if (this->runtime->loose_verts_cache.is_cached() &&
      this->runtime->loose_verts_cache.data().count != 0)
  {
    this->runtime->loose_verts_cache.tag_dirty();
  }
  if (this->runtime->verts_no_face_cache.is_cached() &&
      this->runtime->verts_no_face_cache.data().count != 0)
  {
    this->runtime->verts_no_face_cache.tag_dirty();
  }
  this->runtime->subsurf_face_dot_tags.clear_and_shrink();
  this->runtime->subsurf_optimal_display_edges.clear_and_shrink();
  this->runtime->shrinkwrap_boundary_cache.tag_dirty();
}

void Mesh::tag_sharpness_changed()
{
  this->runtime->vert_normals_cache.tag_dirty();
  this->runtime->face_normals_cache.tag_dirty();
  this->runtime->corner_normals_cache.tag_dirty();
}

void Mesh::tag_custom_normals_changed()
{
  this->runtime->vert_normals_cache.tag_dirty();
  this->runtime->face_normals_cache.tag_dirty();
  this->runtime->corner_normals_cache.tag_dirty();
}

void Mesh::tag_face_winding_changed()
{
  this->runtime->vert_normals_cache.tag_dirty();
  this->runtime->face_normals_cache.tag_dirty();
  this->runtime->vert_normals_true_cache.tag_dirty();
  this->runtime->face_normals_true_cache.tag_dirty();
  this->runtime->corner_normals_cache.tag_dirty();
  this->runtime->vert_to_corner_map_cache.tag_dirty();
  this->runtime->shrinkwrap_boundary_cache.tag_dirty();
}

void Mesh::tag_positions_changed()
{
  this->runtime->vert_normals_cache.tag_dirty();
  this->runtime->face_normals_cache.tag_dirty();
  this->runtime->vert_normals_true_cache.tag_dirty();
  this->runtime->face_normals_true_cache.tag_dirty();
  this->runtime->corner_normals_cache.tag_dirty();
  this->runtime->shrinkwrap_boundary_cache.tag_dirty();
  this->tag_positions_changed_no_normals();
}

void Mesh::tag_positions_changed_no_normals()
{
  free_bvh_caches(*this->runtime);
  this->runtime->corner_tris_cache.tag_dirty();
  this->runtime->bounds_cache.tag_dirty();
  this->runtime->shrinkwrap_boundary_cache.tag_dirty();
}

void Mesh::tag_positions_changed_uniformly()
{
  /* The normals and triangulation didn't change, since all verts moved by the same amount. */
  free_bvh_caches(*this->runtime);
  this->runtime->bounds_cache.tag_dirty();
}

void Mesh::tag_topology_changed()
{
  BKE_mesh_runtime_clear_geometry(this);
}

void Mesh::tag_visibility_changed()
{
  this->runtime->bvh_cache_corner_tris_no_hidden.tag_dirty();
  this->runtime->bvh_cache_loose_verts_no_hidden.tag_dirty();
  this->runtime->bvh_cache_loose_edges_no_hidden.tag_dirty();
}

void Mesh::tag_material_index_changed()
{
  this->runtime->max_material_index.tag_dirty();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Batch Cache Callbacks
 * \{ */

/* Draw Engine */

void (*BKE_mesh_batch_cache_dirty_tag_cb)(Mesh *mesh, eMeshBatchDirtyMode mode) = nullptr;
void (*BKE_mesh_batch_cache_free_cb)(void *batch_cache) = nullptr;

void BKE_mesh_batch_cache_dirty_tag(Mesh *mesh, eMeshBatchDirtyMode mode)
{
  if (mesh->runtime->batch_cache) {
    BKE_mesh_batch_cache_dirty_tag_cb(mesh, mode);
  }

  /* Also tag batch cache for subdivided mesh, if it exists this will be
   * the mesh that is actually being drawn. */
  Mesh *mesh_eval = mesh->runtime->mesh_eval;
  if (mesh_eval && mesh_eval->runtime->batch_cache) {
    BKE_mesh_batch_cache_dirty_tag_cb(mesh_eval, mode);
  }
}
void BKE_mesh_batch_cache_free(void *batch_cache)
{
  BKE_mesh_batch_cache_free_cb(batch_cache);
}

/** \} */
