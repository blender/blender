/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Note: These fixtures test default micro-architecture optimization defined in the
 * util/optimization.h. */

#include "util/math_quaternion.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "util/log.h"
#include "util/transform.h"

#include <iostream>

CCL_NAMESPACE_BEGIN

using testing::Not;

/* Use as:
 * Quaternion q1;
 * Quaternion q2;
 * EXPECT_THAT(q1, IsNearQuaternion(q2, eps)); */
MATCHER_P2(IsNearQuaternion, expected, eps, "")
{
  if (fabsf(arg.w - expected.w) > eps) {
    *result_listener << "component w should be " << expected.w;
    return false;
  }
  if (fabsf(arg.x - expected.x) > eps) {
    *result_listener << "component x should be " << expected.x;
    return false;
  }
  if (fabsf(arg.y - expected.y) > eps) {
    *result_listener << "component y should be " << expected.y;
    return false;
  }
  if (fabsf(arg.z - expected.z) > eps) {
    *result_listener << "component z should be " << expected.z;
    return false;
  }
  return true;
}

/* TODO(sergey): Move to log.h? */
std::ostream &operator<<(std::ostream &os, const Quaternion &q)
{
  os << "(" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ")";
  return os;
}

class QuaternionTest : public ::testing::Test {
  void SetUp() override
  {
    /* The micro-architecture check is not needed here, but use it here as a demonstration of how
     * it can be implemented in a clear way. */
    // GTEST_SKIP() << "Test skipped due to uarch capability";
  }
};

/* Basic test for user-defined matcher. */
TEST_F(QuaternionTest, IsNearQuaternion)
{
  const Quaternion identity = identity_quaternion();
  const Quaternion other = make_quaternion(1.0f, 2.0f, 3.0f, 4.0f);

  EXPECT_THAT(identity, IsNearQuaternion(identity, 1e-6f));
  EXPECT_THAT(other, Not(IsNearQuaternion(identity, 1e-6f)));
}

TEST_F(QuaternionTest, Negate)
{
  EXPECT_THAT(-make_quaternion(1.0f, -2.0f, 3.0f, -4.0f),
              IsNearQuaternion(make_quaternion(-1.0f, 2.0f, -3.0f, 4.0), 1e-6f));
}

TEST_F(QuaternionTest, MultiplyScalar)
{
  EXPECT_THAT(make_quaternion(1.0f, -2.0f, 3.0f, -4.0f) * 0.5f,
              IsNearQuaternion(make_quaternion(0.5f, -1.0f, 1.5f, -2.0f), 1e-6f));
  EXPECT_THAT(0.5f * make_quaternion(1.0f, -2.0f, 3.0f, -4.0f),
              IsNearQuaternion(make_quaternion(0.5f, -1.0f, 1.5f, -2.0f), 1e-6f));
}

TEST_F(QuaternionTest, Add)
{
  EXPECT_THAT(make_quaternion(1.0f, -2.0f, 3.0f, -4.0f) +
                  make_quaternion(0.1f, -0.2f, 0.3f, -0.4f),
              IsNearQuaternion(make_quaternion(1.1f, -2.2f, 3.3f, -4.4f), 1e-6f));

  {
    Quaternion a = make_quaternion(1.0f, -2.0f, 3.0f, -4.0f);
    a += make_quaternion(0.1f, -0.2f, 0.3f, -0.4f);
    EXPECT_THAT(a, IsNearQuaternion(make_quaternion(1.1f, -2.2f, 3.3f, -4.4f), 1e-6f));
  }
}

TEST_F(QuaternionTest, Subtract)
{
  EXPECT_THAT(make_quaternion(1.1f, -2.2f, 3.3f, -4.4f) -
                  make_quaternion(0.1f, -0.2f, 0.3f, -0.4f),
              IsNearQuaternion(make_quaternion(1.0f, -2.0f, 3.0f, -4.0f), 1e-6f));

  {
    Quaternion a = make_quaternion(1.1f, -2.2f, 3.3f, -4.4f);
    a -= make_quaternion(0.1f, -0.2f, 0.3f, -0.4f);
    EXPECT_THAT(a, IsNearQuaternion(make_quaternion(1.0f, -2.0f, 3.0f, -4.0f), 1e-6f));
  }
}

TEST_F(QuaternionTest, dot)
{
  EXPECT_NEAR(
      dot(make_quaternion(1.0f, -2.0f, 3.0f, -4.0f), make_quaternion(0.1f, -0.2f, 0.3f, -0.4f)),
      3.0f,
      1e-6f);
}

TEST_F(QuaternionTest, normalized)
{
  EXPECT_THAT(normalized(make_quaternion(1.0f, 0.0f, 0.0f, 0.0f)),
              IsNearQuaternion(make_quaternion(1.0f, 0.0f, 0.0f, 0.0f), 1e-6f));

  EXPECT_THAT(
      normalized(make_quaternion(1.2f, 2.3f, 3.4f, 4.5f)),
      IsNearQuaternion(make_quaternion(0.193297f, 0.370486f, 0.547675f, 0.724864f), 1e-5f));
}

TEST_F(QuaternionTest, lerp)
{
  EXPECT_THAT(
      lerp(make_quaternion(1.2f, 2.3f, 3.4f, 4.5f), make_quaternion(6.7f, 8.9f, 1.0f, 2.3f), 0.2f),
      IsNearQuaternion(make_quaternion(0.349109f, 0.549467f, 0.443217f, 0.616253f), 1e-5f));
  EXPECT_THAT(
      lerp(
          make_quaternion(1.2f, 2.3f, 3.4f, 4.5f), -make_quaternion(6.7f, 8.9f, 1.0f, 2.3f), 0.2f),
      IsNearQuaternion(make_quaternion(0.349109f, 0.549467f, 0.443217f, 0.616253f), 1e-5f));
}

CCL_NAMESPACE_END
