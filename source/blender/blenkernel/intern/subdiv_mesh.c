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

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_alloca.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"

#include "BKE_mesh.h"

/* TODO(sergey): Somehow move this to subdiv code? */
static int mpoly_ptex_faces_count_get(const MPoly *mp)
{
	if (mp->totloop == 4) {
		return 1;
	}
	else {
		return mp->totloop;
	}
}

static int num_edges_per_ptex_get(const int resolution)
{
	return 2 * (resolution - 1) * resolution;
}

static int num_polys_per_ptex_get(const int resolution)
{
	return (resolution - 1) * (resolution - 1);
}

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
} SubdivMeshContext;

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
        const int ptex_face_index)
{
	const MLoop *coarse_mloop = ctx->coarse_mesh->mloop;
	const int first_ptex_loop_index = coarse_poly->loopstart + ptex_face_index;
	/* Loop which look in the (opposite) V direction of the current
	 * ptex face.
	 *
	 * TOOD(sergey): Get rid of using module on every iteration.
	 */
	const int last_ptex_loop_index =
	        coarse_poly->loopstart +
	        (ptex_face_index + coarse_poly->totloop - 1) % coarse_poly->totloop;
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

typedef struct EdgesOfPtex {
	/* First edge of the ptex, starts at ptex (0, 0) and goes in u direction. */
	const MEdge *first_edge;
	/* Last edge of the ptex, starts at ptex (0, 0) and goes in v direction. */
	const MEdge *last_edge;
	/* For quad coarse faces only. */
	const MEdge *second_edge;
	const MEdge *third_edge;
} EdgesOfPtex;

static void edges_of_ptex_get(
        const SubdivMeshContext *ctx,
        EdgesOfPtex *edges_of_ptex,
        const MPoly *coarse_poly,
        const int ptex_face_index)
{
	const MEdge *coarse_medge = ctx->coarse_mesh->medge;
	LoopsOfPtex loops_of_ptex;
	loops_of_ptex_get(ctx, &loops_of_ptex, coarse_poly, ptex_face_index);
	edges_of_ptex->first_edge = &coarse_medge[loops_of_ptex.first_loop->e];
	edges_of_ptex->last_edge = &coarse_medge[loops_of_ptex.last_loop->e];
	if (coarse_poly->totloop == 4) {
		edges_of_ptex->second_edge =
		        &coarse_medge[loops_of_ptex.second_loop->e];
		edges_of_ptex->third_edge =
		        &coarse_medge[loops_of_ptex.third_loop->e];
	}
	else {
		edges_of_ptex->second_edge = NULL;
		edges_of_ptex->third_edge = NULL;
	}
}

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
			indices[i] = coarse_poly->loopstart + i;
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
        const int ptex_face_index)
{
	if (coarse_poly->totloop == 4) {
		/* Nothing to do, all indices and data is already assigned. */
	}
	else {
		const CustomData *vertex_data = &ctx->coarse_mesh->vdata;
		const Mesh *coarse_mesh = ctx->coarse_mesh;
		const MLoop *coarse_mloop = coarse_mesh->mloop;
		LoopsOfPtex loops_of_ptex;
		loops_of_ptex_get(ctx, &loops_of_ptex, coarse_poly, ptex_face_index);
		/* Ptex face corner corresponds to a poly loop with same index. */
		CustomData_copy_data(
		        vertex_data,
		        &vertex_interpolation->vertex_data_storage,
		        coarse_mloop[coarse_poly->loopstart + ptex_face_index].v,
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
		        coarse_mloop[loops_of_ptex.first_loop - coarse_mloop].v,
		        coarse_mloop[(loops_of_ptex.first_loop + 1 - coarse_mloop) %
		                coarse_poly->totloop].v};
		const int last_indices[2] = {
		        coarse_mloop[loops_of_ptex.last_loop - coarse_mloop].v,
		        coarse_mloop[loops_of_ptex.first_loop - coarse_mloop].v};
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

static void subdiv_copy_vertex_data(
        const SubdivMeshContext *ctx,
        MVert *subdiv_vertex,
        const Mesh *coarse_mesh,
        const MPoly *coarse_poly,
        const VerticesForInterpolation *vertex_interpolation,
        const int ptex_of_poly_index,
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
		if (coarse_poly->totloop == 4) {
			if (u == 0.0f && v == 0.0f) {
				ctx->vert_origindex[subdiv_vertex_index] =
				        vertex_interpolation->vertex_indices[0];
			}
			else if (u == 1.0f && v == 0.0f) {
				ctx->vert_origindex[subdiv_vertex_index] =
				        vertex_interpolation->vertex_indices[1];
			}
			else if (u == 1.0f && v == 1.0f) {
				ctx->vert_origindex[subdiv_vertex_index] =
				        vertex_interpolation->vertex_indices[2];
			}
			else if (u == 0.0f && v == 1.0f) {
				ctx->vert_origindex[subdiv_vertex_index] =
				        vertex_interpolation->vertex_indices[3];
			}
		} else {
			if (u == 0.0f && v == 0.0f) {
				const MLoop *coarse_mloop = coarse_mesh->mloop;
				ctx->vert_origindex[subdiv_vertex_index] =
				        coarse_mloop[coarse_poly->loopstart +
				                             ptex_of_poly_index].v;
			}
		}
	}
}

static void subdiv_evaluate_vertices(SubdivMeshContext *ctx,
                                     const int poly_index)
{
	Subdiv *subdiv = ctx->subdiv;
	const int resolution = ctx->settings->resolution;
	const int resolution2 = resolution * resolution;
	const float inv_resolution_1 = 1.0f / (float)(resolution - 1);
	/* Base/coarse mesh information. */
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MPoly *coarse_polyoly = coarse_mesh->mpoly;
	const MPoly *coarse_poly = &coarse_polyoly[poly_index];
	const int num_poly_ptex_faces = mpoly_ptex_faces_count_get(coarse_poly);
	/* Hi-poly subdivided mesh. */
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MVert *subdiv_vertex = subdiv_mesh->mvert;
	const int ptex_face_index = subdiv->face_ptex_offset[poly_index];
	/* Actual evaluation. */
	VerticesForInterpolation vertex_interpolation;
	vertex_interpolation_init(ctx, &vertex_interpolation, coarse_poly);
	MVert *subdiv_vert = &subdiv_vertex[ptex_face_index * resolution2];
	for (int ptex_of_poly_index = 0;
	     ptex_of_poly_index < num_poly_ptex_faces;
	     ptex_of_poly_index++)
	{
		vertex_interpolation_from_ptex(ctx,
		                               &vertex_interpolation,
		                               coarse_poly,
		                               ptex_of_poly_index);
		const int current_ptex_face_index =
		        ptex_face_index + ptex_of_poly_index;
		BKE_subdiv_eval_limit_patch_resolution_point_and_short_normal(
		        subdiv,
		        current_ptex_face_index,
		        resolution,
		        subdiv_vert, offsetof(MVert, co), sizeof(MVert),
		        subdiv_vert, offsetof(MVert, no), sizeof(MVert));
		for (int y = 0; y < resolution; y++) {
			const float v = y * inv_resolution_1;
			for (int x = 0; x < resolution; x++, subdiv_vert++) {
				const float u = x * inv_resolution_1;
				subdiv_copy_vertex_data(ctx,
				                        subdiv_vert,
				                        coarse_mesh,
				                        coarse_poly,
				                        &vertex_interpolation,
				                        ptex_of_poly_index,
				                        u, v);
			}
		}
	}
	vertex_interpolation_end(&vertex_interpolation);
}

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
	if (ctx->edge_origindex != NULL) {
		ctx->edge_origindex[subdiv_edge_index] = coarse_edge_index;
	}
}

static MEdge *subdiv_create_edges_row(SubdivMeshContext *ctx,
                                      MEdge *subdiv_edge,
                                      const MEdge *coarse_edge,
                                      const int start_vertex_index,
                                      const int resolution)
{
	int vertex_index = start_vertex_index;
	for (int edge_index = 0;
	     edge_index < resolution - 1;
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
                                         const int resolution)
{
	int vertex_index = start_vertex_index;
	for (int edge_index = 0;
	     edge_index < resolution;
	     edge_index++, subdiv_edge++)
	{
		const MEdge *coarse_edge = NULL;
		if (edge_index == 0) {
			coarse_edge = coarse_start_edge;
		}
		else if (edge_index == resolution - 1) {
			coarse_edge = coarse_end_edge;
		}
		subdiv_copy_edge_data(ctx, subdiv_edge, coarse_edge);
		subdiv_edge->v1 = vertex_index;
		subdiv_edge->v2 = vertex_index + resolution;
		vertex_index += 1;
	}
	return subdiv_edge;
}

static void subdiv_create_edges(SubdivMeshContext *ctx, int poly_index)
{
	Subdiv *subdiv = ctx->subdiv;
	const int resolution = ctx->settings->resolution;
	const int resolution2 = resolution * resolution;
	const int ptex_face_index = subdiv->face_ptex_offset[poly_index];
	const int num_edges_per_ptex = num_edges_per_ptex_get(resolution);
	const int start_edge_index = ptex_face_index * num_edges_per_ptex;
	/* Base/coarse mesh information. */
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MPoly *coarse_polyoly = coarse_mesh->mpoly;
	const MPoly *coarse_poly = &coarse_polyoly[poly_index];
	const int num_poly_ptex_faces = mpoly_ptex_faces_count_get(coarse_poly);
	/* Hi-poly subdivided mesh. */
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MEdge *subdiv_medge = subdiv_mesh->medge;
	MEdge *subdiv_edge = &subdiv_medge[start_edge_index];
	const int start_poly_vertex_index = ptex_face_index * resolution2;
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
	for (int i = 0; i < num_poly_ptex_faces; i++) {
		const int start_ptex_face_vertex_index =
		        start_poly_vertex_index + i * resolution2;
		EdgesOfPtex edges_of_ptex;
		edges_of_ptex_get(ctx, &edges_of_ptex, coarse_poly, i);
		/* Create bottom row of edges (0-1, 1-2). */
		subdiv_edge = subdiv_create_edges_row(
		        ctx,
		        subdiv_edge,
		        edges_of_ptex.first_edge,
		        start_ptex_face_vertex_index,
		        resolution);
		/* Create remaining edges. */
		for (int row = 0; row < resolution - 1; row++) {
			const int start_row_vertex_index =
			    start_ptex_face_vertex_index + row * resolution;
			/* Create vertical columns.
			 *
			 * At first iteration it will be edges (0-3. 1-4, 2-5), then it
			 * will be (3-6, 4-7, 5-8) and so on.
			 */
			subdiv_edge = subdiv_create_edges_column(
			        ctx,
			        subdiv_edge,
			        edges_of_ptex.last_edge,
			        edges_of_ptex.second_edge,
			        start_row_vertex_index,
			        resolution);
			/* Create horizontal edge row.
			 *
			 * At first iteration it will be edges (3-4, 4-5), then it will be
			 * (6-7, 7-8) and so on.
			 */
			subdiv_edge = subdiv_create_edges_row(
			        ctx,
			        subdiv_edge,
			        (row == resolution - 2) ? edges_of_ptex.third_edge
			                                : NULL,
			        start_row_vertex_index + resolution,
			        resolution);
		}
	}
}

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
                                 const float inv_resolution_1)
{
	if (ctx->num_uv_layers == 0) {
		return;
	}
	Subdiv *subdiv = ctx->subdiv;
	const int mloop_index = subdiv_loop - ctx->subdiv_mesh->mloop;
	const float du = inv_resolution_1;
	const float dv = inv_resolution_1;
	for (int layer_index = 0; layer_index < ctx->num_uv_layers; layer_index++) {
		MLoopUV *subdiv_loopuv = &ctx->uv_layers[layer_index][mloop_index];
		BKE_subdiv_eval_face_varying(subdiv,
		                             ptex_face_index,
		                             u, v,
		                             subdiv_loopuv[0].uv);
		BKE_subdiv_eval_face_varying(subdiv,
		                             ptex_face_index,
		                             u + du, v,
		                             subdiv_loopuv[1].uv);
		BKE_subdiv_eval_face_varying(subdiv,
		                             ptex_face_index,
		                             u + du, v + dv,
		                             subdiv_loopuv[2].uv);
		BKE_subdiv_eval_face_varying(subdiv,
		                             ptex_face_index,
		                             u, v + dv,
		                             subdiv_loopuv[3].uv);
		/* TODO(sergey): Currently evaluator only has single UV layer, so can
		 * not evaluate more than that. Need to be solved.
		 */
		break;
	}
}

static void subdiv_create_loops(SubdivMeshContext *ctx, int poly_index)
{
	Subdiv *subdiv = ctx->subdiv;
	const int resolution = ctx->settings->resolution;
	const int resolution2 = resolution * resolution;
	const float inv_resolution_1 = 1.0f / (float)(resolution - 1);
	const int ptex_face_index = subdiv->face_ptex_offset[poly_index];
	const int num_edges_per_ptex = num_edges_per_ptex_get(resolution);
	const int start_edge_index = ptex_face_index * num_edges_per_ptex;
	const int num_polys_per_ptex = num_polys_per_ptex_get(resolution);
	const int start_poly_index = ptex_face_index * num_polys_per_ptex;
	const int start_loop_index = 4 * start_poly_index;
	const int start_vert_index = ptex_face_index * resolution2;
	const float du = inv_resolution_1;
	const float dv = inv_resolution_1;
	/* Base/coarse mesh information. */
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MPoly *coarse_polyoly = coarse_mesh->mpoly;
	const MPoly *coarse_poly = &coarse_polyoly[poly_index];
	const int num_poly_ptex_faces = mpoly_ptex_faces_count_get(coarse_poly);
	/* Hi-poly subdivided mesh. */
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MLoop *subdiv_loopoop = subdiv_mesh->mloop;
	MLoop *subdiv_loop = &subdiv_loopoop[start_loop_index];
	LoopsForInterpolation loop_interpolation;
	loop_interpolation_init(ctx, &loop_interpolation, coarse_poly);
	for (int ptex_of_poly_index = 0;
	     ptex_of_poly_index < num_poly_ptex_faces;
	     ptex_of_poly_index++)
	{
		loop_interpolation_from_ptex(ctx,
		                             &loop_interpolation,
		                             coarse_poly,
		                             ptex_of_poly_index);
		const int current_ptex_face_index =
		        ptex_face_index + ptex_of_poly_index;
		for (int y = 0; y < resolution - 1; y++) {
			const float v = y * inv_resolution_1;
			for (int x = 0; x < resolution - 1; x++, subdiv_loop += 4) {
				const float u = x * inv_resolution_1;
				/* Vertex indicies ordered counter-clockwise. */
				const int v0 = start_vert_index +
				               (ptex_of_poly_index * resolution2) +
				               (y * resolution + x);
				const int v1 = v0 + 1;
				const int v2 = v0 + resolution + 1;
				const int v3 = v0 + resolution;
				/* Edge indicies ordered counter-clockwise. */
				const int e0 = start_edge_index +
				               (ptex_of_poly_index * num_edges_per_ptex) +
				               (y * (2 * resolution - 1) + x);
				const int e1 = e0 + resolution;
				const int e2 = e0 + (2 * resolution - 1);
				const int e3 = e0 + resolution - 1;
				/* Initialize 4 loops of corresponding hi-poly poly. */
				/* TODO(sergey): For ptex boundaries we should use loops from
				 * coarse mesh.
				 */
				subdiv_copy_loop_data(ctx,
				                      &subdiv_loop[0],
				                      &loop_interpolation,
				                      u, v);
				subdiv_loop[0].v = v0;
				subdiv_loop[0].e = e0;
				subdiv_copy_loop_data(ctx,
				                      &subdiv_loop[1],
				                      &loop_interpolation,
				                      u + du, v);
				subdiv_loop[1].v = v1;
				subdiv_loop[1].e = e1;
				subdiv_copy_loop_data(ctx,
				                      &subdiv_loop[2],
				                      &loop_interpolation,
				                      u + du, v + dv);
				subdiv_loop[2].v = v2;
				subdiv_loop[2].e = e2;
				subdiv_copy_loop_data(ctx,
				                      &subdiv_loop[3],
				                      &loop_interpolation,
				                      u, v + dv);
				subdiv_loop[3].v = v3;
				subdiv_loop[3].e = e3;
				/* Interpolate UV layers using OpenSubdiv. */
				subdiv_eval_uv_layer(ctx,
				                     subdiv_loop,
				                     current_ptex_face_index,
				                     u, v,
				                     inv_resolution_1);
			}
		}
	}
	loop_interpolation_end(&loop_interpolation);
}

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
	if (ctx->poly_origindex != NULL) {
		ctx->poly_origindex[subdiv_poly_index] = coarse_poly_index;
	}
}

static void subdiv_create_polys(SubdivMeshContext *ctx, int poly_index)
{
	Subdiv *subdiv = ctx->subdiv;
	const int resolution = ctx->settings->resolution;
	const int ptex_face_index = subdiv->face_ptex_offset[poly_index];
	const int num_polys_per_ptex = num_polys_per_ptex_get(resolution);
	const int num_loops_per_ptex = 4 * num_polys_per_ptex;
	const int start_poly_index = ptex_face_index * num_polys_per_ptex;
	const int start_loop_index = 4 * start_poly_index;
	/* Base/coarse mesh information. */
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MPoly *coarse_polyoly = coarse_mesh->mpoly;
	const MPoly *coarse_poly = &coarse_polyoly[poly_index];
	const int num_poly_ptex_faces = mpoly_ptex_faces_count_get(coarse_poly);
	/* Hi-poly subdivided mesh. */
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	MPoly *subdiv_mpoly = subdiv_mesh->mpoly;
	MPoly *subdiv_mp = &subdiv_mpoly[start_poly_index];
	for (int ptex_of_poly_index = 0;
	     ptex_of_poly_index < num_poly_ptex_faces;
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

static void subdiv_eval_task(
        void *__restrict userdata,
        const int poly_index,
        const ParallelRangeTLS *__restrict UNUSED(tls))
{
	SubdivMeshContext *data = userdata;
	/* Evaluate hi-poly vertex coordinates and normals. */
	subdiv_evaluate_vertices(data, poly_index);
	/* Create mesh geometry for the given base poly index. */
	subdiv_create_edges(data, poly_index);
	subdiv_create_loops(data, poly_index);
	subdiv_create_polys(data, poly_index);
}

static void cache_uv_layers(SubdivMeshContext *ctx)
{
	Mesh *subdiv_mesh = ctx->subdiv_mesh;
	ctx->num_uv_layers =
	        CustomData_number_of_layers(&subdiv_mesh->ldata, CD_MLOOPUV);
	for (int layer_index = 0; layer_index < ctx->num_uv_layers; ++layer_index) {
		ctx->uv_layers[layer_index] = CustomData_get_layer_n(
		        &subdiv_mesh->ldata, CD_MLOOPUV, layer_index);
	}
}

static void cache_custom_data_layers(SubdivMeshContext *ctx)
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
	cache_uv_layers(ctx);
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
	BKE_subdiv_eval_update_from_mesh(subdiv, coarse_mesh);
	const int resolution = settings->resolution;
	const int resolution2 = resolution * resolution;
	const int num_result_verts = subdiv->num_ptex_faces * resolution2;
	const int num_result_edges =
			subdiv->num_ptex_faces * num_edges_per_ptex_get(resolution);
	const int num_result_polys =
			subdiv->num_ptex_faces * num_polys_per_ptex_get(resolution);
	const int num_result_loops = 4 * num_result_polys;
	/* Create mesh and its arrays. */
	Mesh *result = BKE_mesh_new_nomain_from_template(
	        coarse_mesh,
	        num_result_verts,
	        num_result_edges,
	        0,
	        num_result_loops,
	        num_result_polys);
	/* Evaluate subdivisions of base faces in threads. */
	SubdivMeshContext ctx;
	ctx.coarse_mesh = coarse_mesh;
	ctx.subdiv = subdiv;
	ctx.subdiv_mesh = result;
	ctx.settings = settings;
	cache_custom_data_layers(&ctx);
	/* Multi-threaded evaluation. */
	ParallelRangeSettings parallel_range_settings;
	BLI_parallel_range_settings_defaults(&parallel_range_settings);
	BLI_task_parallel_range(0, coarse_mesh->totpoly,
	                        &ctx,
	                        subdiv_eval_task,
	                        &parallel_range_settings);
	BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH);
	return result;
}
