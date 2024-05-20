/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BLI_bitmap.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"

#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_iterators.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"

#include "DEG_depsgraph_query.hh"

using blender::Array;
using blender::float3;
using blender::Span;

BMEditMesh *BKE_editmesh_create(BMesh *bm)
{
  BMEditMesh *em = MEM_new<BMEditMesh>(__func__);
  em->bm = bm;
  return em;
}

BMEditMesh *BKE_editmesh_copy(BMEditMesh *em)
{
  BMEditMesh *em_copy = MEM_new<BMEditMesh>(__func__);
  *em_copy = *em;

  em_copy->bm = BM_mesh_copy(em->bm);

  /* The tessellation is NOT calculated on the copy here,
   * because currently all the callers of this function use
   * it to make a backup copy of the #BMEditMesh to restore
   * it in the case of errors in an operation. For performance reasons,
   * in that case it makes more sense to do the
   * tessellation only when/if that copy ends up getting used. */
  em_copy->looptris = {};

  /* Copy various settings. */
  em_copy->selectmode = em->selectmode;
  em_copy->mat_nr = em->mat_nr;

  return em_copy;
}

BMEditMesh *BKE_editmesh_from_object(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return ((Mesh *)ob->data)->runtime->edit_mesh.get();
}

void BKE_editmesh_looptris_calc_ex(BMEditMesh *em, const BMeshCalcTessellation_Params *params)
{
  BMesh *bm = em->bm;
  em->looptris.reinitialize(poly_to_tri_count(bm->totface, bm->totloop));
  BM_mesh_calc_tessellation_ex(em->bm, em->looptris, params);
}

void BKE_editmesh_looptris_calc(BMEditMesh *em)
{
  BMeshCalcTessellation_Params params{};
  params.face_normals = false;
  BKE_editmesh_looptris_calc_ex(em, &params);
}

void BKE_editmesh_looptris_and_normals_calc(BMEditMesh *em)
{
  BMeshCalcTessellation_Params looptris_params{};
  looptris_params.face_normals = true;
  BKE_editmesh_looptris_calc_ex(em, &looptris_params);
  BMeshNormalsUpdate_Params normals_params{};
  normals_params.face_normals = false;
  BM_mesh_normals_update_ex(em->bm, &normals_params);
}

void BKE_editmesh_looptris_calc_with_partial_ex(BMEditMesh *em,
                                                BMPartialUpdate *bmpinfo,
                                                const BMeshCalcTessellation_Params *params)
{
  BLI_assert(em->looptris.size() == poly_to_tri_count(em->bm->totface, em->bm->totloop));
  BLI_assert(!(em->bm->totface && em->looptris.is_empty()));

  BM_mesh_calc_tessellation_with_partial_ex(em->bm, em->looptris, bmpinfo, params);
}

void BKE_editmesh_looptris_calc_with_partial(BMEditMesh *em, BMPartialUpdate *bmpinfo)
{
  BMeshCalcTessellation_Params looptris_params{};
  looptris_params.face_normals = false;
  BKE_editmesh_looptris_calc_with_partial_ex(em, bmpinfo, &looptris_params);
}

void BKE_editmesh_looptris_and_normals_calc_with_partial(BMEditMesh *em, BMPartialUpdate *bmpinfo)
{
  BMeshCalcTessellation_Params looptris_params{};
  looptris_params.face_normals = true;
  BKE_editmesh_looptris_calc_with_partial_ex(em, bmpinfo, &looptris_params);
  BMeshNormalsUpdate_Params normals_params{};
  normals_params.face_normals = false;
  BM_mesh_normals_update_with_partial_ex(em->bm, bmpinfo, &normals_params);
}

void BKE_editmesh_free_data(BMEditMesh *em)
{
  em->looptris = {};

  if (em->bm) {
    BM_mesh_free(em->bm);
  }
}

struct CageUserData {
  int totvert;
  blender::MutableSpan<float3> positions_cage;
  BLI_bitmap *visit_bitmap;
};

static void cage_mapped_verts_callback(void *user_data,
                                       int index,
                                       const float co[3],
                                       const float /*no*/[3])
{
  CageUserData *data = static_cast<CageUserData *>(user_data);

  if ((index >= 0 && index < data->totvert) && !BLI_BITMAP_TEST(data->visit_bitmap, index)) {
    BLI_BITMAP_ENABLE(data->visit_bitmap, index);
    copy_v3_v3(data->positions_cage[index], co);
  }
}

Array<float3> BKE_editmesh_vert_coords_alloc(Depsgraph *depsgraph,
                                             BMEditMesh *em,
                                             Scene *scene,
                                             Object *ob)
{
  Mesh *cage = blender::bke::editbmesh_get_eval_cage(depsgraph, scene, ob, em, &CD_MASK_BAREMESH);
  Array<float3> positions_cage(em->bm->totvert);

  /* When initializing cage verts, we only want the first cage coordinate for each vertex,
   * so that e.g. mirror or array use original vertex coordinates and not mirrored or duplicate. */
  BLI_bitmap *visit_bitmap = BLI_BITMAP_NEW(em->bm->totvert, __func__);

  CageUserData data;
  data.totvert = em->bm->totvert;
  data.positions_cage = positions_cage;
  data.visit_bitmap = visit_bitmap;

  BKE_mesh_foreach_mapped_vert(cage, cage_mapped_verts_callback, &data, MESH_FOREACH_NOP);

  MEM_freeN(visit_bitmap);

  return positions_cage;
}

Span<float3> BKE_editmesh_vert_coords_when_deformed(
    Depsgraph *depsgraph, BMEditMesh *em, Scene *scene, Object *ob, Array<float3> &r_alloc)
{

  const Object *object_eval = DEG_get_evaluated_object(depsgraph, ob);
  const Mesh *editmesh_eval_final = BKE_object_get_editmesh_eval_final(object_eval);
  const Mesh *mesh_cage = BKE_object_get_editmesh_eval_cage(ob);

  Span<float3> vert_positions;
  if (mesh_cage && mesh_cage->runtime->deformed_only) {
    BLI_assert(BKE_mesh_wrapper_vert_len(mesh_cage) == em->bm->totvert);
    /* Deformed, and we have deformed coords already. */
    vert_positions = BKE_mesh_wrapper_vert_coords(mesh_cage);
  }
  else if ((editmesh_eval_final != nullptr) &&
           (editmesh_eval_final->runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH))
  {
    /* If this is an edit-mesh type, leave nullptr as we can use the vertex coords. */
  }
  else {
    /* Constructive modifiers have been used, we need to allocate coordinates. */
    r_alloc = BKE_editmesh_vert_coords_alloc(depsgraph, em, scene, ob);
    return r_alloc.as_span();
  }
  return vert_positions;
}

Array<float3> BKE_editmesh_vert_coords_alloc_orco(BMEditMesh *em)
{
  return BM_mesh_vert_coords_alloc(em->bm);
}

void BKE_editmesh_lnorspace_update(BMEditMesh *em)
{
  BM_lnorspace_update(em->bm);
}
