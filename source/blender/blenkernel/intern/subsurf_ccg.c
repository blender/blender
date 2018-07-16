/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/subsurf_ccg.c
 *  \ingroup bke
 */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#  ifdef __GNUC__
#    pragma GCC diagnostic ignored "-Wvla"
#  endif
#  define USE_DYNSIZE
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "atomic_ops.h"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_edgehash.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_task.h"
#include "BLI_threads.h"

#include "BKE_pbvh.h"
#include "BKE_ccg.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
#include "BKE_scene.h"
#include "BKE_subsurf.h"

#ifndef USE_DYNSIZE
#  include "BLI_array.h"
#endif

#include "GPU_draw.h"
#include "GPU_glew.h"
#include "GPU_buffers.h"
#include "GPU_shader.h"
#include "GPU_basic_shader.h"

#include "CCGSubSurf.h"

#ifdef WITH_OPENSUBDIV
#  include "opensubdiv_capi.h"
#endif

/* assumes MLoop's are layed out 4 for each poly, in order */
#define USE_LOOP_LAYOUT_FAST

static CCGDerivedMesh *getCCGDerivedMesh(CCGSubSurf *ss,
                                         int drawInteriorEdges,
                                         int useSubsurfUv,
                                         DerivedMesh *dm,
                                         bool use_gpu_backend);
static int ccgDM_use_grid_pbvh(CCGDerivedMesh *ccgdm);

///

static void *arena_alloc(CCGAllocatorHDL a, int numBytes)
{
	return BLI_memarena_alloc(a, numBytes);
}

static void *arena_realloc(CCGAllocatorHDL a, void *ptr, int newSize, int oldSize)
{
	void *p2 = BLI_memarena_alloc(a, newSize);
	if (ptr) {
		memcpy(p2, ptr, oldSize);
	}
	return p2;
}

static void arena_free(CCGAllocatorHDL UNUSED(a), void *UNUSED(ptr))
{
	/* do nothing */
}

static void arena_release(CCGAllocatorHDL a)
{
	BLI_memarena_free(a);
}

typedef enum {
	CCG_USE_AGING = 1,
	CCG_USE_ARENA = 2,
	CCG_CALC_NORMALS = 4,
	/* add an extra four bytes for a mask layer */
	CCG_ALLOC_MASK = 8,
	CCG_SIMPLE_SUBDIV = 16
} CCGFlags;

static CCGSubSurf *_getSubSurf(CCGSubSurf *prevSS, int subdivLevels,
                               int numLayers, CCGFlags flags)
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

		ccgSubSurf_getUseAgeCounts(prevSS, &oldUseAging, NULL, NULL, NULL);

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
	if (flags & CCG_CALC_NORMALS)
		ifc.vertDataSize += sizeof(float) * 3;
	if (flags & CCG_ALLOC_MASK)
		ifc.vertDataSize += sizeof(float);
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
		ccgSS = ccgSubSurf_new(&ifc, subdivLevels, NULL, NULL);
	}

	if (useAging) {
		ccgSubSurf_setUseAgeCounts(ccgSS, 1, 8, 8, 8);
	}

	if (flags & CCG_ALLOC_MASK) {
		normalOffset += sizeof(float);
		/* mask is allocated after regular layers */
		ccgSubSurf_setAllocMask(ccgSS, 1, sizeof(float) * numLayers);
	}

	if (flags & CCG_CALC_NORMALS)
		ccgSubSurf_setCalcVertexNormals(ccgSS, 1, normalOffset);
	else
		ccgSubSurf_setCalcVertexNormals(ccgSS, 0, 0);

	return ccgSS;
}

static int getEdgeIndex(CCGSubSurf *ss, CCGEdge *e, int x, int edgeSize)
{
	CCGVert *v0 = ccgSubSurf_getEdgeVert0(e);
	CCGVert *v1 = ccgSubSurf_getEdgeVert1(e);
	int v0idx = *((int *) ccgSubSurf_getVertUserData(ss, v0));
	int v1idx = *((int *) ccgSubSurf_getVertUserData(ss, v1));
	int edgeBase = *((int *) ccgSubSurf_getEdgeUserData(ss, e));

	if (x == 0) {
		return v0idx;
	}
	else if (x == edgeSize - 1) {
		return v1idx;
	}
	else {
		return edgeBase + x - 1;
	}
}

static int getFaceIndex(CCGSubSurf *ss, CCGFace *f, int S, int x, int y, int edgeSize, int gridSize)
{
	int faceBase = *((int *) ccgSubSurf_getFaceUserData(ss, f));
	int numVerts = ccgSubSurf_getFaceNumVerts(f);

	if (x == gridSize - 1 && y == gridSize - 1) {
		CCGVert *v = ccgSubSurf_getFaceVert(f, S);
		return *((int *) ccgSubSurf_getVertUserData(ss, v));
	}
	else if (x == gridSize - 1) {
		CCGVert *v = ccgSubSurf_getFaceVert(f, S);
		CCGEdge *e = ccgSubSurf_getFaceEdge(f, S);
		int edgeBase = *((int *) ccgSubSurf_getEdgeUserData(ss, e));
		if (v == ccgSubSurf_getEdgeVert0(e)) {
			return edgeBase + (gridSize - 1 - y) - 1;
		}
		else {
			return edgeBase + (edgeSize - 2 - 1) - ((gridSize - 1 - y) - 1);
		}
	}
	else if (y == gridSize - 1) {
		CCGVert *v = ccgSubSurf_getFaceVert(f, S);
		CCGEdge *e = ccgSubSurf_getFaceEdge(f, (S + numVerts - 1) % numVerts);
		int edgeBase = *((int *) ccgSubSurf_getEdgeUserData(ss, e));
		if (v == ccgSubSurf_getEdgeVert0(e)) {
			return edgeBase + (gridSize - 1 - x) - 1;
		}
		else {
			return edgeBase + (edgeSize - 2 - 1) - ((gridSize - 1 - x) - 1);
		}
	}
	else if (x == 0 && y == 0) {
		return faceBase;
	}
	else if (x == 0) {
		S = (S + numVerts - 1) % numVerts;
		return faceBase + 1 + (gridSize - 2) * S + (y - 1);
	}
	else if (y == 0) {
		return faceBase + 1 + (gridSize - 2) * S + (x - 1);
	}
	else {
		return faceBase + 1 + (gridSize - 2) * numVerts + S * (gridSize - 2) * (gridSize - 2) + (y - 1) * (gridSize - 2) + (x - 1);
	}
}

static void get_face_uv_map_vert(UvVertMap *vmap, struct MPoly *mpoly, struct MLoop *ml, int fi, CCGVertHDL *fverts)
{
	UvMapVert *v, *nv;
	int j, nverts = mpoly[fi].totloop;

	for (j = 0; j < nverts; j++) {
		for (nv = v = BKE_mesh_uv_vert_map_get_vert(vmap, ml[j].v); v; v = v->next) {
			if (v->separate)
				nv = v;
			if (v->poly_index == fi)
				break;
		}

		fverts[j] = SET_UINT_IN_POINTER(mpoly[nv->poly_index].loopstart + nv->loop_of_poly_index);
	}
}

static int ss_sync_from_uv(CCGSubSurf *ss, CCGSubSurf *origss, DerivedMesh *dm, MLoopUV *mloopuv)
{
	MPoly *mpoly = dm->getPolyArray(dm);
	MLoop *mloop = dm->getLoopArray(dm);
	int totvert = dm->getNumVerts(dm);
	int totface = dm->getNumPolys(dm);
	int i, seam;
	UvMapVert *v;
	UvVertMap *vmap;
	float limit[2];
#ifndef USE_DYNSIZE
	CCGVertHDL *fverts = NULL;
	BLI_array_declare(fverts);
#endif
	EdgeSet *eset;
	float uv[3] = {0.0f, 0.0f, 0.0f}; /* only first 2 values are written into */

	limit[0] = limit[1] = STD_UV_CONNECT_LIMIT;
	/* previous behavior here is without accounting for winding, however this causes stretching in
	 * UV map in really simple cases with mirror + subsurf, see second part of T44530. Also, initially
	 * intention is to treat merged vertices from mirror modifier as seams.
	 * This fixes a very old regression (2.49 was correct here) */
	vmap = BKE_mesh_uv_vert_map_create(mpoly, mloop, mloopuv, totface, totvert, limit, false, true);
	if (!vmap)
		return 0;

	ccgSubSurf_initFullSync(ss);

	/* create vertices */
	for (i = 0; i < totvert; i++) {
		if (!BKE_mesh_uv_vert_map_get_vert(vmap, i))
			continue;

		for (v = BKE_mesh_uv_vert_map_get_vert(vmap, i)->next; v; v = v->next)
			if (v->separate)
				break;

		seam = (v != NULL);

		for (v = BKE_mesh_uv_vert_map_get_vert(vmap, i); v; v = v->next) {
			if (v->separate) {
				CCGVert *ssv;
				int loopid = mpoly[v->poly_index].loopstart + v->loop_of_poly_index;
				CCGVertHDL vhdl = SET_INT_IN_POINTER(loopid);

				copy_v2_v2(uv, mloopuv[loopid].uv);

				ccgSubSurf_syncVert(ss, vhdl, uv, seam, &ssv);
			}
		}
	}

	/* create edges */
	eset = BLI_edgeset_new_ex(__func__, BLI_EDGEHASH_SIZE_GUESS_FROM_POLYS(totface));

	for (i = 0; i < totface; i++) {
		MPoly *mp = &mpoly[i];
		int nverts = mp->totloop;
		int j, j_next;
		CCGFace *origf = ccgSubSurf_getFace(origss, SET_INT_IN_POINTER(i));
		/* unsigned int *fv = &mp->v1; */
		MLoop *ml = mloop + mp->loopstart;

#ifdef USE_DYNSIZE
		CCGVertHDL fverts[nverts];
#else
		BLI_array_clear(fverts);
		BLI_array_grow_items(fverts, nverts);
#endif

		get_face_uv_map_vert(vmap, mpoly, ml, i, fverts);

		for (j = 0, j_next = nverts - 1; j < nverts; j_next = j++) {
			unsigned int v0 = GET_UINT_FROM_POINTER(fverts[j_next]);
			unsigned int v1 = GET_UINT_FROM_POINTER(fverts[j]);

			if (BLI_edgeset_add(eset, v0, v1)) {
				CCGEdge *e, *orige = ccgSubSurf_getFaceEdge(origf, j_next);
				CCGEdgeHDL ehdl = SET_INT_IN_POINTER(mp->loopstart + j_next);
				float crease = ccgSubSurf_getEdgeCrease(orige);

				ccgSubSurf_syncEdge(ss, ehdl, fverts[j_next], fverts[j], crease, &e);
			}
		}
	}

	BLI_edgeset_free(eset);

	/* create faces */
	for (i = 0; i < totface; i++) {
		MPoly *mp = &mpoly[i];
		MLoop *ml = &mloop[mp->loopstart];
		int nverts = mp->totloop;
		CCGFace *f;

#ifdef USE_DYNSIZE
		CCGVertHDL fverts[nverts];
#else
		BLI_array_clear(fverts);
		BLI_array_grow_items(fverts, nverts);
#endif

		get_face_uv_map_vert(vmap, mpoly, ml, i, fverts);
		ccgSubSurf_syncFace(ss, SET_INT_IN_POINTER(i), nverts, fverts, &f);
	}

#ifndef USE_DYNSIZE
	BLI_array_free(fverts);
#endif

	BKE_mesh_uv_vert_map_free(vmap);
	ccgSubSurf_processSync(ss);

	return 1;
}

#ifdef WITH_OPENSUBDIV
static void UNUSED_FUNCTION(set_subsurf_osd_ccg_uv)(CCGSubSurf *ss,
                                                    DerivedMesh *dm,
                                                    DerivedMesh *result,
                                                    int layer_index)
{
	CCGFace **faceMap;
	MTFace *tf;
	MLoopUV *mluv;
	CCGFaceIterator fi;
	int index, gridSize, gridFaces, totface, x, y, S;
	MLoopUV *dmloopuv = CustomData_get_layer_n(&dm->loopData, CD_MLOOPUV, layer_index);
	/* need to update both CD_MTFACE & CD_MLOOPUV, hrmf, we could get away with
	 * just tface except applying the modifier then looses subsurf UV */
	MTFace *tface = CustomData_get_layer_n(&result->faceData, CD_MTFACE, layer_index);
	MLoopUV *mloopuv = CustomData_get_layer_n(&result->loopData, CD_MLOOPUV, layer_index);

	if (dmloopuv == NULL || (tface == NULL && mloopuv == NULL)) {
		return;
	}

	ccgSubSurf_evaluatorSetFVarUV(ss, dm, layer_index);

	/* get some info from CCGSubSurf */
	totface = ccgSubSurf_getNumFaces(ss);
	gridSize = ccgSubSurf_getGridSize(ss);
	gridFaces = gridSize - 1;

	/* make a map from original faces to CCGFaces */
	faceMap = MEM_mallocN(totface * sizeof(*faceMap), "facemapuv");
	for (ccgSubSurf_initFaceIterator(ss, &fi); !ccgFaceIterator_isStopped(&fi); ccgFaceIterator_next(&fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(&fi);
		faceMap[GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(f))] = f;
	}

	/* load coordinates from uvss into tface */
	tf = tface;
	mluv = mloopuv;
	for (index = 0; index < totface; index++) {
		CCGFace *f = faceMap[index];
		int numVerts = ccgSubSurf_getFaceNumVerts(f);
		for (S = 0; S < numVerts; S++) {
			for (y = 0; y < gridFaces; y++) {
				for (x = 0; x < gridFaces; x++) {
					const int delta[4][2] = {{0, 0},
					                         {0, 1},
					                         {1, 1},
					                         {1, 0}};
					float uv[4][2];
					int i;
					for (i = 0; i < 4; i++) {
						const int dx = delta[i][0],
						          dy = delta[i][1];
						const float grid_u = ((float)(x + dx)) / (gridSize - 1),
						            grid_v = ((float)(y + dy)) / (gridSize - 1);
						ccgSubSurf_evaluatorFVarUV(ss,
						                           index,
						                           S,
						                           grid_u, grid_v,
						                           uv[i]);
					}
					if (tf) {
						copy_v2_v2(tf->uv[0], uv[0]);
						copy_v2_v2(tf->uv[1], uv[1]);
						copy_v2_v2(tf->uv[2], uv[2]);
						copy_v2_v2(tf->uv[3], uv[3]);
						tf++;
					}
					if (mluv) {
						copy_v2_v2(mluv[0].uv, uv[0]);
						copy_v2_v2(mluv[1].uv, uv[1]);
						copy_v2_v2(mluv[2].uv, uv[2]);
						copy_v2_v2(mluv[3].uv, uv[3]);
						mluv += 4;
					}
				}
			}
		}
	}
	MEM_freeN(faceMap);
}
#endif  /* WITH_OPENSUBDIV */

static void set_subsurf_legacy_uv(CCGSubSurf *ss, DerivedMesh *dm, DerivedMesh *result, int n)
{
	CCGSubSurf *uvss;
	CCGFace **faceMap;
	MTFace *tf;
	MLoopUV *mluv;
	CCGFaceIterator fi;
	int index, gridSize, gridFaces, /*edgeSize,*/ totface, x, y, S;
	MLoopUV *dmloopuv = CustomData_get_layer_n(&dm->loopData, CD_MLOOPUV, n);
	/* need to update both CD_MTFACE & CD_MLOOPUV, hrmf, we could get away with
	 * just tface except applying the modifier then looses subsurf UV */
	MTFace *tface = CustomData_get_layer_n(&result->faceData, CD_MTFACE, n);
	MLoopUV *mloopuv = CustomData_get_layer_n(&result->loopData, CD_MLOOPUV, n);

	if (!dmloopuv || (!tface && !mloopuv))
		return;

	/* create a CCGSubSurf from uv's */
	uvss = _getSubSurf(NULL, ccgSubSurf_getSubdivisionLevels(ss), 2, CCG_USE_ARENA);

	if (!ss_sync_from_uv(uvss, ss, dm, dmloopuv)) {
		ccgSubSurf_free(uvss);
		return;
	}

	/* get some info from CCGSubSurf */
	totface = ccgSubSurf_getNumFaces(uvss);
	/* edgeSize = ccgSubSurf_getEdgeSize(uvss); */ /*UNUSED*/
	gridSize = ccgSubSurf_getGridSize(uvss);
	gridFaces = gridSize - 1;

	/* make a map from original faces to CCGFaces */
	faceMap = MEM_mallocN(totface * sizeof(*faceMap), "facemapuv");
	for (ccgSubSurf_initFaceIterator(uvss, &fi); !ccgFaceIterator_isStopped(&fi); ccgFaceIterator_next(&fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(&fi);
		faceMap[GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(f))] = f;
	}

	/* load coordinates from uvss into tface */
	tf = tface;
	mluv = mloopuv;

	for (index = 0; index < totface; index++) {
		CCGFace *f = faceMap[index];
		int numVerts = ccgSubSurf_getFaceNumVerts(f);

		for (S = 0; S < numVerts; S++) {
			float (*faceGridData)[2] = ccgSubSurf_getFaceGridDataArray(uvss, f, S);

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
						copy_v2_v2(mluv[0].uv, a);
						copy_v2_v2(mluv[1].uv, d);
						copy_v2_v2(mluv[2].uv, c);
						copy_v2_v2(mluv[3].uv, b);
						mluv += 4;
					}

				}
			}
		}
	}

	ccgSubSurf_free(uvss);
	MEM_freeN(faceMap);
}

static void set_subsurf_uv(CCGSubSurf *ss,
                           DerivedMesh *dm,
                           DerivedMesh *result,
                           int layer_index)
{
#ifdef WITH_OPENSUBDIV
	if (!ccgSubSurf_needGrids(ss)) {
		/* GPU backend is used, no need to evaluate UVs on CPU. */
		/* TODO(sergey): Think of how to support edit mode of UVs. */
	}
	else
#endif
	{
		set_subsurf_legacy_uv(ss, dm, result, layer_index);
	}
}

/* face weighting */
typedef struct FaceVertWeightEntry {
	FaceVertWeight *weight;
	float *w;
	int valid;
} FaceVertWeightEntry;

typedef struct WeightTable {
	FaceVertWeightEntry *weight_table;
	int len;
} WeightTable;

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

		wtable->weight_table = tmp;
		wtable->len = faceLen + 1;
	}

	if (!wtable->weight_table[faceLen].valid) {
		wtable->weight_table[faceLen].valid = 1;
		wtable->weight_table[faceLen].w = w = MEM_callocN(sizeof(float) * faceLen * faceLen * (gridCuts + 2) * (gridCuts + 2), "weight table alloc");
		fac = 1.0f / (float)faceLen;

		for (i = 0; i < faceLen; i++) {
			for (x = 0; x < gridCuts + 2; x++) {
				for (y = 0; y < gridCuts + 2; y++) {
					fx = 0.5f - (float)x / (float)(gridCuts + 1) / 2.0f;
					fy = 0.5f - (float)y / (float)(gridCuts + 1) / 2.0f;

					fac2 = faceLen - 4;
					w1 = (1.0f - fx) * (1.0f - fy) + (-fac2 * fx * fy * fac);
					w2 = (1.0f - fx + fac2 * fx * -fac) * (fy);
					w4 = (fx) * (1.0f - fy + -fac2 * fy * fac);

					/* these values aren't used for tri's and cause divide by zero */
					if (faceLen > 3) {
						fac2 = 1.0f - (w1 + w2 + w4);
						fac2 = fac2 / (float)(faceLen - 3);
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
		if (wtable->weight_table[i].valid)
			MEM_freeN(wtable->weight_table[i].w);
	}

	if (wtable->weight_table)
		MEM_freeN(wtable->weight_table);
}

static void ss_sync_ccg_from_derivedmesh(CCGSubSurf *ss,
                                         DerivedMesh *dm,
                                         float (*vertexCos)[3],
                                         int useFlatSubdiv)
{
	float creaseFactor = (float) ccgSubSurf_getSubdivisionLevels(ss);
#ifndef USE_DYNSIZE
	CCGVertHDL *fVerts = NULL;
	BLI_array_declare(fVerts);
#endif
	MVert *mvert = dm->getVertArray(dm);
	MEdge *medge = dm->getEdgeArray(dm);
	/* MFace *mface = dm->getTessFaceArray(dm); */ /* UNUSED */
	MVert *mv;
	MEdge *me;
	MLoop *mloop = dm->getLoopArray(dm), *ml;
	MPoly *mpoly = dm->getPolyArray(dm), *mp;
	/*MFace *mf;*/ /*UNUSED*/
	int totvert = dm->getNumVerts(dm);
	int totedge = dm->getNumEdges(dm);
	/*int totface = dm->getNumTessFaces(dm);*/ /*UNUSED*/
	/*int totpoly = dm->getNumFaces(dm);*/ /*UNUSED*/
	int i, j;
	int *index;

	ccgSubSurf_initFullSync(ss);

	mv = mvert;
	index = (int *)dm->getVertDataArray(dm, CD_ORIGINDEX);
	for (i = 0; i < totvert; i++, mv++) {
		CCGVert *v;

		if (vertexCos) {
			ccgSubSurf_syncVert(ss, SET_INT_IN_POINTER(i), vertexCos[i], 0, &v);
		}
		else {
			ccgSubSurf_syncVert(ss, SET_INT_IN_POINTER(i), mv->co, 0, &v);
		}

		((int *)ccgSubSurf_getVertUserData(ss, v))[1] = (index) ? *index++ : i;
	}

	me = medge;
	index = (int *)dm->getEdgeDataArray(dm, CD_ORIGINDEX);
	for (i = 0; i < totedge; i++, me++) {
		CCGEdge *e;
		float crease;

		crease = useFlatSubdiv ? creaseFactor :
		         me->crease * creaseFactor / 255.0f;

		ccgSubSurf_syncEdge(ss, SET_INT_IN_POINTER(i), SET_UINT_IN_POINTER(me->v1),
		                    SET_UINT_IN_POINTER(me->v2), crease, &e);

		((int *)ccgSubSurf_getEdgeUserData(ss, e))[1] = (index) ? *index++ : i;
	}

	mp = mpoly;
	index = (int *)dm->getPolyDataArray(dm, CD_ORIGINDEX);
	for (i = 0; i < dm->numPolyData; i++, mp++) {
		CCGFace *f;

#ifdef USE_DYNSIZE
		CCGVertHDL fVerts[mp->totloop];
#else
		BLI_array_clear(fVerts);
		BLI_array_grow_items(fVerts, mp->totloop);
#endif

		ml = mloop + mp->loopstart;
		for (j = 0; j < mp->totloop; j++, ml++) {
			fVerts[j] = SET_UINT_IN_POINTER(ml->v);
		}

		/* this is very bad, means mesh is internally inconsistent.
		 * it is not really possible to continue without modifying
		 * other parts of code significantly to handle missing faces.
		 * since this really shouldn't even be possible we just bail.*/
		if (ccgSubSurf_syncFace(ss, SET_INT_IN_POINTER(i), mp->totloop,
		                        fVerts, &f) == eCCGError_InvalidValue)
		{
			static int hasGivenError = 0;

			if (!hasGivenError) {
				//XXX error("Unrecoverable error in SubSurf calculation,"
				//      " mesh is inconsistent.");

				hasGivenError = 1;
			}

			return;
		}

		((int *)ccgSubSurf_getFaceUserData(ss, f))[1] = (index) ? *index++ : i;
	}

	ccgSubSurf_processSync(ss);

#ifndef USE_DYNSIZE
	BLI_array_free(fVerts);
#endif
}

#ifdef WITH_OPENSUBDIV
static void ss_sync_osd_from_derivedmesh(CCGSubSurf *ss,
                                         DerivedMesh *dm)
{
	ccgSubSurf_initFullSync(ss);
	ccgSubSurf_prepareTopologyRefiner(ss, dm);
	ccgSubSurf_processSync(ss);
}
#endif  /* WITH_OPENSUBDIV */

static void ss_sync_from_derivedmesh(CCGSubSurf *ss,
                                     DerivedMesh *dm,
                                     float (*vertexCos)[3],
                                     int use_flat_subdiv,
                                     bool use_subdiv_uvs)
{
#ifndef WITH_OPENSUBDIV
	UNUSED_VARS(use_subdiv_uvs);
#endif

#ifdef WITH_OPENSUBDIV
	/* Reset all related descriptors if actual mesh topology changed or if
	 * other evaluation-related settings changed.
	 */
	if (!ccgSubSurf_needGrids(ss)) {
		/* TODO(sergey): Use vertex coordinates and flat subdiv flag. */
		ccgSubSurf__sync_subdivUvs(ss, use_subdiv_uvs);
		ccgSubSurf_checkTopologyChanged(ss, dm);
		ss_sync_osd_from_derivedmesh(ss, dm);
	}
	else
#endif
	{
		ss_sync_ccg_from_derivedmesh(ss, dm, vertexCos, use_flat_subdiv);
	}
}

/***/

static int ccgDM_getVertMapIndex(CCGSubSurf *ss, CCGVert *v)
{
	return ((int *) ccgSubSurf_getVertUserData(ss, v))[1];
}

static int ccgDM_getEdgeMapIndex(CCGSubSurf *ss, CCGEdge *e)
{
	return ((int *) ccgSubSurf_getEdgeUserData(ss, e))[1];
}

static int ccgDM_getFaceMapIndex(CCGSubSurf *ss, CCGFace *f)
{
	return ((int *) ccgSubSurf_getFaceUserData(ss, f))[1];
}

static void minmax_v3_v3v3(const float vec[3], float min[3], float max[3])
{
	if (min[0] > vec[0]) min[0] = vec[0];
	if (min[1] > vec[1]) min[1] = vec[1];
	if (min[2] > vec[2]) min[2] = vec[2];
	if (max[0] < vec[0]) max[0] = vec[0];
	if (max[1] < vec[1]) max[1] = vec[1];
	if (max[2] < vec[2]) max[2] = vec[2];
}

static void ccgDM_getMinMax(DerivedMesh *dm, float r_min[3], float r_max[3])
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGVertIterator vi;
	CCGEdgeIterator ei;
	CCGFaceIterator fi;
	CCGKey key;
	int i, edgeSize = ccgSubSurf_getEdgeSize(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);

#ifdef WITH_OPENSUBDIV
	if (ccgdm->useGpuBackend) {
		ccgSubSurf_getMinMax(ccgdm->ss, r_min, r_max);
		return;
	}
#endif

	CCG_key_top_level(&key, ss);

	if (!ccgSubSurf_getNumVerts(ss))
		r_min[0] = r_min[1] = r_min[2] = r_max[0] = r_max[1] = r_max[2] = 0.0;

	for (ccgSubSurf_initVertIterator(ss, &vi); !ccgVertIterator_isStopped(&vi); ccgVertIterator_next(&vi)) {
		CCGVert *v = ccgVertIterator_getCurrent(&vi);
		float *co = ccgSubSurf_getVertData(ss, v);

		minmax_v3_v3v3(co, r_min, r_max);
	}

	for (ccgSubSurf_initEdgeIterator(ss, &ei); !ccgEdgeIterator_isStopped(&ei); ccgEdgeIterator_next(&ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(&ei);
		CCGElem *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);

		for (i = 0; i < edgeSize; i++)
			minmax_v3_v3v3(CCG_elem_offset_co(&key, edgeData, i), r_min, r_max);
	}

	for (ccgSubSurf_initFaceIterator(ss, &fi); !ccgFaceIterator_isStopped(&fi); ccgFaceIterator_next(&fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(&fi);
		int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(f);

		for (S = 0; S < numVerts; S++) {
			CCGElem *faceGridData = ccgSubSurf_getFaceGridDataArray(ss, f, S);

			for (y = 0; y < gridSize; y++)
				for (x = 0; x < gridSize; x++)
					minmax_v3_v3v3(CCG_grid_elem_co(&key, faceGridData, x, y), r_min, r_max);
		}
	}
}

static int ccgDM_getNumVerts(DerivedMesh *dm)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;

	return ccgSubSurf_getNumFinalVerts(ccgdm->ss);
}

static int ccgDM_getNumEdges(DerivedMesh *dm)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;

	return ccgSubSurf_getNumFinalEdges(ccgdm->ss);
}

static int ccgDM_getNumPolys(DerivedMesh *dm)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;

	return ccgSubSurf_getNumFinalFaces(ccgdm->ss);
}

static int ccgDM_getNumTessFaces(DerivedMesh *dm)
{
	return dm->numTessFaceData;
}

static int ccgDM_getNumLoops(DerivedMesh *dm)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;

	/* All subsurf faces are quads */
	return 4 * ccgSubSurf_getNumFinalFaces(ccgdm->ss);
}

static void ccgDM_getFinalVert(DerivedMesh *dm, int vertNum, MVert *mv)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGElem *vd;
	CCGKey key;
	int i;

	CCG_key_top_level(&key, ss);
	memset(mv, 0, sizeof(*mv));

	if ((vertNum < ccgdm->edgeMap[0].startVert) && (ccgSubSurf_getNumFaces(ss) > 0)) {
		/* this vert comes from face data */
		int lastface = ccgSubSurf_getNumFaces(ss) - 1;
		CCGFace *f;
		int x, y, grid, numVerts;
		int offset;
		int gridSize = ccgSubSurf_getGridSize(ss);
		int gridSideVerts;
		int gridInternalVerts;
		int gridSideEnd;
		int gridInternalEnd;

		i = 0;
		while (i < lastface && vertNum >= ccgdm->faceMap[i + 1].startVert) {
			i++;
		}

		f = ccgdm->faceMap[i].face;
		numVerts = ccgSubSurf_getFaceNumVerts(f);

		gridSideVerts = gridSize - 2;
		gridInternalVerts = gridSideVerts * gridSideVerts;

		gridSideEnd = 1 + numVerts * gridSideVerts;
		gridInternalEnd = gridSideEnd + numVerts * gridInternalVerts;

		offset = vertNum - ccgdm->faceMap[i].startVert;
		if (offset < 1) {
			vd = ccgSubSurf_getFaceCenterData(f);
			copy_v3_v3(mv->co, CCG_elem_co(&key, vd));
			normal_float_to_short_v3(mv->no, CCG_elem_no(&key, vd));
		}
		else if (offset < gridSideEnd) {
			offset -= 1;
			grid = offset / gridSideVerts;
			x = offset % gridSideVerts + 1;
			vd = ccgSubSurf_getFaceGridEdgeData(ss, f, grid, x);
			copy_v3_v3(mv->co, CCG_elem_co(&key, vd));
			normal_float_to_short_v3(mv->no, CCG_elem_no(&key, vd));
		}
		else if (offset < gridInternalEnd) {
			offset -= gridSideEnd;
			grid = offset / gridInternalVerts;
			offset %= gridInternalVerts;
			y = offset / gridSideVerts + 1;
			x = offset % gridSideVerts + 1;
			vd = ccgSubSurf_getFaceGridData(ss, f, grid, x, y);
			copy_v3_v3(mv->co, CCG_elem_co(&key, vd));
			normal_float_to_short_v3(mv->no, CCG_elem_no(&key, vd));
		}
	}
	else if ((vertNum < ccgdm->vertMap[0].startVert) && (ccgSubSurf_getNumEdges(ss) > 0)) {
		/* this vert comes from edge data */
		CCGEdge *e;
		int lastedge = ccgSubSurf_getNumEdges(ss) - 1;
		int x;

		i = 0;
		while (i < lastedge && vertNum >= ccgdm->edgeMap[i + 1].startVert) {
			i++;
		}

		e = ccgdm->edgeMap[i].edge;

		x = vertNum - ccgdm->edgeMap[i].startVert + 1;
		vd = ccgSubSurf_getEdgeData(ss, e, x);
		copy_v3_v3(mv->co, CCG_elem_co(&key, vd));
		normal_float_to_short_v3(mv->no, CCG_elem_no(&key, vd));
	}
	else {
		/* this vert comes from vert data */
		CCGVert *v;
		i = vertNum - ccgdm->vertMap[0].startVert;

		v = ccgdm->vertMap[i].vert;
		vd = ccgSubSurf_getVertData(ss, v);
		copy_v3_v3(mv->co, CCG_elem_co(&key, vd));
		normal_float_to_short_v3(mv->no, CCG_elem_no(&key, vd));
	}
}

static void ccgDM_getFinalVertCo(DerivedMesh *dm, int vertNum, float r_co[3])
{
	MVert mvert;

	ccgDM_getFinalVert(dm, vertNum, &mvert);
	copy_v3_v3(r_co, mvert.co);
}

static void ccgDM_getFinalVertNo(DerivedMesh *dm, int vertNum, float r_no[3])
{
	MVert mvert;

	ccgDM_getFinalVert(dm, vertNum, &mvert);
	normal_short_to_float_v3(r_no, mvert.no);
}

static void ccgDM_getFinalEdge(DerivedMesh *dm, int edgeNum, MEdge *med)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	int i;

	memset(med, 0, sizeof(*med));

	if (edgeNum < ccgdm->edgeMap[0].startEdge) {
		/* this edge comes from face data */
		int lastface = ccgSubSurf_getNumFaces(ss) - 1;
		CCGFace *f;
		int x, y, grid /*, numVerts*/;
		int offset;
		int gridSize = ccgSubSurf_getGridSize(ss);
		int edgeSize = ccgSubSurf_getEdgeSize(ss);
		int gridSideEdges;
		int gridInternalEdges;

		/* code added in bmesh but works correctly without, commenting - campbell */
#if 0
		int lasti, previ;
		i = lastface;
		lasti = 0;
		while (1) {
			previ = i;
			if (ccgdm->faceMap[i].startEdge >= edgeNum) {
				i -= fabsf(i - lasti) / 2.0f;
			}
			else if (ccgdm->faceMap[i].startEdge < edgeNum) {
				i += fabsf(i - lasti) / 2.0f;
			}
			else {
				break;
			}

			if (i < 0) {
				i = 0;
				break;
			}

			if (i > lastface) {
				i = lastface;
				break;

			}

			if (i == lasti)
				break;

			lasti = previ;
		}

		i = i > 0 ? i - 1 : i;
#endif

		i = 0;
		while (i < lastface && edgeNum >= ccgdm->faceMap[i + 1].startEdge) {
			i++;
		}

		f = ccgdm->faceMap[i].face;
		/* numVerts = ccgSubSurf_getFaceNumVerts(f); */ /*UNUSED*/

		gridSideEdges = gridSize - 1;
		gridInternalEdges = (gridSideEdges - 1) * gridSideEdges * 2;

		offset = edgeNum - ccgdm->faceMap[i].startEdge;
		grid = offset / (gridSideEdges + gridInternalEdges);
		offset %= (gridSideEdges + gridInternalEdges);

		if (offset < gridSideEdges) {
			x = offset;
			med->v1 = getFaceIndex(ss, f, grid, x, 0, edgeSize, gridSize);
			med->v2 = getFaceIndex(ss, f, grid, x + 1, 0, edgeSize, gridSize);
		}
		else {
			offset -= gridSideEdges;
			x = (offset / 2) / gridSideEdges + 1;
			y = (offset / 2) % gridSideEdges;
			if (offset % 2 == 0) {
				med->v1 = getFaceIndex(ss, f, grid, x, y, edgeSize, gridSize);
				med->v2 = getFaceIndex(ss, f, grid, x, y + 1, edgeSize, gridSize);
			}
			else {
				med->v1 = getFaceIndex(ss, f, grid, y, x, edgeSize, gridSize);
				med->v2 = getFaceIndex(ss, f, grid, y + 1, x, edgeSize, gridSize);
			}
		}
	}
	else {
		/* this vert comes from edge data */
		CCGEdge *e;
		int edgeSize = ccgSubSurf_getEdgeSize(ss);
		int x;
		short *edgeFlag;
		unsigned int flags = 0;

		i = (edgeNum - ccgdm->edgeMap[0].startEdge) / (edgeSize - 1);

		e = ccgdm->edgeMap[i].edge;

		if (!ccgSubSurf_getEdgeNumFaces(e)) flags |= ME_LOOSEEDGE;

		x = edgeNum - ccgdm->edgeMap[i].startEdge;

		med->v1 = getEdgeIndex(ss, e, x, edgeSize);
		med->v2 = getEdgeIndex(ss, e, x + 1, edgeSize);

		edgeFlag = (ccgdm->edgeFlags) ? &ccgdm->edgeFlags[i] : NULL;
		if (edgeFlag)
			flags |= (*edgeFlag & (ME_SEAM | ME_SHARP)) | ME_EDGEDRAW | ME_EDGERENDER;
		else
			flags |= ME_EDGEDRAW | ME_EDGERENDER;

		med->flag = flags;
	}
}

static void ccgDM_getFinalFace(DerivedMesh *dm, int faceNum, MFace *mf)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	int gridSize = ccgSubSurf_getGridSize(ss);
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	int gridSideEdges = gridSize - 1;
	int gridFaces = gridSideEdges * gridSideEdges;
	int i;
	CCGFace *f;
	/*int numVerts;*/
	int offset;
	int grid;
	int x, y;
	/*int lastface = ccgSubSurf_getNumFaces(ss) - 1;*/ /*UNUSED*/
	DMFlagMat *faceFlags = ccgdm->faceFlags;

	memset(mf, 0, sizeof(*mf));
	if (faceNum >= ccgdm->dm.numTessFaceData)
		return;

	i = ccgdm->reverseFaceMap[faceNum];

	f = ccgdm->faceMap[i].face;
	/*numVerts = ccgSubSurf_getFaceNumVerts(f);*/ /*UNUSED*/

	offset = faceNum - ccgdm->faceMap[i].startFace;
	grid = offset / gridFaces;
	offset %= gridFaces;
	y = offset / gridSideEdges;
	x = offset % gridSideEdges;

	mf->v1 = getFaceIndex(ss, f, grid, x + 0, y + 0, edgeSize, gridSize);
	mf->v2 = getFaceIndex(ss, f, grid, x + 0, y + 1, edgeSize, gridSize);
	mf->v3 = getFaceIndex(ss, f, grid, x + 1, y + 1, edgeSize, gridSize);
	mf->v4 = getFaceIndex(ss, f, grid, x + 1, y + 0, edgeSize, gridSize);

	if (faceFlags) {
		mf->flag = faceFlags[i].flag;
		mf->mat_nr = faceFlags[i].mat_nr;
	}
	else {
		mf->flag = ME_SMOOTH;
	}

	mf->edcode = 0;
}

/* Translate GridHidden into the ME_HIDE flag for MVerts. Assumes
 * vertices are in the order output by ccgDM_copyFinalVertArray. */
void subsurf_copy_grid_hidden(DerivedMesh *dm, const MPoly *mpoly,
                              MVert *mvert, const MDisps *mdisps)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
	CCGSubSurf *ss = ccgdm->ss;
	int level = ccgSubSurf_getSubdivisionLevels(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	int totface = ccgSubSurf_getNumFaces(ss);
	int i, j, x, y;

	for (i = 0; i < totface; i++) {
		CCGFace *f = ccgdm->faceMap[i].face;

		for (j = 0; j < mpoly[i].totloop; j++) {
			const MDisps *md = &mdisps[mpoly[i].loopstart + j];
			int hidden_gridsize = BKE_ccg_gridsize(md->level);
			int factor = BKE_ccg_factor(level, md->level);
			BLI_bitmap *hidden = md->hidden;

			if (!hidden)
				continue;

			for (y = 0; y < gridSize; y++) {
				for (x = 0; x < gridSize; x++) {
					int vndx, offset;

					vndx = getFaceIndex(ss, f, j, x, y, edgeSize, gridSize);
					offset = (y * factor) * hidden_gridsize + (x * factor);
					if (BLI_BITMAP_TEST(hidden, offset))
						mvert[vndx].flag |= ME_HIDE;
				}
			}
		}
	}
}

/* Translate GridPaintMask into vertex paint masks. Assumes vertices
 * are in the order output by ccgDM_copyFinalVertArray. */
void subsurf_copy_grid_paint_mask(DerivedMesh *dm, const MPoly *mpoly,
                                  float *paint_mask,
                                  const GridPaintMask *grid_paint_mask)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
	CCGSubSurf *ss = ccgdm->ss;
	int level = ccgSubSurf_getSubdivisionLevels(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	int totface = ccgSubSurf_getNumFaces(ss);
	int i, j, x, y, factor, gpm_gridsize;

	for (i = 0; i < totface; i++) {
		CCGFace *f = ccgdm->faceMap[i].face;
		const MPoly *p = &mpoly[i];

		for (j = 0; j < p->totloop; j++) {
			const GridPaintMask *gpm = &grid_paint_mask[p->loopstart + j];
			if (!gpm->data)
				continue;

			factor = BKE_ccg_factor(level, gpm->level);
			gpm_gridsize = BKE_ccg_gridsize(gpm->level);

			for (y = 0; y < gridSize; y++) {
				for (x = 0; x < gridSize; x++) {
					int vndx, offset;

					vndx = getFaceIndex(ss, f, j, x, y, edgeSize, gridSize);
					offset = y * factor * gpm_gridsize + x * factor;
					paint_mask[vndx] = gpm->data[offset];
				}
			}
		}
	}
}

/* utility functon */
BLI_INLINE void ccgDM_to_MVert(MVert *mv, const CCGKey *key, CCGElem *elem)
{
	copy_v3_v3(mv->co, CCG_elem_co(key, elem));
	normal_float_to_short_v3(mv->no, CCG_elem_no(key, elem));
	mv->flag = mv->bweight = 0;
}

static void ccgDM_copyFinalVertArray(DerivedMesh *dm, MVert *mvert)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGElem *vd;
	CCGKey key;
	int index;
	int totvert, totedge, totface;
	int gridSize = ccgSubSurf_getGridSize(ss);
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	unsigned int i = 0;

	CCG_key_top_level(&key, ss);

	totface = ccgSubSurf_getNumFaces(ss);
	for (index = 0; index < totface; index++) {
		CCGFace *f = ccgdm->faceMap[index].face;
		int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(f);

		vd = ccgSubSurf_getFaceCenterData(f);
		ccgDM_to_MVert(&mvert[i++], &key, vd);

		for (S = 0; S < numVerts; S++) {
			for (x = 1; x < gridSize - 1; x++) {
				vd = ccgSubSurf_getFaceGridEdgeData(ss, f, S, x);
				ccgDM_to_MVert(&mvert[i++], &key, vd);
			}
		}

		for (S = 0; S < numVerts; S++) {
			for (y = 1; y < gridSize - 1; y++) {
				for (x = 1; x < gridSize - 1; x++) {
					vd = ccgSubSurf_getFaceGridData(ss, f, S, x, y);
					ccgDM_to_MVert(&mvert[i++], &key, vd);
				}
			}
		}
	}

	totedge = ccgSubSurf_getNumEdges(ss);
	for (index = 0; index < totedge; index++) {
		CCGEdge *e = ccgdm->edgeMap[index].edge;
		int x;

		for (x = 1; x < edgeSize - 1; x++) {
			/* This gives errors with -debug-fpe
			 * the normals don't seem to be unit length.
			 * this is most likely caused by edges with no
			 * faces which are now zerod out, see comment in:
			 * ccgSubSurf__calcVertNormals(), - campbell */
			vd = ccgSubSurf_getEdgeData(ss, e, x);
			ccgDM_to_MVert(&mvert[i++], &key, vd);
		}
	}

	totvert = ccgSubSurf_getNumVerts(ss);
	for (index = 0; index < totvert; index++) {
		CCGVert *v = ccgdm->vertMap[index].vert;

		vd = ccgSubSurf_getVertData(ss, v);
		ccgDM_to_MVert(&mvert[i++], &key, vd);
	}
}


/* utility functon */
BLI_INLINE void ccgDM_to_MEdge(MEdge *med, const int v1, const int v2, const short flag)
{
	med->v1 = v1;
	med->v2 = v2;
	med->crease = med->bweight = 0;
	med->flag = flag;
}

static void ccgDM_copyFinalEdgeArray(DerivedMesh *dm, MEdge *medge)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	int index;
	int totedge, totface;
	int gridSize = ccgSubSurf_getGridSize(ss);
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	unsigned int i = 0;
	short *edgeFlags = ccgdm->edgeFlags;
	const short ed_interior_flag = ccgdm->drawInteriorEdges ? (ME_EDGEDRAW | ME_EDGERENDER) : 0;

	totface = ccgSubSurf_getNumFaces(ss);
	for (index = 0; index < totface; index++) {
		CCGFace *f = ccgdm->faceMap[index].face;
		int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(f);

		for (S = 0; S < numVerts; S++) {
			for (x = 0; x < gridSize - 1; x++) {
				ccgDM_to_MEdge(&medge[i++],
				               getFaceIndex(ss, f, S, x,     0, edgeSize, gridSize),
				               getFaceIndex(ss, f, S, x + 1, 0, edgeSize, gridSize),
				               ed_interior_flag);
			}

			for (x = 1; x < gridSize - 1; x++) {
				for (y = 0; y < gridSize - 1; y++) {
					ccgDM_to_MEdge(&medge[i++],
					               getFaceIndex(ss, f, S, x, y,    edgeSize, gridSize),
					               getFaceIndex(ss, f, S, x, y + 1, edgeSize, gridSize),
					               ed_interior_flag);
					ccgDM_to_MEdge(&medge[i++],
					               getFaceIndex(ss, f, S, y, x,     edgeSize, gridSize),
					               getFaceIndex(ss, f, S, y + 1, x, edgeSize, gridSize),
					               ed_interior_flag);
				}
			}
		}
	}

	totedge = ccgSubSurf_getNumEdges(ss);
	for (index = 0; index < totedge; index++) {
		CCGEdge *e = ccgdm->edgeMap[index].edge;
		short ed_flag = 0;
		int x;
		int edgeIdx = GET_INT_FROM_POINTER(ccgSubSurf_getEdgeEdgeHandle(e));

		if (!ccgSubSurf_getEdgeNumFaces(e)) {
			ed_flag |= ME_LOOSEEDGE;
		}

		if (edgeFlags) {
			if (edgeIdx != -1) {
				ed_flag |= ((edgeFlags[index] & (ME_SEAM | ME_SHARP)) | ME_EDGEDRAW | ME_EDGERENDER);
			}
		}
		else {
			ed_flag |= ME_EDGEDRAW | ME_EDGERENDER;
		}

		for (x = 0; x < edgeSize - 1; x++) {
			ccgDM_to_MEdge(&medge[i++],
			               getEdgeIndex(ss, e, x, edgeSize),
			               getEdgeIndex(ss, e, x + 1, edgeSize),
			               ed_flag);
		}
	}
}

static void ccgDM_copyFinalFaceArray(DerivedMesh *dm, MFace *mface)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	int index;
	int totface;
	int gridSize = ccgSubSurf_getGridSize(ss);
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	int i = 0;
	DMFlagMat *faceFlags = ccgdm->faceFlags;

	totface = dm->getNumTessFaces(dm);
	for (index = 0; index < totface; index++) {
		CCGFace *f = ccgdm->faceMap[index].face;
		int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(f);
		/* keep types in sync with MFace, avoid many conversions */
		char flag = (faceFlags) ? faceFlags[index].flag : ME_SMOOTH;
		short mat_nr = (faceFlags) ? faceFlags[index].mat_nr : 0;

		for (S = 0; S < numVerts; S++) {
			for (y = 0; y < gridSize - 1; y++) {
				for (x = 0; x < gridSize - 1; x++) {
					MFace *mf = &mface[i];
					mf->v1 = getFaceIndex(ss, f, S, x + 0, y + 0,
					                      edgeSize, gridSize);
					mf->v2 = getFaceIndex(ss, f, S, x + 0, y + 1,
					                      edgeSize, gridSize);
					mf->v3 = getFaceIndex(ss, f, S, x + 1, y + 1,
					                      edgeSize, gridSize);
					mf->v4 = getFaceIndex(ss, f, S, x + 1, y + 0,
					                      edgeSize, gridSize);
					mf->mat_nr = mat_nr;
					mf->flag = flag;
					mf->edcode = 0;

					i++;
				}
			}
		}
	}
}

typedef struct CopyFinalLoopArrayData {
	CCGDerivedMesh *ccgdm;
	MLoop *mloop;
	int grid_size;
	int *grid_offset;
	int edge_size;
	size_t mloop_index;
} CopyFinalLoopArrayData;

static void copyFinalLoopArray_task_cb(
        void *__restrict userdata,
        const int iter,
        const ParallelRangeTLS *__restrict UNUSED(tls))
{
	CopyFinalLoopArrayData *data = userdata;
	CCGDerivedMesh *ccgdm = data->ccgdm;
	CCGSubSurf *ss = ccgdm->ss;
	const int grid_size = data->grid_size;
	const int edge_size = data->edge_size;
	CCGFace *f = ccgdm->faceMap[iter].face;
	const int num_verts = ccgSubSurf_getFaceNumVerts(f);
	const int grid_index = data->grid_offset[iter];
	const size_t loop_index = 4 * (size_t)grid_index * (grid_size - 1) * (grid_size - 1);
	MLoop *ml = &data->mloop[loop_index];
	for (int S = 0; S < num_verts; S++) {
		for (int y = 0; y < grid_size - 1; y++) {
			for (int x = 0; x < grid_size - 1; x++) {

				uint v1 = getFaceIndex(ss, f, S, x + 0, y + 0,
				                       edge_size, grid_size);
				uint v2 = getFaceIndex(ss, f, S, x + 0, y + 1,
				                       edge_size, grid_size);
				uint v3 = getFaceIndex(ss, f, S, x + 1, y + 1,
				                       edge_size, grid_size);
				uint v4 = getFaceIndex(ss, f, S, x + 1, y + 0,
				                       edge_size, grid_size);

				ml->v = v1;
				ml->e = GET_UINT_FROM_POINTER(BLI_edgehash_lookup(ccgdm->ehash, v1, v2));
				ml++;

				ml->v = v2;
				ml->e = GET_UINT_FROM_POINTER(BLI_edgehash_lookup(ccgdm->ehash, v2, v3));
				ml++;

				ml->v = v3;
				ml->e = GET_UINT_FROM_POINTER(BLI_edgehash_lookup(ccgdm->ehash, v3, v4));
				ml++;

				ml->v = v4;
				ml->e = GET_UINT_FROM_POINTER(BLI_edgehash_lookup(ccgdm->ehash, v4, v1));
				ml++;
			}
		}
	}
}

static void ccgDM_copyFinalLoopArray(DerivedMesh *dm, MLoop *mloop)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;

	if (!ccgdm->ehash) {
		BLI_mutex_lock(&ccgdm->loops_cache_lock);
		if (!ccgdm->ehash) {
			MEdge *medge;
			EdgeHash *ehash;

			ehash = BLI_edgehash_new_ex(__func__, ccgdm->dm.numEdgeData);
			medge = ccgdm->dm.getEdgeArray((DerivedMesh *)ccgdm);

			for (int i = 0; i < ccgdm->dm.numEdgeData; i++) {
				BLI_edgehash_insert(ehash, medge[i].v1, medge[i].v2, SET_INT_IN_POINTER(i));
			}

			atomic_cas_ptr((void **)&ccgdm->ehash, ccgdm->ehash, ehash);
		}
		BLI_mutex_unlock(&ccgdm->loops_cache_lock);
	}

	CopyFinalLoopArrayData data;
	data.ccgdm = ccgdm;
	data.mloop = mloop;
	data.grid_size = ccgSubSurf_getGridSize(ss);
	data.grid_offset = dm->getGridOffset(dm);
	data.edge_size = ccgSubSurf_getEdgeSize(ss);

	/* NOTE: For a dense subdivision we've got enough work for each face and
	 * hence can dedicate whole thread to single face. For less dense
	 * subdivision we handle multiple faces per thread.
	 */
	data.mloop_index = data.grid_size >= 5 ? 1 : 8;

	ParallelRangeSettings settings;
	BLI_parallel_range_settings_defaults(&settings);
	settings.min_iter_per_thread = 1;

	BLI_task_parallel_range(0, ccgSubSurf_getNumFaces(ss),
	                        &data,
	                        copyFinalLoopArray_task_cb,
	                        &settings);
}

static void ccgDM_copyFinalPolyArray(DerivedMesh *dm, MPoly *mpoly)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	int index;
	int totface;
	int gridSize = ccgSubSurf_getGridSize(ss);
	/* int edgeSize = ccgSubSurf_getEdgeSize(ss); */ /* UNUSED */
	int i = 0, k = 0;
	DMFlagMat *faceFlags = ccgdm->faceFlags;

	totface = ccgSubSurf_getNumFaces(ss);
	for (index = 0; index < totface; index++) {
		CCGFace *f = ccgdm->faceMap[index].face;
		int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(f);
		int flag = (faceFlags) ? faceFlags[index].flag : ME_SMOOTH;
		int mat_nr = (faceFlags) ? faceFlags[index].mat_nr : 0;

		for (S = 0; S < numVerts; S++) {
			for (y = 0; y < gridSize - 1; y++) {
				for (x = 0; x < gridSize - 1; x++) {
					MPoly *mp = &mpoly[i];

					mp->mat_nr = mat_nr;
					mp->flag = flag;
					mp->loopstart = k;
					mp->totloop = 4;

					k += 4;
					i++;
				}
			}
		}
	}
}

static void ccgdm_getVertCos(DerivedMesh *dm, float (*cos)[3])
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);
	int i;
	CCGVertIterator vi;
	CCGEdgeIterator ei;
	CCGFaceIterator fi;
	CCGFace **faceMap2;
	CCGEdge **edgeMap2;
	CCGVert **vertMap2;
	int index, totvert, totedge, totface;

	totvert = ccgSubSurf_getNumVerts(ss);
	vertMap2 = MEM_mallocN(totvert * sizeof(*vertMap2), "vertmap");
	for (ccgSubSurf_initVertIterator(ss, &vi); !ccgVertIterator_isStopped(&vi); ccgVertIterator_next(&vi)) {
		CCGVert *v = ccgVertIterator_getCurrent(&vi);

		vertMap2[GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(v))] = v;
	}

	totedge = ccgSubSurf_getNumEdges(ss);
	edgeMap2 = MEM_mallocN(totedge * sizeof(*edgeMap2), "edgemap");
	for (ccgSubSurf_initEdgeIterator(ss, &ei), i = 0; !ccgEdgeIterator_isStopped(&ei); i++, ccgEdgeIterator_next(&ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(&ei);

		edgeMap2[GET_INT_FROM_POINTER(ccgSubSurf_getEdgeEdgeHandle(e))] = e;
	}

	totface = ccgSubSurf_getNumFaces(ss);
	faceMap2 = MEM_mallocN(totface * sizeof(*faceMap2), "facemap");
	for (ccgSubSurf_initFaceIterator(ss, &fi); !ccgFaceIterator_isStopped(&fi); ccgFaceIterator_next(&fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(&fi);

		faceMap2[GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(f))] = f;
	}

	i = 0;
	for (index = 0; index < totface; index++) {
		CCGFace *f = faceMap2[index];
		int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(f);

		copy_v3_v3(cos[i++], ccgSubSurf_getFaceCenterData(f));

		for (S = 0; S < numVerts; S++) {
			for (x = 1; x < gridSize - 1; x++) {
				copy_v3_v3(cos[i++], ccgSubSurf_getFaceGridEdgeData(ss, f, S, x));
			}
		}

		for (S = 0; S < numVerts; S++) {
			for (y = 1; y < gridSize - 1; y++) {
				for (x = 1; x < gridSize - 1; x++) {
					copy_v3_v3(cos[i++], ccgSubSurf_getFaceGridData(ss, f, S, x, y));
				}
			}
		}
	}

	for (index = 0; index < totedge; index++) {
		CCGEdge *e = edgeMap2[index];
		int x;

		for (x = 1; x < edgeSize - 1; x++) {
			copy_v3_v3(cos[i++], ccgSubSurf_getEdgeData(ss, e, x));
		}
	}

	for (index = 0; index < totvert; index++) {
		CCGVert *v = vertMap2[index];
		copy_v3_v3(cos[i++], ccgSubSurf_getVertData(ss, v));
	}

	MEM_freeN(vertMap2);
	MEM_freeN(edgeMap2);
	MEM_freeN(faceMap2);
}

static void ccgDM_foreachMappedVert(
        DerivedMesh *dm,
        void (*func)(void *userData, int index, const float co[3], const float no_f[3], const short no_s[3]),
        void *userData,
        DMForeachFlag flag)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGVertIterator vi;
	CCGKey key;
	CCG_key_top_level(&key, ccgdm->ss);

	for (ccgSubSurf_initVertIterator(ccgdm->ss, &vi); !ccgVertIterator_isStopped(&vi); ccgVertIterator_next(&vi)) {
		CCGVert *v = ccgVertIterator_getCurrent(&vi);
		const int index = ccgDM_getVertMapIndex(ccgdm->ss, v);

		if (index != -1) {
			CCGElem *vd = ccgSubSurf_getVertData(ccgdm->ss, v);
			const float *no = (flag & DM_FOREACH_USE_NORMAL) ? CCG_elem_no(&key, vd) : NULL;
			func(userData, index, CCG_elem_co(&key, vd), no, NULL);
		}
	}
}

static void ccgDM_foreachMappedEdge(
        DerivedMesh *dm,
        void (*func)(void *userData, int index, const float v0co[3], const float v1co[3]),
        void *userData)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGEdgeIterator ei;
	CCGKey key;
	int i, edgeSize = ccgSubSurf_getEdgeSize(ss);

	CCG_key_top_level(&key, ss);

	for (ccgSubSurf_initEdgeIterator(ss, &ei); !ccgEdgeIterator_isStopped(&ei); ccgEdgeIterator_next(&ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(&ei);
		const int index = ccgDM_getEdgeMapIndex(ss, e);

		if (index != -1) {
			CCGElem *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);
			for (i = 0; i < edgeSize - 1; i++) {
				func(userData, index, CCG_elem_offset_co(&key, edgeData, i), CCG_elem_offset_co(&key, edgeData, i + 1));
			}
		}
	}
}

static void ccgDM_foreachMappedLoop(
        DerivedMesh *dm,
        void (*func)(void *userData, int vertex_index, int face_index, const float co[3], const float no[3]),
        void *userData,
        DMForeachFlag flag)
{
	/* We can't use dm->getLoopDataLayout(dm) here, we want to always access dm->loopData, EditDerivedBMesh would
	 * return loop data from bmesh itself. */
	const float (*lnors)[3] = (flag & DM_FOREACH_USE_NORMAL) ? DM_get_loop_data_layer(dm, CD_NORMAL) : NULL;

	MVert *mv = dm->getVertArray(dm);
	MLoop *ml = dm->getLoopArray(dm);
	MPoly *mp = dm->getPolyArray(dm);
	const int *v_index = dm->getVertDataArray(dm, CD_ORIGINDEX);
	const int *f_index = dm->getPolyDataArray(dm, CD_ORIGINDEX);
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

static void ccgDM_drawVerts(DerivedMesh *dm)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);
	CCGVertIterator vi;
	CCGEdgeIterator ei;
	CCGFaceIterator fi;

	glBegin(GL_POINTS);
	for (ccgSubSurf_initVertIterator(ss, &vi); !ccgVertIterator_isStopped(&vi); ccgVertIterator_next(&vi)) {
		CCGVert *v = ccgVertIterator_getCurrent(&vi);
		glVertex3fv(ccgSubSurf_getVertData(ss, v));
	}

	for (ccgSubSurf_initEdgeIterator(ss, &ei); !ccgEdgeIterator_isStopped(&ei); ccgEdgeIterator_next(&ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(&ei);
		int x;

		for (x = 1; x < edgeSize - 1; x++)
			glVertex3fv(ccgSubSurf_getEdgeData(ss, e, x));
	}

	for (ccgSubSurf_initFaceIterator(ss, &fi); !ccgFaceIterator_isStopped(&fi); ccgFaceIterator_next(&fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(&fi);
		int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(f);

		glVertex3fv(ccgSubSurf_getFaceCenterData(f));
		for (S = 0; S < numVerts; S++)
			for (x = 1; x < gridSize - 1; x++)
				glVertex3fv(ccgSubSurf_getFaceGridEdgeData(ss, f, S, x));
		for (S = 0; S < numVerts; S++)
			for (y = 1; y < gridSize - 1; y++)
				for (x = 1; x < gridSize - 1; x++)
					glVertex3fv(ccgSubSurf_getFaceGridData(ss, f, S, x, y));
	}
	glEnd();
}

static void ccgdm_pbvh_update(CCGDerivedMesh *ccgdm)
{
	if (ccgdm->pbvh && ccgDM_use_grid_pbvh(ccgdm)) {
		CCGFace **faces;
		int totface;

		BKE_pbvh_get_grid_updates(ccgdm->pbvh, 1, (void ***)&faces, &totface);
		if (totface) {
			ccgSubSurf_updateFromFaces(ccgdm->ss, 0, faces, totface);
			ccgSubSurf_updateNormals(ccgdm->ss, faces, totface);
			MEM_freeN(faces);
		}
	}
}

static void ccgDM_drawEdges(DerivedMesh *dm, bool drawLooseEdges, bool drawAllEdges)
{
	GPUDrawObject *gdo;
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;

#ifdef WITH_OPENSUBDIV
	if (ccgdm->useGpuBackend) {
		/* TODO(sergey): We currently only support all edges drawing. */
		if (ccgSubSurf_prepareGLMesh(ccgdm->ss, true, -1)) {
			ccgSubSurf_drawGLMesh(ccgdm->ss, false, -1, -1);
		}
		return;
	}
#endif

	ccgdm_pbvh_update(ccgdm);

/* old debug feature for edges, unsupported for now */
#if 0
	int useAging = 0;

	if (!(G.f & G_BACKBUFSEL)) {
		CCGSubSurf *ss = ccgdm->ss;
		ccgSubSurf_getUseAgeCounts(ss, &useAging, NULL, NULL, NULL);

		/* it needs some way to upload this to VBO now */
		if (useAging) {
			int ageCol = 255 - ccgSubSurf_getEdgeAge(ss, e) * 4;
			glColor3ub(0, ageCol > 0 ? ageCol : 0, 0);
		}
	}
#endif

	GPU_edge_setup(dm);
	gdo = dm->drawObject;
	if (gdo->edges && gdo->points) {
		if (drawAllEdges && drawLooseEdges) {
			GPU_buffer_draw_elements(gdo->edges, GL_LINES, 0, (gdo->totedge - gdo->totinterior) * 2);
		}
		else if (drawAllEdges) {
			GPU_buffer_draw_elements(gdo->edges, GL_LINES, 0, gdo->loose_edge_offset * 2);
		}
		else {
			GPU_buffer_draw_elements(gdo->edges, GL_LINES, 0, gdo->tot_edge_drawn * 2);
			GPU_buffer_draw_elements(gdo->edges, GL_LINES, gdo->loose_edge_offset * 2, gdo->tot_loose_edge_drawn * 2);
		}
	}

	if (gdo->edges && ccgdm->drawInteriorEdges) {
		GPU_buffer_draw_elements(gdo->edges, GL_LINES, gdo->interior_offset * 2, gdo->totinterior * 2);
	}
	GPU_buffers_unbind();
}

static void ccgDM_drawLooseEdges(DerivedMesh *dm)
{
	int start;
	int count;

#ifdef WITH_OPENSUBDIV
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	if (ccgdm->useGpuBackend) {
		/* TODO(sergey): Needs implementation. */
		return;
	}
#endif

	GPU_edge_setup(dm);

	start = (dm->drawObject->loose_edge_offset * 2);
	count = (dm->drawObject->interior_offset - dm->drawObject->loose_edge_offset) * 2;

	if (count) {
		GPU_buffer_draw_elements(dm->drawObject->edges, GL_LINES, start, count);
	}

	GPU_buffers_unbind();
}

static void ccgDM_NormalFast(float *a, float *b, float *c, float *d, float no[3])
{
	float a_cX = c[0] - a[0], a_cY = c[1] - a[1], a_cZ = c[2] - a[2];
	float b_dX = d[0] - b[0], b_dY = d[1] - b[1], b_dZ = d[2] - b[2];

	no[0] = b_dY * a_cZ - b_dZ * a_cY;
	no[1] = b_dZ * a_cX - b_dX * a_cZ;
	no[2] = b_dX * a_cY - b_dY * a_cX;

	normalize_v3(no);
}


static void ccgDM_glNormalFast(float *a, float *b, float *c, float *d)
{
	float a_cX = c[0] - a[0], a_cY = c[1] - a[1], a_cZ = c[2] - a[2];
	float b_dX = d[0] - b[0], b_dY = d[1] - b[1], b_dZ = d[2] - b[2];
	float no[3];

	no[0] = b_dY * a_cZ - b_dZ * a_cY;
	no[1] = b_dZ * a_cX - b_dX * a_cZ;
	no[2] = b_dX * a_cY - b_dY * a_cX;

	/* don't normalize, GL_NORMALIZE is enabled */
	glNormal3fv(no);
}

/* Only used by non-editmesh types */
static void ccgDM_buffer_copy_normal(
        DerivedMesh *dm, short *varray)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGKey key;
	const float (*lnors)[3] = dm->getLoopDataArray(dm, CD_NORMAL);
	int gridSize = ccgSubSurf_getGridSize(ss);
	int gridFaces = gridSize - 1;
	DMFlagMat *faceFlags = ccgdm->faceFlags;
	int i, totface = ccgSubSurf_getNumFaces(ss);
	int shademodel;
	int start = 0;

	/* we are in sculpt mode, disable loop normals (since they won't get updated) */
	if (ccgdm->pbvh)
		lnors = NULL;

	CCG_key_top_level(&key, ss);

	for (i = 0; i < totface; i++) {
		CCGFace *f = ccgdm->faceMap[i].face;
		int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(f);
		int index = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(f));
		const float (*ln)[3] = NULL;

		if (faceFlags) {
			shademodel = (lnors || (faceFlags[index].flag & ME_SMOOTH)) ? GL_SMOOTH : GL_FLAT;
		}
		else {
			shademodel = GL_SMOOTH;
		}

		if (lnors) {
			ln = lnors;
			lnors += gridFaces * gridFaces * numVerts * 4;
		}

		for (S = 0; S < numVerts; S++) {
			CCGElem *faceGridData = ccgSubSurf_getFaceGridDataArray(ss, f, S);

			if (ln) {
				/* Can't use quad strips here... */
				for (y = 0; y < gridFaces; y ++) {
					for (x = 0; x < gridFaces; x ++) {
						normal_float_to_short_v3(&varray[start + 0],  ln[0]);
						normal_float_to_short_v3(&varray[start + 4],  ln[3]);
						normal_float_to_short_v3(&varray[start + 8],  ln[2]);
						normal_float_to_short_v3(&varray[start + 12], ln[1]);

						start += 16;
						ln += 4;
					}
				}
			}
			else if (shademodel == GL_SMOOTH) {
				for (y = 0; y < gridFaces; y ++) {
					for (x = 0; x < gridFaces; x ++) {
						float *a = CCG_grid_elem_no(&key, faceGridData, x, y );
						float *b = CCG_grid_elem_no(&key, faceGridData, x + 1, y);
						float *c = CCG_grid_elem_no(&key, faceGridData, x + 1, y + 1);
						float *d = CCG_grid_elem_no(&key, faceGridData, x, y + 1);

						normal_float_to_short_v3(&varray[start], a);
						normal_float_to_short_v3(&varray[start + 4], b);
						normal_float_to_short_v3(&varray[start + 8], c);
						normal_float_to_short_v3(&varray[start + 12], d);

						start += 16;
					}
				}
			}
			else {
				for (y = 0; y < gridFaces; y ++) {
					for (x = 0; x < gridFaces; x ++) {
						float f_no[3];
						short f_no_s[3];

						float *a = CCG_grid_elem_co(&key, faceGridData, x, y );
						float *b = CCG_grid_elem_co(&key, faceGridData, x + 1, y );
						float *c = CCG_grid_elem_co(&key, faceGridData, x + 1, y + 1);
						float *d = CCG_grid_elem_co(&key, faceGridData, x, y + 1);

						ccgDM_NormalFast(a, b, c, d, f_no);
						normal_float_to_short_v3(f_no_s, f_no);

						copy_v3_v3_short(&varray[start], f_no_s);
						copy_v3_v3_short(&varray[start + 4], f_no_s);
						copy_v3_v3_short(&varray[start + 8], f_no_s);
						copy_v3_v3_short(&varray[start + 12], f_no_s);

						start += 16;
					}
				}
			}
		}
	}
}

typedef struct FaceCount {
	unsigned int i_visible;
	unsigned int i_hidden;
	unsigned int i_tri_visible;
	unsigned int i_tri_hidden;
} FaceCount;


/* Only used by non-editmesh types */
static void ccgDM_buffer_copy_triangles(
        DerivedMesh *dm, unsigned int *varray,
        const int *mat_orig_to_new)
{
	GPUBufferMaterial *gpumat, *gpumaterials = dm->drawObject->materials;
	const int gpu_totmat = dm->drawObject->totmaterial;
	const short dm_totmat = dm->totmat;
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGKey key;
	int gridSize = ccgSubSurf_getGridSize(ss);
	int gridFaces = gridSize - 1;
	DMFlagMat *faceFlags = ccgdm->faceFlags;
	int i, totface = ccgSubSurf_getNumFaces(ss);
	short mat_nr = -1;
	int start;
	int totloops = 0;
	FaceCount *fc = MEM_mallocN(sizeof(*fc) * gpu_totmat, "gpumaterial.facecount");

	CCG_key_top_level(&key, ss);

	for (i = 0; i < gpu_totmat; i++) {
		fc[i].i_visible = 0;
		fc[i].i_tri_visible = 0;
		fc[i].i_hidden = gpumaterials[i].totpolys - 1;
		fc[i].i_tri_hidden = gpumaterials[i].totelements - 1;
	}

	for (i = 0; i < totface; i++) {
		CCGFace *f = ccgdm->faceMap[i].face;
		int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(f);
		int index = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(f));
		bool is_hidden;
		int mati;

		if (faceFlags) {
			mat_nr = ME_MAT_NR_TEST(faceFlags[index].mat_nr, dm_totmat);
			is_hidden = (faceFlags[index].flag & ME_HIDE) != 0;
		}
		else {
			mat_nr = 0;
			is_hidden = false;
		}
		mati = mat_orig_to_new[mat_nr];
		gpumat = dm->drawObject->materials + mati;

		if (is_hidden) {
			for (S = 0; S < numVerts; S++) {
				for (y = 0; y < gridFaces; y++) {
					for (x = 0; x < gridFaces; x++) {
						start = gpumat->start + fc[mati].i_tri_hidden;

						varray[start--] = totloops;
						varray[start--] = totloops + 2;
						varray[start--] = totloops + 3;

						varray[start--] = totloops;
						varray[start--] = totloops + 1;
						varray[start--] = totloops + 2;

						fc[mati].i_tri_hidden -= 6;

						totloops += 4;
					}
				}
			}
			gpumat->polys[fc[mati].i_hidden--] = i;
		}
		else {
			for (S = 0; S < numVerts; S++) {
				for (y = 0; y < gridFaces; y++) {
					for (x = 0; x < gridFaces; x++) {
						start = gpumat->start + fc[mati].i_tri_visible;

						varray[start++] = totloops + 3;
						varray[start++] = totloops + 2;
						varray[start++] = totloops;

						varray[start++] = totloops + 2;
						varray[start++] = totloops + 1;
						varray[start++] = totloops;

						fc[mati].i_tri_visible += 6;

						totloops += 4;
					}
				}
			}
			gpumat->polys[fc[mati].i_visible++] = i;
		}
	}

	/* set the visible polygons */
	for (i = 0; i < gpu_totmat; i++) {
		gpumaterials[i].totvisiblepolys = fc[i].i_visible;
	}

	MEM_freeN(fc);
}


/* Only used by non-editmesh types */
static void ccgDM_buffer_copy_vertex(
        DerivedMesh *dm, void *varray_p)
{
	float *varray = varray_p;
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGKey key;
	int gridSize = ccgSubSurf_getGridSize(ss);
	int gridFaces = gridSize - 1;
	int i, totface = ccgSubSurf_getNumFaces(ss);
	int totedge = ccgSubSurf_getNumEdges(ss);
	int start = 0;
	int edgeSize = ccgSubSurf_getEdgeSize(ss);

	CCG_key_top_level(&key, ss);

	for (i = 0; i < totface; i++) {
		CCGFace *f = ccgdm->faceMap[i].face;
		int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(f);

		for (S = 0; S < numVerts; S++) {
			CCGElem *faceGridData = ccgSubSurf_getFaceGridDataArray(ss, f, S);
			for (y = 0; y < gridFaces; y++) {
				for (x = 0; x < gridFaces; x++) {
					float *a = CCG_grid_elem_co(&key, faceGridData, x, y);
					float *b = CCG_grid_elem_co(&key, faceGridData, x + 1, y);
					float *c = CCG_grid_elem_co(&key, faceGridData, x + 1, y + 1);
					float *d = CCG_grid_elem_co(&key, faceGridData, x, y + 1);

					copy_v3_v3(&varray[start], a);
					copy_v3_v3(&varray[start + 3], b);
					copy_v3_v3(&varray[start + 6], c);
					copy_v3_v3(&varray[start + 9], d);

					start += 12;
				}
			}
		}
	}

	/* upload loose points */
	for (i = 0; i < totedge; i++) {
		CCGEdge *e = ccgdm->edgeMap[i].edge;
		CCGElem *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);

		if (!ccgSubSurf_getEdgeNumFaces(e)) {
			int j = 0;
			for (j = 0; j < edgeSize; j++) {
				copy_v3_v3(&varray[start], CCG_elem_offset_co(&key, edgeData, j));
				start += 3;
			}
		}
	}
}

/* Only used by non-editmesh types */
static void ccgDM_buffer_copy_color(
        DerivedMesh *dm, unsigned char *varray,
        const void *user_data)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGKey key;
	const unsigned char *mloopcol = user_data;
	int gridSize = ccgSubSurf_getGridSize(ss);
	int gridFaces = gridSize - 1;
	int i, totface = ccgSubSurf_getNumFaces(ss);
	int start = 0;
	int iface = 0;

	CCG_key_top_level(&key, ss);


	for (i = 0; i < totface; i++) {
		CCGFace *f = ccgdm->faceMap[i].face;
		int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(f);

		for (S = 0; S < numVerts; S++) {
			for (y = 0; y < gridFaces; y++) {
				for (x = 0; x < gridFaces; x++) {
					copy_v4_v4_uchar(&varray[start + 0], &mloopcol[iface * 16 + 0]);
					copy_v4_v4_uchar(&varray[start + 4], &mloopcol[iface * 16 + 12]);
					copy_v4_v4_uchar(&varray[start + 8], &mloopcol[iface * 16 + 8]);
					copy_v4_v4_uchar(&varray[start + 12], &mloopcol[iface * 16 + 4]);

					start += 16;
					iface++;
				}
			}
		}
	}
}

static void ccgDM_buffer_copy_uv(
        DerivedMesh *dm, void *varray_p)
{
	float *varray = varray_p;
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGKey key;
	MLoopUV *mloopuv = DM_get_loop_data_layer(dm, CD_MLOOPUV);
	int gridSize = ccgSubSurf_getGridSize(ss);
	int gridFaces = gridSize - 1;
	int i, totface = ccgSubSurf_getNumFaces(ss);
	int start = 0;

	CCG_key_top_level(&key, ss);

	for (i = 0; i < totface; i++) {
		CCGFace *f = ccgdm->faceMap[i].face;
		int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(f);

		for (S = 0; S < numVerts; S++) {
			for (y = 0; y < gridFaces; y++) {
				for (x = 0; x < gridFaces; x++) {
					copy_v2_v2(&varray[start + 0], mloopuv[0].uv);
					copy_v2_v2(&varray[start + 2], mloopuv[3].uv);
					copy_v2_v2(&varray[start + 4], mloopuv[2].uv);
					copy_v2_v2(&varray[start + 6], mloopuv[1].uv);

					mloopuv += 4;
					start += 8;
				}
			}
		}
	}
}

static void ccgDM_buffer_copy_uv_texpaint(
        DerivedMesh *dm, float *varray)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGKey key;
	int gridSize = ccgSubSurf_getGridSize(ss);
	int gridFaces = gridSize - 1;
	int i, totface = ccgSubSurf_getNumFaces(ss);
	int start = 0;
	DMFlagMat *faceFlags = ccgdm->faceFlags;
	int dm_totmat = dm->totmat;
	MLoopUV **mloopuv_base;
	MLoopUV  *stencil_base;
	int stencil;

	CCG_key_top_level(&key, ss);

	/* should have been checked for before, reassert */
	BLI_assert(DM_get_loop_data_layer(dm, CD_MLOOPUV));
	mloopuv_base = MEM_mallocN(dm_totmat * sizeof(*mloopuv_base), "texslots");

	for (i = 0; i < dm_totmat; i++) {
		mloopuv_base[i] = DM_paint_uvlayer_active_get(dm, i);
	}

	stencil = CustomData_get_stencil_layer(&dm->loopData, CD_MLOOPUV);
	stencil_base = CustomData_get_layer_n(&dm->loopData, CD_MLOOPUV, stencil);

	start = 0;

	for (i = 0; i < totface; i++) {
		CCGFace *f = ccgdm->faceMap[i].face;
		int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(f);
		int index = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(f));
		int matnr;

		if (faceFlags) {
			matnr = faceFlags[index].mat_nr;
		}
		else {
			matnr = 0;
		}

		for (S = 0; S < numVerts; S++) {
			for (y = 0; y < gridFaces; y++) {
				for (x = 0; x < gridFaces; x++) {
					/* divide by 4, gives us current loop-index */
					unsigned int i_ml = start / 4;
					copy_v2_v2(&varray[start + 0],  mloopuv_base[matnr][i_ml + 0].uv);
					copy_v2_v2(&varray[start + 2],         stencil_base[i_ml + 0].uv);
					copy_v2_v2(&varray[start + 4],  mloopuv_base[matnr][i_ml + 3].uv);
					copy_v2_v2(&varray[start + 6],         stencil_base[i_ml + 3].uv);
					copy_v2_v2(&varray[start + 8],  mloopuv_base[matnr][i_ml + 2].uv);
					copy_v2_v2(&varray[start + 10],        stencil_base[i_ml + 2].uv);
					copy_v2_v2(&varray[start + 12], mloopuv_base[matnr][i_ml + 1].uv);
					copy_v2_v2(&varray[start + 14],        stencil_base[i_ml + 1].uv);
					start += 16;
				}
			}
		}
	}

	MEM_freeN(mloopuv_base);
}

static void ccgDM_buffer_copy_uvedge(
        DerivedMesh *dm, float *varray)
{
	int i, totpoly;
	int start;
	const MLoopUV *mloopuv;
#ifndef USE_LOOP_LAYOUT_FAST
	const MPoly *mpoly = dm->getPolyArray(dm);
#endif

	if ((mloopuv = DM_get_loop_data_layer(dm, CD_MLOOPUV)) == NULL) {
		return;
	}

	totpoly = dm->getNumPolys(dm);
	start = 0;

#ifndef USE_LOOP_LAYOUT_FAST
	for (i = 0; i < totpoly; i++, mpoly++) {
		for (j = 0; j < mpoly->totloop; j++) {
			copy_v2_v2(&varray[start], mloopuv[mpoly->loopstart + j].uv);
			copy_v2_v2(&varray[start + 2], mloopuv[mpoly->loopstart + (j + 1) % mpoly->totloop].uv);
			start += 4;
		}
	}
#else
	for (i = 0; i < totpoly; i++) {
		copy_v2_v2(&varray[start +  0], mloopuv[(i * 4) + 0].uv);
		copy_v2_v2(&varray[start +  2], mloopuv[(i * 4) + 1].uv);

		copy_v2_v2(&varray[start +  4], mloopuv[(i * 4) + 1].uv);
		copy_v2_v2(&varray[start +  6], mloopuv[(i * 4) + 2].uv);

		copy_v2_v2(&varray[start +  8], mloopuv[(i * 4) + 2].uv);
		copy_v2_v2(&varray[start + 10], mloopuv[(i * 4) + 3].uv);

		copy_v2_v2(&varray[start + 12], mloopuv[(i * 4) + 3].uv);
		copy_v2_v2(&varray[start + 14], mloopuv[(i * 4) + 0].uv);

		start += 16;
	}
#endif
}

static void ccgDM_buffer_copy_edge(
        DerivedMesh *dm, unsigned int *varray)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	/* getEdgeSuze returns num of verts, edges is one less */
	int i, j, edgeSize = ccgSubSurf_getEdgeSize(ss) - 1;
	int totedge = ccgSubSurf_getNumEdges(ss);
	int grid_face_side = ccgSubSurf_getGridSize(ss) - 1;
	int totface = ccgSubSurf_getNumFaces(ss);
	unsigned int index_start;
	unsigned int tot_interior = 0;
	unsigned int grid_tot_face = grid_face_side * grid_face_side;

	unsigned int iloose, inorm, iloosehidden, inormhidden;
	unsigned int tot_loose_hidden = 0, tot_loose = 0;
	unsigned int tot_hidden = 0, tot = 0;
	unsigned int iloosevert;
	/* int tot_interior = 0; */

	/* first, handle hidden/loose existing edges, then interior edges */
	for (j = 0; j < totedge; j++) {
		CCGEdge *e = ccgdm->edgeMap[j].edge;

		if (ccgdm->edgeFlags && !(ccgdm->edgeFlags[j] & ME_EDGEDRAW)) {
			if (!ccgSubSurf_getEdgeNumFaces(e)) tot_loose_hidden++;
			else tot_hidden++;
		}
		else {
			if (!ccgSubSurf_getEdgeNumFaces(e)) tot_loose++;
			else tot++;
		}
	}

	inorm = 0;
	inormhidden = tot * edgeSize;
	iloose = (tot + tot_hidden) * edgeSize;
	iloosehidden = (tot + tot_hidden + tot_loose) * edgeSize;
	iloosevert = dm->drawObject->tot_loop_verts;

	/* part one, handle all normal edges */
	for (j = 0; j < totedge; j++) {
		CCGFace *f;
		int fhandle = 0;
		int totvert = 0;
		unsigned int S = 0;
		CCGEdge *e = ccgdm->edgeMap[j].edge;
		bool isloose = !ccgSubSurf_getEdgeNumFaces(e);

		if (!isloose) {
			CCGVert *v1, *v2;
			CCGVert *ev1 = ccgSubSurf_getEdgeVert0(e);
			CCGVert *ev2 = ccgSubSurf_getEdgeVert1(e);

			f = ccgSubSurf_getEdgeFace(e, 0);
			fhandle = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(f));
			totvert = ccgSubSurf_getFaceNumVerts(f);

			/* find the index of vertices in the face */
			for (i = 0; i < totvert; i++) {
				v1 = ccgSubSurf_getFaceVert(f, i);
				v2 = ccgSubSurf_getFaceVert(f, (i + 1) % totvert);

				if ((ev1 == v1 && ev2 == v2) || (ev1 == v2 && ev2 == v1)) {
					S = i;
					break;
				}
			}
		}

		if (ccgdm->edgeFlags && !(ccgdm->edgeFlags[j] & ME_EDGEDRAW)) {
			if (isloose) {
				for (i = 0; i < edgeSize; i++) {
					varray[iloosehidden * 2] = iloosevert;
					varray[iloosehidden * 2 + 1] = iloosevert + 1;
					iloosehidden++;
					iloosevert++;
				}
				/* we are through with this loose edge and moving to the next, so increase by one */
				iloosevert++;
			}
			else {
				index_start = ccgdm->faceMap[fhandle].startFace;

				for (i = 0; i < grid_face_side; i++) {
					varray[inormhidden * 2] = (index_start + S * grid_tot_face + i * grid_face_side + grid_face_side - 1) * 4 + 1;
					varray[inormhidden * 2 + 1] = (index_start + S * grid_tot_face + i * grid_face_side + grid_face_side - 1) * 4 + 2;
					varray[inormhidden * 2 + 2] = (index_start + ((S + 1) % totvert) * grid_tot_face + grid_face_side * (grid_face_side - 1) + i) * 4 + 2;
					varray[inormhidden * 2 + 3] = (index_start + ((S + 1) % totvert) * grid_tot_face + grid_face_side * (grid_face_side - 1) + i) * 4 + 3;
					inormhidden += 2;
				}
			}
		}
		else {
			if (isloose) {
				for (i = 0; i < edgeSize; i++) {
					varray[iloose * 2] = iloosevert;
					varray[iloose * 2 + 1] = iloosevert + 1;
					iloose++;
					iloosevert++;
				}
				/* we are through with this loose edge and moving to the next, so increase by one */
				iloosevert++;
			}
			else {
				index_start = ccgdm->faceMap[fhandle].startFace;

				for (i = 0; i < grid_face_side; i++) {
					varray[inorm * 2] = (index_start + S * grid_tot_face + i * grid_face_side + grid_face_side - 1) * 4 + 1;
					varray[inorm * 2 + 1] = (index_start + S * grid_tot_face + i * grid_face_side + grid_face_side - 1) * 4 + 2;
					varray[inorm * 2 + 2] = (index_start + ((S + 1) % totvert) * grid_tot_face + grid_face_side * (grid_face_side - 1) + i) * 4 + 2;
					varray[inorm * 2 + 3] = (index_start + ((S + 1) % totvert) * grid_tot_face + grid_face_side * (grid_face_side - 1) + i) * 4 + 3;
					inorm += 2;
				}
			}
		}
	}

	/* part two, handle interior edges */
	inorm = totedge * grid_face_side * 2;

	index_start = 0;
	for (i = 0; i < totface; i++) {
		CCGFace *f = ccgdm->faceMap[i].face;
		unsigned int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(f);

		for (S = 0; S < numVerts; S++) {
			for (x = 1; x < grid_face_side; x++) {
				for (y = 0; y < grid_face_side; y++) {
					unsigned int tmp = (index_start + x * grid_face_side + y) * 4;
					varray[inorm * 2] = tmp;
					varray[inorm * 2 + 1] = tmp + 1;
					inorm++;
				}
			}
			for (x = 0; x < grid_face_side; x++) {
				for (y = 0; y < grid_face_side; y++) {
					unsigned int tmp = (index_start + x * grid_face_side + y) * 4;
					varray[inorm * 2] = tmp + 3;
					varray[inorm * 2 + 1] = tmp;
					inorm++;
				}
			}

			tot_interior += grid_face_side * (2 * grid_face_side - 1);
			index_start += grid_tot_face;
		}
	}

	dm->drawObject->tot_loose_edge_drawn = tot_loose * edgeSize;
	dm->drawObject->loose_edge_offset = (tot + tot_hidden) * edgeSize;
	dm->drawObject->tot_edge_drawn = tot * edgeSize;

	dm->drawObject->interior_offset = totedge * edgeSize;
	dm->drawObject->totinterior = tot_interior;
}

static void ccgDM_copy_gpu_data(
        DerivedMesh *dm, int type, void *varray_p,
        const int *mat_orig_to_new, const void *user_data)
{
	/* 'varray_p' cast is redundant but include for self-documentation */
	switch (type) {
		case GPU_BUFFER_VERTEX:
			ccgDM_buffer_copy_vertex(dm, (float *)varray_p);
			break;
		case GPU_BUFFER_NORMAL:
			ccgDM_buffer_copy_normal(dm, (short *)varray_p);
			break;
		case GPU_BUFFER_UV:
			ccgDM_buffer_copy_uv(dm, (float *)varray_p);
			break;
		case GPU_BUFFER_UV_TEXPAINT:
			ccgDM_buffer_copy_uv_texpaint(dm, (float *)varray_p);
			break;
		case GPU_BUFFER_COLOR:
			ccgDM_buffer_copy_color(dm, (unsigned char *)varray_p, user_data);
			break;
		case GPU_BUFFER_UVEDGE:
			ccgDM_buffer_copy_uvedge(dm, (float *)varray_p);
			break;
		case GPU_BUFFER_EDGE:
			ccgDM_buffer_copy_edge(dm, (unsigned int *)varray_p);
			break;
		case GPU_BUFFER_TRIANGLES:
			ccgDM_buffer_copy_triangles(dm, (unsigned int *)varray_p, mat_orig_to_new);
			break;
		default:
			break;
	}
}

static GPUDrawObject *ccgDM_GPUObjectNew(DerivedMesh *dm)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	GPUDrawObject *gdo;
	DMFlagMat *faceFlags = ccgdm->faceFlags;
	int gridFaces = ccgSubSurf_getGridSize(ss) - 1;
	const short dm_totmat = (faceFlags) ? dm->totmat : 1;
	GPUBufferMaterial *matinfo;
	int i;
	unsigned int tot_internal_edges = 0;
	int edgeVerts = ccgSubSurf_getEdgeSize(ss);
	int edgeSize = edgeVerts - 1;

	int totedge = ccgSubSurf_getNumEdges(ss);
	int totface = ccgSubSurf_getNumFaces(ss);

	/* object contains at least one material (default included) so zero means uninitialized dm */
	BLI_assert(dm_totmat != 0);

	matinfo = MEM_callocN(sizeof(*matinfo) * dm_totmat, "GPU_drawobject_new.mat_orig_to_new");

	if (faceFlags) {
		for (i = 0; i < totface; i++) {
			CCGFace *f = ccgdm->faceMap[i].face;
			int numVerts = ccgSubSurf_getFaceNumVerts(f);
			int index = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(f));
			const short new_matnr = ME_MAT_NR_TEST(faceFlags[index].mat_nr, dm_totmat);
			matinfo[new_matnr].totelements += numVerts * gridFaces * gridFaces * 6;
			matinfo[new_matnr].totloops += numVerts * gridFaces * gridFaces * 4;
			matinfo[new_matnr].totpolys++;
			tot_internal_edges += numVerts * gridFaces * (2 * gridFaces - 1);
		}
	}
	else {
		for (i = 0; i < totface; i++) {
			CCGFace *f = ccgdm->faceMap[i].face;
			int numVerts = ccgSubSurf_getFaceNumVerts(f);
			matinfo[0].totelements += numVerts * gridFaces * gridFaces * 6;
			matinfo[0].totloops += numVerts * gridFaces * gridFaces * 4;
			matinfo[0].totpolys++;
			tot_internal_edges += numVerts * gridFaces * (2 * gridFaces - 1);
		}
	}

	/* create the GPUDrawObject */
	gdo = MEM_callocN(sizeof(GPUDrawObject), "GPUDrawObject");
	gdo->totvert = 0; /* used to count indices, doesn't really matter for ccgsubsurf */
	gdo->totedge = (totedge * edgeSize + tot_internal_edges);

	GPU_buffer_material_finalize(gdo, matinfo, dm_totmat);

	/* store total number of points used for triangles */
	gdo->tot_triangle_point = ccgSubSurf_getNumFinalFaces(ss) * 6;
	gdo->tot_loop_verts = ccgSubSurf_getNumFinalFaces(ss) * 4;

	/* finally, count loose points */
	for (i = 0; i < totedge; i++) {
		CCGEdge *e = ccgdm->edgeMap[i].edge;

		if (!ccgSubSurf_getEdgeNumFaces(e))
			gdo->tot_loose_point += edgeVerts;
	}

	return gdo;
}

/* Only used by non-editmesh types */
static void ccgDM_drawFacesSolid(DerivedMesh *dm, float (*partial_redraw_planes)[4], bool fast, DMSetMaterial setMaterial)
{
	int a;
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;

	ccgdm_pbvh_update(ccgdm);

	if (ccgdm->pbvh && ccgdm->multires.mmd) {
		if (BKE_pbvh_has_faces(ccgdm->pbvh)) {
			BKE_pbvh_draw(ccgdm->pbvh, partial_redraw_planes, NULL,
			              setMaterial, false, fast);
		}

		return;
	}

#ifdef WITH_OPENSUBDIV
	if (ccgdm->useGpuBackend) {
		CCGSubSurf *ss = ccgdm->ss;
		const DMFlagMat *faceFlags = ccgdm->faceFlags;
		const int level = ccgSubSurf_getSubdivisionLevels(ss);
		const int face_side = 1 << level;
		const int grid_side = 1 << (level - 1);
		const int face_patches = face_side * face_side;
		const int grid_patches = grid_side * grid_side;
		const int num_base_faces = ccgSubSurf_getNumGLMeshBaseFaces(ss);
		int i, current_patch = 0;
		int mat_nr = -1;
		bool draw_smooth = false;
		int start_draw_patch = -1, num_draw_patches = 0;
		if (UNLIKELY(ccgSubSurf_prepareGLMesh(ss, setMaterial != NULL, -1) == false)) {
			return;
		}
		if (setMaterial == NULL) {
			ccgSubSurf_drawGLMesh(ss,
			                      true,
			                      -1,
			                      -1);
			return;
		}
		for (i = 0; i < num_base_faces; ++i) {
			const int num_face_verts = ccgSubSurf_getNumGLMeshBaseFaceVerts(ss, i);
			const int num_patches = (num_face_verts == 4) ? face_patches
			                                              : num_face_verts * grid_patches;
			int new_matnr;
			bool new_draw_smooth;
			if (faceFlags) {
				new_draw_smooth = (faceFlags[i].flag & ME_SMOOTH);
				new_matnr = (faceFlags[i].mat_nr + 1);
			}
			else {
				new_draw_smooth = true;
				new_matnr = 1;
			}
			if (new_draw_smooth != draw_smooth || new_matnr != mat_nr) {
				if (num_draw_patches != 0) {
					bool do_draw = setMaterial(mat_nr, NULL);
					if (do_draw) {
						glShadeModel(draw_smooth ? GL_SMOOTH : GL_FLAT);
						ccgSubSurf_drawGLMesh(ss,
						                      true,
						                      start_draw_patch,
						                      num_draw_patches);
					}
				}
				start_draw_patch = current_patch;
				num_draw_patches = num_patches;
				mat_nr = new_matnr;
				draw_smooth = new_draw_smooth;
			}
			else {
				num_draw_patches += num_patches;
			}
			current_patch += num_patches;
		}
		if (num_draw_patches != 0) {
			bool do_draw = setMaterial(mat_nr, NULL);
			if (do_draw) {
				glShadeModel(draw_smooth ? GL_SMOOTH : GL_FLAT);
				ccgSubSurf_drawGLMesh(ss,
				                      true,
				                      start_draw_patch,
				                      num_draw_patches);
			}
		}
		glShadeModel(GL_SMOOTH);
		return;
	}
#endif

	GPU_vertex_setup(dm);
	GPU_normal_setup(dm);
	GPU_triangle_setup(dm);
	for (a = 0; a < dm->drawObject->totmaterial; a++) {
		if (!setMaterial || setMaterial(dm->drawObject->materials[a].mat_nr + 1, NULL)) {
			GPU_buffer_draw_elements(dm->drawObject->triangles, GL_TRIANGLES, dm->drawObject->materials[a].start,
			                         dm->drawObject->materials[a].totelements);
		}
	}
	GPU_buffers_unbind();
}

typedef struct {
	DMVertexAttribs attribs;
	int numdata;

	GPUAttrib datatypes[GPU_MAX_ATTRIB]; /* TODO, messing up when switching materials many times - [#21056]*/
} GPUMaterialConv;

/* Only used by non-editmesh types */
static void ccgDM_drawMappedFacesGLSL(DerivedMesh *dm,
                                      DMSetMaterial setMaterial,
                                      DMSetDrawOptions setDrawOptions,
                                      void *userData)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGKey key;
	GPUVertexAttribs gattribs;
	int a, b;
	const DMFlagMat *faceFlags = ccgdm->faceFlags;
	unsigned char *varray;
	size_t max_element_size = 0;
	int tot_loops = 0;
	int totpoly = ccgSubSurf_getNumFaces(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);
	int gridFaces = gridSize - 1;
	int edgeSize = ccgSubSurf_getEdgeSize(ss);

#ifdef WITH_OPENSUBDIV
	if (ccgdm->useGpuBackend) {
		const int level = ccgSubSurf_getSubdivisionLevels(ss);
		const int face_side = 1 << level;
		const int grid_side = 1 << (level - 1);
		const int face_patches = face_side * face_side;
		const int grid_patches = grid_side * grid_side;
		const int num_base_faces = ccgSubSurf_getNumGLMeshBaseFaces(ss);
		int i, current_patch = 0;
		int mat_nr = -1;
		bool draw_smooth = false;
		int start_draw_patch = -1, num_draw_patches = 0;
		GPU_draw_update_fvar_offset(dm);
		if (UNLIKELY(ccgSubSurf_prepareGLMesh(ss, false, -1) == false)) {
			return;
		}
		for (i = 0; i < num_base_faces; ++i) {
			const int num_face_verts = ccgSubSurf_getNumGLMeshBaseFaceVerts(ss, i);
			const int num_patches = (num_face_verts == 4) ? face_patches
			                                              : num_face_verts * grid_patches;
			int new_matnr;
			bool new_draw_smooth;

			if (faceFlags) {
				new_draw_smooth = (faceFlags[i].flag & ME_SMOOTH);
				new_matnr = (faceFlags[i].mat_nr + 1);
			}
			else {
				new_draw_smooth = true;
				new_matnr = 1;
			}
			if (new_draw_smooth != draw_smooth || new_matnr != mat_nr) {
				if (num_draw_patches != 0) {
					bool do_draw = setMaterial(mat_nr, &gattribs);
					if (do_draw) {
						glShadeModel(draw_smooth ? GL_SMOOTH : GL_FLAT);
						ccgSubSurf_drawGLMesh(ss,
						                      true,
						                      start_draw_patch,
						                      num_draw_patches);
					}
				}
				start_draw_patch = current_patch;
				num_draw_patches = num_patches;
				mat_nr = new_matnr;
				draw_smooth = new_draw_smooth;
			}
			else {
				num_draw_patches += num_patches;
			}
			current_patch += num_patches;
		}
		if (num_draw_patches != 0) {
			bool do_draw = setMaterial(mat_nr, &gattribs);
			if (do_draw) {
				glShadeModel(draw_smooth ? GL_SMOOTH : GL_FLAT);
				ccgSubSurf_drawGLMesh(ss,
				                      true,
				                      start_draw_patch,
				                      num_draw_patches);
			}
		}
		glShadeModel(GL_SMOOTH);
		return;
	}
#endif

	CCG_key_top_level(&key, ss);
	ccgdm_pbvh_update(ccgdm);

	if (setDrawOptions != NULL) {
		const float (*lnors)[3] = dm->getLoopDataArray(dm, CD_NORMAL);
		DMVertexAttribs attribs = {{{NULL}}};
		int i;
		int matnr = -1;
		int do_draw = 0;

#define PASSATTRIB(dx, dy, vert) {                                           \
    if (attribs.totorco)                                                     \
        index = getFaceIndex(ss, f, S, x + dx, y + dy, edgeSize, gridSize);  \
    else                                                                     \
        index = 0;                                                           \
    DM_draw_attrib_vertex(&attribs, a, index, vert, ((a) * 4) + vert);       \
    DM_draw_attrib_vertex_uniforms(&attribs);                                \
} (void)0

		totpoly = ccgSubSurf_getNumFaces(ss);
		for (a = 0, i = 0; i < totpoly; i++) {
			CCGFace *f = ccgdm->faceMap[i].face;
			const float (*ln)[3] = NULL;
			int S, x, y, drawSmooth;
			int index = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(f));
			int origIndex = ccgDM_getFaceMapIndex(ss, f);

			int numVerts = ccgSubSurf_getFaceNumVerts(f);
			int new_matnr;

			if (faceFlags) {
				drawSmooth = (lnors || (faceFlags[index].flag & ME_SMOOTH));
				new_matnr = faceFlags[index].mat_nr + 1;
			}
			else {
				drawSmooth = 1;
				new_matnr = 1;
			}

			if (lnors) {
				ln = lnors;
				lnors += (gridFaces * gridFaces * numVerts) * 4;
			}

			if (new_matnr != matnr) {
				do_draw = setMaterial(matnr = new_matnr, &gattribs);
				if (do_draw)
					DM_vertex_attributes_from_gpu(dm, &gattribs, &attribs);
			}

			if (!do_draw || (setDrawOptions && (origIndex != ORIGINDEX_NONE) &&
			                (setDrawOptions(userData, origIndex) == DM_DRAW_OPTION_SKIP)))
			{
				a += gridFaces * gridFaces * numVerts;
				continue;
			}

			glShadeModel(drawSmooth ? GL_SMOOTH : GL_FLAT);
			for (S = 0; S < numVerts; S++) {
				CCGElem *faceGridData = ccgSubSurf_getFaceGridDataArray(ss, f, S);
				CCGElem *vda, *vdb;

				if (ln) {
					glBegin(GL_QUADS);
					for (y = 0; y < gridFaces; y++) {
						for (x = 0; x < gridFaces; x++) {
							float *aco = CCG_grid_elem_co(&key, faceGridData, x, y);
							float *bco = CCG_grid_elem_co(&key, faceGridData, x + 1, y);
							float *cco = CCG_grid_elem_co(&key, faceGridData, x + 1, y + 1);
							float *dco = CCG_grid_elem_co(&key, faceGridData, x, y + 1);

							PASSATTRIB(0, 1, 1);
							glNormal3fv(ln[1]);
							glVertex3fv(dco);
							PASSATTRIB(1, 1, 2);
							glNormal3fv(ln[2]);
							glVertex3fv(cco);
							PASSATTRIB(1, 0, 3);
							glNormal3fv(ln[3]);
							glVertex3fv(bco);
							PASSATTRIB(0, 0, 0);
							glNormal3fv(ln[0]);
							glVertex3fv(aco);

							ln += 4;
							a++;
						}
					}
					glEnd();
				}
				else if (drawSmooth) {
					for (y = 0; y < gridFaces; y++) {
						glBegin(GL_QUAD_STRIP);
						for (x = 0; x < gridFaces; x++) {
							vda = CCG_grid_elem(&key, faceGridData, x, y + 0);
							vdb = CCG_grid_elem(&key, faceGridData, x, y + 1);

							PASSATTRIB(0, 0, 0);
							glNormal3fv(CCG_elem_no(&key, vda));
							glVertex3fv(CCG_elem_co(&key, vda));

							PASSATTRIB(0, 1, 1);
							glNormal3fv(CCG_elem_no(&key, vdb));
							glVertex3fv(CCG_elem_co(&key, vdb));

							if (x != gridFaces - 1)
								a++;
						}

						vda = CCG_grid_elem(&key, faceGridData, x, y + 0);
						vdb = CCG_grid_elem(&key, faceGridData, x, y + 1);

						PASSATTRIB(0, 0, 3);
						glNormal3fv(CCG_elem_no(&key, vda));
						glVertex3fv(CCG_elem_co(&key, vda));

						PASSATTRIB(0, 1, 2);
						glNormal3fv(CCG_elem_no(&key, vdb));
						glVertex3fv(CCG_elem_co(&key, vdb));

						glEnd();

						a++;
					}
				}
				else {
					glBegin(GL_QUADS);
					for (y = 0; y < gridFaces; y++) {
						for (x = 0; x < gridFaces; x++) {
							float *aco = CCG_grid_elem_co(&key, faceGridData, x, y);
							float *bco = CCG_grid_elem_co(&key, faceGridData, x + 1, y);
							float *cco = CCG_grid_elem_co(&key, faceGridData, x + 1, y + 1);
							float *dco = CCG_grid_elem_co(&key, faceGridData, x, y + 1);

							ccgDM_glNormalFast(aco, bco, cco, dco);

							PASSATTRIB(0, 1, 1);
							glVertex3fv(dco);
							PASSATTRIB(1, 1, 2);
							glVertex3fv(cco);
							PASSATTRIB(1, 0, 3);
							glVertex3fv(bco);
							PASSATTRIB(0, 0, 0);
							glVertex3fv(aco);

							a++;
						}
					}
					glEnd();
				}
			}
		}

		glShadeModel(GL_SMOOTH);
#undef PASSATTRIB
	}
	else {
		GPUMaterialConv *matconv;
		size_t offset;
		int *mat_orig_to_new;
		int tot_active_mat;
		GPUBuffer *buffer = NULL;

		GPU_vertex_setup(dm);
		GPU_normal_setup(dm);
		GPU_triangle_setup(dm);

		tot_active_mat = dm->drawObject->totmaterial;

		matconv = MEM_callocN(sizeof(*matconv) * tot_active_mat,
		                      "cdDM_drawMappedFacesGLSL.matconv");
		mat_orig_to_new = MEM_mallocN(sizeof(*mat_orig_to_new) * dm->totmat,
		                              "cdDM_drawMappedFacesGLSL.mat_orig_to_new");

		/* part one, check what attributes are needed per material */
		for (a = 0; a < tot_active_mat; a++) {
			int new_matnr;
			int do_draw;

			new_matnr = dm->drawObject->materials[a].mat_nr;

			/* map from original material index to new
			 * GPUBufferMaterial index */
			mat_orig_to_new[new_matnr] = a;
			do_draw = setMaterial(new_matnr + 1, &gattribs);

			if (do_draw) {
				int numdata = 0;
				DM_vertex_attributes_from_gpu(dm, &gattribs, &matconv[a].attribs);

				if (matconv[a].attribs.totorco && matconv[a].attribs.orco.array) {
					matconv[a].datatypes[numdata].index = matconv[a].attribs.orco.gl_index;
					matconv[a].datatypes[numdata].info_index = matconv[a].attribs.orco.gl_info_index;
					matconv[a].datatypes[numdata].size = 3;
					matconv[a].datatypes[numdata].type = GL_FLOAT;
					numdata++;
				}
				for (b = 0; b < matconv[a].attribs.tottface; b++) {
					if (matconv[a].attribs.tface[b].array) {
						matconv[a].datatypes[numdata].index = matconv[a].attribs.tface[b].gl_index;
						matconv[a].datatypes[numdata].info_index = matconv[a].attribs.tface[b].gl_info_index;
						matconv[a].datatypes[numdata].size = 2;
						matconv[a].datatypes[numdata].type = GL_FLOAT;
						numdata++;
					}
				}
				for (b = 0; b < matconv[a].attribs.totmcol; b++) {
					if (matconv[a].attribs.mcol[b].array) {
						matconv[a].datatypes[numdata].index = matconv[a].attribs.mcol[b].gl_index;
						matconv[a].datatypes[numdata].info_index = matconv[a].attribs.mcol[b].gl_info_index;
						matconv[a].datatypes[numdata].size = 4;
						matconv[a].datatypes[numdata].type = GL_UNSIGNED_BYTE;
						numdata++;
					}
				}
				for (b = 0; b < matconv[a].attribs.tottang; b++) {
					if (matconv[a].attribs.tottang && matconv[a].attribs.tang[b].array) {
						matconv[a].datatypes[numdata].index = matconv[a].attribs.tang[b].gl_index;
						matconv[a].datatypes[numdata].info_index = matconv[a].attribs.tang[b].gl_info_index;
						matconv[a].datatypes[numdata].size = 4;
						matconv[a].datatypes[numdata].type = GL_FLOAT;
						numdata++;
					}
				}
				if (numdata != 0) {
					matconv[a].numdata = numdata;
					max_element_size = max_ii(GPU_attrib_element_size(matconv[a].datatypes, numdata), max_element_size);
				}
			}
		}

		/* part two, generate and fill the arrays with the data */
		if (max_element_size > 0) {
			buffer = GPU_buffer_alloc(max_element_size * dm->drawObject->tot_loop_verts);

			varray = GPU_buffer_lock_stream(buffer, GPU_BINDING_ARRAY);
			if (varray == NULL) {
				GPU_buffers_unbind();
				GPU_buffer_free(buffer);
				MEM_freeN(mat_orig_to_new);
				MEM_freeN(matconv);
				fprintf(stderr, "Out of memory, can't draw object\n");
				return;
			}

			for (a = 0; a < totpoly; a++) {
				CCGFace *f = ccgdm->faceMap[a].face;
				int orig_index = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(f));
				int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(f);
				int i;

				if (faceFlags) {
					i = mat_orig_to_new[faceFlags[orig_index].mat_nr];
				}
				else {
					i = mat_orig_to_new[0];
				}

				if (matconv[i].numdata != 0) {
					for (S = 0; S < numVerts; S++) {
						for (y = 0; y < gridFaces; y++) {
							for (x = 0; x < gridFaces; x++) {

								offset = tot_loops * max_element_size;

								if (matconv[i].attribs.totorco && matconv[i].attribs.orco.array) {
									int index;

									index = getFaceIndex(ss, f, S, x, y, edgeSize, gridSize);
									copy_v3_v3((float *)&varray[offset],
									           (float *)matconv[i].attribs.orco.array[index]);
									index = getFaceIndex(ss, f, S, x + 1, y, edgeSize, gridSize);
									copy_v3_v3((float *)&varray[offset + max_element_size],
									        (float *)matconv[i].attribs.orco.array[index]);
									index = getFaceIndex(ss, f, S, x + 1, y + 1, edgeSize, gridSize);
									copy_v3_v3((float *)&varray[offset + 2 * max_element_size],
									        (float *)matconv[i].attribs.orco.array[index]);
									index = getFaceIndex(ss, f, S, x, y + 1, edgeSize, gridSize);
									copy_v3_v3((float *)&varray[offset + 3 * max_element_size],
									        (float *)matconv[i].attribs.orco.array[index]);

									offset += sizeof(float) * 3;
								}
								for (b = 0; b < matconv[i].attribs.tottface; b++) {
									if (matconv[i].attribs.tface[b].array) {
										const MLoopUV *mloopuv = matconv[i].attribs.tface[b].array + tot_loops;

										copy_v2_v2((float *)&varray[offset], mloopuv[0].uv);
										copy_v2_v2((float *)&varray[offset + max_element_size], mloopuv[3].uv);
										copy_v2_v2((float *)&varray[offset + 2 * max_element_size], mloopuv[2].uv);
										copy_v2_v2((float *)&varray[offset + 3 * max_element_size], mloopuv[1].uv);

										offset += sizeof(float) * 2;
									}
								}
								for (b = 0; b < matconv[i].attribs.totmcol; b++) {
									if (matconv[i].attribs.mcol[b].array) {
										const MLoopCol *mloopcol = matconv[i].attribs.mcol[b].array + tot_loops;

										copy_v4_v4_uchar(&varray[offset], &mloopcol[0].r);
										copy_v4_v4_uchar(&varray[offset + max_element_size], &mloopcol[3].r);
										copy_v4_v4_uchar(&varray[offset + 2 * max_element_size], &mloopcol[2].r);
										copy_v4_v4_uchar(&varray[offset + 3 * max_element_size], &mloopcol[1].r);

										offset += sizeof(unsigned char) * 4;
									}
								}
								for (b = 0; b < matconv[i].attribs.tottang; b++) {
									if (matconv[i].attribs.tottang && matconv[i].attribs.tang[b].array) {
										const float (*looptang)[4] = (const float (*)[4])matconv[i].attribs.tang[b].array + tot_loops;

										copy_v4_v4((float *)&varray[offset], looptang[0]);
										copy_v4_v4((float *)&varray[offset + max_element_size], looptang[3]);
										copy_v4_v4((float *)&varray[offset + 2 * max_element_size], looptang[2]);
										copy_v4_v4((float *)&varray[offset + 3 * max_element_size], looptang[1]);

										offset += sizeof(float) * 4;
									}
								}

								tot_loops += 4;
							}
						}
					}
				}
				else {
					tot_loops += 4 * numVerts * gridFaces * gridFaces;
				}
			}
			GPU_buffer_unlock(buffer, GPU_BINDING_ARRAY);
		}

		for (a = 0; a < tot_active_mat; a++) {
			int new_matnr;
			int do_draw;

			new_matnr = dm->drawObject->materials[a].mat_nr;

			do_draw = setMaterial(new_matnr + 1, &gattribs);

			if (do_draw) {
				if (matconv[a].numdata) {
					GPU_interleaved_attrib_setup(buffer, matconv[a].datatypes, matconv[a].numdata, max_element_size);
				}
				GPU_buffer_draw_elements(dm->drawObject->triangles, GL_TRIANGLES,
				                         dm->drawObject->materials[a].start, dm->drawObject->materials[a].totelements);
				if (matconv[a].numdata) {
					GPU_interleaved_attrib_unbind();
				}
			}
		}

		GPU_buffers_unbind();
		if (buffer)
			GPU_buffer_free(buffer);

		MEM_freeN(mat_orig_to_new);
		MEM_freeN(matconv);
	}
}

static void ccgDM_drawFacesGLSL(DerivedMesh *dm, DMSetMaterial setMaterial)
{
	dm->drawMappedFacesGLSL(dm, setMaterial, NULL, NULL);
}

/* Only used by non-editmesh types */
static void ccgDM_drawMappedFacesMat(DerivedMesh *dm,
                                     void (*setMaterial)(void *userData, int matnr, void *attribs),
                                     bool (*setFace)(void *userData, int index), void *userData)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGKey key;
	GPUVertexAttribs gattribs;
	DMVertexAttribs attribs = {{{NULL}}};
	int gridSize = ccgSubSurf_getGridSize(ss);
	int gridFaces = gridSize - 1;
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	DMFlagMat *faceFlags = ccgdm->faceFlags;
	const float (*lnors)[3] = dm->getLoopDataArray(dm, CD_NORMAL);
	int a, i, numVerts, matnr, totface;

#ifdef WITH_OPENSUBDIV
	if (ccgdm->useGpuBackend) {
		const int level = ccgSubSurf_getSubdivisionLevels(ss);
		const int face_side = 1 << level;
		const int grid_side = 1 << (level - 1);
		const int face_patches = face_side * face_side;
		const int grid_patches = grid_side * grid_side;
		const int num_base_faces = ccgSubSurf_getNumGLMeshBaseFaces(ss);
		int current_patch = 0;
		int mat_nr = -1;
		bool draw_smooth = false;
		int start_draw_patch = -1, num_draw_patches = 0;
		GPU_draw_update_fvar_offset(dm);
		if (UNLIKELY(ccgSubSurf_prepareGLMesh(ss, true, -1) == false)) {
			return;
		}
		for (i = 0; i < num_base_faces; ++i) {
			const int num_face_verts = ccgSubSurf_getNumGLMeshBaseFaceVerts(ss, i);
			const int num_patches = (num_face_verts == 4) ? face_patches
			                                              : num_face_verts * grid_patches;
			int new_matnr;
			bool new_draw_smooth;

			if (faceFlags) {
				new_draw_smooth = (faceFlags[i].flag & ME_SMOOTH);
				new_matnr = (faceFlags[i].mat_nr + 1);
			}
			else {
				new_draw_smooth = true;
				new_matnr = 1;
			}
			if (new_draw_smooth != draw_smooth || new_matnr != mat_nr) {
				if (num_draw_patches != 0) {
					setMaterial(userData, mat_nr, &gattribs);
					glShadeModel(draw_smooth ? GL_SMOOTH : GL_FLAT);
					ccgSubSurf_drawGLMesh(ss,
					                      true,
					                      start_draw_patch,
					                      num_draw_patches);
				}
				start_draw_patch = current_patch;
				num_draw_patches = num_patches;
				mat_nr = new_matnr;
				draw_smooth = new_draw_smooth;
			}
			else {
				num_draw_patches += num_patches;
			}
			current_patch += num_patches;
		}
		if (num_draw_patches != 0) {
			setMaterial(userData, mat_nr, &gattribs);
			glShadeModel(draw_smooth ? GL_SMOOTH : GL_FLAT);
			ccgSubSurf_drawGLMesh(ss,
			                      true,
			                      start_draw_patch,
			                      num_draw_patches);
		}
		glShadeModel(GL_SMOOTH);
		return;
	}
#endif

	CCG_key_top_level(&key, ss);
	ccgdm_pbvh_update(ccgdm);

	matnr = -1;

#define PASSATTRIB(dx, dy, vert) {                                           \
    if (attribs.totorco)                                                     \
        index = getFaceIndex(ss, f, S, x + dx, y + dy, edgeSize, gridSize);  \
    else                                                                     \
        index = 0;                                                           \
    DM_draw_attrib_vertex(&attribs, a, index, vert, ((a) * 4) + vert);       \
    DM_draw_attrib_vertex_uniforms(&attribs);                                \
} (void)0

	totface = ccgSubSurf_getNumFaces(ss);
	for (a = 0, i = 0; i < totface; i++) {
		CCGFace *f = ccgdm->faceMap[i].face;
		const float (*ln)[3] = NULL;
		int S, x, y, drawSmooth;
		int index = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(f));
		int origIndex = ccgDM_getFaceMapIndex(ss, f);
		int new_matnr;

		numVerts = ccgSubSurf_getFaceNumVerts(f);

		/* get flags */
		if (faceFlags) {
			drawSmooth = (lnors || (faceFlags[index].flag & ME_SMOOTH));
			new_matnr = faceFlags[index].mat_nr + 1;
		}
		else {
			drawSmooth = 1;
			new_matnr = 1;
		}

		if (lnors) {
			ln = lnors;
			lnors += (gridFaces * gridFaces * numVerts) * 4;
		}

		/* material */
		if (new_matnr != matnr) {
			setMaterial(userData, matnr = new_matnr, &gattribs);
			DM_vertex_attributes_from_gpu(dm, &gattribs, &attribs);
		}

		/* face hiding */
		if ((setFace && (origIndex != ORIGINDEX_NONE) && !setFace(userData, origIndex))) {
			a += gridFaces * gridFaces * numVerts;
			continue;
		}

		/* draw face*/
		glShadeModel(drawSmooth ? GL_SMOOTH : GL_FLAT);
		for (S = 0; S < numVerts; S++) {
			CCGElem *faceGridData = ccgSubSurf_getFaceGridDataArray(ss, f, S);
			CCGElem *vda, *vdb;

			if (ln) {
				glBegin(GL_QUADS);
				for (y = 0; y < gridFaces; y++) {
					for (x = 0; x < gridFaces; x++) {
						float *aco = CCG_grid_elem_co(&key, faceGridData, x, y + 0);
						float *bco = CCG_grid_elem_co(&key, faceGridData, x + 1, y + 0);
						float *cco = CCG_grid_elem_co(&key, faceGridData, x + 1, y + 1);
						float *dco = CCG_grid_elem_co(&key, faceGridData, x, y + 1);

						PASSATTRIB(0, 1, 1);
						glNormal3fv(ln[1]);
						glVertex3fv(dco);
						PASSATTRIB(1, 1, 2);
						glNormal3fv(ln[2]);
						glVertex3fv(cco);
						PASSATTRIB(1, 0, 3);
						glNormal3fv(ln[3]);
						glVertex3fv(bco);
						PASSATTRIB(0, 0, 0);
						glNormal3fv(ln[0]);
						glVertex3fv(aco);

						ln += 4;
						a++;
					}
				}
				glEnd();
			}
			else if (drawSmooth) {
				for (y = 0; y < gridFaces; y++) {
					glBegin(GL_QUAD_STRIP);
					for (x = 0; x < gridFaces; x++) {
						vda = CCG_grid_elem(&key, faceGridData, x, y);
						vdb = CCG_grid_elem(&key, faceGridData, x, y + 1);

						PASSATTRIB(0, 0, 0);
						glNormal3fv(CCG_elem_no(&key, vda));
						glVertex3fv(CCG_elem_co(&key, vda));

						PASSATTRIB(0, 1, 1);
						glNormal3fv(CCG_elem_no(&key, vdb));
						glVertex3fv(CCG_elem_co(&key, vdb));

						if (x != gridFaces - 1)
							a++;
					}

					vda = CCG_grid_elem(&key, faceGridData, x, y + 0);
					vdb = CCG_grid_elem(&key, faceGridData, x, y + 1);

					PASSATTRIB(0, 0, 3);
					glNormal3fv(CCG_elem_no(&key, vda));
					glVertex3fv(CCG_elem_co(&key, vda));

					PASSATTRIB(0, 1, 2);
					glNormal3fv(CCG_elem_no(&key, vdb));
					glVertex3fv(CCG_elem_co(&key, vdb));

					glEnd();

					a++;
				}
			}
			else {
				glBegin(GL_QUADS);
				for (y = 0; y < gridFaces; y++) {
					for (x = 0; x < gridFaces; x++) {
						float *aco = CCG_grid_elem_co(&key, faceGridData, x, y + 0);
						float *bco = CCG_grid_elem_co(&key, faceGridData, x + 1, y + 0);
						float *cco = CCG_grid_elem_co(&key, faceGridData, x + 1, y + 1);
						float *dco = CCG_grid_elem_co(&key, faceGridData, x, y + 1);

						ccgDM_glNormalFast(aco, bco, cco, dco);

						PASSATTRIB(0, 1, 1);
						glVertex3fv(dco);
						PASSATTRIB(1, 1, 2);
						glVertex3fv(cco);
						PASSATTRIB(1, 0, 3);
						glVertex3fv(bco);
						PASSATTRIB(0, 0, 0);
						glVertex3fv(aco);

						a++;
					}
				}
				glEnd();
			}
		}
	}

	glShadeModel(GL_SMOOTH);
#undef PASSATTRIB
}

static void ccgDM_drawFacesTex_common(DerivedMesh *dm,
                                      DMSetDrawOptionsTex drawParams,
                                      DMSetDrawOptionsMappedTex drawParamsMapped,
                                      DMCompareDrawOptions compareDrawOptions,
                                      void *userData, DMDrawFlag flag)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGKey key;
	int colType;
	const MLoopCol *mloopcol = NULL;
	MTexPoly *mtexpoly = DM_get_poly_data_layer(dm, CD_MTEXPOLY);
	DMFlagMat *faceFlags = ccgdm->faceFlags;
	DMDrawOption draw_option;
	int i, totpoly;
	bool flush;
	const bool use_tface = (flag & DM_DRAW_USE_ACTIVE_UV) != 0;
	const bool use_colors = (flag & DM_DRAW_USE_COLORS) != 0;
	unsigned int next_actualFace;
	unsigned int gridFaces = ccgSubSurf_getGridSize(ss) - 1;
	int mat_index;
	int tot_element, start_element, tot_drawn;

	if (use_colors) {
		colType = CD_TEXTURE_MLOOPCOL;
		mloopcol = dm->getLoopDataArray(dm, colType);
		if (!mloopcol) {
			colType = CD_PREVIEW_MLOOPCOL;
			mloopcol = dm->getLoopDataArray(dm, colType);
		}
		if (!mloopcol) {
			colType = CD_MLOOPCOL;
			mloopcol = dm->getLoopDataArray(dm, colType);
		}
	}

#ifdef WITH_OPENSUBDIV
	if (ccgdm->useGpuBackend) {
		const int active_uv_layer = CustomData_get_active_layer_index(&dm->loopData, CD_MLOOPUV);
		if (UNLIKELY(ccgSubSurf_prepareGLMesh(ss, true, active_uv_layer) == false)) {
			return;
		}
		if (drawParams == NULL) {
			ccgSubSurf_drawGLMesh(ss, true, -1, -1);
			return;
		}
		const int level = ccgSubSurf_getSubdivisionLevels(ss);
		const int face_side = 1 << level;
		const int grid_side = 1 << (level - 1);
		const int face_patches = face_side * face_side;
		const int grid_patches = grid_side * grid_side;
		const int num_base_faces = ccgSubSurf_getNumGLMeshBaseFaces(ss);
		int current_patch = 0;
		int mat_nr = -1;
		int start_draw_patch = 0, num_draw_patches = 0;
		bool draw_smooth = false;
		for (i = 0; i < num_base_faces; ++i) {
			const int num_face_verts = ccgSubSurf_getNumGLMeshBaseFaceVerts(ss, i);
			const int num_patches = (num_face_verts == 4) ? face_patches
			                                              : num_face_verts * grid_patches;
			if (faceFlags) {
				mat_nr = faceFlags[i].mat_nr;
				draw_smooth = (faceFlags[i].flag & ME_SMOOTH);
			}
			else {
				mat_nr = 0;
				draw_smooth = false;
			}

			if (drawParams != NULL) {
				MTexPoly *tp = (use_tface && mtexpoly) ? &mtexpoly[i] : NULL;
				draw_option = drawParams(tp, (mloopcol != NULL), mat_nr);
			}
			else {
				draw_option = (drawParamsMapped)
				                  ? drawParamsMapped(userData, i, mat_nr)
				                  : DM_DRAW_OPTION_NORMAL;
			}

			flush = (draw_option == DM_DRAW_OPTION_SKIP) || (i == num_base_faces - 1);

			const int next_face = min_ii(i + 1, num_base_faces - 1);
			if (!flush && compareDrawOptions) {
				flush |= compareDrawOptions(userData, i, next_face) == 0;
			}
			if (!flush && faceFlags) {
				bool new_draw_smooth = (faceFlags[next_face].flag & ME_SMOOTH);
				flush |= (new_draw_smooth != draw_smooth);
			}

			current_patch += num_patches;

			if (flush) {
				if (draw_option != DM_DRAW_OPTION_SKIP) {
					num_draw_patches += num_patches;
				}
				if (num_draw_patches != 0) {
					glShadeModel(draw_smooth ? GL_SMOOTH : GL_FLAT);
					ccgSubSurf_drawGLMesh(ss,
					                      true,
					                      start_draw_patch,
					                      num_draw_patches);
				}
				start_draw_patch = current_patch;
				num_draw_patches = 0;
			}
			else {
				num_draw_patches += num_patches;
			}
		}
		glShadeModel(GL_SMOOTH);
		return;
	}
#endif

	CCG_key_top_level(&key, ss);
	ccgdm_pbvh_update(ccgdm);

	GPU_vertex_setup(dm);
	GPU_normal_setup(dm);
	GPU_triangle_setup(dm);
	if (flag & DM_DRAW_USE_TEXPAINT_UV)
		GPU_texpaint_uv_setup(dm);
	else
		GPU_uv_setup(dm);
	if (mloopcol) {
		GPU_color_setup(dm, colType);
	}

	next_actualFace = 0;

	/* lastFlag = 0; */ /* UNUSED */
	for (mat_index = 0; mat_index < dm->drawObject->totmaterial; mat_index++) {
		GPUBufferMaterial *bufmat = dm->drawObject->materials + mat_index;
		next_actualFace = bufmat->polys[0];
		totpoly = bufmat->totpolys;

		tot_element = 0;
		tot_drawn = 0;
		start_element = 0;

		for (i = 0; i < totpoly; i++) {
			int polyindex = bufmat->polys[i];
			CCGFace *f = ccgdm->faceMap[polyindex].face;
			int numVerts = ccgSubSurf_getFaceNumVerts(f);
			int index = ccgDM_getFaceMapIndex(ss, f);
			int orig_index = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(f));
			int mat_nr;
			int facequads = numVerts * gridFaces * gridFaces;
			int actualFace = ccgdm->faceMap[polyindex].startFace;

			if (i != totpoly - 1) {
				polyindex = bufmat->polys[i + 1];
				next_actualFace = ccgdm->faceMap[polyindex].startFace;
			}

			if (faceFlags) {
				mat_nr = faceFlags[orig_index].mat_nr;
			}
			else {
				mat_nr = 0;
			}

			if (drawParams) {
				MTexPoly *tp = (use_tface && mtexpoly) ? &mtexpoly[actualFace] : NULL;
				draw_option = drawParams(tp, (mloopcol != NULL), mat_nr);
			}
			else if (index != ORIGINDEX_NONE)
				draw_option = (drawParamsMapped) ? drawParamsMapped(userData, index, mat_nr) : DM_DRAW_OPTION_NORMAL;
			else
				draw_option = DM_DRAW_OPTION_NORMAL;

			/* flush buffer if current triangle isn't drawable or it's last triangle */
			flush = (draw_option == DM_DRAW_OPTION_SKIP) || (i == totpoly - 1);

			if (!flush && compareDrawOptions) {
				/* also compare draw options and flush buffer if they're different
				 * need for face selection highlight in edit mode */
				flush |= compareDrawOptions(userData, actualFace, next_actualFace) == 0;
			}

			tot_element += facequads * 6;

			if (flush) {
				if (draw_option != DM_DRAW_OPTION_SKIP)
					tot_drawn += facequads * 6;

				if (tot_drawn) {
					if (mloopcol && draw_option != DM_DRAW_OPTION_NO_MCOL)
						GPU_color_switch(1);
					else
						GPU_color_switch(0);

					GPU_buffer_draw_elements(dm->drawObject->triangles, GL_TRIANGLES, bufmat->start + start_element, tot_drawn);
					tot_drawn = 0;
				}

				start_element = tot_element;
			}
			else {
				tot_drawn += facequads * 6;
			}
		}
	}


	GPU_buffers_unbind();
}

static void ccgDM_drawFacesTex(DerivedMesh *dm,
                               DMSetDrawOptionsTex setDrawOptions,
                               DMCompareDrawOptions compareDrawOptions,
                               void *userData, DMDrawFlag flag)
{
	ccgDM_drawFacesTex_common(dm, setDrawOptions, NULL, compareDrawOptions, userData, flag);
}

static void ccgDM_drawMappedFacesTex(DerivedMesh *dm,
                                     DMSetDrawOptionsMappedTex setDrawOptions,
                                     DMCompareDrawOptions compareDrawOptions,
                                     void *userData, DMDrawFlag flag)
{
	ccgDM_drawFacesTex_common(dm, NULL, setDrawOptions, compareDrawOptions, userData, flag);
}

/* same as cdDM_drawUVEdges */
static void ccgDM_drawUVEdges(DerivedMesh *dm)
{
	MPoly *mpoly = dm->getPolyArray(dm);
	int totpoly = dm->getNumPolys(dm);
	int prevstart = 0;
	bool prevdraw = true;
	int curpos = 0;
	int i;

	GPU_uvedge_setup(dm);
	for (i = 0; i < totpoly; i++, mpoly++) {
		const bool draw = (mpoly->flag & ME_HIDE) == 0;

		if (prevdraw != draw) {
			if (prevdraw && (curpos != prevstart)) {
				glDrawArrays(GL_LINES, prevstart, curpos - prevstart);
			}
			prevstart = curpos;
		}

		curpos += 2 * mpoly->totloop;
		prevdraw = draw;
	}
	if (prevdraw && (curpos != prevstart)) {
		glDrawArrays(GL_LINES, prevstart, curpos - prevstart);
	}
	GPU_buffers_unbind();
}

static void ccgDM_drawMappedFaces(DerivedMesh *dm,
                                  DMSetDrawOptions setDrawOptions,
                                  DMSetMaterial setMaterial,
                                  DMCompareDrawOptions compareDrawOptions,
                                  void *userData, DMDrawFlag flag)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGKey key;
	MLoopCol *mloopcol = NULL;
	const float (*lnors)[3] = dm->getLoopDataArray(dm, CD_NORMAL);
	int i, gridSize = ccgSubSurf_getGridSize(ss);
	DMFlagMat *faceFlags = ccgdm->faceFlags;
	int useColors = flag & DM_DRAW_USE_COLORS;
	int gridFaces = gridSize - 1, totface;
	int prev_mat_nr = -1;

	if (ccgdm->pbvh) {
		if (G.debug_value == 14)
			BKE_pbvh_draw_BB(ccgdm->pbvh);
	}

#ifdef WITH_OPENSUBDIV
	if (ccgdm->useGpuBackend) {
		int new_matnr;
		bool draw_smooth, do_draw = true;
		if (setDrawOptions == NULL) {
			/* TODO(sergey): This is for cases when vertex colors or weights
			 * are visualising. Currently we don't have CD layers for this data
			 * and here we only make it so there's no garbage displayed.
			 *
			 * In the future we'll either need to have CD for this data or pass
			 * this data as face-varying or vertex-varying data in OSD mesh.
			 */
			glColor3f(0.8f, 0.8f, 0.8f);
		}
		if (UNLIKELY(ccgSubSurf_prepareGLMesh(ss, true, -1) == false)) {
			return;
		}
		if (faceFlags) {
			draw_smooth = (faceFlags[0].flag & ME_SMOOTH);
			new_matnr = (faceFlags[0].mat_nr + 1);
		}
		else {
			draw_smooth = true;
			new_matnr = 1;
		}
		if (setMaterial) {
			setMaterial(new_matnr, NULL);
		}
		if (setDrawOptions) {
			if (setDrawOptions(userData, 0) == DM_DRAW_OPTION_SKIP) {
				do_draw = false;
			}
		}
		if (do_draw) {
			glShadeModel(draw_smooth ? GL_SMOOTH : GL_FLAT);
			ccgSubSurf_drawGLMesh(ss, true, -1, -1);
			glShadeModel(GL_SMOOTH);
		}
		return;
	}
#endif

	CCG_key_top_level(&key, ss);

	/* currently unused -- each original face is handled separately */
	(void)compareDrawOptions;

	if (useColors) {
		mloopcol = dm->getLoopDataArray(dm, CD_PREVIEW_MLOOPCOL);
		if (!mloopcol)
			mloopcol = dm->getLoopDataArray(dm, CD_MLOOPCOL);
	}

	totface = ccgSubSurf_getNumFaces(ss);
	for (i = 0; i < totface; i++) {
		CCGFace *f = ccgdm->faceMap[i].face;
		int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(f);
		int drawSmooth, index = ccgDM_getFaceMapIndex(ss, f);
		int origIndex;
		unsigned char *cp = NULL;
		const float (*ln)[3] = NULL;

		origIndex = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(f));

		if (flag & DM_DRAW_ALWAYS_SMOOTH) drawSmooth = 1;
		else if (faceFlags) drawSmooth = (lnors || (faceFlags[origIndex].flag & ME_SMOOTH));
		else drawSmooth = 1;

		if (mloopcol) {
			cp = (unsigned char *)mloopcol;
			mloopcol += gridFaces * gridFaces * numVerts * 4;
		}

		if (lnors) {
			ln = lnors;
			lnors += (gridFaces * gridFaces * numVerts) * 4;
		}

		{
			DMDrawOption draw_option = DM_DRAW_OPTION_NORMAL;

			if (setMaterial) {
				int mat_nr = faceFlags ? faceFlags[origIndex].mat_nr + 1 : 1;

				if (mat_nr != prev_mat_nr) {
					setMaterial(mat_nr, NULL);  /* XXX, no faceFlags no material */
					prev_mat_nr = mat_nr;
				}
			}

			if (setDrawOptions && (index != ORIGINDEX_NONE))
				draw_option = setDrawOptions(userData, index);

			if (draw_option != DM_DRAW_OPTION_SKIP) {
				if (draw_option == DM_DRAW_OPTION_STIPPLE) {
					GPU_basic_shader_bind(GPU_SHADER_STIPPLE | GPU_SHADER_USE_COLOR);
					GPU_basic_shader_stipple(GPU_SHADER_STIPPLE_QUARTTONE);
				}

				for (S = 0; S < numVerts; S++) {
					CCGElem *faceGridData = ccgSubSurf_getFaceGridDataArray(ss, f, S);
					if (ln) {
						glBegin(GL_QUADS);
						for (y = 0; y < gridFaces; y++) {
							for (x = 0; x < gridFaces; x++) {
								float *a = CCG_grid_elem_co(&key, faceGridData, x, y + 0);
								float *b = CCG_grid_elem_co(&key, faceGridData, x + 1, y + 0);
								float *c = CCG_grid_elem_co(&key, faceGridData, x + 1, y + 1);
								float *d = CCG_grid_elem_co(&key, faceGridData, x, y + 1);

								if (cp) glColor4ubv(&cp[4]);
								glNormal3fv(ln[1]);
								glVertex3fv(d);
								if (cp) glColor4ubv(&cp[8]);
								glNormal3fv(ln[2]);
								glVertex3fv(c);
								if (cp) glColor4ubv(&cp[12]);
								glNormal3fv(ln[3]);
								glVertex3fv(b);
								if (cp) glColor4ubv(&cp[0]);
								glNormal3fv(ln[0]);
								glVertex3fv(a);

								if (cp) cp += 16;
								ln += 4;
							}
						}
						glEnd();
					}
					else if (drawSmooth) {
						for (y = 0; y < gridFaces; y++) {
							CCGElem *a, *b;
							glBegin(GL_QUAD_STRIP);
							for (x = 0; x < gridFaces; x++) {
								a = CCG_grid_elem(&key, faceGridData, x, y + 0);
								b = CCG_grid_elem(&key, faceGridData, x, y + 1);

								if (cp) glColor4ubv(&cp[0]);
								glNormal3fv(CCG_elem_no(&key, a));
								glVertex3fv(CCG_elem_co(&key, a));
								if (cp) glColor4ubv(&cp[4]);
								glNormal3fv(CCG_elem_no(&key, b));
								glVertex3fv(CCG_elem_co(&key, b));

								if (x != gridFaces - 1) {
									if (cp) cp += 16;
								}
							}

							a = CCG_grid_elem(&key, faceGridData, x, y + 0);
							b = CCG_grid_elem(&key, faceGridData, x, y + 1);

							if (cp) glColor4ubv(&cp[12]);
							glNormal3fv(CCG_elem_no(&key, a));
							glVertex3fv(CCG_elem_co(&key, a));
							if (cp) glColor4ubv(&cp[8]);
							glNormal3fv(CCG_elem_no(&key, b));
							glVertex3fv(CCG_elem_co(&key, b));

							if (cp) cp += 16;

							glEnd();
						}
					}
					else {
						glBegin(GL_QUADS);
						for (y = 0; y < gridFaces; y++) {
							for (x = 0; x < gridFaces; x++) {
								float *a = CCG_grid_elem_co(&key, faceGridData, x, y + 0);
								float *b = CCG_grid_elem_co(&key, faceGridData, x + 1, y + 0);
								float *c = CCG_grid_elem_co(&key, faceGridData, x + 1, y + 1);
								float *d = CCG_grid_elem_co(&key, faceGridData, x, y + 1);

								ccgDM_glNormalFast(a, b, c, d);

								if (cp) glColor4ubv(&cp[4]);
								glVertex3fv(d);
								if (cp) glColor4ubv(&cp[8]);
								glVertex3fv(c);
								if (cp) glColor4ubv(&cp[12]);
								glVertex3fv(b);
								if (cp) glColor4ubv(&cp[0]);
								glVertex3fv(a);

								if (cp) cp += 16;
							}
						}
						glEnd();
					}
				}
				if (draw_option == DM_DRAW_OPTION_STIPPLE)
					GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);
			}
		}
	}
}

static void ccgDM_drawMappedEdges(DerivedMesh *dm,
                                  DMSetDrawOptions setDrawOptions,
                                  void *userData)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGEdgeIterator ei;
	CCGKey key;
	int i, useAging, edgeSize = ccgSubSurf_getEdgeSize(ss);

#ifdef WITH_OPENSUBDIV
	if (ccgdm->useGpuBackend) {
		/* TODO(sergey): Only draw edges from base mesh. */
		if (ccgSubSurf_prepareGLMesh(ccgdm->ss, true, -1)) {
			if (!setDrawOptions || (setDrawOptions(userData, 0) != DM_DRAW_OPTION_SKIP)) {
				ccgSubSurf_drawGLMesh(ccgdm->ss, false, -1, -1);
			}
		}
		return;
	}
#endif

	CCG_key_top_level(&key, ss);
	ccgSubSurf_getUseAgeCounts(ss, &useAging, NULL, NULL, NULL);

	for (ccgSubSurf_initEdgeIterator(ss, &ei); !ccgEdgeIterator_isStopped(&ei); ccgEdgeIterator_next(&ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(&ei);
		CCGElem *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);
		int index = ccgDM_getEdgeMapIndex(ss, e);

		glBegin(GL_LINE_STRIP);
		if (index != -1 && (!setDrawOptions || (setDrawOptions(userData, index) != DM_DRAW_OPTION_SKIP))) {
			if (useAging && !(G.f & G_BACKBUFSEL)) {
				int ageCol = 255 - ccgSubSurf_getEdgeAge(ss, e) * 4;
				glColor3ub(0, ageCol > 0 ? ageCol : 0, 0);
			}

			for (i = 0; i < edgeSize - 1; i++) {
				glVertex3fv(CCG_elem_offset_co(&key, edgeData, i));
				glVertex3fv(CCG_elem_offset_co(&key, edgeData, i + 1));
			}
		}
		glEnd();
	}
}

static void ccgDM_drawMappedEdgesInterp(DerivedMesh *dm,
                                        DMSetDrawOptions setDrawOptions,
                                        DMSetDrawInterpOptions setDrawInterpOptions,
                                        void *userData)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGKey key;
	CCGEdgeIterator ei;
	int i, useAging, edgeSize = ccgSubSurf_getEdgeSize(ss);

#ifdef WITH_OPENSUBDIV
	if (ccgdm->useGpuBackend) {
		BLI_assert(!"Not currently supported");
		return;
	}
#endif

	CCG_key_top_level(&key, ss);
	ccgSubSurf_getUseAgeCounts(ss, &useAging, NULL, NULL, NULL);

	for (ccgSubSurf_initEdgeIterator(ss, &ei); !ccgEdgeIterator_isStopped(&ei); ccgEdgeIterator_next(&ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(&ei);
		CCGElem *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);
		int index = ccgDM_getEdgeMapIndex(ss, e);

		glBegin(GL_LINE_STRIP);
		if (index != -1 && (!setDrawOptions || (setDrawOptions(userData, index) != DM_DRAW_OPTION_SKIP))) {
			for (i = 0; i < edgeSize; i++) {
				setDrawInterpOptions(userData, index, (float) i / (edgeSize - 1));

				if (useAging && !(G.f & G_BACKBUFSEL)) {
					int ageCol = 255 - ccgSubSurf_getEdgeAge(ss, e) * 4;
					glColor3ub(0, ageCol > 0 ? ageCol : 0, 0);
				}

				glVertex3fv(CCG_elem_offset_co(&key, edgeData, i));
			}
		}
		glEnd();
	}
}

static void ccgDM_foreachMappedFaceCenter(
        DerivedMesh *dm,
        void (*func)(void *userData, int index, const float co[3], const float no[3]),
        void *userData,
        DMForeachFlag flag)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGKey key;
	CCGFaceIterator fi;

	CCG_key_top_level(&key, ss);

	for (ccgSubSurf_initFaceIterator(ss, &fi); !ccgFaceIterator_isStopped(&fi); ccgFaceIterator_next(&fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(&fi);
		const int index = ccgDM_getFaceMapIndex(ss, f);

		if (index != -1) {
			/* Face center data normal isn't updated atm. */
			CCGElem *vd = ccgSubSurf_getFaceGridData(ss, f, 0, 0, 0);
			const float *no = (flag & DM_FOREACH_USE_NORMAL) ? CCG_elem_no(&key, vd) : NULL;
			func(userData, index, CCG_elem_co(&key, vd), no);
		}
	}
}

static void ccgDM_release(DerivedMesh *dm)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;

	if (DM_release(dm)) {
		/* Before freeing, need to update the displacement map */
		if (ccgdm->multires.modified_flags) {
			/* Check that mmd still exists */
			if (!ccgdm->multires.local_mmd &&
			    BLI_findindex(&ccgdm->multires.ob->modifiers, ccgdm->multires.mmd) < 0)
			{
				ccgdm->multires.mmd = NULL;
			}

			if (ccgdm->multires.mmd) {
				if (ccgdm->multires.modified_flags & MULTIRES_COORDS_MODIFIED)
					multires_modifier_update_mdisps(dm);
				if (ccgdm->multires.modified_flags & MULTIRES_HIDDEN_MODIFIED)
					multires_modifier_update_hidden(dm);
			}
		}

		if (ccgdm->ehash)
			BLI_edgehash_free(ccgdm->ehash, NULL);

		if (ccgdm->reverseFaceMap) MEM_freeN(ccgdm->reverseFaceMap);
		if (ccgdm->gridFaces) MEM_freeN(ccgdm->gridFaces);
		if (ccgdm->gridData) MEM_freeN(ccgdm->gridData);
		if (ccgdm->gridOffset) MEM_freeN(ccgdm->gridOffset);
		if (ccgdm->gridFlagMats) MEM_freeN(ccgdm->gridFlagMats);
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
		if (ccgdm->freeSS) ccgSubSurf_free(ccgdm->ss);
		if (ccgdm->pmap) MEM_freeN(ccgdm->pmap);
		if (ccgdm->pmap_mem) MEM_freeN(ccgdm->pmap_mem);
		MEM_freeN(ccgdm->edgeFlags);
		MEM_freeN(ccgdm->faceFlags);
		if (ccgdm->useGpuBackend == false) {
			MEM_freeN(ccgdm->vertMap);
			MEM_freeN(ccgdm->edgeMap);
			MEM_freeN(ccgdm->faceMap);
		}

		BLI_mutex_end(&ccgdm->loops_cache_lock);
		BLI_rw_mutex_end(&ccgdm->origindex_cache_rwlock);

		MEM_freeN(ccgdm);
	}
}

static void *ccgDM_get_vert_data_layer(DerivedMesh *dm, int type)
{
	if (type == CD_ORIGINDEX) {
		/* create origindex on demand to save memory */
		CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
		CCGSubSurf *ss = ccgdm->ss;
		int *origindex;
		int a, index, totnone, totorig;

		/* Avoid re-creation if the layer exists already */
		BLI_rw_mutex_lock(&ccgdm->origindex_cache_rwlock, THREAD_LOCK_READ);
		origindex = DM_get_vert_data_layer(dm, CD_ORIGINDEX);
		BLI_rw_mutex_unlock(&ccgdm->origindex_cache_rwlock);
		if (origindex) {
			return origindex;
		}

		BLI_rw_mutex_lock(&ccgdm->origindex_cache_rwlock, THREAD_LOCK_WRITE);
		DM_add_vert_layer(dm, CD_ORIGINDEX, CD_CALLOC, NULL);
		origindex = DM_get_vert_data_layer(dm, CD_ORIGINDEX);

		totorig = ccgSubSurf_getNumVerts(ss);
		totnone = dm->numVertData - totorig;

		/* original vertices are at the end */
		for (a = 0; a < totnone; a++)
			origindex[a] = ORIGINDEX_NONE;

		for (index = 0; index < totorig; index++, a++) {
			CCGVert *v = ccgdm->vertMap[index].vert;
			origindex[a] = ccgDM_getVertMapIndex(ccgdm->ss, v);
		}
		BLI_rw_mutex_unlock(&ccgdm->origindex_cache_rwlock);

		return origindex;
	}

	return DM_get_vert_data_layer(dm, type);
}

static void *ccgDM_get_edge_data_layer(DerivedMesh *dm, int type)
{
	if (type == CD_ORIGINDEX) {
		/* create origindex on demand to save memory */
		CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
		CCGSubSurf *ss = ccgdm->ss;
		int *origindex;
		int a, i, index, totnone, totorig, totedge;
		int edgeSize = ccgSubSurf_getEdgeSize(ss);

		/* Avoid re-creation if the layer exists already */
		origindex = DM_get_edge_data_layer(dm, CD_ORIGINDEX);
		if (origindex) {
			return origindex;
		}

		DM_add_edge_layer(dm, CD_ORIGINDEX, CD_CALLOC, NULL);
		origindex = DM_get_edge_data_layer(dm, CD_ORIGINDEX);

		totedge = ccgSubSurf_getNumEdges(ss);
		totorig = totedge * (edgeSize - 1);
		totnone = dm->numEdgeData - totorig;

		/* original edges are at the end */
		for (a = 0; a < totnone; a++)
			origindex[a] = ORIGINDEX_NONE;

		for (index = 0; index < totedge; index++) {
			CCGEdge *e = ccgdm->edgeMap[index].edge;
			int mapIndex = ccgDM_getEdgeMapIndex(ss, e);

			for (i = 0; i < edgeSize - 1; i++, a++)
				origindex[a] = mapIndex;
		}

		return origindex;
	}

	return DM_get_edge_data_layer(dm, type);
}

static void *ccgDM_get_tessface_data_layer(DerivedMesh *dm, int type)
{
	if (type == CD_ORIGINDEX) {
		/* create origindex on demand to save memory */
		int *origindex;

		/* Avoid re-creation if the layer exists already */
		origindex = DM_get_tessface_data_layer(dm, CD_ORIGINDEX);
		if (origindex) {
			return origindex;
		}

		DM_add_tessface_layer(dm, CD_ORIGINDEX, CD_CALLOC, NULL);
		origindex = DM_get_tessface_data_layer(dm, CD_ORIGINDEX);

		/* silly loop counting up */
		range_vn_i(origindex, dm->getNumTessFaces(dm), 0);

		return origindex;
	}

	if (type == CD_TESSLOOPNORMAL) {
		/* Create tessloopnormal on demand to save memory. */
		/* Note that since tessellated face corners are the same a loops in CCGDM, and since all faces have four
		 * loops/corners, we can simplify the code here by converting tessloopnormals from 'short (*)[4][3]'
		 * to 'short (*)[3]'.
		 */
		short (*tlnors)[3];

		/* Avoid re-creation if the layer exists already */
		tlnors = DM_get_tessface_data_layer(dm, CD_TESSLOOPNORMAL);
		if (!tlnors) {
			float (*lnors)[3];
			short (*tlnors_it)[3];
			const int numLoops = ccgDM_getNumLoops(dm);
			int i;

			lnors = dm->getLoopDataArray(dm, CD_NORMAL);
			if (!lnors) {
				return NULL;
			}

			DM_add_tessface_layer(dm, CD_TESSLOOPNORMAL, CD_CALLOC, NULL);
			tlnors = tlnors_it = (short (*)[3])DM_get_tessface_data_layer(dm, CD_TESSLOOPNORMAL);

			/* With ccgdm, we have a simple one to one mapping between loops and tessellated face corners. */
			for (i = 0; i < numLoops; ++i, ++tlnors_it, ++lnors) {
				normal_float_to_short_v3(*tlnors_it, *lnors);
			}
		}

		return tlnors;
	}

	return DM_get_tessface_data_layer(dm, type);
}

static void *ccgDM_get_poly_data_layer(DerivedMesh *dm, int type)
{
	if (type == CD_ORIGINDEX) {
		/* create origindex on demand to save memory */
		CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
		CCGSubSurf *ss = ccgdm->ss;
		int *origindex;
		int a, i, index, totface;
		int gridFaces = ccgSubSurf_getGridSize(ss) - 1;

		/* Avoid re-creation if the layer exists already */
		origindex = DM_get_poly_data_layer(dm, CD_ORIGINDEX);
		if (origindex) {
			return origindex;
		}

		DM_add_poly_layer(dm, CD_ORIGINDEX, CD_CALLOC, NULL);
		origindex = DM_get_poly_data_layer(dm, CD_ORIGINDEX);

		totface = ccgSubSurf_getNumFaces(ss);

		for (a = 0, index = 0; index < totface; index++) {
			CCGFace *f = ccgdm->faceMap[index].face;
			int numVerts = ccgSubSurf_getFaceNumVerts(f);
			int mapIndex = ccgDM_getFaceMapIndex(ss, f);

			for (i = 0; i < gridFaces * gridFaces * numVerts; i++, a++)
				origindex[a] = mapIndex;
		}

		return origindex;
	}

	return DM_get_poly_data_layer(dm, type);
}

static void *ccgDM_get_vert_data(DerivedMesh *dm, int index, int type)
{
	if (type == CD_ORIGINDEX) {
		/* ensure creation of CD_ORIGINDEX layer */
		ccgDM_get_vert_data_layer(dm, type);
	}

	return DM_get_vert_data(dm, index, type);
}

static void *ccgDM_get_edge_data(DerivedMesh *dm, int index, int type)
{
	if (type == CD_ORIGINDEX) {
		/* ensure creation of CD_ORIGINDEX layer */
		ccgDM_get_edge_data_layer(dm, type);
	}

	return DM_get_edge_data(dm, index, type);
}

static void *ccgDM_get_tessface_data(DerivedMesh *dm, int index, int type)
{
	if (ELEM(type, CD_ORIGINDEX, CD_TESSLOOPNORMAL)) {
		/* ensure creation of CD_ORIGINDEX/CD_TESSLOOPNORMAL layers */
		ccgDM_get_tessface_data_layer(dm, type);
	}

	return DM_get_tessface_data(dm, index, type);
}

static void *ccgDM_get_poly_data(DerivedMesh *dm, int index, int type)
{
	if (type == CD_ORIGINDEX) {
		/* ensure creation of CD_ORIGINDEX layer */
		ccgDM_get_tessface_data_layer(dm, type);
	}

	return DM_get_poly_data(dm, index, type);
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
	int index, numFaces, numGrids, S, gIndex /*, gridSize*/;

	if (ccgdm->gridData)
		return;

	numGrids = ccgDM_getNumGrids(dm);
	numFaces = ccgSubSurf_getNumFaces(ss);
	/*gridSize = ccgDM_getGridSize(dm);*/  /*UNUSED*/

	/* compute offset into grid array for each face */
	gridOffset = MEM_mallocN(sizeof(int) * numFaces, "ccgdm.gridOffset");

	for (gIndex = 0, index = 0; index < numFaces; index++) {
		CCGFace *f = ccgdm->faceMap[index].face;
		int numVerts = ccgSubSurf_getFaceNumVerts(f);

		gridOffset[index] = gIndex;
		gIndex += numVerts;
	}

	/* compute grid data */
	gridData = MEM_mallocN(sizeof(CCGElem *) * numGrids, "ccgdm.gridData");
	gridFaces = MEM_mallocN(sizeof(CCGFace *) * numGrids, "ccgdm.gridFaces");
	gridFlagMats = MEM_mallocN(sizeof(DMFlagMat) * numGrids, "ccgdm.gridFlagMats");

	ccgdm->gridHidden = MEM_callocN(sizeof(*ccgdm->gridHidden) * numGrids, "ccgdm.gridHidden");

	for (gIndex = 0, index = 0; index < numFaces; index++) {
		CCGFace *f = ccgdm->faceMap[index].face;
		int numVerts = ccgSubSurf_getFaceNumVerts(f);

		for (S = 0; S < numVerts; S++, gIndex++) {
			gridData[gIndex] = ccgSubSurf_getFaceGridDataArray(ss, f, S);
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

static const MeshElemMap *ccgDM_getPolyMap(Object *ob, DerivedMesh *dm)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;

	if (!ccgdm->multires.mmd && !ccgdm->pmap && ob->type == OB_MESH) {
		Mesh *me = ob->data;

		BKE_mesh_vert_poly_map_create(&ccgdm->pmap, &ccgdm->pmap_mem,
		                     me->mpoly, me->mloop,
		                     me->totvert, me->totpoly, me->totloop);
	}

	return ccgdm->pmap;
}

static int ccgDM_use_grid_pbvh(CCGDerivedMesh *ccgdm)
{
	MultiresModifierData *mmd = ccgdm->multires.mmd;

	/* both of multires and subsurf modifiers are CCG, but
	 * grids should only be used when sculpting on multires */
	if (!mmd)
		return 0;

	return 1;
}

static struct PBVH *ccgDM_getPBVH(Object *ob, DerivedMesh *dm)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
	CCGKey key;
	int numGrids;

	CCG_key_top_level(&key, ccgdm->ss);

	if (!ob) {
		ccgdm->pbvh = NULL;
		return NULL;
	}

	if (!ob->sculpt)
		return NULL;

	bool grid_pbvh = ccgDM_use_grid_pbvh(ccgdm);
	if ((ob->mode & OB_MODE_SCULPT) == 0) {
		/* In vwpaint, we may use a grid_pbvh for multires/subsurf, under certain conditions.
		 * More complex cases break 'history' trail back to original vertices, in that case we fall back to
		 * deformed cage only (i.e. original deformed mesh). */
		VirtualModifierData virtualModifierData;
		ModifierData *md = modifiers_getVirtualModifierList(ob, &virtualModifierData);

		grid_pbvh = true;
		bool has_one_ccg_modifier = false;
		for (; md; md = md->next) {
			/* We can only accept to use this ccgdm if:
			 *   - it's the only active ccgdm in the stack.
			 *   - there is no topology-modifying modifier in the stack.
			 * Otherwise, there is no way to map back to original geometry from grid-generated PBVH.
			 */
			const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

			if (!modifier_isEnabled(NULL, md, eModifierMode_Realtime)) {
				continue;
			}
			if (ELEM(mti->type, eModifierTypeType_OnlyDeform, eModifierTypeType_NonGeometrical)) {
				continue;
			}

			if (ELEM(md->type, eModifierType_Subsurf, eModifierType_Multires)) {
				if (has_one_ccg_modifier) {
					/* We only allow a single active ccg modifier in the stack. */
					grid_pbvh = false;
					break;
				}
				has_one_ccg_modifier = true;
				continue;
			}

			/* Any other non-deforming modifier makes it impossible to use grid pbvh. */
			grid_pbvh = false;
			break;
		}
	}

	if (ob->sculpt->pbvh) {
		/* Note that we have to clean up exisitng pbvh instead of updating it in case it does not match current
		 * grid_pbvh status. */
		if (grid_pbvh) {
			if (BKE_pbvh_get_ccgdm(ob->sculpt->pbvh) != NULL) {
				/* pbvh's grids, gridadj and gridfaces points to data inside ccgdm
				 * but this can be freed on ccgdm release, this updates the pointers
				 * when the ccgdm gets remade, the assumption is that the topology
				 * does not change. */
				ccgdm_create_grids(dm);
				BKE_pbvh_grids_update(ob->sculpt->pbvh, ccgdm->gridData, (void **)ccgdm->gridFaces,
				                      ccgdm->gridFlagMats, ccgdm->gridHidden);
			}
			else {
				BKE_pbvh_free(ob->sculpt->pbvh);
				ob->sculpt->pbvh = NULL;
			}
		}
		else if (BKE_pbvh_get_ccgdm(ob->sculpt->pbvh) != NULL) {
			BKE_pbvh_free(ob->sculpt->pbvh);
			ob->sculpt->pbvh = NULL;
		}

		ccgdm->pbvh = ob->sculpt->pbvh;
	}

	if (ccgdm->pbvh) {
		/* For grid pbvh, keep track of ccgdm */
		if (grid_pbvh) {
			BKE_pbvh_set_ccgdm(ccgdm->pbvh, ccgdm);
		}
		return ccgdm->pbvh;
	}

	/* No pbvh exists yet, we need to create one. only in case of multires
	 * we build a pbvh over the modified mesh, in other cases the base mesh
	 * is being sculpted, so we build a pbvh from that. */
	/* Note: vwpaint tries to always build a pbvh over the modified mesh. */
	if (grid_pbvh) {
		ccgdm_create_grids(dm);

		numGrids = ccgDM_getNumGrids(dm);

		ob->sculpt->pbvh = ccgdm->pbvh = BKE_pbvh_new();
		BKE_pbvh_build_grids(ccgdm->pbvh, ccgdm->gridData,
		                     numGrids, &key, (void **) ccgdm->gridFaces, ccgdm->gridFlagMats, ccgdm->gridHidden);
	}
	else if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		const int looptris_num = poly_to_tri_count(me->totpoly, me->totloop);
		MLoopTri *looptri;

		looptri = MEM_mallocN(sizeof(*looptri) * looptris_num, __func__);

		BKE_mesh_recalc_looptri(
		        me->mloop, me->mpoly,
		        me->mvert,
		        me->totloop, me->totpoly,
		        looptri);

		ob->sculpt->pbvh = ccgdm->pbvh = BKE_pbvh_new();
		BKE_pbvh_build_mesh(ccgdm->pbvh, me->mpoly, me->mloop, me->mvert, me->totvert, &me->vdata,
		                    looptri, looptris_num);

		if (ob->sculpt->modifiers_active && ob->derivedDeform != NULL) {
			DerivedMesh *deformdm = ob->derivedDeform;
			float (*vertCos)[3];
			int totvert;

			totvert = deformdm->getNumVerts(deformdm);
			vertCos = MEM_malloc_arrayN(totvert, sizeof(float[3]), "ccgDM_getPBVH vertCos");
			deformdm->getVertCos(deformdm, vertCos);
			BKE_pbvh_apply_vertCos(ccgdm->pbvh, vertCos);
			MEM_freeN(vertCos);
		}
	}

	if (ccgdm->pbvh != NULL) {
		pbvh_show_diffuse_color_set(ccgdm->pbvh, ob->sculpt->show_diffuse_color);
		pbvh_show_mask_set(ccgdm->pbvh, ob->sculpt->show_mask);
	}

	/* For grid pbvh, keep track of ccgdm. */
	if (grid_pbvh && ccgdm->pbvh) {
		BKE_pbvh_set_ccgdm(ccgdm->pbvh, ccgdm);
	}
	return ccgdm->pbvh;
}

static void ccgDM_recalcTessellation(DerivedMesh *UNUSED(dm))
{
	/* Nothing to do: CCG handles creating its own tessfaces */
}

/* WARNING! *MUST* be called in an 'loops_cache_rwlock' protected thread context! */
static void ccgDM_recalcLoopTri(DerivedMesh *dm)
{
	MLoopTri *mlooptri = dm->looptris.array;
	const int tottri = dm->numPolyData * 2;
	int i, poly_index;

	DM_ensure_looptri_data(dm);
	mlooptri = dm->looptris.array_wip;

	BLI_assert(tottri == 0 || mlooptri != NULL);
	BLI_assert(poly_to_tri_count(dm->numPolyData, dm->numLoopData) == dm->looptris.num);
	BLI_assert(tottri == dm->looptris.num);

	for (i = 0, poly_index = 0; i < tottri; i += 2, poly_index += 1) {
		MLoopTri *lt;
		lt = &mlooptri[i];
		/* quad is (0, 3, 2, 1) */
		lt->tri[0] = (poly_index * 4) + 0;
		lt->tri[1] = (poly_index * 4) + 2;
		lt->tri[2] = (poly_index * 4) + 3;
		lt->poly = poly_index;

		lt = &mlooptri[i + 1];
		lt->tri[0] = (poly_index * 4) + 0;
		lt->tri[1] = (poly_index * 4) + 1;
		lt->tri[2] = (poly_index * 4) + 2;
		lt->poly = poly_index;
	}

	BLI_assert(dm->looptris.array == NULL);
	atomic_cas_ptr((void **)&dm->looptris.array, dm->looptris.array, dm->looptris.array_wip);
	dm->looptris.array_wip = NULL;
}

static void ccgDM_calcNormals(DerivedMesh *dm)
{
	/* Nothing to do: CCG calculates normals during drawing */
	dm->dirty &= ~DM_DIRTY_NORMALS;
}

static void set_default_ccgdm_callbacks(CCGDerivedMesh *ccgdm)
{
	ccgdm->dm.getMinMax = ccgDM_getMinMax;
	ccgdm->dm.getNumVerts = ccgDM_getNumVerts;
	ccgdm->dm.getNumEdges = ccgDM_getNumEdges;
	ccgdm->dm.getNumLoops = ccgDM_getNumLoops;
	/* reuse of ccgDM_getNumTessFaces is intentional here: subsurf polys are just created from tessfaces */
	ccgdm->dm.getNumPolys = ccgDM_getNumPolys;
	ccgdm->dm.getNumTessFaces = ccgDM_getNumTessFaces;

	ccgdm->dm.getVert = ccgDM_getFinalVert;
	ccgdm->dm.getEdge = ccgDM_getFinalEdge;
	ccgdm->dm.getTessFace = ccgDM_getFinalFace;

	ccgdm->dm.getVertCo = ccgDM_getFinalVertCo;
	ccgdm->dm.getVertNo = ccgDM_getFinalVertNo;

	ccgdm->dm.copyVertArray = ccgDM_copyFinalVertArray;
	ccgdm->dm.copyEdgeArray = ccgDM_copyFinalEdgeArray;
	ccgdm->dm.copyTessFaceArray = ccgDM_copyFinalFaceArray;
	ccgdm->dm.copyLoopArray = ccgDM_copyFinalLoopArray;
	ccgdm->dm.copyPolyArray = ccgDM_copyFinalPolyArray;

	ccgdm->dm.getVertData = ccgDM_get_vert_data;
	ccgdm->dm.getEdgeData = ccgDM_get_edge_data;
	ccgdm->dm.getTessFaceData = ccgDM_get_tessface_data;
	ccgdm->dm.getPolyData = ccgDM_get_poly_data;
	ccgdm->dm.getVertDataArray = ccgDM_get_vert_data_layer;
	ccgdm->dm.getEdgeDataArray = ccgDM_get_edge_data_layer;
	ccgdm->dm.getTessFaceDataArray = ccgDM_get_tessface_data_layer;
	ccgdm->dm.getPolyDataArray = ccgDM_get_poly_data_layer;
	ccgdm->dm.getNumGrids = ccgDM_getNumGrids;
	ccgdm->dm.getGridSize = ccgDM_getGridSize;
	ccgdm->dm.getGridData = ccgDM_getGridData;
	ccgdm->dm.getGridOffset = ccgDM_getGridOffset;
	ccgdm->dm.getGridKey = ccgDM_getGridKey;
	ccgdm->dm.getGridFlagMats = ccgDM_getGridFlagMats;
	ccgdm->dm.getGridHidden = ccgDM_getGridHidden;
	ccgdm->dm.getPolyMap = ccgDM_getPolyMap;
	ccgdm->dm.getPBVH = ccgDM_getPBVH;

	ccgdm->dm.calcNormals = ccgDM_calcNormals;
	ccgdm->dm.calcLoopNormals = CDDM_calc_loop_normals;
	ccgdm->dm.calcLoopNormalsSpaceArray = CDDM_calc_loop_normals_spacearr;
	ccgdm->dm.calcLoopTangents = DM_calc_loop_tangents;
	ccgdm->dm.recalcTessellation = ccgDM_recalcTessellation;
	ccgdm->dm.recalcLoopTri = ccgDM_recalcLoopTri;

	ccgdm->dm.getVertCos = ccgdm_getVertCos;
	ccgdm->dm.foreachMappedVert = ccgDM_foreachMappedVert;
	ccgdm->dm.foreachMappedEdge = ccgDM_foreachMappedEdge;
	ccgdm->dm.foreachMappedLoop = ccgDM_foreachMappedLoop;
	ccgdm->dm.foreachMappedFaceCenter = ccgDM_foreachMappedFaceCenter;

	ccgdm->dm.drawVerts = ccgDM_drawVerts;
	ccgdm->dm.drawEdges = ccgDM_drawEdges;
	ccgdm->dm.drawLooseEdges = ccgDM_drawLooseEdges;
	ccgdm->dm.drawFacesSolid = ccgDM_drawFacesSolid;
	ccgdm->dm.drawFacesTex = ccgDM_drawFacesTex;
	ccgdm->dm.drawFacesGLSL = ccgDM_drawFacesGLSL;
	ccgdm->dm.drawMappedFaces = ccgDM_drawMappedFaces;
	ccgdm->dm.drawMappedFacesTex = ccgDM_drawMappedFacesTex;
	ccgdm->dm.drawMappedFacesGLSL = ccgDM_drawMappedFacesGLSL;
	ccgdm->dm.drawMappedFacesMat = ccgDM_drawMappedFacesMat;
	ccgdm->dm.drawUVEdges = ccgDM_drawUVEdges;

	ccgdm->dm.drawMappedEdgesInterp = ccgDM_drawMappedEdgesInterp;
	ccgdm->dm.drawMappedEdges = ccgDM_drawMappedEdges;
	ccgdm->dm.gpuObjectNew = ccgDM_GPUObjectNew;
	ccgdm->dm.copy_gpu_data = ccgDM_copy_gpu_data;

	ccgdm->dm.release = ccgDM_release;
}

static void create_ccgdm_maps(CCGDerivedMesh *ccgdm,
                              CCGSubSurf *ss)
{
	CCGVertIterator vi;
	CCGEdgeIterator ei;
	CCGFaceIterator fi;
	int totvert, totedge, totface;

	totvert = ccgSubSurf_getNumVerts(ss);
	ccgdm->vertMap = MEM_mallocN(totvert * sizeof(*ccgdm->vertMap), "vertMap");
	for (ccgSubSurf_initVertIterator(ss, &vi);
	     !ccgVertIterator_isStopped(&vi);
	     ccgVertIterator_next(&vi))
	{
		CCGVert *v = ccgVertIterator_getCurrent(&vi);

		ccgdm->vertMap[GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(v))].vert = v;
	}

	totedge = ccgSubSurf_getNumEdges(ss);
	ccgdm->edgeMap = MEM_mallocN(totedge * sizeof(*ccgdm->edgeMap), "edgeMap");
	for (ccgSubSurf_initEdgeIterator(ss, &ei);
	     !ccgEdgeIterator_isStopped(&ei);
	     ccgEdgeIterator_next(&ei))
	{
		CCGEdge *e = ccgEdgeIterator_getCurrent(&ei);

		ccgdm->edgeMap[GET_INT_FROM_POINTER(ccgSubSurf_getEdgeEdgeHandle(e))].edge = e;
	}

	totface = ccgSubSurf_getNumFaces(ss);
	ccgdm->faceMap = MEM_mallocN(totface * sizeof(*ccgdm->faceMap), "faceMap");
	for (ccgSubSurf_initFaceIterator(ss, &fi);
	     !ccgFaceIterator_isStopped(&fi);
	     ccgFaceIterator_next(&fi))
	{
		CCGFace *f = ccgFaceIterator_getCurrent(&fi);

		ccgdm->faceMap[GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(f))].face = f;
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
	int *vertOrigIndex, *faceOrigIndex, *polyOrigIndex, *base_polyOrigIndex, *edgeOrigIndex;
	short *edgeFlags = ccgdm->edgeFlags;
	DMFlagMat *faceFlags = ccgdm->faceFlags;
	int *polyidx = NULL;
#ifndef USE_DYNSIZE
	int *loopidx = NULL, *vertidx = NULL;
	BLI_array_declare(loopidx);
	BLI_array_declare(vertidx);
#endif
	int loopindex, loopindex2;
	int edgeSize;
	int gridSize;
	int gridFaces, gridCuts;
	int gridSideEdges;
	int gridInternalEdges;
	WeightTable wtable = {NULL};
	MEdge *medge = NULL;
	MPoly *mpoly = NULL;
	bool has_edge_cd;

	edgeSize = ccgSubSurf_getEdgeSize(ss);
	gridSize = ccgSubSurf_getGridSize(ss);
	gridFaces = gridSize - 1;
	gridCuts = gridSize - 2;
	/*gridInternalVerts = gridSideVerts * gridSideVerts; - as yet, unused */
	gridSideEdges = gridSize - 1;
	gridInternalEdges = (gridSideEdges - 1) * gridSideEdges * 2;

	/* mvert = dm->getVertArray(dm); */ /* UNUSED */
	medge = dm->getEdgeArray(dm);
	/* mface = dm->getTessFaceArray(dm); */ /* UNUSED */

	mpoly = CustomData_get_layer(&dm->polyData, CD_MPOLY);
	base_polyOrigIndex = CustomData_get_layer(&dm->polyData, CD_ORIGINDEX);

	vertOrigIndex = DM_get_vert_data_layer(&ccgdm->dm, CD_ORIGINDEX);
	edgeOrigIndex = DM_get_edge_data_layer(&ccgdm->dm, CD_ORIGINDEX);

	faceOrigIndex = DM_get_tessface_data_layer(&ccgdm->dm, CD_ORIGINDEX);
	polyOrigIndex = DM_get_poly_data_layer(&ccgdm->dm, CD_ORIGINDEX);

	has_edge_cd = ((ccgdm->dm.edgeData.totlayer - (edgeOrigIndex ? 1 : 0)) != 0);

#if 0
	/* this is not in trunk, can gives problems because colors initialize
	 * as black, just don't do it!, it works fine - campbell */
	if (!CustomData_has_layer(&ccgdm->dm.faceData, CD_MCOL))
		DM_add_tessface_layer(&ccgdm->dm, CD_MCOL, CD_CALLOC, NULL);
	mcol = DM_get_tessface_data_layer(&ccgdm->dm, CD_MCOL);
#endif

	loopindex = loopindex2 = 0; /* current loop index */
	for (index = 0; index < totface; index++) {
		CCGFace *f = ccgdm->faceMap[index].face;
		int numVerts = ccgSubSurf_getFaceNumVerts(f);
		int numFinalEdges = numVerts * (gridSideEdges + gridInternalEdges);
		int origIndex = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(f));
		int g2_wid = gridCuts + 2;
		float *w, *w2;
		int s, x, y;
#ifdef USE_DYNSIZE
		int loopidx[numVerts], vertidx[numVerts];
#endif
		w = get_ss_weights(&wtable, gridCuts, numVerts);

		ccgdm->faceMap[index].startVert = vertNum;
		ccgdm->faceMap[index].startEdge = edgeNum;
		ccgdm->faceMap[index].startFace = faceNum;

		faceFlags->flag = mpoly ?  mpoly[origIndex].flag : 0;
		faceFlags->mat_nr = mpoly ? mpoly[origIndex].mat_nr : 0;
		faceFlags++;

		/* set the face base vert */
		*((int *)ccgSubSurf_getFaceUserData(ss, f)) = vertNum;

#ifndef USE_DYNSIZE
		BLI_array_clear(loopidx);
		BLI_array_grow_items(loopidx, numVerts);
#endif
		for (s = 0; s < numVerts; s++) {
			loopidx[s] = loopindex++;
		}

#ifndef USE_DYNSIZE
		BLI_array_clear(vertidx);
		BLI_array_grow_items(vertidx, numVerts);
#endif
		for (s = 0; s < numVerts; s++) {
			CCGVert *v = ccgSubSurf_getFaceVert(f, s);
			vertidx[s] = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(v));
		}

		/*I think this is for interpolating the center vert?*/
		w2 = w; // + numVerts*(g2_wid-1) * (g2_wid-1); //numVerts*((g2_wid-1) * g2_wid+g2_wid-1);
		DM_interp_vert_data(dm, &ccgdm->dm, vertidx, w2,
		                    numVerts, vertNum);
		if (vertOrigIndex) {
			*vertOrigIndex = ORIGINDEX_NONE;
			vertOrigIndex++;
		}

		vertNum++;

		/*interpolate per-vert data*/
		for (s = 0; s < numVerts; s++) {
			for (x = 1; x < gridFaces; x++) {
				w2 = w + s * numVerts * g2_wid * g2_wid + x * numVerts;
				DM_interp_vert_data(dm, &ccgdm->dm, vertidx, w2,
				                    numVerts, vertNum);

				if (vertOrigIndex) {
					*vertOrigIndex = ORIGINDEX_NONE;
					vertOrigIndex++;
				}

				vertNum++;
			}
		}

		/*interpolate per-vert data*/
		for (s = 0; s < numVerts; s++) {
			for (y = 1; y < gridFaces; y++) {
				for (x = 1; x < gridFaces; x++) {
					w2 = w + s * numVerts * g2_wid * g2_wid + (y * g2_wid + x) * numVerts;
					DM_interp_vert_data(dm, &ccgdm->dm, vertidx, w2,
					                    numVerts, vertNum);

					if (vertOrigIndex) {
						*vertOrigIndex = ORIGINDEX_NONE;
						vertOrigIndex++;
					}

					vertNum++;
				}
			}
		}

		if (edgeOrigIndex) {
			for (i = 0; i < numFinalEdges; ++i) {
				edgeOrigIndex[edgeNum + i] = ORIGINDEX_NONE;
			}
		}

		for (s = 0; s < numVerts; s++) {
			/*interpolate per-face data*/
			for (y = 0; y < gridFaces; y++) {
				for (x = 0; x < gridFaces; x++) {
					w2 = w + s * numVerts * g2_wid * g2_wid + (y * g2_wid + x) * numVerts;
					CustomData_interp(&dm->loopData, &ccgdm->dm.loopData,
					                  loopidx, w2, NULL, numVerts, loopindex2);
					loopindex2++;

					w2 = w + s * numVerts * g2_wid * g2_wid + ((y + 1) * g2_wid + (x)) * numVerts;
					CustomData_interp(&dm->loopData, &ccgdm->dm.loopData,
					                  loopidx, w2, NULL, numVerts, loopindex2);
					loopindex2++;

					w2 = w + s * numVerts * g2_wid * g2_wid + ((y + 1) * g2_wid + (x + 1)) * numVerts;
					CustomData_interp(&dm->loopData, &ccgdm->dm.loopData,
					                  loopidx, w2, NULL, numVerts, loopindex2);
					loopindex2++;

					w2 = w + s * numVerts * g2_wid * g2_wid + ((y) * g2_wid + (x + 1)) * numVerts;
					CustomData_interp(&dm->loopData, &ccgdm->dm.loopData,
					                  loopidx, w2, NULL, numVerts, loopindex2);
					loopindex2++;

					/*copy over poly data, e.g. mtexpoly*/
					CustomData_copy_data(&dm->polyData, &ccgdm->dm.polyData, origIndex, faceNum, 1);

					/*set original index data*/
					if (faceOrigIndex) {
						/* reference the index in 'polyOrigIndex' */
						*faceOrigIndex = faceNum;
						faceOrigIndex++;
					}
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

	for (index = 0; index < totedge; ++index) {
		CCGEdge *e = ccgdm->edgeMap[index].edge;
		int numFinalEdges = edgeSize - 1;
		int mapIndex = ccgDM_getEdgeMapIndex(ss, e);
		int x;
		int vertIdx[2];
		int edgeIdx = GET_INT_FROM_POINTER(ccgSubSurf_getEdgeEdgeHandle(e));

		CCGVert *v;
		v = ccgSubSurf_getEdgeVert0(e);
		vertIdx[0] = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(v));
		v = ccgSubSurf_getEdgeVert1(e);
		vertIdx[1] = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(v));

		ccgdm->edgeMap[index].startVert = vertNum;
		ccgdm->edgeMap[index].startEdge = edgeNum;

		if (edgeIdx >= 0 && edgeFlags)
			edgeFlags[edgeIdx] = medge[edgeIdx].flag;

		/* set the edge base vert */
		*((int *)ccgSubSurf_getEdgeUserData(ss, e)) = vertNum;

		for (x = 1; x < edgeSize - 1; x++) {
			float w[2];
			w[1] = (float) x / (edgeSize - 1);
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
			for (i = 0; i < numFinalEdges; ++i) {
				CustomData_copy_data(&dm->edgeData, &ccgdm->dm.edgeData, edgeIdx, edgeNum + i, 1);
			}
		}

		if (edgeOrigIndex) {
			for (i = 0; i < numFinalEdges; ++i) {
				edgeOrigIndex[edgeNum + i] = mapIndex;
			}
		}

		edgeNum += numFinalEdges;
	}

	if (useSubsurfUv) {
		CustomData *ldata = &ccgdm->dm.loopData;
		CustomData *dmldata = &dm->loopData;
		int numlayer = CustomData_number_of_layers(ldata, CD_MLOOPUV);
		int dmnumlayer = CustomData_number_of_layers(dmldata, CD_MLOOPUV);

		for (i = 0; i < numlayer && i < dmnumlayer; i++)
			set_subsurf_uv(ss, dm, &ccgdm->dm, i);
	}

	for (index = 0; index < totvert; ++index) {
		CCGVert *v = ccgdm->vertMap[index].vert;
		int mapIndex = ccgDM_getVertMapIndex(ccgdm->ss, v);
		int vertIdx;

		vertIdx = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(v));

		ccgdm->vertMap[index].startVert = vertNum;

		/* set the vert base vert */
		*((int *) ccgSubSurf_getVertUserData(ss, v)) = vertNum;

		DM_copy_vert_data(dm, &ccgdm->dm, vertIdx, vertNum, 1);

		if (vertOrigIndex) {
			*vertOrigIndex = mapIndex;
			vertOrigIndex++;
		}
		vertNum++;
	}

#ifndef USE_DYNSIZE
	BLI_array_free(vertidx);
	BLI_array_free(loopidx);
#endif
	free_ss_weights(&wtable);

	BLI_assert(vertNum == ccgSubSurf_getNumFinalVerts(ss));
	BLI_assert(edgeNum == ccgSubSurf_getNumFinalEdges(ss));
	BLI_assert(loopindex2 == ccgSubSurf_getNumFinalFaces(ss) * 4);
	BLI_assert(faceNum == ccgSubSurf_getNumFinalFaces(ss));

}

/* Fill in only geometry arrays needed for the GPU tessellation. */
static void set_ccgdm_gpu_geometry(CCGDerivedMesh *ccgdm, DerivedMesh *dm)
{
	const int totface = dm->getNumPolys(dm);
	MPoly *mpoly = CustomData_get_layer(&dm->polyData, CD_MPOLY);
	int index;
	DMFlagMat *faceFlags = ccgdm->faceFlags;

	for (index = 0; index < totface; index++) {
		faceFlags->flag = mpoly ?  mpoly[index].flag : 0;
		faceFlags->mat_nr = mpoly ? mpoly[index].mat_nr : 0;
		faceFlags++;
	}

	/* TODO(sergey): Fill in edge flags. */
}

static CCGDerivedMesh *getCCGDerivedMesh(CCGSubSurf *ss,
                                         int drawInteriorEdges,
                                         int useSubsurfUv,
                                         DerivedMesh *dm,
                                         bool use_gpu_backend)
{
#ifdef WITH_OPENSUBDIV
	const int totedge = dm->getNumEdges(dm);
	const int totface = dm->getNumPolys(dm);
#else
	const int totedge = ccgSubSurf_getNumEdges(ss);
	const int totface = ccgSubSurf_getNumFaces(ss);
#endif
	CCGDerivedMesh *ccgdm = MEM_callocN(sizeof(*ccgdm), "ccgdm");

	if (use_gpu_backend == false) {
		BLI_assert(totedge == ccgSubSurf_getNumEdges(ss));
		BLI_assert(totface == ccgSubSurf_getNumFaces(ss));
		DM_from_template(&ccgdm->dm, dm, DM_TYPE_CCGDM,
		                 ccgSubSurf_getNumFinalVerts(ss),
		                 ccgSubSurf_getNumFinalEdges(ss),
		                 0,
		                 ccgSubSurf_getNumFinalFaces(ss) * 4,
		                 ccgSubSurf_getNumFinalFaces(ss));

		CustomData_free_layer_active(&ccgdm->dm.polyData, CD_NORMAL,
		                             ccgdm->dm.numPolyData);

		ccgdm->reverseFaceMap =
			MEM_callocN(sizeof(int) * ccgSubSurf_getNumFinalFaces(ss),
			            "reverseFaceMap");

		create_ccgdm_maps(ccgdm, ss);
	}
	else {
		DM_from_template(&ccgdm->dm, dm, DM_TYPE_CCGDM,
		                 0, 0, 0, 0, dm->getNumPolys(dm));
		CustomData_copy_data(&dm->polyData,
		                     &ccgdm->dm.polyData,
		                     0, 0, dm->getNumPolys(dm));
	}

	set_default_ccgdm_callbacks(ccgdm);

	ccgdm->ss = ss;
	ccgdm->drawInteriorEdges = drawInteriorEdges;
	ccgdm->useSubsurfUv = useSubsurfUv;
	ccgdm->useGpuBackend = use_gpu_backend;

	/* CDDM hack. */
	ccgdm->edgeFlags = MEM_callocN(sizeof(short) * totedge, "edgeFlags");
	ccgdm->faceFlags = MEM_callocN(sizeof(DMFlagMat) * totface, "faceFlags");

	if (use_gpu_backend == false) {
		set_ccgdm_all_geometry(ccgdm, ss, dm, useSubsurfUv != 0);
	}
	else {
		set_ccgdm_gpu_geometry(ccgdm, dm);
	}

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

static bool subsurf_use_gpu_backend(SubsurfFlags flags)
{
#ifdef WITH_OPENSUBDIV
	/* Use GPU backend if it's a last modifier in the stack
	 * and user choosed to use any of the OSD compute devices,
	 * but also check if GPU has all needed features.
	 */
	return
	        (flags & SUBSURF_USE_GPU_BACKEND) != 0 &&
	        (U.opensubdiv_compute_type != USER_OPENSUBDIV_COMPUTE_NONE) &&
	        (openSubdiv_supportGPUDisplay());
#else
	(void)flags;
	return false;
#endif
}

struct DerivedMesh *subsurf_make_derived_from_derived(
        struct DerivedMesh *dm,
        struct SubsurfModifierData *smd,
        float (*vertCos)[3],
        SubsurfFlags flags)
{
	int useSimple = (smd->subdivType == ME_SIMPLE_SUBSURF) ? CCG_SIMPLE_SUBDIV : 0;
	CCGFlags useAging = (smd->flags & eSubsurfModifierFlag_DebugIncr) ? CCG_USE_AGING : 0;
	int useSubsurfUv = smd->flags & eSubsurfModifierFlag_SubsurfUv;
	int drawInteriorEdges = !(smd->flags & eSubsurfModifierFlag_ControlEdges);
	CCGDerivedMesh *result;
	bool use_gpu_backend = subsurf_use_gpu_backend(flags);

	/* note: editmode calculation can only run once per
	 * modifier stack evaluation (uses freed cache) [#36299] */
	if (flags & SUBSURF_FOR_EDIT_MODE) {
		int levels = (smd->modifier.scene) ? get_render_subsurf_level(&smd->modifier.scene->r, smd->levels, false) : smd->levels;

		/* TODO(sergey): Same as emCache below. */
		if ((flags & SUBSURF_IN_EDIT_MODE) && smd->mCache) {
			ccgSubSurf_free(smd->mCache);
			smd->mCache = NULL;
		}

		smd->emCache = _getSubSurf(smd->emCache, levels, 3, useSimple | useAging | CCG_CALC_NORMALS);

#ifdef WITH_OPENSUBDIV
		ccgSubSurf_setSkipGrids(smd->emCache, use_gpu_backend);
#endif
		ss_sync_from_derivedmesh(smd->emCache, dm, vertCos, useSimple, useSubsurfUv);
		result = getCCGDerivedMesh(smd->emCache,
		                           drawInteriorEdges,
		                           useSubsurfUv, dm, use_gpu_backend);
	}
	else if (flags & SUBSURF_USE_RENDER_PARAMS) {
		/* Do not use cache in render mode. */
		CCGSubSurf *ss;
		int levels = (smd->modifier.scene) ? get_render_subsurf_level(&smd->modifier.scene->r, smd->renderLevels, true) : smd->renderLevels;

		if (levels == 0)
			return dm;

		ss = _getSubSurf(NULL, levels, 3, useSimple | CCG_USE_ARENA | CCG_CALC_NORMALS);

		ss_sync_from_derivedmesh(ss, dm, vertCos, useSimple, useSubsurfUv);

		result = getCCGDerivedMesh(ss,
		                           drawInteriorEdges, useSubsurfUv, dm, false);

		result->freeSS = 1;
	}
	else {
		int useIncremental = (smd->flags & eSubsurfModifierFlag_Incremental);
		int levels = (smd->modifier.scene) ? get_render_subsurf_level(&smd->modifier.scene->r, smd->levels, false) : smd->levels;
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
			ccgSubSurf_free(smd->emCache);
			smd->emCache = NULL;
		}

		if (useIncremental && (flags & SUBSURF_IS_FINAL_CALC)) {
			smd->mCache = ss = _getSubSurf(smd->mCache, levels, 3, useSimple | useAging | CCG_CALC_NORMALS);

			ss_sync_from_derivedmesh(ss, dm, vertCos, useSimple, useSubsurfUv);

			result = getCCGDerivedMesh(smd->mCache,
			                           drawInteriorEdges,
			                           useSubsurfUv, dm, false);
		}
		else {
			CCGFlags ccg_flags = useSimple | CCG_USE_ARENA | CCG_CALC_NORMALS;
			CCGSubSurf *prevSS = NULL;

			if (smd->mCache && (flags & SUBSURF_IS_FINAL_CALC)) {
#ifdef WITH_OPENSUBDIV
				/* With OpenSubdiv enabled we always tries to re-use previos
				 * subsurf structure in order to save computation time since
				 * re-creation is rather a complicated business.
				 *
				 * TODO(sergey): There was a good eason why final calculation
				 * used to free entirely cached subsurf structure. reason of
				 * this is to be investigated still to be sure we don't have
				 * regressions here.
				 */
				if (use_gpu_backend) {
					prevSS = smd->mCache;
				}
				else
#endif
				{
					ccgSubSurf_free(smd->mCache);
					smd->mCache = NULL;
				}
			}


			if (flags & SUBSURF_ALLOC_PAINT_MASK)
				ccg_flags |= CCG_ALLOC_MASK;

			ss = _getSubSurf(prevSS, levels, 3, ccg_flags);
#ifdef WITH_OPENSUBDIV
			ccgSubSurf_setSkipGrids(ss, use_gpu_backend);
#endif
			ss_sync_from_derivedmesh(ss, dm, vertCos, useSimple, useSubsurfUv);

			result = getCCGDerivedMesh(ss, drawInteriorEdges, useSubsurfUv, dm, use_gpu_backend);

			if (flags & SUBSURF_IS_FINAL_CALC)
				smd->mCache = ss;
			else
				result->freeSS = 1;

			if (flags & SUBSURF_ALLOC_PAINT_MASK)
				ccgSubSurf_setNumLayers(ss, 4);
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
	CCGSubSurf *ss = _getSubSurf(NULL, 1, 3, CCG_USE_ARENA);
	float edge_sum[3], face_sum[3];
	CCGVertIterator vi;
	DerivedMesh *dm = CDDM_from_mesh(me);

	ss_sync_from_derivedmesh(ss, dm, NULL, 0, 0);

	for (ccgSubSurf_initVertIterator(ss, &vi); !ccgVertIterator_isStopped(&vi); ccgVertIterator_next(&vi)) {
		CCGVert *v = ccgVertIterator_getCurrent(&vi);
		int idx = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(v));
		int N = ccgSubSurf_getVertNumEdges(v);
		int numFaces = ccgSubSurf_getVertNumFaces(v);
		float *co;
		int i;

		zero_v3(edge_sum);
		zero_v3(face_sum);

		for (i = 0; i < N; i++) {
			CCGEdge *e = ccgSubSurf_getVertEdge(v, i);
			add_v3_v3v3(edge_sum, edge_sum, ccgSubSurf_getEdgeData(ss, e, 1));
		}
		for (i = 0; i < numFaces; i++) {
			CCGFace *f = ccgSubSurf_getVertFace(v, i);
			add_v3_v3(face_sum, ccgSubSurf_getFaceCenterData(f));
		}

		/* ad-hoc correction for boundary vertices, to at least avoid them
		 * moving completely out of place (brecht) */
		if (numFaces && numFaces != N)
			mul_v3_fl(face_sum, (float)N / (float)numFaces);

		co = ccgSubSurf_getVertData(ss, v);
		r_positions[idx][0] = (co[0] * N * N + edge_sum[0] * 4 + face_sum[0]) / (N * (N + 5));
		r_positions[idx][1] = (co[1] * N * N + edge_sum[1] * 4 + face_sum[1]) / (N * (N + 5));
		r_positions[idx][2] = (co[2] * N * N + edge_sum[2] * 4 + face_sum[2]) / (N * (N + 5));
	}

	ccgSubSurf_free(ss);

	dm->release(dm);
}

bool subsurf_has_edges(DerivedMesh *dm)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
#ifdef WITH_OPENSUBDIV
	if (ccgdm->useGpuBackend) {
		return true;
	}
#else
	(void)ccgdm;
#endif
	return dm->getNumEdges(dm) != 0;
}

bool subsurf_has_faces(DerivedMesh *dm)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
#ifdef WITH_OPENSUBDIV
	if (ccgdm->useGpuBackend) {
		return true;
	}
#else
	(void)ccgdm;
#endif
	return dm->getNumPolys(dm) != 0;
}
