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

/** \file blender/blenkernel/intern/subdiv_ccg.c
 *  \ingroup bke
 */

#include "BKE_subdiv_ccg.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math_bits.h"
#include "BLI_task.h"

#include "BKE_DerivedMesh.h"
#include "BKE_ccg.h"
#include "BKE_mesh.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_eval.h"

/* =============================================================================
 * Generally useful internal helpers.
 */

/* For a given subdivision level (NOT the refinement level) get resolution
 * of grid.
 */
static int grid_size_for_level_get(const SubdivCCG *subdiv_ccg, int level)
{
	BLI_assert(level >= 1);
	BLI_assert(level <= subdiv_ccg->level);
	UNUSED_VARS_NDEBUG(subdiv_ccg);
	return (1 << (level - 1)) + 1;
}

/* Number of floats in per-vertex elements.  */
static int num_element_float_get(const SubdivCCG *subdiv_ccg)
{
	/* We always have 3 floats for coordinate. */
	int num_floats = 3;
	if (subdiv_ccg->has_normal) {
		num_floats += 3;
	}
	if (subdiv_ccg->has_mask) {
		num_floats += 1;
	}
	return num_floats;
}

/* Per-vertex element size in bytes. */
static int element_size_bytes_get(const SubdivCCG *subdiv_ccg)
{
	return sizeof(float) * num_element_float_get(subdiv_ccg);
}

/* =============================================================================
 * Internal helpers for CCG creation.
 */

static void subdiv_ccg_init_layers(SubdivCCG *subdiv_ccg,
                                   const SubdivToCCGSettings *settings)
{
	/* CCG always contains coordinates. Rest of layers are coming after them. */
	int layer_offset = sizeof(float) * 3;
	/* Mask. */
	if (settings->need_mask) {
		subdiv_ccg->has_mask = true;
		subdiv_ccg->mask_offset = layer_offset;
		layer_offset += sizeof(float);
	}
	else {
		subdiv_ccg->has_mask = false;
		subdiv_ccg->mask_offset = -1;
	}
	/* Normals.
	 *
	 * NOTE: Keep them at the end, matching old CCGDM. Doesn't really matter
	 * here, but some other area might in theory depend memory layout.
	 */
	if (settings->need_normal) {
		subdiv_ccg->has_normal = true;
		subdiv_ccg->normal_offset = layer_offset;
		layer_offset += sizeof(float) * 3;
	}
	else {
		subdiv_ccg->has_normal = false;
		subdiv_ccg->normal_offset = -1;
	}
}

/* NOTE: Grid size and layer flags are to be filled in before calling this
 * function.
 */
static void subdiv_ccg_alloc_elements(SubdivCCG *subdiv_ccg,
                                      const Mesh *coarse_mesh)
{
	const int element_size = element_size_bytes_get(subdiv_ccg);
	/* Allocate memory for surface grids. */
	const int num_grids = coarse_mesh->totloop;
	const int grid_size = grid_size_for_level_get(
	        subdiv_ccg, subdiv_ccg->level);
	const int grid_area = grid_size * grid_size;
	subdiv_ccg->num_grids = num_grids;
	subdiv_ccg->grids =
	        MEM_calloc_arrayN(num_grids, sizeof(CCGElem *), "subdiv ccg grids");
	subdiv_ccg->grids_storage = MEM_calloc_arrayN(
	        num_grids, ((size_t)grid_area) * element_size,
	        "subdiv ccg grids storage");
	const size_t grid_size_in_bytes = (size_t)grid_area * element_size;
	for (int grid_index = 0; grid_index < num_grids; grid_index++) {
		const size_t grid_offset = grid_size_in_bytes * grid_index;
		subdiv_ccg->grids[grid_index] =
		        (CCGElem *)&subdiv_ccg->grids_storage[grid_offset];
	}
	/* Grid material flags. */
	subdiv_ccg->grid_flag_mats = MEM_calloc_arrayN(
	        num_grids, sizeof(DMFlagMat), "ccg grid material flags");
	/* Grid hidden flags. */
	subdiv_ccg->grid_hidden = MEM_calloc_arrayN(
	        num_grids, sizeof(BLI_bitmap *), "ccg grid material flags");
	for (int grid_index = 0; grid_index < num_grids; grid_index++) {
		subdiv_ccg->grid_hidden[grid_index] =
		        BLI_BITMAP_NEW(grid_area, "ccg grid hidden");
	}
	/* TOOD(sergey): Allocate memory for loose elements. */
}

/* =============================================================================
 * Grids evaluation.
 */

typedef struct CCGEvalGridsData {
	SubdivCCG *subdiv_ccg;
	Subdiv *subdiv;
	const Mesh *coarse_mesh;
	int *face_petx_offset;
} CCGEvalGridsData;

static void subdiv_ccg_eval_grid_element(
        CCGEvalGridsData *data,
        const int ptex_face_index,
        const float u, const float v,
        unsigned char *element)
{
	/* TODO(sergey): Support displacement. */
	if (data->subdiv_ccg->has_normal) {
		BKE_subdiv_eval_limit_point_and_normal(
		        data->subdiv, ptex_face_index, u, v,
		        (float *)element,
		        (float *)(element + data->subdiv_ccg->normal_offset));
	}
	else {
		BKE_subdiv_eval_limit_point(
		        data->subdiv, ptex_face_index, u, v, (float *)element);
	}
}

BLI_INLINE void rotate_corner_to_quad(const int corner,
	                                  const float u, const float v,
                                      float *r_u, float *r_v)
{
	if (corner == 0) {
		*r_u = 0.5f - v * 0.5f;
		*r_v = 0.5f - u * 0.5f;
	}
	else if (corner == 1) {
		*r_u = 0.5f + u * 0.5f;
		*r_v = 0.5f - v * 0.5f;
	}
	else if (corner == 2) {
		*r_u = 0.5f + v * 0.5f;
		*r_v = 0.5f + u * 0.5f;
	}
	else if (corner == 3) {
		*r_u = 0.5f - u * 0.5f;
		*r_v = 0.5f + v * 0.5f;
	}
	else {
		BLI_assert(!"Unexpected corner configuration");
	}
}

static void subdiv_ccg_eval_regular_grid(CCGEvalGridsData *data,
                                         const MPoly *coarse_poly)
{
	SubdivCCG *subdiv_ccg = data->subdiv_ccg;
	const int coarse_poly_index = coarse_poly - data->coarse_mesh->mpoly;
	const int ptex_face_index = data->face_petx_offset[coarse_poly_index];
	const int grid_size = subdiv_ccg->grid_size;
	const float grid_size_1_inv = 1.0f / (float)(grid_size - 1);
	const int element_size = element_size_bytes_get(subdiv_ccg);
	for (int corner = 0; corner < coarse_poly->totloop; corner++) {
		unsigned char *grid = (unsigned char *)subdiv_ccg->grids[
		        coarse_poly->loopstart + corner];
		for (int y = 0; y < grid_size; y++) {
			const float grid_v = (float)y * grid_size_1_inv;
			for (int x = 0; x < grid_size; x++) {
				const float grid_u = (float)x * grid_size_1_inv;
				float u, v;
				rotate_corner_to_quad(corner, grid_u, grid_v, &u, &v);
				const size_t grid_element_index = (size_t)y * grid_size + x;
				const size_t grid_element_offset =
				        grid_element_index * element_size;
				subdiv_ccg_eval_grid_element(
				        data,
				        ptex_face_index, u, v,
				        &grid[grid_element_offset]);
			}
		}
	}
}

static void subdiv_ccg_eval_special_grid(CCGEvalGridsData *data,
                                         const MPoly *coarse_poly)
{
	SubdivCCG *subdiv_ccg = data->subdiv_ccg;
	const int coarse_poly_index = coarse_poly - data->coarse_mesh->mpoly;
	const int grid_size = subdiv_ccg->grid_size;
	const float grid_size_1_inv = 1.0f / (float)(grid_size - 1);
	const int element_size = element_size_bytes_get(subdiv_ccg);
	for (int corner = 0; corner < coarse_poly->totloop; corner++) {
		unsigned char *grid = (unsigned char *)subdiv_ccg->grids[
		        coarse_poly->loopstart + corner];
		for (int y = 0; y < grid_size; y++) {
			const float u = 1.0f - ((float)y * grid_size_1_inv);
			for (int x = 0; x < grid_size; x++) {
				const float v = 1.0f - ((float)x * grid_size_1_inv);
				const int ptex_face_index =
				        data->face_petx_offset[coarse_poly_index] + corner;
				const size_t grid_element_index = (size_t)y * grid_size + x;
				const size_t grid_element_offset =
				        grid_element_index * element_size;
				subdiv_ccg_eval_grid_element(
				        data,
				        ptex_face_index, u, v,
				        &grid[grid_element_offset]);
			}
		}
	}
}

static void subdiv_ccg_eval_grids_task(
        void *__restrict userdata_v,
        const int coarse_poly_index,
        const ParallelRangeTLS *__restrict UNUSED(tls))
{
	CCGEvalGridsData *data = userdata_v;
	const Mesh *coarse_mesh = data->coarse_mesh;
	const MPoly *coarse_mpoly = coarse_mesh->mpoly;
	const MPoly *coarse_poly = &coarse_mpoly[coarse_poly_index];
	if (coarse_poly->totloop == 4) {
		subdiv_ccg_eval_regular_grid(data, coarse_poly);
	}
	else {
		subdiv_ccg_eval_special_grid(data, coarse_poly);
	}
}

static bool subdiv_ccg_evaluate_grids(SubdivCCG *subdiv_ccg,
	                                  Subdiv *subdiv,
                                      const Mesh *coarse_mesh)
{
	/* Make sure evaluator is ready. */
	if (!BKE_subdiv_eval_update_from_mesh(subdiv, coarse_mesh)) {
		if (coarse_mesh->totpoly) {
			return false;
		}
	}
	/* Initialize data passed to all the tasks. */
	CCGEvalGridsData data;
	data.subdiv_ccg = subdiv_ccg;
	data.subdiv = subdiv;
	data.coarse_mesh = coarse_mesh;
	data.face_petx_offset = BKE_subdiv_face_ptex_offset_get(subdiv);
	/* Threaded grids evaluation/ */
	ParallelRangeSettings parallel_range_settings;
	BLI_parallel_range_settings_defaults(&parallel_range_settings);
	BLI_task_parallel_range(0, coarse_mesh->totpoly,
	                        &data,
	                        subdiv_ccg_eval_grids_task,
	                        &parallel_range_settings);
	return true;
}

/* =============================================================================
 * Public API.
 */

SubdivCCG *BKE_subdiv_to_ccg(
        Subdiv *subdiv,
        const SubdivToCCGSettings *settings,
        const Mesh *coarse_mesh)
{
	BKE_subdiv_stats_begin(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_CCG);
	SubdivCCG *subdiv_ccg = MEM_callocN(sizeof(SubdivCCG), "subdiv ccg");
	subdiv_ccg->level = bitscan_forward_i(settings->resolution - 1);
	subdiv_ccg->grid_size =
	        grid_size_for_level_get(subdiv_ccg, subdiv_ccg->level);
	subdiv_ccg_init_layers(subdiv_ccg, settings);
	subdiv_ccg_alloc_elements(subdiv_ccg, coarse_mesh);
	if (!subdiv_ccg_evaluate_grids(subdiv_ccg, subdiv, coarse_mesh)) {
		BKE_subdiv_ccg_destroy(subdiv_ccg);
		BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_CCG);
		return NULL;
	}
	BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_CCG);
	return subdiv_ccg;
}

Mesh *BKE_subdiv_to_ccg_mesh(
        Subdiv *subdiv,
        const SubdivToCCGSettings *settings,
        const Mesh *coarse_mesh)
{
	SubdivCCG *subdiv_ccg = BKE_subdiv_to_ccg(
	        subdiv, settings, coarse_mesh);
	if (subdiv_ccg == NULL) {
		return NULL;
	}
	Mesh *result = BKE_mesh_new_nomain_from_template(
	        coarse_mesh, 0, 0, 0, 0, 0);
	result->runtime.subsurf_ccg = subdiv_ccg;
	return result;
}

void BKE_subdiv_ccg_destroy(SubdivCCG *subdiv_ccg)
{
	const int num_grids = subdiv_ccg->num_grids;
	MEM_SAFE_FREE(subdiv_ccg->grids);
	MEM_SAFE_FREE(subdiv_ccg->grids_storage);
	MEM_SAFE_FREE(subdiv_ccg->edges);
	MEM_SAFE_FREE(subdiv_ccg->vertices);
	MEM_SAFE_FREE(subdiv_ccg->grid_flag_mats);
	if (subdiv_ccg->grid_hidden != NULL) {
		for (int grid_index = 0; grid_index < num_grids; grid_index++) {
			MEM_freeN(subdiv_ccg->grid_hidden[grid_index]);
		}
		MEM_freeN(subdiv_ccg->grid_hidden);
	}
	MEM_freeN(subdiv_ccg);
}

void BKE_subdiv_ccg_key(CCGKey *key, const SubdivCCG *subdiv_ccg, int level)
{
	key->level = level;
	key->elem_size = element_size_bytes_get(subdiv_ccg);
	key->grid_size = grid_size_for_level_get(subdiv_ccg, level);
	key->grid_area = key->grid_size * key->grid_size;
	key->grid_bytes = key->elem_size * key->grid_area;

	key->normal_offset = subdiv_ccg->normal_offset;
	key->mask_offset = subdiv_ccg->mask_offset;

	key->has_normals = subdiv_ccg->has_normal;
	key->has_mask = subdiv_ccg->has_mask;
}

void BKE_subdiv_ccg_key_top_level(CCGKey *key, const SubdivCCG *subdiv_ccg)
{
	BKE_subdiv_ccg_key(key, subdiv_ccg, subdiv_ccg->level);
}
