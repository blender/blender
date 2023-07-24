/* SPDX-FileCopyrightText: 2006 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 * Implementation of #CDDerivedMesh.
 * BKE_cdderivedmesh.h contains the function prototypes for this file.
 */

#include <climits>
#include <cmath>
#include <cstring>

#include "atomic_ops.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_curve.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"

#include "DNA_curve_types.h" /* for Curve */
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

struct CDDerivedMesh {
  DerivedMesh dm;

  /* these point to data in the DerivedMesh custom data layers,
   * they are only here for efficiency and convenience */
  float (*vert_positions)[3];
  vec2i *medge;
  MFace *mface;
  int *corner_verts;
  int *corner_edges;

  /* Cached */
  PBVH *pbvh;
  bool pbvh_draw;

  /* Mesh connectivity */
  MeshElemMap *pmap;
  int *pmap_mem;
};

/**************** DerivedMesh interface functions ****************/
static int cdDM_getNumVerts(DerivedMesh *dm)
{
  return dm->numVertData;
}

static int cdDM_getNumEdges(DerivedMesh *dm)
{
  return dm->numEdgeData;
}

static int cdDM_getNumLoops(DerivedMesh *dm)
{
  return dm->numLoopData;
}

static int cdDM_getNumPolys(DerivedMesh *dm)
{
  return dm->numPolyData;
}

static void cdDM_copyVertArray(DerivedMesh *dm, float (*r_positions)[3])
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  memcpy(r_positions, cddm->vert_positions, sizeof(float[3]) * dm->numVertData);
}

static void cdDM_copyEdgeArray(DerivedMesh *dm, vec2i *r_edge)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  memcpy(r_edge, cddm->medge, sizeof(*r_edge) * dm->numEdgeData);
}

static void cdDM_copyCornerVertArray(DerivedMesh *dm, int *r_corner_verts)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  memcpy(r_corner_verts, cddm->corner_verts, sizeof(*r_corner_verts) * dm->numLoopData);
}

static void cdDM_copyCornerEdgeArray(DerivedMesh *dm, int *r_corner_edges)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
  memcpy(r_corner_edges, cddm->corner_edges, sizeof(*r_corner_edges) * dm->numLoopData);
}

static void cdDM_copyPolyArray(DerivedMesh *dm, int *r_face_offsets)
{
  memcpy(r_face_offsets, dm->face_offsets, sizeof(int) * (dm->numPolyData + 1));
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

  DM_release(dm);
  cdDM_free_internal(cddm);
  MEM_freeN(cddm);
}

/**************** CDDM interface functions ****************/
static CDDerivedMesh *cdDM_create(const char *desc)
{
  CDDerivedMesh *cddm = MEM_cnew<CDDerivedMesh>(desc);
  DerivedMesh *dm = &cddm->dm;

  dm->getNumVerts = cdDM_getNumVerts;
  dm->getNumEdges = cdDM_getNumEdges;
  dm->getNumLoops = cdDM_getNumLoops;
  dm->getNumPolys = cdDM_getNumPolys;

  dm->copyVertArray = cdDM_copyVertArray;
  dm->copyEdgeArray = cdDM_copyEdgeArray;
  dm->copyCornerVertArray = cdDM_copyCornerVertArray;
  dm->copyCornerEdgeArray = cdDM_copyCornerEdgeArray;
  dm->copyPolyArray = cdDM_copyPolyArray;

  dm->getVertDataArray = DM_get_vert_data_layer;
  dm->getEdgeDataArray = DM_get_edge_data_layer;

  dm->release = cdDM_release;

  return cddm;
}

static DerivedMesh *cdDM_from_mesh_ex(Mesh *mesh, const CustomData_MeshMasks *mask)
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
          0 /* `mesh->totface` */,
          mesh->totloop,
          mesh->faces_num);

  CustomData_merge(&mesh->vdata, &dm->vertData, cddata_masks.vmask, mesh->totvert);
  CustomData_merge(&mesh->edata, &dm->edgeData, cddata_masks.emask, mesh->totedge);
  CustomData_merge(&mesh->fdata_legacy,
                   &dm->faceData,
                   cddata_masks.fmask | CD_MASK_ORIGINDEX,
                   0 /* `mesh->totface` */);
  CustomData_merge(&mesh->ldata, &dm->loopData, cddata_masks.lmask, mesh->totloop);
  CustomData_merge(&mesh->pdata, &dm->polyData, cddata_masks.pmask, mesh->faces_num);

  cddm->vert_positions = static_cast<float(*)[3]>(CustomData_get_layer_named_for_write(
      &dm->vertData, CD_PROP_FLOAT3, "position", mesh->totvert));
  cddm->medge = static_cast<vec2i *>(CustomData_get_layer_named_for_write(
      &dm->edgeData, CD_PROP_INT32_2D, ".edge_verts", mesh->totedge));
  cddm->corner_verts = static_cast<int *>(CustomData_get_layer_named_for_write(
      &dm->loopData, CD_PROP_INT32, ".corner_vert", mesh->totloop));
  cddm->corner_edges = static_cast<int *>(CustomData_get_layer_named_for_write(
      &dm->loopData, CD_PROP_INT32, ".corner_edge", mesh->totloop));
  dm->face_offsets = static_cast<int *>(MEM_dupallocN(mesh->face_offset_indices));
#if 0
  cddm->mface = CustomData_get_layer(&dm->faceData, CD_MFACE);
#else
  cddm->mface = nullptr;
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
  return cdDM_from_mesh_ex(mesh, &CD_MASK_MESH);
}
