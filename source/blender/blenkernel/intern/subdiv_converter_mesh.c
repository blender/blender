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
#include "BLI_bitmap.h"
#include "BLI_math_vector.h"

#include "BKE_customdata.h"
#include "BKE_mesh_mapping.h"
#include "BKE_subdiv.h"

#include "MEM_guardedalloc.h"

#include "opensubdiv_capi.h"
#include "opensubdiv_converter_capi.h"

typedef struct ConverterStorage {
	SubdivSettings settings;
	const Mesh *mesh;
	/* Indexed by loop index, value denotes index of face-varying vertex
	 * which corresponds to the UV coordinate.
	 */
	int *loop_uv_indices;
	int num_uv_coordinates;
	/* Indexed by coarse mesh elements, gives index of corresponding element
	 * with ignoring all non-manifold entities.
	 *
	 * NOTE: This isn't strictly speaking manifold, this is more like non-loose
	 * geometry index. As in, index of element as if there were no loose edges
	 * or vertices in the mesh.
	 */
	int *manifold_vertex_index;
	/* Indexed by vertex index from mesh, corresponds to whether this vertex has
	 * infinite sharpness due to non-manifol topology.
	 */
	BLI_bitmap *infinite_sharp_vertices_map;
	/* Reverse mapping to above. */
	int *manifold_vertex_index_reverse;
	int *manifold_edge_index_reverse;
	/* Number of non-loose elements. */
	int num_manifold_vertices;
	int num_manifold_edges;
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

static bool specifies_full_topology(
        const OpenSubdiv_Converter *UNUSED(converter))
{
	return false;
}

static int get_num_faces(const OpenSubdiv_Converter *converter)
{
	ConverterStorage *storage = converter->user_data;
	return storage->mesh->totpoly;
}

static int get_num_edges(const OpenSubdiv_Converter *converter)
{
	ConverterStorage *storage = converter->user_data;
	return storage->num_manifold_edges;
}

static int get_num_vertices(const OpenSubdiv_Converter *converter)
{
	ConverterStorage *storage = converter->user_data;
	return storage->num_manifold_vertices;
}

static int get_num_face_vertices(const OpenSubdiv_Converter *converter,
                                 int manifold_face_index)
{
	ConverterStorage *storage = converter->user_data;
	return storage->mesh->mpoly[manifold_face_index].totloop;
}

static void get_face_vertices(const OpenSubdiv_Converter *converter,
                              int manifold_face_index,
                              int *manifold_face_vertices)
{
	ConverterStorage *storage = converter->user_data;
	const MPoly *poly = &storage->mesh->mpoly[manifold_face_index];
	const MLoop *mloop = storage->mesh->mloop;
	for (int corner = 0; corner < poly->totloop; corner++) {
		manifold_face_vertices[corner] = storage->manifold_vertex_index[
		        mloop[poly->loopstart + corner].v];
	}
}

static void get_edge_vertices(const OpenSubdiv_Converter *converter,
                              int manifold_edge_index,
                              int *manifold_edge_vertices)
{
	ConverterStorage *storage = converter->user_data;
	const int edge_index =
	        storage->manifold_edge_index_reverse[manifold_edge_index];
	const MEdge *edge = &storage->mesh->medge[edge_index];
	manifold_edge_vertices[0] = storage->manifold_vertex_index[edge->v1];
	manifold_edge_vertices[1] = storage->manifold_vertex_index[edge->v2];
}

static float get_edge_sharpness(const OpenSubdiv_Converter *converter,
                                int manifold_edge_index)
{
	ConverterStorage *storage = converter->user_data;
	const int edge_index =
	        storage->manifold_edge_index_reverse[manifold_edge_index];
	const MEdge *medge = storage->mesh->medge;
	const float edge_crease =  (float)medge[edge_index].crease / 255.0f;
	return edge_crease * edge_crease * 10.0f;
}


static bool is_infinite_sharp_vertex(const OpenSubdiv_Converter *converter,
                                     int manifold_vertex_index)
{
	ConverterStorage *storage = converter->user_data;
	const int vertex_index =
	        storage->manifold_vertex_index_reverse[manifold_vertex_index];
	return BLI_BITMAP_TEST_BOOL(storage->infinite_sharp_vertices_map,
	                            vertex_index);
}

static float get_vertex_sharpness(const OpenSubdiv_Converter *UNUSED(converter),
                                  int UNUSED(manifold_vertex_index))
{
	return 0.0f;
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
	MEM_freeN(user_data->manifold_vertex_index);
	MEM_freeN(user_data->infinite_sharp_vertices_map);
	MEM_freeN(user_data->manifold_vertex_index_reverse);
	MEM_freeN(user_data->manifold_edge_index_reverse);
	MEM_freeN(user_data);
}

static void init_functions(OpenSubdiv_Converter *converter)
{
	converter->getSchemeType = get_scheme_type;
	converter->getFVarLinearInterpolation = get_fvar_linear_interpolation;
	converter->specifiesFullTopology = specifies_full_topology;

	converter->getNumFaces = get_num_faces;
	converter->getNumEdges = get_num_edges;
	converter->getNumVertices = get_num_vertices;

	converter->getNumFaceVertices = get_num_face_vertices;
	converter->getFaceVertices = get_face_vertices;
	converter->getFaceEdges = NULL;

	converter->getEdgeVertices = get_edge_vertices;
	converter->getNumEdgeFaces = NULL;
	converter->getEdgeFaces = NULL;
	converter->getEdgeSharpness = get_edge_sharpness;

	converter->getNumVertexEdges = NULL;
	converter->getVertexEdges = NULL;
	converter->getNumVertexFaces = NULL;
	converter->getVertexFaces = NULL;
	converter->isInfiniteSharpVertex = is_infinite_sharp_vertex;
	converter->getVertexSharpness = get_vertex_sharpness;

	converter->getNumUVLayers = get_num_uv_layers;
	converter->precalcUVLayer = precalc_uv_layer;
	converter->finishUVLayer = finish_uv_layer;
	converter->getNumUVCoordinates = get_num_uvs;
	converter->getFaceCornerUVIndex = get_face_corner_uv_index;

	converter->freeUserData = free_user_data;
}

static void initialize_manifold_index_array(const BLI_bitmap *used_map,
                                            const int num_elements,
                                            int **indices_r,
                                            int **indices_reverse_r,
                                            int *num_manifold_elements_r)
{
	int *indices = NULL;
	if (indices_r != NULL) {
		indices = MEM_malloc_arrayN(
	        num_elements, sizeof(int), "manifold indices");
	}
	int *indices_reverse = NULL;
	if (indices_reverse_r != NULL) {
		indices_reverse = MEM_malloc_arrayN(
	        num_elements, sizeof(int), "manifold indices reverse");
	}
	int offset = 0;
	for (int i = 0; i < num_elements; i++) {
		if (BLI_BITMAP_TEST_BOOL(used_map, i)) {
			if (indices != NULL) {
				indices[i] = i - offset;
			}
			if (indices_reverse != NULL) {
				indices_reverse[i - offset] = i;
			}
		}
		else {
			if (indices != NULL) {
				indices[i] = -1;
			}
			offset++;
		}
	}
	if (indices_r != NULL) {
		*indices_r = indices;
	}
	if (indices_reverse_r != NULL) {
		*indices_reverse_r = indices_reverse;
	}
	*num_manifold_elements_r = num_elements - offset;
}

static void initialize_manifold_indices(ConverterStorage *storage)
{
	const Mesh *mesh = storage->mesh;
	const MEdge *medge = mesh->medge;
	const MLoop *mloop = mesh->mloop;
	const MPoly *mpoly = mesh->mpoly;
	/* Set bits of elements which are not loose. */
	BLI_bitmap *vert_used_map = BLI_BITMAP_NEW(mesh->totvert, "vert used map");
	BLI_bitmap *edge_used_map = BLI_BITMAP_NEW(mesh->totedge, "edge used map");
	for (int poly_index = 0; poly_index < mesh->totpoly; poly_index++) {
		const MPoly *poly = &mpoly[poly_index];
		for (int corner = 0; corner < poly->totloop; corner++) {
			const MLoop *loop = &mloop[poly->loopstart + corner];
			BLI_BITMAP_ENABLE(vert_used_map, loop->v);
			BLI_BITMAP_ENABLE(edge_used_map, loop->e);
		}
	}
	initialize_manifold_index_array(vert_used_map,
	                                mesh->totvert,
	                                &storage->manifold_vertex_index,
	                                &storage->manifold_vertex_index_reverse,
	                                &storage->num_manifold_vertices);
	initialize_manifold_index_array(edge_used_map,
	                                mesh->totedge,
	                                NULL,
	                                &storage->manifold_edge_index_reverse,
	                                &storage->num_manifold_edges);
	/* Initialize infinite sharp mapping. */
	storage->infinite_sharp_vertices_map =
	        BLI_BITMAP_NEW(mesh->totvert, "vert used map");
	for (int edge_index = 0; edge_index < mesh->totedge; edge_index++) {
		if (!BLI_BITMAP_TEST_BOOL(edge_used_map, edge_index)) {
			const MEdge *edge = &medge[edge_index];
			BLI_BITMAP_ENABLE(storage->infinite_sharp_vertices_map, edge->v1);
			BLI_BITMAP_ENABLE(storage->infinite_sharp_vertices_map, edge->v2);
		}
	}
	/* Free working variables. */
	MEM_freeN(vert_used_map);
	MEM_freeN(edge_used_map);
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
	initialize_manifold_indices(user_data);
	converter->user_data = user_data;
}

void BKE_subdiv_converter_init_for_mesh(struct OpenSubdiv_Converter *converter,
                                        const SubdivSettings *settings,
                                        const Mesh *mesh)
{
	init_functions(converter);
	init_user_data(converter, settings, mesh);
}
