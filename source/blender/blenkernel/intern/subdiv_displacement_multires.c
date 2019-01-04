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

/** \file blender/blenkernel/intern/subdiv_displacement_multires.c
 *  \ingroup bke
 */

#include "BKE_subdiv.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_utildefines.h"
#include "BLI_math_vector.h"

#include "BKE_customdata.h"
#include "BKE_multires.h"

#include "MEM_guardedalloc.h"

typedef struct PolyCornerIndex {
	int poly_index;
	int corner;
} PolyCornerIndex;

typedef struct MultiresDisplacementData {
	int grid_size;
	/* Mesh is used to read external displacement. */
	Mesh *mesh;
	const MPoly *mpoly;
	const MDisps *mdisps;
	/* Indexed by ptex face index, contains polygon/corner which corresponds
	 * to it.
	 *
	 * NOTE: For quad polygon this is an index of first corner only, since
	 * there we only have one ptex. */
	PolyCornerIndex *ptex_poly_corner;
	/* Sanity check, is used in debug builds.
	 * Controls that initialize() was called prior to eval_displacement(). */
	bool is_initialized;
} MultiresDisplacementData;

/* Denotes which grid to use to average value of the displacement read from the
 * grid which corresponds to the ptex face. */
typedef enum eAverageWith {
	AVERAGE_WITH_NONE,
	AVERAGE_WITH_ALL,
	AVERAGE_WITH_PREV,
	AVERAGE_WITH_NEXT,
} eAverageWith;

static int displacement_get_grid_and_coord(
        SubdivDisplacement *displacement,
        const int ptex_face_index, const float u, const float v,
        const MDisps **r_displacement_grid,
        float *grid_u, float *grid_v)
{
	MultiresDisplacementData *data = displacement->user_data;
	const PolyCornerIndex *poly_corner =
	        &data->ptex_poly_corner[ptex_face_index];
	const MPoly *poly = &data->mpoly[poly_corner->poly_index];
	const int start_grid_index = poly->loopstart + poly_corner->corner;
	int corner = 0;
	if (poly->totloop == 4) {
		float corner_u, corner_v;
		corner = BKE_subdiv_rotate_quad_to_corner(u, v, &corner_u, &corner_v);
		*r_displacement_grid = &data->mdisps[start_grid_index + corner];
		BKE_subdiv_ptex_face_uv_to_grid_uv(corner_u, corner_v, grid_u, grid_v);
	}
	else {
		*r_displacement_grid = &data->mdisps[start_grid_index];
		BKE_subdiv_ptex_face_uv_to_grid_uv(u, v, grid_u, grid_v);
	}
	return corner;
}

static const MDisps *displacement_get_next_grid(
        SubdivDisplacement *displacement,
        const int ptex_face_index, const int corner)
{
	MultiresDisplacementData *data = displacement->user_data;
	const PolyCornerIndex *poly_corner =
	        &data->ptex_poly_corner[ptex_face_index];
	const MPoly *poly = &data->mpoly[poly_corner->poly_index];
	const int effective_corner = (poly->totloop == 4) ? corner
	                                                  : poly_corner->corner;
	const int next_corner = (effective_corner + 1) % poly->totloop;
	return &data->mdisps[poly->loopstart + next_corner];
}

static const MDisps *displacement_get_prev_grid(
        SubdivDisplacement *displacement,
        const int ptex_face_index, const int corner)
{
	MultiresDisplacementData *data = displacement->user_data;
	const PolyCornerIndex *poly_corner =
	        &data->ptex_poly_corner[ptex_face_index];
	const MPoly *poly = &data->mpoly[poly_corner->poly_index];
	const int effective_corner = (poly->totloop == 4) ? corner
	                                                  : poly_corner->corner;
	const int prev_corner =
	        (effective_corner - 1 + poly->totloop) % poly->totloop;
	return &data->mdisps[poly->loopstart + prev_corner];
}

BLI_INLINE eAverageWith read_displacement_grid(
        const MDisps *displacement_grid,
        const int grid_size,
        const float grid_u, const float grid_v,
        float r_tangent_D[3])
{
	if (displacement_grid->disps == NULL) {
		zero_v3(r_tangent_D);
		return AVERAGE_WITH_NONE;
	}
	const int x = (grid_u * (grid_size - 1) + 0.5f);
	const int y = (grid_v * (grid_size - 1) + 0.5f);
	copy_v3_v3(r_tangent_D, displacement_grid->disps[y * grid_size + x]);
	if (x == 0 && y == 0) {
		return AVERAGE_WITH_ALL;
	}
	else if (x == 0) {
		return AVERAGE_WITH_PREV;
	}
	else if (y == 0) {
		return AVERAGE_WITH_NEXT;
	}
	return AVERAGE_WITH_NONE;
}

static void average_with_all(
        SubdivDisplacement *displacement,
        const int ptex_face_index, const int corner,
        const float UNUSED(grid_u), const float UNUSED(grid_v),
        float r_tangent_D[3])
{
	MultiresDisplacementData *data = displacement->user_data;
	const PolyCornerIndex *poly_corner =
	        &data->ptex_poly_corner[ptex_face_index];
	const MPoly *poly = &data->mpoly[poly_corner->poly_index];
	for (int current_corner = 0;
	     current_corner < poly->totloop;
	     current_corner++)
	{
		if (current_corner == corner) {
			continue;
		}
		const MDisps *displacement_grid =
		        &data->mdisps[poly->loopstart + current_corner];
		const float *current_tangent_D = displacement_grid->disps[0];
		r_tangent_D[2] += current_tangent_D[2];
	}
	r_tangent_D[2] /= (float)poly->totloop;
}

static void average_with_next(SubdivDisplacement *displacement,
                              const int ptex_face_index, const int corner,
                              const float grid_u, const float UNUSED(grid_v),
                              float r_tangent_D[3])
{
	MultiresDisplacementData *data = displacement->user_data;
	const int grid_size = data->grid_size;
	const MDisps *next_displacement_grid = displacement_get_next_grid(
	         displacement, ptex_face_index, corner);
	float next_tangent_D[3];
	read_displacement_grid(next_displacement_grid, grid_size,
	                       0.0f, grid_u,
	                       next_tangent_D);
	r_tangent_D[2] += next_tangent_D[2];
	r_tangent_D[2] *= 0.5f;
}

static void average_with_prev(SubdivDisplacement *displacement,
                              const int ptex_face_index, const int corner,
                              const float UNUSED(grid_u), const float grid_v,
                              float r_tangent_D[3])
{
	MultiresDisplacementData *data = displacement->user_data;
	const int grid_size = data->grid_size;
	const MDisps *prev_displacement_grid = displacement_get_prev_grid(
	         displacement, ptex_face_index, corner);
	float prev_tangent_D[3];
	read_displacement_grid(prev_displacement_grid, grid_size,
	                       grid_v, 0.0f,
	                       prev_tangent_D);
	r_tangent_D[2] += prev_tangent_D[2];
	r_tangent_D[2] *= 0.5f;
}

static void average_displacement(SubdivDisplacement *displacement,
                                 const int ptex_face_index, const int corner,
                                 eAverageWith average_with,
                                 const float grid_u, const float grid_v,
                                 float r_tangent_D[3])
{
	switch (average_with) {
		case AVERAGE_WITH_ALL:
			average_with_all(displacement,
			                 ptex_face_index, corner,
			                 grid_u, grid_v,
			                 r_tangent_D);
			break;
		case AVERAGE_WITH_PREV:
			average_with_prev(displacement,
			                  ptex_face_index, corner,
			                  grid_u, grid_v,
			                  r_tangent_D);
			break;
		case AVERAGE_WITH_NEXT:
			average_with_next(displacement,
			                  ptex_face_index, corner,
			                  grid_u, grid_v,
			                  r_tangent_D);
			break;
		case AVERAGE_WITH_NONE:
			break;
	}
}

static void initialize(SubdivDisplacement *displacement)
{
	MultiresDisplacementData *data = displacement->user_data;
	Mesh *mesh = data->mesh;
	/* Make sure external displacement is read. */
	CustomData_external_read(
	    &mesh->ldata, &mesh->id, CD_MASK_MDISPS, mesh->totloop);
	data->is_initialized = true;
}

static void eval_displacement(SubdivDisplacement *displacement,
                              const int ptex_face_index,
                              const float u, const float v,
                              const float dPdu[3], const float dPdv[3],
                              float r_D[3])
{
	MultiresDisplacementData *data = displacement->user_data;
	BLI_assert(data->is_initialized);
	const int grid_size = data->grid_size;
	/* Get displacement in tangent space. */
	const MDisps *displacement_grid;
	float grid_u, grid_v;
	int corner = displacement_get_grid_and_coord(displacement,
	                                             ptex_face_index, u, v,
	                                             &displacement_grid,
	                                             &grid_u, &grid_v);
	/* Read displacement from the current displacement grid and see if any
	 * averaging is needed. */
	float tangent_D[3];
	eAverageWith average_with =
	        read_displacement_grid(displacement_grid, grid_size,
	                               grid_u, grid_v,
	                               tangent_D);
	average_displacement(displacement,
	                     ptex_face_index, corner,
	                     average_with, grid_u, grid_v,
	                     tangent_D);
	/* Convert it to the object space. */
	float tangent_matrix[3][3];
	BKE_multires_construct_tangent_matrix(tangent_matrix, dPdu, dPdv, corner);
	mul_v3_m3v3(r_D, tangent_matrix, tangent_D);
}

static void free_displacement(SubdivDisplacement *displacement)
{
	MultiresDisplacementData *data = displacement->user_data;
	MEM_freeN(data->ptex_poly_corner);
	MEM_freeN(data);
}

/* TODO(sergey): This seems to be generally used information, which almost
 * worth adding to a subdiv itself, with possible cache of the value. */
static int count_num_ptex_faces(const Mesh *mesh)
{
	int num_ptex_faces = 0;
	const MPoly *mpoly = mesh->mpoly;
	for (int poly_index = 0; poly_index < mesh->totpoly; poly_index++) {
		const MPoly *poly = &mpoly[poly_index];
		num_ptex_faces += (poly->totloop == 4) ? 1 : poly->totloop;
	}
	return num_ptex_faces;
}

static void displacement_data_init_mapping(SubdivDisplacement *displacement,
                                           const Mesh *mesh)
{
	MultiresDisplacementData *data = displacement->user_data;
	const MPoly *mpoly = mesh->mpoly;
	const int num_ptex_faces = count_num_ptex_faces(mesh);
	/* Allocate memory. */
	data->ptex_poly_corner = MEM_malloc_arrayN(num_ptex_faces,
	                                           sizeof(*data->ptex_poly_corner),
	                                           "ptex poly corner");
	/* Fill in offsets. */
	int ptex_face_index = 0;
	PolyCornerIndex *ptex_poly_corner = data->ptex_poly_corner;
	for (int poly_index = 0; poly_index < mesh->totpoly; poly_index++) {
		const MPoly *poly = &mpoly[poly_index];
		if (poly->totloop == 4) {
			ptex_poly_corner[ptex_face_index].poly_index = poly_index;
			ptex_poly_corner[ptex_face_index].corner = 0;
			ptex_face_index++;
		}
		else {
			for (int corner = 0; corner < poly->totloop; corner++) {
				ptex_poly_corner[ptex_face_index].poly_index = poly_index;
				ptex_poly_corner[ptex_face_index].corner = corner;
				ptex_face_index++;
			}
		}
	}
}

static void displacement_init_data(SubdivDisplacement *displacement,
                                   Mesh *mesh,
                                   const MultiresModifierData *mmd)
{
	MultiresDisplacementData *data = displacement->user_data;
	data->grid_size = BKE_subdiv_grid_size_from_level(mmd->totlvl);
	data->mesh = mesh;
	data->mpoly = mesh->mpoly;
	data->mdisps = CustomData_get_layer(&mesh->ldata, CD_MDISPS);
	data->is_initialized = false;
	displacement_data_init_mapping(displacement, mesh);
}

static void displacement_init_functions(SubdivDisplacement *displacement)
{
	displacement->initialize = initialize;
	displacement->eval_displacement = eval_displacement;
	displacement->free = free_displacement;
}

void BKE_subdiv_displacement_attach_from_multires(
        Subdiv *subdiv,
        Mesh *mesh,
        const MultiresModifierData *mmd)
{
	/* Make sure we don't have previously assigned displacement. */
	BKE_subdiv_displacement_detach(subdiv);
	/* Allocate all required memory. */
	SubdivDisplacement *displacement = MEM_callocN(sizeof(SubdivDisplacement),
	                                               "multires displacement");
	displacement->user_data = MEM_callocN(sizeof(MultiresDisplacementData),
	                                      "multires displacement data");
	displacement_init_data(displacement, mesh, mmd);
	displacement_init_functions(displacement);
	/* Finish. */
	subdiv->displacement_evaluator = displacement;
}
