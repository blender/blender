/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_float3x3.hh"
#include "BLI_math_base.h"
#include "BLI_math_vector_types.hh"

namespace blender::tests {

TEST(float3x3, Identity)
{
  float2 point(1.0f, 2.0f);
  float3x3 transformation = float3x3::identity();
  float2 result = transformation * point;
  EXPECT_EQ(result, point);
}

TEST(float3x3, Translation)
{
  float2 point(1.0f, 2.0f);
  float3x3 transformation = float3x3::from_translation(float2(5.0f, 3.0f));
  float2 result = transformation * point;
  EXPECT_FLOAT_EQ(result[0], 6.0f);
  EXPECT_FLOAT_EQ(result[1], 5.0f);
}

TEST(float3x3, Rotation)
{
  float2 point(1.0f, 2.0f);
  float3x3 transformation = float3x3::from_rotation(M_PI_2);
  float2 result = transformation * point;
  EXPECT_FLOAT_EQ(result[0], -2.0f);
  EXPECT_FLOAT_EQ(result[1], 1.0f);
}

TEST(float3x3, Scale)
{
  float2 point(1.0f, 2.0f);
  float3x3 transformation = float3x3::from_scale(float2(2.0f, 3.0f));
  float2 result = transformation * point;
  EXPECT_FLOAT_EQ(result[0], 2.0f);
  EXPECT_FLOAT_EQ(result[1], 6.0f);
}

TEST(float3x3, TranslationRotationScale)
{
  float2 point(1.0f, 2.0f);
  float3x3 transformation = float3x3::from_translation_rotation_scale(
      float2(1.0f, 3.0f), M_PI_2, float2(2.0f, 3.0f));
  float2 result = transformation * point;
  EXPECT_FLOAT_EQ(result[0], -5.0f);
  EXPECT_FLOAT_EQ(result[1], 5.0f);
}

TEST(float3x3, NormalizedAxes)
{
  float2 point(1.0f, 2.0f);

  /* The horizontal is aligned with (1, 1) and vertical is aligned with (-1, 1), in other words, a
   * Pi / 4 rotation. */
  float value = std::sqrt(2.0f) / 2.0f;
  float3x3 transformation = float3x3::from_normalized_axes(
      float2(1.0f, 3.0f), float2(value), float2(-value, value));
  float2 result = transformation * point;

  float3x3 expected_transformation = float3x3::from_translation_rotation_scale(
      float2(1.0f, 3.0f), M_PI_4, float2(1.0f));
  float2 expected = expected_transformation * point;

  EXPECT_FLOAT_EQ(result[0], expected[0]);
  EXPECT_FLOAT_EQ(result[1], expected[1]);
}

TEST(float3x3, PostTransformationMultiplication)
{
  float2 point(1.0f, 2.0f);
  float3x3 translation = float3x3::from_translation(float2(5.0f, 3.0f));
  float3x3 rotation = float3x3::from_rotation(M_PI_2);
  float3x3 transformation = translation * rotation;
  float2 result = transformation * point;
  EXPECT_FLOAT_EQ(result[0], 3.0f);
  EXPECT_FLOAT_EQ(result[1], 4.0f);
}

TEST(float3x3, PreTransformationMultiplication)
{
  float2 point(1.0f, 2.0f);
  float3x3 translation = float3x3::from_translation(float2(5.0f, 3.0f));
  float3x3 rotation = float3x3::from_rotation(M_PI_2);
  float3x3 transformation = rotation * translation;
  float2 result = transformation * point;
  EXPECT_FLOAT_EQ(result[0], -5.0f);
  EXPECT_FLOAT_EQ(result[1], 6.0f);
}

TEST(float3x3, TransformationMultiplicationAssignment)
{
  float2 point(1.0f, 2.0f);
  float3x3 transformation = float3x3::from_translation(float2(5.0f, 3.0f));
  transformation *= float3x3::from_rotation(M_PI_2);
  float2 result = transformation * point;
  EXPECT_FLOAT_EQ(result[0], 3.0f);
  EXPECT_FLOAT_EQ(result[1], 4.0f);
}

TEST(float3x3, Inverted)
{
  float2 point(1.0f, 2.0f);
  float3x3 transformation = float3x3::from_translation_rotation_scale(
      float2(1.0f, 3.0f), M_PI_4, float2(1.0f));
  transformation *= transformation.inverted();
  float2 result = transformation * point;
  EXPECT_FLOAT_EQ(result[0], 1.0f);
  EXPECT_FLOAT_EQ(result[1], 2.0f);
}

TEST(float3x3, Origin)
{
  float2 point(1.0f, 2.0f);
  float3x3 rotation = float3x3::from_rotation(M_PI_2);
  float3x3 transformation = float3x3::from_origin_transformation(rotation, float2(0.0f, 2.0f));
  float2 result = transformation * point;
  EXPECT_FLOAT_EQ(result[0], 0.0f);
  EXPECT_FLOAT_EQ(result[1], 3.0f);
}

TEST(float3x3, GetScale2D)
{
  float2 scale(2.0f, 3.0f);
  float3x3 transformation = float3x3::from_scale(scale);
  EXPECT_EQ(scale, transformation.scale_2d());
}

}  // namespace blender::tests
