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
struct MultiresModifierData;
struct Object;
struct OpenSubdiv_Converter;
struct OpenSubdiv_Evaluator;
struct OpenSubdiv_TopologyRefiner;
struct Subdiv;

/** \file BKE_subdiv.h
 *  \ingroup bke
 *  \since July 2018
 *  \author Sergey Sharybin
 */

typedef enum {
	SUBDIV_FVAR_LINEAR_INTERPOLATION_NONE,
	SUBDIV_FVAR_LINEAR_INTERPOLATION_CORNERS_ONLY,
	SUBDIV_FVAR_LINEAR_INTERPOLATION_CORNERS_AND_JUNCTIONS,
	SUBDIV_FVAR_LINEAR_INTERPOLATION_CORNERS_JUNCTIONS_AND_CONCAVE,
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
	SUBDIV_STATS_SUBDIV_TO_MESH_GEOMETRY,
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
			/* Geometry (MVert and co) creation time during SUBDIV_TYO_MESH. */
			double subdiv_to_mesh_geometry_time;
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

/* Functor which evaluates dispalcement at a given (u, v) of given ptex face. */
typedef struct SubdivDisplacement {
	/* Return displacement which is to be added to the original coordinate.
	 *
	 * NOTE: This function is supposed to return "continuous" displacement for
	 * each pf PTex faces created for special (non-quad) polygon. This means,
	 * if displacement is stored on per-corner manner (like MDisps for multires)
	 * this is up the displacement implementation to average boundaries of the
	 * displacement grids if needed.
	 *
	 * Averaging of displacement for vertices created for over coarse vertices
	 * and edges is done by subdiv code.
	 */
	void (*eval_displacement)(struct SubdivDisplacement *displacement,
	                          const int ptex_face_index,
	                          const float u, const float v,
	                          const float dPdu[3], const float dPdv[3],
	                          float r_D[3]);

	/* Free the data, not the evaluator itself. */
	void (*free)(struct SubdivDisplacement *displacement);

	void *user_data;
} SubdivDisplacement;

typedef struct Subdiv {
	/* Settings this subdivision surface is created for.
	 *
	 * It is read-only after assignment in BKE_subdiv_new_from_FOO().
	 */
	SubdivSettings settings;
	/* Topology refiner includes all the glue logic to feed Blender side
	 * topology to OpenSubdiv. It can be shared by both evaluator and GL mesh
	 * drawer.
	 */
	struct OpenSubdiv_TopologyRefiner *topology_refiner;
	/* CPU side evaluator. */
	struct OpenSubdiv_Evaluator *evaluator;
	/* Optional displacement evaluator. */
	struct SubdivDisplacement *displacement_evaluator;
	/* Statistics for debugging. */
	SubdivStats stats;
} Subdiv;

/* ================================ HELPERS ================================= */

/* NOTE: uv_smooth is eSubsurfUVSmooth. */
eSubdivFVarLinearInterpolation
BKE_subdiv_fvar_interpolation_from_uv_smooth(int uv_smooth);

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

/* Returns true if evaluator is ready for use. */
bool BKE_subdiv_eval_begin(Subdiv *subdiv);
bool BKE_subdiv_eval_update_from_mesh(Subdiv *subdiv, const struct Mesh *mesh);

/* Single point queries. */

void BKE_subdiv_eval_limit_point(
        Subdiv *subdiv,
        const int ptex_face_index,
        const float u, const float v,
        float r_P[3]);
void BKE_subdiv_eval_limit_point_and_derivatives(
        Subdiv *subdiv,
        const int ptex_face_index,
        const float u, const float v,
        float r_P[3], float r_dPdu[3], float r_dPdv[3]);
void BKE_subdiv_eval_limit_point_and_normal(
        Subdiv *subdiv,
        const int ptex_face_index,
        const float u, const float v,
        float r_P[3], float r_N[3]);
void BKE_subdiv_eval_limit_point_and_short_normal(
        Subdiv *subdiv,
        const int ptex_face_index,
        const float u, const float v,
        float r_P[3], short r_N[3]);

void BKE_subdiv_eval_face_varying(
        Subdiv *subdiv,
        const int face_varying_channel,
        const int ptex_face_index,
        const float u, const float v,
        float r_varying[2]);

/* NOTE: Expects derivatives to be correct.
 *
 * TODO(sergey): This is currently used together with
 * BKE_subdiv_eval_final_point() which cas easily evaluate derivatives.
 * Would be nice to have dispalcement evaluation function which does not require
 * knowing derivatives ahead of a time.
 */
void BKE_subdiv_eval_displacement(
        Subdiv *subdiv,
        const int ptex_face_index,
        const float u, const float v,
		const float dPdu[3], const float dPdv[3],
        float r_D[3]);

void BKE_subdiv_eval_final_point(
        Subdiv *subdiv,
        const int ptex_face_index,
        const float u, const float v,
        float r_P[3]);

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

/* ============================ DISPLACEMENT API ============================ */

void BKE_subdiv_displacement_attach_from_multires(
        Subdiv *subdiv,
        const struct Object *object,
        const struct MultiresModifierData *mmd);

void BKE_subdiv_displacement_detach(Subdiv *subdiv);

#endif  /* __BKE_SUBDIV_H__ */
