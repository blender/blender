/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.hh"
#include "BKE_mesh_legacy_derived_mesh.hh"

struct MeshElemMap;
struct PBVH;

/* -------------------------------------------------------------------- */

static float *dm_getVertArray(DerivedMesh *dm)
{
  float(*positions)[3] = (float(*)[3])CustomData_get_layer_named_for_write(
      &dm->vertData, CD_PROP_FLOAT3, "position", dm->getNumVerts(dm));

  if (!positions) {
    positions = (float(*)[3])CustomData_add_layer_named(
        &dm->vertData, CD_PROP_FLOAT3, CD_SET_DEFAULT, dm->getNumVerts(dm), "position");
    CustomData_set_layer_flag(&dm->vertData, CD_PROP_FLOAT3, CD_FLAG_TEMPORARY);
    dm->copyVertArray(dm, positions);
  }

  return (float *)positions;
}

static blender::int2 *dm_getEdgeArray(DerivedMesh *dm)
{
  blender::int2 *edge = (blender::int2 *)CustomData_get_layer_named_for_write(
      &dm->edgeData, CD_PROP_INT32_2D, ".edge_verts", dm->getNumEdges(dm));

  if (!edge) {
    edge = (blender::int2 *)CustomData_add_layer_named(
        &dm->edgeData, CD_PROP_INT32_2D, CD_SET_DEFAULT, dm->getNumEdges(dm), ".edge_verts");
    CustomData_set_layer_flag(&dm->edgeData, CD_PROP_INT32_2D, CD_FLAG_TEMPORARY);
    dm->copyEdgeArray(dm, edge);
  }

  return edge;
}

static int *dm_getCornerVertArray(DerivedMesh *dm)
{
  int *corner_verts = (int *)CustomData_get_layer_named_for_write(
      &dm->loopData, CD_PROP_INT32, ".corner_vert", dm->getNumLoops(dm));

  if (!corner_verts) {
    corner_verts = (int *)CustomData_add_layer_named(
        &dm->loopData, CD_PROP_INT32, CD_SET_DEFAULT, dm->getNumLoops(dm), ".corner_vert");
    dm->copyCornerVertArray(dm, corner_verts);
  }

  return corner_verts;
}

static int *dm_getCornerEdgeArray(DerivedMesh *dm)
{
  int *corner_edges = (int *)CustomData_get_layer_named(
      &dm->loopData, CD_PROP_INT32, ".corner_edge");

  if (!corner_edges) {
    corner_edges = (int *)CustomData_add_layer_named(
        &dm->loopData, CD_PROP_INT32, CD_SET_DEFAULT, dm->getNumLoops(dm), ".corner_edge");
    dm->copyCornerEdgeArray(dm, corner_edges);
  }

  return corner_edges;
}

static int *dm_getPolyArray(DerivedMesh *dm)
{
  if (!dm->face_offsets) {
    dm->face_offsets = MEM_cnew_array<int>(dm->getNumPolys(dm) + 1, __func__);
    dm->copyPolyArray(dm, dm->face_offsets);
  }
  return dm->face_offsets;
}

void DM_init_funcs(DerivedMesh *dm)
{
  /* default function implementations */
  dm->getVertArray = dm_getVertArray;
  dm->getEdgeArray = dm_getEdgeArray;
  dm->getCornerVertArray = dm_getCornerVertArray;
  dm->getCornerEdgeArray = dm_getCornerEdgeArray;
  dm->getPolyArray = dm_getPolyArray;

  dm->getVertDataArray = DM_get_vert_data_layer;
  dm->getEdgeDataArray = DM_get_edge_data_layer;
  dm->getPolyDataArray = DM_get_poly_data_layer;
  dm->getLoopDataArray = DM_get_loop_data_layer;
}

void DM_init(DerivedMesh *dm,
             DerivedMeshType type,
             int numVerts,
             int numEdges,
             int numTessFaces,
             int numLoops,
             int numPolys)
{
  dm->type = type;
  dm->numVertData = numVerts;
  dm->numEdgeData = numEdges;
  dm->numTessFaceData = numTessFaces;
  dm->numLoopData = numLoops;
  dm->numPolyData = numPolys;

  DM_init_funcs(dm);

  /* Don't use #CustomData_reset because we don't want to touch custom-data. */
  copy_vn_i(dm->vertData.typemap, CD_NUMTYPES, -1);
  copy_vn_i(dm->edgeData.typemap, CD_NUMTYPES, -1);
  copy_vn_i(dm->faceData.typemap, CD_NUMTYPES, -1);
  copy_vn_i(dm->loopData.typemap, CD_NUMTYPES, -1);
  copy_vn_i(dm->polyData.typemap, CD_NUMTYPES, -1);
}

void DM_from_template(DerivedMesh *dm,
                      DerivedMesh *source,
                      DerivedMeshType type,
                      int numVerts,
                      int numEdges,
                      int numTessFaces,
                      int numLoops,
                      int numPolys)
{
  const CustomData_MeshMasks *mask = &CD_MASK_DERIVEDMESH;
  CustomData_copy_layout(&source->vertData, &dm->vertData, mask->vmask, CD_SET_DEFAULT, numVerts);
  CustomData_copy_layout(&source->edgeData, &dm->edgeData, mask->emask, CD_SET_DEFAULT, numEdges);
  CustomData_copy_layout(
      &source->faceData, &dm->faceData, mask->fmask, CD_SET_DEFAULT, numTessFaces);
  CustomData_copy_layout(&source->loopData, &dm->loopData, mask->lmask, CD_SET_DEFAULT, numLoops);
  CustomData_copy_layout(&source->polyData, &dm->polyData, mask->pmask, CD_SET_DEFAULT, numPolys);
  dm->face_offsets = static_cast<int *>(MEM_dupallocN(source->face_offsets));

  dm->type = type;
  dm->numVertData = numVerts;
  dm->numEdgeData = numEdges;
  dm->numTessFaceData = numTessFaces;
  dm->numLoopData = numLoops;
  dm->numPolyData = numPolys;

  DM_init_funcs(dm);
}

void DM_release(DerivedMesh *dm)
{
  CustomData_free(&dm->vertData, dm->numVertData);
  CustomData_free(&dm->edgeData, dm->numEdgeData);
  CustomData_free(&dm->faceData, dm->numTessFaceData);
  CustomData_free(&dm->loopData, dm->numLoopData);
  CustomData_free(&dm->polyData, dm->numPolyData);
  MEM_SAFE_FREE(dm->face_offsets);
}

void DM_set_only_copy(DerivedMesh *dm, const CustomData_MeshMasks *mask)
{
  CustomData_set_only_copy(&dm->vertData, mask->vmask);
  CustomData_set_only_copy(&dm->edgeData, mask->emask);
  CustomData_set_only_copy(&dm->faceData, mask->fmask);
  /* this wasn't in 2.63 and is disabled for 2.64 because it gives problems with
   * weight paint mode when there are modifiers applied, needs further investigation,
   * see replies to r50969, Campbell */
#if 0
  CustomData_set_only_copy(&dm->loopData, mask->lmask);
  Custom(&dm->polyData, mask->pmask);
#endif
}

void *DM_get_vert_data_layer(DerivedMesh *dm, const eCustomDataType type)
{
  return CustomData_get_layer_for_write(&dm->vertData, type, dm->getNumVerts(dm));
}

void *DM_get_edge_data_layer(DerivedMesh *dm, const eCustomDataType type)
{
  return CustomData_get_layer_for_write(&dm->edgeData, type, dm->getNumEdges(dm));
}

void *DM_get_poly_data_layer(DerivedMesh *dm, const eCustomDataType type)
{
  return CustomData_get_layer_for_write(&dm->polyData, type, dm->getNumPolys(dm));
}

void *DM_get_loop_data_layer(DerivedMesh *dm, const eCustomDataType type)
{
  return CustomData_get_layer_for_write(&dm->loopData, type, dm->getNumLoops(dm));
}

void DM_copy_vert_data(
    const DerivedMesh *source, DerivedMesh *dest, int source_index, int dest_index, int count)
{
  CustomData_copy_data(&source->vertData, &dest->vertData, source_index, dest_index, count);
}

void DM_interp_vert_data(const DerivedMesh *source,
                         DerivedMesh *dest,
                         int *src_indices,
                         float *weights,
                         int count,
                         int dest_index)
{
  CustomData_interp(
      &source->vertData, &dest->vertData, src_indices, weights, nullptr, count, dest_index);
}

struct CDDerivedMesh {
  DerivedMesh dm;

  /* these point to data in the DerivedMesh custom data layers,
   * they are only here for efficiency and convenience */
  float (*vert_positions)[3];
  blender::int2 *medge;
  MFace *mface;
  int *corner_verts;
  int *corner_edges;
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

static void cdDM_copyEdgeArray(DerivedMesh *dm, blender::int2 *r_edge)
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

static void cdDM_release(DerivedMesh *dm)
{
  CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

  DM_release(dm);
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
          mesh->verts_num,
          mesh->edges_num,
          0 /* `mesh->totface` */,
          mesh->corners_num,
          mesh->faces_num);

  CustomData_merge(&mesh->vert_data, &dm->vertData, cddata_masks.vmask, mesh->verts_num);
  CustomData_merge(&mesh->edge_data, &dm->edgeData, cddata_masks.emask, mesh->edges_num);
  CustomData_merge(&mesh->fdata_legacy,
                   &dm->faceData,
                   cddata_masks.fmask | CD_MASK_ORIGINDEX,
                   0 /* `mesh->totface` */);
  CustomData_merge(&mesh->corner_data, &dm->loopData, cddata_masks.lmask, mesh->corners_num);
  CustomData_merge(&mesh->face_data, &dm->polyData, cddata_masks.pmask, mesh->faces_num);

  cddm->vert_positions = static_cast<float(*)[3]>(CustomData_get_layer_named_for_write(
      &dm->vertData, CD_PROP_FLOAT3, "position", mesh->verts_num));
  cddm->medge = static_cast<blender::int2 *>(CustomData_get_layer_named_for_write(
      &dm->edgeData, CD_PROP_INT32_2D, ".edge_verts", mesh->edges_num));
  cddm->corner_verts = static_cast<int *>(CustomData_get_layer_named_for_write(
      &dm->loopData, CD_PROP_INT32, ".corner_vert", mesh->corners_num));
  cddm->corner_edges = static_cast<int *>(CustomData_get_layer_named_for_write(
      &dm->loopData, CD_PROP_INT32, ".corner_edge", mesh->corners_num));
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
