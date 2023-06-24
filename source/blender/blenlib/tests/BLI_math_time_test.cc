/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math.h"

TEST(math_time, SecondsExplode)
{
  const double seconds = 2.0 * SECONDS_IN_DAY + 13.0 * SECONDS_IN_HOUR + 33.0 * SECONDS_IN_MINUTE +
                         9.0 + 369.0 * SECONDS_IN_MILLISECONDS;
  const double epsilon = 1e-8;

  double r_days, r_hours, r_minutes, r_seconds, r_milliseconds;

  BLI_math_time_seconds_decompose(
      seconds, &r_days, &r_hours, &r_minutes, &r_seconds, &r_milliseconds);
  EXPECT_NEAR(2.0, r_days, epsilon);
  EXPECT_NEAR(13.0, r_hours, epsilon);
  EXPECT_NEAR(33.0, r_minutes, epsilon);
  EXPECT_NEAR(9.0, r_seconds, epsilon);
  EXPECT_NEAR(369.0, r_milliseconds, epsilon);

  BLI_math_time_seconds_decompose(seconds, nullptr, &r_hours, &r_minutes, &r_seconds, nullptr);
  EXPECT_NEAR(61.0, r_hours, epsilon);
  EXPECT_NEAR(33.0, r_minutes, epsilon);
  EXPECT_NEAR(9.369, r_seconds, epsilon);

  BLI_math_time_seconds_decompose(seconds, nullptr, nullptr, nullptr, &r_seconds, nullptr);
  EXPECT_NEAR(seconds, r_seconds, epsilon);

  BLI_math_time_seconds_decompose(seconds, &r_days, nullptr, &r_minutes, nullptr, &r_milliseconds);
  EXPECT_NEAR(2.0, r_days, epsilon);
  EXPECT_NEAR(813.0, r_minutes, epsilon);
  EXPECT_NEAR(9369.0, r_milliseconds, epsilon);
}
