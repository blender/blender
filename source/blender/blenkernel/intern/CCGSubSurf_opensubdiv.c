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

/** \file blender/blenkernel/intern/CCGSubSurf_opensubdiv.c
 *  \ingroup bke
 */

#ifdef WITH_OPENSUBDIV

#include "MEM_guardedalloc.h"
#include "BLI_sys_types.h" // for intptr_t support

#include "BLI_utildefines.h" /* for BLI_assert */
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_threads.h"

#include "CCGSubSurf.h"
#include "CCGSubSurf_intern.h"

#include "BKE_DerivedMesh.h"
#include "BKE_subsurf.h"

#include "DNA_userdef_types.h"

#include "opensubdiv_capi.h"
#include "opensubdiv_converter_capi.h"
#include "opensubdiv_evaluator_capi.h"
#include "opensubdiv_gl_mesh_capi.h"
#include "opensubdiv_topology_refiner_capi.h"

#include "GPU_glew.h"
#include "GPU_extensions.h"

#define OSD_LOG if (false) printf

static bool compare_ccg_derivedmesh_topology(CCGSubSurf *ss, DerivedMesh *dm)
{
	const int num_verts = dm->getNumVerts(dm);
	const int num_edges = dm->getNumEdges(dm);
	const int num_polys = dm->getNumPolys(dm);
	const MEdge *medge = dm->getEdgeArray(dm);
	const MLoop *mloop = dm->getLoopArray(dm);
	const MPoly *mpoly = dm->getPolyArray(dm);

	/* Quick preliminary tests based on the number of verts and facces. */
	{
		if (num_verts != ss->vMap->numEntries ||
		    num_edges != ss->eMap->numEntries ||
		    num_polys != ss->fMap->numEntries)
		{
			return false;
		}
	}

	/* Rather slow check for faces topology change. */
	{
		CCGFaceIterator ccg_face_iter;
		for (ccgSubSurf_initFaceIterator(ss, &ccg_face_iter);
		     !ccgFaceIterator_isStopped(&ccg_face_iter);
		     ccgFaceIterator_next(&ccg_face_iter))
		{
			/*const*/ CCGFace *ccg_face = ccgFaceIterator_getCurrent(&ccg_face_iter);
			const int poly_index = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(ccg_face));
			const MPoly *mp = &mpoly[poly_index];
			int corner;
			if (ccg_face->numVerts != mp->totloop) {
				return false;
			}
			for (corner = 0; corner < ccg_face->numVerts; corner++) {
				/*const*/ CCGVert *ccg_vert = FACE_getVerts(ccg_face)[corner];
				const int vert_index = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ccg_vert));
				if (vert_index != mloop[mp->loopstart + corner].v) {
					return false;
				}
			}
		}
	}

	/* Check for edge topology change. */
	{
		CCGEdgeIterator ccg_edge_iter;
		for (ccgSubSurf_initEdgeIterator(ss, &ccg_edge_iter);
		     !ccgEdgeIterator_isStopped(&ccg_edge_iter);
		     ccgEdgeIterator_next(&ccg_edge_iter))
		{
			/* const */ CCGEdge *ccg_edge = ccgEdgeIterator_getCurrent(&ccg_edge_iter);
			/* const */ CCGVert *ccg_vert1 = ccg_edge->v0;
			/* const */ CCGVert *ccg_vert2 = ccg_edge->v1;
			const int ccg_vert1_index = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ccg_vert1));
			const int ccg_vert2_index = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ccg_vert2));
			const int edge_index = GET_INT_FROM_POINTER(ccgSubSurf_getEdgeEdgeHandle(ccg_edge));
			const MEdge *me = &medge[edge_index];
			if (me->v1 != ccg_vert1_index || me->v2 != ccg_vert2_index) {
				return false;
			}
		}
	}

	/* TODO(sergey): Crease topology changes detection. */
	{
		CCGEdgeIterator ccg_edge_iter;
		for (ccgSubSurf_initEdgeIterator(ss, &ccg_edge_iter);
		     !ccgEdgeIterator_isStopped(&ccg_edge_iter);
		     ccgEdgeIterator_next(&ccg_edge_iter))
		{
			/* const */ CCGEdge *ccg_edge = ccgEdgeIterator_getCurrent(&ccg_edge_iter);
			const int edge_index = GET_INT_FROM_POINTER(ccgSubSurf_getEdgeEdgeHandle(ccg_edge));
			if (ccg_edge->crease != medge[edge_index].crease) {
				return false;
			}
		}
	}

	return true;
}

static bool compare_osd_derivedmesh_topology(CCGSubSurf *ss, DerivedMesh *dm)
{
	OpenSubdiv_Converter converter;
	bool result;
	if (ss->osd_mesh == NULL && ss->osd_topology_refiner == NULL) {
		return true;
	}
	/* TODO(sergey): De-duplicate with topology counter at the bottom of
	 * the file.
	 */
	ccgSubSurf_converter_setup_from_derivedmesh(ss, dm, &converter);
	result = openSubdiv_topologyRefinerCompareWithConverter(
	        ss->osd_topology_refiner,
	        &converter);
	ccgSubSurf_converter_free(&converter);
	return result;
}

static bool opensubdiv_is_topology_changed(CCGSubSurf *ss, DerivedMesh *dm)
{
	if (ss->osd_compute != U.opensubdiv_compute_type) {
		return true;
	}
	if (ss->osd_topology_refiner != NULL) {
		const int levels = ss->osd_topology_refiner->getSubdivisionLevel(
		        ss->osd_topology_refiner);
		BLI_assert(ss->osd_mesh_invalid == true);
		if (levels != ss->subdivLevels) {
			return true;
		}
	}
	if (ss->skip_grids == false) {
		return compare_ccg_derivedmesh_topology(ss, dm) == false;
	}
	else {
		return compare_osd_derivedmesh_topology(ss, dm) == false;
	}
	return false;
}

void ccgSubSurf_checkTopologyChanged(CCGSubSurf *ss, DerivedMesh *dm)
{
	if (opensubdiv_is_topology_changed(ss, dm)) {
		/* ** Make sure both GPU and CPU backends are properly reset. ** */

		ss->osd_coarse_coords_invalid = true;

		/* Reset GPU part. */
		ss->osd_mesh_invalid = true;
		if (ss->osd_topology_refiner != NULL) {
			openSubdiv_deleteTopologyRefiner(ss->osd_topology_refiner);
			ss->osd_topology_refiner = NULL;
		}

		/* Reset CPU side. */
		if (ss->osd_evaluator != NULL) {
			openSubdiv_deleteEvaluator(ss->osd_evaluator);
			ss->osd_evaluator = NULL;
		}
	}
}

static void ccgSubSurf__updateGLMeshCoords(CCGSubSurf *ss)
{
	BLI_assert(ss->meshIFC.numLayers == 3);
	ss->osd_mesh->setCoarsePositions(ss->osd_mesh,
	                                 (float *) ss->osd_coarse_coords,
	                                 0,
	                                 ss->osd_num_coarse_coords);
}

bool ccgSubSurf_prepareGLMesh(CCGSubSurf *ss,
                              bool use_osd_glsl,
                              int active_uv_index)
{
	int compute_type;

	switch (U.opensubdiv_compute_type) {
#define CHECK_COMPUTE_TYPE(type) \
		case USER_OPENSUBDIV_COMPUTE_ ## type: \
			compute_type = OPENSUBDIV_EVALUATOR_ ## type; \
			break;
		CHECK_COMPUTE_TYPE(CPU)
		CHECK_COMPUTE_TYPE(OPENMP)
		CHECK_COMPUTE_TYPE(OPENCL)
		CHECK_COMPUTE_TYPE(CUDA)
		CHECK_COMPUTE_TYPE(GLSL_TRANSFORM_FEEDBACK)
		CHECK_COMPUTE_TYPE(GLSL_COMPUTE)
		default:
			compute_type = OPENSUBDIV_EVALUATOR_CPU;
			break;
#undef CHECK_COMPUTE_TYPE
	}

	if (ss->osd_vao == 0) {
		glGenVertexArrays(1, &ss->osd_vao);
	}

	if (ss->osd_mesh_invalid) {
		if (ss->osd_mesh != NULL) {
			ccgSubSurf__delete_osdGLMesh(ss->osd_mesh);
			ss->osd_mesh = NULL;
		}
		ss->osd_mesh_invalid = false;
	}

	if (ss->osd_mesh == NULL) {
		if (ss->osd_topology_refiner == NULL) {
			/* Happens with empty meshes. */
			/* TODO(sergey): Add assert that mesh is indeed empty. */
			return false;
		}

		ss->osd_mesh = openSubdiv_createOsdGLMeshFromTopologyRefiner(
		        ss->osd_topology_refiner,
		        compute_type);

		if (UNLIKELY(ss->osd_mesh == NULL)) {
			/* Most likely compute device is not available. */
			return false;
		}

		ccgSubSurf__updateGLMeshCoords(ss);
		ss->osd_mesh->refine(ss->osd_mesh);
		ss->osd_mesh->synchronize(ss->osd_mesh);
		ss->osd_coarse_coords_invalid = false;

		glBindVertexArray(ss->osd_vao);
		ss->osd_mesh->bindVertexBuffer(ss->osd_mesh);

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
		                      sizeof(GLfloat) * 6, 0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
		                      sizeof(GLfloat) * 6, (float *)12);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	else if (ss->osd_coarse_coords_invalid) {
		ccgSubSurf__updateGLMeshCoords(ss);
		ss->osd_mesh->refine(ss->osd_mesh);
		ss->osd_mesh->synchronize(ss->osd_mesh);
		ss->osd_coarse_coords_invalid = false;
	}

	ss->osd_mesh->prepareDraw(ss->osd_mesh, use_osd_glsl, active_uv_index);

	return true;
}

void ccgSubSurf_drawGLMesh(CCGSubSurf *ss, bool fill_quads,
                           int start_partition, int num_partitions)
{
	if (LIKELY(ss->osd_mesh != NULL)) {
		glBindVertexArray(ss->osd_vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
		             ss->osd_mesh->getPatchIndexBuffer(ss->osd_mesh));

		ss->osd_mesh->bindVertexBuffer(ss->osd_mesh);
		glBindVertexArray(ss->osd_vao);
		ss->osd_mesh->drawPatches(ss->osd_mesh, fill_quads,
		                          start_partition, num_partitions);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
}

int ccgSubSurf_getNumGLMeshBaseFaces(CCGSubSurf *ss)
{
	if (ss->osd_topology_refiner != NULL) {
		return ss->osd_topology_refiner->getNumFaces(
		        ss->osd_topology_refiner);
	}
	return 0;
}

/* Get number of vertices in base faces in a particular GL mesh. */
int ccgSubSurf_getNumGLMeshBaseFaceVerts(CCGSubSurf *ss, int face)
{
	if (ss->osd_topology_refiner != NULL) {
		return ss->osd_topology_refiner->getNumFaceVertices(
		        ss->osd_topology_refiner, face);
	}
	return 0;
}

void ccgSubSurf_setSkipGrids(CCGSubSurf *ss, bool skip_grids)
{
	ss->skip_grids = skip_grids;
}

bool ccgSubSurf_needGrids(CCGSubSurf *ss)
{
	return ss->skip_grids == false;
}

BLI_INLINE void ccgSubSurf__mapGridToFace(int S, float grid_u, float grid_v,
                                          float *face_u, float *face_v)
{
	float u, v;

	/* - Each grid covers half of the face along the edges.
	 * - Grid's (0, 0) starts from the middle of the face.
	 */
	u = 0.5f - 0.5f * grid_u;
	v = 0.5f - 0.5f * grid_v;

	if (S == 0) {
		*face_u = v;
		*face_v = u;
	}
	else if (S == 1) {
		*face_u = 1.0f - u;
		*face_v = v;
	}
	else if (S == 2) {
		*face_u = 1.0f - v;
		*face_v = 1.0f - u;
	}
	else {
		*face_u = u;
		*face_v = 1.0f - v;
	}
}

BLI_INLINE void ccgSubSurf__mapEdgeToFace(int S,
                                          int edge_segment,
                                          bool inverse_edge,
                                          int edgeSize,
                                          float *face_u, float *face_v)
{
	int t = inverse_edge ? edgeSize - edge_segment - 1 : edge_segment;
	if (S == 0) {
		*face_u = (float) t / (edgeSize - 1);
		*face_v = 0.0f;
	}
	else if (S == 1) {
		*face_u = 1.0f;
		*face_v = (float) t / (edgeSize - 1);
	}
	else if (S == 2) {
		*face_u = 1.0f - (float) t / (edgeSize - 1);
		*face_v = 1.0f;
	}
	else {
		*face_u = 0.0f;
		*face_v = 1.0f - (float) t / (edgeSize - 1);
	}
}

void ccgSubSurf_evaluatorSetFVarUV(CCGSubSurf *ss,
                                   DerivedMesh *dm,
                                   int layer_index)
{
	MPoly *mpoly = dm->getPolyArray(dm);
	MLoopUV *mloopuv = CustomData_get_layer_n(&dm->loopData, CD_MLOOPUV, layer_index);
	int num_polys = dm->getNumPolys(dm);
	int index, poly;
	BLI_assert(ss->osd_evaluator != NULL);
	for (poly = 0, index = 0; poly < num_polys; poly++) {
		int loop;
		MPoly *mp = &mpoly[poly];
		for (loop = 0; loop < mp->totloop; loop++, index++) {
			MLoopUV *mluv = &mloopuv[loop + mp->loopstart];
			(void)mluv;
			/* TODO(sergey): Send mluv->uv to the evaluator's face varying
			 * buffer.
			 */
		}
	}
	(void)ss;
}

void ccgSubSurf_evaluatorFVarUV(CCGSubSurf *ss,
                                int face_index, int S,
                                float grid_u, float grid_v,
                                float uv[2])
{
	float face_u, face_v;
	ccgSubSurf__mapGridToFace(S,
	                          grid_u, grid_v,
	                          &face_u, &face_v);
	(void)ss;
	(void)face_index;
	/* TODO(sergey): Evaluate face varying coordinate. */
	zero_v2(uv);
}

static bool opensubdiv_createEvaluator(CCGSubSurf *ss)
{
	OpenSubdiv_Converter converter;
	OpenSubdiv_TopologyRefiner *topology_refiner;
	if (ss->fMap->numEntries == 0) {
		/* OpenSubdiv doesn't support meshes without faces. */
		return false;
	}
	ccgSubSurf_converter_setup_from_ccg(ss, &converter);
	OpenSubdiv_TopologyRefinerSettings settings;
	settings.level = ss->subdivLevels;
	settings.is_adaptive = false;
	topology_refiner =
	        openSubdiv_createTopologyRefinerFromConverter(
	                &converter, &settings);
	ccgSubSurf_converter_free(&converter);
	ss->osd_evaluator =
	        openSubdiv_createEvaluatorFromTopologyRefiner(topology_refiner);
	if (ss->osd_evaluator == NULL) {
		BLI_assert(!"OpenSubdiv initialization failed, should not happen.");
		return false;
	}
	return true;
}

static bool opensubdiv_ensureEvaluator(CCGSubSurf *ss)
{
	if (ss->osd_evaluator == NULL) {
		OSD_LOG("Allocating new evaluator, %d verts\n", ss->vMap->numEntries);
		opensubdiv_createEvaluator(ss);
	}
	return ss->osd_evaluator != NULL;
}

static void opensubdiv_updateEvaluatorCoarsePositions(CCGSubSurf *ss)
{
	float (*positions)[3];
	int vertDataSize = ss->meshIFC.vertDataSize;
	int num_basis_verts = ss->vMap->numEntries;
	int i;

	/* TODO(sergey): Avoid allocation on every update. We could either update
	 * coordinates in chunks of 1K vertices (which will only use stack memory)
	 * or do some callback magic for OSD evaluator can invoke it and fill in
	 * buffer directly.
	 */
	if (ss->meshIFC.numLayers == 3) {
		/* If all the components are to be initialized, no need to memset the
		 * new memory block.
		 */
		positions = MEM_mallocN(3 * sizeof(float) * num_basis_verts,
		                        "OpenSubdiv coarse points");
	}
	else {
		/* Calloc in order to have z component initialized to 0 for Uvs */
		positions = MEM_callocN(3 * sizeof(float) * num_basis_verts,
		                        "OpenSubdiv coarse points");
	}
#pragma omp parallel for
	for (i = 0; i < ss->vMap->curSize; i++) {
		CCGVert *v = (CCGVert *) ss->vMap->buckets[i];
		for (; v; v = v->next) {
			float *co = VERT_getCo(v, 0);
			BLI_assert(v->osd_index < ss->vMap->numEntries);
			VertDataCopy(positions[v->osd_index], co, ss);
			OSD_LOG("Point %d has value %f %f %f\n",
			        v->osd_index,
			        positions[v->osd_index][0],
			        positions[v->osd_index][1],
			        positions[v->osd_index][2]);
		}
	}

	ss->osd_evaluator->setCoarsePositions(ss->osd_evaluator,
	                                      (float *)positions,
	                                      0,
	                                      num_basis_verts);
	ss->osd_evaluator->refine(ss->osd_evaluator);

	MEM_freeN(positions);
}

static void opensubdiv_evaluateQuadFaceGrids(CCGSubSurf *ss,
                                             CCGFace *face,
                                             const int osd_face_index)
{
	int normalDataOffset = ss->normalDataOffset;
	int subdivLevels = ss->subdivLevels;
	int gridSize = ccg_gridsize(subdivLevels);
	int edgeSize = ccg_edgesize(subdivLevels);
	int vertDataSize = ss->meshIFC.vertDataSize;
	int S;
	bool do_normals = ss->meshIFC.numLayers == 3;

#pragma omp parallel for
	for (S = 0; S < face->numVerts; S++) {
		int x, y, k;
		CCGEdge *edge = NULL;
		bool inverse_edge = false;

		for (x = 0; x < gridSize; x++) {
			for (y = 0; y < gridSize; y++) {
				float *co = FACE_getIFCo(face, subdivLevels, S, x, y);
				float *no = FACE_getIFNo(face, subdivLevels, S, x, y);
				float grid_u = (float) x / (gridSize - 1),
				      grid_v = (float) y / (gridSize - 1);
				float face_u, face_v;
				float P[3], dPdu[3], dPdv[3];

				ccgSubSurf__mapGridToFace(S, grid_u, grid_v, &face_u, &face_v);

				/* TODO(sergey): Need proper port. */
				ss->osd_evaluator->evaluateLimit(
				        ss->osd_evaluator, osd_face_index,
				        face_u, face_v,
				        P,
				        do_normals ? dPdu : NULL,
				        do_normals ? dPdv : NULL);

				OSD_LOG("face=%d, corner=%d, grid_u=%f, grid_v=%f, face_u=%f, face_v=%f, P=(%f, %f, %f)\n",
				        osd_face_index, S, grid_u, grid_v, face_u, face_v, P[0], P[1], P[2]);

				VertDataCopy(co, P, ss);
				if (do_normals) {
					cross_v3_v3v3(no, dPdu, dPdv);
					normalize_v3(no);
				}

				if (x == gridSize - 1 && y == gridSize - 1) {
					float *vert_co = VERT_getCo(FACE_getVerts(face)[S], subdivLevels);
					VertDataCopy(vert_co, co, ss);
					if (do_normals) {
						float *vert_no = VERT_getNo(FACE_getVerts(face)[S], subdivLevels);
						VertDataCopy(vert_no, no, ss);
					}
				}
				if (S == 0 && x == 0 && y == 0) {
					float *center_co = (float *)FACE_getCenterData(face);
					VertDataCopy(center_co, co, ss);
					if (do_normals) {
						float *center_no = (float *)((byte *)FACE_getCenterData(face) + normalDataOffset);
						VertDataCopy(center_no, no, ss);
					}
				}
			}
		}

		for (x = 0; x < gridSize; x++) {
			VertDataCopy(FACE_getIECo(face, subdivLevels, S, x),
			             FACE_getIFCo(face, subdivLevels, S, x, 0), ss);
			if (do_normals) {
				VertDataCopy(FACE_getIENo(face, subdivLevels, S, x),
				             FACE_getIFNo(face, subdivLevels, S, x, 0), ss);
			}
		}

		for (k = 0; k < face->numVerts; k++) {
			CCGEdge *current_edge = FACE_getEdges(face)[k];
			CCGVert **face_verts = FACE_getVerts(face);
			if (current_edge->v0 == face_verts[S] &&
			    current_edge->v1 == face_verts[(S + 1) % face->numVerts])
			{
				edge = current_edge;
				inverse_edge = false;
				break;
			}
			if (current_edge->v1 == face_verts[S] &&
			    current_edge->v0 == face_verts[(S + 1) % face->numVerts])
			{
				edge = current_edge;
				inverse_edge = true;
				break;
			}
		}

		BLI_assert(edge != NULL);

		for (x = 0; x < edgeSize; x++) {
			float u = 0, v = 0;
			float *co = EDGE_getCo(edge, subdivLevels, x);
			float *no = EDGE_getNo(edge, subdivLevels, x);
			float P[3], dPdu[3], dPdv[3];
			ccgSubSurf__mapEdgeToFace(S, x,
			                          inverse_edge,
			                          edgeSize,
			                          &u, &v);

			/* TODO(sergey): Ideally we will re-use grid here, but for now
			 * let's just re-evaluate for simplicity.
			 */
			/* TODO(sergey): Need proper port. */
			ss->osd_evaluator->evaluateLimit(
			        ss->osd_evaluator,
			        osd_face_index,
			        u, v,
			        P, dPdu, dPdv);
			VertDataCopy(co, P, ss);
			if (do_normals) {
				cross_v3_v3v3(no, dPdu, dPdv);
				normalize_v3(no);
			}
		}
	}
}

static void opensubdiv_evaluateNGonFaceGrids(CCGSubSurf *ss,
                                             CCGFace *face,
                                             const int osd_face_index)
{
	CCGVert **all_verts = FACE_getVerts(face);
	int normalDataOffset = ss->normalDataOffset;
	int subdivLevels = ss->subdivLevels;
	int gridSize = ccg_gridsize(subdivLevels);
	int edgeSize = ccg_edgesize(subdivLevels);
	int vertDataSize = ss->meshIFC.vertDataSize;
	int S;
	bool do_normals = ss->meshIFC.numLayers == 3;

	/* Note about handling non-quad faces.
	 *
	 * In order to deal with non-quad faces we need to split them
	 * into a quads in the following way:
	 *
	 *                                                     |
	 *                                                (vert_next)
	 *                                                     |
	 *                                                     |
	 *                                                     |
	 *                  (face_center) ------------------- (v2)
	 *                         | (o)-------------------->  |
	 *                         |  |                     v  |
	 *                         |  |                        |
	 *                         |  |                        |
	 *                         |  |                        |
	 *                         |  |                   y ^  |
	 *                         |  |                     |  |
	 *                         |  v  u             x    |  |
	 *                         |                   <---(o) |
	 * ---- (vert_prev) ---- (v1)  --------------------  (vert)
	 *
	 * This is how grids are expected to be stored and it's how
	 * OpenSubdiv deals with non-quad faces using ptex face indices.
	 * We only need to convert ptex (x, y) to grid (u, v) by some
	 * simple flips and evaluate the ptex face.
	 */

	/* Evaluate face grids. */
#pragma omp parallel for
	for (S = 0; S < face->numVerts; S++) {
		int x, y;
		for (x = 0; x < gridSize; x++) {
			for (y = 0; y < gridSize; y++) {
				float *co = FACE_getIFCo(face, subdivLevels, S, x, y);
				float *no = FACE_getIFNo(face, subdivLevels, S, x, y);
				float u = 1.0f - (float) y / (gridSize - 1),
				      v = 1.0f - (float) x / (gridSize - 1);
				float P[3], dPdu[3], dPdv[3];

				/* TODO(sergey): Need proper port. */
				ss->osd_evaluator->evaluateLimit(
				        ss->osd_evaluator,
				        osd_face_index + S,
				        u, v,
				        P, dPdu, dPdv);

				OSD_LOG("face=%d, corner=%d, u=%f, v=%f, P=(%f, %f, %f)\n",
				        osd_face_index + S, S, u, v, P[0], P[1], P[2]);

				VertDataCopy(co, P, ss);
				if (do_normals) {
					cross_v3_v3v3(no, dPdu, dPdv);
					normalize_v3(no);
				}

				/* TODO(sergey): De-dpuplicate with the quad case. */
				if (x == gridSize - 1 && y == gridSize - 1) {
					float *vert_co = VERT_getCo(FACE_getVerts(face)[S], subdivLevels);
					VertDataCopy(vert_co, co, ss);
					if (do_normals) {
						float *vert_no = VERT_getNo(FACE_getVerts(face)[S], subdivLevels);
						VertDataCopy(vert_no, no, ss);
					}
				}
				if (S == 0 && x == 0 && y == 0) {
					float *center_co = (float *)FACE_getCenterData(face);
					VertDataCopy(center_co, co, ss);
					if (do_normals) {
						float *center_no = (float *)((byte *)FACE_getCenterData(face) + normalDataOffset);
						VertDataCopy(center_no, no, ss);
					}
				}
			}
		}
		for (x = 0; x < gridSize; x++) {
			VertDataCopy(FACE_getIECo(face, subdivLevels, S, x),
			             FACE_getIFCo(face, subdivLevels, S, x, 0), ss);
			if (do_normals) {
				VertDataCopy(FACE_getIENo(face, subdivLevels, S, x),
				             FACE_getIFNo(face, subdivLevels, S, x, 0), ss);
			}
		}
	}

	/* Evaluate edges. */
	for (S = 0; S < face->numVerts; S++) {
		CCGEdge *edge = FACE_getEdges(face)[S];
		int x, S0 = 0, S1 = 0;
		bool flip;

		for (x = 0; x < face->numVerts; ++x) {
			if (all_verts[x] == edge->v0) {
				S0 = x;
			}
			else if (all_verts[x] == edge->v1) {
				S1 = x;
			}
		}
		if (S == face->numVerts - 1) {
			flip = S0 > S1;
		}
		else {
			flip = S0 < S1;
		}

		for (x = 0; x <= edgeSize / 2; x++) {
			float *edge_co = EDGE_getCo(edge, subdivLevels, x);
			float *edge_no = EDGE_getNo(edge, subdivLevels, x);
			float *face_edge_co;
			float *face_edge_no;
			if (flip) {
				face_edge_co = FACE_getIFCo(face, subdivLevels, S0, gridSize - 1, gridSize - 1 - x);
				face_edge_no = FACE_getIFNo(face, subdivLevels, S0, gridSize - 1, gridSize - 1 - x);
			}
			else {
				face_edge_co = FACE_getIFCo(face, subdivLevels, S0, gridSize - 1 - x, gridSize - 1);
				face_edge_no = FACE_getIFNo(face, subdivLevels, S0, gridSize - 1 - x, gridSize - 1);
			}
			VertDataCopy(edge_co, face_edge_co, ss);
			if (do_normals) {
				VertDataCopy(edge_no, face_edge_no, ss);
			}
		}
		for (x = edgeSize / 2 + 1; x < edgeSize; x++) {
			float *edge_co = EDGE_getCo(edge, subdivLevels, x);
			float *edge_no = EDGE_getNo(edge, subdivLevels, x);
			float *face_edge_co;
			float *face_edge_no;
			if (flip) {
				face_edge_co = FACE_getIFCo(face, subdivLevels, S1, x - edgeSize / 2, gridSize - 1);
				face_edge_no = FACE_getIFNo(face, subdivLevels, S1, x - edgeSize / 2, gridSize - 1);
			}
			else {
				face_edge_co = FACE_getIFCo(face, subdivLevels, S1, gridSize - 1, x - edgeSize / 2);
				face_edge_no = FACE_getIFNo(face, subdivLevels, S1, gridSize - 1, x - edgeSize / 2);
			}
			VertDataCopy(edge_co, face_edge_co, ss);
			if (do_normals) {
				VertDataCopy(edge_no, face_edge_no, ss);
			}
		}
	}
}

static void opensubdiv_evaluateGrids(CCGSubSurf *ss)
{
	int i;
	for (i = 0; i < ss->fMap->curSize; i++) {
		CCGFace *face = (CCGFace *) ss->fMap->buckets[i];
		for (; face; face = face->next) {
			if (face->numVerts == 4) {
				/* For quads we do special magic with converting face coords
				 * into corner coords and interpolating grids from it.
				 */
				opensubdiv_evaluateQuadFaceGrids(ss, face, face->osd_index);
			}
			else {
				/* NGons and tris are split into separate osd faces which
				 * evaluates onto grids directly.
				 */
				opensubdiv_evaluateNGonFaceGrids(ss, face, face->osd_index);
			}
		}
	}
}

CCGError ccgSubSurf_initOpenSubdivSync(CCGSubSurf *ss)
{
	if (ss->syncState != eSyncState_None) {
		return eCCGError_InvalidSyncState;
	}
	ss->syncState = eSyncState_OpenSubdiv;
	return eCCGError_None;
}

void ccgSubSurf_prepareTopologyRefiner(CCGSubSurf *ss, DerivedMesh *dm)
{
	if (ss->osd_mesh == NULL || ss->osd_mesh_invalid) {
		if (dm->getNumPolys(dm) != 0) {
			OpenSubdiv_Converter converter;
			ccgSubSurf_converter_setup_from_derivedmesh(ss, dm, &converter);
			/* TODO(sergey): Remove possibly previously allocated refiner. */
			OpenSubdiv_TopologyRefinerSettings settings;
			settings.level = ss->subdivLevels;
			settings.is_adaptive = false;
			ss->osd_topology_refiner =
			        openSubdiv_createTopologyRefinerFromConverter(
			                &converter, &settings);
			ccgSubSurf_converter_free(&converter);
		}
	}

	/* Update number of grids, needed for things like final faces
	 * counter, used by display drawing.
	 */
	{
		const int num_polys = dm->getNumPolys(dm);
		const MPoly *mpoly = dm->getPolyArray(dm);
		int poly;
		ss->numGrids = 0;
		for (poly = 0; poly < num_polys; ++poly) {
			ss->numGrids += mpoly[poly].totloop;
		}
	}

	{
		const int num_verts = dm->getNumVerts(dm);
		const MVert *mvert = dm->getVertArray(dm);
		int vert;
		if (ss->osd_coarse_coords != NULL &&
		    num_verts != ss->osd_num_coarse_coords)
		{
			MEM_freeN(ss->osd_coarse_coords);
			ss->osd_coarse_coords = NULL;
		}
		if (ss->osd_coarse_coords == NULL) {
			ss->osd_coarse_coords = MEM_mallocN(sizeof(float) * 6 * num_verts, "osd coarse positions");
		}
		for (vert = 0; vert < num_verts; vert++) {
			copy_v3_v3(ss->osd_coarse_coords[vert * 2 + 0], mvert[vert].co);
			normal_short_to_float_v3(ss->osd_coarse_coords[vert * 2 + 1], mvert[vert].no);
		}
		ss->osd_num_coarse_coords = num_verts;
		ss->osd_coarse_coords_invalid = true;
	}
}

void ccgSubSurf__sync_opensubdiv(CCGSubSurf *ss)
{
	BLI_assert(ss->meshIFC.numLayers == 2 || ss->meshIFC.numLayers == 3);

	/* Common synchronization steps */
	ss->osd_compute = U.opensubdiv_compute_type;

	if (ss->skip_grids == false) {
		/* Make sure OSD evaluator is up-to-date. */
		if (opensubdiv_ensureEvaluator(ss)) {
			/* Update coarse points in the OpenSubdiv evaluator. */
			opensubdiv_updateEvaluatorCoarsePositions(ss);

			/* Evaluate opensubdiv mesh into the CCG grids. */
			opensubdiv_evaluateGrids(ss);
		}
	}
	else {
		BLI_assert(ss->meshIFC.numLayers == 3);
	}

#ifdef DUMP_RESULT_GRIDS
	ccgSubSurf__dumpCoords(ss);
#endif
}

void ccgSubSurf_free_osd_mesh(CCGSubSurf *ss)
{
	if (ss->osd_mesh != NULL) {
		ccgSubSurf__delete_osdGLMesh(ss->osd_mesh);
		ss->osd_mesh = NULL;
	}
	if (ss->osd_vao != 0) {
		glDeleteVertexArrays(1, &ss->osd_vao);
		ss->osd_vao = 0;
	}
}

void ccgSubSurf_getMinMax(CCGSubSurf *ss, float r_min[3], float r_max[3])
{
	int i;
	BLI_assert(ss->skip_grids == true);
	if (ss->osd_num_coarse_coords == 0) {
		zero_v3(r_min);
		zero_v3(r_max);
	}
	for (i = 0; i < ss->osd_num_coarse_coords; i++) {
		/* Coarse coordinates has normals interleaved into the array. */
		DO_MINMAX(ss->osd_coarse_coords[2 * i], r_min, r_max);
	}
}

/* ** Delayed delete routines ** */

typedef struct OsdDeletePendingItem {
	struct OsdDeletePendingItem *next, *prev;
	OpenSubdiv_GLMesh *osd_mesh;
	unsigned int vao;
} OsdDeletePendingItem;

static SpinLock delete_spin;
static ListBase delete_pool = {NULL, NULL};

static void delete_pending_push(OpenSubdiv_GLMesh *osd_mesh,
                                unsigned int vao)
{
	OsdDeletePendingItem *new_entry = MEM_mallocN(sizeof(OsdDeletePendingItem),
	                                              "opensubdiv delete entry");
	new_entry->osd_mesh = osd_mesh;
	new_entry->vao = vao;
	BLI_spin_lock(&delete_spin);
	BLI_addtail(&delete_pool, new_entry);
	BLI_spin_unlock(&delete_spin);
}

void ccgSubSurf__delete_osdGLMesh(OpenSubdiv_GLMesh *osd_mesh)
{
	if (BLI_thread_is_main()) {
		openSubdiv_deleteOsdGLMesh(osd_mesh);
	}
	else {
		delete_pending_push(osd_mesh, 0);
	}
}

void ccgSubSurf__delete_vertex_array(unsigned int vao)
{
	if (BLI_thread_is_main()) {
		glDeleteVertexArrays(1, &vao);
	}
	else {
		delete_pending_push(NULL, vao);
	}
}

void ccgSubSurf__delete_pending(void)
{
	OsdDeletePendingItem *entry;
	BLI_assert(BLI_thread_is_main());
	BLI_spin_lock(&delete_spin);
	for (entry = delete_pool.first; entry != NULL; entry = entry->next) {
		if (entry->osd_mesh != NULL) {
			openSubdiv_deleteOsdGLMesh(entry->osd_mesh);
		}
		if (entry->vao != 0) {
			glDeleteVertexArrays(1, &entry->vao);
		}
	}
	BLI_freelistN(&delete_pool);
	BLI_spin_unlock(&delete_spin);
}

void ccgSubSurf__sync_subdivUvs(CCGSubSurf *ss, bool subdiv_uvs)
{
    ss->osd_subdiv_uvs = subdiv_uvs;
}

/* ** Public API ** */

void BKE_subsurf_osd_init(void)
{
	openSubdiv_init();
	BLI_spin_init(&delete_spin);
}

void BKE_subsurf_free_unused_buffers(void)
{
	ccgSubSurf__delete_pending();
}

void BKE_subsurf_osd_cleanup(void)
{
	openSubdiv_cleanup();
	ccgSubSurf__delete_pending();
	BLI_spin_end(&delete_spin);
}

#endif  /* WITH_OPENSUBDIV */
