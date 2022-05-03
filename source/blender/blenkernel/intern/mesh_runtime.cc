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
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_shrinkwrap.h"
#include "BKE_subdiv_ccg.h"

/* -------------------------------------------------------------------- */
/** \name Mesh Runtime Struct Utils
 * \{ */

/**
 * \brief Initialize the runtime mutexes of the given mesh.
 *
 * Any existing mutexes will be overridden.
 */
static void mesh_runtime_init_mutexes(Mesh *mesh)
{
  mesh->runtime.eval_mutex = MEM_new<ThreadMutex>("mesh runtime eval_mutex");
  BLI_mutex_init(static_cast<ThreadMutex *>(mesh->runtime.eval_mutex));
  mesh->runtime.normals_mutex = MEM_new<ThreadMutex>("mesh runtime normals_mutex");
  BLI_mutex_init(static_cast<ThreadMutex *>(mesh->runtime.normals_mutex));
  mesh->runtime.render_mutex = MEM_new<ThreadMutex>("mesh runtime render_mutex");
  BLI_mutex_init(static_cast<ThreadMutex *>(mesh->runtime.render_mutex));
}

/**
 * \brief free the mutexes of the given mesh runtime.
 */
static void mesh_runtime_free_mutexes(Mesh *mesh)
{
  if (mesh->runtime.eval_mutex != nullptr) {
    BLI_mutex_end(static_cast<ThreadMutex *>(mesh->runtime.eval_mutex));
    MEM_freeN(mesh->runtime.eval_mutex);
    mesh->runtime.eval_mutex = nullptr;
  }
  if (mesh->runtime.normals_mutex != nullptr) {
    BLI_mutex_end(static_cast<ThreadMutex *>(mesh->runtime.normals_mutex));
    MEM_freeN(mesh->runtime.normals_mutex);
    mesh->runtime.normals_mutex = nullptr;
  }
  if (mesh->runtime.render_mutex != nullptr) {
    BLI_mutex_end(static_cast<ThreadMutex *>(mesh->runtime.render_mutex));
    MEM_freeN(mesh->runtime.render_mutex);
    mesh->runtime.render_mutex = nullptr;
  }
}

void BKE_mesh_runtime_init_data(Mesh *mesh)
{
  mesh_runtime_init_mutexes(mesh);
}

void BKE_mesh_runtime_free_data(Mesh *mesh)
{
  BKE_mesh_runtime_clear_cache(mesh);
  mesh_runtime_free_mutexes(mesh);
}

void BKE_mesh_runtime_reset_on_copy(Mesh *mesh, const int UNUSED(flag))
{
  Mesh_Runtime *runtime = &mesh->runtime;

  runtime->mesh_eval = nullptr;
  runtime->edit_data = nullptr;
  runtime->batch_cache = nullptr;
  runtime->subdiv_ccg = nullptr;
  runtime->looptris = blender::dna::shallow_zero_initialize();
  runtime->bvh_cache = nullptr;
  runtime->shrinkwrap_data = nullptr;
  runtime->subsurf_face_dot_tags = nullptr;

  runtime->vert_normals_dirty = true;
  runtime->poly_normals_dirty = true;
  runtime->vert_normals = nullptr;
  runtime->poly_normals = nullptr;

  mesh_runtime_init_mutexes(mesh);
}

void BKE_mesh_runtime_clear_cache(Mesh *mesh)
{
  if (mesh->runtime.mesh_eval != nullptr) {
    mesh->runtime.mesh_eval->edit_mesh = nullptr;
    BKE_id_free(nullptr, mesh->runtime.mesh_eval);
    mesh->runtime.mesh_eval = nullptr;
  }
  BKE_mesh_runtime_clear_geometry(mesh);
  BKE_mesh_batch_cache_free(mesh);
  BKE_mesh_runtime_clear_edit_data(mesh);
  BKE_mesh_clear_derived_normals(mesh);
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

  BLI_assert(mesh->runtime.looptris.array_wip == nullptr);

  SWAP(MLoopTri *, mesh->runtime.looptris.array, mesh->runtime.looptris.array_wip);

  if ((looptris_len > mesh->runtime.looptris.len_alloc) ||
      (looptris_len < mesh->runtime.looptris.len_alloc * 2) || (totpoly == 0)) {
    MEM_SAFE_FREE(mesh->runtime.looptris.array_wip);
    mesh->runtime.looptris.len_alloc = 0;
    mesh->runtime.looptris.len = 0;
  }

  if (totpoly) {
    if (mesh->runtime.looptris.array_wip == nullptr) {
      mesh->runtime.looptris.array_wip = static_cast<MLoopTri *>(
          MEM_malloc_arrayN(looptris_len, sizeof(*mesh->runtime.looptris.array_wip), __func__));
      mesh->runtime.looptris.len_alloc = looptris_len;
    }

    mesh->runtime.looptris.len = looptris_len;
  }
}

void BKE_mesh_runtime_looptri_recalc(Mesh *mesh)
{
  mesh_ensure_looptri_data(mesh);
  BLI_assert(mesh->totpoly == 0 || mesh->runtime.looptris.array_wip != nullptr);

  BKE_mesh_recalc_looptri(mesh->mloop,
                          mesh->mpoly,
                          mesh->mvert,
                          mesh->totloop,
                          mesh->totpoly,
                          mesh->runtime.looptris.array_wip);

  BLI_assert(mesh->runtime.looptris.array == nullptr);
  atomic_cas_ptr((void **)&mesh->runtime.looptris.array,
                 mesh->runtime.looptris.array,
                 mesh->runtime.looptris.array_wip);
  mesh->runtime.looptris.array_wip = nullptr;
}

int BKE_mesh_runtime_looptri_len(const Mesh *mesh)
{
  /* This is a ported copy of `dm_getNumLoopTri(dm)`. */
  const int looptri_len = poly_to_tri_count(mesh->totpoly, mesh->totloop);
  BLI_assert(ELEM(mesh->runtime.looptris.len, 0, looptri_len));
  return looptri_len;
}

const MLoopTri *BKE_mesh_runtime_looptri_ensure(const Mesh *mesh)
{
  ThreadMutex *mesh_eval_mutex = (ThreadMutex *)mesh->runtime.eval_mutex;
  BLI_mutex_lock(mesh_eval_mutex);

  MLoopTri *looptri = mesh->runtime.looptris.array;

  if (looptri != nullptr) {
    BLI_assert(BKE_mesh_runtime_looptri_len(mesh) == mesh->runtime.looptris.len);
  }
  else {
    /* Must isolate multithreaded tasks while holding a mutex lock. */
    blender::threading::isolate_task(
        [&]() { BKE_mesh_runtime_looptri_recalc(const_cast<Mesh *>(mesh)); });
    looptri = mesh->runtime.looptris.array;
  }

  BLI_mutex_unlock(mesh_eval_mutex);

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
  if (mesh->runtime.edit_data != nullptr) {
    return false;
  }

  mesh->runtime.edit_data = MEM_cnew<EditMeshData>(__func__);
  return true;
}

bool BKE_mesh_runtime_reset_edit_data(Mesh *mesh)
{
  EditMeshData *edit_data = mesh->runtime.edit_data;
  if (edit_data == nullptr) {
    return false;
  }

  MEM_SAFE_FREE(edit_data->polyCos);
  MEM_SAFE_FREE(edit_data->polyNos);
  MEM_SAFE_FREE(edit_data->vertexCos);
  MEM_SAFE_FREE(edit_data->vertexNos);

  return true;
}

bool BKE_mesh_runtime_clear_edit_data(Mesh *mesh)
{
  if (mesh->runtime.edit_data == nullptr) {
    return false;
  }
  BKE_mesh_runtime_reset_edit_data(mesh);

  MEM_freeN(mesh->runtime.edit_data);
  mesh->runtime.edit_data = nullptr;

  return true;
}

void BKE_mesh_runtime_clear_geometry(Mesh *mesh)
{
  if (mesh->runtime.bvh_cache) {
    bvhcache_free(mesh->runtime.bvh_cache);
    mesh->runtime.bvh_cache = nullptr;
  }
  MEM_SAFE_FREE(mesh->runtime.looptris.array);
  /* TODO(sergey): Does this really belong here? */
  if (mesh->runtime.subdiv_ccg != nullptr) {
    BKE_subdiv_ccg_destroy(mesh->runtime.subdiv_ccg);
    mesh->runtime.subdiv_ccg = nullptr;
  }
  BKE_shrinkwrap_discard_boundary_data(mesh);

  MEM_SAFE_FREE(mesh->runtime.subsurf_face_dot_tags);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Batch Cache Callbacks
 * \{ */

/* Draw Engine */
void (*BKE_mesh_batch_cache_dirty_tag_cb)(Mesh *me, eMeshBatchDirtyMode mode) = nullptr;
void (*BKE_mesh_batch_cache_free_cb)(Mesh *me) = nullptr;

void BKE_mesh_batch_cache_dirty_tag(Mesh *me, eMeshBatchDirtyMode mode)
{
  if (me->runtime.batch_cache) {
    BKE_mesh_batch_cache_dirty_tag_cb(me, mode);
  }
}
void BKE_mesh_batch_cache_free(Mesh *me)
{
  if (me->runtime.batch_cache) {
    BKE_mesh_batch_cache_free_cb(me);
  }
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
                                       me_eval->mvert,
                                       me_eval->totvert,
                                       me_eval->medge,
                                       me_eval->totedge,
                                       me_eval->mface,
                                       me_eval->totface,
                                       me_eval->mloop,
                                       me_eval->totloop,
                                       me_eval->mpoly,
                                       me_eval->totpoly,
                                       me_eval->dvert,
                                       do_verbose,
                                       do_fixes,
                                       &changed);

  BLI_assert(changed == false);

  return is_valid;
}

#endif /* NDEBUG */

/** \} */
