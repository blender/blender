/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#ifndef __MATH_GEOM_INLINE_C__
#define __MATH_GEOM_INLINE_C__

#include "BLI_math_vector.h"

#include <cstring>

/* A few small defines. Keep them local! */
#define SMALL_NUMBER 1.e-8f

/********************************** Polygons *********************************/

MINLINE float cross_tri_v2(const float v1[2], const float v2[2], const float v3[2])
{
  return (v1[0] - v2[0]) * (v2[1] - v3[1]) + (v1[1] - v2[1]) * (v3[0] - v2[0]);
}

MINLINE float area_tri_signed_v2(const float v1[2], const float v2[2], const float v3[2])
{
  return 0.5f * ((v1[0] - v2[0]) * (v2[1] - v3[1]) + (v1[1] - v2[1]) * (v3[0] - v2[0]));
}

MINLINE float area_tri_v2(const float v1[2], const float v2[2], const float v3[2])
{
  return fabsf(area_tri_signed_v2(v1, v2, v3));
}

MINLINE float area_squared_tri_v2(const float v1[2], const float v2[2], const float v3[2])
{
  float area = area_tri_signed_v2(v1, v2, v3);
  return area * area;
}

MINLINE void axis_dominant_v3(int *r_axis_a, int *r_axis_b, const float axis[3])
{
  const float xn = fabsf(axis[0]);
  const float yn = fabsf(axis[1]);
  const float zn = fabsf(axis[2]);

  if (zn >= xn && zn >= yn) {
    *r_axis_a = 0;
    *r_axis_b = 1;
  }
  else if (yn >= xn && yn >= zn) {
    *r_axis_a = 0;
    *r_axis_b = 2;
  }
  else {
    *r_axis_a = 1;
    *r_axis_b = 2;
  }
}

MINLINE float axis_dominant_v3_max(int *r_axis_a, int *r_axis_b, const float axis[3])
{
  const float xn = fabsf(axis[0]);
  const float yn = fabsf(axis[1]);
  const float zn = fabsf(axis[2]);

  if (zn >= xn && zn >= yn) {
    *r_axis_a = 0;
    *r_axis_b = 1;
    return zn;
  }
  if (yn >= xn && yn >= zn) {
    *r_axis_a = 0;
    *r_axis_b = 2;
    return yn;
  }
  *r_axis_a = 1;
  *r_axis_b = 2;
  return xn;
}

MINLINE int axis_dominant_v3_single(const float vec[3])
{
  const float x = fabsf(vec[0]);
  const float y = fabsf(vec[1]);
  const float z = fabsf(vec[2]);
  return ((x > y) ? ((x > z) ? 0 : 2) : ((y > z) ? 1 : 2));
}

MINLINE int axis_dominant_v3_ortho_single(const float vec[3])
{
  const float x = fabsf(vec[0]);
  const float y = fabsf(vec[1]);
  const float z = fabsf(vec[2]);
  return ((x < y) ? ((x < z) ? 0 : 2) : ((y < z) ? 1 : 2));
}

MINLINE int max_axis_v3(const float vec[3])
{
  const float x = vec[0];
  const float y = vec[1];
  const float z = vec[2];
  return ((x > y) ? ((x > z) ? 0 : 2) : ((y > z) ? 1 : 2));
}

MINLINE int min_axis_v3(const float vec[3])
{
  const float x = vec[0];
  const float y = vec[1];
  const float z = vec[2];
  return ((x < y) ? ((x < z) ? 0 : 2) : ((y < z) ? 1 : 2));
}

MINLINE int poly_to_tri_count(const int poly_count, const int corner_count)
{
  BLI_assert(!poly_count || corner_count > poly_count * 2);
  return corner_count - (poly_count * 2);
}

MINLINE float plane_point_side_v3(const float plane[4], const float co[3])
{
  return dot_v3v3(co, plane) + plane[3];
}

MINLINE float shell_angle_to_dist(const float angle)
{
  return (UNLIKELY(angle < SMALL_NUMBER)) ? 1.0f : fabsf(1.0f / cosf(angle));
}
MINLINE float shell_v3v3_normalized_to_dist(const float a[3], const float b[3])
{
  BLI_ASSERT_UNIT_V3(a);
  BLI_ASSERT_UNIT_V3(b);
  const float angle_cos = fabsf(dot_v3v3(a, b));
  return (UNLIKELY(angle_cos < SMALL_NUMBER)) ? 1.0f : (1.0f / angle_cos);
}
MINLINE float shell_v2v2_normalized_to_dist(const float a[2], const float b[2])
{
  BLI_ASSERT_UNIT_V2(a);
  BLI_ASSERT_UNIT_V2(b);
  const float angle_cos = fabsf(dot_v2v2(a, b));
  return (UNLIKELY(angle_cos < SMALL_NUMBER)) ? 1.0f : (1.0f / angle_cos);
}

MINLINE float shell_v3v3_mid_normalized_to_dist(const float a[3], const float b[3])
{
  BLI_ASSERT_UNIT_V3(a);
  BLI_ASSERT_UNIT_V3(b);
  float angle_cos;
  float ab[3];
  add_v3_v3v3(ab, a, b);
  angle_cos = (normalize_v3(ab) != 0.0f) ? fabsf(dot_v3v3(a, ab)) : 0.0f;
  return (UNLIKELY(angle_cos < SMALL_NUMBER)) ? 1.0f : (1.0f / angle_cos);
}

MINLINE float shell_v2v2_mid_normalized_to_dist(const float a[2], const float b[2])
{
  BLI_ASSERT_UNIT_V2(a);
  BLI_ASSERT_UNIT_V2(b);
  float angle_cos;
  float ab[2];
  add_v2_v2v2(ab, a, b);
  angle_cos = (normalize_v2(ab) != 0.0f) ? fabsf(dot_v2v2(a, ab)) : 0.0f;
  return (UNLIKELY(angle_cos < SMALL_NUMBER)) ? 1.0f : (1.0f / angle_cos);
}

#undef SMALL_NUMBER

#endif /* __MATH_GEOM_INLINE_C__ */
