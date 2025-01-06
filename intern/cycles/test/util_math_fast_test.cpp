/* SPDX-FileCopyrightText: 2024 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "util/math.h"
#include "util/math_fast.h"

CCL_NAMESPACE_BEGIN

TEST(math, fast_sinf)
{
  /* Test values in -2PI .. 2PI range. */
  constexpr int N = 1000 + 1;
  for (int i = 0; i < N; ++i) {
    const float delta = 4 * M_PI_F / (N - 1);
    const float phi = -2 * M_PI_F + i * delta;
    EXPECT_NEAR(fast_sinf(phi), sinf(phi), 1.9e-7f);
  }

  /* Test exact PI/2 values. */
  EXPECT_NEAR(fast_sinf(M_PI_2_F), 1.0f, 1e-9f);
  EXPECT_NEAR(fast_sinf(-M_PI_2_F), -1.0f, 1e-9f);
  /* Test these close to PI/2 values. */
  EXPECT_NEAR(fast_sinf(1.57085085f), 0.9999999985f, 1e-9f);
  EXPECT_NEAR(fast_sinf(-1.57085085f), -0.9999999985f, 1e-9f);

  /* Test large values; fast_sinf expected to do good range reduction. */
  EXPECT_NEAR(fast_sinf(15378.3f), -0.2025494905f, 2e-4f);
  EXPECT_NEAR(fast_sinf(-78431.5f), 0.9976474762f, 2e-4f);
}

TEST(math, fast_cosf)
{
  /* Test values in -2PI .. 2PI range. */
  constexpr int N = 1000 + 1;
  for (int i = 0; i < N; ++i) {
    const float delta = 4 * M_PI_F / (N - 1);
    const float phi = -2 * M_PI_F + i * delta;
    EXPECT_NEAR(fast_cosf(phi), cosf(phi), 4.5e-7f);
  }

  /* Test exact PI/2 values. */
  EXPECT_NEAR(fast_cosf(M_PI_2_F), 0.0f, 3e-7f);
  EXPECT_NEAR(fast_cosf(-M_PI_2_F), 0.0f, 3e-7f);
  /* Test these close to PI/2 values. */
  EXPECT_NEAR(fast_cosf(1.57085085f), -0.00005452320508f, 3e-7f);
  EXPECT_NEAR(fast_cosf(-1.57085085f), -0.00005452320508f, 3e-7f);

  /* Test large values; fast_cosf expected to do good range reduction. */
  EXPECT_NEAR(fast_cosf(15378.3f), -0.9792720275f, 2e-4f);
  EXPECT_NEAR(fast_cosf(-78431.5f), 0.06855299586f, 2e-4f);
}

CCL_NAMESPACE_END
