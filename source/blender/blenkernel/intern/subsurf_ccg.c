/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
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

#include "BKE_bad_level_calls.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_subsurf.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_arithb.h"
#include "BLI_linklist.h"
#include "BLI_memarena.h"

#include "BIF_gl.h"

#include "CCGSubSurf.h"

typedef struct _VertData {
	float co[3];
	float no[3];
} VertData;

typedef struct CCGDerivedMesh CCGDerivedMesh;

static EditVert *ccgDM_getVertHandle(CCGDerivedMesh *ccgdm, CCGVert *v);
static EditEdge *ccgDM_getEdgeHandle(CCGDerivedMesh *ccgdm, CCGEdge *e);
static EditFace *ccgDM_getFaceHandle(CCGDerivedMesh *ccgdm, CCGFace *f);

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

static CCGSubSurf *_getSubSurf(CCGSubSurf *prevSS, int subdivLevels, int useAging, int useArena, int useEdgeCreation, int useFlatSubdiv) {
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
		ifc.vertUserSize = ifc.edgeUserSize = ifc.faceUserSize = 8;
	} else {
		ifc.vertUserSize = ifc.edgeUserSize = ifc.faceUserSize = 4;
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
		ccgSubSurf_setUseAgeCounts(ccgSS, 1, 4, 4, 4);
	}
	if (useEdgeCreation) {
		ccgSubSurf_setAllowEdgeCreation(ccgSS, 1, useFlatSubdiv?subdivLevels:0.0f);
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
static DispListMesh *ss_to_displistmesh(CCGSubSurf *ss, CCGDerivedMesh *ccgdm, int ssFromEditmesh, int drawInteriorEdges, Mesh *inMe, DispListMesh *inDLM, EditVert ***vertMap_r, EditEdge ***edgeMap_r, EditFace ***faceMap_r) {
	DispListMesh *dlm = MEM_callocN(sizeof(*dlm), "dlm");
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);
	int edgeIndexBase, edgeBase, faceBase;
	int i, j, k, S, x, y;
	int vertBase = 0;
	TFace *tface = NULL;
	MEdge *medge = NULL;
	MFace *mface = NULL;
	MCol *mcol = NULL;
	CCGVertIterator *vi;
	CCGEdgeIterator *ei;
	CCGFaceIterator *fi;
	EditVert **vertMap = NULL;
	EditEdge **edgeMap = NULL;
	EditFace **faceMap = NULL;

	if (!ssFromEditmesh) {
		if (inDLM) {
			tface = inDLM->tface;
			medge = inDLM->medge;
			mface = inDLM->mface;
			mcol = inDLM->mcol;
		} else if (inMe) {
			tface = inMe->tface;
			medge = inMe->medge;
			mface = inMe->mface;
			mcol = inMe->mcol;
		}
	}

	dlm->totvert = ccgSubSurf_getNumFinalVerts(ss);
	dlm->totedge = ccgSubSurf_getNumFinalEdges(ss);
	dlm->totface = ccgSubSurf_getNumFinalFaces(ss);

	if (vertMap_r) {
		*vertMap_r = vertMap = MEM_callocN(dlm->totvert*sizeof(*vertMap), "vmap");
		*edgeMap_r = edgeMap = MEM_callocN(dlm->totedge*sizeof(*edgeMap), "emap");
		*faceMap_r = faceMap = MEM_callocN(dlm->totface*sizeof(*faceMap), "fmap");
	}

	dlm->mvert = MEM_callocN(dlm->totvert*sizeof(*dlm->mvert), "dlm->mvert");
	dlm->medge = MEM_callocN(dlm->totedge*sizeof(*dlm->medge), "dlm->medge");
	dlm->mface = MEM_callocN(dlm->totface*sizeof(*dlm->mface), "dlm->mface");
	if (!ssFromEditmesh && tface) {
		dlm->tface = MEM_callocN(dlm->totface*sizeof(*dlm->tface), "dlm->tface");
		dlm->mcol = NULL;
	} else if (!ssFromEditmesh && mcol) {
		dlm->tface = NULL;
		dlm->mcol = MEM_mallocN(dlm->totface*4*sizeof(*dlm->mcol), "dlm->mcol");
	} else {
		dlm->tface = NULL;
		dlm->mcol = NULL;
	}

		// load vertices

	vertBase = i = 0;
	vi = ccgSubSurf_getVertIterator(ss);
	for (; !ccgVertIterator_isStopped(vi); ccgVertIterator_next(vi)) {
		CCGVert *v = ccgVertIterator_getCurrent(vi);
		VecCopyf(dlm->mvert[i].co, ccgSubSurf_getVertData(ss, v));
		if (vertMap) {
			vertMap[i] = ccgDM_getVertHandle(ccgdm, v);
		}
		*((int*) ccgSubSurf_getVertUserData(ss, v)) = i++;
	}
	ccgVertIterator_free(vi);

	edgeIndexBase = edgeBase = i;
	ei = ccgSubSurf_getEdgeIterator(ss);
	for (; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);
		int x;

		for (x=1; x<edgeSize-1; x++) {
			VecCopyf(dlm->mvert[i++].co, ccgSubSurf_getEdgeData(ss, e, x));
		}

		*((int*) ccgSubSurf_getEdgeUserData(ss, e)) = edgeBase;
		edgeBase += edgeSize-2;
	}
	ccgEdgeIterator_free(ei);

	faceBase = i;
	fi = ccgSubSurf_getFaceIterator(ss);
	for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

		VecCopyf(dlm->mvert[i++].co, ccgSubSurf_getFaceCenterData(ss, f));
		
		for (S=0; S<numVerts; S++) {
			for (x=1; x<gridSize-1; x++) {
				VecCopyf(dlm->mvert[i++].co, ccgSubSurf_getFaceGridEdgeData(ss, f, S, x));
			}
		}

		for (S=0; S<numVerts; S++) {
			for (y=1; y<gridSize-1; y++) {
				for (x=1; x<gridSize-1; x++) {
					VecCopyf(dlm->mvert[i++].co, ccgSubSurf_getFaceGridData(ss, f, S, x, y));
				}
			}
		}

		*((int*) ccgSubSurf_getFaceUserData(ss, f)) = faceBase;
		faceBase += 1 + numVerts*((gridSize-2) + (gridSize-2)*(gridSize-2));
	}
	ccgFaceIterator_free(fi);

		// load edges

	i=0;
	ei = ccgSubSurf_getEdgeIterator(ss);
	for (; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);
		for (x=0; x<edgeSize-1; x++) {
			MEdge *med = &dlm->medge[i];
			med->v1 = getEdgeIndex(ss, e, x, edgeSize);
			med->v2 = getEdgeIndex(ss, e, x+1, edgeSize);
			med->flag = ME_EDGEDRAW|ME_EDGERENDER;

			if (!ccgSubSurf_getEdgeNumFaces(ss, e)) med->flag |= ME_LOOSEEDGE;

			if (ssFromEditmesh) {
				EditEdge *eed = ccgSubSurf_getEdgeEdgeHandle(ss, e);

				if (eed->seam) {
					med->flag|= ME_SEAM;
				}
			} else {
				int edgeIdx = (int) ccgSubSurf_getEdgeEdgeHandle(ss, e);

					/* Edges created by lib have handle of -1 */
				if (edgeIdx!=-1 && medge) {
					MEdge *origMed = &medge[edgeIdx];

					med->flag|= (origMed->flag&ME_SEAM);
				}
			}

			if (edgeMap) {
				edgeMap[i] = ccgDM_getEdgeHandle(ccgdm, e);
			}

			i++;
		}
	}
	ccgEdgeIterator_free(ei);

	fi = ccgSubSurf_getFaceIterator(ss);
	for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		int numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

		for (k=0; k<numVerts; k++) {
			for (x=0; x<gridSize-1; x++) {
				MEdge *med = &dlm->medge[i];
				if (drawInteriorEdges) med->flag = ME_EDGEDRAW|ME_EDGERENDER;
				med->v1 = getFaceIndex(ss, f, k, x, 0, edgeSize, gridSize);
				med->v2 = getFaceIndex(ss, f, k, x+1, 0, edgeSize, gridSize);
				i++;
			}

			for (x=1; x<gridSize-1; x++) {
				for (y=0; y<gridSize-1; y++) {
					MEdge *med;
					
					med = &dlm->medge[i];
					if (drawInteriorEdges) med->flag = ME_EDGEDRAW|ME_EDGERENDER;
					med->v1 = getFaceIndex(ss, f, k, x, y, edgeSize, gridSize);
					med->v2 = getFaceIndex(ss, f, k, x, y+1, edgeSize, gridSize);
					i++;

					med = &dlm->medge[i];
					if (drawInteriorEdges) med->flag = ME_EDGEDRAW|ME_EDGERENDER;
					med->v1 = getFaceIndex(ss, f, k, y, x, edgeSize, gridSize);
					med->v2 = getFaceIndex(ss, f, k, y+1, x, edgeSize, gridSize);
					i++;
				}
			}
		}
	}
	ccgFaceIterator_free(fi);

		// load faces

	i = 0;
	fi = ccgSubSurf_getFaceIterator(ss);
	for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		int numVerts = ccgSubSurf_getFaceNumVerts(ss, f);
		float edge_data[4][6];
		float corner_data[4][6];
		float center_data[6] = {0};
		int numDataComponents = 0;
		TFace *origTFace = NULL;
		int mat_nr;
		int flag;

		if (!ssFromEditmesh) {
			int origIdx = (int) ccgSubSurf_getFaceFaceHandle(ss, f);
			MFace *origMFace = &mface[origIdx];
			
			if (tface) {
				origTFace = &tface[origIdx];

				for (S=0; S<numVerts; S++) {
					unsigned char *col = (unsigned char*) &origTFace->col[S];
					corner_data[S][0] = col[0]/255.0f;
					corner_data[S][1] = col[1]/255.0f;
					corner_data[S][2] = col[2]/255.0f;
					corner_data[S][3] = col[3]/255.0f;
					corner_data[S][4] = origTFace->uv[S][0];
					corner_data[S][5] = origTFace->uv[S][1];
				}
				numDataComponents = 6;
			} else if (mcol) {
				MCol *origMCol = &mcol[origIdx*4];

				for (S=0; S<numVerts; S++) {
					unsigned char *col = (unsigned char*) &origMCol[S];
					corner_data[S][0] = col[0]/255.0f;
					corner_data[S][1] = col[1]/255.0f;
					corner_data[S][2] = col[2]/255.0f;
					corner_data[S][3] = col[3]/255.0f;
				}
				numDataComponents = 4;
			}

			mat_nr = origMFace->mat_nr;
			flag = origMFace->flag;
		} else {
			EditFace *ef = ccgSubSurf_getFaceFaceHandle(ss, f);
			mat_nr = ef->mat_nr;
			flag = ef->flag;
		}

		for (S=0; S<numVerts; S++) {
			for (k=0; k<numDataComponents; k++) {
				edge_data[S][k] = (corner_data[S][k] + corner_data[(S+1)%numVerts][k])*0.5f;
				center_data[k]+= corner_data[S][k];
			}
		}
		for (k=0; k<numDataComponents; k++) {
			center_data[k]/= numVerts;
		}

		for (S=0; S<numVerts; S++) {
			int prevS= (S-1+numVerts)%numVerts;

			for (y=0; y<gridSize-1; y++) {
				for (x=0; x<gridSize-1; x++) {
					MFace *mf = &dlm->mface[i];
					mf->v1 = getFaceIndex(ss, f, S, x+0, y+1, edgeSize, gridSize);
					mf->v2 = getFaceIndex(ss, f, S, x+1, y+1, edgeSize, gridSize);
					mf->v3 = getFaceIndex(ss, f, S, x+1, y+0, edgeSize, gridSize);
					mf->v4 = getFaceIndex(ss, f, S, x+0, y+0, edgeSize, gridSize);
					mf->mat_nr = mat_nr;
					mf->flag = flag;
					mf->edcode = 0;

					if (faceMap) {
						faceMap[i] = ccgDM_getFaceHandle(ccgdm, f);
					}

					if (x+1==gridSize-1)
						mf->edcode|= ME_V2V3;
					if (y+1==gridSize-1)
						mf->edcode|= ME_V1V2;

					for (j=0; j<4; j++) {
						int fx = x + (j==1||j==2);
						int fy = y + (j==0||j==1);
						float x_v = (float) fx/(gridSize-1);
						float y_v = (float) fy/(gridSize-1);
						float data[6];

						for (k=0; k<numDataComponents; k++) {
							data[k] = (center_data[k]*(1.0f-x_v) + edge_data[S][k]*x_v)*(1.0f-y_v) + 
									(edge_data[prevS][k]*(1.0f-x_v) + corner_data[S][k]*x_v)*y_v;
						}

						if (dlm->tface) {
							TFace *tf = &dlm->tface[i];
							unsigned char col[4];
							col[0] = (int) (data[0]*255);
							col[1] = (int) (data[1]*255);
							col[2] = (int) (data[2]*255);
							col[3] = (int) (data[3]*255);
							tf->col[j] = *((unsigned int*) col);
							tf->uv[j][0] = data[4];
							tf->uv[j][1] = data[5];
						} else if (dlm->mcol) {
							unsigned char *col = (unsigned char*) &dlm->mcol[i*4+j];
							col[0] = (int) (data[0]*255);
							col[1] = (int) (data[1]*255);
							col[2] = (int) (data[2]*255);
							col[3] = (int) (data[3]*255);
						}
					}
					if (dlm->tface) {
						TFace *tf = &dlm->tface[i];
						tf->tpage = origTFace->tpage;
						tf->flag = origTFace->flag;
						tf->transp = origTFace->transp;
						tf->mode = origTFace->mode;
						tf->tile = origTFace->tile;
					}

					i++;
				}
			}
		}
	}
	ccgFaceIterator_free(fi);

	mesh_calc_normals(dlm->mvert, dlm->totvert, dlm->mface, dlm->totface, &dlm->nors);

	return dlm;
}

static void ss_sync_from_mesh(CCGSubSurf *ss, Mesh *me, DispListMesh *dlm, float (*vertexCos)[3], int useFlatSubdiv) {
	float creaseFactor = (float) ccgSubSurf_getSubdivisionLevels(ss);
	CCGVertHDL fVerts[4];
	MVert *mvert = dlm?dlm->mvert:me->mvert;
	MEdge *medge = dlm?dlm->medge:me->medge;
	MFace *mface = dlm?dlm->mface:me->mface;
	int totvert = dlm?dlm->totvert:me->totvert;
	int totedge = dlm?dlm->totedge:me->totedge;
	int totface = dlm?dlm->totface:me->totface;
	int i;

	ccgSubSurf_initFullSync(ss);

	if (vertexCos) {
		for (i=0; i<totvert; i++) {
			ccgSubSurf_syncVert(ss, (CCGVertHDL) i, vertexCos[i]);
		}
	} else {
		for (i=0; i<totvert; i++) {
			ccgSubSurf_syncVert(ss, (CCGVertHDL) i, mvert[i].co);
		}
	}

	if (medge) {
		for (i=0; i<totedge; i++) {
			MEdge *med = &medge[i];
			float crease = useFlatSubdiv?creaseFactor:med->crease*creaseFactor/255.0f;

			ccgSubSurf_syncEdge(ss, (CCGEdgeHDL) i, (CCGVertHDL) med->v1, (CCGVertHDL) med->v2, crease);
		}
	} else {
		for (i=0; i<totface; i++) {
			MFace *mf = &((MFace*) mface)[i];

			if (!mf->v3) {
				ccgSubSurf_syncEdge(ss, (CCGEdgeHDL) i, (CCGVertHDL) mf->v1, (CCGVertHDL) mf->v2, useFlatSubdiv?creaseFactor:0.0f);
			}
		}
	}

	for (i=0; i<totface; i++) {
		MFace *mf = &((MFace*) mface)[i];

		if (mf->v3) {
			fVerts[0] = (CCGVertHDL) mf->v1;
			fVerts[1] = (CCGVertHDL) mf->v2;
			fVerts[2] = (CCGVertHDL) mf->v3;
			fVerts[3] = (CCGVertHDL) mf->v4;

			ccgSubSurf_syncFace(ss, (CCGFaceHDL) i, fVerts[3]?4:3, fVerts);
		}
	}

	ccgSubSurf_processSync(ss);
}

void ss_sync_from_editmesh(CCGSubSurf *ss, EditMesh *em, float (*vertCos)[3], int useFlatSubdiv)
{
	float creaseFactor = (float) ccgSubSurf_getSubdivisionLevels(ss);
	EditVert *ev, *fVerts[4];
	EditEdge *ee;
	EditFace *ef;

	ccgSubSurf_initFullSync(ss);

	if (vertCos) {
		int i=0;

		for (ev=em->verts.first; ev; ev=ev->next) {
			ccgSubSurf_syncVert(ss, ev, vertCos[i++]);
		}
	} else {
		for (ev=em->verts.first; ev; ev=ev->next) {
			ccgSubSurf_syncVert(ss, ev, ev->co);
		}
	}

	for (ee=em->edges.first; ee; ee=ee->next) {
		ccgSubSurf_syncEdge(ss, ee, ee->v1, ee->v2, useFlatSubdiv?creaseFactor:ee->crease*creaseFactor);
	}

	for (ef=em->faces.first; ef; ef=ef->next) {
		fVerts[0] = ef->v1;
		fVerts[1] = ef->v2;
		fVerts[2] = ef->v3;
		fVerts[3] = ef->v4;

		ccgSubSurf_syncFace(ss, ef, ef->v4?4:3, (CCGVertHDL*) fVerts);
	}

	ccgSubSurf_processSync(ss);
}

/***/

struct CCGDerivedMesh {
	DerivedMesh dm;

	CCGSubSurf *ss;
	int fromEditmesh, drawInteriorEdges;

	Mesh *me;
	DispListMesh *dlm;

	EditVert **vertMap;
	EditEdge **edgeMap;
	EditFace **faceMap;
};

static EditVert *ccgDM_getVertHandle(CCGDerivedMesh *ccgdm, CCGVert *v) {
	if (ccgdm->vertMap) {
		int index = (int) ccgSubSurf_getVertVertHandle(ccgdm->ss, v);

		return ccgdm->vertMap[index];
	} else {
		return ccgSubSurf_getVertVertHandle(ccgdm->ss, v);
	}
}
static EditEdge *ccgDM_getEdgeHandle(CCGDerivedMesh *ccgdm, CCGEdge *e) {
	if (ccgdm->vertMap) {
		int index = (int) ccgSubSurf_getEdgeEdgeHandle(ccgdm->ss, e);

		return ccgdm->edgeMap[index];
	} else {
		return ccgSubSurf_getEdgeEdgeHandle(ccgdm->ss, e);
	}
}
static EditFace *ccgDM_getFaceHandle(CCGDerivedMesh *ccgdm, CCGFace *f) {
	if (ccgdm->vertMap) {
		int index = (int) ccgSubSurf_getFaceFaceHandle(ccgdm->ss, f);

		return ccgdm->faceMap[index];
	} else {
		return ccgSubSurf_getFaceFaceHandle(ccgdm->ss, f);
	}
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
static int ccgDM_getNumFaces(DerivedMesh *dm) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;

	return ccgSubSurf_getNumFinalFaces(ccgdm->ss);
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

	i = 0;
	vi = ccgSubSurf_getVertIterator(ss);
	for (; !ccgVertIterator_isStopped(vi); ccgVertIterator_next(vi)) {
		CCGVert *v = ccgVertIterator_getCurrent(vi);
		VecCopyf(cos[i++], ccgSubSurf_getVertData(ss, v));
	}
	ccgVertIterator_free(vi);

	ei = ccgSubSurf_getEdgeIterator(ss);
	for (; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);
		int x;

		for (x=1; x<edgeSize-1; x++)
			VecCopyf(cos[i++], ccgSubSurf_getEdgeData(ss, e, x));
	}
	ccgEdgeIterator_free(ei);

	fi = ccgSubSurf_getFaceIterator(ss);
	for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

		VecCopyf(cos[i++], ccgSubSurf_getFaceCenterData(ss, f));
		for (S=0; S<numVerts; S++)
			for (x=1; x<gridSize-1; x++)
				VecCopyf(cos[i++], ccgSubSurf_getFaceGridEdgeData(ss, f, S, x));
		for (S=0; S<numVerts; S++)
			for (y=1; y<gridSize-1; y++)
				for (x=1; x<gridSize-1; x++)
					VecCopyf(cos[i++], ccgSubSurf_getFaceGridData(ss, f, S, x, y));
	}
	ccgFaceIterator_free(fi);
}
static void ccgDM_foreachMappedVertEM(DerivedMesh *dm, void (*func)(void *userData, EditVert *vert, float *co, float *no_f, short *no_s), void *userData) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGVertIterator *vi = ccgSubSurf_getVertIterator(ccgdm->ss);

	for (; !ccgVertIterator_isStopped(vi); ccgVertIterator_next(vi)) {
		CCGVert *v = ccgVertIterator_getCurrent(vi);
		VertData *vd = ccgSubSurf_getVertData(ccgdm->ss, v);

		func(userData, ccgDM_getVertHandle(ccgdm, v), vd->co, vd->no, NULL);
	}

	ccgVertIterator_free(vi);
}
static void ccgDM_foreachMappedEdgeEM(DerivedMesh *dm, void (*func)(void *userData, EditEdge *eed, float *v0co, float *v1co), void *userData) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGEdgeIterator *ei = ccgSubSurf_getEdgeIterator(ss);
	int i, edgeSize = ccgSubSurf_getEdgeSize(ss);

	for (; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);
		EditEdge *edge = ccgDM_getEdgeHandle(ccgdm, e);
		VertData *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);

		for (i=0; i<edgeSize-1; i++)
			func(userData, edge, edgeData[i].co, edgeData[i+1].co);
	}

	ccgEdgeIterator_free(ei);
}
static DispListMesh *ccgDM_convertToDispListMesh(DerivedMesh *dm, int allowShared) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;

	return ss_to_displistmesh(ccgdm->ss, ccgdm, ccgdm->fromEditmesh, ccgdm->drawInteriorEdges, ccgdm->me, ccgdm->dlm, NULL, NULL, NULL);
}
static DispListMesh *ccgDM_convertToDispListMeshMapped(DerivedMesh *dm, int allowShared, EditVert ***vertMap_r, EditEdge ***edgeMap_r, EditFace ***faceMap_r) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;

	return ss_to_displistmesh(ccgdm->ss, ccgdm, ccgdm->fromEditmesh, ccgdm->drawInteriorEdges, ccgdm->me, ccgdm->dlm, vertMap_r, edgeMap_r, faceMap_r);
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

		if (ccgdm->fromEditmesh) {
			EditEdge *eed = ccgDM_getEdgeHandle(ccgdm, e);
			if (eed && eed->h!=0)
				continue;
		}
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

			if (ccgdm->fromEditmesh) {
				EditFace *efa = ccgDM_getFaceHandle(ccgdm, f);
				if (efa && efa->h!=0)
					continue;
			}

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

		if (!ccgSubSurf_getEdgeNumFaces(ss, e)) {
			VertData *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);

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

static void ccgDM_drawFacesSolid(DerivedMesh *dm, int (*setMaterial)(int)) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGFaceIterator *fi = ccgSubSurf_getFaceIterator(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);

	for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);
		unsigned char flag,mat_nr;

		if (ccgdm->fromEditmesh || ccgdm->vertMap) {
			EditFace *efa = ccgDM_getFaceHandle(ccgdm, f);
			if (efa && efa->h!=0)
				continue;

			flag = efa->flag;
			mat_nr = efa->mat_nr;
		} else {
			int index = (int) ccgSubSurf_getFaceFaceHandle(ss, f);
			MFace *mf = (ccgdm->dlm?ccgdm->dlm->mface:ccgdm->me->mface) + index;
			flag = mf->flag;
			mat_nr = mf->mat_nr;
		}

		if (!setMaterial(mat_nr+1))
			continue;

		glShadeModel((flag&ME_SMOOTH)?GL_SMOOTH:GL_FLAT);
		for (S=0; S<numVerts; S++) {
			VertData *faceGridData = ccgSubSurf_getFaceGridDataArray(ss, f, S);

			if (flag&ME_SMOOTH) {
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
	}

	ccgFaceIterator_free(fi);
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
static void ccgDM_drawFacesTex(DerivedMesh *dm, int (*setDrawParams)(TFace *tf, int matnr)) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGFaceIterator *fi = ccgSubSurf_getFaceIterator(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);
	MFace *mface = ccgdm->dlm?ccgdm->dlm->mface:ccgdm->me->mface;
	TFace *tface = ccgdm->dlm?ccgdm->dlm->tface:ccgdm->me->tface;
	MCol *mcol = ccgdm->dlm?ccgdm->dlm->mcol:ccgdm->me->mcol;
//	float uv[4][2];
//	float col[4][3];

	glBegin(GL_QUADS);
	for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);
		int index = (int) ccgSubSurf_getFaceFaceHandle(ss, f);
		MFace *mf = &mface[index];
		TFace *tf = tface?&tface[index]:NULL;
		unsigned char *cp= NULL;
		
		if(tf && ((tf->flag&TF_HIDE) || (tf->mode&TF_INVISIBLE))) continue;

		if (setDrawParams(tf, mf->mat_nr)) {
			if (tf) {
				cp= (unsigned char *) tf->col;
			} else if (mcol) {
				cp= (unsigned char *) &mcol[index*4];
			}
		}

		for (S=0; S<numVerts; S++) {
			VertData *faceGridData = ccgSubSurf_getFaceGridDataArray(ss, f, S);
			for (y=0; y<gridSize-1; y++) {
				for (x=0; x<gridSize-1; x++) {
					VertData *a = &faceGridData[(y+0)*gridSize + x];
					VertData *b = &faceGridData[(y+0)*gridSize + x + 1];
					VertData *c = &faceGridData[(y+1)*gridSize + x + 1];
					VertData *d = &faceGridData[(y+1)*gridSize + x];

					if (!(mf->flag&ME_SMOOTH)) {
						float a_cX = c->co[0]-a->co[0], a_cY = c->co[1]-a->co[1], a_cZ = c->co[2]-a->co[2];
						float b_dX = d->co[0]-b->co[0], b_dY = d->co[1]-b->co[1], b_dZ = d->co[2]-b->co[2];
						float no[3];

						no[0] = b_dY*a_cZ - b_dZ*a_cY;
						no[1] = b_dZ*a_cX - b_dX*a_cZ;
						no[2] = b_dX*a_cY - b_dY*a_cX;

						glNormal3fv(no);
					}

//					if (tf) glTexCoord2fv(tf->uv[0]);
//					if (cp) glColor3ub(cp[3], cp[2], cp[1]);
//					if (mf->flag&ME_SMOOTH) glNormal3sv(mvert[mf->v1].no);
//					glVertex3fv(mvert[mf->v1].co);

/*
					{
						float x_v = (float) fx/(gridSize-1);
						float y_v = (float) fy/(gridSize-1);
						float data[6];

						for (k=0; k<numDataComponents; k++) {
							data[k] = (center_data[k]*(1.0f-x_v) + edge_data[S][k]*x_v)*(1.0f-y_v) + 
									(edge_data[prevS][k]*(1.0f-x_v) + corner_data[S][k]*x_v)*y_v;
					}
*/

//					if (cp) glColor3ub(cp[3], cp[2], cp[1]);
					if (mf->flag&ME_SMOOTH) glNormal3fv(d->no);
					glVertex3fv(d->co);
//					if (cp) glColor3ub(cp[7], cp[6], cp[5]);
					if (mf->flag&ME_SMOOTH) glNormal3fv(c->no);
					glVertex3fv(c->co);
//					if (cp) glColor3ub(cp[11], cp[10], cp[9]);
					if (mf->flag&ME_SMOOTH) glNormal3fv(b->no);
					glVertex3fv(b->co);
//					if (cp) glColor3ub(cp[15], cp[14], cp[13]);
					if (mf->flag&ME_SMOOTH) glNormal3fv(a->no);
					glVertex3fv(a->co);
				}
			}
		}
	}
	glEnd();

	ccgFaceIterator_free(fi);
/*
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me = mdm->me;
	MVert *mvert= mdm->verts;
	MFace *mface= me->mface;
	TFace *tface = me->tface;
	float *nors = mdm->nors;
	int a;

	for (a=0; a<me->totface; a++) {
		MFace *mf= &mface[a];
		if (tf) glTexCoord2fv(tf->uv[1]);
		if (cp) glColor3ub(cp[7], cp[6], cp[5]);
		if (mf->flag&ME_SMOOTH) glNormal3sv(mvert[mf->v2].no);
		glVertex3fv(mvert[mf->v2].co);

		if (tf) glTexCoord2fv(tf->uv[2]);
		if (cp) glColor3ub(cp[11], cp[10], cp[9]);
		if (mf->flag&ME_SMOOTH) glNormal3sv(mvert[mf->v3].no);
		glVertex3fv(mvert[mf->v3].co);

		if(mf->v4) {
			if (tf) glTexCoord2fv(tf->uv[3]);
			if (cp) glColor3ub(cp[15], cp[14], cp[13]);
			if (mf->flag&ME_SMOOTH) glNormal3sv(mvert[mf->v4].no);
			glVertex3fv(mvert[mf->v4].co);
		}
		glEnd();
	}
*/
}

static void ccgDM_drawMappedEdgesEM(DerivedMesh *dm, int (*setDrawOptions)(void *userData, EditEdge *edge), void *userData) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGEdgeIterator *ei = ccgSubSurf_getEdgeIterator(ss);
	int i, useAging, edgeSize = ccgSubSurf_getEdgeSize(ss);

	ccgSubSurf_getUseAgeCounts(ss, &useAging, NULL, NULL, NULL);

	for (; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);
		EditEdge *edge = ccgDM_getEdgeHandle(ccgdm, e);
		VertData *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);

		glBegin(GL_LINE_STRIP);
		if (edge && (!setDrawOptions || setDrawOptions(userData, edge))) {
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
static void ccgDM_drawMappedEdgesInterpEM(DerivedMesh *dm, int (*setDrawOptions)(void *userData, EditEdge *edge), void (*setDrawInterpOptions)(void *userData, EditEdge *edge, float t), void *userData) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGEdgeIterator *ei = ccgSubSurf_getEdgeIterator(ss);
	int i, useAging, edgeSize = ccgSubSurf_getEdgeSize(ss);

	ccgSubSurf_getUseAgeCounts(ss, &useAging, NULL, NULL, NULL);

	for (; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);
		EditEdge *edge = ccgDM_getEdgeHandle(ccgdm, e);
		VertData *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);

		glBegin(GL_LINE_STRIP);
		if (edge && (!setDrawOptions || setDrawOptions(userData, edge))) {
			for (i=0; i<edgeSize; i++) {
				setDrawInterpOptions(userData, edge, (float) i/(edgeSize-1));

				if (useAging && !(G.f&G_BACKBUFSEL)) {
					int ageCol = 255-ccgSubSurf_getEdgeAge(ss, e)*4;
					glColor3ub(0, ageCol>0?ageCol:0, 0);
				}

				glVertex3fv(edgeData[i].co);
			}
		}
		glEnd();
	}
}
static void ccgDM_drawMappedFacesEM(DerivedMesh *dm, int (*setDrawOptions)(void *userData, EditFace *face), void *userData) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGFaceIterator *fi = ccgSubSurf_getFaceIterator(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);

	for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		EditFace *efa = ccgDM_getFaceHandle(ccgdm, f);
		if (efa && (!setDrawOptions || setDrawOptions(userData, efa))) {
			int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

			for (S=0; S<numVerts; S++) {
				VertData *faceGridData = ccgSubSurf_getFaceGridDataArray(ss, f, S);

				for (y=0; y<gridSize-1; y++) {
					glBegin(GL_QUAD_STRIP);
					for (x=0; x<gridSize; x++) {
						glVertex3fv(faceGridData[(y+0)*gridSize + x].co);
						glVertex3fv(faceGridData[(y+1)*gridSize + x].co);
					}
					glEnd();
				}
			}
		}
	}

	ccgFaceIterator_free(fi);
}
static void ccgDM_foreachMappedFaceCenterEM(DerivedMesh *dm, void (*func)(void *userData, EditFace *efa, float *co, float *no), void *userData) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGFaceIterator *fi = ccgSubSurf_getFaceIterator(ss);

	for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		EditFace *efa = ccgDM_getFaceHandle(ccgdm, f);

		if (efa) {
				/* Face center data normal isn't updated atm. */
			VertData *vd = ccgSubSurf_getFaceGridData(ss, f, 0, 0, 0);

			func(userData, efa, vd->co, vd->no);
		}
	}

	ccgFaceIterator_free(fi);
}

static void ccgDM_release(DerivedMesh *dm) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;

	if (ccgdm->dlm) displistmesh_free(ccgdm->dlm);
	if (ccgdm->vertMap) {
		MEM_freeN(ccgdm->vertMap);
		MEM_freeN(ccgdm->edgeMap);
		MEM_freeN(ccgdm->faceMap);
	}

	MEM_freeN(ccgdm);
}

static CCGDerivedMesh *getCCGDerivedMesh(CCGSubSurf *ss, int fromEditmesh, int drawInteriorEdges, Mesh *me, DispListMesh *dlm, EditVert **vertMap, EditEdge **edgeMap, EditFace **faceMap) {
	CCGDerivedMesh *ccgdm = MEM_callocN(sizeof(*ccgdm), "ccgdm");

	ccgdm->dm.getMinMax = ccgDM_getMinMax;
	ccgdm->dm.getNumVerts = ccgDM_getNumVerts;
	ccgdm->dm.getNumFaces = ccgDM_getNumFaces;
	ccgdm->dm.getVertCos = ccgdm_getVertCos;
	ccgdm->dm.foreachMappedVertEM = ccgDM_foreachMappedVertEM;
	ccgdm->dm.foreachMappedEdgeEM = ccgDM_foreachMappedEdgeEM;
	ccgdm->dm.foreachMappedFaceCenterEM = ccgDM_foreachMappedFaceCenterEM;
	ccgdm->dm.convertToDispListMesh = ccgDM_convertToDispListMesh;
	ccgdm->dm.convertToDispListMeshMapped = ccgDM_convertToDispListMeshMapped;

	ccgdm->dm.drawVerts = ccgDM_drawVerts;
	ccgdm->dm.drawEdges = ccgDM_drawEdges;
	ccgdm->dm.drawLooseEdges = ccgDM_drawLooseEdges;
	ccgdm->dm.drawFacesSolid = ccgDM_drawFacesSolid;
	ccgdm->dm.drawFacesColored = ccgDM_drawFacesColored;
	ccgdm->dm.drawFacesTex = ccgDM_drawFacesTex;

	ccgdm->dm.drawMappedEdgesInterpEM = ccgDM_drawMappedEdgesInterpEM;
	ccgdm->dm.drawMappedEdgesEM = ccgDM_drawMappedEdgesEM;
	ccgdm->dm.drawMappedFacesEM = ccgDM_drawMappedFacesEM;

	ccgdm->dm.release = ccgDM_release;
	
	ccgdm->ss = ss;
	ccgdm->fromEditmesh = fromEditmesh;
	ccgdm->drawInteriorEdges = drawInteriorEdges;
	ccgdm->me = me;
	ccgdm->dlm = dlm;
	ccgdm->vertMap = vertMap;
	ccgdm->edgeMap = edgeMap;
	ccgdm->faceMap = faceMap;

	return ccgdm;
}

/***/

DerivedMesh *subsurf_make_derived_from_editmesh(EditMesh *em, SubsurfModifierData *smd, float (*vertCos)[3]) {
	int useSimple = smd->subdivType==ME_SIMPLE_SUBSURF;
	int useAging = smd->flags&eSubsurfModifierFlag_DebugIncr;
	int drawInteriorEdges = !(smd->flags&eSubsurfModifierFlag_ControlEdges);

	smd->emCache = _getSubSurf(smd->emCache, smd->levels, useAging, 0, 0, useSimple);
	ss_sync_from_editmesh(smd->emCache, em, vertCos, useSimple);

	return (DerivedMesh*) getCCGDerivedMesh(smd->emCache, 1, drawInteriorEdges, NULL, NULL, NULL, NULL, NULL);
}

DerivedMesh *subsurf_make_derived_from_dlm_em(DispListMesh *dlm, SubsurfModifierData *smd, float (*vertCos)[3], EditVert **vertMap, EditEdge **edgeMap, EditFace **faceMap) {
	int useSimple = smd->subdivType==ME_SIMPLE_SUBSURF;
	int useAging = smd->flags&eSubsurfModifierFlag_DebugIncr;
	int drawInteriorEdges = !(smd->flags&eSubsurfModifierFlag_ControlEdges);
		
	smd->emCache = _getSubSurf(smd->emCache, smd->levels, useAging, 0, 0, useSimple);

	ss_sync_from_mesh(smd->emCache, NULL, dlm, vertCos, useSimple);

	return (DerivedMesh*) getCCGDerivedMesh(smd->emCache, 0, drawInteriorEdges, NULL, dlm, vertMap, edgeMap, faceMap);
}

DerivedMesh *subsurf_make_derived_from_mesh(Mesh *me, DispListMesh *dlm, SubsurfModifierData *smd, int useRenderParams, float (*vertCos)[3], int isFinalCalc) {
	int useSimple = smd->subdivType==ME_SIMPLE_SUBSURF;
	int drawInteriorEdges = !(smd->flags&eSubsurfModifierFlag_ControlEdges);
	DispListMesh *ndlm;

		/* Do not use cache in render mode. */
	if (useRenderParams) {
		CCGSubSurf *ss = _getSubSurf(NULL, smd->renderLevels, 0, 1, 1, useSimple);

		ss_sync_from_mesh(ss, me, dlm, vertCos, useSimple);

		ndlm = ss_to_displistmesh(ss, NULL, 0, drawInteriorEdges, me, dlm, NULL, NULL, NULL);
		if (dlm) displistmesh_free(dlm);

		ccgSubSurf_free(ss);
		
		return derivedmesh_from_displistmesh(ndlm, NULL, NULL, NULL, NULL);
	} else {
		int useEdgeCreation = !(dlm?dlm->medge:me->medge);
		int useIncremental = (smd->flags&eSubsurfModifierFlag_Incremental) && !useEdgeCreation;
		int useAging = smd->flags&eSubsurfModifierFlag_DebugIncr;
		CCGSubSurf *ss;
		
			/* It is quite possible there is a much better place to do this. It
			 * depends a bit on how rigourously we expect this function to never
			 * be called in editmode. In semi-theory we could share a single
			 * cache, but the handles used inside and outside editmode are not
			 * the same so we would need some way of converting them. Its probably
			 * not worth the effort. But then why am I even writing this long
			 * comment that no one will read? Hmmm. - zr
			 */
		if (smd->emCache) {
			ccgSubSurf_free(smd->emCache);
			smd->emCache = NULL;
		}

		if (useIncremental && isFinalCalc) {
			smd->mCache = ss = _getSubSurf(smd->mCache, smd->levels, useAging, 0, 0, useSimple);

			ss_sync_from_mesh(ss, me, dlm, vertCos, useSimple);

			return (DerivedMesh*) getCCGDerivedMesh(ss, 0, drawInteriorEdges, me, dlm, NULL, NULL, NULL);
		} else {
			if (smd->mCache && isFinalCalc) {
				ccgSubSurf_free(smd->mCache);
				smd->mCache = NULL;
			}

			ss = _getSubSurf(NULL, smd->levels, 0, 1, useEdgeCreation, useSimple);
			ss_sync_from_mesh(ss, me, dlm, vertCos, useSimple);
			ndlm = ss_to_displistmesh(ss, NULL, 0, drawInteriorEdges, me, dlm, NULL, NULL, NULL);

			if (dlm) displistmesh_free(dlm);
			ccgSubSurf_free(ss);

			return derivedmesh_from_displistmesh(ndlm, NULL, NULL, NULL, NULL);
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
	CCGSubSurf *ss = _getSubSurf(NULL, 1, 0, 1, 0, 0);
	float edge_sum[3], face_sum[3];
	CCGVertIterator *vi;

	ss_sync_from_mesh(ss, me, NULL, NULL, 0);

	vi = ccgSubSurf_getVertIterator(ss);
	for (; !ccgVertIterator_isStopped(vi); ccgVertIterator_next(vi)) {
		CCGVert *v = ccgVertIterator_getCurrent(vi);
		int idx = (int) ccgSubSurf_getVertVertHandle(ss, v);
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

		co = ccgSubSurf_getVertData(ss, v);
		positions_r[idx][0] = (co[0]*N*N + edge_sum[0]*4 + face_sum[0])/(N*(N+5));
		positions_r[idx][1] = (co[1]*N*N + edge_sum[1]*4 + face_sum[1])/(N*(N+5));
		positions_r[idx][2] = (co[2]*N*N + edge_sum[2]*4 + face_sum[2])/(N*(N+5));
	}
	ccgVertIterator_free(vi);

	ccgSubSurf_free(ss);
}

