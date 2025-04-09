/* SPDX-FileCopyrightText: 2011-2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Note: These fixtures test default micro-architecture optimization defined in the
 * util/optimization.h. */

#include "testing/testing.h"
#include "util/math.h"
#include "util/system.h"

CCL_NAMESPACE_BEGIN

class Float4Test : public ::testing::Test {
  void SetUp() override
  {
    /* The micro-architecture check is not needed here, but use it here as a demonstration of how
     * it can be implemented in a clear way. */
    // GTEST_SKIP() << "Test skipped due to uarch capability";
  }
};

TEST_F(Float4Test, fmod)
{
  {
    const float4 c = fmod(make_float4(1.2f, 2.3f, 3.4f, 4.5f), 1.0f);
    EXPECT_NEAR(c.x, 0.2f, 1e-6f);
    EXPECT_NEAR(c.y, 0.3f, 1e-6f);
    EXPECT_NEAR(c.z, 0.4f, 1e-6f);
    EXPECT_NEAR(c.w, 0.5f, 1e-6f);
  }

  {
    const float4 c = fmod(make_float4(1.2f, 2.3f, 3.4f, 0.9f), 1.2f);
    EXPECT_NEAR(c.x, 0.0f, 1e-6f);
    EXPECT_NEAR(c.y, 1.1f, 1e-6f);
    EXPECT_NEAR(c.z, 1.0f, 1e-6f);
    EXPECT_NEAR(c.w, 0.9f, 1e-6f);
  }

  {
    const float4 c = fmod(make_float4(1.2f, 2.3f, 3.4f, 0.0f), 1000000.0f);
    EXPECT_NEAR(c.x, 1.2f, 1e-6f);
    EXPECT_NEAR(c.y, 2.3f, 1e-6f);
    EXPECT_NEAR(c.z, 3.4f, 1e-6f);
  }

  {
    const float4 c = fmod(make_float4(1999999.2f, 2000000.3f, 2000001.4f, 0.0f), 1000000.0f);
    EXPECT_NEAR(c.x, 999999.25f, 1e-6f);
    EXPECT_NEAR(c.y, 0.25f, 1e-6f);
    EXPECT_NEAR(c.z, 1.375f, 1e-6f);
  }

  {
    const float4 c = fmod(make_float4(5.1f, -5.1f, 0.0f, 0.0f), 3.0f);
    EXPECT_NEAR(c.x, 2.1f, 1e-6f);
    EXPECT_NEAR(c.y, -2.1, 1e-6f);
    EXPECT_NEAR(c.z, 0.0f, 1e-6f);
  }

  {
    const float4 c = fmod(make_float4(5.1f, -5.1f, 0.0f, 0.0f), -3.0f);
    EXPECT_NEAR(c.x, 2.1f, 1e-6f);
    EXPECT_NEAR(c.y, -2.1, 1e-6f);
  }
}

CCL_NAMESPACE_END
