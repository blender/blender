/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Note: These fixtures test default micro-architecture optimization defined in the
 * util/optimization.h. */

#include "util/types_spherical_harmonics.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "util/log.h"

CCL_NAMESPACE_BEGIN

/* Use as:
 * float3 vector1;
 * float3 vector2;
 * EXPECT_THAT(vector1, IsNearFloat3(vector2)); */
MATCHER_P2(IsNearFloat3, expected, eps, "")
{
  for (int i = 0; i < 3; ++i) {
    if (fabsf(arg[i] - expected[i]) > eps) {
      *result_listener << "component " << i << " should be " << expected[i];
      return false;
    }
  }
  return true;
}

class PackedSphericalHarmonicsTest : public ::testing::Test {
  void SetUp() override
  {
    /* The micro-architecture check is not needed here, but use it here as a demonstration of how
     * it can be implemented in a clear way. */
    // GTEST_SKIP() << "Test skipped due to uarch capability";
  }
};

TEST_F(PackedSphericalHarmonicsTest, PackUnpack)
{
  PackedSphericalHarmonics sh;

  spherical_harmonics_fill_zero(sh);

  spherical_harmonics_set_coefficient(sh, 1, make_float3(0.1f, -0.2f, 0.3f));
  EXPECT_THAT(spherical_harmonics_get_coefficient(sh, 1),
              IsNearFloat3(make_float3(0.1f, -0.2f, 0.3f), 1.0f / 127.0f));
}

CCL_NAMESPACE_END
