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
#include "FN_multi_function.hh"

namespace blender {
namespace fn {

class AddFunction : public MultiFunction {
 public:
  AddFunction()
  {
    MFSignatureBuilder builder = this->get_builder("Add");
    builder.single_input<int>("A");
    builder.single_input<int>("B");
    builder.single_output<int>("Result");
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VSpan<int> a = params.readonly_single_input<int>(0, "A");
    VSpan<int> b = params.readonly_single_input<int>(1, "B");
    MutableSpan<int> result = params.uninitialized_single_output<int>(2, "Result");

    for (uint i : mask) {
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

class AddPrefixFunction : public MultiFunction {
 public:
  AddPrefixFunction()
  {
    MFSignatureBuilder builder = this->get_builder("Add Prefix");
    builder.single_input<std::string>("Prefix");
    builder.single_mutable<std::string>("Strings");
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VSpan<std::string> prefixes = params.readonly_single_input<std::string>(0, "Prefix");
    MutableSpan<std::string> strings = params.single_mutable<std::string>(1, "Strings");

    for (uint i : mask) {
      strings[i] = prefixes[i] + strings[i];
    }
  }
};

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

class CreateRangeFunction : public MultiFunction {
 public:
  CreateRangeFunction()
  {
    MFSignatureBuilder builder = this->get_builder("Create Range");
    builder.single_input<uint>("Size");
    builder.vector_output<uint>("Range");
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VSpan<uint> sizes = params.readonly_single_input<uint>(0, "Size");
    GVectorArrayRef<uint> ranges = params.vector_output<uint>(1, "Range");

    for (uint i : mask) {
      uint size = sizes[i];
      for (uint j : IndexRange(size)) {
        ranges.append(i, j);
      }
    }
  }
};

TEST(multi_function, CreateRangeFunction)
{
  CreateRangeFunction fn;

  GVectorArray ranges(CPPType_uint32, 5);
  GVectorArrayRef<uint> ranges_ref(ranges);
  Array<uint> sizes = {3, 0, 6, 1, 4};

  MFParamsBuilder params(fn, ranges.size());
  params.add_readonly_single_input(sizes.as_span());
  params.add_vector_output(ranges);

  MFContextBuilder context;

  fn.call({0, 1, 2, 3}, params, context);

  EXPECT_EQ(ranges_ref[0].size(), 3);
  EXPECT_EQ(ranges_ref[1].size(), 0);
  EXPECT_EQ(ranges_ref[2].size(), 6);
  EXPECT_EQ(ranges_ref[3].size(), 1);
  EXPECT_EQ(ranges_ref[4].size(), 0);

  EXPECT_EQ(ranges_ref[0][0], 0);
  EXPECT_EQ(ranges_ref[0][1], 1);
  EXPECT_EQ(ranges_ref[0][2], 2);
  EXPECT_EQ(ranges_ref[2][0], 0);
  EXPECT_EQ(ranges_ref[2][1], 1);
}

class GenericAppendFunction : public MultiFunction {
 public:
  GenericAppendFunction(const CPPType &type)
  {
    MFSignatureBuilder builder = this->get_builder("Append");
    builder.vector_mutable("Vector", type);
    builder.single_input("Value", type);
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    GVectorArray &vectors = params.vector_mutable(0, "Vector");
    GVSpan values = params.readonly_single_input(1, "Value");

    for (uint i : mask) {
      vectors.append(i, values[i]);
    }
  }
};

TEST(multi_function, GenericAppendFunction)
{
  GenericAppendFunction fn(CPPType_int32);

  GVectorArray vectors(CPPType_int32, 4);
  GVectorArrayRef<int> vectors_ref(vectors);
  vectors_ref.append(0, 1);
  vectors_ref.append(0, 2);
  vectors_ref.append(2, 6);
  Array<int> values = {5, 7, 3, 1};

  MFParamsBuilder params(fn, vectors.size());
  params.add_vector_mutable(vectors);
  params.add_readonly_single_input(values.as_span());

  MFContextBuilder context;

  fn.call(IndexRange(vectors.size()), params, context);

  EXPECT_EQ(vectors_ref[0].size(), 3);
  EXPECT_EQ(vectors_ref[1].size(), 1);
  EXPECT_EQ(vectors_ref[2].size(), 2);
  EXPECT_EQ(vectors_ref[3].size(), 1);

  EXPECT_EQ(vectors_ref[0][0], 1);
  EXPECT_EQ(vectors_ref[0][1], 2);
  EXPECT_EQ(vectors_ref[0][2], 5);
  EXPECT_EQ(vectors_ref[1][0], 7);
  EXPECT_EQ(vectors_ref[2][0], 6);
  EXPECT_EQ(vectors_ref[2][1], 3);
  EXPECT_EQ(vectors_ref[3][0], 1);
}

}  // namespace fn
}  // namespace blender
