/*
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
 */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Mesh;
struct Subdiv;
struct OpenSubdiv_EvaluatorCache;

typedef enum eSubdivEvaluatorType {
  SUBDIV_EVALUATOR_TYPE_CPU,
  SUBDIV_EVALUATOR_TYPE_GLSL_COMPUTE,
} eSubdivEvaluatorType;

/* Returns true if evaluator is ready for use. */
bool BKE_subdiv_eval_begin(struct Subdiv *subdiv,
                           eSubdivEvaluatorType evaluator_type,
                           struct OpenSubdiv_EvaluatorCache *evaluator_cache);

/* coarse_vertex_cos is an optional argument which allows to override coordinates of the coarse
 * mesh. */
bool BKE_subdiv_eval_begin_from_mesh(struct Subdiv *subdiv,
                                     const struct Mesh *mesh,
                                     const float (*coarse_vertex_cos)[3],
                                     eSubdivEvaluatorType evaluator_type,
                                     struct OpenSubdiv_EvaluatorCache *evaluator_cache);
bool BKE_subdiv_eval_refine_from_mesh(struct Subdiv *subdiv,
                                      const struct Mesh *mesh,
                                      const float (*coarse_vertex_cos)[3]);

/* Makes sure displacement evaluator is initialized.
 *
 * NOTE: This function must be called once before evaluating displacement or
 * final surface position. */
void BKE_subdiv_eval_init_displacement(struct Subdiv *subdiv);

/* Single point queries. */

/* Evaluate point at a limit surface, with optional derivatives and normal. */

void BKE_subdiv_eval_limit_point(
    struct Subdiv *subdiv, int ptex_face_index, float u, float v, float r_P[3]);
void BKE_subdiv_eval_limit_point_and_derivatives(struct Subdiv *subdiv,
                                                 int ptex_face_index,
                                                 float u,
                                                 float v,
                                                 float r_P[3],
                                                 float r_dPdu[3],
                                                 float r_dPdv[3]);
void BKE_subdiv_eval_limit_point_and_normal(
    struct Subdiv *subdiv, int ptex_face_index, float u, float v, float r_P[3], float r_N[3]);

/* Evaluate face-varying layer (such as UV). */
void BKE_subdiv_eval_face_varying(struct Subdiv *subdiv,
                                  int face_varying_channel,
                                  int ptex_face_index,
                                  float u,
                                  float v,
                                  float r_face_varying[2]);

/* NOTE: Expects derivatives to be correct.
 *
 * TODO(sergey): This is currently used together with
 * BKE_subdiv_eval_final_point() which can easily evaluate derivatives.
 * Would be nice to have displacement evaluation function which does not require
 * knowing derivatives ahead of a time. */
void BKE_subdiv_eval_displacement(struct Subdiv *subdiv,
                                  int ptex_face_index,
                                  float u,
                                  float v,
                                  const float dPdu[3],
                                  const float dPdv[3],
                                  float r_D[3]);

/* Evaluate point on a limit surface with displacement applied to it. */
void BKE_subdiv_eval_final_point(
    struct Subdiv *subdiv, int ptex_face_index, float u, float v, float r_P[3]);

#ifdef __cplusplus
}
#endif
