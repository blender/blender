/* SPDX-FileCopyrightText: 2005 Blender Foundation
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
#include "BLI_math.h"

#include "BKE_DerivedMesh.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.hh"
#include "BKE_lib_id.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_iterators.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_object.h"

#include "DEG_depsgraph_query.h"

BMEditMesh *BKE_editmesh_create(BMesh *bm)
{
  BMEditMesh *em = MEM_cnew<BMEditMesh>(__func__);
  em->bm = bm;
  return em;
}

BMEditMesh *BKE_editmesh_copy(BMEditMesh *em)
{
  BMEditMesh *em_copy = MEM_cnew<BMEditMesh>(__func__);
  *em_copy = *em;

  em_copy->bm = BM_mesh_copy(em->bm);

  /* The tessellation is NOT calculated on the copy here,
   * because currently all the callers of this function use
   * it to make a backup copy of the #BMEditMesh to restore
   * it in the case of errors in an operation. For performance reasons,
   * in that case it makes more sense to do the
   * tessellation only when/if that copy ends up getting used. */
  em_copy->looptris = nullptr;

  /* Copy various settings. */
  em_copy->selectmode = em->selectmode;
  em_copy->mat_nr = em->mat_nr;

  return em_copy;
}

BMEditMesh *BKE_editmesh_from_object(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  /* sanity check */
#if 0 /* disable in multi-object edit. */
#  ifndef NDEBUG
  if (((Mesh *)ob->data)->edit_mesh) {
    BLI_assert(((Mesh *)ob->data)->edit_mesh->ob == ob);
  }
#  endif
#endif
  return ((Mesh *)ob->data)->edit_mesh;
}

static void editmesh_tessface_calc_intern(BMEditMesh *em,
                                          const BMeshCalcTessellation_Params *params)
{
  /* allocating space before calculating the tessellation */

  BMesh *bm = em->bm;

  /* This assumes all faces can be scan-filled, which isn't always true,
   * worst case we over allocate a little which is acceptable. */
  const int looptris_tot = poly_to_tri_count(bm->totface, bm->totloop);
  const int looptris_tot_prev_alloc = em->looptris ?
                                          (MEM_allocN_len(em->looptris) / sizeof(*em->looptris)) :
                                          0;

  BMLoop *(*looptris)[3];

  /* This means no reallocations for quad dominant models. */
  if ((em->looptris != nullptr) &&
      // (*em->tottri >= looptris_tot))
      /* Check against allocated size in case we over allocated a little. */
      ((looptris_tot_prev_alloc >= looptris_tot) && (looptris_tot_prev_alloc <= looptris_tot * 2)))
  {
    looptris = em->looptris;
  }
  else {
    if (em->looptris) {
      MEM_freeN(em->looptris);
    }
    looptris = static_cast<BMLoop *(*)[3]>(
        MEM_mallocN(sizeof(*looptris) * looptris_tot, __func__));
  }

  em->looptris = looptris;
  em->tottri = looptris_tot;

  /* after allocating the em->looptris, we're ready to tessellate */
  BM_mesh_calc_tessellation_ex(em->bm, em->looptris, params);
}

void BKE_editmesh_looptri_calc_ex(BMEditMesh *em, const BMeshCalcTessellation_Params *params)
{
  editmesh_tessface_calc_intern(em, params);
}

void BKE_editmesh_looptri_calc(BMEditMesh *em)
{
  BMeshCalcTessellation_Params params{};
  params.face_normals = false;
  BKE_editmesh_looptri_calc_ex(em, &params);
}

void BKE_editmesh_looptri_and_normals_calc(BMEditMesh *em)
{
  BMeshCalcTessellation_Params looptri_params{};
  looptri_params.face_normals = true;
  BKE_editmesh_looptri_calc_ex(em, &looptri_params);
  BMeshNormalsUpdate_Params normals_params{};
  normals_params.face_normals = false;
  BM_mesh_normals_update_ex(em->bm, &normals_params);
}

void BKE_editmesh_looptri_calc_with_partial_ex(BMEditMesh *em,
                                               BMPartialUpdate *bmpinfo,
                                               const BMeshCalcTessellation_Params *params)
{
  BLI_assert(em->tottri == poly_to_tri_count(em->bm->totface, em->bm->totloop));
  BLI_assert(em->looptris != nullptr);

  BM_mesh_calc_tessellation_with_partial_ex(em->bm, em->looptris, bmpinfo, params);
}

void BKE_editmesh_looptri_calc_with_partial(BMEditMesh *em, BMPartialUpdate *bmpinfo)
{
  BMeshCalcTessellation_Params looptri_params{};
  looptri_params.face_normals = false;
  BKE_editmesh_looptri_calc_with_partial_ex(em, bmpinfo, &looptri_params);
}

void BKE_editmesh_looptri_and_normals_calc_with_partial(BMEditMesh *em, BMPartialUpdate *bmpinfo)
{
  BMeshCalcTessellation_Params looptri_params{};
  looptri_params.face_normals = true;
  BKE_editmesh_looptri_calc_with_partial_ex(em, bmpinfo, &looptri_params);
  BMeshNormalsUpdate_Params normals_params{};
  normals_params.face_normals = false;
  BM_mesh_normals_update_with_partial_ex(em->bm, bmpinfo, &normals_params);
}

void BKE_editmesh_free_data(BMEditMesh *em)
{

  if (em->looptris) {
    MEM_freeN(em->looptris);
  }

  if (em->bm) {
    BM_mesh_free(em->bm);
  }
}

struct CageUserData {
  int totvert;
  float (*cos_cage)[3];
  BLI_bitmap *visit_bitmap;
};

static void cage_mapped_verts_callback(void *userData,
                                       int index,
                                       const float co[3],
                                       const float /*no*/[3])
{
  CageUserData *data = static_cast<CageUserData *>(userData);

  if ((index >= 0 && index < data->totvert) && !BLI_BITMAP_TEST(data->visit_bitmap, index)) {
    BLI_BITMAP_ENABLE(data->visit_bitmap, index);
    copy_v3_v3(data->cos_cage[index], co);
  }
}

float (*BKE_editmesh_vert_coords_alloc(
    Depsgraph *depsgraph, BMEditMesh *em, Scene *scene, Object *ob, int *r_vert_len))[3]
{
  Mesh *cage = editbmesh_get_eval_cage(depsgraph, scene, ob, em, &CD_MASK_BAREMESH);
  float(*cos_cage)[3] = static_cast<float(*)[3]>(
      MEM_callocN(sizeof(*cos_cage) * em->bm->totvert, __func__));

  /* When initializing cage verts, we only want the first cage coordinate for each vertex,
   * so that e.g. mirror or array use original vertex coordinates and not mirrored or duplicate. */
  BLI_bitmap *visit_bitmap = BLI_BITMAP_NEW(em->bm->totvert, __func__);

  CageUserData data;
  data.totvert = em->bm->totvert;
  data.cos_cage = cos_cage;
  data.visit_bitmap = visit_bitmap;

  BKE_mesh_foreach_mapped_vert(cage, cage_mapped_verts_callback, &data, MESH_FOREACH_NOP);

  MEM_freeN(visit_bitmap);

  if (r_vert_len) {
    *r_vert_len = em->bm->totvert;
  }

  return cos_cage;
}

const float (*BKE_editmesh_vert_coords_when_deformed(Depsgraph *depsgraph,
                                                     BMEditMesh *em,
                                                     Scene *scene,
                                                     Object *ob,
                                                     int *r_vert_len,
                                                     bool *r_is_alloc))[3]
{
  const float(*coords)[3] = nullptr;
  *r_is_alloc = false;

  Mesh *me = static_cast<Mesh *>(ob->data);
  Object *object_eval = DEG_get_evaluated_object(depsgraph, ob);
  Mesh *editmesh_eval_final = BKE_object_get_editmesh_eval_final(object_eval);

  if (BKE_mesh_wrapper_vert_coords(me) != nullptr) {
    /* Deformed, and we have deformed coords already. */
    coords = BKE_mesh_wrapper_vert_coords(me);
  }
  else if ((editmesh_eval_final != nullptr) &&
           (editmesh_eval_final->runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH))
  {
    /* If this is an edit-mesh type, leave nullptr as we can use the vertex coords. */
  }
  else {
    /* Constructive modifiers have been used, we need to allocate coordinates. */
    *r_is_alloc = true;
    coords = BKE_editmesh_vert_coords_alloc(depsgraph, em, scene, ob, r_vert_len);
  }
  return coords;
}

float (*BKE_editmesh_vert_coords_alloc_orco(BMEditMesh *em, int *r_vert_len))[3]
{
  return BM_mesh_vert_coords_alloc(em->bm, r_vert_len);
}

void BKE_editmesh_lnorspace_update(BMEditMesh *em, Mesh *me)
{
  BMesh *bm = em->bm;

  /* We need to create custom-loop-normals (CLNORS) data if none exist yet,
   * otherwise there is no way to edit them.
   * Similar code to #MESH_OT_customdata_custom_splitnormals_add operator,
   * we want to keep same shading in case we were using auto-smooth so far.
   * NOTE: there is a problem here, which is that if someone starts a normal editing operation on
   * previously auto-smooth-ed mesh, and cancel that operation, generated CLNORS data remain,
   * with related sharp edges (and hence auto-smooth is 'lost').
   * Not sure how critical this is, and how to fix that issue? */
  if (!CustomData_has_layer(&bm->ldata, CD_CUSTOMLOOPNORMAL)) {
    if (me->flag & ME_AUTOSMOOTH) {
      BM_edges_sharp_from_angle_set(bm, me->smoothresh);
    }
  }

  BM_lnorspace_update(bm);
}

void BKE_editmesh_ensure_autosmooth(BMEditMesh *em, Mesh *me)
{
  if (!(me->flag & ME_AUTOSMOOTH)) {
    me->flag |= ME_AUTOSMOOTH;
    BKE_editmesh_lnorspace_update(em, me);
  }
}

BoundBox *BKE_editmesh_cage_boundbox_get(Object *object, BMEditMesh * /*em*/)
{
  if (object->runtime.editmesh_bb_cage == nullptr) {
    float min[3], max[3];
    INIT_MINMAX(min, max);
    if (object->runtime.editmesh_eval_cage) {
      BKE_mesh_wrapper_minmax(object->runtime.editmesh_eval_cage, min, max);
    }

    object->runtime.editmesh_bb_cage = MEM_cnew<BoundBox>("BMEditMesh.bb_cage");
    BKE_boundbox_init_from_minmax(object->runtime.editmesh_bb_cage, min, max);
  }

  return object->runtime.editmesh_bb_cage;
}
