/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "testing/testing.h"

#include "FN_cpp_types.hh"
#include "FN_multi_function_builder.hh"
#include "FN_multi_function_network.hh"
#include "FN_multi_function_network_evaluation.hh"

namespace blender {
namespace fn {

TEST(multi_function_network, Test1)
{
  CustomFunction_SI_SO<int, int> add_10_fn("add 10", [](int value) { return value + 10; });
  CustomFunction_SI_SI_SO<int, int, int> multiply_fn("multiply",
                                                     [](int a, int b) { return a * b; });

  MFNetwork network;

  MFNode &node1 = network.add_function(add_10_fn);
  MFNode &node2 = network.add_function(multiply_fn);
  MFOutputSocket &input_socket = network.add_input("Input", MFDataType::ForSingle<int>());
  MFInputSocket &output_socket = network.add_output("Output", MFDataType::ForSingle<int>());
  network.add_link(node1.output(0), node2.input(0));
  network.add_link(node1.output(0), node2.input(1));
  network.add_link(node2.output(0), output_socket);
  network.add_link(input_socket, node1.input(0));

  MFNetworkEvaluator network_fn{{&input_socket}, {&output_socket}};

  {
    Array<int> values = {4, 6, 1, 2, 0};
    Array<int> results(values.size(), 0);

    MFParamsBuilder params(network_fn, values.size());
    params.add_readonly_single_input(values.as_span());
    params.add_uninitialized_single_output(results.as_mutable_span());

    MFContextBuilder context;

    network_fn.call({0, 2, 3, 4}, params, context);

    EXPECT_EQ(results[0], 14 * 14);
    EXPECT_EQ(results[1], 0);
    EXPECT_EQ(results[2], 11 * 11);
    EXPECT_EQ(results[3], 12 * 12);
    EXPECT_EQ(results[4], 10 * 10);
  }
  {
    int value = 3;
    Array<int> results(5, 0);

    MFParamsBuilder params(network_fn, results.size());
    params.add_readonly_single_input(&value);
    params.add_uninitialized_single_output(results.as_mutable_span());

    MFContextBuilder context;

    network_fn.call({1, 2, 4}, params, context);

    EXPECT_EQ(results[0], 0);
    EXPECT_EQ(results[1], 13 * 13);
    EXPECT_EQ(results[2], 13 * 13);
    EXPECT_EQ(results[3], 0);
    EXPECT_EQ(results[4], 13 * 13);
  }
}

class ConcatVectorsFunction : public MultiFunction {
 public:
  ConcatVectorsFunction()
  {
    MFSignatureBuilder signature = this->get_builder("Concat Vectors");
    signature.vector_mutable<int>("A");
    signature.vector_input<int>("B");
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    GVectorArrayRef<int> a = params.vector_mutable<int>(0);
    VArraySpan<int> b = params.readonly_vector_input<int>(1);

    for (uint i : mask) {
      a.extend(i, b[i]);
    }
  }
};

class AppendFunction : public MultiFunction {
 public:
  AppendFunction()
  {
    MFSignatureBuilder signature = this->get_builder("Append");
    signature.vector_mutable<int>("Vector");
    signature.single_input<int>("Value");
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    GVectorArrayRef<int> vectors = params.vector_mutable<int>(0);
    VSpan<int> values = params.readonly_single_input<int>(1);

    for (uint i : mask) {
      vectors.append(i, values[i]);
    }
  }
};

class SumVectorFunction : public MultiFunction {
 public:
  SumVectorFunction()
  {
    MFSignatureBuilder signature = this->get_builder("Sum Vector");
    signature.vector_input<int>("Vector");
    signature.single_output<int>("Sum");
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VArraySpan<int> vectors = params.readonly_vector_input<int>(0);
    MutableSpan<int> sums = params.uninitialized_single_output<int>(1);

    for (uint i : mask) {
      int sum = 0;
      VSpan<int> vector = vectors[i];
      for (uint j = 0; j < vector.size(); j++) {
        sum += vector[j];
      }
      sums[i] = sum;
    }
  }
};

class CreateRangeFunction : public MultiFunction {
 public:
  CreateRangeFunction()
  {
    MFSignatureBuilder builder = this->get_builder("Create Range");
    builder.single_input<int>("Size");
    builder.vector_output<int>("Range");
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VSpan<int> sizes = params.readonly_single_input<int>(0, "Size");
    GVectorArrayRef<int> ranges = params.vector_output<int>(1, "Range");

    for (int i : mask) {
      int size = sizes[i];
      for (int j : IndexRange(size)) {
        ranges.append(i, j);
      }
    }
  }
};

TEST(multi_function_network, Test2)
{
  CustomFunction_SI_SO<int, int> add_3_fn("add 3", [](int value) { return value + 3; });

  ConcatVectorsFunction concat_vectors_fn;
  AppendFunction append_fn;
  SumVectorFunction sum_fn;
  CreateRangeFunction create_range_fn;

  MFNetwork network;

  MFOutputSocket &input1 = network.add_input("Input 1", MFDataType::ForVector<int>());
  MFOutputSocket &input2 = network.add_input("Input 2", MFDataType::ForSingle<int>());
  MFInputSocket &output1 = network.add_output("Output 1", MFDataType::ForVector<int>());
  MFInputSocket &output2 = network.add_output("Output 2", MFDataType::ForSingle<int>());

  MFNode &node1 = network.add_function(add_3_fn);
  MFNode &node2 = network.add_function(create_range_fn);
  MFNode &node3 = network.add_function(concat_vectors_fn);
  MFNode &node4 = network.add_function(sum_fn);
  MFNode &node5 = network.add_function(append_fn);
  MFNode &node6 = network.add_function(sum_fn);

  network.add_link(input2, node1.input(0));
  network.add_link(node1.output(0), node2.input(0));
  network.add_link(node2.output(0), node3.input(1));
  network.add_link(input1, node3.input(0));
  network.add_link(input1, node4.input(0));
  network.add_link(node4.output(0), node5.input(1));
  network.add_link(node3.output(0), node5.input(0));
  network.add_link(node5.output(0), node6.input(0));
  network.add_link(node3.output(0), output1);
  network.add_link(node6.output(0), output2);

  // std::cout << network.to_dot() << "\n\n";

  MFNetworkEvaluator network_fn{{&input1, &input2}, {&output1, &output2}};

  {
    Array<int> input_value_1 = {3, 6};
    int input_value_2 = 4;

    GVectorArray output_value_1(CPPType_int32, 5);
    Array<int> output_value_2(5, -1);

    MFParamsBuilder params(network_fn, 5);
    params.add_readonly_vector_input(GVArraySpan(input_value_1.as_span(), 5));
    params.add_readonly_single_input(&input_value_2);
    params.add_vector_output(output_value_1);
    params.add_uninitialized_single_output(output_value_2.as_mutable_span());

    MFContextBuilder context;

    network_fn.call({1, 2, 4}, params, context);

    EXPECT_EQ(output_value_1[0].size(), 0);
    EXPECT_EQ(output_value_1[1].size(), 9);
    EXPECT_EQ(output_value_1[2].size(), 9);
    EXPECT_EQ(output_value_1[3].size(), 0);
    EXPECT_EQ(output_value_1[4].size(), 9);

    EXPECT_EQ(output_value_2[0], -1);
    EXPECT_EQ(output_value_2[1], 39);
    EXPECT_EQ(output_value_2[2], 39);
    EXPECT_EQ(output_value_2[3], -1);
    EXPECT_EQ(output_value_2[4], 39);
  }
  {
    GVectorArray input_value_1(CPPType_int32, 3);
    GVectorArrayRef<int> input_value_ref_1 = input_value_1;
    input_value_ref_1.extend(0, {3, 4, 5});
    input_value_ref_1.extend(1, {1, 2});

    Array<int> input_value_2 = {4, 2, 3};

    GVectorArray output_value_1(CPPType_int32, 3);
    Array<int> output_value_2(3, -1);

    MFParamsBuilder params(network_fn, 3);
    params.add_readonly_vector_input(input_value_1);
    params.add_readonly_single_input(input_value_2.as_span());
    params.add_vector_output(output_value_1);
    params.add_uninitialized_single_output(output_value_2.as_mutable_span());

    MFContextBuilder context;

    network_fn.call({0, 1, 2}, params, context);

    EXPECT_EQ(output_value_1[0].size(), 10);
    EXPECT_EQ(output_value_1[1].size(), 7);
    EXPECT_EQ(output_value_1[2].size(), 6);

    EXPECT_EQ(output_value_2[0], 45);
    EXPECT_EQ(output_value_2[1], 16);
    EXPECT_EQ(output_value_2[2], 15);
  }
}

}  // namespace fn
}  // namespace blender
