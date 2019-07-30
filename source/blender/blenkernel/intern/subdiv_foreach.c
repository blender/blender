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

#include "BKE_subdiv_foreach.h"

#include "atomic_ops.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_key_types.h"

#include "BLI_bitmap.h"
#include "BLI_task.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_key.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_mesh.h"

#include "MEM_guardedalloc.h"

/* =============================================================================
 * General helpers.
 */

/* Number of ptex faces for a given polygon. */
BLI_INLINE int num_ptex_faces_per_poly_get(const MPoly *poly)
{
  return (poly->totloop == 4) ? 1 : poly->totloop;
}

BLI_INLINE int num_edges_per_ptex_face_get(const int resolution)
{
  return 2 * (resolution - 1) * resolution;
}

BLI_INLINE int num_inner_edges_per_ptex_face_get(const int resolution)
{
  if (resolution < 2) {
    return 0;
  }
  return (resolution - 2) * resolution + (resolution - 1) * (resolution - 1);
}

/* Number of subdivision polygons per ptex face. */
BLI_INLINE int num_polys_per_ptex_get(const int resolution)
{
  return (resolution - 1) * (resolution - 1);
}

/* Subdivision resolution per given polygon's ptex faces. */
BLI_INLINE int ptex_face_resolution_get(const MPoly *poly, int resolution)
{
  return (poly->totloop == 4) ? (resolution) : ((resolution >> 1) + 1);
}

/* =============================================================================
 * Context which is passed to all threaded tasks.
 */

typedef struct SubdivForeachTaskContext {
  const Mesh *coarse_mesh;
  const SubdivToMeshSettings *settings;
  /* Callbacks. */
  const SubdivForeachContext *foreach_context;
  /* Counters of geometry in subdivided mesh, initialized as a part of
   * offsets calculation.
   */
  int num_subdiv_vertices;
  int num_subdiv_edges;
  int num_subdiv_loops;
  int num_subdiv_polygons;
  /* Offsets of various geometry in the subdivision mesh arrays. */
  int vertices_corner_offset;
  int vertices_edge_offset;
  int vertices_inner_offset;
  int edge_boundary_offset;
  int edge_inner_offset;
  /* Indexed by coarse polygon index, indicates offset in subdivided mesh
   * vertices, edges and polygons arrays, where first element of the poly
   * begins.
   */
  int *subdiv_vertex_offset;
  int *subdiv_edge_offset;
  int *subdiv_polygon_offset;
  /* Indexed by base face index, element indicates total number of ptex faces
   * created for preceding base faces.
   */
  int *face_ptex_offset;
  /* Bitmap indicating whether vertex was used already or not.
   * - During patch evaluation indicates whether coarse vertex was already
   *   evaluated and its position on limit is already known.
   */
  BLI_bitmap *coarse_vertices_used_map;
  /* Bitmap indicating whether edge was used already or not. This includes:
   * - During context initialization it indicates whether subdivided vertices
   *   for corresponding edge were already calculated or not.
   * - During patch evaluation it indicates whether vertices along this edge
   *   were already evaluated.
   */
  BLI_bitmap *coarse_edges_used_map;
} SubdivForeachTaskContext;

/* =============================================================================
 * Threading helpers.
 */

static void *subdiv_foreach_tls_alloc(SubdivForeachTaskContext *ctx)
{
  const SubdivForeachContext *foreach_context = ctx->foreach_context;
  void *tls = NULL;
  if (foreach_context->user_data_tls_size != 0) {
    tls = MEM_mallocN(foreach_context->user_data_tls_size, "tls");
    memcpy(tls, foreach_context->user_data_tls, foreach_context->user_data_tls_size);
  }
  return tls;
}

static void subdiv_foreach_tls_free(SubdivForeachTaskContext *ctx, void *tls)
{
  if (tls == NULL) {
    return;
  }
  if (ctx->foreach_context != NULL) {
    ctx->foreach_context->user_data_tls_free(tls);
  }
  MEM_freeN(tls);
}

/* =============================================================================
 * Initialization.
 */

/* NOTE: Expects edge map to be zeroed. */
static void subdiv_foreach_ctx_count(SubdivForeachTaskContext *ctx)
{
  /* Reset counters. */
  ctx->num_subdiv_vertices = 0;
  ctx->num_subdiv_edges = 0;
  ctx->num_subdiv_loops = 0;
  ctx->num_subdiv_polygons = 0;
  /* Static geometry counters. */
  const int resolution = ctx->settings->resolution;
  const int no_quad_patch_resolution = ((resolution >> 1) + 1);
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const int num_inner_vertices_per_quad = (resolution - 2) * (resolution - 2);
  const int num_inner_vertices_per_noquad_patch = (no_quad_patch_resolution - 2) *
                                                  (no_quad_patch_resolution - 2);
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MLoop *coarse_mloop = coarse_mesh->mloop;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  ctx->num_subdiv_vertices = coarse_mesh->totvert;
  ctx->num_subdiv_edges = coarse_mesh->totedge * (num_subdiv_vertices_per_coarse_edge + 1);
  /* Calculate extra vertices and edges createdd by non-loose geometry. */
  for (int poly_index = 0; poly_index < coarse_mesh->totpoly; poly_index++) {
    const MPoly *coarse_poly = &coarse_mpoly[poly_index];
    const int num_ptex_faces_per_poly = num_ptex_faces_per_poly_get(coarse_poly);
    for (int corner = 0; corner < coarse_poly->totloop; corner++) {
      const MLoop *loop = &coarse_mloop[coarse_poly->loopstart + corner];
      const bool is_edge_used = BLI_BITMAP_TEST_BOOL(ctx->coarse_edges_used_map, loop->e);
      /* Edges which aren't counted yet. */
      if (!is_edge_used) {
        BLI_BITMAP_ENABLE(ctx->coarse_edges_used_map, loop->e);
        ctx->num_subdiv_vertices += num_subdiv_vertices_per_coarse_edge;
      }
    }
    /* Inner vertices of polygon. */
    if (num_ptex_faces_per_poly == 1) {
      ctx->num_subdiv_vertices += num_inner_vertices_per_quad;
      ctx->num_subdiv_edges += num_edges_per_ptex_face_get(resolution - 2) +
                               4 * num_subdiv_vertices_per_coarse_edge;
      ctx->num_subdiv_polygons += num_polys_per_ptex_get(resolution);
    }
    else {
      ctx->num_subdiv_vertices += 1 + num_ptex_faces_per_poly * (no_quad_patch_resolution - 2) +
                                  num_ptex_faces_per_poly * num_inner_vertices_per_noquad_patch;
      ctx->num_subdiv_edges += num_ptex_faces_per_poly *
                               (num_inner_edges_per_ptex_face_get(no_quad_patch_resolution - 1) +
                                (no_quad_patch_resolution - 2) +
                                num_subdiv_vertices_per_coarse_edge);
      if (no_quad_patch_resolution >= 3) {
        ctx->num_subdiv_edges += coarse_poly->totloop;
      }
      ctx->num_subdiv_polygons += num_ptex_faces_per_poly *
                                  num_polys_per_ptex_get(no_quad_patch_resolution);
    }
  }
  /* Calculate extra vertices createdd by loose edges. */
  for (int edge_index = 0; edge_index < coarse_mesh->totedge; edge_index++) {
    if (!BLI_BITMAP_TEST_BOOL(ctx->coarse_edges_used_map, edge_index)) {
      ctx->num_subdiv_vertices += num_subdiv_vertices_per_coarse_edge;
    }
  }
  ctx->num_subdiv_loops = ctx->num_subdiv_polygons * 4;
}

static void subdiv_foreach_ctx_init_offsets(SubdivForeachTaskContext *ctx)
{
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const int resolution = ctx->settings->resolution;
  const int resolution_2 = resolution - 2;
  const int resolution_2_squared = resolution_2 * resolution_2;
  const int no_quad_patch_resolution = ((resolution >> 1) + 1);
  const int num_irregular_vertices_per_patch = (no_quad_patch_resolution - 2) *
                                               (no_quad_patch_resolution - 1);
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const int num_subdiv_edges_per_coarse_edge = resolution - 1;
  /* Constant offsets in arrays. */
  ctx->vertices_corner_offset = 0;
  ctx->vertices_edge_offset = coarse_mesh->totvert;
  ctx->vertices_inner_offset = ctx->vertices_edge_offset +
                               coarse_mesh->totedge * num_subdiv_vertices_per_coarse_edge;
  ctx->edge_boundary_offset = 0;
  ctx->edge_inner_offset = ctx->edge_boundary_offset +
                           coarse_mesh->totedge * num_subdiv_edges_per_coarse_edge;
  /* "Indexed" offsets. */
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  int vertex_offset = 0;
  int edge_offset = 0;
  int polygon_offset = 0;
  for (int poly_index = 0; poly_index < coarse_mesh->totpoly; poly_index++) {
    const MPoly *coarse_poly = &coarse_mpoly[poly_index];
    const int num_ptex_faces_per_poly = num_ptex_faces_per_poly_get(coarse_poly);
    ctx->subdiv_vertex_offset[poly_index] = vertex_offset;
    ctx->subdiv_edge_offset[poly_index] = edge_offset;
    ctx->subdiv_polygon_offset[poly_index] = polygon_offset;
    if (num_ptex_faces_per_poly == 1) {
      vertex_offset += resolution_2_squared;
      edge_offset += num_edges_per_ptex_face_get(resolution - 2) +
                     4 * num_subdiv_vertices_per_coarse_edge;
      polygon_offset += num_polys_per_ptex_get(resolution);
    }
    else {
      vertex_offset += 1 + num_ptex_faces_per_poly * num_irregular_vertices_per_patch;
      edge_offset += num_ptex_faces_per_poly *
                     (num_inner_edges_per_ptex_face_get(no_quad_patch_resolution - 1) +
                      (no_quad_patch_resolution - 2) + num_subdiv_vertices_per_coarse_edge);
      if (no_quad_patch_resolution >= 3) {
        edge_offset += coarse_poly->totloop;
      }
      polygon_offset += num_ptex_faces_per_poly * num_polys_per_ptex_get(no_quad_patch_resolution);
    }
  }
}

static void subdiv_foreach_ctx_init(Subdiv *subdiv, SubdivForeachTaskContext *ctx)
{
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  /* Allocate maps and offsets. */
  ctx->coarse_vertices_used_map = BLI_BITMAP_NEW(coarse_mesh->totvert, "vertices used map");
  ctx->coarse_edges_used_map = BLI_BITMAP_NEW(coarse_mesh->totedge, "edges used map");
  ctx->subdiv_vertex_offset = MEM_malloc_arrayN(
      coarse_mesh->totpoly, sizeof(*ctx->subdiv_vertex_offset), "vertex_offset");
  ctx->subdiv_edge_offset = MEM_malloc_arrayN(
      coarse_mesh->totpoly, sizeof(*ctx->subdiv_edge_offset), "subdiv_edge_offset");
  ctx->subdiv_polygon_offset = MEM_malloc_arrayN(
      coarse_mesh->totpoly, sizeof(*ctx->subdiv_polygon_offset), "subdiv_edge_offset");
  /* Initialize all offsets. */
  subdiv_foreach_ctx_init_offsets(ctx);
  /* Calculate number of geometry in the result subdivision mesh. */
  subdiv_foreach_ctx_count(ctx);
  /* Re-set maps which were used at this step. */
  BLI_bitmap_set_all(ctx->coarse_edges_used_map, false, coarse_mesh->totedge);
  ctx->face_ptex_offset = BKE_subdiv_face_ptex_offset_get(subdiv);
}

static void subdiv_foreach_ctx_free(SubdivForeachTaskContext *ctx)
{
  MEM_freeN(ctx->coarse_vertices_used_map);
  MEM_freeN(ctx->coarse_edges_used_map);
  MEM_freeN(ctx->subdiv_vertex_offset);
  MEM_freeN(ctx->subdiv_edge_offset);
  MEM_freeN(ctx->subdiv_polygon_offset);
}

/* =============================================================================
 * Vertex traversal process.
 */

/* Traversal of corner vertices. They are coming from coarse vertices. */

static void subdiv_foreach_corner_vertices_regular_do(
    SubdivForeachTaskContext *ctx,
    void *tls,
    const MPoly *coarse_poly,
    SubdivForeachVertexFromCornerCb vertex_corner,
    bool check_usage)
{
  const float weights[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MLoop *coarse_mloop = coarse_mesh->mloop;
  const int coarse_poly_index = coarse_poly - coarse_mesh->mpoly;
  const int ptex_face_index = ctx->face_ptex_offset[coarse_poly_index];
  for (int corner = 0; corner < coarse_poly->totloop; corner++) {
    const MLoop *coarse_loop = &coarse_mloop[coarse_poly->loopstart + corner];
    if (check_usage &&
        BLI_BITMAP_TEST_AND_SET_ATOMIC(ctx->coarse_vertices_used_map, coarse_loop->v)) {
      continue;
    }
    const int coarse_vertex_index = coarse_loop->v;
    const int subdiv_vertex_index = ctx->vertices_corner_offset + coarse_vertex_index;
    const float u = weights[corner][0];
    const float v = weights[corner][1];
    vertex_corner(ctx->foreach_context,
                  tls,
                  ptex_face_index,
                  u,
                  v,
                  coarse_vertex_index,
                  coarse_poly_index,
                  0,
                  subdiv_vertex_index);
  }
}

static void subdiv_foreach_corner_vertices_regular(SubdivForeachTaskContext *ctx,
                                                   void *tls,
                                                   const MPoly *coarse_poly)
{
  subdiv_foreach_corner_vertices_regular_do(
      ctx, tls, coarse_poly, ctx->foreach_context->vertex_corner, true);
}

static void subdiv_foreach_corner_vertices_special_do(
    SubdivForeachTaskContext *ctx,
    void *tls,
    const MPoly *coarse_poly,
    SubdivForeachVertexFromCornerCb vertex_corner,
    bool check_usage)
{
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MLoop *coarse_mloop = coarse_mesh->mloop;
  const int coarse_poly_index = coarse_poly - coarse_mesh->mpoly;
  int ptex_face_index = ctx->face_ptex_offset[coarse_poly_index];
  for (int corner = 0; corner < coarse_poly->totloop; corner++, ptex_face_index++) {
    const MLoop *coarse_loop = &coarse_mloop[coarse_poly->loopstart + corner];
    if (check_usage &&
        BLI_BITMAP_TEST_AND_SET_ATOMIC(ctx->coarse_vertices_used_map, coarse_loop->v)) {
      continue;
    }
    const int coarse_vertex_index = coarse_loop->v;
    const int subdiv_vertex_index = ctx->vertices_corner_offset + coarse_vertex_index;
    vertex_corner(ctx->foreach_context,
                  tls,
                  ptex_face_index,
                  0.0f,
                  0.0f,
                  coarse_vertex_index,
                  coarse_poly_index,
                  corner,
                  subdiv_vertex_index);
  }
}

static void subdiv_foreach_corner_vertices_special(SubdivForeachTaskContext *ctx,
                                                   void *tls,
                                                   const MPoly *coarse_poly)
{
  subdiv_foreach_corner_vertices_special_do(
      ctx, tls, coarse_poly, ctx->foreach_context->vertex_corner, true);
}

static void subdiv_foreach_corner_vertices(SubdivForeachTaskContext *ctx,
                                           void *tls,
                                           const MPoly *coarse_poly)
{
  if (coarse_poly->totloop == 4) {
    subdiv_foreach_corner_vertices_regular(ctx, tls, coarse_poly);
  }
  else {
    subdiv_foreach_corner_vertices_special(ctx, tls, coarse_poly);
  }
}

static void subdiv_foreach_every_corner_vertices_regular(SubdivForeachTaskContext *ctx,
                                                         void *tls,
                                                         const MPoly *coarse_poly)
{
  subdiv_foreach_corner_vertices_regular_do(
      ctx, tls, coarse_poly, ctx->foreach_context->vertex_every_corner, false);
}

static void subdiv_foreach_every_corner_vertices_special(SubdivForeachTaskContext *ctx,
                                                         void *tls,
                                                         const MPoly *coarse_poly)
{
  subdiv_foreach_corner_vertices_special_do(
      ctx, tls, coarse_poly, ctx->foreach_context->vertex_every_corner, false);
}

static void subdiv_foreach_every_corner_vertices(SubdivForeachTaskContext *ctx, void *tls)
{
  if (ctx->foreach_context->vertex_every_corner == NULL) {
    return;
  }
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  for (int poly_index = 0; poly_index < coarse_mesh->totpoly; poly_index++) {
    const MPoly *coarse_poly = &coarse_mpoly[poly_index];
    if (coarse_poly->totloop == 4) {
      subdiv_foreach_every_corner_vertices_regular(ctx, tls, coarse_poly);
    }
    else {
      subdiv_foreach_every_corner_vertices_special(ctx, tls, coarse_poly);
    }
  }
}

/* Traverse of edge vertices. They are coming from coarse edges. */

static void subdiv_foreach_edge_vertices_regular_do(SubdivForeachTaskContext *ctx,
                                                    void *tls,
                                                    const MPoly *coarse_poly,
                                                    SubdivForeachVertexFromEdgeCb vertex_edge,
                                                    bool check_usage)
{
  const int resolution = ctx->settings->resolution;
  const int resolution_1 = resolution - 1;
  const float inv_resolution_1 = 1.0f / (float)resolution_1;
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MEdge *coarse_medge = coarse_mesh->medge;
  const MLoop *coarse_mloop = coarse_mesh->mloop;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const int coarse_poly_index = coarse_poly - coarse_mpoly;
  const int poly_index = coarse_poly - coarse_mesh->mpoly;
  const int ptex_face_index = ctx->face_ptex_offset[poly_index];
  for (int corner = 0; corner < coarse_poly->totloop; corner++) {
    const MLoop *coarse_loop = &coarse_mloop[coarse_poly->loopstart + corner];
    const int coarse_edge_index = coarse_loop->e;
    if (check_usage &&
        BLI_BITMAP_TEST_AND_SET_ATOMIC(ctx->coarse_edges_used_map, coarse_edge_index)) {
      continue;
    }
    const MEdge *coarse_edge = &coarse_medge[coarse_edge_index];
    const bool flip = (coarse_edge->v2 == coarse_loop->v);
    int subdiv_vertex_index = ctx->vertices_edge_offset +
                              coarse_edge_index * num_subdiv_vertices_per_coarse_edge;
    for (int vertex_index = 0; vertex_index < num_subdiv_vertices_per_coarse_edge;
         vertex_index++, subdiv_vertex_index++) {
      float fac = (vertex_index + 1) * inv_resolution_1;
      if (flip) {
        fac = 1.0f - fac;
      }
      if (corner >= 2) {
        fac = 1.0f - fac;
      }
      float u, v;
      if ((corner & 1) == 0) {
        u = fac;
        v = (corner == 2) ? 1.0f : 0.0f;
      }
      else {
        u = (corner == 1) ? 1.0f : 0.0f;
        v = fac;
      }
      vertex_edge(ctx->foreach_context,
                  tls,
                  ptex_face_index,
                  u,
                  v,
                  coarse_edge_index,
                  coarse_poly_index,
                  0,
                  subdiv_vertex_index);
    }
  }
}

static void subdiv_foreach_edge_vertices_regular(SubdivForeachTaskContext *ctx,
                                                 void *tls,
                                                 const MPoly *coarse_poly)
{
  subdiv_foreach_edge_vertices_regular_do(
      ctx, tls, coarse_poly, ctx->foreach_context->vertex_edge, true);
}

static void subdiv_foreach_edge_vertices_special_do(SubdivForeachTaskContext *ctx,
                                                    void *tls,
                                                    const MPoly *coarse_poly,
                                                    SubdivForeachVertexFromEdgeCb vertex_edge,
                                                    bool check_usage)
{
  const int resolution = ctx->settings->resolution;
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const int num_vertices_per_ptex_edge = ((resolution >> 1) + 1);
  const float inv_ptex_resolution_1 = 1.0f / (float)(num_vertices_per_ptex_edge - 1);
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MEdge *coarse_medge = coarse_mesh->medge;
  const MLoop *coarse_mloop = coarse_mesh->mloop;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const int coarse_poly_index = coarse_poly - coarse_mpoly;
  const int poly_index = coarse_poly - coarse_mesh->mpoly;
  const int ptex_face_start_index = ctx->face_ptex_offset[poly_index];
  int ptex_face_index = ptex_face_start_index;
  for (int corner = 0; corner < coarse_poly->totloop; corner++, ptex_face_index++) {
    const MLoop *coarse_loop = &coarse_mloop[coarse_poly->loopstart + corner];
    const int coarse_edge_index = coarse_loop->e;
    if (check_usage &&
        BLI_BITMAP_TEST_AND_SET_ATOMIC(ctx->coarse_edges_used_map, coarse_edge_index)) {
      continue;
    }
    const MEdge *coarse_edge = &coarse_medge[coarse_edge_index];
    const bool flip = (coarse_edge->v2 == coarse_loop->v);
    int subdiv_vertex_index = ctx->vertices_edge_offset +
                              coarse_edge_index * num_subdiv_vertices_per_coarse_edge;
    int veretx_delta = 1;
    if (flip) {
      subdiv_vertex_index += num_subdiv_vertices_per_coarse_edge - 1;
      veretx_delta = -1;
    }
    for (int vertex_index = 1; vertex_index < num_vertices_per_ptex_edge;
         vertex_index++, subdiv_vertex_index += veretx_delta) {
      const float u = vertex_index * inv_ptex_resolution_1;
      vertex_edge(ctx->foreach_context,
                  tls,
                  ptex_face_index,
                  u,
                  0.0f,
                  coarse_edge_index,
                  coarse_poly_index,
                  corner,
                  subdiv_vertex_index);
    }
    const int next_corner = (corner + 1) % coarse_poly->totloop;
    const int next_ptex_face_index = ptex_face_start_index + next_corner;
    for (int vertex_index = 1; vertex_index < num_vertices_per_ptex_edge - 1;
         vertex_index++, subdiv_vertex_index += veretx_delta) {
      const float v = 1.0f - vertex_index * inv_ptex_resolution_1;
      vertex_edge(ctx->foreach_context,
                  tls,
                  next_ptex_face_index,
                  0.0f,
                  v,
                  coarse_edge_index,
                  coarse_poly_index,
                  next_corner,
                  subdiv_vertex_index);
    }
  }
}

static void subdiv_foreach_edge_vertices_special(SubdivForeachTaskContext *ctx,
                                                 void *tls,
                                                 const MPoly *coarse_poly)
{
  subdiv_foreach_edge_vertices_special_do(
      ctx, tls, coarse_poly, ctx->foreach_context->vertex_edge, true);
}

static void subdiv_foreach_edge_vertices(SubdivForeachTaskContext *ctx,
                                         void *tls,
                                         const MPoly *coarse_poly)
{
  if (coarse_poly->totloop == 4) {
    subdiv_foreach_edge_vertices_regular(ctx, tls, coarse_poly);
  }
  else {
    subdiv_foreach_edge_vertices_special(ctx, tls, coarse_poly);
  }
}

static void subdiv_foreach_every_edge_vertices_regular(SubdivForeachTaskContext *ctx,
                                                       void *tls,
                                                       const MPoly *coarse_poly)
{
  subdiv_foreach_edge_vertices_regular_do(
      ctx, tls, coarse_poly, ctx->foreach_context->vertex_every_edge, false);
}

static void subdiv_foreach_every_edge_vertices_special(SubdivForeachTaskContext *ctx,
                                                       void *tls,
                                                       const MPoly *coarse_poly)
{
  subdiv_foreach_edge_vertices_special_do(
      ctx, tls, coarse_poly, ctx->foreach_context->vertex_every_edge, false);
}

static void subdiv_foreach_every_edge_vertices(SubdivForeachTaskContext *ctx, void *tls)
{
  if (ctx->foreach_context->vertex_every_edge == NULL) {
    return;
  }
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  for (int poly_index = 0; poly_index < coarse_mesh->totpoly; poly_index++) {
    const MPoly *coarse_poly = &coarse_mpoly[poly_index];
    if (coarse_poly->totloop == 4) {
      subdiv_foreach_every_edge_vertices_regular(ctx, tls, coarse_poly);
    }
    else {
      subdiv_foreach_every_edge_vertices_special(ctx, tls, coarse_poly);
    }
  }
}

/* Traversal of inner vertices, they are coming from ptex patches. */

static void subdiv_foreach_inner_vertices_regular(SubdivForeachTaskContext *ctx,
                                                  void *tls,
                                                  const MPoly *coarse_poly)
{
  const int resolution = ctx->settings->resolution;
  const float inv_resolution_1 = 1.0f / (float)(resolution - 1);
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const int coarse_poly_index = coarse_poly - coarse_mesh->mpoly;
  const int ptex_face_index = ctx->face_ptex_offset[coarse_poly_index];
  const int start_vertex_index = ctx->subdiv_vertex_offset[coarse_poly_index];
  int subdiv_vertex_index = ctx->vertices_inner_offset + start_vertex_index;
  for (int y = 1; y < resolution - 1; y++) {
    const float v = y * inv_resolution_1;
    for (int x = 1; x < resolution - 1; x++, subdiv_vertex_index++) {
      const float u = x * inv_resolution_1;
      ctx->foreach_context->vertex_inner(ctx->foreach_context,
                                         tls,
                                         ptex_face_index,
                                         u,
                                         v,
                                         coarse_poly_index,
                                         0,
                                         subdiv_vertex_index);
    }
  }
}

static void subdiv_foreach_inner_vertices_special(SubdivForeachTaskContext *ctx,
                                                  void *tls,
                                                  const MPoly *coarse_poly)
{
  const int resolution = ctx->settings->resolution;
  const int ptex_face_resolution = ptex_face_resolution_get(coarse_poly, resolution);
  const float inv_ptex_face_resolution_1 = 1.0f / (float)(ptex_face_resolution - 1);
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const int coarse_poly_index = coarse_poly - coarse_mesh->mpoly;
  int ptex_face_index = ctx->face_ptex_offset[coarse_poly_index];
  const int start_vertex_index = ctx->subdiv_vertex_offset[coarse_poly_index];
  int subdiv_vertex_index = ctx->vertices_inner_offset + start_vertex_index;
  ctx->foreach_context->vertex_inner(ctx->foreach_context,
                                     tls,
                                     ptex_face_index,
                                     1.0f,
                                     1.0f,
                                     coarse_poly_index,
                                     0,
                                     subdiv_vertex_index);
  subdiv_vertex_index++;
  for (int corner = 0; corner < coarse_poly->totloop; corner++, ptex_face_index++) {
    for (int y = 1; y < ptex_face_resolution - 1; y++) {
      const float v = y * inv_ptex_face_resolution_1;
      for (int x = 1; x < ptex_face_resolution; x++, subdiv_vertex_index++) {
        const float u = x * inv_ptex_face_resolution_1;
        ctx->foreach_context->vertex_inner(ctx->foreach_context,
                                           tls,
                                           ptex_face_index,
                                           u,
                                           v,
                                           coarse_poly_index,
                                           corner,
                                           subdiv_vertex_index);
      }
    }
  }
}

static void subdiv_foreach_inner_vertices(SubdivForeachTaskContext *ctx,
                                          void *tls,
                                          const MPoly *coarse_poly)
{
  if (coarse_poly->totloop == 4) {
    subdiv_foreach_inner_vertices_regular(ctx, tls, coarse_poly);
  }
  else {
    subdiv_foreach_inner_vertices_special(ctx, tls, coarse_poly);
  }
}

/* Traverse all vertices which are emitted from given coarse polygon. */
static void subdiv_foreach_vertices(SubdivForeachTaskContext *ctx, void *tls, const int poly_index)
{
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const MPoly *coarse_poly = &coarse_mpoly[poly_index];
  if (ctx->foreach_context->vertex_inner != NULL) {
    subdiv_foreach_inner_vertices(ctx, tls, coarse_poly);
  }
}

/* =============================================================================
 * Edge traversal process.
 */

/* TODO(sergey): Coarse edge are always NONE, consider getting rid of it. */
static int subdiv_foreach_edges_row(SubdivForeachTaskContext *ctx,
                                    void *tls,
                                    const int coarse_edge_index,
                                    const int start_subdiv_edge_index,
                                    const int start_vertex_index,
                                    const int num_edges_per_row)
{
  int subdiv_edge_index = start_subdiv_edge_index;
  int vertex_index = start_vertex_index;
  for (int edge_index = 0; edge_index < num_edges_per_row - 1; edge_index++, subdiv_edge_index++) {
    const int v1 = vertex_index;
    const int v2 = vertex_index + 1;
    ctx->foreach_context->edge(
        ctx->foreach_context, tls, coarse_edge_index, subdiv_edge_index, v1, v2);
    vertex_index += 1;
  }
  return subdiv_edge_index;
}

/* TODO(sergey): Coarse edges are always NONE, consider getting rid of them. */
static int subdiv_foreach_edges_column(SubdivForeachTaskContext *ctx,
                                       void *tls,
                                       const int coarse_start_edge_index,
                                       const int coarse_end_edge_index,
                                       const int start_subdiv_edge_index,
                                       const int start_vertex_index,
                                       const int num_edges_per_row)
{
  int subdiv_edge_index = start_subdiv_edge_index;
  int vertex_index = start_vertex_index;
  for (int edge_index = 0; edge_index < num_edges_per_row; edge_index++, subdiv_edge_index++) {
    int coarse_edge_index = ORIGINDEX_NONE;
    if (edge_index == 0) {
      coarse_edge_index = coarse_start_edge_index;
    }
    else if (edge_index == num_edges_per_row - 1) {
      coarse_edge_index = coarse_end_edge_index;
    }
    const int v1 = vertex_index;
    const int v2 = vertex_index + num_edges_per_row;
    ctx->foreach_context->edge(
        ctx->foreach_context, tls, coarse_edge_index, subdiv_edge_index, v1, v2);
    vertex_index += 1;
  }
  return subdiv_edge_index;
}

/* Defines edges between inner vertices of patch, and also edges to the
 * boundary.
 */

/* Consider a subdivision of base face at level 1:
 *
 *  y
 *  ^
 *  |   (6) ---- (7) ---- (8)
 *  |    |        |        |
 *  |   (3) ---- (4) ---- (5)
 *  |    |        |        |
 *  |   (0) ---- (1) ---- (2)
 *  o---------------------------> x
 *
 * This is illustrate which parts of geometry is created by code below.
 */

static void subdiv_foreach_edges_all_patches_regular(SubdivForeachTaskContext *ctx,
                                                     void *tls,
                                                     const MPoly *coarse_poly)
{
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MEdge *coarse_medge = coarse_mesh->medge;
  const MLoop *coarse_mloop = coarse_mesh->mloop;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const int poly_index = coarse_poly - coarse_mpoly;
  const int resolution = ctx->settings->resolution;
  const int start_vertex_index = ctx->vertices_inner_offset +
                                 ctx->subdiv_vertex_offset[poly_index];
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  int subdiv_edge_index = ctx->edge_inner_offset + ctx->subdiv_edge_offset[poly_index];
  /* Traverse bottom row of edges (0-1, 1-2). */
  subdiv_edge_index = subdiv_foreach_edges_row(
      ctx, tls, ORIGINDEX_NONE, subdiv_edge_index, start_vertex_index, resolution - 2);
  /* Traverse remaining edges. */
  for (int row = 0; row < resolution - 3; row++) {
    const int start_row_vertex_index = start_vertex_index + row * (resolution - 2);
    /* Traverse vertical columns.
     *
     * At first iteration it will be edges (0-3. 1-4, 2-5), then it
     * will be (3-6, 4-7, 5-8) and so on.
     */
    subdiv_edge_index = subdiv_foreach_edges_column(ctx,
                                                    tls,
                                                    ORIGINDEX_NONE,
                                                    ORIGINDEX_NONE,
                                                    subdiv_edge_index,
                                                    start_row_vertex_index,
                                                    resolution - 2);
    /* Create horizontal edge row.
     *
     * At first iteration it will be edges (3-4, 4-5), then it will be
     * (6-7, 7-8) and so on.
     */
    subdiv_edge_index = subdiv_foreach_edges_row(ctx,
                                                 tls,
                                                 ORIGINDEX_NONE,
                                                 subdiv_edge_index,
                                                 start_row_vertex_index + resolution - 2,
                                                 resolution - 2);
  }
  /* Connect inner part of patch to boundary. */
  for (int corner = 0; corner < coarse_poly->totloop; corner++) {
    const MLoop *coarse_loop = &coarse_mloop[coarse_poly->loopstart + corner];
    const MEdge *coarse_edge = &coarse_medge[coarse_loop->e];
    const int start_edge_vertex = ctx->vertices_edge_offset +
                                  coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
    const bool flip = (coarse_edge->v2 == coarse_loop->v);
    int side_start_index = start_vertex_index;
    int side_stride = 0;
    /* Calculate starting veretx of corresponding inner part of ptex. */
    if (corner == 0) {
      side_stride = 1;
    }
    else if (corner == 1) {
      side_start_index += resolution - 3;
      side_stride = resolution - 2;
    }
    else if (corner == 2) {
      side_start_index += num_subdiv_vertices_per_coarse_edge *
                              num_subdiv_vertices_per_coarse_edge -
                          1;
      side_stride = -1;
    }
    else if (corner == 3) {
      side_start_index += num_subdiv_vertices_per_coarse_edge *
                          (num_subdiv_vertices_per_coarse_edge - 1);
      side_stride = -(resolution - 2);
    }
    for (int i = 0; i < resolution - 2; i++, subdiv_edge_index++) {
      const int v1 = (flip) ? (start_edge_vertex + (resolution - i - 3)) : (start_edge_vertex + i);
      const int v2 = side_start_index + side_stride * i;
      ctx->foreach_context->edge(
          ctx->foreach_context, tls, ORIGINDEX_NONE, subdiv_edge_index, v1, v2);
    }
  }
}

static void subdiv_foreach_edges_all_patches_special(SubdivForeachTaskContext *ctx,
                                                     void *tls,
                                                     const MPoly *coarse_poly)
{
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MEdge *coarse_medge = coarse_mesh->medge;
  const MLoop *coarse_mloop = coarse_mesh->mloop;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const int poly_index = coarse_poly - coarse_mpoly;
  const int resolution = ctx->settings->resolution;
  const int ptex_face_resolution = ptex_face_resolution_get(coarse_poly, resolution);
  const int ptex_face_inner_resolution = ptex_face_resolution - 2;
  const int num_inner_vertices_per_ptex = (ptex_face_resolution - 1) * (ptex_face_resolution - 2);
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const int center_vertex_index = ctx->vertices_inner_offset +
                                  ctx->subdiv_vertex_offset[poly_index];
  const int start_vertex_index = center_vertex_index + 1;
  int subdiv_edge_index = ctx->edge_inner_offset + ctx->subdiv_edge_offset[poly_index];
  /* Traverse inner ptex edges. */
  for (int corner = 0; corner < coarse_poly->totloop; corner++) {
    const int start_ptex_face_vertex_index = start_vertex_index +
                                             corner * num_inner_vertices_per_ptex;
    /* Similar steps to regular patch case. */
    subdiv_edge_index = subdiv_foreach_edges_row(ctx,
                                                 tls,
                                                 ORIGINDEX_NONE,
                                                 subdiv_edge_index,
                                                 start_ptex_face_vertex_index,
                                                 ptex_face_inner_resolution + 1);
    for (int row = 0; row < ptex_face_inner_resolution - 1; row++) {
      const int start_row_vertex_index = start_ptex_face_vertex_index +
                                         row * (ptex_face_inner_resolution + 1);
      subdiv_edge_index = subdiv_foreach_edges_column(ctx,
                                                      tls,
                                                      ORIGINDEX_NONE,
                                                      ORIGINDEX_NONE,
                                                      subdiv_edge_index,
                                                      start_row_vertex_index,
                                                      ptex_face_inner_resolution + 1);
      subdiv_edge_index = subdiv_foreach_edges_row(ctx,
                                                   tls,
                                                   ORIGINDEX_NONE,
                                                   subdiv_edge_index,
                                                   start_row_vertex_index +
                                                       ptex_face_inner_resolution + 1,
                                                   ptex_face_inner_resolution + 1);
    }
  }
  /* Create connections between ptex faces. */
  for (int corner = 0; corner < coarse_poly->totloop; corner++) {
    const int next_corner = (corner + 1) % coarse_poly->totloop;
    int current_patch_vertex_index = start_vertex_index + corner * num_inner_vertices_per_ptex +
                                     ptex_face_inner_resolution;
    int next_path_vertex_index = start_vertex_index + next_corner * num_inner_vertices_per_ptex +
                                 num_inner_vertices_per_ptex - ptex_face_resolution + 1;
    for (int row = 0; row < ptex_face_inner_resolution; row++, subdiv_edge_index++) {
      const int v1 = current_patch_vertex_index;
      const int v2 = next_path_vertex_index;
      ctx->foreach_context->edge(
          ctx->foreach_context, tls, ORIGINDEX_NONE, subdiv_edge_index, v1, v2);
      current_patch_vertex_index += ptex_face_inner_resolution + 1;
      next_path_vertex_index += 1;
    }
  }
  /* Create edges from center. */
  if (ptex_face_resolution >= 3) {
    for (int corner = 0; corner < coarse_poly->totloop; corner++, subdiv_edge_index++) {
      const int current_patch_end_vertex_index = start_vertex_index +
                                                 corner * num_inner_vertices_per_ptex +
                                                 num_inner_vertices_per_ptex - 1;
      const int v1 = center_vertex_index;
      const int v2 = current_patch_end_vertex_index;
      ctx->foreach_context->edge(
          ctx->foreach_context, tls, ORIGINDEX_NONE, subdiv_edge_index, v1, v2);
    }
  }
  /* Connect inner path of patch to boundary. */
  const MLoop *prev_coarse_loop = &coarse_mloop[coarse_poly->loopstart + coarse_poly->totloop - 1];
  for (int corner = 0; corner < coarse_poly->totloop; corner++) {
    const MLoop *coarse_loop = &coarse_mloop[coarse_poly->loopstart + corner];
    {
      const MEdge *coarse_edge = &coarse_medge[coarse_loop->e];
      const int start_edge_vertex = ctx->vertices_edge_offset +
                                    coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
      const bool flip = (coarse_edge->v2 == coarse_loop->v);
      int side_start_index;
      if (ptex_face_resolution >= 3) {
        side_start_index = start_vertex_index + num_inner_vertices_per_ptex * corner;
      }
      else {
        side_start_index = center_vertex_index;
      }
      for (int i = 0; i < ptex_face_resolution - 1; i++, subdiv_edge_index++) {
        const int v1 = (flip) ? (start_edge_vertex + (resolution - i - 3)) :
                                (start_edge_vertex + i);
        const int v2 = side_start_index + i;
        ctx->foreach_context->edge(
            ctx->foreach_context, tls, ORIGINDEX_NONE, subdiv_edge_index, v1, v2);
      }
    }
    if (ptex_face_resolution >= 3) {
      const MEdge *coarse_edge = &coarse_medge[prev_coarse_loop->e];
      const int start_edge_vertex = ctx->vertices_edge_offset +
                                    prev_coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
      const bool flip = (coarse_edge->v2 == coarse_loop->v);
      int side_start_index = start_vertex_index + num_inner_vertices_per_ptex * corner;
      for (int i = 0; i < ptex_face_resolution - 2; i++, subdiv_edge_index++) {
        const int v1 = (flip) ? (start_edge_vertex + (resolution - i - 3)) :
                                (start_edge_vertex + i);
        const int v2 = side_start_index + (ptex_face_inner_resolution + 1) * i;
        ctx->foreach_context->edge(
            ctx->foreach_context, tls, ORIGINDEX_NONE, subdiv_edge_index, v1, v2);
      }
    }
    prev_coarse_loop = coarse_loop;
  }
}

static void subdiv_foreach_edges_all_patches(SubdivForeachTaskContext *ctx,
                                             void *tls,
                                             const MPoly *coarse_poly)
{
  if (coarse_poly->totloop == 4) {
    subdiv_foreach_edges_all_patches_regular(ctx, tls, coarse_poly);
  }
  else {
    subdiv_foreach_edges_all_patches_special(ctx, tls, coarse_poly);
  }
}

static void subdiv_foreach_edges(SubdivForeachTaskContext *ctx, void *tls, int poly_index)
{
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const MPoly *coarse_poly = &coarse_mpoly[poly_index];
  subdiv_foreach_edges_all_patches(ctx, tls, coarse_poly);
}

static void subdiv_foreach_boundary_edges(SubdivForeachTaskContext *ctx,
                                          void *tls,
                                          int coarse_edge_index)
{
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MEdge *coarse_medge = coarse_mesh->medge;
  const MEdge *coarse_edge = &coarse_medge[coarse_edge_index];
  const int resolution = ctx->settings->resolution;
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const int num_subdiv_edges_per_coarse_edge = resolution - 1;
  int subdiv_edge_index = ctx->edge_boundary_offset +
                          coarse_edge_index * num_subdiv_edges_per_coarse_edge;
  int last_vertex_index = ctx->vertices_corner_offset + coarse_edge->v1;
  for (int i = 0; i < num_subdiv_edges_per_coarse_edge - 1; i++, subdiv_edge_index++) {
    const int v1 = last_vertex_index;
    const int v2 = ctx->vertices_edge_offset +
                   coarse_edge_index * num_subdiv_vertices_per_coarse_edge + i;
    ctx->foreach_context->edge(
        ctx->foreach_context, tls, coarse_edge_index, subdiv_edge_index, v1, v2);
    last_vertex_index = v2;
  }
  const int v1 = last_vertex_index;
  const int v2 = ctx->vertices_corner_offset + coarse_edge->v2;
  ctx->foreach_context->edge(
      ctx->foreach_context, tls, coarse_edge_index, subdiv_edge_index, v1, v2);
}

/* =============================================================================
 * Loops traversal.
 */

static void rotate_indices(const int rot, int *a, int *b, int *c, int *d)
{
  int values[4] = {*a, *b, *c, *d};
  *a = values[(0 - rot + 4) % 4];
  *b = values[(1 - rot + 4) % 4];
  *c = values[(2 - rot + 4) % 4];
  *d = values[(3 - rot + 4) % 4];
}

static void subdiv_foreach_loops_of_poly(SubdivForeachTaskContext *ctx,
                                         void *tls,
                                         int subdiv_loop_start_index,
                                         const int ptex_face_index,
                                         const int coarse_poly_index,
                                         const int coarse_corner_index,
                                         const int rotation,
                                         /*const*/ int v0,
                                         /*const*/ int e0,
                                         /*const*/ int v1,
                                         /*const*/ int e1,
                                         /*const*/ int v2,
                                         /*const*/ int e2,
                                         /*const*/ int v3,
                                         /*const*/ int e3,
                                         const float u,
                                         const float v,
                                         const float du,
                                         const float dv)
{
  rotate_indices(rotation, &v0, &v1, &v2, &v3);
  rotate_indices(rotation, &e0, &e1, &e2, &e3);
  ctx->foreach_context->loop(ctx->foreach_context,
                             tls,
                             ptex_face_index,
                             u,
                             v,
                             ORIGINDEX_NONE,
                             coarse_poly_index,
                             coarse_corner_index,
                             subdiv_loop_start_index + 0,
                             v0,
                             e0);
  ctx->foreach_context->loop(ctx->foreach_context,
                             tls,
                             ptex_face_index,
                             u + du,
                             v,
                             ORIGINDEX_NONE,
                             coarse_poly_index,
                             coarse_corner_index,
                             subdiv_loop_start_index + 1,
                             v1,
                             e1);
  ctx->foreach_context->loop(ctx->foreach_context,
                             tls,
                             ptex_face_index,
                             u + du,
                             v + dv,
                             ORIGINDEX_NONE,
                             coarse_poly_index,
                             coarse_corner_index,
                             subdiv_loop_start_index + 2,
                             v2,
                             e2);
  ctx->foreach_context->loop(ctx->foreach_context,
                             tls,
                             ptex_face_index,
                             u,
                             v + dv,
                             ORIGINDEX_NONE,
                             coarse_poly_index,
                             coarse_corner_index,
                             subdiv_loop_start_index + 3,
                             v3,
                             e3);
}

static void subdiv_foreach_loops_regular(SubdivForeachTaskContext *ctx,
                                         void *tls,
                                         const MPoly *coarse_poly)
{
  const int resolution = ctx->settings->resolution;
  /* Base/coarse mesh information. */
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MEdge *coarse_medge = coarse_mesh->medge;
  const MLoop *coarse_mloop = coarse_mesh->mloop;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const int coarse_poly_index = coarse_poly - coarse_mpoly;
  const int ptex_resolution = ptex_face_resolution_get(coarse_poly, resolution);
  const int ptex_inner_resolution = ptex_resolution - 2;
  const int num_subdiv_edges_per_coarse_edge = resolution - 1;
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const float inv_ptex_resolution_1 = 1.0f / (float)(ptex_resolution - 1);
  const int ptex_face_index = ctx->face_ptex_offset[coarse_poly_index];
  const int start_vertex_index = ctx->vertices_inner_offset +
                                 ctx->subdiv_vertex_offset[coarse_poly_index];
  const int start_edge_index = ctx->edge_inner_offset + ctx->subdiv_edge_offset[coarse_poly_index];
  const int start_poly_index = ctx->subdiv_polygon_offset[coarse_poly_index];
  const int start_loop_index = 4 * start_poly_index;
  const float du = inv_ptex_resolution_1;
  const float dv = inv_ptex_resolution_1;
  /* Hi-poly subdivided mesh. */
  int subdiv_loop_index = start_loop_index;
  /* Loops for inner part of ptex. */
  for (int y = 1; y < ptex_resolution - 2; y++) {
    const float v = y * inv_ptex_resolution_1;
    const int inner_y = y - 1;
    for (int x = 1; x < ptex_resolution - 2; x++, subdiv_loop_index += 4) {
      const int inner_x = x - 1;
      const float u = x * inv_ptex_resolution_1;
      /* Vertex indices ordered counter-clockwise. */
      const int v0 = start_vertex_index + (inner_y * ptex_inner_resolution + inner_x);
      const int v1 = v0 + 1;
      const int v2 = v0 + ptex_inner_resolution + 1;
      const int v3 = v0 + ptex_inner_resolution;
      /* Edge indices ordered counter-clockwise. */
      const int e0 = start_edge_index + (inner_y * (2 * ptex_inner_resolution - 1) + inner_x);
      const int e1 = e0 + ptex_inner_resolution;
      const int e2 = e0 + (2 * ptex_inner_resolution - 1);
      const int e3 = e0 + ptex_inner_resolution - 1;
      subdiv_foreach_loops_of_poly(ctx,
                                   tls,
                                   subdiv_loop_index,
                                   ptex_face_index,
                                   coarse_poly_index,
                                   0,
                                   0,
                                   v0,
                                   e0,
                                   v1,
                                   e1,
                                   v2,
                                   e2,
                                   v3,
                                   e3,
                                   u,
                                   v,
                                   du,
                                   dv);
    }
  }
  /* Loops for faces connecting inner ptex part with boundary. */
  const MLoop *prev_coarse_loop = &coarse_mloop[coarse_poly->loopstart + coarse_poly->totloop - 1];
  for (int corner = 0; corner < coarse_poly->totloop; corner++) {
    const MLoop *coarse_loop = &coarse_mloop[coarse_poly->loopstart + corner];
    const MEdge *coarse_edge = &coarse_medge[coarse_loop->e];
    const MEdge *prev_coarse_edge = &coarse_medge[prev_coarse_loop->e];
    const int start_edge_vertex = ctx->vertices_edge_offset +
                                  coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
    const bool flip = (coarse_edge->v2 == coarse_loop->v);
    int side_start_index = start_vertex_index;
    int side_stride = 0;
    int v0 = ctx->vertices_corner_offset + coarse_loop->v;
    int v3, e3;
    int e2_offset, e2_stride;
    float u, v, delta_u, delta_v;
    if (prev_coarse_loop->v == prev_coarse_edge->v1) {
      v3 = ctx->vertices_edge_offset + prev_coarse_loop->e * num_subdiv_vertices_per_coarse_edge +
           num_subdiv_vertices_per_coarse_edge - 1;
      e3 = ctx->edge_boundary_offset + prev_coarse_loop->e * num_subdiv_edges_per_coarse_edge +
           num_subdiv_edges_per_coarse_edge - 1;
    }
    else {
      v3 = ctx->vertices_edge_offset + prev_coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
      e3 = ctx->edge_boundary_offset + prev_coarse_loop->e * num_subdiv_edges_per_coarse_edge;
    }
    /* Calculate starting veretx of corresponding inner part of ptex. */
    if (corner == 0) {
      side_stride = 1;
      e2_offset = 0;
      e2_stride = 1;
      u = 0.0f;
      v = 0.0f;
      delta_u = du;
      delta_v = 0.0f;
    }
    else if (corner == 1) {
      side_start_index += resolution - 3;
      side_stride = resolution - 2;
      e2_offset = 2 * num_subdiv_edges_per_coarse_edge - 4;
      e2_stride = 2 * num_subdiv_edges_per_coarse_edge - 3;
      u = 1.0f - du;
      v = 0;
      delta_u = 0.0f;
      delta_v = dv;
    }
    else if (corner == 2) {
      side_start_index += num_subdiv_vertices_per_coarse_edge *
                              num_subdiv_vertices_per_coarse_edge -
                          1;
      side_stride = -1;
      e2_offset = num_edges_per_ptex_face_get(resolution - 2) - 1;
      e2_stride = -1;
      u = 1.0f - du;
      v = 1.0f - dv;
      delta_u = -du;
      delta_v = 0.0f;
    }
    else if (corner == 3) {
      side_start_index += num_subdiv_vertices_per_coarse_edge *
                          (num_subdiv_vertices_per_coarse_edge - 1);
      side_stride = -(resolution - 2);
      e2_offset = num_edges_per_ptex_face_get(resolution - 2) -
                  (2 * num_subdiv_edges_per_coarse_edge - 3);
      e2_stride = -(2 * num_subdiv_edges_per_coarse_edge - 3);
      u = 0.0f;
      v = 1.0f - dv;
      delta_u = 0.0f;
      delta_v = -dv;
    }
    for (int i = 0; i < resolution - 2; i++, subdiv_loop_index += 4) {
      int v1;
      if (flip) {
        v1 = start_edge_vertex + (resolution - i - 3);
      }
      else {
        v1 = start_edge_vertex + i;
      }
      const int v2 = side_start_index + side_stride * i;
      int e0;
      if (flip) {
        e0 = ctx->edge_boundary_offset + coarse_loop->e * num_subdiv_edges_per_coarse_edge +
             num_subdiv_edges_per_coarse_edge - i - 1;
      }
      else {
        e0 = ctx->edge_boundary_offset + coarse_loop->e * num_subdiv_edges_per_coarse_edge + i;
      }
      int e1 = start_edge_index + num_edges_per_ptex_face_get(resolution - 2) +
               corner * num_subdiv_vertices_per_coarse_edge + i;
      int e2;
      if (i == 0) {
        e2 = start_edge_index + num_edges_per_ptex_face_get(resolution - 2) +
             ((corner - 1 + coarse_poly->totloop) % coarse_poly->totloop) *
                 num_subdiv_vertices_per_coarse_edge +
             num_subdiv_vertices_per_coarse_edge - 1;
      }
      else {
        e2 = start_edge_index + e2_offset + e2_stride * (i - 1);
      }
      subdiv_foreach_loops_of_poly(ctx,
                                   tls,
                                   subdiv_loop_index,
                                   ptex_face_index,
                                   coarse_poly_index,
                                   corner,
                                   corner,
                                   v0,
                                   e0,
                                   v1,
                                   e1,
                                   v2,
                                   e2,
                                   v3,
                                   e3,
                                   u + delta_u * i,
                                   v + delta_v * i,
                                   du,
                                   dv);
      v0 = v1;
      v3 = v2;
      e3 = e1;
    }
    prev_coarse_loop = coarse_loop;
  }
}

static void subdiv_foreach_loops_special(SubdivForeachTaskContext *ctx,
                                         void *tls,
                                         const MPoly *coarse_poly)
{
  const int resolution = ctx->settings->resolution;
  /* Base/coarse mesh information. */
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MEdge *coarse_medge = coarse_mesh->medge;
  const MLoop *coarse_mloop = coarse_mesh->mloop;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const int coarse_poly_index = coarse_poly - coarse_mpoly;
  const int ptex_face_resolution = ptex_face_resolution_get(coarse_poly, resolution);
  const int ptex_face_inner_resolution = ptex_face_resolution - 2;
  const float inv_ptex_resolution_1 = 1.0f / (float)(ptex_face_resolution - 1);
  const int num_inner_vertices_per_ptex = (ptex_face_resolution - 1) * (ptex_face_resolution - 2);
  const int num_inner_edges_per_ptex_face = num_inner_edges_per_ptex_face_get(
      ptex_face_inner_resolution + 1);
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const int num_subdiv_edges_per_coarse_edge = resolution - 1;
  const int ptex_face_index = ctx->face_ptex_offset[coarse_poly_index];
  const int center_vertex_index = ctx->vertices_inner_offset +
                                  ctx->subdiv_vertex_offset[coarse_poly_index];
  const int start_vertex_index = center_vertex_index + 1;
  const int start_inner_vertex_index = center_vertex_index + 1;
  const int start_edge_index = ctx->edge_inner_offset + ctx->subdiv_edge_offset[coarse_poly_index];
  const int start_poly_index = ctx->subdiv_polygon_offset[coarse_poly_index];
  const int start_loop_index = 4 * start_poly_index;
  const float du = inv_ptex_resolution_1;
  const float dv = inv_ptex_resolution_1;
  /* Hi-poly subdivided mesh. */
  int subdiv_loop_index = start_loop_index;
  for (int corner = 0; corner < coarse_poly->totloop; corner++) {
    const int corner_vertex_index = start_vertex_index + corner * num_inner_vertices_per_ptex;
    const int corner_edge_index = start_edge_index + corner * num_inner_edges_per_ptex_face;
    for (int y = 1; y < ptex_face_inner_resolution; y++) {
      const float v = y * inv_ptex_resolution_1;
      const int inner_y = y - 1;
      for (int x = 1; x < ptex_face_inner_resolution + 1; x++, subdiv_loop_index += 4) {
        const int inner_x = x - 1;
        const float u = x * inv_ptex_resolution_1;
        /* Vertex indices ordered counter-clockwise. */
        const int v0 = corner_vertex_index +
                       (inner_y * (ptex_face_inner_resolution + 1) + inner_x);
        const int v1 = v0 + 1;
        const int v2 = v0 + ptex_face_inner_resolution + 2;
        const int v3 = v0 + ptex_face_inner_resolution + 1;
        /* Edge indices ordered counter-clockwise. */
        const int e0 = corner_edge_index +
                       (inner_y * (2 * ptex_face_inner_resolution + 1) + inner_x);
        const int e1 = e0 + ptex_face_inner_resolution + 1;
        const int e2 = e0 + (2 * ptex_face_inner_resolution + 1);
        const int e3 = e0 + ptex_face_inner_resolution;
        subdiv_foreach_loops_of_poly(ctx,
                                     tls,
                                     subdiv_loop_index,
                                     ptex_face_index + corner,
                                     coarse_poly_index,
                                     corner,
                                     0,
                                     v0,
                                     e0,
                                     v1,
                                     e1,
                                     v2,
                                     e2,
                                     v3,
                                     e3,
                                     u,
                                     v,
                                     du,
                                     dv);
      }
    }
  }
  /* Create connections between ptex faces. */
  for (int corner = 0; corner < coarse_poly->totloop; corner++) {
    const int next_corner = (corner + 1) % coarse_poly->totloop;
    const int corner_edge_index = start_edge_index + corner * num_inner_edges_per_ptex_face;
    const int next_corner_edge_index = start_edge_index +
                                       next_corner * num_inner_edges_per_ptex_face;
    int current_patch_vertex_index = start_inner_vertex_index +
                                     corner * num_inner_vertices_per_ptex +
                                     ptex_face_inner_resolution;
    int next_path_vertex_index = start_inner_vertex_index +
                                 next_corner * num_inner_vertices_per_ptex +
                                 num_inner_vertices_per_ptex - ptex_face_resolution + 1;
    int v0 = current_patch_vertex_index;
    int v1 = next_path_vertex_index;
    current_patch_vertex_index += ptex_face_inner_resolution + 1;
    next_path_vertex_index += 1;
    int e0 = start_edge_index + coarse_poly->totloop * num_inner_edges_per_ptex_face +
             corner * (ptex_face_resolution - 2);
    int e1 = next_corner_edge_index + num_inner_edges_per_ptex_face - ptex_face_resolution + 2;
    int e3 = corner_edge_index + 2 * ptex_face_resolution - 4;
    for (int row = 1; row < ptex_face_inner_resolution; row++, subdiv_loop_index += 4) {
      const int v2 = next_path_vertex_index;
      const int v3 = current_patch_vertex_index;
      const int e2 = e0 + 1;
      const float u = row * du;
      const float v = 1.0f - dv;
      subdiv_foreach_loops_of_poly(ctx,
                                   tls,
                                   subdiv_loop_index,
                                   ptex_face_index + next_corner,
                                   coarse_poly_index,
                                   next_corner,
                                   3,
                                   v0,
                                   e0,
                                   v1,
                                   e1,
                                   v2,
                                   e2,
                                   v3,
                                   e3,
                                   u,
                                   v,
                                   du,
                                   dv);
      current_patch_vertex_index += ptex_face_inner_resolution + 1;
      next_path_vertex_index += 1;
      v0 = v3;
      v1 = v2;
      e0 = e2;
      e1 += 1;
      e3 += 2 * ptex_face_resolution - 3;
    }
  }
  /* Create loops from center. */
  if (ptex_face_resolution >= 3) {
    const int start_center_edge_index = start_edge_index + (num_inner_edges_per_ptex_face +
                                                            ptex_face_inner_resolution) *
                                                               coarse_poly->totloop;
    const int start_boundary_edge = start_edge_index +
                                    coarse_poly->totloop * num_inner_edges_per_ptex_face +
                                    ptex_face_inner_resolution - 1;
    for (int corner = 0, prev_corner = coarse_poly->totloop - 1; corner < coarse_poly->totloop;
         prev_corner = corner, corner++, subdiv_loop_index += 4) {
      const int corner_edge_index = start_edge_index + corner * num_inner_edges_per_ptex_face;
      const int current_patch_end_vertex_index = start_vertex_index +
                                                 corner * num_inner_vertices_per_ptex +
                                                 num_inner_vertices_per_ptex - 1;
      const int prev_current_patch_end_vertex_index = start_vertex_index +
                                                      prev_corner * num_inner_vertices_per_ptex +
                                                      num_inner_vertices_per_ptex - 1;
      const int v0 = center_vertex_index;
      const int v1 = prev_current_patch_end_vertex_index;
      const int v2 = current_patch_end_vertex_index - 1;
      const int v3 = current_patch_end_vertex_index;
      const int e0 = start_center_edge_index + prev_corner;
      const int e1 = start_boundary_edge + prev_corner * (ptex_face_inner_resolution);
      const int e2 = corner_edge_index + num_inner_edges_per_ptex_face - 1;
      const int e3 = start_center_edge_index + corner;
      const float u = 1.0f - du;
      const float v = 1.0f - dv;
      subdiv_foreach_loops_of_poly(ctx,
                                   tls,
                                   subdiv_loop_index,
                                   ptex_face_index + corner,
                                   coarse_poly_index,
                                   corner,
                                   2,
                                   v0,
                                   e0,
                                   v1,
                                   e1,
                                   v2,
                                   e2,
                                   v3,
                                   e3,
                                   u,
                                   v,
                                   du,
                                   dv);
    }
  }
  /* Loops for faces connecting inner ptex part with boundary. */
  const MLoop *prev_coarse_loop = &coarse_mloop[coarse_poly->loopstart + coarse_poly->totloop - 1];
  for (int prev_corner = coarse_poly->totloop - 1, corner = 0; corner < coarse_poly->totloop;
       prev_corner = corner, corner++) {
    const MLoop *coarse_loop = &coarse_mloop[coarse_poly->loopstart + corner];
    const MEdge *coarse_edge = &coarse_medge[coarse_loop->e];
    const MEdge *prev_coarse_edge = &coarse_medge[prev_coarse_loop->e];
    const bool flip = (coarse_edge->v2 == coarse_loop->v);
    const int start_edge_vertex = ctx->vertices_edge_offset +
                                  coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
    const int corner_vertex_index = start_vertex_index + corner * num_inner_vertices_per_ptex;
    const int corner_edge_index = start_edge_index + corner * num_inner_edges_per_ptex_face;
    /* Create loops for polygons along U axis. */
    int v0 = ctx->vertices_corner_offset + coarse_loop->v;
    int v3, e3;
    if (prev_coarse_loop->v == prev_coarse_edge->v1) {
      v3 = ctx->vertices_edge_offset + prev_coarse_loop->e * num_subdiv_vertices_per_coarse_edge +
           num_subdiv_vertices_per_coarse_edge - 1;
      e3 = ctx->edge_boundary_offset + prev_coarse_loop->e * num_subdiv_edges_per_coarse_edge +
           num_subdiv_edges_per_coarse_edge - 1;
    }
    else {
      v3 = ctx->vertices_edge_offset + prev_coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
      e3 = ctx->edge_boundary_offset + prev_coarse_loop->e * num_subdiv_edges_per_coarse_edge;
    }
    for (int i = 0; i <= ptex_face_inner_resolution; i++, subdiv_loop_index += 4) {
      int v1;
      if (flip) {
        v1 = start_edge_vertex + (resolution - i - 3);
      }
      else {
        v1 = start_edge_vertex + i;
      }
      int v2;
      if (ptex_face_inner_resolution >= 1) {
        v2 = corner_vertex_index + i;
      }
      else {
        v2 = center_vertex_index;
      }
      int e0;
      if (flip) {
        e0 = ctx->edge_boundary_offset + coarse_loop->e * num_subdiv_edges_per_coarse_edge +
             num_subdiv_edges_per_coarse_edge - i - 1;
      }
      else {
        e0 = ctx->edge_boundary_offset + coarse_loop->e * num_subdiv_edges_per_coarse_edge + i;
      }
      int e1 = start_edge_index + corner * (2 * ptex_face_inner_resolution + 1);
      if (ptex_face_resolution >= 3) {
        e1 += coarse_poly->totloop *
                  (num_inner_edges_per_ptex_face + ptex_face_inner_resolution + 1) +
              i;
      }
      int e2 = 0;
      if (i == 0 && ptex_face_resolution >= 3) {
        e2 = start_edge_index +
             coarse_poly->totloop *
                 (num_inner_edges_per_ptex_face + ptex_face_inner_resolution + 1) +
             corner * (2 * ptex_face_inner_resolution + 1) + ptex_face_inner_resolution + 1;
      }
      else if (i == 0 && ptex_face_resolution < 3) {
        e2 = start_edge_index + prev_corner * (2 * ptex_face_inner_resolution + 1);
      }
      else {
        e2 = corner_edge_index + i - 1;
      }
      const float u = du * i;
      const float v = 0.0f;
      subdiv_foreach_loops_of_poly(ctx,
                                   tls,
                                   subdiv_loop_index,
                                   ptex_face_index + corner,
                                   coarse_poly_index,
                                   corner,
                                   0,
                                   v0,
                                   e0,
                                   v1,
                                   e1,
                                   v2,
                                   e2,
                                   v3,
                                   e3,
                                   u,
                                   v,
                                   du,
                                   dv);
      v0 = v1;
      v3 = v2;
      e3 = e1;
    }
    /* Create loops for polygons along V axis. */
    const bool flip_prev = (prev_coarse_edge->v2 == coarse_loop->v);
    v0 = corner_vertex_index;
    if (prev_coarse_loop->v == prev_coarse_edge->v1) {
      v3 = ctx->vertices_edge_offset + prev_coarse_loop->e * num_subdiv_vertices_per_coarse_edge +
           num_subdiv_vertices_per_coarse_edge - 1;
    }
    else {
      v3 = ctx->vertices_edge_offset + prev_coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
    }
    e3 = start_edge_index +
         coarse_poly->totloop * (num_inner_edges_per_ptex_face + ptex_face_inner_resolution + 1) +
         corner * (2 * ptex_face_inner_resolution + 1) + ptex_face_inner_resolution + 1;
    for (int i = 0; i <= ptex_face_inner_resolution - 1; i++, subdiv_loop_index += 4) {
      int v1;
      int e0, e1;
      if (i == ptex_face_inner_resolution - 1) {
        v1 = start_vertex_index + prev_corner * num_inner_vertices_per_ptex +
             ptex_face_inner_resolution;
        e1 = start_edge_index +
             coarse_poly->totloop *
                 (num_inner_edges_per_ptex_face + ptex_face_inner_resolution + 1) +
             prev_corner * (2 * ptex_face_inner_resolution + 1) + ptex_face_inner_resolution;
        e0 = start_edge_index + coarse_poly->totloop * num_inner_edges_per_ptex_face +
             prev_corner * ptex_face_inner_resolution;
      }
      else {
        v1 = v0 + ptex_face_inner_resolution + 1;
        e0 = corner_edge_index + ptex_face_inner_resolution +
             i * (2 * ptex_face_inner_resolution + 1);
        e1 = e3 + 1;
      }
      int v2 = flip_prev ? v3 - 1 : v3 + 1;
      int e2;
      if (flip_prev) {
        e2 = ctx->edge_boundary_offset + prev_coarse_loop->e * num_subdiv_edges_per_coarse_edge +
             num_subdiv_edges_per_coarse_edge - 2 - i;
      }
      else {
        e2 = ctx->edge_boundary_offset + prev_coarse_loop->e * num_subdiv_edges_per_coarse_edge +
             1 + i;
      }
      const float u = 0.0f;
      const float v = du * (i + 1);
      subdiv_foreach_loops_of_poly(ctx,
                                   tls,
                                   subdiv_loop_index,
                                   ptex_face_index + corner,
                                   coarse_poly_index,
                                   corner,
                                   1,
                                   v0,
                                   e0,
                                   v1,
                                   e1,
                                   v2,
                                   e2,
                                   v3,
                                   e3,
                                   u,
                                   v,
                                   du,
                                   dv);
      v0 = v1;
      v3 = v2;
      e3 = e1;
    }
    prev_coarse_loop = coarse_loop;
  }
}

static void subdiv_foreach_loops(SubdivForeachTaskContext *ctx, void *tls, int poly_index)
{
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const MPoly *coarse_poly = &coarse_mpoly[poly_index];
  if (coarse_poly->totloop == 4) {
    subdiv_foreach_loops_regular(ctx, tls, coarse_poly);
  }
  else {
    subdiv_foreach_loops_special(ctx, tls, coarse_poly);
  }
}

/* =============================================================================
 * Polygons traverse process.
 */

static void subdiv_foreach_polys(SubdivForeachTaskContext *ctx, void *tls, int poly_index)
{
  const int resolution = ctx->settings->resolution;
  const int start_poly_index = ctx->subdiv_polygon_offset[poly_index];
  /* Base/coarse mesh information. */
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const MPoly *coarse_poly = &coarse_mpoly[poly_index];
  const int num_ptex_faces_per_poly = num_ptex_faces_per_poly_get(coarse_poly);
  const int ptex_resolution = ptex_face_resolution_get(coarse_poly, resolution);
  const int num_polys_per_ptex = num_polys_per_ptex_get(ptex_resolution);
  const int num_loops_per_ptex = 4 * num_polys_per_ptex;
  const int start_loop_index = 4 * start_poly_index;
  /* Hi-poly subdivided mesh. */
  int subdiv_polyon_index = start_poly_index;
  for (int ptex_of_poly_index = 0; ptex_of_poly_index < num_ptex_faces_per_poly;
       ptex_of_poly_index++) {
    for (int subdiv_poly_index = 0; subdiv_poly_index < num_polys_per_ptex;
         subdiv_poly_index++, subdiv_polyon_index++) {
      const int loopstart = start_loop_index + (ptex_of_poly_index * num_loops_per_ptex) +
                            (subdiv_poly_index * 4);
      ctx->foreach_context->poly(
          ctx->foreach_context, tls, poly_index, subdiv_polyon_index, loopstart, 4);
    }
  }
}

/* =============================================================================
 * Loose elements traverse process.
 */

static void subdiv_foreach_loose_vertices_task(void *__restrict userdata,
                                               const int coarse_vertex_index,
                                               const TaskParallelTLS *__restrict tls)
{
  SubdivForeachTaskContext *ctx = userdata;
  if (BLI_BITMAP_TEST_BOOL(ctx->coarse_vertices_used_map, coarse_vertex_index)) {
    /* Vertex is not loose, was handled when handling polygons. */
    return;
  }
  const int subdiv_vertex_index = ctx->vertices_corner_offset + coarse_vertex_index;
  ctx->foreach_context->vertex_loose(
      ctx->foreach_context, tls->userdata_chunk, coarse_vertex_index, subdiv_vertex_index);
}

static void subdiv_foreach_vertices_of_loose_edges_task(void *__restrict userdata,
                                                        const int coarse_edge_index,
                                                        const TaskParallelTLS *__restrict tls)
{
  SubdivForeachTaskContext *ctx = userdata;
  if (BLI_BITMAP_TEST_BOOL(ctx->coarse_edges_used_map, coarse_edge_index)) {
    /* Vertex is not loose, was handled when handling polygons. */
    return;
  }
  const int resolution = ctx->settings->resolution;
  const int resolution_1 = resolution - 1;
  const float inv_resolution_1 = 1.0f / (float)resolution_1;
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MEdge *coarse_edge = &coarse_mesh->medge[coarse_edge_index];
  /* Subdivion vertices which corresponds to edge's v1 and v2. */
  const int subdiv_v1_index = ctx->vertices_corner_offset + coarse_edge->v1;
  const int subdiv_v2_index = ctx->vertices_corner_offset + coarse_edge->v2;
  /* First subdivided inner vertex of the edge.  */
  const int subdiv_start_vertex = ctx->vertices_edge_offset +
                                  coarse_edge_index * num_subdiv_vertices_per_coarse_edge;
  /* Perform interpolation. */
  for (int i = 0; i < resolution; i++) {
    const float u = i * inv_resolution_1;
    int subdiv_vertex_index;
    if (i == 0) {
      subdiv_vertex_index = subdiv_v1_index;
    }
    else if (i == resolution - 1) {
      subdiv_vertex_index = subdiv_v2_index;
    }
    else {
      subdiv_vertex_index = subdiv_start_vertex + (i - 1);
    }
    ctx->foreach_context->vertex_of_loose_edge(
        ctx->foreach_context, tls->userdata_chunk, coarse_edge_index, u, subdiv_vertex_index);
  }
}

/* =============================================================================
 * Subdivision process entry points.
 */

static void subdiv_foreach_single_geometry_vertices(SubdivForeachTaskContext *ctx, void *tls)
{
  if (ctx->foreach_context->vertex_corner == NULL) {
    return;
  }
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  for (int poly_index = 0; poly_index < coarse_mesh->totpoly; poly_index++) {
    const MPoly *coarse_poly = &coarse_mpoly[poly_index];
    subdiv_foreach_corner_vertices(ctx, tls, coarse_poly);
    subdiv_foreach_edge_vertices(ctx, tls, coarse_poly);
  }
}

static void subdiv_foreach_single_thread_tasks(SubdivForeachTaskContext *ctx)
{
  /* NOTE: In theory, we can try to skip allocation of TLS here, but in
   * practice if the callbacks used here are not specified then TLS will not
   * be requested anyway. */
  void *tls = subdiv_foreach_tls_alloc(ctx);
  /* Passes to average displacement on the corner vertices
   * and boundary edges. */
  subdiv_foreach_every_corner_vertices(ctx, tls);
  subdiv_foreach_every_edge_vertices(ctx, tls);
  /* Run callbacks which are supposed to be run once per shared geometry. */
  subdiv_foreach_single_geometry_vertices(ctx, tls);
  subdiv_foreach_tls_free(ctx, tls);
}

static void subdiv_foreach_task(void *__restrict userdata,
                                const int poly_index,
                                const TaskParallelTLS *__restrict tls)
{
  SubdivForeachTaskContext *ctx = userdata;
  /* Traverse hi-poly vertex coordinates and normals. */
  subdiv_foreach_vertices(ctx, tls->userdata_chunk, poly_index);
  /* Traverse mesh geometry for the given base poly index. */
  if (ctx->foreach_context->edge != NULL) {
    subdiv_foreach_edges(ctx, tls->userdata_chunk, poly_index);
  }
  if (ctx->foreach_context->loop != NULL) {
    subdiv_foreach_loops(ctx, tls->userdata_chunk, poly_index);
  }
  if (ctx->foreach_context->poly != NULL) {
    subdiv_foreach_polys(ctx, tls->userdata_chunk, poly_index);
  }
}

static void subdiv_foreach_boundary_edges_task(void *__restrict userdata,
                                               const int edge_index,
                                               const TaskParallelTLS *__restrict tls)
{
  SubdivForeachTaskContext *ctx = userdata;
  subdiv_foreach_boundary_edges(ctx, tls->userdata_chunk, edge_index);
}

static void subdiv_foreach_finalize(void *__restrict userdata, void *__restrict userdata_chunk)
{
  SubdivForeachTaskContext *ctx = userdata;
  ctx->foreach_context->user_data_tls_free(userdata_chunk);
}

bool BKE_subdiv_foreach_subdiv_geometry(Subdiv *subdiv,
                                        const SubdivForeachContext *context,
                                        const SubdivToMeshSettings *mesh_settings,
                                        const Mesh *coarse_mesh)
{
  SubdivForeachTaskContext ctx = {0};
  ctx.coarse_mesh = coarse_mesh;
  ctx.settings = mesh_settings;
  ctx.foreach_context = context;
  subdiv_foreach_ctx_init(subdiv, &ctx);
  if (context->topology_info != NULL) {
    if (!context->topology_info(context,
                                ctx.num_subdiv_vertices,
                                ctx.num_subdiv_edges,
                                ctx.num_subdiv_loops,
                                ctx.num_subdiv_polygons)) {
      subdiv_foreach_ctx_free(&ctx);
      return false;
    }
  }
  /* Run all the code which is not supposed to be run from threads. */
  subdiv_foreach_single_thread_tasks(&ctx);
  /* Threaded traversal of the rest of topology. */
  TaskParallelSettings parallel_range_settings;
  BLI_parallel_range_settings_defaults(&parallel_range_settings);
  parallel_range_settings.userdata_chunk = context->user_data_tls;
  parallel_range_settings.userdata_chunk_size = context->user_data_tls_size;
  if (context->user_data_tls_free != NULL) {
    parallel_range_settings.func_finalize = subdiv_foreach_finalize;
  }
  BLI_task_parallel_range(
      0, coarse_mesh->totpoly, &ctx, subdiv_foreach_task, &parallel_range_settings);
  if (context->vertex_loose != NULL) {
    BLI_task_parallel_range(0,
                            coarse_mesh->totvert,
                            &ctx,
                            subdiv_foreach_loose_vertices_task,
                            &parallel_range_settings);
  }
  if (context->vertex_of_loose_edge != NULL) {
    BLI_task_parallel_range(0,
                            coarse_mesh->totedge,
                            &ctx,
                            subdiv_foreach_vertices_of_loose_edges_task,
                            &parallel_range_settings);
  }
  if (context->edge != NULL) {
    BLI_task_parallel_range(0,
                            coarse_mesh->totedge,
                            &ctx,
                            subdiv_foreach_boundary_edges_task,
                            &parallel_range_settings);
  }
  subdiv_foreach_ctx_free(&ctx);
  return true;
}
