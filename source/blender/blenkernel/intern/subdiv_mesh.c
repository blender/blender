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

/** \file blender/blenkernel/intern/subdiv_mesh.c
 *  \ingroup bke
 */

#include "BKE_subdiv.h"

#include "atomic_ops.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_key_types.h"

#include "BLI_alloca.h"
#include "BLI_bitmap.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"

#include "BKE_mesh.h"
#include "BKE_key.h"

#include "MEM_guardedalloc.h"

/* =============================================================================
 * General helpers.
 */

/* Number of ptex faces for a given polygon. */
BLI_INLINE int num_ptex_faces_per_poly_get(const MPoly *poly)
{
	return (poly->totloop == 4) ? 1 : poly->totloop;
}

BLI_INLINE int num_edges_per_ptex_face_get(const int resolution)
{
	return 2 * (resolution - 1) * resolution;
}

BLI_INLINE int num_inner_edges_per_ptex_face_get(const int resolution)
{
	if (resolution < 2) {
		return 0;
	}
	return (resolution - 2) * resolution +
	       (resolution - 1) * (resolution - 1);
}

/* Number of subdivision polygons per ptex face. */
BLI_INLINE int num_polys_per_ptex_get(const int resolution)
{
	return (resolution - 1) * (resolution - 1);
}

/* Subdivision resolution per given polygon's ptex faces. */
BLI_INLINE int ptex_face_resolution_get(const MPoly *poly, int resolution)
{
	return (poly->totloop == 4) ? (resolution)
	                            : ((resolution >> 1) + 1);
}

/* =============================================================================
 * Mesh subdivision context.
 */

typedef struct SubdivMeshContext {
	const Mesh *coarse_mesh;
	Subdiv *subdiv;
	Mesh *subdiv_mesh;
	const SubdivToMeshSettings *settings;
	/* Cached custom data arrays for fastter access. */
	int *vert_origindex;
	int *edge_origindex;
	int *loop_origindex;
	int *poly_origindex;
	/* UV layers interpolation. */
	int num_uv_layers;
	MLoopUV *uv_layers[MAX_MTFACE];
	/* Counters of geometry in subdivided mesh, initialized as a part of
	 * offsets calculation.
	 */
	int num_subdiv_vertices;
	int num_subdiv_edges;
	int num_subdiv_loops;
	int num_subdiv_polygons;
	/* Offsets of various geometry in the subdivision mesh arrays. */
	int vertices_corner_offset;
	int vertices_edge_offset;
	int vertices_inner_offset;
	int edge_boundary_offset;
	int edge_inner_offset;
	/* Indexed by coarse polygon index, indicates offset in subdivided mesh
	 * vertices, edges and polygons arrays, where first element of the poly
	 * begins.
	 */
	int *subdiv_vertex_offset;
	int *subdiv_edge_offset;
	int *subdiv_polygon_offset;
	/* Indexed by base face index, element indicates total number of ptex faces
	 * created for preceding base faces.
	 */
	int *face_ptex_offset;
	/* Bitmap indicating whether vertex was used already or not.
	 * - During patch evaluation indicates whether coarse vertex was already
	 *   evaluated and its position on limit is already known.
	 */
	BLI_bitmap *coarse_vertices_used_map;
	/* Bitmap indicating whether edge was used already or not. This includes:
	 * - During context initialization it indicates whether subdivided verticies
	 *   for corresponding edge were already calculated or not.
	 * - During patch evaluation it indicates whether vertices along this edge
	 *   were already evaluated.
	 */
	BLI_bitmap *coarse_edges_used_map;
} SubdivMeshContext;

static void subdiv_mesh_ctx_cache_uv_layers(SubdivMeshContext *ctx)
{
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	ctx->num_uv_layers =
	        CustomData_number_of_layers(&subdiv_mesh->ldata, CD_MLOOPUV);
	for (int layer_index = 0; layer_index < ctx->num_uv_layers; ++layer_index) {
		ctx->uv_layers[layer_index] = CustomData_get_layer_n(
		        &subdiv_mesh->ldata, CD_MLOOPUV, layer_index);
	}
}

static void subdiv_mesh_ctx_cache_custom_data_layers(SubdivMeshContext *ctx)
{
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	/* Pointers to original indices layers. */
	ctx->vert_origindex = CustomData_get_layer(
	        &subdiv_mesh->vdata, CD_ORIGINDEX);
	ctx->edge_origindex = CustomData_get_layer(
	        &subdiv_mesh->edata, CD_ORIGINDEX);
	ctx->loop_origindex = CustomData_get_layer(
	        &subdiv_mesh->ldata, CD_ORIGINDEX);
	ctx->poly_origindex = CustomData_get_layer(
	        &subdiv_mesh->pdata, CD_ORIGINDEX);
	/* UV layers interpolation. */
	subdiv_mesh_ctx_cache_uv_layers(ctx);
}

/* NOTE: Expects edge map to be zeroed. */
static void subdiv_mesh_ctx_count(SubdivMeshContext *ctx)
{
	/* Reset counters. */
	ctx->num_subdiv_vertices = 0;
	ctx->num_subdiv_edges = 0;
	ctx->num_subdiv_loops = 0;
	ctx->num_subdiv_polygons = 0;
	/* Static geometry counters. */
	const int resolution = ctx->settings->resolution;
	const int no_quad_patch_resolution = ((resolution >> 1) + 1);
	const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
	const int num_inner_vertices_per_quad = (resolution - 2) * (resolution - 2);
	const int num_inner_vertices_per_noquad_patch =
	        (no_quad_patch_resolution - 2) * (no_quad_patch_resolution - 2);
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MLoop *coarse_mloop = coarse_mesh->mloop;
	const MPoly *coarse_mpoly = coarse_mesh->mpoly;
	ctx->num_subdiv_vertices = coarse_mesh->totvert;
	ctx->num_subdiv_edges =
	        coarse_mesh->totedge * (num_subdiv_vertices_per_coarse_edge + 1);
	/* Calculate extra vertices and edges createdd by non-loose geometry. */
	for (int poly_index = 0; poly_index < coarse_mesh->totpoly; poly_index++) {
		const MPoly *coarse_poly = &coarse_mpoly[poly_index];
		const int num_ptex_faces_per_poly =
		        num_ptex_faces_per_poly_get(coarse_poly);
		for (int corner = 0; corner < coarse_poly->totloop; corner++) {
			 const MLoop *loop = &coarse_mloop[coarse_poly->loopstart + corner];
			 const bool is_edge_used =
			         BLI_BITMAP_TEST_BOOL(ctx->coarse_edges_used_map, loop->e);
			/* Edges which aren't counted yet. */
			if (!is_edge_used) {
				BLI_BITMAP_ENABLE(ctx->coarse_edges_used_map, loop->e);
				ctx->num_subdiv_vertices += num_subdiv_vertices_per_coarse_edge;
			}
		}
		/* Inner verticies of polygon. */
		if (num_ptex_faces_per_poly == 1) {
			ctx->num_subdiv_vertices += num_inner_vertices_per_quad;
			ctx->num_subdiv_edges +=
			        num_edges_per_ptex_face_get(resolution - 2) +
			        4 * num_subdiv_vertices_per_coarse_edge;
			ctx->num_subdiv_polygons += num_polys_per_ptex_get(resolution);
		}
		else {
			ctx->num_subdiv_vertices +=
			        1 +
			        num_ptex_faces_per_poly * (no_quad_patch_resolution - 2) +
			        num_ptex_faces_per_poly * num_inner_vertices_per_noquad_patch;
			ctx->num_subdiv_edges +=
			        num_ptex_faces_per_poly *
			                (num_inner_edges_per_ptex_face_get(
			                         no_quad_patch_resolution - 1) +
			                 (no_quad_patch_resolution - 2) +
			                 num_subdiv_vertices_per_coarse_edge);
			if (no_quad_patch_resolution >= 3) {
				ctx->num_subdiv_edges += coarse_poly->totloop;
			}
			ctx->num_subdiv_polygons +=
			        num_ptex_faces_per_poly *
			        num_polys_per_ptex_get(no_quad_patch_resolution);
		}
	}
	/* Calculate extra vertices createdd by loose edges. */
	for (int edge_index = 0; edge_index < coarse_mesh->totedge; edge_index++) {
		if (!BLI_BITMAP_TEST_BOOL(ctx->coarse_edges_used_map, edge_index)) {
			ctx->num_subdiv_vertices += num_subdiv_vertices_per_coarse_edge;
		}
	}
	ctx->num_subdiv_loops = ctx->num_subdiv_polygons * 4;
}

static void subdiv_mesh_ctx_init_offsets(SubdivMeshContext *ctx)
{
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const int resolution = ctx->settings->resolution;
	const int resolution_2 = resolution - 2;
	const int resolution_2_squared = resolution_2 * resolution_2;
	const int no_quad_patch_resolution = ((resolution >> 1) + 1);
	const int num_irregular_vertices_per_patch =
		(no_quad_patch_resolution - 2) * (no_quad_patch_resolution - 1);
	const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
	const int num_subdiv_edges_per_coarse_edge = resolution - 1;
	/* Constant offsets in arrays. */
	ctx->vertices_corner_offset = 0;
	ctx->vertices_edge_offset = coarse_mesh->totvert;
	ctx->vertices_inner_offset =
	        ctx->vertices_edge_offset +
	        coarse_mesh->totedge * num_subdiv_vertices_per_coarse_edge;
	ctx->edge_boundary_offset = 0;
	ctx->edge_inner_offset =
	        ctx->edge_boundary_offset +
	        coarse_mesh->totedge * num_subdiv_edges_per_coarse_edge;
	/* "Indexed" offsets. */
	const MPoly *coarse_mpoly = coarse_mesh->mpoly;
	int vertex_offset = 0;
	int edge_offset = 0;
	int polygon_offset = 0;
	int face_ptex_offset = 0;
	for (int poly_index = 0; poly_index < coarse_mesh->totpoly; poly_index++) {
		const MPoly *coarse_poly = &coarse_mpoly[poly_index];
		const int num_ptex_faces_per_poly =
		        num_ptex_faces_per_poly_get(coarse_poly);
		ctx->face_ptex_offset[poly_index] = face_ptex_offset;
		ctx->subdiv_vertex_offset[poly_index] = vertex_offset;
		ctx->subdiv_edge_offset[poly_index] = edge_offset;
		ctx->subdiv_polygon_offset[poly_index] = polygon_offset;
		face_ptex_offset += num_ptex_faces_per_poly;
		if (num_ptex_faces_per_poly == 1) {
			vertex_offset += resolution_2_squared;
			edge_offset += num_edges_per_ptex_face_get(resolution - 2) +
			               4 * num_subdiv_vertices_per_coarse_edge;
			polygon_offset += num_polys_per_ptex_get(resolution);
		}
		else {
			vertex_offset +=
			        1 +
			        num_ptex_faces_per_poly * num_irregular_vertices_per_patch;
			edge_offset +=
			        num_ptex_faces_per_poly *
			                (num_inner_edges_per_ptex_face_get(
			                         no_quad_patch_resolution - 1) +
			                 (no_quad_patch_resolution - 2) +
			                 num_subdiv_vertices_per_coarse_edge);
			if (no_quad_patch_resolution >= 3) {
				edge_offset += coarse_poly->totloop;
			}
			polygon_offset +=
			        num_ptex_faces_per_poly *
			        num_polys_per_ptex_get(no_quad_patch_resolution);
		}
	}
}

static void subdiv_mesh_ctx_init(SubdivMeshContext *ctx)
{
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	/* Allocate maps and offsets. */
	ctx->coarse_vertices_used_map =
	        BLI_BITMAP_NEW(coarse_mesh->totvert, "vertices used map");
	ctx->coarse_edges_used_map =
	        BLI_BITMAP_NEW(coarse_mesh->totedge, "edges used map");
	ctx->subdiv_vertex_offset = MEM_malloc_arrayN(
	        coarse_mesh->totpoly,
	        sizeof(*ctx->subdiv_vertex_offset),
	        "vertex_offset");
	ctx->subdiv_edge_offset = MEM_malloc_arrayN(
	        coarse_mesh->totpoly,
	        sizeof(*ctx->subdiv_edge_offset),
	        "subdiv_edge_offset");
	ctx->subdiv_polygon_offset = MEM_malloc_arrayN(
	        coarse_mesh->totpoly,
	        sizeof(*ctx->subdiv_polygon_offset),
	        "subdiv_edge_offset");
	ctx->face_ptex_offset = MEM_malloc_arrayN(coarse_mesh->totpoly,
	                                          sizeof(*ctx->face_ptex_offset),
	                                          "face_ptex_offset");
	/* Initialize all offsets. */
	subdiv_mesh_ctx_init_offsets(ctx);
	/* Calculate number of geometry in the result subdivision mesh. */
	subdiv_mesh_ctx_count(ctx);
	/* Re-set maps which were used at this step. */
	BLI_BITMAP_SET_ALL(ctx->coarse_edges_used_map, false, coarse_mesh->totedge);
}

static void subdiv_mesh_ctx_init_result(SubdivMeshContext *ctx)
{
	subdiv_mesh_ctx_cache_custom_data_layers(ctx);
}

static void subdiv_mesh_ctx_free(SubdivMeshContext *ctx)
{
	MEM_freeN(ctx->coarse_vertices_used_map);
	MEM_freeN(ctx->coarse_edges_used_map);
	MEM_freeN(ctx->subdiv_vertex_offset);
	MEM_freeN(ctx->subdiv_edge_offset);
	MEM_freeN(ctx->subdiv_polygon_offset);
	MEM_freeN(ctx->face_ptex_offset);
}

/* =============================================================================
 * Loop custom data copy helpers.
 */

typedef struct LoopsOfPtex {
	/* First loop of the ptex, starts at ptex (0, 0) and goes in u direction. */
	const MLoop *first_loop;
	/* Last loop of the ptex, starts at ptex (0, 0) and goes in v direction. */
	const MLoop *last_loop;
	/* For quad coarse faces only. */
	const MLoop *second_loop;
	const MLoop *third_loop;
} LoopsOfPtex;

static void loops_of_ptex_get(
        const SubdivMeshContext *ctx,
        LoopsOfPtex *loops_of_ptex,
        const MPoly *coarse_poly,
        const int ptex_of_poly_index)
{
	const MLoop *coarse_mloop = ctx->coarse_mesh->mloop;
	const int first_ptex_loop_index =
	        coarse_poly->loopstart + ptex_of_poly_index;
	/* Loop which look in the (opposite) V direction of the current
	 * ptex face.
	 *
	 * TOOD(sergey): Get rid of using module on every iteration.
	 */
	const int last_ptex_loop_index =
	        coarse_poly->loopstart +
	        (ptex_of_poly_index + coarse_poly->totloop - 1) %
	                coarse_poly->totloop;
	loops_of_ptex->first_loop = &coarse_mloop[first_ptex_loop_index];
	loops_of_ptex->last_loop = &coarse_mloop[last_ptex_loop_index];
	if (coarse_poly->totloop == 4) {
		loops_of_ptex->second_loop = loops_of_ptex->first_loop + 1;
		loops_of_ptex->third_loop = loops_of_ptex->first_loop + 2;
	}
	else {
		loops_of_ptex->second_loop = NULL;
		loops_of_ptex->third_loop = NULL;
	}
}

/* =============================================================================
 * Vertex custom data interpolation helpers.
 */

/* TODO(sergey): Somehow de-duplicate with loops storage, without too much
 * exception cases all over the code.
 */

typedef struct VerticesForInterpolation {
	/* This field points to a vertex data which is to be used for interpolation.
	 * The idea is to avoid unnecessary allocations for regular faces, where
	 * we can simply
	 */
	const CustomData *vertex_data;
	/* Vertices data calculated for ptex corners. There are always 4 elements
	 * in this custom data, aligned the following way:
	 *
	 *   index 0 -> uv (0, 0)
	 *   index 1 -> uv (0, 1)
	 *   index 2 -> uv (1, 1)
	 *   index 3 -> uv (1, 0)
	 *
	 * Is allocated for non-regular faces (triangles and n-gons).
	 */
	CustomData vertex_data_storage;
	bool vertex_data_storage_allocated;
	/* Infices within vertex_data to interpolate for. The indices are aligned
	 * with uv coordinates in a similar way as indices in loop_data_storage.
	 */
	int vertex_indices[4];
} VerticesForInterpolation;

static void vertex_interpolation_init(
        const SubdivMeshContext *ctx,
        VerticesForInterpolation *vertex_interpolation,
        const MPoly *coarse_poly)
{
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MLoop *coarse_mloop = coarse_mesh->mloop;
	if (coarse_poly->totloop == 4) {
		vertex_interpolation->vertex_data = &coarse_mesh->vdata;
		vertex_interpolation->vertex_indices[0] =
		        coarse_mloop[coarse_poly->loopstart + 0].v;
		vertex_interpolation->vertex_indices[1] =
		        coarse_mloop[coarse_poly->loopstart + 1].v;
		vertex_interpolation->vertex_indices[2] =
		        coarse_mloop[coarse_poly->loopstart + 2].v;
		vertex_interpolation->vertex_indices[3] =
		        coarse_mloop[coarse_poly->loopstart + 3].v;
		vertex_interpolation->vertex_data_storage_allocated = false;
	}
	else {
		vertex_interpolation->vertex_data =
		        &vertex_interpolation->vertex_data_storage;
		/* Allocate storage for loops corresponding to ptex corners. */
		CustomData_copy(&ctx->coarse_mesh->vdata,
		                &vertex_interpolation->vertex_data_storage,
		                CD_MASK_EVERYTHING,
		                CD_CALLOC,
		                4);
		/* Initialize indices. */
		vertex_interpolation->vertex_indices[0] = 0;
		vertex_interpolation->vertex_indices[1] = 1;
		vertex_interpolation->vertex_indices[2] = 2;
		vertex_interpolation->vertex_indices[3] = 3;
		vertex_interpolation->vertex_data_storage_allocated = true;
		/* Interpolate center of poly right away, it stays unchanged for all
		 * ptex faces.
		 */
		const float weight = 1.0f / (float)coarse_poly->totloop;
		float *weights = BLI_array_alloca(weights, coarse_poly->totloop);
		int *indices = BLI_array_alloca(indices, coarse_poly->totloop);
		for (int i = 0; i < coarse_poly->totloop; ++i) {
			weights[i] = weight;
			indices[i] = coarse_mloop[coarse_poly->loopstart + i].v;
		}
		CustomData_interp(&coarse_mesh->vdata,
		                  &vertex_interpolation->vertex_data_storage,
		                  indices,
		                  weights, NULL,
		                  coarse_poly->totloop,
		                  2);
	}
}

static void vertex_interpolation_from_ptex(
        const SubdivMeshContext *ctx,
        VerticesForInterpolation *vertex_interpolation,
        const MPoly *coarse_poly,
        const int ptex_of_poly_index)
{
	if (coarse_poly->totloop == 4) {
		/* Nothing to do, all indices and data is already assigned. */
	}
	else {
		const CustomData *vertex_data = &ctx->coarse_mesh->vdata;
		const Mesh *coarse_mesh = ctx->coarse_mesh;
		const MLoop *coarse_mloop = coarse_mesh->mloop;
		LoopsOfPtex loops_of_ptex;
		loops_of_ptex_get(ctx, &loops_of_ptex, coarse_poly, ptex_of_poly_index);
		/* Ptex face corner corresponds to a poly loop with same index. */
		CustomData_copy_data(
		        vertex_data,
		        &vertex_interpolation->vertex_data_storage,
		        coarse_mloop[coarse_poly->loopstart + ptex_of_poly_index].v,
		        0,
		        1);
		/* Interpolate remaining ptex face corners, which hits loops
		 * middle points.
		 *
		 * TODO(sergey): Re-use one of interpolation results from previous
		 * iteration.
		 */
		const float weights[2] = {0.5f, 0.5f};
		const int first_loop_index = loops_of_ptex.first_loop - coarse_mloop;
		const int last_loop_index = loops_of_ptex.last_loop - coarse_mloop;
		const int first_indices[2] = {
		        coarse_mloop[first_loop_index].v,
		        coarse_mloop[coarse_poly->loopstart +
		                (first_loop_index - coarse_poly->loopstart + 1) %
		                        coarse_poly->totloop].v};
		const int last_indices[2] = {coarse_mloop[first_loop_index].v,
		                             coarse_mloop[last_loop_index].v};
		CustomData_interp(vertex_data,
		                  &vertex_interpolation->vertex_data_storage,
		                  first_indices,
		                  weights, NULL,
		                  2,
		                  1);
		CustomData_interp(vertex_data,
		                  &vertex_interpolation->vertex_data_storage,
		                  last_indices,
		                  weights, NULL,
		                  2,
		                  3);
	}
}

static void vertex_interpolation_end(
        VerticesForInterpolation *vertex_interpolation)
{
	if (vertex_interpolation->vertex_data_storage_allocated) {
		CustomData_free(&vertex_interpolation->vertex_data_storage, 4);
	}
}

/* =============================================================================
 * Loop custom data interpolation helpers.
 */

typedef struct LoopsForInterpolation {
 /* This field points to a loop data which is to be used for interpolation.
	 * The idea is to avoid unnecessary allocations for regular faces, where
	 * we can simply
	 */
	const CustomData *loop_data;
	/* Loops data calculated for ptex corners. There are always 4 elements
	 * in this custom data, aligned the following way:
	 *
	 *   index 0 -> uv (0, 0)
	 *   index 1 -> uv (0, 1)
	 *   index 2 -> uv (1, 1)
	 *   index 3 -> uv (1, 0)
	 *
	 * Is allocated for non-regular faces (triangles and n-gons).
	 */
	CustomData loop_data_storage;
	bool loop_data_storage_allocated;
	/* Infices within loop_data to interpolate for. The indices are aligned with
	 * uv coordinates in a similar way as indices in loop_data_storage.
	 */
	int loop_indices[4];
} LoopsForInterpolation;

static void loop_interpolation_init(
        const SubdivMeshContext *ctx,
        LoopsForInterpolation *loop_interpolation,
        const MPoly *coarse_poly)
{
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	if (coarse_poly->totloop == 4) {
		loop_interpolation->loop_data = &coarse_mesh->ldata;
		loop_interpolation->loop_indices[0] = coarse_poly->loopstart + 0;
		loop_interpolation->loop_indices[1] = coarse_poly->loopstart + 1;
		loop_interpolation->loop_indices[2] = coarse_poly->loopstart + 2;
		loop_interpolation->loop_indices[3] = coarse_poly->loopstart + 3;
		loop_interpolation->loop_data_storage_allocated = false;
	}
	else {
		loop_interpolation->loop_data = &loop_interpolation->loop_data_storage;
		/* Allocate storage for loops corresponding to ptex corners. */
		CustomData_copy(&ctx->coarse_mesh->ldata,
		                &loop_interpolation->loop_data_storage,
		                CD_MASK_EVERYTHING,
		                CD_CALLOC,
		                4);
		/* Initialize indices. */
		loop_interpolation->loop_indices[0] = 0;
		loop_interpolation->loop_indices[1] = 1;
		loop_interpolation->loop_indices[2] = 2;
		loop_interpolation->loop_indices[3] = 3;
		loop_interpolation->loop_data_storage_allocated = true;
		/* Interpolate center of poly right away, it stays unchanged for all
		 * ptex faces.
		 */
		const float weight = 1.0f / (float)coarse_poly->totloop;
		float *weights = BLI_array_alloca(weights, coarse_poly->totloop);
		int *indices = BLI_array_alloca(indices, coarse_poly->totloop);
		for (int i = 0; i < coarse_poly->totloop; ++i) {
			weights[i] = weight;
			indices[i] = coarse_poly->loopstart + i;
		}
		CustomData_interp(&coarse_mesh->ldata,
		                  &loop_interpolation->loop_data_storage,
		                  indices,
		                  weights, NULL,
		                  coarse_poly->totloop,
		                  2);
	}
}

static void loop_interpolation_from_ptex(
        const SubdivMeshContext *ctx,
        LoopsForInterpolation *loop_interpolation,
        const MPoly *coarse_poly,
        const int ptex_face_index)
{
	if (coarse_poly->totloop == 4) {
		/* Nothing to do, all indices and data is already assigned. */
	}
	else {
		const CustomData *loop_data = &ctx->coarse_mesh->ldata;
		const Mesh *coarse_mesh = ctx->coarse_mesh;
		const MLoop *coarse_mloop = coarse_mesh->mloop;
		LoopsOfPtex loops_of_ptex;
		loops_of_ptex_get(ctx, &loops_of_ptex, coarse_poly, ptex_face_index);
		/* Ptex face corner corresponds to a poly loop with same index. */
		CustomData_free_elem(&loop_interpolation->loop_data_storage, 0, 1);
		CustomData_copy_data(loop_data,
		                     &loop_interpolation->loop_data_storage,
		                     coarse_poly->loopstart + ptex_face_index,
		                     0,
		                     1);
		/* Interpolate remaining ptex face corners, which hits loops
		 * middle points.
		 *
		 * TODO(sergey): Re-use one of interpolation results from previous
		 * iteration.
		 */
		const float weights[2] = {0.5f, 0.5f};
		const int first_indices[2] = {
		        loops_of_ptex.first_loop - coarse_mloop,
		        (loops_of_ptex.first_loop + 1 - coarse_mloop) %
		                coarse_poly->totloop};
		const int last_indices[2] = {
		        loops_of_ptex.last_loop - coarse_mloop,
		        loops_of_ptex.first_loop - coarse_mloop};
		CustomData_interp(loop_data,
		                  &loop_interpolation->loop_data_storage,
		                  first_indices,
		                  weights, NULL,
		                  2,
		                  1);
		CustomData_interp(loop_data,
		                  &loop_interpolation->loop_data_storage,
		                  last_indices,
		                  weights, NULL,
		                  2,
		                  3);
	}
}

static void loop_interpolation_end(LoopsForInterpolation *loop_interpolation)
{
	if (loop_interpolation->loop_data_storage_allocated) {
		CustomData_free(&loop_interpolation->loop_data_storage, 4);
	}
}

/* =============================================================================
 * Vertex subdivision process.
 */

/* Custom data interpolation helpers. */

static void subdiv_vertex_data_copy(
        const SubdivMeshContext *ctx,
        const MVert *coarse_vertex,
        MVert *subdiv_vertex)
{
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	const int coarse_vertex_index = coarse_vertex - coarse_mesh->mvert;
	const int subdiv_vertex_index = subdiv_vertex - subdiv_mesh->mvert;
	CustomData_copy_data(&coarse_mesh->vdata,
	                     &ctx->subdiv_mesh->vdata,
	                     coarse_vertex_index,
	                     subdiv_vertex_index,
	                     1);
}

static void subdiv_vertex_data_interpolate(
        const SubdivMeshContext *ctx,
        MVert *subdiv_vertex,
        const VerticesForInterpolation *vertex_interpolation,
        const float u, const float v)
{
	const int subdiv_vertex_index = subdiv_vertex - ctx->subdiv_mesh->mvert;
	const float weights[4] = {(1.0f - u) * (1.0f - v),
	                          u * (1.0f - v),
	                          u * v,
	                          (1.0f - u) * v};
	CustomData_interp(vertex_interpolation->vertex_data,
	                  &ctx->subdiv_mesh->vdata,
	                  vertex_interpolation->vertex_indices,
	                  weights, NULL,
	                  4,
	                  subdiv_vertex_index);
	if (ctx->vert_origindex != NULL) {
		ctx->vert_origindex[subdiv_vertex_index] = ORIGINDEX_NONE;
	}
}

/* Evaluation of corner vertices. They are coming from coarse vertices. */

static void subdiv_evaluate_corner_vertices_regular(
        SubdivMeshContext *ctx,
        const MPoly *coarse_poly)
{
	const float weights[4][2] = {{0.0f, 0.0f},
	                             {1.0f, 0.0f},
	                             {1.0f, 1.0f},
	                             {0.0f, 1.0f}};
	Subdiv *subdiv = ctx->subdiv;
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MVert *coarse_mvert = coarse_mesh->mvert;
	const MLoop *coarse_mloop = coarse_mesh->mloop;
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MVert *subdiv_mvert = subdiv_mesh->mvert;
	const int poly_index = coarse_poly - coarse_mesh->mpoly;
	const int ptex_face_index = ctx->face_ptex_offset[poly_index];
	for (int corner = 0; corner < coarse_poly->totloop; corner++) {
		const MLoop *coarse_loop =
		    &coarse_mloop[coarse_poly->loopstart + corner];
		if (BLI_BITMAP_TEST_AND_SET_ATOMIC(ctx->coarse_vertices_used_map,
		                                   coarse_loop->v))
		{
			continue;
		}
		const MVert *coarse_vert = &coarse_mvert[coarse_loop->v];
		MVert *subdiv_vert = &subdiv_mvert[
		        ctx->vertices_corner_offset + coarse_loop->v];
		subdiv_vertex_data_copy(ctx, coarse_vert, subdiv_vert);
		BKE_subdiv_eval_limit_point_and_short_normal(
		        subdiv,
		        ptex_face_index,
		        weights[corner][0], weights[corner][1],
		        subdiv_vert->co, subdiv_vert->no);
	}
}

static void subdiv_evaluate_corner_vertices_special(
        SubdivMeshContext *ctx,
        const MPoly *coarse_poly)
{
	Subdiv *subdiv = ctx->subdiv;
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MVert *coarse_mvert = coarse_mesh->mvert;
	const MLoop *coarse_mloop = coarse_mesh->mloop;
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MVert *subdiv_mvert = subdiv_mesh->mvert;
	const int poly_index = coarse_poly - coarse_mesh->mpoly;
	int ptex_face_index = ctx->face_ptex_offset[poly_index];
	for (int corner = 0;
	     corner < coarse_poly->totloop;
	     corner++, ptex_face_index++)
	{
		const MLoop *coarse_loop =
		    &coarse_mloop[coarse_poly->loopstart + corner];
		if (BLI_BITMAP_TEST_AND_SET_ATOMIC(ctx->coarse_vertices_used_map,
		                                   coarse_loop->v))
		{
			continue;
		}
		const MVert *coarse_vert = &coarse_mvert[coarse_loop->v];
		MVert *subdiv_vert = &subdiv_mvert[
		        ctx->vertices_corner_offset + coarse_loop->v];
		subdiv_vertex_data_copy(ctx, coarse_vert, subdiv_vert);
		BKE_subdiv_eval_limit_point_and_short_normal(
		        subdiv,
		        ptex_face_index,
		        0.0f, 0.0f,
		        subdiv_vert->co, subdiv_vert->no);
	}
}

static void subdiv_evaluate_corner_vertices(SubdivMeshContext *ctx,
                                            const MPoly *coarse_poly)
{
	if (coarse_poly->totloop == 4) {
		subdiv_evaluate_corner_vertices_regular(ctx, coarse_poly);
	}
	else {
		subdiv_evaluate_corner_vertices_special(ctx, coarse_poly);
	}
}

/* Evaluation of edge vertices. They are coming from coarse edges. */

static void subdiv_evaluate_edge_vertices_regular(
        SubdivMeshContext *ctx,
        const MPoly *coarse_poly,
        VerticesForInterpolation *vertex_interpolation)
{
	const int resolution = ctx->settings->resolution;
	const int resolution_1 = resolution - 1;
	const float inv_resolution_1 = 1.0f / (float)resolution_1;
	const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
	Subdiv *subdiv = ctx->subdiv;
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MEdge *coarse_medge = coarse_mesh->medge;
	const MLoop *coarse_mloop = coarse_mesh->mloop;
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MVert *subdiv_mvert = subdiv_mesh->mvert;
	const int poly_index = coarse_poly - coarse_mesh->mpoly;
	const int ptex_face_index = ctx->face_ptex_offset[poly_index];
	for (int corner = 0; corner < coarse_poly->totloop; corner++) {
		const MLoop *coarse_loop =
		    &coarse_mloop[coarse_poly->loopstart + corner];
		if (BLI_BITMAP_TEST_AND_SET_ATOMIC(ctx->coarse_edges_used_map,
		                                   coarse_loop->e))
		{
			continue;
		}
		vertex_interpolation_from_ptex(ctx,
		                               vertex_interpolation,
		                               coarse_poly,
		                               corner);
		const MEdge *coarse_edge = &coarse_medge[coarse_loop->e];
		const bool flip = (coarse_edge->v2 == coarse_loop->v);
		MVert *subdiv_vert = &subdiv_mvert[
		        ctx->vertices_edge_offset +
		        coarse_loop->e * num_subdiv_vertices_per_coarse_edge];
		for (int vertex_index = 0;
		     vertex_index < num_subdiv_vertices_per_coarse_edge;
		     vertex_index++, subdiv_vert++)
		{
			float fac = (vertex_index + 1) * inv_resolution_1;
			if (flip) {
				fac = 1.0f - fac;
			}
			if (corner >= 2) {
				fac = 1.0f - fac;
			}
			float u, v;
			if ((corner & 1) == 0) {
				u = fac;
				v = (corner == 2) ? 1.0f : 0.0f;
			}
			else {
				u = (corner == 1) ? 1.0f : 0.0f;
				v = fac;
			}
			subdiv_vertex_data_interpolate(ctx,
			                               subdiv_vert,
			                               vertex_interpolation,
			                               u, v);
			BKE_subdiv_eval_limit_point_and_short_normal(
			        subdiv,
			        ptex_face_index,
			        u, v,
			        subdiv_vert->co, subdiv_vert->no);
		}
	}
}

static void subdiv_evaluate_edge_vertices_special(
        SubdivMeshContext *ctx,
        const MPoly *coarse_poly,
        VerticesForInterpolation *vertex_interpolation)
{
	const int resolution = ctx->settings->resolution;
	const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
	const int num_vertices_per_ptex_edge = ((resolution >> 1) + 1);
	const float inv_ptex_resolution_1 =
	        1.0f / (float)(num_vertices_per_ptex_edge - 1);
	Subdiv *subdiv = ctx->subdiv;
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MEdge *coarse_medge = coarse_mesh->medge;
	const MLoop *coarse_mloop = coarse_mesh->mloop;
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MVert *subdiv_mvert = subdiv_mesh->mvert;
	const int poly_index = coarse_poly - coarse_mesh->mpoly;
	const int ptex_face_start_index = ctx->face_ptex_offset[poly_index];
	int ptex_face_index = ptex_face_start_index;
	for (int corner = 0;
	     corner < coarse_poly->totloop;
	     corner++, ptex_face_index++)
	{
		const MLoop *coarse_loop =
		        &coarse_mloop[coarse_poly->loopstart + corner];
		if (BLI_BITMAP_TEST_AND_SET_ATOMIC(ctx->coarse_edges_used_map,
		                                   coarse_loop->e))
		{
			continue;
		}
		vertex_interpolation_from_ptex(ctx,
		                               vertex_interpolation,
		                               coarse_poly,
		                               corner);
		const MEdge *coarse_edge = &coarse_medge[coarse_loop->e];
		const bool flip = (coarse_edge->v2 == coarse_loop->v);
		MVert *subdiv_vert = &subdiv_mvert[
		        ctx->vertices_edge_offset +
		        coarse_loop->e * num_subdiv_vertices_per_coarse_edge];
		int veretx_delta = 1;
		if (flip) {
			subdiv_vert += num_subdiv_vertices_per_coarse_edge - 1;
			veretx_delta = -1;
		}
		for (int vertex_index = 1;
		     vertex_index < num_vertices_per_ptex_edge;
		     vertex_index++, subdiv_vert += veretx_delta)
		{
			float u = vertex_index * inv_ptex_resolution_1;
			subdiv_vertex_data_interpolate(ctx,
			                               subdiv_vert,
			                               vertex_interpolation,
			                               u, 0.0f);
			BKE_subdiv_eval_limit_point_and_short_normal(
			        subdiv,
			        ptex_face_index,
			        u, 0.0f,
			        subdiv_vert->co, subdiv_vert->no);
		}
		const int next_ptex_face_index =
		        ptex_face_start_index + (corner + 1) % coarse_poly->totloop;
		for (int vertex_index = 1;
		     vertex_index < num_vertices_per_ptex_edge - 1;
		     vertex_index++, subdiv_vert += veretx_delta)
		{
			float v = 1.0f - vertex_index * inv_ptex_resolution_1;
			subdiv_vertex_data_interpolate(ctx,
			                               subdiv_vert,
			                               vertex_interpolation,
			                               0.0f, v);
			BKE_subdiv_eval_limit_point_and_short_normal(
			        subdiv,
			        next_ptex_face_index,
			        0.0f, v,
			        subdiv_vert->co, subdiv_vert->no);
		}
	}
}

static void subdiv_evaluate_edge_vertices(
        SubdivMeshContext *ctx,
        const MPoly *coarse_poly,
        VerticesForInterpolation *vertex_interpolation)
{
	if (coarse_poly->totloop == 4) {
		subdiv_evaluate_edge_vertices_regular(
		        ctx, coarse_poly, vertex_interpolation);
	}
	else {
		subdiv_evaluate_edge_vertices_special(
		        ctx, coarse_poly, vertex_interpolation);
	}
}

/* Evaluation of inner vertices, they are coming from ptex patches. */

static void subdiv_evaluate_inner_vertices_regular(
        SubdivMeshContext *ctx,
        const MPoly *coarse_poly,
        VerticesForInterpolation *vertex_interpolation)
{
	const int resolution = ctx->settings->resolution;
	const float inv_resolution_1 = 1.0f / (float)(resolution - 1);
	Subdiv *subdiv = ctx->subdiv;
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MVert *subdiv_mvert = subdiv_mesh->mvert;
	const int poly_index = coarse_poly - coarse_mesh->mpoly;
	const int ptex_face_index = ctx->face_ptex_offset[poly_index];
	const int start_vertex_index = ctx->subdiv_vertex_offset[poly_index];
	MVert *subdiv_vert =
	        &subdiv_mvert[ctx->vertices_inner_offset + start_vertex_index];
	vertex_interpolation_from_ptex(ctx,
	                               vertex_interpolation,
	                               coarse_poly,
	                               0);
	for (int y = 1; y < resolution - 1; y++) {
		const float v = y * inv_resolution_1;
		for (int x = 1; x < resolution - 1; x++, subdiv_vert++) {
			const float u = x * inv_resolution_1;
			subdiv_vertex_data_interpolate(ctx,
			                               subdiv_vert,
			                               vertex_interpolation,
			                               u, v);
			BKE_subdiv_eval_limit_point_and_short_normal(
			        subdiv,
			        ptex_face_index,
			        u, v,
			        subdiv_vert->co, subdiv_vert->no);
		}
	}
}

static void subdiv_evaluate_inner_vertices_special(
        SubdivMeshContext *ctx,
        const MPoly *coarse_poly,
        VerticesForInterpolation *vertex_interpolation)
{
	const int resolution = ctx->settings->resolution;
	const int ptex_face_resolution = ptex_face_resolution_get(
	        coarse_poly, resolution);
	const float inv_ptex_face_resolution_1 =
	        1.0f / (float)(ptex_face_resolution - 1);
	Subdiv *subdiv = ctx->subdiv;
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MVert *subdiv_mvert = subdiv_mesh->mvert;
	const int poly_index = coarse_poly - coarse_mesh->mpoly;
	int ptex_face_index = ctx->face_ptex_offset[poly_index];
	const int start_vertex_index = ctx->subdiv_vertex_offset[poly_index];
	MVert *subdiv_vert =
	        &subdiv_mvert[ctx->vertices_inner_offset + start_vertex_index];
	vertex_interpolation_from_ptex(ctx,
	                               vertex_interpolation,
	                               coarse_poly,
	                               0);
	subdiv_vertex_data_interpolate(ctx,
	                               subdiv_vert,
	                               vertex_interpolation,
	                               1.0f, 1.0f);
	BKE_subdiv_eval_limit_point_and_short_normal(
	        subdiv,
	        ptex_face_index,
	        1.0f, 1.0f,
	        subdiv_vert->co, subdiv_vert->no);
	subdiv_vert++;
	for (int corner = 0;
	     corner < coarse_poly->totloop;
	     corner++, ptex_face_index++)
	{
		if (corner != 0) {
			vertex_interpolation_from_ptex(ctx,
			                               vertex_interpolation,
			                               coarse_poly,
			                               corner);
		}
		for (int y = 1; y < ptex_face_resolution - 1; y++) {
			const float v = y * inv_ptex_face_resolution_1;
			for (int x = 1; x < ptex_face_resolution; x++, subdiv_vert++) {
				const float u = x * inv_ptex_face_resolution_1;
				subdiv_vertex_data_interpolate(ctx,
				                               subdiv_vert,
				                               vertex_interpolation,
				                               u, v);
				BKE_subdiv_eval_limit_point_and_short_normal(
				        subdiv,
				        ptex_face_index,
				        u, v,
				        subdiv_vert->co, subdiv_vert->no);
			}
		}
	}
}

static void subdiv_evaluate_inner_vertices(
        SubdivMeshContext *ctx,
        const MPoly *coarse_poly,
        VerticesForInterpolation *vertex_interpolation)
{
	if (coarse_poly->totloop == 4) {
		subdiv_evaluate_inner_vertices_regular(
		        ctx, coarse_poly, vertex_interpolation);
	}
	else {
		subdiv_evaluate_inner_vertices_special(
		        ctx, coarse_poly, vertex_interpolation);
	}
}

/* Evaluate all vertices which are emitted from given coarse polygon. */
static void subdiv_evaluate_vertices(SubdivMeshContext *ctx,
                                     const int poly_index)
{
	/* Base/coarse mesh information. */
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MPoly *coarse_mpoly = coarse_mesh->mpoly;
	const MPoly *coarse_poly = &coarse_mpoly[poly_index];
	/* Initialize vertex interpolation, it is reused by corner vertices, coarse
	 * edges and patch evaluation.
	 */
	VerticesForInterpolation vertex_interpolation;
	vertex_interpolation_init(ctx, &vertex_interpolation, coarse_poly);
	subdiv_evaluate_corner_vertices(ctx, coarse_poly);
	subdiv_evaluate_edge_vertices(ctx, coarse_poly, &vertex_interpolation);
	subdiv_evaluate_inner_vertices(ctx, coarse_poly, &vertex_interpolation);
	vertex_interpolation_end(&vertex_interpolation);
}

/* =============================================================================
 * Edge subdivision process.
 */

static void subdiv_copy_edge_data(
        SubdivMeshContext *ctx,
        MEdge *subdiv_edge,
        const MEdge *coarse_edge)
{
	const int subdiv_edge_index = subdiv_edge - ctx->subdiv_mesh->medge;
	if (coarse_edge == NULL) {
		subdiv_edge->crease = 0;
		subdiv_edge->bweight = 0;
		subdiv_edge->flag = 0;
		if (ctx->edge_origindex != NULL) {
			ctx->edge_origindex[subdiv_edge_index] = ORIGINDEX_NONE;
		}
		return;
	}
	const int coarse_edge_index = coarse_edge - ctx->coarse_mesh->medge;
	CustomData_copy_data(&ctx->coarse_mesh->edata,
	                     &ctx->subdiv_mesh->edata,
	                     coarse_edge_index,
	                     subdiv_edge_index,
	                     1);
}

static MEdge *subdiv_create_edges_row(SubdivMeshContext *ctx,
                                      MEdge *subdiv_edge,
                                      const MEdge *coarse_edge,
                                      const int start_vertex_index,
                                      const int num_edges_per_row)
{
	int vertex_index = start_vertex_index;
	for (int edge_index = 0;
	     edge_index < num_edges_per_row - 1;
	     edge_index++, subdiv_edge++)
	{
		subdiv_copy_edge_data(ctx, subdiv_edge, coarse_edge);
		subdiv_edge->v1 = vertex_index;
		subdiv_edge->v2 = vertex_index + 1;
		vertex_index += 1;
	}
	return subdiv_edge;
}

static MEdge *subdiv_create_edges_column(SubdivMeshContext *ctx,
                                         MEdge *subdiv_edge,
                                         const MEdge *coarse_start_edge,
                                         const MEdge *coarse_end_edge,
                                         const int start_vertex_index,
                                         const int num_edges_per_row)
{
	int vertex_index = start_vertex_index;
	for (int edge_index = 0;
	     edge_index < num_edges_per_row;
	     edge_index++, subdiv_edge++)
	{
		const MEdge *coarse_edge = NULL;
		if (edge_index == 0) {
			coarse_edge = coarse_start_edge;
		}
		else if (edge_index == num_edges_per_row - 1) {
			coarse_edge = coarse_end_edge;
		}
		subdiv_copy_edge_data(ctx, subdiv_edge, coarse_edge);
		subdiv_edge->v1 = vertex_index;
		subdiv_edge->v2 = vertex_index + num_edges_per_row;
		vertex_index += 1;
	}
	return subdiv_edge;
}

/* Create edges between inner vertices of patch, and also edges to the
 * boundary.
 */

/* Consider a subdivision of base face at level 1:
 *
 *  y
 *  ^
 *  |   (6) ---- (7) ---- (8)
 *  |    |        |        |
 *  |   (3) ---- (4) ---- (5)
 *  |    |        |        |
 *  |   (0) ---- (1) ---- (2)
 *  o---------------------------> x
 *
 * This is illustrate which parts of geometry is created by code below.
 */

static void subdiv_create_edges_all_patches_regular(
        SubdivMeshContext *ctx,
        const MPoly *coarse_poly)
{
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MEdge *coarse_medge = coarse_mesh->medge;
	const MLoop *coarse_mloop = coarse_mesh->mloop;
	const MPoly *coarse_mpoly = coarse_mesh->mpoly;
	const int poly_index = coarse_poly - coarse_mpoly;
	const int resolution = ctx->settings->resolution;
	const int start_vertex_index =
	        ctx->vertices_inner_offset +
	        ctx->subdiv_vertex_offset[poly_index];
	const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MEdge *subdiv_medge = subdiv_mesh->medge;
	MEdge *subdiv_edge = &subdiv_medge[
	        ctx->edge_inner_offset + ctx->subdiv_edge_offset[poly_index]];
	/* Create bottom row of edges (0-1, 1-2). */
	subdiv_edge = subdiv_create_edges_row(
	        ctx,
	        subdiv_edge,
	        NULL,
	        start_vertex_index,
	        resolution - 2);
	/* Create remaining edges. */
	for (int row = 0; row < resolution - 3; row++) {
		const int start_row_vertex_index =
		        start_vertex_index + row * (resolution - 2);
		/* Create vertical columns.
		 *
		 * At first iteration it will be edges (0-3. 1-4, 2-5), then it
		 * will be (3-6, 4-7, 5-8) and so on.
		 */
		subdiv_edge = subdiv_create_edges_column(
		        ctx,
		        subdiv_edge,
		        NULL,
		        NULL,
		        start_row_vertex_index,
		        resolution - 2);
		/* Create horizontal edge row.
		 *
		 * At first iteration it will be edges (3-4, 4-5), then it will be
		 * (6-7, 7-8) and so on.
		 */
		subdiv_edge = subdiv_create_edges_row(
		        ctx,
		        subdiv_edge,
		        NULL,
		        start_row_vertex_index + resolution - 2,
		        resolution - 2);
	}
	/* Connect inner part of patch to boundary. */
	for (int corner = 0; corner < coarse_poly->totloop; corner++) {
		const MLoop *coarse_loop =
		        &coarse_mloop[coarse_poly->loopstart + corner];
		const MEdge *coarse_edge = &coarse_medge[coarse_loop->e];
		const int start_edge_vertex = ctx->vertices_edge_offset +
		        coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
		const bool flip = (coarse_edge->v2 == coarse_loop->v);
		int side_start_index = start_vertex_index;
		int side_stride = 0;
		/* Calculate starting veretx of corresponding inner part of ptex. */
		if (corner == 0) {
			side_stride = 1;
		}
		else if (corner == 1) {
			side_start_index += resolution - 3;
			side_stride = resolution - 2;
		}
		else if (corner == 2) {
			side_start_index += num_subdiv_vertices_per_coarse_edge *
			                    num_subdiv_vertices_per_coarse_edge - 1;
			side_stride = -1;
		}
		else if (corner == 3) {
			side_start_index += num_subdiv_vertices_per_coarse_edge *
			                    (num_subdiv_vertices_per_coarse_edge - 1);
			side_stride = -(resolution - 2);
		}
		for (int i = 0; i < resolution - 2; i++, subdiv_edge++) {
			subdiv_copy_edge_data(ctx, subdiv_edge, NULL);
			if (flip) {
				subdiv_edge->v1 = start_edge_vertex + (resolution - i - 3);
			}
			else {
				subdiv_edge->v1 = start_edge_vertex + i;
			}
			subdiv_edge->v2 = side_start_index + side_stride * i;
		}
	}
}

static void subdiv_create_edges_all_patches_special(
        SubdivMeshContext *ctx,
        const MPoly *coarse_poly)
{
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MEdge *coarse_medge = coarse_mesh->medge;
	const MLoop *coarse_mloop = coarse_mesh->mloop;
	const MPoly *coarse_mpoly = coarse_mesh->mpoly;
	const int poly_index = coarse_poly - coarse_mpoly;
	const int resolution = ctx->settings->resolution;
	const int ptex_face_resolution =
	        ptex_face_resolution_get(coarse_poly, resolution);
	const int ptex_face_inner_resolution = ptex_face_resolution - 2;
	const int num_inner_vertices_per_ptex =
	        (ptex_face_resolution - 1) * (ptex_face_resolution - 2);
	const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
	const int center_vertex_index =
	        ctx->vertices_inner_offset +
	        ctx->subdiv_vertex_offset[poly_index];
	const int start_vertex_index = center_vertex_index + 1;
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MEdge *subdiv_medge = subdiv_mesh->medge;
	MEdge *subdiv_edge = &subdiv_medge[
	        ctx->edge_inner_offset + ctx->subdiv_edge_offset[poly_index]];
	/* Create inner ptex edges. */
	for (int corner = 0; corner < coarse_poly->totloop; corner++) {
		const int start_ptex_face_vertex_index =
		        start_vertex_index + corner * num_inner_vertices_per_ptex;
		/* Similar steps to regular patch case. */
		subdiv_edge = subdiv_create_edges_row(
		        ctx,
		        subdiv_edge,
		        NULL,
		        start_ptex_face_vertex_index,
		        ptex_face_inner_resolution + 1);
		for (int row = 0; row < ptex_face_inner_resolution - 1; row++) {
			const int start_row_vertex_index =
			        start_ptex_face_vertex_index +
			        row * (ptex_face_inner_resolution + 1);
			subdiv_edge = subdiv_create_edges_column(
			        ctx,
			        subdiv_edge,
			        NULL,
			        NULL,
			        start_row_vertex_index,
			        ptex_face_inner_resolution + 1);
			subdiv_edge = subdiv_create_edges_row(
			        ctx,
			        subdiv_edge,
			        NULL,
			        start_row_vertex_index + ptex_face_inner_resolution + 1,
			        ptex_face_inner_resolution + 1);
		}
	}
	/* Create connections between ptex faces. */
	for (int corner = 0; corner < coarse_poly->totloop; corner++) {
		const int next_corner = (corner + 1) % coarse_poly->totloop;
		int current_patch_vertex_index =
		        start_vertex_index + corner * num_inner_vertices_per_ptex +
		        ptex_face_inner_resolution;
		int next_path_vertex_index =
		        start_vertex_index + next_corner * num_inner_vertices_per_ptex +
		        num_inner_vertices_per_ptex - ptex_face_resolution + 1;
		for (int row = 0;
		     row < ptex_face_inner_resolution;
		     row++, subdiv_edge++)
		{
			subdiv_copy_edge_data(ctx, subdiv_edge, NULL);
			subdiv_edge->v1 = current_patch_vertex_index;
			subdiv_edge->v2 = next_path_vertex_index;
			current_patch_vertex_index += ptex_face_inner_resolution + 1;
			next_path_vertex_index += 1;
		}
	}
	/* Create edges from center. */
	if (ptex_face_resolution >= 3) {
		for (int corner = 0;
		     corner < coarse_poly->totloop;
		     corner++, subdiv_edge++)
		{
			const int current_patch_end_vertex_index =
			        start_vertex_index + corner * num_inner_vertices_per_ptex +
			        num_inner_vertices_per_ptex - 1;
			subdiv_copy_edge_data(ctx, subdiv_edge, NULL);
			subdiv_edge->v1 = center_vertex_index;
			subdiv_edge->v2 = current_patch_end_vertex_index;
		}
	}
	/* Connect inner path of patch to boundary. */
	const MLoop *prev_coarse_loop =
	        &coarse_mloop[coarse_poly->loopstart + coarse_poly->totloop - 1];
	for (int corner = 0; corner < coarse_poly->totloop; corner++) {
		const MLoop *coarse_loop =
		        &coarse_mloop[coarse_poly->loopstart + corner];
		{
			const MEdge *coarse_edge = &coarse_medge[coarse_loop->e];
			const int start_edge_vertex = ctx->vertices_edge_offset +
			        coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
			const bool flip = (coarse_edge->v2 == coarse_loop->v);
			int side_start_index;
			if (ptex_face_resolution >= 3) {
				side_start_index =
				        start_vertex_index + num_inner_vertices_per_ptex * corner;
			}
			else {
				side_start_index = center_vertex_index;
			}
			for (int i = 0; i < ptex_face_resolution - 1; i++, subdiv_edge++) {
				subdiv_copy_edge_data(ctx, subdiv_edge, NULL);
				if (flip) {
					subdiv_edge->v1 = start_edge_vertex + (resolution - i - 3);
				}
				else {
					subdiv_edge->v1 = start_edge_vertex + i;
				}
				subdiv_edge->v2 = side_start_index + i;
			}
		}
		if (ptex_face_resolution >= 3) {
			const MEdge *coarse_edge = &coarse_medge[prev_coarse_loop->e];
			const int start_edge_vertex = ctx->vertices_edge_offset +
			        prev_coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
			const bool flip = (coarse_edge->v2 == coarse_loop->v);
			int side_start_index =
			        start_vertex_index + num_inner_vertices_per_ptex * corner;
			for (int i = 0; i < ptex_face_resolution - 2; i++, subdiv_edge++) {
				subdiv_copy_edge_data(ctx, subdiv_edge, NULL);
				if (flip) {
					subdiv_edge->v1 = start_edge_vertex + (resolution - i - 3);
				}
				else {
					subdiv_edge->v1 = start_edge_vertex + i;
				}
				subdiv_edge->v2 = side_start_index +
				                  (ptex_face_inner_resolution + 1) * i;
			}
		}
		prev_coarse_loop = coarse_loop;
	}
}

static void subdiv_create_edges_all_patches(
        SubdivMeshContext *ctx,
        const MPoly *coarse_poly)
{
	if (coarse_poly->totloop == 4) {
		subdiv_create_edges_all_patches_regular(ctx, coarse_poly);
	}
	else {
		subdiv_create_edges_all_patches_special(ctx, coarse_poly);
	}
}

static void subdiv_create_edges(SubdivMeshContext *ctx, int poly_index)
{
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MPoly *coarse_mpoly = coarse_mesh->mpoly;
	const MPoly *coarse_poly = &coarse_mpoly[poly_index];
	subdiv_create_edges_all_patches(ctx, coarse_poly);
}

static void subdiv_create_boundary_edges(
        SubdivMeshContext *ctx,
        int edge_index)
{
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MEdge *coarse_medge = coarse_mesh->medge;
	const MEdge *coarse_edge = &coarse_medge[edge_index];
	const int resolution = ctx->settings->resolution;
	const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
	const int num_subdiv_edges_per_coarse_edge = resolution - 1;
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MEdge *subdiv_medge = subdiv_mesh->medge;
	MEdge *subdiv_edge = &subdiv_medge[
	        ctx->edge_boundary_offset +
	        edge_index * num_subdiv_edges_per_coarse_edge];
	int last_vertex_index = ctx->vertices_corner_offset + coarse_edge->v1;
	for (int i = 0;
	     i < num_subdiv_edges_per_coarse_edge - 1;
	     i++, subdiv_edge++)
	{
		subdiv_copy_edge_data(ctx, subdiv_edge, coarse_edge);
		subdiv_edge->v1 = last_vertex_index;
		subdiv_edge->v2 =
		        ctx->vertices_edge_offset +
		        edge_index * num_subdiv_vertices_per_coarse_edge +
		        i;
		last_vertex_index = subdiv_edge->v2;
	}
	subdiv_copy_edge_data(ctx, subdiv_edge, coarse_edge);
	subdiv_edge->v1 = last_vertex_index;
	subdiv_edge->v2 = ctx->vertices_corner_offset + coarse_edge->v2;
}

/* =============================================================================
 * Loops creation/interpolation.
 */

static void subdiv_copy_loop_data(
        const SubdivMeshContext *ctx,
        MLoop *subdiv_loop,
        const LoopsForInterpolation *loop_interpolation,
        const float u, const float v)
{
	const int subdiv_loop_index = subdiv_loop - ctx->subdiv_mesh->mloop;
	const float weights[4] = {(1.0f - u) * (1.0f - v),
	                          u * (1.0f - v),
	                          u * v,
	                          (1.0f - u) * v};
	CustomData_interp(loop_interpolation->loop_data,
	                  &ctx->subdiv_mesh->ldata,
	                  loop_interpolation->loop_indices,
	                  weights, NULL,
	                  4,
	                  subdiv_loop_index);
	/* TODO(sergey): Set ORIGINDEX. */
}

static void subdiv_eval_uv_layer(SubdivMeshContext *ctx,
                                 MLoop *subdiv_loop,
                                 const int ptex_face_index,
                                 const float u, const float v,
                                 const float du, const float dv)
{
	if (ctx->num_uv_layers == 0) {
		return;
	}
	Subdiv *subdiv = ctx->subdiv;
	const int mloop_index = subdiv_loop - ctx->subdiv_mesh->mloop;
	for (int layer_index = 0; layer_index < ctx->num_uv_layers; layer_index++) {
		MLoopUV *subdiv_loopuv = &ctx->uv_layers[layer_index][mloop_index];
		BKE_subdiv_eval_face_varying(subdiv,
		                             layer_index,
		                             ptex_face_index,
		                             u, v,
		                             subdiv_loopuv[0].uv);
		BKE_subdiv_eval_face_varying(subdiv,
		                             layer_index,
		                             ptex_face_index,
		                             u + du, v,
		                             subdiv_loopuv[1].uv);
		BKE_subdiv_eval_face_varying(subdiv,
		                             layer_index,
		                             ptex_face_index,
		                             u + du, v + dv,
		                             subdiv_loopuv[2].uv);
		BKE_subdiv_eval_face_varying(subdiv,
		                             layer_index,
		                             ptex_face_index,
		                             u, v + dv,
		                             subdiv_loopuv[3].uv);
	}
}

static void rotate_indices(const int rot, int *a, int *b, int *c, int *d)
{
	int values[4] = {*a, *b, *c, *d};
	*a = values[(0 - rot + 4) % 4];
	*b = values[(1 - rot + 4) % 4];
	*c = values[(2 - rot + 4) % 4];
	*d = values[(3 - rot + 4) % 4];
}

static void subdiv_create_loops_of_poly(
        SubdivMeshContext *ctx,
        LoopsForInterpolation *loop_interpolation,
        MLoop *subdiv_loop_start,
        const int ptex_face_index,
        const int rotation,
        /*const*/ int v0, /*const*/ int e0,
        /*const*/ int v1, /*const*/ int e1,
        /*const*/ int v2, /*const*/ int e2,
        /*const*/ int v3, /*const*/ int e3,
        const float u, const float v,
        const float du, const float dv)
{
	rotate_indices(rotation, &v0, &v1, &v2, &v3);
	rotate_indices(rotation, &e0, &e1, &e2, &e3);
	subdiv_copy_loop_data(ctx,
	                      &subdiv_loop_start[0],
	                      loop_interpolation,
	                      u, v);
	subdiv_loop_start[0].v = v0;
	subdiv_loop_start[0].e = e0;
	subdiv_copy_loop_data(ctx,
	                      &subdiv_loop_start[1],
	                      loop_interpolation,
	                      u + du, v);
	subdiv_loop_start[1].v = v1;
	subdiv_loop_start[1].e = e1;
	subdiv_copy_loop_data(ctx,
	                      &subdiv_loop_start[2],
	                      loop_interpolation,
	                      u + du, v + dv);
	subdiv_loop_start[2].v = v2;
	subdiv_loop_start[2].e = e2;
	subdiv_copy_loop_data(ctx,
	                      &subdiv_loop_start[3],
	                      loop_interpolation,
	                      u, v + dv);
	subdiv_loop_start[3].v = v3;
	subdiv_loop_start[3].e = e3;
	/* Interpolate UV layers using OpenSubdiv. */
	subdiv_eval_uv_layer(ctx,
	                     subdiv_loop_start,
	                     ptex_face_index,
	                     u, v, du, dv);
}

static void subdiv_create_loops_regular(SubdivMeshContext *ctx,
                                        const MPoly *coarse_poly)
{
	const int resolution = ctx->settings->resolution;
	/* Base/coarse mesh information. */
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MEdge *coarse_medge = coarse_mesh->medge;
	const MLoop *coarse_mloop = coarse_mesh->mloop;
	const MPoly *coarse_mpoly = coarse_mesh->mpoly;
	const int poly_index = coarse_poly - coarse_mpoly;
	const int ptex_resolution =
	        ptex_face_resolution_get(coarse_poly, resolution);
	const int ptex_inner_resolution = ptex_resolution - 2;
	const int num_subdiv_edges_per_coarse_edge = resolution - 1;
	const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
	const float inv_ptex_resolution_1 = 1.0f / (float)(ptex_resolution - 1);
	const int ptex_face_index = ctx->face_ptex_offset[poly_index];
	const int start_vertex_index =
	        ctx->vertices_inner_offset +
	        ctx->subdiv_vertex_offset[poly_index];
	const int start_edge_index =
	        ctx->edge_inner_offset +
	        ctx->subdiv_edge_offset[poly_index];
	const int start_poly_index = ctx->subdiv_polygon_offset[poly_index];
	const int start_loop_index = 4 * start_poly_index;
	const float du = inv_ptex_resolution_1;
	const float dv = inv_ptex_resolution_1;
	/* Hi-poly subdivided mesh. */
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MLoop *subdiv_loopoop = subdiv_mesh->mloop;
	MLoop *subdiv_loop = &subdiv_loopoop[start_loop_index];
	LoopsForInterpolation loop_interpolation;
	loop_interpolation_init(ctx, &loop_interpolation, coarse_poly);
	loop_interpolation_from_ptex(ctx,
	                             &loop_interpolation,
	                             coarse_poly,
	                             0);
	/* Loops for inner part of ptex. */
	for (int y = 1; y < ptex_resolution - 2; y++) {
		const float v = y * inv_ptex_resolution_1;
		const int inner_y = y - 1;
		for (int x = 1; x < ptex_resolution - 2; x++, subdiv_loop += 4) {
			const int inner_x = x - 1;
			const float u = x * inv_ptex_resolution_1;
			/* Vertex indicies ordered counter-clockwise. */
			const int v0 = start_vertex_index +
			               (inner_y * ptex_inner_resolution + inner_x);
			const int v1 = v0 + 1;
			const int v2 = v0 + ptex_inner_resolution + 1;
			const int v3 = v0 + ptex_inner_resolution;
			/* Edge indicies ordered counter-clockwise. */
			const int e0 = start_edge_index +
			        (inner_y * (2 * ptex_inner_resolution - 1) + inner_x);
			const int e1 = e0 + ptex_inner_resolution;
			const int e2 = e0 + (2 * ptex_inner_resolution - 1);
			const int e3 = e0 + ptex_inner_resolution - 1;
			subdiv_create_loops_of_poly(
			        ctx, &loop_interpolation, subdiv_loop, ptex_face_index, 0,
			        v0, e0, v1, e1, v2, e2, v3, e3,
			        u, v, du, dv);
		}
	}
	/* Loops for faces connecting inner ptex part with boundary. */
	const MLoop *prev_coarse_loop =
	        &coarse_mloop[coarse_poly->loopstart + coarse_poly->totloop - 1];
	for (int corner = 0; corner < coarse_poly->totloop; corner++) {
		const MLoop *coarse_loop =
		        &coarse_mloop[coarse_poly->loopstart + corner];
		const MEdge *coarse_edge = &coarse_medge[coarse_loop->e];
		const MEdge *prev_coarse_edge = &coarse_medge[prev_coarse_loop->e];
		const int start_edge_vertex = ctx->vertices_edge_offset +
		        coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
		const bool flip = (coarse_edge->v2 == coarse_loop->v);
		int side_start_index = start_vertex_index;
		int side_stride = 0;
		int v0 = ctx->vertices_corner_offset + coarse_loop->v;
		int v3, e3;
		int e2_offset, e2_stride;
		float u, v, delta_u, delta_v;
		if (prev_coarse_loop->v == prev_coarse_edge->v1) {
			v3 = ctx->vertices_edge_offset +
		        prev_coarse_loop->e * num_subdiv_vertices_per_coarse_edge +
		        num_subdiv_vertices_per_coarse_edge - 1;
			e3 = ctx->edge_boundary_offset +
			         prev_coarse_loop->e * num_subdiv_edges_per_coarse_edge +
			         num_subdiv_edges_per_coarse_edge - 1;
		}
		else {
			v3 = ctx->vertices_edge_offset +
		        prev_coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
			e3 = ctx->edge_boundary_offset +
			         prev_coarse_loop->e * num_subdiv_edges_per_coarse_edge;
		}
		/* Calculate starting veretx of corresponding inner part of ptex. */
		if (corner == 0) {
			side_stride = 1;
			e2_offset = 0;
			e2_stride = 1;
			u = 0.0f;
			v = 0.0f;
			delta_u = du;
			delta_v = 0.0f;
		}
		else if (corner == 1) {
			side_start_index += resolution - 3;
			side_stride = resolution - 2;
			e2_offset = 2 * num_subdiv_edges_per_coarse_edge - 4;
			e2_stride = 2 * num_subdiv_edges_per_coarse_edge - 3;
			u = 1.0f - du;
			v = 0;
			delta_u = 0.0f;
			delta_v = dv;
		}
		else if (corner == 2) {
			side_start_index += num_subdiv_vertices_per_coarse_edge *
			                    num_subdiv_vertices_per_coarse_edge - 1;
			side_stride = -1;
			e2_offset = num_edges_per_ptex_face_get(resolution - 2) - 1;
			e2_stride = -1;
			u = 1.0f - du;
			v = 1.0f - dv;
			delta_u = -du;
			delta_v = 0.0f;
		}
		else if (corner == 3) {
			side_start_index += num_subdiv_vertices_per_coarse_edge *
			                    (num_subdiv_vertices_per_coarse_edge - 1);
			side_stride = -(resolution - 2);
			e2_offset = num_edges_per_ptex_face_get(resolution - 2) -
			            (2 * num_subdiv_edges_per_coarse_edge - 3);
			e2_stride = -(2 * num_subdiv_edges_per_coarse_edge - 3);
			u = 0.0f;
			v = 1.0f - dv;
			delta_u = 0.0f;
			delta_v = -dv;
		}
		for (int i = 0; i < resolution - 2; i++, subdiv_loop += 4) {
			int v1;
			if (flip) {
				v1 = start_edge_vertex + (resolution - i - 3);
			}
			else {
				v1 = start_edge_vertex + i;
			}
			const int v2 = side_start_index + side_stride * i;
			int e0;
			if (flip) {
				e0 = ctx->edge_boundary_offset +
				         coarse_loop->e * num_subdiv_edges_per_coarse_edge +
				         num_subdiv_edges_per_coarse_edge - i - 1;
			}
			else {
				e0 = ctx->edge_boundary_offset +
				         coarse_loop->e * num_subdiv_edges_per_coarse_edge +
				         i;
			}
			int e1 = start_edge_index +
			        num_edges_per_ptex_face_get(resolution - 2) +
			        corner * num_subdiv_vertices_per_coarse_edge +
			        i;
			int e2;
			if (i == 0) {
				e2 = start_edge_index +
				        num_edges_per_ptex_face_get(resolution - 2) +
				        ((corner - 1 + coarse_poly->totloop) %
				                coarse_poly->totloop) *
				                        num_subdiv_vertices_per_coarse_edge +
				        num_subdiv_vertices_per_coarse_edge - 1;
			}
			else {
				e2 = start_edge_index + e2_offset + e2_stride * (i - 1);
			}
			subdiv_create_loops_of_poly(
			        ctx, &loop_interpolation, subdiv_loop,
			        ptex_face_index, corner,
			        v0, e0, v1, e1, v2, e2, v3, e3,
			        u + delta_u * i, v + delta_v * i, du, dv);
			v0 = v1;
			v3 = v2;
			e3 = e1;
		}
		prev_coarse_loop = coarse_loop;
	}
	loop_interpolation_end(&loop_interpolation);
}

static void subdiv_create_loops_special(SubdivMeshContext *ctx,
                                        const MPoly *coarse_poly)
{
	const int resolution = ctx->settings->resolution;
	/* Base/coarse mesh information. */
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MEdge *coarse_medge = coarse_mesh->medge;
	const MLoop *coarse_mloop = coarse_mesh->mloop;
	const MPoly *coarse_mpoly = coarse_mesh->mpoly;
	const int poly_index = coarse_poly - coarse_mpoly;
	const int ptex_face_resolution =
	        ptex_face_resolution_get(coarse_poly, resolution);
	const int ptex_face_inner_resolution = ptex_face_resolution - 2;
	const float inv_ptex_resolution_1 =
	       1.0f / (float)(ptex_face_resolution - 1);
	const int num_inner_vertices_per_ptex =
	        (ptex_face_resolution - 1) * (ptex_face_resolution - 2);
	const int num_inner_edges_per_ptex_face =
	        num_inner_edges_per_ptex_face_get(
	                ptex_face_inner_resolution + 1);
	const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
	const int num_subdiv_edges_per_coarse_edge = resolution - 1;
	const int ptex_face_index = ctx->face_ptex_offset[poly_index];
	const int center_vertex_index =
	        ctx->vertices_inner_offset +
	        ctx->subdiv_vertex_offset[poly_index];
	const int start_vertex_index = center_vertex_index + 1;
	const int start_inner_vertex_index = center_vertex_index + 1;
	const int start_edge_index = ctx->edge_inner_offset +
	                             ctx->subdiv_edge_offset[poly_index];
	const int start_poly_index = ctx->subdiv_polygon_offset[poly_index];
	const int start_loop_index = 4 * start_poly_index;
	const float du = inv_ptex_resolution_1;
	const float dv = inv_ptex_resolution_1;
	/* Hi-poly subdivided mesh. */
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MLoop *subdiv_loopoop = subdiv_mesh->mloop;
	MLoop *subdiv_loop = &subdiv_loopoop[start_loop_index];
	LoopsForInterpolation loop_interpolation;
	loop_interpolation_init(ctx, &loop_interpolation, coarse_poly);
	for (int corner = 0;
	     corner < coarse_poly->totloop;
	     corner++)
	{
		const int corner_vertex_index =
		        start_vertex_index + corner * num_inner_vertices_per_ptex;
		const int corner_edge_index =
		        start_edge_index + corner * num_inner_edges_per_ptex_face;
		loop_interpolation_from_ptex(ctx,
		                             &loop_interpolation,
		                             coarse_poly,
		                             corner);
		for (int y = 1; y < ptex_face_inner_resolution; y++) {
			const float v = y * inv_ptex_resolution_1;
			const int inner_y = y - 1;
			for (int x = 1;
			     x < ptex_face_inner_resolution + 1;
			     x++, subdiv_loop += 4)
			{
				const int inner_x = x - 1;
				const float u = x * inv_ptex_resolution_1;
				/* Vertex indicies ordered counter-clockwise. */
				const int v0 =
				        corner_vertex_index +
				        (inner_y * (ptex_face_inner_resolution + 1) + inner_x);
				const int v1 = v0 + 1;
				const int v2 = v0 + ptex_face_inner_resolution + 2;
				const int v3 = v0 + ptex_face_inner_resolution + 1;
				/* Edge indicies ordered counter-clockwise. */
				const int e0 = corner_edge_index +
				          (inner_y * (2 * ptex_face_inner_resolution + 1) + inner_x);
				const int e1 = e0 + ptex_face_inner_resolution + 1;
				const int e2 = e0 + (2 * ptex_face_inner_resolution + 1);
				const int e3 = e0 + ptex_face_inner_resolution;
				subdiv_create_loops_of_poly(
				        ctx, &loop_interpolation, subdiv_loop,
				        ptex_face_index + corner, 0,
				        v0, e0, v1, e1, v2, e2, v3, e3,
				        u, v, du, dv);
			}
		}
	}
	/* Create connections between ptex faces. */
	for (int corner = 0; corner < coarse_poly->totloop; corner++) {
		const int next_corner = (corner + 1) % coarse_poly->totloop;
		const int corner_edge_index =
		        start_edge_index + corner * num_inner_edges_per_ptex_face;
		const int next_corner_edge_index =
		        start_edge_index + next_corner * num_inner_edges_per_ptex_face;
		int current_patch_vertex_index =
		        start_inner_vertex_index +
		        corner * num_inner_vertices_per_ptex +
		        ptex_face_inner_resolution;
		int next_path_vertex_index =
		        start_inner_vertex_index +
		        next_corner * num_inner_vertices_per_ptex +
		        num_inner_vertices_per_ptex - ptex_face_resolution + 1;
		int v0 = current_patch_vertex_index;
		int v1 = next_path_vertex_index;
		current_patch_vertex_index += ptex_face_inner_resolution + 1;
		next_path_vertex_index += 1;
		int e0 = start_edge_index +
		         coarse_poly->totloop * num_inner_edges_per_ptex_face +
		         corner * (ptex_face_resolution - 2);
		int e1 = next_corner_edge_index + num_inner_edges_per_ptex_face -
		         ptex_face_resolution + 2;
		int e3 = corner_edge_index + 2 * ptex_face_resolution - 4;
		loop_interpolation_from_ptex(ctx,
		                             &loop_interpolation,
		                             coarse_poly,
		                             next_corner);
		for (int row = 1;
		     row < ptex_face_inner_resolution;
		     row++, subdiv_loop += 4)
		{
			const int v2 = next_path_vertex_index;
			const int v3 = current_patch_vertex_index;
			const int e2 = e0 + 1;
			const float u = row * du;
			const float v = 1.0f - dv;
			subdiv_create_loops_of_poly(
			        ctx, &loop_interpolation, subdiv_loop,
			        ptex_face_index + next_corner, 3,
			        v0, e0, v1, e1, v2, e2, v3, e3,
			        u, v, du, dv);
			current_patch_vertex_index += ptex_face_inner_resolution + 1;
			next_path_vertex_index += 1;
			v0 = v3;
			v1 = v2;
			e0 = e2;
			e1 += 1;
			e3 += 2 * ptex_face_resolution - 3;
		}
	}
	/* Create loops from center. */
	if (ptex_face_resolution >= 3) {
		const int start_center_edge_index =
		        start_edge_index +
		        (num_inner_edges_per_ptex_face +
		         ptex_face_inner_resolution) * coarse_poly->totloop;
		const int start_boundary_edge =
		        start_edge_index +
		        coarse_poly->totloop * num_inner_edges_per_ptex_face +
		        ptex_face_inner_resolution - 1;
		for (int corner = 0, prev_corner = coarse_poly->totloop - 1;
		     corner < coarse_poly->totloop;
		     prev_corner = corner, corner++, subdiv_loop += 4)
		{
			loop_interpolation_from_ptex(ctx,
			                             &loop_interpolation,
			                             coarse_poly,
			                             corner);
			const int corner_edge_index =
			        start_edge_index +
			        corner * num_inner_edges_per_ptex_face;
			const int current_patch_end_vertex_index =
			        start_vertex_index + corner * num_inner_vertices_per_ptex +
			        num_inner_vertices_per_ptex - 1;
			const int prev_current_patch_end_vertex_index =
			        start_vertex_index + prev_corner *
			                num_inner_vertices_per_ptex +
			        num_inner_vertices_per_ptex - 1;
			const int v0 = center_vertex_index;
			const int v1 = prev_current_patch_end_vertex_index;
			const int v2 = current_patch_end_vertex_index - 1;
			const int v3 = current_patch_end_vertex_index;
			const int e0 = start_center_edge_index + prev_corner;
			const int e1 = start_boundary_edge +
			               prev_corner * (ptex_face_inner_resolution);
			const int e2 = corner_edge_index +
			               num_inner_edges_per_ptex_face - 1;
			const int e3 = start_center_edge_index + corner;
			const float u = 1.0f - du;
			const float v = 1.0f - dv;
			subdiv_create_loops_of_poly(
			        ctx, &loop_interpolation, subdiv_loop,
			        ptex_face_index + corner, 2,
			        v0, e0, v1, e1, v2, e2, v3, e3,
			        u, v, du, dv);
		}
	}
	/* Loops for faces connecting inner ptex part with boundary. */
	const MLoop *prev_coarse_loop =
	        &coarse_mloop[coarse_poly->loopstart + coarse_poly->totloop - 1];
	for (int prev_corner = coarse_poly->totloop - 1, corner = 0;
	     corner < coarse_poly->totloop;
	     prev_corner = corner, corner++)
	{
		loop_interpolation_from_ptex(ctx,
		                             &loop_interpolation,
		                             coarse_poly,
		                             corner);
		const MLoop *coarse_loop =
		        &coarse_mloop[coarse_poly->loopstart + corner];
		const MEdge *coarse_edge = &coarse_medge[coarse_loop->e];
		const MEdge *prev_coarse_edge = &coarse_medge[prev_coarse_loop->e];
		const bool flip = (coarse_edge->v2 == coarse_loop->v);
		const int start_edge_vertex = ctx->vertices_edge_offset +
		        coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
		const int corner_vertex_index =
		        start_vertex_index + corner * num_inner_vertices_per_ptex;
		const int corner_edge_index =
		        start_edge_index + corner * num_inner_edges_per_ptex_face;
		/* Create loops for polygons along U axis. */
		int v0 = ctx->vertices_corner_offset + coarse_loop->v;
		int v3, e3;
		if (prev_coarse_loop->v == prev_coarse_edge->v1) {
			v3 = ctx->vertices_edge_offset +
		        prev_coarse_loop->e * num_subdiv_vertices_per_coarse_edge +
		        num_subdiv_vertices_per_coarse_edge - 1;
			e3 = ctx->edge_boundary_offset +
			         prev_coarse_loop->e * num_subdiv_edges_per_coarse_edge +
			         num_subdiv_edges_per_coarse_edge - 1;
		}
		else {
			v3 = ctx->vertices_edge_offset +
		        prev_coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
			e3 = ctx->edge_boundary_offset +
			         prev_coarse_loop->e * num_subdiv_edges_per_coarse_edge;
		}
		for (int i = 0;
		     i <= ptex_face_inner_resolution;
		     i++, subdiv_loop += 4)
		{
			int v1;
			if (flip) {
				v1 = start_edge_vertex + (resolution - i - 3);
			}
			else {
				v1 = start_edge_vertex + i;
			}
			int v2;
			if (ptex_face_inner_resolution >= 1) {
				v2 = corner_vertex_index + i;
			}
			else {
				v2 = center_vertex_index;
			}
			int e0;
			if (flip) {
				e0 = ctx->edge_boundary_offset +
				         coarse_loop->e * num_subdiv_edges_per_coarse_edge +
				         num_subdiv_edges_per_coarse_edge - i - 1;
			}
			else {
				e0 = ctx->edge_boundary_offset +
				         coarse_loop->e * num_subdiv_edges_per_coarse_edge +
				         i;
			}
			int e1 = start_edge_index +
			         corner * (2 * ptex_face_inner_resolution + 1);
			if (ptex_face_resolution >= 3) {
				e1 += coarse_poly->totloop * (num_inner_edges_per_ptex_face +
				                              ptex_face_inner_resolution + 1) +
				      i;
			}
			int e2 = 0;
			if (i == 0 && ptex_face_resolution >= 3) {
				e2 = start_edge_index +
				         coarse_poly->totloop *
				                 (num_inner_edges_per_ptex_face +
				                  ptex_face_inner_resolution + 1) +
				         corner * (2 * ptex_face_inner_resolution + 1) +
				         ptex_face_inner_resolution + 1;
			}
			else if (i == 0 && ptex_face_resolution < 3) {
				e2 = start_edge_index +
			         prev_corner * (2 * ptex_face_inner_resolution + 1);
			}
			else {
				e2 = corner_edge_index + i - 1;
			}
			const float u = du * i;
			const float v = 0.0f;
			subdiv_create_loops_of_poly(
			        ctx, &loop_interpolation, subdiv_loop,
			        ptex_face_index + corner, 0,
			        v0, e0, v1, e1, v2, e2, v3, e3,
			        u, v, du, dv);
			v0 = v1;
			v3 = v2;
			e3 = e1;
		}
		/* Create loops for polygons along V axis. */
		const bool flip_prev = (prev_coarse_edge->v2 == coarse_loop->v);
		v0 = corner_vertex_index;
		if (prev_coarse_loop->v == prev_coarse_edge->v1) {
			v3 = ctx->vertices_edge_offset +
		        prev_coarse_loop->e * num_subdiv_vertices_per_coarse_edge +
		        num_subdiv_vertices_per_coarse_edge - 1;
		}
		else {
			v3 = ctx->vertices_edge_offset +
		        prev_coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
		}
		e3 = start_edge_index +
		         coarse_poly->totloop *
		                 (num_inner_edges_per_ptex_face +
		                  ptex_face_inner_resolution + 1) +
		         corner * (2 * ptex_face_inner_resolution + 1) +
		         ptex_face_inner_resolution + 1;
		for (int i = 0;
		     i <= ptex_face_inner_resolution - 1;
		     i++, subdiv_loop += 4)
		{
			int v1;
			int e0, e1;
			if (i == ptex_face_inner_resolution - 1) {
				v1 = start_vertex_index +
				     prev_corner * num_inner_vertices_per_ptex +
				     ptex_face_inner_resolution;
				e1 = start_edge_index +
				         coarse_poly->totloop *
				                 (num_inner_edges_per_ptex_face +
				                  ptex_face_inner_resolution + 1) +
				         prev_corner * (2 * ptex_face_inner_resolution + 1) +
				         ptex_face_inner_resolution;
				e0 = start_edge_index +
				      coarse_poly->totloop * num_inner_edges_per_ptex_face +
				      prev_corner * ptex_face_inner_resolution;
			}
			else {
				v1 = v0 + ptex_face_inner_resolution + 1;
				e0 = corner_edge_index + ptex_face_inner_resolution +
				         i * (2 * ptex_face_inner_resolution + 1);
				e1 = e3 + 1;
			}
			int v2 = flip_prev ? v3 - 1 : v3 + 1;
			int e2;
			if (flip_prev) {
				e2 = ctx->edge_boundary_offset +
				         prev_coarse_loop->e *
				                 num_subdiv_edges_per_coarse_edge +
				         num_subdiv_edges_per_coarse_edge - 2 - i;
			}
			else {
				e2 = ctx->edge_boundary_offset +
				         prev_coarse_loop->e *
				                 num_subdiv_edges_per_coarse_edge + 1 + i;
			}
			const float u = 0.0f;
			const float v = du * (i + 1);
			subdiv_create_loops_of_poly(
			        ctx, &loop_interpolation, subdiv_loop,
			        ptex_face_index + corner, 1,
			        v0, e0, v1, e1, v2, e2, v3, e3,
			        u, v, du, dv);
			v0 = v1;
			v3 = v2;
			e3 = e1;
		}
		prev_coarse_loop = coarse_loop;
	}
	loop_interpolation_end(&loop_interpolation);
}

static void subdiv_create_loops(SubdivMeshContext *ctx, int poly_index)
{
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MPoly *coarse_mpoly = coarse_mesh->mpoly;
	const MPoly *coarse_poly = &coarse_mpoly[poly_index];
	if (coarse_poly->totloop == 4) {
		subdiv_create_loops_regular(ctx, coarse_poly);
	}
	else {
		subdiv_create_loops_special(ctx, coarse_poly);
	}
}

/* =============================================================================
 * Polygons subdivision process.
 */

static void subdiv_copy_poly_data(const SubdivMeshContext *ctx,
                                  MPoly *subdiv_poly,
                                  const MPoly *coarse_poly)
{
	const int coarse_poly_index = coarse_poly - ctx->coarse_mesh->mpoly;
	const int subdiv_poly_index = subdiv_poly - ctx->subdiv_mesh->mpoly;
	CustomData_copy_data(&ctx->coarse_mesh->pdata,
	                     &ctx->subdiv_mesh->pdata,
	                     coarse_poly_index,
	                     subdiv_poly_index,
	                     1);
}

static void subdiv_create_polys(SubdivMeshContext *ctx, int poly_index)
{
	const int resolution = ctx->settings->resolution;
	const int start_poly_index = ctx->subdiv_polygon_offset[poly_index];
	/* Base/coarse mesh information. */
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MPoly *coarse_mpoly = coarse_mesh->mpoly;
	const MPoly *coarse_poly = &coarse_mpoly[poly_index];
	const int num_ptex_faces_per_poly =
	        num_ptex_faces_per_poly_get(coarse_poly);
	const int ptex_resolution =
	        ptex_face_resolution_get(coarse_poly, resolution);
	const int num_polys_per_ptex = num_polys_per_ptex_get(ptex_resolution);
	const int num_loops_per_ptex = 4 * num_polys_per_ptex;
	const int start_loop_index = 4 * start_poly_index;
	/* Hi-poly subdivided mesh. */
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MPoly *subdiv_mpoly = subdiv_mesh->mpoly;
	MPoly *subdiv_mp = &subdiv_mpoly[start_poly_index];
	for (int ptex_of_poly_index = 0;
	     ptex_of_poly_index < num_ptex_faces_per_poly;
	     ptex_of_poly_index++)
	{
		for (int subdiv_poly_index = 0;
		     subdiv_poly_index < num_polys_per_ptex;
		     subdiv_poly_index++, subdiv_mp++)
		{
			subdiv_copy_poly_data(ctx, subdiv_mp, coarse_poly);
			subdiv_mp->loopstart = start_loop_index +
			                       (ptex_of_poly_index * num_loops_per_ptex) +
			                       (subdiv_poly_index * 4);
			subdiv_mp->totloop = 4;
		}
	}
}

/* =============================================================================
 * Loose elements subdivision process.
 */

static void subdiv_create_loose_vertices_task(
        void *__restrict userdata,
        const int vertex_index,
        const ParallelRangeTLS *__restrict UNUSED(tls))
{
	SubdivMeshContext *ctx = userdata;
	if (BLI_BITMAP_TEST_BOOL(ctx->coarse_vertices_used_map, vertex_index)) {
		/* Vertex is not loose, was handled when handling polygons. */
		return;
	}
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MVert *coarse_mvert = coarse_mesh->mvert;
	const MVert *coarse_vertex = &coarse_mvert[vertex_index];
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MVert *subdiv_mvert = subdiv_mesh->mvert;
	MVert *subdiv_vertex = &subdiv_mvert[
	        ctx->vertices_corner_offset + vertex_index];
	subdiv_vertex_data_copy(ctx, coarse_vertex, subdiv_vertex);
}

/* Get neighbor edges of the given one.
 * - neighbors[0] is an edge adjacent to edge->v1.
 * - neighbors[1] is an edge adjacent to edge->v1.
 */
static void find_edge_neighbors(const SubdivMeshContext *ctx,
                                const MEdge *edge,
                                const MEdge *neighbors[2])
{
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MEdge *coarse_medge = coarse_mesh->medge;
	neighbors[0] = NULL;
	neighbors[1] = NULL;
	for (int edge_index = 0; edge_index < coarse_mesh->totedge; edge_index++) {
		if (BLI_BITMAP_TEST_BOOL(ctx->coarse_edges_used_map, edge_index)) {
			continue;
		}
		const MEdge *current_edge = &coarse_medge[edge_index];
		if (current_edge == edge) {
			continue;
		}
		if (ELEM(edge->v1, current_edge->v1, current_edge->v2)) {
			neighbors[0] = current_edge;
		}
		if (ELEM(edge->v2, current_edge->v1, current_edge->v2)) {
			neighbors[1] = current_edge;
		}
	}
}

static void points_for_loose_edges_interpolation_get(
        SubdivMeshContext *ctx,
        const MEdge *coarse_edge,
        const MEdge *neighbors[2],
        float points_r[4][3])
{
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MVert *coarse_mvert = coarse_mesh->mvert;
	/* Middle points corresponds to the edge. */
	copy_v3_v3(points_r[1], coarse_mvert[coarse_edge->v1].co);
	copy_v3_v3(points_r[2], coarse_mvert[coarse_edge->v2].co);
	/* Start point, duplicate from edge start if no neighbor. */
	if (neighbors[0] != NULL) {
		if (neighbors[0]->v1 == coarse_edge->v1) {
			copy_v3_v3(points_r[0], coarse_mvert[neighbors[0]->v2].co);
		}
		else {
			copy_v3_v3(points_r[0], coarse_mvert[neighbors[0]->v1].co);
		}
	}
	else {
		sub_v3_v3v3(points_r[0], points_r[1], points_r[2]);
		add_v3_v3(points_r[0], points_r[1]);
	}
	/* End point, duplicate from edge end if no neighbor. */
	if (neighbors[1] != NULL) {
		if (neighbors[1]->v1 == coarse_edge->v2) {
			copy_v3_v3(points_r[3], coarse_mvert[neighbors[1]->v2].co);
		}
		else {
			copy_v3_v3(points_r[3], coarse_mvert[neighbors[1]->v1].co);
		}
	}
	else {
		sub_v3_v3v3(points_r[3], points_r[2], points_r[1]);
		add_v3_v3(points_r[3], points_r[2]);
	}
}

static void subdiv_create_vertices_of_loose_edges_task(
        void *__restrict userdata,
        const int edge_index,
        const ParallelRangeTLS *__restrict UNUSED(tls))
{
	SubdivMeshContext *ctx = userdata;
	if (BLI_BITMAP_TEST_BOOL(ctx->coarse_edges_used_map, edge_index)) {
		/* Vertex is not loose, was handled when handling polygons. */
		return;
	}
	const int resolution = ctx->settings->resolution;
	const int resolution_1 = resolution - 1;
	const float inv_resolution_1 = 1.0f / (float)resolution_1;
	const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MEdge *coarse_edge = &coarse_mesh->medge[edge_index];
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MVert *subdiv_mvert = subdiv_mesh->mvert;
	/* Find neighbors of the current loose edge. */
	const MEdge *neighbors[2];
	find_edge_neighbors(ctx, coarse_edge, neighbors);
	/* Get points for b-spline interpolation. */
	float points[4][3];
	points_for_loose_edges_interpolation_get(
	        ctx, coarse_edge, neighbors, points);
	/* Subdivion verticies which corresponds to edge's v1 and v2. */
	MVert *subdiv_v1 = &subdiv_mvert[
	        ctx->vertices_corner_offset + coarse_edge->v1];
	MVert *subdiv_v2 = &subdiv_mvert[
	        ctx->vertices_corner_offset + coarse_edge->v2];
	/* First subdivided inner vertex of the edge.  */
	MVert *subdiv_start_vertex = &subdiv_mvert[
	        ctx->vertices_edge_offset +
	        edge_index * num_subdiv_vertices_per_coarse_edge];
	/* Perform interpolation. */
	for (int i = 0; i < resolution; i++) {
		const float u = i * inv_resolution_1;
		float weights[4];
		key_curve_position_weights(u, weights, KEY_BSPLINE);

		MVert *subdiv_vertex;
		if (i == 0) {
			subdiv_vertex = subdiv_v1;
		}
		else if (i == resolution - 1) {
			subdiv_vertex = subdiv_v2;
		}
		else {
			subdiv_vertex = &subdiv_start_vertex[i - 1];
		}
		interp_v3_v3v3v3v3(subdiv_vertex->co,
		                   points[0],
		                   points[1],
		                   points[2],
		                   points[3],
		                   weights);
		/* Reset flags and such. */
		subdiv_vertex->flag = 0;
		subdiv_vertex->bweight = 0.0f;
		/* Reset normal. */
		subdiv_vertex->no[0] = 0.0f;
		subdiv_vertex->no[1] = 0.0f;
		subdiv_vertex->no[2] = 1.0f;
	}
}

/* =============================================================================
 * Subdivision process entry points.
 */

static void subdiv_eval_task(
        void *__restrict userdata,
        const int poly_index,
        const ParallelRangeTLS *__restrict UNUSED(tls))
{
	SubdivMeshContext *ctx = userdata;
	/* Evaluate hi-poly vertex coordinates and normals. */
	subdiv_evaluate_vertices(ctx, poly_index);
	/* Create mesh geometry for the given base poly index. */
	subdiv_create_edges(ctx, poly_index);
	subdiv_create_loops(ctx, poly_index);
	subdiv_create_polys(ctx, poly_index);
}

static void subdiv_create_boundary_edges_task(
        void *__restrict userdata,
        const int edge_index,
        const ParallelRangeTLS *__restrict UNUSED(tls))
{
	SubdivMeshContext *ctx = userdata;
	subdiv_create_boundary_edges(ctx, edge_index);
}

Mesh *BKE_subdiv_to_mesh(
        Subdiv *subdiv,
        const SubdivToMeshSettings *settings,
        const Mesh *coarse_mesh)
{
	BKE_subdiv_stats_begin(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH);
	/* Make sure evaluator is up to date with possible new topology, and that
	 * is is refined for the new positions of coarse vertices.
	 */
	if (!BKE_subdiv_eval_update_from_mesh(subdiv, coarse_mesh)) {
		/* This could happen in two situations:
		 * - OpenSubdiv is disabled.
		 * - Something totally bad happened, and OpenSubdiv rejected our
		 *   topology.
		 * In either way, we can't safely continue.
		 */
		if (coarse_mesh->totpoly) {
			BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH);
			return NULL;
		}
	}
	SubdivMeshContext ctx = {0};
	ctx.coarse_mesh = coarse_mesh;
	ctx.subdiv = subdiv;
	ctx.settings = settings;
	subdiv_mesh_ctx_init(&ctx);
	Mesh *result = BKE_mesh_new_nomain_from_template(
	        coarse_mesh,
	        ctx.num_subdiv_vertices,
	        ctx.num_subdiv_edges,
	        0,
	        ctx.num_subdiv_loops,
	        ctx.num_subdiv_polygons);
	ctx.subdiv_mesh = result;
	subdiv_mesh_ctx_init_result(&ctx);
	/* Multi-threaded evaluation. */
	ParallelRangeSettings parallel_range_settings;
	BKE_subdiv_stats_begin(&subdiv->stats,
	                       SUBDIV_STATS_SUBDIV_TO_MESH_GEOMETRY);
	BLI_parallel_range_settings_defaults(&parallel_range_settings);
	BLI_task_parallel_range(0, coarse_mesh->totpoly,
	                        &ctx,
	                        subdiv_eval_task,
	                        &parallel_range_settings);
	BLI_task_parallel_range(0, coarse_mesh->totvert,
	                        &ctx,
	                        subdiv_create_loose_vertices_task,
	                        &parallel_range_settings);
	BLI_task_parallel_range(0, coarse_mesh->totedge,
	                        &ctx,
	                        subdiv_create_vertices_of_loose_edges_task,
	                        &parallel_range_settings);
	BLI_task_parallel_range(0, coarse_mesh->totedge,
	                        &ctx,
	                        subdiv_create_boundary_edges_task,
	                        &parallel_range_settings);
	subdiv_mesh_ctx_free(&ctx);
	BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH_GEOMETRY);
	// BKE_mesh_validate(result, true, true);
	BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH);
	return result;
}
