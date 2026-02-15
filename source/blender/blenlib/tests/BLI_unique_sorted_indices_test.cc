/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_array.hh"
#include "BLI_unique_sorted_indices.hh"

#include "testing/testing.h"

namespace blender::unique_sorted_indices::tests {

TEST(unique_sorted_indices, FindRangeEnd)
{
  EXPECT_EQ(find_size_of_next_range<int>({4}), 1);
  EXPECT_EQ(find_size_of_next_range<int>({4, 5, 6, 7}), 4);
  EXPECT_EQ(find_size_of_next_range<int>({4, 5, 6, 8, 9}), 3);
}

TEST(unique_sorted_indices, NonEmptyIsRange)
{
  EXPECT_TRUE(non_empty_is_range<int>({0, 1, 2}));
  EXPECT_TRUE(non_empty_is_range<int>({5}));
  EXPECT_TRUE(non_empty_is_range<int>({7, 8, 9, 10}));
  EXPECT_FALSE(non_empty_is_range<int>({3, 5}));
  EXPECT_FALSE(non_empty_is_range<int>({3, 4, 5, 6, 8, 9}));
}

TEST(unique_sorted_indices, NonEmptyAsRange)
{
  EXPECT_EQ(non_empty_as_range<int>({0, 1, 2}), IndexRange(0, 3));
  EXPECT_EQ(non_empty_as_range<int>({5}), IndexRange(5, 1));
  EXPECT_EQ(non_empty_as_range<int>({10, 11}), IndexRange(10, 2));
}

TEST(unique_sorted_indices, FindSizeOfNextRange)
{
  EXPECT_EQ(find_size_of_next_range<int>({0, 3, 4}), 1);
  EXPECT_EQ(find_size_of_next_range<int>({4, 5, 6, 7}), 4);
  EXPECT_EQ(find_size_of_next_range<int>({4}), 1);
  EXPECT_EQ(find_size_of_next_range<int>({5, 6, 7, 10, 11, 100}), 3);
}

TEST(unique_sorted_indices, FindStartOfNextRange)
{
  EXPECT_EQ(find_size_until_next_range<int>({4}, 3), 1);
  EXPECT_EQ(find_size_until_next_range<int>({4, 5}, 3), 2);
  EXPECT_EQ(find_size_until_next_range<int>({4, 5, 6}, 3), 0);
  EXPECT_EQ(find_size_until_next_range<int>({4, 5, 6, 7}, 3), 0);
  EXPECT_EQ(find_size_until_next_range<int>({0, 1, 3, 5, 10, 11, 12, 20}, 3), 4);
}

TEST(unique_sorted_indices, SplitToRangesAndSpans)
{
  Array<int> data = {1, 2, 3, 4, 7, 9, 10, 13, 14, 15, 20, 21, 22, 23, 24};
  Vector<std::variant<IndexRange, Span<int>>> parts;
  const int64_t parts_num = split_to_ranges_and_spans<int>(data, 3, parts);

  EXPECT_EQ(parts_num, 4);
  EXPECT_EQ(parts.size(), 4);
  EXPECT_EQ(std::get<IndexRange>(parts[0]), IndexRange(1, 4));
  EXPECT_EQ(std::get<Span<int>>(parts[1]), Span<int>({7, 9, 10}));
  EXPECT_EQ(std::get<IndexRange>(parts[2]), IndexRange(13, 3));
  EXPECT_EQ(std::get<IndexRange>(parts[3]), IndexRange(20, 5));
}

TEST(unique_sorted_indices, FindContentRange)
{
  {
    Vector<int> data = {1, 2, 3, 4, 5};
    EXPECT_EQ(find_content_range<int>(data, IndexRange()), IndexRange());
    EXPECT_EQ(find_content_range<int>(data, IndexRange::from_begin_end_inclusive(2, 3)),
              IndexRange::from_begin_end_inclusive(1, 2));
    EXPECT_EQ(find_content_range<int>(data, IndexRange(100)), data.index_range());
  }
  {
    Vector<int> data = {5, 100, 101, 1000};
    EXPECT_EQ(find_content_range<int>(data, IndexRange::from_begin_end_inclusive(0, 4)),
              IndexRange());
    EXPECT_EQ(find_content_range<int>(data, IndexRange::from_begin_end_inclusive(0, 5)),
              IndexRange::from_begin_end_inclusive(0, 0));
    EXPECT_EQ(find_content_range<int>(data, IndexRange::from_begin_end_inclusive(5, 100)),
              IndexRange::from_begin_end_inclusive(0, 1));
    EXPECT_EQ(find_content_range<int>(data, IndexRange::from_begin_end_inclusive(101, 1000)),
              IndexRange::from_begin_end_inclusive(2, 3));
    EXPECT_EQ(find_content_range<int>(data, IndexRange(10000, 5)), IndexRange());
  }
}

}  // namespace blender::unique_sorted_indices::tests
