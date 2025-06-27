/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/camera.h"
#include "scene/mesh.h"

#include "subd/dice.h"
#include "subd/patch.h"
#include "subd/split.h"

#include "subd/subpatch.h"
#include "util/algorithm.h"

#include "util/math.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

/* DiagSplit */

DiagSplit::DiagSplit(const SubdParams &params_) : params(params_) {}

int DiagSplit::alloc_verts(int num)
{
  const int index = num_verts;
  num_verts += num;
  return index;
}

SubEdge *DiagSplit::alloc_edge(const int v0, const int v1, const int depth, bool &was_missing)
{
  const SubEdge edge(v0, v1, depth);
  const auto it = edges.find(edge);
  was_missing = (it == edges.end());
  return const_cast<SubEdge *>(was_missing ? &*(edges.emplace(edge).first) : &*it);
}

void DiagSplit::alloc_edge(SubPatch::Edge *sub_edge,
                           const int v0,
                           const int v1,
                           const int depth,
                           const bool want_to_own_edge,
                           const bool want_to_own_vertex)
{
  bool was_missing;
  sub_edge->edge = (v0 < v1) ? alloc_edge(v0, v1, depth, was_missing) :
                               alloc_edge(v1, v0, depth, was_missing);
  sub_edge->own_vertex = false;
  sub_edge->own_edge = was_missing && want_to_own_edge;
  sub_edge->reversed = sub_edge->edge->start_vert_index != v0;

  if (want_to_own_vertex) {
    if (v0 < owned_verts.size()) {
      /* Vertex in original mesh. */
      if (!owned_verts[v0]) {
        owned_verts[v0] = true;
        sub_edge->own_vertex = true;
      }
    }
    else {
      /* Mid edge vertex. */
      sub_edge->own_vertex = true;
    }
  }
}

void DiagSplit::alloc_subpatch(SubPatch &&sub)
{
  assert(sub.edges[0].edge->T >= 1);
  assert(sub.edges[1].edge->T >= 1);
  assert(sub.edges[2].edge->T >= 1);
  if (sub.shape == SubPatch::QUAD) {
    assert(sub.edges[3].edge->T >= 1);
  }

  sub.inner_grid_vert_offset = alloc_verts(sub.calc_num_inner_verts());
  sub.triangles_offset = num_triangles;
  num_triangles += sub.calc_num_triangles();

  subpatches.push_back(std::move(sub));
}

float3 DiagSplit::to_world(const Patch *patch, const float2 uv)
{
  float3 P;

  patch->eval(&P, nullptr, nullptr, nullptr, uv.x, uv.y);
  if (params.camera) {
    P = transform_point(&params.objecttoworld, P);
  }

  return P;
}

std::pair<int, float> DiagSplit::T(const Patch *patch,
                                   float2 uv_start,
                                   float2 uv_end,
                                   const int depth,
                                   const bool recursive_resolve)
{
  /* May not be necessary, but better to be safe. */
  if (uv_end.x < uv_start.x || uv_end.y < uv_start.y) {
    swap(uv_start, uv_end);
  }

  float Lsum = 0.0f;
  float Lmax = 0.0f;
  float Lsum_world = 0.0f;

  float3 Plast = to_world(patch, uv_start);

  for (int i = 1; i < params.test_steps; i++) {
    const float t = i / (float)(params.test_steps - 1);

    const float3 P = to_world(patch, uv_start + t * (uv_end - uv_start));

    float L = len(P - Plast);
    Lsum_world += L;

    if (params.camera) {
      Camera *cam = params.camera;

      const float pixel_width = cam->world_to_raster_size((P + Plast) * 0.5f);
      L /= pixel_width;
    }

    Lsum += L;
    Lmax = max(L, Lmax);

    Plast = P;
  }

  const int tmin = (int)ceilf(Lsum / params.dicing_rate);
  const int tmax = (int)ceilf(
      (params.test_steps - 1) * Lmax /
      params.dicing_rate);  // XXX paper says N instead of N-1, seems wrong?
  int res = max(tmax, 1);

  if (tmax - tmin > params.split_threshold) {
    if (!recursive_resolve) {
      res = DSPLIT_NON_UNIFORM;
    }
    else {
      const float2 uv_mid = (uv_start + uv_end) * 0.5f;
      const auto result_a = T(patch, uv_start, uv_mid, depth, true);
      const auto result_b = T(patch, uv_mid, uv_end, depth, true);
      res = result_a.first + result_b.first;
      Lsum_world = result_a.second + result_b.second;
    }
  }

  if (!recursive_resolve && res > DSPLIT_MAX_SEGMENTS) {
    res = DSPLIT_NON_UNIFORM;
  }

  res = limit_edge_factor(patch, uv_start, uv_end, res);

  /* Limit edge factor so we don't go beyond max depth. -3 is so that
   * for triangle patches, all 3 edges get an opportunity to get split. */
  if (depth >= DSPLIT_MAX_DEPTH - 3 && res == DSPLIT_NON_UNIFORM) {
    res = DSPLIT_MAX_SEGMENTS;
  }

  return std::make_pair(res, Lsum_world);
}

int DiagSplit::limit_edge_factor(const Patch *patch,
                                 const float2 uv_start,
                                 const float2 uv_end,
                                 const int T)
{
  const int max_t = 1 << params.max_level;
  int max_t_for_edge = int(max_t * len(uv_start - uv_end));

  if (patch->from_ngon) {
    max_t_for_edge >>= 1; /* Initial split of ngon causes edges to extend half the distance. */
  }

  const int limit_T = (max_t_for_edge <= 1) ? 1 : min(T, max_t_for_edge);

  assert(limit_T != 0);
  return limit_T;
}

void DiagSplit::assign_edge_factor(SubEdge *edge,
                                   const Patch *patch,
                                   float2 uv_start,
                                   float2 uv_end,
                                   const bool recursive_resolve)
{
  assert(edge->T <= 0);

  const auto result = T(patch, uv_start, uv_end, edge->depth, recursive_resolve);
  edge->T = result.first;
  edge->length = result.second;

  /* Ensure we can always split at depth - 1. */
  if (edge->depth == -1 && edge->T == 1) {
    edge->T = 2;
  }

  if (edge->T > 0) {
    edge->second_vert_index = alloc_verts(edge->T - 1);
  }
}

void DiagSplit::resolve_edge_factors(const SubPatch &sub)
{
  SubEdge *edge0 = sub.edges[0].edge;
  SubEdge *edge1 = sub.edges[1].edge;
  SubEdge *edge2 = sub.edges[2].edge;

  /* Compute edge factor if not already set. */
  if (edge0->T == 0) {
    assign_edge_factor(edge0, sub.patch, sub.uvs[0], sub.uvs[1], true);
  }
  if (edge1->T == 0) {
    assign_edge_factor(edge1, sub.patch, sub.uvs[1], sub.uvs[2], true);
  }

  if (sub.shape == SubPatch::TRIANGLE) {
    if (edge2->T == 0) {
      assign_edge_factor(edge2, sub.patch, sub.uvs[2], sub.uvs[0], true);
    }
  }
  else {
    SubEdge *edge3 = sub.edges[3].edge;

    if (edge2->T == 0) {
      assign_edge_factor(edge2, sub.patch, sub.uvs[2], sub.uvs[3], true);
    }
    if (edge3->T == 0) {
      assign_edge_factor(edge3, sub.patch, sub.uvs[3], sub.uvs[0], true);
    }
  }
}

float2 DiagSplit::split_edge(const Patch *patch,
                             SubPatch::Edge *subedge,
                             SubPatch::Edge *subedge_a,
                             SubPatch::Edge *subedge_b,
                             float2 uv_start,
                             float2 uv_end)
{
  /* This splits following the direction of the edge itself, not subpatch edge direction. */
  if (subedge->reversed) {
    swap(uv_start, uv_end);
  }

  SubEdge *edge = subedge->edge;

  if (edge->must_split()) {
    /* Split down the middle. */
    const float2 P = 0.5f * (uv_start + uv_end);
    if (edge->mid_vert_index == -1) {
      /* Allocate mid vertex and edges. */
      edge->mid_vert_index = alloc_verts(1);

      bool unused;
      SubEdge *edge_a = alloc_edge(
          edge->start_vert_index, edge->mid_vert_index, edge->depth + 1, unused);
      SubEdge *edge_b = alloc_edge(
          edge->mid_vert_index, edge->end_vert_index, edge->depth + 1, unused);
      assign_edge_factor(edge_a, patch, uv_start, P);
      assign_edge_factor(edge_b, patch, P, uv_end);
    }

    /* Allocate sub edges and set ownership. */
    alloc_edge(subedge_a,
               subedge->start_vert_index(),
               subedge->mid_vert_index(),
               edge->depth + 1,
               false,
               false);
    alloc_edge(subedge_b,
               subedge->mid_vert_index(),
               subedge->end_vert_index(),
               edge->depth + 1,
               false,
               false);

    subedge_a->own_edge = subedge->own_edge;
    subedge_b->own_edge = subedge->own_edge;
    subedge_a->own_vertex = subedge->own_vertex;
    subedge_b->own_vertex = subedge->own_edge;

    assert(P.x >= 0 && P.x <= 1.0f && P.y >= 0.0f && P.y <= 1.0f);
    return P;
  }

  assert(edge->T >= 2);
  const int mid = edge->T / 2;

  /* T is final and edge vertices are already allocated. An adjacent subpatch may not
   * split this edge. So we ensure T and vertex indices match up with the non-split edge. */
  if (edge->mid_vert_index == -1) {
    /* Allocate mid vertex and edges. */
    edge->mid_vert_index = edge->second_vert_index - 1 + mid;

    bool unused;
    SubEdge *edge_a = alloc_edge(
        edge->start_vert_index, edge->mid_vert_index, edge->depth + 1, unused);
    SubEdge *edge_b = alloc_edge(
        edge->mid_vert_index, edge->end_vert_index, edge->depth + 1, unused);
    edge_a->T = mid;
    edge_b->T = edge->T - mid;
    edge_a->second_vert_index = edge->second_vert_index;
    edge_b->second_vert_index = edge->second_vert_index + edge_a->T;
  }

  /* Allocate sub edges and set ownership. */
  alloc_edge(subedge_a,
             subedge->start_vert_index(),
             subedge->mid_vert_index(),
             edge->depth + 1,
             false,
             false);
  alloc_edge(subedge_b,
             subedge->mid_vert_index(),
             subedge->end_vert_index(),
             edge->depth + 1,
             false,
             false);

  subedge_a->own_edge = subedge->own_edge;
  subedge_b->own_edge = subedge->own_edge;
  subedge_a->own_vertex = subedge->own_vertex;
  subedge_b->own_vertex = subedge->own_edge;

  const float2 P = interp(uv_start, uv_end, mid / (float)edge->T);
  assert(P.x >= 0 && P.x <= 1.0f && P.y >= 0.0f && P.y <= 1.0f);

  return P;
}

void DiagSplit::split_quad(SubPatch &&sub)
{
  /* Set edge factors if we haven't already. */
  resolve_edge_factors(sub);

  /* Split subpatch if edges are marked as must split,
   * or if the following conditions are met:
   * - Both edges have at least 2 segments.
   * - Either edge has more than DSPLIT_MAX_SEGMENTS segments.
   * - The ratio of segments for opposite edges doesn't exceed 1.5.
   *   This reduces over tessellation for some patches. */
  const int min_T_u = min(sub.edges[0].edge->T, sub.edges[2].edge->T);
  const int max_T_u = max(sub.edges[0].edge->T, sub.edges[2].edge->T);
  const int min_T_v = min(sub.edges[3].edge->T, sub.edges[1].edge->T);
  const int max_T_v = max(sub.edges[3].edge->T, sub.edges[1].edge->T);

  bool split_u = sub.edges[0].edge->must_split() || sub.edges[2].edge->must_split() ||
                 (min_T_u >= 2 && min_T_v > DSPLIT_MAX_SEGMENTS && max_T_v / min_T_v > 1.5f);
  bool split_v = sub.edges[3].edge->must_split() || sub.edges[1].edge->must_split() ||
                 (min_T_v >= 2 && min_T_u > DSPLIT_MAX_SEGMENTS && max_T_u / min_T_u > 1.5f);

  /* If both need to split, pick longest axis. */
  if (split_u && split_v) {
    /* Slight bias so that for square quads, we get consistent results across
     * platforms rather than choice being decided by precision. */
    const float bias = 1.00012345f;
    if ((sub.edges[0].edge->length + sub.edges[2].edge->length) * bias >=
        sub.edges[1].edge->length + sub.edges[3].edge->length)
    {
      split_u = true;
      split_v = false;
    }
    else {
      split_u = false;
      split_v = true;
    }
  }

  if (!split_u && !split_v) {
    /* Add the unsplit subpatch. */
    alloc_subpatch(std::move(sub));
    return;
  }

  /* Split into triangles if one side must the split, and the opposite side has
   * only a single segment. Then we can't do an even split across the quad. */
  if ((split_u && (sub.edges[0].edge->T == 1 || sub.edges[2].edge->T == 1)) ||
      (!split_u && (sub.edges[1].edge->T == 1 || sub.edges[3].edge->T == 1)))
  {
    split_quad_into_triangles(std::move(sub));
    return;
  }

  /* Copy into new subpatches. */
  SubPatch sub_a(sub);
  SubPatch sub_b(sub);

  for (int i = 0; i < 4; i++) {
    sub_a.edges[i].own_edge = false;
    sub_a.edges[i].own_vertex = false;
    sub_b.edges[i].own_edge = false;
    sub_b.edges[i].own_vertex = false;
  }

  /* Pointers to various subpatch elements. */
  SubPatch::Edge *sub_across_0;
  SubPatch::Edge *sub_across_1;
  SubPatch::Edge *sub_a_across_0;
  SubPatch::Edge *sub_a_across_1;
  SubPatch::Edge *sub_b_across_0;
  SubPatch::Edge *sub_b_across_1;

  SubPatch::Edge *sub_a_split;
  SubPatch::Edge *sub_b_split;

  float2 *uv_a;
  float2 *uv_b;
  float2 *uv_c;
  float2 *uv_d;

  /* Set pointers based on split axis. */
  if (split_u) {
    /*
     *          sub_across_1
     *     -------uv_a  uv_c-------
     *     |         |  |         |
     *     |   A     |  |   B     |
     *     |         |  |         |
     *     -------uv_b  uv_d-------
     *          sub_across_0
     */
    sub_across_0 = &sub.edges[0];
    sub_across_1 = &sub.edges[2];
    sub_a_across_0 = &sub_a.edges[0];
    sub_a_across_1 = &sub_a.edges[2];
    sub_b_across_0 = &sub_b.edges[0];
    sub_b_across_1 = &sub_b.edges[2];

    sub_a.edges[3].own_edge = sub.edges[3].own_edge;
    sub_a.edges[3].own_vertex = sub.edges[3].own_vertex;
    sub_b.edges[1].own_edge = sub.edges[1].own_edge;
    sub_b.edges[1].own_vertex = sub.edges[1].own_vertex;

    sub_a_split = &sub_a.edges[1];
    sub_b_split = &sub_b.edges[3];

    uv_a = &sub_a.uvs[2];
    uv_b = &sub_a.uvs[1];
    uv_c = &sub_b.uvs[3];
    uv_d = &sub_b.uvs[0];
  }
  else {
    /*
     *                --------------------
     *                |        A         |
     *                uv_b------------uv_a
     * sub_across_0                         sub_across_1
     *                uv_d------------uv_c
     *                |        B         |
     *                --------------------
     */
    sub_across_0 = &sub.edges[3];
    sub_across_1 = &sub.edges[1];
    sub_a_across_0 = &sub_a.edges[3];
    sub_a_across_1 = &sub_a.edges[1];
    sub_b_across_0 = &sub_b.edges[3];
    sub_b_across_1 = &sub_b.edges[1];

    sub_a.edges[2].own_edge = sub.edges[2].own_edge;
    sub_a.edges[2].own_vertex = sub.edges[2].own_vertex;
    sub_b.edges[0].own_edge = sub.edges[0].own_edge;
    sub_b.edges[0].own_vertex = sub.edges[0].own_vertex;

    sub_a_split = &sub_a.edges[0];
    sub_b_split = &sub_b.edges[2];

    uv_a = &sub_a.uvs[1];
    uv_b = &sub_a.uvs[0];
    uv_c = &sub_b.uvs[2];
    uv_d = &sub_b.uvs[3];
  }

  /* Allocate new edges and vertices. */
  const float2 uv0 = split_edge(
      sub.patch, sub_across_0, sub_a_across_0, sub_b_across_0, *uv_d, *uv_b);
  const float2 uv1 = split_edge(
      sub.patch, sub_across_1, sub_b_across_1, sub_a_across_1, *uv_a, *uv_c);

  assert(sub_a_across_0->edge->T != 0);
  assert(sub_b_across_0->edge->T != 0);
  assert(sub_a_across_1->edge->T != 0);
  assert(sub_b_across_1->edge->T != 0);

  /* Split */
  *uv_a = uv1;
  *uv_b = uv0;

  *uv_c = uv1;
  *uv_d = uv0;

  /* Create new edge */
  const int split_edge_depth = (split_u) ?
                                   max(sub.edges[1].edge->depth, sub.edges[3].edge->depth) :
                                   max(sub.edges[0].edge->depth, sub.edges[2].edge->depth);
  alloc_edge(sub_a_split,
             sub_across_0->mid_vert_index(),
             sub_across_1->mid_vert_index(),
             split_edge_depth,
             true,
             false);
  alloc_edge(sub_b_split,
             sub_across_1->mid_vert_index(),
             sub_across_0->mid_vert_index(),
             split_edge_depth,
             true,
             false);

  /* Set T for split edge. */
  assign_edge_factor(sub_a_split->edge, sub.patch, uv0, uv1);

  /* Recurse */
  split_quad(std::move(sub_a));
  split_quad(std::move(sub_b));
}

void DiagSplit::split_quad_into_triangles(SubPatch &&sub)
{
  assert(sub.shape == SubPatch::QUAD);

  /* Copy into new subpatches. */
  SubPatch sub_a(sub);
  SubPatch sub_b(sub);

  sub_a.shape = SubPatch::TRIANGLE;
  sub_b.shape = SubPatch::TRIANGLE;

  for (int i = 0; i < 4; i++) {
    sub_a.edges[i].own_edge = false;
    sub_a.edges[i].own_vertex = false;
    sub_b.edges[i].own_edge = false;
    sub_b.edges[i].own_vertex = false;
  }

  const int split_edge_depth = std::max({sub.edges[0].edge->depth,
                                         sub.edges[1].edge->depth,
                                         sub.edges[2].edge->depth,
                                         sub.edges[3].edge->depth});

  sub_a.edges[0] = sub.edges[0];
  sub_a.edges[1] = sub.edges[1];
  sub_a.uvs[0] = sub.uvs[0];
  sub_a.uvs[1] = sub.uvs[1];
  sub_a.uvs[2] = sub.uvs[2];
  alloc_edge(&sub_a.edges[2],
             sub.edges[2].start_vert_index(),
             sub.edges[0].start_vert_index(),
             split_edge_depth,
             true,
             false);

  sub_b.edges[1] = sub.edges[2];
  sub_b.edges[2] = sub.edges[3];
  sub_b.uvs[0] = sub.uvs[0];
  sub_b.uvs[1] = sub.uvs[2];
  sub_b.uvs[2] = sub.uvs[3];
  alloc_edge(&sub_b.edges[0],
             sub.edges[0].start_vert_index(),
             sub.edges[2].start_vert_index(),
             split_edge_depth,
             true,
             false);

  /* Set T for new edge. */
  assign_edge_factor(sub_b.edges[0].edge, sub.patch, sub.uvs[0], sub.uvs[2]);

  /* Recurse */
  split_triangle(std::move(sub_a));
  split_triangle(std::move(sub_b));
}

void DiagSplit::split_triangle(SubPatch &&sub)
{
  assert(sub.shape == SubPatch::TRIANGLE);

  /* Set edge factors if we haven't already. */
  resolve_edge_factors(sub);

  const bool do_split = sub.edges[0].edge->must_split() || sub.edges[1].edge->must_split() ||
                        sub.edges[2].edge->must_split();
  if (!do_split) {
    /* Add the unsplit subpatch. */
    alloc_subpatch(std::move(sub));
    return;
  }

  /* Slight bias so that for equal length edges, we get consistent results across
   * platforms rather than choice being decided by precision. */
  const float bias = 1.00012345f;

  /* Pick longest edge that must be split. Note that in degenerate cases edges may have
   * zero length but still requires splitting at depth 0. */
  float max_length = 0.0f;
  int split_index_0 = -1;
  for (int i = 0; i < 3; i++) {
    if (sub.edges[i].edge->must_split() &&
        (split_index_0 == -1 || sub.edges[i].edge->length > max_length))
    {
      split_index_0 = i;
      max_length = sub.edges[i].edge->length * bias;
    }
  }

  /* Copy into new subpatches. */
  SubPatch sub_a(sub);
  SubPatch sub_b(sub);

  for (int i = 0; i < 4; i++) {
    sub_a.edges[i].own_edge = false;
    sub_a.edges[i].own_vertex = false;
    sub_b.edges[i].own_edge = false;
    sub_b.edges[i].own_vertex = false;
  }

  const int split_index_1 = (split_index_0 + 1) % 3;
  const int split_index_2 = (split_index_0 + 2) % 3;

  sub_a.edges[2] = sub.edges[split_index_2];
  sub_b.edges[1] = sub.edges[split_index_1];

  /*
   *     uv_opposite
   *       2    2
   *      / |   | \
   *     /  |   |  \
   *    / A |   | B \
   *   /    |   |    \
   *  0 --- 1   0 --- 1
   *       uv_split
   */

  /* Allocate new edges and vertices. */
  const float2 uv_split = split_edge(sub.patch,
                                     &sub.edges[split_index_0],
                                     &sub_a.edges[0],
                                     &sub_b.edges[0],
                                     sub.uvs[split_index_0],
                                     sub.uvs[split_index_1]);

  /* Set UVs. */
  sub_a.uvs[0] = sub.uvs[split_index_0];
  sub_a.uvs[1] = uv_split;
  sub_a.uvs[2] = sub.uvs[split_index_2];
  sub_b.uvs[0] = uv_split;
  sub_b.uvs[1] = sub.uvs[split_index_1];
  sub_b.uvs[2] = sub.uvs[split_index_2];

  /* Create new edge */
  const int vsplit = sub.edges[split_index_0].mid_vert_index();
  const int vopposite = sub.edges[split_index_2].start_vert_index();

  const int split_edge_depth = sub.edges[split_index_0].edge->depth + 1;

  alloc_edge(&sub_a.edges[1], vsplit, vopposite, split_edge_depth, true, false);
  alloc_edge(&sub_b.edges[2], vopposite, vsplit, split_edge_depth, true, false);

  /* Set T for split edge. */
  const float2 uv_opposite = sub.uvs[split_index_2];
  assign_edge_factor(sub_a.edges[1].edge, sub.patch, uv_split, uv_opposite);

  /* Recurse */
  split_triangle(std::move(sub_a));
  split_triangle(std::move(sub_b));
}

void DiagSplit::split_quad(const Mesh::SubdFace &face, const int face_index, const Patch *patch)
{
  const int *subd_face_corners = params.mesh->get_subd_face_corners().data();
  const int v0 = subd_face_corners[face.start_corner + 0];
  const int v1 = subd_face_corners[face.start_corner + 1];
  const int v2 = subd_face_corners[face.start_corner + 2];
  const int v3 = subd_face_corners[face.start_corner + 3];

  const int depth = -1;

  SubPatch subpatch(patch, face_index);
  alloc_edge(&subpatch.edges[0], v0, v1, depth, true, true);
  alloc_edge(&subpatch.edges[1], v1, v2, depth, true, true);
  alloc_edge(&subpatch.edges[2], v2, v3, depth, true, true);
  alloc_edge(&subpatch.edges[3], v3, v0, depth, true, true);

  /* Forces a split in both axis for quads, needed to match split of ngons into quads. */
  subpatch.edges[0].edge->T = DSPLIT_NON_UNIFORM;
  subpatch.edges[3].edge->T = DSPLIT_NON_UNIFORM;
  subpatch.edges[2].edge->T = DSPLIT_NON_UNIFORM;
  subpatch.edges[1].edge->T = DSPLIT_NON_UNIFORM;

  split_quad(std::move(subpatch));
}

void DiagSplit::split_ngon(const Mesh::SubdFace &face,
                           const int face_index,
                           const Patch *patches,
                           const size_t patches_byte_stride)
{
  const int *subd_face_corners = params.mesh->get_subd_face_corners().data();
  const int v2 = alloc_verts(1);

  const int depth = 0;

  /* Allocate edges of n-gon. */
  array<SubPatch::Edge> edges(face.num_corners);
  for (int corner = 0; corner < face.num_corners; corner++) {
    const int v = subd_face_corners[face.start_corner + corner];
    const int vnext = subd_face_corners[face.start_corner + mod(corner + 1, face.num_corners)];

    alloc_edge(&edges[corner], v, vnext, depth, true, true);
    if (edges[corner].edge->mid_vert_index == -1) {
      edges[corner].edge->mid_vert_index = alloc_verts(1);
    }
  }

  /* Allocate patches. */
  for (int corner = 0; corner < face.num_corners; corner++) {
    const Patch *patch = (const Patch *)(((char *)patches) + (corner * patches_byte_stride));

    /*         v_prev        .
     *           .           .
     *           .   edge2   .
     *          v3 ←------- v2 . . .
     *           |           ↑
     *    edge3  |           | edge1
     *           ↓           |
     *           v0 ------→ v1 . . v_next
     *              edge0
     */
    SubPatch::Edge &edge3 = edges[mod(corner + face.num_corners - 1, face.num_corners)];
    SubPatch::Edge &edge0 = edges[corner];

    /* Setup edges. */
    const int v0 = edge0.start_vert_index();
    const int v1 = edge0.mid_vert_index();
    const int v3 = edge3.mid_vert_index();

    SubPatch subpatch(patch, face_index, corner);
    alloc_edge(&subpatch.edges[0], v0, v1, depth, false, false);
    alloc_edge(&subpatch.edges[1], v1, v2, depth, true, false);
    alloc_edge(&subpatch.edges[2], v2, v3, depth, true, corner == 0);
    alloc_edge(&subpatch.edges[3], v3, v0, depth, false, false);

    subpatch.edges[0].own_edge = edge0.own_edge;
    subpatch.edges[0].own_vertex = edge0.own_vertex;
    subpatch.edges[3].own_edge = edge3.own_edge;
    subpatch.edges[3].own_vertex = edge3.own_edge;

    /* Perform split. */
    split_quad(std::move(subpatch));
  }
}

void DiagSplit::split_patches(const Patch *patches, const size_t patches_byte_stride)
{
  /* TODO: reuse edge factor vertex position computations. */
  /* TODO: support not splitting n-gons if not needed. */
  /* TODO: multi-threading. */

  /* Keep base mesh vertices, create new triangles. */
  num_verts = params.mesh->get_num_subd_base_verts();
  num_triangles = 0;

  owned_verts.resize(num_verts, false);

  /* Split all faces in the mesh. */
  for (int f = 0; f < params.mesh->get_num_subd_faces(); f++) {
    Mesh::SubdFace face = params.mesh->get_subd_face(f);
    const Patch *patch = (const Patch *)(((char *)patches) +
                                         (face.ptex_offset * patches_byte_stride));
    if (face.is_quad()) {
      split_quad(face, f, patch);
    }
    else {
      split_ngon(face, f, patch, patches_byte_stride);
    }
  }
}

CCL_NAMESPACE_END
