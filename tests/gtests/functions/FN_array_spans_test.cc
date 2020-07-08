/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "FN_array_spans.hh"
#include "FN_cpp_types.hh"
#include "FN_generic_vector_array.hh"

#include "BLI_array.hh"

namespace blender::fn {

TEST(virtual_array_span, EmptyConstructor)
{
  VArraySpan<int> span;
  EXPECT_EQ(span.size(), 0);
  EXPECT_TRUE(span.is_empty());

  GVArraySpan converted(span);
  EXPECT_EQ(converted.type(), CPPType::get<int>());
  EXPECT_EQ(converted.size(), 0);
}

TEST(virtual_array_span, SingleArrayConstructor)
{
  std::array<int, 4> values = {3, 4, 5, 6};
  VArraySpan<int> span{Span<int>(values), 3};
  EXPECT_EQ(span.size(), 3);
  EXPECT_FALSE(span.is_empty());
  EXPECT_EQ(span[0].size(), 4);
  EXPECT_EQ(span[1].size(), 4);
  EXPECT_EQ(span[2].size(), 4);
  EXPECT_EQ(span[0][0], 3);
  EXPECT_EQ(span[0][1], 4);
  EXPECT_EQ(span[0][2], 5);
  EXPECT_EQ(span[0][3], 6);
  EXPECT_EQ(span[1][3], 6);
  EXPECT_EQ(span[2][1], 4);

  GVArraySpan converted(span);
  EXPECT_EQ(converted.type(), CPPType::get<int>());
  EXPECT_EQ(converted.size(), 3);
  EXPECT_EQ(converted[0].size(), 4);
  EXPECT_EQ(converted[1].size(), 4);
  EXPECT_EQ(converted[1][2], &values[2]);
}

TEST(virtual_array_span, MultipleArrayConstructor)
{
  std::array<int, 4> values0 = {1, 2, 3, 4};
  std::array<int, 2> values1 = {6, 7};
  std::array<int, 1> values2 = {8};
  std::array<const int *, 3> starts = {values0.data(), values1.data(), values2.data()};
  std::array<uint, 3> sizes{values0.size(), values1.size(), values2.size()};

  VArraySpan<int> span{starts, sizes};
  EXPECT_EQ(span.size(), 3);
  EXPECT_FALSE(span.is_empty());
  EXPECT_EQ(span[0].size(), 4);
  EXPECT_EQ(span[1].size(), 2);
  EXPECT_EQ(span[2].size(), 1);
  EXPECT_EQ(&span[0][0], values0.data());
  EXPECT_EQ(&span[1][0], values1.data());
  EXPECT_EQ(&span[2][0], values2.data());
  EXPECT_EQ(span[2][0], 8);
  EXPECT_EQ(span[1][1], 7);

  GVArraySpan converted(span);
  EXPECT_EQ(converted.type(), CPPType::get<int>());
  EXPECT_EQ(converted.size(), 3);
  EXPECT_EQ(converted[0].size(), 4);
  EXPECT_EQ(converted[1].size(), 2);
  EXPECT_EQ(converted[2].size(), 1);
  EXPECT_EQ(converted[0][0], values0.data());
  EXPECT_EQ(converted[1][1], values1.data() + 1);
}

TEST(generic_virtual_array_span, TypeConstructor)
{
  GVArraySpan span{CPPType_int32};
  EXPECT_EQ(span.size(), 0);
  EXPECT_TRUE(span.is_empty());

  VArraySpan converted = span.typed<int>();
  EXPECT_EQ(converted.size(), 0);
}

TEST(generic_virtual_array_span, GSpanConstructor)
{
  std::array<std::string, 3> values = {"hello", "world", "test"};
  GVArraySpan span{GSpan(CPPType_string, values.data(), 3), 5};
  EXPECT_EQ(span.size(), 5);
  EXPECT_FALSE(span.is_empty());
  EXPECT_EQ(span[0][0], values.data());
  EXPECT_EQ(span[1][0], values.data());
  EXPECT_EQ(span[4][0], values.data());
  EXPECT_EQ(span[0].size(), 3);
  EXPECT_EQ(span[2].size(), 3);
  EXPECT_EQ(*(std::string *)span[3][1], "world");

  VArraySpan converted = span.typed<std::string>();
  EXPECT_EQ(converted.size(), 5);
  EXPECT_EQ(converted[0][0], "hello");
  EXPECT_EQ(converted[1][0], "hello");
  EXPECT_EQ(converted[4][0], "hello");
  EXPECT_EQ(converted[0].size(), 3);
  EXPECT_EQ(converted[2].size(), 3);
}

TEST(generic_virtual_array_span, IsSingleArray1)
{
  Array<int> values = {5, 6, 7};
  GVArraySpan span{GSpan(values.as_span()), 4};
  EXPECT_TRUE(span.is_single_array());

  VArraySpan converted = span.typed<int>();
  EXPECT_TRUE(converted.is_single_array());
}

TEST(generic_virtual_array_span, IsSingleArray2)
{
  GVectorArray vectors{CPPType_int32, 3};
  GVectorArrayRef<int> vectors_ref = vectors;
  vectors_ref.append(1, 4);

  GVArraySpan span = vectors;
  EXPECT_FALSE(span.is_single_array());

  VArraySpan converted = span.typed<int>();
  EXPECT_FALSE(converted.is_single_array());
}

}  // namespace blender::fn
