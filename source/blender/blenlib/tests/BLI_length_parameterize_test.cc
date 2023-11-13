/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_math_color.hh"
#include "BLI_math_vector.hh"

#include "BLI_array.hh"
#include "BLI_length_parameterize.hh"
#include "BLI_vector.hh"

#include "testing/testing.h"

namespace blender::length_parameterize::tests {

template<typename T> Array<float> calculate_lengths(const Span<T> values, const bool cyclic)
{
  Array<float> lengths(segments_num(values.size(), cyclic));
  accumulate_lengths<T>(values, cyclic, lengths);
  return lengths;
}

template<typename T> void test_uniform_lengths(const Span<T> values)
{
  const float segment_length = math::distance(values.first(), values.last()) / (values.size() - 1);
  for (const int i : values.index_range().drop_back(1)) {
    EXPECT_NEAR(math::distance(values[i], values[i + 1]), segment_length, 1e-5);
  }
}

TEST(length_parameterize, FloatSimple)
{
  Array<float> values{{0, 1, 4}};
  Array<float> lengths = calculate_lengths(values.as_span(), false);

  Array<int> indices(4);
  Array<float> factors(4);
  sample_uniform(lengths, true, indices, factors);
  Array<float> results(4);
  interpolate<float>(values, indices, factors, results);
  Array<float> expected({
      0.0f,
      1.33333f,
      2.66667f,
      4.0f,
  });
  for (const int i : results.index_range()) {
    EXPECT_NEAR(results[i], expected[i], 1e-5);
  }
  test_uniform_lengths(results.as_span());
}

TEST(length_parameterize, Float)
{
  Array<float> values{{1, 2, 3, 5, 10}};
  Array<float> lengths = calculate_lengths(values.as_span(), false);

  Array<int> indices(20);
  Array<float> factors(20);
  sample_uniform(lengths, true, indices, factors);
  Array<float> results(20);
  interpolate<float>(values, indices, factors, results);
  Array<float> expected({
      1.0f,     1.47368f, 1.94737f, 2.42105f, 2.89474f, 3.36842f, 3.84211f,
      4.31579f, 4.78947f, 5.26316f, 5.73684f, 6.21053f, 6.68421f, 7.1579f,
      7.63158f, 8.10526f, 8.57895f, 9.05263f, 9.52632f, 10.0f,
  });
  for (const int i : results.index_range()) {
    EXPECT_NEAR(results[i], expected[i], 1e-5);
  }
  test_uniform_lengths(results.as_span());
}

TEST(length_parameterize, Float2)
{
  Array<float2> values{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
  Array<float> lengths = calculate_lengths(values.as_span(), false);

  Array<int> indices(12);
  Array<float> factors(12);
  sample_uniform(lengths, true, indices, factors);
  Array<float2> results(12);
  interpolate<float2>(values, indices, factors, results);
  Array<float2> expected({
      {0.0f, 0.0f},
      {0.272727f, 0.0f},
      {0.545455f, 0.0f},
      {0.818182f, 0.0f},
      {1.0f, 0.0909091f},
      {1.0f, 0.363636f},
      {1.0f, 0.636364f},
      {1.0f, 0.909091f},
      {0.818182f, 1.0f},
      {0.545455f, 1.0f},
      {0.272727f, 1.0f},
      {0.0f, 1.0f},
  });
  for (const int i : results.index_range()) {
    EXPECT_NEAR(results[i].x, expected[i].x, 1e-5);
    EXPECT_NEAR(results[i].y, expected[i].y, 1e-5);
  }
}

TEST(length_parameterize, Float2Cyclic)
{
  Array<float2> values{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
  Array<float> lengths = calculate_lengths(values.as_span(), true);

  Array<int> indices(12);
  Array<float> factors(12);
  sample_uniform(lengths, false, indices, factors);
  Array<float2> results(12);
  interpolate<float2>(values, indices, factors, results);
  Array<float2> expected({
      {0.0f, 0.0f},
      {0.333333f, 0.0f},
      {0.666667f, 0.0f},
      {1.0f, 0.0f},
      {1.0f, 0.333333f},
      {1.0f, 0.666667f},
      {1.0f, 1.0f},
      {0.666667f, 1.0f},
      {0.333333f, 1.0f},
      {0.0f, 1.0f},
      {0.0f, 0.666667f},
      {0.0f, 0.333333f},
  });
  for (const int i : results.index_range()) {
    EXPECT_NEAR(results[i].x, expected[i].x, 1e-5);
    EXPECT_NEAR(results[i].y, expected[i].y, 1e-5);
  }
}

TEST(length_parameterize, LineMany)
{
  Array<float> values{{1, 2}};
  Array<float> lengths = calculate_lengths(values.as_span(), false);

  Array<int> indices(5007);
  Array<float> factors(5007);
  sample_uniform(lengths, true, indices, factors);
  Array<float> results(5007);
  interpolate<float>(values, indices, factors, results);
  Array<float> expected({
      1.9962f, 1.9964f, 1.9966f, 1.9968f, 1.997f, 1.9972f, 1.9974f, 1.9976f, 1.9978f, 1.998f,
      1.9982f, 1.9984f, 1.9986f, 1.9988f, 1.999f, 1.9992f, 1.9994f, 1.9996f, 1.9998f, 2.0f,
  });
  for (const int i : expected.index_range()) {
    EXPECT_NEAR(results.as_span().take_back(20)[i], expected[i], 1e-5);
  }
}

TEST(length_parameterize, CyclicMany)
{
  Array<float2> values{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
  Array<float> lengths = calculate_lengths(values.as_span(), true);

  Array<int> indices(5007);
  Array<float> factors(5007);
  sample_uniform(lengths, false, indices, factors);
  Array<float2> results(5007);
  interpolate<float2>(values, indices, factors, results);
  Array<float2> expected({
      {0, 0.0159776},  {0, 0.0151787},  {0, 0.0143797},  {0, 0.013581},   {0, 0.0127821},
      {0, 0.0119832},  {0, 0.0111842},  {0, 0.0103855},  {0, 0.00958657}, {0, 0.00878763},
      {0, 0.00798869}, {0, 0.00718999}, {0, 0.00639105}, {0, 0.00559211}, {0, 0.00479317},
      {0, 0.00399446}, {0, 0.00319552}, {0, 0.00239658}, {0, 0.00159764}, {0, 0.000798941},
  });
  for (const int i : expected.index_range()) {
    EXPECT_NEAR(results.as_span().take_back(20)[i].x, expected[i].x, 1e-5);
    EXPECT_NEAR(results.as_span().take_back(20)[i].y, expected[i].y, 1e-5);
  }
}

TEST(length_parameterize, InterpolateColor)
{
  Array<float2> values{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
  Array<float> lengths = calculate_lengths(values.as_span(), true);

  Array<ColorGeometry4f> colors{{{0, 0, 0, 1}, {1, 0, 0, 1}, {1, 1, 0, 1}, {0, 1, 0, 1}}};

  Array<int> indices(10);
  Array<float> factors(10);
  sample_uniform(lengths, false, indices, factors);
  Array<ColorGeometry4f> results(10);
  interpolate<ColorGeometry4f>(colors, indices, factors, results);
  Array<ColorGeometry4f> expected({
      {0, 0, 0, 1},
      {0.4, 0, 0, 1},
      {0.8, 0, 0, 1},
      {1, 0.2, 0, 1},
      {1, 0.6, 0, 1},
      {1, 1, 0, 1},
      {0.6, 1, 0, 1},
      {0.2, 1, 0, 1},
      {0, 0.8, 0, 1},
      {0, 0.4, 0, 1},
  });
  for (const int i : results.index_range()) {
    EXPECT_NEAR(results[i].r, expected[i].r, 1e-6);
    EXPECT_NEAR(results[i].g, expected[i].g, 1e-6);
    EXPECT_NEAR(results[i].b, expected[i].b, 1e-6);
    EXPECT_NEAR(results[i].a, expected[i].a, 1e-6);
  }
}

TEST(length_parameterize, ArbitraryFloatSimple)
{
  Array<float> values{{0, 1, 4}};
  Array<float> lengths = calculate_lengths(values.as_span(), false);

  Array<float> sample_lengths{{0.5f, 1.5f, 2.0f, 4.0f}};
  Array<int> indices(4);
  Array<float> factors(4);
  sample_at_lengths(lengths, sample_lengths, indices, factors);
  Array<float> results(4);
  interpolate<float>(values, indices, factors, results);
  Array<float> expected({
      0.5f,
      1.5f,
      2.0f,
      4.0f,
  });
  for (const int i : results.index_range()) {
    EXPECT_NEAR(results[i], expected[i], 1e-5);
  }
}

TEST(length_parameterize, ArbitraryFloat2)
{
  Array<float2> values{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
  Array<float> lengths = calculate_lengths(values.as_span(), true);

  Array<float> sample_lengths{
      {0.5f, 1.5f, 2.0f, 2.0f, 2.1f, 2.5f, 3.5f, 3.6f, 3.8f, 3.85f, 3.90f, 4.0f}};
  Array<int> indices(12);
  Array<float> factors(12);
  sample_at_lengths(lengths, sample_lengths, indices, factors);
  Array<float2> results(12);
  interpolate<float2>(values, indices, factors, results);
  Array<float2> expected({
      {0.5f, 0.0f},
      {1.0f, 0.5f},
      {1.0f, 1.0f},
      {1.0f, 1.0f},
      {0.9f, 1.0f},
      {0.5f, 1.0f},
      {0.0f, 0.5f},
      {0.0f, 0.4f},
      {0.0f, 0.2f},
      {0.0f, 0.15f},
      {0.0f, 0.1f},
      {0.0f, 0.0f},
  });
  for (const int i : results.index_range()) {
    EXPECT_NEAR(results[i].x, expected[i].x, 1e-5);
    EXPECT_NEAR(results[i].y, expected[i].y, 1e-5);
  }
}

}  // namespace blender::length_parameterize::tests
