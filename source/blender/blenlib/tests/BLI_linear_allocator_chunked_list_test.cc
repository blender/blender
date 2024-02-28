/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include <iostream>

#include "BLI_linear_allocator_chunked_list.hh"
#include "BLI_set.hh"

#include "BLI_strict_flags.h" /* Keep last. */

namespace blender::linear_allocator::tests {

TEST(LinearAllocator_ChunkedList, Append)
{
  LinearAllocator<> allocator;
  ChunkedList<std::string> list;

  list.append(allocator, "1");
  list.append(allocator, "2");
  list.append(allocator, "this_is_an_extra_long_string");

  Set<std::string> retrieved_values;
  for (const std::string &value : const_cast<const ChunkedList<std::string> &>(list)) {
    retrieved_values.add(value);
  }
  EXPECT_EQ(retrieved_values.size(), 3);
  EXPECT_TRUE(retrieved_values.contains("1"));
  EXPECT_TRUE(retrieved_values.contains("2"));
  EXPECT_TRUE(retrieved_values.contains("this_is_an_extra_long_string"));
}

TEST(LinearAllocator_ChunkedList, AppendMany)
{
  LinearAllocator<> allocator;
  ChunkedList<int> list;

  for (const int64_t i : IndexRange(10000)) {
    list.append(allocator, int(i));
  }

  Set<int> values;
  for (const int value : list) {
    values.add(value);
  }

  EXPECT_EQ(values.size(), 10000);
}

TEST(LinearAllocator_ChunkedList, Move)
{
  LinearAllocator<> allocator;
  ChunkedList<int> a;
  a.append(allocator, 1);
  ChunkedList<int> b = std::move(a);

  a.append(allocator, 2);
  b.append(allocator, 3);

  {
    Set<int> a_values;
    for (const int value : a) {
      a_values.add(value);
    }
    Set<int> b_values;
    for (const int value : b) {
      b_values.add(value);
    }

    EXPECT_EQ(a_values.size(), 1);
    EXPECT_TRUE(a_values.contains(2));

    EXPECT_EQ(b_values.size(), 2);
    EXPECT_TRUE(b_values.contains(1));
    EXPECT_TRUE(b_values.contains(3));
  }

  a = std::move(b);
  /* Want to test self-move. Using std::move twice quiets a compiler warning. */
  a = std::move(std::move(a));

  {
    Set<int> a_values;
    for (const int value : a) {
      a_values.add(value);
    }
    Set<int> b_values;
    for (const int value : b) {
      b_values.add(value);
    }

    EXPECT_EQ(a_values.size(), 2);
    EXPECT_TRUE(a_values.contains(1));
    EXPECT_TRUE(a_values.contains(3));

    EXPECT_TRUE(b_values.is_empty());
  }
}

}  // namespace blender::linear_allocator::tests
