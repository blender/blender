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

/** \file BKE_subdiv.h
 *  \ingroup bke
 *  \since July 2018
 *  \author Sergey Sharybin
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
struct SubdivToMeshSettings;

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

/* This structure contains everything needed to construct subdivided surface.
 * It does not specify storage, memory layout or anything else.
 * It is possible to create different storages (like, grid based CPU side
 * buffers, GPU subdivision mesh, CPU side fully qualified mesh) from the same
 * Subdiv structure.
 */
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

	/* Cached values, are not supposed to be accessed directly. */
	struct {
		/* Indexed by base face index, element indicates total number of ptex
		 *faces created for preceding base faces.
		 */
		int *face_ptex_offset;
	} cache_;
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

/* ============================ DISPLACEMENT API ============================ */

void BKE_subdiv_displacement_attach_from_multires(
        Subdiv *subdiv,
        const struct Mesh *mesh,
        const struct MultiresModifierData *mmd);

void BKE_subdiv_displacement_detach(Subdiv *subdiv);

/* ============================ TOPOLOGY HELPERS ============================ */

int *BKE_subdiv_face_ptex_offset_get(Subdiv *subdiv);

#endif  /* __BKE_SUBDIV_H__ */
