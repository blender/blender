/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_bad_level_calls.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_scene.h"
#include "BKE_subsurf.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_arithb.h"
#include "BLI_linklist.h"
#include "BLI_memarena.h"
#include "BLI_edgehash.h"

#include "BIF_gl.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_material.h"

#include "CCGSubSurf.h"

typedef struct _VertData {
	float co[3];
	float no[3];
} VertData;

struct CCGDerivedMesh {
	DerivedMesh dm;

	CCGSubSurf *ss;
	int drawInteriorEdges, useSubsurfUv;

	struct {int startVert; CCGVert *vert;} *vertMap;
	struct {int startVert; int startEdge; CCGEdge *edge;} *edgeMap;
	struct {int startVert; int startEdge;
	        int startFace; CCGFace *face;} *faceMap;
};

typedef struct CCGDerivedMesh CCGDerivedMesh;

static int ccgDM_getVertMapIndex(CCGDerivedMesh *ccgdm, CCGSubSurf *ss, CCGVert *v);
static int ccgDM_getEdgeMapIndex(CCGDerivedMesh *ccgdm, CCGSubSurf *ss, CCGEdge *e);
static int ccgDM_getFaceMapIndex(CCGDerivedMesh *ccgdm, CCGSubSurf *ss, CCGFace *f);

///

static void *arena_alloc(CCGAllocatorHDL a, int numBytes) {
	return BLI_memarena_alloc(a, numBytes);
}
static void *arena_realloc(CCGAllocatorHDL a, void *ptr, int newSize, int oldSize) {
	void *p2 = BLI_memarena_alloc(a, newSize);
	if (ptr) {
		memcpy(p2, ptr, oldSize);
	}
	return p2;
}
static void arena_free(CCGAllocatorHDL a, void *ptr) {
}
static void arena_release(CCGAllocatorHDL a) {
	BLI_memarena_free(a);
}

static CCGSubSurf *_getSubSurf(CCGSubSurf *prevSS, int subdivLevels, int useAging, int useArena, int useFlatSubdiv) {
	CCGMeshIFC ifc;
	CCGSubSurf *ccgSS;

		/* subdivLevels==0 is not allowed */
	subdivLevels = MAX2(subdivLevels, 1);

	if (prevSS) {
		int oldUseAging;

		useAging = !!useAging;
		ccgSubSurf_getUseAgeCounts(prevSS, &oldUseAging, NULL, NULL, NULL);

		if (oldUseAging!=useAging) {
			ccgSubSurf_free(prevSS);
		} else {
			ccgSubSurf_setSubdivisionLevels(prevSS, subdivLevels);

			return prevSS;
		}
	}

	if (useAging) {
		ifc.vertUserSize = ifc.edgeUserSize = ifc.faceUserSize = 12;
	} else {
		ifc.vertUserSize = ifc.edgeUserSize = ifc.faceUserSize = 8;
	}
	ifc.vertDataSize = sizeof(VertData);

	if (useArena) {
		CCGAllocatorIFC allocatorIFC;
		CCGAllocatorHDL allocator = BLI_memarena_new((1<<16));

		allocatorIFC.alloc = arena_alloc;
		allocatorIFC.realloc = arena_realloc;
		allocatorIFC.free = arena_free;
		allocatorIFC.release = arena_release;

		ccgSS = ccgSubSurf_new(&ifc, subdivLevels, &allocatorIFC, allocator);
	} else {
		ccgSS = ccgSubSurf_new(&ifc, subdivLevels, NULL, NULL);
	}

	if (useAging) {
		ccgSubSurf_setUseAgeCounts(ccgSS, 1, 8, 8, 8);
	}

	ccgSubSurf_setCalcVertexNormals(ccgSS, 1, BLI_STRUCT_OFFSET(VertData, no));

	return ccgSS;
}

static int getEdgeIndex(CCGSubSurf *ss, CCGEdge *e, int x, int edgeSize) {
	CCGVert *v0 = ccgSubSurf_getEdgeVert0(ss, e);
	CCGVert *v1 = ccgSubSurf_getEdgeVert1(ss, e);
	int v0idx = *((int*) ccgSubSurf_getVertUserData(ss, v0));
	int v1idx = *((int*) ccgSubSurf_getVertUserData(ss, v1));
	int edgeBase = *((int*) ccgSubSurf_getEdgeUserData(ss, e));

	if (x==0) {
		return v0idx;
	} else if (x==edgeSize-1) {
		return v1idx;
	} else {
		return edgeBase + x-1;
	}
}
static int getFaceIndex(CCGSubSurf *ss, CCGFace *f, int S, int x, int y, int edgeSize, int gridSize) {
	int faceBase = *((int*) ccgSubSurf_getFaceUserData(ss, f));
	int numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

	if (x==gridSize-1 && y==gridSize-1) {
		CCGVert *v = ccgSubSurf_getFaceVert(ss, f, S);
		return *((int*) ccgSubSurf_getVertUserData(ss, v));
	} else if (x==gridSize-1) {
		CCGVert *v = ccgSubSurf_getFaceVert(ss, f, S);
		CCGEdge *e = ccgSubSurf_getFaceEdge(ss, f, S);
		int edgeBase = *((int*) ccgSubSurf_getEdgeUserData(ss, e));
		if (v==ccgSubSurf_getEdgeVert0(ss, e)) {
			return edgeBase + (gridSize-1-y)-1;
		} else {
			return edgeBase + (edgeSize-2-1)-((gridSize-1-y)-1);
		}
	} else if (y==gridSize-1) {
		CCGVert *v = ccgSubSurf_getFaceVert(ss, f, S);
		CCGEdge *e = ccgSubSurf_getFaceEdge(ss, f, (S+numVerts-1)%numVerts);
		int edgeBase = *((int*) ccgSubSurf_getEdgeUserData(ss, e));
		if (v==ccgSubSurf_getEdgeVert0(ss, e)) {
			return edgeBase + (gridSize-1-x)-1;
		} else {
			return edgeBase + (edgeSize-2-1)-((gridSize-1-x)-1);
		}
	} else if (x==0 && y==0) {
		return faceBase;
	} else if (x==0) {
		S = (S+numVerts-1)%numVerts;
		return faceBase + 1 + (gridSize-2)*S + (y-1);
	} else if (y==0) {
		return faceBase + 1 + (gridSize-2)*S + (x-1);
	} else {
		return faceBase + 1 + (gridSize-2)*numVerts + S*(gridSize-2)*(gridSize-2) + (y-1)*(gridSize-2) + (x-1);
	}
}

static void get_face_uv_map_vert(UvVertMap *vmap, struct MFace *mf, int fi, CCGVertHDL *fverts) {
	unsigned int *fv = &mf->v1;
	UvMapVert *v, *nv;
	int j, nverts= mf->v4? 4: 3;

	for (j=0; j<nverts; j++, fv++) {
		for (nv=v=get_uv_map_vert(vmap, *fv); v; v=v->next) {
			if (v->separate)
				nv= v;
			if (v->f == fi)
				break;
		}

		fverts[j]= SET_INT_IN_POINTER(nv->f*4 + nv->tfindex);
	}
}

static int ss_sync_from_uv(CCGSubSurf *ss, CCGSubSurf *origss, DerivedMesh *dm, MTFace *tface) {
	MFace *mface = dm->getFaceArray(dm);
	MVert *mvert = dm->getVertArray(dm);
	int totvert = dm->getNumVerts(dm);
	int totface = dm->getNumFaces(dm);
	int i, j, seam;
	UvMapVert *v;
	UvVertMap *vmap;
	float limit[2];
	CCGVertHDL fverts[4];
	EdgeHash *ehash;
	float creaseFactor = (float)ccgSubSurf_getSubdivisionLevels(ss);

	limit[0]= limit[1]= STD_UV_CONNECT_LIMIT;
	vmap= make_uv_vert_map(mface, tface, totface, totvert, 0, limit);
	if (!vmap)
		return 0;
	
	ccgSubSurf_initFullSync(ss);

	/* create vertices */
	for (i=0; i<totvert; i++) {
		if (!get_uv_map_vert(vmap, i))
			continue;

		for (v=get_uv_map_vert(vmap, i)->next; v; v=v->next)
			if (v->separate)
				break;

		seam = (v != NULL) || ((mvert+i)->flag & ME_VERT_MERGED);

		for (v=get_uv_map_vert(vmap, i); v; v=v->next) {
			if (v->separate) {
				CCGVert *ssv;
				CCGVertHDL vhdl = SET_INT_IN_POINTER(v->f*4 + v->tfindex);
				float uv[3];

				uv[0]= (tface+v->f)->uv[v->tfindex][0];
				uv[1]= (tface+v->f)->uv[v->tfindex][1];
				uv[2]= 0.0f;

				ccgSubSurf_syncVert(ss, vhdl, uv, seam, &ssv);
			}
		}
	}

	/* create edges */
	ehash = BLI_edgehash_new();

	for (i=0; i<totface; i++) {
		MFace *mf = &((MFace*) mface)[i];
		int nverts= mf->v4? 4: 3;
		CCGFace *origf= ccgSubSurf_getFace(origss, SET_INT_IN_POINTER(i));
		unsigned int *fv = &mf->v1;

		get_face_uv_map_vert(vmap, mf, i, fverts);

		for (j=0; j<nverts; j++) {
			int v0 = GET_INT_FROM_POINTER(fverts[j]);
			int v1 = GET_INT_FROM_POINTER(fverts[(j+1)%nverts]);
			MVert *mv0 = mvert + *(fv+j);
			MVert *mv1 = mvert + *(fv+((j+1)%nverts));

			if (!BLI_edgehash_haskey(ehash, v0, v1)) {
				CCGEdge *e, *orige= ccgSubSurf_getFaceEdge(origss, origf, j);
				CCGEdgeHDL ehdl= SET_INT_IN_POINTER(i*4 + j);
				float crease;

				if ((mv0->flag&mv1->flag) & ME_VERT_MERGED)
					crease = creaseFactor;
				else
					crease = ccgSubSurf_getEdgeCrease(origss, orige);

				ccgSubSurf_syncEdge(ss, ehdl, fverts[j], fverts[(j+1)%nverts], crease, &e);
				BLI_edgehash_insert(ehash, v0, v1, NULL);
			}
		}
	}

	BLI_edgehash_free(ehash, NULL);

	/* create faces */
	for (i=0; i<totface; i++) {
		MFace *mf = &((MFace*) mface)[i];
		int nverts= mf->v4? 4: 3;
		CCGFace *f;

		get_face_uv_map_vert(vmap, mf, i, fverts);
		ccgSubSurf_syncFace(ss, SET_INT_IN_POINTER(i), nverts, fverts, &f);
	}

	free_uv_vert_map(vmap);
	ccgSubSurf_processSync(ss);

	return 1;
}

static void set_subsurf_uv(CCGSubSurf *ss, DerivedMesh *dm, DerivedMesh *result, int n)
{
	CCGSubSurf *uvss;
	CCGFace **faceMap;
	MTFace *tf;
	CCGFaceIterator *fi;
	int index, gridSize, gridFaces, edgeSize, totface, x, y, S;
	MTFace *dmtface = CustomData_get_layer_n(&dm->faceData, CD_MTFACE, n);
	MTFace *tface = CustomData_get_layer_n(&result->faceData, CD_MTFACE, n);

	if(!dmtface || !tface)
		return;

	/* create a CCGSubsurf from uv's */
	uvss = _getSubSurf(NULL, ccgSubSurf_getSubdivisionLevels(ss), 0, 1, 0);

	if(!ss_sync_from_uv(uvss, ss, dm, dmtface)) {
		ccgSubSurf_free(uvss);
		return;
	}

	/* get some info from CCGSubsurf */
	totface = ccgSubSurf_getNumFaces(uvss);
	edgeSize = ccgSubSurf_getEdgeSize(uvss);
	gridSize = ccgSubSurf_getGridSize(uvss);
	gridFaces = gridSize - 1;

	/* make a map from original faces to CCGFaces */
	faceMap = MEM_mallocN(totface*sizeof(*faceMap), "facemapuv");

	fi = ccgSubSurf_getFaceIterator(uvss);
	for(; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		faceMap[GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(uvss, f))] = f;
	}
	ccgFaceIterator_free(fi);

	/* load coordinates from uvss into tface */
	tf= tface;

	for(index = 0; index < totface; index++) {
		CCGFace *f = faceMap[index];
		int numVerts = ccgSubSurf_getFaceNumVerts(uvss, f);

		for (S=0; S<numVerts; S++) {
			VertData *faceGridData= ccgSubSurf_getFaceGridDataArray(uvss, f, S);

			for(y = 0; y < gridFaces; y++) {
				for(x = 0; x < gridFaces; x++) {
					float *a = faceGridData[(y + 0)*gridSize + x + 0].co;
					float *b = faceGridData[(y + 0)*gridSize + x + 1].co;
					float *c = faceGridData[(y + 1)*gridSize + x + 1].co;
					float *d = faceGridData[(y + 1)*gridSize + x + 0].co;

					tf->uv[0][0] = a[0]; tf->uv[0][1] = a[1];
					tf->uv[1][0] = d[0]; tf->uv[1][1] = d[1];
					tf->uv[2][0] = c[0]; tf->uv[2][1] = c[1];
					tf->uv[3][0] = b[0]; tf->uv[3][1] = b[1];

					tf++;
				}
			}
		}
	}

	ccgSubSurf_free(uvss);
	MEM_freeN(faceMap);
}

#if 0
static unsigned int ss_getEdgeFlags(CCGSubSurf *ss, CCGEdge *e, int ssFromEditmesh, DispListMesh *dlm, MEdge *medge, MTFace *tface)
{
	unsigned int flags = 0;
	int N = ccgSubSurf_getEdgeNumFaces(ss, e);

	if (!N) flags |= ME_LOOSEEDGE;

	if (ssFromEditmesh) {
		EditEdge *eed = ccgSubSurf_getEdgeEdgeHandle(ss, e);

		flags |= ME_EDGEDRAW|ME_EDGERENDER;
		if (eed->seam) {
			flags |= ME_SEAM;
		}
	} else {
		if (edgeIdx!=-1) {
			MEdge *origMed = &medge[edgeIdx];

			if (dlm) {
				flags |= origMed->flag&~ME_EDGE_STEPINDEX;
			} else {
				flags |= (origMed->flag&ME_SEAM)|ME_EDGEDRAW|ME_EDGERENDER;
			}
		}
	}

	return flags;
}
#endif

/* face weighting */
static void calc_ss_weights(int gridFaces,
                            FaceVertWeight **qweight, FaceVertWeight **tweight)
{
	FaceVertWeight *qw, *tw;
	int x, y, j;
	int numWeights = gridFaces * gridFaces;

	*tweight = MEM_mallocN(sizeof(**tweight) * numWeights, "ssTriWeight");
	*qweight = MEM_mallocN(sizeof(**qweight) * numWeights, "ssQuadWeight");

	qw = *qweight;
	tw = *tweight;

	for (y = 0; y < gridFaces; y++) {
		for (x = 0; x < gridFaces; x++) {
			for (j = 0; j < 4; j++) {
				int fx = x + (j == 2 || j == 3);
				int fy = y + (j == 1 || j == 2);
				float x_v = (float) fx / gridFaces;
				float y_v = (float) fy / gridFaces;
				float tx_v = (1.0f - x_v), ty_v = (1.0f - y_v);
				float center = (1.0f / 3.0f) * tx_v * ty_v;

				(*tw)[j][0] = center + 0.5f * tx_v * y_v;
				(*tw)[j][2] = center + 0.5f * x_v * ty_v;
				(*tw)[j][1] = 1.0f - (*tw)[j][0] - (*tw)[j][2];
				(*tw)[j][3] = 0.0f;

				tx_v *= 0.5f;
				ty_v *= 0.5f;

				(*qw)[j][3] = tx_v * ty_v;
				(*qw)[j][0] = (*qw)[j][3] + tx_v * y_v;
				(*qw)[j][2] = (*qw)[j][3] + x_v * ty_v;
				(*qw)[j][1] = 1.0f - (*qw)[j][0] - (*qw)[j][2] - (*qw)[j][3];

			}
			tw++;
			qw++;
		}
	}
}

DerivedMesh *ss_to_cdderivedmesh(CCGSubSurf *ss, int ssFromEditmesh,
                                 int drawInteriorEdges, int useSubsurfUv,
                                 DerivedMesh *dm)
{
	DerivedMesh *result;
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);
	int gridFaces = gridSize - 1;
	int edgeBase, faceBase;
	int i, j, k, S, x, y, index;
	int vertBase = 0;
	CCGVertIterator *vi;
	CCGEdgeIterator *ei;
	CCGFaceIterator *fi;
	CCGFace **faceMap2;
	CCGEdge **edgeMap2;
	CCGVert **vertMap2;
	int totvert, totedge, totface;
	MVert *mvert;
	MEdge *med;
	MFace *mf;
	int *origIndex;
	FaceVertWeight *qweight, *tweight;

	calc_ss_weights(gridFaces, &qweight, &tweight);

	/* vert map */
	totvert = ccgSubSurf_getNumVerts(ss);
	vertMap2 = MEM_mallocN(totvert*sizeof(*vertMap2), "vertmap");
	vi = ccgSubSurf_getVertIterator(ss);
	for(; !ccgVertIterator_isStopped(vi); ccgVertIterator_next(vi)) {
		CCGVert *v = ccgVertIterator_getCurrent(vi);

		vertMap2[GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ss, v))] = v;
	}
	ccgVertIterator_free(vi);

	totedge = ccgSubSurf_getNumEdges(ss);
	edgeMap2 = MEM_mallocN(totedge*sizeof(*edgeMap2), "edgemap");
	ei = ccgSubSurf_getEdgeIterator(ss);
	for(; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);

		edgeMap2[GET_INT_FROM_POINTER(ccgSubSurf_getEdgeEdgeHandle(ss, e))] = e;
	}

	totface = ccgSubSurf_getNumFaces(ss);
	faceMap2 = MEM_mallocN(totface*sizeof(*faceMap2), "facemap");
	fi = ccgSubSurf_getFaceIterator(ss);
	for(; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);

		faceMap2[GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(ss, f))] = f;
	}
	ccgFaceIterator_free(fi);

	if(dm) {
		result = CDDM_from_template(dm, ccgSubSurf_getNumFinalVerts(ss),
		                            ccgSubSurf_getNumFinalEdges(ss),
		                            ccgSubSurf_getNumFinalFaces(ss));
	} else {
		result = CDDM_new(ccgSubSurf_getNumFinalVerts(ss),
		                  ccgSubSurf_getNumFinalEdges(ss),
		                  ccgSubSurf_getNumFinalFaces(ss));
	}

	// load verts
	faceBase = i = 0;
	mvert = CDDM_get_verts(result);
	origIndex = result->getVertData(result, 0, CD_ORIGINDEX);

	for(index = 0; index < totface; index++) {
		CCGFace *f = faceMap2[index];
		int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);
		FaceVertWeight *weight = (numVerts == 4) ? qweight : tweight;
		int vertIdx[4];

		for(S = 0; S < numVerts; S++) {
			CCGVert *v = ccgSubSurf_getFaceVert(ss, f, S);

			vertIdx[S] = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ss, v));
		}

		DM_interp_vert_data(dm, result, vertIdx, weight[0][0], numVerts, i);
		VecCopyf(mvert->co, ccgSubSurf_getFaceCenterData(ss, f));
		*origIndex = ORIGINDEX_NONE;
		++mvert;
		++origIndex;
		i++;
		
		for(S = 0; S < numVerts; S++) {
			int prevS = (S - 1 + numVerts) % numVerts;
			int nextS = (S + 1) % numVerts;
			int otherS = (numVerts == 4) ? (S + 2) % numVerts : 3;
			for(x = 1; x < gridFaces; x++) {
				float w[4];
				w[prevS]  = weight[x][0][0];
				w[S]      = weight[x][0][1];
				w[nextS]  = weight[x][0][2];
				w[otherS] = weight[x][0][3];
				DM_interp_vert_data(dm, result, vertIdx, w, numVerts, i);
				VecCopyf(mvert->co,
				         ccgSubSurf_getFaceGridEdgeData(ss, f, S, x));
				*origIndex = ORIGINDEX_NONE;
				++mvert;
				++origIndex;
				i++;
			}
		}

		for(S = 0; S < numVerts; S++) {
			int prevS = (S - 1 + numVerts) % numVerts;
			int nextS = (S + 1) % numVerts;
			int otherS = (numVerts == 4) ? (S + 2) % numVerts : 3;
			for(y = 1; y < gridFaces; y++) {
				for(x = 1; x < gridFaces; x++) {
					float w[4];
					w[prevS]  = weight[y * gridFaces + x][0][0];
					w[S]      = weight[y * gridFaces + x][0][1];
					w[nextS]  = weight[y * gridFaces + x][0][2];
					w[otherS] = weight[y * gridFaces + x][0][3];
					DM_interp_vert_data(dm, result, vertIdx, w, numVerts, i);
					VecCopyf(mvert->co,
					         ccgSubSurf_getFaceGridData(ss, f, S, x, y));
					*origIndex = ORIGINDEX_NONE;
					++mvert;
					++origIndex;
					i++;
				}
			}
		}

		*((int*)ccgSubSurf_getFaceUserData(ss, f)) = faceBase;
		faceBase += 1 + numVerts * ((gridSize-2) + (gridSize-2) * (gridSize-2));
	}

	edgeBase = i;
	for(index = 0; index < totedge; index++) {
		CCGEdge *e = edgeMap2[index];
		int x;
		int vertIdx[2];

		CCGVert *v;
		v = ccgSubSurf_getEdgeVert0(ss, e);
		vertIdx[0] = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ss, v));
		v = ccgSubSurf_getEdgeVert1(ss, e);
		vertIdx[1] = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ss, v));

		for(x = 1; x < edgeSize - 1; x++) {
			float w[2];
			w[1] = (float) x / (edgeSize - 1);
			w[0] = 1 - w[1];
			DM_interp_vert_data(dm, result, vertIdx, w, 2, i);
			VecCopyf(mvert->co, ccgSubSurf_getEdgeData(ss, e, x));
			*origIndex = ORIGINDEX_NONE;
			++mvert;
			++origIndex;
			i++;
		}

		*((int*)ccgSubSurf_getEdgeUserData(ss, e)) = edgeBase;
		edgeBase += edgeSize-2;
	}

	vertBase = i;
	for(index = 0; index < totvert; index++) {
		CCGVert *v = vertMap2[index];
		int vertIdx;

		vertIdx = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ss, v));

		DM_copy_vert_data(dm, result, vertIdx, i, 1);
		VecCopyf(mvert->co, ccgSubSurf_getVertData(ss, v));

		*((int*)ccgSubSurf_getVertUserData(ss, v)) = i;
		*origIndex = ccgDM_getVertMapIndex(NULL, ss, v);
		++mvert;
		++origIndex;
		i++;
	}

	// load edges
	i = 0;
	med = CDDM_get_edges(result);
	origIndex = result->getEdgeData(result, 0, CD_ORIGINDEX);

	for(index = 0; index < totface; index++) {
		CCGFace *f = faceMap2[index];
		int numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

		for(k = 0; k < numVerts; k++) {
			for(x = 0; x < gridFaces; x++) {
				if(drawInteriorEdges) med->flag = ME_EDGEDRAW | ME_EDGERENDER;
				med->v1 = getFaceIndex(ss, f, k, x, 0, edgeSize, gridSize);
				med->v2 = getFaceIndex(ss, f, k, x+1, 0, edgeSize, gridSize);
				*origIndex = ORIGINDEX_NONE;
				++med;
				++origIndex;
				i++;
			}

			for(x = 1; x < gridFaces; x++) {
				for(y = 0; y < gridFaces; y++) {
					if(drawInteriorEdges)
						med->flag = ME_EDGEDRAW | ME_EDGERENDER;
					med->v1 = getFaceIndex(ss, f, k, x, y, edgeSize, gridSize);
					med->v2 = getFaceIndex(ss, f, k, x, y + 1,
					                       edgeSize, gridSize);
					*origIndex = ORIGINDEX_NONE;
					++med;
					++origIndex;
					i++;

					if(drawInteriorEdges)
						med->flag = ME_EDGEDRAW | ME_EDGERENDER;
					med->v1 = getFaceIndex(ss, f, k, y, x, edgeSize, gridSize);
					med->v2 = getFaceIndex(ss, f, k, y + 1, x,
					                       edgeSize, gridSize);
					*origIndex = ORIGINDEX_NONE;
					++med;
					++origIndex;
					i++;
				}
			}
		}
	}

	for(index = 0; index < totedge; index++) {
		CCGEdge *e = edgeMap2[index];
		unsigned int flags = 0;
		int edgeIdx = GET_INT_FROM_POINTER(ccgSubSurf_getEdgeEdgeHandle(ss, e));

		if(!ccgSubSurf_getEdgeNumFaces(ss, e)) flags |= ME_LOOSEEDGE;


		if(edgeIdx != -1 && dm) {
			MEdge origMed;
			dm->getEdge(dm, edgeIdx, &origMed);

			flags |= origMed.flag;
		}

		for(x = 0; x < edgeSize - 1; x++) {
			med->v1 = getEdgeIndex(ss, e, x, edgeSize);
			med->v2 = getEdgeIndex(ss, e, x + 1, edgeSize);
			med->flag = flags;
			*origIndex = ccgDM_getEdgeMapIndex(NULL, ss, e);
			++med;
			++origIndex;
			i++;
		}
	}

	// load faces
	i = 0;
	mf = CDDM_get_faces(result);
	origIndex = result->getFaceData(result, 0, CD_ORIGINDEX);

	for(index = 0; index < totface; index++) {
		CCGFace *f = faceMap2[index];
		int numVerts = ccgSubSurf_getFaceNumVerts(ss, f);
		int mat_nr;
		int flag;
		int mapIndex = ccgDM_getFaceMapIndex(NULL, ss, f);
		int faceIdx = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(ss, f));

		if(!ssFromEditmesh) {
			MFace origMFace;
			dm->getFace(dm, faceIdx, &origMFace);
			
			mat_nr = origMFace.mat_nr;
			flag = origMFace.flag;
		} else {
			EditFace *ef = ccgSubSurf_getFaceFaceHandle(ss, f);
			mat_nr = ef->mat_nr;
			flag = ef->flag;
		}

		for(S = 0; S < numVerts; S++) {
			FaceVertWeight *weight = (numVerts == 4) ? qweight : tweight;

			for(y = 0; y < gridFaces; y++) {
				for(x = 0; x < gridFaces; x++) {
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

					if(dm) {
						int prevS = (S - 1 + numVerts) % numVerts;
						int nextS = (S + 1) % numVerts;
						int otherS = (numVerts == 4) ? (S + 2) % numVerts : 3;
						FaceVertWeight w;

						for(j = 0; j < 4; ++j) {
							w[j][prevS]  = (*weight)[j][0];
							w[j][S]      = (*weight)[j][1];
							w[j][nextS]  = (*weight)[j][2];
							w[j][otherS] = (*weight)[j][3];
						}

						DM_interp_face_data(dm, result, &faceIdx, NULL,
						                    &w, 1, i);
						weight++;
					}

					*origIndex = mapIndex;
					++mf;
					++origIndex;
					i++;
				}
			}
		}
	}

	MEM_freeN(faceMap2);
	MEM_freeN(edgeMap2);
	MEM_freeN(vertMap2);

	MEM_freeN(tweight);
	MEM_freeN(qweight);

	if(useSubsurfUv) {
		CustomData *fdata = &result->faceData;
		CustomData *dmfdata = &dm->faceData;
		int numlayer = CustomData_number_of_layers(fdata, CD_MTFACE);
		int dmnumlayer = CustomData_number_of_layers(dmfdata, CD_MTFACE);
		
		for (i=0; i<numlayer && i<dmnumlayer; i++)
			set_subsurf_uv(ss, dm, result, i);
	}

	CDDM_calc_normals(result);

	return result;
}

static void ss_sync_from_derivedmesh(CCGSubSurf *ss, DerivedMesh *dm,
                                     float (*vertexCos)[3], int useFlatSubdiv)
{
	float creaseFactor = (float) ccgSubSurf_getSubdivisionLevels(ss);
	CCGVertHDL fVerts[4];
	int totvert = dm->getNumVerts(dm);
	int totedge = dm->getNumEdges(dm);
	int totface = dm->getNumFaces(dm);
	int i;
	int *index;
	MVert *mvert = dm->getVertArray(dm);
	MEdge *medge = dm->getEdgeArray(dm);
	MFace *mface = dm->getFaceArray(dm);
	MVert *mv;
	MEdge *me;
	MFace *mf;

	ccgSubSurf_initFullSync(ss);

	mv = mvert;
	index = (int *)dm->getVertDataArray(dm, CD_ORIGINDEX);
	for(i = 0; i < totvert; i++, mv++, index++) {
		CCGVert *v;

		if(vertexCos) {
			ccgSubSurf_syncVert(ss, SET_INT_IN_POINTER(i), vertexCos[i], 0, &v);
		} else {
			ccgSubSurf_syncVert(ss, SET_INT_IN_POINTER(i), mv->co, 0, &v);
		}

		((int*)ccgSubSurf_getVertUserData(ss, v))[1] = *index;
	}

	me = medge;
	index = (int *)dm->getEdgeDataArray(dm, CD_ORIGINDEX);
	for(i = 0; i < totedge; i++, me++, index++) {
		CCGEdge *e;
		float crease;

		crease = useFlatSubdiv ? creaseFactor :
		                         me->crease * creaseFactor / 255.0f;

		ccgSubSurf_syncEdge(ss, SET_INT_IN_POINTER(i), SET_INT_IN_POINTER(me->v1),
		                    SET_INT_IN_POINTER(me->v2), crease, &e);

		((int*)ccgSubSurf_getEdgeUserData(ss, e))[1] = *index;
	}

	mf = mface;
	index = (int *)dm->getFaceDataArray(dm, CD_ORIGINDEX);
	for (i = 0; i < totface; i++, mf++, index++) {
		CCGFace *f;

		fVerts[0] = SET_INT_IN_POINTER(mf->v1);
		fVerts[1] = SET_INT_IN_POINTER(mf->v2);
		fVerts[2] = SET_INT_IN_POINTER(mf->v3);
		fVerts[3] = SET_INT_IN_POINTER(mf->v4);

		// this is very bad, means mesh is internally consistent.
		// it is not really possible to continue without modifying
		// other parts of code significantly to handle missing faces.
		// since this really shouldn't even be possible we just bail.
		if(ccgSubSurf_syncFace(ss, SET_INT_IN_POINTER(i), fVerts[3] ? 4 : 3,
		                       fVerts, &f) == eCCGError_InvalidValue) {
			static int hasGivenError = 0;

			if(!hasGivenError) {
				error("Unrecoverable error in SubSurf calculation,"
				      " mesh is inconsistent.");

				hasGivenError = 1;
			}

			return;
		}

		((int*)ccgSubSurf_getFaceUserData(ss, f))[1] = *index;
	}

	ccgSubSurf_processSync(ss);
}

/***/

static int ccgDM_getVertMapIndex(CCGDerivedMesh *ccgdm, CCGSubSurf *ss, CCGVert *v) {
	return ((int*) ccgSubSurf_getVertUserData(ss, v))[1];
}

static int ccgDM_getEdgeMapIndex(CCGDerivedMesh *ccgdm, CCGSubSurf *ss, CCGEdge *e) {
	return ((int*) ccgSubSurf_getEdgeUserData(ss, e))[1];
}

static int ccgDM_getFaceMapIndex(CCGDerivedMesh *ccgdm, CCGSubSurf *ss, CCGFace *f) {
	return ((int*) ccgSubSurf_getFaceUserData(ss, f))[1];
}

static void ccgDM_getMinMax(DerivedMesh *dm, float min_r[3], float max_r[3]) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGVertIterator *vi = ccgSubSurf_getVertIterator(ss);
	CCGEdgeIterator *ei = ccgSubSurf_getEdgeIterator(ss);
	CCGFaceIterator *fi = ccgSubSurf_getFaceIterator(ss);
	int i, edgeSize = ccgSubSurf_getEdgeSize(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);

	if (!ccgSubSurf_getNumVerts(ss))
		min_r[0] = min_r[1] = min_r[2] = max_r[0] = max_r[1] = max_r[2] = 0.0;

	for (; !ccgVertIterator_isStopped(vi); ccgVertIterator_next(vi)) {
		CCGVert *v = ccgVertIterator_getCurrent(vi);
		float *co = ccgSubSurf_getVertData(ss, v);

		DO_MINMAX(co, min_r, max_r);
	}

	for (; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);
		VertData *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);

		for (i=0; i<edgeSize; i++)
			DO_MINMAX(edgeData[i].co, min_r, max_r);
	}

	for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

		for (S=0; S<numVerts; S++) {
			VertData *faceGridData = ccgSubSurf_getFaceGridDataArray(ss, f, S);

			for (y=0; y<gridSize; y++)
				for (x=0; x<gridSize; x++)
					DO_MINMAX(faceGridData[y*gridSize + x].co, min_r, max_r);
		}
	}

	ccgFaceIterator_free(fi);
	ccgEdgeIterator_free(ei);
	ccgVertIterator_free(vi);
}
static int ccgDM_getNumVerts(DerivedMesh *dm) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;

	return ccgSubSurf_getNumFinalVerts(ccgdm->ss);
}
static int ccgDM_getNumEdges(DerivedMesh *dm) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;

	return ccgSubSurf_getNumFinalEdges(ccgdm->ss);
}
static int ccgDM_getNumFaces(DerivedMesh *dm) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;

	return ccgSubSurf_getNumFinalFaces(ccgdm->ss);
}

static void ccgDM_getFinalVert(DerivedMesh *dm, int vertNum, MVert *mv)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	int i;

	memset(mv, 0, sizeof(*mv));

	if(vertNum < ccgdm->edgeMap[0].startVert) {
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
		while(i < lastface && vertNum >= ccgdm->faceMap[i + 1].startVert)
			++i;

		f = ccgdm->faceMap[i].face;
		numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

		gridSideVerts = gridSize - 2;
		gridInternalVerts = gridSideVerts * gridSideVerts;

		gridSideEnd = 1 + numVerts * gridSideVerts;
		gridInternalEnd = gridSideEnd + numVerts * gridInternalVerts;

		offset = vertNum - ccgdm->faceMap[i].startVert;
		if(offset < 1) {
			VecCopyf(mv->co, ccgSubSurf_getFaceCenterData(ss, f));
		} else if(offset < gridSideEnd) {
			offset -= 1;
			grid = offset / gridSideVerts;
			x = offset % gridSideVerts + 1;
			VecCopyf(mv->co, ccgSubSurf_getFaceGridEdgeData(ss, f, grid, x));
		} else if(offset < gridInternalEnd) {
			offset -= gridSideEnd;
			grid = offset / gridInternalVerts;
			offset %= gridInternalVerts;
			y = offset / gridSideVerts + 1;
			x = offset % gridSideVerts + 1;
			VecCopyf(mv->co, ccgSubSurf_getFaceGridData(ss, f, grid, x, y));
		}
	} else if(vertNum < ccgdm->vertMap[0].startVert) {
		/* this vert comes from edge data */
		CCGEdge *e;
		int lastedge = ccgSubSurf_getNumEdges(ss) - 1;
		int x;

		i = 0;
		while(i < lastedge && vertNum >= ccgdm->edgeMap[i + 1].startVert)
			++i;

		e = ccgdm->edgeMap[i].edge;

		x = vertNum - ccgdm->edgeMap[i].startVert + 1;
		VecCopyf(mv->co, ccgSubSurf_getEdgeData(ss, e, x));
	} else {
		/* this vert comes from vert data */
		CCGVert *v;
		i = vertNum - ccgdm->vertMap[0].startVert;

		v = ccgdm->vertMap[i].vert;
		VecCopyf(mv->co, ccgSubSurf_getVertData(ss, v));
	}
}

static void ccgDM_getFinalEdge(DerivedMesh *dm, int edgeNum, MEdge *med)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	int i;

	memset(med, 0, sizeof(*med));

	if(edgeNum < ccgdm->edgeMap[0].startEdge) {
		/* this edge comes from face data */
		int lastface = ccgSubSurf_getNumFaces(ss) - 1;
		CCGFace *f;
		int x, y, grid, numVerts;
		int offset;
		int gridSize = ccgSubSurf_getGridSize(ss);
		int edgeSize = ccgSubSurf_getEdgeSize(ss);
		int gridSideEdges;
		int gridInternalEdges;

		i = 0;
		while(i < lastface && edgeNum >= ccgdm->faceMap[i + 1].startEdge)
			++i;

		f = ccgdm->faceMap[i].face;
		numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

		gridSideEdges = gridSize - 1;
		gridInternalEdges = (gridSideEdges - 1) * gridSideEdges * 2; 

		offset = edgeNum - ccgdm->faceMap[i].startEdge;
		grid = offset / (gridSideEdges + gridInternalEdges);
		offset %= (gridSideEdges + gridInternalEdges);

		if(offset < gridSideEdges) {
			x = offset;
			med->v1 = getFaceIndex(ss, f, grid, x, 0, edgeSize, gridSize);
			med->v2 = getFaceIndex(ss, f, grid, x+1, 0, edgeSize, gridSize);
		} else {
			offset -= gridSideEdges;
			x = (offset / 2) / gridSideEdges + 1;
			y = (offset / 2) % gridSideEdges;
			if(offset % 2 == 0) {
				med->v1 = getFaceIndex(ss, f, grid, x, y, edgeSize, gridSize);
				med->v2 = getFaceIndex(ss, f, grid, x, y+1, edgeSize, gridSize);
			} else {
				med->v1 = getFaceIndex(ss, f, grid, y, x, edgeSize, gridSize);
				med->v2 = getFaceIndex(ss, f, grid, y+1, x, edgeSize, gridSize);
			}
		}
	} else {
		/* this vert comes from edge data */
		CCGEdge *e;
		int edgeSize = ccgSubSurf_getEdgeSize(ss);
		int x, *edgeFlag;
		unsigned int flags = 0;

		i = (edgeNum - ccgdm->edgeMap[0].startEdge) / (edgeSize - 1);

		e = ccgdm->edgeMap[i].edge;

		if(!ccgSubSurf_getEdgeNumFaces(ss, e)) flags |= ME_LOOSEEDGE;

		x = edgeNum - ccgdm->edgeMap[i].startEdge;

		med->v1 = getEdgeIndex(ss, e, x, edgeSize);
		med->v2 = getEdgeIndex(ss, e, x+1, edgeSize);

		edgeFlag = dm->getEdgeData(dm, edgeNum, CD_FLAGS);
		if(edgeFlag)
			flags |= (*edgeFlag & (ME_SEAM | ME_SHARP))
					 | ME_EDGEDRAW | ME_EDGERENDER;
		else
			flags |= ME_EDGEDRAW | ME_EDGERENDER;

		med->flag = flags;
	}
}

static void ccgDM_getFinalFace(DerivedMesh *dm, int faceNum, MFace *mf)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	int gridSize = ccgSubSurf_getGridSize(ss);
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	int gridSideEdges = gridSize - 1;
	int gridFaces = gridSideEdges * gridSideEdges;
	int i;
	CCGFace *f;
	int numVerts;
	int offset;
	int grid;
	int x, y;
	int lastface = ccgSubSurf_getNumFaces(ss) - 1;
	char *faceFlags = dm->getFaceDataArray(dm, CD_FLAGS);

	memset(mf, 0, sizeof(*mf));

	i = 0;
	while(i < lastface && faceNum >= ccgdm->faceMap[i + 1].startFace)
		++i;

	f = ccgdm->faceMap[i].face;
	numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

	offset = faceNum - ccgdm->faceMap[i].startFace;
	grid = offset / gridFaces;
	offset %= gridFaces;
	y = offset / gridSideEdges;
	x = offset % gridSideEdges;

	mf->v1 = getFaceIndex(ss, f, grid, x+0, y+0, edgeSize, gridSize);
	mf->v2 = getFaceIndex(ss, f, grid, x+0, y+1, edgeSize, gridSize);
	mf->v3 = getFaceIndex(ss, f, grid, x+1, y+1, edgeSize, gridSize);
	mf->v4 = getFaceIndex(ss, f, grid, x+1, y+0, edgeSize, gridSize);

	if(faceFlags) mf->flag = faceFlags[i*4];
	else mf->flag = ME_SMOOTH;
}

static void ccgDM_copyFinalVertArray(DerivedMesh *dm, MVert *mvert)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	int index;
	int totvert, totedge, totface;
	int gridSize = ccgSubSurf_getGridSize(ss);
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	int i = 0;

	totface = ccgSubSurf_getNumFaces(ss);
	for(index = 0; index < totface; index++) {
		CCGFace *f = ccgdm->faceMap[index].face;
		int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

		VecCopyf(mvert[i++].co, ccgSubSurf_getFaceCenterData(ss, f));
		
		for(S = 0; S < numVerts; S++) {
			for(x = 1; x < gridSize - 1; x++) {
				VecCopyf(mvert[i++].co,
				         ccgSubSurf_getFaceGridEdgeData(ss, f, S, x));
			}
		}

		for(S = 0; S < numVerts; S++) {
			for(y = 1; y < gridSize - 1; y++) {
				for(x = 1; x < gridSize - 1; x++) {
					VecCopyf(mvert[i++].co,
					         ccgSubSurf_getFaceGridData(ss, f, S, x, y));
				}
			}
		}
	}

	totedge = ccgSubSurf_getNumEdges(ss);
	for(index = 0; index < totedge; index++) {
		CCGEdge *e = ccgdm->edgeMap[index].edge;
		int x;

		for(x = 1; x < edgeSize - 1; x++) {
			VecCopyf(mvert[i++].co, ccgSubSurf_getEdgeData(ss, e, x));
		}
	}

	totvert = ccgSubSurf_getNumVerts(ss);
	for(index = 0; index < totvert; index++) {
		CCGVert *v = ccgdm->vertMap[index].vert;

		VecCopyf(mvert[i].co, ccgSubSurf_getVertData(ss, v));

		i++;
	}
}

static void ccgDM_copyFinalEdgeArray(DerivedMesh *dm, MEdge *medge)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	int index;
	int totedge, totface;
	int gridSize = ccgSubSurf_getGridSize(ss);
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	int i = 0;
	int *edgeFlags = dm->getEdgeDataArray(dm, CD_FLAGS);

	totface = ccgSubSurf_getNumFaces(ss);
	for(index = 0; index < totface; index++) {
		CCGFace *f = ccgdm->faceMap[index].face;
		int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

		for(S = 0; S < numVerts; S++) {
			for(x = 0; x < gridSize - 1; x++) {
				MEdge *med = &medge[i];

				if(ccgdm->drawInteriorEdges)
				    med->flag = ME_EDGEDRAW | ME_EDGERENDER;
				med->v1 = getFaceIndex(ss, f, S, x, 0, edgeSize, gridSize);
				med->v2 = getFaceIndex(ss, f, S, x + 1, 0, edgeSize, gridSize);
				i++;
			}

			for(x = 1; x < gridSize - 1; x++) {
				for(y = 0; y < gridSize - 1; y++) {
					MEdge *med;

					med = &medge[i];
					if(ccgdm->drawInteriorEdges)
					    med->flag = ME_EDGEDRAW | ME_EDGERENDER;
					med->v1 = getFaceIndex(ss, f, S, x, y,
					                       edgeSize, gridSize);
					med->v2 = getFaceIndex(ss, f, S, x, y + 1,
					                       edgeSize, gridSize);
					i++;

					med = &medge[i];
					if(ccgdm->drawInteriorEdges)
					    med->flag = ME_EDGEDRAW | ME_EDGERENDER;
					med->v1 = getFaceIndex(ss, f, S, y, x,
					                       edgeSize, gridSize);
					med->v2 = getFaceIndex(ss, f, S, y + 1, x,
					                       edgeSize, gridSize);
					i++;
				}
			}
		}
	}

	totedge = ccgSubSurf_getNumEdges(ss);
	for(index = 0; index < totedge; index++) {
		CCGEdge *e = ccgdm->edgeMap[index].edge;
		unsigned int flags = 0;
		int x;
		int edgeIdx = GET_INT_FROM_POINTER(ccgSubSurf_getEdgeEdgeHandle(ss, e));

		if(!ccgSubSurf_getEdgeNumFaces(ss, e)) flags |= ME_LOOSEEDGE;

		if(edgeFlags) {
			if(edgeIdx != -1) {
				flags |= (edgeFlags[i] & (ME_SEAM | ME_SHARP))
				         | ME_EDGEDRAW | ME_EDGERENDER;
			}
		} else {
			flags |= ME_EDGEDRAW | ME_EDGERENDER;
		}

		for(x = 0; x < edgeSize - 1; x++) {
			MEdge *med = &medge[i];
			med->v1 = getEdgeIndex(ss, e, x, edgeSize);
			med->v2 = getEdgeIndex(ss, e, x + 1, edgeSize);
			med->flag = flags;
			i++;
		}
	}
}

static void ccgDM_copyFinalFaceArray(DerivedMesh *dm, MFace *mface)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	int index;
	int totface;
	int gridSize = ccgSubSurf_getGridSize(ss);
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	int i = 0;
	char *faceFlags = dm->getFaceDataArray(dm, CD_FLAGS);

	totface = ccgSubSurf_getNumFaces(ss);
	for(index = 0; index < totface; index++) {
		CCGFace *f = ccgdm->faceMap[index].face;
		int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);
		int mat_nr = 0;
		int flag = ME_SMOOTH; /* assume face is smooth by default */

		for(S = 0; S < numVerts; S++) {
			for(y = 0; y < gridSize - 1; y++) {
				for(x = 0; x < gridSize - 1; x++) {
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
					if(faceFlags) mf->flag = faceFlags[index*4];
					else mf->flag = flag;

					i++;
				}
			}
		}
	}
}

static void ccgdm_getVertCos(DerivedMesh *dm, float (*cos)[3]) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);
	int i;
	CCGVertIterator *vi;
	CCGEdgeIterator *ei;
	CCGFaceIterator *fi;
	CCGFace **faceMap2;
	CCGEdge **edgeMap2;
	CCGVert **vertMap2;
	int index, totvert, totedge, totface;
	
	totvert = ccgSubSurf_getNumVerts(ss);
	vertMap2 = MEM_mallocN(totvert*sizeof(*vertMap2), "vertmap");
	vi = ccgSubSurf_getVertIterator(ss);
	for (; !ccgVertIterator_isStopped(vi); ccgVertIterator_next(vi)) {
		CCGVert *v = ccgVertIterator_getCurrent(vi);

		vertMap2[GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ss, v))] = v;
	}
	ccgVertIterator_free(vi);

	totedge = ccgSubSurf_getNumEdges(ss);
	edgeMap2 = MEM_mallocN(totedge*sizeof(*edgeMap2), "edgemap");
	ei = ccgSubSurf_getEdgeIterator(ss);
	for (i=0; !ccgEdgeIterator_isStopped(ei); i++,ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);

		edgeMap2[GET_INT_FROM_POINTER(ccgSubSurf_getEdgeEdgeHandle(ss, e))] = e;
	}

	totface = ccgSubSurf_getNumFaces(ss);
	faceMap2 = MEM_mallocN(totface*sizeof(*faceMap2), "facemap");
	fi = ccgSubSurf_getFaceIterator(ss);
	for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);

		faceMap2[GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(ss, f))] = f;
	}
	ccgFaceIterator_free(fi);

	i = 0;
	for (index=0; index<totface; index++) {
		CCGFace *f = faceMap2[index];
		int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

		VecCopyf(cos[i++], ccgSubSurf_getFaceCenterData(ss, f));
		
		for (S=0; S<numVerts; S++) {
			for (x=1; x<gridSize-1; x++) {
				VecCopyf(cos[i++], ccgSubSurf_getFaceGridEdgeData(ss, f, S, x));
			}
		}

		for (S=0; S<numVerts; S++) {
			for (y=1; y<gridSize-1; y++) {
				for (x=1; x<gridSize-1; x++) {
					VecCopyf(cos[i++], ccgSubSurf_getFaceGridData(ss, f, S, x, y));
				}
			}
		}
	}

	for (index=0; index<totedge; index++) {
		CCGEdge *e= edgeMap2[index];
		int x;

		for (x=1; x<edgeSize-1; x++) {
			VecCopyf(cos[i++], ccgSubSurf_getEdgeData(ss, e, x));
		}
	}

	for (index=0; index<totvert; index++) {
		CCGVert *v = vertMap2[index];
		VecCopyf(cos[i++], ccgSubSurf_getVertData(ss, v));
	}

	MEM_freeN(vertMap2);
	MEM_freeN(edgeMap2);
	MEM_freeN(faceMap2);
}
static void ccgDM_foreachMappedVert(DerivedMesh *dm, void (*func)(void *userData, int index, float *co, float *no_f, short *no_s), void *userData) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGVertIterator *vi = ccgSubSurf_getVertIterator(ccgdm->ss);

	for (; !ccgVertIterator_isStopped(vi); ccgVertIterator_next(vi)) {
		CCGVert *v = ccgVertIterator_getCurrent(vi);
		VertData *vd = ccgSubSurf_getVertData(ccgdm->ss, v);
		int index = ccgDM_getVertMapIndex(ccgdm, ccgdm->ss, v);

		if (index!=-1)
			func(userData, index, vd->co, vd->no, NULL);
	}

	ccgVertIterator_free(vi);
}
static void ccgDM_foreachMappedEdge(DerivedMesh *dm, void (*func)(void *userData, int index, float *v0co, float *v1co), void *userData) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGEdgeIterator *ei = ccgSubSurf_getEdgeIterator(ss);
	int i, edgeSize = ccgSubSurf_getEdgeSize(ss);

	for (; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);
		VertData *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);
		int index = ccgDM_getEdgeMapIndex(ccgdm, ss, e);

		if (index!=-1) {
			for (i=0; i<edgeSize-1; i++)
				func(userData, index, edgeData[i].co, edgeData[i+1].co);
		}
	}

	ccgEdgeIterator_free(ei);
}

static void ccgDM_drawVerts(DerivedMesh *dm) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);
	CCGVertIterator *vi;
	CCGEdgeIterator *ei;
	CCGFaceIterator *fi;

	glBegin(GL_POINTS);
	vi = ccgSubSurf_getVertIterator(ss);
	for (; !ccgVertIterator_isStopped(vi); ccgVertIterator_next(vi)) {
		CCGVert *v = ccgVertIterator_getCurrent(vi);
		glVertex3fv(ccgSubSurf_getVertData(ss, v));
	}
	ccgVertIterator_free(vi);

	ei = ccgSubSurf_getEdgeIterator(ss);
	for (; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);
		int x;

		for (x=1; x<edgeSize-1; x++)
			glVertex3fv(ccgSubSurf_getEdgeData(ss, e, x));
	}
	ccgEdgeIterator_free(ei);

	fi = ccgSubSurf_getFaceIterator(ss);
	for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

		glVertex3fv(ccgSubSurf_getFaceCenterData(ss, f));
		for (S=0; S<numVerts; S++)
			for (x=1; x<gridSize-1; x++)
				glVertex3fv(ccgSubSurf_getFaceGridEdgeData(ss, f, S, x));
		for (S=0; S<numVerts; S++)
			for (y=1; y<gridSize-1; y++)
				for (x=1; x<gridSize-1; x++)
					glVertex3fv(ccgSubSurf_getFaceGridData(ss, f, S, x, y));
	}
	ccgFaceIterator_free(fi);
	glEnd();
}
static void ccgDM_drawEdges(DerivedMesh *dm, int drawLooseEdges) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGEdgeIterator *ei = ccgSubSurf_getEdgeIterator(ss);
	CCGFaceIterator *fi = ccgSubSurf_getFaceIterator(ss);
	int i, edgeSize = ccgSubSurf_getEdgeSize(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);
	int useAging;

	ccgSubSurf_getUseAgeCounts(ss, &useAging, NULL, NULL, NULL);

	for (; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);
		VertData *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);

		if (!drawLooseEdges && !ccgSubSurf_getEdgeNumFaces(ss, e))
			continue;

		if (useAging && !(G.f&G_BACKBUFSEL)) {
			int ageCol = 255-ccgSubSurf_getEdgeAge(ss, e)*4;
			glColor3ub(0, ageCol>0?ageCol:0, 0);
		}

		glBegin(GL_LINE_STRIP);
		for (i=0; i<edgeSize-1; i++) {
			glVertex3fv(edgeData[i].co);
			glVertex3fv(edgeData[i+1].co);
		}
		glEnd();
	}

	if (useAging && !(G.f&G_BACKBUFSEL)) {
		glColor3ub(0, 0, 0);
	}

	if (ccgdm->drawInteriorEdges) {
		for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
			CCGFace *f = ccgFaceIterator_getCurrent(fi);
			int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

			for (S=0; S<numVerts; S++) {
				VertData *faceGridData = ccgSubSurf_getFaceGridDataArray(ss, f, S);

				glBegin(GL_LINE_STRIP);
				for (x=0; x<gridSize; x++)
					glVertex3fv(faceGridData[x].co);
				glEnd();
				for (y=1; y<gridSize-1; y++) {
					glBegin(GL_LINE_STRIP);
					for (x=0; x<gridSize; x++)
						glVertex3fv(faceGridData[y*gridSize + x].co);
					glEnd();
				}
				for (x=1; x<gridSize-1; x++) {
					glBegin(GL_LINE_STRIP);
					for (y=0; y<gridSize; y++)
						glVertex3fv(faceGridData[y*gridSize + x].co);
					glEnd();
				}
			}
		}
	}

	ccgFaceIterator_free(fi);
	ccgEdgeIterator_free(ei);
}
static void ccgDM_drawLooseEdges(DerivedMesh *dm) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGEdgeIterator *ei = ccgSubSurf_getEdgeIterator(ss);
	int i, edgeSize = ccgSubSurf_getEdgeSize(ss);

	for (; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);
		VertData *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);

		if (!ccgSubSurf_getEdgeNumFaces(ss, e)) {
			glBegin(GL_LINE_STRIP);
			for (i=0; i<edgeSize-1; i++) {
				glVertex3fv(edgeData[i].co);
				glVertex3fv(edgeData[i+1].co);
			}
			glEnd();
		}
	}

	ccgEdgeIterator_free(ei);
}

static void ccgDM_glNormalFast(float *a, float *b, float *c, float *d)
{
	float a_cX = c[0]-a[0], a_cY = c[1]-a[1], a_cZ = c[2]-a[2];
	float b_dX = d[0]-b[0], b_dY = d[1]-b[1], b_dZ = d[2]-b[2];
	float no[3];

	no[0] = b_dY*a_cZ - b_dZ*a_cY;
	no[1] = b_dZ*a_cX - b_dX*a_cZ;
	no[2] = b_dX*a_cY - b_dY*a_cX;

	/* don't normalize, GL_NORMALIZE is be enabled */
	glNormal3fv(no);
}

	/* Only used by non-editmesh types */
static void ccgDM_drawFacesSolid(DerivedMesh *dm, int (*setMaterial)(int, void *attribs)) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGFaceIterator *fi = ccgSubSurf_getFaceIterator(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);
	char *faceFlags = DM_get_face_data_layer(dm, CD_FLAGS);

	for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);
		int index = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(ss, f));
		int drawSmooth, mat_nr;

		if(faceFlags) {
			drawSmooth = (faceFlags[index*4] & ME_SMOOTH);
			mat_nr= faceFlags[index*4 + 1];
		}
		else {
			drawSmooth = 1;
			mat_nr= 0;
		}
		
		if (!setMaterial(mat_nr+1, NULL))
			continue;

		glShadeModel(drawSmooth? GL_SMOOTH: GL_FLAT);
		for (S=0; S<numVerts; S++) {
			VertData *faceGridData = ccgSubSurf_getFaceGridDataArray(ss, f, S);

			if (drawSmooth) {
				for (y=0; y<gridSize-1; y++) {
					glBegin(GL_QUAD_STRIP);
					for (x=0; x<gridSize; x++) {
						VertData *a = &faceGridData[(y+0)*gridSize + x];
						VertData *b = &faceGridData[(y+1)*gridSize + x];

						glNormal3fv(a->no);
						glVertex3fv(a->co);
						glNormal3fv(b->no);
						glVertex3fv(b->co);
					}
					glEnd();
				}
			} else {
				glBegin(GL_QUADS);
				for (y=0; y<gridSize-1; y++) {
					for (x=0; x<gridSize-1; x++) {
						float *a = faceGridData[(y+0)*gridSize + x].co;
						float *b = faceGridData[(y+0)*gridSize + x + 1].co;
						float *c = faceGridData[(y+1)*gridSize + x + 1].co;
						float *d = faceGridData[(y+1)*gridSize + x].co;

						ccgDM_glNormalFast(a, b, c, d);

						glVertex3fv(d);
						glVertex3fv(c);
						glVertex3fv(b);
						glVertex3fv(a);
					}
				}
				glEnd();
			}
		}
	}

	ccgFaceIterator_free(fi);
}

	/* Only used by non-editmesh types */
static void ccgDM_drawMappedFacesGLSL(DerivedMesh *dm, int (*setMaterial)(int, void *attribs), int (*setDrawOptions)(void *userData, int index), void *userData) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGFaceIterator *fi = ccgSubSurf_getFaceIterator(ss);
	GPUVertexAttribs gattribs;
	DMVertexAttribs attribs;
	MTFace *tf = dm->getFaceDataArray(dm, CD_MTFACE);
	int gridSize = ccgSubSurf_getGridSize(ss);
	int gridFaces = gridSize - 1;
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	int transp, orig_transp, new_transp;
	char *faceFlags = DM_get_face_data_layer(dm, CD_FLAGS);
	int a, b, i, doDraw, numVerts, matnr, new_matnr, totface;

	doDraw = 0;
	numVerts = 0;
	matnr = -1;
	transp = GPU_get_material_blend_mode();
	orig_transp = transp;

	memset(&attribs, 0, sizeof(attribs));

#define PASSATTRIB(dx, dy, vert) {												\
	if(attribs.totorco) {														\
		index = getFaceIndex(ss, f, S, x+dx, y+dy, edgeSize, gridSize); 		\
		glVertexAttrib3fvARB(attribs.orco.glIndex, attribs.orco.array[index]);	\
	}																			\
	for(b = 0; b < attribs.tottface; b++) {										\
		MTFace *tf = &attribs.tface[b].array[a];								\
		glVertexAttrib2fvARB(attribs.tface[b].glIndex, tf->uv[vert]);			\
	}																			\
	for(b = 0; b < attribs.totmcol; b++) {										\
		MCol *cp = &attribs.mcol[b].array[a*4 + vert];							\
		GLubyte col[4];															\
		col[0]= cp->b; col[1]= cp->g; col[2]= cp->r; col[3]= cp->a;				\
		glVertexAttrib4ubvARB(attribs.mcol[b].glIndex, col);					\
	}																			\
	if(attribs.tottang) {														\
		float *tang = attribs.tang.array[a*4 + vert];							\
		glVertexAttrib3fvARB(attribs.tang.glIndex, tang);						\
	}																			\
}

	totface = ccgSubSurf_getNumFaces(ss);
	for(a = 0, i = 0; i < totface; i++) {
		CCGFace *f = ccgdm->faceMap[i].face;
		int S, x, y, drawSmooth;
		int index = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(ss, f));
		int origIndex = ccgDM_getFaceMapIndex(ccgdm, ss, f);
		
		numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

		if(faceFlags) {
			drawSmooth = (faceFlags[index*4] & ME_SMOOTH);
			new_matnr= faceFlags[index*4 + 1] + 1;
		}
		else {
			drawSmooth = 1;
			new_matnr= 1;
		}

		if(new_matnr != matnr) {
			doDraw = setMaterial(matnr = new_matnr, &gattribs);
			if(doDraw)
				DM_vertex_attributes_from_gpu(dm, &gattribs, &attribs);
		}

		if(!doDraw || (setDrawOptions && !setDrawOptions(userData, origIndex))) {
			a += gridFaces*gridFaces*numVerts;
			continue;
		}

		if(tf) {
			new_transp = tf[i].transp;

			if(new_transp != transp) {
				if(new_transp == GPU_BLEND_SOLID && orig_transp != GPU_BLEND_SOLID)
					GPU_set_material_blend_mode(orig_transp);
				else
					GPU_set_material_blend_mode(new_transp);
				transp = new_transp;
			}
		}

		glShadeModel(drawSmooth? GL_SMOOTH: GL_FLAT);
		for (S=0; S<numVerts; S++) {
			VertData *faceGridData = ccgSubSurf_getFaceGridDataArray(ss, f, S);
			VertData *vda, *vdb;

			if (drawSmooth) {
				for (y=0; y<gridFaces; y++) {
					glBegin(GL_QUAD_STRIP);
					for (x=0; x<gridFaces; x++) {
						vda = &faceGridData[(y+0)*gridSize + x];
						vdb = &faceGridData[(y+1)*gridSize + x];
						
						PASSATTRIB(0, 0, 0);
						glNormal3fv(vda->no);
						glVertex3fv(vda->co);

						PASSATTRIB(0, 1, 1);
						glNormal3fv(vdb->no);
						glVertex3fv(vdb->co);

						if(x != gridFaces-1)
							a++;
					}

					vda = &faceGridData[(y+0)*gridSize + x];
					vdb = &faceGridData[(y+1)*gridSize + x];

					PASSATTRIB(0, 0, 3);
					glNormal3fv(vda->no);
					glVertex3fv(vda->co);

					PASSATTRIB(0, 1, 2);
					glNormal3fv(vdb->no);
					glVertex3fv(vdb->co);

					glEnd();

					a++;
				}
			} else {
				glBegin(GL_QUADS);
				for (y=0; y<gridFaces; y++) {
					for (x=0; x<gridFaces; x++) {
						float *aco = faceGridData[(y+0)*gridSize + x].co;
						float *bco = faceGridData[(y+0)*gridSize + x + 1].co;
						float *cco = faceGridData[(y+1)*gridSize + x + 1].co;
						float *dco = faceGridData[(y+1)*gridSize + x].co;

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

#undef PASSATTRIB

	ccgFaceIterator_free(fi);
}

static void ccgDM_drawFacesGLSL(DerivedMesh *dm, int (*setMaterial)(int, void *attribs)) {
	dm->drawMappedFacesGLSL(dm, setMaterial, NULL, NULL);
}

static void ccgDM_drawFacesColored(DerivedMesh *dm, int useTwoSided, unsigned char *col1, unsigned char *col2) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGFaceIterator *fi = ccgSubSurf_getFaceIterator(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);
	unsigned char *cp1, *cp2;
	int useTwoSide=1;

	cp1= col1;
	if(col2) {
		cp2= col2;
	} else {
		cp2= NULL;
		useTwoSide= 0;
	}

	glShadeModel(GL_SMOOTH);
	if(col1 && col2)
		glEnable(GL_CULL_FACE);

	glBegin(GL_QUADS);
	for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

		for (S=0; S<numVerts; S++) {
			VertData *faceGridData = ccgSubSurf_getFaceGridDataArray(ss, f, S);
			for (y=0; y<gridSize-1; y++) {
				for (x=0; x<gridSize-1; x++) {
					float *a = faceGridData[(y+0)*gridSize + x].co;
					float *b = faceGridData[(y+0)*gridSize + x + 1].co;
					float *c = faceGridData[(y+1)*gridSize + x + 1].co;
					float *d = faceGridData[(y+1)*gridSize + x].co;

					glColor3ub(cp1[3], cp1[2], cp1[1]);
					glVertex3fv(d);
					glColor3ub(cp1[7], cp1[6], cp1[5]);
					glVertex3fv(c);
					glColor3ub(cp1[11], cp1[10], cp1[9]);
					glVertex3fv(b);
					glColor3ub(cp1[15], cp1[14], cp1[13]);
					glVertex3fv(a);

					if (useTwoSide) {
						glColor3ub(cp2[15], cp2[14], cp2[13]);
						glVertex3fv(a);
						glColor3ub(cp2[11], cp2[10], cp2[9]);
						glVertex3fv(b);
						glColor3ub(cp2[7], cp2[6], cp2[5]);
						glVertex3fv(c);
						glColor3ub(cp2[3], cp2[2], cp2[1]);
						glVertex3fv(d);
					}

					if (cp2) cp2+=16;
					cp1+=16;
				}
			}
		}
	}
	glEnd();

	ccgFaceIterator_free(fi);
}

static void ccgDM_drawFacesTex_common(DerivedMesh *dm,
	int (*drawParams)(MTFace *tface, MCol *mcol, int matnr),
	int (*drawParamsMapped)(void *userData, int index),
	void *userData) 
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	MCol *mcol = DM_get_face_data_layer(dm, CD_MCOL);
	MTFace *tf = DM_get_face_data_layer(dm, CD_MTFACE);
	char *faceFlags = DM_get_face_data_layer(dm, CD_FLAGS);
	int i, totface, flag, gridSize = ccgSubSurf_getGridSize(ss);
	int gridFaces = gridSize - 1;

	totface = ccgSubSurf_getNumFaces(ss);
	for(i = 0; i < totface; i++) {
		CCGFace *f = ccgdm->faceMap[i].face;
		int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);
		int drawSmooth, index = ccgDM_getFaceMapIndex(ccgdm, ss, f);
		int origIndex = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(ss, f));
		unsigned char *cp= NULL;
		int mat_nr;

		if(faceFlags) {
			drawSmooth = (faceFlags[origIndex*4] & ME_SMOOTH);
			mat_nr= faceFlags[origIndex*4 + 1];
		}
		else {
			drawSmooth = 1;
			mat_nr= 0;
		}

		if(drawParams)
			flag = drawParams(tf, mcol, mat_nr);
		else
			flag= (drawParamsMapped)? drawParamsMapped(userData, index): 1;
		
		if (flag == 0) { /* flag 0 == the face is hidden or invisible */
			if(tf) tf += gridFaces*gridFaces*numVerts;
			if(mcol) mcol += gridFaces*gridFaces*numVerts*4;
			continue;
		}

		/* flag 1 == use vertex colors */
		if(mcol) {
			if(flag==1) cp= (unsigned char*)mcol;
			mcol += gridFaces*gridFaces*numVerts*4;
		}

		for (S=0; S<numVerts; S++) {
			VertData *faceGridData = ccgSubSurf_getFaceGridDataArray(ss, f, S);
			VertData *a, *b;

			if (drawSmooth) {
				glShadeModel(GL_SMOOTH);
				for (y=0; y<gridFaces; y++) {
					glBegin(GL_QUAD_STRIP);
					for (x=0; x<gridFaces; x++) {
						a = &faceGridData[(y+0)*gridSize + x];
						b = &faceGridData[(y+1)*gridSize + x];

						if(tf) glTexCoord2fv(tf->uv[0]);
						if(cp) glColor3ub(cp[3], cp[2], cp[1]);
						glNormal3fv(a->no);
						glVertex3fv(a->co);

						if(tf) glTexCoord2fv(tf->uv[1]);
						if(cp) glColor3ub(cp[7], cp[6], cp[5]);
						glNormal3fv(b->no);
						glVertex3fv(b->co);
						
						if(x != gridFaces-1) {
							if(tf) tf++;
							if(cp) cp += 16;
						}
					}

					a = &faceGridData[(y+0)*gridSize + x];
					b = &faceGridData[(y+1)*gridSize + x];

					if(tf) glTexCoord2fv(tf->uv[3]);
					if(cp) glColor3ub(cp[15], cp[14], cp[13]);
					glNormal3fv(a->no);
					glVertex3fv(a->co);

					if(tf) glTexCoord2fv(tf->uv[2]);
					if(cp) glColor3ub(cp[11], cp[10], cp[9]);
					glNormal3fv(b->no);
					glVertex3fv(b->co);

					if(tf) tf++;
					if(cp) cp += 16;

					glEnd();
				}
			} else {
				glShadeModel(GL_FLAT);
				glBegin(GL_QUADS);
				for (y=0; y<gridFaces; y++) {
					for (x=0; x<gridFaces; x++) {
						float *a = faceGridData[(y+0)*gridSize + x].co;
						float *b = faceGridData[(y+0)*gridSize + x + 1].co;
						float *c = faceGridData[(y+1)*gridSize + x + 1].co;
						float *d = faceGridData[(y+1)*gridSize + x].co;

						ccgDM_glNormalFast(a, b, c, d);

						if(tf) glTexCoord2fv(tf->uv[1]);
						if(cp) glColor3ub(cp[7], cp[6], cp[5]);
						glVertex3fv(d);

						if(tf) glTexCoord2fv(tf->uv[2]);
						if(cp) glColor3ub(cp[11], cp[10], cp[9]);
						glVertex3fv(c);

						if(tf) glTexCoord2fv(tf->uv[3]);
						if(cp) glColor3ub(cp[15], cp[14], cp[13]);
						glVertex3fv(b);

						if(tf) glTexCoord2fv(tf->uv[0]);
						if(cp) glColor3ub(cp[3], cp[2], cp[1]);
						glVertex3fv(a);

						if(tf) tf++;
						if(cp) cp += 16;
					}
				}
				glEnd();
			}
		}
	}
}

static void ccgDM_drawFacesTex(DerivedMesh *dm, int (*setDrawOptions)(MTFace *tface, MCol *mcol, int matnr))
{
	ccgDM_drawFacesTex_common(dm, setDrawOptions, NULL, NULL);
}

static void ccgDM_drawMappedFacesTex(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void *userData)
{
	ccgDM_drawFacesTex_common(dm, NULL, setDrawOptions, userData);
}

static void ccgDM_drawUVEdges(DerivedMesh *dm)
{

	MFace *mf = dm->getFaceArray(dm);
	MTFace *tf = DM_get_face_data_layer(dm, CD_MTFACE);
	int i;
	
	if (tf) {
		glBegin(GL_LINES);
		for(i = 0; i < dm->numFaceData; i++, mf++, tf++) {
			if(!(mf->flag&ME_HIDE)) {
				glVertex2fv(tf->uv[0]);
				glVertex2fv(tf->uv[1]);
	
				glVertex2fv(tf->uv[1]);
				glVertex2fv(tf->uv[2]);
	
				if(!mf->v4) {
					glVertex2fv(tf->uv[2]);
					glVertex2fv(tf->uv[0]);
				} else {
					glVertex2fv(tf->uv[2]);
					glVertex2fv(tf->uv[3]);
	
					glVertex2fv(tf->uv[3]);
					glVertex2fv(tf->uv[0]);
				}
			}
		}
		glEnd();
	}
}

static void ccgDM_drawMappedFaces(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index, int *drawSmooth_r), void *userData, int useColors) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGFaceIterator *fi = ccgSubSurf_getFaceIterator(ss);
	int i, gridSize = ccgSubSurf_getGridSize(ss);
	char *faceFlags = dm->getFaceDataArray(dm, CD_FLAGS);

	for (i=0; !ccgFaceIterator_isStopped(fi); i++,ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);
		int drawSmooth, index = ccgDM_getFaceMapIndex(ccgdm, ss, f);
		int origIndex;

		origIndex = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(ss, f));

		if(faceFlags) drawSmooth = (faceFlags[origIndex*4] & ME_SMOOTH);
		else drawSmooth = 1;
		
		if (index!=-1) {
			int draw;
			draw = setDrawOptions==NULL ? 1 : setDrawOptions(userData, index, &drawSmooth);
			
			if (draw) {
				if (draw==2) {
		  			glEnable(GL_POLYGON_STIPPLE);
		  			glPolygonStipple(stipple_quarttone);
				}
				
				for (S=0; S<numVerts; S++) {
					VertData *faceGridData = ccgSubSurf_getFaceGridDataArray(ss, f, S);
					if (drawSmooth) {
						glShadeModel(GL_SMOOTH);
						for (y=0; y<gridSize-1; y++) {
							glBegin(GL_QUAD_STRIP);
							for (x=0; x<gridSize; x++) {
								VertData *a = &faceGridData[(y+0)*gridSize + x];
								VertData *b = &faceGridData[(y+1)*gridSize + x];
	
								glNormal3fv(a->no);
								glVertex3fv(a->co);
								glNormal3fv(b->no);
								glVertex3fv(b->co);
							}
							glEnd();
						}
					} else {
						glShadeModel(GL_FLAT);
						glBegin(GL_QUADS);
						for (y=0; y<gridSize-1; y++) {
							for (x=0; x<gridSize-1; x++) {
								float *a = faceGridData[(y+0)*gridSize + x].co;
								float *b = faceGridData[(y+0)*gridSize + x + 1].co;
								float *c = faceGridData[(y+1)*gridSize + x + 1].co;
								float *d = faceGridData[(y+1)*gridSize + x].co;
								float a_cX = c[0]-a[0], a_cY = c[1]-a[1], a_cZ = c[2]-a[2];
								float b_dX = d[0]-b[0], b_dY = d[1]-b[1], b_dZ = d[2]-b[2];
								float no[3];
	
								no[0] = b_dY*a_cZ - b_dZ*a_cY;
								no[1] = b_dZ*a_cX - b_dX*a_cZ;
								no[2] = b_dX*a_cY - b_dY*a_cX;
								glNormal3fv(no);
	
								glVertex3fv(d);
								glVertex3fv(c);
								glVertex3fv(b);
								glVertex3fv(a);
							}
						}
						glEnd();
					}
				}
				if (draw==2)
					glDisable(GL_POLYGON_STIPPLE);
			}
		}
	}

	ccgFaceIterator_free(fi);
}
static void ccgDM_drawMappedEdges(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void *userData) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGEdgeIterator *ei = ccgSubSurf_getEdgeIterator(ss);
	int i, useAging, edgeSize = ccgSubSurf_getEdgeSize(ss);

	ccgSubSurf_getUseAgeCounts(ss, &useAging, NULL, NULL, NULL);

	for (; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);
		VertData *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);
		int index = ccgDM_getEdgeMapIndex(ccgdm, ss, e);

		glBegin(GL_LINE_STRIP);
		if (index!=-1 && (!setDrawOptions || setDrawOptions(userData, index))) {
			if (useAging && !(G.f&G_BACKBUFSEL)) {
				int ageCol = 255-ccgSubSurf_getEdgeAge(ss, e)*4;
				glColor3ub(0, ageCol>0?ageCol:0, 0);
			}

			for (i=0; i<edgeSize-1; i++) {
				glVertex3fv(edgeData[i].co);
				glVertex3fv(edgeData[i+1].co);
			}
		}
		glEnd();
	}

	ccgEdgeIterator_free(ei);
}
static void ccgDM_drawMappedEdgesInterp(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void (*setDrawInterpOptions)(void *userData, int index, float t), void *userData) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGEdgeIterator *ei = ccgSubSurf_getEdgeIterator(ss);
	int i, useAging, edgeSize = ccgSubSurf_getEdgeSize(ss);

	ccgSubSurf_getUseAgeCounts(ss, &useAging, NULL, NULL, NULL);

	for (; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);
		VertData *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);
		int index = ccgDM_getEdgeMapIndex(ccgdm, ss, e);

		glBegin(GL_LINE_STRIP);
		if (index!=-1 && (!setDrawOptions || setDrawOptions(userData, index))) {
			for (i=0; i<edgeSize; i++) {
				setDrawInterpOptions(userData, index, (float) i/(edgeSize-1));

				if (useAging && !(G.f&G_BACKBUFSEL)) {
					int ageCol = 255-ccgSubSurf_getEdgeAge(ss, e)*4;
					glColor3ub(0, ageCol>0?ageCol:0, 0);
				}

				glVertex3fv(edgeData[i].co);
			}
		}
		glEnd();
	}

	ccgEdgeIterator_free(ei);
}
static void ccgDM_foreachMappedFaceCenter(DerivedMesh *dm, void (*func)(void *userData, int index, float *co, float *no), void *userData) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGFaceIterator *fi = ccgSubSurf_getFaceIterator(ss);

	for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		int index = ccgDM_getFaceMapIndex(ccgdm, ss, f);

		if (index!=-1) {
				/* Face center data normal isn't updated atm. */
			VertData *vd = ccgSubSurf_getFaceGridData(ss, f, 0, 0, 0);

			func(userData, index, vd->co, vd->no);
		}
	}

	ccgFaceIterator_free(fi);
}

static void ccgDM_release(DerivedMesh *dm) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;

	if (DM_release(dm)) {
		MEM_freeN(ccgdm->vertMap);
		MEM_freeN(ccgdm->edgeMap);
		MEM_freeN(ccgdm->faceMap);
		MEM_freeN(ccgdm);
	}
}

static CCGDerivedMesh *getCCGDerivedMesh(CCGSubSurf *ss,
                                         int drawInteriorEdges,
                                         int useSubsurfUv,
                                         DerivedMesh *dm)
{
	CCGDerivedMesh *ccgdm = MEM_callocN(sizeof(*ccgdm), "ccgdm");
	CCGVertIterator *vi;
	CCGEdgeIterator *ei;
	CCGFaceIterator *fi;
	int index, totvert, totedge, totface;
	int i;
	int vertNum, edgeNum, faceNum;
	int *vertOrigIndex, *edgeOrigIndex, *faceOrigIndex;
	int *edgeFlags;
	char *faceFlags;
	int edgeSize;
	int gridSize;
	int gridFaces;
	int gridSideVerts;
	int gridInternalVerts;
	int gridSideEdges;
	int gridInternalEdges;
	MVert *mvert = NULL;
	MEdge *medge = NULL;
	MFace *mface = NULL;
	FaceVertWeight *qweight, *tweight;

	DM_from_template(&ccgdm->dm, dm, ccgSubSurf_getNumFinalVerts(ss),
					 ccgSubSurf_getNumFinalEdges(ss),
					 ccgSubSurf_getNumFinalFaces(ss));
	DM_add_face_layer(&ccgdm->dm, CD_FLAGS, CD_CALLOC, NULL);
	DM_add_edge_layer(&ccgdm->dm, CD_FLAGS, CD_CALLOC, NULL);

	CustomData_set_layer_flag(&ccgdm->dm.faceData, CD_FLAGS, CD_FLAG_NOCOPY);
	CustomData_set_layer_flag(&ccgdm->dm.edgeData, CD_FLAGS, CD_FLAG_NOCOPY);

	ccgdm->dm.getMinMax = ccgDM_getMinMax;
	ccgdm->dm.getNumVerts = ccgDM_getNumVerts;
	ccgdm->dm.getNumFaces = ccgDM_getNumFaces;

	ccgdm->dm.getNumEdges = ccgDM_getNumEdges;
	ccgdm->dm.getVert = ccgDM_getFinalVert;
	ccgdm->dm.getEdge = ccgDM_getFinalEdge;
	ccgdm->dm.getFace = ccgDM_getFinalFace;
	ccgdm->dm.copyVertArray = ccgDM_copyFinalVertArray;
	ccgdm->dm.copyEdgeArray = ccgDM_copyFinalEdgeArray;
	ccgdm->dm.copyFaceArray = ccgDM_copyFinalFaceArray;
	ccgdm->dm.getVertData = DM_get_vert_data;
	ccgdm->dm.getEdgeData = DM_get_edge_data;
	ccgdm->dm.getFaceData = DM_get_face_data;
	ccgdm->dm.getVertDataArray = DM_get_vert_data_layer;
	ccgdm->dm.getEdgeDataArray = DM_get_edge_data_layer;
	ccgdm->dm.getFaceDataArray = DM_get_face_data_layer;

	ccgdm->dm.getVertCos = ccgdm_getVertCos;
	ccgdm->dm.foreachMappedVert = ccgDM_foreachMappedVert;
	ccgdm->dm.foreachMappedEdge = ccgDM_foreachMappedEdge;
	ccgdm->dm.foreachMappedFaceCenter = ccgDM_foreachMappedFaceCenter;
	
	ccgdm->dm.drawVerts = ccgDM_drawVerts;
	ccgdm->dm.drawEdges = ccgDM_drawEdges;
	ccgdm->dm.drawLooseEdges = ccgDM_drawLooseEdges;
	ccgdm->dm.drawFacesSolid = ccgDM_drawFacesSolid;
	ccgdm->dm.drawFacesColored = ccgDM_drawFacesColored;
	ccgdm->dm.drawFacesTex = ccgDM_drawFacesTex;
	ccgdm->dm.drawFacesGLSL = ccgDM_drawFacesGLSL;
	ccgdm->dm.drawMappedFaces = ccgDM_drawMappedFaces;
	ccgdm->dm.drawMappedFacesTex = ccgDM_drawMappedFacesTex;
	ccgdm->dm.drawMappedFacesGLSL = ccgDM_drawMappedFacesGLSL;
	ccgdm->dm.drawUVEdges = ccgDM_drawUVEdges;

	ccgdm->dm.drawMappedEdgesInterp = ccgDM_drawMappedEdgesInterp;
	ccgdm->dm.drawMappedEdges = ccgDM_drawMappedEdges;
	
	ccgdm->dm.release = ccgDM_release;
	
	ccgdm->ss = ss;
	ccgdm->drawInteriorEdges = drawInteriorEdges;
	ccgdm->useSubsurfUv = useSubsurfUv;

	totvert = ccgSubSurf_getNumVerts(ss);
	ccgdm->vertMap = MEM_mallocN(totvert * sizeof(*ccgdm->vertMap), "vertMap");
	vi = ccgSubSurf_getVertIterator(ss);
	for(; !ccgVertIterator_isStopped(vi); ccgVertIterator_next(vi)) {
		CCGVert *v = ccgVertIterator_getCurrent(vi);

		ccgdm->vertMap[GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ss, v))].vert = v;
	}
	ccgVertIterator_free(vi);

	totedge = ccgSubSurf_getNumEdges(ss);
	ccgdm->edgeMap = MEM_mallocN(totedge * sizeof(*ccgdm->edgeMap), "edgeMap");
	ei = ccgSubSurf_getEdgeIterator(ss);
	for(; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);

		ccgdm->edgeMap[GET_INT_FROM_POINTER(ccgSubSurf_getEdgeEdgeHandle(ss, e))].edge = e;
	}

	totface = ccgSubSurf_getNumFaces(ss);
	ccgdm->faceMap = MEM_mallocN(totface * sizeof(*ccgdm->faceMap), "faceMap");
	fi = ccgSubSurf_getFaceIterator(ss);
	for(; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);

		ccgdm->faceMap[GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(ss, f))].face = f;
	}
	ccgFaceIterator_free(fi);

	edgeSize = ccgSubSurf_getEdgeSize(ss);
	gridSize = ccgSubSurf_getGridSize(ss);
	gridFaces = gridSize - 1;
	gridSideVerts = gridSize - 2;
	gridInternalVerts = gridSideVerts * gridSideVerts;
	gridSideEdges = gridSize - 1;
	gridInternalEdges = (gridSideEdges - 1) * gridSideEdges * 2; 

	calc_ss_weights(gridFaces, &qweight, &tweight);

	vertNum = 0;
	edgeNum = 0;
	faceNum = 0;

	mvert = dm->getVertArray(dm);
	medge = dm->getEdgeArray(dm);
	mface = dm->getFaceArray(dm);

	vertOrigIndex = DM_get_vert_data_layer(&ccgdm->dm, CD_ORIGINDEX);
	edgeOrigIndex = DM_get_edge_data_layer(&ccgdm->dm, CD_ORIGINDEX);
	faceOrigIndex = DM_get_face_data_layer(&ccgdm->dm, CD_ORIGINDEX);

	faceFlags = DM_get_face_data_layer(&ccgdm->dm, CD_FLAGS);

	for(index = 0; index < totface; ++index) {
		CCGFace *f = ccgdm->faceMap[index].face;
		int numVerts = ccgSubSurf_getFaceNumVerts(ss, f);
		int numFinalEdges = numVerts * (gridSideEdges + gridInternalEdges);
		int mapIndex = ccgDM_getFaceMapIndex(ccgdm, ss, f);
		int origIndex = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(ss, f));
		FaceVertWeight *weight = (numVerts == 4) ? qweight : tweight;
		int S, x, y;
		int vertIdx[4];

		ccgdm->faceMap[index].startVert = vertNum;
		ccgdm->faceMap[index].startEdge = edgeNum;
		ccgdm->faceMap[index].startFace = faceNum;

		/* set the face base vert */
		*((int*)ccgSubSurf_getFaceUserData(ss, f)) = vertNum;

		for(S = 0; S < numVerts; S++) {
			CCGVert *v = ccgSubSurf_getFaceVert(ss, f, S);

			vertIdx[S] = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ss, v));
		}

		DM_interp_vert_data(dm, &ccgdm->dm, vertIdx, weight[0][0],
		                    numVerts, vertNum);
		*vertOrigIndex = ORIGINDEX_NONE;
		++vertOrigIndex;
		++vertNum;

		for(S = 0; S < numVerts; S++) {
			int prevS = (S - 1 + numVerts) % numVerts;
			int nextS = (S + 1) % numVerts;
			int otherS = (numVerts == 4) ? (S + 2) % numVerts : 3;
			for(x = 1; x < gridFaces; x++) {
				float w[4];
				w[prevS]  = weight[x][0][0];
				w[S]      = weight[x][0][1];
				w[nextS]  = weight[x][0][2];
				w[otherS] = weight[x][0][3];
				DM_interp_vert_data(dm, &ccgdm->dm, vertIdx, w,
				                    numVerts, vertNum);
				*vertOrigIndex = ORIGINDEX_NONE;
				++vertOrigIndex;
				++vertNum;
			}
		}

		for(S = 0; S < numVerts; S++) {
			int prevS = (S - 1 + numVerts) % numVerts;
			int nextS = (S + 1) % numVerts;
			int otherS = (numVerts == 4) ? (S + 2) % numVerts : 3;
			for(y = 1; y < gridFaces; y++) {
				for(x = 1; x < gridFaces; x++) {
					float w[4];
					w[prevS]  = weight[y * gridFaces + x][0][0];
					w[S]      = weight[y * gridFaces + x][0][1];
					w[nextS]  = weight[y * gridFaces + x][0][2];
					w[otherS] = weight[y * gridFaces + x][0][3];
					DM_interp_vert_data(dm, &ccgdm->dm, vertIdx, w,
					                    numVerts, vertNum);
					*vertOrigIndex = ORIGINDEX_NONE;
					++vertOrigIndex;
					++vertNum;
				}
			}
		}

		for(i = 0; i < numFinalEdges; ++i)
			*(int *)DM_get_edge_data(&ccgdm->dm, edgeNum + i,
			                         CD_ORIGINDEX) = ORIGINDEX_NONE;

		for(S = 0; S < numVerts; S++) {
			int prevS = (S - 1 + numVerts) % numVerts;
			int nextS = (S + 1) % numVerts;
			int otherS = (numVerts == 4) ? (S + 2) % numVerts : 3;

			weight = (numVerts == 4) ? qweight : tweight;

			for(y = 0; y < gridFaces; y++) {
				for(x = 0; x < gridFaces; x++) {
					FaceVertWeight w;
					int j;

					for(j = 0; j < 4; ++j) {
						w[j][prevS]  = (*weight)[j][0];
						w[j][S]      = (*weight)[j][1];
						w[j][nextS]  = (*weight)[j][2];
						w[j][otherS] = (*weight)[j][3];
					}

					DM_interp_face_data(dm, &ccgdm->dm, &origIndex, NULL,
					                    &w, 1, faceNum);
					weight++;

					*faceOrigIndex = mapIndex;

					++faceOrigIndex;
					++faceNum;
				}
			}
		}

		faceFlags[index*4] = mface[origIndex].flag;
		faceFlags[index*4 + 1] = mface[origIndex].mat_nr;

		edgeNum += numFinalEdges;
	}

	if(useSubsurfUv) {
		CustomData *fdata = &ccgdm->dm.faceData;
		CustomData *dmfdata = &dm->faceData;
		int numlayer = CustomData_number_of_layers(fdata, CD_MTFACE);
		int dmnumlayer = CustomData_number_of_layers(dmfdata, CD_MTFACE);

		for (i=0; i<numlayer && i<dmnumlayer; i++)
			set_subsurf_uv(ss, dm, &ccgdm->dm, i);
	}

	edgeFlags = DM_get_edge_data_layer(&ccgdm->dm, CD_FLAGS);

	for(index = 0; index < totedge; ++index) {
		CCGEdge *e = ccgdm->edgeMap[index].edge;
		int numFinalEdges = edgeSize - 1;
		int mapIndex = ccgDM_getEdgeMapIndex(ccgdm, ss, e);
		int x;
		int vertIdx[2];
		int edgeIdx = GET_INT_FROM_POINTER(ccgSubSurf_getEdgeEdgeHandle(ss, e));

		CCGVert *v;
		v = ccgSubSurf_getEdgeVert0(ss, e);
		vertIdx[0] = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ss, v));
		v = ccgSubSurf_getEdgeVert1(ss, e);
		vertIdx[1] = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ss, v));

		ccgdm->edgeMap[index].startVert = vertNum;
		ccgdm->edgeMap[index].startEdge = edgeNum;

		/* set the edge base vert */
		*((int*)ccgSubSurf_getEdgeUserData(ss, e)) = vertNum;

		for(x = 1; x < edgeSize - 1; x++) {
			float w[2];
			w[1] = (float) x / (edgeSize - 1);
			w[0] = 1 - w[1];
			DM_interp_vert_data(dm, &ccgdm->dm, vertIdx, w, 2, vertNum);
			*vertOrigIndex = ORIGINDEX_NONE;
			++vertOrigIndex;
			++vertNum;
		}

		for(i = 0; i < numFinalEdges; ++i) {
			if(edgeIdx >= 0 && edgeFlags)
					edgeFlags[edgeNum + i] = medge[edgeIdx].flag;

			*(int *)DM_get_edge_data(&ccgdm->dm, edgeNum + i,
			                         CD_ORIGINDEX) = mapIndex;
		}

		edgeNum += numFinalEdges;
	}

	for(index = 0; index < totvert; ++index) {
		CCGVert *v = ccgdm->vertMap[index].vert;
		int mapIndex = ccgDM_getVertMapIndex(ccgdm, ccgdm->ss, v);
		int vertIdx;

		vertIdx = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ss, v));

		ccgdm->vertMap[index].startVert = vertNum;

		/* set the vert base vert */
		*((int*) ccgSubSurf_getVertUserData(ss, v)) = vertNum;

		DM_copy_vert_data(dm, &ccgdm->dm, vertIdx, vertNum, 1);

		*vertOrigIndex = mapIndex;
		++vertOrigIndex;
		++vertNum;
	}

	MEM_freeN(qweight);
	MEM_freeN(tweight);

	return ccgdm;
}

/***/

struct DerivedMesh *subsurf_make_derived_from_derived(
                        struct DerivedMesh *dm,
                        struct SubsurfModifierData *smd,
                        int useRenderParams, float (*vertCos)[3],
                        int isFinalCalc, int editMode)
{
	int useSimple = smd->subdivType == ME_SIMPLE_SUBSURF;
	int useAging = smd->flags & eSubsurfModifierFlag_DebugIncr;
	int useSubsurfUv = smd->flags & eSubsurfModifierFlag_SubsurfUv;
	int drawInteriorEdges = !(smd->flags & eSubsurfModifierFlag_ControlEdges);
	DerivedMesh *result;

	if(editMode) {
		smd->emCache = _getSubSurf(smd->emCache, smd->levels, useAging, 0,
		                           useSimple);
		ss_sync_from_derivedmesh(smd->emCache, dm, vertCos, useSimple);

		return (DerivedMesh *)getCCGDerivedMesh(smd->emCache,
		                                        drawInteriorEdges,
	                                            useSubsurfUv, dm);
	} else if(useRenderParams) {
		/* Do not use cache in render mode. */
		CCGSubSurf *ss;
		int levels;
		
		levels= get_render_subsurf_level(&G.scene->r, smd->renderLevels);
		if(levels == 0)
			return dm;
		
		ss = _getSubSurf(NULL, levels, 0, 1, useSimple);

		ss_sync_from_derivedmesh(ss, dm, vertCos, useSimple);

		result = ss_to_cdderivedmesh(ss, 0, drawInteriorEdges,
		                             useSubsurfUv, dm);

		ccgSubSurf_free(ss);
		
		return result;
	} else {
		int useIncremental = (smd->flags & eSubsurfModifierFlag_Incremental);
		int useAging = smd->flags & eSubsurfModifierFlag_DebugIncr;
		CCGSubSurf *ss;
		
		/* It is quite possible there is a much better place to do this. It
		 * depends a bit on how rigourously we expect this function to never
		 * be called in editmode. In semi-theory we could share a single
		 * cache, but the handles used inside and outside editmode are not
		 * the same so we would need some way of converting them. Its probably
		 * not worth the effort. But then why am I even writing this long
		 * comment that no one will read? Hmmm. - zr
		 */
		if(smd->emCache) {
			ccgSubSurf_free(smd->emCache);
			smd->emCache = NULL;
		}

		if(useIncremental && isFinalCalc) {
			smd->mCache = ss = _getSubSurf(smd->mCache, smd->levels,
			                               useAging, 0, useSimple);

			ss_sync_from_derivedmesh(ss, dm, vertCos, useSimple);


			return ss_to_cdderivedmesh(ss, 0, drawInteriorEdges,
		                               useSubsurfUv, dm);

			/*return (DerivedMesh *)getCCGDerivedMesh(smd->mCache,
		                                        drawInteriorEdges,
	                                            useSubsurfUv, dm);*/
		} else {
			if (smd->mCache && isFinalCalc) {
				ccgSubSurf_free(smd->mCache);
				smd->mCache = NULL;
			}

			ss = _getSubSurf(NULL, smd->levels, 0, 1, useSimple);
			ss_sync_from_derivedmesh(ss, dm, vertCos, useSimple);

			/*smd->mCache = ss;
			result = (DerivedMesh *)getCCGDerivedMesh(smd->mCache,
		                                        drawInteriorEdges,
	                                            useSubsurfUv, dm);*/

			result = ss_to_cdderivedmesh(ss, 0, drawInteriorEdges,
			                             useSubsurfUv, dm);

			ccgSubSurf_free(ss);

			return result;
		}
	}
}

void subsurf_calculate_limit_positions(Mesh *me, float (*positions_r)[3]) 
{
	/* Finds the subsurf limit positions for the verts in a mesh 
	 * and puts them in an array of floats. Please note that the 
	 * calculated vert positions is incorrect for the verts 
	 * on the boundary of the mesh.
	 */
	CCGSubSurf *ss = _getSubSurf(NULL, 1, 0, 1, 0);
	float edge_sum[3], face_sum[3];
	CCGVertIterator *vi;
	DerivedMesh *dm = CDDM_from_mesh(me, NULL);

	ss_sync_from_derivedmesh(ss, dm, NULL, 0);

	vi = ccgSubSurf_getVertIterator(ss);
	for (; !ccgVertIterator_isStopped(vi); ccgVertIterator_next(vi)) {
		CCGVert *v = ccgVertIterator_getCurrent(vi);
		int idx = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ss, v));
		int N = ccgSubSurf_getVertNumEdges(ss, v);
		int numFaces = ccgSubSurf_getVertNumFaces(ss, v);
		float *co;
		int i;
                
		edge_sum[0]= edge_sum[1]= edge_sum[2]= 0.0;
		face_sum[0]= face_sum[1]= face_sum[2]= 0.0;

		for (i=0; i<N; i++) {
			CCGEdge *e = ccgSubSurf_getVertEdge(ss, v, i);
			VecAddf(edge_sum, edge_sum, ccgSubSurf_getEdgeData(ss, e, 1));
		}
		for (i=0; i<numFaces; i++) {
			CCGFace *f = ccgSubSurf_getVertFace(ss, v, i);
			VecAddf(face_sum, face_sum, ccgSubSurf_getFaceCenterData(ss, f));
		}

		/* ad-hoc correction for boundary vertices, to at least avoid them
		   moving completely out of place (brecht) */
		if(numFaces && numFaces != N)
			VecMulf(face_sum, (float)N/(float)numFaces);

		co = ccgSubSurf_getVertData(ss, v);
		positions_r[idx][0] = (co[0]*N*N + edge_sum[0]*4 + face_sum[0])/(N*(N+5));
		positions_r[idx][1] = (co[1]*N*N + edge_sum[1]*4 + face_sum[1])/(N*(N+5));
		positions_r[idx][2] = (co[2]*N*N + edge_sum[2]*4 + face_sum[2])/(N*(N+5));
	}
	ccgVertIterator_free(vi);

	ccgSubSurf_free(ss);

	dm->release(dm);
}

