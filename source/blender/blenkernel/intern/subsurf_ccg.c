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

#include "BKE_cdderivedmesh.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_scene.h"
#include "BKE_subsurf.h"
#include "BKE_tessmesh.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_arithb.h"
#include "BLI_linklist.h"
#include "BLI_memarena.h"
#include "BLI_edgehash.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

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

	CSubSurf *ss;
	int drawInteriorEdges, useSubsurfUv;

	struct {int startVert; CCVert *vert;} *vertMap;
	struct {int startVert; int startEdge; CCEdge *edge;} *edgeMap;
	struct {int startVert; int startEdge;
	        int startFace; CCFace *face;} *faceMap;
};

typedef struct CCGDerivedMesh CCGDerivedMesh;

static int cgdm_getVertMapIndex(CSubSurf *ss, CCVert *v);
static int cgdm_getEdgeMapIndex(CSubSurf *ss, CCEdge *e);
static int cgdm_getFaceMapIndex(CSubSurf *ss, CCFace *f);
static CCGDerivedMesh *getCCGDerivedMesh(CSubSurf *ss,
                                         int drawInteriorEdges,
                                         int useSubsurfUv,
                                         DerivedMesh *dm);

///

static void *arena_alloc(CCAllocHDL a, int numBytes) {
	return BLI_memarena_alloc(a, numBytes);
}
static void *arena_realloc(CCAllocHDL a, void *ptr, int newSize, int oldSize) {
	void *p2 = BLI_memarena_alloc(a, newSize);
	if (ptr) {
		memcpy(p2, ptr, oldSize);
	}
	return p2;
}
static void arena_free(CCAllocHDL a, void *ptr) {
}
static void arena_release(CCAllocHDL a) {
	BLI_memarena_free(a);
}

static CSubSurf *_getSubSurf(CSubSurf *prevSS, int subdivLevels, int useAging, int useArena, int useFlatSubdiv) {
	CCGMeshIFC ifc;
	CSubSurf *ccgSS;

		/* subdivLevels==0 is not allowed */
	subdivLevels = MAX2(subdivLevels, 1);

	if (prevSS) {
		int oldUseAging;

		useAging = !!useAging;
		CCS_getUseAgeCounts(prevSS, &oldUseAging, NULL, NULL, NULL);

		if (oldUseAging!=useAging) {
			CCS_free(prevSS);
		} else {
			CCS_setSubdivisionLevels(prevSS, subdivLevels);

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
		CCAllocHDL allocator = BLI_memarena_new((1<<16));

		allocatorIFC.alloc = arena_alloc;
		allocatorIFC.realloc = arena_realloc;
		allocatorIFC.free = arena_free;
		allocatorIFC.release = arena_release;

		ccgSS = CCS_new(&ifc, subdivLevels, &allocatorIFC, allocator);
	} else {
		ccgSS = CCS_new(&ifc, subdivLevels, NULL, NULL);
	}

	if (useAging) {
		CCS_setUseAgeCounts(ccgSS, 1, 8, 8, 8);
	}

	CCS_setCalcVertexNormals(ccgSS, 1, BLI_STRUCT_OFFSET(VertData, no));

	return ccgSS;
}

static int getEdgeIndex(CSubSurf *ss, CCEdge *e, int x, int edgeSize) {
	CCVert *v0 = CCS_getEdgeVert0(e);
	CCVert *v1 = CCS_getEdgeVert1(e);
	int v0idx = *((int*) CCS_getVertUserData(ss, v0));
	int v1idx = *((int*) CCS_getVertUserData(ss, v1));
	int edgeBase = *((int*) CCS_getEdgeUserData(ss, e));

	if (x==0) {
		return v0idx;
	} else if (x==edgeSize-1) {
		return v1idx;
	} else {
		return edgeBase + x-1;
	}
}

static int getFaceIndex(CSubSurf *ss, CCFace *f, int S, int x, int y, int edgeSize, int gridSize) {
	int faceBase = *((int*) CCS_getFaceUserData(ss, f));
	int numVerts = CCS_getFaceNumVerts(f);

	if (x==gridSize-1 && y==gridSize-1) {
		CCVert *v = CCS_getFaceVert(ss, f, S);
		return *((int*) CCS_getVertUserData(ss, v));
	} else if (x==gridSize-1) {
		CCVert *v = CCS_getFaceVert(ss, f, S);
		CCEdge *e = CCS_getFaceEdge(ss, f, S);
		int edgeBase = *((int*) CCS_getEdgeUserData(ss, e));
		if (v==CCS_getEdgeVert0(e)) {
			return edgeBase + (gridSize-1-y)-1;
		} else {
			return edgeBase + (edgeSize-2-1)-((gridSize-1-y)-1);
		}
	} else if (y==gridSize-1) {
		CCVert *v = CCS_getFaceVert(ss, f, S);
		CCEdge *e = CCS_getFaceEdge(ss, f, (S+numVerts-1)%numVerts);
		int edgeBase = *((int*) CCS_getEdgeUserData(ss, e));
		if (v==CCS_getEdgeVert0(e)) {
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

static void get_face_uv_map_vert(UvVertMap *vmap, struct MFace *mf, int fi, CCVertHDL *fverts) {
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

static int ss_sync_from_uv(CSubSurf *ss, CSubSurf *origss, DerivedMesh *dm, MTFace *tface) {
#if 0
	MFace *mface = dm->getTessFaceArray(dm);
	MVert *mvert = dm->getVertArray(dm);
	int totvert = dm->getNumVerts(dm);
	int totface = dm->getNumTessFaces(dm);
	int i, j, seam;
	UvMapVert *v;
	UvVertMap *vmap;
	float limit[2];
	CCVertHDL fverts[4];
	EdgeHash *ehash;
	float creaseFactor = (float)CCS_getSubdivisionLevels(ss);

	limit[0]= limit[1]= STD_UV_CONNECT_LIMIT;
	vmap= make_uv_vert_map(mface, tface, totface, totvert, 0, limit);
	if (!vmap)
		return 0;
	
	CCS_initFullSync(ss);

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
				CCVert *ssv;
				CCVertHDL vhdl = SET_INT_IN_POINTER(v->f*4 + v->tfindex);
				float uv[3];

				uv[0]= (tface+v->f)->uv[v->tfindex][0];
				uv[1]= (tface+v->f)->uv[v->tfindex][1];
				uv[2]= 0.0f;

				CCS_syncVert(ss, vhdl, uv, seam, &ssv);
			}
		}
	}

	/* create edges */
	ehash = BLI_edgehash_new();

	for (i=0; i<totface; i++) {
		MFace *mf = &((MFace*) mface)[i];
		int nverts= mf->v4? 4: 3;
		CCFace *origf= CCS_getFace(origss, SET_INT_IN_POINTER(i));
		unsigned int *fv = &mf->v1;

		get_face_uv_map_vert(vmap, mf, i, fverts);

		for (j=0; j<nverts; j++) {
			int v0 = GET_INT_FROM_POINTER(fverts[j]);
			int v1 = GET_INT_FROM_POINTER(fverts[(j+1)%nverts]);
			MVert *mv0 = mvert + *(fv+j);
			MVert *mv1 = mvert + *(fv+((j+1)%nverts));

			if (!BLI_edgehash_haskey(ehash, v0, v1)) {
				CCEdge *e, *orige= CCS_getFaceEdge(origss, origf, j);
				CCEdgeHDL ehdl= SET_INT_IN_POINTER(i*4 + j);
				float crease;

				if ((mv0->flag&mv1->flag) & ME_VERT_MERGED)
					crease = creaseFactor;
				else
					crease = CCS_getEdgeCrease(orige);

				CCS_syncEdge(ss, ehdl, fverts[j], fverts[(j+1)%nverts], crease, &e);
				BLI_edgehash_insert(ehash, v0, v1, NULL);
			}
		}
	}

	BLI_edgehash_free(ehash, NULL);

	/* create faces */
	for (i=0; i<totface; i++) {
		MFace *mf = &((MFace*) mface)[i];
		int nverts= mf->v4? 4: 3;
		CCFace *f;

		get_face_uv_map_vert(vmap, mf, i, fverts);
		CCS_syncFace(ss, SET_INT_IN_POINTER(i), nverts, fverts, &f);
	}

	free_uv_vert_map(vmap);
	CCS_processSync(ss);

#endif
	return 1;
}

static void set_subsurf_uv(CSubSurf *ss, DerivedMesh *dm, DerivedMesh *result, int n)
{
	CSubSurf *uvss;
	CCFace **faceMap;
	MTFace *tf;
	CCFaceIterator *fi;
	int index, gridSize, gridFaces, edgeSize, totface, x, y, S;
	MTFace *dmtface = CustomData_get_layer_n(&dm->faceData, CD_MTFACE, n);
	MTFace *tface = CustomData_get_layer_n(&result->faceData, CD_MTFACE, n);

	if(!dmtface || !tface)
		return;

	/* create a CCGSubsurf from uv's */
	uvss = _getSubSurf(NULL, CCS_getSubdivisionLevels(ss), 0, 1, 0);

	if(!ss_sync_from_uv(uvss, ss, dm, dmtface)) {
		CCS_free(uvss);
		return;
	}

	/* get some info from CCGSubsurf */
	totface = CCS_getNumFaces(uvss);
	edgeSize = CCS_getEdgeSize(uvss);
	gridSize = CCS_getGridSize(uvss);
	gridFaces = gridSize - 1;

	/* make a map from original faces to CCFaces */
	faceMap = MEM_mallocN(totface*sizeof(*faceMap), "facemapuv");

	fi = CCS_getFaceIterator(uvss);
	for(; !CCFIter_isStopped(fi); CCFIter_next(fi)) {
		CCFace *f = CCFIter_getCurrent(fi);
		faceMap[GET_INT_FROM_POINTER(CCS_getFaceFaceHandle(uvss, f))] = f;
	}
	CCFIter_free(fi);

	/* load coordinates from uvss into tface */
	tf= tface;

	for(index = 0; index < totface; index++) {
		CCFace *f = faceMap[index];
		int numVerts = CCS_getFaceNumVerts(f);

		for (S=0; S<numVerts; S++) {
			VertData *faceGridData= CCS_getFaceGridDataArray(uvss, f, S);

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

	CCS_free(uvss);
	MEM_freeN(faceMap);
}

#if 0
static unsigned int ss_getEdgeFlags(CSubSurf *ss, CCEdge *e, int ssFromEditmesh, DispListMesh *dlm, MEdge *medge, MTFace *tface)
{
	unsigned int flags = 0;
	int N = CCS_getEdgeNumFaces(e);

	if (!N) flags |= ME_LOOSEEDGE;

	if (ssFromEditmesh) {
		EditEdge *eed = CCS_getEdgeEdgeHandle(e);

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
		void *tmp = MEM_callocN(sizeof(FaceVertWeightEntry)*(faceLen+1), "weight table alloc 2");
		
		if (wtable->len) {
			memcpy(tmp, wtable->weight_table, sizeof(FaceVertWeightEntry)*wtable->len);
			MEM_freeN(wtable->weight_table);
		}
		
		wtable->weight_table = tmp;
		wtable->len = faceLen+1;
	}

	if (!wtable->weight_table[faceLen].valid) {
		wtable->weight_table[faceLen].valid = 1;
		wtable->weight_table[faceLen].w = w = MEM_callocN(sizeof(float)*faceLen*faceLen*(gridCuts+2)*(gridCuts+2), "weight table alloc");
		fac = 1.0 / (float)faceLen;

		for (i=0; i<faceLen; i++) {
			for (x=0; x<gridCuts+2; x++) {
				for (y=0; y<gridCuts+2; y++) {
					fx = 0.5f - (float)x / (float)(gridCuts+1) / 2.0f;
					fy = 0.5f - (float)y / (float)(gridCuts+1) / 2.0f;
				
					fac2 = faceLen - 4;
					w1 = (1.0f - fx) * (1.0f - fy) + (-fac2*fx*fy*fac);
					w2 = (1.0f - fx + fac2*fx*-fac) * (fy);
					w4 = (fx) * (1.0 - fy + -fac2*fy*fac);
					
					fac2 = 1.0 - (w1+w2+w4);
					fac2 = fac2 / (float)(faceLen-3);
					for (j=0; j<faceLen; j++)
						w[j] = fac2;
					
					w[i] = w1;
					w[(i-1+faceLen)%faceLen] = w2;
					w[(i+1)%faceLen] = w4;

					w += faceLen;
				}
			}
		}
	}

	return wtable->weight_table[faceLen].w;
#if 0
	/*ensure we have at least the triangle and quad weights*/
	if (wtable->len < 4) {
		wtable->weight_table = MEM_callocN(sizeof(FaceVertWeightEntry)*5, "weight table alloc");
		wtable->len = 5;

		calc_ss_weights(gridFaces, &wtable->weight_table[4].weight, &wtable->weight_table[3].weight);
		wtable->weight_table[4].valid = wtable->weight_table[3].valid = 1;
	}
	
	if (wtable->len <= faceLen) {
		void *tmp = MEM_callocN(sizeof(FaceVertWeightEntry)*(faceLen+1), "weight table alloc 2");
		
		memcpy(tmp, wtable->weight_table, sizeof(FaceVertWeightEntry)*wtable->len);
		MEM_freeN(wtable->weight_table);
		
		wtable->weight_table = tmp;
		wtable->len = faceLen+1;
	}

	return wtable->weight_table[faceLen].weight;
#endif
}

void free_ss_weights(WeightTable *wtable)
{
	int i;

	for (i=0; i<wtable->len; i++) {
		if (wtable->weight_table[i].valid)
			MEM_freeN(wtable->weight_table[i].w);
	}
}

static DerivedMesh *ss_to_cdderivedmesh(CSubSurf *ss, int ssFromEditmesh,
                                 int drawInteriorEdges, int useSubsurfUv,
                                 DerivedMesh *dm, MultiresSubsurf *ms)
{
	DerivedMesh *cgdm, *result;

	cgdm = getCCGDerivedMesh(ss, drawInteriorEdges, useSubsurfUv, dm);
	result = CDDM_copy(cgdm);
	cgdm->needsFree = 1;
	cgdm->release(cgdm);

	return result;
#if 0
	DerivedMesh *result;
	int edgeSize = CCS_getEdgeSize(ss);
	int gridSize = CCS_getGridSize(ss);
	int gridFaces = gridSize - 1;
	int edgeBase, faceBase;
	int i, j, k, S, x, y, index;
	int *vertIdx = NULL;
	V_DECLARE(vertIdx);
	CCVertIterator *vi;
	CCEdgeIterator *ei;
	CCFaceIterator *fi;
	CCFace **faceMap2;
	CCEdge **edgeMap2;
	CCVert **vertMap2;
	int totvert, totedge, totface;
	MVert *mvert;
	MEdge *med;
	float *w = NULL;
	WeightTable wtable;
	V_DECLARE(w);
	MFace *mf;
	int *origIndex;

	memset(&wtable, 0, sizeof(wtable));

	/* vert map */
	totvert = CCS_getNumVerts(ss);
	vertMap2 = MEM_mallocN(totvert*sizeof(*vertMap2), "vertmap");
	vi = CCS_getVertIterator(ss);
	for(; !CCVIter_isStopped(vi); CCVIter_next(vi)) {
		CCVert *v = CCVIter_getCurrent(vi);

		vertMap2[GET_INT_FROM_POINTER(CCS_getVertVertHandle(v))] = v;
	}
	CCVIter_free(vi);

	totedge = CCS_getNumEdges(ss);
	edgeMap2 = MEM_mallocN(totedge*sizeof(*edgeMap2), "edgemap");
	ei = CCS_getEdgeIterator(ss);
	for(; !CCEIter_isStopped(ei); CCEIter_next(ei)) {
		CCEdge *e = CCEIter_getCurrent(ei);

		edgeMap2[GET_INT_FROM_POINTER(CCS_getEdgeEdgeHandle(e))] = e;
	}

	totface = CCS_getNumFaces(ss);
	faceMap2 = MEM_mallocN(totface*sizeof(*faceMap2), "facemap");
	fi = CCS_getFaceIterator(ss);
	for(; !CCFIter_isStopped(fi); CCFIter_next(fi)) {
		CCFace *f = CCFIter_getCurrent(fi);

		faceMap2[GET_INT_FROM_POINTER(CCS_getFaceFaceHandle(ss, f))] = f;
	}
	CCFIter_free(fi);

	if(ms) {
		result = MultiresDM_new(ms, dm, CCS_getNumFinalVerts(ss),
					CCS_getNumFinalEdges(ss),
					CCS_getNumFinalFaces(ss), 0, 0);
	}
	else {
		if(dm) {
			result = CDDM_from_template(dm, CCS_getNumFinalVerts(ss),
						    CCS_getNumFinalEdges(ss),
						    CCS_getNumFinalFaces(ss), 0, 0);
		} else {
			result = CDDM_new(CCS_getNumFinalVerts(ss),
					  CCS_getNumFinalEdges(ss),
					  CCS_getNumFinalFaces(ss), 0, 0);
		}
	}

	// load verts
	faceBase = i = 0;
	mvert = CDDM_get_verts(result);
	origIndex = result->getVertData(result, 0, CD_ORIGINDEX);

	for(index = 0; index < totface; index++) {
		CCFace *f = faceMap2[index];
		int x, y, S, numVerts = CCS_getFaceNumVerts(f);
		FaceVertWeight *weight = 0;//get_ss_weights(&wtable, gridFaces-1, numVerts);

		V_RESET(vertIdx);

		for(S = 0; S < numVerts; S++) {
			CCVert *v = CCS_getFaceVert(ss, f, S);
			V_GROW(vertIdx);

			vertIdx[S] = GET_INT_FROM_POINTER(CCS_getVertVertHandle(v));
		}

#if 0
		DM_interp_vert_data(dm, result, vertIdx, weight[0][0], numVerts, i);
#endif
		VecCopyf(mvert->co, CCS_getFaceCenterData(f));
		*origIndex = ORIGINDEX_NONE;
		++mvert;
		++origIndex;
		i++;

		V_RESET(w);
		for (x=0; x<numVerts; x++) {
			V_GROW(w);
		}

		for(S = 0; S < numVerts; S++) {
			int prevS = (S - 1 + numVerts) % numVerts;
			int nextS = (S + 1) % numVerts;
			int otherS = (numVerts >= 4) ? (S + 2) % numVerts : 3;

			for(x = 1; x < gridFaces; x++) {
#if 0
				w[prevS]  = weight[x][0][0];
				w[S]      = weight[x][0][1];
				w[nextS]  = weight[x][0][2];
				w[otherS] = weight[x][0][3];

				DM_interp_vert_data(dm, result, vertIdx, w, numVerts, i);
#endif
				VecCopyf(mvert->co,
				         CCS_getFaceGridEdgeData(ss, f, S, x));

				*origIndex = ORIGINDEX_NONE;
				++mvert;
				++origIndex;
				i++;
			}
		}
		
		V_RESET(w);
		for (x=0; x<numVerts; x++) {
			V_GROW(w);
		}

		for(S = 0; S < numVerts; S++) {
			int prevS = (S - 1 + numVerts) % numVerts;
			int nextS = (S + 1) % numVerts;
			int otherS = (numVerts == 4) ? (S + 2) % numVerts : 3;
			
			for(y = 1; y < gridFaces; y++) {
				for(x = 1; x < gridFaces; x++) {
#if 0
					w[prevS]  = weight[y * gridFaces + x][0][0];
					w[S]      = weight[y * gridFaces + x][0][1];
					w[nextS]  = weight[y * gridFaces + x][0][2];
					w[otherS] = weight[y * gridFaces + x][0][3];
					DM_interp_vert_data(dm, result, vertIdx, w, numVerts, i);
#endif

					VecCopyf(mvert->co,
					         CCS_getFaceGridData(ss, f, S, x, y));
					*origIndex = ORIGINDEX_NONE;
					++mvert;
					++origIndex;
					i++;
				}
			}
		}
		*((int*)CCS_getFaceUserData(ss, f)) = faceBase;
		faceBase += 1 + numVerts * ((gridSize-2) + (gridSize-2) * (gridSize-2));
	}

	edgeBase = i;
	for(index = 0; index < totedge; index++) {
		CCEdge *e = edgeMap2[index];
		int x;
		int vertIdx[2];

		CCVert *v;
		v = CCS_getEdgeVert0(e);
		vertIdx[0] = GET_INT_FROM_POINTER(CCS_getVertVertHandle(v));
		v = CCS_getEdgeVert1(e);
		vertIdx[1] = GET_INT_FROM_POINTER(CCS_getVertVertHandle(v));
		
		for(x = 1; x < edgeSize - 1; x++) {
			float w2[2];

			w2[1] = (float) x / (edgeSize - 1);
			w2[0] = 1 - w2[1];
			DM_interp_vert_data(dm, result, vertIdx, w2, 2, i);

			VecCopyf(mvert->co, CCS_getEdgeData(ss, e, x));
			*origIndex = ORIGINDEX_NONE;
			++mvert;
			++origIndex;
			i++;
		}

		*((int*)CCS_getEdgeUserData(ss, e)) = edgeBase;
		edgeBase += edgeSize-2;
	}

	for(index = 0; index < totvert; index++) {
		CCVert *v = vertMap2[index];
		int vertIdx;

		vertIdx = GET_INT_FROM_POINTER(CCS_getVertVertHandle(v));

		DM_copy_vert_data(dm, result, vertIdx, i, 1);
		VecCopyf(mvert->co, CCS_getVertData(ss, v));

		*((int*)CCS_getVertUserData(ss, v)) = i;
		*origIndex = cgdm_getVertMapIndex(ss, v);
		++mvert;
		++origIndex;
		i++;
	}

	// load edges
	i = 0;
	med = CDDM_get_edges(result);
	origIndex = result->getEdgeData(result, 0, CD_ORIGINDEX);

	for(index = 0; index < totface; index++) {
		CCFace *f = faceMap2[index];
		int numVerts = CCS_getFaceNumVerts(f);

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
		CCEdge *e = edgeMap2[index];
		unsigned int flags = 0;
		char bweight = 0;
		int edgeIdx = GET_INT_FROM_POINTER(CCS_getEdgeEdgeHandle(e));

		if(!CCS_getEdgeNumFaces(e)) flags |= ME_LOOSEEDGE;


		if(edgeIdx != -1 && dm) {
			MEdge origMed;
			dm->getEdge(dm, edgeIdx, &origMed);

			flags |= origMed.flag;
			bweight = origMed.bweight;
		}

		for(x = 0; x < edgeSize - 1; x++) {
			med->v1 = getEdgeIndex(ss, e, x, edgeSize);
			med->v2 = getEdgeIndex(ss, e, x + 1, edgeSize);
			med->flag = flags;
			med->bweight = bweight;
			*origIndex = cgdm_getEdgeMapIndex(ss, e);
			++med;
			++origIndex;
			i++;
		}
	}

	// load faces
	i = 0;
	mf = CDDM_get_tessfaces(result);
	origIndex = result->getTessFaceData(result, 0, CD_ORIGINDEX);

	for(index = 0; index < totface; index++) {
		CCFace *f = faceMap2[index];
		int numVerts = CCS_getFaceNumVerts(f);
		int mat_nr;
		int flag;
		int mapIndex = cgdm_getFaceMapIndex(ss, f);
		int faceIdx = GET_INT_FROM_POINTER(CCS_getFaceFaceHandle(ss, f));

		if(!ssFromEditmesh) {
			MFace origMFace;
			dm->getTessFace(dm, faceIdx, &origMFace);
			
			mat_nr = origMFace.mat_nr;
			flag = origMFace.flag;
		} else {
			BMFace *ef = CCS_getFaceFaceHandle(ss, f);
			mat_nr = ef->mat_nr;
			flag = BMFlags_To_MEFlags(ef);
		}

		for(S = 0; S < numVerts; S++) {
			FaceVertWeight *weight = 0;//get_ss_weights(&wtable, gridFaces-1, numVerts);
			
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
#if 0 //BMESH_TODO
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
						
						DM_interp_tessface_data(dm, result, &faceIdx, NULL,
						                    &w, 1, i);
						weight++;
					}
#endif

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

	free_ss_weights(&wtable);

	V_FREE(vertIdx);

	if(useSubsurfUv) {
		CustomData *fdata = &result->faceData;
		CustomData *dmfdata = &dm->faceData;
		int numlayer = CustomData_number_of_layers(fdata, CD_MTFACE);
		int dmnumlayer = CustomData_number_of_layers(dmfdata, CD_MTFACE);
		
		for (i=0; i<numlayer && i<dmnumlayer; i++)
			set_subsurf_uv(ss, dm, result, i);
	}

	CDDM_calc_normals(result);
	CDDM_tessfaces_to_faces(result);
	
	V_FREE(w);
	return result;
#endif
}

static void ss_sync_from_derivedmesh(CSubSurf *ss, DerivedMesh *dm,
                                     float (*vertexCos)[3], int useFlatSubdiv)
{
	float creaseFactor = (float) CCS_getSubdivisionLevels(ss);
	CCVertHDL *fVerts = NULL;
	V_DECLARE(fVerts);
	int totvert = dm->getNumVerts(dm);
	int totedge = dm->getNumEdges(dm);
	int totface = dm->getNumTessFaces(dm);
	int totpoly = dm->getNumFaces(dm);
	int i;
	int *index;
	MVert *mvert = dm->getVertArray(dm);
	MEdge *medge = dm->getEdgeArray(dm);
	MFace *mface = dm->getTessFaceArray(dm);
	MVert *mv;
	MEdge *me;
	MFace *mf;
	DMFaceIter *fiter;
	DMLoopIter *liter;

	CCS_initFullSync(ss);

	mv = mvert;
	index = (int *)dm->getVertDataArray(dm, CD_ORIGINDEX);
	for(i = 0; i < totvert; i++, mv++, index++) {
		CCVert *v;

		if(vertexCos) {
			CCS_syncVert(ss, SET_INT_IN_POINTER(i), vertexCos[i], 0, &v);
		} else {
			CCS_syncVert(ss, SET_INT_IN_POINTER(i), mv->co, 0, &v);
		}

		((int*)CCS_getVertUserData(ss, v))[1] = *index;
	}

	me = medge;
	index = (int *)dm->getEdgeDataArray(dm, CD_ORIGINDEX);
	for(i = 0; i < totedge; i++, me++, index++) {
		CCEdge *e;
		float crease;

		crease = useFlatSubdiv ? creaseFactor :
		                         me->crease * creaseFactor / 255.0f;

		CCS_syncEdge(ss, SET_INT_IN_POINTER(i), SET_INT_IN_POINTER(me->v1),
		                    SET_INT_IN_POINTER(me->v2), crease, &e);

		((int*)CCS_getEdgeUserData(ss, e))[1] = *index;
	}
	
	fiter = dm->newFaceIter(dm);
	for (i=0; !fiter->done; fiter->step(fiter), i++) {
		CCFace *f;
		V_RESET(fVerts);

		index = (int*) fiter->getCDData(fiter, CD_ORIGINDEX, -1);
		liter = fiter->getLoopsIter(fiter);

		for (; !liter->done; liter->step(liter)) {
			V_GROW(fVerts);
			fVerts[V_COUNT(fVerts)-1] = SET_INT_IN_POINTER(liter->vindex);
		}

		/* this is very bad, means mesh is internally inconsistent.
		 * it is not really possible to continue without modifying
		 * other parts of code significantly to handle missing faces.
		 * since this really shouldn't even be possible we just bail.*/
		if(CCS_syncFace(ss, SET_INT_IN_POINTER(i), fiter->len, 
		                       fVerts, &f) == eCCGError_InvalidValue) {
			static int hasGivenError = 0;

			if(!hasGivenError) {
				printf("Unrecoverable error in SubSurf calculation,"
				       " mesh is inconsistent.\n");

				hasGivenError = 1;
			}

			return;
		}

		((int*)CCS_getFaceUserData(ss, f))[1] = *index;
	}

	CCS_processSync(ss);
}

/***/

static int cgdm_getVertMapIndex(CSubSurf *ss, CCVert *v) {
	return ((int*) CCS_getVertUserData(ss, v))[1];
}

static int cgdm_getEdgeMapIndex(CSubSurf *ss, CCEdge *e) {
	return ((int*) CCS_getEdgeUserData(ss, e))[1];
}

static int cgdm_getFaceMapIndex(CSubSurf *ss, CCFace *f) {
	return ((int*) CCS_getFaceUserData(ss, f))[1];
}

static void cgdm_getMinMax(DerivedMesh *dm, float min_r[3], float max_r[3]) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	CCVertIterator *vi = CCS_getVertIterator(ss);
	CCEdgeIterator *ei = CCS_getEdgeIterator(ss);
	CCFaceIterator *fi = CCS_getFaceIterator(ss);
	int i, edgeSize = CCS_getEdgeSize(ss);
	int gridSize = CCS_getGridSize(ss);

	if (!CCS_getNumVerts(ss))
		min_r[0] = min_r[1] = min_r[2] = max_r[0] = max_r[1] = max_r[2] = 0.0;

	for (; !CCVIter_isStopped(vi); CCVIter_next(vi)) {
		CCVert *v = CCVIter_getCurrent(vi);
		float *co = CCS_getVertData(ss, v);

		DO_MINMAX(co, min_r, max_r);
	}

	for (; !CCEIter_isStopped(ei); CCEIter_next(ei)) {
		CCEdge *e = CCEIter_getCurrent(ei);
		VertData *edgeData = CCS_getEdgeDataArray(ss, e);

		for (i=0; i<edgeSize; i++)
			DO_MINMAX(edgeData[i].co, min_r, max_r);
	}

	for (; !CCFIter_isStopped(fi); CCFIter_next(fi)) {
		CCFace *f = CCFIter_getCurrent(fi);
		int S, x, y, numVerts = CCS_getFaceNumVerts(f);

		for (S=0; S<numVerts; S++) {
			VertData *faceGridData = CCS_getFaceGridDataArray(ss, f, S);

			for (y=0; y<gridSize; y++)
				for (x=0; x<gridSize; x++)
					DO_MINMAX(faceGridData[y*gridSize + x].co, min_r, max_r);
		}
	}

	CCFIter_free(fi);
	CCEIter_free(ei);
	CCVIter_free(vi);
}
static int cgdm_getNumVerts(DerivedMesh *dm) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;

	return CCS_getNumFinalVerts(cgdm->ss);
}
static int cgdm_getNumEdges(DerivedMesh *dm) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;

	return CCS_getNumFinalEdges(cgdm->ss);
}
static int cgdm_getNumTessFaces(DerivedMesh *dm) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;

	return CCS_getNumFinalFaces(cgdm->ss);
}

static void cgdm_getFinalVert(DerivedMesh *dm, int vertNum, MVert *mv)
{
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	int i;

	memset(mv, 0, sizeof(*mv));

	if((vertNum < cgdm->edgeMap[0].startVert) && (CCS_getNumFaces(ss) > 0)) {
		/* this vert comes from face data */
		int lastface = CCS_getNumFaces(ss) - 1;
		CCFace *f;
		int x, y, grid, numVerts;
		int offset;
		int gridSize = CCS_getGridSize(ss);
		int gridSideVerts;
		int gridInternalVerts;
		int gridSideEnd;
		int gridInternalEnd;

		i = 0;
		while(i < lastface && vertNum >= cgdm->faceMap[i + 1].startVert)
			++i;

		f = cgdm->faceMap[i].face;
		numVerts = CCS_getFaceNumVerts(f);

		gridSideVerts = gridSize - 2;
		gridInternalVerts = gridSideVerts * gridSideVerts;

		gridSideEnd = 1 + numVerts * gridSideVerts;
		gridInternalEnd = gridSideEnd + numVerts * gridInternalVerts;

		offset = vertNum - cgdm->faceMap[i].startVert;
		if(offset < 1) {
			VecCopyf(mv->co, CCS_getFaceCenterData(f));
		} else if(offset < gridSideEnd) {
			offset -= 1;
			grid = offset / gridSideVerts;
			x = offset % gridSideVerts + 1;
			VecCopyf(mv->co, CCS_getFaceGridEdgeData(ss, f, grid, x));
		} else if(offset < gridInternalEnd) {
			offset -= gridSideEnd;
			grid = offset / gridInternalVerts;
			offset %= gridInternalVerts;
			y = offset / gridSideVerts + 1;
			x = offset % gridSideVerts + 1;
			VecCopyf(mv->co, CCS_getFaceGridData(ss, f, grid, x, y));
		}
	} else if((vertNum < cgdm->vertMap[0].startVert) && (CCS_getNumEdges(ss) > 0)) {
		/* this vert comes from edge data */
		CCEdge *e;
		int lastedge = CCS_getNumEdges(ss) - 1;
		int x;

		i = 0;
		while(i < lastedge && vertNum >= cgdm->edgeMap[i + 1].startVert)
			++i;

		e = cgdm->edgeMap[i].edge;

		x = vertNum - cgdm->edgeMap[i].startVert + 1;
		VecCopyf(mv->co, CCS_getEdgeData(ss, e, x));
	} else {
		/* this vert comes from vert data */
		CCVert *v;
		i = vertNum - cgdm->vertMap[0].startVert;

		v = cgdm->vertMap[i].vert;
		VecCopyf(mv->co, CCS_getVertData(ss, v));
	}
}

static void cgdm_getFinalEdge(DerivedMesh *dm, int edgeNum, MEdge *med)
{
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	int i;

	memset(med, 0, sizeof(*med));

	if(edgeNum < cgdm->edgeMap[0].startEdge) {
		/* this edge comes from face data */
		int lastface = CCS_getNumFaces(ss) - 1;
		CCFace *f;
		int x, y, grid, numVerts;
		int offset;
		int gridSize = CCS_getGridSize(ss);
		int edgeSize = CCS_getEdgeSize(ss);
		int gridSideEdges;
		int gridInternalEdges;

		i = 0;
		while(i < lastface && edgeNum >= cgdm->faceMap[i + 1].startEdge)
			++i;

		f = cgdm->faceMap[i].face;
		numVerts = CCS_getFaceNumVerts(f);

		gridSideEdges = gridSize - 1;
		gridInternalEdges = (gridSideEdges - 1) * gridSideEdges * 2; 

		offset = edgeNum - cgdm->faceMap[i].startEdge;
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
		CCEdge *e;
		int edgeSize = CCS_getEdgeSize(ss);
		int x, *edgeFlag;
		unsigned int flags = 0;

		i = (edgeNum - cgdm->edgeMap[0].startEdge) / (edgeSize - 1);

		e = cgdm->edgeMap[i].edge;

		if(!CCS_getEdgeNumFaces(e)) flags |= ME_LOOSEEDGE;

		x = edgeNum - cgdm->edgeMap[i].startEdge;

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

static void cgdm_getFinalFace(DerivedMesh *dm, int faceNum, MFace *mf)
{
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	int gridSize = CCS_getGridSize(ss);
	int edgeSize = CCS_getEdgeSize(ss);
	int gridSideEdges = gridSize - 1;
	int gridFaces = gridSideEdges * gridSideEdges;
	int i;
	CCFace *f;
	int numVerts;
	int offset;
	int grid;
	int x, y;
	int lastface = CCS_getNumFaces(ss) - 1;
	char *faceFlags = dm->getTessFaceDataArray(dm, CD_FLAGS);

	memset(mf, 0, sizeof(*mf));

	i = 0;
	while(i < lastface && faceNum >= cgdm->faceMap[i + 1].startFace)
		++i;

	f = cgdm->faceMap[i].face;
	numVerts = CCS_getFaceNumVerts(f);

	offset = faceNum - cgdm->faceMap[i].startFace;
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

static void cgdm_copyFinalVertArray(DerivedMesh *dm, MVert *mvert)
{
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	int index;
	int totvert, totedge, totface;
	int gridSize = CCS_getGridSize(ss);
	int edgeSize = CCS_getEdgeSize(ss);
	int i = 0;

	totface = CCS_getNumFaces(ss);
	for(index = 0; index < totface; index++) {
		CCFace *f = cgdm->faceMap[index].face;
		int x, y, S, numVerts = CCS_getFaceNumVerts(f);

		VecCopyf(mvert[i++].co, CCS_getFaceCenterData(f));
		
		for(S = 0; S < numVerts; S++) {
			for(x = 1; x < gridSize - 1; x++) {
				VecCopyf(mvert[i++].co,
				         CCS_getFaceGridEdgeData(ss, f, S, x));
			}
		}

		for(S = 0; S < numVerts; S++) {
			for(y = 1; y < gridSize - 1; y++) {
				for(x = 1; x < gridSize - 1; x++) {
					VecCopyf(mvert[i++].co,
					         CCS_getFaceGridData(ss, f, S, x, y));
				}
			}
		}
	}

	totedge = CCS_getNumEdges(ss);
	for(index = 0; index < totedge; index++) {
		CCEdge *e = cgdm->edgeMap[index].edge;
		int x;

		for(x = 1; x < edgeSize - 1; x++) {
			VecCopyf(mvert[i++].co, CCS_getEdgeData(ss, e, x));
		}
	}

	totvert = CCS_getNumVerts(ss);
	for(index = 0; index < totvert; index++) {
		CCVert *v = cgdm->vertMap[index].vert;

		VecCopyf(mvert[i].co, CCS_getVertData(ss, v));

		i++;
	}
}

static void cgdm_copyFinalEdgeArray(DerivedMesh *dm, MEdge *medge)
{
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	int index;
	int totedge, totface;
	int gridSize = CCS_getGridSize(ss);
	int edgeSize = CCS_getEdgeSize(ss);
	int i = 0;
	int *edgeFlags = dm->getEdgeDataArray(dm, CD_FLAGS);

	totface = CCS_getNumFaces(ss);
	for(index = 0; index < totface; index++) {
		CCFace *f = cgdm->faceMap[index].face;
		int x, y, S, numVerts = CCS_getFaceNumVerts(f);

		for(S = 0; S < numVerts; S++) {
			for(x = 0; x < gridSize - 1; x++) {
				MEdge *med = &medge[i];

				if(cgdm->drawInteriorEdges)
				    med->flag = ME_EDGEDRAW | ME_EDGERENDER;
				med->v1 = getFaceIndex(ss, f, S, x, 0, edgeSize, gridSize);
				med->v2 = getFaceIndex(ss, f, S, x + 1, 0, edgeSize, gridSize);
				i++;
			}

			for(x = 1; x < gridSize - 1; x++) {
				for(y = 0; y < gridSize - 1; y++) {
					MEdge *med;

					med = &medge[i];
					if(cgdm->drawInteriorEdges)
					    med->flag = ME_EDGEDRAW | ME_EDGERENDER;
					med->v1 = getFaceIndex(ss, f, S, x, y,
					                       edgeSize, gridSize);
					med->v2 = getFaceIndex(ss, f, S, x, y + 1,
					                       edgeSize, gridSize);
					i++;

					med = &medge[i];
					if(cgdm->drawInteriorEdges)
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

	totedge = CCS_getNumEdges(ss);
	for(index = 0; index < totedge; index++) {
		CCEdge *e = cgdm->edgeMap[index].edge;
		unsigned int flags = 0;
		int x;
		int edgeIdx = GET_INT_FROM_POINTER(CCS_getEdgeEdgeHandle(e));

		if(!CCS_getEdgeNumFaces(e)) flags |= ME_LOOSEEDGE;

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

struct cgdm_faceIter;

typedef struct cgdm_loopIter {
	DMLoopIter head;
	int curloop;
	CCGDerivedMesh *cgdm;
	struct cgdm_faceIter *fiter;
} cgdm_loopIter;

typedef struct cgdm_faceIter {
	DMFaceIter head;
	CCGDerivedMesh *cgdm;
	MFace mface;

	cgdm_loopIter liter;
	EdgeHash *ehash; /*edge map for populating loopiter->eindex*/
} cgdm_faceIter;

void cgdm_faceIterStep(void *self)
{
	cgdm_faceIter *fiter = self;

	if (!fiter->cgdm || !fiter->cgdm->ss) {
		fiter->head.done = 1;
		return;
	}

	if (fiter->head.index >= CCS_getNumFaces(fiter->cgdm->ss)) {
		fiter->head.done = 1;
		return;
	};

	fiter->head.index++;
	
	cgdm_getFinalFace((DerivedMesh*)fiter->cgdm, fiter->head.index, &fiter->mface);

	fiter->head.flags = fiter->mface.flag;
	fiter->head.mat_nr = fiter->mface.mat_nr;
	fiter->head.len = fiter->mface.v4 ? 4 : 3;
}

void *cgdm_faceIterCData(void *self, int type, int layer)
{
	cgdm_faceIter *fiter = self;
	
	if (layer == -1) 
		return CustomData_get(&fiter->cgdm->dm.faceData, fiter->head.index, type);
	else
		return CustomData_get_n(&fiter->cgdm->dm.faceData, type, fiter->head.index, layer);
}

void cgdm_loopIterStep(void *self)
{
	cgdm_loopIter *liter = self;
	MFace *mf = &liter->fiter->mface;
	int i, v1, v2;

	if (liter->head.index >= liter->fiter->head.len) {
		liter->head.done = 1;
		return;
	}

	liter->head.index++;
	i = liter->head.index;

	switch (i) {
		case 0:
			v1 = liter->fiter->mface.v1;
			v2 = liter->fiter->mface.v2;
			break;
		case 1:
			v1 = liter->fiter->mface.v2;
			v2 = liter->fiter->mface.v3;
			break;
		case 2:
			v1 = liter->fiter->mface.v3;
			v2 = liter->fiter->mface.v4;
			break;
		case 3:
			v1 = liter->fiter->mface.v4;
			v2 = liter->fiter->mface.v1;
			break;
	}

	liter->head.vindex = v1;
	liter->head.eindex = GET_INT_FROM_POINTER(BLI_edgehash_lookup(liter->fiter->ehash, v1, v2));
	
	cgdm_getFinalVert((DerivedMesh*)liter->cgdm, v1, &liter->head.v);	
}

void *cgdm_loopIterGetVCData(void *self, int type, int layer)
{
	cgdm_loopIter *liter = self;

	if (layer == -1)
		return CustomData_get(&liter->cgdm->dm.vertData, liter->head.vindex, type);
	else return CustomData_get_n(&liter->cgdm->dm.vertData, type, liter->head.vindex, layer);
}

void *cgdm_loopIterGetCData(void *self, int type, int layer)
{
	cgdm_loopIter *liter = self;
	
	/*BMESH_TODO
	  yeek, this has to convert mface-style uv/mcols to loop-style*/
	return NULL;
}

DMLoopIter *cgdm_faceIterGetLIter(void *self)
{
	cgdm_faceIter *fiter = self;
	
	fiter->liter.head.index = -1;
	fiter->liter.head.done = 0;
	fiter->liter.head.step(&fiter->liter);

	return (DMLoopIter*) &fiter->liter;
}

void cgdm_faceIterFree(void *vfiter)
{
	cgdm_faceIter *fiter = vfiter;
	BLI_edgehash_free(fiter->ehash, NULL);
	MEM_freeN(fiter);
}

DMFaceIter *cgdm_newFaceIter(DerivedMesh *dm)
{
	cgdm_faceIter *fiter = MEM_callocN(sizeof(cgdm_faceIter), "cgdm_faceIter");
	MEdge medge;
	int i, totedge = cgdm_getNumEdges(dm);

	fiter->ehash = BLI_edgehash_new();
	
	for (i=0; i<totedge; i++) {
		cgdm_getFinalEdge(dm, i, &medge);
		BLI_edgehash_insert(fiter->ehash, medge.v1, medge.v2, SET_INT_IN_POINTER(i));
	}

	fiter->head.free = cgdm_faceIterFree;
	fiter->head.step = cgdm_faceIterStep;
	fiter->head.index = -1;
	fiter->head.getCDData = cgdm_faceIterCData;
	fiter->head.getLoopsIter = cgdm_faceIterGetLIter;

	fiter->liter.fiter = fiter;
	fiter->liter.head.getLoopCDData = cgdm_loopIterGetCData;
	fiter->liter.head.getVertCDData = cgdm_loopIterGetVCData;
	fiter->liter.head.step = cgdm_loopIterStep;

	fiter->head.step(fiter);
}

static void cgdm_copyFinalFaceArray(DerivedMesh *dm, MFace *mface)
{
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	int index;
	int totface;
	int gridSize = CCS_getGridSize(ss);
	int edgeSize = CCS_getEdgeSize(ss);
	int i = 0;
	char *faceFlags = dm->getTessFaceDataArray(dm, CD_FLAGS);

	totface = CCS_getNumFaces(ss);
	for(index = 0; index < totface; index++) {
		CCFace *f = cgdm->faceMap[index].face;
		int x, y, S, numVerts = CCS_getFaceNumVerts(f);
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

static void cgdm_getVertCos(DerivedMesh *dm, float (*cos)[3]) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	int edgeSize = CCS_getEdgeSize(ss);
	int gridSize = CCS_getGridSize(ss);
	int i;
	CCVertIterator *vi;
	CCEdgeIterator *ei;
	CCFaceIterator *fi;
	CCFace **faceMap2;
	CCEdge **edgeMap2;
	CCVert **vertMap2;
	int index, totvert, totedge, totface;
	
	totvert = CCS_getNumVerts(ss);
	vertMap2 = MEM_mallocN(totvert*sizeof(*vertMap2), "vertmap");
	vi = CCS_getVertIterator(ss);
	for (; !CCVIter_isStopped(vi); CCVIter_next(vi)) {
		CCVert *v = CCVIter_getCurrent(vi);

		vertMap2[GET_INT_FROM_POINTER(CCS_getVertVertHandle(v))] = v;
	}
	CCVIter_free(vi);

	totedge = CCS_getNumEdges(ss);
	edgeMap2 = MEM_mallocN(totedge*sizeof(*edgeMap2), "edgemap");
	ei = CCS_getEdgeIterator(ss);
	for (i=0; !CCEIter_isStopped(ei); i++,CCEIter_next(ei)) {
		CCEdge *e = CCEIter_getCurrent(ei);

		edgeMap2[GET_INT_FROM_POINTER(CCS_getEdgeEdgeHandle(e))] = e;
	}

	totface = CCS_getNumFaces(ss);
	faceMap2 = MEM_mallocN(totface*sizeof(*faceMap2), "facemap");
	fi = CCS_getFaceIterator(ss);
	for (; !CCFIter_isStopped(fi); CCFIter_next(fi)) {
		CCFace *f = CCFIter_getCurrent(fi);

		faceMap2[GET_INT_FROM_POINTER(CCS_getFaceFaceHandle(ss, f))] = f;
	}
	CCFIter_free(fi);

	i = 0;
	for (index=0; index<totface; index++) {
		CCFace *f = faceMap2[index];
		int x, y, S, numVerts = CCS_getFaceNumVerts(f);

		VecCopyf(cos[i++], CCS_getFaceCenterData(f));
		
		for (S=0; S<numVerts; S++) {
			for (x=1; x<gridSize-1; x++) {
				VecCopyf(cos[i++], CCS_getFaceGridEdgeData(ss, f, S, x));
			}
		}

		for (S=0; S<numVerts; S++) {
			for (y=1; y<gridSize-1; y++) {
				for (x=1; x<gridSize-1; x++) {
					VecCopyf(cos[i++], CCS_getFaceGridData(ss, f, S, x, y));
				}
			}
		}
	}

	for (index=0; index<totedge; index++) {
		CCEdge *e= edgeMap2[index];
		int x;

		for (x=1; x<edgeSize-1; x++) {
			VecCopyf(cos[i++], CCS_getEdgeData(ss, e, x));
		}
	}

	for (index=0; index<totvert; index++) {
		CCVert *v = vertMap2[index];
		VecCopyf(cos[i++], CCS_getVertData(ss, v));
	}

	MEM_freeN(vertMap2);
	MEM_freeN(edgeMap2);
	MEM_freeN(faceMap2);
}
static void cgdm_foreachMappedVert(DerivedMesh *dm, void (*func)(void *userData, int index, float *co, float *no_f, short *no_s), void *userData) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CCVertIterator *vi = CCS_getVertIterator(cgdm->ss);

	for (; !CCVIter_isStopped(vi); CCVIter_next(vi)) {
		CCVert *v = CCVIter_getCurrent(vi);
		VertData *vd = CCS_getVertData(cgdm->ss, v);
		int index = cgdm_getVertMapIndex(cgdm->ss, v);

		if (index!=-1)
			func(userData, index, vd->co, vd->no, NULL);
	}

	CCVIter_free(vi);
}
static void cgdm_foreachMappedEdge(DerivedMesh *dm, void (*func)(void *userData, int index, float *v0co, float *v1co), void *userData) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	CCEdgeIterator *ei = CCS_getEdgeIterator(ss);
	int i, edgeSize = CCS_getEdgeSize(ss);

	for (; !CCEIter_isStopped(ei); CCEIter_next(ei)) {
		CCEdge *e = CCEIter_getCurrent(ei);
		VertData *edgeData = CCS_getEdgeDataArray(ss, e);
		int index = cgdm_getEdgeMapIndex(ss, e);

		if (index!=-1) {
			for (i=0; i<edgeSize-1; i++)
				func(userData, index, edgeData[i].co, edgeData[i+1].co);
		}
	}

	CCEIter_free(ei);
}

static void cgdm_drawVerts(DerivedMesh *dm) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	int edgeSize = CCS_getEdgeSize(ss);
	int gridSize = CCS_getGridSize(ss);
	CCVertIterator *vi;
	CCEdgeIterator *ei;
	CCFaceIterator *fi;

	glBegin(GL_POINTS);
	vi = CCS_getVertIterator(ss);
	for (; !CCVIter_isStopped(vi); CCVIter_next(vi)) {
		CCVert *v = CCVIter_getCurrent(vi);
		glVertex3fv(CCS_getVertData(ss, v));
	}
	CCVIter_free(vi);

	ei = CCS_getEdgeIterator(ss);
	for (; !CCEIter_isStopped(ei); CCEIter_next(ei)) {
		CCEdge *e = CCEIter_getCurrent(ei);
		int x;

		for (x=1; x<edgeSize-1; x++)
			glVertex3fv(CCS_getEdgeData(ss, e, x));
	}
	CCEIter_free(ei);

	fi = CCS_getFaceIterator(ss);
	for (; !CCFIter_isStopped(fi); CCFIter_next(fi)) {
		CCFace *f = CCFIter_getCurrent(fi);
		int x, y, S, numVerts = CCS_getFaceNumVerts(f);

		glVertex3fv(CCS_getFaceCenterData(f));
		for (S=0; S<numVerts; S++)
			for (x=1; x<gridSize-1; x++)
				glVertex3fv(CCS_getFaceGridEdgeData(ss, f, S, x));
		for (S=0; S<numVerts; S++)
			for (y=1; y<gridSize-1; y++)
				for (x=1; x<gridSize-1; x++)
					glVertex3fv(CCS_getFaceGridData(ss, f, S, x, y));
	}
	CCFIter_free(fi);
	glEnd();
}
static void cgdm_drawEdges(DerivedMesh *dm, int drawLooseEdges) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	CCEdgeIterator *ei = CCS_getEdgeIterator(ss);
	CCFaceIterator *fi = CCS_getFaceIterator(ss);
	int i, edgeSize = CCS_getEdgeSize(ss);
	int gridSize = CCS_getGridSize(ss);
	int useAging;

	CCS_getUseAgeCounts(ss, &useAging, NULL, NULL, NULL);

	for (; !CCEIter_isStopped(ei); CCEIter_next(ei)) {
		CCEdge *e = CCEIter_getCurrent(ei);
		VertData *edgeData = CCS_getEdgeDataArray(ss, e);

		if (!drawLooseEdges && !CCS_getEdgeNumFaces(e))
			continue;

		if (useAging && !(G.f&G_BACKBUFSEL)) {
			int ageCol = 255-CCS_getEdgeAge(ss, e)*4;
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

	if (cgdm->drawInteriorEdges) {
		for (; !CCFIter_isStopped(fi); CCFIter_next(fi)) {
			CCFace *f = CCFIter_getCurrent(fi);
			int S, x, y, numVerts = CCS_getFaceNumVerts(f);

			for (S=0; S<numVerts; S++) {
				VertData *faceGridData = CCS_getFaceGridDataArray(ss, f, S);

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

	CCFIter_free(fi);
	CCEIter_free(ei);
}
static void cgdm_drawLooseEdges(DerivedMesh *dm) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	CCEdgeIterator *ei = CCS_getEdgeIterator(ss);
	int i, edgeSize = CCS_getEdgeSize(ss);

	for (; !CCEIter_isStopped(ei); CCEIter_next(ei)) {
		CCEdge *e = CCEIter_getCurrent(ei);
		VertData *edgeData = CCS_getEdgeDataArray(ss, e);

		if (!CCS_getEdgeNumFaces(e)) {
			glBegin(GL_LINE_STRIP);
			for (i=0; i<edgeSize-1; i++) {
				glVertex3fv(edgeData[i].co);
				glVertex3fv(edgeData[i+1].co);
			}
			glEnd();
		}
	}

	CCEIter_free(ei);
}

static void cgdm_glNormalFast(float *a, float *b, float *c, float *d)
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
static void cgdm_drawFacesSolid(DerivedMesh *dm, int (*setMaterial)(int, void *attribs)) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	CCFaceIterator *fi = CCS_getFaceIterator(ss);
	int gridSize = CCS_getGridSize(ss);
	char *faceFlags = DM_get_tessface_data_layer(dm, CD_FLAGS);

	for (; !CCFIter_isStopped(fi); CCFIter_next(fi)) {
		CCFace *f = CCFIter_getCurrent(fi);
		int S, x, y, numVerts = CCS_getFaceNumVerts(f);
		int index = GET_INT_FROM_POINTER(CCS_getFaceFaceHandle(ss, f));
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
			VertData *faceGridData = CCS_getFaceGridDataArray(ss, f, S);

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

						cgdm_glNormalFast(a, b, c, d);

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

	CCFIter_free(fi);
}

	/* Only used by non-editmesh types */
static void cgdm_drawMappedFacesGLSL(DerivedMesh *dm, int (*setMaterial)(int, void *attribs), int (*setDrawOptions)(void *userData, int index), void *userData) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	CCFaceIterator *fi = CCS_getFaceIterator(ss);
	GPUVertexAttribs gattribs;
	DMVertexAttribs attribs;
	MTFace *tf = dm->getTessFaceDataArray(dm, CD_MTFACE);
	int gridSize = CCS_getGridSize(ss);
	int gridFaces = gridSize - 1;
	int edgeSize = CCS_getEdgeSize(ss);
	int transp, orig_transp, new_transp;
	char *faceFlags = DM_get_tessface_data_layer(dm, CD_FLAGS);
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

	totface = CCS_getNumFaces(ss);
	for(a = 0, i = 0; i < totface; i++) {
		CCFace *f = cgdm->faceMap[i].face;
		int S, x, y, drawSmooth;
		int index = GET_INT_FROM_POINTER(CCS_getFaceFaceHandle(ss, f));
		int origIndex = cgdm_getFaceMapIndex(ss, f);
		
		numVerts = CCS_getFaceNumVerts(f);

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
			VertData *faceGridData = CCS_getFaceGridDataArray(ss, f, S);
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

						cgdm_glNormalFast(aco, bco, cco, dco);

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

	CCFIter_free(fi);
}

static void cgdm_drawFacesGLSL(DerivedMesh *dm, int (*setMaterial)(int, void *attribs)) {
	dm->drawMappedFacesGLSL(dm, setMaterial, NULL, NULL);
}

static void cgdm_drawFacesColored(DerivedMesh *dm, int useTwoSided, unsigned char *col1, unsigned char *col2) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	CCFaceIterator *fi = CCS_getFaceIterator(ss);
	int gridSize = CCS_getGridSize(ss);
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
	for (; !CCFIter_isStopped(fi); CCFIter_next(fi)) {
		CCFace *f = CCFIter_getCurrent(fi);
		int S, x, y, numVerts = CCS_getFaceNumVerts(f);

		for (S=0; S<numVerts; S++) {
			VertData *faceGridData = CCS_getFaceGridDataArray(ss, f, S);
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

	CCFIter_free(fi);
}

static void cgdm_drawFacesTex_common(DerivedMesh *dm,
	int (*drawParams)(MTFace *tface, int has_vcol, int matnr),
	int (*drawParamsMapped)(void *userData, int index),
	void *userData) 
{
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	MCol *mcol = DM_get_tessface_data_layer(dm, CD_MCOL);
	MTFace *tf = DM_get_tessface_data_layer(dm, CD_MTFACE);
	char *faceFlags = DM_get_tessface_data_layer(dm, CD_FLAGS);
	int i, totface, flag, gridSize = CCS_getGridSize(ss);
	int gridFaces = gridSize - 1;

	totface = CCS_getNumFaces(ss);
	for(i = 0; i < totface; i++) {
		CCFace *f = cgdm->faceMap[i].face;
		int S, x, y, numVerts = CCS_getFaceNumVerts(f);
		int drawSmooth, index = cgdm_getFaceMapIndex(ss, f);
		int origIndex = GET_INT_FROM_POINTER(CCS_getFaceFaceHandle(ss, f));
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
			flag = drawParams(tf, mcol!=NULL, mat_nr);
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
			VertData *faceGridData = CCS_getFaceGridDataArray(ss, f, S);
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
						float *a_co = faceGridData[(y+0)*gridSize + x].co;
						float *b_co = faceGridData[(y+0)*gridSize + x + 1].co;
						float *c_co = faceGridData[(y+1)*gridSize + x + 1].co;
						float *d_co = faceGridData[(y+1)*gridSize + x].co;

						cgdm_glNormalFast(a_co, b_co, c_co, d_co);

						if(tf) glTexCoord2fv(tf->uv[1]);
						if(cp) glColor3ub(cp[7], cp[6], cp[5]);
						glVertex3fv(d_co);

						if(tf) glTexCoord2fv(tf->uv[2]);
						if(cp) glColor3ub(cp[11], cp[10], cp[9]);
						glVertex3fv(c_co);

						if(tf) glTexCoord2fv(tf->uv[3]);
						if(cp) glColor3ub(cp[15], cp[14], cp[13]);
						glVertex3fv(b_co);

						if(tf) glTexCoord2fv(tf->uv[0]);
						if(cp) glColor3ub(cp[3], cp[2], cp[1]);
						glVertex3fv(a_co);

						if(tf) tf++;
						if(cp) cp += 16;
					}
				}
				glEnd();
			}
		}
	}
}

static void cgdm_drawFacesTex(DerivedMesh *dm, int (*setDrawOptions)(MTFace *tface, int has_vcol, int matnr))
{
	cgdm_drawFacesTex_common(dm, setDrawOptions, NULL, NULL);
}

static void cgdm_drawMappedFacesTex(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void *userData)
{
	cgdm_drawFacesTex_common(dm, NULL, setDrawOptions, userData);
}

static void cgdm_drawUVEdges(DerivedMesh *dm)
{

	MFace *mf = dm->getTessFaceArray(dm);
	MTFace *tf = DM_get_tessface_data_layer(dm, CD_MTFACE);
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

static void cgdm_drawMappedFaces(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index, int *drawSmooth_r), void *userData, int useColors) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	CCFaceIterator *fi = CCS_getFaceIterator(ss);
	int i, gridSize = CCS_getGridSize(ss);
	char *faceFlags = dm->getTessFaceDataArray(dm, CD_FLAGS);

	for (i=0; !CCFIter_isStopped(fi); i++,CCFIter_next(fi)) {
		CCFace *f = CCFIter_getCurrent(fi);
		int S, x, y, numVerts = CCS_getFaceNumVerts(f);
		int drawSmooth, index = cgdm_getFaceMapIndex(ss, f);
		int origIndex;

		origIndex = GET_INT_FROM_POINTER(CCS_getFaceFaceHandle(ss, f));

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
					VertData *faceGridData = CCS_getFaceGridDataArray(ss, f, S);
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

	CCFIter_free(fi);
}
static void cgdm_drawMappedEdges(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void *userData) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	CCEdgeIterator *ei = CCS_getEdgeIterator(ss);
	int i, useAging, edgeSize = CCS_getEdgeSize(ss);

	CCS_getUseAgeCounts(ss, &useAging, NULL, NULL, NULL);

	for (; !CCEIter_isStopped(ei); CCEIter_next(ei)) {
		CCEdge *e = CCEIter_getCurrent(ei);
		VertData *edgeData = CCS_getEdgeDataArray(ss, e);
		int index = cgdm_getEdgeMapIndex(ss, e);

		glBegin(GL_LINE_STRIP);
		if (index!=-1 && (!setDrawOptions || setDrawOptions(userData, index))) {
			if (useAging && !(G.f&G_BACKBUFSEL)) {
				int ageCol = 255-CCS_getEdgeAge(ss, e)*4;
				glColor3ub(0, ageCol>0?ageCol:0, 0);
			}

			for (i=0; i<edgeSize-1; i++) {
				glVertex3fv(edgeData[i].co);
				glVertex3fv(edgeData[i+1].co);
			}
		}
		glEnd();
	}

	CCEIter_free(ei);
}
static void cgdm_drawMappedEdgesInterp(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void (*setDrawInterpOptions)(void *userData, int index, float t), void *userData) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	CCEdgeIterator *ei = CCS_getEdgeIterator(ss);
	int i, useAging, edgeSize = CCS_getEdgeSize(ss);

	CCS_getUseAgeCounts(ss, &useAging, NULL, NULL, NULL);

	for (; !CCEIter_isStopped(ei); CCEIter_next(ei)) {
		CCEdge *e = CCEIter_getCurrent(ei);
		VertData *edgeData = CCS_getEdgeDataArray(ss, e);
		int index = cgdm_getEdgeMapIndex(ss, e);

		glBegin(GL_LINE_STRIP);
		if (index!=-1 && (!setDrawOptions || setDrawOptions(userData, index))) {
			for (i=0; i<edgeSize; i++) {
				setDrawInterpOptions(userData, index, (float) i/(edgeSize-1));

				if (useAging && !(G.f&G_BACKBUFSEL)) {
					int ageCol = 255-CCS_getEdgeAge(ss, e)*4;
					glColor3ub(0, ageCol>0?ageCol:0, 0);
				}

				glVertex3fv(edgeData[i].co);
			}
		}
		glEnd();
	}

	CCEIter_free(ei);
}
static void cgdm_foreachMappedFaceCenter(DerivedMesh *dm, void (*func)(void *userData, int index, float *co, float *no), void *userData) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;
	CSubSurf *ss = cgdm->ss;
	CCFaceIterator *fi = CCS_getFaceIterator(ss);

	for (; !CCFIter_isStopped(fi); CCFIter_next(fi)) {
		CCFace *f = CCFIter_getCurrent(fi);
		int index = cgdm_getFaceMapIndex(ss, f);

		if (index!=-1) {
				/* Face center data normal isn't updated atm. */
			VertData *vd = CCS_getFaceGridData(ss, f, 0, 0, 0);

			func(userData, index, vd->co, vd->no);
		}
	}

	CCFIter_free(fi);
}

static void cgdm_release(DerivedMesh *dm) {
	CCGDerivedMesh *cgdm = (CCGDerivedMesh*) dm;

	if (DM_release(dm)) {
		MEM_freeN(cgdm->vertMap);
		MEM_freeN(cgdm->edgeMap);
		MEM_freeN(cgdm->faceMap);
		MEM_freeN(cgdm);
	}
}


void ccg_loops_to_corners(CustomData *fdata, CustomData *ldata, 
			  CustomData *pdata, int loopstart, int findex, 
			  int polyindex, int numTex, int numCol) 
{
	MTFace *texface;
	MTexPoly *texpoly;
	MCol *mcol;
	MLoopCol *mloopcol;
	MLoopUV *mloopuv;
	int i, j;

	for(i=0; i < numTex; i++){
		texface = CustomData_get_n(fdata, CD_MTFACE, findex, i);
		texpoly = CustomData_get_n(pdata, CD_MTEXPOLY, polyindex, i);
		
		texface->tpage = texpoly->tpage;
		texface->flag = texpoly->flag;
		texface->transp = texpoly->transp;
		texface->mode = texpoly->mode;
		texface->tile = texpoly->tile;
		texface->unwrap = texpoly->unwrap;

		mloopuv = CustomData_get_n(ldata, CD_MLOOPUV, loopstart, i);
		for (j=0; j<4; j++, mloopuv++) {
			texface->uv[j][0] = mloopuv->uv[0];
			texface->uv[j][1] = mloopuv->uv[1];
		}
	}

	for(i=0; i < numCol; i++){
		mloopcol = CustomData_get_n(ldata, CD_MLOOPCOL, loopstart, i);
		mcol = CustomData_get_n(fdata, CD_MCOL, findex, i);

		for (j=0; j<4; j++, mloopcol++) {
			mcol[j].r = mloopcol->r;
			mcol[j].g = mloopcol->g;
			mcol[j].b = mloopcol->b;
			mcol[j].a = mloopcol->a;
		}
	}
}

static CCGDerivedMesh *getCCGDerivedMesh(CSubSurf *ss,
                                         int drawInteriorEdges,
                                         int useSubsurfUv,
                                         DerivedMesh *dm)
{
	CCGDerivedMesh *cgdm = MEM_callocN(sizeof(*cgdm), "cgdm");
	CCVertIterator *vi;
	CCEdgeIterator *ei;
	CCFaceIterator *fi;
	int index, totvert, totedge, totface;
	int i;
	int vertNum, edgeNum, faceNum;
	int *vertOrigIndex, *faceOrigIndex, *polyOrigIndex; /* *edgeOrigIndex - as yet, unused  */
	int *edgeFlags;
	char *faceFlags, *polyFlags;
	int *loopidx = NULL, *vertidx = NULL;
	V_DECLARE(loopidx);
	V_DECLARE(vertidx);
	int loopindex, loopindex2;
	int edgeSize;
	int gridSize;
	int gridFaces, gridCuts;
	int gridSideVerts;
	/*int gridInternalVerts; - as yet unused */
	int gridSideEdges;
	int numTex, numCol;
	int gridInternalEdges;
	int index2;
	float *w = NULL;
	WeightTable wtable = {0};
	V_DECLARE(w);
	/* MVert *mvert = NULL; - as yet unused */
	MCol *mcol;
	MEdge *medge = NULL;
	MFace *mface = NULL;
	/*a spare loop that's not used by anything*/
	int temp_loop = CCS_getNumFinalFaces(ss)*4;
	FaceVertWeight *qweight, *tweight;

	DM_from_template(&cgdm->dm, dm, CCS_getNumFinalVerts(ss),
					 CCS_getNumFinalEdges(ss),
					 CCS_getNumFinalFaces(ss),
					 CCS_getNumFinalFaces(ss)*4+1, 
					 CCS_getNumFinalFaces(ss));
	DM_add_tessface_layer(&cgdm->dm, CD_FLAGS, CD_CALLOC, NULL);
	DM_add_face_layer(&cgdm->dm, CD_FLAGS, CD_CALLOC, NULL);
	DM_add_edge_layer(&cgdm->dm, CD_FLAGS, CD_CALLOC, NULL);
	
	numTex = CustomData_number_of_layers(&cgdm->dm.loopData, CD_MLOOPUV);
	numCol = CustomData_number_of_layers(&cgdm->dm.loopData, CD_MLOOPCOL);
	
	if (numTex && CustomData_number_of_layers(&cgdm->dm.faceData, CD_MTFACE)==0)
		CustomData_from_bmeshpoly(&cgdm->dm.faceData, &cgdm->dm.polyData, &cgdm->dm.loopData, CCS_getNumFinalFaces(ss));
	else if (numCol && CustomData_number_of_layers(&cgdm->dm.faceData, CD_MCOL)==0)
		CustomData_from_bmeshpoly(&cgdm->dm.faceData, &cgdm->dm.polyData, &cgdm->dm.loopData, CCS_getNumFinalFaces(ss));

	CustomData_set_layer_flag(&cgdm->dm.faceData, CD_FLAGS, CD_FLAG_NOCOPY);
	CustomData_set_layer_flag(&cgdm->dm.edgeData, CD_FLAGS, CD_FLAG_NOCOPY);

	cgdm->dm.getMinMax = cgdm_getMinMax;
	cgdm->dm.getNumVerts = cgdm_getNumVerts;
	cgdm->dm.getNumTessFaces = cgdm_getNumTessFaces;
	cgdm->dm.getNumFaces = cgdm_getNumTessFaces;

	cgdm->dm.newFaceIter = cgdm_newFaceIter;

	cgdm->dm.getNumEdges = cgdm_getNumEdges;
	cgdm->dm.getVert = cgdm_getFinalVert;
	cgdm->dm.getEdge = cgdm_getFinalEdge;
	cgdm->dm.getTessFace = cgdm_getFinalFace;
	cgdm->dm.copyVertArray = cgdm_copyFinalVertArray;
	cgdm->dm.copyEdgeArray = cgdm_copyFinalEdgeArray;
	cgdm->dm.copyTessFaceArray = cgdm_copyFinalFaceArray;
	cgdm->dm.getVertData = DM_get_vert_data;
	cgdm->dm.getEdgeData = DM_get_edge_data;
	cgdm->dm.getTessFaceData = DM_get_face_data;
	cgdm->dm.getVertDataArray = DM_get_vert_data_layer;
	cgdm->dm.getEdgeDataArray = DM_get_edge_data_layer;
	cgdm->dm.getTessFaceDataArray = DM_get_tessface_data_layer;

	cgdm->dm.getVertCos = cgdm_getVertCos;
	cgdm->dm.foreachMappedVert = cgdm_foreachMappedVert;
	cgdm->dm.foreachMappedEdge = cgdm_foreachMappedEdge;
	cgdm->dm.foreachMappedFaceCenter = cgdm_foreachMappedFaceCenter;
	
	cgdm->dm.drawVerts = cgdm_drawVerts;
	cgdm->dm.drawEdges = cgdm_drawEdges;
	cgdm->dm.drawLooseEdges = cgdm_drawLooseEdges;
	cgdm->dm.drawFacesSolid = cgdm_drawFacesSolid;
	cgdm->dm.drawFacesColored = cgdm_drawFacesColored;
	cgdm->dm.drawFacesTex = cgdm_drawFacesTex;
	cgdm->dm.drawFacesGLSL = cgdm_drawFacesGLSL;
	cgdm->dm.drawMappedFaces = cgdm_drawMappedFaces;
	cgdm->dm.drawMappedFacesTex = cgdm_drawMappedFacesTex;
	cgdm->dm.drawMappedFacesGLSL = cgdm_drawMappedFacesGLSL;
	cgdm->dm.drawUVEdges = cgdm_drawUVEdges;

	cgdm->dm.drawMappedEdgesInterp = cgdm_drawMappedEdgesInterp;
	cgdm->dm.drawMappedEdges = cgdm_drawMappedEdges;
	
	cgdm->dm.release = cgdm_release;
	
	cgdm->ss = ss;
	cgdm->drawInteriorEdges = drawInteriorEdges;
	cgdm->useSubsurfUv = useSubsurfUv;

	totvert = CCS_getNumVerts(ss);
	cgdm->vertMap = MEM_mallocN(totvert * sizeof(*cgdm->vertMap), "vertMap");
	vi = CCS_getVertIterator(ss);
	for(; !CCVIter_isStopped(vi); CCVIter_next(vi)) {
		CCVert *v = CCVIter_getCurrent(vi);

		cgdm->vertMap[GET_INT_FROM_POINTER(CCS_getVertVertHandle(v))].vert = v;
	}
	CCVIter_free(vi);

	totedge = CCS_getNumEdges(ss);
	cgdm->edgeMap = MEM_mallocN(totedge * sizeof(*cgdm->edgeMap), "edgeMap");
	ei = CCS_getEdgeIterator(ss);
	for(; !CCEIter_isStopped(ei); CCEIter_next(ei)) {
		CCEdge *e = CCEIter_getCurrent(ei);

		cgdm->edgeMap[GET_INT_FROM_POINTER(CCS_getEdgeEdgeHandle(e))].edge = e;
	}

	totface = CCS_getNumFaces(ss);
	cgdm->faceMap = MEM_mallocN(totface * sizeof(*cgdm->faceMap), "faceMap");
	fi = CCS_getFaceIterator(ss);
	for(; !CCFIter_isStopped(fi); CCFIter_next(fi)) {
		CCFace *f = CCFIter_getCurrent(fi);

		cgdm->faceMap[GET_INT_FROM_POINTER(CCS_getFaceFaceHandle(ss, f))].face = f;
	}
	CCFIter_free(fi);

	edgeSize = CCS_getEdgeSize(ss);
	gridSize = CCS_getGridSize(ss);
	gridFaces = gridSize - 1;
	gridSideVerts = gridSize - 2;
	gridCuts = gridSize - 2;
	/*gridInternalVerts = gridSideVerts * gridSideVerts; - as yet, unused */
	gridSideEdges = gridSize - 1;
	gridInternalEdges = (gridSideEdges - 1) * gridSideEdges * 2; 

	calc_ss_weights(gridFaces, &qweight, &tweight);

	vertNum = 0;
	edgeNum = 0;
	faceNum = 0;

	/* mvert = dm->getVertArray(dm); - as yet unused */
	medge = dm->getEdgeArray(dm);
	mface = dm->getTessFaceArray(dm);

	vertOrigIndex = DM_get_vert_data_layer(&cgdm->dm, CD_ORIGINDEX);
	/*edgeOrigIndex = DM_get_edge_data_layer(&cgdm->dm, CD_ORIGINDEX);*/
	faceOrigIndex = DM_get_tessface_data_layer(&cgdm->dm, CD_ORIGINDEX);
	faceFlags = DM_get_tessface_data_layer(&cgdm->dm, CD_FLAGS);

	polyOrigIndex = DM_get_face_data_layer(&cgdm->dm, CD_ORIGINDEX);
	polyFlags = DM_get_face_data_layer(&cgdm->dm, CD_FLAGS);

	if (!CustomData_has_layer(&cgdm->dm.faceData, CD_MCOL))
		DM_add_tessface_layer(&cgdm->dm, CD_MCOL, CD_CALLOC, NULL);

	mcol = DM_get_tessface_data_layer(&cgdm->dm, CD_MCOL);

	index2 = 0;
	loopindex = loopindex2 = 0; //current loop index
	for (index = 0; index < totface; index++) {
		CCFace *f = cgdm->faceMap[index].face;
		int numVerts = CCS_getFaceNumVerts(f);
		int numFinalEdges = numVerts * (gridSideEdges + gridInternalEdges);
		int mapIndex = cgdm_getFaceMapIndex(ss, f);
		int origIndex = GET_INT_FROM_POINTER(CCS_getFaceFaceHandle(ss, f));
		int g2_wid = gridCuts+2;
		float *w2;
		int s, x, y;

		w = get_ss_weights(&wtable, gridCuts, numVerts);

		cgdm->faceMap[index].startVert = vertNum;
		cgdm->faceMap[index].startEdge = edgeNum;
		cgdm->faceMap[index].startFace = index2;
		
		V_RESET(loopidx);
		
		for (s=0; s<numVerts; s++) {
			V_GROW(loopidx);
			loopidx[s] = loopindex++;
		}
		
		V_RESET(vertidx);
		for(s = 0; s < numVerts; s++) {
			CCVert *v = CCS_getFaceVert(ss, f, s);
			
			V_GROW(vertidx);
			vertidx[s] = GET_INT_FROM_POINTER(CCS_getVertVertHandle(v));
		}
		

		w2 = w + s*numVerts*g2_wid*g2_wid;
		DM_interp_vert_data(dm, &cgdm->dm, vertidx, w2,
		                    numVerts, vertNum);
		if (vertOrigIndex)
			*vertOrigIndex = ORIGINDEX_NONE;
		++vertOrigIndex;
		++vertNum;

		/*interpolate per-vert data*/
		for(s = 0; s < numVerts; s++) {
			for(x = 1; x < gridFaces; x++) {
				w2 = w + s*numVerts*g2_wid*g2_wid + x*numVerts;
				DM_interp_vert_data(dm, &cgdm->dm, vertidx, w2,
						    numVerts, vertNum);

				if (vertOrigIndex)
					*vertOrigIndex = ORIGINDEX_NONE;
				++vertOrigIndex;
				++vertNum;
			}
		}

		for(s = 0; s < numVerts; s++) {
			for(y = 1; y < gridFaces; y++) {
				for(x = 1; x < gridFaces; x++) {
					w2 = w + s*numVerts*g2_wid*g2_wid + (y*g2_wid+x)*numVerts;
					DM_interp_vert_data(dm, &cgdm->dm, vertidx, w2,
							    numVerts, vertNum);

					if (vertOrigIndex) 
						*vertOrigIndex = ORIGINDEX_NONE;
					++vertOrigIndex;
					++vertNum;
				}
			}
		}

		/* set the face base vert */
		*((int*)CCS_getFaceUserData(ss, f)) = vertNum;
		for (s=0; s<numVerts; s++) {
			/*interpolate per-face data*/
			for (y=0; y<gridFaces; y++) {
				for (x=0; x<gridFaces; x++) {
					w2 = w + s*numVerts*g2_wid*g2_wid + (y*g2_wid+x)*numVerts;
					CustomData_interp(&dm->loopData, &cgdm->dm.loopData, 
					                  loopidx, w2, NULL, numVerts, loopindex2);
					loopindex2++;

					w2 = w + s*numVerts*g2_wid*g2_wid + ((y+1)*g2_wid+(x))*numVerts;
					CustomData_interp(&dm->loopData, &cgdm->dm.loopData, 
					                  loopidx, w2, NULL, numVerts, loopindex2);
					loopindex2++;

					w2 = w + s*numVerts*g2_wid*g2_wid + ((y+1)*g2_wid+(x+1))*numVerts;
					CustomData_interp(&dm->loopData, &cgdm->dm.loopData, 
					                  loopidx, w2, NULL, numVerts, loopindex2);
					loopindex2++;
					
					w2 = w + s*numVerts*g2_wid*g2_wid + ((y)*g2_wid+(x+1))*numVerts;
					CustomData_interp(&dm->loopData, &cgdm->dm.loopData, 
					                  loopidx, w2, NULL, numVerts, loopindex2);
					loopindex2++;
																			
					/*copy over poly data, e.g. mtexpoly*/
					CustomData_interp(&dm->polyData, &cgdm->dm.polyData, &origIndex, w, NULL, 1, index2);

					/*generate tesselated face data used for drawing*/
					ccg_loops_to_corners(&cgdm->dm.faceData, &cgdm->dm.loopData, 
						&cgdm->dm.polyData, loopindex2-4, index2, index2, numTex, numCol);
					
					index2++;
				}
			}
		}

		edgeNum += numFinalEdges;
	}

	for(index = 0; index < totvert; ++index) {
		CCVert *v = cgdm->vertMap[index].vert;
		int mapIndex = cgdm_getVertMapIndex(cgdm->ss, v);
		int vidx;

		vidx = GET_INT_FROM_POINTER(CCS_getVertVertHandle(v));

		cgdm->vertMap[index].startVert = vertNum;

		/* set the vert base vert */
		*((int*) CCS_getVertUserData(ss, v)) = vertNum;

		DM_copy_vert_data(dm, &cgdm->dm, vidx, vertNum, 1);

		*vertOrigIndex = mapIndex;
		++vertOrigIndex;
		++vertNum;
	}

	edgeFlags = DM_get_edge_data_layer(&cgdm->dm, CD_FLAGS);
	for(index = 0; index < totedge; ++index) {
		CCEdge *e = cgdm->edgeMap[index].edge;
		int numFinalEdges = edgeSize - 1;
		int mapIndex = cgdm_getEdgeMapIndex(ss, e);
		int x;
		int vertIdx[2];
		int edgeIdx = GET_INT_FROM_POINTER(CCS_getEdgeEdgeHandle(e));

		CCVert *v;
		v = CCS_getEdgeVert0(e);
		vertIdx[0] = GET_INT_FROM_POINTER(CCS_getVertVertHandle(v));
		v = CCS_getEdgeVert1(e);
		vertIdx[1] = GET_INT_FROM_POINTER(CCS_getVertVertHandle(v));

		cgdm->edgeMap[index].startVert = vertNum;
		cgdm->edgeMap[index].startEdge = edgeNum;

		/* set the edge base vert */
		*((int*)CCS_getEdgeUserData(ss, e)) = vertNum;

		for(x = 1; x < edgeSize - 1; x++) {
			float w[2];
			w[1] = (float) x / (edgeSize - 1);
			w[0] = 1 - w[1];
			DM_interp_vert_data(dm, &cgdm->dm, vertIdx, w, 2, vertNum);
			*vertOrigIndex = ORIGINDEX_NONE;
			++vertOrigIndex;
			++vertNum;
		}

		for(i = 0; i < numFinalEdges; ++i) {
			if(edgeIdx >= 0 && edgeFlags)
					edgeFlags[edgeNum + i] = medge[edgeIdx].flag;

			*(int *)DM_get_edge_data(&cgdm->dm, edgeNum + i,
			                         CD_ORIGINDEX) = mapIndex;
		}

		edgeNum += numFinalEdges;
	}
#if 0
	for(index = 0; index < totface; ++index) {
		CCFace *f = cgdm->faceMap[index].face;
		int numVerts = CCS_getFaceNumVerts(f);
		int numFinalEdges = numVerts * (gridSideEdges + gridInternalEdges);
		int mapIndex = cgdm_getFaceMapIndex(ss, f);
		int origIndex = GET_INT_FROM_POINTER(CCS_getFaceFaceHandle(ss, f));
		FaceVertWeight *weight = (numVerts == 4) ? qweight : tweight;
		int S, x, y;

		cgdm->faceMap[index].startVert = vertNum;
		cgdm->faceMap[index].startEdge = edgeNum;
		cgdm->faceMap[index].startFace = faceNum;

		/* set the face base vert */
		*((int*)CCS_getFaceUserData(ss, f)) = vertNum;
		for(S = 0; S < numVerts; S++) {
			CCVert *v = CCS_getFaceVert(ss, f, S);
			V_GROW(vertIdx);

			vertIdx[S] = GET_INT_FROM_POINTER(CCS_getVertVertHandle(v));
		}

		DM_interp_vert_data(dm, &cgdm->dm, vertIdx, weight[0][0],
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
#if 0 //BMESH_TODO
				w[prevS]  = weight[x][0][0];
				w[S]      = weight[x][0][1];
				w[nextS]  = weight[x][0][2];
				w[otherS] = weight[x][0][3];
				DM_interp_vert_data(dm, &cgdm->dm, vertIdx, w,
				                    numVerts, vertNum);
#endif
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
#if 0 //BMESH_TODO
					w[prevS]  = weight[y * gridFaces + x][0][0];
					w[S]      = weight[y * gridFaces + x][0][1];
					w[nextS]  = weight[y * gridFaces + x][0][2];
					w[otherS] = weight[y * gridFaces + x][0][3];
					DM_interp_vert_data(dm, &cgdm->dm, vertIdx, w,
					                    numVerts, vertNum);
#endif
					*vertOrigIndex = ORIGINDEX_NONE;
					++vertOrigIndex;
					++vertNum;
				}
			}
		}

		for(i = 0; i < numFinalEdges; ++i)
			*(int *)DM_get_edge_data(&cgdm->dm, edgeNum + i,
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

#if 1 //BMESH_TODO
					for(j = 0; j < 4; ++j) {
						w[j][prevS]  = (*weight)[j][0];
						w[j][S]      = (*weight)[j][1];
						w[j][nextS]  = (*weight)[j][2];
						w[j][otherS] = (*weight)[j][3];
					}

					DM_interp_tessface_data(dm, &cgdm->dm, &origIndex, NULL,
					                    &w, 1, faceNum);
					weight++;
#endif

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
		CustomData *fdata = &cgdm->dm.faceData;
		CustomData *dmfdata = &dm->faceData;
		int numlayer = CustomData_number_of_layers(fdata, CD_MTFACE);
		int dmnumlayer = CustomData_number_of_layers(dmfdata, CD_MTFACE);

		for (i=0; i<numlayer && i<dmnumlayer; i++)
			set_subsurf_uv(ss, dm, &cgdm->dm, i);
	}

	edgeFlags = DM_get_edge_data_layer(&cgdm->dm, CD_FLAGS);

	for(index = 0; index < totedge; ++index) {
		CCEdge *e = cgdm->edgeMap[index].edge;
		int numFinalEdges = edgeSize - 1;
		int mapIndex = cgdm_getEdgeMapIndex(ss, e);
		int x;
		int vertIdx[2];
		int edgeIdx = GET_INT_FROM_POINTER(CCS_getEdgeEdgeHandle(e));

		CCVert *v;
		v = CCS_getEdgeVert0(e);
		vertIdx[0] = GET_INT_FROM_POINTER(CCS_getVertVertHandle(v));
		v = CCS_getEdgeVert1(e);
		vertIdx[1] = GET_INT_FROM_POINTER(CCS_getVertVertHandle(v));

		cgdm->edgeMap[index].startVert = vertNum;
		cgdm->edgeMap[index].startEdge = edgeNum;

		/* set the edge base vert */
		*((int*)CCS_getEdgeUserData(ss, e)) = vertNum;

		for(x = 1; x < edgeSize - 1; x++) {
			float w[2];
			w[1] = (float) x / (edgeSize - 1);
			w[0] = 1 - w[1];
			DM_interp_vert_data(dm, &cgdm->dm, vertIdx, w, 2, vertNum);
			*vertOrigIndex = ORIGINDEX_NONE;
			++vertOrigIndex;
			++vertNum;
		}

		for(i = 0; i < numFinalEdges; ++i) {
			if(edgeIdx >= 0 && edgeFlags)
					edgeFlags[edgeNum + i] = medge[edgeIdx].flag;

			*(int *)DM_get_edge_data(&cgdm->dm, edgeNum + i,
			                         CD_ORIGINDEX) = mapIndex;
		}

		edgeNum += numFinalEdges;
	}

	for(index = 0; index < totvert; ++index) {
		CCVert *v = cgdm->vertMap[index].vert;
		int mapIndex = cgdm_getVertMapIndex(cgdm->ss, v);
		int vertIdx;

		vertIdx = GET_INT_FROM_POINTER(CCS_getVertVertHandle(v));

		cgdm->vertMap[index].startVert = vertNum;

		/* set the vert base vert */
		*((int*) CCS_getVertUserData(ss, v)) = vertNum;

		DM_copy_vert_data(dm, &cgdm->dm, vertIdx, vertNum, 1);

		*vertOrigIndex = mapIndex;
		++vertOrigIndex;
		++vertNum;
	}

	MEM_freeN(qweight);
	MEM_freeN(tweight);

	V_FREE(vertIdx);
#endif
	return cgdm;
}

/***/

struct DerivedMesh *subsurf_make_derived_from_derived_with_multires(
                        struct DerivedMesh *dm,
                        struct SubsurfModifierData *smd,
			struct MultiresSubsurf *ms,
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
		CSubSurf *ss;
		int levels;
		
		levels= smd->renderLevels; // XXX get_render_subsurf_level(&scene->r, smd->renderLevels);
		if(levels == 0)
			return dm;
		
		ss = _getSubSurf(NULL, levels, 0, 1, useSimple);

		ss_sync_from_derivedmesh(ss, dm, vertCos, useSimple);

		result = ss_to_cdderivedmesh(ss, 0, drawInteriorEdges,
		                             useSubsurfUv, dm, ms);

		CCS_free(ss);
		
		return result;
	} else {
		int useIncremental = (smd->flags & eSubsurfModifierFlag_Incremental);
		int useAging = smd->flags & eSubsurfModifierFlag_DebugIncr;
		CSubSurf *ss;
		
		/* It is quite possible there is a much better place to do this. It
		 * depends a bit on how rigourously we expect this function to never
		 * be called in editmode. In semi-theory we could share a single
		 * cache, but the handles used inside and outside editmode are not
		 * the same so we would need some way of converting them. Its probably
		 * not worth the effort. But then why am I even writing this long
		 * comment that no one will read? Hmmm. - zr
		 */
		if(smd->emCache) {
			CCS_free(smd->emCache);
			smd->emCache = NULL;
		}

		if(useIncremental && isFinalCalc) {
			smd->mCache = ss = _getSubSurf(smd->mCache, smd->levels,
			                               useAging, 0, useSimple);

			ss_sync_from_derivedmesh(ss, dm, vertCos, useSimple);


			return ss_to_cdderivedmesh(ss, 0, drawInteriorEdges,
						   useSubsurfUv, dm, ms);

			/*return (DerivedMesh *)getCCGDerivedMesh(smd->mCache,
		                                        drawInteriorEdges,
	                                            useSubsurfUv, dm);*/
		} else {
			if (smd->mCache && isFinalCalc) {
				CCS_free(smd->mCache);
				smd->mCache = NULL;
			}

			ss = _getSubSurf(NULL, smd->levels, 0, 1, useSimple);
			ss_sync_from_derivedmesh(ss, dm, vertCos, useSimple);

			/*smd->mCache = ss;
			result = (DerivedMesh *)getCCGDerivedMesh(smd->mCache,
		                                        drawInteriorEdges,
	                                            useSubsurfUv, dm);*/

			result = ss_to_cdderivedmesh(ss, 0, drawInteriorEdges,
			                             useSubsurfUv, dm, ms);

			CCS_free(ss);

			return result;
		}
	}
}

struct DerivedMesh *subsurf_make_derived_from_derived(
                        struct DerivedMesh *dm,
                        struct SubsurfModifierData *smd,
                        int useRenderParams, float (*vertCos)[3],
                        int isFinalCalc, int editMode)
{
	return subsurf_make_derived_from_derived_with_multires(dm, smd, NULL, useRenderParams, vertCos, isFinalCalc, editMode);
}

void subsurf_calculate_limit_positions(Mesh *me, float (*positions_r)[3]) 
{
	/* Finds the subsurf limit positions for the verts in a mesh 
	 * and puts them in an array of floats. Please note that the 
	 * calculated vert positions is incorrect for the verts 
	 * on the boundary of the mesh.
	 */
	CSubSurf *ss = _getSubSurf(NULL, 1, 0, 1, 0);
	float edge_sum[3], face_sum[3];
	CCVertIterator *vi;
	DerivedMesh *dm = CDDM_from_mesh(me, NULL);

	ss_sync_from_derivedmesh(ss, dm, NULL, 0);

	vi = CCS_getVertIterator(ss);
	for (; !CCVIter_isStopped(vi); CCVIter_next(vi)) {
		CCVert *v = CCVIter_getCurrent(vi);
		int idx = GET_INT_FROM_POINTER(CCS_getVertVertHandle(v));
		int N = CCS_getVertNumEdges(v);
		int numFaces = CCS_getVertNumFaces(v);
		float *co;
		int i;
                
		edge_sum[0]= edge_sum[1]= edge_sum[2]= 0.0;
		face_sum[0]= face_sum[1]= face_sum[2]= 0.0;

		for (i=0; i<N; i++) {
			CCEdge *e = CCS_getVertEdge(v, i);
			VecAddf(edge_sum, edge_sum, CCS_getEdgeData(ss, e, 1));
		}
		for (i=0; i<numFaces; i++) {
			CCFace *f = CCS_getVertFace(v, i);
			VecAddf(face_sum, face_sum, CCS_getFaceCenterData(f));
		}

		/* ad-hoc correction for boundary vertices, to at least avoid them
		   moving completely out of place (brecht) */
		if(numFaces && numFaces != N)
			VecMulf(face_sum, (float)N/(float)numFaces);

		co = CCS_getVertData(ss, v);
		positions_r[idx][0] = (co[0]*N*N + edge_sum[0]*4 + face_sum[0])/(N*(N+5));
		positions_r[idx][1] = (co[1]*N*N + edge_sum[1]*4 + face_sum[1])/(N*(N+5));
		positions_r[idx][2] = (co[2]*N*N + edge_sum[2]*4 + face_sum[2])/(N*(N+5));
	}
	CCVIter_free(vi);

	CCS_free(ss);

	dm->release(dm);
}

