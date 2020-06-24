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

#include <limits.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_cloth_types.h"
#include "DNA_customdata_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_array.h"
#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_DerivedMesh.h"
#include "BKE_bvhutils.h"
#include "BKE_colorband.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mesh_iterators.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_tangent.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_object.h"
#include "BKE_object_deform.h"
#include "BKE_paint.h"

#include "BLI_sys_types.h" /* for intptr_t support */

#include "BKE_shrinkwrap.h"
#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "CLG_log.h"

#ifdef WITH_OPENSUBDIV
#  include "DNA_userdef_types.h"
#endif

/* very slow! enable for testing only! */
//#define USE_MODIFIER_VALIDATE

#ifdef USE_MODIFIER_VALIDATE
#  define ASSERT_IS_VALID_DM(dm) (BLI_assert((dm == NULL) || (DM_is_valid(dm) == true)))
#  define ASSERT_IS_VALID_MESH(mesh) \
    (BLI_assert((mesh == NULL) || (BKE_mesh_is_valid(mesh) == true)))
#else
#  define ASSERT_IS_VALID_DM(dm)
#  define ASSERT_IS_VALID_MESH(mesh)
#endif

static ThreadRWMutex loops_cache_lock = PTHREAD_RWLOCK_INITIALIZER;

static void mesh_init_origspace(Mesh *mesh);
static void editbmesh_calc_modifier_final_normals(Mesh *mesh_final,
                                                  const CustomData_MeshMasks *final_datamask);

/* -------------------------------------------------------------------- */

static MVert *dm_getVertArray(DerivedMesh *dm)
{
  MVert *mvert = CustomData_get_layer(&dm->vertData, CD_MVERT);

  if (!mvert) {
    mvert = CustomData_add_layer(&dm->vertData, CD_MVERT, CD_CALLOC, NULL, dm->getNumVerts(dm));
    CustomData_set_layer_flag(&dm->vertData, CD_MVERT, CD_FLAG_TEMPORARY);
    dm->copyVertArray(dm, mvert);
  }

  return mvert;
}

static MEdge *dm_getEdgeArray(DerivedMesh *dm)
{
  MEdge *medge = CustomData_get_layer(&dm->edgeData, CD_MEDGE);

  if (!medge) {
    medge = CustomData_add_layer(&dm->edgeData, CD_MEDGE, CD_CALLOC, NULL, dm->getNumEdges(dm));
    CustomData_set_layer_flag(&dm->edgeData, CD_MEDGE, CD_FLAG_TEMPORARY);
    dm->copyEdgeArray(dm, medge);
  }

  return medge;
}

static MFace *dm_getTessFaceArray(DerivedMesh *dm)
{
  MFace *mface = CustomData_get_layer(&dm->faceData, CD_MFACE);

  if (!mface) {
    int numTessFaces = dm->getNumTessFaces(dm);

    if (!numTessFaces) {
      /* Do not add layer if there's no elements in it, this leads to issues later when
       * this layer is needed with non-zero size, but currently CD stuff does not check
       * for requested layer size on creation and just returns layer which was previously
       * added (sergey) */
      return NULL;
    }

    mface = CustomData_add_layer(&dm->faceData, CD_MFACE, CD_CALLOC, NULL, numTessFaces);
    CustomData_set_layer_flag(&dm->faceData, CD_MFACE, CD_FLAG_TEMPORARY);
    dm->copyTessFaceArray(dm, mface);
  }

  return mface;
}

static MLoop *dm_getLoopArray(DerivedMesh *dm)
{
  MLoop *mloop = CustomData_get_layer(&dm->loopData, CD_MLOOP);

  if (!mloop) {
    mloop = CustomData_add_layer(&dm->loopData, CD_MLOOP, CD_CALLOC, NULL, dm->getNumLoops(dm));
    CustomData_set_layer_flag(&dm->loopData, CD_MLOOP, CD_FLAG_TEMPORARY);
    dm->copyLoopArray(dm, mloop);
  }

  return mloop;
}

static MPoly *dm_getPolyArray(DerivedMesh *dm)
{
  MPoly *mpoly = CustomData_get_layer(&dm->polyData, CD_MPOLY);

  if (!mpoly) {
    mpoly = CustomData_add_layer(&dm->polyData, CD_MPOLY, CD_CALLOC, NULL, dm->getNumPolys(dm));
    CustomData_set_layer_flag(&dm->polyData, CD_MPOLY, CD_FLAG_TEMPORARY);
    dm->copyPolyArray(dm, mpoly);
  }

  return mpoly;
}

static MVert *dm_dupVertArray(DerivedMesh *dm)
{
  MVert *tmp = MEM_malloc_arrayN(dm->getNumVerts(dm), sizeof(*tmp), "dm_dupVertArray tmp");

  if (tmp) {
    dm->copyVertArray(dm, tmp);
  }

  return tmp;
}

static MEdge *dm_dupEdgeArray(DerivedMesh *dm)
{
  MEdge *tmp = MEM_malloc_arrayN(dm->getNumEdges(dm), sizeof(*tmp), "dm_dupEdgeArray tmp");

  if (tmp) {
    dm->copyEdgeArray(dm, tmp);
  }

  return tmp;
}

static MFace *dm_dupFaceArray(DerivedMesh *dm)
{
  MFace *tmp = MEM_malloc_arrayN(dm->getNumTessFaces(dm), sizeof(*tmp), "dm_dupFaceArray tmp");

  if (tmp) {
    dm->copyTessFaceArray(dm, tmp);
  }

  return tmp;
}

static MLoop *dm_dupLoopArray(DerivedMesh *dm)
{
  MLoop *tmp = MEM_malloc_arrayN(dm->getNumLoops(dm), sizeof(*tmp), "dm_dupLoopArray tmp");

  if (tmp) {
    dm->copyLoopArray(dm, tmp);
  }

  return tmp;
}

static MPoly *dm_dupPolyArray(DerivedMesh *dm)
{
  MPoly *tmp = MEM_malloc_arrayN(dm->getNumPolys(dm), sizeof(*tmp), "dm_dupPolyArray tmp");

  if (tmp) {
    dm->copyPolyArray(dm, tmp);
  }

  return tmp;
}

static int dm_getNumLoopTri(DerivedMesh *dm)
{
  const int numlooptris = poly_to_tri_count(dm->getNumPolys(dm), dm->getNumLoops(dm));
  BLI_assert(ELEM(dm->looptris.num, 0, numlooptris));
  return numlooptris;
}

static const MLoopTri *dm_getLoopTriArray(DerivedMesh *dm)
{
  MLoopTri *looptri;

  BLI_rw_mutex_lock(&loops_cache_lock, THREAD_LOCK_READ);
  looptri = dm->looptris.array;
  BLI_rw_mutex_unlock(&loops_cache_lock);

  if (looptri != NULL) {
    BLI_assert(dm->getNumLoopTri(dm) == dm->looptris.num);
  }
  else {
    BLI_rw_mutex_lock(&loops_cache_lock, THREAD_LOCK_WRITE);
    /* We need to ensure array is still NULL inside mutex-protected code,
     * some other thread might have already recomputed those looptris. */
    if (dm->looptris.array == NULL) {
      dm->recalcLoopTri(dm);
    }
    looptri = dm->looptris.array;
    BLI_rw_mutex_unlock(&loops_cache_lock);
  }
  return looptri;
}

static CustomData *dm_getVertCData(DerivedMesh *dm)
{
  return &dm->vertData;
}

static CustomData *dm_getEdgeCData(DerivedMesh *dm)
{
  return &dm->edgeData;
}

static CustomData *dm_getTessFaceCData(DerivedMesh *dm)
{
  return &dm->faceData;
}

static CustomData *dm_getLoopCData(DerivedMesh *dm)
{
  return &dm->loopData;
}

static CustomData *dm_getPolyCData(DerivedMesh *dm)
{
  return &dm->polyData;
}

/**
 * Utility function to initialize a DerivedMesh's function pointers to
 * the default implementation (for those functions which have a default)
 */
void DM_init_funcs(DerivedMesh *dm)
{
  /* default function implementations */
  dm->getVertArray = dm_getVertArray;
  dm->getEdgeArray = dm_getEdgeArray;
  dm->getTessFaceArray = dm_getTessFaceArray;
  dm->getLoopArray = dm_getLoopArray;
  dm->getPolyArray = dm_getPolyArray;
  dm->dupVertArray = dm_dupVertArray;
  dm->dupEdgeArray = dm_dupEdgeArray;
  dm->dupTessFaceArray = dm_dupFaceArray;
  dm->dupLoopArray = dm_dupLoopArray;
  dm->dupPolyArray = dm_dupPolyArray;

  dm->getLoopTriArray = dm_getLoopTriArray;

  /* subtypes handle getting actual data */
  dm->getNumLoopTri = dm_getNumLoopTri;

  dm->getVertDataLayout = dm_getVertCData;
  dm->getEdgeDataLayout = dm_getEdgeCData;
  dm->getTessFaceDataLayout = dm_getTessFaceCData;
  dm->getLoopDataLayout = dm_getLoopCData;
  dm->getPolyDataLayout = dm_getPolyCData;

  dm->getVertData = DM_get_vert_data;
  dm->getEdgeData = DM_get_edge_data;
  dm->getTessFaceData = DM_get_tessface_data;
  dm->getPolyData = DM_get_poly_data;
  dm->getVertDataArray = DM_get_vert_data_layer;
  dm->getEdgeDataArray = DM_get_edge_data_layer;
  dm->getTessFaceDataArray = DM_get_tessface_data_layer;
  dm->getPolyDataArray = DM_get_poly_data_layer;
  dm->getLoopDataArray = DM_get_loop_data_layer;
}

/**
 * Utility function to initialize a DerivedMesh for the desired number
 * of vertices, edges and faces (doesn't allocate memory for them, just
 * sets up the custom data layers)
 */
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

  dm->needsFree = 1;
  dm->dirty = 0;

  /* don't use CustomData_reset(...); because we dont want to touch customdata */
  copy_vn_i(dm->vertData.typemap, CD_NUMTYPES, -1);
  copy_vn_i(dm->edgeData.typemap, CD_NUMTYPES, -1);
  copy_vn_i(dm->faceData.typemap, CD_NUMTYPES, -1);
  copy_vn_i(dm->loopData.typemap, CD_NUMTYPES, -1);
  copy_vn_i(dm->polyData.typemap, CD_NUMTYPES, -1);
}

/**
 * Utility function to initialize a DerivedMesh for the desired number
 * of vertices, edges and faces, with a layer setup copied from source
 */
void DM_from_template_ex(DerivedMesh *dm,
                         DerivedMesh *source,
                         DerivedMeshType type,
                         int numVerts,
                         int numEdges,
                         int numTessFaces,
                         int numLoops,
                         int numPolys,
                         const CustomData_MeshMasks *mask)
{
  CustomData_copy(&source->vertData, &dm->vertData, mask->vmask, CD_CALLOC, numVerts);
  CustomData_copy(&source->edgeData, &dm->edgeData, mask->emask, CD_CALLOC, numEdges);
  CustomData_copy(&source->faceData, &dm->faceData, mask->fmask, CD_CALLOC, numTessFaces);
  CustomData_copy(&source->loopData, &dm->loopData, mask->lmask, CD_CALLOC, numLoops);
  CustomData_copy(&source->polyData, &dm->polyData, mask->pmask, CD_CALLOC, numPolys);

  dm->cd_flag = source->cd_flag;

  dm->type = type;
  dm->numVertData = numVerts;
  dm->numEdgeData = numEdges;
  dm->numTessFaceData = numTessFaces;
  dm->numLoopData = numLoops;
  dm->numPolyData = numPolys;

  DM_init_funcs(dm);

  dm->needsFree = 1;
  dm->dirty = 0;
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
  DM_from_template_ex(dm,
                      source,
                      type,
                      numVerts,
                      numEdges,
                      numTessFaces,
                      numLoops,
                      numPolys,
                      &CD_MASK_DERIVEDMESH);
}

int DM_release(DerivedMesh *dm)
{
  if (dm->needsFree) {
    CustomData_free(&dm->vertData, dm->numVertData);
    CustomData_free(&dm->edgeData, dm->numEdgeData);
    CustomData_free(&dm->faceData, dm->numTessFaceData);
    CustomData_free(&dm->loopData, dm->numLoopData);
    CustomData_free(&dm->polyData, dm->numPolyData);

    MEM_SAFE_FREE(dm->looptris.array);
    dm->looptris.num = 0;
    dm->looptris.num_alloc = 0;

    return 1;
  }
  else {
    CustomData_free_temporary(&dm->vertData, dm->numVertData);
    CustomData_free_temporary(&dm->edgeData, dm->numEdgeData);
    CustomData_free_temporary(&dm->faceData, dm->numTessFaceData);
    CustomData_free_temporary(&dm->loopData, dm->numLoopData);
    CustomData_free_temporary(&dm->polyData, dm->numPolyData);

    return 0;
  }
}

void DM_DupPolys(DerivedMesh *source, DerivedMesh *target)
{
  CustomData_free(&target->loopData, source->numLoopData);
  CustomData_free(&target->polyData, source->numPolyData);

  CustomData_copy(&source->loopData,
                  &target->loopData,
                  CD_MASK_DERIVEDMESH.lmask,
                  CD_DUPLICATE,
                  source->numLoopData);
  CustomData_copy(&source->polyData,
                  &target->polyData,
                  CD_MASK_DERIVEDMESH.pmask,
                  CD_DUPLICATE,
                  source->numPolyData);

  target->numLoopData = source->numLoopData;
  target->numPolyData = source->numPolyData;

  if (!CustomData_has_layer(&target->polyData, CD_MPOLY)) {
    MPoly *mpoly;
    MLoop *mloop;

    mloop = source->dupLoopArray(source);
    mpoly = source->dupPolyArray(source);
    CustomData_add_layer(&target->loopData, CD_MLOOP, CD_ASSIGN, mloop, source->numLoopData);
    CustomData_add_layer(&target->polyData, CD_MPOLY, CD_ASSIGN, mpoly, source->numPolyData);
  }
}

void DM_ensure_normals(DerivedMesh *dm)
{
  if (dm->dirty & DM_DIRTY_NORMALS) {
    dm->calcNormals(dm);
  }
  BLI_assert((dm->dirty & DM_DIRTY_NORMALS) == 0);
}

/**
 * Ensure the array is large enough
 *
 * \note This function must always be thread-protected by caller.
 * It should only be used by internal code.
 */
void DM_ensure_looptri_data(DerivedMesh *dm)
{
  const unsigned int totpoly = dm->numPolyData;
  const unsigned int totloop = dm->numLoopData;
  const int looptris_num = poly_to_tri_count(totpoly, totloop);

  BLI_assert(dm->looptris.array_wip == NULL);

  SWAP(MLoopTri *, dm->looptris.array, dm->looptris.array_wip);

  if ((looptris_num > dm->looptris.num_alloc) || (looptris_num < dm->looptris.num_alloc * 2) ||
      (totpoly == 0)) {
    MEM_SAFE_FREE(dm->looptris.array_wip);
    dm->looptris.num_alloc = 0;
    dm->looptris.num = 0;
  }

  if (totpoly) {
    if (dm->looptris.array_wip == NULL) {
      dm->looptris.array_wip = MEM_malloc_arrayN(
          looptris_num, sizeof(*dm->looptris.array_wip), __func__);
      dm->looptris.num_alloc = looptris_num;
    }

    dm->looptris.num = looptris_num;
  }
}

/** Utility function to convert an (evaluated) Mesh to a shape key block. */
/* Just a shallow wrapper around BKE_keyblock_convert_from_mesh,
 * that ensures both evaluated mesh and original one has same number of vertices. */
void BKE_mesh_runtime_eval_to_meshkey(Mesh *me_deformed, Mesh *me, KeyBlock *kb)
{
  const int totvert = me_deformed->totvert;

  if (totvert == 0 || me->totvert == 0 || me->totvert != totvert) {
    return;
  }

  BKE_keyblock_convert_from_mesh(me_deformed, me->key, kb);
}

/**
 * set the CD_FLAG_NOCOPY flag in custom data layers where the mask is
 * zero for the layer type, so only layer types specified by the mask
 * will be copied
 */
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
  CustomData_set_only_copy(&dm->polyData, mask->pmask);
#endif
}

static void mesh_set_only_copy(Mesh *mesh, const CustomData_MeshMasks *mask)
{
  CustomData_set_only_copy(&mesh->vdata, mask->vmask);
  CustomData_set_only_copy(&mesh->edata, mask->emask);
  CustomData_set_only_copy(&mesh->fdata, mask->fmask);
  /* this wasn't in 2.63 and is disabled for 2.64 because it gives problems with
   * weight paint mode when there are modifiers applied, needs further investigation,
   * see replies to r50969, Campbell */
#if 0
  CustomData_set_only_copy(&mesh->ldata, mask->lmask);
  CustomData_set_only_copy(&mesh->pdata, mask->pmask);
#endif
}

void DM_add_vert_layer(DerivedMesh *dm, int type, eCDAllocType alloctype, void *layer)
{
  CustomData_add_layer(&dm->vertData, type, alloctype, layer, dm->numVertData);
}

void DM_add_edge_layer(DerivedMesh *dm, int type, eCDAllocType alloctype, void *layer)
{
  CustomData_add_layer(&dm->edgeData, type, alloctype, layer, dm->numEdgeData);
}

void DM_add_tessface_layer(DerivedMesh *dm, int type, eCDAllocType alloctype, void *layer)
{
  CustomData_add_layer(&dm->faceData, type, alloctype, layer, dm->numTessFaceData);
}

void DM_add_loop_layer(DerivedMesh *dm, int type, eCDAllocType alloctype, void *layer)
{
  CustomData_add_layer(&dm->loopData, type, alloctype, layer, dm->numLoopData);
}

void DM_add_poly_layer(DerivedMesh *dm, int type, eCDAllocType alloctype, void *layer)
{
  CustomData_add_layer(&dm->polyData, type, alloctype, layer, dm->numPolyData);
}

void *DM_get_vert_data(DerivedMesh *dm, int index, int type)
{
  BLI_assert(index >= 0 && index < dm->getNumVerts(dm));
  return CustomData_get(&dm->vertData, index, type);
}

void *DM_get_edge_data(DerivedMesh *dm, int index, int type)
{
  BLI_assert(index >= 0 && index < dm->getNumEdges(dm));
  return CustomData_get(&dm->edgeData, index, type);
}

void *DM_get_tessface_data(DerivedMesh *dm, int index, int type)
{
  BLI_assert(index >= 0 && index < dm->getNumTessFaces(dm));
  return CustomData_get(&dm->faceData, index, type);
}

void *DM_get_poly_data(DerivedMesh *dm, int index, int type)
{
  BLI_assert(index >= 0 && index < dm->getNumPolys(dm));
  return CustomData_get(&dm->polyData, index, type);
}

void *DM_get_vert_data_layer(DerivedMesh *dm, int type)
{
  if (type == CD_MVERT) {
    return dm->getVertArray(dm);
  }

  return CustomData_get_layer(&dm->vertData, type);
}

void *DM_get_edge_data_layer(DerivedMesh *dm, int type)
{
  if (type == CD_MEDGE) {
    return dm->getEdgeArray(dm);
  }

  return CustomData_get_layer(&dm->edgeData, type);
}

void *DM_get_tessface_data_layer(DerivedMesh *dm, int type)
{
  if (type == CD_MFACE) {
    return dm->getTessFaceArray(dm);
  }

  return CustomData_get_layer(&dm->faceData, type);
}

void *DM_get_poly_data_layer(DerivedMesh *dm, int type)
{
  return CustomData_get_layer(&dm->polyData, type);
}

void *DM_get_loop_data_layer(DerivedMesh *dm, int type)
{
  return CustomData_get_layer(&dm->loopData, type);
}

void DM_copy_vert_data(
    DerivedMesh *source, DerivedMesh *dest, int source_index, int dest_index, int count)
{
  CustomData_copy_data(&source->vertData, &dest->vertData, source_index, dest_index, count);
}

/**
 * interpolates vertex data from the vertices indexed by src_indices in the
 * source mesh using the given weights and stores the result in the vertex
 * indexed by dest_index in the dest mesh
 */
void DM_interp_vert_data(DerivedMesh *source,
                         DerivedMesh *dest,
                         int *src_indices,
                         float *weights,
                         int count,
                         int dest_index)
{
  CustomData_interp(
      &source->vertData, &dest->vertData, src_indices, weights, NULL, count, dest_index);
}

static float (*get_editbmesh_orco_verts(BMEditMesh *em))[3]
{
  BMIter iter;
  BMVert *eve;
  float(*orco)[3];
  int i;

  /* these may not really be the orco's, but it's only for preview.
   * could be solver better once, but isn't simple */

  orco = MEM_malloc_arrayN(em->bm->totvert, sizeof(float) * 3, "BMEditMesh Orco");

  BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
    copy_v3_v3(orco[i], eve->co);
  }

  return orco;
}

/* orco custom data layer */
static float (*get_orco_coords(Object *ob, BMEditMesh *em, int layer, int *free))[3]
{
  *free = 0;

  if (layer == CD_ORCO) {
    /* get original coordinates */
    *free = 1;

    if (em) {
      return get_editbmesh_orco_verts(em);
    }
    else {
      return BKE_mesh_orco_verts_get(ob);
    }
  }
  else if (layer == CD_CLOTH_ORCO) {
    /* apply shape key for cloth, this should really be solved
     * by a more flexible customdata system, but not simple */
    if (!em) {
      ClothModifierData *clmd = (ClothModifierData *)BKE_modifiers_findby_type(
          ob, eModifierType_Cloth);
      KeyBlock *kb = BKE_keyblock_from_key(BKE_key_from_object(ob),
                                           clmd->sim_parms->shapekey_rest);

      if (kb && kb->data) {
        return kb->data;
      }
    }

    return NULL;
  }

  return NULL;
}

static Mesh *create_orco_mesh(Object *ob, Mesh *me, BMEditMesh *em, int layer)
{
  Mesh *mesh;
  float(*orco)[3];
  int free;

  if (em) {
    mesh = BKE_mesh_from_bmesh_for_eval_nomain(em->bm, NULL, me);
  }
  else {
    mesh = BKE_mesh_copy_for_eval(me, true);
  }

  orco = get_orco_coords(ob, em, layer, &free);

  if (orco) {
    BKE_mesh_vert_coords_apply(mesh, orco);
    if (free) {
      MEM_freeN(orco);
    }
  }

  return mesh;
}

static void add_orco_mesh(Object *ob, BMEditMesh *em, Mesh *mesh, Mesh *mesh_orco, int layer)
{
  float(*orco)[3], (*layerorco)[3];
  int totvert, free;

  totvert = mesh->totvert;

  if (mesh_orco) {
    free = 1;

    if (mesh_orco->totvert == totvert) {
      orco = BKE_mesh_vert_coords_alloc(mesh_orco, NULL);
    }
    else {
      orco = BKE_mesh_vert_coords_alloc(mesh, NULL);
    }
  }
  else {
    /* TODO(sybren): totvert should potentially change here, as ob->data
     * or em may have a different number of vertices than dm. */
    orco = get_orco_coords(ob, em, layer, &free);
  }

  if (orco) {
    if (layer == CD_ORCO) {
      BKE_mesh_orco_verts_transform(ob->data, orco, totvert, 0);
    }

    if (!(layerorco = CustomData_get_layer(&mesh->vdata, layer))) {
      CustomData_add_layer(&mesh->vdata, layer, CD_CALLOC, NULL, mesh->totvert);
      BKE_mesh_update_customdata_pointers(mesh, false);

      layerorco = CustomData_get_layer(&mesh->vdata, layer);
    }

    memcpy(layerorco, orco, sizeof(float) * 3 * totvert);
    if (free) {
      MEM_freeN(orco);
    }
  }
}

static void mesh_calc_modifier_final_normals(const Mesh *mesh_input,
                                             const CustomData_MeshMasks *final_datamask,
                                             const bool sculpt_dyntopo,
                                             Mesh *mesh_final)
{
  /* Compute normals. */
  const bool do_loop_normals = ((mesh_input->flag & ME_AUTOSMOOTH) != 0 ||
                                (final_datamask->lmask & CD_MASK_NORMAL) != 0);
  /* Some modifiers may need this info from their target (other) object,
   * simpler to generate it here as well.
   * Note that they will always be generated when no loop normals are computed,
   * since they are needed by drawing code. */
  const bool do_poly_normals = ((final_datamask->pmask & CD_MASK_NORMAL) != 0);

  /* In case we also need poly normals, add the layer and compute them here
   * (BKE_mesh_calc_normals_split() assumes that if that data exists, it is always valid). */
  if (do_poly_normals) {
    if (!CustomData_has_layer(&mesh_final->pdata, CD_NORMAL)) {
      float(*polynors)[3] = CustomData_add_layer(
          &mesh_final->pdata, CD_NORMAL, CD_CALLOC, NULL, mesh_final->totpoly);
      BKE_mesh_calc_normals_poly(mesh_final->mvert,
                                 NULL,
                                 mesh_final->totvert,
                                 mesh_final->mloop,
                                 mesh_final->mpoly,
                                 mesh_final->totloop,
                                 mesh_final->totpoly,
                                 polynors,
                                 false);
    }
  }

  if (do_loop_normals) {
    /* Compute loop normals (note: will compute poly and vert normals as well, if needed!) */
    BKE_mesh_calc_normals_split(mesh_final);
    BKE_mesh_tessface_clear(mesh_final);
  }

  if (sculpt_dyntopo == false) {
    /* watch this! after 2.75a we move to from tessface to looptri (by default) */
    if (final_datamask->fmask & CD_MASK_MFACE) {
      BKE_mesh_tessface_ensure(mesh_final);
    }

    /* without this, drawing ngon tri's faces will show ugly tessellated face
     * normals and will also have to calculate normals on the fly, try avoid
     * this where possible since calculating polygon normals isn't fast,
     * note that this isn't a problem for subsurf (only quads) or editmode
     * which deals with drawing differently.
     *
     * Only calc vertex normals if they are flagged as dirty.
     * If using loop normals, poly nors have already been computed.
     */
    if (!do_loop_normals) {
      BKE_mesh_ensure_normals_for_display(mesh_final);
    }
  }

  /* Some modifiers, like data-transfer, may generate those data as temp layer,
   * we do not want to keep them, as they are used by display code when available
   * (i.e. even if autosmooth is disabled). */
  if (!do_loop_normals && CustomData_has_layer(&mesh_final->ldata, CD_NORMAL)) {
    CustomData_free_layers(&mesh_final->ldata, CD_NORMAL, mesh_final->totloop);
  }
}

/* Does final touches to the final evaluated mesh, making sure it is perfectly usable.
 *
 * This is needed because certain information is not passed along intermediate meshes allocated
 * during stack evaluation.
 */
static void mesh_calc_finalize(const Mesh *mesh_input, Mesh *mesh_eval)
{
  /* Make sure the name is the same. This is because mesh allocation from template does not
   * take care of naming. */
  BLI_strncpy(mesh_eval->id.name, mesh_input->id.name, sizeof(mesh_eval->id.name));
  /* Make evaluated mesh to share same edit mesh pointer as original and copied meshes. */
  mesh_eval->edit_mesh = mesh_input->edit_mesh;
}

void BKE_mesh_wrapper_deferred_finalize(Mesh *me_eval,
                                        const CustomData_MeshMasks *cd_mask_finalize)
{
  if (me_eval->runtime.wrapper_type_finalize & (1 << ME_WRAPPER_TYPE_BMESH)) {
    editbmesh_calc_modifier_final_normals(me_eval, cd_mask_finalize);
    me_eval->runtime.wrapper_type_finalize &= ~(1 << ME_WRAPPER_TYPE_BMESH);
  }
  BLI_assert(me_eval->runtime.wrapper_type_finalize == 0);
}

static void mesh_calc_modifiers(struct Depsgraph *depsgraph,
                                Scene *scene,
                                Object *ob,
                                int useDeform,
                                const bool need_mapping,
                                const CustomData_MeshMasks *dataMask,
                                const int index,
                                const bool use_cache,
                                const bool allow_shared_mesh,
                                /* return args */
                                Mesh **r_deform,
                                Mesh **r_final)
{
  /* Input and final mesh. Final mesh is only created the moment the first
   * constructive modifier is executed, or a deform modifier needs normals
   * or certain data layers. */
  Mesh *mesh_input = ob->data;
  Mesh *mesh_final = NULL;
  Mesh *mesh_deform = NULL;
  BLI_assert((mesh_input->id.tag & LIB_TAG_COPIED_ON_WRITE_EVAL_RESULT) == 0);

  /* Deformed vertex locations array. Deform only modifier need this type of
   * float array rather than MVert*. Tracked along with mesh_final as an
   * optimization to avoid copying coordinates back and forth if there are
   * multiple sequential deform only modifiers. */
  float(*deformed_verts)[3] = NULL;
  int num_deformed_verts = mesh_input->totvert;
  bool isPrevDeform = false;

  /* Mesh with constructive modifiers but no deformation applied. Tracked
   * along with final mesh if undeformed / orco coordinates are requested
   * for texturing. */
  Mesh *mesh_orco = NULL;
  Mesh *mesh_orco_cloth = NULL;

  /* Modifier evaluation modes. */
  const bool use_render = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  const int required_mode = use_render ? eModifierMode_Render : eModifierMode_Realtime;

  /* Sculpt can skip certain modifiers. */
  MultiresModifierData *mmd = get_multires_modifier(scene, ob, 0);
  const bool has_multires = (mmd && mmd->sculptlvl != 0);
  bool multires_applied = false;
  const bool sculpt_mode = ob->mode & OB_MODE_SCULPT && ob->sculpt && !use_render;
  const bool sculpt_dyntopo = (sculpt_mode && ob->sculpt->bm) && !use_render;

  /* Modifier evaluation contexts for different types of modifiers. */
  ModifierApplyFlag apply_render = use_render ? MOD_APPLY_RENDER : 0;
  ModifierApplyFlag apply_cache = use_cache ? MOD_APPLY_USECACHE : 0;
  const ModifierEvalContext mectx = {depsgraph, ob, apply_render | apply_cache};
  const ModifierEvalContext mectx_orco = {depsgraph, ob, apply_render | MOD_APPLY_ORCO};

  /* Get effective list of modifiers to execute. Some effects like shape keys
   * are added as virtual modifiers before the user created modifiers. */
  VirtualModifierData virtualModifierData;
  ModifierData *firstmd = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);
  ModifierData *md = firstmd;

  /* Preview colors by modifiers such as dynamic paint, to show the results
   * even if the resulting data is not used in a material. Only in object mode.
   * TODO: this is broken, not drawn by the drawn manager. */
  const bool do_mod_mcol = (ob->mode == OB_MODE_OBJECT);
  ModifierData *previewmd = NULL;
  CustomData_MeshMasks previewmask = {0};
  if (do_mod_mcol) {
    /* Find the last active modifier generating a preview, or NULL if none. */
    /* XXX Currently, DPaint modifier just ignores this.
     *     Needs a stupid hack...
     *     The whole "modifier preview" thing has to be (re?)designed, anyway! */
    previewmd = BKE_modifier_get_last_preview(scene, md, required_mode);
  }

  /* Compute accumulated datamasks needed by each modifier. It helps to do
   * this fine grained so that for example vertex groups are preserved up to
   * an armature modifier, but not through a following subsurf modifier where
   * subdividing them is expensive. */
  CustomData_MeshMasks final_datamask = *dataMask;
  CDMaskLink *datamasks = BKE_modifier_calc_data_masks(
      scene, ob, md, &final_datamask, required_mode, previewmd, &previewmask);
  CDMaskLink *md_datamask = datamasks;
  /* XXX Always copying POLYINDEX, else tessellated data are no more valid! */
  CustomData_MeshMasks append_mask = CD_MASK_BAREMESH_ORIGINDEX;

  /* Clear errors before evaluation. */
  BKE_modifiers_clear_errors(ob);

  /* Apply all leading deform modifiers. */
  if (useDeform) {
    for (; md; md = md->next, md_datamask = md_datamask->next) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);

      if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
        continue;
      }

      if (useDeform < 0 && mti->dependsOnTime && mti->dependsOnTime(md)) {
        continue;
      }

      if (mti->type == eModifierTypeType_OnlyDeform && !sculpt_dyntopo) {
        if (!deformed_verts) {
          deformed_verts = BKE_mesh_vert_coords_alloc(mesh_input, &num_deformed_verts);
        }
        else if (isPrevDeform && mti->dependsOnNormals && mti->dependsOnNormals(md)) {
          if (mesh_final == NULL) {
            mesh_final = BKE_mesh_copy_for_eval(mesh_input, true);
            ASSERT_IS_VALID_MESH(mesh_final);
          }
          BKE_mesh_vert_coords_apply(mesh_final, deformed_verts);
        }

        BKE_modifier_deform_verts(md, &mectx, mesh_final, deformed_verts, num_deformed_verts);

        isPrevDeform = true;
      }
      else {
        break;
      }

      /* grab modifiers until index i */
      if ((index != -1) && (BLI_findindex(&ob->modifiers, md) >= index)) {
        md = NULL;
        break;
      }
    }

    /* Result of all leading deforming modifiers is cached for
     * places that wish to use the original mesh but with deformed
     * coordinates (like vertex paint). */
    if (r_deform) {
      mesh_deform = BKE_mesh_copy_for_eval(mesh_input, true);

      if (deformed_verts) {
        BKE_mesh_vert_coords_apply(mesh_deform, deformed_verts);
      }
    }
  }

  /* Apply all remaining constructive and deforming modifiers. */
  bool have_non_onlydeform_modifiers_appled = false;
  for (; md; md = md->next, md_datamask = md_datamask->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);

    if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
      continue;
    }

    if (mti->type == eModifierTypeType_OnlyDeform && !useDeform) {
      continue;
    }

    if ((mti->flags & eModifierTypeFlag_RequiresOriginalData) &&
        have_non_onlydeform_modifiers_appled) {
      BKE_modifier_set_error(md, "Modifier requires original data, bad stack position");
      continue;
    }

    if (sculpt_mode && (!has_multires || multires_applied || sculpt_dyntopo)) {
      bool unsupported = false;

      if (md->type == eModifierType_Multires && ((MultiresModifierData *)md)->sculptlvl == 0) {
        /* If multires is on level 0 skip it silently without warning message. */
        if (!sculpt_dyntopo) {
          continue;
        }
      }

      if (sculpt_dyntopo) {
        unsupported = true;
      }

      if (scene->toolsettings->sculpt->flags & SCULPT_ONLY_DEFORM) {
        unsupported |= (mti->type != eModifierTypeType_OnlyDeform);
      }

      unsupported |= multires_applied;

      if (unsupported) {
        if (sculpt_dyntopo) {
          BKE_modifier_set_error(md, "Not supported in dyntopo");
        }
        else {
          BKE_modifier_set_error(md, "Not supported in sculpt mode");
        }
        continue;
      }
      else {
        BKE_modifier_set_error(md, "Sculpt: Hide, Mask and optimized display disabled");
      }
    }

    if (need_mapping && !BKE_modifier_supports_mapping(md)) {
      continue;
    }

    if (useDeform < 0 && mti->dependsOnTime && mti->dependsOnTime(md)) {
      continue;
    }

    /* Add orco mesh as layer if needed by this modifier. */
    if (mesh_final && mesh_orco && mti->requiredDataMask) {
      CustomData_MeshMasks mask = {0};
      mti->requiredDataMask(ob, md, &mask);
      if (mask.vmask & CD_MASK_ORCO) {
        add_orco_mesh(ob, NULL, mesh_final, mesh_orco, CD_ORCO);
      }
    }

    /* How to apply modifier depends on (a) what we already have as
     * a result of previous modifiers (could be a Mesh or just
     * deformed vertices) and (b) what type the modifier is. */
    if (mti->type == eModifierTypeType_OnlyDeform) {
      /* No existing verts to deform, need to build them. */
      if (!deformed_verts) {
        if (mesh_final) {
          /* Deforming a mesh, read the vertex locations
           * out of the mesh and deform them. Once done with this
           * run of deformers verts will be written back. */
          deformed_verts = BKE_mesh_vert_coords_alloc(mesh_final, &num_deformed_verts);
        }
        else {
          deformed_verts = BKE_mesh_vert_coords_alloc(mesh_input, &num_deformed_verts);
        }
      }
      /* if this is not the last modifier in the stack then recalculate the normals
       * to avoid giving bogus normals to the next modifier see: [#23673] */
      else if (isPrevDeform && mti->dependsOnNormals && mti->dependsOnNormals(md)) {
        if (mesh_final == NULL) {
          mesh_final = BKE_mesh_copy_for_eval(mesh_input, true);
          ASSERT_IS_VALID_MESH(mesh_final);
        }
        BKE_mesh_vert_coords_apply(mesh_final, deformed_verts);
      }
      BKE_modifier_deform_verts(md, &mectx, mesh_final, deformed_verts, num_deformed_verts);
    }
    else {
      have_non_onlydeform_modifiers_appled = true;

      /* determine which data layers are needed by following modifiers */
      CustomData_MeshMasks nextmask;
      if (md_datamask->next) {
        nextmask = md_datamask->next->mask;
      }
      else {
        nextmask = final_datamask;
      }

      /* apply vertex coordinates or build a Mesh as necessary */
      if (mesh_final) {
        if (deformed_verts) {
          BKE_mesh_vert_coords_apply(mesh_final, deformed_verts);
        }
      }
      else {
        mesh_final = BKE_mesh_copy_for_eval(mesh_input, true);
        ASSERT_IS_VALID_MESH(mesh_final);

        if (deformed_verts) {
          BKE_mesh_vert_coords_apply(mesh_final, deformed_verts);
        }

        /* Initialize original indices the first time we evaluate a
         * constructive modifier. Modifiers will then do mapping mostly
         * automatic by copying them through CustomData_copy_data along
         * with other data.
         *
         * These are created when either requested by evaluation, or if
         * following modifiers requested them. */
        if (need_mapping ||
            ((nextmask.vmask | nextmask.emask | nextmask.pmask) & CD_MASK_ORIGINDEX)) {
          /* calc */
          CustomData_add_layer(
              &mesh_final->vdata, CD_ORIGINDEX, CD_CALLOC, NULL, mesh_final->totvert);
          CustomData_add_layer(
              &mesh_final->edata, CD_ORIGINDEX, CD_CALLOC, NULL, mesh_final->totedge);
          CustomData_add_layer(
              &mesh_final->pdata, CD_ORIGINDEX, CD_CALLOC, NULL, mesh_final->totpoly);

          /* Not worth parallelizing this,
           * gives less than 0.1% overall speedup in best of best cases... */
          range_vn_i(
              CustomData_get_layer(&mesh_final->vdata, CD_ORIGINDEX), mesh_final->totvert, 0);
          range_vn_i(
              CustomData_get_layer(&mesh_final->edata, CD_ORIGINDEX), mesh_final->totedge, 0);
          range_vn_i(
              CustomData_get_layer(&mesh_final->pdata, CD_ORIGINDEX), mesh_final->totpoly, 0);
        }
      }

      /* set the Mesh to only copy needed data */
      CustomData_MeshMasks mask = md_datamask->mask;
      /* needMapping check here fixes bug [#28112], otherwise it's
       * possible that it won't be copied */
      CustomData_MeshMasks_update(&mask, &append_mask);
      if (need_mapping) {
        mask.vmask |= CD_MASK_ORIGINDEX;
        mask.emask |= CD_MASK_ORIGINDEX;
        mask.pmask |= CD_MASK_ORIGINDEX;
      }
      mesh_set_only_copy(mesh_final, &mask);

      /* add cloth rest shape key if needed */
      if (mask.vmask & CD_MASK_CLOTH_ORCO) {
        add_orco_mesh(ob, NULL, mesh_final, mesh_orco, CD_CLOTH_ORCO);
      }

      /* add an origspace layer if needed */
      if ((md_datamask->mask.lmask) & CD_MASK_ORIGSPACE_MLOOP) {
        if (!CustomData_has_layer(&mesh_final->ldata, CD_ORIGSPACE_MLOOP)) {
          CustomData_add_layer(
              &mesh_final->ldata, CD_ORIGSPACE_MLOOP, CD_CALLOC, NULL, mesh_final->totloop);
          mesh_init_origspace(mesh_final);
        }
      }

      Mesh *mesh_next = BKE_modifier_modify_mesh(md, &mectx, mesh_final);
      ASSERT_IS_VALID_MESH(mesh_next);

      if (mesh_next) {
        /* if the modifier returned a new mesh, release the old one */
        if (mesh_final != mesh_next) {
          BLI_assert(mesh_final != mesh_input);
          BKE_id_free(NULL, mesh_final);
        }
        mesh_final = mesh_next;

        if (deformed_verts) {
          MEM_freeN(deformed_verts);
          deformed_verts = NULL;
        }
      }

      /* create an orco mesh in parallel */
      if (nextmask.vmask & CD_MASK_ORCO) {
        if (!mesh_orco) {
          mesh_orco = create_orco_mesh(ob, mesh_input, NULL, CD_ORCO);
        }

        nextmask.vmask &= ~CD_MASK_ORCO;
        CustomData_MeshMasks temp_cddata_masks = {
            .vmask = CD_MASK_ORIGINDEX,
            .emask = CD_MASK_ORIGINDEX,
            .fmask = CD_MASK_ORIGINDEX,
            .pmask = CD_MASK_ORIGINDEX,
        };
        if (mti->requiredDataMask != NULL) {
          mti->requiredDataMask(ob, md, &temp_cddata_masks);
        }
        CustomData_MeshMasks_update(&temp_cddata_masks, &nextmask);
        mesh_set_only_copy(mesh_orco, &temp_cddata_masks);

        mesh_next = BKE_modifier_modify_mesh(md, &mectx_orco, mesh_orco);
        ASSERT_IS_VALID_MESH(mesh_next);

        if (mesh_next) {
          /* if the modifier returned a new mesh, release the old one */
          if (mesh_orco != mesh_next) {
            BLI_assert(mesh_orco != mesh_input);
            BKE_id_free(NULL, mesh_orco);
          }

          mesh_orco = mesh_next;
        }
      }

      /* create cloth orco mesh in parallel */
      if (nextmask.vmask & CD_MASK_CLOTH_ORCO) {
        if (!mesh_orco_cloth) {
          mesh_orco_cloth = create_orco_mesh(ob, mesh_input, NULL, CD_CLOTH_ORCO);
        }

        nextmask.vmask &= ~CD_MASK_CLOTH_ORCO;
        nextmask.vmask |= CD_MASK_ORIGINDEX;
        nextmask.emask |= CD_MASK_ORIGINDEX;
        nextmask.pmask |= CD_MASK_ORIGINDEX;
        mesh_set_only_copy(mesh_orco_cloth, &nextmask);

        mesh_next = BKE_modifier_modify_mesh(md, &mectx_orco, mesh_orco_cloth);
        ASSERT_IS_VALID_MESH(mesh_next);

        if (mesh_next) {
          /* if the modifier returned a new mesh, release the old one */
          if (mesh_orco_cloth != mesh_next) {
            BLI_assert(mesh_orco != mesh_input);
            BKE_id_free(NULL, mesh_orco_cloth);
          }

          mesh_orco_cloth = mesh_next;
        }
      }

      /* in case of dynamic paint, make sure preview mask remains for following modifiers */
      /* XXX Temp and hackish solution! */
      if (md->type == eModifierType_DynamicPaint) {
        append_mask.lmask |= CD_MASK_PREVIEW_MLOOPCOL;
      }

      mesh_final->runtime.deformed_only = false;
    }

    isPrevDeform = (mti->type == eModifierTypeType_OnlyDeform);

    /* grab modifiers until index i */
    if ((index != -1) && (BLI_findindex(&ob->modifiers, md) >= index)) {
      break;
    }

    if (sculpt_mode && md->type == eModifierType_Multires) {
      multires_applied = true;
    }
  }

  BLI_linklist_free((LinkNode *)datamasks, NULL);

  for (md = firstmd; md; md = md->next) {
    BKE_modifier_free_temporary_data(md);
  }

  /* Yay, we are done. If we have a Mesh and deformed vertices,
   * we need to apply these back onto the Mesh. If we have no
   * Mesh then we need to build one. */
  if (mesh_final == NULL) {
    /* Note: this check on cdmask is a bit dodgy, it handles the issue at stake here (see T68211),
     * but other cases might require similar handling?
     * Could be a good idea to define a proper CustomData_MeshMask for that then. */
    if (deformed_verts == NULL && allow_shared_mesh &&
        (final_datamask.lmask & CD_MASK_NORMAL) == 0 &&
        (final_datamask.pmask & CD_MASK_NORMAL) == 0) {
      mesh_final = mesh_input;
    }
    else {
      mesh_final = BKE_mesh_copy_for_eval(mesh_input, true);
    }
  }
  if (deformed_verts) {
    BKE_mesh_vert_coords_apply(mesh_final, deformed_verts);
    MEM_freeN(deformed_verts);
    deformed_verts = NULL;
  }

  /* Denotes whether the object which the modifier stack came from owns the mesh or whether the
   * mesh is shared across multiple objects since there are no effective modifiers. */
  const bool is_own_mesh = (mesh_final != mesh_input);

  /* Add orco coordinates to final and deformed mesh if requested. */
  if (final_datamask.vmask & CD_MASK_ORCO) {
    /* No need in ORCO layer if the mesh was not deformed or modified: undeformed mesh in this case
     * matches input mesh. */
    if (is_own_mesh) {
      add_orco_mesh(ob, NULL, mesh_final, mesh_orco, CD_ORCO);
    }

    if (mesh_deform) {
      add_orco_mesh(ob, NULL, mesh_deform, NULL, CD_ORCO);
    }
  }

  if (mesh_orco) {
    BKE_id_free(NULL, mesh_orco);
  }
  if (mesh_orco_cloth) {
    BKE_id_free(NULL, mesh_orco_cloth);
  }

  /* Compute normals. */
  if (is_own_mesh) {
    mesh_calc_modifier_final_normals(mesh_input, &final_datamask, sculpt_dyntopo, mesh_final);
  }
  else {
    Mesh_Runtime *runtime = &mesh_input->runtime;
    if (runtime->mesh_eval == NULL) {
      BLI_assert(runtime->eval_mutex != NULL);
      BLI_mutex_lock(runtime->eval_mutex);
      if (runtime->mesh_eval == NULL) {
        mesh_final = BKE_mesh_copy_for_eval(mesh_input, true);
        mesh_calc_modifier_final_normals(mesh_input, &final_datamask, sculpt_dyntopo, mesh_final);
        mesh_calc_finalize(mesh_input, mesh_final);
        runtime->mesh_eval = mesh_final;
      }
      BLI_mutex_unlock(runtime->eval_mutex);
    }
    mesh_final = runtime->mesh_eval;
  }

  if (is_own_mesh) {
    mesh_calc_finalize(mesh_input, mesh_final);
  }

  /* Return final mesh */
  *r_final = mesh_final;
  if (r_deform) {
    *r_deform = mesh_deform;
  }
}

float (*editbmesh_vert_coords_alloc(BMEditMesh *em, int *r_vert_len))[3]
{
  BMIter iter;
  BMVert *eve;
  float(*cos)[3];
  int i;

  *r_vert_len = em->bm->totvert;

  cos = MEM_malloc_arrayN(em->bm->totvert, 3 * sizeof(float), "vertexcos");

  BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
    copy_v3_v3(cos[i], eve->co);
  }

  return cos;
}

bool editbmesh_modifier_is_enabled(Scene *scene, ModifierData *md, bool has_prev_mesh)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);
  const int required_mode = eModifierMode_Realtime | eModifierMode_Editmode;

  if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
    return false;
  }

  if ((mti->flags & eModifierTypeFlag_RequiresOriginalData) && has_prev_mesh) {
    BKE_modifier_set_error(md, "Modifier requires original data, bad stack position");
    return false;
  }

  return true;
}

static void editbmesh_calc_modifier_final_normals(Mesh *mesh_final,
                                                  const CustomData_MeshMasks *final_datamask)
{
  if (mesh_final->runtime.wrapper_type != ME_WRAPPER_TYPE_MDATA) {
    /* Generated at draw time. */
    mesh_final->runtime.wrapper_type_finalize = (1 << mesh_final->runtime.wrapper_type);
    return;
  }

  const bool do_loop_normals = ((mesh_final->flag & ME_AUTOSMOOTH) != 0 ||
                                (final_datamask->lmask & CD_MASK_NORMAL) != 0);
  /* Some modifiers may need this info from their target (other) object,
   * simpler to generate it here as well. */
  const bool do_poly_normals = ((final_datamask->pmask & CD_MASK_NORMAL) != 0);

  /* In case we also need poly normals, add the layer and compute them here
   * (BKE_mesh_calc_normals_split() assumes that if that data exists, it is always valid). */
  if (do_poly_normals) {
    if (!CustomData_has_layer(&mesh_final->pdata, CD_NORMAL)) {
      float(*polynors)[3] = CustomData_add_layer(
          &mesh_final->pdata, CD_NORMAL, CD_CALLOC, NULL, mesh_final->totpoly);
      BKE_mesh_calc_normals_poly(mesh_final->mvert,
                                 NULL,
                                 mesh_final->totvert,
                                 mesh_final->mloop,
                                 mesh_final->mpoly,
                                 mesh_final->totloop,
                                 mesh_final->totpoly,
                                 polynors,
                                 false);
    }
  }

  if (do_loop_normals) {
    /* Compute loop normals */
    BKE_mesh_calc_normals_split(mesh_final);
    BKE_mesh_tessface_clear(mesh_final);
  }

  /* BMESH_ONLY, ensure tessface's used for drawing,
   * but don't recalculate if the last modifier in the stack gives us tessfaces
   * check if the derived meshes are DM_TYPE_EDITBMESH before calling, this isn't essential
   * but quiets annoying error messages since tessfaces wont be created. */
  if (final_datamask->fmask & CD_MASK_MFACE) {
    if (mesh_final->edit_mesh == NULL) {
      BKE_mesh_tessface_ensure(mesh_final);
    }
  }

  /* same as mesh_calc_modifiers (if using loop normals, poly nors have already been computed). */
  if (!do_loop_normals) {
    BKE_mesh_ensure_normals_for_display(mesh_final);

    /* Some modifiers, like data-transfer, may generate those data, we do not want to keep them,
     * as they are used by display code when available (i.e. even if autosmooth is disabled). */
    if (CustomData_has_layer(&mesh_final->ldata, CD_NORMAL)) {
      CustomData_free_layers(&mesh_final->ldata, CD_NORMAL, mesh_final->totloop);
    }
  }
}

static void editbmesh_calc_modifiers(struct Depsgraph *depsgraph,
                                     Scene *scene,
                                     Object *ob,
                                     BMEditMesh *em_input,
                                     const CustomData_MeshMasks *dataMask,
                                     /* return args */
                                     Mesh **r_cage,
                                     Mesh **r_final)
{
  /* Input and final mesh. Final mesh is only created the moment the first
   * constructive modifier is executed, or a deform modifier needs normals
   * or certain data layers. */
  Mesh *mesh_input = ob->data;
  Mesh *mesh_final = NULL;
  Mesh *mesh_cage = NULL;

  /* Deformed vertex locations array. Deform only modifier need this type of
   * float array rather than MVert*. Tracked along with mesh_final as an
   * optimization to avoid copying coordinates back and forth if there are
   * multiple sequential deform only modifiers. */
  float(*deformed_verts)[3] = NULL;
  int num_deformed_verts = 0;
  bool isPrevDeform = false;

  /* Mesh with constructive modifiers but no deformation applied. Tracked
   * along with final mesh if undeformed / orco coordinates are requested
   * for texturing. */
  Mesh *mesh_orco = NULL;

  /* Modifier evaluation modes. */
  const int required_mode = eModifierMode_Realtime | eModifierMode_Editmode;

  /* Modifier evaluation contexts for different types of modifiers. */
  const ModifierEvalContext mectx = {depsgraph, ob, MOD_APPLY_USECACHE};
  const ModifierEvalContext mectx_orco = {depsgraph, ob, MOD_APPLY_ORCO};

  /* Get effective list of modifiers to execute. Some effects like shape keys
   * are added as virtual modifiers before the user created modifiers. */
  VirtualModifierData virtualModifierData;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);

  /* Compute accumulated datamasks needed by each modifier. It helps to do
   * this fine grained so that for example vertex groups are preserved up to
   * an armature modifier, but not through a following subsurf modifier where
   * subdividing them is expensive. */
  CustomData_MeshMasks final_datamask = *dataMask;
  CDMaskLink *datamasks = BKE_modifier_calc_data_masks(
      scene, ob, md, &final_datamask, required_mode, NULL, NULL);
  CDMaskLink *md_datamask = datamasks;
  CustomData_MeshMasks append_mask = CD_MASK_BAREMESH;

  /* Evaluate modifiers up to certain index to get the mesh cage. */
  int cageIndex = BKE_modifiers_get_cage_index(scene, ob, NULL, 1);
  if (r_cage && cageIndex == -1) {
    mesh_cage = BKE_mesh_wrapper_from_editmesh_with_coords(
        em_input, &final_datamask, NULL, mesh_input);
  }

  /* Clear errors before evaluation. */
  BKE_modifiers_clear_errors(ob);

  for (int i = 0; md; i++, md = md->next, md_datamask = md_datamask->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);

    if (!editbmesh_modifier_is_enabled(scene, md, mesh_final != NULL)) {
      continue;
    }

    /* Add an orco mesh as layer if needed by this modifier. */
    if (mesh_final && mesh_orco && mti->requiredDataMask) {
      CustomData_MeshMasks mask = {0};
      mti->requiredDataMask(ob, md, &mask);
      if (mask.vmask & CD_MASK_ORCO) {
        add_orco_mesh(ob, em_input, mesh_final, mesh_orco, CD_ORCO);
      }
    }

    /* How to apply modifier depends on (a) what we already have as
     * a result of previous modifiers (could be a mesh or just
     * deformed vertices) and (b) what type the modifier is. */
    if (mti->type == eModifierTypeType_OnlyDeform) {
      /* No existing verts to deform, need to build them. */
      if (!deformed_verts) {
        if (mesh_final) {
          /* Deforming a derived mesh, read the vertex locations
           * out of the mesh and deform them. Once done with this
           * run of deformers verts will be written back. */
          deformed_verts = BKE_mesh_vert_coords_alloc(mesh_final, &num_deformed_verts);
        }
        else {
          deformed_verts = editbmesh_vert_coords_alloc(em_input, &num_deformed_verts);
        }
      }
      else if (isPrevDeform && mti->dependsOnNormals && mti->dependsOnNormals(md)) {
        if (mesh_final == NULL) {
          mesh_final = BKE_mesh_from_bmesh_for_eval_nomain(em_input->bm, NULL, mesh_input);
          ASSERT_IS_VALID_MESH(mesh_final);
        }
        BLI_assert(deformed_verts != NULL);
        BKE_mesh_vert_coords_apply(mesh_final, deformed_verts);
      }

      if (mti->deformVertsEM) {
        BKE_modifier_deform_vertsEM(
            md, &mectx, em_input, mesh_final, deformed_verts, num_deformed_verts);
      }
      else {
        BKE_modifier_deform_verts(md, &mectx, mesh_final, deformed_verts, num_deformed_verts);
      }
    }
    else {
      /* apply vertex coordinates or build a DerivedMesh as necessary */
      if (mesh_final) {
        if (deformed_verts) {
          Mesh *mesh_tmp = BKE_mesh_copy_for_eval(mesh_final, false);
          if (mesh_final != mesh_cage) {
            BKE_id_free(NULL, mesh_final);
          }
          mesh_final = mesh_tmp;
          BKE_mesh_vert_coords_apply(mesh_final, deformed_verts);
        }
        else if (mesh_final == mesh_cage) {
          /* 'me' may be changed by this modifier, so we need to copy it. */
          mesh_final = BKE_mesh_copy_for_eval(mesh_final, false);
        }
      }
      else {
        mesh_final = BKE_mesh_wrapper_from_editmesh_with_coords(
            em_input, NULL, deformed_verts, mesh_input);
        deformed_verts = NULL;
      }

      /* create an orco derivedmesh in parallel */
      CustomData_MeshMasks mask = md_datamask->mask;
      if (mask.vmask & CD_MASK_ORCO) {
        if (!mesh_orco) {
          mesh_orco = create_orco_mesh(ob, mesh_input, em_input, CD_ORCO);
        }

        mask.vmask &= ~CD_MASK_ORCO;
        mask.vmask |= CD_MASK_ORIGINDEX;
        mask.emask |= CD_MASK_ORIGINDEX;
        mask.pmask |= CD_MASK_ORIGINDEX;
        mesh_set_only_copy(mesh_orco, &mask);

        Mesh *mesh_next = BKE_modifier_modify_mesh(md, &mectx_orco, mesh_orco);
        ASSERT_IS_VALID_MESH(mesh_next);

        if (mesh_next) {
          /* if the modifier returned a new dm, release the old one */
          if (mesh_orco && mesh_orco != mesh_next) {
            BKE_id_free(NULL, mesh_orco);
          }
          mesh_orco = mesh_next;
        }
      }

      /* set the DerivedMesh to only copy needed data */
      CustomData_MeshMasks_update(&mask, &append_mask);
      /* XXX WHAT? ovewrites mask ??? */
      /* CD_MASK_ORCO may have been cleared above */
      mask = md_datamask->mask;
      mask.vmask |= CD_MASK_ORIGINDEX;
      mask.emask |= CD_MASK_ORIGINDEX;
      mask.pmask |= CD_MASK_ORIGINDEX;

      mesh_set_only_copy(mesh_final, &mask);

      if (mask.lmask & CD_MASK_ORIGSPACE_MLOOP) {
        if (!CustomData_has_layer(&mesh_final->ldata, CD_ORIGSPACE_MLOOP)) {
          CustomData_add_layer(
              &mesh_final->ldata, CD_ORIGSPACE_MLOOP, CD_CALLOC, NULL, mesh_final->totloop);
          mesh_init_origspace(mesh_final);
        }
      }

      Mesh *mesh_next = BKE_modifier_modify_mesh(md, &mectx, mesh_final);
      ASSERT_IS_VALID_MESH(mesh_next);

      if (mesh_next) {
        if (mesh_final && mesh_final != mesh_next) {
          BKE_id_free(NULL, mesh_final);
        }
        mesh_final = mesh_next;

        if (deformed_verts) {
          MEM_freeN(deformed_verts);
          deformed_verts = NULL;
        }
      }
      mesh_final->runtime.deformed_only = false;
    }

    if (r_cage && i == cageIndex) {
      if (mesh_final && deformed_verts) {
        mesh_cage = BKE_mesh_copy_for_eval(mesh_final, false);
        BKE_mesh_vert_coords_apply(mesh_cage, deformed_verts);
      }
      else if (mesh_final) {
        mesh_cage = mesh_final;
      }
      else {
        Mesh *me_orig = mesh_input;
        if (me_orig->id.tag & LIB_TAG_COPIED_ON_WRITE) {
          if (!BKE_mesh_runtime_ensure_edit_data(me_orig)) {
            BKE_mesh_runtime_reset_edit_data(me_orig);
          }
          me_orig->runtime.edit_data->vertexCos = MEM_dupallocN(deformed_verts);
        }
        mesh_cage = BKE_mesh_wrapper_from_editmesh_with_coords(
            em_input,
            &final_datamask,
            deformed_verts ? MEM_dupallocN(deformed_verts) : NULL,
            mesh_input);
      }
    }

    isPrevDeform = (mti->type == eModifierTypeType_OnlyDeform);
  }

  BLI_linklist_free((LinkNode *)datamasks, NULL);

  /* Yay, we are done. If we have a DerivedMesh and deformed vertices need
   * to apply these back onto the DerivedMesh. If we have no DerivedMesh
   * then we need to build one. */
  if (mesh_final) {
    if (deformed_verts) {
      Mesh *mesh_tmp = BKE_mesh_copy_for_eval(mesh_final, false);
      if (mesh_final != mesh_cage) {
        BKE_id_free(NULL, mesh_final);
      }
      mesh_final = mesh_tmp;
      BKE_mesh_vert_coords_apply(mesh_final, deformed_verts);
    }
  }
  else if (!deformed_verts && mesh_cage) {
    /* cage should already have up to date normals */
    mesh_final = mesh_cage;
  }
  else {
    /* this is just a copy of the editmesh, no need to calc normals */
    mesh_final = BKE_mesh_wrapper_from_editmesh_with_coords(
        em_input, &final_datamask, deformed_verts, mesh_input);
    deformed_verts = NULL;
  }

  if (deformed_verts) {
    MEM_freeN(deformed_verts);
  }

  /* Add orco coordinates to final and deformed mesh if requested. */
  if (final_datamask.vmask & CD_MASK_ORCO) {
    /* FIXME(Campbell): avoid the need to convert to mesh data just to add an orco layer. */
    BKE_mesh_wrapper_ensure_mdata(mesh_final);

    add_orco_mesh(ob, em_input, mesh_final, mesh_orco, CD_ORCO);
  }

  if (mesh_orco) {
    BKE_id_free(NULL, mesh_orco);
  }

  /* Ensure normals calculation below is correct. */
  BLI_assert((mesh_input->flag & ME_AUTOSMOOTH) == (mesh_final->flag & ME_AUTOSMOOTH));
  BLI_assert(mesh_input->smoothresh == mesh_final->smoothresh);
  BLI_assert(mesh_input->smoothresh == mesh_cage->smoothresh);

  /* Compute normals. */
  editbmesh_calc_modifier_final_normals(mesh_final, &final_datamask);
  if (mesh_cage && (mesh_cage != mesh_final)) {
    editbmesh_calc_modifier_final_normals(mesh_cage, &final_datamask);
  }

  /* Return final mesh. */
  *r_final = mesh_final;
  if (r_cage) {
    *r_cage = mesh_cage;
  }
}

static void mesh_build_extra_data(struct Depsgraph *depsgraph, Object *ob, Mesh *mesh_eval)
{
  uint32_t eval_flags = DEG_get_eval_flags_for_id(depsgraph, &ob->id);

  if (eval_flags & DAG_EVAL_NEED_SHRINKWRAP_BOUNDARY) {
    BKE_shrinkwrap_compute_boundary_data(mesh_eval);
  }
}

static void mesh_runtime_check_normals_valid(const Mesh *mesh)
{
  UNUSED_VARS_NDEBUG(mesh);
  BLI_assert(!(mesh->runtime.cd_dirty_vert & CD_MASK_NORMAL));
  BLI_assert(!(mesh->runtime.cd_dirty_loop & CD_MASK_NORMAL));
  BLI_assert(!(mesh->runtime.cd_dirty_poly & CD_MASK_NORMAL));
}

static void mesh_build_data(struct Depsgraph *depsgraph,
                            Scene *scene,
                            Object *ob,
                            const CustomData_MeshMasks *dataMask,
                            const bool need_mapping)
{
  BLI_assert(ob->type == OB_MESH);

  /* Evaluated meshes aren't supposed to be created on original instances. If you do,
   * they aren't cleaned up properly on mode switch, causing crashes, e.g T58150. */
  BLI_assert(ob->id.tag & LIB_TAG_COPIED_ON_WRITE);

  BKE_object_free_derived_caches(ob);
  if (DEG_is_active(depsgraph)) {
    BKE_sculpt_update_object_before_eval(ob);
  }

#if 0 /* XXX This is already taken care of in mesh_calc_modifiers()... */
  if (need_mapping) {
    /* Also add the flag so that it is recorded in lastDataMask. */
    dataMask->vmask |= CD_MASK_ORIGINDEX;
    dataMask->emask |= CD_MASK_ORIGINDEX;
    dataMask->pmask |= CD_MASK_ORIGINDEX;
  }
#endif

  Mesh *mesh_eval = NULL, *mesh_deform_eval = NULL;
  mesh_calc_modifiers(depsgraph,
                      scene,
                      ob,
                      1,
                      need_mapping,
                      dataMask,
                      -1,
                      true,
                      true,
                      &mesh_deform_eval,
                      &mesh_eval);

  /* The modifier stack evaluation is storing result in mesh->runtime.mesh_eval, but this result
   * is not guaranteed to be owned by object.
   *
   * Check ownership now, since later on we can not go to a mesh owned by someone else via
   * object's runtime: this could cause access freed data on depsgraph destruction (mesh who owns
   * the final result might be freed prior to object). */
  Mesh *mesh = ob->data;
  const bool is_mesh_eval_owned = (mesh_eval != mesh->runtime.mesh_eval);
  BKE_object_eval_assign_data(ob, &mesh_eval->id, is_mesh_eval_owned);

  ob->runtime.mesh_deform_eval = mesh_deform_eval;
  ob->runtime.last_data_mask = *dataMask;
  ob->runtime.last_need_mapping = need_mapping;

  BKE_object_boundbox_calc_from_mesh(ob, mesh_eval);

  if ((ob->mode & OB_MODE_ALL_SCULPT) && ob->sculpt) {
    if (DEG_is_active(depsgraph)) {
      BKE_sculpt_update_object_after_eval(depsgraph, ob);
    }
  }

  mesh_runtime_check_normals_valid(mesh_eval);
  mesh_build_extra_data(depsgraph, ob, mesh_eval);
}

static void editbmesh_build_data(struct Depsgraph *depsgraph,
                                 Scene *scene,
                                 Object *obedit,
                                 BMEditMesh *em,
                                 CustomData_MeshMasks *dataMask)
{
  BLI_assert(obedit->id.tag & LIB_TAG_COPIED_ON_WRITE);

  BKE_object_free_derived_caches(obedit);
  if (DEG_is_active(depsgraph)) {
    BKE_sculpt_update_object_before_eval(obedit);
  }

  BKE_editmesh_free_derivedmesh(em);

  Mesh *me_cage;
  Mesh *me_final;

  editbmesh_calc_modifiers(depsgraph, scene, obedit, em, dataMask, &me_cage, &me_final);

  em->mesh_eval_final = me_final;
  em->mesh_eval_cage = me_cage;

  BKE_object_boundbox_calc_from_mesh(obedit, em->mesh_eval_final);

  em->lastDataMask = *dataMask;

  mesh_runtime_check_normals_valid(em->mesh_eval_final);
}

static void object_get_datamask(const Depsgraph *depsgraph,
                                Object *ob,
                                CustomData_MeshMasks *r_mask,
                                bool *r_need_mapping)
{
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);

  DEG_get_customdata_mask_for_object(depsgraph, ob, r_mask);

  if (r_need_mapping) {
    *r_need_mapping = false;
  }

  /* Must never access original objects when dependency graph is not active: it might be already
   * freed. */
  if (!DEG_is_active(depsgraph)) {
    return;
  }

  Object *actob = view_layer->basact ? DEG_get_original_object(view_layer->basact->object) : NULL;
  if (DEG_get_original_object(ob) == actob) {
    bool editing = BKE_paint_select_face_test(actob);

    /* weight paint and face select need original indices because of selection buffer drawing */
    if (r_need_mapping) {
      *r_need_mapping = (editing || (ob->mode & (OB_MODE_WEIGHT_PAINT | OB_MODE_VERTEX_PAINT)));
    }

    /* check if we need tfaces & mcols due to face select or texture paint */
    if ((ob->mode & OB_MODE_TEXTURE_PAINT) || editing) {
      r_mask->lmask |= CD_MASK_MLOOPUV | CD_MASK_MLOOPCOL;
      r_mask->fmask |= CD_MASK_MTFACE;
    }

    /* check if we need mcols due to vertex paint or weightpaint */
    if (ob->mode & OB_MODE_VERTEX_PAINT) {
      r_mask->lmask |= CD_MASK_MLOOPCOL;
    }

    if (ob->mode & OB_MODE_WEIGHT_PAINT) {
      r_mask->vmask |= CD_MASK_MDEFORMVERT;
    }

    if (ob->mode & OB_MODE_EDIT) {
      r_mask->vmask |= CD_MASK_MVERT_SKIN;
    }
  }
}

void makeDerivedMesh(struct Depsgraph *depsgraph,
                     Scene *scene,
                     Object *ob,
                     BMEditMesh *em,
                     const CustomData_MeshMasks *dataMask)
{
  bool need_mapping;
  CustomData_MeshMasks cddata_masks = *dataMask;
  object_get_datamask(depsgraph, ob, &cddata_masks, &need_mapping);

  if (em) {
    editbmesh_build_data(depsgraph, scene, ob, em, &cddata_masks);
  }
  else {
    mesh_build_data(depsgraph, scene, ob, &cddata_masks, need_mapping);
  }
}

/***/

Mesh *mesh_get_eval_final(struct Depsgraph *depsgraph,
                          Scene *scene,
                          Object *ob,
                          const CustomData_MeshMasks *dataMask)
{
  /* This function isn't thread-safe and can't be used during evaluation. */
  BLI_assert(DEG_is_evaluating(depsgraph) == false);

  /* Evaluated meshes aren't supposed to be created on original instances. If you do,
   * they aren't cleaned up properly on mode switch, causing crashes, e.g T58150. */
  BLI_assert(ob->id.tag & LIB_TAG_COPIED_ON_WRITE);

  /* if there's no evaluated mesh or the last data mask used doesn't include
   * the data we need, rebuild the derived mesh
   */
  bool need_mapping;
  CustomData_MeshMasks cddata_masks = *dataMask;
  object_get_datamask(depsgraph, ob, &cddata_masks, &need_mapping);

  Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if ((mesh_eval == NULL) ||
      !CustomData_MeshMasks_are_matching(&(ob->runtime.last_data_mask), &cddata_masks) ||
      (need_mapping && !ob->runtime.last_need_mapping)) {
    CustomData_MeshMasks_update(&cddata_masks, &ob->runtime.last_data_mask);
    mesh_build_data(
        depsgraph, scene, ob, &cddata_masks, need_mapping || ob->runtime.last_need_mapping);
    mesh_eval = BKE_object_get_evaluated_mesh(ob);
  }

  if (mesh_eval != NULL) {
    BLI_assert(!(mesh_eval->runtime.cd_dirty_vert & CD_MASK_NORMAL));
  }
  return mesh_eval;
}

Mesh *mesh_get_eval_deform(struct Depsgraph *depsgraph,
                           Scene *scene,
                           Object *ob,
                           const CustomData_MeshMasks *dataMask)
{
  /* This function isn't thread-safe and can't be used during evaluation. */
  BLI_assert(DEG_is_evaluating(depsgraph) == false);

  /* Evaluated meshes aren't supposed to be created on original instances. If you do,
   * they aren't cleaned up properly on mode switch, causing crashes, e.g T58150. */
  BLI_assert(ob->id.tag & LIB_TAG_COPIED_ON_WRITE);

  /* if there's no derived mesh or the last data mask used doesn't include
   * the data we need, rebuild the derived mesh
   */
  bool need_mapping;

  CustomData_MeshMasks cddata_masks = *dataMask;
  object_get_datamask(depsgraph, ob, &cddata_masks, &need_mapping);

  if (!ob->runtime.mesh_deform_eval ||
      !CustomData_MeshMasks_are_matching(&(ob->runtime.last_data_mask), &cddata_masks) ||
      (need_mapping && !ob->runtime.last_need_mapping)) {
    CustomData_MeshMasks_update(&cddata_masks, &ob->runtime.last_data_mask);
    mesh_build_data(
        depsgraph, scene, ob, &cddata_masks, need_mapping || ob->runtime.last_need_mapping);
  }

  return ob->runtime.mesh_deform_eval;
}

Mesh *mesh_create_eval_final_render(Depsgraph *depsgraph,
                                    Scene *scene,
                                    Object *ob,
                                    const CustomData_MeshMasks *dataMask)
{
  Mesh *final;

  mesh_calc_modifiers(depsgraph, scene, ob, 1, false, dataMask, -1, false, false, NULL, &final);

  return final;
}

Mesh *mesh_create_eval_final_index_render(Depsgraph *depsgraph,
                                          Scene *scene,
                                          Object *ob,
                                          const CustomData_MeshMasks *dataMask,
                                          int index)
{
  Mesh *final;

  mesh_calc_modifiers(depsgraph, scene, ob, 1, false, dataMask, index, false, false, NULL, &final);

  return final;
}

Mesh *mesh_create_eval_final_view(Depsgraph *depsgraph,
                                  Scene *scene,
                                  Object *ob,
                                  const CustomData_MeshMasks *dataMask)
{
  Mesh *final;

  /* XXX hack
   * psys modifier updates particle state when called during dupli-list generation,
   * which can lead to wrong transforms. This disables particle system modifier execution.
   */
  ob->transflag |= OB_NO_PSYS_UPDATE;

  mesh_calc_modifiers(depsgraph, scene, ob, 1, false, dataMask, -1, false, false, NULL, &final);

  ob->transflag &= ~OB_NO_PSYS_UPDATE;

  return final;
}

Mesh *mesh_create_eval_no_deform(Depsgraph *depsgraph,
                                 Scene *scene,
                                 Object *ob,
                                 const CustomData_MeshMasks *dataMask)
{
  Mesh *final;

  mesh_calc_modifiers(depsgraph, scene, ob, 0, false, dataMask, -1, false, false, NULL, &final);

  return final;
}

Mesh *mesh_create_eval_no_deform_render(Depsgraph *depsgraph,
                                        Scene *scene,
                                        Object *ob,
                                        const CustomData_MeshMasks *dataMask)
{
  Mesh *final;

  mesh_calc_modifiers(depsgraph, scene, ob, 0, false, dataMask, -1, false, false, NULL, &final);

  return final;
}

/***/

Mesh *editbmesh_get_eval_cage_and_final(Depsgraph *depsgraph,
                                        Scene *scene,
                                        Object *obedit,
                                        BMEditMesh *em,
                                        const CustomData_MeshMasks *dataMask,
                                        /* return args */
                                        Mesh **r_final)
{
  CustomData_MeshMasks cddata_masks = *dataMask;

  /* if there's no derived mesh or the last data mask used doesn't include
   * the data we need, rebuild the derived mesh
   */
  object_get_datamask(depsgraph, obedit, &cddata_masks, NULL);

  if (!em->mesh_eval_cage ||
      !CustomData_MeshMasks_are_matching(&(em->lastDataMask), &cddata_masks)) {
    editbmesh_build_data(depsgraph, scene, obedit, em, &cddata_masks);
  }

  *r_final = em->mesh_eval_final;
  if (em->mesh_eval_final) {
    BLI_assert(!(em->mesh_eval_final->runtime.cd_dirty_vert & DM_DIRTY_NORMALS));
  }
  return em->mesh_eval_cage;
}

Mesh *editbmesh_get_eval_cage(struct Depsgraph *depsgraph,
                              Scene *scene,
                              Object *obedit,
                              BMEditMesh *em,
                              const CustomData_MeshMasks *dataMask)
{
  CustomData_MeshMasks cddata_masks = *dataMask;

  /* if there's no derived mesh or the last data mask used doesn't include
   * the data we need, rebuild the derived mesh
   */
  object_get_datamask(depsgraph, obedit, &cddata_masks, NULL);

  if (!em->mesh_eval_cage ||
      !CustomData_MeshMasks_are_matching(&(em->lastDataMask), &cddata_masks)) {
    editbmesh_build_data(depsgraph, scene, obedit, em, &cddata_masks);
  }

  return em->mesh_eval_cage;
}

Mesh *editbmesh_get_eval_cage_from_orig(struct Depsgraph *depsgraph,
                                        Scene *scene,
                                        Object *obedit,
                                        const CustomData_MeshMasks *dataMask)
{
  BLI_assert((obedit->id.tag & LIB_TAG_COPIED_ON_WRITE) == 0);
  Scene *scene_eval = (Scene *)DEG_get_evaluated_id(depsgraph, &scene->id);
  Object *obedit_eval = (Object *)DEG_get_evaluated_id(depsgraph, &obedit->id);
  BMEditMesh *em_eval = BKE_editmesh_from_object(obedit_eval);
  return editbmesh_get_eval_cage(depsgraph, scene_eval, obedit_eval, em_eval, dataMask);
}

/***/

/* same as above but for vert coords */
typedef struct {
  float (*vertexcos)[3];
  BLI_bitmap *vertex_visit;
} MappedUserData;

static void make_vertexcos__mapFunc(void *userData,
                                    int index,
                                    const float co[3],
                                    const float UNUSED(no_f[3]),
                                    const short UNUSED(no_s[3]))
{
  MappedUserData *mappedData = (MappedUserData *)userData;

  if (BLI_BITMAP_TEST(mappedData->vertex_visit, index) == 0) {
    /* we need coord from prototype vertex, not from copies,
     * assume they stored in the beginning of vertex array stored in DM
     * (mirror modifier for eg does this) */
    copy_v3_v3(mappedData->vertexcos[index], co);
    BLI_BITMAP_ENABLE(mappedData->vertex_visit, index);
  }
}

void mesh_get_mapped_verts_coords(Mesh *me_eval, float (*r_cos)[3], const int totcos)
{
  if (me_eval->runtime.deformed_only == false) {
    MappedUserData userData;
    memset(r_cos, 0, sizeof(*r_cos) * totcos);
    userData.vertexcos = r_cos;
    userData.vertex_visit = BLI_BITMAP_NEW(totcos, "vertexcos flags");
    BKE_mesh_foreach_mapped_vert(me_eval, make_vertexcos__mapFunc, &userData, MESH_FOREACH_NOP);
    MEM_freeN(userData.vertex_visit);
  }
  else {
    MVert *mv = me_eval->mvert;
    for (int i = 0; i < totcos; i++, mv++) {
      copy_v3_v3(r_cos[i], mv->co);
    }
  }
}

void DM_calc_loop_tangents(DerivedMesh *dm,
                           bool calc_active_tangent,
                           const char (*tangent_names)[MAX_NAME],
                           int tangent_names_len)
{
  BKE_mesh_calc_loop_tangent_ex(dm->getVertArray(dm),
                                dm->getPolyArray(dm),
                                dm->getNumPolys(dm),
                                dm->getLoopArray(dm),
                                dm->getLoopTriArray(dm),
                                dm->getNumLoopTri(dm),
                                &dm->loopData,
                                calc_active_tangent,
                                tangent_names,
                                tangent_names_len,
                                CustomData_get_layer(&dm->polyData, CD_NORMAL),
                                dm->getLoopDataArray(dm, CD_NORMAL),
                                dm->getVertDataArray(dm, CD_ORCO), /* may be NULL */
                                /* result */
                                &dm->loopData,
                                dm->getNumLoops(dm),
                                &dm->tangent_mask);
}

static void mesh_init_origspace(Mesh *mesh)
{
  const float default_osf[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

  OrigSpaceLoop *lof_array = CustomData_get_layer(&mesh->ldata, CD_ORIGSPACE_MLOOP);
  const int numpoly = mesh->totpoly;
  // const int numloop = mesh->totloop;
  MVert *mv = mesh->mvert;
  MLoop *ml = mesh->mloop;
  MPoly *mp = mesh->mpoly;
  int i, j, k;

  float(*vcos_2d)[2] = NULL;
  BLI_array_staticdeclare(vcos_2d, 64);

  for (i = 0; i < numpoly; i++, mp++) {
    OrigSpaceLoop *lof = lof_array + mp->loopstart;

    if (mp->totloop == 3 || mp->totloop == 4) {
      for (j = 0; j < mp->totloop; j++, lof++) {
        copy_v2_v2(lof->uv, default_osf[j]);
      }
    }
    else {
      MLoop *l = &ml[mp->loopstart];
      float p_nor[3], co[3];
      float mat[3][3];

      float min[2] = {FLT_MAX, FLT_MAX}, max[2] = {-FLT_MAX, -FLT_MAX};
      float translate[2], scale[2];

      BKE_mesh_calc_poly_normal(mp, l, mv, p_nor);
      axis_dominant_v3_to_m3(mat, p_nor);

      BLI_array_clear(vcos_2d);
      BLI_array_reserve(vcos_2d, mp->totloop);
      for (j = 0; j < mp->totloop; j++, l++) {
        mul_v3_m3v3(co, mat, mv[l->v].co);
        copy_v2_v2(vcos_2d[j], co);

        for (k = 0; k < 2; k++) {
          if (co[k] > max[k]) {
            max[k] = co[k];
          }
          else if (co[k] < min[k]) {
            min[k] = co[k];
          }
        }
      }

      /* Brings min to (0, 0). */
      negate_v2_v2(translate, min);

      /* Scale will bring max to (1, 1). */
      sub_v2_v2v2(scale, max, min);
      if (scale[0] == 0.0f) {
        scale[0] = 1e-9f;
      }
      if (scale[1] == 0.0f) {
        scale[1] = 1e-9f;
      }
      invert_v2(scale);

      /* Finally, transform all vcos_2d into ((0, 0), (1, 1))
       * square and assign them as origspace. */
      for (j = 0; j < mp->totloop; j++, lof++) {
        add_v2_v2v2(lof->uv, vcos_2d[j], translate);
        mul_v2_v2(lof->uv, scale);
      }
    }
  }

  BKE_mesh_tessface_clear(mesh);
  BLI_array_free(vcos_2d);
}

/* derivedmesh info printing function,
 * to help track down differences DM output */

#ifndef NDEBUG
#  include "BLI_dynstr.h"

static void dm_debug_info_layers(DynStr *dynstr,
                                 DerivedMesh *dm,
                                 CustomData *cd,
                                 void *(*getElemDataArray)(DerivedMesh *, int))
{
  int type;

  for (type = 0; type < CD_NUMTYPES; type++) {
    if (CustomData_has_layer(cd, type)) {
      /* note: doesn't account for multiple layers */
      const char *name = CustomData_layertype_name(type);
      const int size = CustomData_sizeof(type);
      const void *pt = getElemDataArray(dm, type);
      const int pt_size = pt ? (int)(MEM_allocN_len(pt) / size) : 0;
      const char *structname;
      int structnum;
      CustomData_file_write_info(type, &structname, &structnum);
      BLI_dynstr_appendf(
          dynstr,
          "        dict(name='%s', struct='%s', type=%d, ptr='%p', elem=%d, length=%d),\n",
          name,
          structname,
          type,
          (const void *)pt,
          size,
          pt_size);
    }
  }
}

char *DM_debug_info(DerivedMesh *dm)
{
  DynStr *dynstr = BLI_dynstr_new();
  char *ret;
  const char *tstr;

  BLI_dynstr_append(dynstr, "{\n");
  BLI_dynstr_appendf(dynstr, "    'ptr': '%p',\n", (void *)dm);
  switch (dm->type) {
    case DM_TYPE_CDDM:
      tstr = "DM_TYPE_CDDM";
      break;
    case DM_TYPE_CCGDM:
      tstr = "DM_TYPE_CCGDM";
      break;
    default:
      tstr = "UNKNOWN";
      break;
  }
  BLI_dynstr_appendf(dynstr, "    'type': '%s',\n", tstr);
  BLI_dynstr_appendf(dynstr, "    'numVertData': %d,\n", dm->numVertData);
  BLI_dynstr_appendf(dynstr, "    'numEdgeData': %d,\n", dm->numEdgeData);
  BLI_dynstr_appendf(dynstr, "    'numTessFaceData': %d,\n", dm->numTessFaceData);
  BLI_dynstr_appendf(dynstr, "    'numPolyData': %d,\n", dm->numPolyData);
  BLI_dynstr_appendf(dynstr, "    'deformedOnly': %d,\n", dm->deformedOnly);

  BLI_dynstr_append(dynstr, "    'vertexLayers': (\n");
  dm_debug_info_layers(dynstr, dm, &dm->vertData, dm->getVertDataArray);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'edgeLayers': (\n");
  dm_debug_info_layers(dynstr, dm, &dm->edgeData, dm->getEdgeDataArray);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'loopLayers': (\n");
  dm_debug_info_layers(dynstr, dm, &dm->loopData, dm->getLoopDataArray);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'polyLayers': (\n");
  dm_debug_info_layers(dynstr, dm, &dm->polyData, dm->getPolyDataArray);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'tessFaceLayers': (\n");
  dm_debug_info_layers(dynstr, dm, &dm->faceData, dm->getTessFaceDataArray);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "}\n");

  ret = BLI_dynstr_get_cstring(dynstr);
  BLI_dynstr_free(dynstr);
  return ret;
}

void DM_debug_print(DerivedMesh *dm)
{
  char *str = DM_debug_info(dm);
  puts(str);
  fflush(stdout);
  MEM_freeN(str);
}

void DM_debug_print_cdlayers(CustomData *data)
{
  int i;
  const CustomDataLayer *layer;

  printf("{\n");

  for (i = 0, layer = data->layers; i < data->totlayer; i++, layer++) {

    const char *name = CustomData_layertype_name(layer->type);
    const int size = CustomData_sizeof(layer->type);
    const char *structname;
    int structnum;
    CustomData_file_write_info(layer->type, &structname, &structnum);
    printf("        dict(name='%s', struct='%s', type=%d, ptr='%p', elem=%d, length=%d),\n",
           name,
           structname,
           layer->type,
           (const void *)layer->data,
           size,
           (int)(MEM_allocN_len(layer->data) / size));
  }

  printf("}\n");
}

bool DM_is_valid(DerivedMesh *dm)
{
  const bool do_verbose = true;
  const bool do_fixes = false;

  bool is_valid = true;
  bool changed = true;

  is_valid &= BKE_mesh_validate_all_customdata(
      dm->getVertDataLayout(dm),
      dm->getNumVerts(dm),
      dm->getEdgeDataLayout(dm),
      dm->getNumEdges(dm),
      dm->getLoopDataLayout(dm),
      dm->getNumLoops(dm),
      dm->getPolyDataLayout(dm),
      dm->getNumPolys(dm),
      false, /* setting mask here isn't useful, gives false positives */
      do_verbose,
      do_fixes,
      &changed);

  is_valid &= BKE_mesh_validate_arrays(NULL,
                                       dm->getVertArray(dm),
                                       dm->getNumVerts(dm),
                                       dm->getEdgeArray(dm),
                                       dm->getNumEdges(dm),
                                       dm->getTessFaceArray(dm),
                                       dm->getNumTessFaces(dm),
                                       dm->getLoopArray(dm),
                                       dm->getNumLoops(dm),
                                       dm->getPolyArray(dm),
                                       dm->getNumPolys(dm),
                                       dm->getVertDataArray(dm, CD_MDEFORMVERT),
                                       do_verbose,
                                       do_fixes,
                                       &changed);

  BLI_assert(changed == false);

  return is_valid;
}

#endif /* NDEBUG */
