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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/multires_reshape.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"

#include "BKE_ccg.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subdiv_eval.h"
#include "BKE_subdiv_foreach.h"
#include "BKE_subdiv_mesh.h"

#include "DEG_depsgraph_query.h"

/* TODO(sergey): De-duplicate with subdiv_displacement_multires.c. */

/* Coordinates within grid has different convention from PTex coordinates.
 * This function converts the latter ones to former.
 */
BLI_INLINE void ptex_uv_to_grid_uv(const float ptex_u, const float ptex_v,
                                   float *r_grid_u, float *r_grid_v)
{
	*r_grid_u = 1.0f - ptex_v;
	*r_grid_v = 1.0f - ptex_u;
}

/* Simplified version of mdisp_rot_face_to_crn, only handles quad and
 * works in normalized coordinates.
 *
 * NOTE: Output coordinates are in ptex coordinates.
 */
BLI_INLINE int rotate_quad_to_corner(const float u, const float v,
                                     float *r_u, float *r_v)
{
	int corner;
	if (u <= 0.5f && v <= 0.5f) {
		corner = 0;
		*r_u = 2.0f * u;
		*r_v = 2.0f * v;
	}
	else if (u > 0.5f  && v <= 0.5f) {
		corner = 1;
		*r_u = 2.0f * v;
		*r_v = 2.0f * (1.0f - u);
	}
	else if (u > 0.5f  && v > 0.5f) {
		corner = 2;
		*r_u = 2.0f * (1.0f - u);
		*r_v = 2.0f * (1.0f - v);
	}
	else if (u <= 0.5f && v >= 0.5f) {
		corner = 3;
		*r_u = 2.0f * (1.0f - v);
		*r_v = 2.0f * u;
	}
	else {
		BLI_assert(!"Unexpected corner configuration");
	}
	return corner;
}

BLI_INLINE void construct_tangent_matrix(float tangent_matrix[3][3],
                                         const float dPdu[3],
                                         const float dPdv[3],
                                         const int corner)
{
	if (corner == 0) {
		copy_v3_v3(tangent_matrix[0], dPdv);
		copy_v3_v3(tangent_matrix[1], dPdu);
		mul_v3_fl(tangent_matrix[0], -1.0f);
		mul_v3_fl(tangent_matrix[1], -1.0f);
	}
	else if (corner == 1) {
		copy_v3_v3(tangent_matrix[0], dPdu);
		copy_v3_v3(tangent_matrix[1], dPdv);
		mul_v3_fl(tangent_matrix[1], -1.0f);
	}
	else if (corner == 2) {
		copy_v3_v3(tangent_matrix[0], dPdv);
		copy_v3_v3(tangent_matrix[1], dPdu);
	}
	else if (corner == 3) {
		copy_v3_v3(tangent_matrix[0], dPdu);
		copy_v3_v3(tangent_matrix[1], dPdv);
		mul_v3_fl(tangent_matrix[0], -1.0f);
	}
	cross_v3_v3v3(tangent_matrix[2], dPdu, dPdv);
	normalize_v3(tangent_matrix[0]);
	normalize_v3(tangent_matrix[1]);
	normalize_v3(tangent_matrix[2]);
}

static void multires_reshape_init_mmd(
        MultiresModifierData *reshape_mmd,
        const MultiresModifierData *mmd)
{
	*reshape_mmd = *mmd;
}

static void multires_reshape_init_mmd_top_level(
        MultiresModifierData *reshape_mmd,
        const MultiresModifierData *mmd)
{
	*reshape_mmd = *mmd;
	reshape_mmd->lvl = reshape_mmd->totlvl;
}

/* =============================================================================
 * General reshape implementaiton, reused by all particular cases.
 */

typedef struct MultiresReshapeContext {
	Subdiv *subdiv;
	Object *object;
	const Mesh *coarse_mesh;
	MDisps *mdisps;
	GridPaintMask *grid_paint_mask;
	/* NOTE: This is a grid size on the top level, same for level. */
	int grid_size;
	int level;
} MultiresReshapeContext;

static void multires_reshape_allocate_displacement_grid(
        MDisps *displacement_grid, const int level)
{
	/* TODO(sergey): Use grid_size_for_level_get() somehow. */
	const int grid_size = (1 << (level - 1)) + 1;
	const int grid_area = grid_size * grid_size;
	float (*disps)[3] = MEM_calloc_arrayN(
	        grid_area, 3 * sizeof(float), "multires disps");
	displacement_grid->disps = disps;
	displacement_grid->totdisp = grid_area;
	displacement_grid->level = level;
}

static void multires_reshape_ensure_displacement_grid(
        MDisps *displacement_grid, const int level)
{
	if (displacement_grid->disps != NULL) {
		return;
	}
	multires_reshape_allocate_displacement_grid(
        displacement_grid, level);
}

static void multires_reshape_ensure_displacement_grids(
        MultiresReshapeContext *ctx)
{
	const int num_grids = ctx->coarse_mesh->totloop;
	const int grid_level = ctx->level;
	for (int grid_index = 0; grid_index < num_grids; grid_index++) {
		multires_reshape_ensure_displacement_grid(
		        &ctx->mdisps[grid_index], grid_level);
	}
}

static void multires_reshape_ensure_mask_grids(
        MultiresReshapeContext *ctx)
{
	if (ctx->grid_paint_mask == NULL) {
		return;
	}
	const int num_grids = ctx->coarse_mesh->totloop;
	const int grid_level = ctx->level;
	const int grid_area = ctx->grid_size * ctx->grid_size;
	for (int grid_index = 0; grid_index < num_grids; grid_index++) {
		GridPaintMask *grid_paint_mask = &ctx->grid_paint_mask[grid_index];
		if (grid_paint_mask->level == grid_level) {
			continue;
		}
		grid_paint_mask->level = grid_level;
		if (grid_paint_mask->data) {
			MEM_freeN(grid_paint_mask->data);
		}
		grid_paint_mask->data = MEM_calloc_arrayN(
		        grid_area, sizeof(float), "gpm.data");
	}
}

static void multires_reshape_ensure_grids(
        MultiresReshapeContext *ctx)
{
	multires_reshape_ensure_displacement_grids(ctx);
	multires_reshape_ensure_mask_grids(ctx);
}

static void multires_reshape_vertex_copy_to_next(
        MultiresReshapeContext *ctx,
        const MPoly *coarse_poly,
        const int current_corner,
        const MDisps *current_displacement_grid,
		const GridPaintMask *current_mask_grid,
        const int current_grid_x, const int current_grid_y)
{
	const int grid_size = ctx->grid_size;
	const int next_current_corner = (current_corner + 1) % coarse_poly->totloop;
	const int next_grid_x = 0;
	const int next_grid_y = current_grid_x;
	const int current_index = current_grid_y * grid_size + current_grid_x;
	const int next_index = next_grid_y * grid_size + next_grid_x;
	/* Copy displacement. */
	MDisps *next_displacement_grid = &ctx->mdisps[
	        coarse_poly->loopstart + next_current_corner];
	float *next_displacement = next_displacement_grid->disps[next_index];
	copy_v3_v3(next_displacement,
	           current_displacement_grid->disps[current_index]);
	SWAP(float, next_displacement[0], next_displacement[1]);
	next_displacement[0] = -next_displacement[0];
	/* Copy mask, if exists. */
	if (current_mask_grid != NULL) {
		GridPaintMask *next_mask_grid = &ctx->grid_paint_mask[
		        coarse_poly->loopstart + next_current_corner];
		next_mask_grid->data[next_index] =
		        current_mask_grid->data[current_index];
	}
}

static void multires_reshape_vertex_copy_to_prev(
        MultiresReshapeContext *ctx,
        const MPoly *coarse_poly,
        const int current_corner,
        const MDisps *current_displacement_grid,
        const GridPaintMask *current_mask_grid,
        const int current_grid_x, const int current_grid_y)
{
	const int grid_size = ctx->grid_size;
	const int prev_current_corner =
	        (current_corner - 1 + coarse_poly->totloop) % coarse_poly->totloop;
	const int prev_grid_x = current_grid_y;
	const int prev_grid_y = 0;
	const int current_index = current_grid_y * grid_size + current_grid_x;
	const int prev_index = prev_grid_y * grid_size + prev_grid_x;
	/* Copy displacement. */
	MDisps *prev_displacement_grid = &ctx->mdisps[
	        coarse_poly->loopstart + prev_current_corner];
	float *prev_displacement = prev_displacement_grid->disps[prev_index];
	copy_v3_v3(prev_displacement,
	           current_displacement_grid->disps[current_index]);
	SWAP(float, prev_displacement[0], prev_displacement[1]);
	prev_displacement[1] = -prev_displacement[1];
	/* Copy mask, if exists. */
	if (current_mask_grid != NULL) {
		GridPaintMask *prev_mask_grid = &ctx->grid_paint_mask[
		        coarse_poly->loopstart + prev_current_corner];
		prev_mask_grid->data[prev_index] =
		        current_mask_grid->data[current_index];
	}
}

static void copy_boundary_displacement(
        MultiresReshapeContext *ctx,
        const MPoly *coarse_poly,
        const int corner,
        const int grid_x, const int grid_y,
        const MDisps *displacement_grid,
        const GridPaintMask *mask_grid)
{
	if (grid_x == 0 && grid_y == 0) {
		for (int i = 0; i < coarse_poly->totloop; i++) {
			const int current_face_corner =
			        (corner + i) % coarse_poly->totloop;
			const int grid_index = coarse_poly->loopstart + current_face_corner;
			MDisps *current_displacement_grid = &ctx->mdisps[grid_index];
			GridPaintMask *current_mask_grid =
			        mask_grid != NULL ? &ctx->grid_paint_mask[grid_index]
			                          : NULL;
			multires_reshape_vertex_copy_to_next(
			        ctx,
			        coarse_poly,
			        current_face_corner,
			        current_displacement_grid,
			        current_mask_grid,
			        0, 0);
		}
	}
	else if (grid_x == 0) {
		multires_reshape_vertex_copy_to_prev(
		        ctx,
		        coarse_poly,
		        corner,
		        displacement_grid,
		        mask_grid,
		        grid_x, grid_y);
	}
	else if (grid_y == 0) {
		multires_reshape_vertex_copy_to_next(
		        ctx,
		        coarse_poly,
		        corner,
		        displacement_grid,
		        mask_grid,
		        grid_x, grid_y);
	}
}

static void multires_reshape_vertex_from_final_data(
        MultiresReshapeContext *ctx,
        const int ptex_face_index,
        const float u, const float v,
        const int coarse_poly_index,
        const int coarse_corner,
        const float final_P[3], const float final_mask)
{
	Subdiv *subdiv = ctx->subdiv;
	const int grid_size = ctx->grid_size;
	const Mesh *coarse_mesh = ctx->coarse_mesh;
	const MPoly *coarse_mpoly = coarse_mesh->mpoly;
	const MPoly *coarse_poly = &coarse_mpoly[coarse_poly_index];
	const int loop_index = coarse_poly->loopstart + coarse_corner;
	/* Evaluate limit surface. */
	float P[3], dPdu[3], dPdv[3];
	BKE_subdiv_eval_limit_point_and_derivatives(
	        subdiv, ptex_face_index, u, v, P, dPdu, dPdv);
	/* Get coordinate and corner configuration. */
	float grid_u, grid_v;
	MDisps *displacement_grid;
	GridPaintMask *grid_paint_mask = NULL;
	int face_corner = coarse_corner;
	int grid_corner = 0;
	int grid_index;
	if (coarse_poly->totloop == 4) {
		float corner_u, corner_v;
		face_corner = rotate_quad_to_corner(u, v, &corner_u, &corner_v);
		grid_corner = face_corner;
		grid_index = loop_index + face_corner;
		ptex_uv_to_grid_uv(corner_u, corner_v, &grid_u, &grid_v);
	}
	else {
		grid_index = loop_index;
		ptex_uv_to_grid_uv(u, v, &grid_u, &grid_v);
	}
	displacement_grid = &ctx->mdisps[grid_index];
	if (ctx->grid_paint_mask != NULL) {
		grid_paint_mask = &ctx->grid_paint_mask[grid_index];
		BLI_assert(grid_paint_mask->level == displacement_grid->level);
	}
	/* Convert object coordinate to a tangent space of displacement grid. */
	float D[3];
	sub_v3_v3v3(D, final_P, P);
	float tangent_matrix[3][3];
	construct_tangent_matrix(tangent_matrix, dPdu, dPdv, grid_corner);
	float inv_tangent_matrix[3][3];
	invert_m3_m3(inv_tangent_matrix, tangent_matrix);
	float tangent_D[3];
	mul_v3_m3v3(tangent_D, inv_tangent_matrix, D);
	/* Write tangent displacement. */
	const int grid_x = (grid_u * (grid_size - 1) + 0.5f);
	const int grid_y = (grid_v * (grid_size - 1) + 0.5f);
	const int index = grid_y * grid_size + grid_x;
	copy_v3_v3(displacement_grid->disps[index], tangent_D);
	/* Write mask grid. */
	if (grid_paint_mask != NULL) {
		grid_paint_mask->data[index] = final_mask;
	}
	/* Copy boundary to the next/previous grids */
	copy_boundary_displacement(
	        ctx, coarse_poly, face_corner, grid_x, grid_y,
	        displacement_grid, grid_paint_mask);
}

/* =============================================================================
 * Helpers to propagate displacement to higher levels.
 */

typedef struct MultiresPropagateData {
	int reshape_level;
	int top_level;
	int num_grids;
	int reshape_grid_size;
	int top_grid_size;
	MDisps *old_displacement_grids;
	MDisps *new_displacement_grids;
	GridPaintMask *grid_paint_mask;
} MultiresPropagateData;

typedef struct MultiresPropagateCornerData {
	float old_coord[3];
	float new_coord[3];
	float coord_delta[3];
	float mask;
} MultiresPropagateCornerData;

static void multires_reshape_propagate_prepare(
        MultiresPropagateData *data,
        Object *object,
        const int reshape_level,
        const int top_level)
{
	BLI_assert(reshape_level <= top_level);
	data->old_displacement_grids = NULL;
	if (reshape_level == top_level) {
		/* Nothing to do, reshape will happen on the whole grid conent. */
		return;
	}
	Mesh *coarse_mesh = object->data;
	const int num_grids = coarse_mesh->totloop;
	MDisps *mdisps = CustomData_get_layer(&coarse_mesh->ldata, CD_MDISPS);
	MDisps *old_mdisps = MEM_dupallocN(mdisps);
	for (int grid_index = 0; grid_index < num_grids; grid_index++) {
		MDisps *displacement_grid = &mdisps[grid_index];
		MDisps *old_displacement_grid = &old_mdisps[grid_index];
		old_displacement_grid->totdisp = displacement_grid->totdisp;
		old_displacement_grid->level = displacement_grid->level;
		if (displacement_grid->disps) {
			displacement_grid->disps = MEM_dupallocN(displacement_grid->disps);
		}
		else {
			old_displacement_grid->disps = NULL;
		}
		/* TODO(sergey): This might be needed for proper propagation. */
		old_displacement_grid->hidden = NULL;
	}
	data->reshape_level = reshape_level;
	data->top_level = top_level;
	data->num_grids = num_grids;
	/* TODO(sergey): use grid_size_for_level_get(). */
	data->reshape_grid_size = (1 << (reshape_level - 1)) + 1;
	data->top_grid_size = (1 << (top_level - 1)) + 1;
	data->old_displacement_grids = old_mdisps;
	data->new_displacement_grids = mdisps;
	data->grid_paint_mask =
	        CustomData_get_layer(&coarse_mesh->ldata, CD_GRID_PAINT_MASK);
}

static void multires_reshape_propagate_prepare_from_mmd(
        MultiresPropagateData *data,
        struct Depsgraph *depsgraph,
        Object *object,
        const MultiresModifierData *mmd,
        const bool use_render_params)
{
	Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
	const int level = multires_get_level(
	        scene_eval, object, mmd, use_render_params, true);
	multires_reshape_propagate_prepare(data, object, level, mmd->totlvl);
}

static void multires_reshape_propagate_corner_data(
        MultiresPropagateCornerData* corner,
        const MDisps *old_displacement_grid,
        const MDisps *new_displacement_grid,
        const GridPaintMask *grid_paint_mask,
        const int grid_size,
        const int grid_skip,
        const int reshape_x, const int reshape_y)
{
	const int x = reshape_x * grid_skip;
	const int y = reshape_y * grid_skip;
	const int grid_index = y * grid_size + x;
	if (old_displacement_grid->disps != NULL) {
		copy_v3_v3(corner->old_coord, old_displacement_grid->disps[grid_index]);
	}
	else {
		zero_v3(corner->old_coord);
	}
	copy_v3_v3(corner->new_coord, new_displacement_grid->disps[grid_index]);
	sub_v3_v3v3(corner->coord_delta, corner->new_coord, corner->old_coord);
	if (grid_paint_mask != NULL) {
		corner->mask = grid_paint_mask->data[grid_index];
	}
	else {
		corner->mask = 0.0f;
	}
}

static void multires_reshape_propagate_all_corners_data(
        MultiresPropagateCornerData corners[4],
        const MDisps *old_displacement_grid,
        const MDisps *new_displacement_grid,
		const GridPaintMask *grid_paint_mask,
        const int grid_size,
        const int grid_skip,
        const int reshape_x, const int reshape_y)
{
	int corner_index = 0;
	for (int dy = 0; dy <= 1; dy++) {
		for (int dx = 0; dx <= 1; dx++) {
			multires_reshape_propagate_corner_data(
			        &corners[corner_index],
			        old_displacement_grid,
			        new_displacement_grid,
			        grid_paint_mask,
			        grid_size,
			        grid_skip,
			        reshape_x + dx, reshape_y + dy);
			corner_index++;
		}
	}
}

static void multires_reshape_propagate_interpolate_coord(
        MDisps *new_displacement_grid,
        const MultiresPropagateCornerData corners[4],
        const float weights[4],
        const int x, const int y,
        const int grid_size)
{
	float delta[3];
	interp_v3_v3v3v3v3(
	        delta,
	        corners[0].coord_delta, corners[1].coord_delta,
	        corners[2].coord_delta, corners[3].coord_delta,
	        weights);
	const int index = y * grid_size + x;
	float *new_displacement = new_displacement_grid->disps[index];
	add_v3_v3(new_displacement, delta);
}

static void multires_reshape_propagate_interpolate_mask(
        GridPaintMask *grid_paint_mask,
        const MultiresPropagateCornerData corners[4],
        const float weights[4],
        const int x, const int y,
        const int grid_size)
{
	const int index = y * grid_size + x;
	grid_paint_mask->data[index] =
	        corners[0].mask * weights[0] +
	        corners[1].mask * weights[1] +
	        corners[2].mask * weights[2] +
	        corners[3].mask * weights[3];
}

static void multires_reshape_propagate_grid(
        MultiresPropagateData *data,
        const MDisps *old_displacement_grid,
        MDisps *new_displacement_grid,
        GridPaintMask *grid_paint_mask)
{
	const int reshape_grid_size = data->reshape_grid_size;
	const int top_grid_size = data->top_grid_size;
	const int grid_skip = (top_grid_size - 1) / (reshape_grid_size - 1);
	const float grid_skip_inv = 1.0f / (float)grid_skip;
	for (int reshape_y = 0;
	     reshape_y < reshape_grid_size - 1;
	     reshape_y++)
	{
		for (int reshape_x = 0;
		     reshape_x < reshape_grid_size - 1;
		     reshape_x++)
		{
			MultiresPropagateCornerData corners[4];
			multires_reshape_propagate_all_corners_data(
			        corners,
			        old_displacement_grid, new_displacement_grid,
			        grid_paint_mask,
			        top_grid_size,
			        grid_skip,
			        reshape_x, reshape_y);
			/* Propagate to higher levels. */
			for (int y = 0; y <= grid_skip; y++) {
				const float v = (float)y * grid_skip_inv;
				for (int x = 0; x <= grid_skip; x++) {
					/* Ignorevalues at the exact locations of grid which was
					 * reshape. Those points already have proper displacement.
					 */
					if ((x == 0 && y == 0) ||
					    (x == grid_skip && y == 0) ||
					    (x == grid_skip && y == grid_skip) ||
					    (x == 0 && y == grid_skip))
					{
						continue;
					}
					/* Ignore right-most column and top-most row, unless this
					 * is a boundary of the grid, to prevent displacement
					 * being affected twice.
					 */
					if (x == grid_skip && reshape_x != reshape_grid_size - 2) {
						continue;
					}
					if (y == grid_skip && reshape_y != reshape_grid_size - 2) {
						continue;
					}
					const float u = (float)x * grid_skip_inv;
					const int final_x = reshape_x * grid_skip + x;
					const int final_y = reshape_y * grid_skip + y;
					const float linear_weights[4] = {(1.0f - u) * (1.0f - v),
					                                 u * (1.0f - v),
					                                 (1.0f - u) * v,
					                                 u * v};
					multires_reshape_propagate_interpolate_coord(
					        new_displacement_grid,
					        corners,
					        linear_weights,
					        final_x, final_y,
					        top_grid_size);
					if (grid_paint_mask != NULL) {
						multires_reshape_propagate_interpolate_mask(
						        grid_paint_mask,
						        corners,
						        linear_weights,
						        final_x, final_y,
						        top_grid_size);
					}
				}
			}
		}
	}
}

static void multires_reshape_propagate(MultiresPropagateData *data) {
	if (data->old_displacement_grids == NULL) {
		return;
	}
	const int num_grids = data->num_grids;
	for (int grid_index = 0; grid_index < num_grids; grid_index++) {
		const MDisps *old_displacement_grid =
		        &data->old_displacement_grids[grid_index];
		MDisps *new_displacement_grid =
		        &data->new_displacement_grids[grid_index];
		if (old_displacement_grid->level != new_displacement_grid->level) {
			continue;
		}
		GridPaintMask *grid_paint_mask =
		        data->grid_paint_mask != NULL
		                ? &data->grid_paint_mask[grid_index]
		                : NULL;
		multires_reshape_propagate_grid(
		        data, old_displacement_grid,
		        new_displacement_grid,
		        grid_paint_mask);
	}
}

static void multires_reshape_propagate_free(MultiresPropagateData *data) {
	if (data->old_displacement_grids != NULL) {
		const int num_grids = data->num_grids;
		MDisps *old_mdisps = data->old_displacement_grids;
		for (int grid_index = 0; grid_index < num_grids; grid_index++) {
			MDisps *old_displacement_grid = &old_mdisps[grid_index];
			if (old_displacement_grid->disps) {
				MEM_freeN(old_displacement_grid->disps);
			}
		}
		MEM_freeN(data->old_displacement_grids);
	}
}

/* =============================================================================
 * Reshape from deformed vertex coordinates.
 */

typedef struct MultiresReshapeFromDeformedVertsContext {
	MultiresReshapeContext reshape_ctx;
	const float (*deformed_verts)[3];
	int num_deformed_verts;
} MultiresReshapeFromDeformedVertsContext;

static bool multires_reshape_topology_info(
        const SubdivForeachContext *foreach_context,
        const int num_vertices,
        const int UNUSED(num_edges),
        const int UNUSED(num_loops),
        const int UNUSED(num_polygons))
{
	MultiresReshapeFromDeformedVertsContext *ctx = foreach_context->user_data;
	if (num_vertices != ctx->num_deformed_verts) {
		return false;
	}
	return true;
}

static void multires_reshape_vertex(
        MultiresReshapeFromDeformedVertsContext *ctx,
        const int ptex_face_index,
        const float u, const float v,
        const int coarse_poly_index,
        const int coarse_corner,
        const int subdiv_vertex_index)
{
	const float *final_P = ctx->deformed_verts[subdiv_vertex_index];
	multires_reshape_vertex_from_final_data(
	        &ctx->reshape_ctx,
	        ptex_face_index, u, v,
	        coarse_poly_index,
	        coarse_corner,
	        final_P, 0.0f);
}

static void multires_reshape_vertex_inner(
        const SubdivForeachContext *foreach_context,
        void *UNUSED(tls_v),
        const int ptex_face_index,
        const float u, const float v,
        const int coarse_poly_index,
        const int coarse_corner,
        const int subdiv_vertex_index)
{
	MultiresReshapeFromDeformedVertsContext *ctx = foreach_context->user_data;
	multires_reshape_vertex(
	        ctx,
	        ptex_face_index, u, v,
	        coarse_poly_index,
	        coarse_corner,
	        subdiv_vertex_index);
}

static void multires_reshape_vertex_every_corner(
        const struct SubdivForeachContext *foreach_context,
        void *UNUSED(tls_v),
        const int ptex_face_index,
        const float u, const float v,
        const int UNUSED(coarse_vertex_index),
        const int coarse_poly_index,
        const int coarse_corner,
        const int subdiv_vertex_index)
{
	MultiresReshapeFromDeformedVertsContext *ctx = foreach_context->user_data;
	multires_reshape_vertex(
	        ctx,
	        ptex_face_index, u, v,
	        coarse_poly_index,
	        coarse_corner,
	        subdiv_vertex_index);
}

static void multires_reshape_vertex_every_edge(
        const struct SubdivForeachContext *foreach_context,
        void *UNUSED(tls_v),
        const int ptex_face_index,
        const float u, const float v,
        const int UNUSED(coarse_edge_index),
        const int coarse_poly_index,
        const int coarse_corner,
        const int subdiv_vertex_index)
{
	MultiresReshapeFromDeformedVertsContext *ctx = foreach_context->user_data;
	multires_reshape_vertex(
	        ctx,
	        ptex_face_index, u, v,
	        coarse_poly_index,
	        coarse_corner,
	        subdiv_vertex_index);
}

static Subdiv *multires_subdiv_for_reshape(struct Depsgraph *depsgraph,
                                           /*const*/ Object *object,
                                           const MultiresModifierData *mmd)
{
	Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
	Object *object_eval = DEG_get_evaluated_object(depsgraph, object);
	Mesh *deformed_mesh = mesh_get_eval_deform(
	        depsgraph, scene_eval, object_eval, CD_MASK_BAREMESH);
	SubdivSettings subdiv_settings;
	BKE_multires_subdiv_settings_init(&subdiv_settings, mmd);
	Subdiv *subdiv = BKE_subdiv_new_from_mesh(&subdiv_settings, deformed_mesh);
	if (!BKE_subdiv_eval_update_from_mesh(subdiv, deformed_mesh)) {
		BKE_subdiv_free(subdiv);
		return NULL;
	}
	return subdiv;
}

static bool multires_reshape_from_vertcos(
        struct Depsgraph *depsgraph,
        Object *object,
        const MultiresModifierData *mmd,
        const float (*deformed_verts)[3],
        const int num_deformed_verts,
        const bool use_render_params)
{
	Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
	Mesh *coarse_mesh = object->data;
	MDisps *mdisps = CustomData_get_layer(&coarse_mesh->ldata, CD_MDISPS);
	MultiresReshapeFromDeformedVertsContext reshape_deformed_verts_ctx = {
	        .reshape_ctx = {
	                .object = object,
	                .coarse_mesh = coarse_mesh,
	                .mdisps = mdisps,
					.grid_paint_mask = NULL,
	                /* TODO(sergey): Use grid_size_for_level_get */
	                .grid_size = (1 << (mmd->totlvl - 1)) + 1,
	                .level = mmd->totlvl,
	        },
	        .deformed_verts = deformed_verts,
	        .num_deformed_verts = num_deformed_verts,
	};
	SubdivForeachContext foreach_context = {
	        .topology_info = multires_reshape_topology_info,
	        .vertex_inner = multires_reshape_vertex_inner,
	        .vertex_every_edge = multires_reshape_vertex_every_edge,
	        .vertex_every_corner = multires_reshape_vertex_every_corner,
	        .user_data = &reshape_deformed_verts_ctx,
	};
	/* Make sure displacement grids are ready. */
	multires_reshape_ensure_grids(&reshape_deformed_verts_ctx.reshape_ctx);
	/* Initialize subdivision surface. */
	Subdiv *subdiv = multires_subdiv_for_reshape(depsgraph, object, mmd);
	if (subdiv == NULL) {
		return false;
	}
	reshape_deformed_verts_ctx.reshape_ctx.subdiv = subdiv;
	/* Initialize mesh rasterization settings. */
	SubdivToMeshSettings mesh_settings;
	BKE_multires_subdiv_mesh_settings_init(
        &mesh_settings, scene_eval, object, mmd, use_render_params, true);
	/* Initialize propagation to higher levels. */
	MultiresPropagateData propagate_data;
	multires_reshape_propagate_prepare_from_mmd(
        &propagate_data, depsgraph, object, mmd, use_render_params);
	/* Run all the callbacks. */
	BKE_subdiv_foreach_subdiv_geometry(
	        subdiv,
	        &foreach_context,
	        &mesh_settings,
	        coarse_mesh);
	BKE_subdiv_free(subdiv);
	/* Update higher levels if needed. */
	multires_reshape_propagate(&propagate_data);
	multires_reshape_propagate_free(&propagate_data);
	return true;
}

/* =============================================================================
 * Reshape from object.
 */

/* Returns truth on success, false otherwise.
 *
 * This function might fail in cases like source and destination not having
 * matched amount of vertices.
 */
bool multiresModifier_reshapeFromObject(
        struct Depsgraph *depsgraph,
        MultiresModifierData *mmd,
        Object *dst,
        Object *src)
{
	/* Would be cool to support this eventually, but it is very tricky to match
	 * vertices order even for meshes, when mixing meshes and other objects it's
	 * even more tricky.
	 */
	if (src->type != OB_MESH) {
		return false;
	}
	MultiresModifierData reshape_mmd;
	multires_reshape_init_mmd(&reshape_mmd, mmd);
	/* Get evaluated vertices locations to reshape to. */
	Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
	Object *src_eval = DEG_get_evaluated_object(depsgraph, src);
	Mesh *src_mesh_eval = mesh_get_eval_final(
	        depsgraph, scene_eval, src_eval, CD_MASK_BAREMESH);
	int num_deformed_verts;
	float (*deformed_verts)[3] = BKE_mesh_vertexCos_get(
	        src_mesh_eval, &num_deformed_verts);
	bool result = multires_reshape_from_vertcos(
	        depsgraph,
	        dst,
	        &reshape_mmd,
	        deformed_verts,
	        num_deformed_verts,
	        false);
	MEM_freeN(deformed_verts);
	return result;
}

/* =============================================================================
 * Reshape from modifier.
 */

bool multiresModifier_reshapeFromDeformModifier(
        struct Depsgraph *depsgraph,
        MultiresModifierData *mmd,
        Object *object,
        ModifierData *md)
{
	MultiresModifierData highest_mmd;
	/* It is possible that the current subdivision level of multires is lower
	 * that it's maximum possible one (i.e., viewport is set to a lower level
	 * for the performance purposes). But even then, we want all the multires
	 * levels to be reshaped. Most accurate way to do so is to ignore all
	 * simplifications and calculate deformation modifier for the highest
	 * possible multires level.
	 * Alternative would be propagate displacement from current level to a
	 * higher ones, but that is likely to cause artifacts.
	 */
	multires_reshape_init_mmd_top_level(&highest_mmd, mmd);
	Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
	/* Perform sanity checks and early output. */
	if (multires_get_level(
	            scene_eval, object, &highest_mmd, false, true) == 0)
	{
		return false;
	}
	/* Create mesh for the multires, ignoring any further modifiers (leading
	 * deformation modifiers will be applied though).
	 */
	Mesh *multires_mesh = get_multires_mesh(
	        depsgraph, scene_eval, &highest_mmd, object);
	int num_deformed_verts;
	float (*deformed_verts)[3] = BKE_mesh_vertexCos_get(
	        multires_mesh, &num_deformed_verts);
	/* Apply deformation modifier on the multires, */
	const ModifierEvalContext modifier_ctx = {
	        .depsgraph = depsgraph,
	        .object = object,
	        .flag = MOD_APPLY_USECACHE | MOD_APPLY_IGNORE_SIMPLIFY};
	modifier_deformVerts_ensure_normals(
	        md, &modifier_ctx, multires_mesh, deformed_verts,
	        multires_mesh->totvert);
	BKE_id_free(NULL, multires_mesh);
	/* Reshaping */
	bool result = multires_reshape_from_vertcos(
	        depsgraph,
	        object,
	        &highest_mmd,
	        deformed_verts,
	        num_deformed_verts,
	        false);
	/* Cleanup */
	MEM_freeN(deformed_verts);
	return result;
}

/* =============================================================================
 * Reshape from grids.
 */

typedef struct ReshapeFromCCGTaskData {
	MultiresReshapeContext reshape_ctx;
	int *face_ptex_offset;
	const CCGKey *key;
	/*const*/ CCGElem **grids;
} ReshapeFromCCGTaskData;

static void reshape_from_ccg_regular_face(ReshapeFromCCGTaskData *data,
                                          const MPoly *coarse_poly)
{
	const CCGKey *key = data->key;
	/*const*/ CCGElem **grids = data->grids;
	const Mesh *coarse_mesh = data->reshape_ctx.coarse_mesh;
	const MPoly *coarse_mpoly = coarse_mesh->mpoly;
	const int key_grid_size = key->grid_size;
	const int key_grid_size_1 = key_grid_size - 1;
	const int resolution = 2 * key_grid_size - 1;
	const float resolution_1_inv = 1.0f / (float)(resolution - 1);
	const int coarse_poly_index = coarse_poly - coarse_mpoly;
	const int ptex_face_index = data->face_ptex_offset[coarse_poly_index];
	for (int y = 0; y < resolution; y++) {
		const float v = y * resolution_1_inv;
		for (int x = 0; x < resolution; x++) {
			const float u = x * resolution_1_inv;
			float corner_u, corner_v;
			float grid_u, grid_v;
			const int face_corner = rotate_quad_to_corner(
			        u, v, &corner_u, &corner_v);
			ptex_uv_to_grid_uv(corner_u, corner_v, &grid_u, &grid_v);
			/*const*/ CCGElem *grid =
			        grids[coarse_poly->loopstart + face_corner];
			/*const*/ CCGElem *grid_element = CCG_grid_elem(
			        key,
			        grid,
			        key_grid_size_1 * grid_u,
			        key_grid_size_1 * grid_v);
			const float *final_P = CCG_elem_co(key, grid_element);
			float final_mask = 0.0f;
			if (key->has_mask) {
				final_mask = *CCG_elem_mask(key, grid_element);
			}
			multires_reshape_vertex_from_final_data(
			        &data->reshape_ctx,
			        ptex_face_index,
			        u, v,
			        coarse_poly_index,
			        0,
			        final_P, final_mask);
		}
	}
}

static void reshape_from_ccg_special_face(ReshapeFromCCGTaskData *data,
                                          const MPoly *coarse_poly)
{
	const CCGKey *key = data->key;
	/*const*/ CCGElem **grids = data->grids;
	const Mesh *coarse_mesh = data->reshape_ctx.coarse_mesh;
	const MPoly *coarse_mpoly = coarse_mesh->mpoly;
	const int key_grid_size = key->grid_size;
	const int key_grid_size_1 = key_grid_size - 1;
	const int resolution = key_grid_size;
	const float resolution_1_inv = 1.0f / (float)(resolution - 1);
	const int coarse_poly_index = coarse_poly - coarse_mpoly;
	const int ptex_face_index = data->face_ptex_offset[coarse_poly_index];
	for (int corner = 0; corner < coarse_poly->totloop; corner++) {
		for (int y = 0; y < resolution; y++) {
			const float v = y * resolution_1_inv;
			for (int x = 0; x < resolution; x++) {
				const float u = x * resolution_1_inv;
				float grid_u, grid_v;
				ptex_uv_to_grid_uv(u, v, &grid_u, &grid_v);
				/*const*/ CCGElem *grid =
				        grids[coarse_poly->loopstart + corner];
				/*const*/ CCGElem *grid_element = CCG_grid_elem(
				        key,
				        grid,
				        key_grid_size_1 * grid_u,
				        key_grid_size_1 * grid_v);
				const float *final_P = CCG_elem_co(key, grid_element);
				float final_mask = 0.0f;
				if (key->has_mask) {
					final_mask = *CCG_elem_mask(key, grid_element);
				}
				multires_reshape_vertex_from_final_data(
				        &data->reshape_ctx,
				        ptex_face_index + corner,
				        u, v,
				        coarse_poly_index,
				        corner,
				        final_P, final_mask);
			}
		}
	}
}

static void reshape_from_ccg_task(
        void *__restrict userdata,
        const int coarse_poly_index,
        const ParallelRangeTLS *__restrict UNUSED(tls))
{
	ReshapeFromCCGTaskData *data = userdata;
	const Mesh *coarse_mesh = data->reshape_ctx.coarse_mesh;
	const MPoly *coarse_mpoly = coarse_mesh->mpoly;
	const MPoly *coarse_poly = &coarse_mpoly[coarse_poly_index];
	if (coarse_poly->totloop == 4) {
		reshape_from_ccg_regular_face(data, coarse_poly);
	}
	else {
		reshape_from_ccg_special_face(data, coarse_poly);
	}
}

bool multiresModifier_reshapeFromCCG(
        MultiresModifierData *mmd,
        Object *object,
        SubdivCCG *subdiv_ccg)
{
	Mesh *coarse_mesh = object->data;
	CCGKey key;
	BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);
	/* Sanity checks. */
	if (coarse_mesh->totloop != subdiv_ccg->num_grids) {
		/* Grids are supposed to eb created for each face-cornder (aka loop). */
		return false;
	}
	MDisps *mdisps = CustomData_get_layer(&coarse_mesh->ldata, CD_MDISPS);
	GridPaintMask *grid_paint_mask =
	        CustomData_get_layer(&coarse_mesh->ldata, CD_GRID_PAINT_MASK);
	Subdiv *subdiv = subdiv_ccg->subdiv;
	ReshapeFromCCGTaskData data = {
	        .reshape_ctx = {
	                .subdiv = subdiv,
	                .object = object,
	                .coarse_mesh = coarse_mesh,
	                .mdisps  = mdisps,
	                .grid_paint_mask = grid_paint_mask,
	                 /* TODO(sergey): Use grid_size_for_level_get */
	                .grid_size = (1 << (mmd->totlvl - 1)) + 1,
	                .level = mmd->totlvl},
	        .face_ptex_offset = BKE_subdiv_face_ptex_offset_get(subdiv),
	        .key = &key,
	        .grids = subdiv_ccg->grids};
	/* Make sure displacement grids are ready. */
	multires_reshape_ensure_grids(&data.reshape_ctx);
	/* Initialize propagation to higher levels. */
	MultiresPropagateData propagate_data;
	multires_reshape_propagate_prepare(
        &propagate_data, object, key.level, mmd->totlvl);
	/* Threaded grids iteration. */
	ParallelRangeSettings parallel_range_settings;
	BLI_parallel_range_settings_defaults(&parallel_range_settings);
	BLI_task_parallel_range(0, coarse_mesh->totpoly,
	                        &data,
	                        reshape_from_ccg_task,
	                        &parallel_range_settings);
	/* Update higher levels if needed. */
	multires_reshape_propagate(&propagate_data);
	multires_reshape_propagate_free(&propagate_data);
	return true;
}
