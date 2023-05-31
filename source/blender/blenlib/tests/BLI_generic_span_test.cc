/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_generic_span.hh"

namespace blender::tests {

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

}  // namespace blender::tests
