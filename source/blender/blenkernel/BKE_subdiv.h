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

#ifndef __BKE_SUBDIV_H__
#define __BKE_SUBDIV_H__

#include "BLI_sys_types.h"

struct Mesh;
struct OpenSubdiv_Converter;
struct OpenSubdiv_Evaluator;
struct OpenSubdiv_TopologyRefiner;

/** \file BKE_subdiv.h
 *  \ingroup bke
 *  \since July 2018
 *  \author Sergey Sharybin
 */

typedef enum {
	SUBDIV_FVAR_LINEAR_INTERPOLATION_NONE,
	SUBDIV_FVAR_LINEAR_INTERPOLATION_CORNERS_ONLY,
	SUBDIV_FVAR_LINEAR_INTERPOLATION_BOUNDARIES,
	SUBDIV_FVAR_LINEAR_INTERPOLATION_ALL,
} eSubdivFVarLinearInterpolation;

typedef struct SubdivSettings {
	bool is_simple;
	bool is_adaptive;
	int level;
	eSubdivFVarLinearInterpolation fvar_linear_interpolation;
} SubdivSettings;

/* NOTE: Order of enumerators MUST match order of values in SubdivStats. */
typedef enum eSubdivStatsValue {
	SUBDIV_STATS_TOPOLOGY_REFINER_CREATION_TIME = 0,
	SUBDIV_STATS_SUBDIV_TO_MESH,
	SUBDIV_STATS_EVALUATOR_CREATE,
	SUBDIV_STATS_EVALUATOR_REFINE,

	NUM_SUBDIV_STATS_VALUES,
} eSubdivStatsValue;

typedef struct SubdivStats {
	union {
		struct {
			/* Time spend on creating topology refiner, which includes time
			 * spend on conversion from Blender data to OpenSubdiv data, and
			 * time spend on topology orientation on OpenSubdiv C-API side.
			 */
			double topology_refiner_creation_time;
			/* Total time spent in BKE_subdiv_to_mesh(). */
			double subdiv_to_mesh_time;
			/* Time spent on evaluator creation from topology refiner. */
			double evaluator_creation_time;
			/* Time spent on evaluator->refine(). */
			double evaluator_refine_time;
		};
		double values_[NUM_SUBDIV_STATS_VALUES];
	};

	/* Per-value timestamp on when corresponding BKE_subdiv_stats_begin() was
	 * called.
	 */
	double begin_timestamp_[NUM_SUBDIV_STATS_VALUES];
} SubdivStats;

typedef struct Subdiv {
	/* Settings this subdivision surface is created for.
	 *
	 * It is read-only after assignment in BKE_subdiv_new_from_FOO().
	 */
	SubdivSettings settings;

	/* Total number of ptex faces on subdivision level 0.
	 *
	 * Ptex face is what is internally used by OpenSubdiv for evaluator. It is
	 * a quad face, which corresponds to Blender's legacy Catmull Clark grids.
	 *
	 * Basically, here is a correspondence between polygons and ptex faces:
	 * - Triangle consists of 3 PTex faces.
	 * - Quad is a single PTex face.
	 * - N-gon is N PTex faces.
	 *
	 * This value is initialized in BKE_subdiv_new_from_FOO() and is read-only
	 * after this.
	 */
	int num_ptex_faces;

	/* Indexed by base face index, element indicates total number of ptex faces
	 * created for preceding base faces.
	 */
	int *face_ptex_offset;

	/* Topology refiner includes all the glue logic to feed Blender side
	 * topology to OpenSubdiv. It can be shared by both evaluator and GL mesh
	 * drawer.
	 */
	struct OpenSubdiv_TopologyRefiner *topology_refiner;

	/* CPU side evaluator. */
	struct OpenSubdiv_Evaluator *evaluator;

	SubdivStats stats;
} Subdiv;

/* =============================== STATISTICS =============================== */

void BKE_subdiv_stats_init(SubdivStats *stats);

void BKE_subdiv_stats_begin(SubdivStats *stats, eSubdivStatsValue value);
void BKE_subdiv_stats_end(SubdivStats *stats, eSubdivStatsValue value);

void BKE_subdiv_stats_print(const SubdivStats *stats);

/* ============================== CONSTRUCTION ============================== */

Subdiv *BKE_subdiv_new_from_converter(const SubdivSettings *settings,
                                      struct OpenSubdiv_Converter *converter);

Subdiv *BKE_subdiv_new_from_mesh(const SubdivSettings *settings,
                                 struct Mesh *mesh);

void BKE_subdiv_free(Subdiv *subdiv);

/* ============================= EVALUATION API ============================= */

void BKE_subdiv_eval_begin(Subdiv *subdiv);
void BKE_subdiv_eval_update_from_mesh(Subdiv *subdiv, const struct Mesh *mesh);

/* Single point queries. */

void BKE_subdiv_eval_limit_point(
        Subdiv *subdiv,
        const int ptex_face_index,
        const float u, const float v,
        float P[3]);
void BKE_subdiv_eval_limit_point_and_derivatives(
        Subdiv *subdiv,
        const int ptex_face_index,
        const float u, const float v,
        float P[3], float dPdu[3], float dPdv[3]);
void BKE_subdiv_eval_limit_point_and_normal(
        Subdiv *subdiv,
        const int ptex_face_index,
        const float u, const float v,
        float P[3], float N[3]);
void BKE_subdiv_eval_limit_point_and_short_normal(
        Subdiv *subdiv,
        const int ptex_face_index,
        const float u, const float v,
        float P[3], short N[3]);

void BKE_subdiv_eval_face_varying(
        Subdiv *subdiv,
        const int ptex_face_index,
        const float u, const float v,
        float varying[2]);

/* Patch queries at given resolution.
 *
 * Will evaluate patch at uniformly distributed (u, v) coordinates on a grid
 * of given resolution, producing resolution^2 evaluation points. The order
 * goes as u in rows, v in columns.
 */

void BKE_subdiv_eval_limit_patch_resolution_point(
        Subdiv *subdiv,
        const int ptex_face_index,
        const int resolution,
        void *buffer, const int offset, const int stride);
void BKE_subdiv_eval_limit_patch_resolution_point_and_derivatives(
        Subdiv *subdiv,
        const int ptex_face_index,
        const int resolution,
        void *point_buffer, const int point_offset, const int point_stride,
        void *du_buffer, const int du_offset, const int du_stride,
        void *dv_buffer, const int dv_offset, const int dv_stride);
void BKE_subdiv_eval_limit_patch_resolution_point_and_normal(
        Subdiv *subdiv,
        const int ptex_face_index,
        const int resolution,
        void *point_buffer, const int point_offset, const int point_stride,
        void *normal_buffer, const int normal_offset, const int normal_stride);
void BKE_subdiv_eval_limit_patch_resolution_point_and_short_normal(
        Subdiv *subdiv,
        const int ptex_face_index,
        const int resolution,
        void *point_buffer, const int point_offset, const int point_stride,
        void *normal_buffer, const int normal_offset, const int normal_stride);

/* =========================== SUBDIV TO MESH API =========================== */

typedef struct SubdivToMeshSettings {
	/* Resolution at which ptex are being evaluated.
	 * This defines how many vertices final mesh will have: every ptex has
	 * resolution^2 vertices.
	 */
	int resolution;
} SubdivToMeshSettings;

/* Create real hi-res mesh from subdivision, all geometry is "real". */
struct Mesh *BKE_subdiv_to_mesh(
        Subdiv *subdiv,
        const SubdivToMeshSettings *settings,
        const struct Mesh *coarse_mesh);

#endif  /* __BKE_SUBDIV_H__ */
