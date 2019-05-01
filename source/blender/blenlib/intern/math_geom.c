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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: some of this file.
 *
 * */

/** \file
 * \ingroup bli
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_math_bits.h"
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

/**
 * Computes the normal of a planar
 * polygon See Graphics Gems for
 * computing newell normal.
 */
float normal_poly_v3(float n[3], const float verts[][3], unsigned int nr)
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

/* Triangles */
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

float area_poly_v3(const float verts[][3], unsigned int nr)
{
  float n[3];
  cross_poly_v3(n, verts, nr);
  return len_v3(n) * 0.5f;
}

float area_squared_poly_v3(const float verts[][3], unsigned int nr)
{
  float n[3];

  cross_poly_v3(n, verts, nr);
  mul_v3_fl(n, 0.5f);
  return len_squared_v3(n);
}

/**
 * Scalar cross product of a 2d polygon.
 *
 * - equivalent to ``area * 2``
 * - useful for checking polygon winding (a positive value is clockwise).
 */
float cross_poly_v2(const float verts[][2], unsigned int nr)
{
  unsigned int a;
  float cross;
  const float *co_curr, *co_prev;

  /* The Trapezium Area Rule */
  co_prev = verts[nr - 1];
  co_curr = verts[0];
  cross = 0.0f;
  for (a = 0; a < nr; a++) {
    cross += (co_curr[0] - co_prev[0]) * (co_curr[1] + co_prev[1]);
    co_prev = co_curr;
    co_curr += 2;
  }

  return cross;
}

void cross_poly_v3(float n[3], const float verts[][3], unsigned int nr)
{
  const float *v_prev = verts[nr - 1];
  const float *v_curr = verts[0];
  unsigned int i;

  zero_v3(n);

  /* Newell's Method */
  for (i = 0; i < nr; v_prev = v_curr, v_curr = verts[++i]) {
    add_newell_cross_v3_v3v3(n, v_prev, v_curr);
  }
}

float area_poly_v2(const float verts[][2], unsigned int nr)
{
  return fabsf(0.5f * cross_poly_v2(verts, nr));
}

float area_poly_signed_v2(const float verts[][2], unsigned int nr)
{
  return (0.5f * cross_poly_v2(verts, nr));
}

float area_squared_poly_v2(const float verts[][2], unsigned int nr)
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
  else {
    return 0.0f;
  }
}

/********************************* Planes **********************************/

/**
 * Calculate a plane from a point and a direction,
 * \note \a point_no isn't required to be normalized.
 */
void plane_from_point_normal_v3(float r_plane[4], const float plane_co[3], const float plane_no[3])
{
  copy_v3_v3(r_plane, plane_no);
  r_plane[3] = -dot_v3v3(r_plane, plane_co);
}

/**
 * Get a point and a direction from a plane.
 */
void plane_to_point_vector_v3(const float plane[4], float r_plane_co[3], float r_plane_no[3])
{
  mul_v3_v3fl(r_plane_co, plane, (-plane[3] / len_squared_v3(plane)));
  copy_v3_v3(r_plane_no, plane);
}

/**
 * version of #plane_to_point_vector_v3 that gets a unit length vector.
 */
void plane_to_point_vector_v3_normalized(const float plane[4],
                                         float r_plane_co[3],
                                         float r_plane_no[3])
{
  const float length = normalize_v3_v3(r_plane_no, plane);
  mul_v3_v3fl(r_plane_co, r_plane_no, (-plane[3] / length));
}

/********************************* Volume **********************************/

/**
 * The volume from a tetrahedron, points can be in any order
 */
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

/**
 * The volume from a tetrahedron, normal pointing inside gives negative volume
 */
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

/********************************* Distance **********************************/

/* distance p to line v1-v2
 * using Hesse formula, NO LINE PIECE! */
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

/* distance p to line-piece v1-v2 */
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

/* point closest to v1 on line v2-v3 in 2D */
void closest_to_line_segment_v2(float r_close[2],
                                const float p[2],
                                const float l1[2],
                                const float l2[2])
{
  float lambda, cp[2];

  lambda = closest_to_line_v2(cp, p, l1, l2);

  /* flip checks for !finite case (when segment is a point) */
  if (!(lambda > 0.0f)) {
    copy_v2_v2(r_close, l1);
  }
  else if (!(lambda < 1.0f)) {
    copy_v2_v2(r_close, l2);
  }
  else {
    copy_v2_v2(r_close, cp);
  }
}

/* point closest to v1 on line v2-v3 in 3D */
void closest_to_line_segment_v3(float r_close[3],
                                const float p[3],
                                const float l1[3],
                                const float l2[3])
{
  float lambda, cp[3];

  lambda = closest_to_line_v3(cp, p, l1, l2);

  /* flip checks for !finite case (when segment is a point) */
  if (!(lambda > 0.0f)) {
    copy_v3_v3(r_close, l1);
  }
  else if (!(lambda < 1.0f)) {
    copy_v3_v3(r_close, l2);
  }
  else {
    copy_v3_v3(r_close, cp);
  }
}

/**
 * Find the closest point on a plane.
 *
 * \param r_close: Return coordinate
 * \param plane: The plane to test against.
 * \param pt: The point to find the nearest of
 *
 * \note non-unit-length planes are supported.
 */
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

/**
 * Return the signed distance from the point to the plane.
 */
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

/* distance v1 to line-piece l1-l2 in 3D */
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

/**
 * Check if \a p is inside the 2x planes defined by ``(v1, v2, v3)``
 * where the 3x points define 2x planes.
 *
 * \param axis_ref: used when v1,v2,v3 form a line and to check if the corner is concave/convex.
 *
 * \note the distance from \a v1 & \a v3 to \a v2 doesnt matter
 * (it just defines the planes).
 *
 * \return the lowest squared distance to either of the planes.
 * where ``(return < 0.0)`` is outside.
 *
 * <pre>
 *            v1
 *            +
 *           /
 * x - out  /  x - inside
 *         /
 *        +----+
 *        v2   v3
 *           x - also outside
 * </pre>
 */
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

  if ((len_squared_v3(axis) < FLT_EPSILON)) {
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
  else {
    return max_ff(dist_a, dist_b);
  }
}

/**
 * Compute the squared distance of a point to a line (defined as ray).
 * \param ray_origin: A point on the line.
 * \param ray_direction: Normalized direction of the line.
 * \param co: Point to which the distance is to be calculated.
 */
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

/**
 * Find the closest point in a seg to a ray and return the distance squared.
 * \param r_point: Is the point on segment closest to ray
 * (or to ray_origin if the ray and the segment are parallel).
 * \param r_depth: the distance of r_point projection on ray to the ray_origin.
 */
float dist_squared_ray_to_seg_v3(const float ray_origin[3],
                                 const float ray_direction[3],
                                 const float v0[3],
                                 const float v1[3],
                                 float r_point[3],
                                 float *r_depth)
{
  float lambda, depth;
  if (isect_ray_seg_v3(ray_origin, ray_direction, v0, v1, &lambda)) {
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

  return len_squared_v3(dvec) - SQUARE(depth);
}

/* Returns the coordinates of the nearest vertex and
 * the farthest vertex from a plane (or normal). */
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

void dist_squared_ray_to_aabb_v3_precalc(struct DistRayAABB_Precalc *neasrest_precalc,
                                         const float ray_origin[3],
                                         const float ray_direction[3])
{
  copy_v3_v3(neasrest_precalc->ray_origin, ray_origin);
  copy_v3_v3(neasrest_precalc->ray_direction, ray_direction);

  for (int i = 0; i < 3; i++) {
    neasrest_precalc->ray_inv_dir[i] = (neasrest_precalc->ray_direction[i] != 0.0f) ?
                                           (1.0f / neasrest_precalc->ray_direction[i]) :
                                           FLT_MAX;
  }
}

/**
 * Returns the distance from a ray to a bound-box (projected on ray)
 */
float dist_squared_ray_to_aabb_v3(const struct DistRayAABB_Precalc *data,
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
                                         const float bbmin[3],
                                         const float bbmax[3],
                                         float r_point[3],
                                         float *r_depth)
{
  struct DistRayAABB_Precalc data;
  dist_squared_ray_to_aabb_v3_precalc(&data, ray_origin, ray_direction);
  return dist_squared_ray_to_aabb_v3(&data, bbmin, bbmax, r_point, r_depth);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name dist_squared_to_projected_aabb and helpers
 * \{ */

/**
 * \param projmat: Projection Matrix (usually perspective
 * matrix multiplied by object matrix).
 */
void dist_squared_to_projected_aabb_precalc(struct DistProjectedAABBPrecalc *precalc,
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
          projmat_trans[0], projmat_trans[1], projmat_trans[3], precalc->ray_origin)) {
    /* Orthographic projection. */
    isect_plane_plane_v3(px, py, precalc->ray_origin, precalc->ray_direction);
  }
  else {
    /* Perspective projection. */
    cross_v3_v3v3(precalc->ray_direction, py, px);
    //normalize_v3(precalc->ray_direction);
  }
#else
  if (!isect_plane_plane_v3(px, py, precalc->ray_origin, precalc->ray_direction)) {
    /* Matrix with weird coplanar planes. Undetermined origin.*/
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

/* Returns the distance from a 2d coordinate to a BoundBox (Projected) */
float dist_squared_to_projected_aabb(struct DistProjectedAABBPrecalc *data,
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
  struct DistProjectedAABBPrecalc data;
  dist_squared_to_projected_aabb_precalc(&data, projmat, winsize, mval);

  bool dummy[3] = {true, true, true};
  return dist_squared_to_projected_aabb(&data, bbmin, bbmax, dummy);
}
/** \} */

/* Adapted from "Real-Time Collision Detection" by Christer Ericson,
 * published by Morgan Kaufmann Publishers, copyright 2005 Elsevier Inc.
 *
 * Set 'r' to the point in triangle (a, b, c) closest to point 'p' */
void closest_on_tri_to_point_v3(
    float r[3], const float p[3], const float a[3], const float b[3], const float c[3])
{
  float ab[3], ac[3], ap[3], d1, d2;
  float bp[3], d3, d4, vc, cp[3], d5, d6, vb, va;
  float denom, v, w;

  /* Check if P in vertex region outside A */
  sub_v3_v3v3(ab, b, a);
  sub_v3_v3v3(ac, c, a);
  sub_v3_v3v3(ap, p, a);
  d1 = dot_v3v3(ab, ap);
  d2 = dot_v3v3(ac, ap);
  if (d1 <= 0.0f && d2 <= 0.0f) {
    /* barycentric coordinates (1,0,0) */
    copy_v3_v3(r, a);
    return;
  }

  /* Check if P in vertex region outside B */
  sub_v3_v3v3(bp, p, b);
  d3 = dot_v3v3(ab, bp);
  d4 = dot_v3v3(ac, bp);
  if (d3 >= 0.0f && d4 <= d3) {
    /* barycentric coordinates (0,1,0) */
    copy_v3_v3(r, b);
    return;
  }
  /* Check if P in edge region of AB, if so return projection of P onto AB */
  vc = d1 * d4 - d3 * d2;
  if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
    v = d1 / (d1 - d3);
    /* barycentric coordinates (1-v,v,0) */
    madd_v3_v3v3fl(r, a, ab, v);
    return;
  }
  /* Check if P in vertex region outside C */
  sub_v3_v3v3(cp, p, c);
  d5 = dot_v3v3(ab, cp);
  d6 = dot_v3v3(ac, cp);
  if (d6 >= 0.0f && d5 <= d6) {
    /* barycentric coordinates (0,0,1) */
    copy_v3_v3(r, c);
    return;
  }
  /* Check if P in edge region of AC, if so return projection of P onto AC */
  vb = d5 * d2 - d1 * d6;
  if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
    w = d2 / (d2 - d6);
    /* barycentric coordinates (1-w,0,w) */
    madd_v3_v3v3fl(r, a, ac, w);
    return;
  }
  /* Check if P in edge region of BC, if so return projection of P onto BC */
  va = d3 * d6 - d5 * d4;
  if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
    w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    /* barycentric coordinates (0,1-w,w) */
    sub_v3_v3v3(r, c, b);
    mul_v3_fl(r, w);
    add_v3_v3(r, b);
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
  madd_v3_v3v3fl(r, a, ab, v);
  /* a + ab * v + ac * w */
  add_v3_v3(r, ac);
}

/******************************* Intersection ********************************/

/* intersect Line-Line, shorts */
int isect_seg_seg_v2_int(const int v1[2], const int v2[2], const int v3[2], const int v4[2])
{
  float div, lambda, mu;

  div = (float)((v2[0] - v1[0]) * (v4[1] - v3[1]) - (v2[1] - v1[1]) * (v4[0] - v3[0]));
  if (div == 0.0f) {
    return ISECT_LINE_LINE_COLINEAR;
  }

  lambda = (float)((v1[1] - v3[1]) * (v4[0] - v3[0]) - (v1[0] - v3[0]) * (v4[1] - v3[1])) / div;

  mu = (float)((v1[1] - v3[1]) * (v2[0] - v1[0]) - (v1[0] - v3[0]) * (v2[1] - v1[1])) / div;

  if (lambda >= 0.0f && lambda <= 1.0f && mu >= 0.0f && mu <= 1.0f) {
    if (lambda == 0.0f || lambda == 1.0f || mu == 0.0f || mu == 1.0f) {
      return ISECT_LINE_LINE_EXACT;
    }
    return ISECT_LINE_LINE_CROSS;
  }
  return ISECT_LINE_LINE_NONE;
}

/* intersect Line-Line, floats - gives intersection point */
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
  else {
    return ISECT_LINE_LINE_COLINEAR;
  }
}

/* intersect Line-Line, floats */
int isect_seg_seg_v2(const float v1[2], const float v2[2], const float v3[2], const float v4[2])
{
  float div, lambda, mu;

  div = (v2[0] - v1[0]) * (v4[1] - v3[1]) - (v2[1] - v1[1]) * (v4[0] - v3[0]);
  if (div == 0.0f) {
    return ISECT_LINE_LINE_COLINEAR;
  }

  lambda = ((float)(v1[1] - v3[1]) * (v4[0] - v3[0]) - (v1[0] - v3[0]) * (v4[1] - v3[1])) / div;

  mu = ((float)(v1[1] - v3[1]) * (v2[0] - v1[0]) - (v1[0] - v3[0]) * (v2[1] - v1[1])) / div;

  if (lambda >= 0.0f && lambda <= 1.0f && mu >= 0.0f && mu <= 1.0f) {
    if (lambda == 0.0f || lambda == 1.0f || mu == 0.0f || mu == 1.0f) {
      return ISECT_LINE_LINE_EXACT;
    }
    return ISECT_LINE_LINE_CROSS;
  }
  return ISECT_LINE_LINE_NONE;
}

/* Returns a point on each segment that is closest to the other. */
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

/**
 * Get intersection point of two 2D segments.
 *
 * \param endpoint_bias: Bias to use when testing for end-point overlap.
 * A positive value considers intersections that extend past the endpoints,
 * negative values contract the endpoints.
 * Note the bias is applied to a 0-1 factor, not scaled to the length of segments.
 *
 * \returns intersection type:
 * - -1: collinear.
 * -  1: intersection.
 * -  0: no intersection.
 */
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
       * see T45123 */

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
  else {
    if ((cross_v2v2(s10, s30) == 0.0f) && (cross_v2v2(s32, s30) == 0.0f)) {
      /* equal lines */
      float s20[2];
      float u_a, u_b;

      if (equals_v2v2(v0, v1)) {
        if (len_squared_v2v2(v2, v3) > SQUARE(eps)) {
          /* use non-point segment as basis */
          SWAP(const float *, v0, v2);
          SWAP(const float *, v1, v3);

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
        SWAP(float, u_a, u_b);
      }

      if (u_a > endpoint_max || u_b < endpoint_min) {
        /* non-overlapping segments */
        return -1;
      }
      else if (max_ff(0.0f, u_a) == min_ff(1.0f, u_b)) {
        /* one common point: can return result */
        madd_v2_v2v2fl(r_vi, v0, s10, max_ff(0, u_a));
        return 1;
      }
    }

    /* lines are collinear */
    return -1;
  }
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

/**
 * \param l1, l2: Coordinates (point of line).
 * \param sp, r:  Coordinate and radius (sphere).
 * \return r_p1, r_p2: Intersection coordinates.
 *
 * \note The order of assignment for intersection points (\a r_p1, \a r_p2) is predictable,
 * based on the direction defined by ``l2 - l1``,
 * this direction compared with the normal of each point on the sphere:
 * \a r_p1 always has a >= 0.0 dot product.
 * \a r_p2 always has a <= 0.0 dot product.
 * For example, when \a l1 is inside the sphere and \a l2 is outside,
 * \a r_p1 will always be between \a l1 and \a l2.
 */
int isect_line_sphere_v3(const float l1[3],
                         const float l2[3],
                         const float sp[3],
                         const float r,
                         float r_p1[3],
                         float r_p2[3])
{
  /* adapted for use in blender by Campbell Barton - 2011
   *
   * atelier iebele abel - 2001
   * atelier@iebele.nl
   * http://www.iebele.nl
   *
   * sphere_line_intersection function adapted from:
   * http://astronomy.swin.edu.au/pbourke/geometry/sphereline
   * Paul Bourke pbourke@swin.edu.au
   */

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
  else if (i == 0.0f) {
    /* one intersection */
    mu = -b / (2.0f * a);
    madd_v3_v3v3fl(r_p1, l1, ldir, mu);
    return 1;
  }
  else if (i > 0.0f) {
    const float i_sqrt = sqrtf(i); /* avoid calc twice */

    /* first intersection */
    mu = (-b + i_sqrt) / (2.0f * a);
    madd_v3_v3v3fl(r_p1, l1, ldir, mu);

    /* second intersection */
    mu = (-b - i_sqrt) / (2.0f * a);
    madd_v3_v3v3fl(r_p2, l1, ldir, mu);
    return 2;
  }
  else {
    /* math domain error - nan */
    return -1;
  }
}

/* keep in sync with isect_line_sphere_v3 */
int isect_line_sphere_v2(const float l1[2],
                         const float l2[2],
                         const float sp[2],
                         const float r,
                         float r_p1[2],
                         float r_p2[2])
{
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
  else if (i == 0.0f) {
    /* one intersection */
    mu = -b / (2.0f * a);
    madd_v2_v2v2fl(r_p1, l1, ldir, mu);
    return 1;
  }
  else if (i > 0.0f) {
    const float i_sqrt = sqrtf(i); /* avoid calc twice */

    /* first intersection */
    mu = (-b + i_sqrt) / (2.0f * a);
    madd_v2_v2v2fl(r_p1, l1, ldir, mu);

    /* second intersection */
    mu = (-b - i_sqrt) / (2.0f * a);
    madd_v2_v2v2fl(r_p2, l1, ldir, mu);
    return 2;
  }
  else {
    /* math domain error - nan */
    return -1;
  }
}

/* point in polygon (keep float and int versions in sync) */
bool isect_point_poly_v2(const float pt[2],
                         const float verts[][2],
                         const unsigned int nr,
                         const bool UNUSED(use_holes))
{
  unsigned int i, j;
  bool isect = false;
  for (i = 0, j = nr - 1; i < nr; j = i++) {
    if (((verts[i][1] > pt[1]) != (verts[j][1] > pt[1])) &&
        (pt[0] <
         (verts[j][0] - verts[i][0]) * (pt[1] - verts[i][1]) / (verts[j][1] - verts[i][1]) +
             verts[i][0])) {
      isect = !isect;
    }
  }
  return isect;
}
bool isect_point_poly_v2_int(const int pt[2],
                             const int verts[][2],
                             const unsigned int nr,
                             const bool UNUSED(use_holes))
{
  unsigned int i, j;
  bool isect = false;
  for (i = 0, j = nr - 1; i < nr; j = i++) {
    if (((verts[i][1] > pt[1]) != (verts[j][1] > pt[1])) &&
        (pt[0] <
         (verts[j][0] - verts[i][0]) * (pt[1] - verts[i][1]) / (verts[j][1] - verts[i][1]) +
             verts[i][0])) {
      isect = !isect;
    }
  }
  return isect;
}

/* point in tri */

/* only single direction */
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
  if (line_point_side_v2(v1, v2, pt) >= 0.0f) {
    if (line_point_side_v2(v2, v3, pt) >= 0.0f) {
      if (line_point_side_v2(v3, v1, pt) >= 0.0f) {
        return 1;
      }
    }
  }
  else {
    if (!(line_point_side_v2(v2, v3, pt) >= 0.0f)) {
      if (!(line_point_side_v2(v3, v1, pt) >= 0.0f)) {
        return -1;
      }
    }
  }

  return 0;
}

/* point in quad - only convex quads */
int isect_point_quad_v2(
    const float pt[2], const float v1[2], const float v2[2], const float v3[2], const float v4[2])
{
  if (line_point_side_v2(v1, v2, pt) >= 0.0f) {
    if (line_point_side_v2(v2, v3, pt) >= 0.0f) {
      if (line_point_side_v2(v3, v4, pt) >= 0.0f) {
        if (line_point_side_v2(v4, v1, pt) >= 0.0f) {
          return 1;
        }
      }
    }
  }
  else {
    if (!(line_point_side_v2(v2, v3, pt) >= 0.0f)) {
      if (!(line_point_side_v2(v3, v4, pt) >= 0.0f)) {
        if (!(line_point_side_v2(v4, v1, pt) >= 0.0f)) {
          return -1;
        }
      }
    }
  }

  return 0;
}

/* moved from effect.c
 * test if the line starting at p1 ending at p2 intersects the triangle v0..v2
 * return non zero if it does
 */
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

/* like isect_line_segment_tri_v3, but allows epsilon tolerance around triangle */
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

/* moved from effect.c
 * test if the ray starting at p1 going in d direction intersects the triangle v0..v2
 * return non zero if it does
 */
bool isect_ray_tri_v3(const float ray_origin[3],
                      const float ray_direction[3],
                      const float v0[3],
                      const float v1[3],
                      const float v2[3],
                      float *r_lambda,
                      float r_uv[2])
{
  /* note: these values were 0.000001 in 2.4x but for projection snapping on
   * a human head (1BU == 1m), subsurf level 2, this gave many errors - campbell */
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
  if ((*r_lambda < 0.0f)) {
    return false;
  }

  if (r_uv) {
    r_uv[0] = u;
    r_uv[1] = v;
  }

  return true;
}

/**
 * if clip is nonzero, will only return true if lambda is >= 0.0
 * (i.e. intersection point is along positive \a ray_direction)
 *
 * \note #line_plane_factor_v3() shares logic.
 */
bool isect_ray_plane_v3(const float ray_origin[3],
                        const float ray_direction[3],
                        const float plane[4],
                        float *r_lambda,
                        const bool clip)
{
  float h[3], plane_co[3];
  float dot;

  dot = dot_v3v3(plane, ray_direction);
  if (dot == 0.0f) {
    return false;
  }
  mul_v3_v3fl(plane_co, plane, (-plane[3] / len_squared_v3(plane)));
  sub_v3_v3v3(h, ray_origin, plane_co);
  *r_lambda = -dot_v3v3(plane, h) / dot;
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
  if ((*r_lambda < 0.0f)) {
    return false;
  }

  if (r_uv) {
    r_uv[0] = u;
    r_uv[1] = v;
  }

  return true;
}

void isect_ray_tri_watertight_v3_precalc(struct IsectRayPrecalc *isect_precalc,
                                         const float ray_direction[3])
{
  float inv_dir_z;

  /* Calculate dimension where the ray direction is maximal. */
  int kz = axis_dominant_v3_single(ray_direction);
  int kx = (kz != 2) ? (kz + 1) : 0;
  int ky = (kx != 2) ? (kx + 1) : 0;

  /* Swap kx and ky dimensions to preserve winding direction of triangles. */
  if (ray_direction[kz] < 0.0f) {
    SWAP(int, kx, ky);
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
                                 const struct IsectRayPrecalc *isect_precalc,
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
  else {
    /* Calculate scaled z-coordinates of vertices and use them to calculate
     * the hit distance.
     */
    const int sign_det = (float_as_int(det) & (int)0x80000000);
    const float t = (u * a_kz + v * b_kz + w * c_kz) * sz;
    const float sign_t = xor_fl(t, sign_det);
    if ((sign_t < 0.0f)
    /* Differ from Cycles, don't read r_lambda's original value
     * otherwise we won't match any of the other intersect functions here...
     * which would be confusing. */
#if 0
        || (sign_T > *r_lambda * xor_signmask(det, sign_mask))
#endif
    ) {
      return false;
    }
    else {
      /* Normalize u, v and t. */
      const float inv_det = 1.0f / det;
      if (r_uv) {
        r_uv[0] = u * inv_det;
        r_uv[1] = v * inv_det;
      }
      *r_lambda = t * inv_det;
      return true;
    }
  }
}

bool isect_ray_tri_watertight_v3_simple(const float ray_origin[3],
                                        const float ray_direction[3],
                                        const float v0[3],
                                        const float v1[3],
                                        const float v2[3],
                                        float *r_lambda,
                                        float r_uv[2])
{
  struct IsectRayPrecalc isect_precalc;
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
  if ((a > -epsilon) && (a < epsilon))
    return false;
  f = 1.0f / a;

  sub_v3_v3v3(s, ray_origin, v0);

  cross_v3_v3v3(q, s, e1);
  *r_lambda = f * dot_v3v3(e2, q);
  if ((*r_lambda < 0.0f))
    return false;

  u = f * dot_v3v3(s, p);
  v = f * dot_v3v3(ray_direction, q);

  if (u > 0 && v > 0 && u + v > 1) {
    float t = (u + v - 1) / 2;
    du = u - t;
    dv = v - t;
  }
  else {
    if (u < 0)
      du = u;
    else if (u > 1)
      du = u - 1;
    else
      du = 0.0f;

    if (v < 0)
      dv = v;
    else if (v > 1)
      dv = v - 1;
    else
      dv = 0.0f;
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
    float p[2] = {(ray_direction[0] * v) / det, (ray_direction[1] * v) / det};

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

bool isect_ray_seg_v3(const float ray_origin[3],
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
    /* the lines are parallel.*/
    return false;
  }

  float c[3], cray[3];
  sub_v3_v3v3(c, n, t);
  cross_v3_v3v3(cray, c, ray_direction);

  *r_lambda = dot_v3v3(cray, n) / nlen;

  return true;
}

/**
 * Check if a point is behind all planes.
 */
bool isect_point_planes_v3(float (*planes)[4], int totplane, const float p[3])
{
  int i;

  for (i = 0; i < totplane; i++) {
    if (plane_point_side_v3(planes[i], p) > 0.0f) {
      return false;
    }
  }

  return true;
}

/**
 * Check if a point is in front all planes.
 * Same as isect_point_planes_v3 but with planes facing the opposite direction.
 */
bool isect_point_planes_v3_negated(const float (*planes)[4], const int totplane, const float p[3])
{
  for (int i = 0; i < totplane; i++) {
    if (plane_point_side_v3(planes[i], p) <= 0.0f) {
      return false;
    }
  }

  return true;
}

/**
 * Intersect line/plane.
 *
 * \param r_isect_co: The intersection point.
 * \param l1: The first point of the line.
 * \param l2: The second point of the line.
 * \param plane_co: A point on the plane to intersect with.
 * \param plane_no: The direction of the plane (does not need to be normalized).
 *
 * \note #line_plane_factor_v3() shares logic.
 */
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
  else {
    /* The segment is parallel to plane */
    return false;
  }
}

/**
 * Intersect three planes, return the point where all 3 meet.
 * See Graphics Gems 1 pg 305
 *
 * \param plane_a, plane_b, plane_c: Planes.
 * \param r_isect_co: The resulting intersection point.
 */
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
  else {
    return false;
  }
}

/**
 * Intersect two planes, return a point on the intersection and a vector
 * that runs on the direction of the intersection.
 * \note this is a slightly reduced version of #isect_plane_plane_plane_v3
 *
 * \param plane_a, plane_b: Planes.
 * \param r_isect_co: The resulting intersection point.
 * \param r_isect_no: The resulting vector of the intersection.
 *
 * \note \a r_isect_no isn't unit length.
 */
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
  else {
    return false;
  }
}

/**
 * Intersect two triangles.
 *
 * \param r_i1, r_i2: Optional arguments to retrieve the overlapping edge between the 2 triangles.
 * \return true when the triangles intersect.
 *
 * \note intersections between coplanar triangles are currently undetected.
 */
bool isect_tri_tri_epsilon_v3(const float t_a0[3],
                              const float t_a1[3],
                              const float t_a2[3],
                              const float t_b0[3],
                              const float t_b1[3],
                              const float t_b2[3],
                              float r_i1[3],
                              float r_i2[3],
                              const float epsilon)
{
  const float *tri_pair[2][3] = {{t_a0, t_a1, t_a2}, {t_b0, t_b1, t_b2}};
  float plane_a[4], plane_b[4];
  float plane_co[3], plane_no[3];

  BLI_assert((r_i1 != NULL) == (r_i2 != NULL));

  /* normalizing is needed for small triangles T46007 */
  normal_tri_v3(plane_a, UNPACK3(tri_pair[0]));
  normal_tri_v3(plane_b, UNPACK3(tri_pair[1]));

  plane_a[3] = -dot_v3v3(plane_a, t_a0);
  plane_b[3] = -dot_v3v3(plane_b, t_b0);

  if (isect_plane_plane_v3(plane_a, plane_b, plane_co, plane_no) &&
      (normalize_v3(plane_no) > epsilon)) {
    /**
     * Implementation note: its simpler to project the triangles onto the intersection plane
     * before intersecting their edges with the ray, defined by 'isect_plane_plane_v3'.
     * This way we can use 'line_point_factor_v3_ex' to see if an edge crosses 'co_proj',
     * then use the factor to calculate the world-space point.
     */
    struct {
      float min, max;
    } range[2] = {{FLT_MAX, -FLT_MAX}, {FLT_MAX, -FLT_MAX}};
    int t;
    float co_proj[3];

    closest_to_plane3_normalized_v3(co_proj, plane_no, plane_co);

    /* For both triangles, find the overlap with the line defined by the ray [co_proj, plane_no].
     * When the ranges overlap we know the triangles do too. */
    for (t = 0; t < 2; t++) {
      int j, j_prev;
      float tri_proj[3][3];

      closest_to_plane3_normalized_v3(tri_proj[0], plane_no, tri_pair[t][0]);
      closest_to_plane3_normalized_v3(tri_proj[1], plane_no, tri_pair[t][1]);
      closest_to_plane3_normalized_v3(tri_proj[2], plane_no, tri_pair[t][2]);

      for (j = 0, j_prev = 2; j < 3; j_prev = j++) {
        /* note that its important to have a very small nonzero epsilon here
         * otherwise this fails for very small faces.
         * However if its too small, large adjacent faces will count as intersecting */
        const float edge_fac = line_point_factor_v3_ex(
            co_proj, tri_proj[j_prev], tri_proj[j], 1e-10f, -1.0f);
        /* ignore collinear lines, they are either an edge shared between 2 tri's
         * (which runs along [co_proj, plane_no], but can be safely ignored).
         *
         * or a collinear edge placed away from the ray -
         * which we don't intersect with & can ignore. */
        if (UNLIKELY(edge_fac == -1.0f)) {
          /* pass */
        }
        else if (edge_fac > 0.0f && edge_fac < 1.0f) {
          float ix_tri[3];
          float span_fac;

          interp_v3_v3v3(ix_tri, tri_pair[t][j_prev], tri_pair[t][j], edge_fac);
          /* the actual distance, since 'plane_no' is normalized */
          span_fac = dot_v3v3(plane_no, ix_tri);

          range[t].min = min_ff(range[t].min, span_fac);
          range[t].max = max_ff(range[t].max, span_fac);
        }
      }

      if (range[t].min == FLT_MAX) {
        return false;
      }
    }

    if (((range[0].min > range[1].max) || (range[0].max < range[1].min)) == 0) {
      if (r_i1 && r_i2) {
        project_plane_normalized_v3_v3v3(plane_co, plane_co, plane_no);
        madd_v3_v3v3fl(r_i1, plane_co, plane_no, max_ff(range[0].min, range[1].min));
        madd_v3_v3v3fl(r_i2, plane_co, plane_no, min_ff(range[0].max, range[1].max));
      }

      return true;
    }
  }

  return false;
}

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
      SWAP(float, r1, r2);
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

/**
 * Checks status of an AABB in relation to a list of planes.
 *
 * \returns intersection type:
 * - ISECT_AABB_PLANE_BEHIND_ONE   (0): AABB is completely behind at least 1 plane;
 * - ISECT_AABB_PLANE_CROSS_ANY    (1): AABB intersects at least 1 plane;
 * - ISECT_AABB_PLANE_IN_FRONT_ALL (2): AABB is completely in front of all planes;
 */
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
    else if ((ret != ISECT_AABB_PLANE_CROSS_ANY) &&
             (plane_point_side_v3(planes[i], bb_near) < 0.0f)) {
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
      SWAP(float, t0, t1);
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
      //(((unsigned int)z)& ~(((unsigned int)x)|((unsigned int)y))) & 0x80000000) {
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
  sub_v3_v3v3(e3, v2, v1); /* wasnt yet calculated */

  /*e1*/
  sub_v3_v3v3(bv, v0, p1);

  elen2 = dot_v3v3(e1, e1);
  edotv = dot_v3v3(e1, vel);
  edotbv = dot_v3v3(e1, bv);

  a = elen2 * (-dot_v3v3(vel, vel)) + edotv * edotv;
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

  /*e2*/
  /*bv is same*/
  elen2 = dot_v3v3(e2, e2);
  edotv = dot_v3v3(e2, vel);
  edotbv = dot_v3v3(e2, bv);

  a = elen2 * (-dot_v3v3(vel, vel)) + edotv * edotv;
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

  /*e3*/
  /* sub_v3_v3v3(bv, v0, p1); */   /* UNUSED */
  /* elen2 = dot_v3v3(e1, e1); */  /* UNUSED */
  /* edotv = dot_v3v3(e1, vel); */ /* UNUSED */
  /* edotbv = dot_v3v3(e1, bv); */ /* UNUSED */

  sub_v3_v3v3(bv, v1, p1);
  elen2 = dot_v3v3(e3, e3);
  edotv = dot_v3v3(e3, vel);
  edotbv = dot_v3v3(e3, bv);

  a = elen2 * (-dot_v3v3(vel, vel)) + edotv * edotv;
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

/**
 * \return The number of point of interests
 * 0 - lines are collinear
 * 1 - lines are coplanar, i1 is set to intersection
 * 2 - i1 and i2 are the nearest points on line 1 (v1, v2) and line 2 (v3, v4) respectively
 */
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

  /* important not to use an epsilon here, see: T45919 */
  /* test zero length line */
  if (UNLIKELY(div == 0.0f)) {
    return 0;
  }
  /* test if the two lines are coplanar */
  else if (UNLIKELY(fabsf(d) <= epsilon)) {
    cross_v3_v3v3(cb, c, b);

    mul_v3_fl(a, dot_v3v3(cb, ab) / div);
    add_v3_v3v3(r_i1, v1, a);
    copy_v3_v3(r_i2, r_i1);

    return 1; /* one intersection only */
  }
  /* if not */
  else {
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

    /* for the second line, just substract the offset from the first intersection point */
    sub_v3_v3v3(r_i2, r_i1, t);

    return 2; /* two nearest points */
  }
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

/** Intersection point strictly between the two lines
 * \return false when no intersection is found
 */
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

  /* important not to use an epsilon here, see: T45919 */
  /* test zero length line */
  if (UNLIKELY(div == 0.0f)) {
    return false;
  }
  /* test if the two lines are coplanar */
  else if (UNLIKELY(fabsf(d) < epsilon)) {
    return false;
  }
  else {
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
    else {
      return false;
    }
  }
}

bool isect_aabb_aabb_v3(const float min1[3],
                        const float max1[3],
                        const float min2[3],
                        const float max2[3])
{
  return (min1[0] < max2[0] && min1[1] < max2[1] && min1[2] < max2[2] && min2[0] < max1[0] &&
          min2[1] < max1[1] && min2[2] < max1[2]);
}

void isect_ray_aabb_v3_precalc(struct IsectRayAABB_Precalc *data,
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

/* Adapted from http://www.gamedev.net/community/forums/topic.asp?topic_id=459973 */
bool isect_ray_aabb_v3(const struct IsectRayAABB_Precalc *data,
                       const float bb_min[3],
                       const float bb_max[3],
                       float *tmin_out)
{
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

  /* Note: tmax does not need to be updated since we don't use it
   * keeping this here for future reference - jwilkins */
  // if (tzmax < tmax) tmax = tzmax;

  if (tmin_out) {
    (*tmin_out) = tmin;
  }

  return true;
}

/**
 * Test a bounding box (AABB) for ray intersection.
 * Assumes the ray is already local to the boundbox space.
 *
 * \note: \a direction should be normalized
 * if you intend to use the \a tmin or \a tmax distance results!
 */
bool isect_ray_aabb_v3_simple(const float orig[3],
                              const float dir[3],
                              const float bb_min[3],
                              const float bb_max[3],
                              float *tmin,
                              float *tmax)
{
  double t[6];
  float hit_dist[2];
  const double invdirx = (dir[0] > 1e-35f || dir[0] < -1e-35f) ? 1.0 / (double)dir[0] : DBL_MAX;
  const double invdiry = (dir[1] > 1e-35f || dir[1] < -1e-35f) ? 1.0 / (double)dir[1] : DBL_MAX;
  const double invdirz = (dir[2] > 1e-35f || dir[2] < -1e-35f) ? 1.0 / (double)dir[2] : DBL_MAX;
  t[0] = (double)(bb_min[0] - orig[0]) * invdirx;
  t[1] = (double)(bb_max[0] - orig[0]) * invdirx;
  t[2] = (double)(bb_min[1] - orig[1]) * invdiry;
  t[3] = (double)(bb_max[1] - orig[1]) * invdiry;
  t[4] = (double)(bb_min[2] - orig[2]) * invdirz;
  t[5] = (double)(bb_max[2] - orig[2]) * invdirz;
  hit_dist[0] = (float)fmax(fmax(fmin(t[0], t[1]), fmin(t[2], t[3])), fmin(t[4], t[5]));
  hit_dist[1] = (float)fmin(fmin(fmax(t[0], t[1]), fmax(t[2], t[3])), fmax(t[4], t[5]));
  if ((hit_dist[1] < 0.0f || hit_dist[0] > hit_dist[1])) {
    return false;
  }
  else {
    if (tmin) {
      *tmin = hit_dist[0];
    }
    if (tmax) {
      *tmax = hit_dist[1];
    }
    return true;
  }
}

/* find closest point to p on line through (l1, l2) and return lambda,
 * where (0 <= lambda <= 1) when cp is in the line segment (l1, l2)
 */
float closest_to_line_v3(float r_close[3], const float p[3], const float l1[3], const float l2[3])
{
  float h[3], u[3], lambda;
  sub_v3_v3v3(u, l2, l1);
  sub_v3_v3v3(h, p, l1);
  lambda = dot_v3v3(u, h) / dot_v3v3(u, u);
  r_close[0] = l1[0] + u[0] * lambda;
  r_close[1] = l1[1] + u[1] * lambda;
  r_close[2] = l1[2] + u[2] * lambda;
  return lambda;
}

float closest_to_line_v2(float r_close[2], const float p[2], const float l1[2], const float l2[2])
{
  float h[2], u[2], lambda;
  sub_v2_v2v2(u, l2, l1);
  sub_v2_v2v2(h, p, l1);
  lambda = dot_v2v2(u, h) / dot_v2v2(u, u);
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

/**
 * A simplified version of #closest_to_line_v3
 * we only need to return the ``lambda``
 *
 * \param epsilon: avoid approaching divide-by-zero.
 * Passing a zero will just check for nonzero division.
 */
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

/**
 * \note #isect_line_plane_v3() shares logic
 */
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

/**
 * Ensure the distance between these points is no greater than 'dist'.
 * If it is, scale then both into the center.
 */
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

/*
 *     x1,y2
 *     |  \
 *     |   \     .(a,b)
 *     |    \
 *     x1,y1-- x2,y1
 */
int isect_point_tri_v2_int(
    const int x1, const int y1, const int x2, const int y2, const int a, const int b)
{
  float v1[2], v2[2], v3[2], p[2];

  v1[0] = (float)x1;
  v1[1] = (float)y1;

  v2[0] = (float)x1;
  v2[1] = (float)y2;

  v3[0] = (float)x2;
  v3[1] = (float)y1;

  p[0] = (float)a;
  p[1] = (float)b;

  return isect_point_tri_v2(p, v1, v2, v3);
}

static bool point_in_slice(const float p[3],
                           const float v1[3],
                           const float l1[3],
                           const float l2[3])
{
  /*
   * what is a slice ?
   * some maths:
   * a line including (l1, l2) and a point not on the line
   * define a subset of R3 delimited by planes parallel to the line and orthogonal
   * to the (point --> line) distance vector, one plane on the line one on the point,
   * the room inside usually is rather small compared to R3 though still infinite
   * useful for restricting (speeding up) searches
   * e.g. all points of triangular prism are within the intersection of 3 'slices'
   * another trivial case : cube
   * but see a 'spat' which is a deformed cube with paired parallel planes needs only 3 slices too
   */
  float h, rp[3], cp[3], q[3];

  closest_to_line_v3(cp, v1, l1, l2);
  sub_v3_v3v3(q, cp, v1);

  sub_v3_v3v3(rp, p, v1);
  h = dot_v3v3(q, rp) / dot_v3v3(q, q);
  /* note: when 'h' is nan/-nan, this check returns false
   * without explicit check - covering the degenerate case */
  return (h >= 0.0f && h <= 1.0f);
}

/* adult sister defining the slice planes by the origin and the normal
 * NOTE |normal| may not be 1 but defining the thickness of the slice */
static bool point_in_slice_as(float p[3], float origin[3], float normal[3])
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

/**
 * \param r_isect_co: The point \a p projected onto the triangle.
 * \return True when \a p is inside the triangle.
 * \note Its up to the caller to check the distance between \a p and \a r_vi
 * against an error margin.
 */
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
  else {
    return false;
  }
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
    else if (t > 0.0f) {
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
    else if (t > div) {
      const float p1_copy[3] = {UNPACK3(p1)};
      copy_v3_v3(r_p1, p1);
      madd_v3_v3v3fl(r_p2, p1_copy, dp, t / div);
      return true;
    }
  }

  /* incase input/output values match (above also) */
  const float p1_copy[3] = {UNPACK3(p1)};
  copy_v3_v3(r_p2, p2);
  copy_v3_v3(r_p1, p1_copy);
  return true;
}

bool clip_segment_v3_plane_n(const float p1[3],
                             const float p2[3],
                             const float plane_array[][4],
                             const int plane_tot,
                             float r_p1[3],
                             float r_p2[3])
{
  /* intersect from both directions */
  float p1_fac = 0.0f, p2_fac = 1.0f;

  float dp[3];
  sub_v3_v3v3(dp, p2, p1);

  for (int i = 0; i < plane_tot; i++) {
    const float *plane = plane_array[i];
    const float div = dot_v3v3(dp, plane);

    if (div != 0.0f) {
      float t = -plane_point_side_v3(plane, p1);
      if (div > 0.0f) {
        /* clip p1 lower bounds */
        if (t >= div) {
          return false;
        }
        else if (t > 0.0f) {
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
        else if (t > div) {
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

  /* incase input/output values match */
  const float p1_copy[3] = {UNPACK3(p1)};

  madd_v3_v3v3fl(r_p1, p1_copy, dp, p1_fac);
  madd_v3_v3v3fl(r_p2, p1_copy, dp, p2_fac);

  return true;
}

/****************************** Axis Utils ********************************/

/**
 * \brief Normal to x,y matrix
 *
 * Creates a 3x3 matrix from a normal.
 * This matrix can be applied to vectors so their 'z' axis runs along \a normal.
 * In practice it means you can use x,y as 2d coords. \see
 *
 * \param r_mat: The matrix to return.
 * \param normal: A unit length vector.
 */
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

/**
 * Same as axis_dominant_v3_to_m3, but flips the normal
 */
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

/* return 1 when degenerate */
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

  if (fabsf(wtot) > FLT_EPSILON) {
    mul_v3_fl(w, 1.0f / wtot);
    return false;
  }
  else {
    /* zero area triangle */
    copy_v3_fl(w, 1.0f / 3.0f);
    return true;
  }
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

  w[0] = w[1] = w[2] = w[3] = 0.0f;

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
    bool degenerate;

    sub_v3_v3v3(n1, v1, v3);
    sub_v3_v3v3(n2, v2, v4);
    cross_v3_v3v3(n, n1, n2);

    degenerate = barycentric_weights(v1, v2, v4, co, n, w);
    SWAP(float, w[2], w[3]);

    if (degenerate || (w[0] < 0.0f)) {
      /* if w[1] is negative, co is on the other side of the v1-v3 edge,
       * so we interpolate using the other triangle */
      degenerate = barycentric_weights(v2, v3, v4, co, n, w2);

      if (!degenerate) {
        w[0] = 0.0f;
        w[1] = w2[0];
        w[2] = w2[1];
        w[3] = w2[2];
      }
    }
  }
}

/**
 * \return
 * - 0 if the point is outside of triangle.
 * - 1 if the point is inside triangle.
 * - 2 if it's on the edge.
 * */
int barycentric_inside_triangle_v2(const float w[3])
{
  if (IN_RANGE(w[0], 0.0f, 1.0f) && IN_RANGE(w[1], 0.0f, 1.0f) && IN_RANGE(w[2], 0.0f, 1.0f)) {
    return 1;
  }
  else if (IN_RANGE_INCL(w[0], 0.0f, 1.0f) && IN_RANGE_INCL(w[1], 0.0f, 1.0f) &&
           IN_RANGE_INCL(w[2], 0.0f, 1.0f)) {
    return 2;
  }

  return 0;
}

/* returns 0 for degenerated triangles */
bool barycentric_coords_v2(
    const float v1[2], const float v2[2], const float v3[2], const float co[2], float w[3])
{
  const float x = co[0], y = co[1];
  const float x1 = v1[0], y1 = v1[1];
  const float x2 = v2[0], y2 = v2[1];
  const float x3 = v3[0], y3 = v3[1];
  const float det = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3);

  if (fabsf(det) > FLT_EPSILON) {
    w[0] = ((y2 - y3) * (x - x3) + (x3 - x2) * (y - y3)) / det;
    w[1] = ((y3 - y1) * (x - x3) + (x1 - x3) * (y - y3)) / det;
    w[2] = 1.0f - w[0] - w[1];

    return true;
  }

  return false;
}

/**
 * \note: using #cross_tri_v2 means locations outside the triangle are correctly weighted
 *
 * \note This is *exactly* the same calculation as #resolve_tri_uv_v2,
 * although it has double precision and is used for texture baking, so keep both.
 */
void barycentric_weights_v2(
    const float v1[2], const float v2[2], const float v3[2], const float co[2], float w[3])
{
  float wtot;

  w[0] = cross_tri_v2(v2, v3, co);
  w[1] = cross_tri_v2(v3, v1, co);
  w[2] = cross_tri_v2(v1, v2, co);
  wtot = w[0] + w[1] + w[2];

  if (wtot != 0.0f) {
    mul_v3_fl(w, 1.0f / wtot);
  }
  else { /* dummy values for zero area face */
    copy_v3_fl(w, 1.0f / 3.0f);
  }
}

/**
 * A version of #barycentric_weights_v2 that doesn't allow negative weights.
 * Useful when negative values cause problems and points are only
 * ever slightly outside of the triangle.
 */
void barycentric_weights_v2_clamped(
    const float v1[2], const float v2[2], const float v3[2], const float co[2], float w[3])
{
  float wtot;

  w[0] = max_ff(cross_tri_v2(v2, v3, co), 0.0f);
  w[1] = max_ff(cross_tri_v2(v3, v1, co), 0.0f);
  w[2] = max_ff(cross_tri_v2(v1, v2, co), 0.0f);
  wtot = w[0] + w[1] + w[2];

  if (wtot != 0.0f) {
    mul_v3_fl(w, 1.0f / wtot);
  }
  else { /* dummy values for zero area face */
    copy_v3_fl(w, 1.0f / 3.0f);
  }
}

/**
 * still use 2D X,Y space but this works for verts transformed by a perspective matrix,
 * using their 4th component as a weight
 */
void barycentric_weights_v2_persp(
    const float v1[4], const float v2[4], const float v3[4], const float co[2], float w[3])
{
  float wtot;

  w[0] = cross_tri_v2(v2, v3, co) / v1[3];
  w[1] = cross_tri_v2(v3, v1, co) / v2[3];
  w[2] = cross_tri_v2(v1, v2, co) / v3[3];
  wtot = w[0] + w[1] + w[2];

  if (wtot != 0.0f) {
    mul_v3_fl(w, 1.0f / wtot);
  }
  else { /* dummy values for zero area face */
    w[0] = w[1] = w[2] = 1.0f / 3.0f;
  }
}

/**
 * same as #barycentric_weights_v2 but works with a quad,
 * note: untested for values outside the quad's bounds
 * this is #interp_weights_poly_v2 expanded for quads only
 */
void barycentric_weights_v2_quad(const float v1[2],
                                 const float v2[2],
                                 const float v3[2],
                                 const float v4[2],
                                 const float co[2],
                                 float w[4])
{
  /* note: fabsf() here is not needed for convex quads (and not used in interp_weights_poly_v2).
   * but in the case of concave/bow-tie quads for the mask rasterizer it gives unreliable results
   * without adding absf(). If this becomes an issue for more general usage we could have
   * this optional or use a different function - Campbell */
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

    if (wtot != 0.0f) {
      mul_v4_fl(w, 1.0f / wtot);
    }
    else { /* dummy values for zero area face */
      copy_v4_fl(w, 1.0f / 4.0f);
    }
  }
}

/* given 2 triangles in 3D space, and a point in relation to the first triangle.
 * calculate the location of a point in relation to the second triangle.
 * Useful for finding relative positions with geometry */
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

/**
 * Simply re-interpolates,
 * assumes p_src is between \a l_src_p1-l_src_p2
 */
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

/* given an array with some invalid values this function interpolates valid values
 * replacing the invalid ones */
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
  else if (found_invalid == 0) {
    return 0;
  }
  else {
    /* found invalid depths, interpolate */
    float valid_last = skipval;
    int valid_ofs = 0;

    float *array_up = MEM_callocN(sizeof(float) * (size_t)list_size, "interp_sparse_array up");
    float *array_down = MEM_callocN(sizeof(float) * (size_t)list_size, "interp_sparse_array up");

    int *ofs_tot_up = MEM_callocN(sizeof(int) * (size_t)list_size, "interp_sparse_array tup");
    int *ofs_tot_down = MEM_callocN(sizeof(int) * (size_t)list_size, "interp_sparse_array tdown");

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
          array[i] = ((array_up[i] * (float)ofs_tot_down[i]) +
                      (array_down[i] * (float)ofs_tot_up[i])) /
                     (float)(ofs_tot_down[i] + ofs_tot_up[i]);
        }
        else if (array_up[i] != skipval) {
          array[i] = array_up[i];
        }
        else if (array_down[i] != skipval) {
          array[i] = array_down[i];
        }
      }
    }

    MEM_freeN(array_up);
    MEM_freeN(array_down);

    MEM_freeN(ofs_tot_up);
    MEM_freeN(ofs_tot_down);
  }

  return 1;
}

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
    sub_v2_v2v2((d_len)->dir, va, vb); \
    (d_len)->len = len_v2((d_len)->dir); \
  } \
  (void)0

struct Float3_Len {
  float dir[3], len;
};

struct Float2_Len {
  float dir[2], len;
};

/* Mean value weights - smooth interpolation weights for polygons with
 * more than 3 vertices */
static float mean_value_half_tan_v3(const struct Float3_Len *d_curr,
                                    const struct Float3_Len *d_next)
{
  float cross[3], area;
  cross_v3_v3v3(cross, d_curr->dir, d_next->dir);
  area = len_v3(cross);
  if (LIKELY(fabsf(area) > FLT_EPSILON)) {
    const float dot = dot_v3v3(d_curr->dir, d_next->dir);
    const float len = d_curr->len * d_next->len;
    return (len - dot) / area;
  }
  else {
    return 0.0f;
  }
}

static float mean_value_half_tan_v2(const struct Float2_Len *d_curr,
                                    const struct Float2_Len *d_next)
{
  float area;
  /* different from the 3d version but still correct */
  area = cross_v2v2(d_curr->dir, d_next->dir);
  if (LIKELY(fabsf(area) > FLT_EPSILON)) {
    const float dot = dot_v2v2(d_curr->dir, d_next->dir);
    const float len = d_curr->len * d_next->len;
    return (len - dot) / area;
  }
  else {
    return 0.0f;
  }
}

void interp_weights_poly_v3(float *w, float v[][3], const int n, const float co[3])
{
  const float eps = 1e-5f; /* take care, low values cause [#36105] */
  const float eps_sq = eps * eps;
  const float *v_curr, *v_next;
  float ht_prev, ht; /* half tangents */
  float totweight = 0.0f;
  int i_curr, i_next;
  char ix_flag = 0;
  struct Float3_Len d_curr, d_next;

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

    /* 'd_next.len' is infact 'd_curr.len', just avoid copy to begin with */
    if (UNLIKELY(d_next.len < eps)) {
      ix_flag = IS_POINT_IX;
      break;
    }
    else if (UNLIKELY(dist_squared_to_line_segment_v3(co, v_curr, v_next) < eps_sq)) {
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
    memset(w, 0, sizeof(*w) * (size_t)n);

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
  const float eps = 1e-5f; /* take care, low values cause [#36105] */
  const float eps_sq = eps * eps;
  const float *v_curr, *v_next;
  float ht_prev, ht; /* half tangents */
  float totweight = 0.0f;
  int i_curr, i_next;
  char ix_flag = 0;
  struct Float2_Len d_curr, d_next;

  /* loop over 'i_next' */
  i_curr = n - 1;
  i_next = 0;

  v_curr = v[i_curr];
  v_next = v[i_next];

  DIR_V2_SET(&d_curr, v_curr - 2 /* v[n - 2] */, co);
  DIR_V2_SET(&d_next, v_curr /* v[n - 1] */, co);
  ht_prev = mean_value_half_tan_v2(&d_curr, &d_next);

  while (i_next < n) {
    /* Mark Mayer et al algorithm that is used here does not operate well if vertex is close
     * to borders of face. In that case,
     * do simple linear interpolation between the two edge vertices */

    /* 'd_next.len' is infact 'd_curr.len', just avoid copy to begin with */
    if (UNLIKELY(d_next.len < eps)) {
      ix_flag = IS_POINT_IX;
      break;
    }
    else if (UNLIKELY(dist_squared_to_line_segment_v2(co, v_curr, v_next) < eps_sq)) {
      ix_flag = IS_SEGMENT_IX;
      break;
    }

    d_curr = d_next;
    DIR_V2_SET(&d_next, v_next, co);
    ht = mean_value_half_tan_v2(&d_curr, &d_next);
    w[i_curr] = (ht_prev + ht) / d_curr.len;
    totweight += w[i_curr];

    /* step */
    i_curr = i_next++;
    v_curr = v_next;
    v_next = v[i_next];

    ht_prev = ht;
  }

  if (ix_flag) {
    memset(w, 0, sizeof(*w) * (size_t)n);

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

/* (x1, v1)(t1=0)------(x2, v2)(t2=1), 0<t<1 --> (x, v)(t) */
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

/**
 * Barycentric reverse
 *
 * Compute coordinates (u, v) for point \a st with respect to triangle (\a st0, \a st1, \a st2)
 *
 * \note same basic result as #barycentric_weights_v2, see it's comment for details.
 */
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

    r_uv[0] = (float)((d * x[0] - b * x[1]) / det);
    r_uv[1] = (float)(((-c) * x[0] + a * x[1]) / det);
  }
  else {
    zero_v2(r_uv);
  }
}

/**
 * Barycentric reverse 3d
 *
 * Compute coordinates (u, v) for point \a st with respect to triangle (\a st0, \a st1, \a st2)
 */
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

    w = (float)((d00 * d21 - d01 * d20) / det);
    r_uv[1] = (float)((d11 * d20 - d01 * d21) / det);
    r_uv[0] = 1.0f - r_uv[1] - w;
  }
  else {
    zero_v2(r_uv);
  }
}

/* bilinear reverse */
void resolve_quad_uv_v2(float r_uv[2],
                        const float st[2],
                        const float st0[2],
                        const float st1[2],
                        const float st2[2],
                        const float st3[2])
{
  resolve_quad_uv_v2_deriv(r_uv, NULL, st, st0, st1, st2, st3);
}

/* bilinear reverse with derivatives */
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
   * A = (p0 - p) X (p0 - p3)*/
  const double a = (st0[0] - st[0]) * (st0[1] - st3[1]) - (st0[1] - st[1]) * (st0[0] - st3[0]);

  /* B = ( (p0 - p) X (p1 - p2) + (p1 - p) X (p0 - p3) ) / 2 */
  const double b = 0.5 * (double)(((st0[0] - st[0]) * (st1[1] - st2[1]) -
                                   (st0[1] - st[1]) * (st1[0] - st2[0])) +
                                  ((st1[0] - st[0]) * (st0[1] - st3[1]) -
                                   (st1[1] - st[1]) * (st0[0] - st3[0])));

  /* C = (p1-p) X (p1-p2) */
  const double fC = (st1[0] - st[0]) * (st1[1] - st2[1]) - (st1[1] - st[1]) * (st1[0] - st2[0]);
  double denom = a - 2 * b + fC;

  /* clear outputs */
  zero_v2(r_uv);

  if (IS_ZERO(denom) != 0) {
    const double fDen = a - fC;
    if (IS_ZERO(fDen) == 0) {
      r_uv[0] = (float)(a / fDen);
    }
  }
  else {
    const double desc_sq = b * b - a * fC;
    const double desc = sqrt(desc_sq < 0.0 ? 0.0 : desc_sq);
    const double s = signed_area > 0 ? (-1.0) : 1.0;

    r_uv[0] = (float)(((a - b) + s * desc) / denom);
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
      r_uv[1] = (float)((double)((1.0f - r_uv[0]) * (st0[i] - st[i]) +
                                 r_uv[0] * (st1[i] - st[i])) /
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
      r_deriv[0][0] = (float)((double)-t[1] * inv_denom);
      r_deriv[0][1] = (float)((double)t[0] * inv_denom);
      r_deriv[1][0] = (float)((double)s[1] * inv_denom);
      r_deriv[1][1] = (float)((double)-s[0] * inv_denom);
    }
  }
}

/* a version of resolve_quad_uv_v2 that only calculates the 'u' */
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
   * A = (p0 - p) X (p0 - p3)*/
  const double a = (st0[0] - st[0]) * (st0[1] - st3[1]) - (st0[1] - st[1]) * (st0[0] - st3[0]);

  /* B = ( (p0 - p) X (p1 - p2) + (p1 - p) X (p0 - p3) ) / 2 */
  const double b = 0.5 * (double)(((st0[0] - st[0]) * (st1[1] - st2[1]) -
                                   (st0[1] - st[1]) * (st1[0] - st2[0])) +
                                  ((st1[0] - st[0]) * (st0[1] - st3[1]) -
                                   (st1[1] - st[1]) * (st0[0] - st3[0])));

  /* C = (p1-p) X (p1-p2) */
  const double fC = (st1[0] - st[0]) * (st1[1] - st2[1]) - (st1[1] - st[1]) * (st1[0] - st2[0]);
  double denom = a - 2 * b + fC;

  if (IS_ZERO(denom) != 0) {
    const double fDen = a - fC;
    if (IS_ZERO(fDen) == 0) {
      return (float)(a / fDen);
    }
    else {
      return 0.0f;
    }
  }
  else {
    const double desc_sq = b * b - a * fC;
    const double desc = sqrt(desc_sq < 0.0 ? 0.0 : desc_sq);
    const double s = signed_area > 0 ? (-1.0) : 1.0;

    return (float)(((a - b) + s * desc) / denom);
  }
}

#undef IS_ZERO

/* reverse of the functions above */
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

/**
 * Matches `glOrtho` result.
 */
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
  matrix[2][2] = -2.0f / Zdelta; /* note: negate Z */
  matrix[3][2] = -(farClip + nearClip) / Zdelta;
}

/**
 * Matches `glFrustum` result.
 */
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
  mat[2][0] = (right + left) / Xdelta; /* note: negate Z */
  mat[2][1] = (top + bottom) / Ydelta;
  mat[2][2] = -(farClip + nearClip) / Zdelta;
  mat[2][3] = -1.0f;
  mat[3][2] = (-2.0f * nearClip * farClip) / Zdelta;
  mat[0][1] = mat[0][2] = mat[0][3] = mat[1][0] = mat[1][2] = mat[1][3] = mat[3][0] = mat[3][1] =
      mat[3][3] = 0.0f;
}

/* translate a matrix created by orthographic_m4 or perspective_m4 in XY coords
 * (used to jitter the view) */
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

    winmat[2][0] += len1 * winmat[0][0] * x;
    winmat[2][1] += len2 * winmat[1][1] * y;
  }
  else {
    winmat[3][0] += x;
    winmat[3][1] += y;
  }
}

/**
 * Frustum planes extraction from a projection matrix
 * (homogeneous 4d vector representations of planes).
 *
 * plane parameters can be NULL if you do not need them.
 */
void planes_from_projmat(float mat[4][4],
                         float left[4],
                         float right[4],
                         float top[4],
                         float bottom[4],
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

void projmat_dimensions(const float projmat[4][4],
                        float *r_left,
                        float *r_right,
                        float *r_bottom,
                        float *r_top,
                        float *r_near,
                        float *r_far)
{
  bool is_persp = projmat[3][3] == 0.0f;

  if (is_persp) {
    *r_left = (projmat[2][0] - 1.0f) / projmat[0][0];
    *r_right = (projmat[2][0] + 1.0f) / projmat[0][0];
    *r_bottom = (projmat[2][1] - 1.0f) / projmat[1][1];
    *r_top = (projmat[2][1] + 1.0f) / projmat[1][1];
    *r_near = projmat[3][2] / (projmat[2][2] - 1.0f);
    *r_far = projmat[3][2] / (projmat[2][2] + 1.0f);
  }
  else {
    *r_left = (-projmat[3][0] - 1.0f) / projmat[0][0];
    *r_right = (-projmat[3][0] + 1.0f) / projmat[0][0];
    *r_bottom = (-projmat[3][1] - 1.0f) / projmat[1][1];
    *r_top = (-projmat[3][1] + 1.0f) / projmat[1][1];
    *r_near = (projmat[3][2] + 1.0f) / projmat[2][2];
    *r_far = (projmat[3][2] - 1.0f) / projmat[2][2];
  }
}

static void i_multmatrix(float icand[4][4], float Vm[4][4])
{
  int row, col;
  float temp[4][4];

  for (row = 0; row < 4; row++) {
    for (col = 0; col < 4; col++) {
      temp[row][col] = (icand[row][0] * Vm[0][col] + icand[row][1] * Vm[1][col] +
                        icand[row][2] * Vm[2][col] + icand[row][3] * Vm[3][col]);
    }
  }
  copy_m4_m4(Vm, temp);
}

void polarview_m4(float Vm[4][4], float dist, float azimuth, float incidence, float twist)
{
  unit_m4(Vm);

  translate_m4(Vm, 0.0, 0.0, -dist);
  rotate_m4(Vm, 'Z', -twist);
  rotate_m4(Vm, 'X', -incidence);
  rotate_m4(Vm, 'Z', -azimuth);
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

void map_to_tube(float *r_u, float *r_v, const float x, const float y, const float z)
{
  float len;

  *r_v = (z + 1.0f) / 2.0f;

  len = sqrtf(x * x + y * y);
  if (len > 0.0f) {
    *r_u = (1.0f - (atan2f(x / len, y / len) / (float)M_PI)) / 2.0f;
  }
  else {
    *r_v = *r_u = 0.0f; /* to avoid un-initialized variables */
  }
}

void map_to_sphere(float *r_u, float *r_v, const float x, const float y, const float z)
{
  float len;

  len = sqrtf(x * x + y * y + z * z);
  if (len > 0.0f) {
    if (UNLIKELY(x == 0.0f && y == 0.0f)) {
      *r_u = 0.0f; /* othwise domain error */
    }
    else {
      *r_u = (1.0f - atan2f(x, y) / (float)M_PI) / 2.0f;
    }

    *r_v = 1.0f - saacos(z / len) / (float)M_PI;
  }
  else {
    *r_v = *r_u = 0.0f; /* to avoid un-initialized variables */
  }
}

void map_to_plane_v2_v3v3(float r_co[2], const float co[3], const float no[3])
{
  float target[3] = {0.0f, 0.0f, 1.0f};
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
      const float fac = saacos(-dot_v3v3(cur_edge, prev_edge));

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
  const int nverts = (n4 != NULL && co4 != NULL) ? 4 : 3;

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
      const float fac = saacos(-dot_v3v3(cur_edge, prev_edge));

      /* accumulate */
      madd_v3_v3fl(vn[i], f_no, fac);
      prev_edge = cur_edge;
    }
  }
}

/* Add weighted face normal component into normals of the face vertices.
 * Caller must pass pre-allocated vdiffs of nverts length. */
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
      const float fac = saacos(-dot_v3v3(cur_edge, prev_edge));

      /* accumulate */
      madd_v3_v3fl(vertnos[i], polyno, fac);
      prev_edge = cur_edge;
    }
  }
}

/********************************* Tangents **********************************/

void tangent_from_uv_v3(const float uv1[2],
                        const float uv2[2],
                        const float uv3[3],
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
/**
 * input
 *
 * \param list_size: 4 lists as pointer to array[list_size]
 * \param pos: current pos array of 'new' positions
 * \param weight: current weight array of 'new'weights (may be NULL pointer if you have no weights)
 * \param rpos: Reference rpos array of 'old' positions
 * \param rweight: Reference rweight array of 'old'weights
 * (may be NULL pointer if you have no weights).
 *
 * output
 *
 * \param lloc: Center of mass pos.
 * \param rloc: Center of mass rpos.
 * \param lrot: Rotation matrix.
 * \param lscale: Scale matrix.
 *
 * pointers may be NULL if not needed
 */

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
  if (pos && rpos && (list_size > 0)) { /* paranoya check */
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
      accu_weight = accu_rweight = (float)list_size;
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
       * split rotation from scale -> Polardecompose */
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
        /* mul_v3_fl(va, bp->mass);  mass needs renormalzation here ?? */
        sub_v3_v3v3(vb, pos[a], accu_com);
        /* mul_v3_fl(va, rp->mass); */
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
      /* renormalizing for numeric stability */
      mul_m3_fl(q, 1.f / len_v3(stunt));

      /* this is pretty much Polardecompose 'inline' the algo based on Higham's thesis */
      /* without the far case ... but seems to work here pretty neat                   */
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

/******************************* Form Factor *********************************/

static void vec_add_dir(float r[3], const float v1[3], const float v2[3], const float fac)
{
  r[0] = v1[0] + fac * (v2[0] - v1[0]);
  r[1] = v1[1] + fac * (v2[1] - v1[1]);
  r[2] = v1[2] + fac * (v2[2] - v1[2]);
}

bool form_factor_visible_quad(const float p[3],
                              const float n[3],
                              const float v0[3],
                              const float v1[3],
                              const float v2[3],
                              float q0[3],
                              float q1[3],
                              float q2[3],
                              float q3[3])
{
  static const float epsilon = 1e-6f;
  float sd[3];
  const float c = dot_v3v3(n, p);

  /* signed distances from the vertices to the plane. */
  sd[0] = dot_v3v3(n, v0) - c;
  sd[1] = dot_v3v3(n, v1) - c;
  sd[2] = dot_v3v3(n, v2) - c;

  if (fabsf(sd[0]) < epsilon) {
    sd[0] = 0.0f;
  }
  if (fabsf(sd[1]) < epsilon) {
    sd[1] = 0.0f;
  }
  if (fabsf(sd[2]) < epsilon) {
    sd[2] = 0.0f;
  }

  if (sd[0] > 0.0f) {
    if (sd[1] > 0.0f) {
      if (sd[2] > 0.0f) {
        /* +++ */
        copy_v3_v3(q0, v0);
        copy_v3_v3(q1, v1);
        copy_v3_v3(q2, v2);
        copy_v3_v3(q3, q2);
      }
      else if (sd[2] < 0.0f) {
        /* ++- */
        copy_v3_v3(q0, v0);
        copy_v3_v3(q1, v1);
        vec_add_dir(q2, v1, v2, (sd[1] / (sd[1] - sd[2])));
        vec_add_dir(q3, v0, v2, (sd[0] / (sd[0] - sd[2])));
      }
      else {
        /* ++0 */
        copy_v3_v3(q0, v0);
        copy_v3_v3(q1, v1);
        copy_v3_v3(q2, v2);
        copy_v3_v3(q3, q2);
      }
    }
    else if (sd[1] < 0.0f) {
      if (sd[2] > 0.0f) {
        /* +-+ */
        copy_v3_v3(q0, v0);
        vec_add_dir(q1, v0, v1, (sd[0] / (sd[0] - sd[1])));
        vec_add_dir(q2, v1, v2, (sd[1] / (sd[1] - sd[2])));
        copy_v3_v3(q3, v2);
      }
      else if (sd[2] < 0.0f) {
        /* +-- */
        copy_v3_v3(q0, v0);
        vec_add_dir(q1, v0, v1, (sd[0] / (sd[0] - sd[1])));
        vec_add_dir(q2, v0, v2, (sd[0] / (sd[0] - sd[2])));
        copy_v3_v3(q3, q2);
      }
      else {
        /* +-0 */
        copy_v3_v3(q0, v0);
        vec_add_dir(q1, v0, v1, (sd[0] / (sd[0] - sd[1])));
        copy_v3_v3(q2, v2);
        copy_v3_v3(q3, q2);
      }
    }
    else {
      if (sd[2] > 0.0f) {
        /* +0+ */
        copy_v3_v3(q0, v0);
        copy_v3_v3(q1, v1);
        copy_v3_v3(q2, v2);
        copy_v3_v3(q3, q2);
      }
      else if (sd[2] < 0.0f) {
        /* +0- */
        copy_v3_v3(q0, v0);
        copy_v3_v3(q1, v1);
        vec_add_dir(q2, v0, v2, (sd[0] / (sd[0] - sd[2])));
        copy_v3_v3(q3, q2);
      }
      else {
        /* +00 */
        copy_v3_v3(q0, v0);
        copy_v3_v3(q1, v1);
        copy_v3_v3(q2, v2);
        copy_v3_v3(q3, q2);
      }
    }
  }
  else if (sd[0] < 0.0f) {
    if (sd[1] > 0.0f) {
      if (sd[2] > 0.0f) {
        /* -++ */
        vec_add_dir(q0, v0, v1, (sd[0] / (sd[0] - sd[1])));
        copy_v3_v3(q1, v1);
        copy_v3_v3(q2, v2);
        vec_add_dir(q3, v0, v2, (sd[0] / (sd[0] - sd[2])));
      }
      else if (sd[2] < 0.0f) {
        /* -+- */
        vec_add_dir(q0, v0, v1, (sd[0] / (sd[0] - sd[1])));
        copy_v3_v3(q1, v1);
        vec_add_dir(q2, v1, v2, (sd[1] / (sd[1] - sd[2])));
        copy_v3_v3(q3, q2);
      }
      else {
        /* -+0 */
        vec_add_dir(q0, v0, v1, (sd[0] / (sd[0] - sd[1])));
        copy_v3_v3(q1, v1);
        copy_v3_v3(q2, v2);
        copy_v3_v3(q3, q2);
      }
    }
    else if (sd[1] < 0.0f) {
      if (sd[2] > 0.0f) {
        /* --+ */
        vec_add_dir(q0, v0, v2, (sd[0] / (sd[0] - sd[2])));
        vec_add_dir(q1, v1, v2, (sd[1] / (sd[1] - sd[2])));
        copy_v3_v3(q2, v2);
        copy_v3_v3(q3, q2);
      }
      else if (sd[2] < 0.0f) {
        /* --- */
        return false;
      }
      else {
        /* --0 */
        return false;
      }
    }
    else {
      if (sd[2] > 0.0f) {
        /* -0+ */
        vec_add_dir(q0, v0, v2, (sd[0] / (sd[0] - sd[2])));
        copy_v3_v3(q1, v1);
        copy_v3_v3(q2, v2);
        copy_v3_v3(q3, q2);
      }
      else if (sd[2] < 0.0f) {
        /* -0- */
        return false;
      }
      else {
        /* -00 */
        return false;
      }
    }
  }
  else {
    if (sd[1] > 0.0f) {
      if (sd[2] > 0.0f) {
        /* 0++ */
        copy_v3_v3(q0, v0);
        copy_v3_v3(q1, v1);
        copy_v3_v3(q2, v2);
        copy_v3_v3(q3, q2);
      }
      else if (sd[2] < 0.0f) {
        /* 0+- */
        copy_v3_v3(q0, v0);
        copy_v3_v3(q1, v1);
        vec_add_dir(q2, v1, v2, (sd[1] / (sd[1] - sd[2])));
        copy_v3_v3(q3, q2);
      }
      else {
        /* 0+0 */
        copy_v3_v3(q0, v0);
        copy_v3_v3(q1, v1);
        copy_v3_v3(q2, v2);
        copy_v3_v3(q3, q2);
      }
    }
    else if (sd[1] < 0.0f) {
      if (sd[2] > 0.0f) {
        /* 0-+ */
        copy_v3_v3(q0, v0);
        vec_add_dir(q1, v1, v2, (sd[1] / (sd[1] - sd[2])));
        copy_v3_v3(q2, v2);
        copy_v3_v3(q3, q2);
      }
      else if (sd[2] < 0.0f) {
        /* 0-- */
        return false;
      }
      else {
        /* 0-0 */
        return false;
      }
    }
    else {
      if (sd[2] > 0.0f) {
        /* 00+ */
        copy_v3_v3(q0, v0);
        copy_v3_v3(q1, v1);
        copy_v3_v3(q2, v2);
        copy_v3_v3(q3, q2);
      }
      else if (sd[2] < 0.0f) {
        /* 00- */
        return false;
      }
      else {
        /* 000 */
        return false;
      }
    }
  }

  return true;
}

/* altivec optimization, this works, but is unused */

#if 0
#  include <Accelerate/Accelerate.h>

typedef union {
  vFloat v;
  float f[4];
} vFloatResult;

static vFloat vec_splat_float(float val)
{
  return (vFloat){val, val, val, val};
}

static float ff_quad_form_factor(float *p, float *n, float *q0, float *q1, float *q2, float *q3)
{
  vFloat vcos, rlen, vrx, vry, vrz, vsrx, vsry, vsrz, gx, gy, gz, vangle;
  vUInt8 rotate = (vUInt8){4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3};
  vFloatResult vresult;
  float result;

  /* compute r* */
  vrx = (vFloat){q0[0], q1[0], q2[0], q3[0]} - vec_splat_float(p[0]);
  vry = (vFloat){q0[1], q1[1], q2[1], q3[1]} - vec_splat_float(p[1]);
  vrz = (vFloat){q0[2], q1[2], q2[2], q3[2]} - vec_splat_float(p[2]);

  /* normalize r* */
  rlen = vec_rsqrte(vrx * vrx + vry * vry + vrz * vrz + vec_splat_float(1e-16f));
  vrx = vrx * rlen;
  vry = vry * rlen;
  vrz = vrz * rlen;

  /* rotate r* for cross and dot */
  vsrx = vec_perm(vrx, vrx, rotate);
  vsry = vec_perm(vry, vry, rotate);
  vsrz = vec_perm(vrz, vrz, rotate);

  /* cross product */
  gx = vsry * vrz - vsrz * vry;
  gy = vsrz * vrx - vsrx * vrz;
  gz = vsrx * vry - vsry * vrx;

  /* normalize */
  rlen = vec_rsqrte(gx * gx + gy * gy + gz * gz + vec_splat_float(1e-16f));
  gx = gx * rlen;
  gy = gy * rlen;
  gz = gz * rlen;

  /* angle */
  vcos = vrx * vsrx + vry * vsry + vrz * vsrz;
  vcos = vec_max(vec_min(vcos, vec_splat_float(1.0f)), vec_splat_float(-1.0f));
  vangle = vacosf(vcos);

  /* dot */
  vresult.v = (vec_splat_float(n[0]) * gx + vec_splat_float(n[1]) * gy +
               vec_splat_float(n[2]) * gz) *
              vangle;

  result = (vresult.f[0] + vresult.f[1] + vresult.f[2] + vresult.f[3]) * (0.5f / (float)M_PI);
  result = MAX2(result, 0.0f);

  return result;
}

#endif

/* SSE optimization, acos code doesn't work */

#if 0

#  include <xmmintrin.h>

static __m128 sse_approx_acos(__m128 x)
{
  /* needs a better approximation than taylor expansion of acos, since that
   * gives big errors for near 1.0 values, sqrt(2 * x) * acos(1 - x) should work
   * better, see http://www.tom.womack.net/projects/sse-fast-arctrig.html */

  return _mm_set_ps1(1.0f);
}

static float ff_quad_form_factor(float *p, float *n, float *q0, float *q1, float *q2, float *q3)
{
  float r0[3], r1[3], r2[3], r3[3], g0[3], g1[3], g2[3], g3[3];
  float a1, a2, a3, a4, dot1, dot2, dot3, dot4, result;
  float fresult[4] __attribute__((aligned(16)));
  __m128 qx, qy, qz, rx, ry, rz, rlen, srx, sry, srz, gx, gy, gz, glen, rcos, angle, aresult;

  /* compute r */
  qx = _mm_set_ps(q3[0], q2[0], q1[0], q0[0]);
  qy = _mm_set_ps(q3[1], q2[1], q1[1], q0[1]);
  qz = _mm_set_ps(q3[2], q2[2], q1[2], q0[2]);

  rx = qx - _mm_set_ps1(p[0]);
  ry = qy - _mm_set_ps1(p[1]);
  rz = qz - _mm_set_ps1(p[2]);

  /* normalize r */
  rlen = _mm_rsqrt_ps(rx * rx + ry * ry + rz * rz + _mm_set_ps1(1e-16f));
  rx = rx * rlen;
  ry = ry * rlen;
  rz = rz * rlen;

  /* cross product */
  srx = _mm_shuffle_ps(rx, rx, _MM_SHUFFLE(0, 3, 2, 1));
  sry = _mm_shuffle_ps(ry, ry, _MM_SHUFFLE(0, 3, 2, 1));
  srz = _mm_shuffle_ps(rz, rz, _MM_SHUFFLE(0, 3, 2, 1));

  gx = sry * rz - srz * ry;
  gy = srz * rx - srx * rz;
  gz = srx * ry - sry * rx;

  /* normalize g */
  glen = _mm_rsqrt_ps(gx * gx + gy * gy + gz * gz + _mm_set_ps1(1e-16f));
  gx = gx * glen;
  gy = gy * glen;
  gz = gz * glen;

  /* compute angle */
  rcos = rx * srx + ry * sry + rz * srz;
  rcos = _mm_max_ps(_mm_min_ps(rcos, _mm_set_ps1(1.0f)), _mm_set_ps1(-1.0f));

  angle = sse_approx_cos(rcos);
  aresult = (_mm_set_ps1(n[0]) * gx + _mm_set_ps1(n[1]) * gy + _mm_set_ps1(n[2]) * gz) * angle;

  /* sum together */
  result = (fresult[0] + fresult[1] + fresult[2] + fresult[3]) * (0.5f / (float)M_PI);
  result = MAX2(result, 0.0f);

  return result;
}

#endif

static void ff_normalize(float n[3])
{
  float d;

  d = dot_v3v3(n, n);

  if (d > 1.0e-35f) {
    d = 1.0f / sqrtf(d);

    n[0] *= d;
    n[1] *= d;
    n[2] *= d;
  }
}

float form_factor_quad(const float p[3],
                       const float n[3],
                       const float q0[3],
                       const float q1[3],
                       const float q2[3],
                       const float q3[3])
{
  float r0[3], r1[3], r2[3], r3[3], g0[3], g1[3], g2[3], g3[3];
  float a1, a2, a3, a4, dot1, dot2, dot3, dot4, result;

  sub_v3_v3v3(r0, q0, p);
  sub_v3_v3v3(r1, q1, p);
  sub_v3_v3v3(r2, q2, p);
  sub_v3_v3v3(r3, q3, p);

  ff_normalize(r0);
  ff_normalize(r1);
  ff_normalize(r2);
  ff_normalize(r3);

  cross_v3_v3v3(g0, r1, r0);
  ff_normalize(g0);
  cross_v3_v3v3(g1, r2, r1);
  ff_normalize(g1);
  cross_v3_v3v3(g2, r3, r2);
  ff_normalize(g2);
  cross_v3_v3v3(g3, r0, r3);
  ff_normalize(g3);

  a1 = saacosf(dot_v3v3(r0, r1));
  a2 = saacosf(dot_v3v3(r1, r2));
  a3 = saacosf(dot_v3v3(r2, r3));
  a4 = saacosf(dot_v3v3(r3, r0));

  dot1 = dot_v3v3(n, g0);
  dot2 = dot_v3v3(n, g1);
  dot3 = dot_v3v3(n, g2);
  dot4 = dot_v3v3(n, g3);

  result = (a1 * dot1 + a2 * dot2 + a3 * dot3 + a4 * dot4) * 0.5f / (float)M_PI;
  result = MAX2(result, 0.0f);

  return result;
}

float form_factor_hemi_poly(
    float p[3], float n[3], float v1[3], float v2[3], float v3[3], float v4[3])
{
  /* computes how much hemisphere defined by point and normal is
   * covered by a quad or triangle, cosine weighted */
  float q0[3], q1[3], q2[3], q3[3], contrib = 0.0f;

  if (form_factor_visible_quad(p, n, v1, v2, v3, q0, q1, q2, q3)) {
    contrib += form_factor_quad(p, n, q0, q1, q2, q3);
  }

  if (v4 && form_factor_visible_quad(p, n, v1, v3, v4, q0, q1, q2, q3)) {
    contrib += form_factor_quad(p, n, q0, q1, q2, q3);
  }

  return contrib;
}

/**
 * Evaluate if entire quad is a proper convex quad
 */
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
  /* linetests, the 2 diagonals have to instersect to be convex */
  return (isect_seg_seg_v2(v1, v3, v2, v4) > 0);
}

bool is_poly_convex_v2(const float verts[][2], unsigned int nr)
{
  unsigned int sign_flag = 0;
  unsigned int a;
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

/**
 * Check if either of the diagonals along this quad create flipped triangles
 * (normals pointing away from eachother).
 * - (1 << 0): (v1-v3) is flipped.
 * - (1 << 1): (v2-v4) is flipped.
 */
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
  float d_12[3], d_13[3], d_14[3];
  float cross_a[3], cross_b[3];
  sub_v3_v3v3(d_12, v2, v1);
  sub_v3_v3v3(d_13, v3, v1);
  sub_v3_v3v3(d_14, v4, v1);
  cross_v3_v3v3(cross_a, d_12, d_13);
  cross_v3_v3v3(cross_b, d_14, d_13);
  return dot_v3v3(cross_a, cross_b) > 0.0f;
}

/**
 * Return the value which the distance between points will need to be scaled by,
 * to define a handle, given both points are on a perfect circle.
 *
 * Use when we want a bezier curve to match a circle as closely as possible.
 *
 * \note the return value will need to be divided by 0.75 for correct results.
 */
float cubic_tangent_factor_circle_v3(const float tan_l[3], const float tan_r[3])
{
  BLI_ASSERT_UNIT_V3(tan_l);
  BLI_ASSERT_UNIT_V3(tan_r);

  /* -7f causes instability/glitches with Bendy Bones + Custom Refs  */
  const float eps = 1e-5f;

  const float tan_dot = dot_v3v3(tan_l, tan_r);
  if (tan_dot > 1.0f - eps) {
    /* no angle difference (use fallback, length wont make any difference) */
    return (1.0f / 3.0f) * 0.75f;
  }
  else if (tan_dot < -1.0f + eps) {
    /* parallele tangents (half-circle) */
    return (1.0f / 2.0f);
  }
  else {
    /* non-aligned tangents, calculate handle length */
    const float angle = acosf(tan_dot) / 2.0f;

    /* could also use 'angle_sin = len_vnvn(tan_l, tan_r, dims) / 2.0' */
    const float angle_sin = sinf(angle);
    const float angle_cos = cosf(angle);
    return ((1.0f - angle_cos) / (angle_sin * 2.0f)) / angle_sin;
  }
}
