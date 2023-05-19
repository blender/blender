/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

/** \file
 * \ingroup bke
 */

#include "atomic_ops.h"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math_geom.h"
#include "BLI_task.hh"
#include "BLI_timeit.hh"

#include "BKE_bvhutils.h"
#include "BKE_editmesh_cache.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.h"
#include "BKE_shrinkwrap.h"
#include "BKE_subdiv_ccg.h"

using blender::float3;
using blender::MutableSpan;
using blender::Span;

/* -------------------------------------------------------------------- */
/** \name Mesh Runtime Struct Utils
 * \{ */

namespace blender::bke {

static void edit_data_reset(EditMeshData &edit_data)
{
  MEM_SAFE_FREE(edit_data.polyCos);
  MEM_SAFE_FREE(edit_data.polyNos);
  MEM_SAFE_FREE(edit_data.vertexCos);
  MEM_SAFE_FREE(edit_data.vertexNos);
}

static void free_edit_data(MeshRuntime &mesh_runtime)
{
  if (mesh_runtime.edit_data) {
    edit_data_reset(*mesh_runtime.edit_data);
    MEM_freeN(mesh_runtime.edit_data);
    mesh_runtime.edit_data = nullptr;
  }
}

static void free_mesh_eval(MeshRuntime &mesh_runtime)
{
  if (mesh_runtime.mesh_eval != nullptr) {
    mesh_runtime.mesh_eval->edit_mesh = nullptr;
    BKE_id_free(nullptr, mesh_runtime.mesh_eval);
    mesh_runtime.mesh_eval = nullptr;
  }
}

static void free_subdiv_ccg(MeshRuntime &mesh_runtime)
{
  /* TODO(sergey): Does this really belong here? */
  if (mesh_runtime.subdiv_ccg != nullptr) {
    BKE_subdiv_ccg_destroy(mesh_runtime.subdiv_ccg);
    mesh_runtime.subdiv_ccg = nullptr;
  }
}

static void free_bvh_cache(MeshRuntime &mesh_runtime)
{
  if (mesh_runtime.bvh_cache) {
    bvhcache_free(mesh_runtime.bvh_cache);
    mesh_runtime.bvh_cache = nullptr;
  }
}

static void reset_normals(MeshRuntime &mesh_runtime)
{
  mesh_runtime.vert_normals.clear_and_shrink();
  mesh_runtime.poly_normals.clear_and_shrink();
  mesh_runtime.vert_normals_dirty = true;
  mesh_runtime.poly_normals_dirty = true;
}

static void free_batch_cache(MeshRuntime &mesh_runtime)
{
  if (mesh_runtime.batch_cache) {
    BKE_mesh_batch_cache_free(mesh_runtime.batch_cache);
    mesh_runtime.batch_cache = nullptr;
  }
}

MeshRuntime::~MeshRuntime()
{
  free_mesh_eval(*this);
  free_subdiv_ccg(*this);
  free_bvh_cache(*this);
  free_edit_data(*this);
  free_batch_cache(*this);
  if (this->shrinkwrap_data) {
    BKE_shrinkwrap_boundary_data_free(this->shrinkwrap_data);
  }
}

static int reset_bits_and_count(MutableBitSpan bits, const Span<int> indices_to_reset)
{
  int count = bits.size();
  for (const int vert : indices_to_reset) {
    if (bits[vert]) {
      bits[vert].reset();
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

const blender::bke::LooseVertCache &Mesh::loose_verts() const
{
  using namespace blender::bke;
  this->runtime->loose_verts_cache.ensure([&](LooseVertCache &r_data) {
    const Span<int> verts = this->edges().cast<int>();
    bit_vector_with_reset_bits_or_empty(verts, this->totvert, r_data.is_loose_bits, r_data.count);
  });
  return this->runtime->loose_verts_cache.data();
}

const blender::bke::LooseVertCache &Mesh::verts_no_face() const
{
  using namespace blender::bke;
  this->runtime->verts_no_face_cache.ensure([&](LooseVertCache &r_data) {
    const Span<int> verts = this->corner_verts();
    bit_vector_with_reset_bits_or_empty(verts, this->totvert, r_data.is_loose_bits, r_data.count);
  });
  return this->runtime->verts_no_face_cache.data();
}

const blender::bke::LooseEdgeCache &Mesh::loose_edges() const
{
  using namespace blender::bke;
  this->runtime->loose_edges_cache.ensure([&](LooseEdgeCache &r_data) {
    const Span<int> edges = this->corner_edges();
    bit_vector_with_reset_bits_or_empty(edges, this->totedge, r_data.is_loose_bits, r_data.count);
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

void Mesh::loose_edges_tag_none() const
{
  using namespace blender::bke;
  this->runtime->loose_edges_cache.ensure([&](LooseEdgeCache &r_data) {
    r_data.is_loose_bits.clear_and_shrink();
    r_data.count = 0;
  });
  try_tag_verts_no_face_none(*this);
}

blender::Span<MLoopTri> Mesh::looptris() const
{
  this->runtime->looptris_cache.ensure([&](blender::Array<MLoopTri> &r_data) {
    const Span<float3> positions = this->vert_positions();
    const blender::OffsetIndices polys = this->polys();
    const Span<int> corner_verts = this->corner_verts();

    r_data.reinitialize(poly_to_tri_count(polys.size(), corner_verts.size()));

    if (BKE_mesh_poly_normals_are_dirty(this)) {
      blender::bke::mesh::looptris_calc(positions, polys, corner_verts, r_data);
    }
    else {
      blender::bke::mesh::looptris_calc_with_normals(
          positions, polys, corner_verts, this->poly_normals(), r_data);
    }
  });

  return this->runtime->looptris_cache.data();
}

blender::Span<int> Mesh::looptri_polys() const
{
  using namespace blender;
  this->runtime->looptri_polys_cache.ensure([&](blender::Array<int> &r_data) {
    const OffsetIndices polys = this->polys();
    r_data.reinitialize(poly_to_tri_count(polys.size(), this->totloop));
    bke::mesh::looptris_calc_poly_indices(polys, r_data);
  });
  return this->runtime->looptri_polys_cache.data();
}

int BKE_mesh_runtime_looptri_len(const Mesh *mesh)
{
  /* Allow returning the size without calculating the cache. */
  return poly_to_tri_count(mesh->totpoly, mesh->totloop);
}

const MLoopTri *BKE_mesh_runtime_looptri_ensure(const Mesh *mesh)
{
  return mesh->looptris().data();
}

const int *BKE_mesh_runtime_looptri_polys_ensure(const Mesh *mesh)
{
  return mesh->looptri_polys().data();
}

void BKE_mesh_runtime_verttri_from_looptri(MVertTri *r_verttri,
                                           const int *corner_verts,
                                           const MLoopTri *looptri,
                                           int looptri_num)
{
  for (int i = 0; i < looptri_num; i++) {
    r_verttri[i].tri[0] = corner_verts[looptri[i].tri[0]];
    r_verttri[i].tri[1] = corner_verts[looptri[i].tri[1]];
    r_verttri[i].tri[2] = corner_verts[looptri[i].tri[2]];
  }
}

bool BKE_mesh_runtime_ensure_edit_data(struct Mesh *mesh)
{
  if (mesh->runtime->edit_data != nullptr) {
    return false;
  }
  mesh->runtime->edit_data = MEM_cnew<EditMeshData>(__func__);
  return true;
}

void BKE_mesh_runtime_reset_edit_data(Mesh *mesh)
{
  using namespace blender::bke;
  if (EditMeshData *edit_data = mesh->runtime->edit_data) {
    edit_data_reset(*edit_data);
  }
}

void BKE_mesh_runtime_clear_cache(Mesh *mesh)
{
  using namespace blender::bke;
  free_mesh_eval(*mesh->runtime);
  free_batch_cache(*mesh->runtime);
  free_edit_data(*mesh->runtime);
  BKE_mesh_runtime_clear_geometry(mesh);
}

void BKE_mesh_runtime_clear_geometry(Mesh *mesh)
{
  /* Tagging shared caches dirty will free the allocated data if there is only one user. */
  free_bvh_cache(*mesh->runtime);
  reset_normals(*mesh->runtime);
  free_subdiv_ccg(*mesh->runtime);
  mesh->runtime->bounds_cache.tag_dirty();
  mesh->runtime->loose_edges_cache.tag_dirty();
  mesh->runtime->loose_verts_cache.tag_dirty();
  mesh->runtime->verts_no_face_cache.tag_dirty();
  mesh->runtime->looptris_cache.tag_dirty();
  mesh->runtime->looptri_polys_cache.tag_dirty();
  mesh->runtime->subsurf_face_dot_tags.clear_and_shrink();
  mesh->runtime->subsurf_optimal_display_edges.clear_and_shrink();
  if (mesh->runtime->shrinkwrap_data) {
    BKE_shrinkwrap_boundary_data_free(mesh->runtime->shrinkwrap_data);
    mesh->runtime->shrinkwrap_data = nullptr;
  }
}

void BKE_mesh_tag_edges_split(struct Mesh *mesh)
{
  /* Triangulation didn't change because vertex positions and loop vertex indices didn't change.
   * Face normals didn't change either, but tag those anyway, since there is no API function to
   * only tag vertex normals dirty. */
  free_bvh_cache(*mesh->runtime);
  reset_normals(*mesh->runtime);
  free_subdiv_ccg(*mesh->runtime);
  if (mesh->runtime->loose_edges_cache.is_cached() &&
      mesh->runtime->loose_edges_cache.data().count != 0)
  {
  mesh->runtime->loose_edges_cache.tag_dirty();
  }
  if (mesh->runtime->loose_verts_cache.is_cached() &&
      mesh->runtime->loose_verts_cache.data().count != 0)
  {
  mesh->runtime->loose_verts_cache.tag_dirty();
  }
  if (mesh->runtime->verts_no_face_cache.is_cached() &&
      mesh->runtime->verts_no_face_cache.data().count != 0)
  {
  mesh->runtime->verts_no_face_cache.tag_dirty();
  }
  mesh->runtime->subsurf_face_dot_tags.clear_and_shrink();
  mesh->runtime->subsurf_optimal_display_edges.clear_and_shrink();
  if (mesh->runtime->shrinkwrap_data) {
    BKE_shrinkwrap_boundary_data_free(mesh->runtime->shrinkwrap_data);
    mesh->runtime->shrinkwrap_data = nullptr;
  }
}

void BKE_mesh_tag_face_winding_changed(Mesh *mesh)
{
  mesh->runtime->vert_normals_dirty = true;
  mesh->runtime->poly_normals_dirty = true;
}

void BKE_mesh_tag_positions_changed(Mesh *mesh)
{
  mesh->runtime->vert_normals_dirty = true;
  mesh->runtime->poly_normals_dirty = true;
  free_bvh_cache(*mesh->runtime);
  mesh->runtime->looptris_cache.tag_dirty();
  mesh->runtime->bounds_cache.tag_dirty();
}

void BKE_mesh_tag_positions_changed_uniformly(Mesh *mesh)
{
  /* The normals and triangulation didn't change, since all verts moved by the same amount. */
  free_bvh_cache(*mesh->runtime);
  mesh->runtime->bounds_cache.tag_dirty();
}

void BKE_mesh_tag_topology_changed(struct Mesh *mesh)
{
  BKE_mesh_runtime_clear_geometry(mesh);
}

bool BKE_mesh_is_deformed_only(const Mesh *mesh)
{
  return mesh->runtime->deformed_only;
}

eMeshWrapperType BKE_mesh_wrapper_type(const struct Mesh *mesh)
{
  return mesh->runtime->wrapper_type;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Batch Cache Callbacks
 * \{ */

/* Draw Engine */

void (*BKE_mesh_batch_cache_dirty_tag_cb)(Mesh *me, eMeshBatchDirtyMode mode) = nullptr;
void (*BKE_mesh_batch_cache_free_cb)(void *batch_cache) = nullptr;

void BKE_mesh_batch_cache_dirty_tag(Mesh *me, eMeshBatchDirtyMode mode)
{
  if (me->runtime->batch_cache) {
    BKE_mesh_batch_cache_dirty_tag_cb(me, mode);
  }
}
void BKE_mesh_batch_cache_free(void *batch_cache)
{
  BKE_mesh_batch_cache_free_cb(batch_cache);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Runtime Validation
 * \{ */

#ifndef NDEBUG

bool BKE_mesh_runtime_is_valid(Mesh *me_eval)
{
  const bool do_verbose = true;
  const bool do_fixes = false;

  bool is_valid = true;
  bool changed = true;

  if (do_verbose) {
    printf("MESH: %s\n", me_eval->id.name + 2);
  }

  MutableSpan<float3> positions = me_eval->vert_positions_for_write();
  MutableSpan<blender::int2> edges = me_eval->edges_for_write();
  MutableSpan<int> poly_offsets = me_eval->poly_offsets_for_write();
  MutableSpan<int> corner_verts = me_eval->corner_verts_for_write();
  MutableSpan<int> corner_edges = me_eval->corner_edges_for_write();

  is_valid &= BKE_mesh_validate_all_customdata(
      &me_eval->vdata,
      me_eval->totvert,
      &me_eval->edata,
      me_eval->totedge,
      &me_eval->ldata,
      me_eval->totloop,
      &me_eval->pdata,
      me_eval->totpoly,
      false, /* setting mask here isn't useful, gives false positives */
      do_verbose,
      do_fixes,
      &changed);

  is_valid &= BKE_mesh_validate_arrays(me_eval,
                                       reinterpret_cast<float(*)[3]>(positions.data()),
                                       positions.size(),
                                       edges.data(),
                                       edges.size(),
                                       static_cast<MFace *>(CustomData_get_layer_for_write(
                                           &me_eval->fdata, CD_MFACE, me_eval->totface)),
                                       me_eval->totface,
                                       corner_verts.data(),
                                       corner_edges.data(),
                                       corner_verts.size(),
                                       poly_offsets.data(),
                                       me_eval->totpoly,
                                       me_eval->deform_verts_for_write().data(),
                                       do_verbose,
                                       do_fixes,
                                       &changed);

  BLI_assert(changed == false);

  return is_valid;
}

#endif /* NDEBUG */

/** \} */
