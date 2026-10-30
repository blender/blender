/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_array.hh"
#include "BLI_generic_span.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_multi_eval_varray.hh"
#include "BLI_virtual_array.hh"

#include "testing/testing.h"

#include "BLI_strict_flags.hh" /* IWYU pragma: keep. Keep last. */

namespace blender::multi_eval::tests {

class MultiEvalVArrayTest : public testing::Test {
 public:
  static void SetUpTestSuite()
  {
    register_cpp_types();
  }
};

TEST_F(MultiEvalVArrayTest, ExecuteSimple)
{
  VArray<int> in1 = VArray<int>::from_container(std::array{3, 6, 4, 8});
  VArray<int> in2 = VArray<int>::from_single(10, in1.size());
  std::array<int, 4> out;
  evaluate_element_wise_varray<"iio">([](const int a, const int b, int &r_out) { r_out = a + b; },
                                      exec_presets::Materialized(),
                                      IndexMask(4),
                                      in1,
                                      in2,
                                      MutableSpan(out));

  EXPECT_EQ(out[0], 13);
  EXPECT_EQ(out[1], 16);
  EXPECT_EQ(out[2], 14);
  EXPECT_EQ(out[3], 18);
}

TEST_F(MultiEvalVArrayTest, EvaluateChunkedVArray)
{
  VArray<int> in1 = VArray<int>::from_container(Array<int>{3, 6, 4, 8});
  VArray<int> in2 = VArray<int>::from_single(10, 4);
  Array<int> out(4);

  evaluate_chunked_varray<"iio", 64>(
      [](const int64_t size, const int *a, const int *b, int *r_out) {
        for (const int64_t i : IndexRange(size)) {
          r_out[i] = a[i] + b[i];
        }
      },
      IndexMask(4),
      in1,
      in2,
      out);

  EXPECT_EQ(out[0], 13);
  EXPECT_EQ(out[1], 16);
  EXPECT_EQ(out[2], 14);
  EXPECT_EQ(out[3], 18);
}

TEST_F(MultiEvalVArrayTest, EvaluateChunkedVArraySparseMask)
{
  VArray<int> in1 = VArray<int>::from_container(Array<int>{0, 1, 2, 3, 4, 5, 6, 7});
  VArray<int> in2 = VArray<int>::from_single(100, 8);
  Array<int> out(8, -1);

  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_indices<int>({1, 3, 6}, memory);
  evaluate_chunked_varray<"iio", 64>(
      [](const int64_t size, const int *a, const int *b, int *r_out) {
        for (const int64_t i : IndexRange(size)) {
          r_out[i] = a[i] + b[i];
        }
      },
      mask,
      in1,
      in2,
      out);

  EXPECT_EQ(out[0], -1);
  EXPECT_EQ(out[1], 101);
  EXPECT_EQ(out[2], -1);
  EXPECT_EQ(out[3], 103);
  EXPECT_EQ(out[4], -1);
  EXPECT_EQ(out[5], -1);
  EXPECT_EQ(out[6], 106);
  EXPECT_EQ(out[7], -1);
}

TEST_F(MultiEvalVArrayTest, EvaluateChunkedVArrayInt3)
{
  VArray<int3> in1 = VArray<int3>::from_container(Array<int3>{int3(1, 2, 3), int3(4, 5, 6)});
  VArray<int3> in2 = VArray<int3>::from_single(int3(2, 3, 4), 2);
  Array<int3> out(2);

  evaluate_chunked_varray<"i#io#", 64>(
      [](const int64_t size, const SoaPtr<int3> a, const int3 *b, MutableSoaPtr<int3> r_out) {
        for (const int64_t i : IndexRange(size)) {
          r_out.set(i, a[i] * b[i]);
        }
      },
      IndexMask(2),
      in1,
      in2,
      out);

  EXPECT_EQ(out[0], int3(2, 6, 12));
  EXPECT_EQ(out[1], int3(8, 15, 24));
}

TEST_F(MultiEvalVArrayTest, Execute1in1out)
{
  VArray<float> in = VArray<float>::from_container(Array<float>{1.0f, 2.5f, -3.0f});
  Array<float> out(3);
  evaluate_element_wise_varray<"io">([](const float a, float &r_out) { r_out = a * 2.0f; },
                                     exec_presets::Materialized(),
                                     IndexMask(3),
                                     in,
                                     out);

  EXPECT_EQ(out[0], 2.0f);
  EXPECT_EQ(out[1], 5.0f);
  EXPECT_EQ(out[2], -6.0f);
}

TEST_F(MultiEvalVArrayTest, Execute3in1out)
{
  VArray<int> in1 = VArray<int>::from_container(Array<int>{1, 2, 3, 4});
  VArray<int> in2 = VArray<int>::from_single(10, 4);
  VArray<int> in3 = VArray<int>::from_func(4, [](const int64_t i) { return int(i); });
  Array<int> out(4);
  evaluate_element_wise_varray<"iiio">(
      [](const int a, const int b, const int c, int &r_out) { r_out = a + b + c; },
      exec_presets::Materialized(),
      IndexMask(4),
      in1,
      in2,
      in3,
      out);

  EXPECT_EQ(out[0], 11);
  EXPECT_EQ(out[1], 13);
  EXPECT_EQ(out[2], 15);
  EXPECT_EQ(out[3], 17);
}

TEST_F(MultiEvalVArrayTest, Execute5in1out)
{
  VArray<int> in1 = VArray<int>::from_single(1, 3);
  VArray<int> in2 = VArray<int>::from_single(2, 3);
  VArray<int> in3 = VArray<int>::from_single(3, 3);
  VArray<int> in4 = VArray<int>::from_single(4, 3);
  VArray<int> in5 = VArray<int>::from_container(Array<int>{10, 20, 30});
  Array<int> out(3);
  evaluate_element_wise_varray<"iiiiio">(
      [](const int a, const int b, const int c, const int d, const int e, int &r_out) {
        r_out = a + b + c + d + e;
      },
      exec_presets::Materialized(),
      IndexMask(3),
      in1,
      in2,
      in3,
      in4,
      in5,
      out);

  EXPECT_EQ(out[0], 20);
  EXPECT_EQ(out[1], 30);
  EXPECT_EQ(out[2], 40);
}

TEST_F(MultiEvalVArrayTest, ExecuteFloat3)
{
  VArray<float3> in1 = VArray<float3>::from_container(
      Array<float3>{float3(1.0f, 0.0f, 0.0f), float3(0.0f, 2.0f, 0.0f)});
  VArray<float3> in2 = VArray<float3>::from_single(float3(0.0f, 0.0f, 3.0f), 2);
  Array<float3> out(2);
  evaluate_element_wise_varray<"iio">(
      [](const float3 &a, const float3 &b, float3 &r_out) { r_out = a + b; },
      exec_presets::Materialized(),
      IndexMask(2),
      in1,
      in2,
      out);

  EXPECT_EQ(out[0], float3(1.0f, 0.0f, 3.0f));
  EXPECT_EQ(out[1], float3(0.0f, 2.0f, 3.0f));
}

TEST_F(MultiEvalVArrayTest, ExecuteBool)
{
  VArray<bool> in1 = VArray<bool>::from_container(Array<bool>{true, false, true, false});
  VArray<bool> in2 = VArray<bool>::from_single(true, 4);
  Array<bool> out(4);
  evaluate_element_wise_varray<"iio">(
      [](const bool a, const bool b, bool &r_out) { r_out = a && b; },
      exec_presets::Materialized(),
      IndexMask(4),
      in1,
      in2,
      out);

  EXPECT_TRUE(out[0]);
  EXPECT_FALSE(out[1]);
  EXPECT_TRUE(out[2]);
  EXPECT_FALSE(out[3]);
}

TEST_F(MultiEvalVArrayTest, ExecuteMixedInputTypes)
{
  VArray<int> in_int = VArray<int>::from_container(Array<int>{1, 2, 3});
  VArray<float> in_float = VArray<float>::from_single(0.5f, 3);
  Array<float> out(3);
  evaluate_element_wise_varray<"iio">(
      [](const int a, const float b, float &r_out) { r_out = float(a) + b; },
      exec_presets::Materialized(),
      IndexMask(3),
      in_int,
      in_float,
      out);

  EXPECT_EQ(out[0], 1.5f);
  EXPECT_EQ(out[1], 2.5f);
  EXPECT_EQ(out[2], 3.5f);
}

template<typename ExecPreset> static void test_add_with_preset(const ExecPreset &preset)
{
  VArray<int> in1 = VArray<int>::from_container(Array<int>{1, 2, 3, 4});
  VArray<int> in2 = VArray<int>::from_func(4, [](const int64_t i) { return int(i * 10); });
  Array<int> out(4);
  evaluate_element_wise_varray<"iio">([](const int a, const int b, int &r_out) { r_out = a + b; },
                                      preset,
                                      IndexMask(4),
                                      in1,
                                      in2,
                                      out);

  EXPECT_EQ(out[0], 1);
  EXPECT_EQ(out[1], 12);
  EXPECT_EQ(out[2], 23);
  EXPECT_EQ(out[3], 34);
}

TEST_F(MultiEvalVArrayTest, ExecutePresetSimple)
{
  test_add_with_preset(exec_presets::Simple());
}

TEST_F(MultiEvalVArrayTest, ExecutePresetMaterialized)
{
  test_add_with_preset(exec_presets::Materialized());
}

TEST_F(MultiEvalVArrayTest, ExecutePresetAllSpanOrSingle)
{
  test_add_with_preset(exec_presets::AllSpanOrSingle());
}

TEST_F(MultiEvalVArrayTest, ExecutePresetSomeSpanOrSingle)
{
  test_add_with_preset(exec_presets::SomeSpanOrSingle<0>());
}

TEST_F(MultiEvalVArrayTest, ExecuteMutable)
{
  VArray<int> add = VArray<int>::from_single(5, 4);
  Array<int> values{1, 2, 3, 4};

  evaluate_element_wise_varray<"im">([](const int a, int &value) { value += a; },
                                     exec_presets::Materialized(),
                                     IndexMask(4),
                                     add,
                                     values);

  EXPECT_EQ(values[0], 6);
  EXPECT_EQ(values[1], 7);
  EXPECT_EQ(values[2], 8);
  EXPECT_EQ(values[3], 9);
}

TEST_F(MultiEvalVArrayTest, ExecuteMutableAndOutput)
{
  VArray<int> scale = VArray<int>::from_container(Array<int>{2, 3, 4});
  Array<int> values{10, 20, 30};
  Array<int> doubled(3);

  evaluate_element_wise_varray<"imo">(
      [](const int s, int &value, int &out) {
        value *= s;
        new (&out) int(value);
      },
      exec_presets::AllSpanOrSingle(),
      IndexMask(3),
      scale,
      values,
      doubled);

  EXPECT_EQ(values[0], 20);
  EXPECT_EQ(values[1], 60);
  EXPECT_EQ(values[2], 120);
  EXPECT_EQ(doubled[0], 20);
  EXPECT_EQ(doubled[1], 60);
  EXPECT_EQ(doubled[2], 120);
}

TEST_F(MultiEvalVArrayTest, ExecuteTwoOutputs)
{
  VArray<int> in = VArray<int>::from_container(Array<int>{1, 2, 3, 4});
  Array<int> out_double(4);
  Array<int> out_square(4);

  evaluate_element_wise_varray<"ioo">(
      [](const int a, int &doubled, int &squared) {
        new (&doubled) int(a * 2);
        new (&squared) int(a * a);
      },
      exec_presets::Materialized(),
      IndexMask(4),
      in,
      out_double,
      out_square);

  EXPECT_EQ(out_double[0], 2);
  EXPECT_EQ(out_double[1], 4);
  EXPECT_EQ(out_double[2], 6);
  EXPECT_EQ(out_double[3], 8);
  EXPECT_EQ(out_square[0], 1);
  EXPECT_EQ(out_square[1], 4);
  EXPECT_EQ(out_square[2], 9);
  EXPECT_EQ(out_square[3], 16);
}

TEST_F(MultiEvalVArrayTest, ExecuteSparseMask)
{
  VArray<int> in1 = VArray<int>::from_container(Array<int>{0, 1, 2, 3, 4, 5, 6, 7});
  VArray<int> in2 = VArray<int>::from_single(100, 8);
  Array<int> out(8, -1);

  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_indices<int>({1, 3, 6}, memory);
  evaluate_element_wise_varray<"iio">([](const int a, const int b, int &r_out) { r_out = a + b; },
                                      exec_presets::Materialized(),
                                      mask,
                                      in1,
                                      in2,
                                      out);

  EXPECT_EQ(out[0], -1);
  EXPECT_EQ(out[1], 101);
  EXPECT_EQ(out[2], -1);
  EXPECT_EQ(out[3], 103);
  EXPECT_EQ(out[4], -1);
  EXPECT_EQ(out[5], -1);
  EXPECT_EQ(out[6], 106);
  EXPECT_EQ(out[7], -1);
}

TEST_F(MultiEvalVArrayTest, ExecuteSparseMaskMutable)
{
  Array<int> values{0, 1, 2, 3, 4, 5};
  VArray<int> add = VArray<int>::from_single(10, 6);

  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_indices<int>({0, 2, 5}, memory);
  evaluate_element_wise_varray<"im">([](const int a, int &value) { value += a; },
                                     exec_presets::Materialized(),
                                     mask,
                                     add,
                                     values);

  EXPECT_EQ(values[0], 10);
  EXPECT_EQ(values[1], 1);
  EXPECT_EQ(values[2], 12);
  EXPECT_EQ(values[3], 3);
  EXPECT_EQ(values[4], 4);
  EXPECT_EQ(values[5], 15);
}

TEST_F(MultiEvalVArrayTest, ExecuteGenericOutput)
{
  VArray<int> in = VArray<int>::from_container(Array<int>{2, 4, 6});
  Array<int> out(3);
  GMutableSpan generic_out(out.as_mutable_span());

  evaluate_element_wise_varray([](const int a, int &out_value) { new (&out_value) int(a / 2); },
                               exec_presets::Simple(),
                               IndexMask(3),
                               TypeSequence<InputParamTag<int>, OutputParamTag<int>>(),
                               in,
                               generic_out);

  EXPECT_EQ(out[0], 1);
  EXPECT_EQ(out[1], 2);
  EXPECT_EQ(out[2], 3);
}

TEST_F(MultiEvalVArrayTest, ExecuteEmptyMask)
{
  VArray<int> in = VArray<int>::from_container(Array<int>{1, 2, 3});
  Array<int> out(3, -1);

  evaluate_element_wise_varray<"io">([](const int a, int &r_out) { r_out = a * 2; },
                                     exec_presets::Materialized(),
                                     IndexMask(0),
                                     in,
                                     out);

  EXPECT_EQ(out[0], -1);
  EXPECT_EQ(out[1], -1);
  EXPECT_EQ(out[2], -1);
}

TEST_F(MultiEvalVArrayTest, ExecuteLargeForChunking)
{
  constexpr int64_t size = 200;
  Array<int> in_data(size);
  for (const int64_t i : IndexRange(size)) {
    in_data[i] = int(i);
  }
  VArray<int> in = VArray<int>::from_span(in_data.as_span());
  VArray<int> add = VArray<int>::from_func(size, [](const int64_t i) { return int(i % 7); });
  Array<int> out(size);

  evaluate_element_wise_varray<"iio">([](const int a, const int b, int &r_out) { r_out = a + b; },
                                      exec_presets::Materialized(),
                                      IndexMask(size),
                                      in,
                                      add,
                                      out);

  for (const int64_t i : IndexRange(size)) {
    EXPECT_EQ(out[i], int(i) + int(i % 7));
  }
}

TEST_F(MultiEvalVArrayTest, ExecuteAllSingleInputs)
{
  VArray<int> in1 = VArray<int>::from_single(3, 5);
  VArray<int> in2 = VArray<int>::from_single(7, 5);
  Array<int> out(5);
  evaluate_element_wise_varray<"iio">([](const int a, const int b, int &r_out) { r_out = a * b; },
                                      exec_presets::AllSpanOrSingle(),
                                      IndexMask(5),
                                      in1,
                                      in2,
                                      out);

  for (const int value : out) {
    EXPECT_EQ(value, 21);
  }
}

TEST_F(MultiEvalVArrayTest, ExecuteGVArrayInput)
{
  const GVArray in = VArray<int>::from_container(Array<int>{1, 2, 3, 4});
  Array<int> out(4);

  evaluate_element_wise_varray([](const int a, int &out_value) { new (&out_value) int(a * 10); },
                               exec_presets::Materialized(),
                               IndexMask(4),
                               TypeSequence<InputParamTag<int>, OutputParamTag<int>>(),
                               in,
                               out);

  EXPECT_EQ(out[0], 10);
  EXPECT_EQ(out[1], 20);
  EXPECT_EQ(out[2], 30);
  EXPECT_EQ(out[3], 40);
}

TEST_F(MultiEvalVArrayTest, ExecuteGVArrayAndVArrayInputs)
{
  const GVArray in1 = VArray<int>::from_container(Array<int>{1, 2, 3});
  const VArray<int> in2 = VArray<int>::from_single(100, 3);
  Array<int> out(3);

  evaluate_element_wise_varray(
      [](const int a, const int b, int &out_value) { new (&out_value) int(a + b); },
      exec_presets::AllSpanOrSingle(),
      IndexMask(3),
      TypeSequence<InputParamTag<int>, InputParamTag<int>, OutputParamTag<int>>(),
      in1,
      in2,
      out);

  EXPECT_EQ(out[0], 101);
  EXPECT_EQ(out[1], 102);
  EXPECT_EQ(out[2], 103);
}

TEST_F(MultiEvalVArrayTest, ExecuteGVArraySingle)
{
  const int single_value = 7;
  const GVArray in = GVArray::from_single(CPPType::get<int>(), 4, &single_value);
  Array<int> out(4);

  evaluate_element_wise_varray([](const int a, int &out_value) { new (&out_value) int(a + 1); },
                               exec_presets::Simple(),
                               IndexMask(4),
                               TypeSequence<InputParamTag<int>, OutputParamTag<int>>(),
                               in,
                               out);

  for (const int value : out) {
    EXPECT_EQ(value, 8);
  }
}

TEST_F(MultiEvalVArrayTest, ExecuteMutableVArrayInputs)
{
  Array<int> data1 = {1, 2, 3, 4};
  Array<int> data2 = {10, 20, 30, 40};
  const GVMutableArray in1 = GVMutableArray::from_span(GMutableSpan(data1.as_mutable_span()));
  const VMutableArray<int> in2 = VMutableArray<int>::from_span(data2);
  Array<int> out(4);

  evaluate_element_wise_varray(
      [](const int a, const int b, int &out_value) { new (&out_value) int(a + b); },
      exec_presets::Materialized(),
      IndexMask(4),
      TypeSequence<InputParamTag<int>, InputParamTag<int>, OutputParamTag<int>>(),
      in1,
      in2,
      out);

  EXPECT_EQ(out[0], 11);
  EXPECT_EQ(out[1], 22);
  EXPECT_EQ(out[2], 33);
  EXPECT_EQ(out[3], 44);
}

TEST_F(MultiEvalVArrayTest, EvaluateChunkedMutableVArrayInputs)
{
  Array<int> data1 = {1, 2, 3, 4};
  Array<int> data2 = {10, 20, 30, 40};
  const GVMutableArray in1 = GVMutableArray::from_span(GMutableSpan(data1.as_mutable_span()));
  const VMutableArray<int> in2 = VMutableArray<int>::from_span(data2);
  Array<int> out(4);

  evaluate_chunked_varray<64>(
      [](const int64_t size, const int *a, const int *b, int *r_out) {
        for (const int64_t i : IndexRange(size)) {
          r_out[i] = a[i] + b[i];
        }
      },
      IndexMask(4),
      TypeSequence<InputParamTag<int>, InputParamTag<int>, OutputParamTag<int>>(),
      in1,
      in2,
      out);

  EXPECT_EQ(out[0], 11);
  EXPECT_EQ(out[1], 22);
  EXPECT_EQ(out[2], 33);
  EXPECT_EQ(out[3], 44);
}

TEST_F(MultiEvalVArrayTest, ExecuteNonTrivialString)
{
  const VArray<std::string> in1 = VArray<std::string>::from_container(
      Array<std::string>{"a", "bb", "ccc"});
  const VArray<std::string> in2 = VArray<std::string>::from_single("!", 3);
  Array<std::string> out(3, NoInitialization{});

  evaluate_element_wise_varray<"iio">([](const std::string &a,
                                         const std::string &b,
                                         std::string &r_out) { new (&r_out) std::string(a + b); },
                                      exec_presets::Materialized(),
                                      IndexMask(3),
                                      in1,
                                      in2,
                                      out);

  EXPECT_EQ(out[0], "a!");
  EXPECT_EQ(out[1], "bb!");
  EXPECT_EQ(out[2], "ccc!");
}

TEST_F(MultiEvalVArrayTest, ExecuteNonTrivialStringFuncInput)
{
  const VArray<std::string> in = VArray<std::string>::from_func(
      4, [](const int64_t i) { return std::string(size_t(i + 1), 'x'); });
  Array<std::string> out(4, NoInitialization{});

  evaluate_element_wise_varray<"io">(
      [](const std::string &s, std::string &r_out) { new (&r_out) std::string(s + s); },
      exec_presets::Simple(),
      IndexMask(4),
      in,
      out);

  EXPECT_EQ(out[0], "xx");
  EXPECT_EQ(out[1], "xxxx");
  EXPECT_EQ(out[2], "xxxxxx");
  EXPECT_EQ(out[3], "xxxxxxxx");
}

TEST_F(MultiEvalVArrayTest, ExecuteNonTrivialStringMutable)
{
  const VArray<std::string> prefix = VArray<std::string>::from_single(">", 3);
  Array<std::string> values{"a", "b", "c"};

  evaluate_element_wise_varray<"im">(
      [](const std::string &pre, std::string &value) { value = pre + value; },
      exec_presets::Materialized(),
      IndexMask(3),
      prefix,
      values);

  EXPECT_EQ(values[0], ">a");
  EXPECT_EQ(values[1], ">b");
  EXPECT_EQ(values[2], ">c");
}

TEST_F(MultiEvalVArrayTest, ExecuteNonTrivialStringSparseMask)
{
  Array<std::string> values{"0", "1", "2", "3", "4", "5"};
  const VArray<std::string> suffix = VArray<std::string>::from_single("*", 6);

  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_indices<int>({1, 4}, memory);
  evaluate_element_wise_varray<"im">(
      [](const std::string &suf, std::string &value) { value += suf; },
      exec_presets::Materialized(),
      mask,
      suffix,
      values);

  EXPECT_EQ(values[0], "0");
  EXPECT_EQ(values[1], "1*");
  EXPECT_EQ(values[2], "2");
  EXPECT_EQ(values[3], "3");
  EXPECT_EQ(values[4], "4*");
  EXPECT_EQ(values[5], "5");
}

TEST_F(MultiEvalVArrayTest, ExecuteGVArrayNonTrivial)
{
  const GVArray in = VArray<std::string>::from_container(
      Array<std::string>{"one", "two", "three"});
  Array<std::string> out(3, NoInitialization{});

  evaluate_element_wise_varray(
      [](const std::string &s, std::string &out_value) { new (&out_value) std::string(s + "!"); },
      exec_presets::AllSpanOrSingle(),
      IndexMask(3),
      TypeSequence<InputParamTag<std::string>, OutputParamTag<std::string>>(),
      in,
      out);

  EXPECT_EQ(out[0], "one!");
  EXPECT_EQ(out[1], "two!");
  EXPECT_EQ(out[2], "three!");
}

TEST_F(MultiEvalVArrayTest, ExecuteGVArrayNonTrivialFunc)
{
  const GVArray in = GVArray::from_func(
      CPPType::get<std::string>(), 3, [](const int64_t i, void *r_value) {
        new (r_value) std::string("v" + std::to_string(i));
      });
  Array<int> lengths(3);

  evaluate_element_wise_varray(
      [](const std::string &s, int &out_value) { new (&out_value) int(int(s.size())); },
      exec_presets::Materialized(),
      IndexMask(3),
      TypeSequence<InputParamTag<std::string>, OutputParamTag<int>>(),
      in,
      lengths);

  EXPECT_EQ(lengths[0], 2);
  EXPECT_EQ(lengths[1], 2);
  EXPECT_EQ(lengths[2], 2);
}

}  // namespace blender::multi_eval::tests
