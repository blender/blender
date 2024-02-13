/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

/** \file
 * Test that convex hull calculation and fitting convex hulls
 * to a bounding box is working properly.
 *
 * \note Bounding box fitting checks compare against exact values.
 * In this case there are multiple correct angles since both
 * 45 degrees & -45 degrees will give the desired outcome.
 * Keep using exact value matches so any changes to the return values are detected.
 * If this becomes a problem for maintaining tests then values could be normalized for comparison.
 */

#include "testing/testing.h"

#include "BLI_array.hh"
#include "BLI_convexhull_2d.h"
#include "BLI_math_angle_types.hh"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_rand.hh"

using namespace blender;

/**
 * Increase to a large number (8k or so) to test many permutations,
 * too slow for regular tests.
 */
#define DEFAULT_TEST_ITER 8

#define DEFAULT_TEST_RANDOM_SEED 123

/** The epsilon to use when comparing floating point rotations (as radians). */
#define ROTATION_EPS 1e-6

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

/* Brute force test. */
static float convexhull_aabb_fit_hull_2d_for_comparison(blender::Span<float2> points_hull)
{
  float area_best = FLT_MAX;
  float dvec_best[2]; /* best angle, delay atan2 */

  for (int i = 0, i_prev = points_hull.size() - 1; i < points_hull.size(); i_prev = i++) {
    float2 dvec = points_hull[i] - points_hull[i_prev]; /* 2d rotation matrix */
    if (UNLIKELY(normalize_v2(&dvec[0]) == 0.0f)) {
      continue;
    }
    /* rotation matrix */
    float min[2] = {FLT_MAX, FLT_MAX}, max[2] = {-FLT_MAX, -FLT_MAX};
    float area_test;

    for (int j = 0; j < points_hull.size(); j++) {
      float tvec[2];
      mul_v2_v2_cw(tvec, dvec, points_hull[j]);

      min[0] = std::min(min[0], tvec[0]);
      min[1] = std::min(min[1], tvec[1]);

      max[0] = std::max(max[0], tvec[0]);
      max[1] = std::max(max[1], tvec[1]);

      area_test = (max[0] - min[0]) * (max[1] - min[1]);
      if (area_test > area_best) {
        break;
      }
    }

    if (area_test < area_best) {
      area_best = area_test;
      copy_v2_v2(dvec_best, dvec);
    }
  }

  return (area_best != FLT_MAX) ? (float)atan2(dvec_best[0], dvec_best[1]) : 0.0f;
}

static blender::Array<float2> convexhull_points_from_map(blender::Span<float2> points,
                                                         blender::Span<int> points_map)
{
  blender::Array<float2> points_hull(points_map.size());
  int index = 0;
  for (int p_index : points_map) {
    points_hull[index++] = points[p_index];
  }
  return points_hull;
}

static blender::Array<float2> convexhull_2d_as_array(blender::Span<float2> points)
{
  blender::Array<int> points_hull_map(points.size());
  int points_hull_map_num = BLI_convexhull_2d(
      reinterpret_cast<const float(*)[2]>(points.data()), points.size(), points_hull_map.data());

  blender::Span<int> points_hull_map_span(points_hull_map.data(), points_hull_map_num);
  return convexhull_points_from_map(points, points_hull_map_span);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Wrap Public API's
 * \{ */

static float convexhull_aabb_fit_hull_2d(blender::Span<float2> points_hull)
{
  return BLI_convexhull_aabb_fit_hull_2d(reinterpret_cast<const float(*)[2]>(points_hull.data()),
                                         points_hull.size());
}

static float convexhull_2d_aabb_fit_points_2d(blender::Span<float2> points)
{
  return BLI_convexhull_aabb_fit_points_2d(reinterpret_cast<const float(*)[2]>(points.data()),
                                           points.size());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests
 * \{ */

TEST(convexhull_2d, IsConvex)
{
  blender::Array<float2> points(12);
  RandomNumberGenerator rng = RandomNumberGenerator(DEFAULT_TEST_RANDOM_SEED);
  for (int iter = 0; iter < DEFAULT_TEST_ITER; iter++) {
    for (float2 &p : points) {
      p = float2(rng.get_float(), rng.get_float());
    }
    blender::Array<float2> points_hull = convexhull_2d_as_array(points);
    if (UNLIKELY(points_hull.size() < 3)) {
      continue;
    }

    int i_prev = points_hull.size() - 2;
    int i_curr = points_hull.size() - 1;
    for (int i_next = 0; i_next < points_hull.size(); i_prev = i_curr, i_curr = i_next++) {
      EXPECT_GE(cross_tri_v2(points_hull[i_prev], points_hull[i_curr], points_hull[i_next]), 0.0f);
    }
  }
}

TEST(convexhull_2d, IsCCW)
{
  blender::Array<float2> points(12);
  RandomNumberGenerator rng = RandomNumberGenerator(DEFAULT_TEST_RANDOM_SEED);
  for (int iter = 0; iter < DEFAULT_TEST_ITER; iter++) {
    for (float2 &p : points) {
      p = float2(rng.get_float(), rng.get_float());
    }
    blender::Array<float2> points_hull = convexhull_2d_as_array(points);

    EXPECT_GE(
        cross_poly_v2(reinterpret_cast<const float(*)[2]>(points_hull.data()), points_hull.size()),
        0.0f);
  }
}

TEST(convexhull_2d, NOP)
{
  { /* Single point. */
    blender::Array<float2> points = {{0.0f, 0.0f}};
    EXPECT_NEAR(convexhull_2d_aabb_fit_points_2d(points), 0.0f, ROTATION_EPS);
  }

  { /* Single point, 2x duplicates. */
    blender::Array<float2> points = {{0.0f, 0.0f}, {0.0f, 0.0f}};
    EXPECT_NEAR(convexhull_2d_aabb_fit_points_2d(points), 0.0f, ROTATION_EPS);
  }
  { /* Single point, 3x duplicates. */
    blender::Array<float2> points = {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}};
    EXPECT_NEAR(convexhull_2d_aabb_fit_points_2d(points), 0.0f, ROTATION_EPS);
  }
}

TEST(convexhull_2d, Lines_AxisAligned)
{
  { /* Horizontal line (2 points). */
    for (int sign_x = -1; sign_x <= 2; sign_x += 2) {
      blender::Array<float2> points = {{0.0f, 0.0f}, {1.0f * sign_x, 0.0}};
      EXPECT_NEAR(convexhull_2d_aabb_fit_points_2d(points),
                  float(math::AngleRadian::from_degree(-90.0f)),
                  ROTATION_EPS);
    }
  }
  { /* Horizontal line (3 points). */
    for (int sign_x = -1; sign_x <= 2; sign_x += 2) {
      blender::Array<float2> points = {{0.0f, 0.0f}, {1.0f * sign_x, 0.0}, {2.0f * sign_x, 0.0}};
      EXPECT_NEAR(convexhull_2d_aabb_fit_points_2d(points),
                  float(math::AngleRadian::from_degree(-90.0f)),
                  ROTATION_EPS);
    }
  }

  { /* Vertical line (2 points). */
    for (int sign_y = -1; sign_y <= 2; sign_y += 2) {
      blender::Array<float2> points = {{0.0f, 0.0f}, {0.0f, 1.0f * sign_y}};
      EXPECT_NEAR(convexhull_2d_aabb_fit_points_2d(points),
                  float(math::AngleRadian::from_degree(180.0f)),
                  ROTATION_EPS);
    }
  }
  { /* Vertical line (3 points). */
    for (int sign_y = -1; sign_y <= 2; sign_y += 2) {
      blender::Array<float2> points = {{0.0f, 0.0f}, {0.0f, 1.0f * sign_y}, {0.0f, 2.0f * sign_y}};
      EXPECT_NEAR(convexhull_2d_aabb_fit_points_2d(points),
                  float(math::AngleRadian::from_degree(180.0f)),
                  ROTATION_EPS);
    }
  }

  { /* Horizontal line (many points). */
    blender::Array<float2> points(8);
    RandomNumberGenerator rng = RandomNumberGenerator(DEFAULT_TEST_RANDOM_SEED);
    for (int iter = 0; iter < DEFAULT_TEST_ITER; iter++) {
      /* Add points, Y is always positive. */
      for (float2 &p : points) {
        p = rng.get_unit_float2();
        p[0] = 0.0;
      }

      blender::Array<float2> points_hull = convexhull_2d_as_array(points);
      EXPECT_NEAR(std::fmod(convexhull_aabb_fit_hull_2d(points_hull), M_PI), 0.0f, ROTATION_EPS);
    }
  }

  { /* Vertical line (many points). */
    blender::Array<float2> points(8);
    RandomNumberGenerator rng = RandomNumberGenerator(DEFAULT_TEST_RANDOM_SEED);
    for (int iter = 0; iter < DEFAULT_TEST_ITER; iter++) {
      /* Add points, Y is always positive. */
      for (float2 &p : points) {
        p = rng.get_unit_float2();
        p[0] = 0.0;
      }

      blender::Array<float2> points_hull = convexhull_2d_as_array(points);
      EXPECT_NEAR(std::fmod(convexhull_aabb_fit_hull_2d(points_hull), M_PI), 0.0f, ROTATION_EPS);
    }
  }
}

TEST(convexhull_2d, Lines_Diagonal)
{
  { /* Diagonal line (2 points). */
    const float expected[4] = {-135, 135, 135, -135};
    int index = 0;
    for (int sign_x = -1; sign_x <= 2; sign_x += 2) {
      for (int sign_y = -1; sign_y <= 2; sign_y += 2) {
        blender::Array<float2> points = {{0.0f, 0.0f}, {1.0f * sign_x, 1.0f * sign_y}};
        EXPECT_NEAR(convexhull_2d_aabb_fit_points_2d(points),
                    float(math::AngleRadian::from_degree(expected[index])),
                    ROTATION_EPS);
        index++;
      }
    }
  }

  { /* Diagonal line (3 points). */
    const float expected[4] = {-135, 135, 135, -135};
    int index = 0;
    for (int sign_x = -1; sign_x <= 2; sign_x += 2) {
      for (int sign_y = -1; sign_y <= 2; sign_y += 2) {
        blender::Array<float2> points = {
            {0.0f, 0.0f},
            {1.0f * sign_x, 1.0f * sign_y},
            {2.0f * sign_x, 2.0f * sign_y},
        };
        EXPECT_NEAR(convexhull_2d_aabb_fit_points_2d(points),
                    float(math::AngleRadian::from_degree(expected[index])),
                    ROTATION_EPS);
        index++;
      }
    }
  }
}

TEST(convexhull_2d, Simple)
{
  { /* 45degree rotated square. */
    blender::Array<float2> points = {
        {0.0f, -1.0f},
        {-1.0f, 0.0f},
        {0.0f, 1.0f},
        {1.0f, 0.0f},
    };
    EXPECT_NEAR(convexhull_2d_aabb_fit_points_2d(points),
                float(math::AngleRadian::from_degree(135.0f)),
                ROTATION_EPS);
  }

  { /* Axis aligned square. */
    blender::Array<float2> points = {
        {-1.0f, -1.0f},
        {-1.0f, 1.0f},
        {1.0f, 1.0f},
        {1.0f, -1.0f},
    };
    EXPECT_NEAR(convexhull_2d_aabb_fit_points_2d(points),
                float(math::AngleRadian::from_degree(180.0f)),
                ROTATION_EPS);
  }
}

TEST(convexhull_2d, AABB_Fit)
{
  blender::Array<float2> points(32);
  RandomNumberGenerator rng = RandomNumberGenerator(DEFAULT_TEST_RANDOM_SEED);
  for (int iter = 0; iter < DEFAULT_TEST_ITER; iter++) {
    for (float2 &p : points) {
      p = float2(rng.get_float(), rng.get_float());
    }
    blender::Array<float2> points_hull = convexhull_2d_as_array(points);

    EXPECT_NEAR(convexhull_aabb_fit_hull_2d(points_hull),
                convexhull_aabb_fit_hull_2d_for_comparison(points_hull),
                ROTATION_EPS);
  }
}

TEST(convexhull_2d, AABB_Fit_Circular)
{
  /* Use random unit vectors for a shape that's close to a circle.
   * This test is useful as there will be many more rotations which are close fits.
   * The probability increases as the number of points increases. */
  blender::Array<float2> points(32);
  RandomNumberGenerator rng = RandomNumberGenerator(DEFAULT_TEST_RANDOM_SEED);
  for (int iter = 0; iter < DEFAULT_TEST_ITER; iter++) {
    for (float2 &p : points) {
      p = rng.get_unit_float2();
    }
    blender::Array<float2> points_hull = convexhull_2d_as_array(points);

    EXPECT_NEAR(convexhull_aabb_fit_hull_2d(points_hull),
                convexhull_aabb_fit_hull_2d_for_comparison(points_hull),
                ROTATION_EPS);
  }
}

TEST(convexhull_2d, AABB_Fit_LopSided)
{
  blender::Array<float2> points(32);
  RandomNumberGenerator rng = RandomNumberGenerator(DEFAULT_TEST_RANDOM_SEED);
  for (int iter = 0; iter < DEFAULT_TEST_ITER; iter++) {
    /* Add points, Y is always positive. */
    for (float2 &p : points) {
      p = rng.get_unit_float2();
      p[1] = std::abs(p[1]);
    }
    /* A single point negative Y point. */
    points[points.size() / 2] = float2(0.0f, -2.0f);

    blender::Array<float2> points_hull = convexhull_2d_as_array(points);

    EXPECT_NEAR(convexhull_aabb_fit_hull_2d(points_hull),
                convexhull_aabb_fit_hull_2d_for_comparison(points_hull),
                ROTATION_EPS);
  }
}

/** \} */
