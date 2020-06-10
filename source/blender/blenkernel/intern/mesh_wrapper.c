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
 */

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

#include "DNA_defaults.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_bitmap.h"
#include "BLI_edgehash.h"
#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_animsys.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.h"
#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_object.h"

#include "PIL_time.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

Mesh *BKE_mesh_wrapper_from_editmesh_with_coords(BMEditMesh *em,
                                                 const CustomData_MeshMasks *cd_mask_extra,
                                                 float (*vertexCos)[3],
                                                 const Mesh *me_settings)
{
  Mesh *me = BKE_id_new_nomain(ID_ME, NULL);
  BKE_mesh_copy_settings(me, me_settings);
  BKE_mesh_runtime_ensure_edit_data(me);

  me->runtime.wrapper_type = ME_WRAPPER_TYPE_BMESH;
  if (cd_mask_extra) {
    me->runtime.cd_mask_extra = *cd_mask_extra;
  }

  /* Use edit-mesh directly where possible. */
  me->runtime.is_original = true;
  me->edit_mesh = MEM_dupallocN(em);

/* Make sure, we crash if these are ever used. */
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
  edit_data->vertexCos = vertexCos;
  return me;
}

Mesh *BKE_mesh_wrapper_from_editmesh(BMEditMesh *em,
                                     const CustomData_MeshMasks *cd_mask_extra,
                                     const Mesh *me_settings)
{
  return BKE_mesh_wrapper_from_editmesh_with_coords(em, cd_mask_extra, NULL, me_settings);
}

void BKE_mesh_wrapper_ensure_mdata(Mesh *me)
{
  if (me->runtime.wrapper_type == ME_WRAPPER_TYPE_MDATA) {
    return;
  }
  const eMeshWrapperType geom_type_orig = me->runtime.wrapper_type;
  me->runtime.wrapper_type = ME_WRAPPER_TYPE_MDATA;

  switch (geom_type_orig) {
    case ME_WRAPPER_TYPE_MDATA: {
      break; /* Quiet warning. */
    }
    case ME_WRAPPER_TYPE_BMESH: {
      me->totvert = 0;
      me->totedge = 0;
      me->totpoly = 0;
      me->totloop = 0;

      BLI_assert(me->edit_mesh != NULL);
      BLI_assert(me->runtime.edit_data != NULL);

      BMEditMesh *em = me->edit_mesh;
      BM_mesh_bm_to_me_for_eval(em->bm, me, &me->runtime.cd_mask_extra);

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
}

bool BKE_mesh_wrapper_minmax(const Mesh *me, float min[3], float max[3])
{
  switch ((eMeshWrapperType)me->runtime.wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      return BKE_editmesh_cache_calc_minmax(me->edit_mesh, me->runtime.edit_data, min, max);
    case ME_WRAPPER_TYPE_MDATA:
      return BKE_mesh_minmax(me, min, max);
  }
  BLI_assert(0);
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
      if (edit_data->vertexCos != NULL) {
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
    case ME_WRAPPER_TYPE_MDATA: {
      BLI_assert(vert_coords_len <= me->totvert);
      const MVert *mvert = me->mvert;
      for (int i = 0; i < vert_coords_len; i++) {
        copy_v3_v3(vert_coords[i], mvert[i].co);
      }
      return;
    }
  }
  BLI_assert(0);
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
      if (edit_data->vertexCos != NULL) {
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
    case ME_WRAPPER_TYPE_MDATA: {
      BLI_assert(vert_coords_len == me->totvert);
      const MVert *mvert = me->mvert;
      for (int i = 0; i < vert_coords_len; i++) {
        mul_v3_m4v3(vert_coords[i], mat, mvert[i].co);
      }
      return;
    }
  }
  BLI_assert(0);
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
      return me->totvert;
  }
  BLI_assert(0);
  return -1;
}

int BKE_mesh_wrapper_edge_len(const Mesh *me)
{
  switch ((eMeshWrapperType)me->runtime.wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      return me->edit_mesh->bm->totedge;
    case ME_WRAPPER_TYPE_MDATA:
      return me->totedge;
  }
  BLI_assert(0);
  return -1;
}

int BKE_mesh_wrapper_loop_len(const Mesh *me)
{
  switch ((eMeshWrapperType)me->runtime.wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      return me->edit_mesh->bm->totloop;
    case ME_WRAPPER_TYPE_MDATA:
      return me->totloop;
  }
  BLI_assert(0);
  return -1;
}

int BKE_mesh_wrapper_poly_len(const Mesh *me)
{
  switch ((eMeshWrapperType)me->runtime.wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      return me->edit_mesh->bm->totface;
    case ME_WRAPPER_TYPE_MDATA:
      return me->totpoly;
  }
  BLI_assert(0);
  return -1;
}

/** \} */
