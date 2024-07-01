/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "util/math.h"
#include "util/types.h"

#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/globals.h"

#include "kernel/camera/camera.h"
#include "kernel/camera/projection.h"
#include "kernel/types.h"

CCL_NAMESPACE_BEGIN

/**
 * @brief Test #fisheye_lens_polynomial_to_direction and its inverse
 * #direction_to_fisheye_lens_polynomial by checking if sensor position equals
 * direction_to_fisheye_lens_polynomial(fisheye_lens_polynomial_to_direction/sensor position))
 * for a couple of sensor positions and a couple of different sets of parameters.
 */
TEST(KernelCamera, FisheyeLensPolynomialRoundtrip)
{
  const float fov = 150.0f * (M_PI_F / 180.0f);
  const float width = 36.0f;
  const float height = 41.142857142857144f;

  /* Trivial case: The coefficients create a perfect equidistant fisheye */
  const float4 k_equidistant = make_float4(-5.79e-02f, 0.0f, 0.0f, 0.0f);

  /* The coefficients mimic a stereographic fisheye model */
  const float4 k_stereographic = make_float4(-5.79e-02f, 0.0f, 9.48e-05f, -7.67e-06f);

  /* The coefficients mimic a rectilinear camera (badly, but the point is to have a wide range of
   * tests). */
  const float4 k_rectilinear = make_float4(-6.50e-02f, 0.0f, 8.32e-05f, -1.80e-06f);

  const float4 parameters[]{k_equidistant, k_stereographic, k_rectilinear};

  const std::pair<float, float> points[]{
      {0.1f, 0.4f},
      {0.1f, 0.5f},
      {0.1f, 0.7f},
      {0.5f, 0.5f},
      {0.5f, 0.9f},
      {0.6f, 0.9f},
  };

  /* In the test cases k0 = k2 = 0, because for non-zero values the model is not smooth at the
   * center, but real lenses are really smooth near the center. In order to test the method
   * thoroughly, nonzero values are tested for both parameters. */
  for (const float k0 : {0.0f, -1e-2f, -2e-2f, -5e-2f, -1e-1f}) {
    for (const float k2 : {0.0f, -1e-4f, 1e-4f, -2e-4f, 2e-4f}) {
      for (float4 k : parameters) {
        k.y = k2;
        for (std::pair<float, float> const &pt : points) {
          const float x = pt.first;
          const float y = pt.second;
          const float3 direction = fisheye_lens_polynomial_to_direction(
              pt.first, pt.second, k0, k, fov, width, height);

          EXPECT_NEAR(len(direction), 1, 1e-6) << "x: " << x << std::endl
                                               << "y: " << y << std::endl
                                               << "k0: " << k0 << std::endl
                                               << "k2: " << k2;

          const float2 reprojection = direction_to_fisheye_lens_polynomial(
              direction, k0, k, width, height);

          EXPECT_NEAR(reprojection.x, x, 1e-6) << "k0: " << k0 << std::endl
                                               << "k1: " << k.x << std::endl
                                               << "k2: " << k.y << std::endl
                                               << "k3: " << k.z << std::endl
                                               << "k4: " << k.w << std::endl;
          EXPECT_NEAR(reprojection.y, y, 3e-6) << "k0: " << k0 << std::endl
                                               << "k1: " << k.x << std::endl
                                               << "k2: " << k.y << std::endl
                                               << "k3: " << k.z << std::endl
                                               << "k4: " << k.w << std::endl;
        }
      }
    }
  }
}

/**
 * @brief Test symmetry properties of #fisheye_lens_polynomial_to_direction
 */
TEST(KernelCamera, FisheyeLensPolynomialToDirectionSymmetry)
{
  const float fov = M_PI_F;
  const float width = 1.0f;
  const float height = 1.0f;

  /* Trivial case: The coefficients create a perfect equidistant fisheye */
  const float4 k_equidistant = make_float4(-1.0f, 0.0f, 0.0f, 0.0f);
  const float k0 = 0.0f;

  /* Symmetry tests */
  const float2 center{0.5f, 0.5f};
  const float2 offsets[]{
      {0.00f, 0.00f},
      {0.25f, 0.00f},
      {0.00f, 0.25f},
      {0.25f, 0.25f},

      {0.5f, 0.0f},
      {0.0f, 0.5f},
      {0.5f, 0.5f},

      {0.75f, 0.00f},
      {0.00f, 0.75f},
      {0.75f, 0.75f},
  };

  for (float2 const &offset : offsets) {
    const float2 point = center + offset;
    const float3 direction = fisheye_lens_polynomial_to_direction(
        point.x, point.y, k0, k_equidistant, fov, width, height);
    EXPECT_NEAR(len(direction), 1.0, 1e-6);

    const float2 point_mirror = center - offset;
    const float3 direction_mirror = fisheye_lens_polynomial_to_direction(
        point_mirror.x, point_mirror.y, k0, k_equidistant, fov, width, height);
    EXPECT_NEAR(len(direction_mirror), 1.0, 1e-6);

    EXPECT_NEAR(direction.x, +direction_mirror.x, 1e-6)
        << "offset: (" << offset.x << ", " << offset.y << ")";
    EXPECT_NEAR(direction.y, -direction_mirror.y, 1e-6)
        << "offset: (" << offset.x << ", " << offset.y << ")";
    ;
    EXPECT_NEAR(direction.z, -direction_mirror.z, 1e-6)
        << "offset: (" << offset.x << ", " << offset.y << ")";
    ;
  }
}

/**
 * @brief Test #fisheye_lens_polynomial_to_direction with a couple of hand-crafted reference
 * values.
 */
TEST(KernelCamera, FisheyeLensPolynomialToDirection)
{
  const float fov = M_PI_F;
  const float k0 = 0.0f;

  const float rad60 = M_PI_F / 3.0f;
  const float cos60 = 0.5f;
  const float sin60 = M_SQRT3_F / 2.0f;

  const float rad30 = M_PI_F / 6.0f;
  const float cos30 = M_SQRT3_F / 2.0f;
  const float sin30 = 0.5f;

  const float rad45 = M_PI_4F;
  const float cos45 = M_SQRT1_2F;
  const float sin45 = M_SQRT1_2F;

  const std::pair<float2, float3> tests[]{
      /* Center (0°) */
      {make_float2(0.0f, 0.0f), make_float3(1.0f, 0.0f, 0.0f)},

      /* 60° */
      {make_float2(0.0f, +rad60), make_float3(cos60, 0.0f, +sin60)},
      {make_float2(0.0f, -rad60), make_float3(cos60, 0.0f, -sin60)},
      {make_float2(+rad60, 0.0f), make_float3(cos60, -sin60, 0.0f)},
      {make_float2(-rad60, 0.0f), make_float3(cos60, +sin60, 0.0f)},

      /* 45° */
      {make_float2(0.0f, +rad45), make_float3(cos45, 0.0f, +sin45)},
      {make_float2(0.0f, -rad45), make_float3(cos45, 0.0f, -sin45)},
      {make_float2(+rad45, 0.0f), make_float3(cos45, -sin45, 0.0f)},
      {make_float2(-rad45, 0.0f), make_float3(cos45, +sin45, 0.0f)},

      {make_float2(+rad45 * M_SQRT1_2F, +rad45 * M_SQRT1_2F), make_float3(cos45, -0.5f, +0.5f)},
      {make_float2(-rad45 * M_SQRT1_2F, +rad45 * M_SQRT1_2F), make_float3(cos45, +0.5f, +0.5f)},
      {make_float2(+rad45 * M_SQRT1_2F, -rad45 * M_SQRT1_2F), make_float3(cos45, -0.5f, -0.5f)},
      {make_float2(-rad45 * M_SQRT1_2F, -rad45 * M_SQRT1_2F), make_float3(cos45, +0.5f, -0.5f)},

      /* 30° */
      {make_float2(0.0f, +rad30), make_float3(cos30, 0.0f, +sin30)},
      {make_float2(0.0f, -rad30), make_float3(cos30, 0.0f, -sin30)},
      {make_float2(+rad30, 0.0f), make_float3(cos30, -sin30, 0.0f)},
      {make_float2(-rad30, 0.0f), make_float3(cos30, +sin30, 0.0f)},
  };

  for (auto [offset, direction] : tests) {
    const float2 sensor = offset + make_float2(0.5f, 0.5f);
    for (float const scale : {1.0f, 0.5f, 2.0f, 0.25f, 4.0f, 0.125f, 8.0f, 0.0625f, 16.0f}) {
      const float width = 1.0f / scale;
      const float height = 1.0f / scale;
      /* Trivial case: The coefficients create a perfect equidistant fisheye */
      const float4 k_equidistant = make_float4(-scale, 0.0f, 0.0f, 0.0f);

      const float3 computed = fisheye_lens_polynomial_to_direction(
          sensor.x, sensor.y, k0, k_equidistant, fov, width, height);

      EXPECT_NEAR(direction.x, computed.x, 1e-6)
          << "sensor: (" << sensor.x << ", " << sensor.y << ")" << std::endl
          << "scale: " << scale;
      EXPECT_NEAR(direction.y, computed.y, 1e-6)
          << "sensor: (" << sensor.x << ", " << sensor.y << ")" << std::endl
          << "scale: " << scale;
      EXPECT_NEAR(direction.z, computed.z, 1e-6)
          << "sensor: (" << sensor.x << ", " << sensor.y << ")" << std::endl
          << "scale: " << scale;

      const float2 reprojected = direction_to_fisheye_lens_polynomial(
          direction, k0, k_equidistant, width, height);

      EXPECT_NEAR(sensor.x, reprojected.x, 1e-6) << "scale: " << scale;
      EXPECT_NEAR(sensor.y, reprojected.y, 1e-6) << "scale: " << scale;
    }
  }
}

CCL_NAMESPACE_END
