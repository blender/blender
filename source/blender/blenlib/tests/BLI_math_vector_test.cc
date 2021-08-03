/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "BLI_math.h"

TEST(math_vector, ClampVecWithFloats)
{
  const float min = 0.0f;
  const float max = 1.0f;

  float a[2] = {-1.0f, -1.0f};
  clamp_v2(a, min, max);
  EXPECT_FLOAT_EQ(0.0f, a[0]);
  EXPECT_FLOAT_EQ(0.0f, a[1]);

  float b[2] = {0.5f, 0.5f};
  clamp_v2(b, min, max);
  EXPECT_FLOAT_EQ(0.5f, b[0]);
  EXPECT_FLOAT_EQ(0.5f, b[1]);

  float c[2] = {2.0f, 2.0f};
  clamp_v2(c, min, max);
  EXPECT_FLOAT_EQ(1.0f, c[0]);
  EXPECT_FLOAT_EQ(1.0f, c[1]);
}

TEST(math_vector, ClampVecWithVecs)
{
  const float min[2] = {0.0f, 2.0f};
  const float max[2] = {1.0f, 3.0f};

  float a[2] = {-1.0f, -1.0f};
  clamp_v2_v2v2(a, min, max);
  EXPECT_FLOAT_EQ(0.0f, a[0]);
  EXPECT_FLOAT_EQ(2.0f, a[1]);

  float b[2] = {0.5f, 2.5f};
  clamp_v2_v2v2(b, min, max);
  EXPECT_FLOAT_EQ(0.5f, b[0]);
  EXPECT_FLOAT_EQ(2.5f, b[1]);

  float c[2] = {2.0f, 4.0f};
  clamp_v2_v2v2(c, min, max);
  EXPECT_FLOAT_EQ(1.0f, c[0]);
  EXPECT_FLOAT_EQ(3.0f, c[1]);
}

TEST(math_vector, test_invert_v3_safe)
{
  float v3_with_zeroes[3] = {0.0f, 2.0f, 3.0f};
  invert_v3_safe(v3_with_zeroes);
  EXPECT_FLOAT_EQ(0.0f, v3_with_zeroes[0]);
  EXPECT_FLOAT_EQ(0.5f, v3_with_zeroes[1]);
  EXPECT_FLOAT_EQ(0.33333333333f, v3_with_zeroes[2]);

  float v3_without_zeroes[3] = {1.0f, 2.0f, 3.0f};
  float inverted_unsafe[3] = {1.0f, 2.0f, 3.0f};
  invert_v3_safe(v3_without_zeroes);
  invert_v3(inverted_unsafe);

  EXPECT_FLOAT_EQ(inverted_unsafe[0], v3_without_zeroes[0]);
  EXPECT_FLOAT_EQ(inverted_unsafe[1], v3_without_zeroes[1]);
  EXPECT_FLOAT_EQ(inverted_unsafe[2], v3_without_zeroes[2]);
}
