/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "FN_multi_function.hh"
#include "FN_multi_function_builder.hh"
#include "FN_multi_function_test_common.hh"

namespace blender::fn::tests {
namespace {

class AddFunction : public MultiFunction {
 public:
  AddFunction()
  {
    static MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static MFSignature create_signature()
  {
    MFSignatureBuilder signature("Add");
    signature.single_input<int>("A");
    signature.single_input<int>("B");
    signature.single_output<int>("Result");
    return signature.build();
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    const VArray<int> &a = params.readonly_single_input<int>(0, "A");
    const VArray<int> &b = params.readonly_single_input<int>(1, "B");
    MutableSpan<int> result = params.uninitialized_single_output<int>(2, "Result");

    for (int64_t i : mask) {
      result[i] = a[i] + b[i];
    }
  }
};

TEST(multi_function, AddFunction)
{
  AddFunction fn;

  Array<int> input1 = {4, 5, 6};
  Array<int> input2 = {10, 20, 30};
  Array<int> output(3, -1);

  MFParamsBuilder params(fn, 3);
  params.add_readonly_single_input(input1.as_span());
  params.add_readonly_single_input(input2.as_span());
  params.add_uninitialized_single_output(output.as_mutable_span());

  MFContextBuilder context;

  fn.call({0, 2}, params, context);

  EXPECT_EQ(output[0], 14);
  EXPECT_EQ(output[1], -1);
  EXPECT_EQ(output[2], 36);
}

TEST(multi_function, AddPrefixFunction)
{
  AddPrefixFunction fn;

  Array<std::string> strings = {
      "Hello",
      "World",
      "This is a test",
      "Another much longer string to trigger an allocation",
  };

  std::string prefix = "AB";

  MFParamsBuilder params(fn, strings.size());
  params.add_readonly_single_input(&prefix);
  params.add_single_mutable(strings.as_mutable_span());

  MFContextBuilder context;

  fn.call({0, 2, 3}, params, context);

  EXPECT_EQ(strings[0], "ABHello");
  EXPECT_EQ(strings[1], "World");
  EXPECT_EQ(strings[2], "ABThis is a test");
  EXPECT_EQ(strings[3], "ABAnother much longer string to trigger an allocation");
}

TEST(multi_function, CreateRangeFunction)
{
  CreateRangeFunction fn;

  GVectorArray ranges(CPPType::get<int>(), 5);
  GVectorArray_TypedMutableRef<int> ranges_ref{ranges};
  Array<int> sizes = {3, 0, 6, 1, 4};

  MFParamsBuilder params(fn, ranges.size());
  params.add_readonly_single_input(sizes.as_span());
  params.add_vector_output(ranges);

  MFContextBuilder context;

  fn.call({0, 1, 2, 3}, params, context);

  EXPECT_EQ(ranges[0].size(), 3);
  EXPECT_EQ(ranges[1].size(), 0);
  EXPECT_EQ(ranges[2].size(), 6);
  EXPECT_EQ(ranges[3].size(), 1);
  EXPECT_EQ(ranges[4].size(), 0);

  EXPECT_EQ(ranges_ref[0][0], 0);
  EXPECT_EQ(ranges_ref[0][1], 1);
  EXPECT_EQ(ranges_ref[0][2], 2);
  EXPECT_EQ(ranges_ref[2][0], 0);
  EXPECT_EQ(ranges_ref[2][1], 1);
}

TEST(multi_function, GenericAppendFunction)
{
  GenericAppendFunction fn(CPPType::get<int32_t>());

  GVectorArray vectors(CPPType::get<int32_t>(), 4);
  GVectorArray_TypedMutableRef<int> vectors_ref{vectors};
  vectors_ref.append(0, 1);
  vectors_ref.append(0, 2);
  vectors_ref.append(2, 6);
  Array<int> values = {5, 7, 3, 1};

  MFParamsBuilder params(fn, vectors.size());
  params.add_vector_mutable(vectors);
  params.add_readonly_single_input(values.as_span());

  MFContextBuilder context;

  fn.call(IndexRange(vectors.size()), params, context);

  EXPECT_EQ(vectors[0].size(), 3);
  EXPECT_EQ(vectors[1].size(), 1);
  EXPECT_EQ(vectors[2].size(), 2);
  EXPECT_EQ(vectors[3].size(), 1);

  EXPECT_EQ(vectors_ref[0][0], 1);
  EXPECT_EQ(vectors_ref[0][1], 2);
  EXPECT_EQ(vectors_ref[0][2], 5);
  EXPECT_EQ(vectors_ref[1][0], 7);
  EXPECT_EQ(vectors_ref[2][0], 6);
  EXPECT_EQ(vectors_ref[2][1], 3);
  EXPECT_EQ(vectors_ref[3][0], 1);
}

TEST(multi_function, CustomMF_SI_SO)
{
  CustomMF_SI_SO<std::string, uint> fn("strlen",
                                       [](const std::string &str) { return str.size(); });

  Array<std::string> strings = {"hello", "world", "test", "another test"};
  Array<uint> sizes(strings.size(), 0);

  MFParamsBuilder params(fn, strings.size());
  params.add_readonly_single_input(strings.as_span());
  params.add_uninitialized_single_output(sizes.as_mutable_span());

  MFContextBuilder context;

  fn.call(IndexRange(strings.size()), params, context);

  EXPECT_EQ(sizes[0], 5);
  EXPECT_EQ(sizes[1], 5);
  EXPECT_EQ(sizes[2], 4);
  EXPECT_EQ(sizes[3], 12);
}

TEST(multi_function, CustomMF_SI_SI_SO)
{
  CustomMF_SI_SI_SO<int, int, int> fn("mul", [](int a, int b) { return a * b; });

  Array<int> values_a = {4, 6, 8, 9};
  int value_b = 10;
  Array<int> outputs(values_a.size(), -1);

  MFParamsBuilder params(fn, values_a.size());
  params.add_readonly_single_input(values_a.as_span());
  params.add_readonly_single_input(&value_b);
  params.add_uninitialized_single_output(outputs.as_mutable_span());

  MFContextBuilder context;

  fn.call({0, 1, 3}, params, context);

  EXPECT_EQ(outputs[0], 40);
  EXPECT_EQ(outputs[1], 60);
  EXPECT_EQ(outputs[2], -1);
  EXPECT_EQ(outputs[3], 90);
}

TEST(multi_function, CustomMF_SI_SI_SI_SO)
{
  CustomMF_SI_SI_SI_SO<int, std::string, bool, uint> fn{
      "custom",
      [](int a, const std::string &b, bool c) { return (uint)((uint)a + b.size() + (uint)c); }};

  Array<int> values_a = {5, 7, 3, 8};
  Array<std::string> values_b = {"hello", "world", "another", "test"};
  Array<bool> values_c = {true, false, false, true};
  Array<uint> outputs(values_a.size(), 0);

  MFParamsBuilder params(fn, values_a.size());
  params.add_readonly_single_input(values_a.as_span());
  params.add_readonly_single_input(values_b.as_span());
  params.add_readonly_single_input(values_c.as_span());
  params.add_uninitialized_single_output(outputs.as_mutable_span());

  MFContextBuilder context;

  fn.call({1, 2, 3}, params, context);

  EXPECT_EQ(outputs[0], 0);
  EXPECT_EQ(outputs[1], 12);
  EXPECT_EQ(outputs[2], 10);
  EXPECT_EQ(outputs[3], 13);
}

TEST(multi_function, CustomMF_SM)
{
  CustomMF_SM<std::string> fn("AddSuffix", [](std::string &value) { value += " test"; });

  Array<std::string> values = {"a", "b", "c", "d", "e"};

  MFParamsBuilder params(fn, values.size());
  params.add_single_mutable(values.as_mutable_span());

  MFContextBuilder context;

  fn.call({1, 2, 3}, params, context);

  EXPECT_EQ(values[0], "a");
  EXPECT_EQ(values[1], "b test");
  EXPECT_EQ(values[2], "c test");
  EXPECT_EQ(values[3], "d test");
  EXPECT_EQ(values[4], "e");
}

TEST(multi_function, CustomMF_Constant)
{
  CustomMF_Constant<int> fn{42};

  Array<int> outputs(4, 0);

  MFParamsBuilder params(fn, outputs.size());
  params.add_uninitialized_single_output(outputs.as_mutable_span());

  MFContextBuilder context;

  fn.call({0, 2, 3}, params, context);

  EXPECT_EQ(outputs[0], 42);
  EXPECT_EQ(outputs[1], 0);
  EXPECT_EQ(outputs[2], 42);
  EXPECT_EQ(outputs[3], 42);
}

TEST(multi_function, CustomMF_GenericConstant)
{
  int value = 42;
  CustomMF_GenericConstant fn{CPPType::get<int32_t>(), (const void *)&value, false};

  Array<int> outputs(4, 0);

  MFParamsBuilder params(fn, outputs.size());
  params.add_uninitialized_single_output(outputs.as_mutable_span());

  MFContextBuilder context;

  fn.call({0, 1, 2}, params, context);

  EXPECT_EQ(outputs[0], 42);
  EXPECT_EQ(outputs[1], 42);
  EXPECT_EQ(outputs[2], 42);
  EXPECT_EQ(outputs[3], 0);
}

TEST(multi_function, CustomMF_GenericConstantArray)
{
  std::array<int, 4> values = {3, 4, 5, 6};
  CustomMF_GenericConstantArray fn{GSpan(Span(values))};

  GVectorArray vector_array{CPPType::get<int32_t>(), 4};
  GVectorArray_TypedMutableRef<int> vector_array_ref{vector_array};

  MFParamsBuilder params(fn, vector_array.size());
  params.add_vector_output(vector_array);

  MFContextBuilder context;

  fn.call({1, 2, 3}, params, context);

  EXPECT_EQ(vector_array[0].size(), 0);
  EXPECT_EQ(vector_array[1].size(), 4);
  EXPECT_EQ(vector_array[2].size(), 4);
  EXPECT_EQ(vector_array[3].size(), 4);
  for (int i = 1; i < 4; i++) {
    EXPECT_EQ(vector_array_ref[i][0], 3);
    EXPECT_EQ(vector_array_ref[i][1], 4);
    EXPECT_EQ(vector_array_ref[i][2], 5);
    EXPECT_EQ(vector_array_ref[i][3], 6);
  }
}

TEST(multi_function, IgnoredOutputs)
{
  OptionalOutputsFunction fn;
  {
    MFParamsBuilder params(fn, 10);
    params.add_ignored_single_output("Out 1");
    params.add_ignored_single_output("Out 2");
    MFContextBuilder context;
    fn.call(IndexRange(10), params, context);
  }
  {
    Array<int> results_1(10);
    Array<std::string> results_2(10, NoInitialization());

    MFParamsBuilder params(fn, 10);
    params.add_uninitialized_single_output(results_1.as_mutable_span(), "Out 1");
    params.add_uninitialized_single_output(results_2.as_mutable_span(), "Out 2");
    MFContextBuilder context;
    fn.call(IndexRange(10), params, context);

    EXPECT_EQ(results_1[0], 5);
    EXPECT_EQ(results_1[3], 5);
    EXPECT_EQ(results_1[9], 5);
    EXPECT_EQ(results_2[0], "hello, this is a long string");
  }
}

}  // namespace
}  // namespace blender::fn::tests
