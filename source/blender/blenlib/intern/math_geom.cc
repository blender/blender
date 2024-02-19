/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_array.hh"
#include "BLI_math_base.h"
#include "BLI_math_base.hh"
#include "BLI_math_geom.h"

#include "BLI_math_base_safe.h"
#include "BLI_math_bits.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLI_strict_flags.h"

/********************************** Polygons *********************************/

void cross_tri_v3(float n[3], const float v1[3], const float v2[3], const float v3[3])
{
  float n1[3], n2[3];

  n1[0] = v1[0] - v2[0];
  n2[0] = v2[0] - v3[0];
  n1[1] = v1[1] - v2[1];
  n2[1] = v2[1] - v3[1];
  n1[2] = v1[2] - v2[2];
  n2[2] = v2[2] - v3[2];
  n[0] = n1[1] * n2[2] - n1[2] * n2[1];
  n[1] = n1[2] * n2[0] - n1[0] * n2[2];
  n[2] = n1[0] * n2[1] - n1[1] * n2[0];
}

float normal_tri_v3(float n[3], const float v1[3], const float v2[3], const float v3[3])
{
  float n1[3], n2[3];

  n1[0] = v1[0] - v2[0];
  n2[0] = v2[0] - v3[0];
  n1[1] = v1[1] - v2[1];
  n2[1] = v2[1] - v3[1];
  n1[2] = v1[2] - v2[2];
  n2[2] = v2[2] - v3[2];
  n[0] = n1[1] * n2[2] - n1[2] * n2[1];
  n[1] = n1[2] * n2[0] - n1[0] * n2[2];
  n[2] = n1[0] * n2[1] - n1[1] * n2[0];

  return normalize_v3(n);
}

float normal_quad_v3(
    float n[3], const float v1[3], const float v2[3], const float v3[3], const float v4[3])
{
  /* real cross! */
  float n1[3], n2[3];

  n1[0] = v1[0] - v3[0];
  n1[1] = v1[1] - v3[1];
  n1[2] = v1[2] - v3[2];

  n2[0] = v2[0] - v4[0];
  n2[1] = v2[1] - v4[1];
  n2[2] = v2[2] - v4[2];

  n[0] = n1[1] * n2[2] - n1[2] * n2[1];
  n[1] = n1[2] * n2[0] - n1[0] * n2[2];
  n[2] = n1[0] * n2[1] - n1[1] * n2[0];

  return normalize_v3(n);
}

float normal_poly_v3(float n[3], const float verts[][3], uint nr)
{
  cross_poly_v3(n, verts, nr);
  return normalize_v3(n);
}

float area_quad_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3])
{
  const float verts[4][3] = {{UNPACK3(v1)}, {UNPACK3(v2)}, {UNPACK3(v3)}, {UNPACK3(v4)}};
  return area_poly_v3(verts, 4);
}

float area_squared_quad_v3(const float v1[3],
                           const float v2[3],
                           const float v3[3],
                           const float v4[3])
{
  const float verts[4][3] = {{UNPACK3(v1)}, {UNPACK3(v2)}, {UNPACK3(v3)}, {UNPACK3(v4)}};
  return area_squared_poly_v3(verts, 4);
}

float area_tri_v3(const float v1[3], const float v2[3], const float v3[3])
{
  float n[3];
  cross_tri_v3(n, v1, v2, v3);
  return len_v3(n) * 0.5f;
}

float area_squared_tri_v3(const float v1[3], const float v2[3], const float v3[3])
{
  float n[3];
  cross_tri_v3(n, v1, v2, v3);
  mul_v3_fl(n, 0.5f);
  return len_squared_v3(n);
}

float area_tri_signed_v3(const float v1[3],
                         const float v2[3],
                         const float v3[3],
                         const float normal[3])
{
  float area, n[3];

  cross_tri_v3(n, v1, v2, v3);
  area = len_v3(n) * 0.5f;

  /* negate area for flipped triangles */
  if (dot_v3v3(n, normal) < 0.0f) {
    area = -area;
  }

  return area;
}

float area_poly_v3(const float verts[][3], uint nr)
{
  float n[3];
  cross_poly_v3(n, verts, nr);
  return len_v3(n) * 0.5f;
}

float area_squared_poly_v3(const float verts[][3], uint nr)
{
  float n[3];

  cross_poly_v3(n, verts, nr);
  mul_v3_fl(n, 0.5f);
  return len_squared_v3(n);
}

float cross_poly_v2(const float verts[][2], uint nr)
{
  uint a;
  float cross;
  const float *co_curr, *co_prev;

  /* The Trapezium Area Rule */
  co_prev = verts[nr - 1];
  co_curr = verts[0];
  cross = 0.0f;
  for (a = 0; a < nr; a++) {
    cross += (co_prev[0] - co_curr[0]) * (co_curr[1] + co_prev[1]);
    co_prev = co_curr;
    co_curr += 2;
  }

  return cross;
}

void cross_poly_v3(float n[3], const float verts[][3], uint nr)
{
  const float *v_prev = verts[nr - 1];
  const float *v_curr = verts[0];
  uint i;

  zero_v3(n);

  /* Newell's Method */
  for (i = 0; i < nr; v_prev = v_curr, v_curr = verts[++i]) {
    add_newell_cross_v3_v3v3(n, v_prev, v_curr);
  }
}

float area_poly_v2(const float verts[][2], uint nr)
{
  return fabsf(0.5f * cross_poly_v2(verts, nr));
}

float area_poly_signed_v2(const float verts[][2], uint nr)
{
  return (0.5f * cross_poly_v2(verts, nr));
}

float area_squared_poly_v2(const float verts[][2], uint nr)
{
  float area = area_poly_signed_v2(verts, nr);
  return area * area;
}

float cotangent_tri_weight_v3(const float v1[3], const float v2[3], const float v3[3])
{
  float a[3], b[3], c[3], c_len;

  sub_v3_v3v3(a, v2, v1);
  sub_v3_v3v3(b, v3, v1);
  cross_v3_v3v3(c, a, b);

  c_len = len_v3(c);

  if (c_len > FLT_EPSILON) {
    return dot_v3v3(a, b) / c_len;
  }

  return 0.0f;
}

/********************************* Planes **********************************/

void plane_from_point_normal_v3(float r_plane[4], const float plane_co[3], const float plane_no[3])
{
  copy_v3_v3(r_plane, plane_no);
  r_plane[3] = -dot_v3v3(r_plane, plane_co);
}

void plane_to_point_vector_v3(const float plane[4], float r_plane_co[3], float r_plane_no[3])
{
  mul_v3_v3fl(r_plane_co, plane, (-plane[3] / len_squared_v3(plane)));
  copy_v3_v3(r_plane_no, plane);
}

void plane_to_point_vector_v3_normalized(const float plane[4],
                                         float r_plane_co[3],
                                         float r_plane_no[3])
{
  const float length = normalize_v3_v3(r_plane_no, plane);
  mul_v3_v3fl(r_plane_co, r_plane_no, (-plane[3] / length));
}

/********************************* Volume **********************************/

float volume_tetrahedron_v3(const float v1[3],
                            const float v2[3],
                            const float v3[3],
                            const float v4[3])
{
  float m[3][3];
  sub_v3_v3v3(m[0], v1, v2);
  sub_v3_v3v3(m[1], v2, v3);
  sub_v3_v3v3(m[2], v3, v4);
  return fabsf(determinant_m3_array(m)) / 6.0f;
}

float volume_tetrahedron_signed_v3(const float v1[3],
                                   const float v2[3],
                                   const float v3[3],
                                   const float v4[3])
{
  float m[3][3];
  sub_v3_v3v3(m[0], v1, v2);
  sub_v3_v3v3(m[1], v2, v3);
  sub_v3_v3v3(m[2], v3, v4);
  return determinant_m3_array(m) / 6.0f;
}

float volume_tri_tetrahedron_signed_v3_6x(const float v1[3], const float v2[3], const float v3[3])
{
  float v_cross[3];
  cross_v3_v3v3(v_cross, v1, v2);
  float tetra_volume = dot_v3v3(v_cross, v3);
  return tetra_volume;
}

float volume_tri_tetrahedron_signed_v3(const float v1[3], const float v2[3], const float v3[3])
{
  return volume_tri_tetrahedron_signed_v3_6x(v1, v2, v3) / 6.0f;
}

/********************************* Distance **********************************/

float dist_squared_to_line_v2(const float p[2], const float l1[2], const float l2[2])
{
  float closest[2];

  closest_to_line_v2(closest, p, l1, l2);

  return len_squared_v2v2(closest, p);
}
float dist_to_line_v2(const float p[2], const float l1[2], const float l2[2])
{
  return sqrtf(dist_squared_to_line_v2(p, l1, l2));
}

float dist_squared_to_line_segment_v2(const float p[2], const float l1[2], const float l2[2])
{
  float closest[2];

  closest_to_line_segment_v2(closest, p, l1, l2);

  return len_squared_v2v2(closest, p);
}

float dist_to_line_segment_v2(const float p[2], const float l1[2], const float l2[2])
{
  return sqrtf(dist_squared_to_line_segment_v2(p, l1, l2));
}

float closest_seg_seg_v2(float r_closest_a[2],
                         float r_closest_b[2],
                         float *r_lambda_a,
                         float *r_lambda_b,
                         const float a1[2],
                         const float a2[2],
                         const float b1[2],
                         const float b2[2])
{
  if (isect_seg_seg_v2_simple(a1, a2, b1, b2)) {
    float intersection[2];
    isect_line_line_v2_point(a1, a2, b1, b2, intersection);
    copy_v2_v2(r_closest_a, intersection);
    copy_v2_v2(r_closest_b, intersection);
    float tmp[2];
    *r_lambda_a = closest_to_line_v2(tmp, intersection, a1, a2);
    *r_lambda_b = closest_to_line_v2(tmp, intersection, b1, b2);
    const float min_dist_sq = len_squared_v2v2(r_closest_a, r_closest_b);
    return min_dist_sq;
  }

  float p1[2], p2[2], p3[2], p4[2];
  const float lambda1 = closest_to_line_segment_v2(p1, a1, b1, b2);
  const float lambda2 = closest_to_line_segment_v2(p2, a2, b1, b2);
  const float lambda3 = closest_to_line_segment_v2(p3, b1, a1, a2);
  const float lambda4 = closest_to_line_segment_v2(p4, b2, a1, a2);
  const float dist_sq1 = len_squared_v2v2(p1, a1);
  const float dist_sq2 = len_squared_v2v2(p2, a2);
  const float dist_sq3 = len_squared_v2v2(p3, b1);
  const float dist_sq4 = len_squared_v2v2(p4, b2);

  const float min_dist_sq = min_ffff(dist_sq1, dist_sq2, dist_sq3, dist_sq4);
  if (min_dist_sq == dist_sq1) {
    copy_v2_v2(r_closest_a, a1);
    copy_v2_v2(r_closest_b, p1);
    *r_lambda_a = 0.0f;
    *r_lambda_b = lambda1;
  }
  else if (min_dist_sq == dist_sq2) {
    copy_v2_v2(r_closest_a, a2);
    copy_v2_v2(r_closest_b, p2);
    *r_lambda_a = 1.0f;
    *r_lambda_b = lambda2;
  }
  else if (min_dist_sq == dist_sq3) {
    copy_v2_v2(r_closest_a, p3);
    copy_v2_v2(r_closest_b, b1);
    *r_lambda_a = lambda3;
    *r_lambda_b = 0.0f;
  }
  else {
    BLI_assert(min_dist_sq == dist_sq4);
    copy_v2_v2(r_closest_a, p4);
    copy_v2_v2(r_closest_b, b2);
    *r_lambda_a = lambda4;
    *r_lambda_b = 1.0f;
  }
  return min_dist_sq;
}

float closest_to_line_segment_v2(float r_close[2],
                                 const float p[2],
                                 const float l1[2],
                                 const float l2[2])
{
  float lambda, cp[2];

  lambda = closest_to_line_v2(cp, p, l1, l2);

  /* flip checks for !finite case (when segment is a point) */
  if (lambda <= 0.0f) {
    copy_v2_v2(r_close, l1);
    return 0.0f;
  }
  if (lambda >= 1.0f) {
    copy_v2_v2(r_close, l2);
    return 1.0f;
  }
  copy_v2_v2(r_close, cp);
  return lambda;
}

float closest_to_line_segment_v3(float r_close[3],
                                 const float p[3],
                                 const float l1[3],
                                 const float l2[3])
{
  float lambda, cp[3];

  lambda = closest_to_line_v3(cp, p, l1, l2);

  /* flip checks for !finite case (when segment is a point) */
  if (lambda <= 0.0f) {
    copy_v3_v3(r_close, l1);
    return 0.0f;
  }
  if (lambda >= 1.0f) {
    copy_v3_v3(r_close, l2);
    return 1.0f;
  }
  copy_v3_v3(r_close, cp);
  return lambda;
}

void closest_to_plane_v3(float r_close[3], const float plane[4], const float pt[3])
{
  const float len_sq = len_squared_v3(plane);
  const float side = plane_point_side_v3(plane, pt);
  madd_v3_v3v3fl(r_close, pt, plane, -side / len_sq);
}

void closest_to_plane_normalized_v3(float r_close[3], const float plane[4], const float pt[3])
{
  const float side = plane_point_side_v3(plane, pt);
  BLI_ASSERT_UNIT_V3(plane);
  madd_v3_v3v3fl(r_close, pt, plane, -side);
}

void closest_to_plane3_v3(float r_close[3], const float plane[3], const float pt[3])
{
  const float len_sq = len_squared_v3(plane);
  const float side = dot_v3v3(plane, pt);
  madd_v3_v3v3fl(r_close, pt, plane, -side / len_sq);
}

void closest_to_plane3_normalized_v3(float r_close[3], const float plane[3], const float pt[3])
{
  const float side = dot_v3v3(plane, pt);
  BLI_ASSERT_UNIT_V3(plane);
  madd_v3_v3v3fl(r_close, pt, plane, -side);
}

float dist_signed_squared_to_plane_v3(const float pt[3], const float plane[4])
{
  const float len_sq = len_squared_v3(plane);
  const float side = plane_point_side_v3(plane, pt);
  const float fac = side / len_sq;
  return copysignf(len_sq * (fac * fac), side);
}
float dist_squared_to_plane_v3(const float pt[3], const float plane[4])
{
  const float len_sq = len_squared_v3(plane);
  const float side = plane_point_side_v3(plane, pt);
  const float fac = side / len_sq;
  /* only difference to code above - no 'copysignf' */
  return len_sq * (fac * fac);
}

float dist_signed_squared_to_plane3_v3(const float pt[3], const float plane[3])
{
  const float len_sq = len_squared_v3(plane);
  const float side = dot_v3v3(plane, pt); /* only difference with 'plane[4]' version */
  const float fac = side / len_sq;
  return copysignf(len_sq * (fac * fac), side);
}
float dist_squared_to_plane3_v3(const float pt[3], const float plane[3])
{
  const float len_sq = len_squared_v3(plane);
  const float side = dot_v3v3(plane, pt); /* only difference with 'plane[4]' version */
  const float fac = side / len_sq;
  /* only difference to code above - no 'copysignf' */
  return len_sq * (fac * fac);
}

float dist_signed_to_plane_v3(const float pt[3], const float plane[4])
{
  const float len_sq = len_squared_v3(plane);
  const float side = plane_point_side_v3(plane, pt);
  const float fac = side / len_sq;
  return sqrtf(len_sq) * fac;
}
float dist_to_plane_v3(const float pt[3], const float plane[4])
{
  return fabsf(dist_signed_to_plane_v3(pt, plane));
}

float dist_signed_to_plane3_v3(const float pt[3], const float plane[3])
{
  const float len_sq = len_squared_v3(plane);
  const float side = dot_v3v3(plane, pt); /* only difference with 'plane[4]' version */
  const float fac = side / len_sq;
  return sqrtf(len_sq) * fac;
}
float dist_to_plane3_v3(const float pt[3], const float plane[3])
{
  return fabsf(dist_signed_to_plane3_v3(pt, plane));
}

float dist_squared_to_line_segment_v3(const float p[3], const float l1[3], const float l2[3])
{
  float closest[3];

  closest_to_line_segment_v3(closest, p, l1, l2);

  return len_squared_v3v3(closest, p);
}

float dist_to_line_segment_v3(const float p[3], const float l1[3], const float l2[3])
{
  return sqrtf(dist_squared_to_line_segment_v3(p, l1, l2));
}

float dist_squared_to_line_v3(const float p[3], const float l1[3], const float l2[3])
{
  float closest[3];

  closest_to_line_v3(closest, p, l1, l2);

  return len_squared_v3v3(closest, p);
}
float dist_to_line_v3(const float p[3], const float l1[3], const float l2[3])
{
  return sqrtf(dist_squared_to_line_v3(p, l1, l2));
}

float dist_signed_squared_to_corner_v3v3v3(const float p[3],
                                           const float v1[3],
                                           const float v2[3],
                                           const float v3[3],
                                           const float axis_ref[3])
{
  float dir_a[3], dir_b[3];
  float plane_a[3], plane_b[3];
  float dist_a, dist_b;
  float axis[3];
  float s_p_v2[3];
  bool flip = false;

  sub_v3_v3v3(dir_a, v1, v2);
  sub_v3_v3v3(dir_b, v3, v2);

  cross_v3_v3v3(axis, dir_a, dir_b);

  if (len_squared_v3(axis) < FLT_EPSILON) {
    copy_v3_v3(axis, axis_ref);
  }
  else if (dot_v3v3(axis, axis_ref) < 0.0f) {
    /* concave */
    flip = true;
    negate_v3(axis);
  }

  cross_v3_v3v3(plane_a, dir_a, axis);
  cross_v3_v3v3(plane_b, axis, dir_b);

#if 0
  plane_from_point_normal_v3(plane_a, v2, plane_a);
  plane_from_point_normal_v3(plane_b, v2, plane_b);

  dist_a = dist_signed_squared_to_plane_v3(p, plane_a);
  dist_b = dist_signed_squared_to_plane_v3(p, plane_b);
#else
  /* calculate without the planes 4th component to avoid float precision issues */
  sub_v3_v3v3(s_p_v2, p, v2);

  dist_a = dist_signed_squared_to_plane3_v3(s_p_v2, plane_a);
  dist_b = dist_signed_squared_to_plane3_v3(s_p_v2, plane_b);
#endif

  if (flip) {
    return min_ff(dist_a, dist_b);
  }

  return max_ff(dist_a, dist_b);
}

float dist_squared_to_ray_v3_normalized(const float ray_origin[3],
                                        const float ray_direction[3],
                                        const float co[3])
{
  float origin_to_co[3];
  sub_v3_v3v3(origin_to_co, co, ray_origin);

  float origin_to_proj[3];
  project_v3_v3v3_normalized(origin_to_proj, origin_to_co, ray_direction);

  float co_projected_on_ray[3];
  add_v3_v3v3(co_projected_on_ray, ray_origin, origin_to_proj);

  return len_squared_v3v3(co, co_projected_on_ray);
}

float dist_squared_ray_to_seg_v3(const float ray_origin[3],
                                 const float ray_direction[3],
                                 const float v0[3],
                                 const float v1[3],
                                 float r_point[3],
                                 float *r_depth)
{
  float lambda, depth;
  if (isect_ray_line_v3(ray_origin, ray_direction, v0, v1, &lambda)) {
    if (lambda <= 0.0f) {
      copy_v3_v3(r_point, v0);
    }
    else if (lambda >= 1.0f) {
      copy_v3_v3(r_point, v1);
    }
    else {
      interp_v3_v3v3(r_point, v0, v1, lambda);
    }
  }
  else {
    /* has no nearest point, only distance squared. */
    /* Calculate the distance to the point v0 then */
    copy_v3_v3(r_point, v0);
  }

  float dvec[3];
  sub_v3_v3v3(dvec, r_point, ray_origin);
  depth = dot_v3v3(dvec, ray_direction);

  if (r_depth) {
    *r_depth = depth;
  }

  return len_squared_v3(dvec) - square_f(depth);
}

void aabb_get_near_far_from_plane(const float plane_no[3],
                                  const float bbmin[3],
                                  const float bbmax[3],
                                  float bb_near[3],
                                  float bb_afar[3])
{
  if (plane_no[0] < 0.0f) {
    bb_near[0] = bbmax[0];
    bb_afar[0] = bbmin[0];
  }
  else {
    bb_near[0] = bbmin[0];
    bb_afar[0] = bbmax[0];
  }
  if (plane_no[1] < 0.0f) {
    bb_near[1] = bbmax[1];
    bb_afar[1] = bbmin[1];
  }
  else {
    bb_near[1] = bbmin[1];
    bb_afar[1] = bbmax[1];
  }
  if (plane_no[2] < 0.0f) {
    bb_near[2] = bbmax[2];
    bb_afar[2] = bbmin[2];
  }
  else {
    bb_near[2] = bbmin[2];
    bb_afar[2] = bbmax[2];
  }
}

/* -------------------------------------------------------------------- */
/** \name dist_squared_to_ray_to_aabb and helpers
 * \{ */

DistRayAABB_Precalc dist_squared_ray_to_aabb_v3_precalc(const float ray_origin[3],
                                                        const float ray_direction[3])
{
  DistRayAABB_Precalc nearest_precalc{};
  copy_v3_v3(nearest_precalc.ray_origin, ray_origin);
  copy_v3_v3(nearest_precalc.ray_direction, ray_direction);

  for (int i = 0; i < 3; i++) {
    nearest_precalc.ray_inv_dir[i] = (nearest_precalc.ray_direction[i] != 0.0f) ?
                                         (1.0f / nearest_precalc.ray_direction[i]) :
                                         FLT_MAX;
  }
  return nearest_precalc;
}

float dist_squared_ray_to_aabb_v3(const DistRayAABB_Precalc *data,
                                  const float bb_min[3],
                                  const float bb_max[3],
                                  float r_point[3],
                                  float *r_depth)
{
  // bool r_axis_closest[3];
  float local_bvmin[3], local_bvmax[3];
  aabb_get_near_far_from_plane(data->ray_direction, bb_min, bb_max, local_bvmin, local_bvmax);

  const float tmin[3] = {
      (local_bvmin[0] - data->ray_origin[0]) * data->ray_inv_dir[0],
      (local_bvmin[1] - data->ray_origin[1]) * data->ray_inv_dir[1],
      (local_bvmin[2] - data->ray_origin[2]) * data->ray_inv_dir[2],
  };
  const float tmax[3] = {
      (local_bvmax[0] - data->ray_origin[0]) * data->ray_inv_dir[0],
      (local_bvmax[1] - data->ray_origin[1]) * data->ray_inv_dir[1],
      (local_bvmax[2] - data->ray_origin[2]) * data->ray_inv_dir[2],
  };
  /* `va` and `vb` are the coordinates of the AABB edge closest to the ray */
  float va[3], vb[3];
  /* `rtmin` and `rtmax` are the minimum and maximum distances of the ray hits on the AABB */
  float rtmin, rtmax;
  int main_axis;

  if ((tmax[0] <= tmax[1]) && (tmax[0] <= tmax[2])) {
    rtmax = tmax[0];
    va[0] = vb[0] = local_bvmax[0];
    main_axis = 3;
    // r_axis_closest[0] = neasrest_precalc->ray_direction[0] < 0.0f;
  }
  else if ((tmax[1] <= tmax[0]) && (tmax[1] <= tmax[2])) {
    rtmax = tmax[1];
    va[1] = vb[1] = local_bvmax[1];
    main_axis = 2;
    // r_axis_closest[1] = neasrest_precalc->ray_direction[1] < 0.0f;
  }
  else {
    rtmax = tmax[2];
    va[2] = vb[2] = local_bvmax[2];
    main_axis = 1;
    // r_axis_closest[2] = neasrest_precalc->ray_direction[2] < 0.0f;
  }

  if ((tmin[0] >= tmin[1]) && (tmin[0] >= tmin[2])) {
    rtmin = tmin[0];
    va[0] = vb[0] = local_bvmin[0];
    main_axis -= 3;
    // r_axis_closest[0] = neasrest_precalc->ray_direction[0] >= 0.0f;
  }
  else if ((tmin[1] >= tmin[0]) && (tmin[1] >= tmin[2])) {
    rtmin = tmin[1];
    va[1] = vb[1] = local_bvmin[1];
    main_axis -= 1;
    // r_axis_closest[1] = neasrest_precalc->ray_direction[1] >= 0.0f;
  }
  else {
    rtmin = tmin[2];
    va[2] = vb[2] = local_bvmin[2];
    main_axis -= 2;
    // r_axis_closest[2] = neasrest_precalc->ray_direction[2] >= 0.0f;
  }
  if (main_axis < 0) {
    main_axis += 3;
  }

  /* if rtmin <= rtmax, ray intersect `AABB` */
  if (rtmin <= rtmax) {
    float dvec[3];
    copy_v3_v3(r_point, local_bvmax);
    sub_v3_v3v3(dvec, local_bvmax, data->ray_origin);
    *r_depth = dot_v3v3(dvec, data->ray_direction);
    return 0.0f;
  }

  if (data->ray_direction[main_axis] >= 0.0f) {
    va[main_axis] = local_bvmin[main_axis];
    vb[main_axis] = local_bvmax[main_axis];
  }
  else {
    va[main_axis] = local_bvmax[main_axis];
    vb[main_axis] = local_bvmin[main_axis];
  }

  return dist_squared_ray_to_seg_v3(
      data->ray_origin, data->ray_direction, va, vb, r_point, r_depth);
}

float dist_squared_ray_to_aabb_v3_simple(const float ray_origin[3],
                                         const float ray_direction[3],
                                         const float bb_min[3],
                                         const float bb_max[3],
                                         float r_point[3],
                                         float *r_depth)
{
  const DistRayAABB_Precalc data = dist_squared_ray_to_aabb_v3_precalc(ray_origin, ray_direction);
  return dist_squared_ray_to_aabb_v3(&data, bb_min, bb_max, r_point, r_depth);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name dist_squared_to_projected_aabb and helpers
 * \{ */

void dist_squared_to_projected_aabb_precalc(DistProjectedAABBPrecalc *precalc,
                                            const float projmat[4][4],
                                            const float winsize[2],
                                            const float mval[2])
{
  float win_half[2], relative_mval[2], px[4], py[4];

  mul_v2_v2fl(win_half, winsize, 0.5f);
  sub_v2_v2v2(precalc->mval, mval, win_half);

  relative_mval[0] = precalc->mval[0] / win_half[0];
  relative_mval[1] = precalc->mval[1] / win_half[1];

  copy_m4_m4(precalc->pmat, projmat);
  for (int i = 0; i < 4; i++) {
    px[i] = precalc->pmat[i][0] - precalc->pmat[i][3] * relative_mval[0];
    py[i] = precalc->pmat[i][1] - precalc->pmat[i][3] * relative_mval[1];

    precalc->pmat[i][0] *= win_half[0];
    precalc->pmat[i][1] *= win_half[1];
  }
#if 0
  float projmat_trans[4][4];
  transpose_m4_m4(projmat_trans, projmat);
  if (!isect_plane_plane_plane_v3(
          projmat_trans[0], projmat_trans[1], projmat_trans[3], precalc->ray_origin))
  {
    /* Orthographic projection. */
    isect_plane_plane_v3(px, py, precalc->ray_origin, precalc->ray_direction);
  }
  else {
    /* Perspective projection. */
    cross_v3_v3v3(precalc->ray_direction, py, px);
    // normalize_v3(precalc->ray_direction);
  }
#else
  if (!isect_plane_plane_v3(px, py, precalc->ray_origin, precalc->ray_direction)) {
    /* Matrix with weird co-planar planes. Undetermined origin. */
    zero_v3(precalc->ray_origin);
    precalc->ray_direction[0] = precalc->pmat[0][3];
    precalc->ray_direction[1] = precalc->pmat[1][3];
    precalc->ray_direction[2] = precalc->pmat[2][3];
  }
#endif

  for (int i = 0; i < 3; i++) {
    precalc->ray_inv_dir[i] = (precalc->ray_direction[i] != 0.0f) ?
                                  (1.0f / precalc->ray_direction[i]) :
                                  FLT_MAX;
  }
}

float dist_squared_to_projected_aabb(DistProjectedAABBPrecalc *data,
                                     const float bbmin[3],
                                     const float bbmax[3],
                                     bool r_axis_closest[3])
{
  float local_bvmin[3], local_bvmax[3];
  aabb_get_near_far_from_plane(data->ray_direction, bbmin, bbmax, local_bvmin, local_bvmax);

  const float tmin[3] = {
      (local_bvmin[0] - data->ray_origin[0]) * data->ray_inv_dir[0],
      (local_bvmin[1] - data->ray_origin[1]) * data->ray_inv_dir[1],
      (local_bvmin[2] - data->ray_origin[2]) * data->ray_inv_dir[2],
  };
  const float tmax[3] = {
      (local_bvmax[0] - data->ray_origin[0]) * data->ray_inv_dir[0],
      (local_bvmax[1] - data->ray_origin[1]) * data->ray_inv_dir[1],
      (local_bvmax[2] - data->ray_origin[2]) * data->ray_inv_dir[2],
  };
  /* `va` and `vb` are the coordinates of the AABB edge closest to the ray */
  float va[3], vb[3];
  /* `rtmin` and `rtmax` are the minimum and maximum distances of the ray hits on the AABB */
  float rtmin, rtmax;
  int main_axis;

  r_axis_closest[0] = false;
  r_axis_closest[1] = false;
  r_axis_closest[2] = false;

  if ((tmax[0] <= tmax[1]) && (tmax[0] <= tmax[2])) {
    rtmax = tmax[0];
    va[0] = vb[0] = local_bvmax[0];
    main_axis = 3;
    r_axis_closest[0] = data->ray_direction[0] < 0.0f;
  }
  else if ((tmax[1] <= tmax[0]) && (tmax[1] <= tmax[2])) {
    rtmax = tmax[1];
    va[1] = vb[1] = local_bvmax[1];
    main_axis = 2;
    r_axis_closest[1] = data->ray_direction[1] < 0.0f;
  }
  else {
    rtmax = tmax[2];
    va[2] = vb[2] = local_bvmax[2];
    main_axis = 1;
    r_axis_closest[2] = data->ray_direction[2] < 0.0f;
  }

  if ((tmin[0] >= tmin[1]) && (tmin[0] >= tmin[2])) {
    rtmin = tmin[0];
    va[0] = vb[0] = local_bvmin[0];
    main_axis -= 3;
    r_axis_closest[0] = data->ray_direction[0] >= 0.0f;
  }
  else if ((tmin[1] >= tmin[0]) && (tmin[1] >= tmin[2])) {
    rtmin = tmin[1];
    va[1] = vb[1] = local_bvmin[1];
    main_axis -= 1;
    r_axis_closest[1] = data->ray_direction[1] >= 0.0f;
  }
  else {
    rtmin = tmin[2];
    va[2] = vb[2] = local_bvmin[2];
    main_axis -= 2;
    r_axis_closest[2] = data->ray_direction[2] >= 0.0f;
  }
  if (main_axis < 0) {
    main_axis += 3;
  }

  /* if rtmin <= rtmax, ray intersect `AABB` */
  if (rtmin <= rtmax) {
    return 0;
  }

  if (data->ray_direction[main_axis] >= 0.0f) {
    va[main_axis] = local_bvmin[main_axis];
    vb[main_axis] = local_bvmax[main_axis];
  }
  else {
    va[main_axis] = local_bvmax[main_axis];
    vb[main_axis] = local_bvmin[main_axis];
  }
  float scale = fabsf(local_bvmax[main_axis] - local_bvmin[main_axis]);

  float va2d[2] = {
      (dot_m4_v3_row_x(data->pmat, va) + data->pmat[3][0]),
      (dot_m4_v3_row_y(data->pmat, va) + data->pmat[3][1]),
  };
  float vb2d[2] = {
      (va2d[0] + data->pmat[main_axis][0] * scale),
      (va2d[1] + data->pmat[main_axis][1] * scale),
  };

  float w_a = mul_project_m4_v3_zfac(data->pmat, va);
  if (w_a != 1.0f) {
    /* Perspective Projection. */
    float w_b = w_a + data->pmat[main_axis][3] * scale;
    va2d[0] /= w_a;
    va2d[1] /= w_a;
    vb2d[0] /= w_b;
    vb2d[1] /= w_b;
  }

  float dvec[2], edge[2], lambda, rdist_sq;
  sub_v2_v2v2(dvec, data->mval, va2d);
  sub_v2_v2v2(edge, vb2d, va2d);
  lambda = dot_v2v2(dvec, edge);
  if (lambda != 0.0f) {
    lambda /= len_squared_v2(edge);
    if (lambda <= 0.0f) {
      rdist_sq = len_squared_v2v2(data->mval, va2d);
      r_axis_closest[main_axis] = true;
    }
    else if (lambda >= 1.0f) {
      rdist_sq = len_squared_v2v2(data->mval, vb2d);
      r_axis_closest[main_axis] = false;
    }
    else {
      madd_v2_v2fl(va2d, edge, lambda);
      rdist_sq = len_squared_v2v2(data->mval, va2d);
      r_axis_closest[main_axis] = lambda < 0.5f;
    }
  }
  else {
    rdist_sq = len_squared_v2v2(data->mval, va2d);
  }

  return rdist_sq;
}

float dist_squared_to_projected_aabb_simple(const float projmat[4][4],
                                            const float winsize[2],
                                            const float mval[2],
                                            const float bbmin[3],
                                            const float bbmax[3])
{
  DistProjectedAABBPrecalc data;
  dist_squared_to_projected_aabb_precalc(&data, projmat, winsize, mval);

  bool dummy[3] = {true, true, true};
  return dist_squared_to_projected_aabb(&data, bbmin, bbmax, dummy);
}

/** \} */

float dist_seg_seg_v2(const float a1[3], const float a2[3], const float b1[3], const float b2[3])
{
  if (isect_seg_seg_v2_simple(a1, a2, b1, b2)) {
    return 0.0f;
  }
  const float d1 = dist_squared_to_line_segment_v2(a1, b1, b2);
  const float d2 = dist_squared_to_line_segment_v2(a2, b1, b2);
  const float d3 = dist_squared_to_line_segment_v2(b1, a1, a2);
  const float d4 = dist_squared_to_line_segment_v2(b2, a1, a2);
  return sqrtf(min_ffff(d1, d2, d3, d4));
}

void closest_on_tri_to_point_v3(
    float r[3], const float p[3], const float v1[3], const float v2[3], const float v3[3])
{
  /* Adapted from "Real-Time Collision Detection" by Christer Ericson,
   * published by Morgan Kaufmann Publishers, copyright 2005 Elsevier Inc. */

  float ab[3], ac[3], ap[3], d1, d2;
  float bp[3], d3, d4, vc, cp[3], d5, d6, vb, va;
  float denom, v, w;

  /* Check if P in vertex region outside A */
  sub_v3_v3v3(ab, v2, v1);
  sub_v3_v3v3(ac, v3, v1);
  sub_v3_v3v3(ap, p, v1);
  d1 = dot_v3v3(ab, ap);
  d2 = dot_v3v3(ac, ap);
  if (d1 <= 0.0f && d2 <= 0.0f) {
    /* barycentric coordinates (1,0,0) */
    copy_v3_v3(r, v1);
    return;
  }

  /* Check if P in vertex region outside B */
  sub_v3_v3v3(bp, p, v2);
  d3 = dot_v3v3(ab, bp);
  d4 = dot_v3v3(ac, bp);
  if (d3 >= 0.0f && d4 <= d3) {
    /* barycentric coordinates (0,1,0) */
    copy_v3_v3(r, v2);
    return;
  }
  /* Check if P in edge region of AB, if so return projection of P onto AB */
  vc = d1 * d4 - d3 * d2;
  if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
    v = d1 / (d1 - d3);
    /* barycentric coordinates (1-v,v,0) */
    madd_v3_v3v3fl(r, v1, ab, v);
    return;
  }
  /* Check if P in vertex region outside C */
  sub_v3_v3v3(cp, p, v3);
  d5 = dot_v3v3(ab, cp);
  d6 = dot_v3v3(ac, cp);
  if (d6 >= 0.0f && d5 <= d6) {
    /* barycentric coordinates (0,0,1) */
    copy_v3_v3(r, v3);
    return;
  }
  /* Check if P in edge region of AC, if so return projection of P onto AC */
  vb = d5 * d2 - d1 * d6;
  if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
    w = d2 / (d2 - d6);
    /* barycentric coordinates (1-w,0,w) */
    madd_v3_v3v3fl(r, v1, ac, w);
    return;
  }
  /* Check if P in edge region of BC, if so return projection of P onto BC */
  va = d3 * d6 - d5 * d4;
  if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
    w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    /* barycentric coordinates (0,1-w,w) */
    sub_v3_v3v3(r, v3, v2);
    mul_v3_fl(r, w);
    add_v3_v3(r, v2);
    return;
  }

  /* P inside face region. Compute Q through its barycentric coordinates (u,v,w) */
  denom = 1.0f / (va + vb + vc);
  v = vb * denom;
  w = vc * denom;

  /* = u*a + v*b + w*c, u = va * denom = 1.0f - v - w */
  /* ac * w */
  mul_v3_fl(ac, w);
  /* a + ab * v */
  madd_v3_v3v3fl(r, v1, ab, v);
  /* a + ab * v + ac * w */
  add_v3_v3(r, ac);
}

/******************************* Intersection ********************************/

int isect_seg_seg_v2_int(const int v1[2], const int v2[2], const int v3[2], const int v4[2])
{
  float div, lambda, mu;

  div = float((v2[0] - v1[0]) * (v4[1] - v3[1]) - (v2[1] - v1[1]) * (v4[0] - v3[0]));
  if (div == 0.0f) {
    return ISECT_LINE_LINE_COLINEAR;
  }

  lambda = float((v1[1] - v3[1]) * (v4[0] - v3[0]) - (v1[0] - v3[0]) * (v4[1] - v3[1])) / div;

  mu = float((v1[1] - v3[1]) * (v2[0] - v1[0]) - (v1[0] - v3[0]) * (v2[1] - v1[1])) / div;

  if (lambda >= 0.0f && lambda <= 1.0f && mu >= 0.0f && mu <= 1.0f) {
    if (lambda == 0.0f || lambda == 1.0f || mu == 0.0f || mu == 1.0f) {
      return ISECT_LINE_LINE_EXACT;
    }
    return ISECT_LINE_LINE_CROSS;
  }
  return ISECT_LINE_LINE_NONE;
}

int isect_line_line_v2_point(
    const float v0[2], const float v1[2], const float v2[2], const float v3[2], float r_vi[2])
{
  float s10[2], s32[2];
  float div;

  sub_v2_v2v2(s10, v1, v0);
  sub_v2_v2v2(s32, v3, v2);

  div = cross_v2v2(s10, s32);
  if (div != 0.0f) {
    const float u = cross_v2v2(v1, v0);
    const float v = cross_v2v2(v3, v2);

    r_vi[0] = ((s32[0] * u) - (s10[0] * v)) / div;
    r_vi[1] = ((s32[1] * u) - (s10[1] * v)) / div;

    return ISECT_LINE_LINE_CROSS;
  }

  return ISECT_LINE_LINE_COLINEAR;
}

int isect_seg_seg_v2(const float v1[2], const float v2[2], const float v3[2], const float v4[2])
{
  float div, lambda, mu;

  div = (v2[0] - v1[0]) * (v4[1] - v3[1]) - (v2[1] - v1[1]) * (v4[0] - v3[0]);
  if (div == 0.0f) {
    return ISECT_LINE_LINE_COLINEAR;
  }

  lambda = (float(v1[1] - v3[1]) * (v4[0] - v3[0]) - (v1[0] - v3[0]) * (v4[1] - v3[1])) / div;

  mu = (float(v1[1] - v3[1]) * (v2[0] - v1[0]) - (v1[0] - v3[0]) * (v2[1] - v1[1])) / div;

  if (lambda >= 0.0f && lambda <= 1.0f && mu >= 0.0f && mu <= 1.0f) {
    if (lambda == 0.0f || lambda == 1.0f || mu == 0.0f || mu == 1.0f) {
      return ISECT_LINE_LINE_EXACT;
    }
    return ISECT_LINE_LINE_CROSS;
  }
  return ISECT_LINE_LINE_NONE;
}

void isect_seg_seg_v3(const float a0[3],
                      const float a1[3],
                      const float b0[3],
                      const float b1[3],
                      float r_a[3],
                      float r_b[3])
{
  float fac_a, fac_b;
  float a_dir[3], b_dir[3], a0b0[3], crs_ab[3];
  sub_v3_v3v3(a_dir, a1, a0);
  sub_v3_v3v3(b_dir, b1, b0);
  sub_v3_v3v3(a0b0, b0, a0);
  cross_v3_v3v3(crs_ab, b_dir, a_dir);
  const float nlen = len_squared_v3(crs_ab);

  if (nlen == 0.0f) {
    /* Parallel Lines */
    /* In this case return any point that
     * is between the closest segments. */
    float a0b1[3], a1b0[3], len_a, len_b, fac1, fac2;
    sub_v3_v3v3(a0b1, b1, a0);
    sub_v3_v3v3(a1b0, b0, a1);
    len_a = len_squared_v3(a_dir);
    len_b = len_squared_v3(b_dir);

    if (len_a) {
      fac1 = dot_v3v3(a0b0, a_dir);
      fac2 = dot_v3v3(a0b1, a_dir);
      CLAMP(fac1, 0.0f, len_a);
      CLAMP(fac2, 0.0f, len_a);
      fac_a = (fac1 + fac2) / (2 * len_a);
    }
    else {
      fac_a = 0.0f;
    }

    if (len_b) {
      fac1 = -dot_v3v3(a0b0, b_dir);
      fac2 = -dot_v3v3(a1b0, b_dir);
      CLAMP(fac1, 0.0f, len_b);
      CLAMP(fac2, 0.0f, len_b);
      fac_b = (fac1 + fac2) / (2 * len_b);
    }
    else {
      fac_b = 0.0f;
    }
  }
  else {
    float c[3], cray[3];
    sub_v3_v3v3(c, crs_ab, a0b0);

    cross_v3_v3v3(cray, c, b_dir);
    fac_a = dot_v3v3(cray, crs_ab) / nlen;

    cross_v3_v3v3(cray, c, a_dir);
    fac_b = dot_v3v3(cray, crs_ab) / nlen;

    CLAMP(fac_a, 0.0f, 1.0f);
    CLAMP(fac_b, 0.0f, 1.0f);
  }

  madd_v3_v3v3fl(r_a, a0, a_dir, fac_a);
  madd_v3_v3v3fl(r_b, b0, b_dir, fac_b);
}

int isect_seg_seg_v2_point_ex(const float v0[2],
                              const float v1[2],
                              const float v2[2],
                              const float v3[2],
                              const float endpoint_bias,
                              float r_vi[2])
{
  float s10[2], s32[2], s30[2], d;
  const float eps = 1e-6f;
  const float endpoint_min = -endpoint_bias;
  const float endpoint_max = endpoint_bias + 1.0f;

  sub_v2_v2v2(s10, v1, v0);
  sub_v2_v2v2(s32, v3, v2);
  sub_v2_v2v2(s30, v3, v0);

  d = cross_v2v2(s10, s32);

  if (d != 0) {
    float u, v;

    u = cross_v2v2(s30, s32) / d;
    v = cross_v2v2(s10, s30) / d;

    if ((u >= endpoint_min && u <= endpoint_max) && (v >= endpoint_min && v <= endpoint_max)) {
      /* intersection */
      float vi_test[2];
      float s_vi_v2[2];

      madd_v2_v2v2fl(vi_test, v0, s10, u);

      /* When 'd' approaches zero, float precision lets non-overlapping co-linear segments
       * detect as an intersection. So re-calculate 'v' to ensure the point overlaps both.
       * see #45123 */

      /* inline since we have most vars already */
#if 0
      v = line_point_factor_v2(ix_test, v2, v3);
#else
      sub_v2_v2v2(s_vi_v2, vi_test, v2);
      v = (dot_v2v2(s32, s_vi_v2) / dot_v2v2(s32, s32));
#endif
      if (v >= endpoint_min && v <= endpoint_max) {
        copy_v2_v2(r_vi, vi_test);
        return 1;
      }
    }

    /* out of segment intersection */
    return -1;
  }

  if ((cross_v2v2(s10, s30) == 0.0f) && (cross_v2v2(s32, s30) == 0.0f)) {
    /* equal lines */
    float s20[2];
    float u_a, u_b;

    if (equals_v2v2(v0, v1)) {
      if (len_squared_v2v2(v2, v3) > square_f(eps)) {
        /* use non-point segment as basis */
        std::swap(v0, v2);
        std::swap(v1, v3);

        sub_v2_v2v2(s10, v1, v0);
        sub_v2_v2v2(s30, v3, v0);
      }
      else {                       /* both of segments are points */
        if (equals_v2v2(v0, v2)) { /* points are equal */
          copy_v2_v2(r_vi, v0);
          return 1;
        }

        /* two different points */
        return -1;
      }
    }

    sub_v2_v2v2(s20, v2, v0);

    u_a = dot_v2v2(s20, s10) / dot_v2v2(s10, s10);
    u_b = dot_v2v2(s30, s10) / dot_v2v2(s10, s10);

    if (u_a > u_b) {
      std::swap(u_a, u_b);
    }

    if (u_a > endpoint_max || u_b < endpoint_min) {
      /* non-overlapping segments */
      return -1;
    }
    if (max_ff(0.0f, u_a) == min_ff(1.0f, u_b)) {
      /* one common point: can return result */
      madd_v2_v2v2fl(r_vi, v0, s10, max_ff(0, u_a));
      return 1;
    }
  }

  /* lines are collinear */
  return -1;
}

int isect_seg_seg_v2_point(
    const float v0[2], const float v1[2], const float v2[2], const float v3[2], float r_vi[2])
{
  const float endpoint_bias = 1e-6f;
  return isect_seg_seg_v2_point_ex(v0, v1, v2, v3, endpoint_bias, r_vi);
}

bool isect_seg_seg_v2_simple(const float v1[2],
                             const float v2[2],
                             const float v3[2],
                             const float v4[2])
{
#define CCW(A, B, C) ((C[1] - A[1]) * (B[0] - A[0]) > (B[1] - A[1]) * (C[0] - A[0]))

  return CCW(v1, v3, v4) != CCW(v2, v3, v4) && CCW(v1, v2, v3) != CCW(v1, v2, v4);

#undef CCW
}

int isect_seg_seg_v2_lambda_mu_db(const double v1[2],
                                  const double v2[2],
                                  const double v3[2],
                                  const double v4[2],
                                  double *r_lambda,
                                  double *r_mu)
{
  double div, lambda, mu;

  div = (v2[0] - v1[0]) * (v4[1] - v3[1]) - (v2[1] - v1[1]) * (v4[0] - v3[0]);
  if (fabs(div) < DBL_EPSILON) {
    return ISECT_LINE_LINE_COLINEAR;
  }

  lambda = ((v1[1] - v3[1]) * (v4[0] - v3[0]) - (v1[0] - v3[0]) * (v4[1] - v3[1])) / div;

  mu = ((v1[1] - v3[1]) * (v2[0] - v1[0]) - (v1[0] - v3[0]) * (v2[1] - v1[1])) / div;

  if (r_lambda) {
    *r_lambda = lambda;
  }
  if (r_mu) {
    *r_mu = mu;
  }

  if (lambda >= 0.0 && lambda <= 1.0 && mu >= 0.0 && mu <= 1.0) {
    if (lambda == 0.0 || lambda == 1.0 || mu == 0.0 || mu == 1.0) {
      return ISECT_LINE_LINE_EXACT;
    }
    return ISECT_LINE_LINE_CROSS;
  }
  return ISECT_LINE_LINE_NONE;
}

int isect_line_sphere_v3(const float l1[3],
                         const float l2[3],
                         const float sp[3],
                         const float r,
                         float r_p1[3],
                         float r_p2[3])
{
  /* Adapted for use in blender by Campbell Barton, 2011.
   *
   * http://www.iebele.nl
   * `Atelier Iebele Abel <atelier@iebele.nl>` - 2001.
   *
   * sphere_line_intersection function adapted from:
   * http://astronomy.swin.edu.au/pbourke/geometry/sphereline
   * `Paul Bourke <pbourke@swin.edu.au>`. */

  const float ldir[3] = {
      l2[0] - l1[0],
      l2[1] - l1[1],
      l2[2] - l1[2],
  };

  const float a = len_squared_v3(ldir);

  const float b = 2.0f * (ldir[0] * (l1[0] - sp[0]) + ldir[1] * (l1[1] - sp[1]) +
                          ldir[2] * (l1[2] - sp[2]));

  const float c = len_squared_v3(sp) + len_squared_v3(l1) - (2.0f * dot_v3v3(sp, l1)) - (r * r);

  const float i = b * b - 4.0f * a * c;

  float mu;

  if (i < 0.0f) {
    /* no intersections */
    return 0;
  }
  if (i == 0.0f) {
    /* one intersection */
    mu = -b / (2.0f * a);
    madd_v3_v3v3fl(r_p1, l1, ldir, mu);
    return 1;
  }
  if (i > 0.0f) {
    const float i_sqrt = sqrtf(i); /* avoid calc twice */

    /* first intersection */
    mu = (-b + i_sqrt) / (2.0f * a);
    madd_v3_v3v3fl(r_p1, l1, ldir, mu);

    /* second intersection */
    mu = (-b - i_sqrt) / (2.0f * a);
    madd_v3_v3v3fl(r_p2, l1, ldir, mu);
    return 2;
  }

  /* math domain error - nan */
  return -1;
}

int isect_line_sphere_v2(const float l1[2],
                         const float l2[2],
                         const float sp[2],
                         const float r,
                         float r_p1[2],
                         float r_p2[2])
{
  /* Keep in sync with #isect_line_sphere_v3. */

  const float ldir[2] = {l2[0] - l1[0], l2[1] - l1[1]};

  const float a = dot_v2v2(ldir, ldir);

  const float b = 2.0f * (ldir[0] * (l1[0] - sp[0]) + ldir[1] * (l1[1] - sp[1]));

  const float c = dot_v2v2(sp, sp) + dot_v2v2(l1, l1) - (2.0f * dot_v2v2(sp, l1)) - (r * r);

  const float i = b * b - 4.0f * a * c;

  float mu;

  if (i < 0.0f) {
    /* no intersections */
    return 0;
  }
  if (i == 0.0f) {
    /* one intersection */
    mu = -b / (2.0f * a);
    madd_v2_v2v2fl(r_p1, l1, ldir, mu);
    return 1;
  }
  if (i > 0.0f) {
    const float i_sqrt = sqrtf(i); /* avoid calc twice */

    /* first intersection */
    mu = (-b + i_sqrt) / (2.0f * a);
    madd_v2_v2v2fl(r_p1, l1, ldir, mu);

    /* second intersection */
    mu = (-b - i_sqrt) / (2.0f * a);
    madd_v2_v2v2fl(r_p2, l1, ldir, mu);
    return 2;
  }

  /* math domain error - nan */
  return -1;
}

bool isect_point_poly_v2(const float pt[2], const float verts[][2], const uint nr)
{
  /* Keep in sync with #isect_point_poly_v2_int. */

  uint i, j;
  bool isect = false;
  for (i = 0, j = nr - 1; i < nr; j = i++) {
    if (((verts[i][1] > pt[1]) != (verts[j][1] > pt[1])) &&
        (pt[0] <
         (verts[j][0] - verts[i][0]) * (pt[1] - verts[i][1]) / (verts[j][1] - verts[i][1]) +
             verts[i][0]))
    {
      isect = !isect;
    }
  }
  return isect;
}
bool isect_point_poly_v2_int(const int pt[2], const int verts[][2], const uint nr)
{
  /* Keep in sync with #isect_point_poly_v2. */

  uint i, j;
  bool isect = false;
  for (i = 0, j = nr - 1; i < nr; j = i++) {
    if (((verts[i][1] > pt[1]) != (verts[j][1] > pt[1])) &&
        (pt[0] <
         (verts[j][0] - verts[i][0]) * (pt[1] - verts[i][1]) / (verts[j][1] - verts[i][1]) +
             verts[i][0]))
    {
      isect = !isect;
    }
  }
  return isect;
}

/* point in tri */

bool isect_point_tri_v2_cw(const float pt[2],
                           const float v1[2],
                           const float v2[2],
                           const float v3[2])
{
  if (line_point_side_v2(v1, v2, pt) >= 0.0f) {
    if (line_point_side_v2(v2, v3, pt) >= 0.0f) {
      if (line_point_side_v2(v3, v1, pt) >= 0.0f) {
        return true;
      }
    }
  }

  return false;
}

int isect_point_tri_v2(const float pt[2], const float v1[2], const float v2[2], const float v3[2])
{
  float side12 = line_point_side_v2(v1, v2, pt);
  float side23 = line_point_side_v2(v2, v3, pt);
  float side31 = line_point_side_v2(v3, v1, pt);
  if (side12 >= 0.0f && side23 >= 0.0f && side31 >= 0.0f) {
    return 1;
  }
  if (side12 <= 0.0f && side23 <= 0.0f && side31 <= 0.0f) {
    return -1;
  }

  return 0;
}

int isect_point_quad_v2(
    const float pt[2], const float v1[2], const float v2[2], const float v3[2], const float v4[2])
{
  float side12 = line_point_side_v2(v1, v2, pt);
  float side23 = line_point_side_v2(v2, v3, pt);
  float side34 = line_point_side_v2(v3, v4, pt);
  float side41 = line_point_side_v2(v4, v1, pt);
  if (side12 >= 0.0f && side23 >= 0.0f && side34 >= 0.0f && side41 >= 0.0f) {
    return 1;
  }
  if (side12 <= 0.0f && side23 <= 0.0f && side34 <= 0.0f && side41 <= 0.0f) {
    return -1;
  }
  return 0;
}

bool isect_line_segment_tri_v3(const float p1[3],
                               const float p2[3],
                               const float v0[3],
                               const float v1[3],
                               const float v2[3],
                               float *r_lambda,
                               float r_uv[2])
{

  float p[3], s[3], d[3], e1[3], e2[3], q[3];
  float a, f, u, v;

  sub_v3_v3v3(e1, v1, v0);
  sub_v3_v3v3(e2, v2, v0);
  sub_v3_v3v3(d, p2, p1);

  cross_v3_v3v3(p, d, e2);
  a = dot_v3v3(e1, p);
  if (a == 0.0f) {
    return false;
  }
  f = 1.0f / a;

  sub_v3_v3v3(s, p1, v0);

  u = f * dot_v3v3(s, p);
  if ((u < 0.0f) || (u > 1.0f)) {
    return false;
  }

  cross_v3_v3v3(q, s, e1);

  v = f * dot_v3v3(d, q);
  if ((v < 0.0f) || ((u + v) > 1.0f)) {
    return false;
  }

  *r_lambda = f * dot_v3v3(e2, q);
  if ((*r_lambda < 0.0f) || (*r_lambda > 1.0f)) {
    return false;
  }

  if (r_uv) {
    r_uv[0] = u;
    r_uv[1] = v;
  }

  return true;
}

bool isect_line_segment_tri_epsilon_v3(const float p1[3],
                                       const float p2[3],
                                       const float v0[3],
                                       const float v1[3],
                                       const float v2[3],
                                       float *r_lambda,
                                       float r_uv[2],
                                       const float epsilon)
{

  float p[3], s[3], d[3], e1[3], e2[3], q[3];
  float a, f, u, v;

  sub_v3_v3v3(e1, v1, v0);
  sub_v3_v3v3(e2, v2, v0);
  sub_v3_v3v3(d, p2, p1);

  cross_v3_v3v3(p, d, e2);
  a = dot_v3v3(e1, p);
  if (a == 0.0f) {
    return false;
  }
  f = 1.0f / a;

  sub_v3_v3v3(s, p1, v0);

  u = f * dot_v3v3(s, p);
  if ((u < -epsilon) || (u > 1.0f + epsilon)) {
    return false;
  }

  cross_v3_v3v3(q, s, e1);

  v = f * dot_v3v3(d, q);
  if ((v < -epsilon) || ((u + v) > 1.0f + epsilon)) {
    return false;
  }

  *r_lambda = f * dot_v3v3(e2, q);
  if ((*r_lambda < 0.0f) || (*r_lambda > 1.0f)) {
    return false;
  }

  if (r_uv) {
    r_uv[0] = u;
    r_uv[1] = v;
  }

  return true;
}

bool isect_ray_tri_v3(const float ray_origin[3],
                      const float ray_direction[3],
                      const float v0[3],
                      const float v1[3],
                      const float v2[3],
                      float *r_lambda,
                      float r_uv[2])
{
  /* NOTE(@ideasman42): these values were 0.000001 in 2.4x but for projection snapping on
   * a human head `(1BU == 1m)`, subdivision-surface level 2, this gave many errors. */
  const float epsilon = 0.00000001f;
  float p[3], s[3], e1[3], e2[3], q[3];
  float a, f, u, v;

  sub_v3_v3v3(e1, v1, v0);
  sub_v3_v3v3(e2, v2, v0);

  cross_v3_v3v3(p, ray_direction, e2);
  a = dot_v3v3(e1, p);
  if ((a > -epsilon) && (a < epsilon)) {
    return false;
  }
  f = 1.0f / a;

  sub_v3_v3v3(s, ray_origin, v0);

  u = f * dot_v3v3(s, p);
  if ((u < 0.0f) || (u > 1.0f)) {
    return false;
  }

  cross_v3_v3v3(q, s, e1);

  v = f * dot_v3v3(ray_direction, q);
  if ((v < 0.0f) || ((u + v) > 1.0f)) {
    return false;
  }

  *r_lambda = f * dot_v3v3(e2, q);
  if (*r_lambda < 0.0f) {
    return false;
  }

  if (r_uv) {
    r_uv[0] = u;
    r_uv[1] = v;
  }

  return true;
}

bool isect_ray_plane_v3_factor(const float ray_origin[3],
                               const float ray_direction[3],
                               const float plane_co[3],
                               const float plane_no[3],
                               float *r_lambda)
{
  float h[3];
  float dot = dot_v3v3(plane_no, ray_direction);
  if (dot == 0.0f) {
    return false;
  }
  sub_v3_v3v3(h, ray_origin, plane_co);
  *r_lambda = -dot_v3v3(plane_no, h) / dot;
  return true;
}

bool isect_ray_plane_v3(const float ray_origin[3],
                        const float ray_direction[3],
                        const float plane[4],
                        float *r_lambda,
                        const bool clip)
{
  float plane_co[3], plane_no[3];
  plane_to_point_vector_v3(plane, plane_co, plane_no);
  if (!isect_ray_plane_v3_factor(ray_origin, ray_direction, plane_co, plane_no, r_lambda)) {
    return false;
  }
  if (clip && (*r_lambda < 0.0f)) {
    return false;
  }
  return true;
}

bool isect_ray_tri_epsilon_v3(const float ray_origin[3],
                              const float ray_direction[3],
                              const float v0[3],
                              const float v1[3],
                              const float v2[3],
                              float *r_lambda,
                              float r_uv[2],
                              const float epsilon)
{
  float p[3], s[3], e1[3], e2[3], q[3];
  float a, f, u, v;

  sub_v3_v3v3(e1, v1, v0);
  sub_v3_v3v3(e2, v2, v0);

  cross_v3_v3v3(p, ray_direction, e2);
  a = dot_v3v3(e1, p);
  if (a == 0.0f) {
    return false;
  }
  f = 1.0f / a;

  sub_v3_v3v3(s, ray_origin, v0);

  u = f * dot_v3v3(s, p);
  if ((u < -epsilon) || (u > 1.0f + epsilon)) {
    return false;
  }

  cross_v3_v3v3(q, s, e1);

  v = f * dot_v3v3(ray_direction, q);
  if ((v < -epsilon) || ((u + v) > 1.0f + epsilon)) {
    return false;
  }

  *r_lambda = f * dot_v3v3(e2, q);
  if (*r_lambda < 0.0f) {
    return false;
  }

  if (r_uv) {
    r_uv[0] = u;
    r_uv[1] = v;
  }

  return true;
}

void isect_ray_tri_watertight_v3_precalc(IsectRayPrecalc *isect_precalc,
                                         const float ray_direction[3])
{
  float inv_dir_z;

  /* Calculate dimension where the ray direction is maximal. */
  int kz = axis_dominant_v3_single(ray_direction);
  int kx = (kz != 2) ? (kz + 1) : 0;
  int ky = (kx != 2) ? (kx + 1) : 0;

  /* Swap kx and ky dimensions to preserve winding direction of triangles. */
  if (ray_direction[kz] < 0.0f) {
    std::swap(kx, ky);
  }

  /* Calculate the shear constants. */
  inv_dir_z = 1.0f / ray_direction[kz];
  isect_precalc->sx = ray_direction[kx] * inv_dir_z;
  isect_precalc->sy = ray_direction[ky] * inv_dir_z;
  isect_precalc->sz = inv_dir_z;

  /* Store the dimensions. */
  isect_precalc->kx = kx;
  isect_precalc->ky = ky;
  isect_precalc->kz = kz;
}

bool isect_ray_tri_watertight_v3(const float ray_origin[3],
                                 const IsectRayPrecalc *isect_precalc,
                                 const float v0[3],
                                 const float v1[3],
                                 const float v2[3],
                                 float *r_lambda,
                                 float r_uv[2])
{
  const int kx = isect_precalc->kx;
  const int ky = isect_precalc->ky;
  const int kz = isect_precalc->kz;
  const float sx = isect_precalc->sx;
  const float sy = isect_precalc->sy;
  const float sz = isect_precalc->sz;

  /* Calculate vertices relative to ray origin. */
  const float a[3] = {v0[0] - ray_origin[0], v0[1] - ray_origin[1], v0[2] - ray_origin[2]};
  const float b[3] = {v1[0] - ray_origin[0], v1[1] - ray_origin[1], v1[2] - ray_origin[2]};
  const float c[3] = {v2[0] - ray_origin[0], v2[1] - ray_origin[1], v2[2] - ray_origin[2]};

  const float a_kx = a[kx], a_ky = a[ky], a_kz = a[kz];
  const float b_kx = b[kx], b_ky = b[ky], b_kz = b[kz];
  const float c_kx = c[kx], c_ky = c[ky], c_kz = c[kz];

  /* Perform shear and scale of vertices. */
  const float ax = a_kx - sx * a_kz;
  const float ay = a_ky - sy * a_kz;
  const float bx = b_kx - sx * b_kz;
  const float by = b_ky - sy * b_kz;
  const float cx = c_kx - sx * c_kz;
  const float cy = c_ky - sy * c_kz;

  /* Calculate scaled barycentric coordinates. */
  const float u = cx * by - cy * bx;
  const float v = ax * cy - ay * cx;
  const float w = bx * ay - by * ax;
  float det;

  if ((u < 0.0f || v < 0.0f || w < 0.0f) && (u > 0.0f || v > 0.0f || w > 0.0f)) {
    return false;
  }

  /* Calculate determinant. */
  det = u + v + w;
  if (UNLIKELY(det == 0.0f || !isfinite(det))) {
    return false;
  }

  /* Calculate scaled z-coordinates of vertices and use them to calculate
   * the hit distance.
   */
  const int sign_det = (float_as_int(det) & int(0x80000000));
  const float t = (u * a_kz + v * b_kz + w * c_kz) * sz;
  const float sign_t = xor_fl(t, sign_det);
  if ((sign_t < 0.0f)
  /* Differ from Cycles, don't read r_lambda's original value
   * otherwise we won't match any of the other intersect functions here...
   * which would be confusing. */
#if 0
      || (sign_T > *r_lambda * xor_signmask(det, sign_mask))
#endif
  )
  {
    return false;
  }

  /* Normalize u, v and t. */
  const float inv_det = 1.0f / det;
  if (r_uv) {
    r_uv[0] = u * inv_det;
    r_uv[1] = v * inv_det;
  }
  *r_lambda = t * inv_det;
  return true;
}

bool isect_ray_tri_watertight_v3_simple(const float ray_origin[3],
                                        const float ray_direction[3],
                                        const float v0[3],
                                        const float v1[3],
                                        const float v2[3],
                                        float *r_lambda,
                                        float r_uv[2])
{
  IsectRayPrecalc isect_precalc;
  isect_ray_tri_watertight_v3_precalc(&isect_precalc, ray_direction);
  return isect_ray_tri_watertight_v3(ray_origin, &isect_precalc, v0, v1, v2, r_lambda, r_uv);
}

#if 0 /* UNUSED */
/**
 * A version of #isect_ray_tri_v3 which takes a threshold argument
 * so rays slightly outside the triangle to be considered as intersecting.
 */
bool isect_ray_tri_threshold_v3(const float ray_origin[3],
                                const float ray_direction[3],
                                const float v0[3],
                                const float v1[3],
                                const float v2[3],
                                float *r_lambda,
                                float r_uv[2],
                                const float threshold)
{
  const float epsilon = 0.00000001f;
  float p[3], s[3], e1[3], e2[3], q[3];
  float a, f, u, v;
  float du, dv;

  sub_v3_v3v3(e1, v1, v0);
  sub_v3_v3v3(e2, v2, v0);

  cross_v3_v3v3(p, ray_direction, e2);
  a = dot_v3v3(e1, p);
  if ((a > -epsilon) && (a < epsilon)) {
    return false;
  }
  f = 1.0f / a;

  sub_v3_v3v3(s, ray_origin, v0);

  cross_v3_v3v3(q, s, e1);
  *r_lambda = f * dot_v3v3(e2, q);
  if (*r_lambda < 0.0f) {
    return false;
  }

  u = f * dot_v3v3(s, p);
  v = f * dot_v3v3(ray_direction, q);

  if (u > 0 && v > 0 && u + v > 1) {
    float t = (u + v - 1) / 2;
    du = u - t;
    dv = v - t;
  }
  else {
    if (u < 0) {
      du = u;
    }
    else if (u > 1) {
      du = u - 1;
    }
    else {
      du = 0.0f;
    }

    if (v < 0) {
      dv = v;
    }
    else if (v > 1) {
      dv = v - 1;
    }
    else {
      dv = 0.0f;
    }
  }

  mul_v3_fl(e1, du);
  mul_v3_fl(e2, dv);

  if (len_squared_v3(e1) + len_squared_v3(e2) > threshold * threshold) {
    return false;
  }

  if (r_uv) {
    r_uv[0] = u;
    r_uv[1] = v;
  }

  return true;
}
#endif

bool isect_ray_seg_v2(const float ray_origin[2],
                      const float ray_direction[2],
                      const float v0[2],
                      const float v1[2],
                      float *r_lambda,
                      float *r_u)
{
  float v0_local[2], v1_local[2];
  sub_v2_v2v2(v0_local, v0, ray_origin);
  sub_v2_v2v2(v1_local, v1, ray_origin);

  float s10[2];
  float det;

  sub_v2_v2v2(s10, v1_local, v0_local);

  det = cross_v2v2(ray_direction, s10);
  if (det != 0.0f) {
    const float v = cross_v2v2(v0_local, v1_local);
    const float p[2] = {(ray_direction[0] * v) / det, (ray_direction[1] * v) / det};

    const float t = (dot_v2v2(p, ray_direction) / dot_v2v2(ray_direction, ray_direction));
    if ((t >= 0.0f) == 0) {
      return false;
    }

    float h[2];
    sub_v2_v2v2(h, v1_local, p);
    const float u = (dot_v2v2(s10, h) / dot_v2v2(s10, s10));
    if ((u >= 0.0f && u <= 1.0f) == 0) {
      return false;
    }

    if (r_lambda) {
      *r_lambda = t;
    }
    if (r_u) {
      *r_u = u;
    }

    return true;
  }

  return false;
}

bool isect_ray_line_v3(const float ray_origin[3],
                       const float ray_direction[3],
                       const float v0[3],
                       const float v1[3],
                       float *r_lambda)
{
  float a[3], t[3], n[3];
  sub_v3_v3v3(a, v1, v0);
  sub_v3_v3v3(t, v0, ray_origin);
  cross_v3_v3v3(n, a, ray_direction);
  const float nlen = len_squared_v3(n);

  if (nlen == 0.0f) {
    /* The lines are parallel. */
    return false;
  }

  float c[3], cray[3];
  sub_v3_v3v3(c, n, t);
  cross_v3_v3v3(cray, c, ray_direction);

  *r_lambda = dot_v3v3(cray, n) / nlen;

  return true;
}

bool isect_point_planes_v3(const float (*planes)[4], int totplane, const float p[3])
{
  int i;

  for (i = 0; i < totplane; i++) {
    if (plane_point_side_v3(planes[i], p) > 0.0f) {
      return false;
    }
  }

  return true;
}

bool isect_point_planes_v3_negated(const float (*planes)[4], const int totplane, const float p[3])
{
  for (int i = 0; i < totplane; i++) {
    if (plane_point_side_v3(planes[i], p) <= 0.0f) {
      return false;
    }
  }

  return true;
}

bool isect_line_plane_v3(float r_isect_co[3],
                         const float l1[3],
                         const float l2[3],
                         const float plane_co[3],
                         const float plane_no[3])
{
  float u[3], h[3];
  float dot;

  sub_v3_v3v3(u, l2, l1);
  sub_v3_v3v3(h, l1, plane_co);
  dot = dot_v3v3(plane_no, u);

  if (fabsf(dot) > FLT_EPSILON) {
    float lambda = -dot_v3v3(plane_no, h) / dot;
    madd_v3_v3v3fl(r_isect_co, l1, u, lambda);
    return true;
  }

  /* The segment is parallel to plane */
  return false;
}

bool isect_plane_plane_plane_v3(const float plane_a[4],
                                const float plane_b[4],
                                const float plane_c[4],
                                float r_isect_co[3])
{
  float det;

  det = determinant_m3(UNPACK3(plane_a), UNPACK3(plane_b), UNPACK3(plane_c));

  if (det != 0.0f) {
    float tmp[3];

    /* (plane_b.xyz.cross(plane_c.xyz) * -plane_a[3] +
     *  plane_c.xyz.cross(plane_a.xyz) * -plane_b[3] +
     *  plane_a.xyz.cross(plane_b.xyz) * -plane_c[3]) / det; */

    cross_v3_v3v3(tmp, plane_c, plane_b);
    mul_v3_v3fl(r_isect_co, tmp, plane_a[3]);

    cross_v3_v3v3(tmp, plane_a, plane_c);
    madd_v3_v3fl(r_isect_co, tmp, plane_b[3]);

    cross_v3_v3v3(tmp, plane_b, plane_a);
    madd_v3_v3fl(r_isect_co, tmp, plane_c[3]);

    mul_v3_fl(r_isect_co, 1.0f / det);

    return true;
  }

  return false;
}

bool isect_plane_plane_v3(const float plane_a[4],
                          const float plane_b[4],
                          float r_isect_co[3],
                          float r_isect_no[3])
{
  float det, plane_c[3];

  /* direction is simply the cross product */
  cross_v3_v3v3(plane_c, plane_a, plane_b);

  /* in this case we don't need to use 'determinant_m3' */
  det = len_squared_v3(plane_c);

  if (det != 0.0f) {
    float tmp[3];

    /* (plane_b.xyz.cross(plane_c.xyz) * -plane_a[3] +
     *  plane_c.xyz.cross(plane_a.xyz) * -plane_b[3]) / det; */
    cross_v3_v3v3(tmp, plane_c, plane_b);
    mul_v3_v3fl(r_isect_co, tmp, plane_a[3]);

    cross_v3_v3v3(tmp, plane_a, plane_c);
    madd_v3_v3fl(r_isect_co, tmp, plane_b[3]);

    mul_v3_fl(r_isect_co, 1.0f / det);

    copy_v3_v3(r_isect_no, plane_c);

    return true;
  }

  return false;
}

bool isect_planes_v3_fn(
    const float planes[][4],
    const int planes_len,
    const float eps_coplanar,
    const float eps_isect,
    void (*callback_fn)(const float co[3], int i, int j, int k, void *user_data),
    void *user_data)
{
  bool found = false;

  float n1n2[3], n2n3[3], n3n1[3];

  for (int i = 0; i < planes_len; i++) {
    const float *n1 = planes[i];
    for (int j = i + 1; j < planes_len; j++) {
      const float *n2 = planes[j];
      cross_v3_v3v3(n1n2, n1, n2);
      if (len_squared_v3(n1n2) <= eps_coplanar) {
        continue;
      }
      for (int k = j + 1; k < planes_len; k++) {
        const float *n3 = planes[k];
        cross_v3_v3v3(n2n3, n2, n3);
        if (len_squared_v3(n2n3) <= eps_coplanar) {
          continue;
        }

        cross_v3_v3v3(n3n1, n3, n1);
        if (len_squared_v3(n3n1) <= eps_coplanar) {
          continue;
        }
        const float quotient = -dot_v3v3(n1, n2n3);
        if (fabsf(quotient) < eps_coplanar) {
          continue;
        }
        const float co_test[3] = {
            ((n2n3[0] * n1[3]) + (n3n1[0] * n2[3]) + (n1n2[0] * n3[3])) / quotient,
            ((n2n3[1] * n1[3]) + (n3n1[1] * n2[3]) + (n1n2[1] * n3[3])) / quotient,
            ((n2n3[2] * n1[3]) + (n3n1[2] * n2[3]) + (n1n2[2] * n3[3])) / quotient,
        };
        int i_test;
        for (i_test = 0; i_test < planes_len; i_test++) {
          const float *np_test = planes[i_test];
          if ((dot_v3v3(np_test, co_test) + np_test[3]) > eps_isect) {
            /* For low epsilon values the point could intersect its own plane. */
            if (!ELEM(i_test, i, j, k)) {
              break;
            }
          }
        }

        if (i_test == planes_len) { /* ok */
          callback_fn(co_test, i, j, k, user_data);
          found = true;
        }
      }
    }
  }

  return found;
}

bool isect_tri_tri_v3_ex(const float tri_a[3][3],
                         const float tri_b[3][3],
                         float r_i1[3],
                         float r_i2[3],
                         int *r_tri_a_edge_isect_count)
{
  struct {
    /* Factor that indicates the position of the intersection point on the line
     * that intersects the planes of the triangles. */
    float min, max;
    /* Intersection point location. */
    float loc[2][3];
  } range[2];

  float side[2][3];
  double ba[3], bc[3], plane_a[4], plane_b[4];
  *r_tri_a_edge_isect_count = 0;

  sub_v3db_v3fl_v3fl(ba, tri_a[0], tri_a[1]);
  sub_v3db_v3fl_v3fl(bc, tri_a[2], tri_a[1]);
  cross_v3_v3v3_db(plane_a, ba, bc);
  plane_a[3] = -dot_v3db_v3fl(plane_a, tri_a[1]);
  side[1][0] = float(dot_v3db_v3fl(plane_a, tri_b[0]) + plane_a[3]);
  side[1][1] = float(dot_v3db_v3fl(plane_a, tri_b[1]) + plane_a[3]);
  side[1][2] = float(dot_v3db_v3fl(plane_a, tri_b[2]) + plane_a[3]);

  if (!side[1][0] && !side[1][1] && !side[1][2]) {
    /* Coplanar case is not supported. */
    return false;
  }

  if ((side[1][0] && side[1][1] && side[1][2]) && (side[1][0] < 0.0f) == (side[1][1] < 0.0f) &&
      (side[1][0] < 0.0f) == (side[1][2] < 0.0f))
  {
    /* All vertices of the 2nd triangle are positioned on the same side to the
     * plane defined by the 1st triangle. */
    return false;
  }

  sub_v3db_v3fl_v3fl(ba, tri_b[0], tri_b[1]);
  sub_v3db_v3fl_v3fl(bc, tri_b[2], tri_b[1]);
  cross_v3_v3v3_db(plane_b, ba, bc);
  plane_b[3] = -dot_v3db_v3fl(plane_b, tri_b[1]);
  side[0][0] = float(dot_v3db_v3fl(plane_b, tri_a[0]) + plane_b[3]);
  side[0][1] = float(dot_v3db_v3fl(plane_b, tri_a[1]) + plane_b[3]);
  side[0][2] = float(dot_v3db_v3fl(plane_b, tri_a[2]) + plane_b[3]);

  if ((side[0][0] && side[0][1] && side[0][2]) && (side[0][0] < 0.0f) == (side[0][1] < 0.0f) &&
      (side[0][0] < 0.0f) == (side[0][2] < 0.0f))
  {
    /* All vertices of the 1st triangle are positioned on the same side to the
     * plane defined by the 2nd triangle. */
    return false;
  }

  /* Direction of the line that intersects the planes of the triangles. */
  double isect_dir[3];
  cross_v3_v3v3_db(isect_dir, plane_a, plane_b);
  for (int i = 0; i < 2; i++) {
    const float(*tri)[3] = i == 0 ? tri_a : tri_b;
    /* Rearrange the triangle so that the vertex that is alone on one side
     * of the plane is located at index 1. */
    int tri_i[3];
    if ((side[i][0] && side[i][1]) && (side[i][0] < 0.0f) == (side[i][1] < 0.0f)) {
      tri_i[0] = 1;
      tri_i[1] = 2;
      tri_i[2] = 0;
    }
    else if ((side[i][1] && side[i][2]) && (side[i][1] < 0.0f) == (side[i][2] < 0.0f)) {
      tri_i[0] = 2;
      tri_i[1] = 0;
      tri_i[2] = 1;
    }
    else {
      tri_i[0] = 0;
      tri_i[1] = 1;
      tri_i[2] = 2;
    }

    double dot_b = dot_v3db_v3fl(isect_dir, tri[tri_i[1]]);
    float sidec = side[i][tri_i[1]];
    if (sidec) {
      double dot_a = dot_v3db_v3fl(isect_dir, tri[tri_i[0]]);
      double dot_c = dot_v3db_v3fl(isect_dir, tri[tri_i[2]]);
      float fac0 = sidec / (sidec - side[i][tri_i[0]]);
      float fac1 = sidec / (sidec - side[i][tri_i[2]]);
      double offset0 = fac0 * (dot_a - dot_b);
      double offset1 = fac1 * (dot_c - dot_b);
      if (offset0 > offset1) {
        /* Sort min max. */
        std::swap(offset0, offset1);
        std::swap(fac0, fac1);
        std::swap(tri_i[0], tri_i[2]);
      }

      range[i].min = float(dot_b + offset0);
      range[i].max = float(dot_b + offset1);
      interp_v3_v3v3(range[i].loc[0], tri[tri_i[1]], tri[tri_i[0]], fac0);
      interp_v3_v3v3(range[i].loc[1], tri[tri_i[1]], tri[tri_i[2]], fac1);
    }
    else {
      range[i].min = range[i].max = float(dot_b);
      copy_v3_v3(range[i].loc[0], tri[tri_i[1]]);
      copy_v3_v3(range[i].loc[1], tri[tri_i[1]]);
    }
  }

  if ((range[0].max > range[1].min) && (range[0].min < range[1].max)) {
    /* The triangles intersect because they overlap on the intersection line.
     * Now identify the two points of intersection that are in the middle to get the actual
     * intersection between the triangles. (B--C from A--B--C--D) */
    if (range[0].min >= range[1].min) {
      copy_v3_v3(r_i1, range[0].loc[0]);
      if (range[0].max <= range[1].max) {
        copy_v3_v3(r_i2, range[0].loc[1]);
        *r_tri_a_edge_isect_count = 2;
      }
      else {
        copy_v3_v3(r_i2, range[1].loc[1]);
        *r_tri_a_edge_isect_count = 1;
      }
    }
    else {
      if (range[0].max <= range[1].max) {
        copy_v3_v3(r_i1, range[0].loc[1]);
        copy_v3_v3(r_i2, range[1].loc[0]);
        *r_tri_a_edge_isect_count = 1;
      }
      else {
        copy_v3_v3(r_i1, range[1].loc[0]);
        copy_v3_v3(r_i2, range[1].loc[1]);
      }
    }
    return true;
  }

  return false;
}

bool isect_tri_tri_v3(const float t_a0[3],
                      const float t_a1[3],
                      const float t_a2[3],
                      const float t_b0[3],
                      const float t_b1[3],
                      const float t_b2[3],
                      float r_i1[3],
                      float r_i2[3])
{
  float tri_a[3][3], tri_b[3][3];
  int dummy;
  copy_v3_v3(tri_a[0], t_a0);
  copy_v3_v3(tri_a[1], t_a1);
  copy_v3_v3(tri_a[2], t_a2);
  copy_v3_v3(tri_b[0], t_b0);
  copy_v3_v3(tri_b[1], t_b1);
  copy_v3_v3(tri_b[2], t_b2);
  return isect_tri_tri_v3_ex(tri_a, tri_b, r_i1, r_i2, &dummy);
}

/* -------------------------------------------------------------------- */
/** \name Tri-Tri Intersect 2D
 *
 * "Fast and Robust Triangle-Triangle Overlap Test
 * Using Orientation Predicates" P. Guigue - O. Devillers
 * Journal of Graphics Tools, 8(1), 2003.
 *
 * \{ */

static bool isect_tri_tri_v2_impl_vert(const float t_a0[2],
                                       const float t_a1[2],
                                       const float t_a2[2],
                                       const float t_b0[2],
                                       const float t_b1[2],
                                       const float t_b2[2])
{
  if (line_point_side_v2(t_b2, t_b0, t_a1) >= 0.0f) {
    if (line_point_side_v2(t_b2, t_b1, t_a1) <= 0.0f) {
      if (line_point_side_v2(t_a0, t_b0, t_a1) > 0.0f) {
        if (line_point_side_v2(t_a0, t_b1, t_a1) <= 0.0f) {
          return true;
        }

        return false;
      }

      if (line_point_side_v2(t_a0, t_b0, t_a2) >= 0.0f) {
        if (line_point_side_v2(t_a1, t_a2, t_b0) >= 0.0f) {
          return true;
        }

        return false;
      }

      return false;
    }
    if (line_point_side_v2(t_a0, t_b1, t_a1) <= 0.0f) {
      if (line_point_side_v2(t_b2, t_b1, t_a2) <= 0.0f) {
        if (line_point_side_v2(t_a1, t_a2, t_b1) >= 0.0f) {
          return true;
        }

        return false;
      }

      return false;
    }

    return false;
  }
  if (line_point_side_v2(t_b2, t_b0, t_a2) >= 0.0f) {
    if (line_point_side_v2(t_a1, t_a2, t_b2) >= 0.0f) {
      if (line_point_side_v2(t_a0, t_b0, t_a2) >= 0.0f) {
        return true;
      }

      return false;
    }
    if (line_point_side_v2(t_a1, t_a2, t_b1) >= 0.0f) {
      if (line_point_side_v2(t_b2, t_a2, t_b1) >= 0.0f) {
        return true;
      }

      return false;
    }

    return false;
  }

  return false;
}

static bool isect_tri_tri_v2_impl_edge(const float t_a0[2],
                                       const float t_a1[2],
                                       const float t_a2[2],
                                       const float t_b0[2],
                                       const float t_b1[2],
                                       const float t_b2[2])
{
  UNUSED_VARS(t_b1);

  if (line_point_side_v2(t_b2, t_b0, t_a1) >= 0.0f) {
    if (line_point_side_v2(t_a0, t_b0, t_a1) >= 0.0f) {
      if (line_point_side_v2(t_a0, t_a1, t_b2) >= 0.0f) {
        return true;
      }

      return false;
    }

    if (line_point_side_v2(t_a1, t_a2, t_b0) >= 0.0f) {
      if (line_point_side_v2(t_a2, t_a0, t_b0) >= 0.0f) {
        return true;
      }

      return false;
    }

    return false;
  }

  if (line_point_side_v2(t_b2, t_b0, t_a2) >= 0.0f) {
    if (line_point_side_v2(t_a0, t_b0, t_a2) >= 0.0f) {
      if (line_point_side_v2(t_a0, t_a2, t_b2) >= 0.0f) {
        return true;
      }

      if (line_point_side_v2(t_a1, t_a2, t_b2) >= 0.0f) {
        return true;
      }

      return false;
    }

    return false;
  }

  return false;
}

static int isect_tri_tri_impl_ccw_v2(const float t_a0[2],
                                     const float t_a1[2],
                                     const float t_a2[2],
                                     const float t_b0[2],
                                     const float t_b1[2],
                                     const float t_b2[2])
{
  if (line_point_side_v2(t_b0, t_b1, t_a0) >= 0.0f) {
    if (line_point_side_v2(t_b1, t_b2, t_a0) >= 0.0f) {
      if (line_point_side_v2(t_b2, t_b0, t_a0) >= 0.0f) {
        return 1;
      }

      return isect_tri_tri_v2_impl_edge(t_a0, t_a1, t_a2, t_b0, t_b1, t_b2);
    }

    if (line_point_side_v2(t_b2, t_b0, t_a0) >= 0.0f) {
      return isect_tri_tri_v2_impl_edge(t_a0, t_a1, t_a2, t_b2, t_b0, t_b1);
    }

    return isect_tri_tri_v2_impl_vert(t_a0, t_a1, t_a2, t_b0, t_b1, t_b2);
  }

  if (line_point_side_v2(t_b1, t_b2, t_a0) >= 0.0f) {
    if (line_point_side_v2(t_b2, t_b0, t_a0) >= 0.0f) {
      return isect_tri_tri_v2_impl_edge(t_a0, t_a1, t_a2, t_b1, t_b2, t_b0);
    }

    return isect_tri_tri_v2_impl_vert(t_a0, t_a1, t_a2, t_b1, t_b2, t_b0);
  }

  return isect_tri_tri_v2_impl_vert(t_a0, t_a1, t_a2, t_b2, t_b0, t_b1);
}

bool isect_tri_tri_v2(const float t_a0[2],
                      const float t_a1[2],
                      const float t_a2[2],
                      const float t_b0[2],
                      const float t_b1[2],
                      const float t_b2[2])
{
  if (line_point_side_v2(t_a0, t_a1, t_a2) < 0.0f) {
    if (line_point_side_v2(t_b0, t_b1, t_b2) < 0.0f) {
      return isect_tri_tri_impl_ccw_v2(t_a0, t_a2, t_a1, t_b0, t_b2, t_b1);
    }

    return isect_tri_tri_impl_ccw_v2(t_a0, t_a2, t_a1, t_b0, t_b1, t_b2);
  }

  if (line_point_side_v2(t_b0, t_b1, t_b2) < 0.0f) {
    return isect_tri_tri_impl_ccw_v2(t_a0, t_a1, t_a2, t_b0, t_b2, t_b1);
  }

  return isect_tri_tri_impl_ccw_v2(t_a0, t_a1, t_a2, t_b0, t_b1, t_b2);
}

/** \} */

/* Adapted from the paper by Kasper Fauerby */

/* "Improved Collision detection and Response" */
static bool getLowestRoot(
    const float a, const float b, const float c, const float maxR, float *root)
{
  /* Check if a solution exists */
  const float determinant = b * b - 4.0f * a * c;

  /* If determinant is negative it means no solutions. */
  if (determinant >= 0.0f) {
    /* calculate the two roots: (if determinant == 0 then
     * x1==x2 but lets disregard that slight optimization) */
    const float sqrtD = sqrtf(determinant);
    float r1 = (-b - sqrtD) / (2.0f * a);
    float r2 = (-b + sqrtD) / (2.0f * a);

    /* Sort so x1 <= x2 */
    if (r1 > r2) {
      std::swap(r1, r2);
    }

    /* Get lowest root: */
    if (r1 > 0.0f && r1 < maxR) {
      *root = r1;
      return true;
    }

    /* It is possible that we want x2 - this can happen */
    /* if x1 < 0 */
    if (r2 > 0.0f && r2 < maxR) {
      *root = r2;
      return true;
    }
  }
  /* No (valid) solutions */
  return false;
}

int isect_aabb_planes_v3(const float (*planes)[4],
                         const int totplane,
                         const float bbmin[3],
                         const float bbmax[3])
{
  int ret = ISECT_AABB_PLANE_IN_FRONT_ALL;

  float bb_near[3], bb_far[3];
  for (int i = 0; i < totplane; i++) {
    aabb_get_near_far_from_plane(planes[i], bbmin, bbmax, bb_near, bb_far);

    if (plane_point_side_v3(planes[i], bb_far) < 0.0f) {
      return ISECT_AABB_PLANE_BEHIND_ANY;
    }
    if ((ret != ISECT_AABB_PLANE_CROSS_ANY) && (plane_point_side_v3(planes[i], bb_near) < 0.0f)) {
      ret = ISECT_AABB_PLANE_CROSS_ANY;
    }
  }

  return ret;
}

bool isect_sweeping_sphere_tri_v3(const float p1[3],
                                  const float p2[3],
                                  const float radius,
                                  const float v0[3],
                                  const float v1[3],
                                  const float v2[3],
                                  float *r_lambda,
                                  float ipoint[3])
{
  float e1[3], e2[3], e3[3], point[3], vel[3], /*dist[3],*/ nor[3], temp[3], bv[3];
  float a, b, c, d, e, x, y, z, radius2 = radius * radius;
  float elen2, edotv, edotbv, nordotv;
  float newLambda;
  bool found_by_sweep = false;

  sub_v3_v3v3(e1, v1, v0);
  sub_v3_v3v3(e2, v2, v0);
  sub_v3_v3v3(vel, p2, p1);

  /*---test plane of tri---*/
  cross_v3_v3v3(nor, e1, e2);
  normalize_v3(nor);

  /* flip normal */
  if (dot_v3v3(nor, vel) > 0.0f) {
    negate_v3(nor);
  }

  a = dot_v3v3(p1, nor) - dot_v3v3(v0, nor);
  nordotv = dot_v3v3(nor, vel);

  if (fabsf(nordotv) < 0.000001f) {
    if (fabsf(a) >= radius) {
      return false;
    }
  }
  else {
    float t0 = (-a + radius) / nordotv;
    float t1 = (-a - radius) / nordotv;

    if (t0 > t1) {
      std::swap(t0, t1);
    }

    if (t0 > 1.0f || t1 < 0.0f) {
      return false;
    }

    /* clamp to [0, 1] */
    CLAMP(t0, 0.0f, 1.0f);
    CLAMP(t1, 0.0f, 1.0f);

    /*---test inside of tri---*/
    /* plane intersection point */

    point[0] = p1[0] + vel[0] * t0 - nor[0] * radius;
    point[1] = p1[1] + vel[1] * t0 - nor[1] * radius;
    point[2] = p1[2] + vel[2] * t0 - nor[2] * radius;

    /* is the point in the tri? */
    a = dot_v3v3(e1, e1);
    b = dot_v3v3(e1, e2);
    c = dot_v3v3(e2, e2);

    sub_v3_v3v3(temp, point, v0);
    d = dot_v3v3(temp, e1);
    e = dot_v3v3(temp, e2);

    x = d * c - e * b;
    y = e * a - d * b;
    z = x + y - (a * c - b * b);

    if (z <= 0.0f && (x >= 0.0f && y >= 0.0f)) {
      // ((uint32_t(z) & ~(uint32_t(x) | uint32_t(y))) & 0x80000000)
      *r_lambda = t0;
      copy_v3_v3(ipoint, point);
      return true;
    }
  }

  *r_lambda = 1.0f;

  /*---test points---*/
  a = dot_v3v3(vel, vel);

  /*v0*/
  sub_v3_v3v3(temp, p1, v0);
  b = 2.0f * dot_v3v3(vel, temp);
  c = dot_v3v3(temp, temp) - radius2;

  if (getLowestRoot(a, b, c, *r_lambda, r_lambda)) {
    copy_v3_v3(ipoint, v0);
    found_by_sweep = true;
  }

  /*v1*/
  sub_v3_v3v3(temp, p1, v1);
  b = 2.0f * dot_v3v3(vel, temp);
  c = dot_v3v3(temp, temp) - radius2;

  if (getLowestRoot(a, b, c, *r_lambda, r_lambda)) {
    copy_v3_v3(ipoint, v1);
    found_by_sweep = true;
  }

  /*v2*/
  sub_v3_v3v3(temp, p1, v2);
  b = 2.0f * dot_v3v3(vel, temp);
  c = dot_v3v3(temp, temp) - radius2;

  if (getLowestRoot(a, b, c, *r_lambda, r_lambda)) {
    copy_v3_v3(ipoint, v2);
    found_by_sweep = true;
  }

  /*---test edges---*/
  sub_v3_v3v3(e3, v2, v1); /* wasn't yet calculated */

  /* `e1` */
  sub_v3_v3v3(bv, v0, p1);

  elen2 = dot_v3v3(e1, e1);
  edotv = dot_v3v3(e1, vel);
  edotbv = dot_v3v3(e1, bv);

  a = elen2 * -dot_v3v3(vel, vel) + edotv * edotv;
  b = 2.0f * (elen2 * dot_v3v3(vel, bv) - edotv * edotbv);
  c = elen2 * (radius2 - dot_v3v3(bv, bv)) + edotbv * edotbv;

  if (getLowestRoot(a, b, c, *r_lambda, &newLambda)) {
    e = (edotv * newLambda - edotbv) / elen2;

    if (e >= 0.0f && e <= 1.0f) {
      *r_lambda = newLambda;
      copy_v3_v3(ipoint, e1);
      mul_v3_fl(ipoint, e);
      add_v3_v3(ipoint, v0);
      found_by_sweep = true;
    }
  }

  /* `e2` */
  /* `bv` is same. */
  elen2 = dot_v3v3(e2, e2);
  edotv = dot_v3v3(e2, vel);
  edotbv = dot_v3v3(e2, bv);

  a = elen2 * -dot_v3v3(vel, vel) + edotv * edotv;
  b = 2.0f * (elen2 * dot_v3v3(vel, bv) - edotv * edotbv);
  c = elen2 * (radius2 - dot_v3v3(bv, bv)) + edotbv * edotbv;

  if (getLowestRoot(a, b, c, *r_lambda, &newLambda)) {
    e = (edotv * newLambda - edotbv) / elen2;

    if (e >= 0.0f && e <= 1.0f) {
      *r_lambda = newLambda;
      copy_v3_v3(ipoint, e2);
      mul_v3_fl(ipoint, e);
      add_v3_v3(ipoint, v0);
      found_by_sweep = true;
    }
  }

  /* `e3` */
  // sub_v3_v3v3(bv, v0, p1);   /* UNUSED */
  // elen2 = dot_v3v3(e1, e1);  /* UNUSED */
  // edotv = dot_v3v3(e1, vel); /* UNUSED */
  // edotbv = dot_v3v3(e1, bv); /* UNUSED */

  sub_v3_v3v3(bv, v1, p1);
  elen2 = dot_v3v3(e3, e3);
  edotv = dot_v3v3(e3, vel);
  edotbv = dot_v3v3(e3, bv);

  a = elen2 * -dot_v3v3(vel, vel) + edotv * edotv;
  b = 2.0f * (elen2 * dot_v3v3(vel, bv) - edotv * edotbv);
  c = elen2 * (radius2 - dot_v3v3(bv, bv)) + edotbv * edotbv;

  if (getLowestRoot(a, b, c, *r_lambda, &newLambda)) {
    e = (edotv * newLambda - edotbv) / elen2;

    if (e >= 0.0f && e <= 1.0f) {
      *r_lambda = newLambda;
      copy_v3_v3(ipoint, e3);
      mul_v3_fl(ipoint, e);
      add_v3_v3(ipoint, v1);
      found_by_sweep = true;
    }
  }

  return found_by_sweep;
}

bool isect_axial_line_segment_tri_v3(const int axis,
                                     const float p1[3],
                                     const float p2[3],
                                     const float v0[3],
                                     const float v1[3],
                                     const float v2[3],
                                     float *r_lambda)
{
  const float epsilon = 0.000001f;
  float p[3], e1[3], e2[3];
  float u, v, f;
  int a0 = axis, a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;

  sub_v3_v3v3(e1, v1, v0);
  sub_v3_v3v3(e2, v2, v0);
  sub_v3_v3v3(p, v0, p1);

  f = (e2[a1] * e1[a2] - e2[a2] * e1[a1]);
  if ((f > -epsilon) && (f < epsilon)) {
    return false;
  }

  v = (p[a2] * e1[a1] - p[a1] * e1[a2]) / f;
  if ((v < 0.0f) || (v > 1.0f)) {
    return false;
  }

  f = e1[a1];
  if ((f > -epsilon) && (f < epsilon)) {
    f = e1[a2];
    if ((f > -epsilon) && (f < epsilon)) {
      return false;
    }
    u = (-p[a2] - v * e2[a2]) / f;
  }
  else {
    u = (-p[a1] - v * e2[a1]) / f;
  }

  if ((u < 0.0f) || ((u + v) > 1.0f)) {
    return false;
  }

  *r_lambda = (p[a0] + u * e1[a0] + v * e2[a0]) / (p2[a0] - p1[a0]);

  if ((*r_lambda < 0.0f) || (*r_lambda > 1.0f)) {
    return false;
  }

  return true;
}

int isect_line_line_epsilon_v3(const float v1[3],
                               const float v2[3],
                               const float v3[3],
                               const float v4[3],
                               float r_i1[3],
                               float r_i2[3],
                               const float epsilon)
{
  float a[3], b[3], c[3], ab[3], cb[3];
  float d, div;

  sub_v3_v3v3(c, v3, v1);
  sub_v3_v3v3(a, v2, v1);
  sub_v3_v3v3(b, v4, v3);

  cross_v3_v3v3(ab, a, b);
  d = dot_v3v3(c, ab);
  div = dot_v3v3(ab, ab);

  /* important not to use an epsilon here, see: #45919 */
  /* test zero length line */
  if (UNLIKELY(div == 0.0f)) {
    return 0;
  }
  /* test if the two lines are coplanar */
  if (UNLIKELY(fabsf(d) <= epsilon)) {
    cross_v3_v3v3(cb, c, b);

    mul_v3_fl(a, dot_v3v3(cb, ab) / div);
    add_v3_v3v3(r_i1, v1, a);
    copy_v3_v3(r_i2, r_i1);

    return 1; /* one intersection only */
  }
  /* if not */

  float n[3], t[3];
  float v3t[3], v4t[3];
  sub_v3_v3v3(t, v1, v3);

  /* offset between both plane where the lines lies */
  cross_v3_v3v3(n, a, b);
  project_v3_v3v3(t, t, n);

  /* for the first line, offset the second line until it is coplanar */
  add_v3_v3v3(v3t, v3, t);
  add_v3_v3v3(v4t, v4, t);

  sub_v3_v3v3(c, v3t, v1);
  sub_v3_v3v3(a, v2, v1);
  sub_v3_v3v3(b, v4t, v3t);

  cross_v3_v3v3(ab, a, b);
  cross_v3_v3v3(cb, c, b);

  mul_v3_fl(a, dot_v3v3(cb, ab) / dot_v3v3(ab, ab));
  add_v3_v3v3(r_i1, v1, a);

  /* for the second line, just subtract the offset from the first intersection point */
  sub_v3_v3v3(r_i2, r_i1, t);

  return 2; /* two nearest points */
}

int isect_line_line_v3(const float v1[3],
                       const float v2[3],
                       const float v3[3],
                       const float v4[3],
                       float r_i1[3],
                       float r_i2[3])
{
  const float epsilon = 0.000001f;
  return isect_line_line_epsilon_v3(v1, v2, v3, v4, r_i1, r_i2, epsilon);
}

bool isect_line_line_strict_v3(const float v1[3],
                               const float v2[3],
                               const float v3[3],
                               const float v4[3],
                               float vi[3],
                               float *r_lambda)
{
  const float epsilon = 0.000001f;
  float a[3], b[3], c[3], ab[3], cb[3], ca[3];
  float d, div;

  sub_v3_v3v3(c, v3, v1);
  sub_v3_v3v3(a, v2, v1);
  sub_v3_v3v3(b, v4, v3);

  cross_v3_v3v3(ab, a, b);
  d = dot_v3v3(c, ab);
  div = dot_v3v3(ab, ab);

  /* important not to use an epsilon here, see: #45919 */
  /* test zero length line */
  if (UNLIKELY(div == 0.0f)) {
    return false;
  }
  /* test if the two lines are coplanar */
  if (UNLIKELY(fabsf(d) < epsilon)) {
    return false;
  }

  float f1, f2;
  cross_v3_v3v3(cb, c, b);
  cross_v3_v3v3(ca, c, a);

  f1 = dot_v3v3(cb, ab) / div;
  f2 = dot_v3v3(ca, ab) / div;

  if (f1 >= 0 && f1 <= 1 && f2 >= 0 && f2 <= 1) {
    mul_v3_fl(a, f1);
    add_v3_v3v3(vi, v1, a);

    if (r_lambda) {
      *r_lambda = f1;
    }

    return true; /* intersection found */
  }

  return false;
}

bool isect_ray_ray_epsilon_v3(const float ray_origin_a[3],
                              const float ray_direction_a[3],
                              const float ray_origin_b[3],
                              const float ray_direction_b[3],
                              const float epsilon,
                              float *r_lambda_a,
                              float *r_lambda_b)
{
  BLI_assert(r_lambda_a || r_lambda_b);
  float n[3];
  cross_v3_v3v3(n, ray_direction_b, ray_direction_a);
  const float nlen = len_squared_v3(n);

  /* `nlen` is the square of the area formed by the two vectors. */
  if (UNLIKELY(nlen < epsilon)) {
    /* The lines are parallel. */
    return false;
  }

  float t[3], c[3], cray[3];
  sub_v3_v3v3(t, ray_origin_b, ray_origin_a);
  sub_v3_v3v3(c, n, t);

  if (r_lambda_a != nullptr) {
    cross_v3_v3v3(cray, c, ray_direction_b);
    *r_lambda_a = dot_v3v3(cray, n) / nlen;
  }

  if (r_lambda_b != nullptr) {
    cross_v3_v3v3(cray, c, ray_direction_a);
    *r_lambda_b = dot_v3v3(cray, n) / nlen;
  }

  return true;
}

bool isect_ray_ray_v3(const float ray_origin_a[3],
                      const float ray_direction_a[3],
                      const float ray_origin_b[3],
                      const float ray_direction_b[3],
                      float *r_lambda_a,
                      float *r_lambda_b)
{
  return isect_ray_ray_epsilon_v3(ray_origin_a,
                                  ray_direction_a,
                                  ray_origin_b,
                                  ray_direction_b,
                                  FLT_MIN,
                                  r_lambda_a,
                                  r_lambda_b);
}

bool isect_aabb_aabb_v3(const float min1[3],
                        const float max1[3],
                        const float min2[3],
                        const float max2[3])
{
  return (min1[0] < max2[0] && min1[1] < max2[1] && min1[2] < max2[2] && min2[0] < max1[0] &&
          min2[1] < max1[1] && min2[2] < max1[2]);
}

void isect_ray_aabb_v3_precalc(IsectRayAABB_Precalc *data,
                               const float ray_origin[3],
                               const float ray_direction[3])
{
  copy_v3_v3(data->ray_origin, ray_origin);

  data->ray_inv_dir[0] = 1.0f / ray_direction[0];
  data->ray_inv_dir[1] = 1.0f / ray_direction[1];
  data->ray_inv_dir[2] = 1.0f / ray_direction[2];

  data->sign[0] = data->ray_inv_dir[0] < 0.0f;
  data->sign[1] = data->ray_inv_dir[1] < 0.0f;
  data->sign[2] = data->ray_inv_dir[2] < 0.0f;
}

bool isect_ray_aabb_v3(const IsectRayAABB_Precalc *data,
                       const float bb_min[3],
                       const float bb_max[3],
                       float *tmin_out)
{
  /* Adapted from http://www.gamedev.net/community/forums/topic.asp?topic_id=459973 */

  float bbox[2][3];

  copy_v3_v3(bbox[0], bb_min);
  copy_v3_v3(bbox[1], bb_max);

  float tmin = (bbox[data->sign[0]][0] - data->ray_origin[0]) * data->ray_inv_dir[0];
  float tmax = (bbox[1 - data->sign[0]][0] - data->ray_origin[0]) * data->ray_inv_dir[0];

  const float tymin = (bbox[data->sign[1]][1] - data->ray_origin[1]) * data->ray_inv_dir[1];
  const float tymax = (bbox[1 - data->sign[1]][1] - data->ray_origin[1]) * data->ray_inv_dir[1];

  if ((tmin > tymax) || (tymin > tmax)) {
    return false;
  }

  if (tymin > tmin) {
    tmin = tymin;
  }

  if (tymax < tmax) {
    tmax = tymax;
  }

  const float tzmin = (bbox[data->sign[2]][2] - data->ray_origin[2]) * data->ray_inv_dir[2];
  const float tzmax = (bbox[1 - data->sign[2]][2] - data->ray_origin[2]) * data->ray_inv_dir[2];

  if ((tmin > tzmax) || (tzmin > tmax)) {
    return false;
  }

  if (tzmin > tmin) {
    tmin = tzmin;
  }

  /* NOTE(jwilkins): tmax does not need to be updated since we don't use it
   * keeping this here for future reference. */
  // if (tzmax < tmax) tmax = tzmax;

  if (tmin_out) {
    (*tmin_out) = tmin;
  }

  return true;
}

bool isect_ray_aabb_v3_simple(const float orig[3],
                              const float dir[3],
                              const float bb_min[3],
                              const float bb_max[3],
                              float *tmin,
                              float *tmax)
{
  double t[6];
  float hit_dist[2];
  const double invdirx = (dir[0] > 1e-35f || dir[0] < -1e-35f) ? 1.0 / double(dir[0]) : DBL_MAX;
  const double invdiry = (dir[1] > 1e-35f || dir[1] < -1e-35f) ? 1.0 / double(dir[1]) : DBL_MAX;
  const double invdirz = (dir[2] > 1e-35f || dir[2] < -1e-35f) ? 1.0 / double(dir[2]) : DBL_MAX;
  t[0] = double(bb_min[0] - orig[0]) * invdirx;
  t[1] = double(bb_max[0] - orig[0]) * invdirx;
  t[2] = double(bb_min[1] - orig[1]) * invdiry;
  t[3] = double(bb_max[1] - orig[1]) * invdiry;
  t[4] = double(bb_min[2] - orig[2]) * invdirz;
  t[5] = double(bb_max[2] - orig[2]) * invdirz;
  hit_dist[0] = float(fmax(fmax(fmin(t[0], t[1]), fmin(t[2], t[3])), fmin(t[4], t[5])));
  hit_dist[1] = float(fmin(fmin(fmax(t[0], t[1]), fmax(t[2], t[3])), fmax(t[4], t[5])));
  if ((hit_dist[1] < 0.0f) || (hit_dist[0] > hit_dist[1])) {
    return false;
  }

  if (tmin) {
    *tmin = hit_dist[0];
  }
  if (tmax) {
    *tmax = hit_dist[1];
  }
  return true;
}

float closest_to_ray_v3(float r_close[3],
                        const float p[3],
                        const float ray_orig[3],
                        const float ray_dir[3])
{
  float h[3], lambda;

  if (UNLIKELY(is_zero_v3(ray_dir))) {
    lambda = 0.0f;
    copy_v3_v3(r_close, ray_orig);
    return lambda;
  }

  sub_v3_v3v3(h, p, ray_orig);
  lambda = dot_v3v3(ray_dir, h) / dot_v3v3(ray_dir, ray_dir);
  madd_v3_v3v3fl(r_close, ray_orig, ray_dir, lambda);
  return lambda;
}

float closest_to_line_v3(float r_close[3], const float p[3], const float l1[3], const float l2[3])
{
  float u[3];
  sub_v3_v3v3(u, l2, l1);
  return closest_to_ray_v3(r_close, p, l1, u);
}

float closest_to_line_v2(float r_close[2], const float p[2], const float l1[2], const float l2[2])
{
  float h[2], u[2], lambda, denom;
  sub_v2_v2v2(u, l2, l1);
  sub_v2_v2v2(h, p, l1);
  denom = dot_v2v2(u, u);
  if (denom == 0.0f) {
    r_close[0] = l1[0];
    r_close[1] = l1[1];
    return 0.0f;
  }
  lambda = dot_v2v2(u, h) / denom;
  r_close[0] = l1[0] + u[0] * lambda;
  r_close[1] = l1[1] + u[1] * lambda;
  return lambda;
}

double closest_to_line_v2_db(double r_close[2],
                             const double p[2],
                             const double l1[2],
                             const double l2[2])
{
  double h[2], u[2], lambda, denom;
  sub_v2_v2v2_db(u, l2, l1);
  sub_v2_v2v2_db(h, p, l1);
  denom = dot_v2v2_db(u, u);
  if (denom == 0.0) {
    r_close[0] = l1[0];
    r_close[1] = l1[1];
    return 0.0;
  }
  lambda = dot_v2v2_db(u, h) / denom;
  r_close[0] = l1[0] + u[0] * lambda;
  r_close[1] = l1[1] + u[1] * lambda;
  return lambda;
}

float ray_point_factor_v3_ex(const float p[3],
                             const float ray_origin[3],
                             const float ray_direction[3],
                             const float epsilon,
                             const float fallback)
{
  float p_relative[3];
  sub_v3_v3v3(p_relative, p, ray_origin);
  const float dot = len_squared_v3(ray_direction);
  return (dot > epsilon) ? (dot_v3v3(ray_direction, p_relative) / dot) : fallback;
}

float ray_point_factor_v3(const float p[3],
                          const float ray_origin[3],
                          const float ray_direction[3])
{
  return ray_point_factor_v3_ex(p, ray_origin, ray_direction, 0.0f, 0.0f);
}

float line_point_factor_v3_ex(const float p[3],
                              const float l1[3],
                              const float l2[3],
                              const float epsilon,
                              const float fallback)
{
  float h[3], u[3];
  float dot;
  sub_v3_v3v3(u, l2, l1);
  sub_v3_v3v3(h, p, l1);

  /* better check for zero */
  dot = len_squared_v3(u);
  return (dot > epsilon) ? (dot_v3v3(u, h) / dot) : fallback;
}
float line_point_factor_v3(const float p[3], const float l1[3], const float l2[3])
{
  return line_point_factor_v3_ex(p, l1, l2, 0.0f, 0.0f);
}

float line_point_factor_v2_ex(const float p[2],
                              const float l1[2],
                              const float l2[2],
                              const float epsilon,
                              const float fallback)
{
  float h[2], u[2];
  float dot;
  sub_v2_v2v2(u, l2, l1);
  sub_v2_v2v2(h, p, l1);
  /* better check for zero */
  dot = len_squared_v2(u);
  return (dot > epsilon) ? (dot_v2v2(u, h) / dot) : fallback;
}

float line_point_factor_v2(const float p[2], const float l1[2], const float l2[2])
{
  return line_point_factor_v2_ex(p, l1, l2, 0.0f, 0.0f);
}

float line_plane_factor_v3(const float plane_co[3],
                           const float plane_no[3],
                           const float l1[3],
                           const float l2[3])
{
  float u[3], h[3];
  float dot;
  sub_v3_v3v3(u, l2, l1);
  sub_v3_v3v3(h, l1, plane_co);
  dot = dot_v3v3(plane_no, u);
  return (dot != 0.0f) ? -dot_v3v3(plane_no, h) / dot : 0.0f;
}

void limit_dist_v3(float v1[3], float v2[3], const float dist)
{
  const float dist_old = len_v3v3(v1, v2);

  if (dist_old > dist) {
    float v1_old[3];
    float v2_old[3];
    float fac = (dist / dist_old) * 0.5f;

    copy_v3_v3(v1_old, v1);
    copy_v3_v3(v2_old, v2);

    interp_v3_v3v3(v1, v1_old, v2_old, 0.5f - fac);
    interp_v3_v3v3(v2, v1_old, v2_old, 0.5f + fac);
  }
}

int isect_point_tri_v2_int(
    const int x1, const int y1, const int x2, const int y2, const int a, const int b)
{
  float v1[2], v2[2], v3[2], p[2];

  v1[0] = float(x1);
  v1[1] = float(y1);

  v2[0] = float(x1);
  v2[1] = float(y2);

  v3[0] = float(x2);
  v3[1] = float(y1);

  p[0] = float(a);
  p[1] = float(b);

  return isect_point_tri_v2(p, v1, v2, v3);
}

static bool point_in_slice(const float p[3],
                           const float v1[3],
                           const float l1[3],
                           const float l2[3])
{
  /* What is a slice?
   * Some math:
   * a line including (l1, l2) and a point not on the line
   * define a subset of R3 delimited by planes parallel to the line and orthogonal
   * to the (point --> line) distance vector, one plane on the line one on the point,
   * the room inside usually is rather small compared to R3 though still infinite
   * useful for restricting (speeding up) searches
   * e.g. all points of triangular prism are within the intersection of 3 "slices"
   * Another trivial case is a cube, but see a "spat" which is a deformed cube
   * with paired parallel planes needs only 3 slices too. */
  float h, rp[3], cp[3], q[3];

  closest_to_line_v3(cp, v1, l1, l2);
  sub_v3_v3v3(q, cp, v1);

  sub_v3_v3v3(rp, p, v1);
  h = dot_v3v3(q, rp) / dot_v3v3(q, q);
  /* NOTE: when 'h' is nan/-nan, this check returns false
   * without explicit check - covering the degenerate case */
  return (h >= 0.0f && h <= 1.0f);
}

/* Adult sister defining the slice planes by the origin and the normal.
 * NOTE: |normal| may not be 1 but defining the thickness of the slice. */
static bool point_in_slice_as(const float p[3], const float origin[3], const float normal[3])
{
  float h, rp[3];
  sub_v3_v3v3(rp, p, origin);
  h = dot_v3v3(normal, rp) / dot_v3v3(normal, normal);
  if (h < 0.0f || h > 1.0f) {
    return false;
  }
  return true;
}

bool point_in_slice_seg(float p[3], float l1[3], float l2[3])
{
  float normal[3];

  sub_v3_v3v3(normal, l2, l1);

  return point_in_slice_as(p, l1, normal);
}

bool isect_point_tri_prism_v3(const float p[3],
                              const float v1[3],
                              const float v2[3],
                              const float v3[3])
{
  if (!point_in_slice(p, v1, v2, v3)) {
    return false;
  }
  if (!point_in_slice(p, v2, v3, v1)) {
    return false;
  }
  if (!point_in_slice(p, v3, v1, v2)) {
    return false;
  }
  return true;
}

bool isect_point_tri_v3(
    const float p[3], const float v1[3], const float v2[3], const float v3[3], float r_isect_co[3])
{
  if (isect_point_tri_prism_v3(p, v1, v2, v3)) {
    float plane[4];
    float no[3];

    /* Could use normal_tri_v3, but doesn't have to be unit-length */
    cross_tri_v3(no, v1, v2, v3);
    BLI_assert(len_squared_v3(no) != 0.0f);

    plane_from_point_normal_v3(plane, v1, no);
    closest_to_plane_v3(r_isect_co, plane, p);

    return true;
  }

  return false;
}

bool clip_segment_v3_plane(
    const float p1[3], const float p2[3], const float plane[4], float r_p1[3], float r_p2[3])
{
  float dp[3], div;

  sub_v3_v3v3(dp, p2, p1);
  div = dot_v3v3(dp, plane);

  if (div == 0.0f) {
    /* parallel */
    return true;
  }

  float t = -plane_point_side_v3(plane, p1);

  if (div > 0.0f) {
    /* behind plane, completely clipped */
    if (t >= div) {
      return false;
    }
    if (t > 0.0f) {
      const float p1_copy[3] = {UNPACK3(p1)};
      copy_v3_v3(r_p2, p2);
      madd_v3_v3v3fl(r_p1, p1_copy, dp, t / div);
      return true;
    }
  }
  else {
    /* behind plane, completely clipped */
    if (t >= 0.0f) {
      return false;
    }
    if (t > div) {
      const float p1_copy[3] = {UNPACK3(p1)};
      copy_v3_v3(r_p1, p1);
      madd_v3_v3v3fl(r_p2, p1_copy, dp, t / div);
      return true;
    }
  }

  /* In case input/output values match (above also). */
  const float p1_copy[3] = {UNPACK3(p1)};
  copy_v3_v3(r_p2, p2);
  copy_v3_v3(r_p1, p1_copy);
  return true;
}

bool clip_segment_v3_plane_n(const float p1[3],
                             const float p2[3],
                             const float plane_array[][4],
                             const int plane_num,
                             float r_p1[3],
                             float r_p2[3])
{
  /* intersect from both directions */
  float p1_fac = 0.0f, p2_fac = 1.0f;

  float dp[3];
  sub_v3_v3v3(dp, p2, p1);

  for (int i = 0; i < plane_num; i++) {
    const float *plane = plane_array[i];
    const float div = dot_v3v3(dp, plane);

    if (div != 0.0f) {
      float t = -plane_point_side_v3(plane, p1);
      if (div > 0.0f) {
        /* clip p1 lower bounds */
        if (t >= div) {
          return false;
        }
        if (t > 0.0f) {
          t /= div;
          if (t > p1_fac) {
            p1_fac = t;
            if (p1_fac > p2_fac) {
              return false;
            }
          }
        }
      }
      else if (div < 0.0f) {
        /* clip p2 upper bounds */
        if (t >= 0.0f) {
          return false;
        }
        if (t > div) {
          t /= div;
          if (t < p2_fac) {
            p2_fac = t;
            if (p1_fac > p2_fac) {
              return false;
            }
          }
        }
      }
    }
  }

  /* In case input/output values match. */
  const float p1_copy[3] = {UNPACK3(p1)};

  madd_v3_v3v3fl(r_p1, p1_copy, dp, p1_fac);
  madd_v3_v3v3fl(r_p2, p1_copy, dp, p2_fac);

  return true;
}

/****************************** Axis Utils ********************************/

void axis_dominant_v3_to_m3(float r_mat[3][3], const float normal[3])
{
  BLI_ASSERT_UNIT_V3(normal);

  copy_v3_v3(r_mat[2], normal);
  ortho_basis_v3v3_v3(r_mat[0], r_mat[1], r_mat[2]);

  BLI_ASSERT_UNIT_V3(r_mat[0]);
  BLI_ASSERT_UNIT_V3(r_mat[1]);

  transpose_m3(r_mat);

  BLI_assert(!is_negative_m3(r_mat));
  BLI_assert((fabsf(dot_m3_v3_row_z(r_mat, normal) - 1.0f) < BLI_ASSERT_UNIT_EPSILON) ||
             is_zero_v3(normal));
}

void axis_dominant_v3_to_m3_negate(float r_mat[3][3], const float normal[3])
{
  BLI_ASSERT_UNIT_V3(normal);

  negate_v3_v3(r_mat[2], normal);
  ortho_basis_v3v3_v3(r_mat[0], r_mat[1], r_mat[2]);

  BLI_ASSERT_UNIT_V3(r_mat[0]);
  BLI_ASSERT_UNIT_V3(r_mat[1]);

  transpose_m3(r_mat);

  BLI_assert(!is_negative_m3(r_mat));
  BLI_assert((dot_m3_v3_row_z(r_mat, normal) < BLI_ASSERT_UNIT_EPSILON) || is_zero_v3(normal));
}

/****************************** Interpolation ********************************/

static float tri_signed_area(
    const float v1[3], const float v2[3], const float v3[3], const int i, const int j)
{
  return 0.5f * ((v1[i] - v2[i]) * (v2[j] - v3[j]) + (v1[j] - v2[j]) * (v3[i] - v2[i]));
}

/**
 * \return false when degenerate.
 */
static bool barycentric_weights(const float v1[3],
                                const float v2[3],
                                const float v3[3],
                                const float co[3],
                                const float n[3],
                                float w[3])
{
  float wtot;
  int i, j;

  axis_dominant_v3(&i, &j, n);

  w[0] = tri_signed_area(v2, v3, co, i, j);
  w[1] = tri_signed_area(v3, v1, co, i, j);
  w[2] = tri_signed_area(v1, v2, co, i, j);

  wtot = w[0] + w[1] + w[2];

#ifndef NDEBUG /* Avoid floating point exception when debugging. */
  if (wtot != 0.0f)
#endif
  {
    mul_v3_fl(w, 1.0f / wtot);
    if (is_finite_v3(w)) {
      return true;
    }
  }
  /* Zero area triangle. */
  copy_v3_fl(w, 1.0f / 3.0f);
  return false;
}

void interp_weights_tri_v3(
    float w[3], const float v1[3], const float v2[3], const float v3[3], const float co[3])
{
  float n[3];

  normal_tri_v3(n, v1, v2, v3);
  barycentric_weights(v1, v2, v3, co, n, w);
}

void interp_weights_quad_v3(float w[4],
                            const float v1[3],
                            const float v2[3],
                            const float v3[3],
                            const float v4[3],
                            const float co[3])
{
  float w2[3];

  zero_v4(w);

  /* first check for exact match */
  if (equals_v3v3(co, v1)) {
    w[0] = 1.0f;
  }
  else if (equals_v3v3(co, v2)) {
    w[1] = 1.0f;
  }
  else if (equals_v3v3(co, v3)) {
    w[2] = 1.0f;
  }
  else if (equals_v3v3(co, v4)) {
    w[3] = 1.0f;
  }
  else {
    /* otherwise compute barycentric interpolation weights */
    float n1[3], n2[3], n[3];
    bool ok;

    sub_v3_v3v3(n1, v1, v3);
    sub_v3_v3v3(n2, v2, v4);
    cross_v3_v3v3(n, n1, n2);

    ok = barycentric_weights(v1, v2, v4, co, n, w);
    std::swap(w[2], w[3]);

    if (!ok || (w[0] < 0.0f)) {
      /* if w[1] is negative, co is on the other side of the v1-v3 edge,
       * so we interpolate using the other triangle */
      ok = barycentric_weights(v2, v3, v4, co, n, w2);

      if (ok) {
        w[0] = 0.0f;
        w[1] = w2[0];
        w[2] = w2[1];
        w[3] = w2[2];
      }
    }
  }
}

int barycentric_inside_triangle_v2(const float w[3])
{
  if (IN_RANGE(w[0], 0.0f, 1.0f) && IN_RANGE(w[1], 0.0f, 1.0f) && IN_RANGE(w[2], 0.0f, 1.0f)) {
    return 1;
  }
  if (IN_RANGE_INCL(w[0], 0.0f, 1.0f) && IN_RANGE_INCL(w[1], 0.0f, 1.0f) &&
      IN_RANGE_INCL(w[2], 0.0f, 1.0f))
  {
    return 2;
  }

  return 0;
}

bool barycentric_coords_v2(
    const float v1[2], const float v2[2], const float v3[2], const float co[2], float w[3])
{
  const float x = co[0], y = co[1];
  const float x1 = v1[0], y1 = v1[1];
  const float x2 = v2[0], y2 = v2[1];
  const float x3 = v3[0], y3 = v3[1];
  const float det = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3);

#ifndef NDEBUG /* Avoid floating point exception when debugging. */
  if (det != 0.0f)
#endif
  {
    w[0] = ((y2 - y3) * (x - x3) + (x3 - x2) * (y - y3)) / det;
    w[1] = ((y3 - y1) * (x - x3) + (x1 - x3) * (y - y3)) / det;
    w[2] = 1.0f - w[0] - w[1];
    if (is_finite_v3(w)) {
      return true;
    }
  }

  return false;
}

void barycentric_weights_v2(
    const float v1[2], const float v2[2], const float v3[2], const float co[2], float w[3])
{
  float wtot;

  w[0] = cross_tri_v2(v2, v3, co);
  w[1] = cross_tri_v2(v3, v1, co);
  w[2] = cross_tri_v2(v1, v2, co);
  wtot = w[0] + w[1] + w[2];

#ifndef NDEBUG /* Avoid floating point exception when debugging. */
  if (wtot != 0.0f)
#endif
  {
    mul_v3_fl(w, 1.0f / wtot);
    if (is_finite_v3(w)) {
      return;
    }
  }
  /* Dummy values for zero area face. */
  copy_v3_fl(w, 1.0f / 3.0f);
}

void barycentric_weights_v2_clamped(
    const float v1[2], const float v2[2], const float v3[2], const float co[2], float w[3])
{
  float wtot;

  w[0] = max_ff(cross_tri_v2(v2, v3, co), 0.0f);
  w[1] = max_ff(cross_tri_v2(v3, v1, co), 0.0f);
  w[2] = max_ff(cross_tri_v2(v1, v2, co), 0.0f);
  wtot = w[0] + w[1] + w[2];

#ifndef NDEBUG /* Avoid floating point exception when debugging. */
  if (wtot != 0.0f)
#endif
  {
    mul_v3_fl(w, 1.0f / wtot);
    if (is_finite_v3(w)) {
      return;
    }
  }
  /* Dummy values for zero area face. */
  copy_v3_fl(w, 1.0f / 3.0f);
}

void barycentric_weights_v2_persp(
    const float v1[4], const float v2[4], const float v3[4], const float co[2], float w[3])
{
  float wtot;

  w[0] = cross_tri_v2(v2, v3, co) / v1[3];
  w[1] = cross_tri_v2(v3, v1, co) / v2[3];
  w[2] = cross_tri_v2(v1, v2, co) / v3[3];
  wtot = w[0] + w[1] + w[2];

#ifndef NDEBUG /* Avoid floating point exception when debugging. */
  if (wtot != 0.0f)
#endif
  {
    mul_v3_fl(w, 1.0f / wtot);
    if (is_finite_v3(w)) {
      return;
    }
  }
  /* Dummy values for zero area face. */
  copy_v3_fl(w, 1.0f / 3.0f);
}

void barycentric_weights_v2_quad(const float v1[2],
                                 const float v2[2],
                                 const float v3[2],
                                 const float v4[2],
                                 const float co[2],
                                 float w[4])
{
  /* NOTE(@ideasman42): fabsf() here is not needed for convex quads
   * (and not used in #interp_weights_poly_v2).
   * But in the case of concave/bow-tie quads for the mask rasterizer it
   * gives unreliable results without adding `absf()`. If this becomes an issue for more general
   * usage we could have this optional or use a different function. */
#define MEAN_VALUE_HALF_TAN_V2(_area, i1, i2) \
  ((_area = cross_v2v2(dirs[i1], dirs[i2])) != 0.0f ? \
       fabsf(((lens[i1] * lens[i2]) - dot_v2v2(dirs[i1], dirs[i2])) / _area) : \
       0.0f)

  const float dirs[4][2] = {
      {v1[0] - co[0], v1[1] - co[1]},
      {v2[0] - co[0], v2[1] - co[1]},
      {v3[0] - co[0], v3[1] - co[1]},
      {v4[0] - co[0], v4[1] - co[1]},
  };

  const float lens[4] = {
      len_v2(dirs[0]),
      len_v2(dirs[1]),
      len_v2(dirs[2]),
      len_v2(dirs[3]),
  };

  /* avoid divide by zero */
  if (UNLIKELY(lens[0] < FLT_EPSILON)) {
    w[0] = 1.0f;
    w[1] = w[2] = w[3] = 0.0f;
  }
  else if (UNLIKELY(lens[1] < FLT_EPSILON)) {
    w[1] = 1.0f;
    w[0] = w[2] = w[3] = 0.0f;
  }
  else if (UNLIKELY(lens[2] < FLT_EPSILON)) {
    w[2] = 1.0f;
    w[0] = w[1] = w[3] = 0.0f;
  }
  else if (UNLIKELY(lens[3] < FLT_EPSILON)) {
    w[3] = 1.0f;
    w[0] = w[1] = w[2] = 0.0f;
  }
  else {
    float wtot, area;

    /* variable 'area' is just for storage,
     * the order its initialized doesn't matter */
#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunsequenced"
#endif

    /* inline mean_value_half_tan four times here */
    const float t[4] = {
        MEAN_VALUE_HALF_TAN_V2(area, 0, 1),
        MEAN_VALUE_HALF_TAN_V2(area, 1, 2),
        MEAN_VALUE_HALF_TAN_V2(area, 2, 3),
        MEAN_VALUE_HALF_TAN_V2(area, 3, 0),
    };

#ifdef __clang__
#  pragma clang diagnostic pop
#endif

#undef MEAN_VALUE_HALF_TAN_V2

    w[0] = (t[3] + t[0]) / lens[0];
    w[1] = (t[0] + t[1]) / lens[1];
    w[2] = (t[1] + t[2]) / lens[2];
    w[3] = (t[2] + t[3]) / lens[3];

    wtot = w[0] + w[1] + w[2] + w[3];

#ifndef NDEBUG /* Avoid floating point exception when debugging. */
    if (wtot != 0.0f)
#endif
    {
      mul_v4_fl(w, 1.0f / wtot);
      if (is_finite_v4(w)) {
        return;
      }
    }
    /* Dummy values for zero area face. */
    copy_v4_fl(w, 1.0f / 4.0f);
  }
}

void transform_point_by_tri_v3(float pt_tar[3],
                               float const pt_src[3],
                               const float tri_tar_p1[3],
                               const float tri_tar_p2[3],
                               const float tri_tar_p3[3],
                               const float tri_src_p1[3],
                               const float tri_src_p2[3],
                               const float tri_src_p3[3])
{
  /* this works by moving the source triangle so its normal is pointing on the Z
   * axis where its barycentric weights can be calculated in 2D and its Z offset can
   * be re-applied. The weights are applied directly to the targets 3D points and the
   * z-depth is used to scale the targets normal as an offset.
   * This saves transforming the target into its Z-Up orientation and back
   * (which could also work) */
  float no_tar[3], no_src[3];
  float mat_src[3][3];
  float pt_src_xy[3];
  float tri_xy_src[3][3];
  float w_src[3];
  float area_tar, area_src;
  float z_ofs_src;

  normal_tri_v3(no_tar, tri_tar_p1, tri_tar_p2, tri_tar_p3);
  normal_tri_v3(no_src, tri_src_p1, tri_src_p2, tri_src_p3);

  axis_dominant_v3_to_m3(mat_src, no_src);

  /* make the source tri xy space */
  mul_v3_m3v3(pt_src_xy, mat_src, pt_src);
  mul_v3_m3v3(tri_xy_src[0], mat_src, tri_src_p1);
  mul_v3_m3v3(tri_xy_src[1], mat_src, tri_src_p2);
  mul_v3_m3v3(tri_xy_src[2], mat_src, tri_src_p3);

  barycentric_weights_v2(tri_xy_src[0], tri_xy_src[1], tri_xy_src[2], pt_src_xy, w_src);
  interp_v3_v3v3v3(pt_tar, tri_tar_p1, tri_tar_p2, tri_tar_p3, w_src);

  area_tar = sqrtf(area_tri_v3(tri_tar_p1, tri_tar_p2, tri_tar_p3));
  area_src = sqrtf(area_tri_v2(tri_xy_src[0], tri_xy_src[1], tri_xy_src[2]));

  z_ofs_src = pt_src_xy[2] - tri_xy_src[0][2];
  madd_v3_v3v3fl(pt_tar, pt_tar, no_tar, (z_ofs_src / area_src) * area_tar);
}

void transform_point_by_seg_v3(float p_dst[3],
                               const float p_src[3],
                               const float l_dst_p1[3],
                               const float l_dst_p2[3],
                               const float l_src_p1[3],
                               const float l_src_p2[3])
{
  float t = line_point_factor_v3(p_src, l_src_p1, l_src_p2);
  interp_v3_v3v3(p_dst, l_dst_p1, l_dst_p2, t);
}

int interp_sparse_array(float *array, const int list_size, const float skipval)
{
  int found_invalid = 0;
  int found_valid = 0;
  int i;

  for (i = 0; i < list_size; i++) {
    if (array[i] == skipval) {
      found_invalid = 1;
    }
    else {
      found_valid = 1;
    }
  }

  if (found_valid == 0) {
    return -1;
  }
  if (found_invalid == 0) {
    return 0;
  }

  /* found invalid depths, interpolate */
  float valid_last = skipval;
  int valid_ofs = 0;

  blender::Array<float> array_up(list_size);
  blender::Array<float> array_down(list_size);

  blender::Array<int> ofs_tot_up(list_size);
  blender::Array<int> ofs_tot_down(list_size);

  for (i = 0; i < list_size; i++) {
    if (array[i] == skipval) {
      array_up[i] = valid_last;
      ofs_tot_up[i] = ++valid_ofs;
    }
    else {
      valid_last = array[i];
      valid_ofs = 0;
    }
  }

  valid_last = skipval;
  valid_ofs = 0;

  for (i = list_size - 1; i >= 0; i--) {
    if (array[i] == skipval) {
      array_down[i] = valid_last;
      ofs_tot_down[i] = ++valid_ofs;
    }
    else {
      valid_last = array[i];
      valid_ofs = 0;
    }
  }

  /* now blend */
  for (i = 0; i < list_size; i++) {
    if (array[i] == skipval) {
      if (array_up[i] != skipval && array_down[i] != skipval) {
        array[i] = ((array_up[i] * float(ofs_tot_down[i])) +
                    (array_down[i] * float(ofs_tot_up[i]))) /
                   float(ofs_tot_down[i] + ofs_tot_up[i]);
      }
      else if (array_up[i] != skipval) {
        array[i] = array_up[i];
      }
      else if (array_down[i] != skipval) {
        array[i] = array_down[i];
      }
    }
  }

  return 1;
}

/* -------------------------------------------------------------------- */
/** \name interp_weights_poly_v2, v3
 * \{ */

#define IS_POINT_IX (1 << 0)
#define IS_SEGMENT_IX (1 << 1)

#define DIR_V3_SET(d_len, va, vb) \
  { \
    sub_v3_v3v3((d_len)->dir, va, vb); \
    (d_len)->len = len_v3((d_len)->dir); \
  } \
  (void)0

#define DIR_V2_SET(d_len, va, vb) \
  { \
    sub_v2db_v2fl_v2fl((d_len)->dir, va, vb); \
    (d_len)->len = len_v2_db((d_len)->dir); \
  } \
  (void)0

struct Float3_Len {
  float dir[3], len;
};

struct Double2_Len {
  double dir[2], len;
};

/* Mean value weights - smooth interpolation weights for polygons with
 * more than 3 vertices */
static float mean_value_half_tan_v3(const Float3_Len *d_curr, const Float3_Len *d_next)
{
  float cross[3];
  cross_v3_v3v3(cross, d_curr->dir, d_next->dir);
  const float area = len_v3(cross);
  /* Compare against zero since 'FLT_EPSILON' can be too large, see: #73348. */
  if (LIKELY(area != 0.0f)) {
    const float dot = dot_v3v3(d_curr->dir, d_next->dir);
    const float len = d_curr->len * d_next->len;
    const float result = (len - dot) / area;
    if (isfinite(result)) {
      return result;
    }
  }
  return 0.0f;
}

/**
 * Mean value weights - same as #mean_value_half_tan_v3 but for 2D vectors.
 *
 * \note When interpolating a 2D polygon, a point can be considered "outside"
 * the polygon's bounds. Thus, when the point is very distant and the vectors
 * have relatively close values, the precision problems are evident since they
 * do not indicate a point "inside" the polygon.
 * To resolve this, doubles are used.
 */
static double mean_value_half_tan_v2_db(const Double2_Len *d_curr, const Double2_Len *d_next)
{
  /* Different from the 3d version but still correct. */
  const double area = cross_v2v2_db(d_curr->dir, d_next->dir);
  /* Compare against zero since 'FLT_EPSILON' can be too large, see: #73348. */
  if (LIKELY(area != 0.0)) {
    const double dot = dot_v2v2_db(d_curr->dir, d_next->dir);
    const double len = d_curr->len * d_next->len;
    const double result = (len - dot) / area;
    if (isfinite(result)) {
      return result;
    }
  }
  return 0.0;
}

void interp_weights_poly_v3(float *w, float v[][3], const int n, const float co[3])
{
  /* Before starting to calculate the weight, we need to figure out the floating point precision we
   * can expect from the supplied data. */
  float max_value = 0;

  for (int i = 0; i < n; i++) {
    max_value = max_ff(max_value, fabsf(v[i][0] - co[0]));
    max_value = max_ff(max_value, fabsf(v[i][1] - co[1]));
    max_value = max_ff(max_value, fabsf(v[i][2] - co[2]));
  }

  /* These to values we derived by empirically testing different values that works for the test
   * files in D7772. */
  const float eps = 16.0f * FLT_EPSILON * max_value;
  const float eps_sq = eps * eps;
  const float *v_curr, *v_next;
  float ht_prev, ht; /* half tangents */
  float totweight = 0.0f;
  int i_curr, i_next;
  char ix_flag = 0;
  Float3_Len d_curr, d_next;

  /* loop over 'i_next' */
  i_curr = n - 1;
  i_next = 0;

  v_curr = v[i_curr];
  v_next = v[i_next];

  DIR_V3_SET(&d_curr, v_curr - 3 /* v[n - 2] */, co);
  DIR_V3_SET(&d_next, v_curr /* v[n - 1] */, co);
  ht_prev = mean_value_half_tan_v3(&d_curr, &d_next);

  while (i_next < n) {
    /* Mark Mayer et al algorithm that is used here does not operate well if vertex is close
     * to borders of face.
     * In that case, do simple linear interpolation between the two edge vertices */

    /* 'd_next.len' is in fact 'd_curr.len', just avoid copy to begin with */
    if (UNLIKELY(d_next.len < eps)) {
      ix_flag = IS_POINT_IX;
      break;
    }
    if (UNLIKELY(dist_squared_to_line_segment_v3(co, v_curr, v_next) < eps_sq)) {
      ix_flag = IS_SEGMENT_IX;
      break;
    }

    d_curr = d_next;
    DIR_V3_SET(&d_next, v_next, co);
    ht = mean_value_half_tan_v3(&d_curr, &d_next);
    w[i_curr] = (ht_prev + ht) / d_curr.len;
    totweight += w[i_curr];

    /* step */
    i_curr = i_next++;
    v_curr = v_next;
    v_next = v[i_next];

    ht_prev = ht;
  }

  if (ix_flag) {
    memset(w, 0, sizeof(*w) * size_t(n));

    if (ix_flag & IS_POINT_IX) {
      w[i_curr] = 1.0f;
    }
    else {
      float fac = line_point_factor_v3(co, v_curr, v_next);
      CLAMP(fac, 0.0f, 1.0f);
      w[i_curr] = 1.0f - fac;
      w[i_next] = fac;
    }
  }
  else {
    if (totweight != 0.0f) {
      for (i_curr = 0; i_curr < n; i_curr++) {
        w[i_curr] /= totweight;
      }
    }
  }
}

void interp_weights_poly_v2(float *w, float v[][2], const int n, const float co[2])
{
  /* Before starting to calculate the weight, we need to figure out the floating point precision we
   * can expect from the supplied data. */
  float max_value = 0;

  for (int i = 0; i < n; i++) {
    max_value = max_ff(max_value, fabsf(v[i][0] - co[0]));
    max_value = max_ff(max_value, fabsf(v[i][1] - co[1]));
  }

  /* These to values we derived by empirically testing different values that works for the test
   * files in D7772. */
  const float eps = 16.0f * FLT_EPSILON * max_value;
  const float eps_sq = eps * eps;

  const float *v_curr, *v_next;
  double ht_prev, ht; /* half tangents */
  float totweight = 0.0f;
  int i_curr, i_next;
  char ix_flag = 0;
  Double2_Len d_curr, d_next;

  /* loop over 'i_next' */
  i_curr = n - 1;
  i_next = 0;

  v_curr = v[i_curr];
  v_next = v[i_next];

  DIR_V2_SET(&d_curr, v_curr - 2 /* v[n - 2] */, co);
  DIR_V2_SET(&d_next, v_curr /* v[n - 1] */, co);
  ht_prev = mean_value_half_tan_v2_db(&d_curr, &d_next);

  while (i_next < n) {
    /* Mark Mayer et al algorithm that is used here does not operate well if vertex is close
     * to borders of face. In that case,
     * do simple linear interpolation between the two edge vertices */

    /* 'd_next.len' is in fact 'd_curr.len', just avoid copy to begin with */
    if (UNLIKELY(d_next.len < eps)) {
      ix_flag = IS_POINT_IX;
      break;
    }
    if (UNLIKELY(dist_squared_to_line_segment_v2(co, v_curr, v_next) < eps_sq)) {
      ix_flag = IS_SEGMENT_IX;
      break;
    }

    d_curr = d_next;
    DIR_V2_SET(&d_next, v_next, co);
    ht = mean_value_half_tan_v2_db(&d_curr, &d_next);
    w[i_curr] = (d_curr.len == 0.0) ? 0.0f : float((ht_prev + ht) / d_curr.len);
    totweight += w[i_curr];

    /* step */
    i_curr = i_next++;
    v_curr = v_next;
    v_next = v[i_next];

    ht_prev = ht;
  }

  if (ix_flag) {
    memset(w, 0, sizeof(*w) * size_t(n));

    if (ix_flag & IS_POINT_IX) {
      w[i_curr] = 1.0f;
    }
    else {
      float fac = line_point_factor_v2(co, v_curr, v_next);
      CLAMP(fac, 0.0f, 1.0f);
      w[i_curr] = 1.0f - fac;
      w[i_next] = fac;
    }
  }
  else {
    if (totweight != 0.0f) {
      for (i_curr = 0; i_curr < n; i_curr++) {
        w[i_curr] /= totweight;
      }
    }
  }
}

#undef IS_POINT_IX
#undef IS_SEGMENT_IX

#undef DIR_V3_SET
#undef DIR_V2_SET

/** \} */

void interp_cubic_v3(float x[3],
                     float v[3],
                     const float x1[3],
                     const float v1[3],
                     const float x2[3],
                     const float v2[3],
                     const float t)
{
  float a[3], b[3];
  const float t2 = t * t;
  const float t3 = t2 * t;

  /* cubic interpolation */
  a[0] = v1[0] + v2[0] + 2 * (x1[0] - x2[0]);
  a[1] = v1[1] + v2[1] + 2 * (x1[1] - x2[1]);
  a[2] = v1[2] + v2[2] + 2 * (x1[2] - x2[2]);

  b[0] = -2 * v1[0] - v2[0] - 3 * (x1[0] - x2[0]);
  b[1] = -2 * v1[1] - v2[1] - 3 * (x1[1] - x2[1]);
  b[2] = -2 * v1[2] - v2[2] - 3 * (x1[2] - x2[2]);

  x[0] = a[0] * t3 + b[0] * t2 + v1[0] * t + x1[0];
  x[1] = a[1] * t3 + b[1] * t2 + v1[1] * t + x1[1];
  x[2] = a[2] * t3 + b[2] * t2 + v1[2] * t + x1[2];

  v[0] = 3 * a[0] * t2 + 2 * b[0] * t + v1[0];
  v[1] = 3 * a[1] * t2 + 2 * b[1] * t + v1[1];
  v[2] = 3 * a[2] * t2 + 2 * b[2] * t + v1[2];
}

/* unfortunately internal calculations have to be done at double precision
 * to achieve correct/stable results. */

#define IS_ZERO(x) ((x > (-DBL_EPSILON) && x < DBL_EPSILON) ? 1 : 0)

void resolve_tri_uv_v2(
    float r_uv[2], const float st[2], const float st0[2], const float st1[2], const float st2[2])
{
  /* find UV such that
   * t = u * t0 + v * t1 + (1 - u - v) * t2
   * u * (t0 - t2) + v * (t1 - t2) = t - t2 */
  const double a = st0[0] - st2[0], b = st1[0] - st2[0];
  const double c = st0[1] - st2[1], d = st1[1] - st2[1];
  const double det = a * d - c * b;

  /* det should never be zero since the determinant is the signed ST area of the triangle. */
  if (IS_ZERO(det) == 0) {
    const double x[2] = {st[0] - st2[0], st[1] - st2[1]};

    r_uv[0] = float((d * x[0] - b * x[1]) / det);
    r_uv[1] = float(((-c) * x[0] + a * x[1]) / det);
  }
  else {
    zero_v2(r_uv);
  }
}

void resolve_tri_uv_v3(
    float r_uv[2], const float st[3], const float st0[3], const float st1[3], const float st2[3])
{
  float v0[3], v1[3], v2[3];
  double d00, d01, d11, d20, d21, det;

  sub_v3_v3v3(v0, st1, st0);
  sub_v3_v3v3(v1, st2, st0);
  sub_v3_v3v3(v2, st, st0);

  d00 = dot_v3v3(v0, v0);
  d01 = dot_v3v3(v0, v1);
  d11 = dot_v3v3(v1, v1);
  d20 = dot_v3v3(v2, v0);
  d21 = dot_v3v3(v2, v1);

  det = d00 * d11 - d01 * d01;

  /* det should never be zero since the determinant is the signed ST area of the triangle. */
  if (IS_ZERO(det) == 0) {
    float w;

    w = float((d00 * d21 - d01 * d20) / det);
    r_uv[1] = float((d11 * d20 - d01 * d21) / det);
    r_uv[0] = 1.0f - r_uv[1] - w;
  }
  else {
    zero_v2(r_uv);
  }
}

void resolve_quad_uv_v2(float r_uv[2],
                        const float st[2],
                        const float st0[2],
                        const float st1[2],
                        const float st2[2],
                        const float st3[2])
{
  resolve_quad_uv_v2_deriv(r_uv, nullptr, st, st0, st1, st2, st3);
}

void resolve_quad_uv_v2_deriv(float r_uv[2],
                              float r_deriv[2][2],
                              const float st[2],
                              const float st0[2],
                              const float st1[2],
                              const float st2[2],
                              const float st3[2])
{
  const double signed_area = (st0[0] * st1[1] - st0[1] * st1[0]) +
                             (st1[0] * st2[1] - st1[1] * st2[0]) +
                             (st2[0] * st3[1] - st2[1] * st3[0]) +
                             (st3[0] * st0[1] - st3[1] * st0[0]);

  /* X is 2D cross product (determinant)
   * A = (p0 - p) X (p0 - p3) */
  const double a = (st0[0] - st[0]) * (st0[1] - st3[1]) - (st0[1] - st[1]) * (st0[0] - st3[0]);

  /* B = ( (p0 - p) X (p1 - p2) + (p1 - p) X (p0 - p3) ) / 2 */
  const double b =
      0.5 * double(((st0[0] - st[0]) * (st1[1] - st2[1]) - (st0[1] - st[1]) * (st1[0] - st2[0])) +
                   ((st1[0] - st[0]) * (st0[1] - st3[1]) - (st1[1] - st[1]) * (st0[0] - st3[0])));

  /* C = (p1-p) X (p1-p2) */
  const double fC = (st1[0] - st[0]) * (st1[1] - st2[1]) - (st1[1] - st[1]) * (st1[0] - st2[0]);
  double denom = a - 2 * b + fC;

  /* clear outputs */
  zero_v2(r_uv);

  if (IS_ZERO(denom) != 0) {
    const double fDen = a - fC;
    if (IS_ZERO(fDen) == 0) {
      r_uv[0] = float(a / fDen);
    }
  }
  else {
    const double desc_sq = b * b - a * fC;
    const double desc = sqrt(desc_sq < 0.0 ? 0.0 : desc_sq);
    const double s = signed_area > 0 ? (-1.0) : 1.0;

    r_uv[0] = float(((a - b) + s * desc) / denom);
  }

  /* find UV such that
   * fST = (1-u)(1-v) * ST0 + u * (1-v) * ST1 + u * v * ST2 + (1-u) * v * ST3 */
  {
    const double denom_s = (1 - r_uv[0]) * (st0[0] - st3[0]) + r_uv[0] * (st1[0] - st2[0]);
    const double denom_t = (1 - r_uv[0]) * (st0[1] - st3[1]) + r_uv[0] * (st1[1] - st2[1]);
    int i = 0;
    denom = denom_s;

    if (fabs(denom_s) < fabs(denom_t)) {
      i = 1;
      denom = denom_t;
    }

    if (IS_ZERO(denom) == 0) {
      r_uv[1] = float(double((1.0f - r_uv[0]) * (st0[i] - st[i]) + r_uv[0] * (st1[i] - st[i])) /
                      denom);
    }
  }

  if (r_deriv) {
    float tmp1[2], tmp2[2], s[2], t[2];

    /* clear outputs */
    zero_v2(r_deriv[0]);
    zero_v2(r_deriv[1]);

    sub_v2_v2v2(tmp1, st1, st0);
    sub_v2_v2v2(tmp2, st2, st3);
    interp_v2_v2v2(s, tmp1, tmp2, r_uv[1]);
    sub_v2_v2v2(tmp1, st3, st0);
    sub_v2_v2v2(tmp2, st2, st1);
    interp_v2_v2v2(t, tmp1, tmp2, r_uv[0]);

    denom = t[0] * s[1] - t[1] * s[0];

    if (!IS_ZERO(denom)) {
      double inv_denom = 1.0 / denom;
      r_deriv[0][0] = float(double(-t[1]) * inv_denom);
      r_deriv[0][1] = float(double(t[0]) * inv_denom);
      r_deriv[1][0] = float(double(s[1]) * inv_denom);
      r_deriv[1][1] = float(double(-s[0]) * inv_denom);
    }
  }
}

float resolve_quad_u_v2(const float st[2],
                        const float st0[2],
                        const float st1[2],
                        const float st2[2],
                        const float st3[2])
{
  const double signed_area = (st0[0] * st1[1] - st0[1] * st1[0]) +
                             (st1[0] * st2[1] - st1[1] * st2[0]) +
                             (st2[0] * st3[1] - st2[1] * st3[0]) +
                             (st3[0] * st0[1] - st3[1] * st0[0]);

  /* X is 2D cross product (determinant)
   * A = (p0 - p) X (p0 - p3) */
  const double a = (st0[0] - st[0]) * (st0[1] - st3[1]) - (st0[1] - st[1]) * (st0[0] - st3[0]);

  /* B = ( (p0 - p) X (p1 - p2) + (p1 - p) X (p0 - p3) ) / 2 */
  const double b =
      0.5 * double(((st0[0] - st[0]) * (st1[1] - st2[1]) - (st0[1] - st[1]) * (st1[0] - st2[0])) +
                   ((st1[0] - st[0]) * (st0[1] - st3[1]) - (st1[1] - st[1]) * (st0[0] - st3[0])));

  /* C = (p1-p) X (p1-p2) */
  const double fC = (st1[0] - st[0]) * (st1[1] - st2[1]) - (st1[1] - st[1]) * (st1[0] - st2[0]);
  double denom = a - 2 * b + fC;

  if (IS_ZERO(denom) != 0) {
    const double fDen = a - fC;
    if (IS_ZERO(fDen) == 0) {
      return float(a / fDen);
    }

    return 0.0f;
  }

  const double desc_sq = b * b - a * fC;
  const double desc = sqrt(desc_sq < 0.0 ? 0.0 : desc_sq);
  const double s = signed_area > 0 ? (-1.0) : 1.0;

  return float(((a - b) + s * desc) / denom);
}

#undef IS_ZERO

void interp_bilinear_quad_v3(float data[4][3], float u, float v, float res[3])
{
  float vec[3];

  copy_v3_v3(res, data[0]);
  mul_v3_fl(res, (1 - u) * (1 - v));
  copy_v3_v3(vec, data[1]);
  mul_v3_fl(vec, u * (1 - v));
  add_v3_v3(res, vec);
  copy_v3_v3(vec, data[2]);
  mul_v3_fl(vec, u * v);
  add_v3_v3(res, vec);
  copy_v3_v3(vec, data[3]);
  mul_v3_fl(vec, (1 - u) * v);
  add_v3_v3(res, vec);
}

void interp_barycentric_tri_v3(float data[3][3], float u, float v, float res[3])
{
  float vec[3];

  copy_v3_v3(res, data[0]);
  mul_v3_fl(res, u);
  copy_v3_v3(vec, data[1]);
  mul_v3_fl(vec, v);
  add_v3_v3(res, vec);
  copy_v3_v3(vec, data[2]);
  mul_v3_fl(vec, 1.0f - u - v);
  add_v3_v3(res, vec);
}

/***************************** View & Projection *****************************/

void orthographic_m4(float matrix[4][4],
                     const float left,
                     const float right,
                     const float bottom,
                     const float top,
                     const float nearClip,
                     const float farClip)
{
  float Xdelta, Ydelta, Zdelta;

  Xdelta = right - left;
  Ydelta = top - bottom;
  Zdelta = farClip - nearClip;
  if (Xdelta == 0.0f || Ydelta == 0.0f || Zdelta == 0.0f) {
    return;
  }
  unit_m4(matrix);
  matrix[0][0] = 2.0f / Xdelta;
  matrix[3][0] = -(right + left) / Xdelta;
  matrix[1][1] = 2.0f / Ydelta;
  matrix[3][1] = -(top + bottom) / Ydelta;
  matrix[2][2] = -2.0f / Zdelta; /* NOTE: negate Z. */
  matrix[3][2] = -(farClip + nearClip) / Zdelta;
}

void perspective_m4(float mat[4][4],
                    const float left,
                    const float right,
                    const float bottom,
                    const float top,
                    const float nearClip,
                    const float farClip)
{
  const float Xdelta = right - left;
  const float Ydelta = top - bottom;
  const float Zdelta = farClip - nearClip;

  if (Xdelta == 0.0f || Ydelta == 0.0f || Zdelta == 0.0f) {
    return;
  }
  mat[0][0] = nearClip * 2.0f / Xdelta;
  mat[1][1] = nearClip * 2.0f / Ydelta;
  mat[2][0] = (right + left) / Xdelta; /* NOTE: negate Z. */
  mat[2][1] = (top + bottom) / Ydelta;
  mat[2][2] = -(farClip + nearClip) / Zdelta;
  mat[2][3] = -1.0f;
  mat[3][2] = (-2.0f * nearClip * farClip) / Zdelta;
  mat[0][1] = mat[0][2] = mat[0][3] = mat[1][0] = mat[1][2] = mat[1][3] = mat[3][0] = mat[3][1] =
      mat[3][3] = 0.0f;
}

void perspective_m4_fov(float mat[4][4],
                        const float angle_left,
                        const float angle_right,
                        const float angle_up,
                        const float angle_down,
                        const float nearClip,
                        const float farClip)
{
  const float tan_angle_left = tanf(angle_left);
  const float tan_angle_right = tanf(angle_right);
  const float tan_angle_bottom = tanf(angle_up);
  const float tan_angle_top = tanf(angle_down);

  perspective_m4(
      mat, tan_angle_left, tan_angle_right, tan_angle_top, tan_angle_bottom, nearClip, farClip);
  mat[0][0] /= nearClip;
  mat[1][1] /= nearClip;
}

void window_translate_m4(float winmat[4][4], float perspmat[4][4], const float x, const float y)
{
  if (winmat[2][3] == -1.0f) {
    /* in the case of a win-matrix, this means perspective always */
    float v1[3];
    float v2[3];
    float len1, len2;

    v1[0] = perspmat[0][0];
    v1[1] = perspmat[1][0];
    v1[2] = perspmat[2][0];

    v2[0] = perspmat[0][1];
    v2[1] = perspmat[1][1];
    v2[2] = perspmat[2][1];

    len1 = (1.0f / len_v3(v1));
    len2 = (1.0f / len_v3(v2));

    winmat[2][0] -= len1 * winmat[0][0] * x;
    winmat[2][1] -= len2 * winmat[1][1] * y;
  }
  else {
    winmat[3][0] += x;
    winmat[3][1] += y;
  }
}

void planes_from_projmat(const float mat[4][4],
                         float left[4],
                         float right[4],
                         float bottom[4],
                         float top[4],
                         float near[4],
                         float far[4])
{
  /* References:
   *
   * https://fgiesen.wordpress.com/2012/08/31/frustum-planes-from-the-projection-matrix/
   * http://www8.cs.umu.se/kurser/5DV051/HT12/lab/plane_extraction.pdf
   */

  int i;

  if (left) {
    for (i = 4; i--;) {
      left[i] = mat[i][3] + mat[i][0];
    }
  }

  if (right) {
    for (i = 4; i--;) {
      right[i] = mat[i][3] - mat[i][0];
    }
  }

  if (bottom) {
    for (i = 4; i--;) {
      bottom[i] = mat[i][3] + mat[i][1];
    }
  }

  if (top) {
    for (i = 4; i--;) {
      top[i] = mat[i][3] - mat[i][1];
    }
  }

  if (near) {
    for (i = 4; i--;) {
      near[i] = mat[i][3] + mat[i][2];
    }
  }

  if (far) {
    for (i = 4; i--;) {
      far[i] = mat[i][3] - mat[i][2];
    }
  }
}

void projmat_dimensions(const float winmat[4][4],
                        float *r_left,
                        float *r_right,
                        float *r_bottom,
                        float *r_top,
                        float *r_near,
                        float *r_far)
{
  const bool is_persp = winmat[3][3] == 0.0f;
  if (is_persp) {
    const float near = winmat[3][2] / (winmat[2][2] - 1.0f);
    *r_left = near * ((winmat[2][0] - 1.0f) / winmat[0][0]);
    *r_right = near * ((winmat[2][0] + 1.0f) / winmat[0][0]);
    *r_bottom = near * ((winmat[2][1] - 1.0f) / winmat[1][1]);
    *r_top = near * ((winmat[2][1] + 1.0f) / winmat[1][1]);
    *r_near = near;
    *r_far = winmat[3][2] / (winmat[2][2] + 1.0f);
  }
  else {
    *r_left = (-winmat[3][0] - 1.0f) / winmat[0][0];
    *r_right = (-winmat[3][0] + 1.0f) / winmat[0][0];
    *r_bottom = (-winmat[3][1] - 1.0f) / winmat[1][1];
    *r_top = (-winmat[3][1] + 1.0f) / winmat[1][1];
    *r_near = (winmat[3][2] + 1.0f) / winmat[2][2];
    *r_far = (winmat[3][2] - 1.0f) / winmat[2][2];
  }
}

void projmat_dimensions_db(const float winmat_fl[4][4],
                           double *r_left,
                           double *r_right,
                           double *r_bottom,
                           double *r_top,
                           double *r_near,
                           double *r_far)
{
  double winmat[4][4];
  copy_m4d_m4(winmat, winmat_fl);

  const bool is_persp = winmat[3][3] == 0.0f;
  if (is_persp) {
    const double near = winmat[3][2] / (winmat[2][2] - 1.0);
    *r_left = near * ((winmat[2][0] - 1.0) / winmat[0][0]);
    *r_right = near * ((winmat[2][0] + 1.0) / winmat[0][0]);
    *r_bottom = near * ((winmat[2][1] - 1.0) / winmat[1][1]);
    *r_top = near * ((winmat[2][1] + 1.0) / winmat[1][1]);
    *r_near = near;
    *r_far = winmat[3][2] / (winmat[2][2] + 1.0);
  }
  else {
    *r_left = (-winmat[3][0] - 1.0) / winmat[0][0];
    *r_right = (-winmat[3][0] + 1.0) / winmat[0][0];
    *r_bottom = (-winmat[3][1] - 1.0) / winmat[1][1];
    *r_top = (-winmat[3][1] + 1.0) / winmat[1][1];
    *r_near = (winmat[3][2] + 1.0) / winmat[2][2];
    *r_far = (winmat[3][2] - 1.0) / winmat[2][2];
  }
}

void projmat_from_subregion(const float projmat[4][4],
                            const int win_size[2],
                            const int x_min,
                            const int x_max,
                            const int y_min,
                            const int y_max,
                            float r_projmat[4][4])
{
  float rect_width = float(x_max - x_min);
  float rect_height = float(y_max - y_min);

  float x_sca = float(win_size[0]) / rect_width;
  float y_sca = float(win_size[1]) / rect_height;

  float x_fac = float((x_min + x_max) - win_size[0]) / rect_width;
  float y_fac = float((y_min + y_max) - win_size[1]) / rect_height;

  copy_m4_m4(r_projmat, projmat);
  r_projmat[0][0] *= x_sca;
  r_projmat[1][1] *= y_sca;

  if (projmat[3][3] == 0.0f) {
    r_projmat[2][0] = r_projmat[2][0] * x_sca + x_fac;
    r_projmat[2][1] = r_projmat[2][1] * y_sca + y_fac;
  }
  else {
    r_projmat[3][0] = r_projmat[3][0] * x_sca - x_fac;
    r_projmat[3][1] = r_projmat[3][1] * y_sca - y_fac;
  }
}

static void i_multmatrix(const float icand[4][4], float mat[4][4])
{
  int row, col;
  float temp[4][4];

  for (row = 0; row < 4; row++) {
    for (col = 0; col < 4; col++) {
      temp[row][col] = (icand[row][0] * mat[0][col] + icand[row][1] * mat[1][col] +
                        icand[row][2] * mat[2][col] + icand[row][3] * mat[3][col]);
    }
  }
  copy_m4_m4(mat, temp);
}

void polarview_m4(float mat[4][4], float dist, float azimuth, float incidence, float twist)
{
  unit_m4(mat);

  translate_m4(mat, 0.0, 0.0, -dist);
  rotate_m4(mat, 'Z', -twist);
  rotate_m4(mat, 'X', -incidence);
  rotate_m4(mat, 'Z', -azimuth);
}

void lookat_m4(
    float mat[4][4], float vx, float vy, float vz, float px, float py, float pz, float twist)
{
  float sine, cosine, hyp, hyp1, dx, dy, dz;
  float mat1[4][4];

  unit_m4(mat1);

  axis_angle_to_mat4_single(mat, 'Z', -twist);

  dx = px - vx;
  dy = py - vy;
  dz = pz - vz;
  hyp = dx * dx + dz * dz; /* hyp squared */
  hyp1 = sqrtf(dy * dy + hyp);
  hyp = sqrtf(hyp); /* the real hyp */

  if (hyp1 != 0.0f) { /* rotate X */
    sine = -dy / hyp1;
    cosine = hyp / hyp1;
  }
  else {
    sine = 0.0f;
    cosine = 1.0f;
  }
  mat1[1][1] = cosine;
  mat1[1][2] = sine;
  mat1[2][1] = -sine;
  mat1[2][2] = cosine;

  i_multmatrix(mat1, mat);

  mat1[1][1] = mat1[2][2] = 1.0f; /* be careful here to reinit */
  mat1[1][2] = mat1[2][1] = 0.0f; /* those modified by the last */

  /* paragraph */
  if (hyp != 0.0f) { /* rotate Y */
    sine = dx / hyp;
    cosine = -dz / hyp;
  }
  else {
    sine = 0.0f;
    cosine = 1.0f;
  }
  mat1[0][0] = cosine;
  mat1[0][2] = -sine;
  mat1[2][0] = sine;
  mat1[2][2] = cosine;

  i_multmatrix(mat1, mat);
  translate_m4(mat, -vx, -vy, -vz); /* translate viewpoint to origin */
}

int box_clip_bounds_m4(float boundbox[2][3], const float bounds[4], float winmat[4][4])
{
  float mat[4][4], vec[4];
  int a, fl, flag = -1;

  copy_m4_m4(mat, winmat);

  for (a = 0; a < 8; a++) {
    vec[0] = (a & 1) ? boundbox[0][0] : boundbox[1][0];
    vec[1] = (a & 2) ? boundbox[0][1] : boundbox[1][1];
    vec[2] = (a & 4) ? boundbox[0][2] : boundbox[1][2];
    vec[3] = 1.0;
    mul_m4_v4(mat, vec);

    fl = 0;
    if (bounds) {
      if (vec[0] > bounds[1] * vec[3]) {
        fl |= 1;
      }
      if (vec[0] < bounds[0] * vec[3]) {
        fl |= 2;
      }
      if (vec[1] > bounds[3] * vec[3]) {
        fl |= 4;
      }
      if (vec[1] < bounds[2] * vec[3]) {
        fl |= 8;
      }
    }
    else {
      if (vec[0] < -vec[3]) {
        fl |= 1;
      }
      if (vec[0] > vec[3]) {
        fl |= 2;
      }
      if (vec[1] < -vec[3]) {
        fl |= 4;
      }
      if (vec[1] > vec[3]) {
        fl |= 8;
      }
    }
    if (vec[2] < -vec[3]) {
      fl |= 16;
    }
    if (vec[2] > vec[3]) {
      fl |= 32;
    }

    flag &= fl;
    if (flag == 0) {
      return 0;
    }
  }

  return flag;
}

void box_minmax_bounds_m4(float min[3], float max[3], float boundbox[2][3], float mat[4][4])
{
  float mn[3], mx[3], vec[3];
  int a;

  copy_v3_v3(mn, min);
  copy_v3_v3(mx, max);

  for (a = 0; a < 8; a++) {
    vec[0] = (a & 1) ? boundbox[0][0] : boundbox[1][0];
    vec[1] = (a & 2) ? boundbox[0][1] : boundbox[1][1];
    vec[2] = (a & 4) ? boundbox[0][2] : boundbox[1][2];

    mul_m4_v3(mat, vec);
    minmax_v3v3_v3(mn, mx, vec);
  }

  copy_v3_v3(min, mn);
  copy_v3_v3(max, mx);
}

/********************************** Mapping **********************************/

static float snap_coordinate(float u)
{
  /* Adjust a coordinate value `u` to obtain a value inside the (closed) unit interval.
   *   i.e. 0.0 <= snap_coordinate(u) <= 1.0.
   * Due to round-off errors, it is possible that the value of `u` may be close to the boundary of
   * the unit interval, but not exactly on it. In order to handle these cases, `snap_coordinate`
   * checks whether `u` is within `epsilon` of the boundary, and if so, it snaps the return value
   * to the boundary. */
  if (u < 0.0f) {
    u += 1.0f; /* Get back into the unit interval. */
  }
  BLI_assert(0.0f <= u);
  BLI_assert(u <= 1.0f);
  const float epsilon = 0.25f / 65536.0f; /* i.e. Quarter of a texel on a 65536 x 65536 texture. */
  if (u < epsilon) {
    return 0.0f; /* `u` is close to 0, just return 0. */
  }
  if (1.0f - epsilon < u) {
    return 1.0f; /* `u` is close to 1, just return 1. */
  }
  return u;
}

bool map_to_tube(float *r_u, float *r_v, const float x, const float y, const float z)
{
  bool regular = true;
  if (x * x + y * y < 1e-6f * 1e-6f) {
    regular = false; /* We're too close to the cylinder's axis. */
    *r_u = 0.5f;
  }
  else {
    /* The "Regular" case, just compute the coordinate. */
    *r_u = snap_coordinate(atan2f(x, -y) / float(2.0f * M_PI));
  }
  *r_v = (z + 1.0f) / 2.0f;
  return regular;
}

bool map_to_sphere(float *r_u, float *r_v, const float x, const float y, const float z)
{
  bool regular = true;
  const float epsilon = 0.25f / 65536.0f; /* i.e. Quarter of a texel on a 65536 x 65536 texture. */
  const float len_xy = sqrtf(x * x + y * y);
  if (len_xy <= fabsf(z) * epsilon) {
    regular = false; /* We're on the line that runs through the north and south poles. */
    *r_u = 0.5f;
  }
  else {
    /* The "Regular" case, just compute the coordinate. */
    *r_u = snap_coordinate(atan2f(x, -y) / float(2.0f * M_PI));
  }
  *r_v = snap_coordinate(atan2f(len_xy, -z) / float(M_PI));
  return regular;
}

void map_to_plane_v2_v3v3(float r_co[2], const float co[3], const float no[3])
{
  const float target[3] = {0.0f, 0.0f, 1.0f};
  float axis[3];

  cross_v3_v3v3(axis, no, target);
  normalize_v3(axis);

  map_to_plane_axis_angle_v2_v3v3fl(r_co, co, axis, angle_normalized_v3v3(no, target));
}

void map_to_plane_axis_angle_v2_v3v3fl(float r_co[2],
                                       const float co[3],
                                       const float axis[3],
                                       const float angle)
{
  float tmp[3];

  rotate_normalized_v3_v3v3fl(tmp, co, axis, angle);

  copy_v2_v2(r_co, tmp);
}

/********************************* Normals **********************************/

void accumulate_vertex_normals_tri_v3(float n1[3],
                                      float n2[3],
                                      float n3[3],
                                      const float f_no[3],
                                      const float co1[3],
                                      const float co2[3],
                                      const float co3[3])
{
  float vdiffs[3][3];
  const int nverts = 3;

  /* compute normalized edge vectors */
  sub_v3_v3v3(vdiffs[0], co2, co1);
  sub_v3_v3v3(vdiffs[1], co3, co2);
  sub_v3_v3v3(vdiffs[2], co1, co3);

  normalize_v3(vdiffs[0]);
  normalize_v3(vdiffs[1]);
  normalize_v3(vdiffs[2]);

  /* accumulate angle weighted face normal */
  {
    float *vn[] = {n1, n2, n3};
    const float *prev_edge = vdiffs[nverts - 1];
    int i;

    for (i = 0; i < nverts; i++) {
      const float *cur_edge = vdiffs[i];
      const float fac = blender::math::safe_acos_approx(-dot_v3v3(cur_edge, prev_edge));

      /* accumulate */
      madd_v3_v3fl(vn[i], f_no, fac);
      prev_edge = cur_edge;
    }
  }
}

void accumulate_vertex_normals_v3(float n1[3],
                                  float n2[3],
                                  float n3[3],
                                  float n4[3],
                                  const float f_no[3],
                                  const float co1[3],
                                  const float co2[3],
                                  const float co3[3],
                                  const float co4[3])
{
  float vdiffs[4][3];
  const int nverts = (n4 != nullptr && co4 != nullptr) ? 4 : 3;

  /* compute normalized edge vectors */
  sub_v3_v3v3(vdiffs[0], co2, co1);
  sub_v3_v3v3(vdiffs[1], co3, co2);

  if (nverts == 3) {
    sub_v3_v3v3(vdiffs[2], co1, co3);
  }
  else {
    sub_v3_v3v3(vdiffs[2], co4, co3);
    sub_v3_v3v3(vdiffs[3], co1, co4);
    normalize_v3(vdiffs[3]);
  }

  normalize_v3(vdiffs[0]);
  normalize_v3(vdiffs[1]);
  normalize_v3(vdiffs[2]);

  /* accumulate angle weighted face normal */
  {
    float *vn[] = {n1, n2, n3, n4};
    const float *prev_edge = vdiffs[nverts - 1];
    int i;

    for (i = 0; i < nverts; i++) {
      const float *cur_edge = vdiffs[i];
      const float fac = blender::math::safe_acos_approx(-dot_v3v3(cur_edge, prev_edge));

      /* accumulate */
      madd_v3_v3fl(vn[i], f_no, fac);
      prev_edge = cur_edge;
    }
  }
}

void accumulate_vertex_normals_poly_v3(float **vertnos,
                                       const float polyno[3],
                                       const float **vertcos,
                                       float vdiffs[][3],
                                       const int nverts)
{
  int i;

  /* calculate normalized edge directions for each edge in the poly */
  for (i = 0; i < nverts; i++) {
    sub_v3_v3v3(vdiffs[i], vertcos[(i + 1) % nverts], vertcos[i]);
    normalize_v3(vdiffs[i]);
  }

  /* accumulate angle weighted face normal */
  {
    const float *prev_edge = vdiffs[nverts - 1];

    for (i = 0; i < nverts; i++) {
      const float *cur_edge = vdiffs[i];

      /* calculate angle between the two poly edges incident on
       * this vertex */
      const float fac = blender::math::safe_acos_approx(-dot_v3v3(cur_edge, prev_edge));

      /* accumulate */
      madd_v3_v3fl(vertnos[i], polyno, fac);
      prev_edge = cur_edge;
    }
  }
}

/********************************* Tangents **********************************/

void tangent_from_uv_v3(const float uv1[2],
                        const float uv2[2],
                        const float uv3[2],
                        const float co1[3],
                        const float co2[3],
                        const float co3[3],
                        const float n[3],
                        float r_tang[3])
{
  const float s1 = uv2[0] - uv1[0];
  const float s2 = uv3[0] - uv1[0];
  const float t1 = uv2[1] - uv1[1];
  const float t2 = uv3[1] - uv1[1];
  float det = (s1 * t2 - s2 * t1);

  /* otherwise 'r_tang' becomes nan */
  if (det != 0.0f) {
    float tangv[3], ct[3], e1[3], e2[3];

    det = 1.0f / det;

    /* normals in render are inversed... */
    sub_v3_v3v3(e1, co1, co2);
    sub_v3_v3v3(e2, co1, co3);
    r_tang[0] = (t2 * e1[0] - t1 * e2[0]) * det;
    r_tang[1] = (t2 * e1[1] - t1 * e2[1]) * det;
    r_tang[2] = (t2 * e1[2] - t1 * e2[2]) * det;
    tangv[0] = (s1 * e2[0] - s2 * e1[0]) * det;
    tangv[1] = (s1 * e2[1] - s2 * e1[1]) * det;
    tangv[2] = (s1 * e2[2] - s2 * e1[2]) * det;
    cross_v3_v3v3(ct, r_tang, tangv);

    /* check flip */
    if (dot_v3v3(ct, n) < 0.0f) {
      negate_v3(r_tang);
    }
  }
  else {
    zero_v3(r_tang);
  }
}

/****************************** Vector Clouds ********************************/

/* vector clouds */

void vcloud_estimate_transform_v3(const int list_size,
                                  const float (*pos)[3],
                                  const float *weight,
                                  const float (*rpos)[3],
                                  const float *rweight,
                                  float lloc[3],
                                  float rloc[3],
                                  float lrot[3][3],
                                  float lscale[3][3])
{
  float accu_com[3] = {0.0f, 0.0f, 0.0f}, accu_rcom[3] = {0.0f, 0.0f, 0.0f};
  float accu_weight = 0.0f, accu_rweight = 0.0f;
  const float eps = 1e-6f;

  int a;
  /* first set up a nice default response */
  if (lloc) {
    zero_v3(lloc);
  }
  if (rloc) {
    zero_v3(rloc);
  }
  if (lrot) {
    unit_m3(lrot);
  }
  if (lscale) {
    unit_m3(lscale);
  }
  /* do com for both clouds */
  if (pos && rpos && (list_size > 0)) { /* paranoia check */
    /* do com for both clouds */
    for (a = 0; a < list_size; a++) {
      if (weight) {
        float v[3];
        copy_v3_v3(v, pos[a]);
        mul_v3_fl(v, weight[a]);
        add_v3_v3(accu_com, v);
        accu_weight += weight[a];
      }
      else {
        add_v3_v3(accu_com, pos[a]);
      }

      if (rweight) {
        float v[3];
        copy_v3_v3(v, rpos[a]);
        mul_v3_fl(v, rweight[a]);
        add_v3_v3(accu_rcom, v);
        accu_rweight += rweight[a];
      }
      else {
        add_v3_v3(accu_rcom, rpos[a]);
      }
    }
    if (!weight || !rweight) {
      accu_weight = accu_rweight = float(list_size);
    }

    mul_v3_fl(accu_com, 1.0f / accu_weight);
    mul_v3_fl(accu_rcom, 1.0f / accu_rweight);
    if (lloc) {
      copy_v3_v3(lloc, accu_com);
    }
    if (rloc) {
      copy_v3_v3(rloc, accu_rcom);
    }
    if (lrot || lscale) { /* caller does not want rot nor scale, strange but legal */
      /* so now do some reverse engineering and see if we can
       * split rotation from scale -> Polar-decompose. */
      /* build 'projection' matrix */
      float m[3][3], mr[3][3], q[3][3], qi[3][3];
      float va[3], vb[3], stunt[3];
      float odet, ndet;
      int i = 0, imax = 15;
      zero_m3(m);
      zero_m3(mr);

      /* build 'projection' matrix */
      for (a = 0; a < list_size; a++) {
        sub_v3_v3v3(va, rpos[a], accu_rcom);
        // mul_v3_fl(va, bp->mass); /* Mass needs re-normalization here? */
        sub_v3_v3v3(vb, pos[a], accu_com);
        // mul_v3_fl(va, rp->mass);
        m[0][0] += va[0] * vb[0];
        m[0][1] += va[0] * vb[1];
        m[0][2] += va[0] * vb[2];

        m[1][0] += va[1] * vb[0];
        m[1][1] += va[1] * vb[1];
        m[1][2] += va[1] * vb[2];

        m[2][0] += va[2] * vb[0];
        m[2][1] += va[2] * vb[1];
        m[2][2] += va[2] * vb[2];

        /* building the reference matrix on the fly
         * needed to scale properly later */

        mr[0][0] += va[0] * va[0];
        mr[0][1] += va[0] * va[1];
        mr[0][2] += va[0] * va[2];

        mr[1][0] += va[1] * va[0];
        mr[1][1] += va[1] * va[1];
        mr[1][2] += va[1] * va[2];

        mr[2][0] += va[2] * va[0];
        mr[2][1] += va[2] * va[1];
        mr[2][2] += va[2] * va[2];
      }
      copy_m3_m3(q, m);
      stunt[0] = q[0][0];
      stunt[1] = q[1][1];
      stunt[2] = q[2][2];
      /* Re-normalizing for numeric stability. */
      mul_m3_fl(q, 1.0f / len_v3(stunt));

      /* This is pretty much Polar-decompose 'inline' the algorithm based on Higham's thesis
       * without the far case ... but seems to work here pretty neat. */
      odet = 0.0f;
      ndet = determinant_m3_array(q);
      while ((odet - ndet) * (odet - ndet) > eps && i < imax) {
        invert_m3_m3(qi, q);
        transpose_m3(qi);
        add_m3_m3m3(q, q, qi);
        mul_m3_fl(q, 0.5f);
        odet = ndet;
        ndet = determinant_m3_array(q);
        i++;
      }

      if (i) {
        float scale[3][3];
        float irot[3][3];
        if (lrot) {
          copy_m3_m3(lrot, q);
        }
        invert_m3_m3(irot, q);
        invert_m3_m3(qi, mr);
        mul_m3_m3m3(q, m, qi);
        mul_m3_m3m3(scale, irot, q);
        if (lscale) {
          copy_m3_m3(lscale, scale);
        }
      }
    }
  }
}

bool is_edge_convex_v3(const float v1[3],
                       const float v2[3],
                       const float f1_no[3],
                       const float f2_no[3])
{
  if (!equals_v3v3(f1_no, f2_no)) {
    float cross[3];
    float l_dir[3];
    cross_v3_v3v3(cross, f1_no, f2_no);
    /* we assume contiguous normals, otherwise the result isn't meaningful */
    sub_v3_v3v3(l_dir, v2, v1);
    return (dot_v3v3(l_dir, cross) > 0.0f);
  }
  return false;
}

bool is_quad_convex_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3])
{
  /**
   * Method projects points onto a plane and checks its convex using following method:
   *
   * - Create a plane from the cross-product of both diagonal vectors.
   * - Project all points onto the plane.
   * - Subtract for direction vectors.
   * - Return true if all corners cross-products point the direction of the plane.
   */

  /* non-unit length normal, used as a projection plane */
  float plane[3];

  {
    float v13[3], v24[3];

    sub_v3_v3v3(v13, v1, v3);
    sub_v3_v3v3(v24, v2, v4);

    cross_v3_v3v3(plane, v13, v24);

    if (len_squared_v3(plane) < FLT_EPSILON) {
      return false;
    }
  }

  const float *quad_coords[4] = {v1, v2, v3, v4};
  float quad_proj[4][3];

  for (int i = 0; i < 4; i++) {
    project_plane_v3_v3v3(quad_proj[i], quad_coords[i], plane);
  }

  float quad_dirs[4][3];
  for (int i = 0, j = 3; i < 4; j = i++) {
    sub_v3_v3v3(quad_dirs[i], quad_proj[i], quad_proj[j]);
  }

  float test_dir[3];

#define CROSS_SIGN(dir_a, dir_b) \
  ((void)cross_v3_v3v3(test_dir, dir_a, dir_b), (dot_v3v3(plane, test_dir) > 0.0f))

  return (CROSS_SIGN(quad_dirs[0], quad_dirs[1]) && CROSS_SIGN(quad_dirs[1], quad_dirs[2]) &&
          CROSS_SIGN(quad_dirs[2], quad_dirs[3]) && CROSS_SIGN(quad_dirs[3], quad_dirs[0]));

#undef CROSS_SIGN
}

bool is_quad_convex_v2(const float v1[2], const float v2[2], const float v3[2], const float v4[2])
{
  /* Line-tests, the 2 diagonals have to intersect to be convex. */
  return (isect_seg_seg_v2(v1, v3, v2, v4) > 0);
}

bool is_poly_convex_v2(const float verts[][2], uint nr)
{
  uint sign_flag = 0;
  uint a;
  const float *co_curr, *co_prev;
  float dir_curr[2], dir_prev[2];

  co_prev = verts[nr - 1];
  co_curr = verts[0];

  sub_v2_v2v2(dir_prev, verts[nr - 2], co_prev);

  for (a = 0; a < nr; a++) {
    float cross;

    sub_v2_v2v2(dir_curr, co_prev, co_curr);

    cross = cross_v2v2(dir_prev, dir_curr);

    if (cross < 0.0f) {
      sign_flag |= 1;
    }
    else if (cross > 0.0f) {
      sign_flag |= 2;
    }

    if (sign_flag == (1 | 2)) {
      return false;
    }

    copy_v2_v2(dir_prev, dir_curr);

    co_prev = co_curr;
    co_curr += 2;
  }

  return true;
}

int is_quad_flip_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3])
{
  float d_12[3], d_23[3], d_34[3], d_41[3];
  float cross_a[3], cross_b[3];
  int ret = 0;

  sub_v3_v3v3(d_12, v1, v2);
  sub_v3_v3v3(d_23, v2, v3);
  sub_v3_v3v3(d_34, v3, v4);
  sub_v3_v3v3(d_41, v4, v1);

  cross_v3_v3v3(cross_a, d_12, d_23);
  cross_v3_v3v3(cross_b, d_34, d_41);
  ret |= ((dot_v3v3(cross_a, cross_b) < 0.0f) << 0);

  cross_v3_v3v3(cross_a, d_23, d_34);
  cross_v3_v3v3(cross_b, d_41, d_12);
  ret |= ((dot_v3v3(cross_a, cross_b) < 0.0f) << 1);

  return ret;
}

bool is_quad_flip_v3_first_third_fast(const float v1[3],
                                      const float v2[3],
                                      const float v3[3],
                                      const float v4[3])
{
  /* NOTE: if the faces normal has been calculated it's possible to simplify the following checks,
   * however this means the solution may be different depending on the existence of normals
   * causing tessellation to be "unstable" depending on the existence of normals, see #106469. */
  float d_12[3], d_13[3], d_14[3];
  float cross_a[3], cross_b[3];
  sub_v3_v3v3(d_12, v2, v1);
  sub_v3_v3v3(d_13, v3, v1);
  sub_v3_v3v3(d_14, v4, v1);
  cross_v3_v3v3(cross_a, d_12, d_13);
  cross_v3_v3v3(cross_b, d_14, d_13);
  return dot_v3v3(cross_a, cross_b) > 0.0f;
}

float cubic_tangent_factor_circle_v3(const float tan_l[3], const float tan_r[3])
{
  BLI_ASSERT_UNIT_V3(tan_l);
  BLI_ASSERT_UNIT_V3(tan_r);

  /* -7f causes instability/glitches with Bendy Bones + Custom Refs. */
  const float eps = 1e-5f;

  const float tan_dot = dot_v3v3(tan_l, tan_r);
  if (tan_dot > 1.0f - eps) {
    /* no angle difference (use fallback, length won't make any difference) */
    return (1.0f / 3.0f) * 0.75f;
  }
  if (tan_dot < -1.0f + eps) {
    /* Parallel tangents (half-circle). */
    return (1.0f / 2.0f);
  }

  /* non-aligned tangents, calculate handle length */
  const float angle = acosf(tan_dot) / 2.0f;

  /* could also use 'angle_sin = len_vnvn(tan_l, tan_r, dims) / 2.0' */
  const float angle_sin = sinf(angle);
  const float angle_cos = cosf(angle);
  return ((1.0f - angle_cos) / (angle_sin * 2.0f)) / angle_sin;
}

float geodesic_distance_propagate_across_triangle(
    const float v0[3], const float v1[3], const float v2[3], const float dist1, const float dist2)
{
  /* Vectors along triangle edges. */
  float v10[3], v12[3];
  sub_v3_v3v3(v10, v0, v1);
  sub_v3_v3v3(v12, v2, v1);

  if (dist1 != 0.0f && dist2 != 0.0f) {
    /* Local coordinate system in the triangle plane. */
    float u[3], v[3], n[3];
    const float d12 = normalize_v3_v3(u, v12);

    if (d12 * d12 > 0.0f) {
      cross_v3_v3v3(n, v12, v10);
      normalize_v3(n);
      cross_v3_v3v3(v, n, u);

      /* v0 in local coordinates */
      const float v0_[2] = {dot_v3v3(v10, u), fabsf(dot_v3v3(v10, v))};

      /* Compute virtual source point in local coordinates, that we estimate the geodesic
       * distance is being computed from. See figure 9 in the paper for the derivation. */
      const float a = 0.5f * (1.0f + (dist1 * dist1 - dist2 * dist2) / (d12 * d12));
      const float hh = dist1 * dist1 - a * a * d12 * d12;

      if (hh > 0.0f) {
        const float h = sqrtf(hh);
        const float S_[2] = {a * d12, -h};

        /* Only valid if the line between the source point and v0 crosses
         * the edge between v1 and v2. */
        const float x_intercept = S_[0] + h * (v0_[0] - S_[0]) / (v0_[1] + h);
        if (x_intercept >= 0.0f && x_intercept <= d12) {
          return len_v2v2(S_, v0_);
        }
      }
    }
  }

  /* Fall back to Dijsktra approximation in trivial case, or if no valid source
   * point found that connects to v0 across the triangle. */
  return min_ff(dist1 + len_v3(v10), dist2 + len_v3v3(v0, v2));
}
