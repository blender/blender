/*
 * Copyright 2011-2021 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
