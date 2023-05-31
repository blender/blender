/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_generic_array.hh"

namespace blender::tests {

TEST(generic_array, TypeConstructor)
{
  GArray array(CPPType::get<float>());
  EXPECT_TRUE(array.data() == nullptr);
  EXPECT_EQ(array.size(), 0);
  EXPECT_EQ(array.as_span().typed<float>().size(), 0);
  EXPECT_TRUE(array.is_empty());
}

TEST(generic_array, MoveConstructor)
{
  GArray array_a(CPPType::get<int32_t>(), int64_t(10));
  GMutableSpan span_a = array_a.as_mutable_span();
  MutableSpan<int32_t> typed_span_a = span_a.typed<int32_t>();
  typed_span_a.fill(42);

  const GArray array_b = std::move(array_a);
  Span<int32_t> typed_span_b = array_b.as_span().typed<int32_t>();
  EXPECT_FALSE(array_b.data() == nullptr);
  EXPECT_EQ(array_b.size(), 10);
  EXPECT_EQ(typed_span_b[4], 42);

  /* Make sure the copy constructor cleaned up the original, but it shouldn't clear the type. */
  EXPECT_TRUE(array_a.data() == nullptr);    /* NOLINT: bugprone-use-after-move */
  EXPECT_EQ(array_a.size(), 0);              /* NOLINT: bugprone-use-after-move */
  EXPECT_TRUE(array_a.is_empty());           /* NOLINT: bugprone-use-after-move */
  EXPECT_EQ(array_b.type(), array_a.type()); /* NOLINT: bugprone-use-after-move */
}

TEST(generic_array, CopyConstructor)
{
  GArray array_a(CPPType::get<int32_t>(), int64_t(10));
  GMutableSpan span_a = array_a.as_mutable_span();
  MutableSpan<int32_t> typed_span_a = span_a.typed<int32_t>();
  typed_span_a.fill(42);

  /* From span directly. */
  const GArray array_b = array_a.as_span();
  Span<int32_t> typed_span_b = array_b.as_span().typed<int32_t>();
  EXPECT_FALSE(array_b.data() == nullptr);
  EXPECT_EQ(array_b.size(), 10);
  EXPECT_EQ(typed_span_b[4], 42);
  EXPECT_FALSE(array_a.is_empty());

  /* From array. */
  const GArray array_c = array_a;
  Span<int32_t> typed_span_c = array_c.as_span().typed<int32_t>();
  EXPECT_FALSE(array_c.data() == nullptr);
  EXPECT_EQ(array_c.size(), 10);
  EXPECT_EQ(typed_span_c[4], 42);
  EXPECT_FALSE(array_a.is_empty());
}

TEST(generic_array, BufferAndSizeConstructor)
{
  int32_t *values = (int32_t *)MEM_malloc_arrayN(12, sizeof(int32_t), __func__);
  void *buffer = (void *)values;
  GArray array(CPPType::get<int32_t>(), buffer, 4);
  EXPECT_FALSE(array.data() == nullptr);
  EXPECT_EQ(array.size(), 4);
  EXPECT_FALSE(array.is_empty());
  EXPECT_EQ(array.as_span().typed<int>().size(), 4);
  EXPECT_EQ(array[0], &values[0]);
  EXPECT_EQ(array[1], &values[1]);
  EXPECT_EQ(array[2], &values[2]);
  EXPECT_EQ(array[3], &values[3]);
}

TEST(generic_array, Reinitialize)
{
  GArray array(CPPType::get<int32_t>(), int64_t(5));
  EXPECT_FALSE(array.data() == nullptr);
  GMutableSpan span = array.as_mutable_span();
  MutableSpan<int32_t> typed_span = span.typed<int32_t>();
  typed_span.fill(77);
  EXPECT_FALSE(typed_span.data() == nullptr);
  typed_span[2] = 8;
  EXPECT_EQ(array[2], &typed_span[2]);
  EXPECT_EQ(typed_span[0], 77);
  EXPECT_EQ(typed_span[1], 77);

  array.reinitialize(10);
  EXPECT_EQ(array.size(), 10);
  span = array.as_mutable_span();
  EXPECT_EQ(span.size(), 10);

  typed_span = span.typed<int32_t>();
  EXPECT_FALSE(typed_span.data() == nullptr);

  array.reinitialize(0);
  EXPECT_EQ(array.size(), 0);
}

TEST(generic_array, InContainer)
{
  blender::Array<GArray<>> arrays;
  for (GArray<> &array : arrays) {
    array = GArray(CPPType::get<int32_t>(), int64_t(5));
    array.as_mutable_span().typed<int32_t>().fill(55);
  }
  for (GArray<> &array : arrays) {
    EXPECT_EQ(array.as_span().typed<int32_t>()[3], 55);
  }
}

TEST(generic_array, ReinitEmpty)
{
  GArray<> array(CPPType::get<int>());
  array.reinitialize(10);
  array.as_mutable_span().typed<int>()[9] = 7;
  EXPECT_EQ(array.size(), 10);
  EXPECT_EQ(array.as_span().typed<int>()[9], 7);
}

}  // namespace blender::tests
