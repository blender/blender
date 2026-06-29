/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

/** \file
 * \ingroup bli
 *
 * Tests for #BLI_math_geom.hh.
 *
 * Each test cross-checks the modern typed API against the legacy C API to
 * ensure equivalence, and also tests degenerate / boundary conditions.
 */

#include "testing/testing.h"

#include "BLI_math_geom.hh"
#include "BLI_math_geom_c.hh"
#include "BLI_vector.hh"

namespace blender::math {

/* -------------------------------------------------------------------- */
/** \name Normal / Area
 * \{ */

TEST(math_geom_hh, NormalQuad)
{
  const float3 v1(0, 0, 0), v2(1, 0, 0), v3(1, 1, 0), v4(0, 1, 0);

  float c_n[3];
  normal_quad_v3(c_n, &v1.x, &v2.x, &v3.x, &v4.x);

  const float3 n = normal_quad(v1, v2, v3, v4);
  EXPECT_NEAR(c_n[0], n.x, 1e-6f);
  EXPECT_NEAR(c_n[1], n.y, 1e-6f);
  EXPECT_NEAR(c_n[2], n.z, 1e-6f);
}

TEST(math_geom_hh, AreaTri)
{
  /* Unit right triangle in XY plane: area = 0.5. */
  const float3 v1(0, 0, 0), v2(1, 0, 0), v3(0, 1, 0);
  EXPECT_NEAR(0.5f, area_tri(v1, v2, v3), 1e-6f);

  /* Cross-check with C API. */
  EXPECT_NEAR(area_tri_v3(&v1.x, &v2.x, &v3.x), area_tri(v1, v2, v3), 1e-6f);

  /* Degenerate triangle (all same point): area = 0. */
  const float3 degen(1, 2, 3);
  EXPECT_NEAR(0.0f, area_tri(degen, degen, degen), 1e-6f);
}

TEST(math_geom_hh, CotangentTriWeight)
{
  /* Equilateral triangle: all cotangent weights are equal (cot 60° ≈ 0.5774). */
  const float sq3 = std::sqrt(3.0f);
  const float3 v1(0, 0, 0), v2(1, 0, 0), v3(0.5f, sq3 * 0.5f, 0);
  EXPECT_NEAR(
      cotangent_tri_weight_v3(&v1.x, &v2.x, &v3.x), cotangent_tri_weight(v1, v2, v3), 1e-5f);

  /* Degenerate (all on same point): returns 0. */
  const float3 degen(1, 0, 0);
  EXPECT_NEAR(0.0f, cotangent_tri_weight(degen, degen, degen), 1e-6f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Planes
 * \{ */

TEST(math_geom_hh, PlaneFromPointNormal)
{
  const float3 co(1, 2, 3), no(0, 0, 1);

  float c_plane[4];
  plane_from_point_normal_v3(c_plane, &co.x, &no.x);

  const float4 plane = plane_from_point_normal(co, no);
  EXPECT_NEAR(c_plane[0], plane.x, 1e-6f);
  EXPECT_NEAR(c_plane[1], plane.y, 1e-6f);
  EXPECT_NEAR(c_plane[2], plane.z, 1e-6f);
  EXPECT_NEAR(c_plane[3], plane.w, 1e-6f);
}

TEST(math_geom_hh, PlanePointSide)
{
  /* Plane: z = 0, normal pointing +Z, offset 0. */
  const float4 plane(0, 0, 1, 0);
  EXPECT_NEAR(1.0f, plane_point_side(plane, float3(0, 0, 1)), 1e-6f);
  EXPECT_NEAR(-1.0f, plane_point_side(plane, float3(0, 0, -1)), 1e-6f);
  EXPECT_NEAR(0.0f, plane_point_side(plane, float3(5, 7, 0)), 1e-6f);

  /* Cross-check with C API. */
  const float3 pt(1, 2, 3);
  const float c_plane[4] = {plane.x, plane.y, plane.z, plane.w};
  EXPECT_NEAR(plane_point_side_v3(c_plane, &pt.x), plane_point_side(plane, pt), 1e-6f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Distance & Closest Points
 * \{ */

TEST(math_geom_hh, DistSquaredToLineSegment2D)
{
  /* Point directly past end of segment: distance to endpoint. */
  const float2 p(3, 1), l1(0, 0), l2(2, 0);
  EXPECT_NEAR(dist_squared_to_line_segment_v2(&p.x, &l1.x, &l2.x),
              dist_squared_to_line_segment(p, l1, l2),
              1e-6f);

  /* Point beside the middle of the segment. */
  const float2 p2(1, 2), l3(0, 0), l4(2, 0);
  EXPECT_NEAR(dist_squared_to_line_segment_v2(&p2.x, &l3.x, &l4.x),
              dist_squared_to_line_segment(p2, l3, l4),
              1e-6f);

  /* Degenerate segment (l1 == l2). */
  const float2 p3(1, 1), degen(0, 0);
  EXPECT_NEAR(2.0f, dist_squared_to_line_segment(p3, degen, degen), 1e-6f);
}

TEST(math_geom_hh, DistSquaredToLineSegment3D)
{
  const float3 p(0, 1, 0), l1(0, 0, 0), l2(1, 0, 0);
  EXPECT_NEAR(dist_squared_to_line_segment_v3(&p.x, &l1.x, &l2.x),
              dist_squared_to_line_segment(p, l1, l2),
              1e-6f);
}

TEST(math_geom_hh, ClosestToLineSegment3D)
{
  const float3 p(0.5f, 1.0f, 0.0f), l1(0, 0, 0), l2(1, 0, 0);

  float c_r[3];
  closest_to_line_segment_v3(c_r, &p.x, &l1.x, &l2.x);

  const float3 r = closest_to_line_segment(p, l1, l2);
  EXPECT_NEAR(c_r[0], r.x, 1e-6f);
  EXPECT_NEAR(c_r[1], r.y, 1e-6f);
  EXPECT_NEAR(c_r[2], r.z, 1e-6f);

  /* Degenerate segment: closest point is l1. */
  const float3 degen(1, 1, 1);
  const float3 r_degen = closest_to_line_segment(p, degen, degen);
  EXPECT_NEAR(degen.x, r_degen.x, 1e-6f);
  EXPECT_NEAR(degen.y, r_degen.y, 1e-6f);
  EXPECT_NEAR(degen.z, r_degen.z, 1e-6f);
}

TEST(math_geom_hh, ClosestToPlaneNormalized)
{
  /* Plane: z = 1, normal (0,0,1). */
  const float4 plane(0, 0, 1, -1);
  const float3 pt(3, 4, 5);

  float c_r[3];
  closest_to_plane_normalized_v3(c_r, reinterpret_cast<const float *>(&plane), &pt.x);

  const float3 r = closest_to_plane_normalized(plane, pt);
  EXPECT_NEAR(c_r[0], r.x, 1e-6f);
  EXPECT_NEAR(c_r[1], r.y, 1e-6f);
  EXPECT_NEAR(c_r[2], r.z, 1e-6f);
}

TEST(math_geom_hh, ClosestToPlane)
{
  /* Plane: z = 1, with a non-unit normal (0, 0, 2). */
  const float4 plane(0, 0, 2, -2);
  const float3 pt(3, 4, 5);

  float c_r[3];
  closest_to_plane_v3(c_r, reinterpret_cast<const float *>(&plane), &pt.x);

  const float3 r = closest_to_plane(plane, pt);
  EXPECT_NEAR(c_r[0], r.x, 1e-6f);
  EXPECT_NEAR(c_r[1], r.y, 1e-6f);
  EXPECT_NEAR(c_r[2], r.z, 1e-6f);
  /* The projected z should be 1.0. */
  EXPECT_NEAR(1.0f, r.z, 1e-6f);
}

TEST(math_geom_hh, DistSquaredToPlane)
{
  /* Unit-normal plane at z = 0; point at z = 2 → distance² = 4. */
  const float4 unit_plane(0, 0, 1, 0);
  const float3 pt(0, 0, 2);
  EXPECT_NEAR(dist_squared_to_plane_v3(&pt.x, reinterpret_cast<const float *>(&unit_plane)),
              dist_squared_to_plane(pt, unit_plane),
              1e-6f);
  EXPECT_NEAR(4.0f, dist_squared_to_plane(pt, unit_plane), 1e-6f);

  /* Non-unit normal (0, 0, 2); same geometric plane, same result. */
  const float4 scaled_plane(0, 0, 2, 0);
  EXPECT_NEAR(dist_squared_to_plane_v3(&pt.x, reinterpret_cast<const float *>(&scaled_plane)),
              dist_squared_to_plane(pt, scaled_plane),
              1e-6f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Projection Factors
 * \{ */

TEST(math_geom_hh, LinePointFactor3D)
{
  const float3 p(0.5f, 1.0f, 0.0f), l1(0, 0, 0), l2(1, 0, 0);
  EXPECT_NEAR(line_point_factor_v3(&p.x, &l1.x, &l2.x), line_point_factor(p, l1, l2), 1e-6f);

  /* Degenerate: returns 0. */
  EXPECT_NEAR(0.0f, line_point_factor(p, l1, l1), 1e-6f);
}

TEST(math_geom_hh, LinePointFactor2D)
{
  const float2 p(0.5f, 1.0f), l1(0, 0), l2(1, 0);
  EXPECT_NEAR(line_point_factor_v2(&p.x, &l1.x, &l2.x), line_point_factor(p, l1, l2), 1e-6f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Utilities
 * \{ */

TEST(math_geom_hh, AxisDominantToM3)
{
  const float3 normal = normalize(float3(0, 0, 1));
  float c_mat[3][3];
  axis_dominant_v3_to_m3(c_mat, &normal.x);

  const float3x3 mat = axis_dominant_to_m3(normal);
  for (int col = 0; col < 3; col++) {
    for (int row = 0; row < 3; row++) {
      EXPECT_NEAR(c_mat[col][row], mat[col][row], 1e-6f);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Intersection
 * \{ */

TEST(math_geom_hh, IsectLineLine3D)
{
  /* Two perpendicular lines crossing at origin. */
  const float3 v1(-1, 0, 0), v2(1, 0, 0), v3(0, -1, 0), v4(0, 1, 0);
  float3 r_i1, r_i2;
  const int count = isect_line_line(v1, v2, v3, v4, r_i1, r_i2);
  EXPECT_EQ(1, count);
  EXPECT_NEAR(0.0f, r_i1.x, 1e-5f);
  EXPECT_NEAR(0.0f, r_i1.y, 1e-5f);
}

TEST(math_geom_hh, IsectLinePlane)
{
  /* Line along Z, plane at z = 1 (normal = Z, point = (0,0,1)). */
  const float3 l1(0, 0, 0), l2(0, 0, 2);
  const float3 plane_co(0, 0, 1), plane_no(0, 0, 1);
  const std::optional<float3> hit = isect_line_plane(l1, l2, plane_co, plane_no);
  ASSERT_TRUE(hit.has_value());
  EXPECT_NEAR(0.0f, hit->x, 1e-6f);
  EXPECT_NEAR(0.0f, hit->y, 1e-6f);
  EXPECT_NEAR(1.0f, hit->z, 1e-6f);

  /* Parallel line: no intersection. */
  const float3 l3(0, 0, 0), l4(1, 0, 0);
  EXPECT_FALSE(isect_line_plane(l3, l4, plane_co, plane_no).has_value());
}

TEST(math_geom_hh, IsectPointTri2D)
{
  const float2 v1(-1, 0), v2(1, 0), v3(0, 2);

  /* Inside CCW triangle. */
  EXPECT_EQ(1, isect_point_tri(float2(0, 0.5f), v1, v2, v3));
  /* Outside. */
  EXPECT_EQ(0, isect_point_tri(float2(0, 3), v1, v2, v3));
  /* CW winding. */
  EXPECT_EQ(-1, isect_point_tri(float2(0, 0.5f), v3, v2, v1));
}

TEST(math_geom_hh, IsectPointQuad2D)
{
  const float2 v1(0, 0), v2(2, 0), v3(2, 2), v4(0, 2);

  /* Inside CCW quad. */
  EXPECT_EQ(1, isect_point_quad(float2(1, 1), v1, v2, v3, v4));
  /* Outside. */
  EXPECT_EQ(0, isect_point_quad(float2(3, 1), v1, v2, v3, v4));
  /* CW winding. */
  EXPECT_EQ(-1, isect_point_quad(float2(1, 1), v4, v3, v2, v1));

  /* Cross-check with C API (float specialization). */
  const float2 p_inside(1, 1);
  EXPECT_EQ(isect_point_quad_v2(&p_inside.x, &v1.x, &v2.x, &v3.x, &v4.x),
            isect_point_quad(float2(1, 1), v1, v2, v3, v4));
}

TEST(math_geom_hh, IsectPointPoly)
{
  /* Unit square as polygon (use Vector so Span<float2> deduction works). */
  Vector<float2> verts = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  EXPECT_TRUE(isect_point_poly(float2(0.5f, 0.5f), verts));
  EXPECT_FALSE(isect_point_poly(float2(2.0f, 0.5f), verts));

  /* Cross-check with C API. */
  const float2 pt_in(0.5f, 0.5f);
  EXPECT_EQ(isect_point_poly_v2(
                &pt_in.x, reinterpret_cast<const float (*)[2]>(verts.data()), uint(verts.size())),
            isect_point_poly(pt_in, verts));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Barycentric Coordinates
 * \{ */

TEST(math_geom_hh, BarycentricWeights)
{
  const float2 v1(0, 0), v2(1, 0), v3(0, 1);

  /* Centroid: weights should all be ~1/3. */
  const float2 centroid(1.0f / 3.0f, 1.0f / 3.0f);

  float c_w[3];
  barycentric_weights_v2(&v1.x, &v2.x, &v3.x, &centroid.x, c_w);

  const float3 w = barycentric_weights(v1, v2, v3, centroid);
  EXPECT_NEAR(c_w[0], w.x, 1e-6f);
  EXPECT_NEAR(c_w[1], w.y, 1e-6f);
  EXPECT_NEAR(c_w[2], w.z, 1e-6f);
  EXPECT_NEAR(1.0f / 3.0f, w.x, 1e-5f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Polygon Utilities
 * \{ */

TEST(math_geom_hh, PolyToTriCount)
{
  /* 1 quad (4 corners): 1 * (4-2) = 2 triangles. */
  EXPECT_EQ(2, poly_to_tri_count(1, 4));
  /* 2 triangles (6 corners): 2 * (3-2) = 2 triangles. */
  EXPECT_EQ(2, poly_to_tri_count(2, 6));
  /* Cross-check with the C-style macro. */
  EXPECT_EQ(poly_to_tri_count(5, 20), ::blender::poly_to_tri_count(5, 20));
}

TEST(math_geom_hh, AreaPoly)
{
  /* Unit square: area = 1. */
  Vector<float2> verts = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  EXPECT_NEAR(1.0f, area_poly(verts), 1e-6f);

  /* Cross-check with C API. */
  EXPECT_NEAR(area_poly_v2(reinterpret_cast<const float (*)[2]>(verts.data()), uint(verts.size())),
              area_poly(verts),
              1e-6f);

  /* Triangle with base 2, height 1: area = 1. */
  Vector<float2> tri = {{0, 0}, {2, 0}, {1, 1}};
  EXPECT_NEAR(1.0f, area_poly(tri), 1e-6f);
}

TEST(math_geom_hh, InterpWeightsPoly)
{
  /* Centroid of a unit square: all four weights should be 0.25. */
  Vector<float2> verts = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  float w[4];
  interp_weights_poly(MutableSpan(w, 4), verts, float2(0.5f, 0.5f));
  for (int i = 0; i < 4; i++) {
    EXPECT_NEAR(0.25f, w[i], 1e-5f);
  }

  /* Cross-check with C API. */
  float c_w[4];
  interp_weights_poly_v2(
      c_w, reinterpret_cast<float (*)[2]>(verts.data()), int(verts.size()), float2(0.5f, 0.5f));
  for (int i = 0; i < 4; i++) {
    EXPECT_NEAR(c_w[i], w[i], 1e-6f);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Interpolation
 * \{ */

TEST(math_geom_hh, InterpBilinearQuad)
{
  /* Unit square in 3D (CCW order matching interp_bilinear_quad_v3 convention):
   *   v3(0,1,0)---v2(1,1,0)
   *   |               |
   *   v0(0,0,0)---v1(1,0,0)
   */
  const float3 v0(0, 0, 0), v1(1, 0, 0), v2(1, 1, 0), v3(0, 1, 0);

  /* Center (u=0.5, v=0.5) should give (0.5, 0.5, 0). */
  const float3 center = interp_bilinear_quad(v0, v1, v2, v3, 0.5f, 0.5f);
  EXPECT_NEAR(0.5f, center.x, 1e-6f);
  EXPECT_NEAR(0.5f, center.y, 1e-6f);
  EXPECT_NEAR(0.0f, center.z, 1e-6f);

  /* Cross-check with C API at (u=0.25, v=0.75). */
  float quad[4][3] = {
      {v0.x, v0.y, v0.z}, {v1.x, v1.y, v1.z}, {v2.x, v2.y, v2.z}, {v3.x, v3.y, v3.z}};
  float c_res[3];
  interp_bilinear_quad_v3(quad, 0.25f, 0.75f, c_res);
  const float3 res = interp_bilinear_quad(v0, v1, v2, v3, 0.25f, 0.75f);
  EXPECT_NEAR(c_res[0], res.x, 1e-6f);
  EXPECT_NEAR(c_res[1], res.y, 1e-6f);
  EXPECT_NEAR(c_res[2], res.z, 1e-6f);

  /* Corners. */
  EXPECT_EQ(v0, interp_bilinear_quad(v0, v1, v2, v3, 0.0f, 0.0f));
  EXPECT_EQ(v1, interp_bilinear_quad(v0, v1, v2, v3, 1.0f, 0.0f));
  EXPECT_EQ(v2, interp_bilinear_quad(v0, v1, v2, v3, 1.0f, 1.0f));
  EXPECT_EQ(v3, interp_bilinear_quad(v0, v1, v2, v3, 0.0f, 1.0f));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mapping
 * \{ */

TEST(math_geom_hh, MapToSphere)
{
  /* +Z should give v = 0 (north pole) — check C vs. new API agree. */
  const float3 up(0, 0, 1);
  float c_u, c_v;
  blender::map_to_sphere(&c_u, &c_v, up.x, up.y, up.z);

  const float2 uv = map_to_sphere(up);
  EXPECT_NEAR(c_u, uv.x, 1e-6f);
  EXPECT_NEAR(c_v, uv.y, 1e-6f);
}

/** \} */

}  // namespace blender::math
