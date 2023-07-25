/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "atomic_ops.h"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_edgehash.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_ccg.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_scene.h"
#include "BKE_subsurf.h"

#include "CCGSubSurf.h"

static CCGDerivedMesh *getCCGDerivedMesh(CCGSubSurf *ss,
                                         int drawInteriorEdges,
                                         int useSubsurfUv,
                                         DerivedMesh *dm);
///

static void *arena_alloc(CCGAllocatorHDL a, int numBytes)
{
  return BLI_memarena_alloc(reinterpret_cast<MemArena *>(a), numBytes);
}

static void *arena_realloc(CCGAllocatorHDL a, void *ptr, int newSize, int oldSize)
{
  void *p2 = BLI_memarena_alloc(reinterpret_cast<MemArena *>(a), newSize);
  if (ptr) {
    memcpy(p2, ptr, oldSize);
  }
  return p2;
}

static void arena_free(CCGAllocatorHDL /*a*/, void * /*ptr*/)
{
  /* do nothing */
}

static void arena_release(CCGAllocatorHDL a)
{
  BLI_memarena_free(reinterpret_cast<MemArena *>(a));
}

enum CCGFlags {
  CCG_USE_AGING = 1,
  CCG_USE_ARENA = 2,
  CCG_CALC_NORMALS = 4,
  /* add an extra four bytes for a mask layer */
  CCG_ALLOC_MASK = 8,
  CCG_SIMPLE_SUBDIV = 16,
};
ENUM_OPERATORS(CCGFlags, CCG_SIMPLE_SUBDIV);

static CCGSubSurf *_getSubSurf(CCGSubSurf *prevSS, int subdivLevels, int numLayers, CCGFlags flags)
{
  CCGMeshIFC ifc;
  CCGSubSurf *ccgSS;
  int useAging = !!(flags & CCG_USE_AGING);
  int useArena = flags & CCG_USE_ARENA;
  int normalOffset = 0;

  /* (subdivLevels == 0) is not allowed */
  subdivLevels = MAX2(subdivLevels, 1);

  if (prevSS) {
    int oldUseAging;

    ccgSubSurf_getUseAgeCounts(prevSS, &oldUseAging, nullptr, nullptr, nullptr);

    if ((oldUseAging != useAging) ||
        (ccgSubSurf_getSimpleSubdiv(prevSS) != !!(flags & CCG_SIMPLE_SUBDIV)))
    {
      ccgSubSurf_free(prevSS);
    }
    else {
      ccgSubSurf_setSubdivisionLevels(prevSS, subdivLevels);

      return prevSS;
    }
  }

  if (useAging) {
    ifc.vertUserSize = ifc.edgeUserSize = ifc.faceUserSize = 12;
  }
  else {
    ifc.vertUserSize = ifc.edgeUserSize = ifc.faceUserSize = 8;
  }
  ifc.numLayers = numLayers;
  ifc.vertDataSize = sizeof(float) * numLayers;
  normalOffset += sizeof(float) * numLayers;
  if (flags & CCG_CALC_NORMALS) {
    ifc.vertDataSize += sizeof(float[3]);
  }
  if (flags & CCG_ALLOC_MASK) {
    ifc.vertDataSize += sizeof(float);
  }
  ifc.simpleSubdiv = !!(flags & CCG_SIMPLE_SUBDIV);

  if (useArena) {
    CCGAllocatorIFC allocatorIFC;
    CCGAllocatorHDL allocator = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 16), "subsurf arena");

    allocatorIFC.alloc = arena_alloc;
    allocatorIFC.realloc = arena_realloc;
    allocatorIFC.free = arena_free;
    allocatorIFC.release = arena_release;

    ccgSS = ccgSubSurf_new(&ifc, subdivLevels, &allocatorIFC, allocator);
  }
  else {
    ccgSS = ccgSubSurf_new(&ifc, subdivLevels, nullptr, nullptr);
  }

  if (useAging) {
    ccgSubSurf_setUseAgeCounts(ccgSS, 1, 8, 8, 8);
  }

  if (flags & CCG_ALLOC_MASK) {
    normalOffset += sizeof(float);
    /* mask is allocated after regular layers */
    ccgSubSurf_setAllocMask(ccgSS, 1, sizeof(float) * numLayers);
  }

  if (flags & CCG_CALC_NORMALS) {
    ccgSubSurf_setCalcVertexNormals(ccgSS, 1, normalOffset);
  }
  else {
    ccgSubSurf_setCalcVertexNormals(ccgSS, 0, 0);
  }

  return ccgSS;
}

static int getEdgeIndex(CCGSubSurf *ss, CCGEdge *e, int x, int edgeSize)
{
  CCGVert *v0 = ccgSubSurf_getEdgeVert0(e);
  CCGVert *v1 = ccgSubSurf_getEdgeVert1(e);
  int v0idx = *((int *)ccgSubSurf_getVertUserData(ss, v0));
  int v1idx = *((int *)ccgSubSurf_getVertUserData(ss, v1));
  int edgeBase = *((int *)ccgSubSurf_getEdgeUserData(ss, e));

  if (x == 0) {
    return v0idx;
  }
  if (x == edgeSize - 1) {
    return v1idx;
  }

  return edgeBase + x - 1;
}

static int getFaceIndex(
    CCGSubSurf *ss, CCGFace *f, int S, int x, int y, int edgeSize, int gridSize)
{
  int faceBase = *((int *)ccgSubSurf_getFaceUserData(ss, f));
  int numVerts = ccgSubSurf_getFaceNumVerts(f);

  if (x == gridSize - 1 && y == gridSize - 1) {
    CCGVert *v = ccgSubSurf_getFaceVert(f, S);
    return *((int *)ccgSubSurf_getVertUserData(ss, v));
  }
  if (x == gridSize - 1) {
    CCGVert *v = ccgSubSurf_getFaceVert(f, S);
    CCGEdge *e = ccgSubSurf_getFaceEdge(f, S);
    int edgeBase = *((int *)ccgSubSurf_getEdgeUserData(ss, e));
    if (v == ccgSubSurf_getEdgeVert0(e)) {
      return edgeBase + (gridSize - 1 - y) - 1;
    }

    return edgeBase + (edgeSize - 2 - 1) - ((gridSize - 1 - y) - 1);
  }
  if (y == gridSize - 1) {
    CCGVert *v = ccgSubSurf_getFaceVert(f, S);
    CCGEdge *e = ccgSubSurf_getFaceEdge(f, (S + numVerts - 1) % numVerts);
    int edgeBase = *((int *)ccgSubSurf_getEdgeUserData(ss, e));
    if (v == ccgSubSurf_getEdgeVert0(e)) {
      return edgeBase + (gridSize - 1 - x) - 1;
    }

    return edgeBase + (edgeSize - 2 - 1) - ((gridSize - 1 - x) - 1);
  }
  if (x == 0 && y == 0) {
    return faceBase;
  }
  if (x == 0) {
    S = (S + numVerts - 1) % numVerts;
    return faceBase + 1 + (gridSize - 2) * S + (y - 1);
  }
  if (y == 0) {
    return faceBase + 1 + (gridSize - 2) * S + (x - 1);
  }

  return faceBase + 1 + (gridSize - 2) * numVerts + S * (gridSize - 2) * (gridSize - 2) +
         (y - 1) * (gridSize - 2) + (x - 1);
}

static void get_face_uv_map_vert(UvVertMap *vmap,
                                 const blender::OffsetIndices<int> faces,
                                 const int *face_verts,
                                 int fi,
                                 CCGVertHDL *fverts)
{
  UvMapVert *v, *nv;
  int j, nverts = faces[fi].size();

  for (j = 0; j < nverts; j++) {
    for (nv = v = BKE_mesh_uv_vert_map_get_vert(vmap, face_verts[j]); v; v = v->next) {
      if (v->separate) {
        nv = v;
      }
      if (v->face_index == fi) {
        break;
      }
    }

    fverts[j] = POINTER_FROM_UINT(faces[nv->face_index].start() + nv->loop_of_face_index);
  }
}

static int ss_sync_from_uv(CCGSubSurf *ss,
                           CCGSubSurf *origss,
                           DerivedMesh *dm,
                           const float (*mloopuv)[2])
{
  const blender::OffsetIndices faces(blender::Span(dm->getPolyArray(dm), dm->getNumPolys(dm) + 1));
  int *corner_verts = dm->getCornerVertArray(dm);
  int totvert = dm->getNumVerts(dm);
  int totface = dm->getNumPolys(dm);
  int i, seam;
  UvMapVert *v;
  UvVertMap *vmap;
  blender::Vector<CCGVertHDL, 16> fverts;
  float limit[2];
  EdgeSet *eset;
  float uv[3] = {0.0f, 0.0f, 0.0f}; /* only first 2 values are written into */

  limit[0] = limit[1] = STD_UV_CONNECT_LIMIT;
  /* previous behavior here is without accounting for winding, however this causes stretching in
   * UV map in really simple cases with mirror + subsurf, see second part of #44530.
   * Also, initially intention is to treat merged vertices from mirror modifier as seams.
   * This fixes a very old regression (2.49 was correct here) */
  vmap = BKE_mesh_uv_vert_map_create(
      faces, nullptr, nullptr, corner_verts, mloopuv, totvert, limit, false, true);
  if (!vmap) {
    return 0;
  }

  ccgSubSurf_initFullSync(ss);

  /* create vertices */
  for (i = 0; i < totvert; i++) {
    if (!BKE_mesh_uv_vert_map_get_vert(vmap, i)) {
      continue;
    }

    for (v = BKE_mesh_uv_vert_map_get_vert(vmap, i)->next; v; v = v->next) {
      if (v->separate) {
        break;
      }
    }

    seam = (v != nullptr);

    for (v = BKE_mesh_uv_vert_map_get_vert(vmap, i); v; v = v->next) {
      if (v->separate) {
        CCGVert *ssv;
        int loopid = faces[v->face_index].start() + v->loop_of_face_index;
        CCGVertHDL vhdl = POINTER_FROM_INT(loopid);

        copy_v2_v2(uv, mloopuv[loopid]);

        ccgSubSurf_syncVert(ss, vhdl, uv, seam, &ssv);
      }
    }
  }

  /* create edges */
  eset = BLI_edgeset_new_ex(__func__, BLI_EDGEHASH_SIZE_GUESS_FROM_FACES(totface));

  for (i = 0; i < totface; i++) {
    const blender::IndexRange face = faces[i];
    int nverts = face.size();
    int j, j_next;
    CCGFace *origf = ccgSubSurf_getFace(origss, POINTER_FROM_INT(i));
    // uint *fv = &face.v1;

    fverts.reinitialize(nverts);

    get_face_uv_map_vert(vmap, faces, &corner_verts[face.start()], i, fverts.data());

    for (j = 0, j_next = nverts - 1; j < nverts; j_next = j++) {
      uint v0 = POINTER_AS_UINT(fverts[j_next]);
      uint v1 = POINTER_AS_UINT(fverts[j]);

      if (BLI_edgeset_add(eset, v0, v1)) {
        CCGEdge *e, *orige = ccgSubSurf_getFaceEdge(origf, j_next);
        CCGEdgeHDL ehdl = POINTER_FROM_INT(face[j_next]);
        float crease = ccgSubSurf_getEdgeCrease(orige);

        ccgSubSurf_syncEdge(ss, ehdl, fverts[j_next], fverts[j], crease, &e);
      }
    }
  }

  BLI_edgeset_free(eset);

  /* create faces */
  for (i = 0; i < totface; i++) {
    const blender::IndexRange face = faces[i];
    int nverts = face.size();
    CCGFace *f;

    fverts.reinitialize(nverts);

    get_face_uv_map_vert(vmap, faces, &corner_verts[face.start()], i, fverts.data());
    ccgSubSurf_syncFace(ss, POINTER_FROM_INT(i), nverts, fverts.data(), &f);
  }

  BKE_mesh_uv_vert_map_free(vmap);
  ccgSubSurf_processSync(ss);

  return 1;
}

static void set_subsurf_legacy_uv(CCGSubSurf *ss, DerivedMesh *dm, DerivedMesh *result, int n)
{
  CCGFaceIterator fi;
  int index, gridSize, gridFaces, /*edgeSize,*/ totface, x, y, S;
  const float(*dmloopuv)[2] = static_cast<const float(*)[2]>(
      CustomData_get_layer_n(&dm->loopData, CD_PROP_FLOAT2, n));
  /* need to update both CD_MTFACE & CD_PROP_FLOAT2, hrmf, we could get away with
   * just tface except applying the modifier then looses subsurf UV */
  MTFace *tface = static_cast<MTFace *>(
      CustomData_get_layer_n_for_write(&result->faceData, CD_MTFACE, n, result->numTessFaceData));
  float(*mloopuv)[2] = static_cast<float(*)[2]>(CustomData_get_layer_n_for_write(
      &result->loopData, CD_PROP_FLOAT2, n, result->getNumLoops(result)));

  if (!dmloopuv || (!tface && !mloopuv)) {
    return;
  }

  /* create a CCGSubSurf from uv's */
  CCGSubSurf *uvss = _getSubSurf(nullptr, ccgSubSurf_getSubdivisionLevels(ss), 2, CCG_USE_ARENA);

  if (!ss_sync_from_uv(uvss, ss, dm, dmloopuv)) {
    ccgSubSurf_free(uvss);
    return;
  }

  /* get some info from CCGSubSurf */
  totface = ccgSubSurf_getNumFaces(uvss);
  // edgeSize = ccgSubSurf_getEdgeSize(uvss); /* UNUSED */
  gridSize = ccgSubSurf_getGridSize(uvss);
  gridFaces = gridSize - 1;

  /* make a map from original faces to CCGFaces */
  CCGFace **faceMap = static_cast<CCGFace **>(
      MEM_mallocN(totface * sizeof(*faceMap), "facemapuv"));
  for (ccgSubSurf_initFaceIterator(uvss, &fi); !ccgFaceIterator_isStopped(&fi);
       ccgFaceIterator_next(&fi))
  {
    CCGFace *f = ccgFaceIterator_getCurrent(&fi);
    faceMap[POINTER_AS_INT(ccgSubSurf_getFaceFaceHandle(f))] = f;
  }

  /* load coordinates from uvss into tface */
  MTFace *tf = tface;
  float(*mluv)[2] = mloopuv;

  for (index = 0; index < totface; index++) {
    CCGFace *f = faceMap[index];
    int numVerts = ccgSubSurf_getFaceNumVerts(f);

    for (S = 0; S < numVerts; S++) {
      float(*faceGridData)[2] = static_cast<float(*)[2]>(
          ccgSubSurf_getFaceGridDataArray(uvss, f, S));

      for (y = 0; y < gridFaces; y++) {
        for (x = 0; x < gridFaces; x++) {
          float *a = faceGridData[(y + 0) * gridSize + x + 0];
          float *b = faceGridData[(y + 0) * gridSize + x + 1];
          float *c = faceGridData[(y + 1) * gridSize + x + 1];
          float *d = faceGridData[(y + 1) * gridSize + x + 0];

          if (tf) {
            copy_v2_v2(tf->uv[0], a);
            copy_v2_v2(tf->uv[1], d);
            copy_v2_v2(tf->uv[2], c);
            copy_v2_v2(tf->uv[3], b);
            tf++;
          }

          if (mluv) {
            copy_v2_v2(mluv[0], a);
            copy_v2_v2(mluv[1], d);
            copy_v2_v2(mluv[2], c);
            copy_v2_v2(mluv[3], b);
            mluv += 4;
          }
        }
      }
    }
  }

  ccgSubSurf_free(uvss);
  MEM_freeN(faceMap);
}

static void set_subsurf_uv(CCGSubSurf *ss, DerivedMesh *dm, DerivedMesh *result, int layer_index)
{
  set_subsurf_legacy_uv(ss, dm, result, layer_index);
}

/* face weighting */
#define SUB_ELEMS_FACE 50
typedef float FaceVertWeight[SUB_ELEMS_FACE][SUB_ELEMS_FACE];

struct FaceVertWeightEntry {
  FaceVertWeight *weight;
  float *w;
  int valid;
};

struct WeightTable {
  FaceVertWeightEntry *weight_table;
  int len;
};

static float *get_ss_weights(WeightTable *wtable, int gridCuts, int faceLen)
{
  int x, y, i, j;
  float *w, w1, w2, w4, fac, fac2, fx, fy;

  if (wtable->len <= faceLen) {
    void *tmp = MEM_callocN(sizeof(FaceVertWeightEntry) * (faceLen + 1), "weight table alloc 2");

    if (wtable->len) {
      memcpy(tmp, wtable->weight_table, sizeof(FaceVertWeightEntry) * wtable->len);
      MEM_freeN(wtable->weight_table);
    }

    wtable->weight_table = static_cast<FaceVertWeightEntry *>(tmp);
    wtable->len = faceLen + 1;
  }

  if (!wtable->weight_table[faceLen].valid) {
    wtable->weight_table[faceLen].valid = 1;
    wtable->weight_table[faceLen].w = w = static_cast<float *>(
        MEM_callocN(sizeof(float) * faceLen * faceLen * (gridCuts + 2) * (gridCuts + 2),
                    "weight table alloc"));
    fac = 1.0f / float(faceLen);

    for (i = 0; i < faceLen; i++) {
      for (x = 0; x < gridCuts + 2; x++) {
        for (y = 0; y < gridCuts + 2; y++) {
          fx = 0.5f - float(x) / float(gridCuts + 1) / 2.0f;
          fy = 0.5f - float(y) / float(gridCuts + 1) / 2.0f;

          fac2 = faceLen - 4;
          w1 = (1.0f - fx) * (1.0f - fy) + (-fac2 * fx * fy * fac);
          w2 = (1.0f - fx + fac2 * fx * -fac) * (fy);
          w4 = (fx) * (1.0f - fy + -fac2 * fy * fac);

          /* these values aren't used for tri's and cause divide by zero */
          if (faceLen > 3) {
            fac2 = 1.0f - (w1 + w2 + w4);
            fac2 = fac2 / float(faceLen - 3);
            for (j = 0; j < faceLen; j++) {
              w[j] = fac2;
            }
          }

          w[i] = w1;
          w[(i - 1 + faceLen) % faceLen] = w2;
          w[(i + 1) % faceLen] = w4;

          w += faceLen;
        }
      }
    }
  }

  return wtable->weight_table[faceLen].w;
}

static void free_ss_weights(WeightTable *wtable)
{
  int i;

  for (i = 0; i < wtable->len; i++) {
    if (wtable->weight_table[i].valid) {
      MEM_freeN(wtable->weight_table[i].w);
    }
  }

  if (wtable->weight_table) {
    MEM_freeN(wtable->weight_table);
  }
}

static void ss_sync_ccg_from_derivedmesh(CCGSubSurf *ss,
                                         DerivedMesh *dm,
                                         float (*vertexCos)[3],
                                         int useFlatSubdiv)
{
  float creaseFactor = float(ccgSubSurf_getSubdivisionLevels(ss));
  blender::Vector<CCGVertHDL, 16> fverts;
  float(*positions)[3] = (float(*)[3])dm->getVertArray(dm);
  const blender::int2 *edges = reinterpret_cast<const blender::int2 *>(dm->getEdgeArray(dm));
  int *corner_verts = dm->getCornerVertArray(dm);
  const blender::OffsetIndices faces(blender::Span(dm->getPolyArray(dm), dm->getNumPolys(dm) + 1));
  int totvert = dm->getNumVerts(dm);
  int totedge = dm->getNumEdges(dm);
  int i, j;
  int *index;

  ccgSubSurf_initFullSync(ss);

  index = (int *)dm->getVertDataArray(dm, CD_ORIGINDEX);
  for (i = 0; i < totvert; i++) {
    CCGVert *v;

    if (vertexCos) {
      ccgSubSurf_syncVert(ss, POINTER_FROM_INT(i), vertexCos[i], 0, &v);
    }
    else {
      ccgSubSurf_syncVert(ss, POINTER_FROM_INT(i), positions[i], 0, &v);
    }

    ((int *)ccgSubSurf_getVertUserData(ss, v))[1] = (index) ? *index++ : i;
  }

  index = (int *)dm->getEdgeDataArray(dm, CD_ORIGINDEX);
  const float *creases = (const float *)CustomData_get_layer_named(
      &dm->edgeData, CD_PROP_FLOAT, "crease_edge");
  for (i = 0; i < totedge; i++) {
    CCGEdge *e;
    float crease;

    crease = useFlatSubdiv ? creaseFactor : (creases ? creases[i] * creaseFactor : 0.0f);

    ccgSubSurf_syncEdge(ss,
                        POINTER_FROM_INT(i),
                        POINTER_FROM_UINT(edges[i][0]),
                        POINTER_FROM_UINT(edges[i][1]),
                        crease,
                        &e);

    ((int *)ccgSubSurf_getEdgeUserData(ss, e))[1] = (index) ? *index++ : i;
  }

  index = (int *)dm->getPolyDataArray(dm, CD_ORIGINDEX);
  for (i = 0; i < dm->numPolyData; i++) {
    const blender::IndexRange face = faces[i];

    CCGFace *f;

    fverts.reinitialize(face.size());

    for (j = 0; j < face.size(); j++) {
      fverts[j] = POINTER_FROM_UINT(corner_verts[face[j]]);
    }

    /* This is very bad, means mesh is internally inconsistent.
     * it is not really possible to continue without modifying
     * other parts of code significantly to handle missing faces.
     * since this really shouldn't even be possible we just bail. */
    if (ccgSubSurf_syncFace(ss, POINTER_FROM_INT(i), face.size(), fverts.data(), &f) ==
        eCCGError_InvalidValue)
    {
      static int hasGivenError = 0;

      if (!hasGivenError) {
        // XXX error("Unrecoverable error in SubSurf calculation,"
        //      " mesh is inconsistent.");

        hasGivenError = 1;
      }

      return;
    }

    ((int *)ccgSubSurf_getFaceUserData(ss, f))[1] = (index) ? *index++ : i;
  }

  ccgSubSurf_processSync(ss);
}

static void ss_sync_from_derivedmesh(CCGSubSurf *ss,
                                     DerivedMesh *dm,
                                     float (*vertexCos)[3],
                                     int use_flat_subdiv,
                                     bool /*use_subdiv_uvs*/)
{
  ss_sync_ccg_from_derivedmesh(ss, dm, vertexCos, use_flat_subdiv);
}

/***/

static int ccgDM_getVertMapIndex(CCGSubSurf *ss, CCGVert *v)
{
  return ((int *)ccgSubSurf_getVertUserData(ss, v))[1];
}

static int ccgDM_getEdgeMapIndex(CCGSubSurf *ss, CCGEdge *e)
{
  return ((int *)ccgSubSurf_getEdgeUserData(ss, e))[1];
}

static int ccgDM_getFaceMapIndex(CCGSubSurf *ss, CCGFace *f)
{
  return ((int *)ccgSubSurf_getFaceUserData(ss, f))[1];
}

static void minmax_v3_v3v3(const float vec[3], float min[3], float max[3])
{
  if (min[0] > vec[0]) {
    min[0] = vec[0];
  }
  if (min[1] > vec[1]) {
    min[1] = vec[1];
  }
  if (min[2] > vec[2]) {
    min[2] = vec[2];
  }
  if (max[0] < vec[0]) {
    max[0] = vec[0];
  }
  if (max[1] < vec[1]) {
    max[1] = vec[1];
  }
  if (max[2] < vec[2]) {
    max[2] = vec[2];
  }
}

static void UNUSED_FUNCTION(ccgDM_getMinMax)(DerivedMesh *dm, float r_min[3], float r_max[3])
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
  CCGSubSurf *ss = ccgdm->ss;
  CCGVertIterator vi;
  CCGEdgeIterator ei;
  CCGFaceIterator fi;
  CCGKey key;
  int i, edgeSize = ccgSubSurf_getEdgeSize(ss);
  int gridSize = ccgSubSurf_getGridSize(ss);

  CCG_key_top_level(&key, ss);

  if (!ccgSubSurf_getNumVerts(ss)) {
    r_min[0] = r_min[1] = r_min[2] = r_max[0] = r_max[1] = r_max[2] = 0.0;
  }

  for (ccgSubSurf_initVertIterator(ss, &vi); !ccgVertIterator_isStopped(&vi);
       ccgVertIterator_next(&vi))
  {
    CCGVert *v = ccgVertIterator_getCurrent(&vi);
    float *co = static_cast<float *>(ccgSubSurf_getVertData(ss, v));

    minmax_v3_v3v3(co, r_min, r_max);
  }

  for (ccgSubSurf_initEdgeIterator(ss, &ei); !ccgEdgeIterator_isStopped(&ei);
       ccgEdgeIterator_next(&ei))
  {
    CCGEdge *e = ccgEdgeIterator_getCurrent(&ei);
    CCGElem *edgeData = static_cast<CCGElem *>(ccgSubSurf_getEdgeDataArray(ss, e));

    for (i = 0; i < edgeSize; i++) {
      minmax_v3_v3v3(CCG_elem_offset_co(&key, edgeData, i), r_min, r_max);
    }
  }

  for (ccgSubSurf_initFaceIterator(ss, &fi); !ccgFaceIterator_isStopped(&fi);
       ccgFaceIterator_next(&fi))
  {
    CCGFace *f = ccgFaceIterator_getCurrent(&fi);
    int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(f);

    for (S = 0; S < numVerts; S++) {
      CCGElem *faceGridData = static_cast<CCGElem *>(ccgSubSurf_getFaceGridDataArray(ss, f, S));

      for (y = 0; y < gridSize; y++) {
        for (x = 0; x < gridSize; x++) {
          minmax_v3_v3v3(CCG_grid_elem_co(&key, faceGridData, x, y), r_min, r_max);
        }
      }
    }
  }
}

static int ccgDM_getNumVerts(DerivedMesh *dm)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;

  return ccgSubSurf_getNumFinalVerts(ccgdm->ss);
}

static int ccgDM_getNumEdges(DerivedMesh *dm)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;

  return ccgSubSurf_getNumFinalEdges(ccgdm->ss);
}

static int ccgDM_getNumPolys(DerivedMesh *dm)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;

  return ccgSubSurf_getNumFinalFaces(ccgdm->ss);
}

static int ccgDM_getNumLoops(DerivedMesh *dm)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;

  /* All subsurf faces are quads */
  return 4 * ccgSubSurf_getNumFinalFaces(ccgdm->ss);
}

/* utility function */
BLI_INLINE void ccgDM_to_MVert(float mv[3], const CCGKey *key, CCGElem *elem)
{
  copy_v3_v3(mv, CCG_elem_co(key, elem));
}

static void ccgDM_copyFinalVertArray(DerivedMesh *dm, float (*r_positions)[3])
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
  CCGSubSurf *ss = ccgdm->ss;
  CCGElem *vd;
  CCGKey key;
  int index;
  int totvert, totedge, totface;
  int gridSize = ccgSubSurf_getGridSize(ss);
  int edgeSize = ccgSubSurf_getEdgeSize(ss);
  uint i = 0;

  CCG_key_top_level(&key, ss);

  totface = ccgSubSurf_getNumFaces(ss);
  for (index = 0; index < totface; index++) {
    CCGFace *f = ccgdm->faceMap[index].face;
    int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(f);

    vd = static_cast<CCGElem *>(ccgSubSurf_getFaceCenterData(f));
    ccgDM_to_MVert(r_positions[i++], &key, vd);

    for (S = 0; S < numVerts; S++) {
      for (x = 1; x < gridSize - 1; x++) {
        vd = static_cast<CCGElem *>(ccgSubSurf_getFaceGridEdgeData(ss, f, S, x));
        ccgDM_to_MVert(r_positions[i++], &key, vd);
      }
    }

    for (S = 0; S < numVerts; S++) {
      for (y = 1; y < gridSize - 1; y++) {
        for (x = 1; x < gridSize - 1; x++) {
          vd = static_cast<CCGElem *>(ccgSubSurf_getFaceGridData(ss, f, S, x, y));
          ccgDM_to_MVert(r_positions[i++], &key, vd);
        }
      }
    }
  }

  totedge = ccgSubSurf_getNumEdges(ss);
  for (index = 0; index < totedge; index++) {
    CCGEdge *e = ccgdm->edgeMap[index].edge;
    int x;

    for (x = 1; x < edgeSize - 1; x++) {
      /* NOTE(@ideasman42): This gives errors with `--debug-fpe` the normals don't seem to be
       * unit length. This is most likely caused by edges with no faces which are now zeroed out,
       * see comment in: `ccgSubSurf__calcVertNormals()`. */
      vd = static_cast<CCGElem *>(ccgSubSurf_getEdgeData(ss, e, x));
      ccgDM_to_MVert(r_positions[i++], &key, vd);
    }
  }

  totvert = ccgSubSurf_getNumVerts(ss);
  for (index = 0; index < totvert; index++) {
    CCGVert *v = ccgdm->vertMap[index].vert;

    vd = static_cast<CCGElem *>(ccgSubSurf_getVertData(ss, v));
    ccgDM_to_MVert(r_positions[i++], &key, vd);
  }
}

/* utility function */
BLI_INLINE void ccgDM_to_MEdge(vec2i *edge, const int v1, const int v2)
{
  edge->x = v1;
  edge->y = v2;
}

static void ccgDM_copyFinalEdgeArray(DerivedMesh *dm, vec2i *edges)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
  CCGSubSurf *ss = ccgdm->ss;
  int index;
  int totedge, totface;
  int gridSize = ccgSubSurf_getGridSize(ss);
  int edgeSize = ccgSubSurf_getEdgeSize(ss);
  uint i = 0;

  totface = ccgSubSurf_getNumFaces(ss);
  for (index = 0; index < totface; index++) {
    CCGFace *f = ccgdm->faceMap[index].face;
    int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(f);

    for (S = 0; S < numVerts; S++) {
      for (x = 0; x < gridSize - 1; x++) {
        ccgDM_to_MEdge(&edges[i++],
                       getFaceIndex(ss, f, S, x, 0, edgeSize, gridSize),
                       getFaceIndex(ss, f, S, x + 1, 0, edgeSize, gridSize));
      }

      for (x = 1; x < gridSize - 1; x++) {
        for (y = 0; y < gridSize - 1; y++) {
          ccgDM_to_MEdge(&edges[i++],
                         getFaceIndex(ss, f, S, x, y, edgeSize, gridSize),
                         getFaceIndex(ss, f, S, x, y + 1, edgeSize, gridSize));
          ccgDM_to_MEdge(&edges[i++],
                         getFaceIndex(ss, f, S, y, x, edgeSize, gridSize),
                         getFaceIndex(ss, f, S, y + 1, x, edgeSize, gridSize));
        }
      }
    }
  }

  totedge = ccgSubSurf_getNumEdges(ss);
  for (index = 0; index < totedge; index++) {
    CCGEdge *e = ccgdm->edgeMap[index].edge;
    for (int x = 0; x < edgeSize - 1; x++) {
      ccgDM_to_MEdge(
          &edges[i++], getEdgeIndex(ss, e, x, edgeSize), getEdgeIndex(ss, e, x + 1, edgeSize));
    }
  }
}

struct CopyFinalLoopArrayData {
  CCGDerivedMesh *ccgdm;
  int *corner_verts;
  int *corner_edges;
  int grid_size;
  int *grid_offset;
  int edge_size;
};

static void copyFinalLoopArray_task_cb(void *__restrict userdata,
                                       const int iter,
                                       const TaskParallelTLS *__restrict /*tls*/)
{
  CopyFinalLoopArrayData *data = static_cast<CopyFinalLoopArrayData *>(userdata);
  CCGDerivedMesh *ccgdm = data->ccgdm;
  CCGSubSurf *ss = ccgdm->ss;
  const int grid_size = data->grid_size;
  const int edge_size = data->edge_size;
  CCGFace *f = ccgdm->faceMap[iter].face;
  const int num_verts = ccgSubSurf_getFaceNumVerts(f);
  const int grid_index = data->grid_offset[iter];
  int *corner_verts = data->corner_verts;
  int *corner_edges = data->corner_edges;

  size_t loop_i = 4 * size_t(grid_index) * (grid_size - 1) * (grid_size - 1);
  for (int S = 0; S < num_verts; S++) {
    for (int y = 0; y < grid_size - 1; y++) {
      for (int x = 0; x < grid_size - 1; x++) {

        const int v1 = getFaceIndex(ss, f, S, x + 0, y + 0, edge_size, grid_size);
        const int v2 = getFaceIndex(ss, f, S, x + 0, y + 1, edge_size, grid_size);
        const int v3 = getFaceIndex(ss, f, S, x + 1, y + 1, edge_size, grid_size);
        const int v4 = getFaceIndex(ss, f, S, x + 1, y + 0, edge_size, grid_size);

        if (corner_verts) {
          corner_verts[loop_i + 0] = v1;
          corner_verts[loop_i + 1] = v2;
          corner_verts[loop_i + 2] = v3;
          corner_verts[loop_i + 3] = v4;
        }
        if (corner_edges) {
          corner_edges[loop_i + 0] = POINTER_AS_UINT(BLI_edgehash_lookup(ccgdm->ehash, v1, v2));
          corner_edges[loop_i + 1] = POINTER_AS_UINT(BLI_edgehash_lookup(ccgdm->ehash, v2, v3));
          corner_edges[loop_i + 2] = POINTER_AS_UINT(BLI_edgehash_lookup(ccgdm->ehash, v3, v4));
          corner_edges[loop_i + 3] = POINTER_AS_UINT(BLI_edgehash_lookup(ccgdm->ehash, v4, v1));
        }

        loop_i += 4;
      }
    }
  }
}

static void ccgDM_copyFinalCornerVertArray(DerivedMesh *dm, int *r_corner_verts)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
  CCGSubSurf *ss = ccgdm->ss;

  CopyFinalLoopArrayData data;
  data.ccgdm = ccgdm;
  data.corner_verts = r_corner_verts;
  data.corner_edges = nullptr;
  data.grid_size = ccgSubSurf_getGridSize(ss);
  data.grid_offset = dm->getGridOffset(dm);
  data.edge_size = ccgSubSurf_getEdgeSize(ss);

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.min_iter_per_thread = 1;

  BLI_task_parallel_range(
      0, ccgSubSurf_getNumFaces(ss), &data, copyFinalLoopArray_task_cb, &settings);
}

static void ccgDM_copyFinalCornerEdgeArray(DerivedMesh *dm, int *r_corner_edges)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
  CCGSubSurf *ss = ccgdm->ss;

  if (!ccgdm->ehash) {
    BLI_mutex_lock(&ccgdm->loops_cache_lock);
    if (!ccgdm->ehash) {
      const blender::int2 *medge;
      EdgeHash *ehash;

      ehash = BLI_edgehash_new_ex(__func__, ccgdm->dm.numEdgeData);
      medge = reinterpret_cast<const blender::int2 *>(
          ccgdm->dm.getEdgeArray((DerivedMesh *)ccgdm));

      for (int i = 0; i < ccgdm->dm.numEdgeData; i++) {
        BLI_edgehash_insert(ehash, medge[i][0], medge[i][1], POINTER_FROM_INT(i));
      }

      atomic_cas_ptr((void **)&ccgdm->ehash, ccgdm->ehash, ehash);
    }
    BLI_mutex_unlock(&ccgdm->loops_cache_lock);
  }

  CopyFinalLoopArrayData data;
  data.ccgdm = ccgdm;
  data.corner_verts = nullptr;
  data.corner_edges = r_corner_edges;
  data.grid_size = ccgSubSurf_getGridSize(ss);
  data.grid_offset = dm->getGridOffset(dm);
  data.edge_size = ccgSubSurf_getEdgeSize(ss);

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.min_iter_per_thread = 1;

  BLI_task_parallel_range(
      0, ccgSubSurf_getNumFaces(ss), &data, copyFinalLoopArray_task_cb, &settings);
}

static void ccgDM_copyFinalPolyArray(DerivedMesh *dm, int *r_face_offsets)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
  CCGSubSurf *ss = ccgdm->ss;
  int index;
  int totface;
  int gridSize = ccgSubSurf_getGridSize(ss);
  /* int edgeSize = ccgSubSurf_getEdgeSize(ss); */ /* UNUSED */
  int i = 0, k = 0;

  totface = ccgSubSurf_getNumFaces(ss);
  for (index = 0; index < totface; index++) {
    CCGFace *f = ccgdm->faceMap[index].face;
    int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(f);

    for (S = 0; S < numVerts; S++) {
      for (y = 0; y < gridSize - 1; y++) {
        for (x = 0; x < gridSize - 1; x++) {
          r_face_offsets[i] = k;
          k += 4;
          i++;
        }
      }
    }
  }
  r_face_offsets[i] = k;
}

static void ccgDM_release(DerivedMesh *dm)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;

  DM_release(dm);
  /* Before freeing, need to update the displacement map */
  if (ccgdm->multires.modified_flags) {
    /* Check that mmd still exists */
    if (!ccgdm->multires.local_mmd &&
        BLI_findindex(&ccgdm->multires.ob->modifiers, ccgdm->multires.mmd) == -1)
    {
      ccgdm->multires.mmd = nullptr;
    }

    if (ccgdm->multires.mmd) {
      if (ccgdm->multires.modified_flags & MULTIRES_COORDS_MODIFIED) {
        multires_modifier_update_mdisps(dm, nullptr);
      }
      if (ccgdm->multires.modified_flags & MULTIRES_HIDDEN_MODIFIED) {
        multires_modifier_update_hidden(dm);
      }
    }
  }

  if (ccgdm->ehash) {
    BLI_edgehash_free(ccgdm->ehash, nullptr);
  }

  if (ccgdm->reverseFaceMap) {
    MEM_freeN(ccgdm->reverseFaceMap);
  }
  if (ccgdm->gridFaces) {
    MEM_freeN(ccgdm->gridFaces);
  }
  if (ccgdm->gridData) {
    MEM_freeN(ccgdm->gridData);
  }
  if (ccgdm->gridOffset) {
    MEM_freeN(ccgdm->gridOffset);
  }
  if (ccgdm->gridFlagMats) {
    MEM_freeN(ccgdm->gridFlagMats);
  }
  if (ccgdm->gridHidden) {
    /* Using dm->getNumGrids(dm) accesses freed memory */
    uint numGrids = ccgdm->numGrid;
    for (uint i = 0; i < numGrids; i++) {
      if (ccgdm->gridHidden[i]) {
        MEM_freeN(ccgdm->gridHidden[i]);
      }
    }
    MEM_freeN(ccgdm->gridHidden);
  }
  if (ccgdm->freeSS) {
    ccgSubSurf_free(ccgdm->ss);
  }
  if (ccgdm->pmap) {
    MEM_freeN(ccgdm->pmap);
  }
  if (ccgdm->pmap_mem) {
    MEM_freeN(ccgdm->pmap_mem);
  }
  MEM_freeN(ccgdm->faceFlags);
  MEM_freeN(ccgdm->vertMap);
  MEM_freeN(ccgdm->edgeMap);
  MEM_freeN(ccgdm->faceMap);

  BLI_mutex_end(&ccgdm->loops_cache_lock);
  BLI_rw_mutex_end(&ccgdm->origindex_cache_rwlock);

  MEM_freeN(ccgdm);
}

static void *ccgDM_get_vert_data_layer(DerivedMesh *dm, const eCustomDataType type)
{
  if (type == CD_ORIGINDEX) {
    /* create origindex on demand to save memory */
    CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
    CCGSubSurf *ss = ccgdm->ss;
    int *origindex;
    int a, index, totnone, totorig;

    /* Avoid re-creation if the layer exists already */
    BLI_rw_mutex_lock(&ccgdm->origindex_cache_rwlock, THREAD_LOCK_READ);
    origindex = static_cast<int *>(DM_get_vert_data_layer(dm, CD_ORIGINDEX));
    BLI_rw_mutex_unlock(&ccgdm->origindex_cache_rwlock);
    if (origindex) {
      return origindex;
    }

    BLI_rw_mutex_lock(&ccgdm->origindex_cache_rwlock, THREAD_LOCK_WRITE);

    origindex = static_cast<int *>(
        CustomData_add_layer(&dm->vertData, CD_ORIGINDEX, CD_SET_DEFAULT, dm->numVertData));

    totorig = ccgSubSurf_getNumVerts(ss);
    totnone = dm->numVertData - totorig;

    /* original vertices are at the end */
    for (a = 0; a < totnone; a++) {
      origindex[a] = ORIGINDEX_NONE;
    }

    for (index = 0; index < totorig; index++, a++) {
      CCGVert *v = ccgdm->vertMap[index].vert;
      origindex[a] = ccgDM_getVertMapIndex(ccgdm->ss, v);
    }
    BLI_rw_mutex_unlock(&ccgdm->origindex_cache_rwlock);

    return origindex;
  }

  return DM_get_vert_data_layer(dm, type);
}

static void *ccgDM_get_edge_data_layer(DerivedMesh *dm, const eCustomDataType type)
{
  if (type == CD_ORIGINDEX) {
    /* create origindex on demand to save memory */
    CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
    CCGSubSurf *ss = ccgdm->ss;
    int *origindex;
    int a, i, index, totnone, totorig, totedge;
    int edgeSize = ccgSubSurf_getEdgeSize(ss);

    /* Avoid re-creation if the layer exists already */
    origindex = static_cast<int *>(DM_get_edge_data_layer(dm, CD_ORIGINDEX));
    if (origindex) {
      return origindex;
    }

    origindex = static_cast<int *>(
        CustomData_add_layer(&dm->edgeData, CD_ORIGINDEX, CD_SET_DEFAULT, dm->numEdgeData));

    totedge = ccgSubSurf_getNumEdges(ss);
    totorig = totedge * (edgeSize - 1);
    totnone = dm->numEdgeData - totorig;

    /* original edges are at the end */
    for (a = 0; a < totnone; a++) {
      origindex[a] = ORIGINDEX_NONE;
    }

    for (index = 0; index < totedge; index++) {
      CCGEdge *e = ccgdm->edgeMap[index].edge;
      int mapIndex = ccgDM_getEdgeMapIndex(ss, e);

      for (i = 0; i < edgeSize - 1; i++, a++) {
        origindex[a] = mapIndex;
      }
    }

    return origindex;
  }

  return DM_get_edge_data_layer(dm, type);
}

static void *ccgDM_get_poly_data_layer(DerivedMesh *dm, const eCustomDataType type)
{
  if (type == CD_ORIGINDEX) {
    /* create origindex on demand to save memory */
    CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
    CCGSubSurf *ss = ccgdm->ss;
    int *origindex;
    int a, i, index, totface;
    int gridFaces = ccgSubSurf_getGridSize(ss) - 1;

    /* Avoid re-creation if the layer exists already */
    origindex = static_cast<int *>(DM_get_poly_data_layer(dm, CD_ORIGINDEX));
    if (origindex) {
      return origindex;
    }

    origindex = static_cast<int *>(
        CustomData_add_layer(&dm->polyData, CD_ORIGINDEX, CD_SET_DEFAULT, dm->numPolyData));

    totface = ccgSubSurf_getNumFaces(ss);

    for (a = 0, index = 0; index < totface; index++) {
      CCGFace *f = ccgdm->faceMap[index].face;
      int numVerts = ccgSubSurf_getFaceNumVerts(f);
      int mapIndex = ccgDM_getFaceMapIndex(ss, f);

      for (i = 0; i < gridFaces * gridFaces * numVerts; i++, a++) {
        origindex[a] = mapIndex;
      }
    }

    return origindex;
  }

  return DM_get_poly_data_layer(dm, type);
}

static int ccgDM_getNumGrids(DerivedMesh *dm)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
  int index, numFaces, numGrids;

  numFaces = ccgSubSurf_getNumFaces(ccgdm->ss);
  numGrids = 0;

  for (index = 0; index < numFaces; index++) {
    CCGFace *f = ccgdm->faceMap[index].face;
    numGrids += ccgSubSurf_getFaceNumVerts(f);
  }

  return numGrids;
}

static int ccgDM_getGridSize(DerivedMesh *dm)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
  return ccgSubSurf_getGridSize(ccgdm->ss);
}

static void ccgdm_create_grids(DerivedMesh *dm)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
  CCGSubSurf *ss = ccgdm->ss;
  CCGElem **gridData;
  DMFlagMat *gridFlagMats;
  CCGFace **gridFaces;
  int *gridOffset;
  int index, numFaces, numGrids, S, gIndex /*, gridSize */;

  if (ccgdm->gridData) {
    return;
  }

  numGrids = ccgDM_getNumGrids(dm);
  numFaces = ccgSubSurf_getNumFaces(ss);
  // gridSize = ccgDM_getGridSize(dm); /* UNUSED */

  /* compute offset into grid array for each face */
  gridOffset = static_cast<int *>(MEM_mallocN(sizeof(int) * numFaces, "ccgdm.gridOffset"));

  for (gIndex = 0, index = 0; index < numFaces; index++) {
    CCGFace *f = ccgdm->faceMap[index].face;
    int numVerts = ccgSubSurf_getFaceNumVerts(f);

    gridOffset[index] = gIndex;
    gIndex += numVerts;
  }

  /* compute grid data */
  gridData = static_cast<CCGElem **>(MEM_mallocN(sizeof(CCGElem *) * numGrids, "ccgdm.gridData"));
  gridFaces = static_cast<CCGFace **>(
      MEM_mallocN(sizeof(CCGFace *) * numGrids, "ccgdm.gridFaces"));
  gridFlagMats = static_cast<DMFlagMat *>(
      MEM_mallocN(sizeof(DMFlagMat) * numGrids, "ccgdm.gridFlagMats"));

  ccgdm->gridHidden = static_cast<uint **>(
      MEM_callocN(sizeof(*ccgdm->gridHidden) * numGrids, "ccgdm.gridHidden"));

  for (gIndex = 0, index = 0; index < numFaces; index++) {
    CCGFace *f = ccgdm->faceMap[index].face;
    int numVerts = ccgSubSurf_getFaceNumVerts(f);

    for (S = 0; S < numVerts; S++, gIndex++) {
      gridData[gIndex] = static_cast<CCGElem *>(ccgSubSurf_getFaceGridDataArray(ss, f, S));
      gridFaces[gIndex] = f;
      gridFlagMats[gIndex] = ccgdm->faceFlags[index];
    }
  }

  ccgdm->gridData = gridData;
  ccgdm->gridFaces = gridFaces;
  ccgdm->gridOffset = gridOffset;
  ccgdm->gridFlagMats = gridFlagMats;
  ccgdm->numGrid = numGrids;
}

static CCGElem **ccgDM_getGridData(DerivedMesh *dm)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;

  ccgdm_create_grids(dm);
  return ccgdm->gridData;
}

static int *ccgDM_getGridOffset(DerivedMesh *dm)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;

  ccgdm_create_grids(dm);
  return ccgdm->gridOffset;
}

static void ccgDM_getGridKey(DerivedMesh *dm, CCGKey *key)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
  CCG_key_top_level(key, ccgdm->ss);
}

static DMFlagMat *ccgDM_getGridFlagMats(DerivedMesh *dm)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;

  ccgdm_create_grids(dm);
  return ccgdm->gridFlagMats;
}

static BLI_bitmap **ccgDM_getGridHidden(DerivedMesh *dm)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;

  ccgdm_create_grids(dm);
  return ccgdm->gridHidden;
}

static void set_default_ccgdm_callbacks(CCGDerivedMesh *ccgdm)
{
  ccgdm->dm.getNumVerts = ccgDM_getNumVerts;
  ccgdm->dm.getNumEdges = ccgDM_getNumEdges;
  ccgdm->dm.getNumLoops = ccgDM_getNumLoops;
  ccgdm->dm.getNumPolys = ccgDM_getNumPolys;

  ccgdm->dm.copyVertArray = ccgDM_copyFinalVertArray;
  ccgdm->dm.copyEdgeArray = ccgDM_copyFinalEdgeArray;
  ccgdm->dm.copyCornerVertArray = ccgDM_copyFinalCornerVertArray;
  ccgdm->dm.copyCornerEdgeArray = ccgDM_copyFinalCornerEdgeArray;
  ccgdm->dm.copyPolyArray = ccgDM_copyFinalPolyArray;

  ccgdm->dm.getVertDataArray = ccgDM_get_vert_data_layer;
  ccgdm->dm.getEdgeDataArray = ccgDM_get_edge_data_layer;
  ccgdm->dm.getPolyDataArray = ccgDM_get_poly_data_layer;
  ccgdm->dm.getNumGrids = ccgDM_getNumGrids;
  ccgdm->dm.getGridSize = ccgDM_getGridSize;
  ccgdm->dm.getGridData = ccgDM_getGridData;
  ccgdm->dm.getGridOffset = ccgDM_getGridOffset;
  ccgdm->dm.getGridKey = ccgDM_getGridKey;
  ccgdm->dm.getGridFlagMats = ccgDM_getGridFlagMats;
  ccgdm->dm.getGridHidden = ccgDM_getGridHidden;

  ccgdm->dm.release = ccgDM_release;
}

static void create_ccgdm_maps(CCGDerivedMesh *ccgdm, CCGSubSurf *ss)
{
  CCGVertIterator vi;
  CCGEdgeIterator ei;
  CCGFaceIterator fi;
  int totvert, totedge, totface;

  totvert = ccgSubSurf_getNumVerts(ss);
  ccgdm->vertMap = static_cast<decltype(CCGDerivedMesh::vertMap)>(
      MEM_mallocN(totvert * sizeof(*ccgdm->vertMap), "vertMap"));
  for (ccgSubSurf_initVertIterator(ss, &vi); !ccgVertIterator_isStopped(&vi);
       ccgVertIterator_next(&vi))
  {
    CCGVert *v = ccgVertIterator_getCurrent(&vi);

    ccgdm->vertMap[POINTER_AS_INT(ccgSubSurf_getVertVertHandle(v))].vert = v;
  }

  totedge = ccgSubSurf_getNumEdges(ss);
  ccgdm->edgeMap = static_cast<decltype(CCGDerivedMesh::edgeMap)>(
      MEM_mallocN(totedge * sizeof(*ccgdm->edgeMap), "edgeMap"));
  for (ccgSubSurf_initEdgeIterator(ss, &ei); !ccgEdgeIterator_isStopped(&ei);
       ccgEdgeIterator_next(&ei))
  {
    CCGEdge *e = ccgEdgeIterator_getCurrent(&ei);

    ccgdm->edgeMap[POINTER_AS_INT(ccgSubSurf_getEdgeEdgeHandle(e))].edge = e;
  }

  totface = ccgSubSurf_getNumFaces(ss);
  ccgdm->faceMap = static_cast<decltype(CCGDerivedMesh::faceMap)>(
      MEM_mallocN(totface * sizeof(*ccgdm->faceMap), "faceMap"));
  for (ccgSubSurf_initFaceIterator(ss, &fi); !ccgFaceIterator_isStopped(&fi);
       ccgFaceIterator_next(&fi))
  {
    CCGFace *f = ccgFaceIterator_getCurrent(&fi);

    ccgdm->faceMap[POINTER_AS_INT(ccgSubSurf_getFaceFaceHandle(f))].face = f;
  }
}

/* Fill in all geometry arrays making it possible to access any
 * hires data from the CPU.
 */
static void set_ccgdm_all_geometry(CCGDerivedMesh *ccgdm,
                                   CCGSubSurf *ss,
                                   DerivedMesh *dm,
                                   bool useSubsurfUv)
{
  const int totvert = ccgSubSurf_getNumVerts(ss);
  const int totedge = ccgSubSurf_getNumEdges(ss);
  const int totface = ccgSubSurf_getNumFaces(ss);
  int index;
  int i;
  int vertNum = 0, edgeNum = 0, faceNum = 0;
  DMFlagMat *faceFlags = ccgdm->faceFlags;
  int *polyidx = nullptr;
  blender::Vector<int, 16> loopidx;
  blender::Vector<int, 16> vertidx;
  int loopindex, loopindex2;
  int edgeSize;
  int gridSize;
  int gridFaces, gridCuts;
  int gridSideEdges;
  int gridInternalEdges;
  WeightTable wtable = {nullptr};
  bool has_edge_cd;

  edgeSize = ccgSubSurf_getEdgeSize(ss);
  gridSize = ccgSubSurf_getGridSize(ss);
  gridFaces = gridSize - 1;
  gridCuts = gridSize - 2;
  // gridInternalVerts = gridSideVerts * gridSideVerts; /* As yet, unused. */
  gridSideEdges = gridSize - 1;
  gridInternalEdges = (gridSideEdges - 1) * gridSideEdges * 2;

  const int *material_indices = static_cast<const int *>(
      CustomData_get_layer_named(&dm->polyData, CD_PROP_INT32, "material_index"));
  const bool *sharp_faces = static_cast<const bool *>(
      CustomData_get_layer_named(&dm->polyData, CD_PROP_BOOL, "sharp_face"));

  const int *base_polyOrigIndex = static_cast<const int *>(
      CustomData_get_layer(&dm->polyData, CD_ORIGINDEX));

  int *vertOrigIndex = static_cast<int *>(DM_get_vert_data_layer(&ccgdm->dm, CD_ORIGINDEX));
  int *edgeOrigIndex = static_cast<int *>(DM_get_edge_data_layer(&ccgdm->dm, CD_ORIGINDEX));
  int *polyOrigIndex = static_cast<int *>(DM_get_poly_data_layer(&ccgdm->dm, CD_ORIGINDEX));

  has_edge_cd = ((ccgdm->dm.edgeData.totlayer - (edgeOrigIndex ? 1 : 0)) != 0);

  loopindex = loopindex2 = 0; /* current loop index */
  for (index = 0; index < totface; index++) {
    CCGFace *f = ccgdm->faceMap[index].face;
    int numVerts = ccgSubSurf_getFaceNumVerts(f);
    int numFinalEdges = numVerts * (gridSideEdges + gridInternalEdges);
    int origIndex = POINTER_AS_INT(ccgSubSurf_getFaceFaceHandle(f));
    int g2_wid = gridCuts + 2;
    float *w, *w2;
    int s, x, y;

    w = get_ss_weights(&wtable, gridCuts, numVerts);

    ccgdm->faceMap[index].startVert = vertNum;
    ccgdm->faceMap[index].startEdge = edgeNum;
    ccgdm->faceMap[index].startFace = faceNum;

    faceFlags->sharp = sharp_faces ? sharp_faces[origIndex] : false;
    faceFlags->mat_nr = material_indices ? material_indices[origIndex] : 0;
    faceFlags++;

    /* set the face base vert */
    *((int *)ccgSubSurf_getFaceUserData(ss, f)) = vertNum;

    loopidx.reinitialize(numVerts);
    for (s = 0; s < numVerts; s++) {
      loopidx[s] = loopindex++;
    }

    vertidx.reinitialize(numVerts);
    for (s = 0; s < numVerts; s++) {
      CCGVert *v = ccgSubSurf_getFaceVert(f, s);
      vertidx[s] = POINTER_AS_INT(ccgSubSurf_getVertVertHandle(v));
    }

    /* I think this is for interpolating the center vert? */
    w2 = w;  // + numVerts*(g2_wid-1) * (g2_wid-1); //numVerts*((g2_wid-1) * g2_wid+g2_wid-1);
    DM_interp_vert_data(dm, &ccgdm->dm, vertidx.data(), w2, numVerts, vertNum);
    if (vertOrigIndex) {
      *vertOrigIndex = ORIGINDEX_NONE;
      vertOrigIndex++;
    }

    vertNum++;

    /* Interpolate per-vert data. */
    for (s = 0; s < numVerts; s++) {
      for (x = 1; x < gridFaces; x++) {
        w2 = w + s * numVerts * g2_wid * g2_wid + x * numVerts;
        DM_interp_vert_data(dm, &ccgdm->dm, vertidx.data(), w2, numVerts, vertNum);

        if (vertOrigIndex) {
          *vertOrigIndex = ORIGINDEX_NONE;
          vertOrigIndex++;
        }

        vertNum++;
      }
    }

    /* Interpolate per-vert data. */
    for (s = 0; s < numVerts; s++) {
      for (y = 1; y < gridFaces; y++) {
        for (x = 1; x < gridFaces; x++) {
          w2 = w + s * numVerts * g2_wid * g2_wid + (y * g2_wid + x) * numVerts;
          DM_interp_vert_data(dm, &ccgdm->dm, vertidx.data(), w2, numVerts, vertNum);

          if (vertOrigIndex) {
            *vertOrigIndex = ORIGINDEX_NONE;
            vertOrigIndex++;
          }

          vertNum++;
        }
      }
    }

    if (edgeOrigIndex) {
      for (i = 0; i < numFinalEdges; i++) {
        edgeOrigIndex[edgeNum + i] = ORIGINDEX_NONE;
      }
    }

    for (s = 0; s < numVerts; s++) {
      /* Interpolate per-face data. */
      for (y = 0; y < gridFaces; y++) {
        for (x = 0; x < gridFaces; x++) {
          w2 = w + s * numVerts * g2_wid * g2_wid + (y * g2_wid + x) * numVerts;
          CustomData_interp(&dm->loopData,
                            &ccgdm->dm.loopData,
                            loopidx.data(),
                            w2,
                            nullptr,
                            numVerts,
                            loopindex2);
          loopindex2++;

          w2 = w + s * numVerts * g2_wid * g2_wid + ((y + 1) * g2_wid + (x)) * numVerts;
          CustomData_interp(&dm->loopData,
                            &ccgdm->dm.loopData,
                            loopidx.data(),
                            w2,
                            nullptr,
                            numVerts,
                            loopindex2);
          loopindex2++;

          w2 = w + s * numVerts * g2_wid * g2_wid + ((y + 1) * g2_wid + (x + 1)) * numVerts;
          CustomData_interp(&dm->loopData,
                            &ccgdm->dm.loopData,
                            loopidx.data(),
                            w2,
                            nullptr,
                            numVerts,
                            loopindex2);
          loopindex2++;

          w2 = w + s * numVerts * g2_wid * g2_wid + ((y)*g2_wid + (x + 1)) * numVerts;
          CustomData_interp(&dm->loopData,
                            &ccgdm->dm.loopData,
                            loopidx.data(),
                            w2,
                            nullptr,
                            numVerts,
                            loopindex2);
          loopindex2++;

          CustomData_copy_data(&dm->polyData, &ccgdm->dm.polyData, origIndex, faceNum, 1);

          if (polyOrigIndex) {
            *polyOrigIndex = base_polyOrigIndex ? base_polyOrigIndex[origIndex] : origIndex;
            polyOrigIndex++;
          }

          ccgdm->reverseFaceMap[faceNum] = index;

          /* This is a simple one to one mapping, here... */
          if (polyidx) {
            polyidx[faceNum] = faceNum;
          }

          faceNum++;
        }
      }
    }

    edgeNum += numFinalEdges;
  }

  for (index = 0; index < totedge; index++) {
    CCGEdge *e = ccgdm->edgeMap[index].edge;
    int numFinalEdges = edgeSize - 1;
    int mapIndex = ccgDM_getEdgeMapIndex(ss, e);
    int x;
    int vertIdx[2];
    int edgeIdx = POINTER_AS_INT(ccgSubSurf_getEdgeEdgeHandle(e));

    CCGVert *v;
    v = ccgSubSurf_getEdgeVert0(e);
    vertIdx[0] = POINTER_AS_INT(ccgSubSurf_getVertVertHandle(v));
    v = ccgSubSurf_getEdgeVert1(e);
    vertIdx[1] = POINTER_AS_INT(ccgSubSurf_getVertVertHandle(v));

    ccgdm->edgeMap[index].startVert = vertNum;
    ccgdm->edgeMap[index].startEdge = edgeNum;

    /* set the edge base vert */
    *((int *)ccgSubSurf_getEdgeUserData(ss, e)) = vertNum;

    for (x = 1; x < edgeSize - 1; x++) {
      float w[2];
      w[1] = float(x) / (edgeSize - 1);
      w[0] = 1 - w[1];
      DM_interp_vert_data(dm, &ccgdm->dm, vertIdx, w, 2, vertNum);
      if (vertOrigIndex) {
        *vertOrigIndex = ORIGINDEX_NONE;
        vertOrigIndex++;
      }
      vertNum++;
    }

    if (has_edge_cd) {
      BLI_assert(edgeIdx >= 0 && edgeIdx < dm->getNumEdges(dm));
      for (i = 0; i < numFinalEdges; i++) {
        CustomData_copy_data(&dm->edgeData, &ccgdm->dm.edgeData, edgeIdx, edgeNum + i, 1);
      }
    }

    if (edgeOrigIndex) {
      for (i = 0; i < numFinalEdges; i++) {
        edgeOrigIndex[edgeNum + i] = mapIndex;
      }
    }

    edgeNum += numFinalEdges;
  }

  if (useSubsurfUv) {
    CustomData *ldata = &ccgdm->dm.loopData;
    CustomData *dmldata = &dm->loopData;
    int numlayer = CustomData_number_of_layers(ldata, CD_PROP_FLOAT2);
    int dmnumlayer = CustomData_number_of_layers(dmldata, CD_PROP_FLOAT2);

    for (i = 0; i < numlayer && i < dmnumlayer; i++) {
      set_subsurf_uv(ss, dm, &ccgdm->dm, i);
    }
  }

  for (index = 0; index < totvert; index++) {
    CCGVert *v = ccgdm->vertMap[index].vert;
    int mapIndex = ccgDM_getVertMapIndex(ccgdm->ss, v);
    int vertIdx;

    vertIdx = POINTER_AS_INT(ccgSubSurf_getVertVertHandle(v));

    ccgdm->vertMap[index].startVert = vertNum;

    /* set the vert base vert */
    *((int *)ccgSubSurf_getVertUserData(ss, v)) = vertNum;

    DM_copy_vert_data(dm, &ccgdm->dm, vertIdx, vertNum, 1);

    if (vertOrigIndex) {
      *vertOrigIndex = mapIndex;
      vertOrigIndex++;
    }
    vertNum++;
  }

  free_ss_weights(&wtable);

  BLI_assert(vertNum == ccgSubSurf_getNumFinalVerts(ss));
  BLI_assert(edgeNum == ccgSubSurf_getNumFinalEdges(ss));
  BLI_assert(loopindex2 == ccgSubSurf_getNumFinalFaces(ss) * 4);
  BLI_assert(faceNum == ccgSubSurf_getNumFinalFaces(ss));
}

static CCGDerivedMesh *getCCGDerivedMesh(CCGSubSurf *ss,
                                         int drawInteriorEdges,
                                         int useSubsurfUv,
                                         DerivedMesh *dm)
{
  const int totedge = ccgSubSurf_getNumEdges(ss);
  const int totface = ccgSubSurf_getNumFaces(ss);
  CCGDerivedMesh *ccgdm = MEM_cnew<CCGDerivedMesh>(__func__);

  BLI_assert(totedge == ccgSubSurf_getNumEdges(ss));
  UNUSED_VARS_NDEBUG(totedge);
  BLI_assert(totface == ccgSubSurf_getNumFaces(ss));
  DM_from_template(&ccgdm->dm,
                   dm,
                   DM_TYPE_CCGDM,
                   ccgSubSurf_getNumFinalVerts(ss),
                   ccgSubSurf_getNumFinalEdges(ss),
                   0,
                   ccgSubSurf_getNumFinalFaces(ss) * 4,
                   ccgSubSurf_getNumFinalFaces(ss));
  CustomData_free_layer_named(&ccgdm->dm.vertData, "position", ccgSubSurf_getNumFinalVerts(ss));
  CustomData_free_layer_named(&ccgdm->dm.edgeData, ".edge_verts", ccgSubSurf_getNumFinalEdges(ss));
  CustomData_free_layer_named(
      &ccgdm->dm.loopData, ".corner_vert", ccgSubSurf_getNumFinalFaces(ss) * 4);
  CustomData_free_layer_named(
      &ccgdm->dm.loopData, ".corner_edge", ccgSubSurf_getNumFinalFaces(ss) * 4);
  MEM_SAFE_FREE(ccgdm->dm.face_offsets);

  ccgdm->reverseFaceMap = static_cast<int *>(
      MEM_callocN(sizeof(int) * ccgSubSurf_getNumFinalFaces(ss), "reverseFaceMap"));

  create_ccgdm_maps(ccgdm, ss);

  set_default_ccgdm_callbacks(ccgdm);

  ccgdm->ss = ss;
  ccgdm->drawInteriorEdges = drawInteriorEdges;
  ccgdm->useSubsurfUv = useSubsurfUv;

  /* CDDM hack. */
  ccgdm->faceFlags = static_cast<DMFlagMat *>(
      MEM_callocN(sizeof(DMFlagMat) * totface, "faceFlags"));

  set_ccgdm_all_geometry(ccgdm, ss, dm, useSubsurfUv != 0);

  ccgdm->dm.numVertData = ccgSubSurf_getNumFinalVerts(ss);
  ccgdm->dm.numEdgeData = ccgSubSurf_getNumFinalEdges(ss);
  ccgdm->dm.numPolyData = ccgSubSurf_getNumFinalFaces(ss);
  ccgdm->dm.numLoopData = ccgdm->dm.numPolyData * 4;
  ccgdm->dm.numTessFaceData = 0;

  BLI_mutex_init(&ccgdm->loops_cache_lock);
  BLI_rw_mutex_init(&ccgdm->origindex_cache_rwlock);

  return ccgdm;
}

/***/

DerivedMesh *subsurf_make_derived_from_derived(DerivedMesh *dm,
                                               SubsurfModifierData *smd,
                                               const Scene *scene,
                                               float (*vertCos)[3],
                                               SubsurfFlags flags)
{
  const CCGFlags useSimple = (smd->subdivType == ME_SIMPLE_SUBSURF) ? CCG_SIMPLE_SUBDIV :
                                                                      CCGFlags(0);
  const CCGFlags useAging = (smd->flags & eSubsurfModifierFlag_DebugIncr) ? CCG_USE_AGING :
                                                                            CCGFlags(0);
  const int useSubsurfUv = (smd->uv_smooth != SUBSURF_UV_SMOOTH_NONE);
  const int drawInteriorEdges = !(smd->flags & eSubsurfModifierFlag_ControlEdges);
  const bool ignore_simplify = (flags & SUBSURF_IGNORE_SIMPLIFY);
  CCGDerivedMesh *result;

  /* NOTE: editmode calculation can only run once per
   * modifier stack evaluation (uses freed cache) #36299. */
  if (flags & SUBSURF_FOR_EDIT_MODE) {
    int levels = (scene != nullptr && !ignore_simplify) ?
                     get_render_subsurf_level(&scene->r, smd->levels, false) :
                     smd->levels;

    /* TODO(sergey): Same as emCache below. */
    if ((flags & SUBSURF_IN_EDIT_MODE) && smd->mCache) {
      ccgSubSurf_free(static_cast<CCGSubSurf *>(smd->mCache));
      smd->mCache = nullptr;
    }

    smd->emCache = _getSubSurf(static_cast<CCGSubSurf *>(smd->emCache),
                               levels,
                               3,
                               useSimple | useAging | CCG_CALC_NORMALS);

    ss_sync_from_derivedmesh(
        static_cast<CCGSubSurf *>(smd->emCache), dm, vertCos, useSimple, useSubsurfUv);
    result = getCCGDerivedMesh(
        static_cast<CCGSubSurf *>(smd->emCache), drawInteriorEdges, useSubsurfUv, dm);
  }
  else if (flags & SUBSURF_USE_RENDER_PARAMS) {
    /* Do not use cache in render mode. */
    CCGSubSurf *ss;
    int levels = (scene != nullptr && !ignore_simplify) ?
                     get_render_subsurf_level(&scene->r, smd->renderLevels, true) :
                     smd->renderLevels;

    if (levels == 0) {
      return dm;
    }

    ss = _getSubSurf(nullptr, levels, 3, useSimple | CCG_USE_ARENA | CCG_CALC_NORMALS);

    ss_sync_from_derivedmesh(ss, dm, vertCos, useSimple, useSubsurfUv);

    result = getCCGDerivedMesh(ss, drawInteriorEdges, useSubsurfUv, dm);

    result->freeSS = 1;
  }
  else {
    int useIncremental = (smd->flags & eSubsurfModifierFlag_Incremental);
    int levels = (scene != nullptr && !ignore_simplify) ?
                     get_render_subsurf_level(&scene->r, smd->levels, false) :
                     smd->levels;
    CCGSubSurf *ss;

    /* It is quite possible there is a much better place to do this. It
     * depends a bit on how rigorously we expect this function to never
     * be called in editmode. In semi-theory we could share a single
     * cache, but the handles used inside and outside editmode are not
     * the same so we would need some way of converting them. Its probably
     * not worth the effort. But then why am I even writing this long
     * comment that no one will read? Hmmm. - zr
     *
     * Addendum: we can't really ensure that this is never called in edit
     * mode, so now we have a parameter to verify it. - brecht
     */
    if (!(flags & SUBSURF_IN_EDIT_MODE) && smd->emCache) {
      ccgSubSurf_free(static_cast<CCGSubSurf *>(smd->emCache));
      smd->emCache = nullptr;
    }

    if (useIncremental && (flags & SUBSURF_IS_FINAL_CALC)) {
      smd->mCache = ss = _getSubSurf(static_cast<CCGSubSurf *>(smd->mCache),
                                     levels,
                                     3,
                                     useSimple | useAging | CCG_CALC_NORMALS);

      ss_sync_from_derivedmesh(ss, dm, vertCos, useSimple, useSubsurfUv);

      result = getCCGDerivedMesh(
          static_cast<CCGSubSurf *>(smd->mCache), drawInteriorEdges, useSubsurfUv, dm);
    }
    else {
      CCGFlags ccg_flags = useSimple | CCG_USE_ARENA | CCG_CALC_NORMALS;
      CCGSubSurf *prevSS = nullptr;

      if ((smd->mCache) && (flags & SUBSURF_IS_FINAL_CALC)) {
        ccgSubSurf_free(static_cast<CCGSubSurf *>(smd->mCache));
        smd->mCache = nullptr;
      }

      if (flags & SUBSURF_ALLOC_PAINT_MASK) {
        ccg_flags |= CCG_ALLOC_MASK;
      }

      ss = _getSubSurf(prevSS, levels, 3, ccg_flags);
      ss_sync_from_derivedmesh(ss, dm, vertCos, useSimple, useSubsurfUv);

      result = getCCGDerivedMesh(ss, drawInteriorEdges, useSubsurfUv, dm);

      if (flags & SUBSURF_IS_FINAL_CALC) {
        smd->mCache = ss;
      }
      else {
        result->freeSS = 1;
      }

      if (flags & SUBSURF_ALLOC_PAINT_MASK) {
        ccgSubSurf_setNumLayers(ss, 4);
      }
    }
  }

  return (DerivedMesh *)result;
}

void subsurf_calculate_limit_positions(Mesh *me, float (*r_positions)[3])
{
  /* Finds the subsurf limit positions for the verts in a mesh
   * and puts them in an array of floats. Please note that the
   * calculated vert positions is incorrect for the verts
   * on the boundary of the mesh.
   */
  CCGSubSurf *ss = _getSubSurf(nullptr, 1, 3, CCG_USE_ARENA);
  float edge_sum[3], face_sum[3];
  CCGVertIterator vi;
  DerivedMesh *dm = CDDM_from_mesh(me);

  ss_sync_from_derivedmesh(ss, dm, nullptr, 0, false);

  for (ccgSubSurf_initVertIterator(ss, &vi); !ccgVertIterator_isStopped(&vi);
       ccgVertIterator_next(&vi))
  {
    CCGVert *v = ccgVertIterator_getCurrent(&vi);
    int idx = POINTER_AS_INT(ccgSubSurf_getVertVertHandle(v));
    int N = ccgSubSurf_getVertNumEdges(v);
    int numFaces = ccgSubSurf_getVertNumFaces(v);
    int i;

    zero_v3(edge_sum);
    zero_v3(face_sum);

    for (i = 0; i < N; i++) {
      CCGEdge *e = ccgSubSurf_getVertEdge(v, i);
      add_v3_v3v3(
          edge_sum, edge_sum, static_cast<const float *>(ccgSubSurf_getEdgeData(ss, e, 1)));
    }
    for (i = 0; i < numFaces; i++) {
      CCGFace *f = ccgSubSurf_getVertFace(v, i);
      add_v3_v3(face_sum, static_cast<const float *>(ccgSubSurf_getFaceCenterData(f)));
    }

    /* ad-hoc correction for boundary vertices, to at least avoid them
     * moving completely out of place (brecht) */
    if (numFaces && numFaces != N) {
      mul_v3_fl(face_sum, float(N) / float(numFaces));
    }

    const float *co = static_cast<const float *>(ccgSubSurf_getVertData(ss, v));
    r_positions[idx][0] = (co[0] * N * N + edge_sum[0] * 4 + face_sum[0]) / (N * (N + 5));
    r_positions[idx][1] = (co[1] * N * N + edge_sum[1] * 4 + face_sum[1]) / (N * (N + 5));
    r_positions[idx][2] = (co[2] * N * N + edge_sum[2] * 4 + face_sum[2]) / (N * (N + 5));
  }

  ccgSubSurf_free(ss);

  dm->release(dm);
}
