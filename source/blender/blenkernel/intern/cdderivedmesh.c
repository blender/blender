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

static void cdDM_getVertCos(DerivedMesh *dm, float (*r_cos)[3])
{
  MVert *mv = CDDM_get_verts(dm);
  int i;

  for (i = 0; i < dm->numVertData; i++, mv++) {
    copy_v3_v3(r_cos[i], mv->co);
  }
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

static bool check_sculpt_object_deformed(Object *object, bool for_construction)
{
  bool deformed = false;

  /* Active modifiers means extra deformation, which can't be handled correct
   * on birth of PBVH and sculpt "layer" levels, so use PBVH only for internal brush
   * stuff and show final DerivedMesh so user would see actual object shape.
   */
  deformed |= object->sculpt->modifiers_active;

  if (for_construction) {
    deformed |= object->sculpt->kb != NULL;
  }
  else {
    /* As in case with modifiers, we can't synchronize deformation made against
     * PBVH and non-locked keyblock, so also use PBVH only for brushes and
     * final DM to give final result to user.
     */
    deformed |= object->sculpt->kb && (object->shapeflag & OB_SHAPE_LOCK) == 0;
  }

  return deformed;
}

static bool can_pbvh_draw(Object *ob, DerivedMesh *dm)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  Mesh *me = ob->data;
  bool deformed = check_sculpt_object_deformed(ob, false);

  if (deformed) {
    return false;
  }

  return cddm->mvert == me->mvert || ob->sculpt->kb;
}

static PBVH *cdDM_getPBVH(Object *ob, DerivedMesh *dm)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

  if (!ob) {
    cddm->pbvh = NULL;
    return NULL;
  }

  if (!ob->sculpt) {
    return NULL;
  }

  if (ob->sculpt->pbvh) {
    cddm->pbvh = ob->sculpt->pbvh;
    cddm->pbvh_draw = can_pbvh_draw(ob, dm);
  }

  /* Sculpting on a BMesh (dynamic-topology) gets a special PBVH */
  if (!cddm->pbvh && ob->sculpt->bm) {
    cddm->pbvh = BKE_pbvh_new();
    cddm->pbvh_draw = true;

    BKE_pbvh_build_bmesh(cddm->pbvh,
                         ob->sculpt->bm,
                         ob->sculpt->bm_smooth_shading,
                         ob->sculpt->bm_log,
                         ob->sculpt->cd_vert_node_offset,
                         ob->sculpt->cd_face_node_offset);

    pbvh_show_diffuse_color_set(cddm->pbvh, ob->sculpt->show_diffuse_color);
    pbvh_show_mask_set(cddm->pbvh, ob->sculpt->show_mask);
  }

  /* always build pbvh from original mesh, and only use it for drawing if
   * this derivedmesh is just original mesh. it's the multires subsurf dm
   * that this is actually for, to support a pbvh on a modified mesh */
  if (!cddm->pbvh && ob->type == OB_MESH) {
    Mesh *me = BKE_object_get_original_mesh(ob);
    const int looptris_num = poly_to_tri_count(me->totpoly, me->totloop);
    MLoopTri *looptri;
    bool deformed;

    cddm->pbvh = BKE_pbvh_new();
    cddm->pbvh_draw = can_pbvh_draw(ob, dm);

    looptri = MEM_malloc_arrayN(looptris_num, sizeof(*looptri), __func__);

    BKE_mesh_recalc_looptri(me->mloop, me->mpoly, me->mvert, me->totloop, me->totpoly, looptri);

    BKE_pbvh_build_mesh(cddm->pbvh,
                        me->mpoly,
                        me->mloop,
                        me->mvert,
                        me->totvert,
                        &me->vdata,
                        &me->ldata,
                        looptri,
                        looptris_num);

    pbvh_show_diffuse_color_set(cddm->pbvh, ob->sculpt->show_diffuse_color);
    pbvh_show_mask_set(cddm->pbvh, ob->sculpt->show_mask);

    deformed = check_sculpt_object_deformed(ob, true);

    if (deformed && ob->derivedDeform) {
      DerivedMesh *deformdm = ob->derivedDeform;
      float(*vertCos)[3];
      int totvert;

      totvert = deformdm->getNumVerts(deformdm);
      vertCos = MEM_malloc_arrayN(totvert, sizeof(float[3]), "cdDM_getPBVH vertCos");
      deformdm->getVertCos(deformdm, vertCos);
      BKE_pbvh_apply_vertCos(cddm->pbvh, vertCos, totvert);
      MEM_freeN(vertCos);
    }
  }

  return cddm->pbvh;
}

static void cdDM_foreachMappedVert(DerivedMesh *dm,
                                   void (*func)(void *userData,
                                                int index,
                                                const float co[3],
                                                const float no_f[3],
                                                const short no_s[3]),
                                   void *userData,
                                   DMForeachFlag flag)
{
  MVert *mv = CDDM_get_verts(dm);
  const int *index = DM_get_vert_data_layer(dm, CD_ORIGINDEX);
  int i;

  if (index) {
    for (i = 0; i < dm->numVertData; i++, mv++) {
      const short *no = (flag & DM_FOREACH_USE_NORMAL) ? mv->no : NULL;
      const int orig = *index++;
      if (orig == ORIGINDEX_NONE) {
        continue;
      }
      func(userData, orig, mv->co, NULL, no);
    }
  }
  else {
    for (i = 0; i < dm->numVertData; i++, mv++) {
      const short *no = (flag & DM_FOREACH_USE_NORMAL) ? mv->no : NULL;
      func(userData, i, mv->co, NULL, no);
    }
  }
}

static void cdDM_foreachMappedEdge(
    DerivedMesh *dm,
    void (*func)(void *userData, int index, const float v0co[3], const float v1co[3]),
    void *userData)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  MVert *mv = cddm->mvert;
  MEdge *med = cddm->medge;
  int i, orig, *index = DM_get_edge_data_layer(dm, CD_ORIGINDEX);

  for (i = 0; i < dm->numEdgeData; i++, med++) {
    if (index) {
      orig = *index++;
      if (orig == ORIGINDEX_NONE) {
        continue;
      }
      func(userData, orig, mv[med->v1].co, mv[med->v2].co);
    }
    else {
      func(userData, i, mv[med->v1].co, mv[med->v2].co);
    }
  }
}

static void cdDM_foreachMappedLoop(DerivedMesh *dm,
                                   void (*func)(void *userData,
                                                int vertex_index,
                                                int face_index,
                                                const float co[3],
                                                const float no[3]),
                                   void *userData,
                                   DMForeachFlag flag)
{
  /* We can't use dm->getLoopDataLayout(dm) here,
   * we want to always access dm->loopData, EditDerivedBMesh would
   * return loop data from bmesh itself. */
  const float(*lnors)[3] = (flag & DM_FOREACH_USE_NORMAL) ? DM_get_loop_data_layer(dm, CD_NORMAL) :
                                                            NULL;

  const MVert *mv = CDDM_get_verts(dm);
  const MLoop *ml = CDDM_get_loops(dm);
  const MPoly *mp = CDDM_get_polys(dm);
  const int *v_index = DM_get_vert_data_layer(dm, CD_ORIGINDEX);
  const int *f_index = DM_get_poly_data_layer(dm, CD_ORIGINDEX);
  int p_idx, i;

  for (p_idx = 0; p_idx < dm->numPolyData; ++p_idx, ++mp) {
    for (i = 0; i < mp->totloop; ++i, ++ml) {
      const int v_idx = v_index ? v_index[ml->v] : ml->v;
      const int f_idx = f_index ? f_index[p_idx] : p_idx;
      const float *no = lnors ? *lnors++ : NULL;
      if (!ELEM(ORIGINDEX_NONE, v_idx, f_idx)) {
        func(userData, v_idx, f_idx, mv[ml->v].co, no);
      }
    }
  }
}

static void cdDM_foreachMappedFaceCenter(
    DerivedMesh *dm,
    void (*func)(void *userData, int index, const float cent[3], const float no[3]),
    void *userData,
    DMForeachFlag flag)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  MVert *mvert = cddm->mvert;
  MPoly *mp;
  MLoop *ml;
  int i, orig, *index;

  index = CustomData_get_layer(&dm->polyData, CD_ORIGINDEX);
  mp = cddm->mpoly;
  for (i = 0; i < dm->numPolyData; i++, mp++) {
    float cent[3];
    float *no, _no[3];

    if (index) {
      orig = *index++;
      if (orig == ORIGINDEX_NONE) {
        continue;
      }
    }
    else {
      orig = i;
    }

    ml = &cddm->mloop[mp->loopstart];
    BKE_mesh_calc_poly_center(mp, ml, mvert, cent);

    if (flag & DM_FOREACH_USE_NORMAL) {
      BKE_mesh_calc_poly_normal(mp, ml, mvert, (no = _no));
    }
    else {
      no = NULL;
    }

    func(userData, orig, cent, no);
  }
}

void CDDM_recalc_tessellation_ex(DerivedMesh *dm, const bool do_face_nor_cpy)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

  dm->numTessFaceData = BKE_mesh_recalc_tessellation(&dm->faceData,
                                                     &dm->loopData,
                                                     &dm->polyData,
                                                     cddm->mvert,
                                                     dm->numTessFaceData,
                                                     dm->numLoopData,
                                                     dm->numPolyData,
                                                     do_face_nor_cpy);

  cddm->mface = CustomData_get_layer(&dm->faceData, CD_MFACE);

  /* Tessellation recreated faceData, and the active layer indices need to get re-propagated
   * from loops and polys to faces */
  CustomData_bmesh_update_active_layers(&dm->faceData, &dm->loopData);
}

void CDDM_recalc_tessellation(DerivedMesh *dm)
{
  CDDM_recalc_tessellation_ex(dm, true);
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

  dm->calcNormals = CDDM_calc_normals;
  dm->calcLoopNormals = CDDM_calc_loop_normals;
  dm->calcLoopNormalsSpaceArray = CDDM_calc_loop_normals_spacearr;
  dm->calcLoopTangents = DM_calc_loop_tangents;
  dm->recalcTessellation = CDDM_recalc_tessellation;
  dm->recalcLoopTri = CDDM_recalc_looptri;

  dm->getVertCos = cdDM_getVertCos;
  dm->getVertCo = cdDM_getVertCo;
  dm->getVertNo = cdDM_getVertNo;

  dm->getPBVH = cdDM_getPBVH;
  dm->getPolyMap = cdDM_getPolyMap;

  dm->foreachMappedVert = cdDM_foreachMappedVert;
  dm->foreachMappedEdge = cdDM_foreachMappedEdge;
  dm->foreachMappedLoop = cdDM_foreachMappedLoop;
  dm->foreachMappedFaceCenter = cdDM_foreachMappedFaceCenter;

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

DerivedMesh *CDDM_from_curve(Object *ob)
{
  ListBase disp = {NULL, NULL};

  if (ob->runtime.curve_cache) {
    disp = ob->runtime.curve_cache->disp;
  }

  return CDDM_from_curve_displist(ob, &disp);
}

DerivedMesh *CDDM_from_curve_displist(Object *ob, ListBase *dispbase)
{
  Curve *cu = (Curve *)ob->data;
  DerivedMesh *dm;
  CDDerivedMesh *cddm;
  MVert *allvert;
  MEdge *alledge;
  MLoop *allloop;
  MPoly *allpoly;
  MLoopUV *alluv = NULL;
  int totvert, totedge, totloop, totpoly;
  bool use_orco_uv = (cu->flag & CU_UV_ORCO) != 0;

  if (BKE_mesh_nurbs_displist_to_mdata(ob,
                                       dispbase,
                                       &allvert,
                                       &totvert,
                                       &alledge,
                                       &totedge,
                                       &allloop,
                                       &allpoly,
                                       (use_orco_uv) ? &alluv : NULL,
                                       &totloop,
                                       &totpoly) != 0) {
    /* Error initializing mdata. This often happens when curve is empty */
    return CDDM_new(0, 0, 0, 0, 0);
  }

  dm = CDDM_new(totvert, totedge, 0, totloop, totpoly);
  dm->deformedOnly = 1;
  dm->dirty |= DM_DIRTY_NORMALS;

  cddm = (CDDerivedMesh *)dm;

  memcpy(cddm->mvert, allvert, totvert * sizeof(MVert));
  memcpy(cddm->medge, alledge, totedge * sizeof(MEdge));
  memcpy(cddm->mloop, allloop, totloop * sizeof(MLoop));
  memcpy(cddm->mpoly, allpoly, totpoly * sizeof(MPoly));

  if (alluv) {
    const char *uvname = "Orco";
    CustomData_add_layer_named(&cddm->dm.loopData, CD_MLOOPUV, CD_ASSIGN, alluv, totloop, uvname);
  }

  MEM_freeN(allvert);
  MEM_freeN(alledge);
  MEM_freeN(allloop);
  MEM_freeN(allpoly);

  return dm;
}

static void loops_to_customdata_corners(
    BMesh *bm, CustomData *facedata, int cdindex, const BMLoop *l3[3], int numCol, int numUV)
{
  const BMLoop *l;
  //  BMFace *f = l3[0]->f;
  MTFace *texface;
  MCol *mcol;
  MLoopCol *mloopcol;
  MLoopUV *mloopuv;
  int i, j, hasPCol = CustomData_has_layer(&bm->ldata, CD_PREVIEW_MLOOPCOL);

  for (i = 0; i < numUV; i++) {
    texface = CustomData_get_n(facedata, CD_MTFACE, cdindex, i);

    for (j = 0; j < 3; j++) {
      l = l3[j];
      mloopuv = CustomData_bmesh_get_n(&bm->ldata, l->head.data, CD_MLOOPUV, i);
      copy_v2_v2(texface->uv[j], mloopuv->uv);
    }
  }

  for (i = 0; i < numCol; i++) {
    mcol = CustomData_get_n(facedata, CD_MCOL, cdindex, i);

    for (j = 0; j < 3; j++) {
      l = l3[j];
      mloopcol = CustomData_bmesh_get_n(&bm->ldata, l->head.data, CD_MLOOPCOL, i);
      MESH_MLOOPCOL_TO_MCOL(mloopcol, &mcol[j]);
    }
  }

  if (hasPCol) {
    mcol = CustomData_get(facedata, cdindex, CD_PREVIEW_MCOL);

    for (j = 0; j < 3; j++) {
      l = l3[j];
      mloopcol = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_PREVIEW_MLOOPCOL);
      MESH_MLOOPCOL_TO_MCOL(mloopcol, &mcol[j]);
    }
  }
}

/* TODO(campbell): remove, use BKE_mesh_from_bmesh_for_eval_nomain instead. */

/* used for both editbmesh and bmesh */
static DerivedMesh *cddm_from_bmesh_ex(struct BMesh *bm,
                                       const bool use_mdisps,
                                       /* EditBMesh vars for use_tessface */
                                       const bool use_tessface,
                                       const int em_tottri,
                                       const BMLoop *(*em_looptris)[3])
{
  DerivedMesh *dm = CDDM_new(
      bm->totvert, bm->totedge, use_tessface ? em_tottri : 0, bm->totloop, bm->totface);

  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  BMIter iter;
  BMVert *eve;
  BMEdge *eed;
  BMFace *efa;
  MVert *mvert = cddm->mvert;
  MEdge *medge = cddm->medge;
  MFace *mface = cddm->mface;
  MLoop *mloop = cddm->mloop;
  MPoly *mpoly = cddm->mpoly;
  int numCol = CustomData_number_of_layers(&bm->ldata, CD_MLOOPCOL);
  int numUV = CustomData_number_of_layers(&bm->ldata, CD_MLOOPUV);
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

  /* add tessellation mface layers */
  if (use_tessface) {
    CustomData_from_bmeshpoly(&dm->faceData, &dm->loopData, em_tottri);
  }

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

  /* avoid this where possiblem, takes extra memory */
  if (use_tessface) {

    BM_mesh_elem_index_ensure(bm, BM_FACE);

    index = dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
    for (i = 0; i < dm->numTessFaceData; i++) {
      MFace *mf = &mface[i];
      const BMLoop **l = em_looptris[i];
      efa = l[0]->f;

      mf->v1 = BM_elem_index_get(l[0]->v);
      mf->v2 = BM_elem_index_get(l[1]->v);
      mf->v3 = BM_elem_index_get(l[2]->v);
      mf->v4 = 0;
      mf->mat_nr = efa->mat_nr;
      mf->flag = BM_face_flag_to_mflag(efa);

      /* map mfaces to polygons in the same cddm intentionally */
      *index++ = BM_elem_index_get(efa);

      loops_to_customdata_corners(bm, &dm->faceData, i, l, numCol, numUV);
      test_index_face(mf, &dm->faceData, i, 3);
    }
  }

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

struct DerivedMesh *CDDM_from_bmesh(struct BMesh *bm, const bool use_mdisps)
{
  return cddm_from_bmesh_ex(bm,
                            use_mdisps,
                            false,
                            /* these vars are for editmesh only */
                            0,
                            NULL);
}

DerivedMesh *CDDM_from_editbmesh(BMEditMesh *em, const bool use_mdisps, const bool use_tessface)
{
  return cddm_from_bmesh_ex(em->bm,
                            use_mdisps,
                            /* editmesh */
                            use_tessface,
                            em->tottri,
                            (const BMLoop *(*)[3])em->looptris);
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

/* note, the CD_ORIGINDEX layers are all 0, so if there is a direct
 * relationship between mesh data this needs to be set by the caller. */
DerivedMesh *CDDM_from_template_ex(DerivedMesh *source,
                                   int numVerts,
                                   int numEdges,
                                   int numTessFaces,
                                   int numLoops,
                                   int numPolys,
                                   const CustomData_MeshMasks *mask)
{
  CDDerivedMesh *cddm = cdDM_create("CDDM_from_template dest");
  DerivedMesh *dm = &cddm->dm;

  /* ensure these are created if they are made on demand */
  source->getVertDataArray(source, CD_ORIGINDEX);
  source->getEdgeDataArray(source, CD_ORIGINDEX);
  source->getTessFaceDataArray(source, CD_ORIGINDEX);
  source->getPolyDataArray(source, CD_ORIGINDEX);

  /* this does a copy of all non mvert/medge/mface layers */
  DM_from_template_ex(
      dm, source, DM_TYPE_CDDM, numVerts, numEdges, numTessFaces, numLoops, numPolys, mask);

  /* now add mvert/medge/mface layers */
  CustomData_add_layer(&dm->vertData, CD_MVERT, CD_CALLOC, NULL, numVerts);
  CustomData_add_layer(&dm->edgeData, CD_MEDGE, CD_CALLOC, NULL, numEdges);
  CustomData_add_layer(&dm->faceData, CD_MFACE, CD_CALLOC, NULL, numTessFaces);
  CustomData_add_layer(&dm->loopData, CD_MLOOP, CD_CALLOC, NULL, numLoops);
  CustomData_add_layer(&dm->polyData, CD_MPOLY, CD_CALLOC, NULL, numPolys);

  if (!CustomData_get_layer(&dm->vertData, CD_ORIGINDEX)) {
    CustomData_add_layer(&dm->vertData, CD_ORIGINDEX, CD_CALLOC, NULL, numVerts);
  }
  if (!CustomData_get_layer(&dm->edgeData, CD_ORIGINDEX)) {
    CustomData_add_layer(&dm->edgeData, CD_ORIGINDEX, CD_CALLOC, NULL, numEdges);
  }
  if (!CustomData_get_layer(&dm->faceData, CD_ORIGINDEX)) {
    CustomData_add_layer(&dm->faceData, CD_ORIGINDEX, CD_CALLOC, NULL, numTessFaces);
  }

  cddm->mvert = CustomData_get_layer(&dm->vertData, CD_MVERT);
  cddm->medge = CustomData_get_layer(&dm->edgeData, CD_MEDGE);
  cddm->mface = CustomData_get_layer(&dm->faceData, CD_MFACE);
  cddm->mloop = CustomData_get_layer(&dm->loopData, CD_MLOOP);
  cddm->mpoly = CustomData_get_layer(&dm->polyData, CD_MPOLY);

  return dm;
}
DerivedMesh *CDDM_from_template(
    DerivedMesh *source, int numVerts, int numEdges, int numTessFaces, int numLoops, int numPolys)
{
  return CDDM_from_template_ex(
      source, numVerts, numEdges, numTessFaces, numLoops, numPolys, &CD_MASK_DERIVEDMESH);
}

void CDDM_apply_vert_coords(DerivedMesh *dm, float (*vertCoords)[3])
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  MVert *vert;
  int i;

  /* this will just return the pointer if it wasn't a referenced layer */
  vert = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MVERT, dm->numVertData);
  cddm->mvert = vert;

  for (i = 0; i < dm->numVertData; ++i, ++vert) {
    copy_v3_v3(vert->co, vertCoords[i]);
  }

  cddm->dm.dirty |= DM_DIRTY_NORMALS;
}

void CDDM_apply_vert_normals(DerivedMesh *dm, short (*vertNormals)[3])
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  MVert *vert;
  int i;

  /* this will just return the pointer if it wasn't a referenced layer */
  vert = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MVERT, dm->numVertData);
  cddm->mvert = vert;

  for (i = 0; i < dm->numVertData; ++i, ++vert) {
    copy_v3_v3_short(vert->no, vertNormals[i]);
  }

  cddm->dm.dirty &= ~DM_DIRTY_NORMALS;
}

void CDDM_calc_normals_mapping_ex(DerivedMesh *dm, const bool only_face_normals)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  float(*face_nors)[3] = NULL;

  if (dm->numVertData == 0) {
    cddm->dm.dirty &= ~DM_DIRTY_NORMALS;
    return;
  }

  /* now we skip calculating vertex normals for referenced layer,
   * no need to duplicate verts.
   * WATCH THIS, bmesh only change!,
   * need to take care of the side effects here - campbell */
#if 0
  /* we don't want to overwrite any referenced layers */
  cddm->mvert = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MVERT, dm->numVertData);
#endif

#if 0
  if (dm->numTessFaceData == 0) {
    /* No tessellation on this mesh yet, need to calculate one.
     *
     * Important not to update face normals from polys since it
     * interferes with assigning the new normal layer in the following code.
     */
    CDDM_recalc_tessellation_ex(dm, false);
  }
  else {
    /* A tessellation already exists, it should always have a CD_ORIGINDEX */
    BLI_assert(CustomData_has_layer(&dm->faceData, CD_ORIGINDEX));
    CustomData_free_layers(&dm->faceData, CD_NORMAL, dm->numTessFaceData);
  }
#endif

  face_nors = MEM_malloc_arrayN(dm->numPolyData, sizeof(*face_nors), "face_nors");

  /* calculate face normals */
  BKE_mesh_calc_normals_poly(cddm->mvert,
                             NULL,
                             dm->numVertData,
                             CDDM_get_loops(dm),
                             CDDM_get_polys(dm),
                             dm->numLoopData,
                             dm->numPolyData,
                             face_nors,
                             only_face_normals);

  CustomData_add_layer(&dm->polyData, CD_NORMAL, CD_ASSIGN, face_nors, dm->numPolyData);

  cddm->dm.dirty &= ~DM_DIRTY_NORMALS;
}

void CDDM_calc_normals_mapping(DerivedMesh *dm)
{
  /* use this to skip calculating normals on original vert's, this may need to be changed */
  const bool only_face_normals = CustomData_is_referenced_layer(&dm->vertData, CD_MVERT);

  CDDM_calc_normals_mapping_ex(dm, only_face_normals);
}

#if 0
/* bmesh note: this matches what we have in trunk */
void CDDM_calc_normals(DerivedMesh *dm)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  float(*poly_nors)[3];

  if (dm->numVertData == 0)
    return;

  /* we don't want to overwrite any referenced layers */
  cddm->mvert = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MVERT, dm->numVertData);

  /* fill in if it exists */
  poly_nors = CustomData_get_layer(&dm->polyData, CD_NORMAL);
  if (!poly_nors) {
    poly_nors = CustomData_add_layer(&dm->polyData, CD_NORMAL, CD_CALLOC, NULL, dm->numPolyData);
  }

  BKE_mesh_calc_normals_poly(cddm->mvert,
                             dm->numVertData,
                             CDDM_get_loops(dm),
                             CDDM_get_polys(dm),
                             dm->numLoopData,
                             dm->numPolyData,
                             poly_nors,
                             false);

  cddm->dm.dirty &= ~DM_DIRTY_NORMALS;
}
#else

/* poly normal layer is now only for final display */
void CDDM_calc_normals(DerivedMesh *dm)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

  /* we don't want to overwrite any referenced layers */
  cddm->mvert = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MVERT, dm->numVertData);

  BKE_mesh_calc_normals_poly(cddm->mvert,
                             NULL,
                             dm->numVertData,
                             CDDM_get_loops(dm),
                             CDDM_get_polys(dm),
                             dm->numLoopData,
                             dm->numPolyData,
                             NULL,
                             false);

  cddm->dm.dirty &= ~DM_DIRTY_NORMALS;
}

#endif

void CDDM_calc_loop_normals(DerivedMesh *dm, const bool use_split_normals, const float split_angle)
{
  CDDM_calc_loop_normals_spacearr(dm, use_split_normals, split_angle, NULL);
}

/* #define DEBUG_CLNORS */
#ifdef DEBUG_CLNORS
#  include "BLI_linklist.h"
#endif

void CDDM_calc_loop_normals_spacearr(DerivedMesh *dm,
                                     const bool use_split_normals,
                                     const float split_angle,
                                     MLoopNorSpaceArray *r_lnors_spacearr)
{
  MVert *mverts = dm->getVertArray(dm);
  MEdge *medges = dm->getEdgeArray(dm);
  MLoop *mloops = dm->getLoopArray(dm);
  MPoly *mpolys = dm->getPolyArray(dm);

  CustomData *ldata, *pdata;

  float(*lnors)[3];
  short(*clnor_data)[2];
  float(*pnors)[3];

  const int numVerts = dm->getNumVerts(dm);
  const int numEdges = dm->getNumEdges(dm);
  const int numLoops = dm->getNumLoops(dm);
  const int numPolys = dm->getNumPolys(dm);

  ldata = dm->getLoopDataLayout(dm);
  if (CustomData_has_layer(ldata, CD_NORMAL)) {
    lnors = CustomData_get_layer(ldata, CD_NORMAL);
  }
  else {
    lnors = CustomData_add_layer(ldata, CD_NORMAL, CD_CALLOC, NULL, numLoops);
  }

  /* Compute poly (always needed) and vert normals. */
  /* Note we can't use DM_ensure_normals, since it won't keep computed poly nors... */
  pdata = dm->getPolyDataLayout(dm);
  pnors = CustomData_get_layer(pdata, CD_NORMAL);
  if (!pnors) {
    pnors = CustomData_add_layer(pdata, CD_NORMAL, CD_CALLOC, NULL, numPolys);
  }
  BKE_mesh_calc_normals_poly(mverts,
                             NULL,
                             numVerts,
                             mloops,
                             mpolys,
                             numLoops,
                             numPolys,
                             pnors,
                             (dm->dirty & DM_DIRTY_NORMALS) ? false : true);

  dm->dirty &= ~DM_DIRTY_NORMALS;

  clnor_data = CustomData_get_layer(ldata, CD_CUSTOMLOOPNORMAL);

  BKE_mesh_normals_loop_split(mverts,
                              numVerts,
                              medges,
                              numEdges,
                              mloops,
                              lnors,
                              numLoops,
                              mpolys,
                              (const float(*)[3])pnors,
                              numPolys,
                              use_split_normals,
                              split_angle,
                              r_lnors_spacearr,
                              clnor_data,
                              NULL);
#ifdef DEBUG_CLNORS
  if (r_lnors_spacearr) {
    int i;
    for (i = 0; i < numLoops; i++) {
      if (r_lnors_spacearr->lspacearr[i]->ref_alpha != 0.0f) {
        LinkNode *loops = r_lnors_spacearr->lspacearr[i]->loops;
        printf("Loop %d uses lnor space %p:\n", i, r_lnors_spacearr->lspacearr[i]);
        print_v3("\tfinal lnor", lnors[i]);
        print_v3("\tauto lnor", r_lnors_spacearr->lspacearr[i]->vec_lnor);
        print_v3("\tref_vec", r_lnors_spacearr->lspacearr[i]->vec_ref);
        printf("\talpha: %f\n\tbeta: %f\n\tloops: %p\n",
               r_lnors_spacearr->lspacearr[i]->ref_alpha,
               r_lnors_spacearr->lspacearr[i]->ref_beta,
               r_lnors_spacearr->lspacearr[i]->loops);
        printf("\t\t(shared with loops");
        while (loops) {
          printf(" %d", POINTER_AS_INT(loops->link));
          loops = loops->next;
        }
        printf(")\n");
      }
      else {
        printf("Loop %d has no lnor space\n", i);
      }
    }
  }
#endif
}

void CDDM_lower_num_verts(DerivedMesh *dm, int numVerts)
{
  BLI_assert(numVerts >= 0);
  if (numVerts < dm->numVertData) {
    CustomData_free_elem(&dm->vertData, numVerts, dm->numVertData - numVerts);
  }

  dm->numVertData = numVerts;
}

void CDDM_lower_num_edges(DerivedMesh *dm, int numEdges)
{
  BLI_assert(numEdges >= 0);
  if (numEdges < dm->numEdgeData) {
    CustomData_free_elem(&dm->edgeData, numEdges, dm->numEdgeData - numEdges);
  }

  dm->numEdgeData = numEdges;
}

void CDDM_lower_num_tessfaces(DerivedMesh *dm, int numTessFaces)
{
  BLI_assert(numTessFaces >= 0);
  if (numTessFaces < dm->numTessFaceData) {
    CustomData_free_elem(&dm->faceData, numTessFaces, dm->numTessFaceData - numTessFaces);
  }

  dm->numTessFaceData = numTessFaces;
}

void CDDM_lower_num_loops(DerivedMesh *dm, int numLoops)
{
  BLI_assert(numLoops >= 0);
  if (numLoops < dm->numLoopData) {
    CustomData_free_elem(&dm->loopData, numLoops, dm->numLoopData - numLoops);
  }

  dm->numLoopData = numLoops;
}

void CDDM_lower_num_polys(DerivedMesh *dm, int numPolys)
{
  BLI_assert(numPolys >= 0);
  if (numPolys < dm->numPolyData) {
    CustomData_free_elem(&dm->polyData, numPolys, dm->numPolyData - numPolys);
  }

  dm->numPolyData = numPolys;
}

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

void CDDM_set_mvert(DerivedMesh *dm, MVert *mvert)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

  if (!CustomData_has_layer(&dm->vertData, CD_MVERT)) {
    CustomData_add_layer(&dm->vertData, CD_MVERT, CD_ASSIGN, mvert, dm->numVertData);
  }

  cddm->mvert = mvert;
}

void CDDM_set_medge(DerivedMesh *dm, MEdge *medge)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

  if (!CustomData_has_layer(&dm->edgeData, CD_MEDGE)) {
    CustomData_add_layer(&dm->edgeData, CD_MEDGE, CD_ASSIGN, medge, dm->numEdgeData);
  }

  cddm->medge = medge;
}

void CDDM_set_mface(DerivedMesh *dm, MFace *mface)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

  if (!CustomData_has_layer(&dm->faceData, CD_MFACE)) {
    CustomData_add_layer(&dm->faceData, CD_MFACE, CD_ASSIGN, mface, dm->numTessFaceData);
  }

  cddm->mface = mface;
}

void CDDM_set_mloop(DerivedMesh *dm, MLoop *mloop)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

  if (!CustomData_has_layer(&dm->loopData, CD_MLOOP)) {
    CustomData_add_layer(&dm->loopData, CD_MLOOP, CD_ASSIGN, mloop, dm->numLoopData);
  }

  cddm->mloop = mloop;
}

void CDDM_set_mpoly(DerivedMesh *dm, MPoly *mpoly)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

  if (!CustomData_has_layer(&dm->polyData, CD_MPOLY)) {
    CustomData_add_layer(&dm->polyData, CD_MPOLY, CD_ASSIGN, mpoly, dm->numPolyData);
  }

  cddm->mpoly = mpoly;
}
