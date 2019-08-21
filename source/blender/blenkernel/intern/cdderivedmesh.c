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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 * Implementation of CDDerivedMesh.
 *
 * BKE_cdderivedmesh.h contains the function prototypes for this file.
 */

/** \file
 * \ingroup bke
 */

#include "atomic_ops.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_pbvh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_editmesh.h"
#include "BKE_curve.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_curve_types.h" /* for Curve */

#include "MEM_guardedalloc.h"

#include <string.h>
#include <limits.h>
#include <math.h>

typedef struct {
  DerivedMesh dm;

  /* these point to data in the DerivedMesh custom data layers,
   * they are only here for efficiency and convenience */
  MVert *mvert;
  MEdge *medge;
  MFace *mface;
  MLoop *mloop;
  MPoly *mpoly;

  /* Cached */
  struct PBVH *pbvh;
  bool pbvh_draw;

  /* Mesh connectivity */
  MeshElemMap *pmap;
  int *pmap_mem;
} CDDerivedMesh;

/**************** DerivedMesh interface functions ****************/
static int cdDM_getNumVerts(DerivedMesh *dm)
{
  return dm->numVertData;
}

static int cdDM_getNumEdges(DerivedMesh *dm)
{
  return dm->numEdgeData;
}

static int cdDM_getNumTessFaces(DerivedMesh *dm)
{
  /* uncomment and add a breakpoint on the printf()
   * to help debug tessfaces issues since BMESH merge. */
#if 0
  if (dm->numTessFaceData == 0 && dm->numPolyData != 0) {
    printf("%s: has no faces!\n");
  }
#endif
  return dm->numTessFaceData;
}

static int cdDM_getNumLoops(DerivedMesh *dm)
{
  return dm->numLoopData;
}

static int cdDM_getNumPolys(DerivedMesh *dm)
{
  return dm->numPolyData;
}

static void cdDM_getVert(DerivedMesh *dm, int index, MVert *r_vert)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  *r_vert = cddm->mvert[index];
}

static void cdDM_getEdge(DerivedMesh *dm, int index, MEdge *r_edge)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  *r_edge = cddm->medge[index];
}

static void cdDM_getTessFace(DerivedMesh *dm, int index, MFace *r_face)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  *r_face = cddm->mface[index];
}

static void cdDM_copyVertArray(DerivedMesh *dm, MVert *r_vert)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  memcpy(r_vert, cddm->mvert, sizeof(*r_vert) * dm->numVertData);
}

static void cdDM_copyEdgeArray(DerivedMesh *dm, MEdge *r_edge)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  memcpy(r_edge, cddm->medge, sizeof(*r_edge) * dm->numEdgeData);
}

static void cdDM_copyTessFaceArray(DerivedMesh *dm, MFace *r_face)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  memcpy(r_face, cddm->mface, sizeof(*r_face) * dm->numTessFaceData);
}

static void cdDM_copyLoopArray(DerivedMesh *dm, MLoop *r_loop)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  memcpy(r_loop, cddm->mloop, sizeof(*r_loop) * dm->numLoopData);
}

static void cdDM_copyPolyArray(DerivedMesh *dm, MPoly *r_poly)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  memcpy(r_poly, cddm->mpoly, sizeof(*r_poly) * dm->numPolyData);
}

static void cdDM_getMinMax(DerivedMesh *dm, float r_min[3], float r_max[3])
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  int i;

  if (dm->numVertData) {
    for (i = 0; i < dm->numVertData; i++) {
      minmax_v3v3_v3(r_min, r_max, cddm->mvert[i].co);
    }
  }
  else {
    zero_v3(r_min);
    zero_v3(r_max);
  }
}

static void cdDM_getVertCo(DerivedMesh *dm, int index, float r_co[3])
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

  copy_v3_v3(r_co, cddm->mvert[index].co);
}

static void cdDM_getVertNo(DerivedMesh *dm, int index, float r_no[3])
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  normal_short_to_float_v3(r_no, cddm->mvert[index].no);
}

static const MeshElemMap *cdDM_getPolyMap(Object *ob, DerivedMesh *dm)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

  if (!cddm->pmap && ob->type == OB_MESH) {
    Mesh *me = ob->data;

    BKE_mesh_vert_poly_map_create(
        &cddm->pmap, &cddm->pmap_mem, me->mpoly, me->mloop, me->totvert, me->totpoly, me->totloop);
  }

  return cddm->pmap;
}

void CDDM_recalc_looptri(DerivedMesh *dm)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  const unsigned int totpoly = dm->numPolyData;
  const unsigned int totloop = dm->numLoopData;

  DM_ensure_looptri_data(dm);
  BLI_assert(totpoly == 0 || cddm->dm.looptris.array_wip != NULL);

  BKE_mesh_recalc_looptri(
      cddm->mloop, cddm->mpoly, cddm->mvert, totloop, totpoly, cddm->dm.looptris.array_wip);

  BLI_assert(cddm->dm.looptris.array == NULL);
  atomic_cas_ptr(
      (void **)&cddm->dm.looptris.array, cddm->dm.looptris.array, cddm->dm.looptris.array_wip);
  cddm->dm.looptris.array_wip = NULL;
}

static void cdDM_free_internal(CDDerivedMesh *cddm)
{
  if (cddm->pmap) {
    MEM_freeN(cddm->pmap);
  }
  if (cddm->pmap_mem) {
    MEM_freeN(cddm->pmap_mem);
  }
}

static void cdDM_release(DerivedMesh *dm)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

  if (DM_release(dm)) {
    cdDM_free_internal(cddm);
    MEM_freeN(cddm);
  }
}

/**************** CDDM interface functions ****************/
static CDDerivedMesh *cdDM_create(const char *desc)
{
  CDDerivedMesh *cddm;
  DerivedMesh *dm;

  cddm = MEM_callocN(sizeof(*cddm), desc);
  dm = &cddm->dm;

  dm->getMinMax = cdDM_getMinMax;

  dm->getNumVerts = cdDM_getNumVerts;
  dm->getNumEdges = cdDM_getNumEdges;
  dm->getNumTessFaces = cdDM_getNumTessFaces;
  dm->getNumLoops = cdDM_getNumLoops;
  dm->getNumPolys = cdDM_getNumPolys;

  dm->getVert = cdDM_getVert;
  dm->getEdge = cdDM_getEdge;
  dm->getTessFace = cdDM_getTessFace;

  dm->copyVertArray = cdDM_copyVertArray;
  dm->copyEdgeArray = cdDM_copyEdgeArray;
  dm->copyTessFaceArray = cdDM_copyTessFaceArray;
  dm->copyLoopArray = cdDM_copyLoopArray;
  dm->copyPolyArray = cdDM_copyPolyArray;

  dm->getVertData = DM_get_vert_data;
  dm->getEdgeData = DM_get_edge_data;
  dm->getTessFaceData = DM_get_tessface_data;
  dm->getVertDataArray = DM_get_vert_data_layer;
  dm->getEdgeDataArray = DM_get_edge_data_layer;
  dm->getTessFaceDataArray = DM_get_tessface_data_layer;

  dm->recalcLoopTri = CDDM_recalc_looptri;

  dm->getVertCo = cdDM_getVertCo;
  dm->getVertNo = cdDM_getVertNo;

  dm->getPolyMap = cdDM_getPolyMap;

  dm->release = cdDM_release;

  return cddm;
}

DerivedMesh *CDDM_new(int numVerts, int numEdges, int numTessFaces, int numLoops, int numPolys)
{
  CDDerivedMesh *cddm = cdDM_create("CDDM_new dm");
  DerivedMesh *dm = &cddm->dm;

  DM_init(dm, DM_TYPE_CDDM, numVerts, numEdges, numTessFaces, numLoops, numPolys);

  CustomData_add_layer(&dm->vertData, CD_ORIGINDEX, CD_CALLOC, NULL, numVerts);
  CustomData_add_layer(&dm->edgeData, CD_ORIGINDEX, CD_CALLOC, NULL, numEdges);
  CustomData_add_layer(&dm->faceData, CD_ORIGINDEX, CD_CALLOC, NULL, numTessFaces);
  CustomData_add_layer(&dm->polyData, CD_ORIGINDEX, CD_CALLOC, NULL, numPolys);

  CustomData_add_layer(&dm->vertData, CD_MVERT, CD_CALLOC, NULL, numVerts);
  CustomData_add_layer(&dm->edgeData, CD_MEDGE, CD_CALLOC, NULL, numEdges);
  CustomData_add_layer(&dm->faceData, CD_MFACE, CD_CALLOC, NULL, numTessFaces);
  CustomData_add_layer(&dm->loopData, CD_MLOOP, CD_CALLOC, NULL, numLoops);
  CustomData_add_layer(&dm->polyData, CD_MPOLY, CD_CALLOC, NULL, numPolys);

  cddm->mvert = CustomData_get_layer(&dm->vertData, CD_MVERT);
  cddm->medge = CustomData_get_layer(&dm->edgeData, CD_MEDGE);
  cddm->mface = CustomData_get_layer(&dm->faceData, CD_MFACE);
  cddm->mloop = CustomData_get_layer(&dm->loopData, CD_MLOOP);
  cddm->mpoly = CustomData_get_layer(&dm->polyData, CD_MPOLY);

  return dm;
}

DerivedMesh *CDDM_from_mesh(Mesh *mesh)
{
  return CDDM_from_mesh_ex(mesh, CD_REFERENCE, &CD_MASK_MESH);
}

DerivedMesh *CDDM_from_mesh_ex(Mesh *mesh,
                               eCDAllocType alloctype,
                               const CustomData_MeshMasks *mask)
{
  CDDerivedMesh *cddm = cdDM_create(__func__);
  DerivedMesh *dm = &cddm->dm;
  CustomData_MeshMasks cddata_masks = *mask;

  cddata_masks.lmask &= ~CD_MASK_MDISPS;

  /* this does a referenced copy, with an exception for fluidsim */

  DM_init(dm,
          DM_TYPE_CDDM,
          mesh->totvert,
          mesh->totedge,
          0 /* mesh->totface */,
          mesh->totloop,
          mesh->totpoly);

  /* This should actually be dm->deformedOnly = mesh->runtime.deformed_only,
   * but only if the original mesh had its deformed_only flag correctly set
   * (which isn't generally the case). */
  dm->deformedOnly = 1;
  dm->cd_flag = mesh->cd_flag;

  if (mesh->runtime.cd_dirty_vert & CD_MASK_NORMAL) {
    dm->dirty |= DM_DIRTY_NORMALS;
  }
  /* TODO DM_DIRTY_TESS_CDLAYERS ? Maybe not though,
   * since we probably want to switch to looptris? */

  CustomData_merge(&mesh->vdata, &dm->vertData, cddata_masks.vmask, alloctype, mesh->totvert);
  CustomData_merge(&mesh->edata, &dm->edgeData, cddata_masks.emask, alloctype, mesh->totedge);
  CustomData_merge(&mesh->fdata,
                   &dm->faceData,
                   cddata_masks.fmask | CD_MASK_ORIGINDEX,
                   alloctype,
                   0 /* mesh->totface */);
  CustomData_merge(&mesh->ldata, &dm->loopData, cddata_masks.lmask, alloctype, mesh->totloop);
  CustomData_merge(&mesh->pdata, &dm->polyData, cddata_masks.pmask, alloctype, mesh->totpoly);

  cddm->mvert = CustomData_get_layer(&dm->vertData, CD_MVERT);
  cddm->medge = CustomData_get_layer(&dm->edgeData, CD_MEDGE);
  cddm->mloop = CustomData_get_layer(&dm->loopData, CD_MLOOP);
  cddm->mpoly = CustomData_get_layer(&dm->polyData, CD_MPOLY);
#if 0
  cddm->mface = CustomData_get_layer(&dm->faceData, CD_MFACE);
#else
  cddm->mface = NULL;
#endif

  /* commented since even when CD_ORIGINDEX was first added this line fails
   * on the default cube, (after editmode toggle too) - campbell */
#if 0
  BLI_assert(CustomData_has_layer(&cddm->dm.faceData, CD_ORIGINDEX));
#endif

  return dm;
}

/* TODO(campbell): remove, use BKE_mesh_from_bmesh_for_eval_nomain instead. */

/* used for both editbmesh and bmesh */
static DerivedMesh *cddm_from_bmesh_ex(struct BMesh *bm, const bool use_mdisps)
{
  DerivedMesh *dm = CDDM_new(bm->totvert, bm->totedge, 0, bm->totloop, bm->totface);

  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  BMIter iter;
  BMVert *eve;
  BMEdge *eed;
  BMFace *efa;
  MVert *mvert = cddm->mvert;
  MEdge *medge = cddm->medge;
  MLoop *mloop = cddm->mloop;
  MPoly *mpoly = cddm->mpoly;
  int *index, add_orig;
  CustomData_MeshMasks mask = {0};
  unsigned int i, j;

  const int cd_vert_bweight_offset = CustomData_get_offset(&bm->vdata, CD_BWEIGHT);
  const int cd_edge_bweight_offset = CustomData_get_offset(&bm->edata, CD_BWEIGHT);
  const int cd_edge_crease_offset = CustomData_get_offset(&bm->edata, CD_CREASE);

  dm->deformedOnly = 1;

  /* don't add origindex layer if one already exists */
  add_orig = !CustomData_has_layer(&bm->pdata, CD_ORIGINDEX);

  mask = CD_MASK_DERIVEDMESH;
  if (use_mdisps) {
    mask.lmask |= CD_MASK_MDISPS;
  }

  /* don't process shapekeys, we only feed them through the modifier stack as needed,
   * e.g. for applying modifiers or the like*/
  mask.vmask &= ~CD_MASK_SHAPEKEY;
  CustomData_merge(&bm->vdata, &dm->vertData, mask.vmask, CD_CALLOC, dm->numVertData);
  CustomData_merge(&bm->edata, &dm->edgeData, mask.emask, CD_CALLOC, dm->numEdgeData);
  CustomData_merge(&bm->ldata, &dm->loopData, mask.lmask, CD_CALLOC, dm->numLoopData);
  CustomData_merge(&bm->pdata, &dm->polyData, mask.pmask, CD_CALLOC, dm->numPolyData);

  index = dm->getVertDataArray(dm, CD_ORIGINDEX);

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

    CustomData_from_bmesh_block(&bm->vdata, &dm->vertData, eve->head.data, i);
  }
  bm->elem_index_dirty &= ~BM_VERT;

  index = dm->getEdgeDataArray(dm, CD_ORIGINDEX);
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

    CustomData_from_bmesh_block(&bm->edata, &dm->edgeData, eed->head.data, i);
    if (add_orig) {
      *index++ = i;
    }
  }
  bm->elem_index_dirty &= ~BM_EDGE;

  index = CustomData_get_layer(&dm->polyData, CD_ORIGINDEX);
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
      CustomData_from_bmesh_block(&bm->ldata, &dm->loopData, l_iter->head.data, j);

      BM_elem_index_set(l_iter, j); /* set_inline */

      j++;
      mloop++;
    } while ((l_iter = l_iter->next) != l_first);

    CustomData_from_bmesh_block(&bm->pdata, &dm->polyData, efa->head.data, i);

    if (add_orig) {
      *index++ = i;
    }
  }
  bm->elem_index_dirty &= ~(BM_FACE | BM_LOOP);

  dm->cd_flag = BM_mesh_cd_flag_from_bmesh(bm);

  return dm;
}

DerivedMesh *CDDM_from_editbmesh(BMEditMesh *em, const bool use_mdisps)
{
  return cddm_from_bmesh_ex(em->bm, use_mdisps);
}

DerivedMesh *CDDM_copy(DerivedMesh *source)
{
  CDDerivedMesh *cddm = cdDM_create("CDDM_copy cddm");
  DerivedMesh *dm = &cddm->dm;
  int numVerts = source->numVertData;
  int numEdges = source->numEdgeData;
  int numTessFaces = 0;
  int numLoops = source->numLoopData;
  int numPolys = source->numPolyData;

  /* NOTE: Don't copy tessellation faces if not requested explicitly. */

  /* ensure these are created if they are made on demand */
  source->getVertDataArray(source, CD_ORIGINDEX);
  source->getEdgeDataArray(source, CD_ORIGINDEX);
  source->getPolyDataArray(source, CD_ORIGINDEX);

  /* this initializes dm, and copies all non mvert/medge/mface layers */
  DM_from_template(dm, source, DM_TYPE_CDDM, numVerts, numEdges, numTessFaces, numLoops, numPolys);
  dm->deformedOnly = source->deformedOnly;
  dm->cd_flag = source->cd_flag;
  dm->dirty = source->dirty;

  /* Tessellation data is never copied, so tag it here.
   * Only tag dirty layers if we really ignored tessellation faces.
   */
  dm->dirty |= DM_DIRTY_TESS_CDLAYERS;

  CustomData_copy_data(&source->vertData, &dm->vertData, 0, 0, numVerts);
  CustomData_copy_data(&source->edgeData, &dm->edgeData, 0, 0, numEdges);

  /* now add mvert/medge/mface layers */
  cddm->mvert = source->dupVertArray(source);
  cddm->medge = source->dupEdgeArray(source);

  CustomData_add_layer(&dm->vertData, CD_MVERT, CD_ASSIGN, cddm->mvert, numVerts);
  CustomData_add_layer(&dm->edgeData, CD_MEDGE, CD_ASSIGN, cddm->medge, numEdges);

  DM_DupPolys(source, dm);

  cddm->mloop = CustomData_get_layer(&dm->loopData, CD_MLOOP);
  cddm->mpoly = CustomData_get_layer(&dm->polyData, CD_MPOLY);

  return dm;
}

/* #define DEBUG_CLNORS */
#ifdef DEBUG_CLNORS
#  include "BLI_linklist.h"
#endif

/* mesh element access functions */

MVert *CDDM_get_vert(DerivedMesh *dm, int index)
{
  return &((CDDerivedMesh *)dm)->mvert[index];
}

MEdge *CDDM_get_edge(DerivedMesh *dm, int index)
{
  return &((CDDerivedMesh *)dm)->medge[index];
}

MFace *CDDM_get_tessface(DerivedMesh *dm, int index)
{
  return &((CDDerivedMesh *)dm)->mface[index];
}

MLoop *CDDM_get_loop(DerivedMesh *dm, int index)
{
  return &((CDDerivedMesh *)dm)->mloop[index];
}

MPoly *CDDM_get_poly(DerivedMesh *dm, int index)
{
  return &((CDDerivedMesh *)dm)->mpoly[index];
}

/* array access functions */

MVert *CDDM_get_verts(DerivedMesh *dm)
{
  return ((CDDerivedMesh *)dm)->mvert;
}

MEdge *CDDM_get_edges(DerivedMesh *dm)
{
  return ((CDDerivedMesh *)dm)->medge;
}

MFace *CDDM_get_tessfaces(DerivedMesh *dm)
{
  return ((CDDerivedMesh *)dm)->mface;
}

MLoop *CDDM_get_loops(DerivedMesh *dm)
{
  return ((CDDerivedMesh *)dm)->mloop;
}

MPoly *CDDM_get_polys(DerivedMesh *dm)
{
  return ((CDDerivedMesh *)dm)->mpoly;
}
