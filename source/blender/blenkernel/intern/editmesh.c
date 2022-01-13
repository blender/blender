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

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"

#include "BKE_DerivedMesh.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_iterators.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_object.h"

BMEditMesh *BKE_editmesh_create(BMesh *bm)
{
  BMEditMesh *em = MEM_callocN(sizeof(BMEditMesh), __func__);
  em->bm = bm;
  return em;
}

BMEditMesh *BKE_editmesh_copy(BMEditMesh *em)
{
  BMEditMesh *em_copy = MEM_callocN(sizeof(BMEditMesh), __func__);
  *em_copy = *em;

  em_copy->mesh_eval_cage = em_copy->mesh_eval_final = NULL;
  em_copy->bb_cage = NULL;

  em_copy->bm = BM_mesh_copy(em->bm);

  /* The tessellation is NOT calculated on the copy here,
   * because currently all the callers of this function use
   * it to make a backup copy of the #BMEditMesh to restore
   * it in the case of errors in an operation. For performance reasons,
   * in that case it makes more sense to do the
   * tessellation only when/if that copy ends up getting used. */
  em_copy->looptris = NULL;

  /* Copy various settings. */
  em_copy->selectmode = em->selectmode;
  em_copy->mat_nr = em->mat_nr;

  return em_copy;
}

BMEditMesh *BKE_editmesh_from_object(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  /* sanity check */
#if 0 /* disable in mutlti-object edit. */
#  ifndef NDEBUG
  if (((Mesh *)ob->data)->edit_mesh) {
    BLI_assert(((Mesh *)ob->data)->edit_mesh->ob == ob);
  }
#  endif
#endif
  return ((Mesh *)ob->data)->edit_mesh;
}

static void editmesh_tessface_calc_intern(BMEditMesh *em,
                                          const struct BMeshCalcTessellation_Params *params)
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

  /* this means no reallocs for quad dominant models, for */
  if ((em->looptris != NULL) &&
      /* (*em->tottri >= looptris_tot)) */
      /* Check against allocated size in case we over allocated a little. */
      ((looptris_tot_prev_alloc >= looptris_tot) &&
       (looptris_tot_prev_alloc <= looptris_tot * 2))) {
    looptris = em->looptris;
  }
  else {
    if (em->looptris) {
      MEM_freeN(em->looptris);
    }
    looptris = MEM_mallocN(sizeof(*looptris) * looptris_tot, __func__);
  }

  em->looptris = looptris;
  em->tottri = looptris_tot;

  /* after allocating the em->looptris, we're ready to tessellate */
  BM_mesh_calc_tessellation_ex(em->bm, em->looptris, params);
}

void BKE_editmesh_looptri_calc_ex(BMEditMesh *em,
                                  const struct BMeshCalcTessellation_Params *params)
{
  editmesh_tessface_calc_intern(em, params);

  /* commented because editbmesh_build_data() ensures we get tessfaces */
#if 0
  if (em->mesh_eval_final && em->mesh_eval_final == em->mesh_eval_cage) {
    BKE_mesh_runtime_looptri_ensure(em->mesh_eval_final);
  }
  else if (em->mesh_eval_final) {
    BKE_mesh_runtime_looptri_ensure(em->mesh_eval_final);
    BKE_mesh_runtime_looptri_ensure(em->mesh_eval_cage);
  }
#endif
}

void BKE_editmesh_looptri_calc(BMEditMesh *em)
{
  BKE_editmesh_looptri_calc_ex(em,
                               &(const struct BMeshCalcTessellation_Params){
                                   .face_normals = false,
                               });
}

void BKE_editmesh_looptri_and_normals_calc(BMEditMesh *em)
{
  BKE_editmesh_looptri_calc_ex(em,
                               &(const struct BMeshCalcTessellation_Params){
                                   .face_normals = true,
                               });
  BM_mesh_normals_update_ex(em->bm,
                            &(const struct BMeshNormalsUpdate_Params){
                                .face_normals = false,
                            });
}

void BKE_editmesh_looptri_calc_with_partial_ex(BMEditMesh *em,
                                               struct BMPartialUpdate *bmpinfo,
                                               const struct BMeshCalcTessellation_Params *params)
{
  BLI_assert(em->tottri == poly_to_tri_count(em->bm->totface, em->bm->totloop));
  BLI_assert(em->looptris != NULL);

  BM_mesh_calc_tessellation_with_partial_ex(em->bm, em->looptris, bmpinfo, params);
}

void BKE_editmesh_looptri_calc_with_partial(BMEditMesh *em, struct BMPartialUpdate *bmpinfo)
{
  BKE_editmesh_looptri_calc_with_partial_ex(em,
                                            bmpinfo,
                                            &(const struct BMeshCalcTessellation_Params){
                                                .face_normals = false,
                                            });
}

void BKE_editmesh_looptri_and_normals_calc_with_partial(BMEditMesh *em,
                                                        struct BMPartialUpdate *bmpinfo)
{
  BKE_editmesh_looptri_calc_with_partial_ex(em,
                                            bmpinfo,
                                            &(const struct BMeshCalcTessellation_Params){
                                                .face_normals = true,
                                            });
  BM_mesh_normals_update_with_partial_ex(em->bm,
                                         bmpinfo,
                                         &(const struct BMeshNormalsUpdate_Params){
                                             .face_normals = false,
                                         });
}

void BKE_editmesh_free_derived_caches(BMEditMesh *em)
{
  if (em->mesh_eval_cage) {
    BKE_id_free(NULL, em->mesh_eval_cage);
  }
  if (em->mesh_eval_final && em->mesh_eval_final != em->mesh_eval_cage) {
    BKE_id_free(NULL, em->mesh_eval_final);
  }
  em->mesh_eval_cage = em->mesh_eval_final = NULL;

  MEM_SAFE_FREE(em->bb_cage);
}

void BKE_editmesh_free_data(BMEditMesh *em)
{
  BKE_editmesh_free_derived_caches(em);

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
                                       const float UNUSED(no[3]))
{
  struct CageUserData *data = userData;

  if ((index >= 0 && index < data->totvert) && (!BLI_BITMAP_TEST(data->visit_bitmap, index))) {
    BLI_BITMAP_ENABLE(data->visit_bitmap, index);
    copy_v3_v3(data->cos_cage[index], co);
  }
}

float (*BKE_editmesh_vert_coords_alloc(struct Depsgraph *depsgraph,
                                       BMEditMesh *em,
                                       struct Scene *scene,
                                       Object *ob,
                                       int *r_vert_len))[3]
{
  Mesh *cage;
  BLI_bitmap *visit_bitmap;
  struct CageUserData data;
  float(*cos_cage)[3];

  cage = editbmesh_get_eval_cage(depsgraph, scene, ob, em, &CD_MASK_BAREMESH);
  cos_cage = MEM_callocN(sizeof(*cos_cage) * em->bm->totvert, "bmbvh cos_cage");

  /* when initializing cage verts, we only want the first cage coordinate for each vertex,
   * so that e.g. mirror or array use original vertex coordinates and not mirrored or duplicate */
  visit_bitmap = BLI_BITMAP_NEW(em->bm->totvert, __func__);

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

const float (*BKE_editmesh_vert_coords_when_deformed(struct Depsgraph *depsgraph,
                                                     BMEditMesh *em,
                                                     struct Scene *scene,
                                                     Object *ob,
                                                     int *r_vert_len,
                                                     bool *r_is_alloc))[3]
{
  const float(*coords)[3] = NULL;
  *r_is_alloc = false;

  Mesh *me = ob->data;

  if ((me->runtime.edit_data != NULL) && (me->runtime.edit_data->vertexCos != NULL)) {
    /* Deformed, and we have deformed coords already. */
    coords = me->runtime.edit_data->vertexCos;
  }
  else if ((em->mesh_eval_final != NULL) &&
           (em->mesh_eval_final->runtime.wrapper_type == ME_WRAPPER_TYPE_BMESH)) {
    /* If this is an edit-mesh type, leave NULL as we can use the vertex coords. */
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

BoundBox *BKE_editmesh_cage_boundbox_get(BMEditMesh *em)
{
  if (em->bb_cage == NULL) {
    float min[3], max[3];
    INIT_MINMAX(min, max);
    if (em->mesh_eval_cage) {
      BKE_mesh_wrapper_minmax(em->mesh_eval_cage, min, max);
    }

    em->bb_cage = MEM_callocN(sizeof(BoundBox), "BMEditMesh.bb_cage");
    BKE_boundbox_init_from_minmax(em->bb_cage, min, max);
  }

  return em->bb_cage;
}
