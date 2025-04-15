/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/camera.h"
#include "scene/mesh.h"

#include "subd/dice.h"
#include "subd/interpolation.h"
#include "subd/patch.h"
#include "subd/split.h"

#include "util/tbb.h"

CCL_NAMESPACE_BEGIN

EdgeDice::EdgeDice(const SubdParams &params_,
                   const int num_verts,
                   const int num_triangles,
                   SubdAttributeInterpolation &interpolation)
    : params(params_), interpolation(interpolation)
{
  Mesh *mesh = params.mesh;

  mesh->num_subd_added_verts = num_verts - mesh->get_verts().size();
  mesh->resize_mesh(num_verts, num_triangles);

  Attribute *attr_vN = mesh->attributes.add(ATTR_STD_VERTEX_NORMAL);

  mesh_triangles = mesh->triangles.data();
  mesh_shader = mesh->shader.data();
  mesh_smooth = mesh->smooth.data();
  mesh_P = mesh->verts.data();
  mesh_N = attr_vN->data_float3();

  if (params.ptex) {
    Attribute *attr_ptex_face_id = params.mesh->attributes.add(ATTR_STD_PTEX_FACE_ID);
    Attribute *attr_ptex_uv = params.mesh->attributes.add(ATTR_STD_PTEX_UV);

    mesh_ptex_face_id = attr_ptex_face_id->data_float();
    mesh_ptex_uv = attr_ptex_uv->data_float2();
  }

  interpolation.setup();
}

float3 EdgeDice::eval_projected(const SubPatch &sub, const float2 uv)
{
  float3 P;

  sub.patch->eval(&P, nullptr, nullptr, nullptr, uv.x, uv.y);
  if (params.camera) {
    P = transform_perspective(&params.camera->worldtoraster, P);
  }

  return P;
}

float EdgeDice::quad_area(const float3 &a, const float3 &b, const float3 &c, const float3 &d)
{
  return triangle_area(a, b, d) + triangle_area(a, d, c);
}

float EdgeDice::scale_factor(const SubPatch &sub, const int Mu, const int Mv)
{
  /* estimate area as 4x largest of 4 quads */
  float3 P[3][3];

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      P[i][j] = eval_projected(sub, sub.map_uv(make_float2(i * 0.5f, j * 0.5f)));
    }
  }

  const float A1 = quad_area(P[0][0], P[1][0], P[0][1], P[1][1]);
  const float A2 = quad_area(P[1][0], P[2][0], P[1][1], P[2][1]);
  const float A3 = quad_area(P[0][1], P[1][1], P[0][2], P[1][2]);
  const float A4 = quad_area(P[1][1], P[2][1], P[1][2], P[2][2]);
  const float Apatch = max(A1, max(A2, max(A3, A4))) * 4.0f;

  /* solve for scaling factor */
  const float Atri = params.dicing_rate * params.dicing_rate * 0.5f;
  const float Ntris = Apatch / Atri;

  // XXX does the -sqrt solution matter
  // XXX max(D, 0.0) is highly suspicious, need to test cases
  // where D goes negative
  const float N = 0.5f * (Ntris - (sub.edges[0].edge->T + sub.edges[2].edge->T +
                                   sub.edges[3].edge->T + sub.edges[1].edge->T));
  const float D = (4.0f * N * Mu * Mv) + ((Mu + Mv) * (Mu + Mv));
  const float S = (Mu + Mv + sqrtf(max(D, 0.0f))) / (2 * Mu * Mv);

  return S;
}

void EdgeDice::set_vertex(const SubPatch &sub, const int index, const float2 uv)
{
  assert(index < params.mesh->verts.size());

  float3 P;
  float3 N;

  sub.patch->eval(&P, nullptr, nullptr, &N, uv.x, uv.y);

  mesh_P[index] = P;
  mesh_N[index] = N;

  for (const SubdAttribute &attr : interpolation.vertex_attributes) {
    attr.interp(sub.patch->patch_index, sub.face_index, sub.corner, &index, &uv, 1);
  }
}

void EdgeDice::set_triangle(const SubPatch &sub,
                            const int triangle_index,
                            const int v0,
                            const int v1,
                            const int v2,
                            const float2 uv0,
                            const float2 uv1,
                            const float2 uv2)
{
  assert(triangle_index * 3 < params.mesh->triangles.size());

  const Patch *patch = sub.patch;

  mesh_triangles[triangle_index * 3 + 0] = v0;
  mesh_triangles[triangle_index * 3 + 1] = v1;
  mesh_triangles[triangle_index * 3 + 2] = v2;
  mesh_shader[triangle_index] = patch->shader;
  mesh_smooth[triangle_index] = patch->smooth;

  if (mesh_ptex_face_id) {
    mesh_ptex_face_id[triangle_index] = patch->patch_index;
  }
  if (mesh_ptex_uv) {
    mesh_ptex_uv[triangle_index * 3 + 0] = uv0;
    mesh_ptex_uv[triangle_index * 3 + 0] = uv1;
    mesh_ptex_uv[triangle_index * 3 + 0] = uv2;
  }

  /* TODO: batch together multiple triangles. */
  float2 uv[3] = {uv0, uv1, uv2};
  for (const SubdAttribute &attr : interpolation.triangle_attributes) {
    attr.interp(sub.patch->patch_index, sub.face_index, sub.corner, &triangle_index, uv, 1);
  }
}

void EdgeDice::add_grid_triangles_and_stitch(const SubPatch &sub, const int Mu, const int Mv)
{
  const float du = 1.0f / (float)Mu;
  const float dv = 1.0f / (float)Mv;
  const int grid_vertex_offset = sub.inner_grid_vert_offset;
  int triangle_index = sub.triangles_offset;

  /* Create inner grid. */
  for (int j = 1; j < Mv; j++) {
    for (int i = 1; i < Mu; i++) {
      const float u = i * du;
      const float v = j * dv;
      const int center_i = grid_vertex_offset + (i - 1) + (j - 1) * (Mu - 1);

      set_vertex(sub, center_i, sub.map_uv(make_float2(u, v)));

      if (i < Mu - 1 && j < Mv - 1) {
        const int i1 = grid_vertex_offset + (i - 1) + (j - 1) * (Mu - 1);
        const int i2 = grid_vertex_offset + i + (j - 1) * (Mu - 1);
        const int i3 = grid_vertex_offset + i + j * (Mu - 1);
        const int i4 = grid_vertex_offset + (i - 1) + j * (Mu - 1);

        const float2 uv1 = sub.map_uv(make_float2(u, v));
        const float2 uv2 = sub.map_uv(make_float2(u + du, v));
        const float2 uv3 = sub.map_uv(make_float2(u + du, v + dv));
        const float2 uv4 = sub.map_uv(make_float2(u, v + dv));

        set_triangle(sub, triangle_index++, i1, i2, i3, uv1, uv2, uv3);
        set_triangle(sub, triangle_index++, i1, i3, i4, uv1, uv3, uv4);
      }
    }
  }

  /* Stitch inner grid to edges. */
  for (int edge = 0; edge < 4; edge++) {
    const int outer_T = sub.edges[edge].edge->T;
    const int inner_T = ((edge % 2) == 0) ? Mu - 2 : Mv - 2;

    float2 inner_uv, outer_uv, inner_uv_step, outer_uv_step;
    switch (edge) {
      case 0:
        inner_uv = make_float2(du, dv);
        outer_uv = make_float2(0.0f, 0.0f);
        inner_uv_step = make_float2(du, 0.0f);
        outer_uv_step = make_float2(1.0f / (float)outer_T, 0.0f);
        break;
      case 1:
        inner_uv = make_float2(1.0f - du, dv);
        outer_uv = make_float2(1.0f, 0.0f);
        inner_uv_step = make_float2(0.0f, dv);
        outer_uv_step = make_float2(0.0f, 1.0f / (float)outer_T);
        break;
      case 2:
        inner_uv = make_float2(1.0f - du, 1.0f - dv);
        outer_uv = make_float2(1.0f, 1.0f);
        inner_uv_step = make_float2(-du, 0.0f);
        outer_uv_step = make_float2(-1.0f / (float)outer_T, 0.0f);
        break;
      case 3:
      default:
        inner_uv = make_float2(du, 1.0f - dv);
        outer_uv = make_float2(0.0f, 1.0f);
        inner_uv_step = make_float2(0.0f, -dv);
        outer_uv_step = make_float2(0.0f, -1.0f / (float)outer_T);
        break;
    }

    /* Stitch together two arrays of verts with triangles. At each step, we compare using
     * the next verts on both sides, to find the split direction with the smallest
     * diagonal, and use that in order to keep the triangle shape reasonable. */
    for (size_t i = 0, j = 0; i < inner_T || j < outer_T;) {
      const int v0 = sub.get_vert_along_grid_edge(edge, i);
      const int v1 = sub.get_vert_along_edge(edge, j);
      int v2;

      const float2 uv0 = sub.map_uv(inner_uv);
      const float2 uv1 = sub.map_uv(outer_uv);
      float2 uv2;

      if (j == outer_T) {
        v2 = sub.get_vert_along_grid_edge(edge, ++i);
        inner_uv += inner_uv_step;
        uv2 = sub.map_uv(inner_uv);
      }
      else if (i == inner_T) {
        v2 = sub.get_vert_along_edge(edge, ++j);
        outer_uv += outer_uv_step;
        uv2 = sub.map_uv(outer_uv);
      }
      else {
        /* Length of diagonals. */
        const int v2_a = sub.get_vert_along_edge(edge, j + 1);
        const int v2_b = sub.get_vert_along_grid_edge(edge, i + 1);

        const float len_a = len_squared(mesh_P[v0] - mesh_P[v2_a]);
        const float len_b = len_squared(mesh_P[v1] - mesh_P[v2_b]);

        /* Use smallest diagonal. */
        if (len_a < len_b) {
          v2 = v2_a;
          outer_uv += outer_uv_step;
          uv2 = sub.map_uv(outer_uv);
          j++;
        }
        else {
          v2 = v2_b;
          inner_uv += inner_uv_step;
          uv2 = sub.map_uv(inner_uv);
          i++;
        }
      }

      set_triangle(sub, triangle_index++, v0, v1, v2, uv0, uv1, uv2);
    }
  }
}

void EdgeDice::add_triangle_strip(const SubPatch &sub, const int left_edge, const int right_edge)
{
  /* Stitch triangles from side to side, edge in the other direction has T = 1. */
  const int left_T = sub.edges[left_edge].edge->T;
  const int right_T = sub.edges[right_edge].edge->T;

  float2 left_uv, right_uv, left_uv_step, right_uv_step;
  if (right_edge == 0) {
    left_uv = make_float2(0.0f, 1.0f);
    right_uv = make_float2(0.0f, 0.0f);
    left_uv_step = make_float2(1.0f / (float)left_T, 0.0f);
    right_uv_step = make_float2(1.0f / (float)right_T, 0.0f);
  }
  else {
    left_uv = make_float2(0.0f, 0.0f);
    right_uv = make_float2(1.0f, 0.0f);
    left_uv_step = make_float2(0.0f, 1.0f / (float)left_T);
    right_uv_step = make_float2(0.0f, 1.0f / (float)right_T);
  }

  /* Stitch together two arrays of verts with triangles. at each step, we compare using the next
   * verts on both sides, to find the split direction with the smallest diagonal, and use that
   * in order to keep the triangle shape reasonable. */
  int triangle_index = sub.triangles_offset;

  for (size_t i = 0, j = 0; i < left_T || j < right_T;) {
    const int v0 = sub.get_vert_along_edge_reverse(left_edge, i);
    const int v1 = sub.get_vert_along_edge(right_edge, j);
    int v2;

    const float2 uv0 = sub.map_uv(left_uv);
    const float2 uv1 = sub.map_uv(right_uv);
    float2 uv2;

    if (j == right_T) {
      v2 = sub.get_vert_along_edge_reverse(left_edge, ++i);
      left_uv += left_uv_step;
      uv2 = sub.map_uv(left_uv);
    }
    else if (i == left_T) {
      v2 = sub.get_vert_along_edge(right_edge, ++j);
      right_uv += right_uv_step;
      uv2 = sub.map_uv(right_uv);
    }
    else {
      /* Length of diagonals. */
      const int v2_a = sub.get_vert_along_edge(right_edge, j + 1);
      const int v2_b = sub.get_vert_along_edge_reverse(left_edge, i + 1);

      const float len_a = len_squared(mesh_P[v0] - mesh_P[v2_a]);
      const float len_b = len_squared(mesh_P[v1] - mesh_P[v2_b]);

      /* Use smallest diagonal. */
      if (len_a < len_b) {
        v2 = v2_a;
        right_uv += right_uv_step;
        uv2 = sub.map_uv(right_uv);
        j++;
      }
      else {
        v2 = v2_b;
        left_uv += left_uv_step;
        uv2 = sub.map_uv(left_uv);
        i++;
      }
    }

    set_triangle(sub, triangle_index++, v0, v1, v2, uv0, uv1, uv2);
  }
}

void EdgeDice::quad_set_sides(const SubPatch &sub)
{
  for (int edge = 0; edge < 4; edge++) {
    const int t = sub.edges[edge].edge->T;
    const int i_start = (sub.edges[edge].own_vertex) ? 0 : 1;
    const int i_end = (sub.edges[edge].own_edge) ? t : 1;

    /* set verts on the edge of the patch */
    for (int i = i_start; i < i_end; i++) {
      const float f = i / (float)t;

      float2 uv;
      switch (edge) {
        case 0:
          uv = make_float2(f, 0.0f);
          break;
        case 1:
          uv = make_float2(1.0f, f);
          break;
        case 2:
          uv = make_float2(1.0f - f, 1.0f);
          break;
        case 3:
        default:
          uv = make_float2(0.0f, 1.0f - f);
          break;
      }

      const int vert_index = sub.get_vert_along_edge(edge, i);
      set_vertex(sub, vert_index, sub.map_uv(uv));
    }
  }
}

void EdgeDice::quad_dice(const SubPatch &sub)
{
  /* Compute inner grid size with scale factor. */
  const int Mu = max(sub.edges[0].edge->T, sub.edges[2].edge->T);
  const int Mv = max(sub.edges[3].edge->T, sub.edges[1].edge->T);

  if (Mv == 1) {
    /* No inner grid, stitch triangles from side to side. */
    add_triangle_strip(sub, 2, 0);
  }
  else if (Mu == 1) {
    /* No inner grid, stitch triangles from side to side. */
    add_triangle_strip(sub, 3, 1);
  }
  else {
#if 0
    /* Doesn't work very well, especially at grazing angles. */
    const float S = scale_factor(sub, ef, Mu, Mv);
    const int grid_Mu = max((int)ceilf(S * Mu), 1);  // XXX handle 0 & 1?
    const int grid_Mv = max((int)ceilf(S * Mv), 1);  // XXX handle 0 & 1?
    add_grid_triangles_and_stitch(sub, grid_Mu, grid_Mv);
#else
    add_grid_triangles_and_stitch(sub, Mu, Mv);
#endif
  }
}

void EdgeDice::tri_set_sides(const SubPatch &sub)
{
  for (int edge = 0; edge < 3; edge++) {
    const int t = sub.edges[edge].edge->T;
    const int i_start = (sub.edges[edge].own_vertex) ? 0 : 1;
    const int i_end = (sub.edges[edge].own_edge) ? t : 1;

    /* set verts on the edge of the patch */
    for (int i = i_start; i < i_end; i++) {
      const float f = i / (float)t;

      float2 uv;
      switch (edge) {
        case 0:
          uv = make_float2(f, 0.0f);
          break;
        case 1:
          uv = make_float2(1.0f - f, f);
          break;
        case 2:
        default:
          uv = make_float2(0.0f, 1.0f - f);
          break;
      }

      const int vert_index = sub.get_vert_along_edge(edge, i);
      set_vertex(sub, vert_index, sub.map_uv(uv));
    }
  }
}

void EdgeDice::tri_dice(const SubPatch &sub)
{
  const int M = max(max(sub.edges[0].edge->T, sub.edges[1].edge->T), sub.edges[2].edge->T);
  const float d = 1.0f / (float)(M + 1);

  int triangle_index = sub.triangles_offset;

  if (M == 1) {
    /* Single triangle. */
    set_triangle(sub,
                 triangle_index++,
                 sub.edges[0].start_vert_index(),
                 sub.edges[1].start_vert_index(),
                 sub.edges[2].start_vert_index(),
                 sub.map_uv(make_float2(0.0f, 0.0f)),
                 sub.map_uv(make_float2(1.0f, 0.0f)),
                 sub.map_uv(make_float2(0.0f, 1.0f)));
    assert(triangle_index == sub.triangles_offset + sub.calc_num_triangles());
    return;
  }
  if (M == 2) {
    /* Edges have 2 segments or less. */
    int num_split = 0;
    int split_0 = -1;
    for (int i = 0; i < 3; i++) {
      if (sub.edges[i].edge->T == 2) {
        num_split++;
        if (split_0 == -1) {
          split_0 = i;
        }
      }
    }
    /* When two edges have 2 segments, we assume split_0 is the first of two consecutive edges. */
    if (split_0 == 0 && sub.edges[2].edge->T == 2) {
      split_0 = 2;
    }

    const int split_1 = (split_0 + 1) % 3;
    const int split_2 = (split_0 + 2) % 3;

    const int v[3] = {sub.edges[0].start_vert_index(),
                      sub.edges[1].start_vert_index(),
                      sub.edges[2].start_vert_index()};
    const int mid_v[3] = {sub.get_vert_along_edge(0, 1),
                          sub.get_vert_along_edge(1, 1),
                          sub.get_vert_along_edge(2, 1)};
    const float2 uv[3] = {sub.map_uv(make_float2(0.0f, 0.0f)),
                          sub.map_uv(make_float2(1.0f, 0.0f)),
                          sub.map_uv(make_float2(0.0f, 1.0f))};
    const float2 mid_uv[3] = {sub.map_uv(make_float2(0.5f, 0.0f)),
                              sub.map_uv(make_float2(0.5f, 0.5f)),
                              sub.map_uv(make_float2(0.0f, 0.5f))};

    if (num_split == 3) {
      /* All edges have two segments
       *    /\
       *   /--\
       *  / \/ \
       *  ------- */
      set_triangle(sub, triangle_index++, v[0], mid_v[0], mid_v[2], uv[0], mid_uv[0], mid_uv[2]);
      set_triangle(sub, triangle_index++, v[1], mid_v[1], mid_v[0], uv[1], mid_uv[1], mid_uv[0]);
      set_triangle(sub, triangle_index++, v[2], mid_v[2], mid_v[1], uv[2], mid_uv[2], mid_uv[1]);
      set_triangle(
          sub, triangle_index++, mid_v[0], mid_v[1], mid_v[2], mid_uv[0], mid_uv[1], mid_uv[2]);
    }
    else {
      /* One edge has two segments.
       *    / \
       *   / | \
       *  /  |  \
       *  ------- */
      set_triangle(sub,
                   triangle_index++,
                   v[split_0],
                   mid_v[split_0],
                   v[split_2],
                   uv[split_0],
                   mid_uv[split_0],
                   uv[split_2]);
      if (num_split == 1) {
        set_triangle(sub,
                     triangle_index++,
                     mid_v[split_0],
                     v[split_1],
                     v[split_2],
                     mid_uv[split_0],
                     uv[split_1],
                     uv[split_2]);
      }
      else {
        /* Two edges have two segments.
         *    /|\
         *   / | \
         *  /  |/ \
         *  ------- */
        set_triangle(sub,
                     triangle_index++,
                     mid_v[split_0],
                     v[split_1],
                     mid_v[split_1],
                     mid_uv[split_0],
                     uv[split_1],
                     mid_uv[split_1]);
        set_triangle(sub,
                     triangle_index++,
                     mid_v[split_0],
                     mid_v[split_1],
                     v[split_2],
                     mid_uv[split_0],
                     mid_uv[split_1],
                     uv[split_2]);
      }
    }
    assert(triangle_index == sub.triangles_offset + sub.calc_num_triangles());
    return;
  }

  const int inner_M = M - 2;

  for (int j = 0; j < inner_M; j++) {
    for (int i = 0; i < j + 1; i++) {
      const int i_next = i + 1;
      const int j_next = j + 1;

      const float2 inner_uv = make_float2(d, d);

      const int v0 = sub.get_inner_grid_vert_triangle(i, j);
      const int v1 = sub.get_inner_grid_vert_triangle(i_next, j_next);
      const int v2 = sub.get_inner_grid_vert_triangle(i, j_next);

      const float2 uv0 = sub.map_uv(inner_uv + make_float2(i, j - i) * d);
      const float2 uv1 = sub.map_uv(inner_uv + make_float2(i_next, j - i) * d);
      const float2 uv2 = sub.map_uv(inner_uv + make_float2(i, j_next - i) * d);

      set_vertex(sub, v0, uv0);
      if (j == inner_M - 1) {
        set_vertex(sub, v1, uv1);
        set_vertex(sub, v2, uv2);
      }

      set_triangle(sub, triangle_index++, v0, v1, v2, uv0, uv1, uv2);

      if (i < j) {
        const int v3 = sub.get_inner_grid_vert_triangle(i_next, j);
        const float2 uv3 = sub.map_uv(inner_uv + make_float2(i_next, j - i_next) * d);

        set_vertex(sub, v3, uv3);

        set_triangle(sub, triangle_index++, v0, v3, v1, uv0, uv3, uv1);
      }
    }
  }

  assert(triangle_index == sub.triangles_offset + inner_M * inner_M);

  /* Stitch inner grid to edges. */
  for (int edge = 0; edge < 3; edge++) {
    const int outer_T = sub.edges[edge].edge->T;
    const int inner_T = inner_M;

    float2 inner_uv, outer_uv, inner_uv_step, outer_uv_step;
    switch (edge) {
      case 0:
        inner_uv = make_float2(d, d);
        outer_uv = make_float2(0.0f, 0.0f);
        inner_uv_step = make_float2(d, 0.0f);
        outer_uv_step = make_float2(1.0f / (float)outer_T, 0.0f);
        break;
      case 1:
        inner_uv = make_float2(1.0f - 2.0f * d, d);
        outer_uv = make_float2(1.0f, 0.0f);
        inner_uv_step = make_float2(-d, d);
        outer_uv_step = make_float2(-1.0f / (float)outer_T, 1.0f / (float)outer_T);
        break;
      case 2:
      default:
        inner_uv = make_float2(d, 1.0f - 2.0f * d);
        outer_uv = make_float2(0.0f, 1.0f);
        inner_uv_step = make_float2(0.0f, -d);
        outer_uv_step = make_float2(0.0f, -1.0f / (float)outer_T);
        break;
    }

    /* Stitch together two arrays of verts with triangles. At each step, we compare using
     * the next verts on both sides, to find the split direction with the smallest
     * diagonal, and use that in order to keep the triangle shape reasonable. */
    for (size_t i = 0, j = 0; i < inner_T || j < outer_T;) {
      const int v0 = sub.get_vert_along_grid_edge(edge, i);
      const int v1 = sub.get_vert_along_edge(edge, j);
      int v2;

      const float2 uv0 = sub.map_uv(inner_uv);
      const float2 uv1 = sub.map_uv(outer_uv);
      float2 uv2;

      if (j == outer_T) {
        v2 = sub.get_vert_along_grid_edge(edge, ++i);
        inner_uv += inner_uv_step;
        uv2 = sub.map_uv(inner_uv);
      }
      else if (i == inner_T) {
        v2 = sub.get_vert_along_edge(edge, ++j);
        outer_uv += outer_uv_step;
        uv2 = sub.map_uv(outer_uv);
      }
      else {
        /* Length of diagonals. */
        const int v2_a = sub.get_vert_along_edge(edge, j + 1);
        const int v2_b = sub.get_vert_along_grid_edge(edge, i + 1);

        const float len_a = len_squared(mesh_P[v0] - mesh_P[v2_a]);
        const float len_b = len_squared(mesh_P[v1] - mesh_P[v2_b]);

        /* Use smallest diagonal. */
        if (len_a < len_b) {
          v2 = v2_a;
          outer_uv += outer_uv_step;
          uv2 = sub.map_uv(outer_uv);
          j++;
        }
        else {
          v2 = v2_b;
          inner_uv += inner_uv_step;
          uv2 = sub.map_uv(inner_uv);
          i++;
        }
      }

      set_triangle(sub, triangle_index++, v0, v1, v2, uv0, uv1, uv2);
    }
  }

  assert(triangle_index == sub.triangles_offset + sub.calc_num_triangles());
}

void EdgeDice::dice(const DiagSplit &split)
{
  const size_t num_subpatches = split.get_num_subpatches();

  /* Vertex coordinates for sides. Needs to be done first because tessellation depends
   * on these coordinates and they are unique assigned to a subpatch for determinism. */
  parallel_for(blocked_range<size_t>(0, num_subpatches, 8), [&](const blocked_range<size_t> &r) {
    for (size_t i = r.begin(); i != r.end(); i++) {
      const SubPatch &subpatch = split.get_subpatch(i);
      if (subpatch.shape == SubPatch::TRIANGLE) {
        tri_set_sides(subpatch);
      }
      else {
        quad_set_sides(subpatch);
      }
    }
  });

  /* Inner vertex coordinates and triangles. */
  parallel_for(blocked_range<size_t>(0, num_subpatches, 8), [&](const blocked_range<size_t> &r) {
    for (size_t i = r.begin(); i != r.end(); i++) {
      const SubPatch &subpatch = split.get_subpatch(i);
      if (subpatch.shape == SubPatch::TRIANGLE) {
        tri_dice(subpatch);
      }
      else {
        quad_dice(subpatch);
      }
    }
  });
}

CCL_NAMESPACE_END
