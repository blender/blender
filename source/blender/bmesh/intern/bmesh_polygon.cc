/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * This file contains code for dealing
 * with polygons (normal/area calculation, tessellation, etc)
 */

#include "DNA_modifier_types.h"

#include "BLI_alloca.h"
#include "BLI_linklist.h"
#include "BLI_math_base.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_polyfill_2d_beautify.h"

#include "bmesh.hh"
#include "bmesh_tools.hh"

#include "BKE_customdata.hh"

#include "intern/bmesh_private.hh"

using blender::float3;
using blender::Span;

/**
 * \brief COMPUTE POLY NORMAL (BMFace)
 *
 * Same as #normal_poly_v3 but operates directly on a bmesh face.
 */
static float bm_face_calc_poly_normal(const BMFace *f, float n[3])
{
  BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
  BMLoop *l_iter = l_first;
  const float *v_prev = l_first->prev->v->co;
  const float *v_curr = l_first->v->co;

  zero_v3(n);

  /* Newell's Method */
  do {
    add_newell_cross_v3_v3v3(n, v_prev, v_curr);

    l_iter = l_iter->next;
    v_prev = v_curr;
    v_curr = l_iter->v->co;

  } while (l_iter != l_first);

  return normalize_v3(n);
}

/**
 * \brief COMPUTE POLY NORMAL (BMFace)
 *
 * Same as #bm_face_calc_poly_normal
 * but takes an array of vertex locations.
 */
static float bm_face_calc_poly_normal_vertex_cos(const BMFace *f,
                                                 float r_no[3],
                                                 const Span<float3> vertexCos)
{
  BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
  BMLoop *l_iter = l_first;
  const float *v_prev = vertexCos[BM_elem_index_get(l_first->prev->v)];
  const float *v_curr = vertexCos[BM_elem_index_get(l_first->v)];

  zero_v3(r_no);

  /* Newell's Method */
  do {
    add_newell_cross_v3_v3v3(r_no, v_prev, v_curr);

    l_iter = l_iter->next;
    v_prev = v_curr;
    v_curr = vertexCos[BM_elem_index_get(l_iter->v)];
  } while (l_iter != l_first);

  return normalize_v3(r_no);
}

/**
 * \brief COMPUTE POLY CENTER (BMFace)
 */
static void bm_face_calc_poly_center_median_vertex_cos(
    const BMFace *f, float r_cent[3], const blender::Span<blender::float3> vert_positions)
{
  const BMLoop *l_first, *l_iter;

  zero_v3(r_cent);

  /* Newell's Method */
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    add_v3_v3(r_cent, vert_positions[BM_elem_index_get(l_iter->v)]);
  } while ((l_iter = l_iter->next) != l_first);
  mul_v3_fl(r_cent, 1.0f / f->len);
}

void BM_face_calc_tessellation(const BMFace *f,
                               const bool use_fixed_quad,
                               BMLoop **r_loops,
                               uint (*r_index)[3])
{
  BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
  BMLoop *l_iter;

  if (f->len == 3) {
    *r_loops++ = (l_iter = l_first);
    *r_loops++ = (l_iter = l_iter->next);
    *r_loops++ = (l_iter->next);

    r_index[0][0] = 0;
    r_index[0][1] = 1;
    r_index[0][2] = 2;
  }
  else if (f->len == 4 && use_fixed_quad) {
    *r_loops++ = (l_iter = l_first);
    *r_loops++ = (l_iter = l_iter->next);
    *r_loops++ = (l_iter = l_iter->next);
    *r_loops++ = (l_iter->next);

    r_index[0][0] = 0;
    r_index[0][1] = 1;
    r_index[0][2] = 2;

    r_index[1][0] = 0;
    r_index[1][1] = 2;
    r_index[1][2] = 3;
  }
  else {
    float axis_mat[3][3];
    float (*projverts)[2] = BLI_array_alloca(projverts, f->len);
    int j;

    axis_dominant_v3_to_m3_negate(axis_mat, f->no);

    j = 0;
    l_iter = l_first;
    do {
      mul_v2_m3v3(projverts[j], axis_mat, l_iter->v->co);
      r_loops[j] = l_iter;
      j++;
    } while ((l_iter = l_iter->next) != l_first);

    /* complete the loop */
    BLI_polyfill_calc(projverts, f->len, 1, r_index);
  }
}

void BM_face_calc_point_in_face(const BMFace *f, float r_co[3])
{
  const BMLoop *ltri[3];

  if (f->len == 3) {
    const BMLoop *l = BM_FACE_FIRST_LOOP(f);
    ARRAY_SET_ITEMS(ltri, l, l->next, l->prev);
  }
  else {
    /* tessellation here seems overkill when in many cases this will be the center,
     * but without this we can't be sure the point is inside a concave face. */
    const int tottri = f->len - 2;
    BMLoop **loops = BLI_array_alloca(loops, f->len);
    uint(*index)[3] = BLI_array_alloca(index, tottri);
    int j;
    int j_best = 0; /* use as fallback when unset */
    float area_best = -1.0f;

    BM_face_calc_tessellation(f, false, loops, index);

    for (j = 0; j < tottri; j++) {
      const float *p1 = loops[index[j][0]]->v->co;
      const float *p2 = loops[index[j][1]]->v->co;
      const float *p3 = loops[index[j][2]]->v->co;
      const float area = area_squared_tri_v3(p1, p2, p3);
      if (area > area_best) {
        j_best = j;
        area_best = area;
      }
    }

    ARRAY_SET_ITEMS(
        ltri, loops[index[j_best][0]], loops[index[j_best][1]], loops[index[j_best][2]]);
  }

  mid_v3_v3v3v3(r_co, ltri[0]->v->co, ltri[1]->v->co, ltri[2]->v->co);
}

float BM_face_calc_area(const BMFace *f)
{
  /* inline 'area_poly_v3' logic, avoid creating a temp array */
  const BMLoop *l_iter, *l_first;
  float n[3];

  zero_v3(n);
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    add_newell_cross_v3_v3v3(n, l_iter->v->co, l_iter->next->v->co);
  } while ((l_iter = l_iter->next) != l_first);
  return len_v3(n) * 0.5f;
}

float BM_face_calc_area_with_mat3(const BMFace *f, const float mat3[3][3])
{
  /* inline 'area_poly_v3' logic, avoid creating a temp array */
  const BMLoop *l_iter, *l_first;
  float co[3];
  float n[3];

  zero_v3(n);
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  mul_v3_m3v3(co, mat3, l_iter->v->co);
  do {
    float co_next[3];
    mul_v3_m3v3(co_next, mat3, l_iter->next->v->co);
    add_newell_cross_v3_v3v3(n, co, co_next);
    copy_v3_v3(co, co_next);
  } while ((l_iter = l_iter->next) != l_first);
  return len_v3(n) * 0.5f;
}

float BM_face_calc_area_uv_signed(const BMFace *f, int cd_loop_uv_offset)
{
  /* inline 'area_poly_v2' logic, avoid creating a temp array */
  const BMLoop *l_iter, *l_first;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  /* Green's theorem applied to area of a polygon.
   * TODO: `cross` should be of type `double` to reduce rounding error. */
  float cross = 0.0f;
  do {
    const float *luv = BM_ELEM_CD_GET_FLOAT_P(l_iter, cd_loop_uv_offset);
    const float *luv_next = BM_ELEM_CD_GET_FLOAT_P(l_iter->next, cd_loop_uv_offset);
    cross += (luv_next[0] - luv[0]) * (luv_next[1] + luv[1]);
  } while ((l_iter = l_iter->next) != l_first);
  return cross * 0.5f;
}

float BM_face_calc_area_uv(const BMFace *f, int cd_loop_uv_offset)
{
  return fabsf(BM_face_calc_area_uv_signed(f, cd_loop_uv_offset));
}

float BM_face_calc_perimeter(const BMFace *f)
{
  const BMLoop *l_iter, *l_first;
  float perimeter = 0.0f;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    perimeter += len_v3v3(l_iter->v->co, l_iter->next->v->co);
  } while ((l_iter = l_iter->next) != l_first);

  return perimeter;
}

float BM_face_calc_perimeter_with_mat3(const BMFace *f, const float mat3[3][3])
{
  const BMLoop *l_iter, *l_first;
  float co[3];
  float perimeter = 0.0f;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  mul_v3_m3v3(co, mat3, l_iter->v->co);
  do {
    float co_next[3];
    mul_v3_m3v3(co_next, mat3, l_iter->next->v->co);
    perimeter += len_v3v3(co, co_next);
    copy_v3_v3(co, co_next);
  } while ((l_iter = l_iter->next) != l_first);

  return perimeter;
}

/**
 * Utility function to calculate the edge which is most different from the other two.
 *
 * \return The first edge index, where the second vertex is `(index + 1) % 3`.
 */
static int bm_vert_tri_find_unique_edge(BMVert *verts[3])
{
/* find the most 'unique' loop, (greatest difference to others) */
#if 1
  /* Optimized version that avoids `sqrt`. */
  float difs[3];
  for (int i_prev = 1, i_curr = 2, i_next = 0; i_next < 3; i_prev = i_curr, i_curr = i_next++) {
    const float *co = verts[i_curr]->co;
    const float *co_other[2] = {verts[i_prev]->co, verts[i_next]->co};
    float proj_dir[3];
    mid_v3_v3v3(proj_dir, co_other[0], co_other[1]);
    sub_v3_v3(proj_dir, co);

    float proj_pair[2][3];
    project_v3_v3v3(proj_pair[0], co_other[0], proj_dir);
    project_v3_v3v3(proj_pair[1], co_other[1], proj_dir);
    difs[i_next] = len_squared_v3v3(proj_pair[0], proj_pair[1]);
  }
#else
  const float lens[3] = {
      len_v3v3(verts[0]->co, verts[1]->co),
      len_v3v3(verts[1]->co, verts[2]->co),
      len_v3v3(verts[2]->co, verts[0]->co),
  };
  const float difs[3] = {
      fabsf(lens[1] - lens[2]),
      fabsf(lens[2] - lens[0]),
      fabsf(lens[0] - lens[1]),
  };
#endif

  int order[3] = {0, 1, 2};
  axis_sort_v3(difs, order);

  return order[0];
}

void BM_vert_tri_calc_tangent_from_edge(BMVert *verts[3], float r_tangent[3])
{
  const int index = bm_vert_tri_find_unique_edge(verts);
  const int index_next = (index + 1) % 3;

  sub_v3_v3v3(r_tangent, verts[index]->co, verts[index_next]->co);
  normalize_v3(r_tangent);
}

void BM_vert_tri_calc_tangent_pair_from_edge(BMVert *verts[3],
                                             float r_tangent_a[3],
                                             float r_tangent_b[3])
{
  const int index = bm_vert_tri_find_unique_edge(verts);
  const int index_next = (index + 1) % 3;
  const int index_prev = (index_next + 1) % 3;

  sub_v3_v3v3(r_tangent_a, verts[index]->co, verts[index_next]->co);
  normalize_v3(r_tangent_a);

  /* Pick the adjacent loop that is least co-linear. */
  float vec_prev[3], vec_next[3];
  float tmp_prev[3], tmp_next[3];

  sub_v3_v3v3(vec_prev, verts[index_prev]->co, verts[index]->co);
  sub_v3_v3v3(vec_next, verts[index_next]->co, verts[index_prev]->co);

  cross_v3_v3v3(tmp_prev, r_tangent_a, vec_prev);
  cross_v3_v3v3(tmp_next, r_tangent_a, vec_next);

  normalize_v3_v3(r_tangent_b,
                  len_squared_v3(tmp_next) > len_squared_v3(tmp_prev) ? vec_next : vec_prev);
}

void BM_vert_tri_calc_tangent_edge_pair(BMVert *verts[3], float r_tangent[3])
{
  const int index = bm_vert_tri_find_unique_edge(verts);

  const float *v_a = verts[index]->co;
  const float *v_b = verts[(index + 1) % 3]->co;
  const float *v_other = verts[(index + 2) % 3]->co;

  mid_v3_v3v3(r_tangent, v_a, v_b);
  sub_v3_v3v3(r_tangent, v_other, r_tangent);

  normalize_v3(r_tangent);
}

void BM_face_calc_tangent_from_edge(const BMFace *f, float r_tangent[3])
{
  const BMLoop *l_long = BM_face_find_longest_loop((BMFace *)f);

  sub_v3_v3v3(r_tangent, l_long->v->co, l_long->next->v->co);

  normalize_v3(r_tangent);
}

static void bm_face_calc_tangent_from_quad_edge_pair(const BMFace *f, float r_tangent[3])
{
  BMVert *verts[4];
  float vec[3], vec_a[3], vec_b[3];

  BM_face_as_array_vert_quad((BMFace *)f, verts);

  sub_v3_v3v3(vec_a, verts[3]->co, verts[2]->co);
  sub_v3_v3v3(vec_b, verts[0]->co, verts[1]->co);
  add_v3_v3v3(r_tangent, vec_a, vec_b);

  sub_v3_v3v3(vec_a, verts[0]->co, verts[3]->co);
  sub_v3_v3v3(vec_b, verts[1]->co, verts[2]->co);
  add_v3_v3v3(vec, vec_a, vec_b);
  /* use the longest edge length */
  if (len_squared_v3(r_tangent) < len_squared_v3(vec)) {
    copy_v3_v3(r_tangent, vec);
  }
  normalize_v3(r_tangent);
}

static void bm_face_calc_tangent_pair_from_quad_edge_pair(const BMFace *f,
                                                          float r_tangent_a[3],
                                                          float r_tangent_b[3])
{
  BLI_assert(f->len == 4);
  BMVert *verts[4];
  float vec_a[3], vec_b[3];

  BM_face_as_array_vert_quad((BMFace *)f, verts);

  sub_v3_v3v3(vec_a, verts[3]->co, verts[2]->co);
  sub_v3_v3v3(vec_b, verts[0]->co, verts[1]->co);
  add_v3_v3v3(r_tangent_a, vec_a, vec_b);

  sub_v3_v3v3(vec_a, verts[0]->co, verts[3]->co);
  sub_v3_v3v3(vec_b, verts[1]->co, verts[2]->co);
  add_v3_v3v3(r_tangent_b, vec_a, vec_b);

  /* `r_tangent_a` always gets the longest edge. */
  if (normalize_v3(r_tangent_a) < normalize_v3(r_tangent_b)) {
    swap_v3_v3(r_tangent_a, r_tangent_b);
  }
}

void BM_face_calc_tangent_pair_from_edge(const BMFace *f,
                                         float r_tangent_a[3],
                                         float r_tangent_b[3])
{
  const BMLoop *l_long = BM_face_find_longest_loop((BMFace *)f);

  sub_v3_v3v3(r_tangent_a, l_long->v->co, l_long->next->v->co);
  normalize_v3(r_tangent_a);

  /* Pick the adjacent loop that is least co-linear. */
  float vec_prev[3], vec_next[3];
  float tmp_prev[3], tmp_next[3];

  sub_v3_v3v3(vec_prev, l_long->prev->v->co, l_long->v->co);
  sub_v3_v3v3(vec_next, l_long->next->v->co, l_long->next->next->v->co);

  cross_v3_v3v3(tmp_prev, r_tangent_a, vec_prev);
  cross_v3_v3v3(tmp_next, r_tangent_a, vec_next);

  normalize_v3_v3(r_tangent_b,
                  len_squared_v3(tmp_next) > len_squared_v3(tmp_prev) ? vec_next : vec_prev);
}

void BM_face_calc_tangent_from_edge_pair(const BMFace *f, float r_tangent[3])
{
  if (f->len == 3) {
    BMVert *verts[3];

    BM_face_as_array_vert_tri((BMFace *)f, verts);

    BM_vert_tri_calc_tangent_edge_pair(verts, r_tangent);
  }
  else if (f->len == 4) {
    /* Use longest edge pair */
    float r_tangent_dummy[3];
    bm_face_calc_tangent_pair_from_quad_edge_pair(f, r_tangent, r_tangent_dummy);
  }
  else {
    /* For ngons use two longest disconnected edges */
    BMLoop *l_long = BM_face_find_longest_loop((BMFace *)f);
    BMLoop *l_long_other = nullptr;

    float len_max_sq = 0.0f;
    float vec_a[3], vec_b[3];

    BMLoop *l_iter = l_long->prev->prev;
    BMLoop *l_last = l_long->next;

    do {
      const float len_sq = len_squared_v3v3(l_iter->v->co, l_iter->next->v->co);
      if (len_sq >= len_max_sq) {
        l_long_other = l_iter;
        len_max_sq = len_sq;
      }
    } while ((l_iter = l_iter->prev) != l_last);

    sub_v3_v3v3(vec_a, l_long->next->v->co, l_long->v->co);
    sub_v3_v3v3(vec_b, l_long_other->v->co, l_long_other->next->v->co);
    add_v3_v3v3(r_tangent, vec_a, vec_b);

    /* Edges may not be opposite side of the ngon,
     * this could cause problems for ngons with multiple-aligned edges of the same length.
     * Fall back to longest edge. */
    if (UNLIKELY(normalize_v3(r_tangent) == 0.0f)) {
      normalize_v3_v3(r_tangent, vec_a);
    }
  }
}

void BM_face_calc_tangent_from_edge_diagonal(const BMFace *f, float r_tangent[3])
{
  BMLoop *l_iter, *l_first;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);

  /* In case of degenerate faces. */
  zero_v3(r_tangent);

  /* WARNING: O(n^2) loop here, take care! */
  float dist_max_sq = 0.0f;
  do {
    BMLoop *l_iter_other = l_iter->next;
    BMLoop *l_iter_last = l_iter->prev;
    do {
      BLI_assert(!ELEM(l_iter->v, l_iter_other->v, l_iter_other->next->v));
      float co_other[3], vec[3];
      closest_to_line_segment_v3(
          co_other, l_iter->v->co, l_iter_other->v->co, l_iter_other->next->v->co);
      sub_v3_v3v3(vec, l_iter->v->co, co_other);

      const float dist_sq = len_squared_v3(vec);
      if (dist_sq > dist_max_sq) {
        dist_max_sq = dist_sq;
        copy_v3_v3(r_tangent, vec);
      }
    } while ((l_iter_other = l_iter_other->next) != l_iter_last);
  } while ((l_iter = l_iter->next) != l_first);

  normalize_v3(r_tangent);
}

void BM_face_calc_tangent_from_vert_diagonal(const BMFace *f, float r_tangent[3])
{
  BMLoop *l_iter, *l_first;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);

  /* In case of degenerate faces. */
  zero_v3(r_tangent);

  /* WARNING: O(n^2) loop here, take care! */
  float dist_max_sq = 0.0f;
  do {
    BMLoop *l_iter_other = l_iter->next;
    do {
      float vec[3];
      sub_v3_v3v3(vec, l_iter->v->co, l_iter_other->v->co);

      const float dist_sq = len_squared_v3(vec);
      if (dist_sq > dist_max_sq) {
        dist_max_sq = dist_sq;
        copy_v3_v3(r_tangent, vec);
      }
    } while ((l_iter_other = l_iter_other->next) != l_iter);
  } while ((l_iter = l_iter->next) != l_first);

  normalize_v3(r_tangent);
}

void BM_face_calc_tangent_auto(const BMFace *f, float r_tangent[3])
{
  if (f->len == 3) {
    /* most 'unique' edge of a triangle */
    BMVert *verts[3];
    BM_face_as_array_vert_tri((BMFace *)f, verts);
    BM_vert_tri_calc_tangent_from_edge(verts, r_tangent);
  }
  else if (f->len == 4) {
    /* longest edge pair of a quad */
    bm_face_calc_tangent_from_quad_edge_pair(f, r_tangent);
  }
  else {
    /* longest edge of an ngon */
    BM_face_calc_tangent_from_edge(f, r_tangent);
  }
}

void BM_face_calc_tangent_pair_auto(const BMFace *f, float r_tangent_a[3], float r_tangent_b[3])
{
  if (f->len == 3) {
    /* most 'unique' edge of a triangle */
    BMVert *verts[3];
    BM_face_as_array_vert_tri((BMFace *)f, verts);
    BM_vert_tri_calc_tangent_pair_from_edge(verts, r_tangent_a, r_tangent_b);
  }
  else if (f->len == 4) {
    /* longest edge pair of a quad */
    bm_face_calc_tangent_pair_from_quad_edge_pair(f, r_tangent_a, r_tangent_b);
  }
  else {
    /* longest edge of an ngon */
    BM_face_calc_tangent_pair_from_edge(f, r_tangent_a, r_tangent_b);
  }
}

void BM_face_calc_bounds_expand(const BMFace *f, float min[3], float max[3])
{
  const BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    minmax_v3v3_v3(min, max, l_iter->v->co);
  } while ((l_iter = l_iter->next) != l_first);
}

void BM_face_calc_center_bounds(const BMFace *f, float r_cent[3])
{
  const BMLoop *l_iter, *l_first;
  float min[3], max[3];

  INIT_MINMAX(min, max);

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    minmax_v3v3_v3(min, max, l_iter->v->co);
  } while ((l_iter = l_iter->next) != l_first);

  mid_v3_v3v3(r_cent, min, max);
}

void BM_face_calc_center_bounds_vcos(const BMesh *bm,
                                     const BMFace *f,
                                     float r_cent[3],
                                     const blender::Span<blender::float3> vert_positions)
{
  /* must have valid index data */
  BLI_assert((bm->elem_index_dirty & BM_VERT) == 0);
  (void)bm;

  const BMLoop *l_iter, *l_first;
  float min[3], max[3];

  INIT_MINMAX(min, max);

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    minmax_v3v3_v3(min, max, vert_positions[BM_elem_index_get(l_iter->v)]);
  } while ((l_iter = l_iter->next) != l_first);

  mid_v3_v3v3(r_cent, min, max);
}

void BM_face_calc_center_median(const BMFace *f, float r_cent[3])
{
  const BMLoop *l_iter, *l_first;

  zero_v3(r_cent);

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    add_v3_v3(r_cent, l_iter->v->co);
  } while ((l_iter = l_iter->next) != l_first);
  mul_v3_fl(r_cent, 1.0f / float(f->len));
}

void BM_face_calc_center_median_weighted(const BMFace *f, float r_cent[3])
{
  const BMLoop *l_iter;
  const BMLoop *l_first;
  float totw = 0.0f;
  float w_prev;

  zero_v3(r_cent);

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  w_prev = BM_edge_calc_length(l_iter->prev->e);
  do {
    const float w_curr = BM_edge_calc_length(l_iter->e);
    const float w = (w_curr + w_prev);
    madd_v3_v3fl(r_cent, l_iter->v->co, w);
    totw += w;
    w_prev = w_curr;
  } while ((l_iter = l_iter->next) != l_first);

  if (totw != 0.0f) {
    mul_v3_fl(r_cent, 1.0f / totw);
  }
}

void poly_rotate_plane(const float normal[3], float (*verts)[3], const uint nverts)
{
  float mat[3][3];
  float co[3];
  uint i;

  co[2] = 0.0f;

  axis_dominant_v3_to_m3(mat, normal);
  for (i = 0; i < nverts; i++) {
    mul_v2_m3v3(co, mat, verts[i]);
    copy_v3_v3(verts[i], co);
  }
}

void BM_edge_normals_update(BMEdge *e)
{
  BMIter iter;
  BMFace *f;

  BM_ITER_ELEM (f, &iter, e, BM_FACES_OF_EDGE) {
    BM_face_normal_update(f);
  }

  BM_vert_normal_update(e->v1);
  BM_vert_normal_update(e->v2);
}

static void bm_loop_normal_accum(const BMLoop *l, float no[3])
{
  float vec1[3], vec2[3], fac;

  /* Same calculation used in BM_mesh_normals_update */
  sub_v3_v3v3(vec1, l->v->co, l->prev->v->co);
  sub_v3_v3v3(vec2, l->next->v->co, l->v->co);
  normalize_v3(vec1);
  normalize_v3(vec2);

  fac = blender::math::safe_acos_approx(-dot_v3v3(vec1, vec2));

  madd_v3_v3fl(no, l->f->no, fac);
}

bool BM_vert_calc_normal_ex(const BMVert *v, const char hflag, float r_no[3])
{
  int len = 0;

  zero_v3(r_no);

  if (v->e) {
    const BMEdge *e = v->e;
    do {
      if (e->l) {
        const BMLoop *l = e->l;
        do {
          if (l->v == v) {
            if (BM_elem_flag_test(l->f, hflag)) {
              bm_loop_normal_accum(l, r_no);
              len++;
            }
          }
        } while ((l = l->radial_next) != e->l);
      }
    } while ((e = bmesh_disk_edge_next(e, v)) != v->e);
  }

  if (len) {
    normalize_v3(r_no);
    return true;
  }
  return false;
}

bool BM_vert_calc_normal(const BMVert *v, float r_no[3])
{
  int len = 0;

  zero_v3(r_no);

  if (v->e) {
    const BMEdge *e = v->e;
    do {
      if (e->l) {
        const BMLoop *l = e->l;
        do {
          if (l->v == v) {
            bm_loop_normal_accum(l, r_no);
            len++;
          }
        } while ((l = l->radial_next) != e->l);
      }
    } while ((e = bmesh_disk_edge_next(e, v)) != v->e);
  }

  if (len) {
    normalize_v3(r_no);
    return true;
  }
  return false;
}

void BM_vert_normal_update_all(BMVert *v)
{
  int len = 0;

  zero_v3(v->no);

  if (v->e) {
    const BMEdge *e = v->e;
    do {
      if (e->l) {
        const BMLoop *l = e->l;
        do {
          if (l->v == v) {
            BM_face_normal_update(l->f);
            bm_loop_normal_accum(l, v->no);
            len++;
          }
        } while ((l = l->radial_next) != e->l);
      }
    } while ((e = bmesh_disk_edge_next(e, v)) != v->e);
  }

  if (len) {
    normalize_v3(v->no);
  }
}

void BM_vert_normal_update(BMVert *v)
{
  BM_vert_calc_normal(v, v->no);
}

float BM_face_calc_normal(const BMFace *f, float r_no[3])
{
  BMLoop *l;

  /* common cases first */
  switch (f->len) {
    case 4: {
      const float *co1 = (l = BM_FACE_FIRST_LOOP(f))->v->co;
      const float *co2 = (l = l->next)->v->co;
      const float *co3 = (l = l->next)->v->co;
      const float *co4 = (l->next)->v->co;

      return normal_quad_v3(r_no, co1, co2, co3, co4);
    }
    case 3: {
      const float *co1 = (l = BM_FACE_FIRST_LOOP(f))->v->co;
      const float *co2 = (l = l->next)->v->co;
      const float *co3 = (l->next)->v->co;

      return normal_tri_v3(r_no, co1, co2, co3);
    }
    default: {
      return bm_face_calc_poly_normal(f, r_no);
    }
  }
}
void BM_face_normal_update(BMFace *f)
{
  BM_face_calc_normal(f, f->no);
}

float BM_face_calc_normal_vcos(const BMesh *bm,
                               const BMFace *f,
                               float r_no[3],
                               const Span<float3> vertexCos)
{
  BMLoop *l;

  /* must have valid index data */
  BLI_assert((bm->elem_index_dirty & BM_VERT) == 0);
  (void)bm;

  /* common cases first */
  switch (f->len) {
    case 4: {
      const float *co1 = vertexCos[BM_elem_index_get((l = BM_FACE_FIRST_LOOP(f))->v)];
      const float *co2 = vertexCos[BM_elem_index_get((l = l->next)->v)];
      const float *co3 = vertexCos[BM_elem_index_get((l = l->next)->v)];
      const float *co4 = vertexCos[BM_elem_index_get((l->next)->v)];

      return normal_quad_v3(r_no, co1, co2, co3, co4);
    }
    case 3: {
      const float *co1 = vertexCos[BM_elem_index_get((l = BM_FACE_FIRST_LOOP(f))->v)];
      const float *co2 = vertexCos[BM_elem_index_get((l = l->next)->v)];
      const float *co3 = vertexCos[BM_elem_index_get((l->next)->v)];

      return normal_tri_v3(r_no, co1, co2, co3);
    }
    default: {
      return bm_face_calc_poly_normal_vertex_cos(f, r_no, vertexCos);
    }
  }
}

void BM_verts_calc_normal_from_cloud_ex(
    BMVert **varr, int varr_len, float r_normal[3], float r_center[3], int *r_index_tangent)
{
  const float varr_len_inv = 1.0f / float(varr_len);

  /* Get the center point and collect vector array since we loop over these a lot. */
  float center[3] = {0.0f, 0.0f, 0.0f};
  for (int i = 0; i < varr_len; i++) {
    madd_v3_v3fl(center, varr[i]->co, varr_len_inv);
  }

  /* Find the 'co_a' point from center. */
  int co_a_index = 0;
  const float *co_a = nullptr;
  {
    float dist_sq_max = -1.0f;
    for (int i = 0; i < varr_len; i++) {
      const float dist_sq_test = len_squared_v3v3(varr[i]->co, center);
      if (!(dist_sq_test <= dist_sq_max)) {
        co_a = varr[i]->co;
        co_a_index = i;
        dist_sq_max = dist_sq_test;
      }
    }
  }

  float dir_a[3];
  sub_v3_v3v3(dir_a, co_a, center);
  normalize_v3(dir_a);

  const float *co_b = nullptr;
  float dir_b[3] = {0.0f, 0.0f, 0.0f};
  {
    float dist_sq_max = -1.0f;
    for (int i = 0; i < varr_len; i++) {
      if (varr[i]->co == co_a) {
        continue;
      }
      float dir_test[3];
      sub_v3_v3v3(dir_test, varr[i]->co, center);
      project_plane_normalized_v3_v3v3(dir_test, dir_test, dir_a);
      const float dist_sq_test = len_squared_v3(dir_test);
      if (!(dist_sq_test <= dist_sq_max)) {
        co_b = varr[i]->co;
        dist_sq_max = dist_sq_test;
        copy_v3_v3(dir_b, dir_test);
      }
    }
  }

  if (varr_len <= 3) {
    normal_tri_v3(r_normal, center, co_a, co_b);
    goto finally;
  }

  {
    normalize_v3(dir_b);

    const float *co_a_opposite = nullptr;
    const float *co_b_opposite = nullptr;

    {
      float dot_a_min = FLT_MAX;
      float dot_b_min = FLT_MAX;
      for (int i = 0; i < varr_len; i++) {
        const float *co_test = varr[i]->co;
        float dot_test;

        if (co_test != co_a) {
          dot_test = dot_v3v3(dir_a, co_test);
          if (dot_test < dot_a_min) {
            dot_a_min = dot_test;
            co_a_opposite = co_test;
          }
        }

        if (co_test != co_b) {
          dot_test = dot_v3v3(dir_b, co_test);
          if (dot_test < dot_b_min) {
            dot_b_min = dot_test;
            co_b_opposite = co_test;
          }
        }
      }
    }

    normal_quad_v3(r_normal, co_a, co_b, co_a_opposite, co_b_opposite);
  }

finally:
  if (r_center != nullptr) {
    copy_v3_v3(r_center, center);
  }
  if (r_index_tangent != nullptr) {
    *r_index_tangent = co_a_index;
  }
}

void BM_verts_calc_normal_from_cloud(BMVert **varr, int varr_len, float r_normal[3])
{
  BM_verts_calc_normal_from_cloud_ex(varr, varr_len, r_normal, nullptr, nullptr);
}

float BM_face_calc_normal_subset(const BMLoop *l_first, const BMLoop *l_last, float r_no[3])
{
  const float *v_prev, *v_curr;

  /* Newell's Method */
  const BMLoop *l_iter = l_first;
  const BMLoop *l_term = l_last->next;

  zero_v3(r_no);

  v_prev = l_last->v->co;
  do {
    v_curr = l_iter->v->co;
    add_newell_cross_v3_v3v3(r_no, v_prev, v_curr);
    v_prev = v_curr;
  } while ((l_iter = l_iter->next) != l_term);

  return normalize_v3(r_no);
}

void BM_face_calc_center_median_vcos(const BMesh *bm,
                                     const BMFace *f,
                                     float r_cent[3],
                                     const blender::Span<blender::float3> vert_positions)
{
  /* must have valid index data */
  BLI_assert((bm->elem_index_dirty & BM_VERT) == 0);
  (void)bm;

  bm_face_calc_poly_center_median_vertex_cos(f, r_cent, vert_positions);
}

void BM_face_normal_flip_ex(BMesh *bm,
                            BMFace *f,
                            const int cd_loop_mdisp_offset,
                            const bool use_loop_mdisp_flip)
{
  bmesh_kernel_loop_reverse(bm, f, cd_loop_mdisp_offset, use_loop_mdisp_flip);
  negate_v3(f->no);
}

void BM_face_normal_flip(BMesh *bm, BMFace *f)
{
  const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
  BM_face_normal_flip_ex(bm, f, cd_loop_mdisp_offset, true);
}

bool BM_face_point_inside_test(const BMFace *f, const float co[3])
{
  float axis_mat[3][3];
  float (*projverts)[2] = BLI_array_alloca(projverts, f->len);

  float co_2d[2];
  BMLoop *l_iter;
  int i;

  BLI_assert(BM_face_is_normal_valid(f));

  axis_dominant_v3_to_m3(axis_mat, f->no);

  mul_v2_m3v3(co_2d, axis_mat, co);

  for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f); i < f->len; i++, l_iter = l_iter->next) {
    mul_v2_m3v3(projverts[i], axis_mat, l_iter->v->co);
  }

  return isect_point_poly_v2(co_2d, projverts, f->len);
}

void BM_face_triangulate(BMesh *bm,
                         BMFace *f,
                         BMFace **r_faces_new,
                         int *r_faces_new_tot,
                         BMEdge **r_edges_new,
                         int *r_edges_new_tot,
                         LinkNode **r_faces_double,
                         const int quad_method,
                         const int ngon_method,
                         const bool use_tag,
                         /* use for ngons only! */
                         MemArena *pf_arena,

                         /* use for MOD_TRIANGULATE_NGON_BEAUTY only! */
                         Heap *pf_heap)
{
  const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
  const bool use_beauty = (ngon_method == MOD_TRIANGULATE_NGON_BEAUTY);
  BMLoop *l_first, *l_new;
  BMFace *f_new;
  int nf_i = 0;
  int ne_i = 0;

  BLI_assert(BM_face_is_normal_valid(f));

  /* ensure both are valid or nullptr */
  BLI_assert((r_faces_new == nullptr) == (r_faces_new_tot == nullptr));

  BLI_assert(f->len > 3);

  {
    BMLoop **loops = BLI_array_alloca(loops, f->len);
    uint(*tris)[3] = BLI_array_alloca(tris, f->len);
    const int totfilltri = f->len - 2;
    const int last_tri = f->len - 3;
    int i;
    /* for mdisps */
    float f_center[3];

    if (f->len == 4) {
      /* even though we're not using BLI_polyfill, fill in 'tris' and 'loops'
       * so we can share code to handle face creation afterwards. */
      BMLoop *l_v1, *l_v2;

      l_first = BM_FACE_FIRST_LOOP(f);

      switch (quad_method) {
        case MOD_TRIANGULATE_QUAD_FIXED: {
          l_v1 = l_first;
          l_v2 = l_first->next->next;
          break;
        }
        case MOD_TRIANGULATE_QUAD_ALTERNATE: {
          l_v1 = l_first->next;
          l_v2 = l_first->prev;
          break;
        }
        case MOD_TRIANGULATE_QUAD_SHORTEDGE:
        case MOD_TRIANGULATE_QUAD_LONGEDGE:
        case MOD_TRIANGULATE_QUAD_BEAUTY:
        default: {
          BMLoop *l_v3, *l_v4;
          bool split_24;

          l_v1 = l_first->next;
          l_v2 = l_first->next->next;
          l_v3 = l_first->prev;
          l_v4 = l_first;

          if (quad_method == MOD_TRIANGULATE_QUAD_SHORTEDGE) {
            float d1, d2;
            d1 = len_squared_v3v3(l_v4->v->co, l_v2->v->co);
            d2 = len_squared_v3v3(l_v1->v->co, l_v3->v->co);
            split_24 = ((d2 - d1) > 0.0f);
          }
          else if (quad_method == MOD_TRIANGULATE_QUAD_LONGEDGE) {
            float d1, d2;
            d1 = len_squared_v3v3(l_v4->v->co, l_v2->v->co);
            d2 = len_squared_v3v3(l_v1->v->co, l_v3->v->co);
            split_24 = ((d2 - d1) < 0.0f);
          }
          else {
            /* first check if the quad is concave on either diagonal */
            const int flip_flag = is_quad_flip_v3(
                l_v1->v->co, l_v2->v->co, l_v3->v->co, l_v4->v->co);
            if (UNLIKELY(flip_flag & (1 << 0))) {
              split_24 = true;
            }
            else if (UNLIKELY(flip_flag & (1 << 1))) {
              split_24 = false;
            }
            else {
              split_24 = (BM_verts_calc_rotate_beauty(l_v1->v, l_v2->v, l_v3->v, l_v4->v, 0, 0) >
                          0.0f);
            }
          }

          /* named confusingly, l_v1 is in fact the second vertex */
          if (split_24) {
            l_v1 = l_v4;
            // l_v2 = l_v2;
          }
          else {
            // l_v1 = l_v1;
            l_v2 = l_v3;
          }
          break;
        }
      }

      loops[0] = l_v1;
      loops[1] = l_v1->next;
      loops[2] = l_v2;
      loops[3] = l_v2->next;

      ARRAY_SET_ITEMS(tris[0], 0, 1, 2);
      ARRAY_SET_ITEMS(tris[1], 0, 2, 3);
    }
    else {
      BMLoop *l_iter;
      float axis_mat[3][3];
      float (*projverts)[2] = BLI_array_alloca(projverts, f->len);

      axis_dominant_v3_to_m3_negate(axis_mat, f->no);

      for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f); i < f->len; i++, l_iter = l_iter->next) {
        loops[i] = l_iter;
        mul_v2_m3v3(projverts[i], axis_mat, l_iter->v->co);
      }

      BLI_polyfill_calc_arena(projverts, f->len, 1, tris, pf_arena);

      if (use_beauty) {
        BLI_polyfill_beautify(projverts, f->len, tris, pf_arena, pf_heap);
      }

      BLI_memarena_clear(pf_arena);
    }

    if (cd_loop_mdisp_offset != -1) {
      BM_face_calc_center_median(f, f_center);
    }

    /* loop over calculated triangles and create new geometry */
    for (i = 0; i < totfilltri; i++) {
      BMLoop *ltri[3] = {loops[tris[i][0]], loops[tris[i][1]], loops[tris[i][2]]};

      BMVert *v_tri[3] = {ltri[0]->v, ltri[1]->v, ltri[2]->v};

      f_new = BM_face_create_verts(bm, v_tri, 3, f, BM_CREATE_NOP, true);
      l_new = BM_FACE_FIRST_LOOP(f_new);

      BLI_assert(v_tri[0] == l_new->v);

      /* check for duplicate */
      if (l_new->radial_next != l_new) {
        BMLoop *l_iter = l_new->radial_next;
        do {
          if (UNLIKELY((l_iter->f->len == 3) && (l_new->prev->v == l_iter->prev->v))) {
            /* Check the last tri because we swap last f_new with f at the end... */
            BLI_linklist_prepend(r_faces_double, (i != last_tri) ? f_new : f);
            break;
          }
        } while ((l_iter = l_iter->radial_next) != l_new);
      }

      /* copy CD data */
      BM_elem_attrs_copy(bm, ltri[0], l_new);
      BM_elem_attrs_copy(bm, ltri[1], l_new->next);
      BM_elem_attrs_copy(bm, ltri[2], l_new->prev);

      /* add all but the last face which is swapped and removed (below) */
      if (i != last_tri) {
        if (use_tag) {
          BM_elem_flag_enable(f_new, BM_ELEM_TAG);
        }
        if (r_faces_new) {
          r_faces_new[nf_i++] = f_new;
        }
      }

      if (use_tag || r_edges_new) {
        /* new faces loops */
        BMLoop *l_iter;

        l_iter = l_first = l_new;
        do {
          BMEdge *e = l_iter->e;
          /* Confusing! if its not a boundary now, we know it will be later since this will be an
           * edge of one of the new faces which we're in the middle of creating. */
          bool is_new_edge = (l_iter == l_iter->radial_next);

          if (is_new_edge) {
            if (use_tag) {
              BM_elem_flag_enable(e, BM_ELEM_TAG);
            }
            if (r_edges_new) {
              r_edges_new[ne_i++] = e;
            }
          }
          /* NOTE: never disable tag's. */
        } while ((l_iter = l_iter->next) != l_first);
      }

      if (cd_loop_mdisp_offset != -1) {
        float f_new_center[3];
        BM_face_calc_center_median(f_new, f_new_center);
        BM_face_interp_multires_ex(bm, f_new, f, f_new_center, f_center, cd_loop_mdisp_offset);
      }
    }

    {
      /* we can't delete the real face, because some of the callers expect it to remain valid.
       * so swap data and delete the last created tri */
      bmesh_face_swap_data(f, f_new);
      BM_face_kill(bm, f_new);
    }
  }
  bm->elem_index_dirty |= BM_FACE;

  if (r_faces_new_tot) {
    *r_faces_new_tot = nf_i;
  }

  if (r_edges_new_tot) {
    *r_edges_new_tot = ne_i;
  }
}

void BM_face_splits_check_legal(BMesh *bm, BMFace *f, BMLoop *(*loops)[2], int len)
{
  blender::float2 out = {-FLT_MAX, -FLT_MAX};
  float center[2] = {0.0f, 0.0f};
  float axis_mat[3][3];
  float (*projverts)[2] = BLI_array_alloca(projverts, f->len);
  const float *(*edgeverts)[2] = BLI_array_alloca(edgeverts, len);
  BMLoop *l;
  int i, i_prev, j;

  BLI_assert(BM_face_is_normal_valid(f));

  axis_dominant_v3_to_m3(axis_mat, f->no);

  for (i = 0, l = BM_FACE_FIRST_LOOP(f); i < f->len; i++, l = l->next) {
    mul_v2_m3v3(projverts[i], axis_mat, l->v->co);
    add_v2_v2(center, projverts[i]);
  }

  /* first test for completely convex face */
  if (is_poly_convex_v2(projverts, f->len)) {
    return;
  }

  mul_v2_fl(center, 1.0f / f->len);

  for (i = 0, l = BM_FACE_FIRST_LOOP(f); i < f->len; i++, l = l->next) {
    BM_elem_index_set(l, i); /* set_dirty */

    /* center the projection for maximum accuracy */
    sub_v2_v2(projverts[i], center);

    out[0] = max_ff(out[0], projverts[i][0]);
    out[1] = max_ff(out[1], projverts[i][1]);
  }
  bm->elem_index_dirty |= BM_LOOP;

  /* ensure we are well outside the face bounds (value is arbitrary) */
  out += 1.0f;

  for (i = 0; i < len; i++) {
    edgeverts[i][0] = projverts[BM_elem_index_get(loops[i][0])];
    edgeverts[i][1] = projverts[BM_elem_index_get(loops[i][1])];
  }

  /* do convexity test */
  for (i = 0; i < len; i++) {
    float mid[2];
    mid_v2_v2v2(mid, edgeverts[i][0], edgeverts[i][1]);

    int isect = 0;
    int j_prev;
    for (j = 0, j_prev = f->len - 1; j < f->len; j_prev = j++) {
      const float *f_edge[2] = {projverts[j_prev], projverts[j]};
      if (isect_seg_seg_v2(UNPACK2(f_edge), mid, out) == ISECT_LINE_LINE_CROSS) {
        isect++;
      }
    }

    if (isect % 2 == 0) {
      loops[i][0] = nullptr;
    }
  }

#define EDGE_SHARE_VERT(e1, e2) \
  (ELEM((e1)[0], (e2)[0], (e2)[1]) || ELEM((e1)[1], (e2)[0], (e2)[1]))

  /* do line crossing tests */
  for (i = 0, i_prev = f->len - 1; i < f->len; i_prev = i++) {
    const float *f_edge[2] = {projverts[i_prev], projverts[i]};
    for (j = 0; j < len; j++) {
      if ((loops[j][0] != nullptr) && !EDGE_SHARE_VERT(f_edge, edgeverts[j])) {
        if (isect_seg_seg_v2(UNPACK2(f_edge), UNPACK2(edgeverts[j])) == ISECT_LINE_LINE_CROSS) {
          loops[j][0] = nullptr;
        }
      }
    }
  }

  /* self intersect tests */
  for (i = 0; i < len; i++) {
    if (loops[i][0]) {
      for (j = i + 1; j < len; j++) {
        if ((loops[j][0] != nullptr) && !EDGE_SHARE_VERT(edgeverts[i], edgeverts[j])) {
          if (isect_seg_seg_v2(UNPACK2(edgeverts[i]), UNPACK2(edgeverts[j])) ==
              ISECT_LINE_LINE_CROSS)
          {
            loops[i][0] = nullptr;
            break;
          }
        }
      }
    }
  }

#undef EDGE_SHARE_VERT
}

void BM_face_splits_check_optimal(BMFace *f, BMLoop *(*loops)[2], int len)
{
  int i;

  for (i = 0; i < len; i++) {
    BMLoop *l_a_dummy, *l_b_dummy;
    if (f != BM_vert_pair_share_face_by_angle(
                 loops[i][0]->v, loops[i][1]->v, &l_a_dummy, &l_b_dummy, false))
    {
      loops[i][0] = nullptr;
    }
  }
}

void BM_face_as_array_vert_tri(BMFace *f, BMVert *r_verts[3])
{
  BMLoop *l = BM_FACE_FIRST_LOOP(f);

  BLI_assert(f->len == 3);

  r_verts[0] = l->v;
  l = l->next;
  r_verts[1] = l->v;
  l = l->next;
  r_verts[2] = l->v;
}

void BM_face_as_array_vert_quad(BMFace *f, BMVert *r_verts[4])
{
  BMLoop *l = BM_FACE_FIRST_LOOP(f);

  BLI_assert(f->len == 4);

  r_verts[0] = l->v;
  l = l->next;
  r_verts[1] = l->v;
  l = l->next;
  r_verts[2] = l->v;
  l = l->next;
  r_verts[3] = l->v;
}

void BM_face_as_array_loop_tri(BMFace *f, BMLoop *r_loops[3])
{
  BMLoop *l = BM_FACE_FIRST_LOOP(f);

  BLI_assert(f->len == 3);

  r_loops[0] = l;
  l = l->next;
  r_loops[1] = l;
  l = l->next;
  r_loops[2] = l;
}

void BM_face_as_array_loop_quad(BMFace *f, BMLoop *r_loops[4])
{
  BMLoop *l = BM_FACE_FIRST_LOOP(f);

  BLI_assert(f->len == 4);

  r_loops[0] = l;
  l = l->next;
  r_loops[1] = l;
  l = l->next;
  r_loops[2] = l;
  l = l->next;
  r_loops[3] = l;
}
