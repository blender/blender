/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_cpp_type.hh"
#include "FN_field.hh"
#include "FN_multi_function_builder.hh"
#include "FN_multi_function_test_common.hh"

namespace blender::fn::tests {

TEST(field, ConstantFunction)
{
  /* TODO: Figure out how to not use another "FieldOperation(" inside of std::make_shared. */
  GField constant_field{std::make_shared<FieldOperation>(
                            FieldOperation(std::make_unique<mf::CustomMF_Constant<int>>(10), {})),
                        0};

  Array<int> result(4);

  FieldContext context;
  FieldEvaluator evaluator{context, 4};
  evaluator.add_with_destination(constant_field, result.as_mutable_span());
  evaluator.evaluate();
  EXPECT_EQ(result[0], 10);
  EXPECT_EQ(result[1], 10);
  EXPECT_EQ(result[2], 10);
  EXPECT_EQ(result[3], 10);
}

class IndexFieldInput final : public FieldInput {
 public:
  IndexFieldInput() : FieldInput(CPPType::get<int>(), "Index")
  {
  }

  GVArray get_varray_for_context(const FieldContext & /*context*/,
                                 IndexMask mask,
                                 ResourceScope & /*scope*/) const final
  {
    auto index_func = [](int i) { return i; };
    return VArray<int>::ForFunc(mask.min_array_size(), index_func);
  }
};

TEST(field, VArrayInput)
{
  GField index_field{std::make_shared<IndexFieldInput>()};

  Array<int> result_1(4);

  FieldContext context;
  FieldEvaluator evaluator{context, 4};
  evaluator.add_with_destination(index_field, result_1.as_mutable_span());
  evaluator.evaluate();
  EXPECT_EQ(result_1[0], 0);
  EXPECT_EQ(result_1[1], 1);
  EXPECT_EQ(result_1[2], 2);
  EXPECT_EQ(result_1[3], 3);

  /* Evaluate a second time, just to test that the first didn't break anything. */
  Array<int> result_2(10);

  const Array<int64_t> indices = {2, 4, 6, 8};
  const IndexMask mask{indices};

  FieldEvaluator evaluator_2{context, &mask};
  evaluator_2.add_with_destination(index_field, result_2.as_mutable_span());
  evaluator_2.evaluate();
  EXPECT_EQ(result_2[2], 2);
  EXPECT_EQ(result_2[4], 4);
  EXPECT_EQ(result_2[6], 6);
  EXPECT_EQ(result_2[8], 8);
}

TEST(field, VArrayInputMultipleOutputs)
{
  std::shared_ptr<FieldInput> index_input = std::make_shared<IndexFieldInput>();
  GField field_1{index_input};
  GField field_2{index_input};

  Array<int> result_1(10);
  Array<int> result_2(10);

  const Array<int64_t> indices = {2, 4, 6, 8};
  const IndexMask mask{indices};

  FieldContext context;
  FieldEvaluator evaluator{context, &mask};
  evaluator.add_with_destination(field_1, result_1.as_mutable_span());
  evaluator.add_with_destination(field_2, result_2.as_mutable_span());
  evaluator.evaluate();
  EXPECT_EQ(result_1[2], 2);
  EXPECT_EQ(result_1[4], 4);
  EXPECT_EQ(result_1[6], 6);
  EXPECT_EQ(result_1[8], 8);
  EXPECT_EQ(result_2[2], 2);
  EXPECT_EQ(result_2[4], 4);
  EXPECT_EQ(result_2[6], 6);
  EXPECT_EQ(result_2[8], 8);
}

TEST(field, InputAndFunction)
{
  GField index_field{std::make_shared<IndexFieldInput>()};

  auto add_fn = mf::build::SI2_SO<int, int, int>("add", [](int a, int b) { return a + b; });
  GField output_field{
      std::make_shared<FieldOperation>(FieldOperation(add_fn, {index_field, index_field})), 0};

  Array<int> result(10);

  const Array<int64_t> indices = {2, 4, 6, 8};
  const IndexMask mask{indices};

  FieldContext context;
  FieldEvaluator evaluator{context, &mask};
  evaluator.add_with_destination(output_field, result.as_mutable_span());
  evaluator.evaluate();
  EXPECT_EQ(result[2], 4);
  EXPECT_EQ(result[4], 8);
  EXPECT_EQ(result[6], 12);
  EXPECT_EQ(result[8], 16);
}

TEST(field, TwoFunctions)
{
  GField index_field{std::make_shared<IndexFieldInput>()};

  auto add_fn = mf::build::SI2_SO<int, int, int>("add", [](int a, int b) { return a + b; });
  GField add_field{
      std::make_shared<FieldOperation>(FieldOperation(add_fn, {index_field, index_field})), 0};

  auto add_10_fn = mf::build::SI1_SO<int, int>("add_10", [](int a) { return a + 10; });
  GField result_field{std::make_shared<FieldOperation>(FieldOperation(add_10_fn, {add_field})), 0};

  Array<int> result(10);

  const Array<int64_t> indices = {2, 4, 6, 8};
  const IndexMask mask{indices};

  FieldContext context;
  FieldEvaluator evaluator{context, &mask};
  evaluator.add_with_destination(result_field, result.as_mutable_span());
  evaluator.evaluate();
  EXPECT_EQ(result[2], 14);
  EXPECT_EQ(result[4], 18);
  EXPECT_EQ(result[6], 22);
  EXPECT_EQ(result[8], 26);
}

class TwoOutputFunction : public mf::MultiFunction {
 private:
  mf::Signature signature_;

 public:
  TwoOutputFunction()
  {
    mf::SignatureBuilder builder{"Two Outputs", signature_};
    builder.single_input<int>("In1");
    builder.single_input<int>("In2");
    builder.single_output<int>("Add");
    builder.single_output<int>("Add10");
    this->set_signature(&signature_);
  }

  void call(IndexMask mask, mf::MFParams params, mf::Context /*context*/) const override
  {
    const VArray<int> &in1 = params.readonly_single_input<int>(0, "In1");
    const VArray<int> &in2 = params.readonly_single_input<int>(1, "In2");
    MutableSpan<int> add = params.uninitialized_single_output<int>(2, "Add");
    MutableSpan<int> add_10 = params.uninitialized_single_output<int>(3, "Add10");
    mask.foreach_index([&](const int64_t i) {
      add[i] = in1[i] + in2[i];
      add_10[i] = add[i] + 10;
    });
  }
};

TEST(field, FunctionTwoOutputs)
{
  /* Also use two separate input fields, why not. */
  GField index_field_1{std::make_shared<IndexFieldInput>()};
  GField index_field_2{std::make_shared<IndexFieldInput>()};

  std::shared_ptr<FieldOperation> fn = std::make_shared<FieldOperation>(
      FieldOperation(std::make_unique<TwoOutputFunction>(), {index_field_1, index_field_2}));

  GField result_field_1{fn, 0};
  GField result_field_2{fn, 1};

  Array<int> result_1(10);
  Array<int> result_2(10);

  const Array<int64_t> indices = {2, 4, 6, 8};
  const IndexMask mask{indices};

  FieldContext context;
  FieldEvaluator evaluator{context, &mask};
  evaluator.add_with_destination(result_field_1, result_1.as_mutable_span());
  evaluator.add_with_destination(result_field_2, result_2.as_mutable_span());
  evaluator.evaluate();
  EXPECT_EQ(result_1[2], 4);
  EXPECT_EQ(result_1[4], 8);
  EXPECT_EQ(result_1[6], 12);
  EXPECT_EQ(result_1[8], 16);
  EXPECT_EQ(result_2[2], 14);
  EXPECT_EQ(result_2[4], 18);
  EXPECT_EQ(result_2[6], 22);
  EXPECT_EQ(result_2[8], 26);
}

TEST(field, TwoFunctionsTwoOutputs)
{
  GField index_field{std::make_shared<IndexFieldInput>()};

  std::shared_ptr<FieldOperation> fn = std::make_shared<FieldOperation>(
      FieldOperation(std::make_unique<TwoOutputFunction>(), {index_field, index_field}));

  Array<int64_t> mask_indices = {2, 4, 6, 8};
  IndexMask mask = mask_indices.as_span();

  Field<int> result_field_1{fn, 0};
  Field<int> intermediate_field{fn, 1};

  auto add_10_fn = mf::build::SI1_SO<int, int>("add_10", [](int a) { return a + 10; });
  Field<int> result_field_2{
      std::make_shared<FieldOperation>(FieldOperation(add_10_fn, {intermediate_field})), 0};

  FieldContext field_context;
  FieldEvaluator field_evaluator{field_context, &mask};
  VArray<int> result_1;
  VArray<int> result_2;
  field_evaluator.add(result_field_1, &result_1);
  field_evaluator.add(result_field_2, &result_2);
  field_evaluator.evaluate();

  EXPECT_EQ(result_1.get(2), 4);
  EXPECT_EQ(result_1.get(4), 8);
  EXPECT_EQ(result_1.get(6), 12);
  EXPECT_EQ(result_1.get(8), 16);
  EXPECT_EQ(result_2.get(2), 24);
  EXPECT_EQ(result_2.get(4), 28);
  EXPECT_EQ(result_2.get(6), 32);
  EXPECT_EQ(result_2.get(8), 36);
}

TEST(field, SameFieldTwice)
{
  GField constant_field{
      std::make_shared<FieldOperation>(std::make_unique<mf::CustomMF_Constant<int>>(10)), 0};

  FieldContext field_context;
  IndexMask mask{IndexRange(2)};
  ResourceScope scope;
  Vector<GVArray> results = evaluate_fields(
      scope, {constant_field, constant_field}, mask, field_context);

  VArray<int> varray1 = results[0].typed<int>();
  VArray<int> varray2 = results[1].typed<int>();

  EXPECT_EQ(varray1.get(0), 10);
  EXPECT_EQ(varray1.get(1), 10);
  EXPECT_EQ(varray2.get(0), 10);
  EXPECT_EQ(varray2.get(1), 10);
}

TEST(field, IgnoredOutput)
{
  static mf::tests::OptionalOutputsFunction fn;
  Field<int> field{std::make_shared<FieldOperation>(fn), 0};

  FieldContext field_context;
  FieldEvaluator field_evaluator{field_context, 10};
  VArray<int> results;
  field_evaluator.add(field, &results);
  field_evaluator.evaluate();

  EXPECT_EQ(results.get(0), 5);
  EXPECT_EQ(results.get(3), 5);
}

}  // namespace blender::fn::tests
