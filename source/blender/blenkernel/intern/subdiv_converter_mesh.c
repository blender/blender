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
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "subdiv_converter.h"

#include <string.h>

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_math_vector.h"

#include "BKE_customdata.h"
#include "BKE_mesh_mapping.h"
#include "BKE_subdiv.h"

#include "MEM_guardedalloc.h"

#ifdef WITH_OPENSUBDIV
#  include "opensubdiv_capi.h"
#  include "opensubdiv_converter_capi.h"
#endif

/* Use mesh element mapping structures during conversion.
 * Uses more memory but is much faster than naive algorithm.
 */
#define USE_MESH_ELEMENT_MAPPING

#ifdef WITH_OPENSUBDIV
typedef struct ConverterStorage {
	SubdivSettings settings;
	const Mesh *mesh;

#ifdef USE_MESH_ELEMENT_MAPPING
	MeshElemMap *vert_edge_map;
	MeshElemMap *vert_poly_map;
	MeshElemMap *edge_poly_map;
	int *vert_edge_mem;
	int *vert_poly_mem;
	int *edge_poly_mem;
#endif

	/* Indexed by loop index, value denotes index of face-varying vertex
	 * which corresponds to the UV coordinate.
	 */
	int *loop_uv_indices;
	int num_uv_coordinates;
} ConverterStorage;

static OpenSubdiv_SchemeType get_scheme_type(
        const OpenSubdiv_Converter *converter)
{
	ConverterStorage *storage = converter->user_data;
	if (storage->settings.is_simple) {
		return OSD_SCHEME_BILINEAR;
	}
	else {
		return OSD_SCHEME_CATMARK;
	}
}

static OpenSubdiv_FVarLinearInterpolation get_fvar_linear_interpolation(
        const OpenSubdiv_Converter *converter)
{
	ConverterStorage *storage = converter->user_data;
	return BKE_subdiv_converter_fvar_linear_from_settings(&storage->settings);
}

static int get_num_faces(const OpenSubdiv_Converter *converter)
{
	ConverterStorage *storage = converter->user_data;
	return storage->mesh->totpoly;
}

static int get_num_edges(const OpenSubdiv_Converter *converter)
{
	ConverterStorage *storage = converter->user_data;
	return storage->mesh->totedge;
}

static int get_num_verts(const OpenSubdiv_Converter *converter)
{
	ConverterStorage *storage = converter->user_data;
	return storage->mesh->totvert;
}

static int get_num_face_verts(const OpenSubdiv_Converter *converter, int face)
{
	ConverterStorage *storage = converter->user_data;
	return storage->mesh->mpoly[face].totloop;
}

static void get_face_verts(const OpenSubdiv_Converter *converter,
                           int face,
                           int *face_verts)
{
	ConverterStorage *storage = converter->user_data;
	const MPoly *mp = &storage->mesh->mpoly[face];
	const MLoop *mloop = storage->mesh->mloop;
	for (int loop = 0; loop < mp->totloop; loop++) {
		face_verts[loop] = mloop[mp->loopstart + loop].v;
	}
}

static void get_face_edges(const OpenSubdiv_Converter *converter,
                           int face,
                           int *face_edges)
{
	ConverterStorage *storage = converter->user_data;
	const MPoly *mp = &storage->mesh->mpoly[face];
	const MLoop *mloop = storage->mesh->mloop;
	for (int loop = 0; loop < mp->totloop; loop++) {
		face_edges[loop] = mloop[mp->loopstart + loop].e;
	}
}

static void get_edge_verts(const OpenSubdiv_Converter *converter,
                           int edge,
                           int *edge_verts)
{
	ConverterStorage *storage = converter->user_data;
	const MEdge *me = &storage->mesh->medge[edge];
	edge_verts[0] = me->v1;
	edge_verts[1] = me->v2;
}

static int get_num_edge_faces(const OpenSubdiv_Converter *converter, int edge)
{
	ConverterStorage *storage = converter->user_data;
#ifdef USE_MESH_ELEMENT_MAPPING
	return storage->edge_poly_map[edge].count;
#else
	const Mesh *mesh = storage->mesh;
	const MPoly *mpoly = mesh->mpoly;
	const MLoop *mloop = mesh->mloop;
	int num = 0;
	for (int poly = 0; poly < mesh->totpoly; poly++) {
		const MPoly *mp = &mpoly[poly];
		for (int loop = 0; loop < mp->totloop; loop++) {
			const MLoop *ml = &mloop[mp->loopstart + loop];
			if (ml->e == edge) {
				++num;
				break;
			}
		}
	}
	return num;
#endif
}

static void get_edge_faces(const OpenSubdiv_Converter *converter,
                           int edge,
                           int *edge_faces)
{
	ConverterStorage *storage = converter->user_data;
#ifdef USE_MESH_ELEMENT_MAPPING
	memcpy(edge_faces,
	       storage->edge_poly_map[edge].indices,
	       sizeof(int) * storage->edge_poly_map[edge].count);
#else
	const Mesh *mesh = storage->mesh;
	const MPoly *mpoly = mesh->mpoly;
	const MLoop *mloop = mesh->mloop;
	int num = 0;
	for (int poly = 0; poly < mesh->totpoly; poly++) {
		const MPoly *mp = &mpoly[poly];
		for (int loop = 0; loop < mpoly->totloop; loop++) {
			const MLoop *ml = &mloop[mp->loopstart + loop];
			if (ml->e == edge) {
				edge_faces[num++] = poly;
				break;
			}
		}
	}
#endif
}

static float get_edge_sharpness(const OpenSubdiv_Converter *converter, int edge)
{
	ConverterStorage *storage = converter->user_data;
	const MEdge *medge = storage->mesh->medge;
	const float edge_crease =  (float)medge[edge].crease / 255.0f;
	return edge_crease * storage->settings.level;
}

static int get_num_vert_edges(const OpenSubdiv_Converter *converter, int vert)
{
	ConverterStorage *storage = converter->user_data;
#ifdef USE_MESH_ELEMENT_MAPPING
	return storage->vert_edge_map[vert].count;
#else
	const Mesh *mesh = storage->mesh;
	const MEdge *medge = mesh->medge;
	int num = 0;
	for (int edge = 0; edge < mesh->totedge; edge++) {
		const MEdge *me = &medge[edge];
		if (me->v1 == vert || me->v2 == vert) {
			++num;
		}
	}
	return num;
#endif
}

static void get_vert_edges(const OpenSubdiv_Converter *converter,
                           int vert,
                           int *vert_edges)
{
	ConverterStorage *storage = converter->user_data;
#ifdef USE_MESH_ELEMENT_MAPPING
	memcpy(vert_edges,
	       storage->vert_edge_map[vert].indices,
	       sizeof(int) * storage->vert_edge_map[vert].count);
#else
	const Mesh *mesh = storage->mesh;
	const MEdge *medge = mesh->medge;
	int num = 0;
	for (int edge = 0; edge < mesh->totedge; edge++) {
		const MEdge *me = &medge[edge];
		if (me->v1 == vert || me->v2 == vert) {
			vert_edges[num++] = edge;
		}
	}
#endif
}

static int get_num_vert_faces(const OpenSubdiv_Converter *converter, int vert)
{
	ConverterStorage *storage = converter->user_data;
#ifdef USE_MESH_ELEMENT_MAPPING
	return storage->vert_poly_map[vert].count;
#else
	const Mesh *mesh = storage->mesh;
	const MPoly *mpoly = mesh->mpoly;
	const MLoop *mloop = mesh->mloop;
	int num = 0;
	for (int poly = 0; poly < mesh->totpoly; poly++) {
		const MPoly *mp = &mpoly[poly];
		for (int loop = 0; loop < mpoly->totloop; loop++) {
			const MLoop *ml = &mloop[mp->loopstart + loop];
			if (ml->v == vert) {
				++num;
				break;
			}
		}
	}
	return num;
#endif
}

static void get_vert_faces(const OpenSubdiv_Converter *converter,
                           int vert,
                           int *vert_faces)
{
	ConverterStorage *storage = converter->user_data;
#ifdef USE_MESH_ELEMENT_MAPPING
	memcpy(vert_faces,
	       storage->vert_poly_map[vert].indices,
	       sizeof(int) * storage->vert_poly_map[vert].count);
#else
	const Mesh *mesh = storage->mesh;
	const MPoly *mpoly = mesh->mpoly;
	const MLoop *mloop = mesh->mloop;
	int num = 0;
	for (int poly = 0; poly < mesh->totpoly; poly++) {
		const MPoly *mp = &mpoly[poly];
		for (int loop = 0; loop < mpoly->totloop; loop++) {
			const MLoop *ml = &mloop[mp->loopstart + loop];
			if (ml->v == vert) {
				vert_faces[num++] = poly;
				break;
			}
		}
	}
#endif
}

static int get_num_uv_layers(const OpenSubdiv_Converter *converter)
{
	ConverterStorage *storage = converter->user_data;
	const Mesh *mesh = storage->mesh;
	return CustomData_number_of_layers(&mesh->ldata, CD_MLOOPUV);
}

static void precalc_uv_layer(const OpenSubdiv_Converter *converter,
                             const int layer_index)
{
	ConverterStorage *storage = converter->user_data;
	const Mesh *mesh = storage->mesh;
	const MPoly *mpoly = mesh->mpoly;
	const MLoop *mloop = mesh->mloop;
	const MLoopUV *mloopuv = CustomData_get_layer_n(
	        &mesh->ldata, CD_MLOOPUV, layer_index);
	const int num_poly = mesh->totpoly;
	const int num_vert = mesh->totvert;
	const float limit[2] = {STD_UV_CONNECT_LIMIT, STD_UV_CONNECT_LIMIT};
	/* Initialize memory required for the operations. */
	if (storage->loop_uv_indices == NULL) {
		storage->loop_uv_indices = MEM_malloc_arrayN(
		        mesh->totloop, sizeof(int), "loop uv vertex index");
	}
	UvVertMap *uv_vert_map = BKE_mesh_uv_vert_map_create(
	        mpoly, mloop, mloopuv,
	        num_poly, num_vert,
	        limit,
	        false, true);
	/* NOTE: First UV vertex is supposed to be always marked as separate. */
	storage->num_uv_coordinates = -1;
	for (int vertex_index = 0; vertex_index < num_vert; ++vertex_index) {
		const UvMapVert *uv_vert = BKE_mesh_uv_vert_map_get_vert(uv_vert_map,
		                                                         vertex_index);
		while (uv_vert != NULL) {
			if (uv_vert->separate) {
				storage->num_uv_coordinates++;
			}
			const MPoly *mp = &mpoly[uv_vert->poly_index];
			const int global_loop_index = mp->loopstart +
			                              uv_vert->loop_of_poly_index;
			storage->loop_uv_indices[global_loop_index] =
			        storage->num_uv_coordinates;
			uv_vert = uv_vert->next;
		}
	}
	/* So far this value was used as a 0-based index, actual number of UV
	 * vertices is 1 more.
	 */
	storage->num_uv_coordinates += 1;
	BKE_mesh_uv_vert_map_free(uv_vert_map);
}

static void finish_uv_layer(const OpenSubdiv_Converter *UNUSED(converter))
{
}

static int get_num_uvs(const OpenSubdiv_Converter *converter)
{
	ConverterStorage *storage = converter->user_data;
	return storage->num_uv_coordinates;
}

static int get_face_corner_uv_index(const OpenSubdiv_Converter *converter,
                                    const int face_index,
                                    const int corner)
{
	ConverterStorage *storage = converter->user_data;
	const MPoly *mp = &storage->mesh->mpoly[face_index];
	return storage->loop_uv_indices[mp->loopstart + corner];
}

static void free_user_data(const OpenSubdiv_Converter *converter)
{
	ConverterStorage *user_data = converter->user_data;
	MEM_SAFE_FREE(user_data->loop_uv_indices);
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

static void init_functions(OpenSubdiv_Converter *converter)
{
	converter->getSchemeType = get_scheme_type;

	converter->getFVarLinearInterpolation = get_fvar_linear_interpolation;

	converter->getNumFaces = get_num_faces;
	converter->getNumEdges = get_num_edges;
	converter->getNumVertices = get_num_verts;

	converter->getNumFaceVertices = get_num_face_verts;
	converter->getFaceVertices = get_face_verts;
	converter->getFaceEdges = get_face_edges;

	converter->getEdgeVertices = get_edge_verts;
	converter->getNumEdgeFaces = get_num_edge_faces;
	converter->getEdgeFaces = get_edge_faces;
	converter->getEdgeSharpness = get_edge_sharpness;

	converter->getNumVertexEdges = get_num_vert_edges;
	converter->getVertexEdges = get_vert_edges;
	converter->getNumVertexFaces = get_num_vert_faces;
	converter->getVertexFaces = get_vert_faces;

	converter->getNumUVLayers = get_num_uv_layers;
	converter->precalcUVLayer = precalc_uv_layer;
	converter->finishUVLayer = finish_uv_layer;
	converter->getNumUVCoordinates = get_num_uvs;
	converter->getFaceCornerUVIndex = get_face_corner_uv_index;

	converter->freeUserData = free_user_data;
}

static void create_element_maps_if_needed(ConverterStorage *storage)
{
#ifdef USE_MESH_ELEMENT_MAPPING
	const Mesh *mesh = storage->mesh;
	BKE_mesh_vert_edge_map_create(&storage->vert_edge_map,
	                              &storage->vert_edge_mem,
	                              mesh->medge,
	                              mesh->totvert,
	                              mesh->totedge);
	BKE_mesh_vert_poly_map_create(&storage->vert_poly_map,
	                              &storage->vert_poly_mem,
	                              mesh->mpoly,
	                              mesh->mloop,
	                              mesh->totvert,
	                              mesh->totpoly,
	                              mesh->totloop);
	BKE_mesh_edge_poly_map_create(&storage->edge_poly_map,
	                              &storage->edge_poly_mem,
	                              mesh->medge, mesh->totedge,
	                              mesh->mpoly, mesh->totpoly,
	                              mesh->mloop, mesh->totloop);
#else
	(void) storage;  /* Ignored. */
#endif
}

static void init_user_data(OpenSubdiv_Converter *converter,
                           const SubdivSettings *settings,
                           const Mesh *mesh)
{
	ConverterStorage *user_data =
	         MEM_mallocN(sizeof(ConverterStorage), __func__);
	user_data->settings = *settings;
	user_data->mesh = mesh;
	user_data->loop_uv_indices = NULL;
	create_element_maps_if_needed(user_data);
	converter->user_data = user_data;
}
#endif

void BKE_subdiv_converter_init_for_mesh(struct OpenSubdiv_Converter *converter,
                                        const SubdivSettings *settings,
                                        const Mesh *mesh)
{
#ifdef WITH_OPENSUBDIV
	init_functions(converter);
	init_user_data(converter, settings, mesh);
#else
	UNUSED_VARS(converter, settings, mesh);
#endif
}
