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

/** \file blender/blenkernel/intern/CCGSubSurf_opensubdiv_converter.c
 *  \ingroup bke
 */

#ifdef WITH_OPENSUBDIV

#include <stdlib.h>

#include "MEM_guardedalloc.h"
#include "BLI_sys_types.h" // for intptr_t support

#include "BLI_utildefines.h" /* for BLI_assert */
#include "BLI_math.h"

#include "CCGSubSurf.h"
#include "CCGSubSurf_intern.h"

#include "BKE_DerivedMesh.h"
#include "BKE_mesh_mapping.h"

#include "opensubdiv_capi.h"
#include "opensubdiv_converter_capi.h"

/* Use mesh element mapping structures during conversion.
 * Uses more memory but is much faster than naive algorithm.
 */
#define USE_MESH_ELEMENT_MAPPING

/**
 * Converter from DerivedMesh.
 */

typedef struct ConvDMStorage {
	CCGSubSurf *ss;
	DerivedMesh *dm;

#ifdef USE_MESH_ELEMENT_MAPPING
	MeshElemMap *vert_edge_map,
	            *vert_poly_map,
	            *edge_poly_map;
	int *vert_edge_mem,
	    *vert_poly_mem,
	    *edge_poly_mem;
#endif

	MVert *mvert;
	MEdge *medge;
	MLoop *mloop;
	MPoly *mpoly;

	MeshIslandStore island_store;
	int num_uvs;
	float *uvs;
	int *face_uvs;
} ConvDMStorage;

static OpenSubdiv_SchemeType conv_dm_get_type(
        const OpenSubdiv_Converter *converter)
{
	ConvDMStorage *storage = converter->user_data;
	if (storage->ss->meshIFC.simpleSubdiv)
		return OSD_SCHEME_BILINEAR;
	else
		return OSD_SCHEME_CATMARK;
}

static OpenSubdiv_FVarLinearInterpolation conv_dm_get_fvar_linear_interpolation(
        const OpenSubdiv_Converter *converter)
{
	ConvDMStorage *storage = converter->user_data;
	if (storage->ss->osd_subdiv_uvs) {
		return OSD_FVAR_LINEAR_INTERPOLATION_CORNERS_ONLY;
	}
	return OSD_FVAR_LINEAR_INTERPOLATION_ALL;
}

static int conv_dm_get_num_faces(const OpenSubdiv_Converter *converter)
{
	ConvDMStorage *storage = converter->user_data;
	DerivedMesh *dm = storage->dm;
	return dm->getNumPolys(dm);
}

static int conv_dm_get_num_edges(const OpenSubdiv_Converter *converter)
{
	ConvDMStorage *storage = converter->user_data;
	DerivedMesh *dm = storage->dm;
	return dm->getNumEdges(dm);
}

static int conv_dm_get_num_verts(const OpenSubdiv_Converter *converter)
{
	ConvDMStorage *storage = converter->user_data;
	DerivedMesh *dm = storage->dm;
	return dm->getNumVerts(dm);
}

static int conv_dm_get_num_face_verts(const OpenSubdiv_Converter *converter,
                                      int face)
{
	ConvDMStorage *storage = converter->user_data;
	const MPoly *mpoly = &storage->mpoly[face];
	return mpoly->totloop;
}

static void conv_dm_get_face_verts(const OpenSubdiv_Converter *converter,
                                   int face,
                                   int *face_verts)
{
	ConvDMStorage *storage = converter->user_data;
	const MPoly *mpoly = &storage->mpoly[face];
	int loop;
	for (loop = 0; loop < mpoly->totloop; loop++) {
		face_verts[loop] = storage->mloop[mpoly->loopstart + loop].v;
	}
}

static void conv_dm_get_face_edges(const OpenSubdiv_Converter *converter,
                                   int face,
                                   int *face_edges)
{
	ConvDMStorage *storage = converter->user_data;
	const MPoly *mpoly = &storage->mpoly[face];
	int loop;
	for (loop = 0; loop < mpoly->totloop; loop++) {
		face_edges[loop] = storage->mloop[mpoly->loopstart + loop].e;
	}
}

static void conv_dm_get_edge_verts(const OpenSubdiv_Converter *converter,
                                   int edge,
                                   int *edge_verts)
{
	ConvDMStorage *storage = converter->user_data;
	const MEdge *medge = &storage->medge[edge];
	edge_verts[0] = medge->v1;
	edge_verts[1] = medge->v2;
}

static int conv_dm_get_num_edge_faces(const OpenSubdiv_Converter *converter,
                                      int edge)
{
	ConvDMStorage *storage = converter->user_data;
#ifndef USE_MESH_ELEMENT_MAPPING
	DerivedMesh *dm = storage->dm;
	int num = 0, poly;
	for (poly = 0; poly < dm->getNumPolys(dm); poly++) {
		const MPoly *mpoly = &user_data->mpoly[poly];
		int loop;
		for (loop = 0; loop < mpoly->totloop; loop++) {
			const MLoop *mloop = &storage->mloop[mpoly->loopstart + loop];
			if (mloop->e == edge) {
				++num;
				break;
			}
		}
	}
	return num;
#else
	return storage->edge_poly_map[edge].count;
#endif
}

static void conv_dm_get_edge_faces(const OpenSubdiv_Converter *converter,
                                   int edge,
                                   int *edge_faces)
{
	ConvDMStorage *storage = converter->user_data;
#ifndef USE_MESH_ELEMENT_MAPPING
	DerivedMesh *dm = storage->dm;
	int num = 0, poly;
	for (poly = 0; poly < dm->getNumPolys(dm); poly++) {
		const MPoly *mpoly = &user_data->mpoly[poly];
		int loop;
		for (loop = 0; loop < mpoly->totloop; loop++) {
			const MLoop *mloop = &storage->mloop[mpoly->loopstart + loop];
			if (mloop->e == edge) {
				edge_faces[num++] = poly;
				break;
			}
		}
	}
#else
	memcpy(edge_faces,
	       storage->edge_poly_map[edge].indices,
	       sizeof(int) * storage->edge_poly_map[edge].count);
#endif
}

static float conv_dm_get_edge_sharpness(const OpenSubdiv_Converter *converter,
                                        int edge)
{
	ConvDMStorage *storage = converter->user_data;
	CCGSubSurf *ss = storage->ss;
	const MEdge *medge = storage->medge;
	return (float)medge[edge].crease / 255.0f * ss->subdivLevels;
}

static int conv_dm_get_num_vert_edges(const OpenSubdiv_Converter *converter,
                                      int vert)
{
	ConvDMStorage *storage = converter->user_data;
#ifndef USE_MESH_ELEMENT_MAPPING
	DerivedMesh *dm = storage->dm;
	int num = 0, edge;
	for (edge = 0; edge < dm->getNumEdges(dm); edge++) {
		const MEdge *medge = &user_data->medge[edge];
		if (medge->v1 == vert || medge->v2 == vert) {
			++num;
		}
	}
	return num;
#else
	return storage->vert_edge_map[vert].count;
#endif
}

static void conv_dm_get_vert_edges(const OpenSubdiv_Converter *converter,
                                   int vert,
                                   int *vert_edges)
{
	ConvDMStorage *storage = converter->user_data;
#ifndef USE_MESH_ELEMENT_MAPPING
	DerivedMesh *dm = storage->dm;
	int num = 0, edge;
	for (edge = 0; edge < dm->getNumEdges(dm); edge++) {
		const MEdge *medge = &user_data->medge[edge];
		if (medge->v1 == vert || medge->v2 == vert) {
			vert_edges[num++] = edge;
		}
	}
#else
	memcpy(vert_edges,
	       storage->vert_edge_map[vert].indices,
	       sizeof(int) * storage->vert_edge_map[vert].count);
#endif
}

static int conv_dm_get_num_vert_faces(const OpenSubdiv_Converter *converter,
                                      int vert)
{
	ConvDMStorage *storage = converter->user_data;
#ifndef USE_MESH_ELEMENT_MAPPING
	DerivedMesh *dm = storage->dm;
	int num = 0, poly;
	for (poly = 0; poly < dm->getNumPolys(dm); poly++) {
		const MPoly *mpoly = &user_data->mpoly[poly];
		int loop;
		for (loop = 0; loop < mpoly->totloop; loop++) {
			const MLoop *mloop = &storage->mloop[mpoly->loopstart + loop];
			if (mloop->v == vert) {
				++num;
				break;
			}
		}
	}
	return num;
#else
	return storage->vert_poly_map[vert].count;
#endif
}

static void conv_dm_get_vert_faces(const OpenSubdiv_Converter *converter,
                                   int vert,
                                   int *vert_faces)
{
	ConvDMStorage *storage = converter->user_data;
#ifndef USE_MESH_ELEMENT_MAPPING
	DerivedMesh *dm = storage->dm;
	int num = 0, poly;
	for (poly = 0; poly < dm->getNumPolys(dm); poly++) {
		const MPoly *mpoly = &storage->mpoly[poly];
		int loop;
		for (loop = 0; loop < mpoly->totloop; loop++) {
			const MLoop *mloop = &storage->mloop[mpoly->loopstart + loop];
			if (mloop->v == vert) {
				vert_faces[num++] = poly;
				break;
			}
		}
	}
#else
	memcpy(vert_faces,
	       storage->vert_poly_map[vert].indices,
	       sizeof(int) * storage->vert_poly_map[vert].count);
#endif
}

static int conv_dm_get_num_uv_layers(const OpenSubdiv_Converter *converter)
{
	ConvDMStorage *storage = converter->user_data;
	DerivedMesh *dm = storage->dm;
	int num_uv_layers = CustomData_number_of_layers(&dm->loopData, CD_MLOOPUV);
	return num_uv_layers;
}

static void conv_dm_precalc_uv_layer(const OpenSubdiv_Converter *converter,
                                     int layer)
{
	ConvDMStorage *storage = converter->user_data;
	DerivedMesh *dm = storage->dm;

	const MLoopUV *mloopuv = CustomData_get_layer_n(&dm->loopData, CD_MLOOPUV, layer);
	const int num_loops = dm->getNumLoops(dm);

	/* Initialize memory required for the operations. */
	if (storage->uvs == NULL) {
		storage->uvs = MEM_mallocN(sizeof(float) * 2 * num_loops, "osd uvs");
	}
	if (storage->face_uvs == NULL) {
		storage->face_uvs = MEM_mallocN(sizeof(int) * num_loops, "osd face uvs");
	}

	/* Calculate islands connectivity of the UVs. */
	BKE_mesh_calc_islands_loop_poly_uvmap(
	        storage->mvert, dm->getNumVerts(dm),
	        storage->medge, dm->getNumEdges(dm),
	        storage->mpoly, dm->getNumPolys(dm),
	        storage->mloop, dm->getNumLoops(dm),
	        mloopuv,
	        &storage->island_store);

	/* Here we "weld" duplicated vertices from island to the same UV value.
	 * The idea here is that we need to pass individual islands to OpenSubdiv.
	 */
	storage->num_uvs = 0;
	for (int island = 0; island < storage->island_store.islands_num; ++island) {
		MeshElemMap *island_poly_map = storage->island_store.islands[island];
		for (int poly = 0; poly < island_poly_map->count; ++poly) {
			int poly_index = island_poly_map->indices[poly];
			/* Within the same UV island we should share UV points across
			 * loops. Otherwise each poly will be subdivided individually
			 * which we don't really want.
			 */
			const MPoly *mpoly = &storage->mpoly[poly_index];
			for (int loop = 0; loop < mpoly->totloop; ++loop) {
				const MLoopUV *luv = &mloopuv[mpoly->loopstart + loop];
				bool found = false;
				/* TODO(sergey): Quite bad loop, which gives us O(N^2)
				 * complexity here. But how can we do it smarter, hopefully
				 * without requiring lots of additional memory.
				 */
				for (int i = 0; i < storage->num_uvs; ++i) {
					if (equals_v2v2(luv->uv, &storage->uvs[2 * i])) {
						storage->face_uvs[mpoly->loopstart + loop] = i;
						found = true;
						break;
					}
				}
				if (!found) {
					copy_v2_v2(&storage->uvs[2 * storage->num_uvs], luv->uv);
					storage->face_uvs[mpoly->loopstart + loop] = storage->num_uvs;
					++storage->num_uvs;
				}
			}
		}
	}
}

static void conv_dm_finish_uv_layer(const OpenSubdiv_Converter *converter)
{
	ConvDMStorage *storage = converter->user_data;
	BKE_mesh_loop_islands_free(&storage->island_store);
}

static int conv_dm_get_num_uvs(const OpenSubdiv_Converter *converter)
{
	ConvDMStorage *storage = converter->user_data;
	return storage->num_uvs;
}

static int conv_dm_get_face_corner_uv_index(const OpenSubdiv_Converter *converter,
                                            int face,
                                            int corner)
{
	ConvDMStorage *storage = converter->user_data;
	const MPoly *mpoly = &storage->mpoly[face];
	return storage->face_uvs[mpoly->loopstart + corner];
}

static void conv_dm_free_user_data(const OpenSubdiv_Converter *converter)
{
	ConvDMStorage *user_data = converter->user_data;
	if (user_data->uvs != NULL) {
		MEM_freeN(user_data->uvs);
	}
	if (user_data->face_uvs != NULL) {
		MEM_freeN(user_data->face_uvs);
	}

#ifdef USE_MESH_ELEMENT_MAPPING
	MEM_freeN(user_data->vert_edge_map);
	MEM_freeN(user_data->vert_edge_mem);
	MEM_freeN(user_data->vert_poly_map);
	MEM_freeN(user_data->vert_poly_mem);
	MEM_freeN(user_data->edge_poly_map);
	MEM_freeN(user_data->edge_poly_mem);
#endif
	MEM_freeN(user_data);
}

void ccgSubSurf_converter_setup_from_derivedmesh(
        CCGSubSurf *ss,
        DerivedMesh *dm,
        OpenSubdiv_Converter *converter)
{
	ConvDMStorage *user_data;

	converter->getSchemeType = conv_dm_get_type;

	converter->getFVarLinearInterpolation =
	        conv_dm_get_fvar_linear_interpolation;

	converter->getNumFaces = conv_dm_get_num_faces;
	converter->getNumEdges = conv_dm_get_num_edges;
	converter->getNumVertices = conv_dm_get_num_verts;

	converter->getNumFaceVertices = conv_dm_get_num_face_verts;
	converter->getFaceVertices = conv_dm_get_face_verts;
	converter->getFaceEdges = conv_dm_get_face_edges;

	converter->getEdgeVertices = conv_dm_get_edge_verts;
	converter->getNumEdgeFaces = conv_dm_get_num_edge_faces;
	converter->getEdgeFaces = conv_dm_get_edge_faces;
	converter->getEdgeSharpness = conv_dm_get_edge_sharpness;

	converter->getNumVertexEdges = conv_dm_get_num_vert_edges;
	converter->getVertexEdges = conv_dm_get_vert_edges;
	converter->getNumVertexFaces = conv_dm_get_num_vert_faces;
	converter->getVertexFaces = conv_dm_get_vert_faces;

	converter->getNumUVLayers = conv_dm_get_num_uv_layers;
	converter->precalcUVLayer = conv_dm_precalc_uv_layer;
	converter->finishUVLayer = conv_dm_finish_uv_layer;
	converter->getNumUVCoordinates = conv_dm_get_num_uvs;
	converter->getFaceCornerUVIndex = conv_dm_get_face_corner_uv_index;

	user_data = MEM_mallocN(sizeof(ConvDMStorage), __func__);
	user_data->ss = ss;
	user_data->dm = dm;

	user_data->mvert = dm->getVertArray(dm);
	user_data->medge = dm->getEdgeArray(dm);
	user_data->mloop = dm->getLoopArray(dm);
	user_data->mpoly = dm->getPolyArray(dm);

	memset(&user_data->island_store, 0, sizeof(user_data->island_store));

	user_data->uvs = NULL;
	user_data->face_uvs = NULL;

	converter->freeUserData = conv_dm_free_user_data;
	converter->user_data = user_data;

#ifdef USE_MESH_ELEMENT_MAPPING
	{
		const MEdge *medge = dm->getEdgeArray(dm);
		const MLoop *mloop = dm->getLoopArray(dm);
		const MPoly *mpoly = dm->getPolyArray(dm);
		const int num_vert = dm->getNumVerts(dm),
		          num_edge = dm->getNumEdges(dm),
		          num_loop = dm->getNumLoops(dm),
		          num_poly = dm->getNumPolys(dm);
		BKE_mesh_vert_edge_map_create(&user_data->vert_edge_map,
		                              &user_data->vert_edge_mem,
		                              medge,
		                              num_vert,
		                              num_edge);

		BKE_mesh_vert_poly_map_create(&user_data->vert_poly_map,
		                              &user_data->vert_poly_mem,
		                              mpoly,
		                              mloop,
		                              num_vert,
		                              num_poly,
		                              num_loop);

		BKE_mesh_edge_poly_map_create(&user_data->edge_poly_map,
		                              &user_data->edge_poly_mem,
		                              medge,
		                              num_edge,
		                              mpoly,
		                              num_poly,
		                              mloop,
		                              num_loop);
	}
#endif  /* USE_MESH_ELEMENT_MAPPING */
}

/**
 * Converter from CCGSubSurf
 */

static OpenSubdiv_SchemeType conv_ccg_get_bilinear_type(
        const OpenSubdiv_Converter *converter)
{
	CCGSubSurf *ss = converter->user_data;
	if (ss->meshIFC.simpleSubdiv) {
		return OSD_SCHEME_BILINEAR;
	}
	else {
		return OSD_SCHEME_CATMARK;
	}
}

static OpenSubdiv_FVarLinearInterpolation
conv_ccg_get_fvar_linear_interpolation(const OpenSubdiv_Converter *converter)
{
	CCGSubSurf *ss = converter->user_data;
	if (ss->osd_subdiv_uvs) {
		return OSD_FVAR_LINEAR_INTERPOLATION_CORNERS_ONLY;
	}
	return OSD_FVAR_LINEAR_INTERPOLATION_ALL;
}

static int conv_ccg_get_num_faces(const OpenSubdiv_Converter *converter)
{
	CCGSubSurf *ss = converter->user_data;
	return ss->fMap->numEntries;
}

static int conv_ccg_get_num_edges(const OpenSubdiv_Converter *converter)
{
	CCGSubSurf *ss = converter->user_data;
	return ss->eMap->numEntries;
}

static int conv_ccg_get_num_verts(const OpenSubdiv_Converter *converter)
{
	CCGSubSurf *ss = converter->user_data;
	return ss->vMap->numEntries;
}

static int conv_ccg_get_num_face_verts(const OpenSubdiv_Converter *converter,
                                       int face)
{
	CCGSubSurf *ss = converter->user_data;
	CCGFace *ccg_face = ccgSubSurf_getFace(ss, SET_INT_IN_POINTER(face));
	return ccgSubSurf_getFaceNumVerts(ccg_face);
}

static void conv_ccg_get_face_verts(const OpenSubdiv_Converter *converter,
                                    int face,
                                    int *face_verts)
{
	CCGSubSurf *ss = converter->user_data;
	CCGFace *ccg_face = ccgSubSurf_getFace(ss, SET_INT_IN_POINTER(face));
	int num_face_verts = ccgSubSurf_getFaceNumVerts(ccg_face);
	int loop;
	for (loop = 0; loop < num_face_verts; loop++) {
		CCGVert *ccg_vert = ccgSubSurf_getFaceVert(ccg_face, loop);
		face_verts[loop] = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ccg_vert));
	}
}

static void conv_ccg_get_face_edges(const OpenSubdiv_Converter *converter,
                                    int face,
                                    int *face_edges)
{
	CCGSubSurf *ss = converter->user_data;
	CCGFace *ccg_face = ccgSubSurf_getFace(ss, SET_INT_IN_POINTER(face));
	int num_face_verts = ccgSubSurf_getFaceNumVerts(ccg_face);
	int loop;
	for (loop = 0; loop < num_face_verts; loop++) {
		CCGEdge *ccg_edge = ccgSubSurf_getFaceEdge(ccg_face, loop);
		face_edges[loop] = GET_INT_FROM_POINTER(ccgSubSurf_getEdgeEdgeHandle(ccg_edge));
	}
}

static void conv_ccg_get_edge_verts(const OpenSubdiv_Converter *converter,
                                    int edge,
                                    int *edge_verts)
{
	CCGSubSurf *ss = converter->user_data;
	CCGEdge *ccg_edge = ccgSubSurf_getEdge(ss, SET_INT_IN_POINTER(edge));
	CCGVert *ccg_vert0 = ccgSubSurf_getEdgeVert0(ccg_edge);
	CCGVert *ccg_vert1 = ccgSubSurf_getEdgeVert1(ccg_edge);
	edge_verts[0] = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ccg_vert0));
	edge_verts[1] = GET_INT_FROM_POINTER(ccgSubSurf_getVertVertHandle(ccg_vert1));
}

static int conv_ccg_get_num_edge_faces(const OpenSubdiv_Converter *converter,
                                       int edge)
{
	CCGSubSurf *ss = converter->user_data;
	CCGEdge *ccg_edge = ccgSubSurf_getEdge(ss, SET_INT_IN_POINTER(edge));
	return ccgSubSurf_getEdgeNumFaces(ccg_edge);
}

static void conv_ccg_get_edge_faces(const OpenSubdiv_Converter *converter,
                                    int edge,
                                    int *edge_faces)
{
	CCGSubSurf *ss = converter->user_data;
	CCGEdge *ccg_edge = ccgSubSurf_getEdge(ss, SET_INT_IN_POINTER(edge));
	int num_edge_faces = ccgSubSurf_getEdgeNumFaces(ccg_edge);
	int face;
	for (face = 0; face < num_edge_faces; face++) {
		CCGFace *ccg_face = ccgSubSurf_getEdgeFace(ccg_edge, face);
		edge_faces[face] = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(ccg_face));
	}
}

static float conv_ccg_get_edge_sharpness(const OpenSubdiv_Converter *converter,
                                         int edge)
{
	CCGSubSurf *ss = converter->user_data;
	CCGEdge *ccg_edge = ccgSubSurf_getEdge(ss, SET_INT_IN_POINTER(edge));
	/* TODO(sergey): Multiply by subdivision level once CPU evaluator
	 * is switched to uniform subdivision type.
	 */
	return ccg_edge->crease;
}

static int conv_ccg_get_num_vert_edges(const OpenSubdiv_Converter *converter,
                                       int vert)
{
	CCGSubSurf *ss = converter->user_data;
	CCGVert *ccg_vert = ccgSubSurf_getVert(ss, SET_INT_IN_POINTER(vert));
	return ccgSubSurf_getVertNumEdges(ccg_vert);
}

static void conv_ccg_get_vert_edges(const OpenSubdiv_Converter *converter,
                                    int vert,
                                    int *vert_edges)
{
	CCGSubSurf *ss = converter->user_data;
	CCGVert *ccg_vert = ccgSubSurf_getVert(ss, SET_INT_IN_POINTER(vert));
	int num_vert_edges = ccgSubSurf_getVertNumEdges(ccg_vert);
	int edge;
	for (edge = 0; edge < num_vert_edges; edge++) {
		CCGEdge *ccg_edge = ccgSubSurf_getVertEdge(ccg_vert, edge);
		vert_edges[edge] = GET_INT_FROM_POINTER(ccgSubSurf_getEdgeEdgeHandle(ccg_edge));
	}
}

static int conv_ccg_get_num_vert_faces(const OpenSubdiv_Converter *converter,
                                       int vert)
{
	CCGSubSurf *ss = converter->user_data;
	CCGVert *ccg_vert = ccgSubSurf_getVert(ss, SET_INT_IN_POINTER(vert));
	return ccgSubSurf_getVertNumFaces(ccg_vert);
}

static void conv_ccg_get_vert_faces(const OpenSubdiv_Converter *converter,
                                    int vert,
                                    int *vert_faces)
{
	CCGSubSurf *ss = converter->user_data;
	CCGVert *ccg_vert = ccgSubSurf_getVert(ss, SET_INT_IN_POINTER(vert));
	int num_vert_faces = ccgSubSurf_getVertNumFaces(ccg_vert);
	int face;
	for (face = 0; face < num_vert_faces; face++) {
		CCGFace *ccg_face = ccgSubSurf_getVertFace(ccg_vert, face);
		vert_faces[face] = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(ccg_face));
	}
}

static int conv_ccg_get_num_uv_layers(const OpenSubdiv_Converter *UNUSED(converter))
{
	return 0;
}

static void conv_ccg_precalc_uv_layer(const OpenSubdiv_Converter * UNUSED(converter),
                                      int UNUSED(layer))
{
}

static void conv_ccg_finish_uv_layer(const OpenSubdiv_Converter *UNUSED(converter))
{
}

static int conv_ccg_get_num_uvs(const OpenSubdiv_Converter *UNUSED(converter))
{
	return 0;
}

static int conv_ccg_get_face_corner_uv_index(const OpenSubdiv_Converter *UNUSED(converter),
                                             int UNUSED(face),
                                             int UNUSED(corner_))
{
	return 0;
}

void ccgSubSurf_converter_setup_from_ccg(CCGSubSurf *ss,
                                         OpenSubdiv_Converter *converter)
{
	converter->getSchemeType = conv_ccg_get_bilinear_type;

	converter->getFVarLinearInterpolation =
	        conv_ccg_get_fvar_linear_interpolation;

	converter->getNumFaces = conv_ccg_get_num_faces;
	converter->getNumEdges = conv_ccg_get_num_edges;
	converter->getNumVertices = conv_ccg_get_num_verts;

	converter->getNumFaceVertices = conv_ccg_get_num_face_verts;
	converter->getFaceVertices = conv_ccg_get_face_verts;
	converter->getFaceEdges = conv_ccg_get_face_edges;

	converter->getEdgeVertices = conv_ccg_get_edge_verts;
	converter->getNumEdgeFaces = conv_ccg_get_num_edge_faces;
	converter->getEdgeFaces = conv_ccg_get_edge_faces;
	converter->getEdgeSharpness = conv_ccg_get_edge_sharpness;

	converter->getNumVertexEdges = conv_ccg_get_num_vert_edges;
	converter->getVertexEdges = conv_ccg_get_vert_edges;
	converter->getNumVertexFaces = conv_ccg_get_num_vert_faces;
	converter->getVertexFaces = conv_ccg_get_vert_faces;

	converter->getNumUVLayers = conv_ccg_get_num_uv_layers;
	converter->precalcUVLayer = conv_ccg_precalc_uv_layer;
	converter->finishUVLayer = conv_ccg_finish_uv_layer;
	converter->getNumUVCoordinates = conv_ccg_get_num_uvs;
	converter->getFaceCornerUVIndex = conv_ccg_get_face_corner_uv_index;

	converter->freeUserData = NULL;
	converter->user_data = ss;
}

void ccgSubSurf_converter_free(
        struct OpenSubdiv_Converter *converter)
{
	if (converter->freeUserData) {
		converter->freeUserData(converter);
	}
}

#endif  /* WITH_OPENSUBDIV */
