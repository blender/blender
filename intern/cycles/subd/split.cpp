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

enum {
  DSPLIT_NON_UNIFORM = -1,
  DSPLIT_MAX_DEPTH = 32,
  DSPLIT_MAX_SEGMENTS = 8,
};

DiagSplit::DiagSplit(const SubdParams &params_) : params(params_) {}

int DiagSplit::alloc_verts(int num)
{
  const int index = num_verts;
  num_verts += num;
  return index;
}

SubEdge *DiagSplit::alloc_edge(const int v0, const int v1)
{
  const SubEdge edge(v0, v1);
  const auto it = edges.find(edge);
  return const_cast<SubEdge *>((it == edges.end()) ? &*(edges.emplace(edge).first) : &*it);
}

void DiagSplit::alloc_edge(SubPatch::Edge *sub_edge, int v0, int v1)
{
  sub_edge->edge = (v0 < v1) ? alloc_edge(v0, v1) : alloc_edge(v1, v0);
  sub_edge->reversed = sub_edge->edge->start_vert_index != v0;
}

void DiagSplit::alloc_subpatch(SubPatch &&sub)
{
  assert(sub.edge_u0.edge->T >= 1);
  assert(sub.edge_v1.edge->T >= 1);
  assert(sub.edge_u1.edge->T >= 1);
  assert(sub.edge_v0.edge->T >= 1);

  sub.inner_grid_vert_offset = alloc_verts(sub.calc_num_inner_verts());
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

int DiagSplit::T(
    const Patch *patch, float2 Pstart, float2 Pend, const int depth, const bool recursive_resolve)
{
  /* May not be necessary, but better to be safe. */
  if (Pend.x < Pstart.x || Pend.y < Pstart.y) {
    swap(Pstart, Pend);
  }

  float Lsum = 0.0f;
  float Lmax = 0.0f;

  float3 Plast = to_world(patch, Pstart);

  for (int i = 1; i < params.test_steps; i++) {
    const float t = i / (float)(params.test_steps - 1);

    const float3 P = to_world(patch, Pstart + t * (Pend - Pstart));

    float L;

    if (!params.camera) {
      L = len(P - Plast);
    }
    else {
      Camera *cam = params.camera;

      const float pixel_width = cam->world_to_raster_size((P + Plast) * 0.5f);
      L = len(P - Plast) / pixel_width;
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
      const float2 P = (Pstart + Pend) * 0.5f;
      res = T(patch, Pstart, P, depth, true) + T(patch, P, Pend, depth, true);
    }
  }

  res = limit_edge_factor(patch, Pstart, Pend, res);

  /* Limit edge factor so we don't go beyond max depth. */
  if (depth >= DSPLIT_MAX_DEPTH - 2) {
    if (res == DSPLIT_NON_UNIFORM || res > DSPLIT_MAX_SEGMENTS) {
      res = DSPLIT_MAX_SEGMENTS;
    }
  }

  return res;
}

int DiagSplit::limit_edge_factor(const Patch *patch,
                                 const float2 Pstart,
                                 const float2 Pend,
                                 const int T)
{
  const int max_t = 1 << params.max_level;
  int max_t_for_edge = int(max_t * len(Pstart - Pend));

  if (patch->from_ngon) {
    max_t_for_edge >>= 1; /* Initial split of ngon causes edges to extend half the distance. */
  }

  const int limit_T = (max_t_for_edge <= 1) ? 1 : min(T, max_t_for_edge);

  assert(limit_T >= 1 || limit_T == DSPLIT_NON_UNIFORM);
  return limit_T;
}

void DiagSplit::assign_edge_factor(SubEdge *edge, const int T)
{
  assert(edge->T == 0 || edge->T == DSPLIT_NON_UNIFORM);
  edge->T = T;

  if (edge->T != DSPLIT_NON_UNIFORM) {
    edge->second_vert_index = alloc_verts(edge->T - 1);
  }
}

void DiagSplit::resolve_edge_factors(const SubPatch &sub, const int depth)
{
  /* Compute edge factor if not already set. Or if DSPLIT_NON_UNIFORM and splitting is
   * no longer possible because the opposite side can't be split. */
  if (sub.edge_u0.edge->T == 0 ||
      (sub.edge_u0.edge->T == DSPLIT_NON_UNIFORM && sub.edge_u1.edge->T == 1))
  {
    assign_edge_factor(sub.edge_u0.edge, T(sub.patch, sub.uv00, sub.uv10, depth, true));
  }
  if (sub.edge_v1.edge->T == 0 ||
      (sub.edge_v1.edge->T == DSPLIT_NON_UNIFORM && sub.edge_v0.edge->T == 1))
  {
    assign_edge_factor(sub.edge_v1.edge, T(sub.patch, sub.uv10, sub.uv11, depth, true));
  }
  if (sub.edge_u1.edge->T == 0 ||
      (sub.edge_u1.edge->T == DSPLIT_NON_UNIFORM && sub.edge_u0.edge->T == 1))
  {
    assign_edge_factor(sub.edge_u1.edge, T(sub.patch, sub.uv11, sub.uv01, depth, true));
  }
  if (sub.edge_v0.edge->T == 0 ||
      (sub.edge_v0.edge->T == DSPLIT_NON_UNIFORM && sub.edge_v1.edge->T == 1))
  {
    assign_edge_factor(sub.edge_v0.edge, T(sub.patch, sub.uv01, sub.uv00, depth, true));
  }
}

float2 DiagSplit::split_edge(
    const Patch *patch, SubPatch::Edge *subedge, float2 Pstart, float2 Pend, const int depth)
{
  /* This splits following the direction of the edge itself, not subpatch edge direction. */
  if (subedge->reversed) {
    swap(Pstart, Pend);
  }

  SubEdge *edge = subedge->edge;

  if (edge->T == DSPLIT_NON_UNIFORM) {
    /* Split down the middle. */
    const float2 P = 0.5f * (Pstart + Pend);
    if (edge->mid_vert_index == -1) {
      /* Allocate mid vertex and edges. */
      edge->mid_vert_index = alloc_verts(1);

      SubEdge *edge_a = alloc_edge(edge->start_vert_index, edge->mid_vert_index);
      SubEdge *edge_b = alloc_edge(edge->mid_vert_index, edge->end_vert_index);
      assign_edge_factor(edge_a, T(patch, Pstart, P, depth));
      assign_edge_factor(edge_b, T(patch, P, Pend, depth));
    }
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

    SubEdge *edge_a = alloc_edge(edge->start_vert_index, edge->mid_vert_index);
    SubEdge *edge_b = alloc_edge(edge->mid_vert_index, edge->end_vert_index);
    edge_a->T = mid;
    edge_b->T = edge->T - mid;
    edge_a->second_vert_index = edge->second_vert_index;
    edge_b->second_vert_index = edge->second_vert_index + edge_a->T;
  }

  const float2 P = interp(Pstart, Pend, mid / (float)edge->T);
  assert(P.x >= 0 && P.x <= 1.0f && P.y >= 0.0f && P.y <= 1.0f);

  return P;
}

void DiagSplit::split(SubPatch &&sub, const int depth)
{
  /* Edge factors are limited so that this should never happen. */
  assert(depth <= DSPLIT_MAX_DEPTH);

  /* Set edge factors if we haven't already. */
  resolve_edge_factors(sub, depth);

  /* Split subpatch if edges are marked as DSPLIT_NON_UNIFORM,
   * or if the following conditions are met:
   * - Both edges have at least 2 segments.
   * - Either edge has more than DSPLIT_MAX_SEGMENTS segments.
   * - The ratio of segments for opposite edges doesn't exceed 1.5.
   *   This reduces over tessellation for some patches. */
  const int min_T_u = min(sub.edge_u0.edge->T, sub.edge_u1.edge->T);
  const int max_T_u = max(sub.edge_u0.edge->T, sub.edge_u1.edge->T);
  const int min_T_v = min(sub.edge_v0.edge->T, sub.edge_v1.edge->T);
  const int max_T_v = max(sub.edge_v0.edge->T, sub.edge_v1.edge->T);

  bool split_u = sub.edge_u0.edge->T == DSPLIT_NON_UNIFORM ||
                 sub.edge_u1.edge->T == DSPLIT_NON_UNIFORM ||
                 (min_T_u >= 2 && min_T_v > DSPLIT_MAX_SEGMENTS && max_T_v / min_T_v > 1.5f);
  bool split_v = sub.edge_v0.edge->T == DSPLIT_NON_UNIFORM ||
                 sub.edge_v1.edge->T == DSPLIT_NON_UNIFORM ||
                 (min_T_v >= 2 && min_T_u > DSPLIT_MAX_SEGMENTS && max_T_u / min_T_u > 1.5f);

  /* Alternate axis. */
  if (split_u && split_v) {
    split_u = depth % 2;
  }

  if (!split_u && !split_v) {
    /* Add the unsplit subpatch. */
    alloc_subpatch(std::move(sub));
    return;
  }

  /* Copy into new subpatches. */
  SubPatch sub_a = sub;
  SubPatch sub_b = sub;

  /* Pointers to various subpatch elements. */
  SubPatch::Edge *sub_across_0;
  SubPatch::Edge *sub_across_1;
  SubPatch::Edge *sub_a_across_0;
  SubPatch::Edge *sub_a_across_1;
  SubPatch::Edge *sub_b_across_0;
  SubPatch::Edge *sub_b_across_1;

  SubPatch::Edge *sub_a_split;
  SubPatch::Edge *sub_b_split;

  float2 *Pa;
  float2 *Pb;
  float2 *Pc;
  float2 *Pd;

  /* Set pointers based on split axis. */
  if (split_u) {
    /*
     *          sub_across_1
     *     -------Pa  Pc-------
     *     |       |  |       |
     *     |   A   |  |   B   |
     *     |       |  |       |
     *     -------Pb  Pd-------
     *          sub_across_0
     */
    sub_across_0 = &sub.edge_u0;
    sub_across_1 = &sub.edge_u1;
    sub_a_across_0 = &sub_a.edge_u0;
    sub_a_across_1 = &sub_a.edge_u1;
    sub_b_across_0 = &sub_b.edge_u0;
    sub_b_across_1 = &sub_b.edge_u1;

    sub_a_split = &sub_a.edge_v1;
    sub_b_split = &sub_b.edge_v0;

    Pa = &sub_a.uv11;
    Pb = &sub_a.uv10;
    Pc = &sub_b.uv01;
    Pd = &sub_b.uv00;
  }
  else {
    /*
     *                --------------------
     *                |        A         |
     *                Pb----------------Pa
     * sub_across_0                         sub_across_1
     *                Pd----------------Pc
     *                |        B         |
     *                --------------------
     */
    sub_across_0 = &sub.edge_v0;
    sub_across_1 = &sub.edge_v1;
    sub_a_across_0 = &sub_a.edge_v0;
    sub_a_across_1 = &sub_a.edge_v1;
    sub_b_across_0 = &sub_b.edge_v0;
    sub_b_across_1 = &sub_b.edge_v1;

    sub_a_split = &sub_a.edge_u0;
    sub_b_split = &sub_b.edge_u1;

    Pa = &sub_a.uv10;
    Pb = &sub_a.uv00;
    Pc = &sub_b.uv11;
    Pd = &sub_b.uv01;
  }

  /* Allocate new edges and vertices. */
  const float2 P0 = split_edge(sub.patch, sub_across_0, *Pd, *Pb, depth);
  const float2 P1 = split_edge(sub.patch, sub_across_1, *Pa, *Pc, depth);

  alloc_edge(sub_a_across_0, sub_across_0->start_vert_index(), sub_across_0->mid_vert_index());
  alloc_edge(sub_b_across_0, sub_across_0->mid_vert_index(), sub_across_0->end_vert_index());
  alloc_edge(sub_b_across_1, sub_across_1->start_vert_index(), sub_across_1->mid_vert_index());
  alloc_edge(sub_a_across_1, sub_across_1->mid_vert_index(), sub_across_1->end_vert_index());

  assert(sub_a_across_0->edge->T != 0);
  assert(sub_b_across_0->edge->T != 0);
  assert(sub_a_across_1->edge->T != 0);
  assert(sub_b_across_1->edge->T != 0);

  /* Split */
  *Pa = P1;
  *Pb = P0;

  *Pc = P1;
  *Pd = P0;

  /* Create new edge */
  alloc_edge(sub_a_split, sub_across_0->mid_vert_index(), sub_across_1->mid_vert_index());
  alloc_edge(sub_b_split, sub_across_1->mid_vert_index(), sub_across_0->mid_vert_index());

  /* Set T for split edge. */
  int tsplit = T(sub.patch, P0, P1, depth);
  if (depth == -2 && tsplit == 1) {
    tsplit = 2; /* Ensure we can always split at depth -1. */
  }
  assign_edge_factor(sub_a_split->edge, tsplit);

  /* Recurse */
  split(std::move(sub_a), depth + 1);
  split(std::move(sub_b), depth + 1);
}

void DiagSplit::split_quad(const Mesh::SubdFace &face, const Patch *patch)
{
  /*
   *                edge_u1
   *          uv01 ←-------- uv11
   *          |                 ↑
   *  edge_v0 |                 | edge_v1
   *          ↓                 |
   *          uv00 --------→ uv10
   *                edge_u0
   */

  const int *subd_face_corners = params.mesh->get_subd_face_corners().data();
  const int v00 = subd_face_corners[face.start_corner + 0];
  const int v10 = subd_face_corners[face.start_corner + 1];
  const int v11 = subd_face_corners[face.start_corner + 2];
  const int v01 = subd_face_corners[face.start_corner + 3];

  SubPatch subpatch(patch);
  alloc_edge(&subpatch.edge_u0, v00, v10);
  alloc_edge(&subpatch.edge_v1, v10, v11);
  alloc_edge(&subpatch.edge_u1, v11, v01);
  alloc_edge(&subpatch.edge_v0, v01, v00);

  /* Forces a split in both axis for quads, needed to match split of ngons into quads. */
  subpatch.edge_u0.edge->T = DSPLIT_NON_UNIFORM;
  subpatch.edge_v0.edge->T = DSPLIT_NON_UNIFORM;
  subpatch.edge_u1.edge->T = DSPLIT_NON_UNIFORM;
  subpatch.edge_v1.edge->T = DSPLIT_NON_UNIFORM;

  split(std::move(subpatch), -2);
}

void DiagSplit::split_ngon(const Mesh::SubdFace &face,
                           const Patch *patches,
                           const size_t patches_byte_stride)
{
  const int *subd_face_corners = params.mesh->get_subd_face_corners().data();
  const int v11 = alloc_verts(1);

  for (int corner = 0; corner < face.num_corners; corner++) {
    const Patch *patch = (const Patch *)(((char *)patches) + (corner * patches_byte_stride));

    /*         vprev         .
     *           .           .
     *           .  edge_u1  .
     *          v01 ←------ v11 . . .
     *           |           ↑
     *  edge_v0  |           | edge_v1
     *           ↓           |
     *          v00 ------→ v10 . . vnext
     *              edge_u0
     */

    /* Setup edges. */
    const int v00 = subd_face_corners[face.start_corner + corner];
    const int vprev = subd_face_corners[face.start_corner +
                                        mod(corner + face.num_corners - 1, face.num_corners)];
    const int vnext = subd_face_corners[face.start_corner + mod(corner + 1, face.num_corners)];

    SubPatch::Edge edge_u0;
    SubPatch::Edge edge_v0;

    alloc_edge(&edge_u0, v00, vnext);
    alloc_edge(&edge_v0, vprev, v00);

    if (edge_u0.edge->mid_vert_index == -1) {
      edge_u0.edge->mid_vert_index = alloc_verts(1);
    }
    if (edge_v0.edge->mid_vert_index == -1) {
      edge_v0.edge->mid_vert_index = alloc_verts(1);
    }

    const int v10 = edge_u0.edge->mid_vert_index;
    const int v01 = edge_v0.edge->mid_vert_index;

    SubPatch subpatch(patch);
    alloc_edge(&subpatch.edge_u0, v00, v10);
    alloc_edge(&subpatch.edge_v1, v10, v11);
    alloc_edge(&subpatch.edge_u1, v11, v01);
    alloc_edge(&subpatch.edge_v0, v01, v00);

    /* Perform split. */
    split(std::move(subpatch), 0);
  }
}

void DiagSplit::split_patches(const Patch *patches, const size_t patches_byte_stride)
{
  /* Keep base mesh vertices, create new triangels. */
  num_verts = params.mesh->get_num_subd_base_verts();
  num_triangles = 0;

  /* Split all faces in the mesh. */
  int patch_index = 0;
  for (int f = 0; f < params.mesh->get_num_subd_faces(); f++) {
    Mesh::SubdFace face = params.mesh->get_subd_face(f);
    const Patch *patch = (const Patch *)(((char *)patches) + (patch_index * patches_byte_stride));

    if (face.is_quad()) {
      patch_index++;
      split_quad(face, patch);
    }
    else {
      patch_index += face.num_corners;
      split_ngon(face, patch, patches_byte_stride);
    }
  }

  // TODO: avoid multiple write for shared vertices
  // TODO: avoid multiple write for linear vert attributes
  // TODO: avoid multiple write for smooth vert attributes
  // TODO: support not splitting n-gons if not needed
  // TODO: multithreading

  /* Dice all patches. */
  QuadDice dice(params);
  dice.reserve(num_verts, num_triangles);

  for (SubPatch &sub : subpatches) {
    dice.dice(sub);
  }

  /* Cleanup */
  subpatches.clear();
  edges.clear();
}

CCL_NAMESPACE_END
