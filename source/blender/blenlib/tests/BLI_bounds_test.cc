/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math_base.hh"

#include "BLI_array.hh"
#include "BLI_bounds.hh"

namespace blender::tests {

TEST(bounds, Empty)
{
  Bounds<float2> bounds1(float2(0.0f));
  Bounds<float2> bounds2(float2(1.0f), float2(-1.0f));
  Bounds<float2> bounds3(float2(-1.0f), float2(1.0f));
  EXPECT_TRUE(bounds1.is_empty());
  EXPECT_TRUE(bounds2.is_empty());
  EXPECT_FALSE(bounds3.is_empty());
}

TEST(bounds, EmptyInt)
{
  Bounds<int> bounds1(0);
  Bounds<int> bounds2(1, -1);
  Bounds<int> bounds3(-1, 1);
  EXPECT_TRUE(bounds1.is_empty());
  EXPECT_TRUE(bounds2.is_empty());
  EXPECT_FALSE(bounds3.is_empty());
}

TEST(bounds, Center)
{
  Bounds<float2> bounds1(float2(0.0f));
  Bounds<float2> bounds2(float2(-1.0f));
  Bounds<float2> bounds3(float2(-1.0f), float2(1.0f));
  Bounds<float2> bounds4(float2(-3.0f, -5.0f), float2(2.0f, 4.0f));
  EXPECT_EQ(bounds1.center(), float2(0.0f));
  EXPECT_EQ(bounds2.center(), float2(-1.0f));
  EXPECT_EQ(bounds3.center(), float2(0.0f));
  EXPECT_EQ(bounds4.center(), float2(-0.5f, -0.5f));
}

TEST(bounds, Size)
{
  Bounds<float2> bounds1(float2(0.0f));
  Bounds<float2> bounds2(float2(-1.0f));
  Bounds<float2> bounds3(float2(-3.0f, -5.0f), float2(2.0f, 4.0f));
  EXPECT_EQ(bounds1.size(), float2(0.0f));
  EXPECT_EQ(bounds2.size(), float2(0.0f));
  EXPECT_EQ(bounds3.size(), float2(5.0f, 9.0f));
}

TEST(bounds, Translate)
{
  Bounds<float2> bounds1(float2(0.0f));
  Bounds<float2> bounds2(float2(-3.0f, -5.0f), float2(2.0f, 4.0f));
  bounds1.translate(float2(-1.0f));
  bounds2.translate(float2(2.0f));
  EXPECT_EQ(bounds1.min, float2(-1.0f));
  EXPECT_EQ(bounds1.max, float2(-1.0f));
  EXPECT_EQ(bounds2.min, float2(-1.0f, -3.0f));
  EXPECT_EQ(bounds2.max, float2(4.0f, 6.0f));
}

TEST(bounds, ScaleFromCenter)
{
  Bounds<float2> bounds1(float2(0.0f));
  Bounds<float2> bounds2(float2(-3.0f, -5.0f), float2(2.0f, 4.0f));
  bounds1.scale_from_center(float2(2.0f));
  const float2 bound2_size = bounds2.size();
  bounds2.scale_from_center(float2(2.0f, 1.0f));
  EXPECT_EQ(bounds1.min, float2(0.0f));
  EXPECT_EQ(bounds1.max, float2(0.0f));
  EXPECT_EQ(bounds2.min, float2(-5.5f, -5.0f));
  EXPECT_EQ(bounds2.max, float2(4.5f, 4.0f));
  EXPECT_EQ(bounds2.size(), bound2_size * float2(2.0f, 1.0f));
}

TEST(bounds, Resize)
{
  Bounds<float2> bounds1(float2(0.0f));
  Bounds<float2> bounds2(float2(-3.0f, -5.0f), float2(2.0f, 4.0f));
  bounds1.resize(float2(1.0f));
  bounds2.resize(float2(7.0f, 10.0f));
  EXPECT_EQ(bounds1.center(), float2(0.0f));
  EXPECT_EQ(bounds1.size(), float2(1.0f));
  EXPECT_EQ(bounds2.size(), float2(7.0f, 10.0f));
}

TEST(bounds, Recenter)
{
  Bounds<float2> bounds1(float2(0.0f));
  Bounds<float2> bounds2(float2(-3.0f, -5.0f), float2(2.0f, 4.0f));
  bounds1.recenter(float2(-1.0f));
  bounds2.recenter(float2(2.0f, 3.0f));
  EXPECT_EQ(bounds1.center(), float2(-1.0f));
  EXPECT_EQ(bounds2.center(), float2(2.0f, 3.0f));
}

TEST(bounds, Pad)
{
  Bounds<float2> bounds1(float2(0.0f));
  Bounds<float2> bounds2(float2(-1.0f), float2(1.0f));
  Bounds<float2> bounds3(float2(-3.0f, -5.0f), float2(2.0f, 4.0f));
  bounds1.pad(float2(1.0f));
  bounds2.pad(1.0f);
  bounds3.pad(float2(1.0f, 2.0f));
  EXPECT_EQ(bounds1.min, float2(-1.0f));
  EXPECT_EQ(bounds1.max, float2(1.0f));
  EXPECT_EQ(bounds2.min, float2(-2.0f));
  EXPECT_EQ(bounds2.max, float2(2.0f));
  EXPECT_EQ(bounds3.min, float2(-4.0f, -7.0f));
  EXPECT_EQ(bounds3.max, float2(3.0f, 6.0f));
}

TEST(bounds, MinMaxEmpty)
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

TEST(bounds, Contains)
{
  Bounds<int2> bounds1(int2(-3, -5), int2(2, 4));
  Array<int2> data1 = {int2(0, 1), int2(3, -1), int2(-3, -2), int2(-1, 1)};
  Array<bool> expected1 = {true, false, true, true};

  for (const int i : data1.index_range()) {
    EXPECT_EQ(bounds1.contains(data1[i]), expected1[i]);
  }

  Bounds<float2> bounds2(float2(-2.0f, -1.0f), float2(4.0f, 5.0f));
  Array<float2> data2 = {
      float2(-2.0f, -2.0f), float2(-3.0f, -1.0f), float2(4.0f, 6.0f), float2(5.0f, 5.0f)};
  Array<bool> expected2 = {false, false, false, false};
  for (const int i : data2.index_range()) {
    EXPECT_EQ(bounds2.contains(data2[i]), expected2[i]);
  }
}

TEST(bounds, IntersectSegment1D)
{
  Bounds<int> bounds1(-1, 6);
  EXPECT_TRUE(bounds1.intersects_segment(8, 2));
  EXPECT_FALSE(bounds1.intersects_segment(-2, -3));
  EXPECT_TRUE(bounds1.intersects_segment(8, 6));
  EXPECT_FALSE(bounds1.intersects_segment(8, 8));
  EXPECT_TRUE(bounds1.intersects_segment(0, 0));

  Bounds<float> bounds2(-1.0f, 6.0f);
  EXPECT_TRUE(bounds2.intersects_segment(8.0f, 2.0f));
  EXPECT_FALSE(bounds2.intersects_segment(-2.0f, -3.0f));
  EXPECT_TRUE(bounds2.intersects_segment(8.0f, 6.0f));
  EXPECT_FALSE(bounds2.intersects_segment(8.0f, 8.0f));
  EXPECT_TRUE(bounds2.intersects_segment(0.0f, 0.0f));
}

TEST(bounds, IntersectSegment2D)
{
  Bounds<int2> bounds1(int2(-2, -1), int2(4, 5));
  EXPECT_TRUE(bounds1.intersects_segment(int2(1, 2), int2(5, 3)));
  EXPECT_FALSE(bounds1.intersects_segment(int2(-4, 7), int2(5, 6)));
  EXPECT_TRUE(bounds1.intersects_segment(int2(-2, 2), int2(-4, 2)));
  EXPECT_FALSE(bounds1.intersects_segment(int2(5, 5), int2(5, 5)));
  EXPECT_TRUE(bounds1.intersects_segment(int2(1, 1), int2(1, 1)));
  EXPECT_FALSE(bounds1.intersects_segment(int2(0, -3), int2(-4, 0)));
  EXPECT_TRUE(bounds1.intersects_segment(int2(1, -2), int2(-3, 1)));

  Bounds<float2> bounds2(float2(-2.0f, -1.0f), float2(4.0f, 5.0f));
  EXPECT_TRUE(bounds2.intersects_segment(float2(1.0f, 2.0f), float2(5.0f, 3.0f)));
  EXPECT_FALSE(bounds2.intersects_segment(float2(-4.0f, 7.0f), float2(5.0f, 6.0f)));
  EXPECT_TRUE(bounds2.intersects_segment(float2(-2.0f, 2.0f), float2(-4.0f, 2.0f)));
  EXPECT_FALSE(bounds2.intersects_segment(float2(5.0f, 5.0f), float2(5.0f, 5.0f)));
  EXPECT_TRUE(bounds2.intersects_segment(float2(1.0f, 1.0f), float2(1.0f, 1.0f)));
  EXPECT_FALSE(bounds2.intersects_segment(float2(0.0f, -3.0f), float2(-4.0f, 0.0f)));
  EXPECT_TRUE(bounds2.intersects_segment(float2(1.0f, -2.0f), float2(-3.0f, 1.0f)));
}

}  // namespace blender::tests
