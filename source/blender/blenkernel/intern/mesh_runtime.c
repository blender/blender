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

static ThreadRWMutex loops_cache_lock = PTHREAD_RWLOCK_INITIALIZER;

/**
 * Default values defined at read time.
 */
void BKE_mesh_runtime_reset(Mesh *mesh)
{
  memset(&mesh->runtime, 0, sizeof(mesh->runtime));
  mesh->runtime.eval_mutex = MEM_mallocN(sizeof(ThreadMutex), "mesh runtime eval_mutex");
  BLI_mutex_init(mesh->runtime.eval_mutex);
  mesh->runtime.bvh_cache = NULL;
}

/* Clear all pointers which we don't want to be shared on copying the datablock.
 * However, keep all the flags which defines what the mesh is (for example, that
 * it's deformed only, or that its custom data layers are out of date.) */
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

  mesh->runtime.eval_mutex = MEM_mallocN(sizeof(ThreadMutex), "mesh runtime eval_mutex");
  BLI_mutex_init(mesh->runtime.eval_mutex);
}

void BKE_mesh_runtime_clear_cache(Mesh *mesh)
{
  if (mesh->runtime.eval_mutex != NULL) {
    BLI_mutex_end(mesh->runtime.eval_mutex);
    MEM_freeN(mesh->runtime.eval_mutex);
    mesh->runtime.eval_mutex = NULL;
  }
  if (mesh->runtime.mesh_eval != NULL) {
    mesh->runtime.mesh_eval->edit_mesh = NULL;
    BKE_id_free(NULL, mesh->runtime.mesh_eval);
    mesh->runtime.mesh_eval = NULL;
  }
  BKE_mesh_runtime_clear_geometry(mesh);
  BKE_mesh_batch_cache_free(mesh);
  BKE_mesh_runtime_clear_edit_data(mesh);
}

/* This is a ported copy of DM_ensure_looptri_data(dm) */
/**
 * Ensure the array is large enough
 *
 * \note This function must always be thread-protected by caller.
 * It should only be used by internal code.
 */
static void mesh_ensure_looptri_data(Mesh *mesh)
{
  const unsigned int totpoly = mesh->totpoly;
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

/* This is a ported copy of CDDM_recalc_looptri(dm). */
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

/* This is a ported copy of dm_getNumLoopTri(dm). */
int BKE_mesh_runtime_looptri_len(const Mesh *mesh)
{
  const int looptri_len = poly_to_tri_count(mesh->totpoly, mesh->totloop);
  BLI_assert(ELEM(mesh->runtime.looptris.len, 0, looptri_len));
  return looptri_len;
}

/* This is a ported copy of dm_getLoopTriArray(dm). */
const MLoopTri *BKE_mesh_runtime_looptri_ensure(Mesh *mesh)
{
  MLoopTri *looptri;

  BLI_rw_mutex_lock(&loops_cache_lock, THREAD_LOCK_READ);
  looptri = mesh->runtime.looptris.array;
  BLI_rw_mutex_unlock(&loops_cache_lock);

  if (looptri != NULL) {
    BLI_assert(BKE_mesh_runtime_looptri_len(mesh) == mesh->runtime.looptris.len);
  }
  else {
    BLI_rw_mutex_lock(&loops_cache_lock, THREAD_LOCK_WRITE);
    /* We need to ensure array is still NULL inside mutex-protected code,
     * some other thread might have already recomputed those looptris. */
    if (mesh->runtime.looptris.array == NULL) {
      BKE_mesh_runtime_looptri_recalc(mesh);
    }
    looptri = mesh->runtime.looptris.array;
    BLI_rw_mutex_unlock(&loops_cache_lock);
  }
  return looptri;
}

/* This is a copy of DM_verttri_from_looptri(). */
void BKE_mesh_runtime_verttri_from_looptri(MVertTri *r_verttri,
                                           const MLoop *mloop,
                                           const MLoopTri *looptri,
                                           int looptri_num)
{
  int i;
  for (i = 0; i < looptri_num; i++) {
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

bool BKE_mesh_runtime_clear_edit_data(Mesh *mesh)
{
  if (mesh->runtime.edit_data == NULL) {
    return false;
  }

  if (mesh->runtime.edit_data->polyCos != NULL) {
    MEM_freeN((void *)mesh->runtime.edit_data->polyCos);
  }
  if (mesh->runtime.edit_data->polyNos != NULL) {
    MEM_freeN((void *)mesh->runtime.edit_data->polyNos);
  }
  if (mesh->runtime.edit_data->vertexCos != NULL) {
    MEM_freeN((void *)mesh->runtime.edit_data->vertexCos);
  }
  if (mesh->runtime.edit_data->vertexNos != NULL) {
    MEM_freeN((void *)mesh->runtime.edit_data->vertexNos);
  }

  MEM_SAFE_FREE(mesh->runtime.edit_data);
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
void (*BKE_mesh_batch_cache_dirty_tag_cb)(Mesh *me, int mode) = NULL;
void (*BKE_mesh_batch_cache_free_cb)(Mesh *me) = NULL;

void BKE_mesh_batch_cache_dirty_tag(Mesh *me, int mode)
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

/** \name Mesh runtime debug helpers.
 * \{ */
/* evaluated mesh info printing function,
 * to help track down differences output */

#ifndef NDEBUG
#  include "BLI_dynstr.h"

static void mesh_runtime_debug_info_layers(DynStr *dynstr, CustomData *cd)
{
  int type;

  for (type = 0; type < CD_NUMTYPES; type++) {
    if (CustomData_has_layer(cd, type)) {
      /* note: doesn't account for multiple layers */
      const char *name = CustomData_layertype_name(type);
      const int size = CustomData_sizeof(type);
      const void *pt = CustomData_get_layer(cd, type);
      const int pt_size = pt ? (int)(MEM_allocN_len(pt) / size) : 0;
      const char *structname;
      int structnum;
      CustomData_file_write_info(type, &structname, &structnum);
      BLI_dynstr_appendf(
          dynstr,
          "        dict(name='%s', struct='%s', type=%d, ptr='%p', elem=%d, length=%d),\n",
          name,
          structname,
          type,
          (const void *)pt,
          size,
          pt_size);
    }
  }
}

char *BKE_mesh_runtime_debug_info(Mesh *me_eval)
{
  DynStr *dynstr = BLI_dynstr_new();
  char *ret;

  BLI_dynstr_append(dynstr, "{\n");
  BLI_dynstr_appendf(dynstr, "    'ptr': '%p',\n", (void *)me_eval);
#  if 0
  const char *tstr;
  switch (me_eval->type) {
    case DM_TYPE_CDDM:
      tstr = "DM_TYPE_CDDM";
      break;
    case DM_TYPE_CCGDM:
      tstr = "DM_TYPE_CCGDM";
      break;
    default:
      tstr = "UNKNOWN";
      break;
  }
  BLI_dynstr_appendf(dynstr, "    'type': '%s',\n", tstr);
#  endif
  BLI_dynstr_appendf(dynstr, "    'totvert': %d,\n", me_eval->totvert);
  BLI_dynstr_appendf(dynstr, "    'totedge': %d,\n", me_eval->totedge);
  BLI_dynstr_appendf(dynstr, "    'totface': %d,\n", me_eval->totface);
  BLI_dynstr_appendf(dynstr, "    'totpoly': %d,\n", me_eval->totpoly);
  BLI_dynstr_appendf(dynstr, "    'deformed_only': %d,\n", me_eval->runtime.deformed_only);

  BLI_dynstr_append(dynstr, "    'vertexLayers': (\n");
  mesh_runtime_debug_info_layers(dynstr, &me_eval->vdata);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'edgeLayers': (\n");
  mesh_runtime_debug_info_layers(dynstr, &me_eval->edata);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'loopLayers': (\n");
  mesh_runtime_debug_info_layers(dynstr, &me_eval->ldata);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'polyLayers': (\n");
  mesh_runtime_debug_info_layers(dynstr, &me_eval->pdata);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'tessFaceLayers': (\n");
  mesh_runtime_debug_info_layers(dynstr, &me_eval->fdata);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "}\n");

  ret = BLI_dynstr_get_cstring(dynstr);
  BLI_dynstr_free(dynstr);
  return ret;
}

void BKE_mesh_runtime_debug_print(Mesh *me_eval)
{
  char *str = BKE_mesh_runtime_debug_info(me_eval);
  puts(str);
  fflush(stdout);
  MEM_freeN(str);
}

/* XXX Should go in customdata file? */
void BKE_mesh_runtime_debug_print_cdlayers(CustomData *data)
{
  int i;
  const CustomDataLayer *layer;

  printf("{\n");

  for (i = 0, layer = data->layers; i < data->totlayer; i++, layer++) {

    const char *name = CustomData_layertype_name(layer->type);
    const int size = CustomData_sizeof(layer->type);
    const char *structname;
    int structnum;
    CustomData_file_write_info(layer->type, &structname, &structnum);
    printf("        dict(name='%s', struct='%s', type=%d, ptr='%p', elem=%d, length=%d),\n",
           name,
           structname,
           layer->type,
           (const void *)layer->data,
           size,
           (int)(MEM_allocN_len(layer->data) / size));
  }

  printf("}\n");
}

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
