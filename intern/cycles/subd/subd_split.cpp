/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "render/camera.h"
#include "render/mesh.h"

#include "subd/subd_dice.h"
#include "subd/subd_patch.h"
#include "subd/subd_split.h"

#include "util/util_algorithm.h"
#include "util/util_foreach.h"
#include "util/util_hash.h"
#include "util/util_math.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

/* DiagSplit */

#define DSPLIT_NON_UNIFORM -1
#define STITCH_NGON_CENTER_VERT_INDEX_OFFSET 0x60000000
#define STITCH_NGON_SPLIT_EDGE_CENTER_VERT_TAG (0x60000000 - 1)

DiagSplit::DiagSplit(const SubdParams &params_) : params(params_)
{
}

float3 DiagSplit::to_world(Patch *patch, float2 uv)
{
  float3 P;

  patch->eval(&P, NULL, NULL, NULL, uv.x, uv.y);
  if (params.camera)
    P = transform_point(&params.objecttoworld, P);

  return P;
}

static void order_float2(float2 &a, float2 &b)
{
  if (b.x < a.x || b.y < a.y) {
    swap(a, b);
  }
}

int DiagSplit::T(Patch *patch, float2 Pstart, float2 Pend, bool recursive_resolve)
{
  order_float2(Pstart, Pend); /* May not be necessary, but better to be safe. */

  float Lsum = 0.0f;
  float Lmax = 0.0f;

  float3 Plast = to_world(patch, Pstart);

  for (int i = 1; i < params.test_steps; i++) {
    float t = i / (float)(params.test_steps - 1);

    float3 P = to_world(patch, Pstart + t * (Pend - Pstart));

    float L;

    if (!params.camera) {
      L = len(P - Plast);
    }
    else {
      Camera *cam = params.camera;

      float pixel_width = cam->world_to_raster_size((P + Plast) * 0.5f);
      L = len(P - Plast) / pixel_width;
    }

    Lsum += L;
    Lmax = max(L, Lmax);

    Plast = P;
  }

  int tmin = (int)ceilf(Lsum / params.dicing_rate);
  int tmax = (int)ceilf((params.test_steps - 1) * Lmax /
                        params.dicing_rate);  // XXX paper says N instead of N-1, seems wrong?
  int res = max(tmax, 1);

  if (tmax - tmin > params.split_threshold) {
    if (!recursive_resolve) {
      res = DSPLIT_NON_UNIFORM;
    }
    else {
      float2 P = (Pstart + Pend) * 0.5f;
      res = T(patch, Pstart, P, true) + T(patch, P, Pend, true);
    }
  }

  limit_edge_factor(res, patch, Pstart, Pend);
  return res;
}

void DiagSplit::partition_edge(
    Patch *patch, float2 *P, int *t0, int *t1, float2 Pstart, float2 Pend, int t)
{
  if (t == DSPLIT_NON_UNIFORM) {
    *P = (Pstart + Pend) * 0.5f;
    *t0 = T(patch, Pstart, *P);
    *t1 = T(patch, *P, Pend);
  }
  else {
    assert(t >= 2); /* Need at least two segments to partition into. */

    int I = (int)floorf((float)t * 0.5f);
    *P = interp(Pstart, Pend, I / (float)t);
    *t0 = I;
    *t1 = t - I;
  }
}

void DiagSplit::limit_edge_factor(int &T, Patch *patch, float2 Pstart, float2 Pend)
{
  int max_t = 1 << params.max_level;
  int max_t_for_edge = int(max_t * len(Pstart - Pend));

  if (patch->from_ngon) {
    max_t_for_edge >>= 1; /* Initial split of ngon causes edges to extend half the distance. */
  }

  T = (max_t_for_edge <= 1) ? 1 : min(T, max_t_for_edge);

  assert(T >= 1 || T == DSPLIT_NON_UNIFORM);
}

void DiagSplit::resolve_edge_factors(Subpatch &sub)
{
  /* Resolve DSPLIT_NON_UNIFORM to actual T value if splitting is no longer possible. */
  if (sub.edge_u0.T == 1 && sub.edge_u1.T == DSPLIT_NON_UNIFORM) {
    sub.edge_u1.T = T(sub.patch, sub.c01, sub.c11, true);
  }
  if (sub.edge_u1.T == 1 && sub.edge_u0.T == DSPLIT_NON_UNIFORM) {
    sub.edge_u0.T = T(sub.patch, sub.c00, sub.c10, true);
  }
  if (sub.edge_v0.T == 1 && sub.edge_v1.T == DSPLIT_NON_UNIFORM) {
    sub.edge_v1.T = T(sub.patch, sub.c11, sub.c10, true);
  }
  if (sub.edge_v1.T == 1 && sub.edge_v0.T == DSPLIT_NON_UNIFORM) {
    sub.edge_v0.T = T(sub.patch, sub.c01, sub.c00, true);
  }
}

void DiagSplit::split(Subpatch &sub, int depth)
{
  if (depth > 32) {
    /* We should never get here, but just in case end recursion safely. */
    assert(!"diagsplit recursion limit reached");

    sub.edge_u0.T = 1;
    sub.edge_u1.T = 1;
    sub.edge_v0.T = 1;
    sub.edge_v1.T = 1;

    subpatches.push_back(sub);
    return;
  }

  bool split_u = (sub.edge_u0.T == DSPLIT_NON_UNIFORM || sub.edge_u1.T == DSPLIT_NON_UNIFORM);
  bool split_v = (sub.edge_v0.T == DSPLIT_NON_UNIFORM || sub.edge_v1.T == DSPLIT_NON_UNIFORM);

  /* Split subpatches such that the ratio of T for opposite edges doesn't
   * exceed 1.5, this reduces over tessellation for some patches
   */
  /* clang-format off */
  if (min(sub.edge_u0.T, sub.edge_u1.T) > 8 && /* Must be uniform and preferably greater than 8 to split. */
      min(sub.edge_v0.T, sub.edge_v1.T) >= 2 && /* Must be uniform and at least 2 to split. */
      max(sub.edge_u0.T, sub.edge_u1.T) / min(sub.edge_u0.T, sub.edge_u1.T) > 1.5f)
  {
    split_v = true;
  }
  if (min(sub.edge_v0.T, sub.edge_v1.T) > 8 &&
      min(sub.edge_u0.T, sub.edge_u1.T) >= 2 &&
      max(sub.edge_v0.T, sub.edge_v1.T) / min(sub.edge_v0.T, sub.edge_v1.T) > 1.5f)
  {
    split_u = true;
  }
  /* clang-format on */

  /* Alternate axis. */
  if (split_u && split_v) {
    split_u = depth % 2;
  }

  if (!split_u && !split_v) {
    /* Add the unsplit subpatch. */
    subpatches.push_back(sub);
    Subpatch &subpatch = subpatches[subpatches.size() - 1];

    /* Update T values and offsets. */
    for (int i = 0; i < 4; i++) {
      Subpatch::edge_t &edge = subpatch.edges[i];

      edge.offset = edge.edge->T;
      edge.edge->T += edge.T;
    }
  }
  else {
    /* Copy into new subpatches. */
    Subpatch sub_a = sub;
    Subpatch sub_b = sub;

    /* Pointers to various subpatch elements. */
    Subpatch::edge_t *sub_across_0, *sub_across_1;
    Subpatch::edge_t *sub_a_across_0, *sub_a_across_1;
    Subpatch::edge_t *sub_b_across_0, *sub_b_across_1;

    Subpatch::edge_t *sub_a_split, *sub_b_split;

    float2 *Pa, *Pb, *Pc, *Pd;

    /* Set pointers based on split axis. */
    if (split_u) {
      sub_across_0 = &sub.edge_u0;
      sub_across_1 = &sub.edge_u1;
      sub_a_across_0 = &sub_a.edge_u0;
      sub_a_across_1 = &sub_a.edge_u1;
      sub_b_across_0 = &sub_b.edge_u0;
      sub_b_across_1 = &sub_b.edge_u1;

      sub_a_split = &sub_a.edge_v1;
      sub_b_split = &sub_b.edge_v0;

      Pa = &sub_a.c11;
      Pb = &sub_a.c10;
      Pc = &sub_b.c01;
      Pd = &sub_b.c00;
    }
    else {
      sub_across_0 = &sub.edge_v0;
      sub_across_1 = &sub.edge_v1;
      sub_a_across_0 = &sub_a.edge_v0;
      sub_a_across_1 = &sub_a.edge_v1;
      sub_b_across_0 = &sub_b.edge_v0;
      sub_b_across_1 = &sub_b.edge_v1;

      sub_a_split = &sub_a.edge_u0;
      sub_b_split = &sub_b.edge_u1;

      Pa = &sub_a.c10;
      Pb = &sub_a.c00;
      Pc = &sub_b.c11;
      Pd = &sub_b.c01;
    }

    /* Partition edges */
    float2 P0, P1;

    partition_edge(
        sub.patch, &P0, &sub_a_across_0->T, &sub_b_across_0->T, *Pd, *Pb, sub_across_0->T);
    partition_edge(
        sub.patch, &P1, &sub_a_across_1->T, &sub_b_across_1->T, *Pc, *Pa, sub_across_1->T);

    /* Split */
    *Pa = P1;
    *Pb = P0;

    *Pc = P1;
    *Pd = P0;

    int tsplit = T(sub.patch, P0, P1);

    if (depth == -2 && tsplit == 1) {
      tsplit = 2; /* Ensure we can always split at depth -1. */
    }

    sub_a_split->T = tsplit;
    sub_b_split->T = tsplit;

    resolve_edge_factors(sub_a);
    resolve_edge_factors(sub_b);

    /* Create new edge */
    Edge &edge = *alloc_edge();

    sub_a_split->edge = &edge;
    sub_b_split->edge = &edge;

    sub_a_split->offset = 0;
    sub_b_split->offset = 0;

    sub_a_split->indices_decrease_along_edge = false;
    sub_b_split->indices_decrease_along_edge = true;

    sub_a_split->sub_edges_created_in_reverse_order = !split_u;
    sub_b_split->sub_edges_created_in_reverse_order = !split_u;

    edge.top_indices_decrease = sub_across_1->sub_edges_created_in_reverse_order;
    edge.bottom_indices_decrease = sub_across_0->sub_edges_created_in_reverse_order;

    /* Recurse */
    edge.T = 0;
    split(sub_a, depth + 1);

    int edge_t = edge.T;
    (void)edge_t;

    edge.top_offset = sub_across_1->edge->T;
    edge.bottom_offset = sub_across_0->edge->T;

    edge.T = 0; /* We calculate T twice along each edge. :/ */
    split(sub_b, depth + 1);

    assert(edge.T == edge_t); /* If this fails we will crash at some later point! */

    edge.top = sub_across_1->edge;
    edge.bottom = sub_across_0->edge;
  }
}

int DiagSplit::alloc_verts(int n)
{
  int a = num_alloced_verts;
  num_alloced_verts += n;
  return a;
}

Edge *DiagSplit::alloc_edge()
{
  edges.emplace_back();
  return &edges.back();
}

void DiagSplit::split_patches(Patch *patches, size_t patches_byte_stride)
{
  int patch_index = 0;

  for (int f = 0; f < params.mesh->get_num_subd_faces(); f++) {
    Mesh::SubdFace face = params.mesh->get_subd_face(f);

    Patch *patch = (Patch *)(((char *)patches) + patch_index * patches_byte_stride);

    if (face.is_quad()) {
      patch_index++;

      split_quad(face, patch);
    }
    else {
      patch_index += face.num_corners;

      split_ngon(face, patch, patches_byte_stride);
    }
  }

  params.mesh->vert_to_stitching_key_map.clear();
  params.mesh->vert_stitching_map.clear();

  post_split();
}

static Edge *create_edge_from_corner(DiagSplit *split,
                                     const Mesh *mesh,
                                     const Mesh::SubdFace &face,
                                     int corner,
                                     bool &reversed,
                                     int v0,
                                     int v1)
{
  int a = mesh->get_subd_face_corners()[face.start_corner + mod(corner + 0, face.num_corners)];
  int b = mesh->get_subd_face_corners()[face.start_corner + mod(corner + 1, face.num_corners)];

  reversed = !(b < a);

  if (b < a) {
    swap(a, b);
    swap(v0, v1);
  }

  Edge *edge = split->alloc_edge();

  edge->is_stitch_edge = true;
  edge->stitch_start_vert_index = a;
  edge->stitch_end_vert_index = b;

  edge->start_vert_index = v0;
  edge->end_vert_index = v1;

  edge->stitch_edge_key = {a, b};

  return edge;
}

void DiagSplit::split_quad(const Mesh::SubdFace &face, Patch *patch)
{
  Subpatch subpatch(patch);

  int v = alloc_verts(4);

  bool v0_reversed, u1_reversed, v1_reversed, u0_reversed;
  subpatch.edge_v0.edge = create_edge_from_corner(
      this, params.mesh, face, 3, v0_reversed, v + 3, v + 0);
  subpatch.edge_u1.edge = create_edge_from_corner(
      this, params.mesh, face, 2, u1_reversed, v + 2, v + 3);
  subpatch.edge_v1.edge = create_edge_from_corner(
      this, params.mesh, face, 1, v1_reversed, v + 1, v + 2);
  subpatch.edge_u0.edge = create_edge_from_corner(
      this, params.mesh, face, 0, u0_reversed, v + 0, v + 1);

  subpatch.edge_v0.sub_edges_created_in_reverse_order = !v0_reversed;
  subpatch.edge_u1.sub_edges_created_in_reverse_order = u1_reversed;
  subpatch.edge_v1.sub_edges_created_in_reverse_order = v1_reversed;
  subpatch.edge_u0.sub_edges_created_in_reverse_order = !u0_reversed;

  subpatch.edge_v0.indices_decrease_along_edge = v0_reversed;
  subpatch.edge_u1.indices_decrease_along_edge = u1_reversed;
  subpatch.edge_v1.indices_decrease_along_edge = v1_reversed;
  subpatch.edge_u0.indices_decrease_along_edge = u0_reversed;

  /* Forces a split in both axis for quads, needed to match split of ngons into quads. */
  subpatch.edge_u0.T = DSPLIT_NON_UNIFORM;
  subpatch.edge_u1.T = DSPLIT_NON_UNIFORM;
  subpatch.edge_v0.T = DSPLIT_NON_UNIFORM;
  subpatch.edge_v1.T = DSPLIT_NON_UNIFORM;

  split(subpatch, -2);
}

static Edge *create_split_edge_from_corner(DiagSplit *split,
                                           const Mesh *mesh,
                                           const Mesh::SubdFace &face,
                                           int corner,
                                           int side,
                                           bool &reversed,
                                           int v0,
                                           int v1,
                                           int vc)
{
  Edge *edge = split->alloc_edge();

  int a = mesh->get_subd_face_corners()[face.start_corner + mod(corner + 0, face.num_corners)];
  int b = mesh->get_subd_face_corners()[face.start_corner + mod(corner + 1, face.num_corners)];

  if (b < a) {
    edge->stitch_edge_key = {b, a};
  }
  else {
    edge->stitch_edge_key = {a, b};
  }

  reversed = !(b < a);

  if (side == 0) {
    a = vc;
  }
  else {
    b = vc;
  }

  if (!reversed) {
    swap(a, b);
    swap(v0, v1);
  }

  edge->is_stitch_edge = true;
  edge->stitch_start_vert_index = a;
  edge->stitch_end_vert_index = b;

  edge->start_vert_index = v0;
  edge->end_vert_index = v1;

  return edge;
}

void DiagSplit::split_ngon(const Mesh::SubdFace &face, Patch *patches, size_t patches_byte_stride)
{
  Edge *prev_edge_u0 = nullptr;
  Edge *first_edge_v0 = nullptr;

  for (int corner = 0; corner < face.num_corners; corner++) {
    Patch *patch = (Patch *)(((char *)patches) + corner * patches_byte_stride);

    Subpatch subpatch(patch);

    int v = alloc_verts(4);

    /* Setup edges. */
    Edge *edge_u1 = alloc_edge();
    Edge *edge_v1 = alloc_edge();

    edge_v1->is_stitch_edge = true;
    edge_u1->is_stitch_edge = true;

    edge_u1->stitch_start_vert_index = -(face.start_corner + mod(corner + 0, face.num_corners)) -
                                       1;
    edge_u1->stitch_end_vert_index = STITCH_NGON_CENTER_VERT_INDEX_OFFSET + face.ptex_offset;

    edge_u1->start_vert_index = v + 3;
    edge_u1->end_vert_index = v + 2;

    edge_u1->stitch_edge_key = {edge_u1->stitch_start_vert_index, edge_u1->stitch_end_vert_index};

    edge_v1->stitch_start_vert_index = -(face.start_corner + mod(corner + 1, face.num_corners)) -
                                       1;
    edge_v1->stitch_end_vert_index = STITCH_NGON_CENTER_VERT_INDEX_OFFSET + face.ptex_offset;

    edge_v1->start_vert_index = v + 1;
    edge_v1->end_vert_index = v + 2;

    edge_v1->stitch_edge_key = {edge_v1->stitch_start_vert_index, edge_v1->stitch_end_vert_index};

    bool v0_reversed, u0_reversed;

    subpatch.edge_v0.edge = create_split_edge_from_corner(this,
                                                          params.mesh,
                                                          face,
                                                          corner - 1,
                                                          0,
                                                          v0_reversed,
                                                          v + 3,
                                                          v + 0,
                                                          STITCH_NGON_SPLIT_EDGE_CENTER_VERT_TAG);

    subpatch.edge_u1.edge = edge_u1;
    subpatch.edge_v1.edge = edge_v1;

    subpatch.edge_u0.edge = create_split_edge_from_corner(this,
                                                          params.mesh,
                                                          face,
                                                          corner + 0,
                                                          1,
                                                          u0_reversed,
                                                          v + 0,
                                                          v + 1,
                                                          STITCH_NGON_SPLIT_EDGE_CENTER_VERT_TAG);

    subpatch.edge_v0.sub_edges_created_in_reverse_order = !v0_reversed;
    subpatch.edge_u1.sub_edges_created_in_reverse_order = false;
    subpatch.edge_v1.sub_edges_created_in_reverse_order = true;
    subpatch.edge_u0.sub_edges_created_in_reverse_order = !u0_reversed;

    subpatch.edge_v0.indices_decrease_along_edge = v0_reversed;
    subpatch.edge_u1.indices_decrease_along_edge = false;
    subpatch.edge_v1.indices_decrease_along_edge = true;
    subpatch.edge_u0.indices_decrease_along_edge = u0_reversed;

    /* Perform split. */
    {
      subpatch.edge_u0.T = T(subpatch.patch, subpatch.c00, subpatch.c10);
      subpatch.edge_u1.T = T(subpatch.patch, subpatch.c01, subpatch.c11);
      subpatch.edge_v0.T = T(subpatch.patch, subpatch.c00, subpatch.c01);
      subpatch.edge_v1.T = T(subpatch.patch, subpatch.c10, subpatch.c11);

      resolve_edge_factors(subpatch);

      split(subpatch, 0);
    }

    /* Update offsets after T is known from split. */
    edge_u1->top = subpatch.edge_v0.edge;
    edge_u1->stitch_top_offset = edge_u1->top->T * (v0_reversed ? -1 : 1);
    edge_v1->top = subpatch.edge_u0.edge;
    edge_v1->stitch_top_offset = edge_v1->top->T * (!u0_reversed ? -1 : 1);

    if (corner == 0) {
      first_edge_v0 = subpatch.edge_v0.edge;
    }

    if (prev_edge_u0) {
      if (v0_reversed) {
        subpatch.edge_v0.edge->stitch_offset = prev_edge_u0->T;
      }
      else {
        prev_edge_u0->stitch_offset = subpatch.edge_v0.edge->T;
      }

      int T = subpatch.edge_v0.edge->T + prev_edge_u0->T;
      subpatch.edge_v0.edge->stitch_edge_T = T;
      prev_edge_u0->stitch_edge_T = T;
    }

    if (corner == face.num_corners - 1) {
      if (v0_reversed) {
        subpatch.edge_u0.edge->stitch_offset = first_edge_v0->T;
      }
      else {
        first_edge_v0->stitch_offset = subpatch.edge_u0.edge->T;
      }

      int T = first_edge_v0->T + subpatch.edge_u0.edge->T;
      first_edge_v0->stitch_edge_T = T;
      subpatch.edge_u0.edge->stitch_edge_T = T;
    }

    prev_edge_u0 = subpatch.edge_u0.edge;
  }
}

void DiagSplit::post_split()
{
  int num_stitch_verts = 0;

  /* All patches are now split, and all T values known. */

  foreach (Edge &edge, edges) {
    if (edge.second_vert_index < 0) {
      edge.second_vert_index = alloc_verts(edge.T - 1);
    }

    if (edge.is_stitch_edge) {
      num_stitch_verts = max(num_stitch_verts,
                             max(edge.stitch_start_vert_index, edge.stitch_end_vert_index));
    }
  }

  num_stitch_verts += 1;

  /* Map of edge key to edge stitching vert offset. */
  struct pair_hasher {
    size_t operator()(const pair<int, int> &k) const
    {
      return hash_uint2(k.first, k.second);
    }
  };
  typedef unordered_map<pair<int, int>, int, pair_hasher> edge_stitch_verts_map_t;
  edge_stitch_verts_map_t edge_stitch_verts_map;

  foreach (Edge &edge, edges) {
    if (edge.is_stitch_edge) {
      if (edge.stitch_edge_T == 0) {
        edge.stitch_edge_T = edge.T;
      }

      if (edge_stitch_verts_map.find(edge.stitch_edge_key) == edge_stitch_verts_map.end()) {
        edge_stitch_verts_map[edge.stitch_edge_key] = num_stitch_verts;
        num_stitch_verts += edge.stitch_edge_T - 1;
      }
    }
  }

  /* Set start and end indices for edges generated from a split. */
  foreach (Edge &edge, edges) {
    if (edge.start_vert_index < 0) {
      /* Fix up offsets. */
      if (edge.top_indices_decrease) {
        edge.top_offset = edge.top->T - edge.top_offset;
      }

      edge.start_vert_index = edge.top->get_vert_along_edge(edge.top_offset);
    }

    if (edge.end_vert_index < 0) {
      if (edge.bottom_indices_decrease) {
        edge.bottom_offset = edge.bottom->T - edge.bottom_offset;
      }

      edge.end_vert_index = edge.bottom->get_vert_along_edge(edge.bottom_offset);
    }
  }

  int vert_offset = params.mesh->verts.size();

  /* Add verts to stitching map. */
  foreach (const Edge &edge, edges) {
    if (edge.is_stitch_edge) {
      int second_stitch_vert_index = edge_stitch_verts_map[edge.stitch_edge_key];

      for (int i = 0; i <= edge.T; i++) {
        /* Get proper stitching key. */
        int key;

        if (i == 0) {
          key = edge.stitch_start_vert_index;
        }
        else if (i == edge.T) {
          key = edge.stitch_end_vert_index;
        }
        else {
          key = second_stitch_vert_index + i - 1 + edge.stitch_offset;
        }

        if (key == STITCH_NGON_SPLIT_EDGE_CENTER_VERT_TAG) {
          if (i == 0) {
            key = second_stitch_vert_index - 1 + edge.stitch_offset;
          }
          else if (i == edge.T) {
            key = second_stitch_vert_index - 1 + edge.T;
          }
        }
        else if (key < 0 && edge.top) { /* ngon spoke edge */
          int s = edge_stitch_verts_map[edge.top->stitch_edge_key];
          if (edge.stitch_top_offset >= 0) {
            key = s - 1 + edge.stitch_top_offset;
          }
          else {
            key = s - 1 + edge.top->stitch_edge_T + edge.stitch_top_offset;
          }
        }

        /* Get real vert index. */
        int vert = edge.get_vert_along_edge(i) + vert_offset;

        /* Add to map */
        if (params.mesh->vert_to_stitching_key_map.find(vert) ==
            params.mesh->vert_to_stitching_key_map.end()) {
          params.mesh->vert_to_stitching_key_map[vert] = key;
          params.mesh->vert_stitching_map.insert({key, vert});
        }
      }
    }
  }

  /* Dice; TODO(mai): Move this out of split. */
  QuadDice dice(params);

  int num_verts = num_alloced_verts;
  int num_triangles = 0;

  for (size_t i = 0; i < subpatches.size(); i++) {
    subpatches[i].inner_grid_vert_offset = num_verts;
    num_verts += subpatches[i].calc_num_inner_verts();
    num_triangles += subpatches[i].calc_num_triangles();
  }

  dice.reserve(num_verts, num_triangles);

  for (size_t i = 0; i < subpatches.size(); i++) {
    Subpatch &sub = subpatches[i];

    sub.edge_u0.T = max(sub.edge_u0.T, 1);
    sub.edge_u1.T = max(sub.edge_u1.T, 1);
    sub.edge_v0.T = max(sub.edge_v0.T, 1);
    sub.edge_v1.T = max(sub.edge_v1.T, 1);

    dice.dice(sub);
  }

  /* Cleanup */
  subpatches.clear();
  edges.clear();
}

CCL_NAMESPACE_END
