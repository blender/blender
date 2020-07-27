/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "FN_spans.hh"

namespace blender::fn::tests {

TEST(generic_span, TypeConstructor)
{
  GSpan span(CPPType::get<float>());
  EXPECT_EQ(span.size(), 0);
  EXPECT_EQ(span.typed<float>().size(), 0);
  EXPECT_TRUE(span.is_empty());
}

TEST(generic_span, BufferAndSizeConstructor)
{
  int values[4] = {6, 7, 3, 2};
  void *buffer = (void *)values;
  GSpan span(CPPType::get<int32_t>(), buffer, 4);
  EXPECT_EQ(span.size(), 4);
  EXPECT_FALSE(span.is_empty());
  EXPECT_EQ(span.typed<int>().size(), 4);
  EXPECT_EQ(span[0], &values[0]);
  EXPECT_EQ(span[1], &values[1]);
  EXPECT_EQ(span[2], &values[2]);
  EXPECT_EQ(span[3], &values[3]);
}

TEST(generic_mutable_span, TypeConstructor)
{
  GMutableSpan span(CPPType::get<int32_t>());
  EXPECT_EQ(span.size(), 0);
  EXPECT_TRUE(span.is_empty());
}

TEST(generic_mutable_span, BufferAndSizeConstructor)
{
  int values[4] = {4, 7, 3, 5};
  void *buffer = (void *)values;
  GMutableSpan span(CPPType::get<int32_t>(), buffer, 4);
  EXPECT_EQ(span.size(), 4);
  EXPECT_FALSE(span.is_empty());
  EXPECT_EQ(span.typed<int>().size(), 4);
  EXPECT_EQ(values[2], 3);
  *(int *)span[2] = 10;
  EXPECT_EQ(values[2], 10);
  span.typed<int>()[2] = 20;
  EXPECT_EQ(values[2], 20);
}

TEST(virtual_span, EmptyConstructor)
{
  VSpan<int> span;
  EXPECT_EQ(span.size(), 0);
  EXPECT_TRUE(span.is_empty());
  EXPECT_FALSE(span.is_single_element());
  EXPECT_TRUE(span.is_full_array());

  GVSpan converted(span);
  EXPECT_EQ(converted.type(), CPPType::get<int>());
  EXPECT_EQ(converted.size(), 0);
}

TEST(virtual_span, SpanConstructor)
{
  std::array<int, 5> values = {7, 3, 8, 6, 4};
  Span<int> span = values;
  VSpan<int> virtual_span = span;
  EXPECT_EQ(virtual_span.size(), 5);
  EXPECT_FALSE(virtual_span.is_empty());
  EXPECT_EQ(virtual_span[0], 7);
  EXPECT_EQ(virtual_span[2], 8);
  EXPECT_EQ(virtual_span[3], 6);
  EXPECT_FALSE(virtual_span.is_single_element());
  EXPECT_TRUE(virtual_span.is_full_array());

  GVSpan converted(span);
  EXPECT_EQ(converted.type(), CPPType::get<int>());
  EXPECT_EQ(converted.size(), 5);
}

TEST(virtual_span, PointerSpanConstructor)
{
  int x0 = 3;
  int x1 = 6;
  int x2 = 7;
  std::array<const int *, 3> pointers = {&x0, &x2, &x1};
  VSpan<int> span = Span<const int *>(pointers);
  EXPECT_EQ(span.size(), 3);
  EXPECT_FALSE(span.is_empty());
  EXPECT_EQ(span[0], 3);
  EXPECT_EQ(span[1], 7);
  EXPECT_EQ(span[2], 6);
  EXPECT_EQ(&span[1], &x2);
  EXPECT_FALSE(span.is_single_element());
  EXPECT_FALSE(span.is_full_array());

  GVSpan converted(span);
  EXPECT_EQ(converted.type(), CPPType::get<int>());
  EXPECT_EQ(converted.size(), 3);
  EXPECT_EQ(converted[0], &x0);
  EXPECT_EQ(converted[1], &x2);
  EXPECT_EQ(converted[2], &x1);
}

TEST(virtual_span, SingleConstructor)
{
  int value = 5;
  VSpan<int> span = VSpan<int>::FromSingle(&value, 3);
  EXPECT_EQ(span.size(), 3);
  EXPECT_FALSE(span.is_empty());
  EXPECT_EQ(span[0], 5);
  EXPECT_EQ(span[1], 5);
  EXPECT_EQ(span[2], 5);
  EXPECT_EQ(&span[0], &value);
  EXPECT_EQ(&span[1], &value);
  EXPECT_EQ(&span[2], &value);
  EXPECT_TRUE(span.is_single_element());
  EXPECT_FALSE(span.is_full_array());

  GVSpan converted(span);
  EXPECT_EQ(converted.type(), CPPType::get<int>());
  EXPECT_EQ(converted.size(), 3);
  EXPECT_EQ(converted[0], &value);
  EXPECT_EQ(converted[1], &value);
  EXPECT_EQ(converted[2], &value);
}

TEST(generic_virtual_span, TypeConstructor)
{
  GVSpan span(CPPType::get<int32_t>());
  EXPECT_EQ(span.size(), 0);
  EXPECT_TRUE(span.is_empty());
  EXPECT_FALSE(span.is_single_element());
  EXPECT_TRUE(span.is_full_array());

  VSpan<int> converted = span.typed<int>();
  EXPECT_EQ(converted.size(), 0);
}

TEST(generic_virtual_span, GenericSpanConstructor)
{
  int values[4] = {3, 4, 5, 6};
  GVSpan span{GSpan(CPPType::get<int32_t>(), values, 4)};
  EXPECT_EQ(span.size(), 4);
  EXPECT_FALSE(span.is_empty());
  EXPECT_EQ(span[0], &values[0]);
  EXPECT_EQ(span[1], &values[1]);
  EXPECT_EQ(span[2], &values[2]);
  EXPECT_EQ(span[3], &values[3]);
  EXPECT_FALSE(span.is_single_element());
  EXPECT_TRUE(span.is_full_array());

  int materialized[4] = {0};
  span.materialize_to_uninitialized(materialized);
  EXPECT_EQ(materialized[0], 3);
  EXPECT_EQ(materialized[1], 4);
  EXPECT_EQ(materialized[2], 5);
  EXPECT_EQ(materialized[3], 6);

  VSpan<int> converted = span.typed<int>();
  EXPECT_EQ(converted.size(), 4);
  EXPECT_EQ(converted[0], 3);
  EXPECT_EQ(converted[1], 4);
  EXPECT_EQ(converted[2], 5);
  EXPECT_EQ(converted[3], 6);
}

TEST(generic_virtual_span, SpanConstructor)
{
  std::array<int, 3> values = {6, 7, 8};
  GVSpan span{Span<int>(values)};
  EXPECT_EQ(span.type(), CPPType::get<int32_t>());
  EXPECT_EQ(span.size(), 3);
  EXPECT_EQ(span[0], &values[0]);
  EXPECT_EQ(span[1], &values[1]);
  EXPECT_EQ(span[2], &values[2]);
  EXPECT_FALSE(span.is_single_element());
  EXPECT_TRUE(span.is_full_array());

  int materialized[3] = {0};
  span.materialize_to_uninitialized(materialized);
  EXPECT_EQ(materialized[0], 6);
  EXPECT_EQ(materialized[1], 7);
  EXPECT_EQ(materialized[2], 8);

  VSpan<int> converted = span.typed<int>();
  EXPECT_EQ(converted.size(), 3);
  EXPECT_EQ(converted[0], 6);
  EXPECT_EQ(converted[1], 7);
  EXPECT_EQ(converted[2], 8);
}

TEST(generic_virtual_span, SingleConstructor)
{
  int value = 5;
  GVSpan span = GVSpan::FromSingle(CPPType::get<int32_t>(), &value, 3);
  EXPECT_EQ(span.size(), 3);
  EXPECT_FALSE(span.is_empty());
  EXPECT_EQ(span[0], &value);
  EXPECT_EQ(span[1], &value);
  EXPECT_EQ(span[2], &value);
  EXPECT_TRUE(span.is_single_element());
  EXPECT_EQ(span.as_single_element(), &value);
  EXPECT_FALSE(span.is_full_array());

  int materialized[3] = {0};
  span.materialize_to_uninitialized({1, 2}, materialized);
  EXPECT_EQ(materialized[0], 0);
  EXPECT_EQ(materialized[1], 5);
  EXPECT_EQ(materialized[2], 5);

  VSpan<int> converted = span.typed<int>();
  EXPECT_EQ(converted.size(), 3);
  EXPECT_EQ(converted[0], 5);
  EXPECT_EQ(converted[1], 5);
  EXPECT_EQ(converted[2], 5);
}

}  // namespace blender::fn::tests
