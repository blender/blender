/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

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

#include "BKE_bvhutils.h"
#include "BKE_editmesh_cache.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_shrinkwrap.h"
#include "BKE_subdiv_ccg.h"

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

static void free_normals(MeshRuntime &mesh_runtime)
{
  MEM_SAFE_FREE(mesh_runtime.vert_normals);
  MEM_SAFE_FREE(mesh_runtime.poly_normals);
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
  free_normals(*this);
  if (this->shrinkwrap_data) {
    BKE_shrinkwrap_boundary_data_free(this->shrinkwrap_data);
  }
  MEM_SAFE_FREE(this->subsurf_face_dot_tags);
  MEM_SAFE_FREE(this->looptris.array);
}

}  // namespace blender::bke

blender::Span<MLoopTri> Mesh::looptris() const
{
  const MLoopTri *looptris = BKE_mesh_runtime_looptri_ensure(this);
  const int num_looptris = BKE_mesh_runtime_looptri_len(this);
  return {looptris, num_looptris};
}

/**
 * Ensure the array is large enough
 *
 * \note This function must always be thread-protected by caller.
 * It should only be used by internal code.
 */
static void mesh_ensure_looptri_data(Mesh *mesh)
{
  /* This is a ported copy of `DM_ensure_looptri_data(dm)`. */
  const uint totpoly = mesh->totpoly;
  const int looptris_len = poly_to_tri_count(totpoly, mesh->totloop);

  BLI_assert(mesh->runtime->looptris.array_wip == nullptr);

  SWAP(MLoopTri *, mesh->runtime->looptris.array, mesh->runtime->looptris.array_wip);

  if ((looptris_len > mesh->runtime->looptris.len_alloc) ||
      (looptris_len < mesh->runtime->looptris.len_alloc * 2) || (totpoly == 0)) {
    MEM_SAFE_FREE(mesh->runtime->looptris.array_wip);
    mesh->runtime->looptris.len_alloc = 0;
    mesh->runtime->looptris.len = 0;
  }

  if (totpoly) {
    if (mesh->runtime->looptris.array_wip == nullptr) {
      mesh->runtime->looptris.array_wip = static_cast<MLoopTri *>(
          MEM_malloc_arrayN(looptris_len, sizeof(*mesh->runtime->looptris.array_wip), __func__));
      mesh->runtime->looptris.len_alloc = looptris_len;
    }

    mesh->runtime->looptris.len = looptris_len;
  }
}

static void recalc_loopris(Mesh *mesh)
{
  mesh_ensure_looptri_data(mesh);
  BLI_assert(mesh->totpoly == 0 || mesh->runtime->looptris.array_wip != nullptr);
  const Span<MVert> verts = mesh->verts();
  const Span<MPoly> polys = mesh->polys();
  const Span<MLoop> loops = mesh->loops();

  if (!BKE_mesh_poly_normals_are_dirty(mesh)) {
    BKE_mesh_recalc_looptri_with_normals(loops.data(),
                                         polys.data(),
                                         verts.data(),
                                         mesh->totloop,
                                         mesh->totpoly,
                                         mesh->runtime->looptris.array_wip,
                                         BKE_mesh_poly_normals_ensure(mesh));
  }
  else {
    BKE_mesh_recalc_looptri(loops.data(),
                            polys.data(),
                            verts.data(),
                            mesh->totloop,
                            mesh->totpoly,
                            mesh->runtime->looptris.array_wip);
  }

  BLI_assert(mesh->runtime->looptris.array == nullptr);
  atomic_cas_ptr((void **)&mesh->runtime->looptris.array,
                 mesh->runtime->looptris.array,
                 mesh->runtime->looptris.array_wip);
  mesh->runtime->looptris.array_wip = nullptr;
}

int BKE_mesh_runtime_looptri_len(const Mesh *mesh)
{
  /* This is a ported copy of `dm_getNumLoopTri(dm)`. */
  const int looptri_len = poly_to_tri_count(mesh->totpoly, mesh->totloop);
  BLI_assert(ELEM(mesh->runtime->looptris.len, 0, looptri_len));
  return looptri_len;
}

const MLoopTri *BKE_mesh_runtime_looptri_ensure(const Mesh *mesh)
{
  std::lock_guard lock{mesh->runtime->eval_mutex};

  MLoopTri *looptri = mesh->runtime->looptris.array;

  if (looptri != nullptr) {
    BLI_assert(BKE_mesh_runtime_looptri_len(mesh) == mesh->runtime->looptris.len);
  }
  else {
    /* Must isolate multithreaded tasks while holding a mutex lock. */
    blender::threading::isolate_task([&]() { recalc_loopris(const_cast<Mesh *>(mesh)); });
    looptri = mesh->runtime->looptris.array;
  }

  return looptri;
}

void BKE_mesh_runtime_verttri_from_looptri(MVertTri *r_verttri,
                                           const MLoop *mloop,
                                           const MLoopTri *looptri,
                                           int looptri_num)
{
  for (int i = 0; i < looptri_num; i++) {
    r_verttri[i].tri[0] = mloop[looptri[i].tri[0]].v;
    r_verttri[i].tri[1] = mloop[looptri[i].tri[1]].v;
    r_verttri[i].tri[2] = mloop[looptri[i].tri[2]].v;
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
  free_bvh_cache(*mesh->runtime);
  free_normals(*mesh->runtime);
  free_subdiv_ccg(*mesh->runtime);
  if (mesh->runtime->shrinkwrap_data) {
    BKE_shrinkwrap_boundary_data_free(mesh->runtime->shrinkwrap_data);
  }
  MEM_SAFE_FREE(mesh->runtime->subsurf_face_dot_tags);
  MEM_SAFE_FREE(mesh->runtime->looptris.array);
}

void BKE_mesh_tag_coords_changed(Mesh *mesh)
{
  BKE_mesh_normals_tag_dirty(mesh);
  free_bvh_cache(*mesh->runtime);
  MEM_SAFE_FREE(mesh->runtime->looptris.array);
  mesh->runtime->bounds_cache.tag_dirty();
}

void BKE_mesh_tag_coords_changed_uniformly(Mesh *mesh)
{
  /* The normals and triangulation didn't change, since all verts moved by the same amount. */
  free_bvh_cache(*mesh->runtime);
  mesh->runtime->bounds_cache.tag_dirty();
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

  MutableSpan<MVert> verts = me_eval->verts_for_write();
  MutableSpan<MEdge> edges = me_eval->edges_for_write();
  MutableSpan<MPoly> polys = me_eval->polys_for_write();
  MutableSpan<MLoop> loops = me_eval->loops_for_write();

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

  is_valid &= BKE_mesh_validate_arrays(
      me_eval,
      verts.data(),
      verts.size(),
      edges.data(),
      edges.size(),
      static_cast<MFace *>(CustomData_get_layer(&me_eval->fdata, CD_MFACE)),
      me_eval->totface,
      loops.data(),
      loops.size(),
      polys.data(),
      polys.size(),
      me_eval->deform_verts_for_write().data(),
      do_verbose,
      do_fixes,
      &changed);

  BLI_assert(changed == false);

  return is_valid;
}

#endif /* NDEBUG */

/** \} */
