/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/camera.h"
#include "scene/mesh.h"

#include "subd/dice.h"
#include "subd/patch.h"

CCL_NAMESPACE_BEGIN

/* EdgeDice Base */

EdgeDice::EdgeDice(const SubdParams &params_) : params(params_)
{
  params.mesh->attributes.add(ATTR_STD_VERTEX_NORMAL);

  if (params.ptex) {
    params.mesh->attributes.add(ATTR_STD_PTEX_FACE_ID);
    params.mesh->attributes.add(ATTR_STD_PTEX_UV);
  }
}

void EdgeDice::reserve(const int num_verts, const int num_triangles)
{
  Mesh *mesh = params.mesh;

  mesh->num_subd_added_verts = num_verts;

  mesh->resize_mesh(mesh->get_verts().size() + num_verts, mesh->num_triangles());
  mesh->reserve_mesh(mesh->get_verts().size() + num_verts, mesh->num_triangles() + num_triangles);

  Attribute *attr_vN = mesh->attributes.add(ATTR_STD_VERTEX_NORMAL);

  mesh_P = mesh->verts.data();
  mesh_N = attr_vN->data_float3();

  Attribute *attr_ptex_face_id = mesh->attributes.find(ATTR_STD_PTEX_FACE_ID);
  if (attr_ptex_face_id) {
    mesh_ptex_face_id = attr_ptex_face_id->data_float();
  }
  Attribute *attr_ptex_uv = mesh->attributes.find(ATTR_STD_PTEX_UV);
  if (attr_ptex_uv) {
    mesh_ptex_uv = attr_ptex_uv->data_float2();
  }
}

void EdgeDice::set_vert(const Patch *patch, const int index, const float2 uv)
{
  float3 P;
  float3 N;

  patch->eval(&P, nullptr, nullptr, &N, uv.x, uv.y);

  assert(index < params.mesh->verts.size());

  mesh_P[index] = P;
  mesh_N[index] = N;
}

void EdgeDice::add_triangle(const Patch *patch,
                            const int v0,
                            const int v1,
                            const int v2,
                            const float2 uv0,
                            const float2 uv1,
                            const float2 uv2)
{
  Mesh *mesh = params.mesh;

  mesh->add_triangle(v0, v1, v2, patch->shader, true);

  const int triangle_offset = params.mesh->num_triangles() - 1;

  params.mesh->subd_triangle_patch_index[triangle_offset] = patch->patch_index;
  if (mesh_ptex_face_id) {
    mesh_ptex_face_id[triangle_offset] = patch->patch_index;
  }

  params.mesh->subd_corner_patch_uv[(triangle_offset * 3) + 0] = uv0;
  params.mesh->subd_corner_patch_uv[(triangle_offset * 3) + 1] = uv1;
  params.mesh->subd_corner_patch_uv[(triangle_offset * 3) + 2] = uv2;

  if (mesh_ptex_uv) {
    mesh_ptex_uv[(triangle_offset * 3) + 0] = uv0;
    mesh_ptex_uv[(triangle_offset * 3) + 0] = uv1;
    mesh_ptex_uv[(triangle_offset * 3) + 0] = uv2;
  }
}

void EdgeDice::stitch_triangles(SubPatch &sub, const int edge)
{
  int Mu = max(sub.edge_u0.T, sub.edge_u1.T);
  int Mv = max(sub.edge_v0.T, sub.edge_v1.T);
  Mu = max(Mu, 2);
  Mv = max(Mv, 2);

  const int outer_T = sub.edges[edge].T;
  const int inner_T = ((edge % 2) == 0) ? Mv - 2 : Mu - 2;

  if (inner_T < 0 || outer_T < 0) {
    return;  // XXX avoid crashes for Mu or Mv == 1, missing polygons
  }

  const float du = 1.0f / (float)Mu;
  const float dv = 1.0f / (float)Mv;
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

  /* Stitch together two arrays of verts with triangles. at each step, we compare using the next
   * verts on both sides, to find the split direction with the smallest diagonal, and use that
   * in order to keep the triangle shape reasonable. */
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
      const float len1 = len_squared(mesh_P[sub.get_vert_along_grid_edge(edge, i)] -
                                     mesh_P[sub.get_vert_along_edge(edge, j + 1)]);
      const float len2 = len_squared(mesh_P[sub.get_vert_along_edge(edge, j)] -
                                     mesh_P[sub.get_vert_along_grid_edge(edge, i + 1)]);

      /* Use smallest diagonal. */
      if (len1 < len2) {
        v2 = sub.get_vert_along_edge(edge, ++j);
        outer_uv += outer_uv_step;
        uv2 = sub.map_uv(outer_uv);
      }
      else {
        v2 = sub.get_vert_along_grid_edge(edge, ++i);
        inner_uv += inner_uv_step;
        uv2 = sub.map_uv(inner_uv);
      }
    }

    add_triangle(sub.patch, v1, v0, v2, uv1, uv0, uv2);
  }
}

/* QuadDice */

QuadDice::QuadDice(const SubdParams &params_) : EdgeDice(params_) {}

float3 QuadDice::eval_projected(SubPatch &sub, const float2 uv)
{
  float3 P;

  sub.patch->eval(&P, nullptr, nullptr, nullptr, uv.x, uv.y);
  if (params.camera) {
    P = transform_perspective(&params.camera->worldtoraster, P);
  }

  return P;
}

void QuadDice::set_vert(SubPatch &sub, const int index, const float2 uv)
{
  EdgeDice::set_vert(sub.patch, index, sub.map_uv(uv));
}

void QuadDice::set_side(SubPatch &sub, const int edge)
{
  const int t = sub.edges[edge].T;

  /* set verts on the edge of the patch */
  for (int i = 0; i < t; i++) {
    const float f = i / (float)t;

    float2 uv;
    switch (edge) {
      case 0:
        uv = make_float2(0.0f, f);
        break;
      case 1:
        uv = make_float2(f, 1.0f);
        break;
      case 2:
        uv = make_float2(1.0f, 1.0f - f);
        break;
      case 3:
      default:
        uv = make_float2(1.0f - f, 0.0f);
        break;
    }

    set_vert(sub, sub.get_vert_along_edge(edge, i), uv);
  }
}

float QuadDice::quad_area(const float3 &a, const float3 &b, const float3 &c, const float3 &d)
{
  return triangle_area(a, b, d) + triangle_area(a, d, c);
}

float QuadDice::scale_factor(SubPatch &sub, const int Mu, const int Mv)
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
  const float N = 0.5f * (Ntris - (sub.edge_u0.T + sub.edge_u1.T + sub.edge_v0.T + sub.edge_v1.T));
  const float D = 4.0f * N * Mu * Mv + (Mu + Mv) * (Mu + Mv);
  const float S = (Mu + Mv + sqrtf(max(D, 0.0f))) / (2 * Mu * Mv);

  return S;
}

void QuadDice::add_grid(SubPatch &sub, const int Mu, const int Mv, const int offset)
{
  /* create inner grid */
  const float du = 1.0f / (float)Mu;
  const float dv = 1.0f / (float)Mv;

  for (int j = 1; j < Mv; j++) {
    for (int i = 1; i < Mu; i++) {
      const float u = i * du;
      const float v = j * dv;
      const int center_i = offset + (i - 1) + (j - 1) * (Mu - 1);

      set_vert(sub, center_i, make_float2(u, v));

      if (i < Mu - 1 && j < Mv - 1) {
        const int i1 = offset + (i - 1) + (j - 1) * (Mu - 1);
        const int i2 = offset + i + (j - 1) * (Mu - 1);
        const int i3 = offset + i + j * (Mu - 1);
        const int i4 = offset + (i - 1) + j * (Mu - 1);

        const float2 uv1 = sub.map_uv(make_float2(u, v));
        const float2 uv2 = sub.map_uv(make_float2(u + du, v));
        const float2 uv3 = sub.map_uv(make_float2(u + du, v + dv));
        const float2 uv4 = sub.map_uv(make_float2(u, v + dv));

        add_triangle(sub.patch, i1, i2, i3, uv1, uv2, uv3);
        add_triangle(sub.patch, i1, i3, i4, uv1, uv3, uv4);
      }
    }
  }
}

void QuadDice::dice(SubPatch &sub)
{
  /* compute inner grid size with scale factor */
  int Mu = max(sub.edge_u0.T, sub.edge_u1.T);
  int Mv = max(sub.edge_v0.T, sub.edge_v1.T);

#if 0 /* Doesn't work very well, especially at grazing angles. */
  const float S = scale_factor(sub, ef, Mu, Mv);
#else
  const float S = 1.0f;
#endif

  Mu = max((int)ceilf(S * Mu), 2);  // XXX handle 0 & 1?
  Mv = max((int)ceilf(S * Mv), 2);  // XXX handle 0 & 1?

  /* inner grid */
  add_grid(sub, Mu, Mv, sub.inner_grid_vert_offset);

  /* sides */
  set_side(sub, 0);
  set_side(sub, 1);
  set_side(sub, 2);
  set_side(sub, 3);

  stitch_triangles(sub, 0);
  stitch_triangles(sub, 1);
  stitch_triangles(sub, 2);
  stitch_triangles(sub, 3);
}

CCL_NAMESPACE_END
