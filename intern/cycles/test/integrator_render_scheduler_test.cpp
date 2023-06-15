/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "integrator/render_scheduler.h"

CCL_NAMESPACE_BEGIN

TEST(IntegratorRenderScheduler, calculate_resolution_divider_for_resolution)
{
  EXPECT_EQ(calculate_resolution_divider_for_resolution(1920, 1080, 1920), 1);
  EXPECT_EQ(calculate_resolution_divider_for_resolution(1920, 1080, 960), 2);
  EXPECT_EQ(calculate_resolution_divider_for_resolution(1920, 1080, 480), 4);
}

TEST(IntegratorRenderScheduler, calculate_resolution_for_divider)
{
  EXPECT_EQ(calculate_resolution_for_divider(1920, 1080, 1), 1440);
  EXPECT_EQ(calculate_resolution_for_divider(1920, 1080, 2), 720);
  EXPECT_EQ(calculate_resolution_for_divider(1920, 1080, 4), 360);
}

CCL_NAMESPACE_END
