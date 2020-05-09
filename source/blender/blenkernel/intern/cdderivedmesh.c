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

#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_curve.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"

#include "DNA_curve_types.h" /* for Curve */
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include <limits.h>
#include <math.h>
#include <string.h>

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

static void cdDM_recalc_looptri(DerivedMesh *dm)
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

  dm->recalcLoopTri = cdDM_recalc_looptri;

  dm->getVertCo = cdDM_getVertCo;
  dm->getVertNo = cdDM_getVertNo;

  dm->getPolyMap = cdDM_getPolyMap;

  dm->release = cdDM_release;

  return cddm;
}

static DerivedMesh *cdDM_from_mesh_ex(Mesh *mesh,
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

DerivedMesh *CDDM_from_mesh(Mesh *mesh)
{
  return cdDM_from_mesh_ex(mesh, CD_REFERENCE, &CD_MASK_MESH);
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
