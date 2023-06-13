/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_multi_value_map.hh"
#include "BLI_vector.hh"
#include "testing/testing.h"

namespace blender::tests {

TEST(multi_value_map, LookupNotExistant)
{
  MultiValueMap<int, int> map;
  EXPECT_EQ(map.lookup(5).size(), 0);
  map.add(2, 5);
  EXPECT_EQ(map.lookup(5).size(), 0);
}

TEST(multi_value_map, LookupExistant)
{
  MultiValueMap<int, int> map;
  map.add(2, 4);
  map.add(2, 5);
  map.add(3, 6);

  EXPECT_EQ(map.lookup(2).size(), 2);
  EXPECT_EQ(map.lookup(2)[0], 4);
  EXPECT_EQ(map.lookup(2)[1], 5);

  EXPECT_EQ(map.lookup(3).size(), 1);
  EXPECT_EQ(map.lookup(3)[0], 6);
}

TEST(multi_value_map, LookupMutable)
{
  MultiValueMap<int, int> map;
  map.add(1, 2);
  map.add(4, 5);
  map.add(4, 6);
  map.add(6, 7);

  MutableSpan<int> span = map.lookup(4);
  EXPECT_EQ(span.size(), 2);
  span[0] = 10;
  span[1] = 20;

  map.add(4, 5);
  MutableSpan<int> new_span = map.lookup(4);
  EXPECT_EQ(new_span.size(), 3);
  EXPECT_EQ(new_span[0], 10);
  EXPECT_EQ(new_span[1], 20);
  EXPECT_EQ(new_span[2], 5);
}

TEST(multi_value_map, AddMultiple)
{
  MultiValueMap<int, int> map;
  map.add_multiple(2, {4, 5, 6});
  map.add_multiple(2, {1, 2});
  map.add_multiple(5, {7, 5, 3});

  EXPECT_EQ(map.lookup(2).size(), 5);
  EXPECT_EQ(map.lookup(2)[0], 4);
  EXPECT_EQ(map.lookup(2)[1], 5);
  EXPECT_EQ(map.lookup(2)[2], 6);
  EXPECT_EQ(map.lookup(2)[3], 1);
  EXPECT_EQ(map.lookup(2)[4], 2);

  EXPECT_EQ(map.lookup(5).size(), 3);
  EXPECT_EQ(map.lookup(5)[0], 7);
  EXPECT_EQ(map.lookup(5)[1], 5);
  EXPECT_EQ(map.lookup(5)[2], 3);
}

TEST(multi_value_map, Keys)
{
  MultiValueMap<int, int> map;
  map.add(5, 7);
  map.add(5, 7);
  map.add_multiple(2, {6, 7, 8});

  Vector<int> keys;
  for (int key : map.keys()) {
    keys.append(key);
  }

  EXPECT_EQ(keys.size(), 2);
  EXPECT_TRUE(keys.contains(5));
  EXPECT_TRUE(keys.contains(2));
}

TEST(multi_value_map, Values)
{
  MultiValueMap<int, int> map;
  map.add(3, 5);
  map.add_multiple(3, {1, 2});
  map.add(6, 1);

  Vector<Span<int>> values;
  for (Span<int> value_span : map.values()) {
    values.append(value_span);
  }

  EXPECT_EQ(values.size(), 2);
}

TEST(multi_value_map, Items)
{
  MultiValueMap<int, int> map;
  map.add_multiple(4, {1, 2, 3});

  for (auto &&item : map.items()) {
    int key = item.key;
    Span<int> values = item.value;
    EXPECT_EQ(key, 4);
    EXPECT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
  }
}

TEST(multi_value_map, UniquePtr)
{
  /* Mostly testing if it compiles here. */
  MultiValueMap<std::unique_ptr<int>, std::unique_ptr<int>> map;
  map.add(std::make_unique<int>(4), std::make_unique<int>(6));
  map.add(std::make_unique<int>(4), std::make_unique<int>(7));
  EXPECT_EQ(map.lookup(std::make_unique<int>(10)).size(), 0);
}

}  // namespace blender::tests
