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
 * \ingroup bmesh
 *
 * BM mesh conversion functions.
 *
 * \section bm_mesh_conv_shapekey Converting Shape Keys
 *
 * When converting to/from a Mesh/BMesh you can optionally pass a shape key to edit.
 * This has the effect of editing the shape key-block rather then the original mesh vertex coords
 * (although additional geometry is still allowed and uses fallback locations on converting).
 *
 * While this works for any mesh/bmesh this is made use of by entering and exiting edit-mode.
 *
 * There are comments in code but this should help explain the general
 * intention as to how this works converting from/to bmesh.
 * \subsection user_pov User Perspective
 *
 * - Editmode operations when a shape key-block is active edits only that key-block.
 * - The first Basis key-block always matches the Mesh verts.
 * - Changing vertex locations of _any_ Basis
 *   will apply offsets to those shape keys using this as their Basis.
 *
 * \subsection enter_editmode Entering EditMode - #BM_mesh_bm_from_me
 *
 * - The active key-block is used for BMesh vertex locations on entering edit-mode.
 *   So obviously the meshes vertex locations remain unchanged and the shape key
 *   its self is not being edited directly.
 *   Simply the #BMVert.co is a initialized from active shape key (when its set).
 * - All key-blocks are added as CustomData layers (read code for details).
 *
 * \subsection exit_editmode Exiting EditMode - #BM_mesh_bm_to_me
 *
 * This is where the most confusing code is! Won't attempt to document the details here,
 * for that read the code.
 * But basics are as follows.
 *
 * - Vertex locations (possibly modified from initial active key-block)
 *   are copied directly into #MVert.co
 *   (special confusing note that these may be restored later, when editing the 'Basis', read on).
 * - if the 'Key' is relative, and the active key-block is the basis for ANY other key-blocks -
 *   get an array of offsets between the new vertex locations and the original shape key
 *   (before entering edit-mode), these offsets get applied later on to inactive key-blocks
 *   using the active one (which we are editing) as their Basis.
 *
 * Copying the locations back to the shape keys is quite confusing...
 * One main area of confusion is that when editing a 'Basis' key-block 'me->key->refkey'
 * The coords are written into the mesh, from the users perspective the Basis coords are written
 * into the mesh when exiting edit-mode.
 *
 * When _not_ editing the 'Basis', the original vertex locations
 * (stored in the mesh and unchanged during edit-mode), are copied back into the mesh.
 *
 * This has the effect from the users POV of leaving the mesh un-touched,
 * and only editing the active shape key-block.
 */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_key_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_alloca.h"
#include "BLI_math_vector.h"

#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_customdata.h"
#include "BKE_multires.h"

#include "BKE_main.h"
#include "BKE_key.h"

#include "bmesh.h"
#include "intern/bmesh_private.h" /* for element checking */

void BM_mesh_cd_flag_ensure(BMesh *bm, Mesh *mesh, const char cd_flag)
{
  const char cd_flag_all = BM_mesh_cd_flag_from_bmesh(bm) | cd_flag;
  BM_mesh_cd_flag_apply(bm, cd_flag_all);
  if (mesh) {
    mesh->cd_flag = cd_flag_all;
  }
}

void BM_mesh_cd_flag_apply(BMesh *bm, const char cd_flag)
{
  /* CustomData_bmesh_init_pool() must run first */
  BLI_assert(bm->vdata.totlayer == 0 || bm->vdata.pool != NULL);
  BLI_assert(bm->edata.totlayer == 0 || bm->edata.pool != NULL);
  BLI_assert(bm->pdata.totlayer == 0 || bm->pdata.pool != NULL);

  if (cd_flag & ME_CDFLAG_VERT_BWEIGHT) {
    if (!CustomData_has_layer(&bm->vdata, CD_BWEIGHT)) {
      BM_data_layer_add(bm, &bm->vdata, CD_BWEIGHT);
    }
  }
  else {
    if (CustomData_has_layer(&bm->vdata, CD_BWEIGHT)) {
      BM_data_layer_free(bm, &bm->vdata, CD_BWEIGHT);
    }
  }

  if (cd_flag & ME_CDFLAG_EDGE_BWEIGHT) {
    if (!CustomData_has_layer(&bm->edata, CD_BWEIGHT)) {
      BM_data_layer_add(bm, &bm->edata, CD_BWEIGHT);
    }
  }
  else {
    if (CustomData_has_layer(&bm->edata, CD_BWEIGHT)) {
      BM_data_layer_free(bm, &bm->edata, CD_BWEIGHT);
    }
  }

  if (cd_flag & ME_CDFLAG_EDGE_CREASE) {
    if (!CustomData_has_layer(&bm->edata, CD_CREASE)) {
      BM_data_layer_add(bm, &bm->edata, CD_CREASE);
    }
  }
  else {
    if (CustomData_has_layer(&bm->edata, CD_CREASE)) {
      BM_data_layer_free(bm, &bm->edata, CD_CREASE);
    }
  }
}

char BM_mesh_cd_flag_from_bmesh(BMesh *bm)
{
  char cd_flag = 0;
  if (CustomData_has_layer(&bm->vdata, CD_BWEIGHT)) {
    cd_flag |= ME_CDFLAG_VERT_BWEIGHT;
  }
  if (CustomData_has_layer(&bm->edata, CD_BWEIGHT)) {
    cd_flag |= ME_CDFLAG_EDGE_BWEIGHT;
  }
  if (CustomData_has_layer(&bm->edata, CD_CREASE)) {
    cd_flag |= ME_CDFLAG_EDGE_CREASE;
  }
  return cd_flag;
}

/* Static function for alloc (duplicate in modifiers_bmesh.c) */
static BMFace *bm_face_create_from_mpoly(
    MPoly *mp, MLoop *ml, BMesh *bm, BMVert **vtable, BMEdge **etable)
{
  BMVert **verts = BLI_array_alloca(verts, mp->totloop);
  BMEdge **edges = BLI_array_alloca(edges, mp->totloop);
  int j;

  for (j = 0; j < mp->totloop; j++, ml++) {
    verts[j] = vtable[ml->v];
    edges[j] = etable[ml->e];
  }

  return BM_face_create(bm, verts, edges, mp->totloop, NULL, BM_CREATE_SKIP_CD);
}

/**
 * \brief Mesh -> BMesh
 * \param bm: The mesh to write into, while this is typically a newly created BMesh,
 * merging into existing data is supported.
 * Note the custom-data layout isn't used.
 * If more comprehensive merging is needed we should move this into a separate function
 * since this should be kept fast for edit-mode switching and storing undo steps.
 *
 * \warning This function doesn't calculate face normals.
 */
void BM_mesh_bm_from_me(BMesh *bm, const Mesh *me, const struct BMeshFromMeshParams *params)
{
  const bool is_new = !(bm->totvert || (bm->vdata.totlayer || bm->edata.totlayer ||
                                        bm->pdata.totlayer || bm->ldata.totlayer));
  MVert *mvert;
  MEdge *medge;
  MLoop *mloop;
  MPoly *mp;
  KeyBlock *actkey, *block;
  BMVert *v, **vtable = NULL;
  BMEdge *e, **etable = NULL;
  BMFace *f, **ftable = NULL;
  float(*keyco)[3] = NULL;
  int totloops, i;
  CustomData_MeshMasks mask = CD_MASK_BMESH;
  CustomData_MeshMasks_update(&mask, &params->cd_mask_extra);

  if (!me || !me->totvert) {
    if (me && is_new) { /*no verts? still copy customdata layout*/
      CustomData_copy(&me->vdata, &bm->vdata, mask.vmask, CD_ASSIGN, 0);
      CustomData_copy(&me->edata, &bm->edata, mask.emask, CD_ASSIGN, 0);
      CustomData_copy(&me->ldata, &bm->ldata, mask.lmask, CD_ASSIGN, 0);
      CustomData_copy(&me->pdata, &bm->pdata, mask.pmask, CD_ASSIGN, 0);

      CustomData_bmesh_init_pool(&bm->vdata, me->totvert, BM_VERT);
      CustomData_bmesh_init_pool(&bm->edata, me->totedge, BM_EDGE);
      CustomData_bmesh_init_pool(&bm->ldata, me->totloop, BM_LOOP);
      CustomData_bmesh_init_pool(&bm->pdata, me->totpoly, BM_FACE);
    }
    return; /* sanity check */
  }

  if (is_new) {
    CustomData_copy(&me->vdata, &bm->vdata, mask.vmask, CD_CALLOC, 0);
    CustomData_copy(&me->edata, &bm->edata, mask.emask, CD_CALLOC, 0);
    CustomData_copy(&me->ldata, &bm->ldata, mask.lmask, CD_CALLOC, 0);
    CustomData_copy(&me->pdata, &bm->pdata, mask.pmask, CD_CALLOC, 0);
  }
  else {
    CustomData_bmesh_merge(&me->vdata, &bm->vdata, mask.vmask, CD_CALLOC, bm, BM_VERT);
    CustomData_bmesh_merge(&me->edata, &bm->edata, mask.emask, CD_CALLOC, bm, BM_EDGE);
    CustomData_bmesh_merge(&me->ldata, &bm->ldata, mask.lmask, CD_CALLOC, bm, BM_LOOP);
    CustomData_bmesh_merge(&me->pdata, &bm->pdata, mask.pmask, CD_CALLOC, bm, BM_FACE);
  }

  /* -------------------------------------------------------------------- */
  /* Shape Key */
  int tot_shape_keys = me->key ? BLI_listbase_count(&me->key->block) : 0;
  if (is_new == false) {
    tot_shape_keys = min_ii(tot_shape_keys, CustomData_number_of_layers(&bm->vdata, CD_SHAPEKEY));
  }
  const float(**shape_key_table)[3] = tot_shape_keys ?
                                          BLI_array_alloca(shape_key_table, tot_shape_keys) :
                                          NULL;

  if ((params->active_shapekey != 0) && (me->key != NULL)) {
    actkey = BLI_findlink(&me->key->block, params->active_shapekey - 1);
  }
  else {
    actkey = NULL;
  }

  if (is_new) {
    if (tot_shape_keys || params->add_key_index) {
      CustomData_add_layer(&bm->vdata, CD_SHAPE_KEYINDEX, CD_ASSIGN, NULL, 0);
    }
  }

  if (tot_shape_keys) {
    if (is_new) {
      /* check if we need to generate unique ids for the shapekeys.
       * this also exists in the file reading code, but is here for
       * a sanity check */
      if (!me->key->uidgen) {
        fprintf(stderr,
                "%s had to generate shape key uid's in a situation we shouldn't need to! "
                "(bmesh internal error)\n",
                __func__);

        me->key->uidgen = 1;
        for (block = me->key->block.first; block; block = block->next) {
          block->uid = me->key->uidgen++;
        }
      }
    }

    if (actkey && actkey->totelem == me->totvert) {
      keyco = params->use_shapekey ? actkey->data : NULL;
      if (is_new) {
        bm->shapenr = params->active_shapekey;
      }
    }

    for (i = 0, block = me->key->block.first; i < tot_shape_keys; block = block->next, i++) {
      if (is_new) {
        CustomData_add_layer_named(&bm->vdata, CD_SHAPEKEY, CD_ASSIGN, NULL, 0, block->name);
        int j = CustomData_get_layer_index_n(&bm->vdata, CD_SHAPEKEY, i);
        bm->vdata.layers[j].uid = block->uid;
      }
      shape_key_table[i] = (const float(*)[3])block->data;
    }
  }

  if (is_new) {
    CustomData_bmesh_init_pool(&bm->vdata, me->totvert, BM_VERT);
    CustomData_bmesh_init_pool(&bm->edata, me->totedge, BM_EDGE);
    CustomData_bmesh_init_pool(&bm->ldata, me->totloop, BM_LOOP);
    CustomData_bmesh_init_pool(&bm->pdata, me->totpoly, BM_FACE);

    BM_mesh_cd_flag_apply(bm, me->cd_flag);
  }

  const int cd_vert_bweight_offset = CustomData_get_offset(&bm->vdata, CD_BWEIGHT);
  const int cd_edge_bweight_offset = CustomData_get_offset(&bm->edata, CD_BWEIGHT);
  const int cd_edge_crease_offset = CustomData_get_offset(&bm->edata, CD_CREASE);
  const int cd_shape_key_offset = me->key ? CustomData_get_offset(&bm->vdata, CD_SHAPEKEY) : -1;
  const int cd_shape_keyindex_offset = is_new && (tot_shape_keys || params->add_key_index) ?
                                           CustomData_get_offset(&bm->vdata, CD_SHAPE_KEYINDEX) :
                                           -1;

  vtable = MEM_mallocN(sizeof(BMVert **) * me->totvert, __func__);

  for (i = 0, mvert = me->mvert; i < me->totvert; i++, mvert++) {
    v = vtable[i] = BM_vert_create(bm, keyco ? keyco[i] : mvert->co, NULL, BM_CREATE_SKIP_CD);
    BM_elem_index_set(v, i); /* set_ok */

    /* transfer flag */
    v->head.hflag = BM_vert_flag_from_mflag(mvert->flag & ~SELECT);

    /* this is necessary for selection counts to work properly */
    if (mvert->flag & SELECT) {
      BM_vert_select_set(bm, v, true);
    }

    normal_short_to_float_v3(v->no, mvert->no);

    /* Copy Custom Data */
    CustomData_to_bmesh_block(&me->vdata, &bm->vdata, i, &v->head.data, true);

    if (cd_vert_bweight_offset != -1) {
      BM_ELEM_CD_SET_FLOAT(v, cd_vert_bweight_offset, (float)mvert->bweight / 255.0f);
    }

    /* set shape key original index */
    if (cd_shape_keyindex_offset != -1) {
      BM_ELEM_CD_SET_INT(v, cd_shape_keyindex_offset, i);
    }

    /* set shapekey data */
    if (tot_shape_keys) {
      float(*co_dst)[3] = BM_ELEM_CD_GET_VOID_P(v, cd_shape_key_offset);
      for (int j = 0; j < tot_shape_keys; j++, co_dst++) {
        copy_v3_v3(*co_dst, shape_key_table[j][i]);
      }
    }
  }
  if (is_new) {
    bm->elem_index_dirty &= ~BM_VERT; /* added in order, clear dirty flag */
  }

  etable = MEM_mallocN(sizeof(BMEdge **) * me->totedge, __func__);

  medge = me->medge;
  for (i = 0; i < me->totedge; i++, medge++) {
    e = etable[i] = BM_edge_create(
        bm, vtable[medge->v1], vtable[medge->v2], NULL, BM_CREATE_SKIP_CD);
    BM_elem_index_set(e, i); /* set_ok */

    /* transfer flags */
    e->head.hflag = BM_edge_flag_from_mflag(medge->flag & ~SELECT);

    /* this is necessary for selection counts to work properly */
    if (medge->flag & SELECT) {
      BM_edge_select_set(bm, e, true);
    }

    /* Copy Custom Data */
    CustomData_to_bmesh_block(&me->edata, &bm->edata, i, &e->head.data, true);

    if (cd_edge_bweight_offset != -1) {
      BM_ELEM_CD_SET_FLOAT(e, cd_edge_bweight_offset, (float)medge->bweight / 255.0f);
    }
    if (cd_edge_crease_offset != -1) {
      BM_ELEM_CD_SET_FLOAT(e, cd_edge_crease_offset, (float)medge->crease / 255.0f);
    }
  }
  if (is_new) {
    bm->elem_index_dirty &= ~BM_EDGE; /* added in order, clear dirty flag */
  }

  /* only needed for selection. */
  if (me->mselect && me->totselect != 0) {
    ftable = MEM_mallocN(sizeof(BMFace **) * me->totpoly, __func__);
  }

  mloop = me->mloop;
  mp = me->mpoly;
  for (i = 0, totloops = 0; i < me->totpoly; i++, mp++) {
    BMLoop *l_iter;
    BMLoop *l_first;

    f = bm_face_create_from_mpoly(mp, mloop + mp->loopstart, bm, vtable, etable);
    if (ftable != NULL) {
      ftable[i] = f;
    }

    if (UNLIKELY(f == NULL)) {
      printf(
          "%s: Warning! Bad face in mesh"
          " \"%s\" at index %d!, skipping\n",
          __func__,
          me->id.name + 2,
          i);
      continue;
    }

    /* don't use 'i' since we may have skipped the face */
    BM_elem_index_set(f, bm->totface - 1); /* set_ok */

    /* transfer flag */
    f->head.hflag = BM_face_flag_from_mflag(mp->flag & ~ME_FACE_SEL);

    /* this is necessary for selection counts to work properly */
    if (mp->flag & ME_FACE_SEL) {
      BM_face_select_set(bm, f, true);
    }

    f->mat_nr = mp->mat_nr;
    if (i == me->act_face) {
      bm->act_face = f;
    }

    int j = mp->loopstart;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      /* don't use 'j' since we may have skipped some faces, hence some loops. */
      BM_elem_index_set(l_iter, totloops++); /* set_ok */

      /* Save index of correspsonding MLoop */
      CustomData_to_bmesh_block(&me->ldata, &bm->ldata, j++, &l_iter->head.data, true);
    } while ((l_iter = l_iter->next) != l_first);

    /* Copy Custom Data */
    CustomData_to_bmesh_block(&me->pdata, &bm->pdata, i, &f->head.data, true);

    if (params->calc_face_normal) {
      BM_face_normal_update(f);
    }
  }
  if (is_new) {
    bm->elem_index_dirty &= ~(BM_FACE | BM_LOOP); /* added in order, clear dirty flag */
  }

  /* -------------------------------------------------------------------- */
  /* MSelect clears the array elements (avoid adding multiple times).
   *
   * Take care to keep this last and not use (v/e/ftable) after this.
   */

  if (me->mselect && me->totselect != 0) {
    MSelect *msel;
    for (i = 0, msel = me->mselect; i < me->totselect; i++, msel++) {
      BMElem **ele_p;
      switch (msel->type) {
        case ME_VSEL:
          ele_p = (BMElem **)&vtable[msel->index];
          break;
        case ME_ESEL:
          ele_p = (BMElem **)&etable[msel->index];
          break;
        case ME_FSEL:
          ele_p = (BMElem **)&ftable[msel->index];
          break;
        default:
          continue;
      }

      if (*ele_p != NULL) {
        BM_select_history_store_notest(bm, *ele_p);
        *ele_p = NULL;
      }
    }
  }
  else {
    BM_select_history_clear(bm);
  }

  MEM_freeN(vtable);
  MEM_freeN(etable);
  if (ftable) {
    MEM_freeN(ftable);
  }
}

/**
 * \brief BMesh -> Mesh
 */
static BMVert **bm_to_mesh_vertex_map(BMesh *bm, int ototvert)
{
  const int cd_shape_keyindex_offset = CustomData_get_offset(&bm->vdata, CD_SHAPE_KEYINDEX);
  BMVert **vertMap = NULL;
  BMVert *eve;
  int i = 0;
  BMIter iter;

  /* caller needs to ensure this */
  BLI_assert(ototvert > 0);

  vertMap = MEM_callocN(sizeof(*vertMap) * ototvert, "vertMap");
  if (cd_shape_keyindex_offset != -1) {
    BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
      const int keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset);
      if ((keyi != ORIGINDEX_NONE) && (keyi < ototvert) &&
          /* not fool-proof, but chances are if we have many verts with the same index,
           * we will want to use the first one,
           * since the second is more likely to be a duplicate. */
          (vertMap[keyi] == NULL)) {
        vertMap[keyi] = eve;
      }
    }
  }
  else {
    BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
      if (i < ototvert) {
        vertMap[i] = eve;
      }
      else {
        break;
      }
    }
  }

  return vertMap;
}

/**
 * returns customdata shapekey index from a keyblock or -1
 * \note could split this out into a more generic function */
static int bm_to_mesh_shape_layer_index_from_kb(BMesh *bm, KeyBlock *currkey)
{
  int i;
  int j = 0;

  for (i = 0; i < bm->vdata.totlayer; i++) {
    if (bm->vdata.layers[i].type == CD_SHAPEKEY) {
      if (currkey->uid == bm->vdata.layers[i].uid) {
        return j;
      }
      j++;
    }
  }
  return -1;
}

BLI_INLINE void bmesh_quick_edgedraw_flag(MEdge *med, BMEdge *e)
{
  /* this is a cheap way to set the edge draw, its not precise and will
   * pick the first 2 faces an edge uses.
   * The dot comparison is a little arbitrary, but set so that a 5 subd
   * IcoSphere won't vanish but subd 6 will (as with pre-bmesh blender) */

  if (/* (med->flag & ME_EDGEDRAW) && */ /* assume to be true */
      (e->l && (e->l != e->l->radial_next)) &&
      (dot_v3v3(e->l->f->no, e->l->radial_next->f->no) > 0.9995f)) {
    med->flag &= ~ME_EDGEDRAW;
  }
  else {
    med->flag |= ME_EDGEDRAW;
  }
}

/**
 *
 * \param bmain: May be NULL in case \a calc_object_remap parameter option is not set.
 */
void BM_mesh_bm_to_me(Main *bmain, BMesh *bm, Mesh *me, const struct BMeshToMeshParams *params)
{
  MLoop *mloop;
  MPoly *mpoly;
  MVert *mvert, *oldverts;
  MEdge *med, *medge;
  BMVert *v, *eve;
  BMEdge *e;
  BMFace *f;
  BMIter iter;
  int i, j, ototvert;

  const int cd_vert_bweight_offset = CustomData_get_offset(&bm->vdata, CD_BWEIGHT);
  const int cd_edge_bweight_offset = CustomData_get_offset(&bm->edata, CD_BWEIGHT);
  const int cd_edge_crease_offset = CustomData_get_offset(&bm->edata, CD_CREASE);

  ototvert = me->totvert;

  /* new vertex block */
  if (bm->totvert == 0) {
    mvert = NULL;
  }
  else {
    mvert = MEM_callocN(bm->totvert * sizeof(MVert), "loadeditbMesh vert");
  }

  /* new edge block */
  if (bm->totedge == 0) {
    medge = NULL;
  }
  else {
    medge = MEM_callocN(bm->totedge * sizeof(MEdge), "loadeditbMesh edge");
  }

  /* new ngon face block */
  if (bm->totface == 0) {
    mpoly = NULL;
  }
  else {
    mpoly = MEM_callocN(bm->totface * sizeof(MPoly), "loadeditbMesh poly");
  }

  /* new loop block */
  if (bm->totloop == 0) {
    mloop = NULL;
  }
  else {
    mloop = MEM_callocN(bm->totloop * sizeof(MLoop), "loadeditbMesh loop");
  }

  /* lets save the old verts just in case we are actually working on
   * a key ... we now do processing of the keys at the end */
  oldverts = me->mvert;

  /* don't free this yet */
  if (oldverts) {
    CustomData_set_layer(&me->vdata, CD_MVERT, NULL);
  }

  /* free custom data */
  CustomData_free(&me->vdata, me->totvert);
  CustomData_free(&me->edata, me->totedge);
  CustomData_free(&me->fdata, me->totface);
  CustomData_free(&me->ldata, me->totloop);
  CustomData_free(&me->pdata, me->totpoly);

  /* add new custom data */
  me->totvert = bm->totvert;
  me->totedge = bm->totedge;
  me->totloop = bm->totloop;
  me->totpoly = bm->totface;
  /* will be overwritten with a valid value if 'dotess' is set, otherwise we
   * end up with 'me->totface' and me->mface == NULL which can crash [#28625]
   */
  me->totface = 0;
  me->act_face = -1;

  {
    CustomData_MeshMasks mask = CD_MASK_MESH;
    CustomData_MeshMasks_update(&mask, &params->cd_mask_extra);
    CustomData_copy(&bm->vdata, &me->vdata, mask.vmask, CD_CALLOC, me->totvert);
    CustomData_copy(&bm->edata, &me->edata, mask.emask, CD_CALLOC, me->totedge);
    CustomData_copy(&bm->ldata, &me->ldata, mask.lmask, CD_CALLOC, me->totloop);
    CustomData_copy(&bm->pdata, &me->pdata, mask.pmask, CD_CALLOC, me->totpoly);
  }

  CustomData_add_layer(&me->vdata, CD_MVERT, CD_ASSIGN, mvert, me->totvert);
  CustomData_add_layer(&me->edata, CD_MEDGE, CD_ASSIGN, medge, me->totedge);
  CustomData_add_layer(&me->ldata, CD_MLOOP, CD_ASSIGN, mloop, me->totloop);
  CustomData_add_layer(&me->pdata, CD_MPOLY, CD_ASSIGN, mpoly, me->totpoly);

  me->cd_flag = BM_mesh_cd_flag_from_bmesh(bm);

  /* this is called again, 'dotess' arg is used there */
  BKE_mesh_update_customdata_pointers(me, 0);

  i = 0;
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    copy_v3_v3(mvert->co, v->co);
    normal_float_to_short_v3(mvert->no, v->no);

    mvert->flag = BM_vert_flag_to_mflag(v);

    BM_elem_index_set(v, i); /* set_inline */

    /* copy over customdat */
    CustomData_from_bmesh_block(&bm->vdata, &me->vdata, v->head.data, i);

    if (cd_vert_bweight_offset != -1) {
      mvert->bweight = BM_ELEM_CD_GET_FLOAT_AS_UCHAR(v, cd_vert_bweight_offset);
    }

    i++;
    mvert++;

    BM_CHECK_ELEMENT(v);
  }
  bm->elem_index_dirty &= ~BM_VERT;

  med = medge;
  i = 0;
  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    med->v1 = BM_elem_index_get(e->v1);
    med->v2 = BM_elem_index_get(e->v2);

    med->flag = BM_edge_flag_to_mflag(e);

    BM_elem_index_set(e, i); /* set_inline */

    /* copy over customdata */
    CustomData_from_bmesh_block(&bm->edata, &me->edata, e->head.data, i);

    bmesh_quick_edgedraw_flag(med, e);

    if (cd_edge_crease_offset != -1) {
      med->crease = BM_ELEM_CD_GET_FLOAT_AS_UCHAR(e, cd_edge_crease_offset);
    }
    if (cd_edge_bweight_offset != -1) {
      med->bweight = BM_ELEM_CD_GET_FLOAT_AS_UCHAR(e, cd_edge_bweight_offset);
    }

    i++;
    med++;
    BM_CHECK_ELEMENT(e);
  }
  bm->elem_index_dirty &= ~BM_EDGE;

  i = 0;
  j = 0;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    BMLoop *l_iter, *l_first;
    mpoly->loopstart = j;
    mpoly->totloop = f->len;
    mpoly->mat_nr = f->mat_nr;
    mpoly->flag = BM_face_flag_to_mflag(f);

    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      mloop->e = BM_elem_index_get(l_iter->e);
      mloop->v = BM_elem_index_get(l_iter->v);

      /* copy over customdata */
      CustomData_from_bmesh_block(&bm->ldata, &me->ldata, l_iter->head.data, j);

      j++;
      mloop++;
      BM_CHECK_ELEMENT(l_iter);
      BM_CHECK_ELEMENT(l_iter->e);
      BM_CHECK_ELEMENT(l_iter->v);
    } while ((l_iter = l_iter->next) != l_first);

    if (f == bm->act_face) {
      me->act_face = i;
    }

    /* copy over customdata */
    CustomData_from_bmesh_block(&bm->pdata, &me->pdata, f->head.data, i);

    i++;
    mpoly++;
    BM_CHECK_ELEMENT(f);
  }

  /* patch hook indices and vertex parents */
  if (params->calc_object_remap && (ototvert > 0)) {
    BLI_assert(bmain != NULL);
    Object *ob;
    ModifierData *md;
    BMVert **vertMap = NULL;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      if ((ob->parent) && (ob->parent->data == me) && ELEM(ob->partype, PARVERT1, PARVERT3)) {

        if (vertMap == NULL) {
          vertMap = bm_to_mesh_vertex_map(bm, ototvert);
        }

        if (ob->par1 < ototvert) {
          eve = vertMap[ob->par1];
          if (eve) {
            ob->par1 = BM_elem_index_get(eve);
          }
        }
        if (ob->par2 < ototvert) {
          eve = vertMap[ob->par2];
          if (eve) {
            ob->par2 = BM_elem_index_get(eve);
          }
        }
        if (ob->par3 < ototvert) {
          eve = vertMap[ob->par3];
          if (eve) {
            ob->par3 = BM_elem_index_get(eve);
          }
        }
      }
      if (ob->data == me) {
        for (md = ob->modifiers.first; md; md = md->next) {
          if (md->type == eModifierType_Hook) {
            HookModifierData *hmd = (HookModifierData *)md;

            if (vertMap == NULL) {
              vertMap = bm_to_mesh_vertex_map(bm, ototvert);
            }

            for (i = j = 0; i < hmd->totindex; i++) {
              if (hmd->indexar[i] < ototvert) {
                eve = vertMap[hmd->indexar[i]];

                if (eve) {
                  hmd->indexar[j++] = BM_elem_index_get(eve);
                }
              }
              else {
                j++;
              }
            }

            hmd->totindex = j;
          }
        }
      }
    }

    if (vertMap) {
      MEM_freeN(vertMap);
    }
  }

  BKE_mesh_update_customdata_pointers(me, false);

  {
    BMEditSelection *selected;
    me->totselect = BLI_listbase_count(&(bm->selected));

    MEM_SAFE_FREE(me->mselect);
    if (me->totselect != 0) {
      me->mselect = MEM_mallocN(sizeof(MSelect) * me->totselect, "Mesh selection history");
    }

    for (i = 0, selected = bm->selected.first; selected; i++, selected = selected->next) {
      if (selected->htype == BM_VERT) {
        me->mselect[i].type = ME_VSEL;
      }
      else if (selected->htype == BM_EDGE) {
        me->mselect[i].type = ME_ESEL;
      }
      else if (selected->htype == BM_FACE) {
        me->mselect[i].type = ME_FSEL;
      }

      me->mselect[i].index = BM_elem_index_get(selected->ele);
    }
  }

  /* see comment below, this logic is in twice */

  if (me->key) {
    const int cd_shape_keyindex_offset = CustomData_get_offset(&bm->vdata, CD_SHAPE_KEYINDEX);

    KeyBlock *currkey;
    KeyBlock *actkey = BLI_findlink(&me->key->block, bm->shapenr - 1);

    float(*ofs)[3] = NULL;

    /* go through and find any shapekey customdata layers
     * that might not have corresponding KeyBlocks, and add them if
     * necessary */
    j = 0;
    for (i = 0; i < bm->vdata.totlayer; i++) {
      if (bm->vdata.layers[i].type != CD_SHAPEKEY) {
        continue;
      }

      for (currkey = me->key->block.first; currkey; currkey = currkey->next) {
        if (currkey->uid == bm->vdata.layers[i].uid) {
          break;
        }
      }

      if (!currkey) {
        currkey = BKE_keyblock_add(me->key, bm->vdata.layers[i].name);
        currkey->uid = bm->vdata.layers[i].uid;
      }

      j++;
    }

    /* editing the base key should update others */
    if ((me->key->type == KEY_RELATIVE) && /* only need offsets for relative shape keys */
        (actkey != NULL) &&                /* unlikely, but the active key may not be valid if the
                                            * bmesh and the mesh are out of sync */
        (oldverts != NULL)) /* not used here, but 'oldverts' is used later for applying 'ofs' */
    {
      const bool act_is_basis = BKE_keyblock_is_basis(me->key, bm->shapenr - 1);

      /* active key is a base */
      if (act_is_basis && (cd_shape_keyindex_offset != -1)) {
        float(*fp)[3] = actkey->data;

        ofs = MEM_callocN(sizeof(float) * 3 * bm->totvert, "currkey->data");
        mvert = me->mvert;
        BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
          const int keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset);

          if (keyi != ORIGINDEX_NONE) {
            sub_v3_v3v3(ofs[i], mvert->co, fp[keyi]);
          }
          else {
            /* if there are new vertices in the mesh, we can't propagate the offset
             * because it will only work for the existing vertices and not the new
             * ones, creating a mess when doing e.g. subdivide + translate */
            MEM_freeN(ofs);
            ofs = NULL;
            break;
          }

          mvert++;
        }
      }
    }

    for (currkey = me->key->block.first; currkey; currkey = currkey->next) {
      const bool apply_offset = (ofs && (currkey != actkey) &&
                                 (bm->shapenr - 1 == currkey->relative));
      int cd_shape_offset;
      int keyi;
      float(*ofs_pt)[3] = ofs;
      float *newkey, (*oldkey)[3], *fp;

      j = bm_to_mesh_shape_layer_index_from_kb(bm, currkey);
      cd_shape_offset = CustomData_get_n_offset(&bm->vdata, CD_SHAPEKEY, j);

      fp = newkey = MEM_callocN(me->key->elemsize * bm->totvert, "currkey->data");
      oldkey = currkey->data;

      mvert = me->mvert;
      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {

        if (currkey == actkey) {
          copy_v3_v3(fp, eve->co);

          if (actkey != me->key->refkey) { /* important see bug [#30771] */
            if (cd_shape_keyindex_offset != -1) {
              if (oldverts) {
                keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset);
                if (keyi != ORIGINDEX_NONE && keyi < currkey->totelem) { /* valid old vertex */
                  copy_v3_v3(mvert->co, oldverts[keyi].co);
                }
              }
            }
          }
        }
        else if (j != -1) {
          /* in most cases this runs */
          copy_v3_v3(fp, BM_ELEM_CD_GET_VOID_P(eve, cd_shape_offset));
        }
        else if ((oldkey != NULL) && (cd_shape_keyindex_offset != -1) &&
                 ((keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset)) != ORIGINDEX_NONE) &&
                 (keyi < currkey->totelem)) {
          /* old method of reconstructing keys via vertice's original key indices,
           * currently used if the new method above fails (which is theoretically
           * possible in certain cases of undo) */
          copy_v3_v3(fp, oldkey[keyi]);
        }
        else {
          /* fail! fill in with dummy value */
          copy_v3_v3(fp, mvert->co);
        }

        /* propagate edited basis offsets to other shapes */
        if (apply_offset) {
          add_v3_v3(fp, *ofs_pt++);
          /* Apply back new coordinates of offsetted shapekeys into BMesh.
           * Otherwise, in case we call again BM_mesh_bm_to_me on same BMesh,
           * we'll apply diff from previous call to BM_mesh_bm_to_me,
           * to shapekey values from *original creation of the BMesh*. See T50524. */
          copy_v3_v3(BM_ELEM_CD_GET_VOID_P(eve, cd_shape_offset), fp);
        }

        fp += 3;
        mvert++;
      }

      currkey->totelem = bm->totvert;
      if (currkey->data) {
        MEM_freeN(currkey->data);
      }
      currkey->data = newkey;
    }

    if (ofs) {
      MEM_freeN(ofs);
    }
  }

  if (oldverts) {
    MEM_freeN(oldverts);
  }

  /* topology could be changed, ensure mdisps are ok */
  multires_topology_changed(me);

  /* to be removed as soon as COW is enabled by default. */
  BKE_mesh_runtime_clear_geometry(me);
}

/**
 * A version of #BM_mesh_bm_to_me intended for getting the mesh
 * to pass to the modifier stack for evaluation,
 * instad of mode switching (where we make sure all data is kept
 * and do expensive lookups to maintain shape keys).
 *
 * Key differences:
 *
 * - Don't support merging with existing mesh.
 * - Ignore shape-keys.
 * - Ignore vertex-parents.
 * - Ignore selection history.
 * - Uses simpler method to calculate #ME_EDGEDRAW
 * - Uses #CD_MASK_DERIVEDMESH instead of #CD_MASK_MESH.
 *
 * \note Was `cddm_from_bmesh_ex` in 2.7x, removed `MFace` support.
 */
void BM_mesh_bm_to_me_for_eval(BMesh *bm, Mesh *me, const CustomData_MeshMasks *cd_mask_extra)
{
  /* must be an empty mesh. */
  BLI_assert(me->totvert == 0);
  BLI_assert(cd_mask_extra == NULL || (cd_mask_extra->vmask & CD_MASK_SHAPEKEY) == 0);

  me->totvert = bm->totvert;
  me->totedge = bm->totedge;
  me->totface = 0;
  me->totloop = bm->totloop;
  me->totpoly = bm->totface;

  CustomData_add_layer(&me->vdata, CD_ORIGINDEX, CD_CALLOC, NULL, bm->totvert);
  CustomData_add_layer(&me->edata, CD_ORIGINDEX, CD_CALLOC, NULL, bm->totedge);
  CustomData_add_layer(&me->pdata, CD_ORIGINDEX, CD_CALLOC, NULL, bm->totface);

  CustomData_add_layer(&me->vdata, CD_MVERT, CD_CALLOC, NULL, bm->totvert);
  CustomData_add_layer(&me->edata, CD_MEDGE, CD_CALLOC, NULL, bm->totedge);
  CustomData_add_layer(&me->ldata, CD_MLOOP, CD_CALLOC, NULL, bm->totloop);
  CustomData_add_layer(&me->pdata, CD_MPOLY, CD_CALLOC, NULL, bm->totface);

  /* don't process shapekeys, we only feed them through the modifier stack as needed,
   * e.g. for applying modifiers or the like*/
  CustomData_MeshMasks mask = CD_MASK_DERIVEDMESH;
  if (cd_mask_extra != NULL) {
    CustomData_MeshMasks_update(&mask, cd_mask_extra);
  }
  mask.vmask &= ~CD_MASK_SHAPEKEY;
  CustomData_merge(&bm->vdata, &me->vdata, mask.vmask, CD_CALLOC, me->totvert);
  CustomData_merge(&bm->edata, &me->edata, mask.emask, CD_CALLOC, me->totedge);
  CustomData_merge(&bm->ldata, &me->ldata, mask.lmask, CD_CALLOC, me->totloop);
  CustomData_merge(&bm->pdata, &me->pdata, mask.pmask, CD_CALLOC, me->totpoly);

  BKE_mesh_update_customdata_pointers(me, false);

  BMIter iter;
  BMVert *eve;
  BMEdge *eed;
  BMFace *efa;
  MVert *mvert = me->mvert;
  MEdge *medge = me->medge;
  MLoop *mloop = me->mloop;
  MPoly *mpoly = me->mpoly;
  int *index, add_orig;
  unsigned int i, j;

  const int cd_vert_bweight_offset = CustomData_get_offset(&bm->vdata, CD_BWEIGHT);
  const int cd_edge_bweight_offset = CustomData_get_offset(&bm->edata, CD_BWEIGHT);
  const int cd_edge_crease_offset = CustomData_get_offset(&bm->edata, CD_CREASE);

  me->runtime.deformed_only = true;

  /* don't add origindex layer if one already exists */
  add_orig = !CustomData_has_layer(&bm->pdata, CD_ORIGINDEX);

  index = CustomData_get_layer(&me->vdata, CD_ORIGINDEX);

  BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
    MVert *mv = &mvert[i];

    copy_v3_v3(mv->co, eve->co);

    BM_elem_index_set(eve, i); /* set_inline */

    normal_float_to_short_v3(mv->no, eve->no);

    mv->flag = BM_vert_flag_to_mflag(eve);

    if (cd_vert_bweight_offset != -1) {
      mv->bweight = BM_ELEM_CD_GET_FLOAT_AS_UCHAR(eve, cd_vert_bweight_offset);
    }

    if (add_orig) {
      *index++ = i;
    }

    CustomData_from_bmesh_block(&bm->vdata, &me->vdata, eve->head.data, i);
  }
  bm->elem_index_dirty &= ~BM_VERT;

  index = CustomData_get_layer(&me->edata, CD_ORIGINDEX);
  BM_ITER_MESH_INDEX (eed, &iter, bm, BM_EDGES_OF_MESH, i) {
    MEdge *med = &medge[i];

    BM_elem_index_set(eed, i); /* set_inline */

    med->v1 = BM_elem_index_get(eed->v1);
    med->v2 = BM_elem_index_get(eed->v2);

    med->flag = BM_edge_flag_to_mflag(eed);

    /* handle this differently to editmode switching,
     * only enable draw for single user edges rather then calculating angle */
    if ((med->flag & ME_EDGEDRAW) == 0) {
      if (eed->l && eed->l == eed->l->radial_next) {
        med->flag |= ME_EDGEDRAW;
      }
    }

    if (cd_edge_crease_offset != -1) {
      med->crease = BM_ELEM_CD_GET_FLOAT_AS_UCHAR(eed, cd_edge_crease_offset);
    }
    if (cd_edge_bweight_offset != -1) {
      med->bweight = BM_ELEM_CD_GET_FLOAT_AS_UCHAR(eed, cd_edge_bweight_offset);
    }

    CustomData_from_bmesh_block(&bm->edata, &me->edata, eed->head.data, i);
    if (add_orig) {
      *index++ = i;
    }
  }
  bm->elem_index_dirty &= ~BM_EDGE;

  index = CustomData_get_layer(&me->pdata, CD_ORIGINDEX);
  j = 0;
  BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
    BMLoop *l_iter;
    BMLoop *l_first;
    MPoly *mp = &mpoly[i];

    BM_elem_index_set(efa, i); /* set_inline */

    mp->totloop = efa->len;
    mp->flag = BM_face_flag_to_mflag(efa);
    mp->loopstart = j;
    mp->mat_nr = efa->mat_nr;

    l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
    do {
      mloop->v = BM_elem_index_get(l_iter->v);
      mloop->e = BM_elem_index_get(l_iter->e);
      CustomData_from_bmesh_block(&bm->ldata, &me->ldata, l_iter->head.data, j);

      BM_elem_index_set(l_iter, j); /* set_inline */

      j++;
      mloop++;
    } while ((l_iter = l_iter->next) != l_first);

    CustomData_from_bmesh_block(&bm->pdata, &me->pdata, efa->head.data, i);

    if (add_orig) {
      *index++ = i;
    }
  }
  bm->elem_index_dirty &= ~(BM_FACE | BM_LOOP);

  me->cd_flag = BM_mesh_cd_flag_from_bmesh(bm);
}
