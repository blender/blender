/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "scene/camera.h"
#include "scene/mesh.h"

#include "subd/dice.h"
#include "subd/patch.h"

CCL_NAMESPACE_BEGIN

/* EdgeDice Base */

EdgeDice::EdgeDice(const SubdParams &params_) : params(params_)
{
  mesh_P = NULL;
  mesh_N = NULL;
  vert_offset = 0;

  params.mesh->attributes.add(ATTR_STD_VERTEX_NORMAL);

  if (params.ptex) {
    params.mesh->attributes.add(ATTR_STD_PTEX_UV);
    params.mesh->attributes.add(ATTR_STD_PTEX_FACE_ID);
  }
}

void EdgeDice::reserve(int num_verts, int num_triangles)
{
  Mesh *mesh = params.mesh;

  vert_offset = mesh->get_verts().size();
  tri_offset = mesh->num_triangles();

  mesh->resize_mesh(mesh->get_verts().size() + num_verts, mesh->num_triangles());
  mesh->reserve_mesh(mesh->get_verts().size() + num_verts, mesh->num_triangles() + num_triangles);

  Attribute *attr_vN = mesh->attributes.add(ATTR_STD_VERTEX_NORMAL);

  mesh_P = mesh->verts.data() + vert_offset;
  mesh_N = attr_vN->data_float3() + vert_offset;

  params.mesh->num_subd_verts += num_verts;
}

void EdgeDice::set_vert(Patch *patch, int index, float2 uv)
{
  float3 P, N;

  patch->eval(&P, NULL, NULL, &N, uv.x, uv.y);

  assert(index < params.mesh->verts.size());

  mesh_P[index] = P;
  mesh_N[index] = N;
  params.mesh->vert_patch_uv[index + vert_offset] = make_float2(uv.x, uv.y);
}

void EdgeDice::add_triangle(Patch *patch, int v0, int v1, int v2)
{
  Mesh *mesh = params.mesh;

  mesh->add_triangle(v0 + vert_offset, v1 + vert_offset, v2 + vert_offset, patch->shader, true);
  params.mesh->triangle_patch[params.mesh->num_triangles() - 1] = patch->patch_index;

  tri_offset++;
}

void EdgeDice::stitch_triangles(Subpatch &sub, int edge)
{
  int Mu = max(sub.edge_u0.T, sub.edge_u1.T);
  int Mv = max(sub.edge_v0.T, sub.edge_v1.T);
  Mu = max(Mu, 2);
  Mv = max(Mv, 2);

  int outer_T = sub.edges[edge].T;
  int inner_T = ((edge % 2) == 0) ? Mv - 2 : Mu - 2;

  if (inner_T < 0 || outer_T < 0)
    return;  // XXX avoid crashes for Mu or Mv == 1, missing polygons

  /* stitch together two arrays of verts with triangles. at each step,
   * we compare using the next verts on both sides, to find the split
   * direction with the smallest diagonal, and use that in order to keep
   * the triangle shape reasonable. */
  for (size_t i = 0, j = 0; i < inner_T || j < outer_T;) {
    int v0, v1, v2;

    v0 = sub.get_vert_along_grid_edge(edge, i);
    v1 = sub.get_vert_along_edge(edge, j);

    if (j == outer_T) {
      v2 = sub.get_vert_along_grid_edge(edge, ++i);
    }
    else if (i == inner_T) {
      v2 = sub.get_vert_along_edge(edge, ++j);
    }
    else {
      /* length of diagonals */
      float len1 = len_squared(mesh_P[sub.get_vert_along_grid_edge(edge, i)] -
                               mesh_P[sub.get_vert_along_edge(edge, j + 1)]);
      float len2 = len_squared(mesh_P[sub.get_vert_along_edge(edge, j)] -
                               mesh_P[sub.get_vert_along_grid_edge(edge, i + 1)]);

      /* use smallest diagonal */
      if (len1 < len2)
        v2 = sub.get_vert_along_edge(edge, ++j);
      else
        v2 = sub.get_vert_along_grid_edge(edge, ++i);
    }

    add_triangle(sub.patch, v1, v0, v2);
  }
}

/* QuadDice */

QuadDice::QuadDice(const SubdParams &params_) : EdgeDice(params_) {}

float2 QuadDice::map_uv(Subpatch &sub, float u, float v)
{
  /* map UV from subpatch to patch parametric coordinates */
  float2 d0 = interp(sub.c00, sub.c01, v);
  float2 d1 = interp(sub.c10, sub.c11, v);
  return interp(d0, d1, u);
}

float3 QuadDice::eval_projected(Subpatch &sub, float u, float v)
{
  float2 uv = map_uv(sub, u, v);
  float3 P;

  sub.patch->eval(&P, NULL, NULL, NULL, uv.x, uv.y);
  if (params.camera)
    P = transform_perspective(&params.camera->worldtoraster, P);

  return P;
}

void QuadDice::set_vert(Subpatch &sub, int index, float u, float v)
{
  EdgeDice::set_vert(sub.patch, index, map_uv(sub, u, v));
}

void QuadDice::set_side(Subpatch &sub, int edge)
{
  int t = sub.edges[edge].T;

  /* set verts on the edge of the patch */
  for (int i = 0; i < t; i++) {
    float f = i / (float)t;

    float u, v;
    switch (edge) {
      case 0:
        u = 0;
        v = f;
        break;
      case 1:
        u = f;
        v = 1;
        break;
      case 2:
        u = 1;
        v = 1.0f - f;
        break;
      case 3:
      default:
        u = 1.0f - f;
        v = 0;
        break;
    }

    set_vert(sub, sub.get_vert_along_edge(edge, i), u, v);
  }
}

float QuadDice::quad_area(const float3 &a, const float3 &b, const float3 &c, const float3 &d)
{
  return triangle_area(a, b, d) + triangle_area(a, d, c);
}

float QuadDice::scale_factor(Subpatch &sub, int Mu, int Mv)
{
  /* estimate area as 4x largest of 4 quads */
  float3 P[3][3];

  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      P[i][j] = eval_projected(sub, i * 0.5f, j * 0.5f);

  float A1 = quad_area(P[0][0], P[1][0], P[0][1], P[1][1]);
  float A2 = quad_area(P[1][0], P[2][0], P[1][1], P[2][1]);
  float A3 = quad_area(P[0][1], P[1][1], P[0][2], P[1][2]);
  float A4 = quad_area(P[1][1], P[2][1], P[1][2], P[2][2]);
  float Apatch = max(A1, max(A2, max(A3, A4))) * 4.0f;

  /* solve for scaling factor */
  float Atri = params.dicing_rate * params.dicing_rate * 0.5f;
  float Ntris = Apatch / Atri;

  // XXX does the -sqrt solution matter
  // XXX max(D, 0.0) is highly suspicious, need to test cases
  // where D goes negative
  float N = 0.5f * (Ntris - (sub.edge_u0.T + sub.edge_u1.T + sub.edge_v0.T + sub.edge_v1.T));
  float D = 4.0f * N * Mu * Mv + (Mu + Mv) * (Mu + Mv);
  float S = (Mu + Mv + sqrtf(max(D, 0.0f))) / (2 * Mu * Mv);

  return S;
}

void QuadDice::add_grid(Subpatch &sub, int Mu, int Mv, int offset)
{
  /* create inner grid */
  float du = 1.0f / (float)Mu;
  float dv = 1.0f / (float)Mv;

  for (int j = 1; j < Mv; j++) {
    for (int i = 1; i < Mu; i++) {
      float u = i * du;
      float v = j * dv;

      set_vert(sub, offset + (i - 1) + (j - 1) * (Mu - 1), u, v);

      if (i < Mu - 1 && j < Mv - 1) {
        int i1 = offset + (i - 1) + (j - 1) * (Mu - 1);
        int i2 = offset + i + (j - 1) * (Mu - 1);
        int i3 = offset + i + j * (Mu - 1);
        int i4 = offset + (i - 1) + j * (Mu - 1);

        add_triangle(sub.patch, i1, i2, i3);
        add_triangle(sub.patch, i1, i3, i4);
      }
    }
  }
}

void QuadDice::dice(Subpatch &sub)
{
  /* compute inner grid size with scale factor */
  int Mu = max(sub.edge_u0.T, sub.edge_u1.T);
  int Mv = max(sub.edge_v0.T, sub.edge_v1.T);

#if 0 /* Doesn't work very well, especially at grazing angles. */
  float S = scale_factor(sub, ef, Mu, Mv);
#else
  float S = 1.0f;
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
