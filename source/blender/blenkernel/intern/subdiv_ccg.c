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
#include "BLI_math_vector.h"
#include "BLI_task.h"

#include "BKE_DerivedMesh.h"
#include "BKE_ccg.h"
#include "BKE_mesh.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_eval.h"

#include "opensubdiv_topology_refiner_capi.h"

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

/* TODO(sergey): Make it more accessible function. */
static int topology_refiner_count_face_corners(
        OpenSubdiv_TopologyRefiner *topology_refiner)
{
	const int num_faces = topology_refiner->getNumFaces(topology_refiner);
	int num_corners = 0;
	for (int face_index = 0; face_index < num_faces; face_index++) {
		num_corners += topology_refiner->getNumFaceVertices(
		        topology_refiner, face_index);
	}
	return num_corners;
}

/* NOTE: Grid size and layer flags are to be filled in before calling this
 * function.
 */
static void subdiv_ccg_alloc_elements(SubdivCCG *subdiv_ccg, Subdiv *subdiv)
{
	OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
	const int element_size = element_size_bytes_get(subdiv_ccg);
	/* Allocate memory for surface grids. */
	const int num_faces = topology_refiner->getNumFaces(topology_refiner);
	const int num_grids = topology_refiner_count_face_corners(topology_refiner);
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
	/* Allocate memory for faces. */
	subdiv_ccg->num_faces = num_faces;
	if (num_faces) {
		subdiv_ccg->faces = MEM_calloc_arrayN(
		        num_faces, sizeof(SubdivCCGFace), "Subdiv CCG faces");
		subdiv_ccg->grid_faces = MEM_calloc_arrayN(
		        num_grids, sizeof(SubdivCCGFace *), "Subdiv CCG grid faces");
	}
}

/* =============================================================================
 * Grids evaluation.
 */

typedef struct CCGEvalGridsData {
	SubdivCCG *subdiv_ccg;
	Subdiv *subdiv;
	int *face_ptex_offset;
} CCGEvalGridsData;

static void subdiv_ccg_eval_grid_element(
        CCGEvalGridsData *data,
        const int ptex_face_index,
        const float u, const float v,
        unsigned char *element)
{
	if (data->subdiv->displacement_evaluator != NULL) {
		BKE_subdiv_eval_final_point(
		        data->subdiv, ptex_face_index, u, v, (float *)element);
	}
	else if (data->subdiv_ccg->has_normal) {
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

BLI_INLINE void rotate_corner_to_quad(
        const int corner,
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
                                         const int face_index)
{
	SubdivCCG *subdiv_ccg = data->subdiv_ccg;
	const int ptex_face_index = data->face_ptex_offset[face_index];
	const int grid_size = subdiv_ccg->grid_size;
	const float grid_size_1_inv = 1.0f / (float)(grid_size - 1);
	const int element_size = element_size_bytes_get(subdiv_ccg);
	SubdivCCGFace *faces = subdiv_ccg->faces;
	SubdivCCGFace **grid_faces = subdiv_ccg->grid_faces;
	const SubdivCCGFace *face = &faces[face_index];
	for (int corner = 0; corner < face->num_grids; corner++) {
		const int grid_index = face->start_grid_index + corner;
		unsigned char *grid = (unsigned char *)subdiv_ccg->grids[grid_index];
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
		/* Assign grid's face. */
		grid_faces[grid_index] = &faces[face_index];
	}
}

static void subdiv_ccg_eval_special_grid(CCGEvalGridsData *data,
                                         const int face_index)
{
	SubdivCCG *subdiv_ccg = data->subdiv_ccg;
	const int grid_size = subdiv_ccg->grid_size;
	const float grid_size_1_inv = 1.0f / (float)(grid_size - 1);
	const int element_size = element_size_bytes_get(subdiv_ccg);
	SubdivCCGFace *faces = subdiv_ccg->faces;
	SubdivCCGFace **grid_faces = subdiv_ccg->grid_faces;
	const SubdivCCGFace *face = &faces[face_index];
	for (int corner = 0; corner < face->num_grids; corner++) {
		const int grid_index = face->start_grid_index + corner;
		unsigned char *grid = (unsigned char *)subdiv_ccg->grids[grid_index];
		for (int y = 0; y < grid_size; y++) {
			const float u = 1.0f - ((float)y * grid_size_1_inv);
			for (int x = 0; x < grid_size; x++) {
				const float v = 1.0f - ((float)x * grid_size_1_inv);
				const int ptex_face_index =
				        data->face_ptex_offset[face_index] + corner;
				const size_t grid_element_index = (size_t)y * grid_size + x;
				const size_t grid_element_offset =
				        grid_element_index * element_size;
				subdiv_ccg_eval_grid_element(
				        data,
				        ptex_face_index, u, v,
				        &grid[grid_element_offset]);
			}
		}
		/* Assign grid's face. */
		grid_faces[grid_index] = &faces[face_index];
	}
}

static void subdiv_ccg_eval_grids_task(
        void *__restrict userdata_v,
        const int face_index,
        const ParallelRangeTLS *__restrict UNUSED(tls))
{
	CCGEvalGridsData *data = userdata_v;
	SubdivCCG *subdiv_ccg = data->subdiv_ccg;
	SubdivCCGFace *face = &subdiv_ccg->faces[face_index];
	if (face->num_grids == 4) {
		subdiv_ccg_eval_regular_grid(data, face_index);
	}
	else {
		subdiv_ccg_eval_special_grid(data, face_index);
	}
}

static bool subdiv_ccg_evaluate_grids(
        SubdivCCG *subdiv_ccg,
        Subdiv *subdiv)
{
	OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
	const int num_faces = topology_refiner->getNumFaces(topology_refiner);
	/* Initialize data passed to all the tasks. */
	CCGEvalGridsData data;
	data.subdiv_ccg = subdiv_ccg;
	data.subdiv = subdiv;
	data.face_ptex_offset = BKE_subdiv_face_ptex_offset_get(subdiv);
	/* Threaded grids evaluation. */
	ParallelRangeSettings parallel_range_settings;
	BLI_parallel_range_settings_defaults(&parallel_range_settings);
	BLI_task_parallel_range(0, num_faces,
	                        &data,
	                        subdiv_ccg_eval_grids_task,
	                        &parallel_range_settings);
	/* If displacement is used, need to calculate normals after all final
	 * coordinates are known.
	 */
	if (subdiv->displacement_evaluator != NULL) {
		BKE_subdiv_ccg_recalc_normals(subdiv_ccg);
	}
	return true;
}

/* Initialize face descriptors, assuming memory for them was already
 * allocated.
 */
static void subdiv_ccg_init_faces(SubdivCCG *subdiv_ccg)
{
	Subdiv *subdiv = subdiv_ccg->subdiv;
	OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
	const int num_faces = subdiv_ccg->num_faces;
	int corner_index = 0;
	for (int face_index = 0; face_index < num_faces; face_index++) {
		const int num_corners = topology_refiner->getNumFaceVertices(
		        topology_refiner, face_index);
		subdiv_ccg->faces[face_index].num_grids = num_corners;
		subdiv_ccg->faces[face_index].start_grid_index = corner_index;
		corner_index += num_corners;
	}
}

/* =============================================================================
 * Creation / evaluation.
 */

SubdivCCG *BKE_subdiv_to_ccg(
        Subdiv *subdiv,
        const SubdivToCCGSettings *settings)
{
	BKE_subdiv_stats_begin(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_CCG);
	SubdivCCG *subdiv_ccg = MEM_callocN(sizeof(SubdivCCG), "subdiv ccg");
	subdiv_ccg->subdiv = subdiv;
	subdiv_ccg->level = bitscan_forward_i(settings->resolution - 1);
	subdiv_ccg->grid_size =
	        grid_size_for_level_get(subdiv_ccg, subdiv_ccg->level);
	subdiv_ccg_init_layers(subdiv_ccg, settings);
	subdiv_ccg_alloc_elements(subdiv_ccg, subdiv);
	subdiv_ccg_init_faces(subdiv_ccg);
	if (!subdiv_ccg_evaluate_grids(subdiv_ccg, subdiv)) {
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
	/* Make sure evaluator is ready. */
	BKE_subdiv_stats_begin(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_CCG);
	if (!BKE_subdiv_eval_update_from_mesh(subdiv, coarse_mesh)) {
		if (coarse_mesh->totpoly) {
			return false;
		}
	}
	BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_CCG);
	SubdivCCG *subdiv_ccg = BKE_subdiv_to_ccg(subdiv, settings);
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
	if (subdiv_ccg->subdiv != NULL) {
		BKE_subdiv_free(subdiv_ccg->subdiv);
	}
	MEM_SAFE_FREE(subdiv_ccg->faces);
	MEM_SAFE_FREE(subdiv_ccg->grid_faces);
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

/* =============================================================================
 * Normals.
 */

typedef struct RecalcInnerNormalsData {
	SubdivCCG *subdiv_ccg;
	CCGKey *key;
} RecalcInnerNormalsData;

typedef struct RecalcInnerNormalsTLSData {
	float (*face_normals)[3];
} RecalcInnerNormalsTLSData;

/* Evaluate high-res face normals, for faces which corresponds to grid elements
 *
 *   {(x, y), {x + 1, y}, {x + 1, y + 1}, {x, y + 1}}
 *
 * The result is stored in normals storage from TLS.
 */
static void subdiv_ccg_recalc_inner_face_normals(
        RecalcInnerNormalsData *data,
        RecalcInnerNormalsTLSData *tls,
        const int grid_index)
{
	SubdivCCG *subdiv_ccg = data->subdiv_ccg;
	CCGKey *key = data->key;
	const int grid_size = subdiv_ccg->grid_size;
	const int grid_size_1 = grid_size - 1;
	CCGElem *grid = subdiv_ccg->grids[grid_index];
	if (tls->face_normals == NULL) {
		tls->face_normals = MEM_malloc_arrayN(
		        grid_size_1 * grid_size_1,
		        3 * sizeof(float),
		        "CCG TLS normals");
	}
	for (int y = 0; y < grid_size -1; y++) {
		for (int x = 0; x < grid_size - 1; x++) {
			CCGElem *grid_elements[4] = {
				CCG_grid_elem(key, grid, x, y + 1),
				CCG_grid_elem(key, grid, x + 1, y + 1),
				CCG_grid_elem(key, grid, x + 1, y),
				CCG_grid_elem(key, grid, x, y)
			};
			float *co[4] = {
			    CCG_elem_co(key, grid_elements[0]),
			    CCG_elem_co(key, grid_elements[1]),
			    CCG_elem_co(key, grid_elements[2]),
			    CCG_elem_co(key, grid_elements[3])
			};
			const int face_index = y * grid_size_1 + x;
			float *face_normal = tls->face_normals[face_index];
			normal_quad_v3(face_normal, co[0], co[1], co[2], co[3]);
		}
	}
}

/* Average normals at every grid element, using adjacent faces normals. */
static void subdiv_ccg_average_inner_face_normals(
        RecalcInnerNormalsData *data,
        RecalcInnerNormalsTLSData *tls,
        const int grid_index)
{
	SubdivCCG *subdiv_ccg = data->subdiv_ccg;
	CCGKey *key = data->key;
	const int grid_size = subdiv_ccg->grid_size;
	const int grid_size_1 = grid_size - 1;
	CCGElem *grid = subdiv_ccg->grids[grid_index];
	const float (*face_normals)[3] = tls->face_normals;
	for (int y = 0; y < grid_size; y++) {
		for (int x = 0; x < grid_size; x++) {
			float normal_acc[3] = {0.0f, 0.0f, 0.0f};
			int counter = 0;
			/* Accumulate normals of all adjacent faces. */
			if (x < grid_size_1 && y < grid_size_1) {
				add_v3_v3(normal_acc, face_normals[y * grid_size_1 + x]);
				counter++;
			}
			if (x >= 1) {
				if (y < grid_size_1) {
					add_v3_v3(normal_acc,
					          face_normals[y * grid_size_1 + (x - 1)]);
					counter++;
				}
				if (y >= 1) {
					add_v3_v3(normal_acc,
					          face_normals[(y - 1) * grid_size_1 + (x - 1)]);
					counter++;
				}
			}
			if (y >= 1 && x < grid_size_1) {
				add_v3_v3(normal_acc, face_normals[(y - 1) * grid_size_1 + x]);
				counter++;
			}
			/* Normalize and store. */
			mul_v3_v3fl(CCG_grid_elem_no(key, grid, x, y),
			            normal_acc,
			            1.0f / (float)counter);
		}
	}
}

static void subdiv_ccg_recalc_inner_normal_task(
        void *__restrict userdata_v,
        const int grid_index,
        const ParallelRangeTLS *__restrict tls_v)
{
	RecalcInnerNormalsData *data = userdata_v;
	RecalcInnerNormalsTLSData *tls = tls_v->userdata_chunk;
	subdiv_ccg_recalc_inner_face_normals(data, tls, grid_index);
	subdiv_ccg_average_inner_face_normals(data, tls, grid_index);
}

static void subdiv_ccg_recalc_inner_normal_finalize(
        void *__restrict UNUSED(userdata),
        void *__restrict tls_v)
{
	RecalcInnerNormalsTLSData *tls = tls_v;
	MEM_SAFE_FREE(tls->face_normals);
}

/* Recalculate normals which corresponds to non-boundaries elements of grids. */
static void subdiv_ccg_recalc_inner_grid_normals(SubdivCCG *subdiv_ccg)
{
	CCGKey key;
	BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);
	RecalcInnerNormalsData data = {
	        .subdiv_ccg = subdiv_ccg,
	        .key = &key
	};
	RecalcInnerNormalsTLSData tls_data = {NULL};
	ParallelRangeSettings parallel_range_settings;
	BLI_parallel_range_settings_defaults(&parallel_range_settings);
	parallel_range_settings.userdata_chunk = &tls_data;
	parallel_range_settings.userdata_chunk_size = sizeof(tls_data);
	parallel_range_settings.func_finalize =
	        subdiv_ccg_recalc_inner_normal_finalize;
	BLI_task_parallel_range(0, subdiv_ccg->num_grids,
	                        &data,
	                        subdiv_ccg_recalc_inner_normal_task,
	                        &parallel_range_settings);
}

void BKE_subdiv_ccg_recalc_normals(SubdivCCG *subdiv_ccg)
{
	if (!subdiv_ccg->has_normal) {
		/* Grids don't have normals, can do early output. */
		return;
	}
	subdiv_ccg_recalc_inner_grid_normals(subdiv_ccg);
	BKE_subdiv_ccg_average_grids(subdiv_ccg);
}

/* =============================================================================
 * Boundary averaging/stitching.
 */

typedef struct AverageInnerGridsData {
	SubdivCCG *subdiv_ccg;
	CCGKey *key;
} AverageInnerGridsData;

static void average_grid_element_value_v3(float a[3], float b[3])
{
	add_v3_v3(a, b);
	mul_v3_fl(a, 0.5f);
	copy_v3_v3(b, a);
}

static void average_grid_element(SubdivCCG *subdiv_ccg,
                                 CCGKey *key,
                                 CCGElem *grid_element_a,
                                 CCGElem *grid_element_b)
{
	average_grid_element_value_v3(CCG_elem_co(key, grid_element_a),
	                              CCG_elem_co(key, grid_element_b));
	if (subdiv_ccg->has_normal) {
		average_grid_element_value_v3(CCG_elem_no(key, grid_element_a),
		                              CCG_elem_no(key, grid_element_b));
	}
}

static void subdiv_ccg_average_inner_face_grids(
        SubdivCCG *subdiv_ccg,
        CCGKey *key,
        SubdivCCGFace *face)
{
	CCGElem **grids = subdiv_ccg->grids;
	const int num_face_grids = face->num_grids;
	const int grid_size = subdiv_ccg->grid_size;
	CCGElem *prev_grid = grids[face->start_grid_index + num_face_grids - 1];
	for (int corner = 0; corner < num_face_grids; corner++) {
		CCGElem *grid = grids[face->start_grid_index + corner];
		for (int i = 0; i < grid_size; i++) {
			CCGElem *prev_grid_element = CCG_grid_elem(key, prev_grid, i, 0);
			CCGElem *grid_element = CCG_grid_elem(key, grid, 0, i);
			average_grid_element(
			        subdiv_ccg, key, prev_grid_element, grid_element);
		}
		prev_grid = grid;
	}

}

static void subdiv_ccg_average_inner_grids_task(
        void *__restrict userdata_v,
        const int face_index,
        const ParallelRangeTLS *__restrict UNUSED(tls_v))
{
	AverageInnerGridsData *data = userdata_v;
	SubdivCCG *subdiv_ccg = data->subdiv_ccg;
	CCGKey *key = data->key;
	SubdivCCGFace *faces = subdiv_ccg->faces;
	SubdivCCGFace *face = &faces[face_index];
	subdiv_ccg_average_inner_face_grids(subdiv_ccg, key, face);
}

void BKE_subdiv_ccg_average_grids(SubdivCCG *subdiv_ccg)
{
	CCGKey key;
	BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);
	AverageInnerGridsData data = {
	        .subdiv_ccg = subdiv_ccg,
	        .key = &key,
	};
	ParallelRangeSettings parallel_range_settings;
	BLI_parallel_range_settings_defaults(&parallel_range_settings);
	BLI_task_parallel_range(0, subdiv_ccg->num_faces,
	                        &data,
	                        subdiv_ccg_average_inner_grids_task,
	                        &parallel_range_settings);
}

typedef struct StitchFacesInnerGridsData {
	SubdivCCG *subdiv_ccg;
	CCGKey *key;
	struct CCGFace **effected_ccg_faces;
} StitchFacesInnerGridsData;

static void subdiv_ccg_stitch_face_inner_grids_task(
        void *__restrict userdata_v,
        const int face_index,
        const ParallelRangeTLS *__restrict UNUSED(tls_v))
{
	StitchFacesInnerGridsData *data = userdata_v;
	SubdivCCG *subdiv_ccg = data->subdiv_ccg;
	CCGKey *key = data->key;
	struct CCGFace **effected_ccg_faces = data->effected_ccg_faces;
	struct CCGFace *effected_ccg_face = effected_ccg_faces[face_index];
	SubdivCCGFace *face = (SubdivCCGFace *)effected_ccg_face;
	subdiv_ccg_average_inner_face_grids(subdiv_ccg, key, face);
}

void BKE_subdiv_ccg_average_stitch_faces(SubdivCCG *subdiv_ccg,
                                         struct CCGFace **effected_faces,
                                         int num_effected_faces)
{
	CCGKey key;
	BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);
	StitchFacesInnerGridsData data = {
	        .subdiv_ccg = subdiv_ccg,
	        .key = &key,
	        .effected_ccg_faces = effected_faces,
	};
	ParallelRangeSettings parallel_range_settings;
	BLI_parallel_range_settings_defaults(&parallel_range_settings);
	BLI_task_parallel_range(0, num_effected_faces,
	                        &data,
	                        subdiv_ccg_stitch_face_inner_grids_task,
	                        &parallel_range_settings);
}
