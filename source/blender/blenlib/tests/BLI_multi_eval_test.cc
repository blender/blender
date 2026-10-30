/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_multi_eval.hh"
#include "BLI_span.hh"
#include "BLI_structure_of_array_fixed.hh"

#include "testing/testing.h"

#include "BLI_strict_flags.hh" /* IWYU pragma: keep. Keep last. */

namespace blender::multi_eval::tests {

static_assert(
    std::is_same_v<
        decltype(detail::param_tags_from_str_and_args<"ioi", SoaPtr<float3>, int *, char *>()),
        TypeSequence<ParamTag<ParamCategory::Input, float3, true>,
                     ParamTag<ParamCategory::Output, int>,
                     ParamTag<ParamCategory::Input, char>>>);
static_assert(std::is_same_v<
              decltype(detail::param_tags_from_str_and_args<"io#i", float3 *, int *, char *>()),
              TypeSequence<ParamTag<ParamCategory::Input, float3>,
                           ParamTag<ParamCategory::Output, int, true>,
                           ParamTag<ParamCategory::Input, char>>>);

static_assert(detail::parse_param_str<"", 0>().ok);
static_assert(detail::parse_param_str<"i", 1>().ok);
static_assert(detail::parse_param_str<"imo", 3>().ok);
static_assert(detail::parse_param_str<"i#o#m#", 3>().ok);
static_assert(detail::parse_param_str<"io#", 2>().ok);

static_assert(!detail::parse_param_str<"", 1>().ok);
static_assert(!detail::parse_param_str<"i", 0>().ok);
static_assert(!detail::parse_param_str<"i", 2>().ok);
static_assert(!detail::parse_param_str<"ioi", 2>().ok);
static_assert(!detail::parse_param_str<"iox", 3>().ok);
static_assert(!detail::parse_param_str<"i##o", 2>().ok);
static_assert(!detail::parse_param_str<"#i", 1>().ok);
static_assert(!detail::parse_param_str<"i#o#x", 2>().ok);

TEST(MultiEval, InputOutputSpans)
{
  const Array<int> in{1, 2, 3, 4};
  Array<int> out(4);

  evaluate_element_wise<"io">([](const int a, int &r_out) { r_out = a * 10; }, in.size(), in, out);

  EXPECT_EQ(out[0], 10);
  EXPECT_EQ(out[1], 20);
  EXPECT_EQ(out[2], 30);
  EXPECT_EQ(out[3], 40);
}

TEST(MultiEval, InputOutputPointers)
{
  const int in[] = {3, 6, 9};
  int out[3] = {};

  evaluate_element_wise<"io">([](const int a, int &r_out) { r_out = a + 1; },
                              3,
                              static_cast<const int *>(in),
                              static_cast<int *>(out));

  EXPECT_EQ(out[0], 4);
  EXPECT_EQ(out[1], 7);
  EXPECT_EQ(out[2], 10);
}

TEST(MultiEval, Mutable)
{
  const Array<int> add{5, 5, 5, 5};
  Array<int> values{1, 2, 3, 4};

  evaluate_element_wise<"im">(
      [](const int a, int &value) { value += a; }, values.size(), add, values);

  EXPECT_EQ(values[0], 6);
  EXPECT_EQ(values[1], 7);
  EXPECT_EQ(values[2], 8);
  EXPECT_EQ(values[3], 9);
}

TEST(MultiEval, TwoInputsOneOutput)
{
  const Array<int> in1{1, 2, 3};
  const Array<int> in2{10, 20, 30};
  Array<int> out(3);

  evaluate_element_wise<"iio">(
      [](const int a, const int b, int &r_out) { r_out = a + b; }, out.size(), in1, in2, out);

  EXPECT_EQ(out[0], 11);
  EXPECT_EQ(out[1], 22);
  EXPECT_EQ(out[2], 33);
}

TEST(MultiEval, SoaBuffers)
{
  FixedSoaArray<float3, 4> in;
  FixedSoaArray<float3, 4> out;
  in.set(0, float3(1.0f, 0.0f, 0.0f));
  in.set(1, float3(0.0f, 2.0f, 0.0f));
  in.set(2, float3(0.0f, 0.0f, 3.0f));
  in.set(3, float3(1.0f, 1.0f, 1.0f));

  evaluate_element_wise<"io">(
      [](const float3 &a, float3 &r_out) { r_out = a * 2.0f; }, 4, in, out);

  EXPECT_EQ(out[0], float3(2.0f, 0.0f, 0.0f));
  EXPECT_EQ(out[1], float3(0.0f, 4.0f, 0.0f));
  EXPECT_EQ(out[2], float3(0.0f, 0.0f, 6.0f));
  EXPECT_EQ(out[3], float3(2.0f, 2.0f, 2.0f));
}

TEST(MultiEval, Empty)
{
  Array<int> out(0);
  evaluate_element_wise<"io">([](const int a, int &r_out) { r_out = a; }, 0, Span<int>(), out);
}

TEST(MultiEval, UnrollAttribute)
{
  const Array<int> in{1, 2, 3, 4, 5, 6, 7, 8};
  Array<int> out(8);

  evaluate_element_wise<"io", MultiEvalCompilerAttributes{.unroll = 4}>(
      [](const int a, int &r_out) { r_out = a * a; }, in.size(), in, out);

  EXPECT_EQ(out[0], 1);
  EXPECT_EQ(out[1], 4);
  EXPECT_EQ(out[2], 9);
  EXPECT_EQ(out[3], 16);
  EXPECT_EQ(out[4], 25);
  EXPECT_EQ(out[5], 36);
  EXPECT_EQ(out[6], 49);
  EXPECT_EQ(out[7], 64);
}

}  // namespace blender::multi_eval::tests
