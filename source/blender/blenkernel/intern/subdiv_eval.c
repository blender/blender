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

/** \file blender/blenkernel/intern/subdiv_eval.c
 *  \ingroup bke
 */

#include "BKE_subdiv.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_bitmap.h"
#include "BLI_math_vector.h"

#include "BKE_customdata.h"

#include "MEM_guardedalloc.h"

#ifdef WITH_OPENSUBDIV
#  include "opensubdiv_evaluator_capi.h"
#  include "opensubdiv_topology_refiner_capi.h"
#endif

void BKE_subdiv_eval_begin(Subdiv *subdiv)
{
#ifdef WITH_OPENSUBDIV
	if (subdiv->topology_refiner == NULL) {
		/* Happens on input mesh with just loose geometry. */
	}
	else if (subdiv->evaluator == NULL) {
		BKE_subdiv_stats_begin(&subdiv->stats, SUBDIV_STATS_EVALUATOR_CREATE);
		subdiv->evaluator = openSubdiv_createEvaluatorFromTopologyRefiner(
		        subdiv->topology_refiner);
		BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_EVALUATOR_CREATE);
	}
	else {
		/* TODO(sergey): Check for topology change. */
	}
#else
	UNUSED_VARS(subdiv);
#endif
}

#ifdef WITH_OPENSUBDIV
static void set_coarse_positions(Subdiv *subdiv, const Mesh *mesh)
{
	const MVert *mvert = mesh->mvert;
	const MLoop *mloop = mesh->mloop;
	const MPoly *mpoly = mesh->mpoly;
	/* Mark vertices which needs new coordinates. */
	/* TODO(sergey): This is annoying to calculate this on every update,
	 * maybe it's better to cache this mapping. Or make it possible to have
	 * OpenSubdiv's vertices match mesh ones?
	 */
	BLI_bitmap *vertex_used_map =
	        BLI_BITMAP_NEW(mesh->totvert, "vert used map");
	for (int poly_index = 0; poly_index < mesh->totpoly; poly_index++) {
		const MPoly *poly = &mpoly[poly_index];
		for (int corner = 0; corner < poly->totloop; corner++) {
			const MLoop *loop = &mloop[poly->loopstart + corner];
			BLI_BITMAP_ENABLE(vertex_used_map, loop->v);
		}
	}
	for (int vertex_index = 0, manifold_veretx_index = 0;
	     vertex_index < mesh->totvert;
	     vertex_index++)
	{
		if (!BLI_BITMAP_TEST_BOOL(vertex_used_map, vertex_index)) {
			continue;
		}
		const MVert *vertex = &mvert[vertex_index];
		subdiv->evaluator->setCoarsePositions(
		        subdiv->evaluator,
		        vertex->co,
		        manifold_veretx_index, 1);
		manifold_veretx_index++;
	}
	MEM_freeN(vertex_used_map);
}

static void set_face_varying_data_from_uv(Subdiv *subdiv,
                                          const MLoopUV *mloopuv,
                                          const int layer_index)
{
	OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
	OpenSubdiv_Evaluator *evaluator = subdiv->evaluator;
	const int num_faces = topology_refiner->getNumFaces(topology_refiner);
	const MLoopUV *mluv = mloopuv;
	/* TODO(sergey): OpenSubdiv's C-API converter can change winding of
	 * loops of a face, need to watch for that, to prevent wrong UVs assigned.
	 */
	for (int face_index = 0; face_index < num_faces; ++face_index) {
		const int num_face_vertices = topology_refiner->getNumFaceVertices(
		        topology_refiner, face_index);
		const int *uv_indicies = topology_refiner->getFaceFVarValueIndices(
		        topology_refiner, face_index, layer_index);
		for (int vertex_index = 0;
		     vertex_index < num_face_vertices;
		     vertex_index++, mluv++)
		{
		evaluator->setFaceVaryingData(evaluator,
                                      layer_index,
		                              mluv->uv,
		                              uv_indicies[vertex_index],
		                              1);
		}
	}
}
#endif

void BKE_subdiv_eval_update_from_mesh(Subdiv *subdiv, const Mesh *mesh)
{
#ifdef WITH_OPENSUBDIV
	BKE_subdiv_eval_begin(subdiv);
	if (subdiv->evaluator == NULL) {
		return;
	}
	/* Set coordinates of base mesh vertices. */
	set_coarse_positions(subdiv, mesh);
	/* Set face-varyign data to UV maps. */
	const int num_uv_layers =
	        CustomData_number_of_layers(&mesh->ldata, CD_MLOOPUV);
	for (int layer_index = 0; layer_index < num_uv_layers; layer_index++) {
		const MLoopUV *mloopuv = CustomData_get_layer_n(
		        &mesh->ldata, CD_MLOOPUV, layer_index);
		set_face_varying_data_from_uv(subdiv, mloopuv, layer_index);
	}
	/* Update evaluator to the new coarse geometry. */
	BKE_subdiv_stats_begin(&subdiv->stats, SUBDIV_STATS_EVALUATOR_REFINE);
	subdiv->evaluator->refine(subdiv->evaluator);
	BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_EVALUATOR_REFINE);
#else
	UNUSED_VARS(subdiv, mesh);
#endif
}

/* ========================== Single point queries ========================== */

void BKE_subdiv_eval_limit_point(
        Subdiv *subdiv,
        const int ptex_face_index,
        const float u, const float v,
        float P[3])
{
	BKE_subdiv_eval_limit_point_and_derivatives(subdiv,
	                                            ptex_face_index,
	                                            u, v,
	                                            P, NULL, NULL);
}

void BKE_subdiv_eval_limit_point_and_derivatives(
        Subdiv *subdiv,
        const int ptex_face_index,
        const float u, const float v,
        float P[3], float dPdu[3], float dPdv[3])
{
#ifdef WITH_OPENSUBDIV
	subdiv->evaluator->evaluateLimit(subdiv->evaluator,
	                                 ptex_face_index,
	                                 u, v,
	                                 P, dPdu, dPdv);
#else
	UNUSED_VARS(subdiv, ptex_face_index, u, v, P, dPdu, dPdv);
#endif
}

void BKE_subdiv_eval_limit_point_and_normal(
        Subdiv *subdiv,
        const int ptex_face_index,
        const float u, const float v,
        float P[3], float N[3])
{
	float dPdu[3], dPdv[3];
	BKE_subdiv_eval_limit_point_and_derivatives(subdiv,
	                                            ptex_face_index,
	                                            u, v,
	                                            P, dPdu, dPdv);
	cross_v3_v3v3(N, dPdu, dPdv);
	normalize_v3(N);
}

void BKE_subdiv_eval_limit_point_and_short_normal(
        Subdiv *subdiv,
        const int ptex_face_index,
        const float u, const float v,
        float P[3], short N[3])
{
	float N_float[3];
	BKE_subdiv_eval_limit_point_and_normal(subdiv,
	                                       ptex_face_index,
	                                       u, v,
	                                       P, N_float);
	normal_float_to_short_v3(N, N_float);
}

void BKE_subdiv_eval_face_varying(
        Subdiv *subdiv,
        const int face_varying_channel,
        const int ptex_face_index,
        const float u, const float v,
        float face_varying[2])
{
#ifdef WITH_OPENSUBDIV
	subdiv->evaluator->evaluateFaceVarying(subdiv->evaluator,
	                                       face_varying_channel,
	                                       ptex_face_index,
	                                       u, v,
	                                       face_varying);
#else
	UNUSED_VARS(subdiv, ptex_face_index, u, v, face_varying);
#endif
}

/* ===================  Patch queries at given resolution =================== */

/* Move buffer forward by a given number of bytes. */
static void buffer_apply_offset(void **buffer, const int offset)
{
  *buffer = ((unsigned char *)*buffer) + offset;
}

/* Write given number of floats to the beginning of given buffer.  */
static void buffer_write_float_value(void **buffer,
                                     const float *values_buffer, int num_values)
{
	memcpy(*buffer, values_buffer, sizeof(float) * num_values);
}

/* Similar to above, just operates with short values. */
static void buffer_write_short_value(void **buffer,
                                     const short *values_buffer, int num_values)
{
	memcpy(*buffer, values_buffer, sizeof(short) * num_values);
}

void BKE_subdiv_eval_limit_patch_resolution_point(
        Subdiv *subdiv,
        const int ptex_face_index,
        const int resolution,
        void *buffer, const int offset, const int stride)
{
	buffer_apply_offset(&buffer, offset);
	const float inv_resolution_1 = 1.0f / (float)(resolution - 1);
	for (int y = 0; y < resolution; y++) {
		const float v = y * inv_resolution_1;
		for (int x = 0; x < resolution; x++) {
			const float u = x * inv_resolution_1;
			BKE_subdiv_eval_limit_point(subdiv,
			                            ptex_face_index,
			                            u, v,
			                            buffer);
			buffer_apply_offset(&buffer, stride);
		}
	}
}

void BKE_subdiv_eval_limit_patch_resolution_point_and_derivatives(
        Subdiv *subdiv,
        const int ptex_face_index,
        const int resolution,
        void *point_buffer, const int point_offset, const int point_stride,
        void *du_buffer, const int du_offset, const int du_stride,
        void *dv_buffer, const int dv_offset, const int dv_stride)
{
	buffer_apply_offset(&point_buffer, point_offset);
	buffer_apply_offset(&du_buffer, du_offset);
	buffer_apply_offset(&dv_buffer, dv_offset);
	const float inv_resolution_1 = 1.0f / (float)(resolution - 1);
	for (int y = 0; y < resolution; y++) {
		const float v = y * inv_resolution_1;
		for (int x = 0; x < resolution; x++) {
			const float u = x * inv_resolution_1;
			BKE_subdiv_eval_limit_point_and_derivatives(
			        subdiv,
			        ptex_face_index,
			        u, v,
			        point_buffer, du_buffer, dv_buffer);
			buffer_apply_offset(&point_buffer, point_stride);
			buffer_apply_offset(&du_buffer, du_stride);
			buffer_apply_offset(&dv_buffer, dv_stride);
		}
	}
}

void BKE_subdiv_eval_limit_patch_resolution_point_and_normal(
        Subdiv *subdiv,
        const int ptex_face_index,
        const int resolution,
        void *point_buffer, const int point_offset, const int point_stride,
        void *normal_buffer, const int normal_offset, const int normal_stride)
{
	buffer_apply_offset(&point_buffer, point_offset);
	buffer_apply_offset(&normal_buffer, normal_offset);
	const float inv_resolution_1 = 1.0f / (float)(resolution - 1);
	for (int y = 0; y < resolution; y++) {
		const float v = y * inv_resolution_1;
		for (int x = 0; x < resolution; x++) {
			const float u = x * inv_resolution_1;
			float normal[3];
			BKE_subdiv_eval_limit_point_and_normal(
			        subdiv,
			        ptex_face_index,
			        u, v,
			        point_buffer, normal);
			buffer_write_float_value(&normal_buffer, normal, 3);
			buffer_apply_offset(&point_buffer, point_stride);
			buffer_apply_offset(&normal_buffer, normal_stride);
		}
	}
}

void BKE_subdiv_eval_limit_patch_resolution_point_and_short_normal(
        Subdiv *subdiv,
        const int ptex_face_index,
        const int resolution,
        void *point_buffer, const int point_offset, const int point_stride,
        void *normal_buffer, const int normal_offset, const int normal_stride)
{
	buffer_apply_offset(&point_buffer, point_offset);
	buffer_apply_offset(&normal_buffer, normal_offset);
	const float inv_resolution_1 = 1.0f / (float)(resolution - 1);
	for (int y = 0; y < resolution; y++) {
		const float v = y * inv_resolution_1;
		for (int x = 0; x < resolution; x++) {
			const float u = x * inv_resolution_1;
			short normal[3];
			BKE_subdiv_eval_limit_point_and_short_normal(
			        subdiv,
			        ptex_face_index,
			        u, v,
			        point_buffer, normal);
			buffer_write_short_value(&normal_buffer, normal, 3);
			buffer_apply_offset(&point_buffer, point_stride);
			buffer_apply_offset(&normal_buffer, normal_stride);
		}
	}
}
