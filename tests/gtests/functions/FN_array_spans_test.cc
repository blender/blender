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

#include "FN_array_spans.hh"
#include "FN_cpp_types.hh"

namespace blender {
namespace fn {

TEST(virtual_array_span, EmptyConstructor)
{
  VArraySpan<int> span;
  EXPECT_EQ(span.size(), 0);
  EXPECT_TRUE(span.is_empty());
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
}

TEST(generic_virtual_array_span, TypeConstructor)
{
  GVArraySpan span{CPPType_int32};
  EXPECT_EQ(span.size(), 0);
  EXPECT_TRUE(span.is_empty());
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
}

}  // namespace fn
}  // namespace blender
