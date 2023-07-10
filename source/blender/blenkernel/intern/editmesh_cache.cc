/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Manage edit mesh cache: #EditMeshData
 */

#include "MEM_guardedalloc.h"

#include "BLI_bounds.hh"
#include "BLI_math_vector.h"
#include "BLI_span.hh"

#include "DNA_mesh_types.h"

#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name Ensure Data (derived from coords)
 * \{ */

void BKE_editmesh_cache_ensure_poly_normals(BMEditMesh *em, EditMeshData *emd)
{
  if (!(emd->vertexCos && (emd->polyNos == nullptr))) {
    return;
  }

  BMesh *bm = em->bm;
  BMFace *efa;
  BMIter fiter;
  int i;

  BM_mesh_elem_index_ensure(bm, BM_VERT);

  float(*polyNos)[3] = static_cast<float(*)[3]>(
      MEM_mallocN(sizeof(*polyNos) * bm->totface, __func__));

  const float(*vertexCos)[3] = emd->vertexCos;

  BM_ITER_MESH_INDEX (efa, &fiter, bm, BM_FACES_OF_MESH, i) {
    BM_elem_index_set(efa, i); /* set_inline */
    BM_face_calc_normal_vcos(bm, efa, polyNos[i], vertexCos);
  }
  bm->elem_index_dirty &= ~BM_FACE;

  emd->polyNos = (const float(*)[3])polyNos;
}

void BKE_editmesh_cache_ensure_vert_normals(BMEditMesh *em, EditMeshData *emd)
{
  if (!(emd->vertexCos && (emd->vertexNos == nullptr))) {
    return;
  }

  BMesh *bm = em->bm;
  const float(*vertexCos)[3], (*polyNos)[3];
  float(*vertexNos)[3];

  /* Calculate vertex normals from poly normals. */
  BKE_editmesh_cache_ensure_poly_normals(em, emd);

  BM_mesh_elem_index_ensure(bm, BM_FACE);

  polyNos = emd->polyNos;
  vertexCos = emd->vertexCos;
  vertexNos = static_cast<float(*)[3]>(MEM_callocN(sizeof(*vertexNos) * bm->totvert, __func__));

  BM_verts_calc_normal_vcos(bm, polyNos, vertexCos, vertexNos);

  emd->vertexNos = (const float(*)[3])vertexNos;
}

void BKE_editmesh_cache_ensure_poly_centers(BMEditMesh *em, EditMeshData *emd)
{
  if (emd->polyCos != nullptr) {
    return;
  }
  BMesh *bm = em->bm;

  BMFace *efa;
  BMIter fiter;
  int i;

  float(*polyCos)[3] = static_cast<float(*)[3]>(
      MEM_mallocN(sizeof(*polyCos) * bm->totface, __func__));

  if (emd->vertexCos) {
    const float(*vertexCos)[3];
    vertexCos = emd->vertexCos;

    BM_mesh_elem_index_ensure(bm, BM_VERT);

    BM_ITER_MESH_INDEX (efa, &fiter, bm, BM_FACES_OF_MESH, i) {
      BM_face_calc_center_median_vcos(bm, efa, polyCos[i], vertexCos);
    }
  }
  else {
    BM_ITER_MESH_INDEX (efa, &fiter, bm, BM_FACES_OF_MESH, i) {
      BM_face_calc_center_median(efa, polyCos[i]);
    }
  }

  emd->polyCos = (const float(*)[3])polyCos;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Calculate Min/Max
 * \{ */

bool BKE_editmesh_cache_calc_minmax(BMEditMesh *em, EditMeshData *emd, float min[3], float max[3])
{
  using namespace blender;
  BMesh *bm = em->bm;

  if (bm->totvert) {
    if (emd->vertexCos) {
      Span<float3> vert_coords(reinterpret_cast<const float3 *>(emd->vertexCos), bm->totvert);
      std::optional<Bounds<float3>> bounds = bounds::min_max(vert_coords);
      BLI_assert(bounds.has_value());
      copy_v3_v3(min, math::min(bounds->min, float3(min)));
      copy_v3_v3(max, math::max(bounds->max, float3(max)));
    }
    else {
      BMVert *eve;
      BMIter iter;
      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        minmax_v3v3_v3(min, max, eve->co);
      }
    }
    return true;
  }

  zero_v3(min);
  zero_v3(max);
  return false;
}

/** \} */
