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
#include "FN_spans.hh"

namespace blender {
namespace fn {

TEST(generic_span, TypeConstructor)
{
  GSpan span(CPPType_float);
  EXPECT_EQ(span.size(), 0);
  EXPECT_EQ(span.typed<float>().size(), 0);
  EXPECT_TRUE(span.is_empty());
}

TEST(generic_span, BufferAndSizeConstructor)
{
  int values[4] = {6, 7, 3, 2};
  void *buffer = (void *)values;
  GSpan span(CPPType_int32, buffer, 4);
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
  GMutableSpan span(CPPType_int32);
  EXPECT_EQ(span.size(), 0);
  EXPECT_TRUE(span.is_empty());
}

TEST(generic_mutable_span, BufferAndSizeConstructor)
{
  int values[4] = {4, 7, 3, 5};
  void *buffer = (void *)values;
  GMutableSpan span(CPPType_int32, buffer, 4);
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
}

TEST(generic_virtual_span, TypeConstructor)
{
  GVSpan span(CPPType_int32);
  EXPECT_EQ(span.size(), 0);
  EXPECT_TRUE(span.is_empty());
}

TEST(generic_virtual_span, GenericSpanConstructor)
{
  int values[4] = {3, 4, 5, 6};
  GVSpan span{GSpan(CPPType_int32, values, 4)};
  EXPECT_EQ(span.size(), 4);
  EXPECT_FALSE(span.is_empty());
  EXPECT_EQ(span[0], &values[0]);
  EXPECT_EQ(span[1], &values[1]);
  EXPECT_EQ(span[2], &values[2]);
  EXPECT_EQ(span[3], &values[3]);
}

TEST(generic_virtual_span, SpanConstructor)
{
  std::array<int, 3> values = {6, 7, 8};
  GVSpan span{Span<int>(values)};
  EXPECT_EQ(span.type(), CPPType_int32);
  EXPECT_EQ(span.size(), 3);
  EXPECT_EQ(span[0], &values[0]);
  EXPECT_EQ(span[1], &values[1]);
  EXPECT_EQ(span[2], &values[2]);
}

TEST(generic_virtual_span, SingleConstructor)
{
  int value = 5;
  GVSpan span = GVSpan::FromSingle(CPPType_int32, &value, 3);
  EXPECT_EQ(span.size(), 3);
  EXPECT_FALSE(span.is_empty());
  EXPECT_EQ(span[0], &value);
  EXPECT_EQ(span[1], &value);
  EXPECT_EQ(span[2], &value);
}

}  // namespace fn
}  // namespace blender
