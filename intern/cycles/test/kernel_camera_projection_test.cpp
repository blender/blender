/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "util/math.h"
#include "util/types.h"

#include "kernel/camera/projection.h"

CCL_NAMESPACE_BEGIN

/**
 * @brief Test #fisheye_lens_polynomial_to_direction and its inverse
 * #direction_to_fisheye_lens_polynomial by checking if sensor position equals
 * direction_to_fisheye_lens_polynomial(fisheye_lens_polynomial_to_direction(sensor position))
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
        for (const std::pair<float, float> &pt : points) {
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

  for (const float2 &offset : offsets) {
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
    for (const float scale : {1.0f, 0.5f, 2.0f, 0.25f, 4.0f, 0.125f, 8.0f, 0.0625f, 16.0f}) {
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

/**
 * @brief The CommonValues struct contains information about the tests
 * which is common across the different tests.
 * Derived classes may override functions to make tests less strict
 * if necessary.
 */
struct CommonValues {
  /**
   * @brief Threshold for the reprojection error.
   * @return
   */
  virtual double threshold() const
  {
    return 2e-6;
  }

  /**
   * @brief If skip_invalid returns true, invalid unprojections are ignored in the test.
   * @return
   */
  virtual bool skip_invalid() const
  {
    return false;
  }
};

struct Spherical : public CommonValues {
  static float2 direction_to_sensor(const float3 &dir,
                                    const float fov,
                                    const float width,
                                    const float height)
  {
    return direction_to_spherical(dir);
  }
  static float3 sensor_to_direction(const float2 &sensor,
                                    const float fov,
                                    const float width,
                                    const float height)
  {
    return spherical_to_direction(sensor.x, sensor.y);
  }
};

struct Equirectangular : public CommonValues {
  static float2 direction_to_sensor(const float3 &dir,
                                    const float fov,
                                    const float width,
                                    const float height)
  {
    return direction_to_equirectangular(dir);
  }
  static float3 sensor_to_direction(const float2 &sensor,
                                    const float fov,
                                    const float width,
                                    const float height)
  {
    return equirectangular_to_direction(sensor.x, sensor.y);
  }
};

struct FisheyeEquidistant : public CommonValues {
  static float2 direction_to_sensor(const float3 &dir,
                                    const float fov,
                                    const float width,
                                    const float height)
  {
    return direction_to_fisheye_equidistant(dir, fov);
  }
  static float3 sensor_to_direction(const float2 &sensor,
                                    const float fov,
                                    const float width,
                                    const float height)
  {
    return fisheye_equidistant_to_direction(sensor.x, sensor.y, fov);
  }
};

struct FisheyeEquisolid : public CommonValues {
  bool skip_invalid() const override
  {
    return true;
  }

  static constexpr float lens = 15.0f;

  static float2 direction_to_sensor(const float3 &dir,
                                    const float fov,
                                    const float width,
                                    const float height)
  {
    return direction_to_fisheye_equisolid(dir, lens, width, height);
  }
  static float3 sensor_to_direction(const float2 &sensor,
                                    const float fov,
                                    const float width,
                                    const float height)
  {
    return fisheye_equisolid_to_direction(sensor.x, sensor.y, lens, fov, width, height);
  }
};

struct MirrorBall : public CommonValues {
  static float2 direction_to_sensor(const float3 &dir,
                                    const float fov,
                                    const float width,
                                    const float height)
  {
    return direction_to_mirrorball(dir);
  }
  static float3 sensor_to_direction(const float2 &sensor,
                                    const float fov,
                                    const float width,
                                    const float height)
  {
    return mirrorball_to_direction(sensor.x, sensor.y);
  }
};

struct EquiangularCubemapFace : public CommonValues {
  static float2 direction_to_sensor(const float3 &dir,
                                    const float fov,
                                    const float width,
                                    const float height)
  {
    return direction_to_equiangular_cubemap_face(dir);
  }
  static float3 sensor_to_direction(const float2 &sensor,
                                    const float fov,
                                    const float width,
                                    const float height)
  {
    return equiangular_cubemap_face_to_direction(sensor.x, sensor.y);
  }
};

template<typename T> class PanoramaProjection : public testing::Test {};
using MyTypes = ::testing::Types<Spherical,
                                 Equirectangular,
                                 FisheyeEquidistant,
                                 FisheyeEquisolid,
                                 MirrorBall,
                                 EquiangularCubemapFace>;
TYPED_TEST_SUITE(PanoramaProjection, MyTypes);

/**
 * @brief Test <projection>_to_direction and its inverse
 * direction_to_<projection> by checking if sensor position equals
 * direction_to_<projection>(<projection>_to_direction(sensor position))
 * for a couple of sensor positions and a couple of different sets of parameters.
 */
TYPED_TEST(PanoramaProjection, round_trip)
{

  const TypeParam test;

  const float2 sensors[]{{0.5f, 0.5f},
                         {0.4f, 0.4f},
                         {0.3f, 0.3f},
                         {0.4f, 0.6f},
                         {0.3f, 0.7f},
                         {0.2f, 0.8f},
                         {0.5f, 0.9f},
                         {0.5f, 0.1f},
                         {0.1f, 0.5f},
                         {0.9f, 0.5f}};

  for (const float size : {36.0f, 24.0f, 6.0f * M_PI_F}) {
    const float width = size;
    const float height = size;
    for (const float fov : {2.0f * M_PI_F, M_PI_F, M_PI_2_F, M_PI_4_F, 1.0f, 2.0f}) {
      size_t test_count = 0;
      for (const float2 &sensor : sensors) {
        const float3 direction = TypeParam::sensor_to_direction(sensor, fov, width, height);
        if (test.skip_invalid() && len(direction) < 0.9f) {
          continue;
        }
        test_count++;
        EXPECT_NEAR(len(direction), 1.0, 1e-6)
            << "dir: (" << direction.x << ", " << direction.y << ", " << direction.z << ")"
            << std::endl
            << "fov: " << fov << std::endl
            << "sensor: (" << sensor.x << ", " << sensor.y << ")" << std::endl;
        const float2 projection = TypeParam::direction_to_sensor(direction, fov, width, height);
        EXPECT_NEAR(sensor.x, projection.x, test.threshold())
            << "dir: (" << direction.x << ", " << direction.y << ", " << direction.z << ")"
            << std::endl
            << "fov: " << fov << std::endl
            << "sensor: (" << sensor.x << ", " << sensor.y << ")" << std::endl;
        EXPECT_NEAR(sensor.y, projection.y, test.threshold())
            << "dir: (" << direction.x << ", " << direction.y << ", " << direction.z << ")"
            << std::endl
            << "fov: " << fov << std::endl
            << "sensor: (" << sensor.x << ", " << sensor.y << ")" << std::endl;
      }
      EXPECT_GE(test_count, 2) << "fov: " << fov << std::endl << "size: " << size << std::endl;
    }
  }
}

CCL_NAMESPACE_END
