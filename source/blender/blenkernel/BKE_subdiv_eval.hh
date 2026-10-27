/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_span.hh"
struct OpenSubdiv_EvaluatorCache;
struct OpenSubdiv_EvaluatorSettings;
namespace blender {

struct Mesh;
namespace bke::subdiv {

struct Subdiv;

/** A location on a ptex face of the limit surface, see #eval_limit_point(). */
struct PtexCoord {
  int ptex_face_index;
  float2 uv;
};

enum eSubdivEvaluatorType {
  SUBDIV_EVALUATOR_TYPE_CPU,
  SUBDIV_EVALUATOR_TYPE_GPU,
};

/** Returns true if evaluator is ready for use. */
bool eval_begin(Subdiv *subdiv,
                eSubdivEvaluatorType evaluator_type,
                OpenSubdiv_EvaluatorCache *evaluator_cache,
                const OpenSubdiv_EvaluatorSettings *settings);

/**
 * \param coarse_vert_positions: optional span of positions to override the mesh positions.
 */
bool eval_begin_from_mesh(Subdiv *subdiv,
                          const Mesh *mesh,
                          eSubdivEvaluatorType evaluator_type,
                          Span<float3> coarse_vert_positions = {},
                          OpenSubdiv_EvaluatorCache *evaluator_cache = nullptr);
bool eval_refine_from_mesh(Subdiv *subdiv, const Mesh *mesh, Span<float3> coarse_vert_positions);

/**
 * Makes sure displacement evaluator is initialized.
 *
 * \note This function must be called once before evaluating displacement or
 * final surface position.
 */
void eval_init_displacement(Subdiv *subdiv);

/* Single point queries. */

/* Evaluate point at a limit surface, with optional derivatives and normal. */

float3 eval_limit_point(Subdiv *subdiv, int ptex_face_index, float u, float v);
void eval_limit_point_and_derivatives(Subdiv *subdiv,
                                      int ptex_face_index,
                                      float u,
                                      float v,
                                      float3 &r_P,
                                      float3 &r_dPdu,
                                      float3 &r_dPdv);
void eval_limit_point_and_normal(
    Subdiv *subdiv, int ptex_face_index, float u, float v, float3 &r_P, float3 &r_N);

/** Evaluate smoothly interpolated vertex data (such as ORCO). */
void eval_vert_data(Subdiv *subdiv, int ptex_face_index, float u, float v, float r_vert_data[]);

/** Evaluate face-varying layer (such as UV). */
void eval_face_varying(Subdiv *subdiv,
                       int face_varying_channel,
                       int ptex_face_index,
                       float u,
                       float v,
                       float2 &r_face_varying);

/**
 * \note Expects derivatives to be correct.
 *
 * TODO(sergey): This is currently used together with
 * eval_final_point() which can easily evaluate derivatives.
 * Would be nice to have displacement evaluation function which does not require
 * knowing derivatives ahead of a time.
 */
void eval_displacement(Subdiv *subdiv,
                       int ptex_face_index,
                       float u,
                       float v,
                       const float3 &dPdu,
                       const float3 &dPdv,
                       float3 &r_D);

/** Evaluate point on a limit surface with displacement applied to it. */
float3 eval_final_point(Subdiv *subdiv, int ptex_face_index, float u, float v);

/* Batched queries, which have less overhead per point than the single point queries above. */

void eval_limit_points(Subdiv *subdiv, Span<PtexCoord> coords, MutableSpan<float3> r_P);
void eval_limit_points_and_derivatives(Subdiv *subdiv,
                                       Span<PtexCoord> coords,
                                       MutableSpan<float3> r_P,
                                       MutableSpan<float3> r_dPdu,
                                       MutableSpan<float3> r_dPdv);
void eval_final_points(Subdiv *subdiv, Span<PtexCoord> coords, MutableSpan<float3> r_P);

/**
 * Evaluate the displacement at every coordinate, and average it within each group. This is used
 * for the displacement of vertices that are shared by multiple ptex faces.
 */
void eval_displacement_averaged(Subdiv *subdiv,
                                Span<PtexCoord> coords,
                                OffsetIndices<int> groups,
                                MutableSpan<float3> r_D);

/**
 * \param r_vert_data: The vertex data for every coordinate, with the number of vertex data
 * channels of the evaluator for each.
 */
void eval_vert_data(Subdiv *subdiv, Span<PtexCoord> coords, MutableSpan<float> r_vert_data);
void eval_face_varying(Subdiv *subdiv,
                       int face_varying_channel,
                       Span<PtexCoord> coords,
                       MutableSpan<float2> r_face_varying);

}  // namespace bke::subdiv
}  // namespace blender
