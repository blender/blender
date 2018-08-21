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

#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_subdiv.h"

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

/* =============================================================================
 * Reshape internal functionality..
 */

typedef struct MultiresReshapeContext {
	Subdiv *subdiv;
	Object *object;
	const Mesh *coarse_mesh;
	MDisps *mdisps;
	const float (*deformed_verts)[3];
	int num_deformed_verts;
	int grid_size;
} MultiresReshapeContext;

static bool multires_reshape_topology_info(
        const SubdivForeachContext *foreach_context,
        const int num_vertices,
        const int UNUSED(num_edges),
        const int UNUSED(num_loops),
        const int UNUSED(num_polygons))
{
	MultiresReshapeContext *ctx = foreach_context->user_data;
	if (num_vertices != ctx->num_deformed_verts) {
		return false;
	}
	return true;
}

static void multires_reshape_vertex_copy_to_next(
        MultiresReshapeContext *ctx,
        const MPoly *coarse_poly,
        const int current_corner,
        const MDisps *current_displacement_grid,
        const int current_grid_x, const int current_grid_y)
{
	const int grid_size = ctx->grid_size;
	const int next_current_corner = (current_corner + 1) % coarse_poly->totloop;
	MDisps *next_displacement_grid = &ctx->mdisps[
	        coarse_poly->loopstart + next_current_corner];
	const int next_grid_x = 0;
	const int next_grid_y = current_grid_x;
	const int current_index = current_grid_y * grid_size + current_grid_x;
	const int next_index = next_grid_y * grid_size + next_grid_x;
	float *next_displacement = next_displacement_grid->disps[next_index];
	copy_v3_v3(next_displacement,
	           current_displacement_grid->disps[current_index]);
	SWAP(float, next_displacement[0], next_displacement[1]);
	next_displacement[0] = -next_displacement[0];
}

static void multires_reshape_vertex_copy_to_prev(
        MultiresReshapeContext *ctx,
        const MPoly *coarse_poly,
        const int current_corner,
        const MDisps *current_displacement_grid,
        const int current_grid_x, const int current_grid_y)
{
	const int grid_size = ctx->grid_size;
	const int prev_current_corner =
	        (current_corner - 1 + coarse_poly->totloop) % coarse_poly->totloop;
	MDisps *prev_displacement_grid = &ctx->mdisps[
	        coarse_poly->loopstart + prev_current_corner];
	const int prev_grid_x = current_grid_y;
	const int prev_grid_y = 0;
	const int current_index = current_grid_y * grid_size + current_grid_x;
	const int prev_index = prev_grid_y * grid_size + prev_grid_x;
	float *prev_displacement = prev_displacement_grid->disps[prev_index];
	copy_v3_v3(prev_displacement,
	           current_displacement_grid->disps[current_index]);
	SWAP(float, prev_displacement[0], prev_displacement[1]);
	prev_displacement[1] = -prev_displacement[1];
}

static void copy_boundary_displacement(
        MultiresReshapeContext *ctx,
        const MPoly *coarse_poly,
        const int corner,
        const int grid_x, const int grid_y,
        const MDisps *displacement_grid)
{
	if (grid_x == 0 && grid_y == 0) {
		for (int i = 0; i < coarse_poly->totloop; i++) {
			const int current_face_corner =
			        (corner + i) % coarse_poly->totloop;
			MDisps *current_displacement_grid = &ctx->mdisps[
			        coarse_poly->loopstart + current_face_corner];
			multires_reshape_vertex_copy_to_next(
			        ctx,
			        coarse_poly,
			        current_face_corner,
			        current_displacement_grid,
			        0, 0);
		}
	}
	else if (grid_x == 0) {
		multires_reshape_vertex_copy_to_prev(
		        ctx,
		        coarse_poly,
		        corner,
		        displacement_grid,
		        grid_x, grid_y);
	}
	else if (grid_y == 0) {
		multires_reshape_vertex_copy_to_next(
		        ctx,
		        coarse_poly,
		        corner,
		        displacement_grid,
		        grid_x, grid_y);
	}
}

static void multires_reshape_vertex(
        MultiresReshapeContext *ctx,
        const int ptex_face_index,
        const float u, const float v,
        const int coarse_poly_index,
        const int coarse_corner,
        const int subdiv_vertex_index)
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
	/* Get coordinate and corner configuration.. */
	float grid_u, grid_v;
	MDisps *displacement_grid;
	int face_corner = coarse_corner;
	int grid_corner = 0;
	if (coarse_poly->totloop == 4) {
		float corner_u, corner_v;
		face_corner = rotate_quad_to_corner(u, v, &corner_u, &corner_v);
		grid_corner = face_corner;
		displacement_grid = &ctx->mdisps[loop_index + face_corner];
		ptex_uv_to_grid_uv(corner_u, corner_v, &grid_u, &grid_v);
	}
	else {
		displacement_grid = &ctx->mdisps[loop_index];
		ptex_uv_to_grid_uv(u, v, &grid_u, &grid_v);
	}
	/* Convert object coordinate to a tangent space of displacement grid. */
	float D[3];
	sub_v3_v3v3(D, ctx->deformed_verts[subdiv_vertex_index], P);
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
	/* Copy boundary to the next/previous grids */
	copy_boundary_displacement(
	        ctx, coarse_poly, face_corner, grid_x, grid_y, displacement_grid);
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
	MultiresReshapeContext *ctx = foreach_context->user_data;
	multires_reshape_vertex(
	        ctx,
	        ptex_face_index, u, v, 
	        coarse_poly_index,
	        coarse_corner,
	        subdiv_vertex_index);
}

static void multires_reshape_vertex_corner(
        const struct SubdivForeachContext *foreach_context,
        void *UNUSED(tls_v),
        const int ptex_face_index,
        const float u, const float v,
        const int UNUSED(coarse_vertex_index),
        const int coarse_poly_index,
        const int coarse_corner,
        const int subdiv_vertex_index)
{
	MultiresReshapeContext *ctx = foreach_context->user_data;
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
	MultiresReshapeContext *ctx = foreach_context->user_data;
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

static bool multires_reshape_from_vertcos(struct Depsgraph *depsgraph,
	                                      Object *object,
                                          const MultiresModifierData *mmd,
                                          const float (*deformed_verts)[3],
                                          const int num_deformed_verts,
                                          const bool use_render_params)
{
	Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
	Mesh *coarse_mesh = object->data;
	MultiresReshapeContext ctx = {
	        .object = object,
	        .coarse_mesh = coarse_mesh,
	        .deformed_verts = deformed_verts,
	        .mdisps = CustomData_get_layer(&coarse_mesh->ldata, CD_MDISPS),
	        .num_deformed_verts = num_deformed_verts,
	        .grid_size = (1 << (mmd->totlvl - 1)) + 1,
	};
	SubdivForeachContext foreach_context = {
	        .topology_info = multires_reshape_topology_info,
	        .vertex_inner = multires_reshape_vertex_inner,
	        .vertex_every_edge = multires_reshape_vertex_every_edge,
	        .vertex_corner = multires_reshape_vertex_corner,
	        .user_data = &ctx,
	};
	/* Initialize subdivision surface. */
	ctx.subdiv = multires_subdiv_for_reshape(depsgraph, object, mmd);
	if (ctx.subdiv == NULL) {
		return false;
	}
	/* Initialize mesh rasterization settings. */
	SubdivToMeshSettings mesh_settings;
	BKE_multires_subdiv_mesh_settings_init(
        &mesh_settings, scene_eval, object, mmd, use_render_params);
	/* Run all the callbacks. */
	BKE_subdiv_foreach_subdiv_geometry(
	        ctx.subdiv,
	        &foreach_context,
	        &mesh_settings,
	        coarse_mesh);
	BKE_subdiv_free(ctx.subdiv);
	return true;
}

/* =============================================================================
 * Public entry points..
 */

/* Returns truth on success, false otherwise.
 *
 * This function might fail in cases like source and destination not having
 * matched amount of vertices.
 */
bool multiresModifier_reshape(
        struct Depsgraph *UNUSED(depsgraph),
        Scene *UNUSED(scene),
        MultiresModifierData *UNUSED(mmd),
        Object *UNUSED(dst),
        Object *UNUSED(src))
{
	/* TODO(sergey): Need to port to a new rehape routines. Old ones were
	 * based on DerivedMesh and can not used anymore.
	 */
	return false;
}

bool multiresModifier_reshapeFromDeformModifier(
        struct Depsgraph *depsgraph,
        Scene *UNUSED(scene),
        MultiresModifierData *mmd,
        Object *object,
        ModifierData *md)
{
	Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
	/* Perform sanity checks and early output. */
	if (multires_get_level(scene_eval, object, mmd, false, true) == 0) {
		return false;
	}
	/* Create mesh for the multires, ignoring any further modifiers (leading
	 * deformation modifiers will be applied though).
	 */
	Mesh *multires_mesh = get_multires_mesh(depsgraph, scene_eval, mmd, object);
	float (*deformed_verts)[3] = BKE_mesh_vertexCos_get(multires_mesh, NULL);
	/* Apply deformation modifier on the multires, */
	const ModifierEvalContext modifier_ctx = {.depsgraph = depsgraph,
	                                          .object = object,
	                                          .flag = 0};
	modifier_deformVerts_ensure_normals(
	        md, &modifier_ctx, multires_mesh, deformed_verts,
	        multires_mesh->totvert);
	const int num_deformed_verts = multires_mesh->totvert;
	BKE_id_free(NULL, multires_mesh);
	/* Reshaping */
	bool result = multires_reshape_from_vertcos(
	        depsgraph,
	        object,
	        mmd,
	        deformed_verts,
	        num_deformed_verts,
	        false);
	/* Cleanup */
	MEM_freeN(deformed_verts);
	return result;
}
