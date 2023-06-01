/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_any.hh"
#include "BLI_map.hh"

#include "testing/testing.h"

namespace blender::tests {

TEST(any, DefaultConstructor)
{
  Any<> a;
  EXPECT_FALSE(a.has_value());
}

TEST(any, AssignInt)
{
  Any<> a = 5;
  EXPECT_TRUE(a.has_value());
  EXPECT_TRUE(a.is<int>());
  EXPECT_FALSE(a.is<float>());
  const int &value = a.get<int>();
  EXPECT_EQ(value, 5);
  a = 10;
  EXPECT_EQ(value, 10);

  Any<> b = a;
  EXPECT_TRUE(b.has_value());
  EXPECT_EQ(b.get<int>(), 10);

  Any<> c = std::move(a);
  EXPECT_TRUE(c);
  EXPECT_EQ(c.get<int>(), 10);

  EXPECT_EQ(a.get<int>(), 10); /* NOLINT: bugprone-use-after-move */

  a.reset();
  EXPECT_FALSE(a);
}

TEST(any, AssignMap)
{
  Any<> a = Map<int, int>();
  EXPECT_TRUE(a.has_value());
  EXPECT_TRUE((a.is<Map<int, int>>()));
  EXPECT_FALSE((a.is<Map<int, float>>()));
  Map<int, int> &map = a.get<Map<int, int>>();
  map.add(4, 2);
  EXPECT_EQ((a.get<Map<int, int>>().lookup(4)), 2);

  Any<> b = a;
  EXPECT_TRUE(b);
  EXPECT_EQ((b.get<Map<int, int>>().lookup(4)), 2);

  Any<> c = std::move(a);
  /* Test valid state after self assignment. Clang emits `-Wself-assign-overloaded` with `c=c;`.
   * And `pragma` suppression creates warnings on other compilers. */
  c = static_cast<decltype(a) &>(c);
  EXPECT_TRUE(c);
  EXPECT_EQ((c.get<Map<int, int>>().lookup(4)), 2);

  EXPECT_TRUE((a.get<Map<int, int>>().is_empty())); /* NOLINT: bugprone-use-after-move */
}

TEST(any, AssignAny)
{
  Any<> a = 5;
  Any<> b = std::string("hello");
  Any<> c;

  Any<> z;
  EXPECT_FALSE(z.has_value());

  z = a;
  EXPECT_TRUE(z.has_value());
  EXPECT_EQ(z.get<int>(), 5);

  z = b;
  EXPECT_EQ(z.get<std::string>(), "hello");

  z = c;
  EXPECT_FALSE(z.has_value());

  z = Any(std::in_place_type<Any<>>, a);
  EXPECT_FALSE(z.is<int>());
  EXPECT_TRUE(z.is<Any<>>());
  EXPECT_EQ(z.get<Any<>>().get<int>(), 5);
}

struct ExtraSizeInfo {
  size_t size;

  template<typename T> static constexpr ExtraSizeInfo get()
  {
    return {sizeof(T)};
  }
};

TEST(any, ExtraInfo)
{
  using MyAny = Any<ExtraSizeInfo>;

  MyAny a = 5;
  EXPECT_EQ(a.extra_info().size, sizeof(int));

  a = std::string("hello");
  EXPECT_EQ(a.extra_info().size, sizeof(std::string));
}

}  // namespace blender::tests
