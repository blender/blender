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
 * The Original Code is Copyright (C) 2019 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "BKE_subdiv_deform.h"

#include <string.h>

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BKE_customdata.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_eval.h"
#include "BKE_subdiv_foreach.h"
#include "BKE_subdiv_mesh.h"

#include "MEM_guardedalloc.h"

/* ================================================================================================
 * Subdivision context.
 */

typedef struct SubdivDeformContext {
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
} SubdivDeformContext;

static void subdiv_mesh_prepare_accumulator(SubdivDeformContext *ctx, int num_vertices)
{
  if (!ctx->have_displacement) {
    return;
  }
  ctx->accumulated_counters = MEM_calloc_arrayN(
      sizeof(*ctx->accumulated_counters), num_vertices, "subdiv accumulated counters");
}

static void subdiv_mesh_context_free(SubdivDeformContext *ctx)
{
  MEM_SAFE_FREE(ctx->accumulated_counters);
}

/* ================================================================================================
 * Accumulation helpers.
 */

static void subdiv_accumulate_vertex_displacement(SubdivDeformContext *ctx,
                                                  const int ptex_face_index,
                                                  const float u,
                                                  const float v,
                                                  int vertex_index)
{
  Subdiv *subdiv = ctx->subdiv;
  float dummy_P[3], dPdu[3], dPdv[3], D[3];
  BKE_subdiv_eval_limit_point_and_derivatives(subdiv, ptex_face_index, u, v, dummy_P, dPdu, dPdv);
  /* Accumulate displacement if needed. */
  if (ctx->have_displacement) {
    BKE_subdiv_eval_displacement(subdiv, ptex_face_index, u, v, dPdu, dPdv, D);
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

/* ================================================================================================
 * Subdivision callbacks.
 */

static bool subdiv_mesh_topology_info(const SubdivForeachContext *foreach_context,
                                      const int UNUSED(num_vertices),
                                      const int UNUSED(num_edges),
                                      const int UNUSED(num_loops),
                                      const int UNUSED(num_polygons))
{
  SubdivDeformContext *subdiv_context = foreach_context->user_data;
  subdiv_mesh_prepare_accumulator(subdiv_context, subdiv_context->coarse_mesh->totvert);
  return true;
}

static void subdiv_mesh_vertex_every_corner(const SubdivForeachContext *foreach_context,
                                            void *UNUSED(tls),
                                            const int ptex_face_index,
                                            const float u,
                                            const float v,
                                            const int coarse_vertex_index,
                                            const int UNUSED(coarse_poly_index),
                                            const int UNUSED(coarse_corner),
                                            const int UNUSED(subdiv_vertex_index))
{
  SubdivDeformContext *ctx = foreach_context->user_data;
  subdiv_accumulate_vertex_displacement(ctx, ptex_face_index, u, v, coarse_vertex_index);
}

static void subdiv_mesh_vertex_corner(const SubdivForeachContext *foreach_context,
                                      void *UNUSED(tls),
                                      const int ptex_face_index,
                                      const float u,
                                      const float v,
                                      const int coarse_vertex_index,
                                      const int UNUSED(coarse_poly_index),
                                      const int UNUSED(coarse_corner),
                                      const int UNUSED(subdiv_vertex_index))
{
  SubdivDeformContext *ctx = foreach_context->user_data;
  BLI_assert(coarse_vertex_index != ORIGINDEX_NONE);
  BLI_assert(coarse_vertex_index < ctx->num_verts);
  float inv_num_accumulated = 1.0f;
  if (ctx->accumulated_counters != NULL) {
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
  BKE_subdiv_eval_limit_point(ctx->subdiv, ptex_face_index, u, v, vertex_co);
  /* Apply displacement. */
  add_v3_v3(vertex_co, D);
}

/* ================================================================================================
 * Initialization.
 */

static void setup_foreach_callbacks(const SubdivDeformContext *subdiv_context,
                                    SubdivForeachContext *foreach_context)
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

/* ================================================================================================
 * Public entry point.
 */

void BKE_subdiv_deform_coarse_vertices(struct Subdiv *subdiv,
                                       const struct Mesh *coarse_mesh,
                                       float (*vertex_cos)[3],
                                       int num_verts)
{
  BKE_subdiv_stats_begin(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH);
  /* Make sure evaluator is up to date with possible new topology, and that
   * is refined for the new positions of coarse vertices. */
  if (!BKE_subdiv_eval_update_from_mesh(subdiv, coarse_mesh, vertex_cos)) {
    /* This could happen in two situations:
     * - OpenSubdiv is disabled.
     * - Something totally bad happened, and OpenSubdiv rejected our
     *   topology.
     * In either way, we can't safely continue. */
    if (coarse_mesh->totpoly) {
      BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH);
      return;
    }
  }

  /* Initialize subdivion mesh creation context. */
  SubdivDeformContext subdiv_context = {0};
  subdiv_context.coarse_mesh = coarse_mesh;
  subdiv_context.subdiv = subdiv;
  subdiv_context.vertex_cos = vertex_cos;
  subdiv_context.num_verts = num_verts;
  subdiv_context.have_displacement = (subdiv->displacement_evaluator != NULL);

  SubdivForeachContext foreach_context;
  setup_foreach_callbacks(&subdiv_context, &foreach_context);
  foreach_context.user_data = &subdiv_context;

  /* Dummy mesh rasterization settings. */
  SubdivToMeshSettings mesh_settings;
  mesh_settings.resolution = 1;
  mesh_settings.use_optimal_display = false;

  /* Multi-threaded traversal/evaluation. */
  BKE_subdiv_stats_begin(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH_GEOMETRY);
  BKE_subdiv_foreach_subdiv_geometry(subdiv, &foreach_context, &mesh_settings, coarse_mesh);
  BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH_GEOMETRY);

  // BKE_mesh_validate(result, true, true);
  BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH);

  /* Free used memory. */
  subdiv_mesh_context_free(&subdiv_context);
}
