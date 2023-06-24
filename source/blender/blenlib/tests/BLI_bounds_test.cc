/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math_base.hh"

#include "BLI_array.hh"
#include "BLI_bounds.hh"

namespace blender::tests {

TEST(bounds, Empty)
{
  Span<float2> empty_span{};
  EXPECT_TRUE(empty_span.is_empty());
  auto result = bounds::min_max(empty_span);
  EXPECT_EQ(result, std::nullopt);
}

TEST(bounds, MinMax)
{
  Array<float2> data = {float2(0, 1), float2(3, -1), float2(0, -2), float2(-1, 1)};
  auto result = bounds::min_max(data.as_span());
  EXPECT_EQ(result->min, float2(-1, -2));
  EXPECT_EQ(result->max, float2(3, 1));
}

TEST(bounds, MinMaxFloat)
{
  Array<float> data = {1.0f, 3.0f, 0.0f, -1.0f};
  auto result = bounds::min_max(data.as_span());
  EXPECT_EQ(result->min, -1.0f);
  EXPECT_EQ(result->max, 3.0f);
}

TEST(bounds, MinGreaterThanZero)
{
  Array<float> data = {1.5f, 3.0f, 1.1f, 100.0f};
  auto result = bounds::min_max(data.as_span());
  EXPECT_GT(result->min, 1.0f);
}

TEST(bounds, MinMaxRadii)
{
  Array<int2> data = {int2(0, 1), int2(3, -1), int2(0, -2), int2(-1, 1)};
  Array<int> radii = {5, 1, 1, 4};
  auto result = bounds::min_max_with_radii(data.as_span(), radii.as_span());
  EXPECT_EQ(result->min, int2(-5, -4));
  EXPECT_EQ(result->max, int2(5, 6));
}

TEST(bounds, Large)
{
  Array<int2> data(10000);
  for (const int64_t i : data.index_range()) {
    data[i] = int2(i, i);
  }

  auto result = bounds::min_max(data.as_span());
  EXPECT_EQ(result->min, int2(0, 0));
  EXPECT_EQ(result->max, int2(9999, 9999));
}

}  // namespace blender::tests
