/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_subdiv_deform.hh"

#include <cstring>

#include "DNA_mesh_types.h"

#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BKE_customdata.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_eval.hh"
#include "BKE_subdiv_foreach.hh"
#include "BKE_subdiv_mesh.hh"

#include "MEM_guardedalloc.h"

namespace blender::bke::subdiv {

/* -------------------------------------------------------------------- */
/** \name Subdivision context
 * \{ */

struct SubdivDeformContext {
  const Mesh *coarse_mesh;
  Subdiv *subdiv;

  float (*vertex_cos)[3];
  int num_verts;

  /* Accumulated values.
   *
   * Averaging is happening for vertices which correspond to the coarse ones.
   *  This is needed for displacement.
   *
   * Displacement is being accumulated to a vertices coordinates, since those
   * are not needed during traversal of face-vertices vertices. */
  /* Per-subdivided vertex counter of averaged values. */
  int *accumulated_counters;

  bool have_displacement;
};

static void subdiv_mesh_prepare_accumulator(SubdivDeformContext *ctx, int num_vertices)
{
  if (!ctx->have_displacement) {
    return;
  }
  ctx->accumulated_counters = static_cast<int *>(
      MEM_calloc_arrayN(num_vertices, sizeof(*ctx->accumulated_counters), __func__));
}

static void subdiv_mesh_context_free(SubdivDeformContext *ctx)
{
  MEM_SAFE_FREE(ctx->accumulated_counters);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Accumulation helpers
 * \{ */

static void subdiv_accumulate_vertex_displacement(SubdivDeformContext *ctx,
                                                  const int ptex_face_index,
                                                  const float u,
                                                  const float v,
                                                  int vertex_index)
{
  Subdiv *subdiv = ctx->subdiv;
  float dummy_P[3], dPdu[3], dPdv[3], D[3];
  eval_limit_point_and_derivatives(subdiv, ptex_face_index, u, v, dummy_P, dPdu, dPdv);
  /* Accumulate displacement if needed. */
  if (ctx->have_displacement) {
    eval_displacement(subdiv, ptex_face_index, u, v, dPdu, dPdv, D);
    /* NOTE: The storage for vertex coordinates is coming from an external world, not necessarily
     * initialized to zeroes. */
    if (ctx->accumulated_counters[vertex_index] == 0) {
      copy_v3_v3(ctx->vertex_cos[vertex_index], D);
    }
    else {
      add_v3_v3(ctx->vertex_cos[vertex_index], D);
    }
  }
  ++ctx->accumulated_counters[vertex_index];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Subdivision callbacks
 * \{ */

static bool subdiv_mesh_topology_info(const ForeachContext *foreach_context,
                                      const int /*num_vertices*/,
                                      const int /*num_edges*/,
                                      const int /*num_loops*/,
                                      const int /*num_faces*/,
                                      const int * /*subdiv_face_offset*/)
{
  SubdivDeformContext *subdiv_context = static_cast<SubdivDeformContext *>(
      foreach_context->user_data);
  subdiv_mesh_prepare_accumulator(subdiv_context, subdiv_context->coarse_mesh->verts_num);
  return true;
}

static void subdiv_mesh_vertex_every_corner(const ForeachContext *foreach_context,
                                            void * /*tls*/,
                                            const int ptex_face_index,
                                            const float u,
                                            const float v,
                                            const int coarse_vertex_index,
                                            const int /*coarse_face_index*/,
                                            const int /*coarse_corner*/,
                                            const int /*subdiv_vertex_index*/)
{
  SubdivDeformContext *ctx = static_cast<SubdivDeformContext *>(foreach_context->user_data);
  subdiv_accumulate_vertex_displacement(ctx, ptex_face_index, u, v, coarse_vertex_index);
}

static void subdiv_mesh_vertex_corner(const ForeachContext *foreach_context,
                                      void * /*tls*/,
                                      const int ptex_face_index,
                                      const float u,
                                      const float v,
                                      const int coarse_vertex_index,
                                      const int /*coarse_face_index*/,
                                      const int /*coarse_corner*/,
                                      const int /*subdiv_vertex_index*/)
{
  SubdivDeformContext *ctx = static_cast<SubdivDeformContext *>(foreach_context->user_data);
  BLI_assert(coarse_vertex_index != ORIGINDEX_NONE);
  BLI_assert(coarse_vertex_index < ctx->num_verts);
  float inv_num_accumulated = 1.0f;
  if (ctx->accumulated_counters != nullptr) {
    inv_num_accumulated = 1.0f / ctx->accumulated_counters[coarse_vertex_index];
  }
  /* Displacement is accumulated in subdiv vertex position.
   * Needs to be backed up before copying data from original vertex. */
  float D[3] = {0.0f, 0.0f, 0.0f};
  float *vertex_co = ctx->vertex_cos[coarse_vertex_index];
  if (ctx->have_displacement) {
    copy_v3_v3(D, vertex_co);
    mul_v3_fl(D, inv_num_accumulated);
  }
  /* Copy custom data and evaluate position. */
  eval_limit_point(ctx->subdiv, ptex_face_index, u, v, vertex_co);
  /* Apply displacement. */
  add_v3_v3(vertex_co, D);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Initialization
 * \{ */

static void setup_foreach_callbacks(const SubdivDeformContext *subdiv_context,
                                    ForeachContext *foreach_context)
{
  memset(foreach_context, 0, sizeof(*foreach_context));
  /* General information. */
  foreach_context->topology_info = subdiv_mesh_topology_info;
  /* Every boundary geometry. Used for displacement and normals averaging. */
  if (subdiv_context->have_displacement) {
    foreach_context->vertex_every_corner = subdiv_mesh_vertex_every_corner;
  }
  foreach_context->vertex_corner = subdiv_mesh_vertex_corner;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public entry point
 * \{ */

void deform_coarse_vertices(Subdiv *subdiv,
                            const Mesh *coarse_mesh,
                            float (*vertex_cos)[3],
                            int num_verts)
{
  stats_begin(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH);
  /* Make sure evaluator is up to date with possible new topology, and that
   * is refined for the new positions of coarse vertices. */
  if (!eval_begin_from_mesh(subdiv, coarse_mesh, vertex_cos, SUBDIV_EVALUATOR_TYPE_CPU, nullptr)) {
    /* This could happen in two situations:
     * - OpenSubdiv is disabled.
     * - Something totally bad happened, and OpenSubdiv rejected our
     *   topology.
     * In either way, we can't safely continue. */
    if (coarse_mesh->faces_num) {
      stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH);
      return;
    }
  }

  /* Initialize subdivision mesh creation context. */
  SubdivDeformContext subdiv_context = {nullptr};
  subdiv_context.coarse_mesh = coarse_mesh;
  subdiv_context.subdiv = subdiv;
  subdiv_context.vertex_cos = vertex_cos;
  subdiv_context.num_verts = num_verts;
  subdiv_context.have_displacement = (subdiv->displacement_evaluator != nullptr);

  ForeachContext foreach_context;
  setup_foreach_callbacks(&subdiv_context, &foreach_context);
  foreach_context.user_data = &subdiv_context;

  /* Dummy mesh rasterization settings. */
  ToMeshSettings mesh_settings;
  mesh_settings.resolution = 1;
  mesh_settings.use_optimal_display = false;

  /* Multi-threaded traversal/evaluation. */
  stats_begin(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH_GEOMETRY);
  foreach_subdiv_geometry(subdiv, &foreach_context, &mesh_settings, coarse_mesh);
  stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH_GEOMETRY);

  // BKE_mesh_validate(result, true, true);
  stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH);

  /* Free used memory. */
  subdiv_mesh_context_free(&subdiv_context);
}

/** \} */

}  // namespace blender::bke::subdiv
