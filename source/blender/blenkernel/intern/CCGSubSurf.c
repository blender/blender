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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/CCGSubSurf.c
 *  \ingroup bke
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"
#include "BLI_sys_types.h" // for intptr_t support

#include "BLI_utildefines.h" /* for BLI_assert */
#include "BLI_math.h"

#include "BKE_ccg.h"
#include "CCGSubSurf.h"
#include "CCGSubSurf_intern.h"
#include "BKE_subsurf.h"

#ifdef WITH_OPENSUBDIV
#  include "opensubdiv_capi.h"
#  include "opensubdiv_converter_capi.h"
#endif

#include "GL/glew.h"

/***/

int BKE_ccg_gridsize(int level)
{
	return ccg_gridsize(level);
}

int BKE_ccg_factor(int low_level, int high_level)
{
	BLI_assert(low_level > 0 && high_level > 0);
	BLI_assert(low_level <= high_level);

	return 1 << (high_level - low_level);
}

/***/

static CCGVert *_vert_new(CCGVertHDL vHDL, CCGSubSurf *ss)
{
	int num_vert_data = ss->subdivLevels + 1;
	CCGVert *v = CCGSUBSURF_alloc(ss,
	                              sizeof(CCGVert) +
	                              ss->meshIFC.vertDataSize * num_vert_data +
	                              ss->meshIFC.vertUserSize);
	byte *userData;

	v->vHDL = vHDL;
	v->edges = NULL;
	v->faces = NULL;
	v->numEdges = v->numFaces = 0;
	v->flags = 0;

	userData = ccgSubSurf_getVertUserData(ss, v);
	memset(userData, 0, ss->meshIFC.vertUserSize);
	if (ss->useAgeCounts) *((int *) &userData[ss->vertUserAgeOffset]) = ss->currentAge;

	return v;
}
static void _vert_remEdge(CCGVert *v, CCGEdge *e)
{
	int i;
	for (i = 0; i < v->numEdges; i++) {
		if (v->edges[i] == e) {
			v->edges[i] = v->edges[--v->numEdges];
			break;
		}
	}
}
static void _vert_remFace(CCGVert *v, CCGFace *f)
{
	int i;
	for (i = 0; i < v->numFaces; i++) {
		if (v->faces[i] == f) {
			v->faces[i] = v->faces[--v->numFaces];
			break;
		}
	}
}
static void _vert_addEdge(CCGVert *v, CCGEdge *e, CCGSubSurf *ss)
{
	v->edges = CCGSUBSURF_realloc(ss, v->edges, (v->numEdges + 1) * sizeof(*v->edges), v->numEdges * sizeof(*v->edges));
	v->edges[v->numEdges++] = e;
}
static void _vert_addFace(CCGVert *v, CCGFace *f, CCGSubSurf *ss)
{
	v->faces = CCGSUBSURF_realloc(ss, v->faces, (v->numFaces + 1) * sizeof(*v->faces), v->numFaces * sizeof(*v->faces));
	v->faces[v->numFaces++] = f;
}
static CCGEdge *_vert_findEdgeTo(const CCGVert *v, const CCGVert *vQ)
{
	int i;
	for (i = 0; i < v->numEdges; i++) {
		CCGEdge *e = v->edges[v->numEdges - 1 - i]; // XXX, note reverse
		if ((e->v0 == v && e->v1 == vQ) ||
		    (e->v1 == v && e->v0 == vQ))
		{
			return e;
		}
	}
	return NULL;
}
static void _vert_free(CCGVert *v, CCGSubSurf *ss)
{
	if (v->edges) {
		CCGSUBSURF_free(ss, v->edges);
	}

	if (v->faces) {
		CCGSUBSURF_free(ss, v->faces);
	}

	CCGSUBSURF_free(ss, v);
}

/***/

static CCGEdge *_edge_new(CCGEdgeHDL eHDL, CCGVert *v0, CCGVert *v1, float crease, CCGSubSurf *ss)
{
	int num_edge_data = ccg_edgebase(ss->subdivLevels + 1);
	CCGEdge *e = CCGSUBSURF_alloc(ss,
	                              sizeof(CCGEdge) +
	                              ss->meshIFC.vertDataSize * num_edge_data +
	                              ss->meshIFC.edgeUserSize);
	byte *userData;

	e->eHDL = eHDL;
	e->v0 = v0;
	e->v1 = v1;
	e->crease = crease;
	e->faces = NULL;
	e->numFaces = 0;
	e->flags = 0;
	_vert_addEdge(v0, e, ss);
	_vert_addEdge(v1, e, ss);

	userData = ccgSubSurf_getEdgeUserData(ss, e);
	memset(userData, 0, ss->meshIFC.edgeUserSize);
	if (ss->useAgeCounts) *((int *) &userData[ss->edgeUserAgeOffset]) = ss->currentAge;

	return e;
}
static void _edge_remFace(CCGEdge *e, CCGFace *f)
{
	int i;
	for (i = 0; i < e->numFaces; i++) {
		if (e->faces[i] == f) {
			e->faces[i] = e->faces[--e->numFaces];
			break;
		}
	}
}
static void _edge_addFace(CCGEdge *e, CCGFace *f, CCGSubSurf *ss)
{
	e->faces = CCGSUBSURF_realloc(ss, e->faces, (e->numFaces + 1) * sizeof(*e->faces), e->numFaces * sizeof(*e->faces));
	e->faces[e->numFaces++] = f;
}
static void *_edge_getCoVert(CCGEdge *e, CCGVert *v, int lvl, int x, int dataSize)
{
	int levelBase = ccg_edgebase(lvl);
	if (v == e->v0) {
		return &EDGE_getLevelData(e)[dataSize * (levelBase + x)];
	}
	else {
		return &EDGE_getLevelData(e)[dataSize * (levelBase + (1 << lvl) - x)];
	}
}

static void _edge_free(CCGEdge *e, CCGSubSurf *ss)
{
	if (e->faces) {
		CCGSUBSURF_free(ss, e->faces);
	}

	CCGSUBSURF_free(ss, e);
}
static void _edge_unlinkMarkAndFree(CCGEdge *e, CCGSubSurf *ss)
{
	_vert_remEdge(e->v0, e);
	_vert_remEdge(e->v1, e);
	e->v0->flags |= Vert_eEffected;
	e->v1->flags |= Vert_eEffected;
	_edge_free(e, ss);
}

static CCGFace *_face_new(CCGFaceHDL fHDL, CCGVert **verts, CCGEdge **edges, int numVerts, CCGSubSurf *ss)
{
	int maxGridSize = ccg_gridsize(ss->subdivLevels);
	int num_face_data = (numVerts * maxGridSize +
	                     numVerts * maxGridSize * maxGridSize + 1);
	CCGFace *f = CCGSUBSURF_alloc(ss,
	                              sizeof(CCGFace) +
	                              sizeof(CCGVert *) * numVerts +
	                              sizeof(CCGEdge *) * numVerts +
	                              ss->meshIFC.vertDataSize * num_face_data +
	                              ss->meshIFC.faceUserSize);
	byte *userData;
	int i;

	f->numVerts = numVerts;
	f->fHDL = fHDL;
	f->flags = 0;

	for (i = 0; i < numVerts; i++) {
		FACE_getVerts(f)[i] = verts[i];
		FACE_getEdges(f)[i] = edges[i];
		_vert_addFace(verts[i], f, ss);
		_edge_addFace(edges[i], f, ss);
	}

	userData = ccgSubSurf_getFaceUserData(ss, f);
	memset(userData, 0, ss->meshIFC.faceUserSize);
	if (ss->useAgeCounts) *((int *) &userData[ss->faceUserAgeOffset]) = ss->currentAge;

	return f;
}
static void _face_free(CCGFace *f, CCGSubSurf *ss)
{
	CCGSUBSURF_free(ss, f);
}
static void _face_unlinkMarkAndFree(CCGFace *f, CCGSubSurf *ss)
{
	int j;
	for (j = 0; j < f->numVerts; j++) {
		_vert_remFace(FACE_getVerts(f)[j], f);
		_edge_remFace(FACE_getEdges(f)[j], f);
		FACE_getVerts(f)[j]->flags |= Vert_eEffected;
	}
	_face_free(f, ss);
}

/***/

CCGSubSurf *ccgSubSurf_new(CCGMeshIFC *ifc, int subdivLevels, CCGAllocatorIFC *allocatorIFC, CCGAllocatorHDL allocator)
{
	if (!allocatorIFC) {
		allocatorIFC = ccg_getStandardAllocatorIFC();
		allocator = NULL;
	}

	if (subdivLevels < 1) {
		return NULL;
	}
	else {
		CCGSubSurf *ss = allocatorIFC->alloc(allocator, sizeof(*ss));

		ss->allocatorIFC = *allocatorIFC;
		ss->allocator = allocator;

		ss->vMap = ccg_ehash_new(0, &ss->allocatorIFC, ss->allocator);
		ss->eMap = ccg_ehash_new(0, &ss->allocatorIFC, ss->allocator);
		ss->fMap = ccg_ehash_new(0, &ss->allocatorIFC, ss->allocator);

		ss->meshIFC = *ifc;

		ss->subdivLevels = subdivLevels;
		ss->numGrids = 0;
		ss->allowEdgeCreation = 0;
		ss->defaultCreaseValue = 0;
		ss->defaultEdgeUserData = NULL;

		ss->useAgeCounts = 0;
		ss->vertUserAgeOffset = ss->edgeUserAgeOffset = ss->faceUserAgeOffset = 0;

		ss->calcVertNormals = 0;
		ss->normalDataOffset = 0;

		ss->allocMask = 0;

		ss->q = CCGSUBSURF_alloc(ss, ss->meshIFC.vertDataSize);
		ss->r = CCGSUBSURF_alloc(ss, ss->meshIFC.vertDataSize);

		ss->currentAge = 0;

		ss->syncState = eSyncState_None;

		ss->oldVMap = ss->oldEMap = ss->oldFMap = NULL;
		ss->lenTempArrays = 0;
		ss->tempVerts = NULL;
		ss->tempEdges = NULL;

#ifdef WITH_OPENSUBDIV
		ss->osd_evaluator = NULL;
		ss->osd_mesh = NULL;
		ss->osd_topology_refiner = NULL;
		ss->osd_mesh_invalid = false;
		ss->osd_coarse_coords_invalid = false;
		ss->osd_vao = 0;
		ss->skip_grids = false;
		ss->osd_compute = 0;
		ss->osd_next_face_ptex_index = 0;
		ss->osd_coarse_coords = NULL;
		ss->osd_num_coarse_coords = 0;
		ss->osd_subdiv_uvs = false;
#endif

		return ss;
	}
}

void ccgSubSurf_free(CCGSubSurf *ss)
{
	CCGAllocatorIFC allocatorIFC = ss->allocatorIFC;
	CCGAllocatorHDL allocator = ss->allocator;
#ifdef WITH_OPENSUBDIV
	if (ss->osd_evaluator != NULL) {
		openSubdiv_deleteEvaluatorDescr(ss->osd_evaluator);
	}
	if (ss->osd_mesh != NULL) {
		ccgSubSurf__delete_osdGLMesh(ss->osd_mesh);
	}
	if (ss->osd_vao != 0) {
		ccgSubSurf__delete_vertex_array(ss->osd_vao);
	}
	if (ss->osd_coarse_coords != NULL) {
		MEM_freeN(ss->osd_coarse_coords);
	}
	if (ss->osd_topology_refiner != NULL) {
		openSubdiv_deleteTopologyRefinerDescr(ss->osd_topology_refiner);
	}
#endif

	if (ss->syncState) {
		ccg_ehash_free(ss->oldFMap, (EHEntryFreeFP) _face_free, ss);
		ccg_ehash_free(ss->oldEMap, (EHEntryFreeFP) _edge_free, ss);
		ccg_ehash_free(ss->oldVMap, (EHEntryFreeFP) _vert_free, ss);

		MEM_freeN(ss->tempVerts);
		MEM_freeN(ss->tempEdges);
	}

	CCGSUBSURF_free(ss, ss->r);
	CCGSUBSURF_free(ss, ss->q);
	if (ss->defaultEdgeUserData) CCGSUBSURF_free(ss, ss->defaultEdgeUserData);

	ccg_ehash_free(ss->fMap, (EHEntryFreeFP) _face_free, ss);
	ccg_ehash_free(ss->eMap, (EHEntryFreeFP) _edge_free, ss);
	ccg_ehash_free(ss->vMap, (EHEntryFreeFP) _vert_free, ss);

	CCGSUBSURF_free(ss, ss);

	if (allocatorIFC.release) {
		allocatorIFC.release(allocator);
	}
}

CCGError ccgSubSurf_setAllowEdgeCreation(CCGSubSurf *ss, int allowEdgeCreation, float defaultCreaseValue, void *defaultUserData)
{
	if (ss->defaultEdgeUserData) {
		CCGSUBSURF_free(ss, ss->defaultEdgeUserData);
	}

	ss->allowEdgeCreation = !!allowEdgeCreation;
	ss->defaultCreaseValue = defaultCreaseValue;
	ss->defaultEdgeUserData = CCGSUBSURF_alloc(ss, ss->meshIFC.edgeUserSize);

	if (defaultUserData) {
		memcpy(ss->defaultEdgeUserData, defaultUserData, ss->meshIFC.edgeUserSize);
	}
	else {
		memset(ss->defaultEdgeUserData, 0, ss->meshIFC.edgeUserSize);
	}

	return eCCGError_None;
}
void ccgSubSurf_getAllowEdgeCreation(CCGSubSurf *ss, int *allowEdgeCreation_r, float *defaultCreaseValue_r, void *defaultUserData_r)
{
	if (allowEdgeCreation_r) *allowEdgeCreation_r = ss->allowEdgeCreation;
	if (ss->allowEdgeCreation) {
		if (defaultCreaseValue_r) *defaultCreaseValue_r = ss->defaultCreaseValue;
		if (defaultUserData_r) memcpy(defaultUserData_r, ss->defaultEdgeUserData, ss->meshIFC.edgeUserSize);
	}
}

CCGError ccgSubSurf_setSubdivisionLevels(CCGSubSurf *ss, int subdivisionLevels)
{
	if (subdivisionLevels <= 0) {
		return eCCGError_InvalidValue;
	}
	else if (subdivisionLevels != ss->subdivLevels) {
		ss->numGrids = 0;
		ss->subdivLevels = subdivisionLevels;
		ccg_ehash_free(ss->vMap, (EHEntryFreeFP) _vert_free, ss);
		ccg_ehash_free(ss->eMap, (EHEntryFreeFP) _edge_free, ss);
		ccg_ehash_free(ss->fMap, (EHEntryFreeFP) _face_free, ss);
		ss->vMap = ccg_ehash_new(0, &ss->allocatorIFC, ss->allocator);
		ss->eMap = ccg_ehash_new(0, &ss->allocatorIFC, ss->allocator);
		ss->fMap = ccg_ehash_new(0, &ss->allocatorIFC, ss->allocator);
	}

	return eCCGError_None;
}

void ccgSubSurf_getUseAgeCounts(CCGSubSurf *ss, int *useAgeCounts_r, int *vertUserOffset_r, int *edgeUserOffset_r, int *faceUserOffset_r)
{
	*useAgeCounts_r = ss->useAgeCounts;

	if (vertUserOffset_r) *vertUserOffset_r = ss->vertUserAgeOffset;
	if (edgeUserOffset_r) *edgeUserOffset_r = ss->edgeUserAgeOffset;
	if (faceUserOffset_r) *faceUserOffset_r = ss->faceUserAgeOffset;
}

CCGError ccgSubSurf_setUseAgeCounts(CCGSubSurf *ss, int useAgeCounts, int vertUserOffset, int edgeUserOffset, int faceUserOffset)
{
	if (useAgeCounts) {
		if ((vertUserOffset + 4 > ss->meshIFC.vertUserSize) ||
		    (edgeUserOffset + 4 > ss->meshIFC.edgeUserSize) ||
		    (faceUserOffset + 4 > ss->meshIFC.faceUserSize))
		{
			return eCCGError_InvalidValue;
		}
		else {
			ss->useAgeCounts = 1;
			ss->vertUserAgeOffset = vertUserOffset;
			ss->edgeUserAgeOffset = edgeUserOffset;
			ss->faceUserAgeOffset = faceUserOffset;
		}
	}
	else {
		ss->useAgeCounts = 0;
		ss->vertUserAgeOffset = ss->edgeUserAgeOffset = ss->faceUserAgeOffset = 0;
	}

	return eCCGError_None;
}

CCGError ccgSubSurf_setCalcVertexNormals(CCGSubSurf *ss, int useVertNormals, int normalDataOffset)
{
	if (useVertNormals) {
		if (normalDataOffset < 0 || normalDataOffset + 12 > ss->meshIFC.vertDataSize) {
			return eCCGError_InvalidValue;
		}
		else {
			ss->calcVertNormals = 1;
			ss->normalDataOffset = normalDataOffset;
		}
	}
	else {
		ss->calcVertNormals = 0;
		ss->normalDataOffset = 0;
	}

	return eCCGError_None;
}

void ccgSubSurf_setAllocMask(CCGSubSurf *ss, int allocMask, int maskOffset)
{
	ss->allocMask = allocMask;
	ss->maskDataOffset = maskOffset;
}

void ccgSubSurf_setNumLayers(CCGSubSurf *ss, int numLayers)
{
	ss->meshIFC.numLayers = numLayers;
}

/***/

CCGError ccgSubSurf_initFullSync(CCGSubSurf *ss)
{
	if (ss->syncState != eSyncState_None) {
		return eCCGError_InvalidSyncState;
	}

	ss->currentAge++;

	ss->oldVMap = ss->vMap;
	ss->oldEMap = ss->eMap;
	ss->oldFMap = ss->fMap;

	ss->vMap = ccg_ehash_new(0, &ss->allocatorIFC, ss->allocator);
	ss->eMap = ccg_ehash_new(0, &ss->allocatorIFC, ss->allocator);
	ss->fMap = ccg_ehash_new(0, &ss->allocatorIFC, ss->allocator);

	ss->numGrids = 0;

	ss->lenTempArrays = 12;
	ss->tempVerts = MEM_mallocN(sizeof(*ss->tempVerts) * ss->lenTempArrays, "CCGSubsurf tempVerts");
	ss->tempEdges = MEM_mallocN(sizeof(*ss->tempEdges) * ss->lenTempArrays, "CCGSubsurf tempEdges");

	ss->syncState = eSyncState_Vert;
#ifdef WITH_OPENSUBDIV
	ss->osd_next_face_ptex_index = 0;
#endif

	return eCCGError_None;
}

CCGError ccgSubSurf_initPartialSync(CCGSubSurf *ss)
{
	if (ss->syncState != eSyncState_None) {
		return eCCGError_InvalidSyncState;
	}

	ss->currentAge++;

	ss->syncState = eSyncState_Partial;

	return eCCGError_None;
}

CCGError ccgSubSurf_syncVertDel(CCGSubSurf *ss, CCGVertHDL vHDL)
{
	if (ss->syncState != eSyncState_Partial) {
		return eCCGError_InvalidSyncState;
	}
	else {
		void **prevp;
		CCGVert *v = ccg_ehash_lookupWithPrev(ss->vMap, vHDL, &prevp);

		if (!v || v->numFaces || v->numEdges) {
			return eCCGError_InvalidValue;
		}
		else {
			*prevp = v->next;
			_vert_free(v, ss);
		}
	}

	return eCCGError_None;
}

CCGError ccgSubSurf_syncEdgeDel(CCGSubSurf *ss, CCGEdgeHDL eHDL)
{
	if (ss->syncState != eSyncState_Partial) {
		return eCCGError_InvalidSyncState;
	}
	else {
		void **prevp;
		CCGEdge *e = ccg_ehash_lookupWithPrev(ss->eMap, eHDL, &prevp);

		if (!e || e->numFaces) {
			return eCCGError_InvalidValue;
		}
		else {
			*prevp = e->next;
			_edge_unlinkMarkAndFree(e, ss);
		}
	}

	return eCCGError_None;
}

CCGError ccgSubSurf_syncFaceDel(CCGSubSurf *ss, CCGFaceHDL fHDL)
{
	if (ss->syncState != eSyncState_Partial) {
		return eCCGError_InvalidSyncState;
	}
	else {
		void **prevp;
		CCGFace *f = ccg_ehash_lookupWithPrev(ss->fMap, fHDL, &prevp);

		if (!f) {
			return eCCGError_InvalidValue;
		}
		else {
			*prevp = f->next;
			_face_unlinkMarkAndFree(f, ss);
		}
	}

	return eCCGError_None;
}

CCGError ccgSubSurf_syncVert(CCGSubSurf *ss, CCGVertHDL vHDL, const void *vertData, int seam, CCGVert **v_r)
{
	void **prevp;
	CCGVert *v = NULL;
	short seamflag = (seam) ? Vert_eSeam : 0;

	if (ss->syncState == eSyncState_Partial) {
		v = ccg_ehash_lookupWithPrev(ss->vMap, vHDL, &prevp);
		if (!v) {
			v = _vert_new(vHDL, ss);
			VertDataCopy(ccg_vert_getCo(v, 0, ss->meshIFC.vertDataSize), vertData, ss);
			ccg_ehash_insert(ss->vMap, (EHEntry *) v);
			v->flags = Vert_eEffected | seamflag;
		}
		else if (!VertDataEqual(vertData, ccg_vert_getCo(v, 0, ss->meshIFC.vertDataSize), ss) ||
		         ((v->flags & Vert_eSeam) != seamflag))
		{
			int i, j;

			VertDataCopy(ccg_vert_getCo(v, 0, ss->meshIFC.vertDataSize), vertData, ss);
			v->flags = Vert_eEffected | seamflag;

			for (i = 0; i < v->numEdges; i++) {
				CCGEdge *e = v->edges[i];
				e->v0->flags |= Vert_eEffected;
				e->v1->flags |= Vert_eEffected;
			}
			for (i = 0; i < v->numFaces; i++) {
				CCGFace *f = v->faces[i];
				for (j = 0; j < f->numVerts; j++) {
					FACE_getVerts(f)[j]->flags |= Vert_eEffected;
				}
			}
		}
	}
	else {
		if (ss->syncState != eSyncState_Vert) {
			return eCCGError_InvalidSyncState;
		}

		v = ccg_ehash_lookupWithPrev(ss->oldVMap, vHDL, &prevp);
		if (!v) {
			v = _vert_new(vHDL, ss);
			VertDataCopy(ccg_vert_getCo(v, 0, ss->meshIFC.vertDataSize), vertData, ss);
			ccg_ehash_insert(ss->vMap, (EHEntry *) v);
			v->flags = Vert_eEffected | seamflag;
		}
		else if (!VertDataEqual(vertData, ccg_vert_getCo(v, 0, ss->meshIFC.vertDataSize), ss) ||
		         ((v->flags & Vert_eSeam) != seamflag))
		{
			*prevp = v->next;
			ccg_ehash_insert(ss->vMap, (EHEntry *) v);
			VertDataCopy(ccg_vert_getCo(v, 0, ss->meshIFC.vertDataSize), vertData, ss);
			v->flags = Vert_eEffected | Vert_eChanged | seamflag;
		}
		else {
			*prevp = v->next;
			ccg_ehash_insert(ss->vMap, (EHEntry *) v);
			v->flags = 0;
		}
#ifdef WITH_OPENSUBDIV
		v->osd_index = ss->vMap->numEntries - 1;
#endif
	}

	if (v_r) *v_r = v;
	return eCCGError_None;
}

CCGError ccgSubSurf_syncEdge(CCGSubSurf *ss, CCGEdgeHDL eHDL, CCGVertHDL e_vHDL0, CCGVertHDL e_vHDL1, float crease, CCGEdge **e_r)
{
	void **prevp;
	CCGEdge *e = NULL, *eNew;

	if (ss->syncState == eSyncState_Partial) {
		e = ccg_ehash_lookupWithPrev(ss->eMap, eHDL, &prevp);
		if (!e || e->v0->vHDL != e_vHDL0 || e->v1->vHDL != e_vHDL1 || crease != e->crease) {
			CCGVert *v0 = ccg_ehash_lookup(ss->vMap, e_vHDL0);
			CCGVert *v1 = ccg_ehash_lookup(ss->vMap, e_vHDL1);

			eNew = _edge_new(eHDL, v0, v1, crease, ss);

			if (e) {
				*prevp = eNew;
				eNew->next = e->next;

				_edge_unlinkMarkAndFree(e, ss);
			}
			else {
				ccg_ehash_insert(ss->eMap, (EHEntry *) eNew);
			}

			eNew->v0->flags |= Vert_eEffected;
			eNew->v1->flags |= Vert_eEffected;
		}
	}
	else {
		if (ss->syncState == eSyncState_Vert) {
			ss->syncState = eSyncState_Edge;
		}
		else if (ss->syncState != eSyncState_Edge) {
			return eCCGError_InvalidSyncState;
		}

		e = ccg_ehash_lookupWithPrev(ss->oldEMap, eHDL, &prevp);
		if (!e || e->v0->vHDL != e_vHDL0 || e->v1->vHDL != e_vHDL1 || e->crease != crease) {
			CCGVert *v0 = ccg_ehash_lookup(ss->vMap, e_vHDL0);
			CCGVert *v1 = ccg_ehash_lookup(ss->vMap, e_vHDL1);
			e = _edge_new(eHDL, v0, v1, crease, ss);
			ccg_ehash_insert(ss->eMap, (EHEntry *) e);
			e->v0->flags |= Vert_eEffected;
			e->v1->flags |= Vert_eEffected;
		}
		else {
			*prevp = e->next;
			ccg_ehash_insert(ss->eMap, (EHEntry *) e);
			e->flags = 0;
			if ((e->v0->flags | e->v1->flags) & Vert_eChanged) {
				e->v0->flags |= Vert_eEffected;
				e->v1->flags |= Vert_eEffected;
			}
		}
	}

	if (e_r) *e_r = e;
	return eCCGError_None;
}

CCGError ccgSubSurf_syncFace(CCGSubSurf *ss, CCGFaceHDL fHDL, int numVerts, CCGVertHDL *vHDLs, CCGFace **f_r)
{
	void **prevp;
	CCGFace *f = NULL, *fNew;
	int j, k, topologyChanged = 0;

	if (UNLIKELY(numVerts > ss->lenTempArrays)) {
		ss->lenTempArrays = (numVerts < ss->lenTempArrays * 2) ? ss->lenTempArrays * 2 : numVerts;
		ss->tempVerts = MEM_reallocN(ss->tempVerts, sizeof(*ss->tempVerts) * ss->lenTempArrays);
		ss->tempEdges = MEM_reallocN(ss->tempEdges, sizeof(*ss->tempEdges) * ss->lenTempArrays);
	}

	if (ss->syncState == eSyncState_Partial) {
		f = ccg_ehash_lookupWithPrev(ss->fMap, fHDL, &prevp);

		for (k = 0; k < numVerts; k++) {
			ss->tempVerts[k] = ccg_ehash_lookup(ss->vMap, vHDLs[k]);
		}
		for (k = 0; k < numVerts; k++) {
			ss->tempEdges[k] = _vert_findEdgeTo(ss->tempVerts[k], ss->tempVerts[(k + 1) % numVerts]);
		}

		if (f) {
			if (f->numVerts != numVerts ||
			    memcmp(FACE_getVerts(f), ss->tempVerts, sizeof(*ss->tempVerts) * numVerts) ||
			    memcmp(FACE_getEdges(f), ss->tempEdges, sizeof(*ss->tempEdges) * numVerts))
			{
				topologyChanged = 1;
			}
		}

		if (!f || topologyChanged) {
			fNew = _face_new(fHDL, ss->tempVerts, ss->tempEdges, numVerts, ss);

			if (f) {
				ss->numGrids += numVerts - f->numVerts;

				*prevp = fNew;
				fNew->next = f->next;

				_face_unlinkMarkAndFree(f, ss);
			}
			else {
				ss->numGrids += numVerts;
				ccg_ehash_insert(ss->fMap, (EHEntry *) fNew);
			}

			for (k = 0; k < numVerts; k++)
				FACE_getVerts(fNew)[k]->flags |= Vert_eEffected;
		}
	}
	else {
		if (ss->syncState == eSyncState_Vert || ss->syncState == eSyncState_Edge) {
			ss->syncState = eSyncState_Face;
		}
		else if (ss->syncState != eSyncState_Face) {
			return eCCGError_InvalidSyncState;
		}

		f = ccg_ehash_lookupWithPrev(ss->oldFMap, fHDL, &prevp);

		for (k = 0; k < numVerts; k++) {
			ss->tempVerts[k] = ccg_ehash_lookup(ss->vMap, vHDLs[k]);

			if (!ss->tempVerts[k])
				return eCCGError_InvalidValue;
		}
		for (k = 0; k < numVerts; k++) {
			ss->tempEdges[k] = _vert_findEdgeTo(ss->tempVerts[k], ss->tempVerts[(k + 1) % numVerts]);

			if (!ss->tempEdges[k]) {
				if (ss->allowEdgeCreation) {
					CCGEdge *e = ss->tempEdges[k] = _edge_new((CCGEdgeHDL) - 1, ss->tempVerts[k], ss->tempVerts[(k + 1) % numVerts], ss->defaultCreaseValue, ss);
					ccg_ehash_insert(ss->eMap, (EHEntry *) e);
					e->v0->flags |= Vert_eEffected;
					e->v1->flags |= Vert_eEffected;
					if (ss->meshIFC.edgeUserSize) {
						memcpy(ccgSubSurf_getEdgeUserData(ss, e), ss->defaultEdgeUserData, ss->meshIFC.edgeUserSize);
					}
				}
				else {
					return eCCGError_InvalidValue;
				}
			}
		}

		if (f) {
			if (f->numVerts != numVerts ||
			    memcmp(FACE_getVerts(f), ss->tempVerts, sizeof(*ss->tempVerts) * numVerts) ||
			    memcmp(FACE_getEdges(f), ss->tempEdges, sizeof(*ss->tempEdges) * numVerts))
			{
				topologyChanged = 1;
			}
		}

		if (!f || topologyChanged) {
			f = _face_new(fHDL, ss->tempVerts, ss->tempEdges, numVerts, ss);
			ccg_ehash_insert(ss->fMap, (EHEntry *) f);
			ss->numGrids += numVerts;

			for (k = 0; k < numVerts; k++)
				FACE_getVerts(f)[k]->flags |= Vert_eEffected;
		}
		else {
			*prevp = f->next;
			ccg_ehash_insert(ss->fMap, (EHEntry *) f);
			f->flags = 0;
			ss->numGrids += f->numVerts;

			for (j = 0; j < f->numVerts; j++) {
				if (FACE_getVerts(f)[j]->flags & Vert_eChanged) {
					for (k = 0; k < f->numVerts; k++)
						FACE_getVerts(f)[k]->flags |= Vert_eEffected;
					break;
				}
			}
		}
#ifdef WITH_OPENSUBDIV
		f->osd_index = ss->osd_next_face_ptex_index;
		if (numVerts == 4) {
			ss->osd_next_face_ptex_index++;
		}
		else {
			ss->osd_next_face_ptex_index += numVerts;
		}
#endif
	}

	if (f_r) *f_r = f;
	return eCCGError_None;
}

static void ccgSubSurf__sync(CCGSubSurf *ss)
{
#ifdef WITH_OPENSUBDIV
	if (ss->skip_grids) {
		ccgSubSurf__sync_opensubdiv(ss);
	}
	else
#endif
	{
		ccgSubSurf__sync_legacy(ss);
	}
}

CCGError ccgSubSurf_processSync(CCGSubSurf *ss)
{
	if (ss->syncState == eSyncState_Partial) {
		ss->syncState = eSyncState_None;

		ccgSubSurf__sync(ss);
	}
	else if (ss->syncState) {
		ccg_ehash_free(ss->oldFMap, (EHEntryFreeFP) _face_unlinkMarkAndFree, ss);
		ccg_ehash_free(ss->oldEMap, (EHEntryFreeFP) _edge_unlinkMarkAndFree, ss);
		ccg_ehash_free(ss->oldVMap, (EHEntryFreeFP) _vert_free, ss);
		MEM_freeN(ss->tempEdges);
		MEM_freeN(ss->tempVerts);

		ss->lenTempArrays = 0;

		ss->oldFMap = ss->oldEMap = ss->oldVMap = NULL;
		ss->tempVerts = NULL;
		ss->tempEdges = NULL;

		ss->syncState = eSyncState_None;

		ccgSubSurf__sync(ss);
	}
	else {
		return eCCGError_InvalidSyncState;
	}

	return eCCGError_None;
}

void ccgSubSurf__allFaces(CCGSubSurf *ss, CCGFace ***faces, int *numFaces, int *freeFaces)
{
	CCGFace **array;
	int i, num;

	if (*faces == NULL) {
		array = MEM_mallocN(sizeof(*array) * ss->fMap->numEntries, "CCGSubsurf allFaces");
		num = 0;
		for (i = 0; i < ss->fMap->curSize; i++) {
			CCGFace *f = (CCGFace *) ss->fMap->buckets[i];

			for (; f; f = f->next)
				array[num++] = f;
		}

		*faces = array;
		*numFaces = num;
		*freeFaces = 1;
	}
	else {
		*freeFaces = 0;
	}
}

void ccgSubSurf__effectedFaceNeighbours(CCGSubSurf *ss, CCGFace **faces, int numFaces, CCGVert ***verts, int *numVerts, CCGEdge ***edges, int *numEdges)
{
	CCGVert **arrayV;
	CCGEdge **arrayE;
	int numV, numE, i, j;

	arrayV = MEM_mallocN(sizeof(*arrayV) * ss->vMap->numEntries, "CCGSubsurf arrayV");
	arrayE = MEM_mallocN(sizeof(*arrayE) * ss->eMap->numEntries, "CCGSubsurf arrayV");
	numV = numE = 0;

	for (i = 0; i < numFaces; i++) {
		CCGFace *f = faces[i];
		f->flags |= Face_eEffected;
	}

	for (i = 0; i < ss->vMap->curSize; i++) {
		CCGVert *v = (CCGVert *) ss->vMap->buckets[i];

		for (; v; v = v->next) {
			for (j = 0; j < v->numFaces; j++)
				if (!(v->faces[j]->flags & Face_eEffected))
					break;

			if (j == v->numFaces) {
				arrayV[numV++] = v;
				v->flags |= Vert_eEffected;
			}
		}
	}

	for (i = 0; i < ss->eMap->curSize; i++) {
		CCGEdge *e = (CCGEdge *) ss->eMap->buckets[i];

		for (; e; e = e->next) {
			for (j = 0; j < e->numFaces; j++)
				if (!(e->faces[j]->flags & Face_eEffected))
					break;

			if (j == e->numFaces) {
				e->flags |= Edge_eEffected;
				arrayE[numE++] = e;
			}
		}
	}

	*verts = arrayV;
	*numVerts = numV;
	*edges = arrayE;
	*numEdges = numE;
}

/* copy face grid coordinates to other places */
CCGError ccgSubSurf_updateFromFaces(CCGSubSurf *ss, int lvl, CCGFace **effectedF, int numEffectedF)
{
	int i, S, x, gridSize, cornerIdx, subdivLevels;
	int vertDataSize = ss->meshIFC.vertDataSize, freeF;

	subdivLevels = ss->subdivLevels;
	lvl = (lvl) ? lvl : subdivLevels;
	gridSize = ccg_gridsize(lvl);
	cornerIdx = gridSize - 1;

	ccgSubSurf__allFaces(ss, &effectedF, &numEffectedF, &freeF);

	for (i = 0; i < numEffectedF; i++) {
		CCGFace *f = effectedF[i];

		for (S = 0; S < f->numVerts; S++) {
			CCGEdge *e = FACE_getEdges(f)[S];
			CCGEdge *prevE = FACE_getEdges(f)[(S + f->numVerts - 1) % f->numVerts];

			VertDataCopy((float *)FACE_getCenterData(f), FACE_getIFCo(f, lvl, S, 0, 0), ss);
			VertDataCopy(VERT_getCo(FACE_getVerts(f)[S], lvl), FACE_getIFCo(f, lvl, S, cornerIdx, cornerIdx), ss);

			for (x = 0; x < gridSize; x++)
				VertDataCopy(FACE_getIECo(f, lvl, S, x), FACE_getIFCo(f, lvl, S, x, 0), ss);

			for (x = 0; x < gridSize; x++) {
				int eI = gridSize - 1 - x;
				VertDataCopy(_edge_getCoVert(e, FACE_getVerts(f)[S], lvl, eI, vertDataSize), FACE_getIFCo(f, lvl, S, cornerIdx, x), ss);
				VertDataCopy(_edge_getCoVert(prevE, FACE_getVerts(f)[S], lvl, eI, vertDataSize), FACE_getIFCo(f, lvl, S, x, cornerIdx), ss);
			}
		}
	}

	if (freeF) MEM_freeN(effectedF);

	return eCCGError_None;
}

/* copy other places to face grid coordinates */
CCGError ccgSubSurf_updateToFaces(CCGSubSurf *ss, int lvl, CCGFace **effectedF, int numEffectedF)
{
	int i, S, x, gridSize, cornerIdx, subdivLevels;
	int vertDataSize = ss->meshIFC.vertDataSize, freeF;

	subdivLevels = ss->subdivLevels;
	lvl = (lvl) ? lvl : subdivLevels;
	gridSize = ccg_gridsize(lvl);
	cornerIdx = gridSize - 1;

	ccgSubSurf__allFaces(ss, &effectedF, &numEffectedF, &freeF);

	for (i = 0; i < numEffectedF; i++) {
		CCGFace *f = effectedF[i];

		for (S = 0; S < f->numVerts; S++) {
			int prevS = (S + f->numVerts - 1) % f->numVerts;
			CCGEdge *e = FACE_getEdges(f)[S];
			CCGEdge *prevE = FACE_getEdges(f)[prevS];

			for (x = 0; x < gridSize; x++) {
				int eI = gridSize - 1 - x;
				VertDataCopy(FACE_getIFCo(f, lvl, S, cornerIdx, x), _edge_getCoVert(e, FACE_getVerts(f)[S], lvl, eI, vertDataSize), ss);
				VertDataCopy(FACE_getIFCo(f, lvl, S, x, cornerIdx), _edge_getCoVert(prevE, FACE_getVerts(f)[S], lvl, eI, vertDataSize), ss);
			}

			for (x = 1; x < gridSize - 1; x++) {
				VertDataCopy(FACE_getIFCo(f, lvl, S, 0, x), FACE_getIECo(f, lvl, prevS, x), ss);
				VertDataCopy(FACE_getIFCo(f, lvl, S, x, 0), FACE_getIECo(f, lvl, S, x), ss);
			}

			VertDataCopy(FACE_getIFCo(f, lvl, S, 0, 0), (float *)FACE_getCenterData(f), ss);
			VertDataCopy(FACE_getIFCo(f, lvl, S, cornerIdx, cornerIdx), VERT_getCo(FACE_getVerts(f)[S], lvl), ss);
		}
	}

	if (freeF) MEM_freeN(effectedF);

	return eCCGError_None;
}

/* stitch together face grids, averaging coordinates at edges
 * and vertices, for multires displacements */
CCGError ccgSubSurf_stitchFaces(CCGSubSurf *ss, int lvl, CCGFace **effectedF, int numEffectedF)
{
	CCGVert **effectedV;
	CCGEdge **effectedE;
	int numEffectedV, numEffectedE, freeF;
	int i, S, x, gridSize, cornerIdx, subdivLevels, edgeSize;
	int vertDataSize = ss->meshIFC.vertDataSize;

	subdivLevels = ss->subdivLevels;
	lvl = (lvl) ? lvl : subdivLevels;
	gridSize = ccg_gridsize(lvl);
	edgeSize = ccg_edgesize(lvl);
	cornerIdx = gridSize - 1;

	ccgSubSurf__allFaces(ss, &effectedF, &numEffectedF, &freeF);
	ccgSubSurf__effectedFaceNeighbours(ss, effectedF, numEffectedF,
	                                   &effectedV, &numEffectedV, &effectedE, &numEffectedE);

	/* zero */
	for (i = 0; i < numEffectedV; i++) {
		CCGVert *v = effectedV[i];
		if (v->numFaces)
			VertDataZero(VERT_getCo(v, lvl), ss);
	}

	for (i = 0; i < numEffectedE; i++) {
		CCGEdge *e = effectedE[i];

		if (e->numFaces)
			for (x = 0; x < edgeSize; x++)
				VertDataZero(EDGE_getCo(e, lvl, x), ss);
	}

	/* add */
	for (i = 0; i < numEffectedF; i++) {
		CCGFace *f = effectedF[i];

		VertDataZero((float *)FACE_getCenterData(f), ss);

		for (S = 0; S < f->numVerts; S++)
			for (x = 0; x < gridSize; x++)
				VertDataZero(FACE_getIECo(f, lvl, S, x), ss);

		for (S = 0; S < f->numVerts; S++) {
			int prevS = (S + f->numVerts - 1) % f->numVerts;
			CCGEdge *e = FACE_getEdges(f)[S];
			CCGEdge *prevE = FACE_getEdges(f)[prevS];

			VertDataAdd((float *)FACE_getCenterData(f), FACE_getIFCo(f, lvl, S, 0, 0), ss);
			if (FACE_getVerts(f)[S]->flags & Vert_eEffected)
				VertDataAdd(VERT_getCo(FACE_getVerts(f)[S], lvl), FACE_getIFCo(f, lvl, S, cornerIdx, cornerIdx), ss);

			for (x = 1; x < gridSize - 1; x++) {
				VertDataAdd(FACE_getIECo(f, lvl, S, x), FACE_getIFCo(f, lvl, S, x, 0), ss);
				VertDataAdd(FACE_getIECo(f, lvl, prevS, x), FACE_getIFCo(f, lvl, S, 0, x), ss);
			}

			for (x = 0; x < gridSize - 1; x++) {
				int eI = gridSize - 1 - x;
				if (FACE_getEdges(f)[S]->flags & Edge_eEffected)
					VertDataAdd(_edge_getCoVert(e, FACE_getVerts(f)[S], lvl, eI, vertDataSize), FACE_getIFCo(f, lvl, S, cornerIdx, x), ss);
				if (FACE_getEdges(f)[prevS]->flags & Edge_eEffected)
					if (x != 0)
						VertDataAdd(_edge_getCoVert(prevE, FACE_getVerts(f)[S], lvl, eI, vertDataSize), FACE_getIFCo(f, lvl, S, x, cornerIdx), ss);
			}
		}
	}

	/* average */
	for (i = 0; i < numEffectedV; i++) {
		CCGVert *v = effectedV[i];
		if (v->numFaces)
			VertDataMulN(VERT_getCo(v, lvl), 1.0f / v->numFaces, ss);
	}

	for (i = 0; i < numEffectedE; i++) {
		CCGEdge *e = effectedE[i];

		VertDataCopy(EDGE_getCo(e, lvl, 0), VERT_getCo(e->v0, lvl), ss);
		VertDataCopy(EDGE_getCo(e, lvl, edgeSize - 1), VERT_getCo(e->v1, lvl), ss);

		if (e->numFaces)
			for (x = 1; x < edgeSize - 1; x++)
				VertDataMulN(EDGE_getCo(e, lvl, x), 1.0f / e->numFaces, ss);
	}

	/* copy */
	for (i = 0; i < numEffectedF; i++) {
		CCGFace *f = effectedF[i];

		VertDataMulN((float *)FACE_getCenterData(f), 1.0f / f->numVerts, ss);

		for (S = 0; S < f->numVerts; S++)
			for (x = 1; x < gridSize - 1; x++)
				VertDataMulN(FACE_getIECo(f, lvl, S, x), 0.5f, ss);

		for (S = 0; S < f->numVerts; S++) {
			int prevS = (S + f->numVerts - 1) % f->numVerts;
			CCGEdge *e = FACE_getEdges(f)[S];
			CCGEdge *prevE = FACE_getEdges(f)[prevS];

			VertDataCopy(FACE_getIFCo(f, lvl, S, 0, 0), (float *)FACE_getCenterData(f), ss);
			VertDataCopy(FACE_getIFCo(f, lvl, S, cornerIdx, cornerIdx), VERT_getCo(FACE_getVerts(f)[S], lvl), ss);

			for (x = 1; x < gridSize - 1; x++) {
				VertDataCopy(FACE_getIFCo(f, lvl, S, x, 0), FACE_getIECo(f, lvl, S, x), ss);
				VertDataCopy(FACE_getIFCo(f, lvl, S, 0, x), FACE_getIECo(f, lvl, prevS, x), ss);
			}

			for (x = 0; x < gridSize - 1; x++) {
				int eI = gridSize - 1 - x;

				VertDataCopy(FACE_getIFCo(f, lvl, S, cornerIdx, x), _edge_getCoVert(e, FACE_getVerts(f)[S], lvl, eI, vertDataSize), ss);
				VertDataCopy(FACE_getIFCo(f, lvl, S, x, cornerIdx), _edge_getCoVert(prevE, FACE_getVerts(f)[S], lvl, eI, vertDataSize), ss);
			}

			VertDataCopy(FACE_getIECo(f, lvl, S, 0), (float *)FACE_getCenterData(f), ss);
			VertDataCopy(FACE_getIECo(f, lvl, S, gridSize - 1), FACE_getIFCo(f, lvl, S, gridSize - 1, 0), ss);
		}
	}

	for (i = 0; i < numEffectedV; i++)
		effectedV[i]->flags = 0;
	for (i = 0; i < numEffectedE; i++)
		effectedE[i]->flags = 0;
	for (i = 0; i < numEffectedF; i++)
		effectedF[i]->flags = 0;

	MEM_freeN(effectedE);
	MEM_freeN(effectedV);
	if (freeF) MEM_freeN(effectedF);

	return eCCGError_None;
}

/*** External API accessor functions ***/

int ccgSubSurf_getNumVerts(const CCGSubSurf *ss)
{
	return ss->vMap->numEntries;
}
int ccgSubSurf_getNumEdges(const CCGSubSurf *ss)
{
	return ss->eMap->numEntries;
}
int ccgSubSurf_getNumFaces(const CCGSubSurf *ss)
{
	return ss->fMap->numEntries;
}

CCGVert *ccgSubSurf_getVert(CCGSubSurf *ss, CCGVertHDL v)
{
	return (CCGVert *) ccg_ehash_lookup(ss->vMap, v);
}
CCGEdge *ccgSubSurf_getEdge(CCGSubSurf *ss, CCGEdgeHDL e)
{
	return (CCGEdge *) ccg_ehash_lookup(ss->eMap, e);
}
CCGFace *ccgSubSurf_getFace(CCGSubSurf *ss, CCGFaceHDL f)
{
	return (CCGFace *) ccg_ehash_lookup(ss->fMap, f);
}

int ccgSubSurf_getSubdivisionLevels(const CCGSubSurf *ss)
{
	return ss->subdivLevels;
}
int ccgSubSurf_getEdgeSize(const CCGSubSurf *ss)
{
	return ccgSubSurf_getEdgeLevelSize(ss, ss->subdivLevels);
}
int ccgSubSurf_getEdgeLevelSize(const CCGSubSurf *ss, int level)
{
	if (level < 1 || level > ss->subdivLevels) {
		return -1;
	}
	else {
		return ccg_edgesize(level);
	}
}
int ccgSubSurf_getGridSize(const CCGSubSurf *ss)
{
	return ccgSubSurf_getGridLevelSize(ss, ss->subdivLevels);
}
int ccgSubSurf_getGridLevelSize(const CCGSubSurf *ss, int level)
{
	if (level < 1 || level > ss->subdivLevels) {
		return -1;
	}
	else {
		return ccg_gridsize(level);
	}
}

int ccgSubSurf_getSimpleSubdiv(const CCGSubSurf *ss)
{
	return ss->meshIFC.simpleSubdiv;
}

/* Vert accessors */

CCGVertHDL ccgSubSurf_getVertVertHandle(CCGVert *v)
{
	return v->vHDL;
}
int ccgSubSurf_getVertAge(CCGSubSurf *ss, CCGVert *v)
{
	if (ss->useAgeCounts) {
		byte *userData = ccgSubSurf_getVertUserData(ss, v);
		return ss->currentAge - *((int *) &userData[ss->vertUserAgeOffset]);
	}
	else {
		return 0;
	}
}
void *ccgSubSurf_getVertUserData(CCGSubSurf *ss, CCGVert *v)
{
	return VERT_getLevelData(v) + ss->meshIFC.vertDataSize * (ss->subdivLevels + 1);
}
int ccgSubSurf_getVertNumFaces(CCGVert *v)
{
	return v->numFaces;
}
CCGFace *ccgSubSurf_getVertFace(CCGVert *v, int index)
{
	if (index < 0 || index >= v->numFaces) {
		return NULL;
	}
	else {
		return v->faces[index];
	}
}
int ccgSubSurf_getVertNumEdges(CCGVert *v)
{
	return v->numEdges;
}
CCGEdge *ccgSubSurf_getVertEdge(CCGVert *v, int index)
{
	if (index < 0 || index >= v->numEdges) {
		return NULL;
	}
	else {
		return v->edges[index];
	}
}
void *ccgSubSurf_getVertData(CCGSubSurf *ss, CCGVert *v)
{
	return ccgSubSurf_getVertLevelData(ss, v, ss->subdivLevels);
}
void *ccgSubSurf_getVertLevelData(CCGSubSurf *ss, CCGVert *v, int level)
{
	if (level < 0 || level > ss->subdivLevels) {
		return NULL;
	}
	else {
		return ccg_vert_getCo(v, level, ss->meshIFC.vertDataSize);
	}
}

/* Edge accessors */

CCGEdgeHDL ccgSubSurf_getEdgeEdgeHandle(CCGEdge *e)
{
	return e->eHDL;
}
int ccgSubSurf_getEdgeAge(CCGSubSurf *ss, CCGEdge *e)
{
	if (ss->useAgeCounts) {
		byte *userData = ccgSubSurf_getEdgeUserData(ss, e);
		return ss->currentAge - *((int *) &userData[ss->edgeUserAgeOffset]);
	}
	else {
		return 0;
	}
}
void *ccgSubSurf_getEdgeUserData(CCGSubSurf *ss, CCGEdge *e)
{
	return (EDGE_getLevelData(e) +
	        ss->meshIFC.vertDataSize * ccg_edgebase(ss->subdivLevels + 1));
}
int ccgSubSurf_getEdgeNumFaces(CCGEdge *e)
{
	return e->numFaces;
}
CCGFace *ccgSubSurf_getEdgeFace(CCGEdge *e, int index)
{
	if (index < 0 || index >= e->numFaces) {
		return NULL;
	}
	else {
		return e->faces[index];
	}
}
CCGVert *ccgSubSurf_getEdgeVert0(CCGEdge *e)
{
	return e->v0;
}
CCGVert *ccgSubSurf_getEdgeVert1(CCGEdge *e)
{
	return e->v1;
}
void *ccgSubSurf_getEdgeDataArray(CCGSubSurf *ss, CCGEdge *e)
{
	return ccgSubSurf_getEdgeData(ss, e, 0);
}
void *ccgSubSurf_getEdgeData(CCGSubSurf *ss, CCGEdge *e, int x)
{
	return ccgSubSurf_getEdgeLevelData(ss, e, x, ss->subdivLevels);
}
void *ccgSubSurf_getEdgeLevelData(CCGSubSurf *ss, CCGEdge *e, int x, int level)
{
	if (level < 0 || level > ss->subdivLevels) {
		return NULL;
	}
	else {
		return ccg_edge_getCo(e, level, x, ss->meshIFC.vertDataSize);
	}
}
float ccgSubSurf_getEdgeCrease(CCGEdge *e)
{
	return e->crease;
}

/* Face accessors */

CCGFaceHDL ccgSubSurf_getFaceFaceHandle(CCGFace *f)
{
	return f->fHDL;
}
int ccgSubSurf_getFaceAge(CCGSubSurf *ss, CCGFace *f)
{
	if (ss->useAgeCounts) {
		byte *userData = ccgSubSurf_getFaceUserData(ss, f);
		return ss->currentAge - *((int *) &userData[ss->faceUserAgeOffset]);
	}
	else {
		return 0;
	}
}
void *ccgSubSurf_getFaceUserData(CCGSubSurf *ss, CCGFace *f)
{
	int maxGridSize = ccg_gridsize(ss->subdivLevels);
	return FACE_getCenterData(f) + ss->meshIFC.vertDataSize * (1 + f->numVerts * maxGridSize + f->numVerts * maxGridSize * maxGridSize);
}
int ccgSubSurf_getFaceNumVerts(CCGFace *f)
{
	return f->numVerts;
}
CCGVert *ccgSubSurf_getFaceVert(CCGFace *f, int index)
{
	if (index < 0 || index >= f->numVerts) {
		return NULL;
	}
	else {
		return FACE_getVerts(f)[index];
	}
}
CCGEdge *ccgSubSurf_getFaceEdge(CCGFace *f, int index)
{
	if (index < 0 || index >= f->numVerts) {
		return NULL;
	}
	else {
		return FACE_getEdges(f)[index];
	}
}
int ccgSubSurf_getFaceEdgeIndex(CCGFace *f, CCGEdge *e)
{
	int i;

	for (i = 0; i < f->numVerts; i++) {
		if (FACE_getEdges(f)[i] == e) {
			return i;
		}
	}
	return -1;
}
void *ccgSubSurf_getFaceCenterData(CCGFace *f)
{
	return FACE_getCenterData(f);
}
void *ccgSubSurf_getFaceGridEdgeDataArray(CCGSubSurf *ss, CCGFace *f, int gridIndex)
{
	return ccgSubSurf_getFaceGridEdgeData(ss, f, gridIndex, 0);
}
void *ccgSubSurf_getFaceGridEdgeData(CCGSubSurf *ss, CCGFace *f, int gridIndex, int x)
{
	return ccg_face_getIECo(f, ss->subdivLevels, gridIndex, x, ss->subdivLevels, ss->meshIFC.vertDataSize);
}
void *ccgSubSurf_getFaceGridDataArray(CCGSubSurf *ss, CCGFace *f, int gridIndex)
{
	return ccgSubSurf_getFaceGridData(ss, f, gridIndex, 0, 0);
}
void *ccgSubSurf_getFaceGridData(CCGSubSurf *ss, CCGFace *f, int gridIndex, int x, int y)
{
	return ccg_face_getIFCo(f, ss->subdivLevels, gridIndex, x, y, ss->subdivLevels, ss->meshIFC.vertDataSize);
}

/*** External API iterator functions ***/

void ccgSubSurf_initVertIterator(CCGSubSurf *ss, CCGVertIterator *viter)
{
	ccg_ehashIterator_init(ss->vMap, viter);
}
void ccgSubSurf_initEdgeIterator(CCGSubSurf *ss, CCGEdgeIterator *eiter)
{
	ccg_ehashIterator_init(ss->eMap, eiter);
}
void ccgSubSurf_initFaceIterator(CCGSubSurf *ss, CCGFaceIterator *fiter)
{
	ccg_ehashIterator_init(ss->fMap, fiter);
}

CCGVert *ccgVertIterator_getCurrent(CCGVertIterator *vi)
{
	return (CCGVert *) ccg_ehashIterator_getCurrent((EHashIterator *) vi);
}
int ccgVertIterator_isStopped(CCGVertIterator *vi)
{
	return ccg_ehashIterator_isStopped((EHashIterator *) vi);
}
void ccgVertIterator_next(CCGVertIterator *vi)
{
	ccg_ehashIterator_next((EHashIterator *) vi);
}

CCGEdge *ccgEdgeIterator_getCurrent(CCGEdgeIterator *vi)
{
	return (CCGEdge *) ccg_ehashIterator_getCurrent((EHashIterator *) vi);
}
int ccgEdgeIterator_isStopped(CCGEdgeIterator *vi)
{
	return ccg_ehashIterator_isStopped((EHashIterator *) vi);
}
void ccgEdgeIterator_next(CCGEdgeIterator *vi)
{
	ccg_ehashIterator_next((EHashIterator *) vi);
}

CCGFace *ccgFaceIterator_getCurrent(CCGFaceIterator *vi)
{
	return (CCGFace *) ccg_ehashIterator_getCurrent((EHashIterator *) vi);
}
int ccgFaceIterator_isStopped(CCGFaceIterator *vi)
{
	return ccg_ehashIterator_isStopped((EHashIterator *) vi);
}
void ccgFaceIterator_next(CCGFaceIterator *vi)
{
	ccg_ehashIterator_next((EHashIterator *) vi);
}

/*** Extern API final vert/edge/face interface ***/

int ccgSubSurf_getNumFinalVerts(const CCGSubSurf *ss)
{
	int edgeSize = ccg_edgesize(ss->subdivLevels);
	int gridSize = ccg_gridsize(ss->subdivLevels);
	int numFinalVerts = (ss->vMap->numEntries +
	                     ss->eMap->numEntries * (edgeSize - 2) +
	                     ss->fMap->numEntries +
	                     ss->numGrids * ((gridSize - 2) + ((gridSize - 2) * (gridSize - 2))));

#ifdef WITH_OPENSUBDIV
	if (ss->skip_grids) {
		return 0;
	}
#endif

	return numFinalVerts;
}
int ccgSubSurf_getNumFinalEdges(const CCGSubSurf *ss)
{
	int edgeSize = ccg_edgesize(ss->subdivLevels);
	int gridSize = ccg_gridsize(ss->subdivLevels);
	int numFinalEdges = (ss->eMap->numEntries * (edgeSize - 1) +
	                     ss->numGrids * ((gridSize - 1) + 2 * ((gridSize - 2) * (gridSize - 1))));
#ifdef WITH_OPENSUBDIV
	if (ss->skip_grids) {
		return 0;
	}
#endif
	return numFinalEdges;
}
int ccgSubSurf_getNumFinalFaces(const CCGSubSurf *ss)
{
	int gridSize = ccg_gridsize(ss->subdivLevels);
	int numFinalFaces = ss->numGrids * ((gridSize - 1) * (gridSize - 1));
#ifdef WITH_OPENSUBDIV
	if (ss->skip_grids) {
		return 0;
	}
#endif
	return numFinalFaces;
}

/***/

void CCG_key(CCGKey *key, const CCGSubSurf *ss, int level)
{
	key->level = level;

	key->elem_size = ss->meshIFC.vertDataSize;
	key->has_normals = ss->calcVertNormals;

	/* if normals are present, always the last three floats of an
	 * element */
	if (key->has_normals)
		key->normal_offset = key->elem_size - sizeof(float) * 3;
	else
		key->normal_offset = -1;

	key->grid_size = ccgSubSurf_getGridLevelSize(ss, level);
	key->grid_area = key->grid_size * key->grid_size;
	key->grid_bytes = key->elem_size * key->grid_area;

	key->has_mask = ss->allocMask;
	if (key->has_mask)
		key->mask_offset = ss->maskDataOffset;
	else
		key->mask_offset = -1;
}

void CCG_key_top_level(CCGKey *key, const CCGSubSurf *ss)
{
	CCG_key(key, ss, ccgSubSurf_getSubdivisionLevels(ss));
}
