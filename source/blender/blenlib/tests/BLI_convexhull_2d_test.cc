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
#include "BLI_math_matrix.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_rotation.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_rand.hh"

using namespace blender;

/**
 * Increase to a large number (8k or so) to test many permutations,
 * too slow for regular tests.
 */
#define DEFAULT_TEST_ITER 8

/** The size of a polygon when generating data.  */
#define DEFAULT_TEST_POLY_NUM 12

#define DEFAULT_TEST_RANDOM_SEED 123

/** The epsilon to use when comparing floating point rotations (as radians). */
#define ROTATION_EPS 1e-6

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

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
  blender::Array<float2> points(DEFAULT_TEST_POLY_NUM);
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
  blender::Array<float2> points(DEFAULT_TEST_POLY_NUM);
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
                  float(math::AngleRadian::from_degree(90.0f)),
                  ROTATION_EPS);
    }
  }
  { /* Horizontal line (3 points). */
    for (int sign_x = -1; sign_x <= 2; sign_x += 2) {
      blender::Array<float2> points = {{0.0f, 0.0f}, {1.0f * sign_x, 0.0}, {2.0f * sign_x, 0.0}};
      EXPECT_NEAR(convexhull_2d_aabb_fit_points_2d(points),
                  float(math::AngleRadian::from_degree(90.0f)),
                  ROTATION_EPS);
    }
  }

  { /* Vertical line (2 points). */
    for (int sign_y = -1; sign_y <= 2; sign_y += 2) {
      blender::Array<float2> points = {{0.0f, 0.0f}, {0.0f, 1.0f * sign_y}};
      EXPECT_NEAR(convexhull_2d_aabb_fit_points_2d(points),
                  float(math::AngleRadian::from_degree(0.0f)),
                  ROTATION_EPS);
    }
  }
  { /* Vertical line (3 points). */
    for (int sign_y = -1; sign_y <= 2; sign_y += 2) {
      blender::Array<float2> points = {{0.0f, 0.0f}, {0.0f, 1.0f * sign_y}, {0.0f, 2.0f * sign_y}};
      EXPECT_NEAR(convexhull_2d_aabb_fit_points_2d(points),
                  float(math::AngleRadian::from_degree(0.0f)),
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

      EXPECT_NEAR(convexhull_2d_aabb_fit_points_2d(points), 0.0f, ROTATION_EPS);
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
      EXPECT_NEAR(convexhull_2d_aabb_fit_points_2d(points_hull), 0.0f, ROTATION_EPS);
    }
  }
}

TEST(convexhull_2d, Lines_Diagonal)
{
  { /* Diagonal line (2 points). */
    const float expected[4] = {45, -45, -45, 45};
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
    const float expected[4] = {45, -45, -45, 45};
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
                float(math::AngleRadian::from_degree(45.0f)),
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
                float(math::AngleRadian::from_degree(90.0f)),
                ROTATION_EPS);
  }
}

/**
 * Generate complex rotated/translated shapes with a known size.
 * Check the rotation returned by #BLI_convexhull_aabb_fit_points_2d
 * rotates the points into a bounding box with an area no larger than generated size.
 */
TEST(convexhull_2d, Complex)
{
  auto shape_generate_fn = [](RandomNumberGenerator &rng,
                              const float2 &size,
                              const int points_num) -> blender::Array<float2> {
    /* Avoid zero area boxes. */
    blender::Array<float2> points(points_num);
    const int points_num_reserved = 4;
    BLI_assert(points_num_reserved >= 4);

    /* Ensure there are always points at the bounds. */
    points[0] = {0.0f, rng.get_float()}; /* Left. */
    points[1] = {1.0f, rng.get_float()}; /* Right. */
    points[2] = {rng.get_float(), 0.0f}; /* Bottom. */
    points[3] = {rng.get_float(), 1.0f}; /* Top. */

    for (int i = points_num_reserved; i < points_num; i++) {
      points[i] = {rng.get_float(), rng.get_float()};
    }

    /* Shuffle to ensure the solution is valid no matter the order of the input,
     * Only the first `points_num_reserved` matter as remaining points are random. */
    for (int i = 0; i < points_num_reserved; i++) {
      std::swap(points[i], points[rng.get_int32(points_num)]);
    }

    /* Map from 0-1 to a random transformation. */
    const float2 translation = {
        (rng.get_float() * 2.0f) - 1.0f,
        (rng.get_float() * 2.0f) - 1.0f,
    };

    const float2x2 rot_mat = math::from_rotation<float2x2>(
        math::AngleRadian(rng.get_float() * M_PI));
    for (float2 &p : points) {
      BLI_assert(p[0] >= 0.0 && p[0] <= 1.0f);
      BLI_assert(p[1] >= 0.0 && p[1] <= 1.0f);
      /* Center from [-0.5..0.5], apply size, rotate & translate.  */
      p = (((p - float2(0.5f, 0.5f)) * size) * rot_mat) + translation;
    }

    return points;
  };

  RandomNumberGenerator rng = RandomNumberGenerator(DEFAULT_TEST_RANDOM_SEED);
  for (int i = 0; i < DEFAULT_TEST_ITER; i++) {
    constexpr float size_margin = 0.1;
    /* Random size from `[size_margin..2]`. */
    float2 size = {
        math::min((rng.get_float() * 2.0f) + size_margin, 2.0f),
        math::min((rng.get_float() * 2.0f) + size_margin, 2.0f),
    };

    blender::Array<float2> points = shape_generate_fn(rng, size, DEFAULT_TEST_POLY_NUM);
    const float angle = convexhull_2d_aabb_fit_points_2d(points);

    const float2x2 rot_mat = math::from_rotation<float2x2>(-angle);
    float2 tempmin, tempmax;
    INIT_MINMAX2(tempmin, tempmax);
    for (const float2 &p : points) {
      math::min_max(p * rot_mat, tempmin, tempmax);
    }

    const float2 size_result = tempmax - tempmin;
    float area_input = size[0] * size[1];
    float area_result = size_result[0] * size_result[1];
    EXPECT_LE(area_result, area_input + 1e-6f);
  }
}

/** \} */
