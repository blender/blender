/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math_geom.h"
#include "BLI_math_vector_types.hh"

using namespace blender;

TEST(math_geom, DistToLine2DSimple)
{
  float p[2] = {5.0f, 1.0f}, a[2] = {0.0f, 0.0f}, b[2] = {2.0f, 0.0f};
  float distance = dist_to_line_v2(p, a, b);
  EXPECT_NEAR(1.0f, distance, 1e-6);
}

TEST(math_geom, DistToLineSegment2DSimple)
{
  float p[2] = {3.0f, 1.0f}, a[2] = {0.0f, 0.0f}, b[2] = {2.0f, 0.0f};
  float distance = dist_to_line_segment_v2(p, a, b);
  EXPECT_NEAR(sqrtf(2.0f), distance, 1e-6);
}

TEST(math_geom, IsectPointTri2D)
{
  float2 tri_cw[3] = {{-2, 1}, {4, 4}, {2, -3}};
  float2 tri_ccw[3] = {{-2, 1}, {2, -3}, {4, 4}};

  float2 inside1{0, 0};
  float2 inside2{2, 2};
  float2 inside3{2, -1};
  float2 inside4{-1, 1};
  EXPECT_EQ(-1, isect_point_tri_v2(inside1, tri_cw[0], tri_cw[1], tri_cw[2]));
  EXPECT_EQ(+1, isect_point_tri_v2(inside1, tri_ccw[0], tri_ccw[1], tri_ccw[2]));
  EXPECT_EQ(-1, isect_point_tri_v2(inside2, tri_cw[0], tri_cw[1], tri_cw[2]));
  EXPECT_EQ(+1, isect_point_tri_v2(inside2, tri_ccw[0], tri_ccw[1], tri_ccw[2]));
  EXPECT_EQ(-1, isect_point_tri_v2(inside3, tri_cw[0], tri_cw[1], tri_cw[2]));
  EXPECT_EQ(+1, isect_point_tri_v2(inside3, tri_ccw[0], tri_ccw[1], tri_ccw[2]));
  EXPECT_EQ(-1, isect_point_tri_v2(inside4, tri_cw[0], tri_cw[1], tri_cw[2]));
  EXPECT_EQ(+1, isect_point_tri_v2(inside4, tri_ccw[0], tri_ccw[1], tri_ccw[2]));

  float2 outside1{2, 4};
  float2 outside2{-1, -1};
  float2 outside3{0, 3};
  float2 outside4{-4, 0};
  EXPECT_EQ(0, isect_point_tri_v2(outside1, tri_cw[0], tri_cw[1], tri_cw[2]));
  EXPECT_EQ(0, isect_point_tri_v2(outside1, tri_ccw[0], tri_ccw[1], tri_ccw[2]));
  EXPECT_EQ(0, isect_point_tri_v2(outside2, tri_cw[0], tri_cw[1], tri_cw[2]));
  EXPECT_EQ(0, isect_point_tri_v2(outside2, tri_ccw[0], tri_ccw[1], tri_ccw[2]));
  EXPECT_EQ(0, isect_point_tri_v2(outside3, tri_cw[0], tri_cw[1], tri_cw[2]));
  EXPECT_EQ(0, isect_point_tri_v2(outside3, tri_ccw[0], tri_ccw[1], tri_ccw[2]));
  EXPECT_EQ(0, isect_point_tri_v2(outside4, tri_cw[0], tri_cw[1], tri_cw[2]));
  EXPECT_EQ(0, isect_point_tri_v2(outside4, tri_ccw[0], tri_ccw[1], tri_ccw[2]));

  float2 edge1{0, 2};
  float2 edge2{1, -2};
  EXPECT_EQ(-1, isect_point_tri_v2(edge1, tri_cw[0], tri_cw[1], tri_cw[2]));
  EXPECT_EQ(+1, isect_point_tri_v2(edge1, tri_ccw[0], tri_ccw[1], tri_ccw[2]));
  EXPECT_EQ(-1, isect_point_tri_v2(edge2, tri_cw[0], tri_cw[1], tri_cw[2]));
  EXPECT_EQ(+1, isect_point_tri_v2(edge2, tri_ccw[0], tri_ccw[1], tri_ccw[2]));

  float2 corner1{4, 4};
  float2 corner2{2, -3};
  EXPECT_EQ(-1, isect_point_tri_v2(corner1, tri_cw[0], tri_cw[1], tri_cw[2]));
  EXPECT_EQ(+1, isect_point_tri_v2(corner1, tri_ccw[0], tri_ccw[1], tri_ccw[2]));
  EXPECT_EQ(-1, isect_point_tri_v2(corner2, tri_cw[0], tri_cw[1], tri_cw[2]));
  EXPECT_EQ(+1, isect_point_tri_v2(corner2, tri_ccw[0], tri_ccw[1], tri_ccw[2]));
}

TEST(math_geom, IsectPointQuad2D)
{
  float2 quad_cw[4] = {{-2, 1}, {4, 4}, {5, 1}, {2, -3}};
  float2 quad_ccw[4] = {{-2, 1}, {2, -3}, {5, 1}, {4, 4}};

  float2 inside1{0, 0};
  float2 inside2{2, 2};
  float2 inside3{3, -1};
  float2 inside4{-1, 1};
  EXPECT_EQ(-1, isect_point_quad_v2(inside1, quad_cw[0], quad_cw[1], quad_cw[2], quad_cw[3]));
  EXPECT_EQ(+1, isect_point_quad_v2(inside1, quad_ccw[0], quad_ccw[1], quad_ccw[2], quad_ccw[3]));
  EXPECT_EQ(-1, isect_point_quad_v2(inside2, quad_cw[0], quad_cw[1], quad_cw[2], quad_cw[3]));
  EXPECT_EQ(+1, isect_point_quad_v2(inside2, quad_ccw[0], quad_ccw[1], quad_ccw[2], quad_ccw[3]));
  EXPECT_EQ(-1, isect_point_quad_v2(inside3, quad_cw[0], quad_cw[1], quad_cw[2], quad_cw[3]));
  EXPECT_EQ(+1, isect_point_quad_v2(inside3, quad_ccw[0], quad_ccw[1], quad_ccw[2], quad_ccw[3]));
  EXPECT_EQ(-1, isect_point_quad_v2(inside4, quad_cw[0], quad_cw[1], quad_cw[2], quad_cw[3]));
  EXPECT_EQ(+1, isect_point_quad_v2(inside4, quad_ccw[0], quad_ccw[1], quad_ccw[2], quad_ccw[3]));

  float2 outside1{2, 4};
  float2 outside2{-1, -1};
  float2 outside3{0, 3};
  float2 outside4{-4, 0};
  EXPECT_EQ(0, isect_point_quad_v2(outside1, quad_cw[0], quad_cw[1], quad_cw[2], quad_cw[3]));
  EXPECT_EQ(0, isect_point_quad_v2(outside1, quad_ccw[0], quad_ccw[1], quad_ccw[2], quad_ccw[3]));
  EXPECT_EQ(0, isect_point_quad_v2(outside2, quad_cw[0], quad_cw[1], quad_cw[2], quad_cw[3]));
  EXPECT_EQ(0, isect_point_quad_v2(outside2, quad_ccw[0], quad_ccw[1], quad_ccw[2], quad_ccw[3]));
  EXPECT_EQ(0, isect_point_quad_v2(outside3, quad_cw[0], quad_cw[1], quad_cw[2], quad_cw[3]));
  EXPECT_EQ(0, isect_point_quad_v2(outside3, quad_ccw[0], quad_ccw[1], quad_ccw[2], quad_ccw[3]));
  EXPECT_EQ(0, isect_point_quad_v2(outside4, quad_cw[0], quad_cw[1], quad_cw[2], quad_cw[3]));
  EXPECT_EQ(0, isect_point_quad_v2(outside4, quad_ccw[0], quad_ccw[1], quad_ccw[2], quad_ccw[3]));

  float2 edge1{0, 2};
  float2 edge2{1, -2};
  EXPECT_EQ(-1, isect_point_quad_v2(edge1, quad_cw[0], quad_cw[1], quad_cw[2], quad_cw[3]));
  EXPECT_EQ(+1, isect_point_quad_v2(edge1, quad_ccw[0], quad_ccw[1], quad_ccw[2], quad_ccw[3]));
  EXPECT_EQ(-1, isect_point_quad_v2(edge2, quad_cw[0], quad_cw[1], quad_cw[2], quad_cw[3]));
  EXPECT_EQ(+1, isect_point_quad_v2(edge2, quad_ccw[0], quad_ccw[1], quad_ccw[2], quad_ccw[3]));

  float2 corner1{4, 4};
  float2 corner2{2, -3};
  EXPECT_EQ(-1, isect_point_quad_v2(corner1, quad_cw[0], quad_cw[1], quad_cw[2], quad_cw[3]));
  EXPECT_EQ(+1, isect_point_quad_v2(corner1, quad_ccw[0], quad_ccw[1], quad_ccw[2], quad_ccw[3]));
  EXPECT_EQ(-1, isect_point_quad_v2(corner2, quad_cw[0], quad_cw[1], quad_cw[2], quad_cw[3]));
  EXPECT_EQ(+1, isect_point_quad_v2(corner2, quad_ccw[0], quad_ccw[1], quad_ccw[2], quad_ccw[3]));
}

TEST(math_geom, CrossPoly)
{
  const float tri_cw_2d[3][2] = {{-1, 0}, {0, 1}, {1, 0}};
  const float tri_cw_3d[3][3] = {{-1, 0}, {0, 1}, {1, 0}};

  const float tri_ccw_2d[3][2] = {{1, 0}, {0, 1}, {-1, 0}};
  const float tri_ccw_3d[3][3] = {{1, 0}, {0, 1}, {-1, 0}};

  auto cross_tri_v3_as_float3 = [](const float (*poly)[3]) -> float3 {
    float n[3];
    cross_tri_v3(n, UNPACK3(poly));
    return float3(n[0], n[1], n[2]);
  };

  auto cross_poly_v3_as_float3 = [](const float (*poly)[3]) -> float3 {
    float n[3];
    cross_poly_v3(n, poly, 3);
    return float3(n[0], n[1], n[2]);
  };

  /* Clockwise. */
  EXPECT_EQ(cross_tri_v3_as_float3(tri_cw_3d)[2], -2);
  EXPECT_EQ(cross_tri_v2(UNPACK3(tri_cw_2d)), -2);

  EXPECT_EQ(cross_poly_v3_as_float3(tri_cw_3d)[2], -2);
  EXPECT_EQ(cross_poly_v2(tri_cw_2d, 3), -2);

  /* Counter clockwise. */
  EXPECT_EQ(cross_tri_v3_as_float3(tri_ccw_3d)[2], 2);
  EXPECT_EQ(cross_tri_v2(UNPACK3(tri_ccw_2d)), 2);

  EXPECT_EQ(cross_poly_v3_as_float3(tri_ccw_3d)[2], 2);
  EXPECT_EQ(cross_poly_v2(tri_ccw_2d, 3), 2);
}
