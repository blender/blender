/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math_base_safe.h"

TEST(math_base, SafePowf)
{
  EXPECT_FLOAT_EQ(safe_powf(4.0f, 3.0f), 64.0f);
  EXPECT_FLOAT_EQ(safe_powf(3.2f, 5.6f), 674.2793796f);
  EXPECT_FLOAT_EQ(safe_powf(4.0f, -2.0f), 0.0625f);
  EXPECT_FLOAT_EQ(safe_powf(6.0f, -3.2f), 0.003235311f);
  EXPECT_FLOAT_EQ(safe_powf(-4.0f, 6), 4096.0f);
  EXPECT_FLOAT_EQ(safe_powf(-3.0f, 5.5), 0.0f);
  EXPECT_FLOAT_EQ(safe_powf(-2.5f, -4.0f), 0.0256f);
  EXPECT_FLOAT_EQ(safe_powf(-3.7f, -4.5f), 0.0f);
}

TEST(math_base, SafeModf)
{
  EXPECT_FLOAT_EQ(safe_modf(3.4, 2.2f), 1.2f);
  EXPECT_FLOAT_EQ(safe_modf(3.4, -2.2f), 1.2f);
  EXPECT_FLOAT_EQ(safe_modf(-3.4, -2.2f), -1.2f);
  EXPECT_FLOAT_EQ(safe_modf(-3.4, 0.0f), 0.0f);
  EXPECT_FLOAT_EQ(safe_modf(0.0f, 3.0f), 0.0f);
  EXPECT_FLOAT_EQ(safe_modf(55.0f, 10.0f), 5.0f);
}

TEST(math_base, SafeLogf)
{
  EXPECT_FLOAT_EQ(safe_logf(3.3f, 2.5f), 1.302995247f);
  EXPECT_FLOAT_EQ(safe_logf(0.0f, 3.0f), 0.0f);
  EXPECT_FLOAT_EQ(safe_logf(3.0f, 0.0f), 0.0f);
  EXPECT_FLOAT_EQ(safe_logf(-2.0f, 4.3f), 0.0f);
  EXPECT_FLOAT_EQ(safe_logf(2.0f, -4.3f), 0.0f);
  EXPECT_FLOAT_EQ(safe_logf(-2.0f, -4.3f), 0.0f);
}
