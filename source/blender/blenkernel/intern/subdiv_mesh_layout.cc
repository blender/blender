/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_index_mask.hh"
#include "BLI_task.hh"

#include "BKE_subdiv_mesh_layout.hh"

#include "DNA_mesh_types.h"

namespace blender::bke::subdiv {

/* -------------------------------------------------------------------- */
/** \name General helpers
 * \{ */

/* Number of ptex faces for a given face. */
BLI_INLINE int num_ptex_faces_per_face_get(const IndexRange face)
{
  return (face.size() == 4) ? 1 : face.size();
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
BLI_INLINE int num_faces_per_ptex_get(const int resolution)
{
  return (resolution - 1) * (resolution - 1);
}

/* Subdivision resolution per given face's ptex faces. */
BLI_INLINE int ptex_face_resolution_get(const IndexRange face, int resolution)
{
  return (face.size() == 4) ? (resolution) : ((resolution >> 1) + 1);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Base mesh data used by the traversal of each base face
 * \{ */

struct TraversalContext {
  Span<int2> coarse_edges;
  OffsetIndices<int> coarse_faces;
  Span<int> coarse_corner_verts;
  Span<int> coarse_corner_edges;
  const MeshLayout *layout;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Initialization
 * \{ */

MeshLayout build_mesh_layout(const Mesh &coarse_mesh, const int resolution)
{
  const OffsetIndices coarse_faces = coarse_mesh.faces();
  const int resolution_2 = resolution - 2;
  const int resolution_2_squared = resolution_2 * resolution_2;
  const int no_quad_patch_resolution = ((resolution >> 1) + 1);
  const int num_irregular_vertices_per_patch = (no_quad_patch_resolution - 2) *
                                               (no_quad_patch_resolution - 1);
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const int num_subdiv_edges_per_coarse_edge = resolution - 1;
  MeshLayout layout;
  layout.resolution = resolution;
  /* Constant offsets in arrays. */
  layout.edge_verts_start = coarse_mesh.verts_num;
  layout.inner_verts_start = layout.edge_verts_start +
                             coarse_mesh.edges_num * num_subdiv_vertices_per_coarse_edge;
  layout.inner_edges_start = coarse_mesh.edges_num * num_subdiv_edges_per_coarse_edge;
  /* "Indexed" offsets. */
  layout.inner_vert_offsets.reinitialize(coarse_mesh.faces_num + 1);
  layout.inner_edge_offsets.reinitialize(coarse_mesh.faces_num + 1);
  layout.face_offsets.reinitialize(coarse_mesh.faces_num + 1);
  int vert_offset = 0;
  int edge_offset = 0;
  int face_offset = 0;
  for (int face_index = 0; face_index < coarse_mesh.faces_num; face_index++) {
    const IndexRange coarse_face = coarse_faces[face_index];
    const int num_ptex_faces_per_face = num_ptex_faces_per_face_get(coarse_face);
    layout.inner_vert_offsets[face_index] = vert_offset;
    layout.inner_edge_offsets[face_index] = edge_offset;
    layout.face_offsets[face_index] = face_offset;
    if (num_ptex_faces_per_face == 1) {
      vert_offset += resolution_2_squared;
      edge_offset += num_edges_per_ptex_face_get(resolution - 2) +
                     4 * num_subdiv_vertices_per_coarse_edge;
      face_offset += num_faces_per_ptex_get(resolution);
    }
    else {
      vert_offset += 1 + num_ptex_faces_per_face * num_irregular_vertices_per_patch;
      edge_offset += num_ptex_faces_per_face *
                     (num_inner_edges_per_ptex_face_get(no_quad_patch_resolution - 1) +
                      (no_quad_patch_resolution - 2) + num_subdiv_vertices_per_coarse_edge);
      if (no_quad_patch_resolution >= 3) {
        edge_offset += coarse_face.size();
      }
      face_offset += num_ptex_faces_per_face * num_faces_per_ptex_get(no_quad_patch_resolution);
    }
  }
  layout.inner_vert_offsets.last() = vert_offset;
  layout.inner_edge_offsets.last() = edge_offset;
  layout.face_offsets.last() = face_offset;
  layout.verts_num = layout.inner_verts_start + vert_offset;
  layout.edges_num = layout.inner_edges_start + edge_offset;
  layout.faces_num = face_offset;
  layout.corners_num = face_offset * 4;
  return layout;
}

static TraversalContext context_from_mesh(const Mesh &coarse_mesh, const MeshLayout &layout)
{
  TraversalContext ctx;
  ctx.coarse_edges = coarse_mesh.edges();
  ctx.coarse_faces = coarse_mesh.faces();
  ctx.coarse_corner_verts = coarse_mesh.corner_verts();
  ctx.coarse_corner_edges = coarse_mesh.corner_edges();
  ctx.layout = &layout;
  return ctx;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex traversal process
 * \{ */

/* Traversal of inner vertices, they are coming from ptex patches. */

static void subdiv_foreach_inner_vertices_regular(const TraversalContext *ctx,
                                                  const int ptex_face_index,
                                                  MutableSpan<PtexCoord> ptex_coords)
{
  const int resolution = ctx->layout->resolution;
  const float inv_resolution_1 = 1.0f / float(resolution - 1);
  int subdiv_vert_index = 0;
  for (int y = 1; y < resolution - 1; y++) {
    const float v = y * inv_resolution_1;
    for (int x = 1; x < resolution - 1; x++, subdiv_vert_index++) {
      const float u = x * inv_resolution_1;
      ptex_coords[subdiv_vert_index] = {ptex_face_index, float2(u, v)};
    }
  }
}

static void subdiv_foreach_inner_vertices_special(const TraversalContext *ctx,
                                                  const int coarse_face_index,
                                                  int ptex_face_index,
                                                  MutableSpan<PtexCoord> ptex_coords)
{
  const int resolution = ctx->layout->resolution;
  const IndexRange coarse_face = ctx->coarse_faces[coarse_face_index];
  const int ptex_face_resolution = ptex_face_resolution_get(coarse_face, resolution);
  const float inv_ptex_face_resolution_1 = 1.0f / float(ptex_face_resolution - 1);
  int subdiv_vert_index = 0;
  ptex_coords[subdiv_vert_index] = {ptex_face_index, float2(1.0f, 1.0f)};
  subdiv_vert_index++;
  for (int corner = 0; corner < coarse_face.size(); corner++, ptex_face_index++) {
    for (int y = 1; y < ptex_face_resolution - 1; y++) {
      const float v = y * inv_ptex_face_resolution_1;
      for (int x = 1; x < ptex_face_resolution; x++, subdiv_vert_index++) {
        const float u = x * inv_ptex_face_resolution_1;
        ptex_coords[subdiv_vert_index] = {ptex_face_index, float2(u, v)};
      }
    }
  }
}

PtexCoord face_corner_ptex_coord(const IndexRange face,
                                 const int face_ptex_offset,
                                 const int corner)
{
  if (face.size() == 4) {
    static constexpr float2 quad_uvs[4] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
    return {face_ptex_offset, quad_uvs[corner]};
  }
  return {face_ptex_offset + corner, float2(0.0f)};
}

void fill_face_corner_edge_ptex_coords(const MeshLayout &layout,
                                       const IndexRange face,
                                       const int face_ptex_offset,
                                       const int corner,
                                       const bool flip,
                                       MutableSpan<PtexCoord> ptex_coords)
{
  const int resolution = layout.resolution;
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  BLI_assert(ptex_coords.size() == num_subdiv_vertices_per_coarse_edge);
  if (num_subdiv_vertices_per_coarse_edge <= 0) {
    return;
  }
  if (face.size() == 4) {
    const int resolution_1 = resolution - 1;
    const float inv_resolution_1 = 1.0f / float(resolution_1);
    for (int vert_index = 0; vert_index < num_subdiv_vertices_per_coarse_edge; vert_index++) {
      float fac = (vert_index + 1) * inv_resolution_1;
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
      ptex_coords[vert_index] = {face_ptex_offset, float2(u, v)};
    }
    return;
  }
  /* The first half of the edge is in the ptex face of the corner, the second half is in the ptex
   * face of the next corner. */
  const int num_vertices_per_ptex_edge = ((resolution >> 1) + 1);
  const float inv_ptex_resolution_1 = 1.0f / float(num_vertices_per_ptex_edge - 1);
  const int ptex_face_index = face_ptex_offset + corner;
  const int next_corner = (corner + 1) % face.size();
  const int next_ptex_face_index = face_ptex_offset + next_corner;
  int subdiv_vert_index = 0;
  int vert_delta = 1;
  if (flip) {
    subdiv_vert_index += num_subdiv_vertices_per_coarse_edge - 1;
    vert_delta = -1;
  }
  for (int vert_index = 1; vert_index < num_vertices_per_ptex_edge;
       vert_index++, subdiv_vert_index += vert_delta)
  {
    const float u = vert_index * inv_ptex_resolution_1;
    ptex_coords[subdiv_vert_index] = {ptex_face_index, float2(u, 0.0f)};
  }
  for (int vert_index = 1; vert_index < num_vertices_per_ptex_edge - 1;
       vert_index++, subdiv_vert_index += vert_delta)
  {
    const float v = 1.0f - vert_index * inv_ptex_resolution_1;
    ptex_coords[subdiv_vert_index] = {next_ptex_face_index, float2(0.0f, v)};
  }
}

void fill_inner_vert_ptex_coords(const MeshLayout &layout,
                                 const Mesh &base_mesh,
                                 const Span<int> face_ptex_offset,
                                 const IndexRange base_faces,
                                 MutableSpan<PtexCoord> vert_ptex_coords)
{
  const TraversalContext ctx = context_from_mesh(base_mesh, layout);
  const OffsetIndices<int> inner_verts_by_face = layout.inner_verts_by_face();
  const IndexRange chunk_verts = inner_verts_by_face[base_faces];
  BLI_assert(vert_ptex_coords.size() == chunk_verts.size());
  threading::parallel_for(base_faces, layout.base_face_grain_size(), [&](const IndexRange range) {
    for (const int face : range) {
      MutableSpan<PtexCoord> face_coords = vert_ptex_coords.slice(
          inner_verts_by_face[face].shift(-chunk_verts.start()));
      if (ctx.coarse_faces[face].size() == 4) {
        subdiv_foreach_inner_vertices_regular(&ctx, face_ptex_offset[face], face_coords);
      }
      else {
        subdiv_foreach_inner_vertices_special(&ctx, face, face_ptex_offset[face], face_coords);
      }
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge traversal process
 * \{ */

static int subdiv_foreach_edges_row(MutableSpan<int2> edges,
                                    const int start_subdiv_edge_index,
                                    const int start_vert_index,
                                    const int num_edges_per_row)
{
  int subdiv_edge_index = start_subdiv_edge_index;
  int vert_index = start_vert_index;
  for (int edge_index = 0; edge_index < num_edges_per_row - 1; edge_index++, subdiv_edge_index++) {
    const int v1 = vert_index;
    const int v2 = vert_index + 1;
    edges[subdiv_edge_index] = int2(v1, v2);
    vert_index += 1;
  }
  return subdiv_edge_index;
}

static int subdiv_foreach_edges_column(MutableSpan<int2> edges,
                                       const int start_subdiv_edge_index,
                                       const int start_vert_index,
                                       const int num_edges_per_row)
{
  int subdiv_edge_index = start_subdiv_edge_index;
  int vert_index = start_vert_index;
  for (int edge_index = 0; edge_index < num_edges_per_row; edge_index++, subdiv_edge_index++) {
    const int v1 = vert_index;
    const int v2 = vert_index + num_edges_per_row;
    edges[subdiv_edge_index] = int2(v1, v2);
    vert_index += 1;
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

static void subdiv_foreach_edges_all_patches_regular(const TraversalContext *ctx,
                                                     const int coarse_face_index,
                                                     MutableSpan<int2> edges)
{
  const IndexRange coarse_face = ctx->coarse_faces[coarse_face_index];
  const int resolution = ctx->layout->resolution;
  const int start_vert_index = ctx->layout->inner_verts_start +
                               ctx->layout->inner_vert_offsets[coarse_face_index];
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  int subdiv_edge_index = 0;
  /* Traverse bottom row of edges (0-1, 1-2). */
  subdiv_edge_index = subdiv_foreach_edges_row(
      edges, subdiv_edge_index, start_vert_index, resolution - 2);
  /* Traverse remaining edges. */
  for (int row = 0; row < resolution - 3; row++) {
    const int start_row_vert_index = start_vert_index + row * (resolution - 2);
    /* Traverse vertical columns.
     *
     * At first iteration it will be edges (0-3. 1-4, 2-5), then it
     * will be (3-6, 4-7, 5-8) and so on.
     */
    subdiv_edge_index = subdiv_foreach_edges_column(
        edges, subdiv_edge_index, start_row_vert_index, resolution - 2);
    /* Create horizontal edge row.
     *
     * At first iteration it will be edges (3-4, 4-5), then it will be
     * (6-7, 7-8) and so on.
     */
    subdiv_edge_index = subdiv_foreach_edges_row(
        edges, subdiv_edge_index, start_row_vert_index + resolution - 2, resolution - 2);
  }
  /* Connect inner part of patch to boundary. */
  for (int corner = 0; corner < coarse_face.size(); corner++) {
    const int coarse_vert_index = ctx->coarse_corner_verts[coarse_face[corner]];
    const int coarse_edge_index = ctx->coarse_corner_edges[coarse_face[corner]];
    const int2 &coarse_edge = ctx->coarse_edges[coarse_edge_index];
    const int start_edge_vert = ctx->layout->edge_verts_start +
                                coarse_edge_index * num_subdiv_vertices_per_coarse_edge;
    const bool flip = (coarse_edge[1] == coarse_vert_index);
    int side_start_index = start_vert_index;
    int side_stride = 0;
    /* Calculate starting vertex of corresponding inner part of ptex. */
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
      const int v1 = (flip) ? (start_edge_vert + (resolution - i - 3)) : (start_edge_vert + i);
      const int v2 = side_start_index + side_stride * i;
      edges[subdiv_edge_index] = int2(v1, v2);
    }
  }
}

static void subdiv_foreach_edges_all_patches_special(const TraversalContext *ctx,
                                                     const int coarse_face_index,
                                                     MutableSpan<int2> edges)
{
  const int resolution = ctx->layout->resolution;
  const IndexRange coarse_face = ctx->coarse_faces[coarse_face_index];
  const int ptex_face_resolution = ptex_face_resolution_get(coarse_face, resolution);
  const int ptex_face_inner_resolution = ptex_face_resolution - 2;
  const int num_inner_vertices_per_ptex = (ptex_face_resolution - 1) * (ptex_face_resolution - 2);
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const int center_vert_index = ctx->layout->inner_verts_start +
                                ctx->layout->inner_vert_offsets[coarse_face_index];
  const int start_vert_index = center_vert_index + 1;
  int subdiv_edge_index = 0;
  /* Traverse inner ptex edges. */
  for (int corner = 0; corner < coarse_face.size(); corner++) {
    const int start_ptex_face_vert_index = start_vert_index + corner * num_inner_vertices_per_ptex;
    /* Similar steps to regular patch case. */
    subdiv_edge_index = subdiv_foreach_edges_row(
        edges, subdiv_edge_index, start_ptex_face_vert_index, ptex_face_inner_resolution + 1);
    for (int row = 0; row < ptex_face_inner_resolution - 1; row++) {
      const int start_row_vert_index = start_ptex_face_vert_index +
                                       row * (ptex_face_inner_resolution + 1);
      subdiv_edge_index = subdiv_foreach_edges_column(
          edges, subdiv_edge_index, start_row_vert_index, ptex_face_inner_resolution + 1);
      subdiv_edge_index = subdiv_foreach_edges_row(edges,
                                                   subdiv_edge_index,
                                                   start_row_vert_index +
                                                       ptex_face_inner_resolution + 1,
                                                   ptex_face_inner_resolution + 1);
    }
  }
  /* Create connections between ptex faces. */
  for (int corner = 0; corner < coarse_face.size(); corner++) {
    const int next_corner = (corner + 1) % coarse_face.size();
    int current_patch_vert_index = start_vert_index + corner * num_inner_vertices_per_ptex +
                                   ptex_face_inner_resolution;
    int next_path_vert_index = start_vert_index + next_corner * num_inner_vertices_per_ptex +
                               num_inner_vertices_per_ptex - ptex_face_resolution + 1;
    for (int row = 0; row < ptex_face_inner_resolution; row++, subdiv_edge_index++) {
      const int v1 = current_patch_vert_index;
      const int v2 = next_path_vert_index;
      edges[subdiv_edge_index] = int2(v1, v2);
      current_patch_vert_index += ptex_face_inner_resolution + 1;
      next_path_vert_index += 1;
    }
  }
  /* Create edges from center. */
  if (ptex_face_resolution >= 3) {
    for (int corner = 0; corner < coarse_face.size(); corner++, subdiv_edge_index++) {
      const int current_patch_end_vert_index = start_vert_index +
                                               corner * num_inner_vertices_per_ptex +
                                               num_inner_vertices_per_ptex - 1;
      const int v1 = center_vert_index;
      const int v2 = current_patch_end_vert_index;
      edges[subdiv_edge_index] = int2(v1, v2);
    }
  }
  /* Connect inner path of patch to boundary. */
  int prev_corner = coarse_face.size() - 1;
  for (int corner = 0; corner < coarse_face.size(); corner++) {
    const int coarse_vert = ctx->coarse_corner_verts[coarse_face[corner]];
    const int coarse_edge_i = ctx->coarse_corner_edges[coarse_face[corner]];
    const int coarse_prev_edge = ctx->coarse_corner_edges[coarse_face[prev_corner]];
    {
      const int2 &coarse_edge = ctx->coarse_edges[coarse_edge_i];
      const int start_edge_vert = ctx->layout->edge_verts_start +
                                  coarse_edge_i * num_subdiv_vertices_per_coarse_edge;
      const bool flip = (coarse_edge[1] == coarse_vert);
      int side_start_index;
      if (ptex_face_resolution >= 3) {
        side_start_index = start_vert_index + num_inner_vertices_per_ptex * corner;
      }
      else {
        side_start_index = center_vert_index;
      }
      for (int i = 0; i < ptex_face_resolution - 1; i++, subdiv_edge_index++) {
        const int v1 = (flip) ? (start_edge_vert + (resolution - i - 3)) : (start_edge_vert + i);
        const int v2 = side_start_index + i;
        edges[subdiv_edge_index] = int2(v1, v2);
      }
    }
    if (ptex_face_resolution >= 3) {
      const int2 &coarse_edge = ctx->coarse_edges[coarse_prev_edge];
      const int start_edge_vert = ctx->layout->edge_verts_start +
                                  coarse_prev_edge * num_subdiv_vertices_per_coarse_edge;
      const bool flip = (coarse_edge[1] == coarse_vert);
      int side_start_index = start_vert_index + num_inner_vertices_per_ptex * corner;
      for (int i = 0; i < ptex_face_resolution - 2; i++, subdiv_edge_index++) {
        const int v1 = (flip) ? (start_edge_vert + (resolution - i - 3)) : (start_edge_vert + i);
        const int v2 = side_start_index + (ptex_face_inner_resolution + 1) * i;
        edges[subdiv_edge_index] = int2(v1, v2);
      }
    }
    prev_corner = corner;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edges
 * \{ */

void fill_base_edge_edges(const MeshLayout &layout,
                          const Span<int2> base_edges,
                          const IndexMask &selection,
                          MutableSpan<int2> edges)
{
  const int resolution = layout.resolution;
  const int num_subdiv_edges_per_coarse_edge = resolution - 1;
  BLI_assert(edges.size() == selection.size() * num_subdiv_edges_per_coarse_edge);
  selection.foreach_index(
      [&](const int coarse_edge_index, const int pos) {
        const int2 &coarse_edge = base_edges[coarse_edge_index];
        const IndexRange edge_verts = layout.base_edge_verts(coarse_edge_index);
        MutableSpan<int2> dst = edges.slice(pos * num_subdiv_edges_per_coarse_edge,
                                            num_subdiv_edges_per_coarse_edge);
        int last_vert_index = coarse_edge[0];
        for (int i = 0; i < num_subdiv_edges_per_coarse_edge - 1; i++) {
          const int v1 = last_vert_index;
          const int v2 = edge_verts[i];
          dst[i] = int2(v1, v2);
          last_vert_index = v2;
        }
        dst.last() = int2(last_vert_index, coarse_edge[1]);
      },
      exec_mode::grain_size(1024));
}

void fill_inner_edges(const MeshLayout &layout, const Mesh &base_mesh, MutableSpan<int2> edges)
{
  const TraversalContext ctx = context_from_mesh(base_mesh, layout);
  const OffsetIndices<int> inner_edges_by_face = layout.inner_edges_by_face();
  BLI_assert(edges.size() == inner_edges_by_face.total_size());
  threading::parallel_for(
      ctx.coarse_faces.index_range(), layout.base_face_grain_size(), [&](const IndexRange range) {
        for (const int face : range) {
          const IndexRange face_edges = inner_edges_by_face[face];
          if (ctx.coarse_faces[face].size() == 4) {
            subdiv_foreach_edges_all_patches_regular(&ctx, face, edges.slice(face_edges));
          }
          else {
            subdiv_foreach_edges_all_patches_special(&ctx, face, edges.slice(face_edges));
          }
        }
      });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loops traversal
 * \{ */

static void rotate_indices(const int rot, int *a, int *b, int *c, int *d)
{
  const int values[4] = {*a, *b, *c, *d};
  *a = values[(0 - rot + 4) % 4];
  *b = values[(1 - rot + 4) % 4];
  *c = values[(2 - rot + 4) % 4];
  *d = values[(3 - rot + 4) % 4];
}

/**
 * Optional outputs of the face corner traversal for a single base mesh face, aligned with the
 * corners of its subdivided faces. Empty spans are skipped.
 */
struct FaceCornerOutputs {
  MutableSpan<int> corner_verts;
  MutableSpan<int> corner_edges;
  MutableSpan<PtexCoord> corner_ptex_coords;
  /* Aligned with the subdivided faces rather than their corners. */
  MutableSpan<int> face_base_corners;
};

static void subdiv_foreach_loops_of_face(const FaceCornerOutputs &out,
                                         const int subdiv_loop_start_index,
                                         const int ptex_face_index,
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
  if (!out.corner_verts.is_empty()) {
    out.corner_verts[subdiv_loop_start_index + 0] = v0;
    out.corner_verts[subdiv_loop_start_index + 1] = v1;
    out.corner_verts[subdiv_loop_start_index + 2] = v2;
    out.corner_verts[subdiv_loop_start_index + 3] = v3;
  }
  if (!out.corner_edges.is_empty()) {
    out.corner_edges[subdiv_loop_start_index + 0] = e0;
    out.corner_edges[subdiv_loop_start_index + 1] = e1;
    out.corner_edges[subdiv_loop_start_index + 2] = e2;
    out.corner_edges[subdiv_loop_start_index + 3] = e3;
  }
  if (!out.corner_ptex_coords.is_empty()) {
    out.corner_ptex_coords[subdiv_loop_start_index + 0] = {ptex_face_index, float2(u, v)};
    out.corner_ptex_coords[subdiv_loop_start_index + 1] = {ptex_face_index, float2(u + du, v)};
    out.corner_ptex_coords[subdiv_loop_start_index + 2] = {ptex_face_index,
                                                           float2(u + du, v + dv)};
    out.corner_ptex_coords[subdiv_loop_start_index + 3] = {ptex_face_index, float2(u, v + dv)};
  }
  if (!out.face_base_corners.is_empty()) {
    out.face_base_corners[subdiv_loop_start_index / 4] = coarse_corner_index;
  }
}

static int subdiv_foreach_loops_corner_index(const float u,
                                             const float v,
                                             const float du,
                                             const float dv)
{
  if (u + du <= 0.5f && v + dv <= 0.5f) {
    return 0;
  }
  if (u >= 0.5f && v + dv <= 0.5f) {
    return 1;
  }
  if (u >= 0.5f && v >= 0.5f) {
    return 2;
  }
  return 3;
}

static void subdiv_foreach_loops_regular(const TraversalContext *ctx,
                                         const int coarse_face_index,
                                         const int ptex_face_index,
                                         const FaceCornerOutputs &out)
{
  const int resolution = ctx->layout->resolution;
  /* Base/coarse mesh information. */
  const IndexRange coarse_face = ctx->coarse_faces[coarse_face_index];
  const int ptex_resolution = ptex_face_resolution_get(coarse_face, resolution);
  const int ptex_inner_resolution = ptex_resolution - 2;
  const int num_subdiv_edges_per_coarse_edge = resolution - 1;
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const float inv_ptex_resolution_1 = 1.0f / float(ptex_resolution - 1);
  const int start_vert_index = ctx->layout->inner_verts_start +
                               ctx->layout->inner_vert_offsets[coarse_face_index];
  const int start_edge_index = ctx->layout->inner_edges_start +
                               ctx->layout->inner_edge_offsets[coarse_face_index];
  const float du = inv_ptex_resolution_1;
  const float dv = inv_ptex_resolution_1;
  /* Hi-poly subdivided mesh. */
  int subdiv_loop_index = 0;
  /* Loops for inner part of ptex. */
  for (int y = 1; y < ptex_resolution - 2; y++) {
    const float v = y * inv_ptex_resolution_1;
    const int inner_y = y - 1;
    for (int x = 1; x < ptex_resolution - 2; x++, subdiv_loop_index += 4) {
      const int inner_x = x - 1;
      const float u = x * inv_ptex_resolution_1;
      /* Vertex indices ordered counter-clockwise. */
      const int v0 = start_vert_index + (inner_y * ptex_inner_resolution + inner_x);
      const int v1 = v0 + 1;
      const int v2 = v0 + ptex_inner_resolution + 1;
      const int v3 = v0 + ptex_inner_resolution;
      /* Edge indices ordered counter-clockwise. */
      const int e0 = start_edge_index + (inner_y * (2 * ptex_inner_resolution - 1) + inner_x);
      const int e1 = e0 + ptex_inner_resolution;
      const int e2 = e0 + (2 * ptex_inner_resolution - 1);
      const int e3 = e0 + ptex_inner_resolution - 1;

      subdiv_foreach_loops_of_face(out,
                                   subdiv_loop_index,
                                   ptex_face_index,
                                   subdiv_foreach_loops_corner_index(u, v, du, dv),
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
  int prev_corner_index = coarse_face.size() - 1;
  for (int corner = 0; corner < coarse_face.size(); corner++) {
    const int coarse_vert = ctx->coarse_corner_verts[coarse_face[corner]];
    const int coarse_edge_i = ctx->coarse_corner_edges[coarse_face[corner]];
    const int coarse_prev_vert = ctx->coarse_corner_verts[coarse_face[prev_corner_index]];
    const int coarse_prev_edge = ctx->coarse_corner_edges[coarse_face[prev_corner_index]];

    const int2 &coarse_edge = ctx->coarse_edges[coarse_edge_i];
    const int2 &prev_coarse_edge = ctx->coarse_edges[coarse_prev_edge];
    const int start_edge_vert = ctx->layout->edge_verts_start +
                                coarse_edge_i * num_subdiv_vertices_per_coarse_edge;
    const bool flip = (coarse_edge[1] == coarse_vert);
    int side_start_index = start_vert_index;
    int side_stride = 0;
    int v0 = coarse_vert;
    int v3, e3;
    int e2_offset, e2_stride;
    float u, v, delta_u, delta_v;
    if (coarse_prev_vert == prev_coarse_edge[0]) {
      v3 = ctx->layout->edge_verts_start + coarse_prev_edge * num_subdiv_vertices_per_coarse_edge +
           num_subdiv_vertices_per_coarse_edge - 1;
      e3 = (coarse_prev_edge + 1) * num_subdiv_edges_per_coarse_edge - 1;
    }
    else {
      v3 = ctx->layout->edge_verts_start + coarse_prev_edge * num_subdiv_vertices_per_coarse_edge;
      e3 = coarse_prev_edge * num_subdiv_edges_per_coarse_edge;
    }
    /* Calculate starting vertex of corresponding inner part of ptex. */
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
        v1 = start_edge_vert + (resolution - i - 3);
      }
      else {
        v1 = start_edge_vert + i;
      }
      const int v2 = side_start_index + side_stride * i;
      int e0;
      if (flip) {
        e0 = (coarse_edge_i + 1) * num_subdiv_edges_per_coarse_edge - i - 1;
      }
      else {
        e0 = coarse_edge_i * num_subdiv_edges_per_coarse_edge + i;
      }
      int e1 = start_edge_index + num_edges_per_ptex_face_get(resolution - 2) +
               corner * num_subdiv_vertices_per_coarse_edge + i;
      int e2;
      if (i == 0) {
        e2 = start_edge_index + num_edges_per_ptex_face_get(resolution - 2) +
             ((corner - 1 + coarse_face.size()) % coarse_face.size()) *
                 num_subdiv_vertices_per_coarse_edge +
             num_subdiv_vertices_per_coarse_edge - 1;
      }
      else {
        e2 = start_edge_index + e2_offset + e2_stride * (i - 1);
      }

      const float loop_u = u + delta_u * i;
      const float loop_v = v + delta_v * i;

      subdiv_foreach_loops_of_face(out,
                                   subdiv_loop_index,
                                   ptex_face_index,
                                   subdiv_foreach_loops_corner_index(loop_u, loop_v, du, dv),
                                   corner,
                                   v0,
                                   e0,
                                   v1,
                                   e1,
                                   v2,
                                   e2,
                                   v3,
                                   e3,
                                   loop_u,
                                   loop_v,
                                   du,
                                   dv);
      v0 = v1;
      v3 = v2;
      e3 = e1;
    }
    prev_corner_index = corner;
  }
}

static void subdiv_foreach_loops_special(const TraversalContext *ctx,
                                         const int coarse_face_index,
                                         const int ptex_face_index,
                                         const FaceCornerOutputs &out)
{
  const int resolution = ctx->layout->resolution;
  /* Base/coarse mesh information. */
  const IndexRange coarse_face = ctx->coarse_faces[coarse_face_index];
  const int ptex_face_resolution = ptex_face_resolution_get(coarse_face, resolution);
  const int ptex_face_inner_resolution = ptex_face_resolution - 2;
  const float inv_ptex_resolution_1 = 1.0f / float(ptex_face_resolution - 1);
  const int num_inner_vertices_per_ptex = (ptex_face_resolution - 1) * (ptex_face_resolution - 2);
  const int num_inner_edges_per_ptex_face = num_inner_edges_per_ptex_face_get(
      ptex_face_inner_resolution + 1);
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const int num_subdiv_edges_per_coarse_edge = resolution - 1;
  const int center_vert_index = ctx->layout->inner_verts_start +
                                ctx->layout->inner_vert_offsets[coarse_face_index];
  const int start_vert_index = center_vert_index + 1;
  const int start_inner_vert_index = center_vert_index + 1;
  const int start_edge_index = ctx->layout->inner_edges_start +
                               ctx->layout->inner_edge_offsets[coarse_face_index];
  const float du = inv_ptex_resolution_1;
  const float dv = inv_ptex_resolution_1;
  /* Hi-poly subdivided mesh. */
  int subdiv_loop_index = 0;
  for (int corner = 0; corner < coarse_face.size(); corner++) {
    const int corner_vert_index = start_vert_index + corner * num_inner_vertices_per_ptex;
    const int corner_edge_index = start_edge_index + corner * num_inner_edges_per_ptex_face;
    for (int y = 1; y < ptex_face_inner_resolution; y++) {
      const float v = y * inv_ptex_resolution_1;
      const int inner_y = y - 1;
      for (int x = 1; x < ptex_face_inner_resolution + 1; x++, subdiv_loop_index += 4) {
        const int inner_x = x - 1;
        const float u = x * inv_ptex_resolution_1;
        /* Vertex indices ordered counter-clockwise. */
        const int v0 = corner_vert_index + (inner_y * (ptex_face_inner_resolution + 1) + inner_x);
        const int v1 = v0 + 1;
        const int v2 = v0 + ptex_face_inner_resolution + 2;
        const int v3 = v0 + ptex_face_inner_resolution + 1;
        /* Edge indices ordered counter-clockwise. */
        const int e0 = corner_edge_index +
                       (inner_y * (2 * ptex_face_inner_resolution + 1) + inner_x);
        const int e1 = e0 + ptex_face_inner_resolution + 1;
        const int e2 = e0 + (2 * ptex_face_inner_resolution + 1);
        const int e3 = e0 + ptex_face_inner_resolution;
        subdiv_foreach_loops_of_face(out,
                                     subdiv_loop_index,
                                     ptex_face_index + corner,
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
  for (int corner = 0; corner < coarse_face.size(); corner++) {
    const int next_corner = (corner + 1) % coarse_face.size();
    const int corner_edge_index = start_edge_index + corner * num_inner_edges_per_ptex_face;
    const int next_corner_edge_index = start_edge_index +
                                       next_corner * num_inner_edges_per_ptex_face;
    int current_patch_vert_index = start_inner_vert_index + corner * num_inner_vertices_per_ptex +
                                   ptex_face_inner_resolution;
    int next_path_vert_index = start_inner_vert_index + next_corner * num_inner_vertices_per_ptex +
                               num_inner_vertices_per_ptex - ptex_face_resolution + 1;
    int v0 = current_patch_vert_index;
    int v1 = next_path_vert_index;
    current_patch_vert_index += ptex_face_inner_resolution + 1;
    next_path_vert_index += 1;
    int e0 = start_edge_index + coarse_face.size() * num_inner_edges_per_ptex_face +
             corner * (ptex_face_resolution - 2);
    int e1 = next_corner_edge_index + num_inner_edges_per_ptex_face - ptex_face_resolution + 2;
    int e3 = corner_edge_index + 2 * ptex_face_resolution - 4;
    for (int row = 1; row < ptex_face_inner_resolution; row++, subdiv_loop_index += 4) {
      const int v2 = next_path_vert_index;
      const int v3 = current_patch_vert_index;
      const int e2 = e0 + 1;
      const float u = row * du;
      const float v = 1.0f - dv;
      subdiv_foreach_loops_of_face(out,
                                   subdiv_loop_index,
                                   ptex_face_index + next_corner,
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
      current_patch_vert_index += ptex_face_inner_resolution + 1;
      next_path_vert_index += 1;
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
                                                               coarse_face.size();
    const int start_boundary_edge = start_edge_index +
                                    coarse_face.size() * num_inner_edges_per_ptex_face +
                                    ptex_face_inner_resolution - 1;
    for (int corner = 0, prev_corner = coarse_face.size() - 1; corner < coarse_face.size();
         prev_corner = corner, corner++, subdiv_loop_index += 4)
    {
      const int corner_edge_index = start_edge_index + corner * num_inner_edges_per_ptex_face;
      const int current_patch_end_vert_index = start_vert_index +
                                               corner * num_inner_vertices_per_ptex +
                                               num_inner_vertices_per_ptex - 1;
      const int prev_current_patch_end_vert_index = start_vert_index +
                                                    prev_corner * num_inner_vertices_per_ptex +
                                                    num_inner_vertices_per_ptex - 1;
      const int v0 = center_vert_index;
      const int v1 = prev_current_patch_end_vert_index;
      const int v2 = current_patch_end_vert_index - 1;
      const int v3 = current_patch_end_vert_index;
      const int e0 = start_center_edge_index + prev_corner;
      const int e1 = start_boundary_edge + prev_corner * (ptex_face_inner_resolution);
      const int e2 = corner_edge_index + num_inner_edges_per_ptex_face - 1;
      const int e3 = start_center_edge_index + corner;
      const float u = 1.0f - du;
      const float v = 1.0f - dv;
      subdiv_foreach_loops_of_face(out,
                                   subdiv_loop_index,
                                   ptex_face_index + corner,
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
  for (int prev_corner = coarse_face.size() - 1, corner = 0; corner < coarse_face.size();
       prev_corner = corner, corner++)
  {
    const int coarse_vert = ctx->coarse_corner_verts[coarse_face[corner]];
    const int coarse_edge_i = ctx->coarse_corner_edges[coarse_face[corner]];
    const int coarse_prev_vert = ctx->coarse_corner_verts[coarse_face[prev_corner]];
    const int coarse_prev_edge = ctx->coarse_corner_edges[coarse_face[prev_corner]];

    const int2 &coarse_edge = ctx->coarse_edges[coarse_edge_i];
    const int2 &prev_coarse_edge = ctx->coarse_edges[coarse_prev_edge];
    const bool flip = (coarse_edge[1] == coarse_vert);
    const int start_edge_vert = ctx->layout->edge_verts_start +
                                coarse_edge_i * num_subdiv_vertices_per_coarse_edge;
    const int corner_vert_index = start_vert_index + corner * num_inner_vertices_per_ptex;
    const int corner_edge_index = start_edge_index + corner * num_inner_edges_per_ptex_face;
    /* Create loops for polygons along U axis. */
    int v0 = coarse_vert;
    int v3, e3;
    if (coarse_prev_vert == prev_coarse_edge[0]) {
      v3 = ctx->layout->edge_verts_start + coarse_prev_edge * num_subdiv_vertices_per_coarse_edge +
           num_subdiv_vertices_per_coarse_edge - 1;
      e3 = (coarse_prev_edge + 1) * num_subdiv_edges_per_coarse_edge - 1;
    }
    else {
      v3 = ctx->layout->edge_verts_start + coarse_prev_edge * num_subdiv_vertices_per_coarse_edge;
      e3 = coarse_prev_edge * num_subdiv_edges_per_coarse_edge;
    }
    for (int i = 0; i <= ptex_face_inner_resolution; i++, subdiv_loop_index += 4) {
      int v1;
      if (flip) {
        v1 = start_edge_vert + (resolution - i - 3);
      }
      else {
        v1 = start_edge_vert + i;
      }
      int v2;
      if (ptex_face_inner_resolution >= 1) {
        v2 = corner_vert_index + i;
      }
      else {
        v2 = center_vert_index;
      }
      int e0;
      if (flip) {
        e0 = (coarse_edge_i + 1) * num_subdiv_edges_per_coarse_edge - i - 1;
      }
      else {
        e0 = coarse_edge_i * num_subdiv_edges_per_coarse_edge + i;
      }
      int e1 = start_edge_index + corner * (2 * ptex_face_inner_resolution + 1);
      if (ptex_face_resolution >= 3) {
        e1 += coarse_face.size() *
                  (num_inner_edges_per_ptex_face + ptex_face_inner_resolution + 1) +
              i;
      }
      int e2 = 0;
      if (i == 0 && ptex_face_resolution >= 3) {
        e2 = start_edge_index +
             coarse_face.size() *
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
      subdiv_foreach_loops_of_face(out,
                                   subdiv_loop_index,
                                   ptex_face_index + corner,
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
    const bool flip_prev = (prev_coarse_edge[1] == coarse_vert);
    v0 = corner_vert_index;
    if (coarse_prev_vert == prev_coarse_edge[0]) {
      v3 = ctx->layout->edge_verts_start + coarse_prev_edge * num_subdiv_vertices_per_coarse_edge +
           num_subdiv_vertices_per_coarse_edge - 1;
    }
    else {
      v3 = ctx->layout->edge_verts_start + coarse_prev_edge * num_subdiv_vertices_per_coarse_edge;
    }
    e3 = start_edge_index +
         coarse_face.size() * (num_inner_edges_per_ptex_face + ptex_face_inner_resolution + 1) +
         corner * (2 * ptex_face_inner_resolution + 1) + ptex_face_inner_resolution + 1;
    for (int i = 0; i <= ptex_face_inner_resolution - 1; i++, subdiv_loop_index += 4) {
      int v1;
      int e0, e1;
      if (i == ptex_face_inner_resolution - 1) {
        v1 = start_vert_index + prev_corner * num_inner_vertices_per_ptex +
             ptex_face_inner_resolution;
        e1 = start_edge_index +
             coarse_face.size() *
                 (num_inner_edges_per_ptex_face + ptex_face_inner_resolution + 1) +
             prev_corner * (2 * ptex_face_inner_resolution + 1) + ptex_face_inner_resolution;
        e0 = start_edge_index + coarse_face.size() * num_inner_edges_per_ptex_face +
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
        e2 = coarse_prev_edge * num_subdiv_edges_per_coarse_edge +
             num_subdiv_edges_per_coarse_edge - 2 - i;
      }
      else {
        e2 = coarse_prev_edge * num_subdiv_edges_per_coarse_edge + 1 + i;
      }
      const float u = 0.0f;
      const float v = du * (i + 1);
      subdiv_foreach_loops_of_face(out,
                                   subdiv_loop_index,
                                   ptex_face_index + corner,
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
  }
}

static void subdiv_foreach_loops(const TraversalContext *ctx,
                                 const int face_index,
                                 const int ptex_face_index,
                                 const FaceCornerOutputs &out)
{
  if (ctx->coarse_faces[face_index].size() == 4) {
    subdiv_foreach_loops_regular(ctx, face_index, ptex_face_index, out);
  }
  else {
    subdiv_foreach_loops_special(ctx, face_index, ptex_face_index, out);
  }
}

void fill_corner_verts_and_edges(const MeshLayout &layout,
                                 const Mesh &base_mesh,
                                 const IndexRange base_faces,
                                 MutableSpan<int> corner_verts,
                                 MutableSpan<int> corner_edges)
{
  const TraversalContext ctx = context_from_mesh(base_mesh, layout);
  const IndexRange chunk_corners = layout.base_face_corners(base_faces);
  BLI_assert(corner_verts.is_empty() || corner_verts.size() == chunk_corners.size());
  BLI_assert(corner_edges.is_empty() || corner_edges.size() == chunk_corners.size());
  threading::parallel_for(base_faces, layout.base_face_grain_size(), [&](const IndexRange range) {
    for (const int face : range) {
      const IndexRange face_corners = layout.base_face_corners(face).shift(-chunk_corners.start());
      FaceCornerOutputs out;
      if (!corner_verts.is_empty()) {
        out.corner_verts = corner_verts.slice(face_corners);
      }
      if (!corner_edges.is_empty()) {
        out.corner_edges = corner_edges.slice(face_corners);
      }
      /* The ptex face index is only used for the ptex coordinates. */
      subdiv_foreach_loops(&ctx, face, 0, out);
    }
  });
}

void fill_corner_ptex_coords(const MeshLayout &layout,
                             const Mesh &base_mesh,
                             const Span<int> face_ptex_offset,
                             const IndexRange base_faces,
                             MutableSpan<PtexCoord> corner_ptex_coords,
                             MutableSpan<int> face_base_corners)
{
  const TraversalContext ctx = context_from_mesh(base_mesh, layout);
  const OffsetIndices<int> faces_by_base_face = layout.faces_by_base_face();
  const IndexRange chunk_faces = faces_by_base_face[base_faces];
  const IndexRange chunk_corners = layout.base_face_corners(base_faces);
  BLI_assert(corner_ptex_coords.is_empty() || corner_ptex_coords.size() == chunk_corners.size());
  BLI_assert(face_base_corners.is_empty() || face_base_corners.size() == chunk_faces.size());
  threading::parallel_for(base_faces, layout.base_face_grain_size(), [&](const IndexRange range) {
    for (const int face : range) {
      FaceCornerOutputs out;
      if (!corner_ptex_coords.is_empty()) {
        out.corner_ptex_coords = corner_ptex_coords.slice(
            layout.base_face_corners(face).shift(-chunk_corners.start()));
      }
      if (!face_base_corners.is_empty()) {
        out.face_base_corners = face_base_corners.slice(
            faces_by_base_face[face].shift(-chunk_faces.start()));
      }
      subdiv_foreach_loops(&ctx, face, face_ptex_offset[face], out);
    }
  });
}

/** \} */

}  // namespace blender::bke::subdiv
