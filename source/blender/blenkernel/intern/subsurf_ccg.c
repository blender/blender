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
#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_bad_level_calls.h"
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

typedef struct _SubSurf {
	CCGSubSurf *subSurf;

	int useAging;
	int controlType;
#define SUBSURF_CONTROLTYPE_MESH		1
#define SUBSURF_CONTROLTYPE_EDITMESH	2

		/* used by editmesh control type */
	EditMesh *em;

		/* used by mesh control type */
	Mesh *me;
} SubSurf;

typedef struct _VertData {
	float co[3];
	float no[3];
} VertData;

static void _subsurfNew_meshIFC_vertDataCopy(CCGMeshHDL mv, void *tv, void *av) {
	float *t = tv, *a = av;
	t[0] = a[0];
	t[1] = a[1];
	t[2] = a[2];
}
static int _subsurfNew_meshIFC_vertDataEqual(CCGMeshHDL mv, void *av, void *bv) {
	float *a = av, *b = bv;
	return (a[0]==b[0] && a[1]==b[1] && a[2]==b[2]);
}
static void _subsurfNew_meshIFC_vertDataZero(CCGMeshHDL mv, void *tv) {
	float *t = tv;
	t[0] = t[1] = t[2] = 0.0;
}
static void _subsurfNew_meshIFC_vertDataAdd(CCGMeshHDL mv, void *tav, void *bv) {
	float *ta = tav, *b = bv;
	ta[0]+= b[0];
	ta[1]+= b[1];
	ta[2]+= b[2];
}
static void _subsurfNew_meshIFC_vertDataSub(CCGMeshHDL mv, void *tav, void *bv) {
	float *ta = tav, *b = bv;
	ta[0]-= b[0];
	ta[1]-= b[1];
	ta[2]-= b[2];
}
static void _subsurfNew_meshIFC_vertDataMulN(CCGMeshHDL mv, void *tav, double n) {
	float *ta = tav;
	ta[0]*= (float) n;
	ta[1]*= (float) n;
	ta[2]*= (float) n;
}
static void _subsurfNew_meshIFC_ifc_vertDataAvg4(CCGMeshHDL mv, void *tv, void *av, void *bv, void *cv, void *dv) {
	float *t = tv, *a = av, *b = bv, *c = cv, *d = dv;
	t[0] = (a[0]+b[0]+c[0]+d[0])*0.25f;
	t[1] = (a[1]+b[1]+c[1]+d[1])*0.25f;
	t[2] = (a[2]+b[2]+c[2]+d[2])*0.25f;
}

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

static CCGSubSurf *_getSubSurf(SubSurf *ss, int subdivLevels, int useArena) {
	CCGMeshIFC ifc;
	CCGSubSurf *ccgSS;
	CCGAllocatorIFC allocatorIFC, *allocatorIFCp;
	CCGAllocatorHDL allocator;

	if (ss->useAging) {
		ifc.vertUserSize = ifc.edgeUserSize = ifc.faceUserSize = 8;
	} else {
		ifc.vertUserSize = ifc.edgeUserSize = ifc.faceUserSize = 4;
	}
	ifc.vertDataSize = sizeof(VertData);
	ifc.vertDataZero = _subsurfNew_meshIFC_vertDataZero;
	ifc.vertDataEqual = _subsurfNew_meshIFC_vertDataEqual;
	ifc.vertDataCopy = _subsurfNew_meshIFC_vertDataCopy;
	ifc.vertDataAdd = _subsurfNew_meshIFC_vertDataAdd;
	ifc.vertDataSub = _subsurfNew_meshIFC_vertDataSub;
	ifc.vertDataMulN = _subsurfNew_meshIFC_vertDataMulN;
	ifc.vertDataAvg4 = _subsurfNew_meshIFC_ifc_vertDataAvg4;

	if (useArena) {
		allocatorIFC.alloc = arena_alloc;
		allocatorIFC.realloc = arena_realloc;
		allocatorIFC.free = arena_free;
		allocatorIFC.release = arena_release;
		allocatorIFCp = &allocatorIFC;
		allocator = BLI_memarena_new((1<<16));

		ccgSS = ccgSubSurf_new(&ifc, ss, subdivLevels, allocatorIFCp, allocator);
	} else {
		ccgSS = ccgSubSurf_new(&ifc, ss, subdivLevels, NULL, NULL);
	}

	if (ss->useAging) {
		ccgSubSurf_setUseAgeCounts(ccgSS, 1, 4, 4, 4);
	}

	ccgSubSurf_setCalcVertexNormals(ccgSS, 1, BLI_STRUCT_OFFSET(VertData, no));

	return ccgSS;
}

static SubSurf *subSurf_fromEditmesh(EditMesh *em, int subdivLevels, int useAging, int useArena) {
	SubSurf *ss = MEM_mallocN(sizeof(*ss), "ss");

	ss->useAging = useAging;
	ss->controlType = SUBSURF_CONTROLTYPE_EDITMESH;
	ss->subSurf = _getSubSurf(ss, subdivLevels, useArena);
	ss->em = em;

	return ss;
}

static SubSurf *subSurf_fromMesh(Mesh *me, int useFlatSubdiv, int subdivLevels) {
	SubSurf *ss = MEM_mallocN(sizeof(*ss), "ss");

	ss->controlType = SUBSURF_CONTROLTYPE_MESH;
	ss->subSurf = _getSubSurf(ss, subdivLevels, 1);
	ss->me = me;

	ccgSubSurf_setAllowEdgeCreation(ss->subSurf, 1, useFlatSubdiv?subdivLevels:0.0);

	return ss;
}

static void subSurf_free(SubSurf *ss) {
	ccgSubSurf_free(ss->subSurf);
	MEM_freeN(ss);
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
static DispListMesh *subSurf_createDispListMesh(SubSurf *ssm) {
	CCGSubSurf *ss = ssm->subSurf;
	DispListMesh *dlm = MEM_callocN(sizeof(*dlm), "dlm");
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);
	int edgeIndexBase, edgeBase, faceIndexBase, faceBase;
	int i, j, k, S, x, y;
	int vertBase = 0;
	MFace *mf;
	CCGVertIterator *vi;
	CCGEdgeIterator *ei;
	CCGFaceIterator *fi;
	
	dlm->totvert = ccgSubSurf_getNumFinalVerts(ss);
	dlm->totedge = ccgSubSurf_getNumFinalEdges(ss);
	dlm->totface = ccgSubSurf_getNumFinalFaces(ss);

	dlm->mvert = MEM_callocN(dlm->totvert*sizeof(*dlm->mvert), "dlm->mvert");
	dlm->medge = MEM_callocN(dlm->totedge*sizeof(*dlm->medge), "dlm->medge");
	dlm->mface = MEM_callocN(dlm->totface*sizeof(*dlm->mface), "dlm->mface");
	if ((ssm->controlType==SUBSURF_CONTROLTYPE_MESH) && ssm->me->tface) {
		dlm->tface = MEM_callocN(dlm->totface*sizeof(*dlm->tface), "dlm->tface");
		dlm->mcol = NULL;
	} else if ((ssm->controlType==SUBSURF_CONTROLTYPE_MESH) && ssm->me->mcol) {
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

		if (ssm->controlType==SUBSURF_CONTROLTYPE_EDITMESH) {
			EditVert *ev = ccgSubSurf_getVertVertHandle(ss, v);
			
			ev->ssco = dlm->mvert[i].co;
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

	faceIndexBase = faceBase = i;
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
			med->flag = ME_EDGEDRAW;

			if (ssm->controlType==SUBSURF_CONTROLTYPE_EDITMESH) {
				EditEdge *ee = ccgSubSurf_getEdgeEdgeHandle(ss, e);

				if (ee->seam) {
					med->flag|= ME_SEAM;
				}
			} else {
				int edgeIdx = (int) ccgSubSurf_getEdgeEdgeHandle(ss, e);

					/* Edges created by lib have handle of -1 */
				if (edgeIdx!=-1 && ssm->me->medge) {
					MEdge *origMed = &ssm->me->medge[edgeIdx];

					med->flag|= (origMed->flag&ME_SEAM);
				}
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
				med->v1 = getFaceIndex(ss, f, k, x, 0, edgeSize, gridSize);
				med->v2 = getFaceIndex(ss, f, k, x+1, 0, edgeSize, gridSize);
				i++;
			}

			for (x=1; x<gridSize-1; x++) {
				for (y=0; y<gridSize-1; y++) {
					MEdge *med;
					
					med = &dlm->medge[i];
					med->v1 = getFaceIndex(ss, f, k, x, y, edgeSize, gridSize);
					med->v2 = getFaceIndex(ss, f, k, x, y+1, edgeSize, gridSize);
					i++;

					med = &dlm->medge[i];
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
		int numDataComponents;
		TFace *origTFace = NULL;
		MCol *origMCol = NULL;
		int mat_nr;
		int flag;

		if (ssm->controlType==SUBSURF_CONTROLTYPE_MESH) {
			int origIdx = (int) ccgSubSurf_getFaceFaceHandle(ss, f);
			MFace *origMFace = &((MFace*) ssm->me->mface)[origIdx];
			if (ssm->me->tface)
				origTFace = &((TFace*)ssm->me->tface)[origIdx];
			if (ssm->me->mcol)
				origMCol = &ssm->me->mcol[origIdx*4];
			mat_nr = origMFace->mat_nr;
			flag = origMFace->flag;
		} else {
			EditFace *ef = ccgSubSurf_getFaceFaceHandle(ss, f);
			mat_nr = ef->mat_nr;
			flag = ef->flag;
		}

		if (origTFace) {
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
		} else if (origMCol) {
			for (S=0; S<numVerts; S++) {
				unsigned char *col = (unsigned char*) &origMCol[S];
				corner_data[S][0] = col[0]/255.0f;
				corner_data[S][1] = col[1]/255.0f;
				corner_data[S][2] = col[2]/255.0f;
				corner_data[S][3] = col[3]/255.0f;
			}
			numDataComponents = 4;
		} else {
			numDataComponents = 0;
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
					mf = &dlm->mface[i];
					mf->v1 = getFaceIndex(ss, f, S, x+0, y+1, edgeSize, gridSize);
					mf->v2 = getFaceIndex(ss, f, S, x+1, y+1, edgeSize, gridSize);
					mf->v3 = getFaceIndex(ss, f, S, x+1, y+0, edgeSize, gridSize);
					mf->v4 = getFaceIndex(ss, f, S, x+0, y+0, edgeSize, gridSize);
					mf->mat_nr = mat_nr;
					mf->flag = flag;
					mf->edcode = 0;

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

	displistmesh_calc_normals(dlm);

	return dlm;
}

static void subSurf_sync(SubSurf *ss, int useFlatSubdiv) {
	float creaseFactor = (float) ccgSubSurf_getSubdivisionLevels(ss->subSurf);

	ccgSubSurf_initFullSync(ss->subSurf);

	if (ss->controlType==SUBSURF_CONTROLTYPE_MESH) {
		CCGVertHDL fVerts[4];
		int i;

		for (i=0; i<ss->me->totvert; i++) {
			ccgSubSurf_syncVert(ss->subSurf, (CCGVertHDL) i, ss->me->mvert[i].co);
		}

		if (ss->me->medge) {
			for (i=0; i<ss->me->totedge; i++) {
				MEdge *med = &ss->me->medge[i];
				float crease = useFlatSubdiv?creaseFactor:med->crease*creaseFactor/255.0f;

				ccgSubSurf_syncEdge(ss->subSurf, (CCGEdgeHDL) i, (CCGVertHDL) med->v1, (CCGVertHDL) med->v2, crease);
			}
		} else {
			for (i=0; i<ss->me->totface; i++) {
				MFace *mf = &((MFace*) ss->me->mface)[i];

				if (!mf->v3) {
					ccgSubSurf_syncEdge(ss->subSurf, (CCGEdgeHDL) i, (CCGVertHDL) mf->v1, (CCGVertHDL) mf->v2, useFlatSubdiv?creaseFactor:0.0);
				}
			}
		}

		for (i=0; i<ss->me->totface; i++) {
			MFace *mf = &((MFace*) ss->me->mface)[i];

			if (mf->v3) {
				fVerts[0] = (CCGVertHDL) mf->v1;
				fVerts[1] = (CCGVertHDL) mf->v2;
				fVerts[2] = (CCGVertHDL) mf->v3;
				fVerts[3] = (CCGVertHDL) mf->v4;

				ccgSubSurf_syncFace(ss->subSurf, (CCGFaceHDL) i, fVerts[3]?4:3, fVerts);
			}
		}
	} else {
		EditVert *ev, *fVerts[4];
		EditEdge *ee;
		EditFace *ef;

		for (ev=ss->em->verts.first; ev; ev=ev->next) {
			ccgSubSurf_syncVert(ss->subSurf, ev, ev->co);
		}

		for (ee=ss->em->edges.first; ee; ee=ee->next) {
			ccgSubSurf_syncEdge(ss->subSurf, ee, ee->v1, ee->v2, useFlatSubdiv?creaseFactor:ee->crease*creaseFactor);
		}

		for (ef=ss->em->faces.first; ef; ef=ef->next) {
			fVerts[0] = ef->v1;
			fVerts[1] = ef->v2;
			fVerts[2] = ef->v3;
			fVerts[3] = ef->v4;

			ccgSubSurf_syncFace(ss->subSurf, ef, ef->v4?4:3, (CCGVertHDL*) fVerts);
		}
	}

	ccgSubSurf_processSync(ss->subSurf);
}

/***/

typedef struct {
	DerivedMesh dm;

	SubSurf *ss;
} CCGDerivedMesh;

static int ccgDM_getNumVerts(DerivedMesh *dm) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;

	return ccgSubSurf_getNumFinalVerts(ccgdm->ss->subSurf);
}
static int ccgDM_getNumFaces(DerivedMesh *dm) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;

	return ccgSubSurf_getNumFinalFaces(ccgdm->ss->subSurf);
}
static void ccgDM_getMappedVertCoEM(DerivedMesh *dm, void *vert, float co_r[3]) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGVert *v = ccgSubSurf_getVert(ccgdm->ss->subSurf, vert);
	float *co = ccgSubSurf_getVertData(ccgdm->ss->subSurf, v);

	co_r[0] = co[0];
	co_r[1] = co[1];
	co_r[2] = co[2];
}
static DispListMesh *ccgDM_convertToDispListMesh(DerivedMesh *dm) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;

	return subSurf_createDispListMesh(ccgdm->ss);
}

static void ccgDM_drawVerts(DerivedMesh *dm) {
//	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
}
static void ccgDM_drawEdges(DerivedMesh *dm) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss->subSurf;
	CCGEdgeIterator *ei = ccgSubSurf_getEdgeIterator(ss);
	CCGFaceIterator *fi = ccgSubSurf_getFaceIterator(ss);
	int i, edgeSize = ccgSubSurf_getEdgeSize(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);

	for (; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);
		EditEdge *eed = ccgSubSurf_getEdgeEdgeHandle(ss, e);
		VertData *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);

		if (eed->h!=0)
			continue;

		if (ccgdm->ss->useAging && !(G.f&G_BACKBUFSEL)) {
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

	if (ccgdm->ss->useAging && !(G.f&G_BACKBUFSEL)) {
		glColor3ub(0, 0, 0);
	}

	for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		EditFace *efa = ccgSubSurf_getFaceFaceHandle(ss, f);
		int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);

		if (efa->h!=0)
			continue;

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

	ccgFaceIterator_free(fi);
	ccgEdgeIterator_free(ei);
}
static void ccgDM_drawMappedEdges(DerivedMesh *dm) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss->subSurf;
	CCGEdgeIterator *ei = ccgSubSurf_getEdgeIterator(ss);
	int i, edgeSize = ccgSubSurf_getEdgeSize(ss);

	for (; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);
		VertData *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);

		glBegin(GL_LINE_STRIP);
		for (i=0; i<edgeSize-1; i++) {
			glVertex3fv(edgeData[i].co);
			glVertex3fv(edgeData[i+1].co);
		}
		glEnd();
	}

	ccgEdgeIterator_free(ei);
}
static void ccgDM_drawLooseEdges(DerivedMesh *dm) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss->subSurf;
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

static void ccgDM_drawFacesSolid(DerivedMesh *dm, void (*setMaterial)(int)) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss->subSurf;
	CCGFaceIterator *fi = ccgSubSurf_getFaceIterator(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);

	for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		EditFace *efa = ccgSubSurf_getFaceFaceHandle(ss, f);
		int S, x, y, numVerts = ccgSubSurf_getFaceNumVerts(ss, f);
		int isSmooth = efa->flag&ME_SMOOTH;

		if (efa->h!=0)
			continue;

		setMaterial(efa->mat_nr+1);
		glShadeModel(isSmooth?GL_SMOOTH:GL_FLAT);
		for (S=0; S<numVerts; S++) {
			VertData *faceGridData = ccgSubSurf_getFaceGridDataArray(ss, f, S);

			if (isSmooth) {
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
//	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
}
static void ccgDM_drawFacesTex(DerivedMesh *dm, int (*setDrawParams)(TFace *tf, int matnr)) {
//	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
}

static void ccgDM_drawMappedVertsEM(DerivedMesh *dm, int (*setDrawOptions)(void *userData, EditVert *vert), void *userData) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss->subSurf;
	CCGVertIterator *vi = ccgSubSurf_getVertIterator(ss);

	bglBegin(GL_POINTS);
	for (; !ccgVertIterator_isStopped(vi); ccgVertIterator_next(vi)) {
		CCGVert *v = ccgVertIterator_getCurrent(vi);
		EditVert *vert = ccgSubSurf_getVertVertHandle(ss,v);

		if (!setDrawOptions || setDrawOptions(userData, vert)) {
			bglVertex3fv(ccgSubSurf_getVertData(ss, v));
		}
	}
	bglEnd();

	ccgVertIterator_free(vi);
}
static void ccgDM_drawMappedEdgeEM(DerivedMesh *dm, void *edge) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss->subSurf;
	CCGEdgeIterator *ei = ccgSubSurf_getEdgeIterator(ss);
	CCGEdge *e = ccgSubSurf_getEdge(ss, edge);
	VertData *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);
	int i, edgeSize = ccgSubSurf_getEdgeSize(ss);

	glBegin(GL_LINE_STRIP);
	for (i=0; i<edgeSize; i++)
		glVertex3fv(edgeData[i].co);
	glEnd();

	ccgEdgeIterator_free(ei);
}
static void ccgDM_drawMappedEdgesEM(DerivedMesh *dm, int (*setDrawOptions)(void *userData, EditEdge *edge), void *userData) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;
	CCGSubSurf *ss = ccgdm->ss->subSurf;
	CCGEdgeIterator *ei = ccgSubSurf_getEdgeIterator(ss);
	int i, edgeSize = ccgSubSurf_getEdgeSize(ss);

	for (; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);
		EditEdge *edge = ccgSubSurf_getEdgeEdgeHandle(ss, e);
		VertData *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);

		glBegin(GL_LINE_STRIP);
		if (!setDrawOptions || setDrawOptions(userData, edge)) {
			if (ccgdm->ss->useAging && !(G.f&G_BACKBUFSEL)) {
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
	CCGSubSurf *ss = ccgdm->ss->subSurf;
	CCGEdgeIterator *ei = ccgSubSurf_getEdgeIterator(ss);
	int i, edgeSize = ccgSubSurf_getEdgeSize(ss);

	for (; !ccgEdgeIterator_isStopped(ei); ccgEdgeIterator_next(ei)) {
		CCGEdge *e = ccgEdgeIterator_getCurrent(ei);
		EditEdge *edge = ccgSubSurf_getEdgeEdgeHandle(ss, e);
		VertData *edgeData = ccgSubSurf_getEdgeDataArray(ss, e);

		glBegin(GL_LINE_STRIP);
		if (!setDrawOptions || setDrawOptions(userData, edge)) {
			for (i=0; i<edgeSize; i++) {
				setDrawInterpOptions(userData, edge, (float) i/(edgeSize-1));

				if (ccgdm->ss->useAging && !(G.f&G_BACKBUFSEL)) {
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
	CCGSubSurf *ss = ccgdm->ss->subSurf;
	CCGFaceIterator *fi = ccgSubSurf_getFaceIterator(ss);
	int gridSize = ccgSubSurf_getGridSize(ss);

	for (; !ccgFaceIterator_isStopped(fi); ccgFaceIterator_next(fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(fi);
		EditFace *efa = ccgSubSurf_getFaceFaceHandle(ss, f);
		if (!setDrawOptions || setDrawOptions(userData, efa)) {
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

static void ccgDM_release(DerivedMesh *dm) {
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*) dm;

	subSurf_free(ccgdm->ss);

	MEM_freeN(ccgdm);
}

static CCGDerivedMesh *getCCGDerivedMesh(SubSurf *ss) {
	CCGDerivedMesh *ccgdm = MEM_mallocN(sizeof(*ccgdm), "dm");

	ccgdm->dm.getNumVerts = ccgDM_getNumVerts;
	ccgdm->dm.getNumFaces = ccgDM_getNumFaces;
	ccgdm->dm.getMappedVertCoEM = ccgDM_getMappedVertCoEM;
	ccgdm->dm.convertToDispListMesh = ccgDM_convertToDispListMesh;

	ccgdm->dm.drawVerts = ccgDM_drawVerts;
	ccgdm->dm.drawEdges = ccgDM_drawEdges;
	ccgdm->dm.drawMappedEdges = ccgDM_drawMappedEdges;
	ccgdm->dm.drawLooseEdges = ccgDM_drawLooseEdges;
	ccgdm->dm.drawFacesSolid = ccgDM_drawFacesSolid;
	ccgdm->dm.drawFacesColored = ccgDM_drawFacesColored;
	ccgdm->dm.drawFacesTex = ccgDM_drawFacesTex;

	ccgdm->dm.drawMappedVertsEM = ccgDM_drawMappedVertsEM;
	ccgdm->dm.drawMappedEdgeEM = ccgDM_drawMappedEdgeEM;
	ccgdm->dm.drawMappedEdgesInterpEM = ccgDM_drawMappedEdgesInterpEM;
	ccgdm->dm.drawMappedEdgesEM = ccgDM_drawMappedEdgesEM;
	ccgdm->dm.drawMappedFacesEM = ccgDM_drawMappedFacesEM;

	ccgdm->dm.release = ccgDM_release;
	
	ccgdm->ss = ss;

	return ccgdm;
}

/***/

DerivedMesh *subsurf_ccg_make_derived_from_editmesh(EditMesh *em, int useFlatSubdiv, int subdivLevels, DerivedMesh *oldDerived) {
	CCGDerivedMesh *ccgdm;

	if (oldDerived) {
		ccgdm = (CCGDerivedMesh*) oldDerived;
	} else {
		SubSurf *ss = subSurf_fromEditmesh(em, subdivLevels, G.rt==52, 0);
		ccgdm = getCCGDerivedMesh(ss);
	}

	subSurf_sync(ccgdm->ss, useFlatSubdiv);

	return (DerivedMesh*) ccgdm;
}

DerivedMesh *subsurf_ccg_make_derived_from_mesh(Mesh *me, int useFlatSubdiv, int subdivLevels) {
	SubSurf *ss = subSurf_fromMesh(me, useFlatSubdiv, subdivLevels);
	DispListMesh *dlm;

	subSurf_sync(ss, useFlatSubdiv);

	dlm = subSurf_createDispListMesh(ss);
	
	subSurf_free(ss);
	
	return derivedmesh_from_displistmesh(NULL, dlm);
}
