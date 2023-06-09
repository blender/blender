/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_binary_search.hh"
#include "BLI_vector.hh"

#include "testing/testing.h"

namespace blender::binary_search::tests {

TEST(binary_search, Empty)
{
  const Vector<int> vec;
  const int64_t index = find_predicate_begin(vec, [](const int /*value*/) { return true; });
  EXPECT_EQ(index, 0);
}

TEST(binary_search, One)
{
  const Vector<int> vec = {5};
  {
    const int64_t index = find_predicate_begin(vec, [](const int /*value*/) { return false; });
    EXPECT_EQ(index, 1);
  }
  {
    const int64_t index = find_predicate_begin(vec, [](const int /*value*/) { return true; });
    EXPECT_EQ(index, 0);
  }
}

TEST(binary_search, Multiple)
{
  const Vector<int> vec{4, 5, 7, 9, 10, 20, 30};
  {
    const int64_t index = find_predicate_begin(vec, [](const int value) { return value > 0; });
    EXPECT_EQ(index, 0);
  }
  {
    const int64_t index = find_predicate_begin(vec, [](const int value) { return value > 4; });
    EXPECT_EQ(index, 1);
  }
  {
    const int64_t index = find_predicate_begin(vec, [](const int value) { return value > 10; });
    EXPECT_EQ(index, 5);
  }
  {
    const int64_t index = find_predicate_begin(vec, [](const int value) { return value >= 25; });
    EXPECT_EQ(index, 6);
  }
  {
    const int64_t index = find_predicate_begin(vec, [](const int value) { return value >= 30; });
    EXPECT_EQ(index, 6);
  }
  {
    const int64_t index = find_predicate_begin(vec, [](const int value) { return value > 30; });
    EXPECT_EQ(index, 7);
  }
}

}  // namespace blender::binary_search::tests
