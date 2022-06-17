/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * The primary purpose of this API is to avoid unnecessary mesh conversion for the final
 * output of a modified mesh.
 *
 * This API handles the case when the modifier stack outputs a mesh which does not have
 * #Mesh data (#MPoly, #MLoop, #MEdge, #MVert).
 * Currently this is used so the resulting mesh can have #BMEditMesh data,
 * postponing the converting until it's needed or avoiding conversion entirely
 * which can be an expensive operation.
 * Once converted, the meshes type changes to #ME_WRAPPER_TYPE_MDATA,
 * although the edit mesh is not cleared.
 *
 * This API exposes functions that abstract over the different kinds of internal data,
 * as well as supporting converting the mesh into regular mesh.
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_task.hh"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_mesh.h"
#include "BKE_subdiv_modifier.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

Mesh *BKE_mesh_wrapper_from_editmesh_with_coords(BMEditMesh *em,
                                                 const CustomData_MeshMasks *cd_mask_extra,
                                                 const float (*vert_coords)[3],
                                                 const Mesh *me_settings)
{
  Mesh *me = static_cast<Mesh *>(BKE_id_new_nomain(ID_ME, nullptr));
  BKE_mesh_copy_parameters_for_eval(me, me_settings);
  BKE_mesh_runtime_ensure_edit_data(me);

  me->runtime.wrapper_type = ME_WRAPPER_TYPE_BMESH;
  if (cd_mask_extra) {
    me->runtime.cd_mask_extra = *cd_mask_extra;
  }

  /* Use edit-mesh directly where possible. */
  me->runtime.is_original = true;

  me->edit_mesh = static_cast<BMEditMesh *>(MEM_dupallocN(em));
  me->edit_mesh->is_shallow_copy = true;

  /* Make sure we crash if these are ever used. */
#ifdef DEBUG
  me->totvert = INT_MAX;
  me->totedge = INT_MAX;
  me->totpoly = INT_MAX;
  me->totloop = INT_MAX;
#else
  me->totvert = 0;
  me->totedge = 0;
  me->totpoly = 0;
  me->totloop = 0;
#endif

  EditMeshData *edit_data = me->runtime.edit_data;
  edit_data->vertexCos = vert_coords;
  return me;
}

Mesh *BKE_mesh_wrapper_from_editmesh(BMEditMesh *em,
                                     const CustomData_MeshMasks *cd_mask_extra,
                                     const Mesh *me_settings)
{
  return BKE_mesh_wrapper_from_editmesh_with_coords(em, cd_mask_extra, nullptr, me_settings);
}

void BKE_mesh_wrapper_ensure_mdata(Mesh *me)
{
  ThreadMutex *mesh_eval_mutex = (ThreadMutex *)me->runtime.eval_mutex;
  BLI_mutex_lock(mesh_eval_mutex);

  if (me->runtime.wrapper_type == ME_WRAPPER_TYPE_MDATA) {
    BLI_mutex_unlock(mesh_eval_mutex);
    return;
  }

  /* Must isolate multithreaded tasks while holding a mutex lock. */
  blender::threading::isolate_task([&]() {
    const eMeshWrapperType geom_type_orig = static_cast<eMeshWrapperType>(
        me->runtime.wrapper_type);
    me->runtime.wrapper_type = ME_WRAPPER_TYPE_MDATA;

    switch (geom_type_orig) {
      case ME_WRAPPER_TYPE_MDATA:
      case ME_WRAPPER_TYPE_SUBD: {
        break; /* Quiet warning. */
      }
      case ME_WRAPPER_TYPE_BMESH: {
        me->totvert = 0;
        me->totedge = 0;
        me->totpoly = 0;
        me->totloop = 0;

        BLI_assert(me->edit_mesh != nullptr);
        BLI_assert(me->runtime.edit_data != nullptr);

        BMEditMesh *em = me->edit_mesh;
        BM_mesh_bm_to_me_for_eval(em->bm, me, &me->runtime.cd_mask_extra);

        /* Adding original index layers assumes that all BMesh mesh wrappers are created from
         * original edit mode meshes (the only case where adding original indices makes sense).
         * If that assumption is broken, the layers might be incorrect in that they might not
         * actually be "original".
         *
         * There is also a performance aspect, where this also assumes that original indices are
         * always needed when converting an edit mesh to a mesh. That might be wrong, but it's not
         * harmful. */
        BKE_mesh_ensure_default_orig_index_customdata(me);

        EditMeshData *edit_data = me->runtime.edit_data;
        if (edit_data->vertexCos) {
          BKE_mesh_vert_coords_apply(me, edit_data->vertexCos);
          me->runtime.is_original = false;
        }
        break;
      }
    }

    if (me->runtime.wrapper_type_finalize) {
      BKE_mesh_wrapper_deferred_finalize(me, &me->runtime.cd_mask_extra);
    }
  });

  BLI_mutex_unlock(mesh_eval_mutex);
}

bool BKE_mesh_wrapper_minmax(const Mesh *me, float min[3], float max[3])
{
  switch ((eMeshWrapperType)me->runtime.wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      return BKE_editmesh_cache_calc_minmax(me->edit_mesh, me->runtime.edit_data, min, max);
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD:
      return BKE_mesh_minmax(me, min, max);
  }
  BLI_assert_unreachable();
  return false;
}

/* -------------------------------------------------------------------- */
/** \name Mesh Coordinate Access
 * \{ */

void BKE_mesh_wrapper_vert_coords_copy(const Mesh *me,
                                       float (*vert_coords)[3],
                                       int vert_coords_len)
{
  switch ((eMeshWrapperType)me->runtime.wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH: {
      BMesh *bm = me->edit_mesh->bm;
      BLI_assert(vert_coords_len <= bm->totvert);
      EditMeshData *edit_data = me->runtime.edit_data;
      if (edit_data->vertexCos != nullptr) {
        for (int i = 0; i < vert_coords_len; i++) {
          copy_v3_v3(vert_coords[i], edit_data->vertexCos[i]);
        }
      }
      else {
        BMIter iter;
        BMVert *v;
        int i;
        BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
          copy_v3_v3(vert_coords[i], v->co);
        }
      }
      return;
    }
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD: {
      BLI_assert(vert_coords_len <= me->totvert);
      const MVert *mvert = me->mvert;
      for (int i = 0; i < vert_coords_len; i++) {
        copy_v3_v3(vert_coords[i], mvert[i].co);
      }
      return;
    }
  }
  BLI_assert_unreachable();
}

void BKE_mesh_wrapper_vert_coords_copy_with_mat4(const Mesh *me,
                                                 float (*vert_coords)[3],
                                                 int vert_coords_len,
                                                 const float mat[4][4])
{
  switch ((eMeshWrapperType)me->runtime.wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH: {
      BMesh *bm = me->edit_mesh->bm;
      BLI_assert(vert_coords_len == bm->totvert);
      EditMeshData *edit_data = me->runtime.edit_data;
      if (edit_data->vertexCos != nullptr) {
        for (int i = 0; i < vert_coords_len; i++) {
          mul_v3_m4v3(vert_coords[i], mat, edit_data->vertexCos[i]);
        }
      }
      else {
        BMIter iter;
        BMVert *v;
        int i;
        BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
          mul_v3_m4v3(vert_coords[i], mat, v->co);
        }
      }
      return;
    }
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD: {
      BLI_assert(vert_coords_len == me->totvert);
      const MVert *mvert = me->mvert;
      for (int i = 0; i < vert_coords_len; i++) {
        mul_v3_m4v3(vert_coords[i], mat, mvert[i].co);
      }
      return;
    }
  }
  BLI_assert_unreachable();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Array Length Access
 * \{ */

int BKE_mesh_wrapper_vert_len(const Mesh *me)
{
  switch ((eMeshWrapperType)me->runtime.wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      return me->edit_mesh->bm->totvert;
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD:
      return me->totvert;
  }
  BLI_assert_unreachable();
  return -1;
}

int BKE_mesh_wrapper_edge_len(const Mesh *me)
{
  switch ((eMeshWrapperType)me->runtime.wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      return me->edit_mesh->bm->totedge;
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD:
      return me->totedge;
  }
  BLI_assert_unreachable();
  return -1;
}

int BKE_mesh_wrapper_loop_len(const Mesh *me)
{
  switch ((eMeshWrapperType)me->runtime.wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      return me->edit_mesh->bm->totloop;
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD:
      return me->totloop;
  }
  BLI_assert_unreachable();
  return -1;
}

int BKE_mesh_wrapper_poly_len(const Mesh *me)
{
  switch ((eMeshWrapperType)me->runtime.wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      return me->edit_mesh->bm->totface;
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD:
      return me->totpoly;
  }
  BLI_assert_unreachable();
  return -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name CPU Subdivision Evaluation
 * \{ */

static Mesh *mesh_wrapper_ensure_subdivision(const Object *ob, Mesh *me)
{
  SubsurfModifierData *smd = BKE_object_get_last_subsurf_modifier(ob);
  if (!smd) {
    return me;
  }

  SubsurfRuntimeData *runtime_data = (SubsurfRuntimeData *)smd->modifier.runtime;
  if (runtime_data == nullptr || runtime_data->settings.level == 0) {
    return me;
  }

  /* Initialize the settings before ensuring the descriptor as this is checked to decide whether
   * subdivision is needed at all, and checking the descriptor status might involve checking if the
   * data is out-of-date, which is a very expensive operation. */
  SubdivToMeshSettings mesh_settings;
  mesh_settings.resolution = runtime_data->resolution;
  mesh_settings.use_optimal_display = runtime_data->use_optimal_display;

  if (mesh_settings.resolution < 3) {
    return me;
  }

  Subdiv *subdiv = BKE_subsurf_modifier_subdiv_descriptor_ensure(runtime_data, me, false);
  if (subdiv == nullptr) {
    /* Happens on bad topology, but also on empty input mesh. */
    return me;
  }
  const bool use_clnors = BKE_subsurf_modifier_use_custom_loop_normals(smd, me);
  if (use_clnors) {
    /* If custom normals are present and the option is turned on calculate the split
     * normals and clear flag so the normals get interpolated to the result mesh. */
    BKE_mesh_calc_normals_split(me);
    CustomData_clear_layer_flag(&me->ldata, CD_NORMAL, CD_FLAG_TEMPORARY);
  }

  Mesh *subdiv_mesh = BKE_subdiv_to_mesh(subdiv, &mesh_settings, me);

  if (use_clnors) {
    float(*lnors)[3] = static_cast<float(*)[3]>(
        CustomData_get_layer(&subdiv_mesh->ldata, CD_NORMAL));
    BLI_assert(lnors != NULL);
    BKE_mesh_set_custom_normals(subdiv_mesh, lnors);
    CustomData_set_layer_flag(&me->ldata, CD_NORMAL, CD_FLAG_TEMPORARY);
    CustomData_set_layer_flag(&subdiv_mesh->ldata, CD_NORMAL, CD_FLAG_TEMPORARY);
  }
  else if (runtime_data->calc_loop_normals) {
    BKE_mesh_calc_normals_split(subdiv_mesh);
  }

  if (subdiv != runtime_data->subdiv) {
    BKE_subdiv_free(subdiv);
  }

  if (subdiv_mesh != me) {
    if (me->runtime.mesh_eval != nullptr) {
      BKE_id_free(nullptr, me->runtime.mesh_eval);
    }
    me->runtime.mesh_eval = subdiv_mesh;
    me->runtime.wrapper_type = ME_WRAPPER_TYPE_SUBD;
  }

  return me->runtime.mesh_eval;
}

Mesh *BKE_mesh_wrapper_ensure_subdivision(const Object *ob, Mesh *me)
{
  ThreadMutex *mesh_eval_mutex = (ThreadMutex *)me->runtime.eval_mutex;
  BLI_mutex_lock(mesh_eval_mutex);

  if (me->runtime.wrapper_type == ME_WRAPPER_TYPE_SUBD) {
    BLI_mutex_unlock(mesh_eval_mutex);
    return me->runtime.mesh_eval;
  }

  Mesh *result;

  /* Must isolate multithreaded tasks while holding a mutex lock. */
  blender::threading::isolate_task([&]() { result = mesh_wrapper_ensure_subdivision(ob, me); });

  BLI_mutex_unlock(mesh_eval_mutex);
  return result;
}

/** \} */
