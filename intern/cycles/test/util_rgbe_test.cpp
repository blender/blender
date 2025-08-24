/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <gtest/gtest.h>

#include "util/log.h"
#include "util/types_rgbe.h"

CCL_NAMESPACE_BEGIN

TEST(RGBE, round_trip)
{
  {
    const float3 f = make_float3(7.334898f, 5.811583f, 2.414717f);
    EXPECT_EQ(rgbe_to_rgb(rgb_to_rgbe(f)), make_float3(7.34375f, 5.8125f, 2.40625f));
  }

  {
    const float3 f = make_float3(0.08750992f, 0.05150064f, 0.24991725f);
    EXPECT_EQ(rgbe_to_rgb(rgb_to_rgbe(f)), make_float3(0.087890625f, 0.05078125f, 0.25f));
  }

  {
    const float3 f = make_float3(4e-6f, 30257.0f, 1.0f);
    EXPECT_EQ(rgbe_to_rgb(rgb_to_rgbe(f)), make_float3(0.0f, 30208.0f, 0.0f));
  }

  {
    const float3 f = zero_float3();
    EXPECT_EQ(rgbe_to_rgb(rgb_to_rgbe(f)), zero_float3());
  }

  {
    const float3 f = make_float3(5.9e-8f, 0.0f, 0.0f);
    EXPECT_EQ(rgbe_to_rgb(rgb_to_rgbe(f)), zero_float3());
  }

  {
    const float3 f = make_float3(6.0e-8f, 0.0f, 0.0f);
    EXPECT_EQ(rgbe_to_rgb(rgb_to_rgbe(f)), make_float3(1.1920928955078125e-7f, 0.0f, 0.0f));
  }

  {
    const float3 f = make_float3(-0.863880f, 0.558654f, -0.223357f);
    EXPECT_EQ(rgbe_to_rgb(rgb_to_rgbe(f)), make_float3(-0.86328125f, 0.55859375f, -0.22265625f));
  }

  {
    const float3 f = make_float3(-FLT_MAX, FLT_MAX, 0.0f);
    EXPECT_EQ(rgbe_to_rgb(rgb_to_rgbe(f)), make_float3(-65280.0f, 65280.0f, 0.0f));
  }

  {
    const float inf = __uint_as_float(0x7f800000);
    const float3 f = make_float3(inf, 127.0f, 129.0f);
    EXPECT_EQ(rgbe_to_rgb(rgb_to_rgbe(f)), make_float3(65280.0f, 0.0f, 256.0f));
  }

  {
    /* No test for NaN, undefined behavior. */
  }
}

CCL_NAMESPACE_END
