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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "atomic_ops.h"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math_geom.h"
#include "BLI_task.h"
#include "BLI_threads.h"

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
  mesh->runtime.eval_mutex = MEM_mallocN(sizeof(ThreadMutex), "mesh runtime eval_mutex");
  BLI_mutex_init(mesh->runtime.eval_mutex);
  mesh->runtime.normals_mutex = MEM_mallocN(sizeof(ThreadMutex), "mesh runtime normals_mutex");
  BLI_mutex_init(mesh->runtime.normals_mutex);
  mesh->runtime.render_mutex = MEM_mallocN(sizeof(ThreadMutex), "mesh runtime render_mutex");
  BLI_mutex_init(mesh->runtime.render_mutex);
}

/**
 * \brief free the mutexes of the given mesh runtime.
 */
static void mesh_runtime_free_mutexes(Mesh *mesh)
{
  if (mesh->runtime.eval_mutex != NULL) {
    BLI_mutex_end(mesh->runtime.eval_mutex);
    MEM_freeN(mesh->runtime.eval_mutex);
    mesh->runtime.eval_mutex = NULL;
  }
  if (mesh->runtime.normals_mutex != NULL) {
    BLI_mutex_end(mesh->runtime.normals_mutex);
    MEM_freeN(mesh->runtime.normals_mutex);
    mesh->runtime.normals_mutex = NULL;
  }
  if (mesh->runtime.render_mutex != NULL) {
    BLI_mutex_end(mesh->runtime.render_mutex);
    MEM_freeN(mesh->runtime.render_mutex);
    mesh->runtime.render_mutex = NULL;
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

  runtime->mesh_eval = NULL;
  runtime->edit_data = NULL;
  runtime->batch_cache = NULL;
  runtime->subdiv_ccg = NULL;
  memset(&runtime->looptris, 0, sizeof(runtime->looptris));
  runtime->bvh_cache = NULL;
  runtime->shrinkwrap_data = NULL;

  mesh_runtime_init_mutexes(mesh);
}

void BKE_mesh_runtime_clear_cache(Mesh *mesh)
{
  if (mesh->runtime.mesh_eval != NULL) {
    mesh->runtime.mesh_eval->edit_mesh = NULL;
    BKE_id_free(NULL, mesh->runtime.mesh_eval);
    mesh->runtime.mesh_eval = NULL;
  }
  BKE_mesh_runtime_clear_geometry(mesh);
  BKE_mesh_batch_cache_free(mesh);
  BKE_mesh_runtime_clear_edit_data(mesh);
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

  BLI_assert(mesh->runtime.looptris.array_wip == NULL);

  SWAP(MLoopTri *, mesh->runtime.looptris.array, mesh->runtime.looptris.array_wip);

  if ((looptris_len > mesh->runtime.looptris.len_alloc) ||
      (looptris_len < mesh->runtime.looptris.len_alloc * 2) || (totpoly == 0)) {
    MEM_SAFE_FREE(mesh->runtime.looptris.array_wip);
    mesh->runtime.looptris.len_alloc = 0;
    mesh->runtime.looptris.len = 0;
  }

  if (totpoly) {
    if (mesh->runtime.looptris.array_wip == NULL) {
      mesh->runtime.looptris.array_wip = MEM_malloc_arrayN(
          looptris_len, sizeof(*mesh->runtime.looptris.array_wip), __func__);
      mesh->runtime.looptris.len_alloc = looptris_len;
    }

    mesh->runtime.looptris.len = looptris_len;
  }
}

void BKE_mesh_runtime_looptri_recalc(Mesh *mesh)
{
  mesh_ensure_looptri_data(mesh);
  BLI_assert(mesh->totpoly == 0 || mesh->runtime.looptris.array_wip != NULL);

  BKE_mesh_recalc_looptri(mesh->mloop,
                          mesh->mpoly,
                          mesh->mvert,
                          mesh->totloop,
                          mesh->totpoly,
                          mesh->runtime.looptris.array_wip);

  BLI_assert(mesh->runtime.looptris.array == NULL);
  atomic_cas_ptr((void **)&mesh->runtime.looptris.array,
                 mesh->runtime.looptris.array,
                 mesh->runtime.looptris.array_wip);
  mesh->runtime.looptris.array_wip = NULL;
}

int BKE_mesh_runtime_looptri_len(const Mesh *mesh)
{
  /* This is a ported copy of `dm_getNumLoopTri(dm)`. */
  const int looptri_len = poly_to_tri_count(mesh->totpoly, mesh->totloop);
  BLI_assert(ELEM(mesh->runtime.looptris.len, 0, looptri_len));
  return looptri_len;
}

static void mesh_runtime_looptri_recalc_isolated(void *userdata)
{
  Mesh *mesh = userdata;
  BKE_mesh_runtime_looptri_recalc(mesh);
}

const MLoopTri *BKE_mesh_runtime_looptri_ensure(const Mesh *mesh)
{
  ThreadMutex *mesh_eval_mutex = (ThreadMutex *)mesh->runtime.eval_mutex;
  BLI_mutex_lock(mesh_eval_mutex);

  MLoopTri *looptri = mesh->runtime.looptris.array;

  if (looptri != NULL) {
    BLI_assert(BKE_mesh_runtime_looptri_len(mesh) == mesh->runtime.looptris.len);
  }
  else {
    /* Must isolate multithreaded tasks while holding a mutex lock. */
    BLI_task_isolate(mesh_runtime_looptri_recalc_isolated, (void *)mesh);
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
  if (mesh->runtime.edit_data != NULL) {
    return false;
  }

  mesh->runtime.edit_data = MEM_callocN(sizeof(EditMeshData), "EditMeshData");
  return true;
}

bool BKE_mesh_runtime_reset_edit_data(Mesh *mesh)
{
  EditMeshData *edit_data = mesh->runtime.edit_data;
  if (edit_data == NULL) {
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
  if (mesh->runtime.edit_data == NULL) {
    return false;
  }
  BKE_mesh_runtime_reset_edit_data(mesh);

  MEM_freeN(mesh->runtime.edit_data);
  mesh->runtime.edit_data = NULL;

  return true;
}

void BKE_mesh_runtime_clear_geometry(Mesh *mesh)
{
  if (mesh->runtime.bvh_cache) {
    bvhcache_free(mesh->runtime.bvh_cache);
    mesh->runtime.bvh_cache = NULL;
  }
  MEM_SAFE_FREE(mesh->runtime.looptris.array);
  /* TODO(sergey): Does this really belong here? */
  if (mesh->runtime.subdiv_ccg != NULL) {
    BKE_subdiv_ccg_destroy(mesh->runtime.subdiv_ccg);
    mesh->runtime.subdiv_ccg = NULL;
  }
  BKE_shrinkwrap_discard_boundary_data(mesh);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Batch Cache Callbacks
 * \{ */

/* Draw Engine */
void (*BKE_mesh_batch_cache_dirty_tag_cb)(Mesh *me, eMeshBatchDirtyMode mode) = NULL;
void (*BKE_mesh_batch_cache_free_cb)(Mesh *me) = NULL;

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
