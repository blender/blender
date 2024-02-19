/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_index_range.hh"
#include "BLI_strict_flags.h"
#include "BLI_vector.hh"
#include "testing/testing.h"

namespace blender::tests {

TEST(index_range, DefaultConstructor)
{
  IndexRange range;
  EXPECT_EQ(range.size(), 0);

  Vector<int64_t> vector;
  for (int64_t value : range) {
    vector.append(value);
  }
  EXPECT_EQ(vector.size(), 0);
}

TEST(index_range, SingleElementRange)
{
  IndexRange range(4, 1);
  EXPECT_EQ(range.size(), 1);
  EXPECT_EQ(*range.begin(), 4);

  Vector<int64_t> vector;
  for (int64_t value : range) {
    vector.append(value);
  }

  EXPECT_EQ(vector.size(), 1);
  EXPECT_EQ(vector[0], 4);
}

TEST(index_range, MultipleElementRange)
{
  IndexRange range(6, 4);
  EXPECT_EQ(range.size(), 4);

  Vector<int64_t> vector;
  for (int64_t value : range) {
    vector.append(value);
  }

  EXPECT_EQ(vector.size(), 4);
  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(vector[i], i + 6);
  }
}

TEST(index_range, SubscriptOperator)
{
  IndexRange range(5, 5);
  EXPECT_EQ(range[0], 5);
  EXPECT_EQ(range[1], 6);
  EXPECT_EQ(range[2], 7);
}

TEST(index_range, Before)
{
  IndexRange range = IndexRange(5, 5).before(3);
  EXPECT_EQ(range[0], 2);
  EXPECT_EQ(range[1], 3);
  EXPECT_EQ(range[2], 4);
  EXPECT_EQ(range.size(), 3);
}

TEST(index_range, After)
{
  IndexRange range = IndexRange(5, 5).after(4);
  EXPECT_EQ(range[0], 10);
  EXPECT_EQ(range[1], 11);
  EXPECT_EQ(range[2], 12);
  EXPECT_EQ(range[3], 13);
  EXPECT_EQ(range.size(), 4);
}

TEST(index_range, Contains)
{
  IndexRange range = IndexRange(5, 3);
  EXPECT_TRUE(range.contains(5));
  EXPECT_TRUE(range.contains(6));
  EXPECT_TRUE(range.contains(7));
  EXPECT_FALSE(range.contains(4));
  EXPECT_FALSE(range.contains(8));
}

TEST(index_range, First)
{
  IndexRange range = IndexRange(5, 3);
  EXPECT_EQ(range.first(), 5);
}

TEST(index_range, Last)
{
  IndexRange range = IndexRange(5, 3);
  EXPECT_EQ(range.last(), 7);
}

TEST(index_range, OneAfterEnd)
{
  IndexRange range = IndexRange(5, 3);
  EXPECT_EQ(range.one_after_last(), 8);
}

TEST(index_range, OneBeforeStart)
{
  IndexRange range = IndexRange(5, 3);
  EXPECT_EQ(range.one_before_start(), 4);
}

TEST(index_range, Start)
{
  IndexRange range = IndexRange(6, 2);
  EXPECT_EQ(range.start(), 6);
}

TEST(index_range, Slice)
{
  IndexRange range = IndexRange(5, 15);
  IndexRange slice = range.slice(2, 6);
  EXPECT_EQ(slice.size(), 6);
  EXPECT_EQ(slice.first(), 7);
  EXPECT_EQ(slice.last(), 12);
}

TEST(index_range, Intersect)
{
  IndexRange range = IndexRange(5, 15);
  EXPECT_EQ(range.intersect(IndexRange(2, 2)), IndexRange(5, 0));
  EXPECT_EQ(range.intersect(IndexRange(4, 2)), IndexRange(5, 1));
  EXPECT_EQ(range.intersect(IndexRange(3, 20)), IndexRange(5, 15));
  EXPECT_EQ(range.intersect(IndexRange(5, 15)), IndexRange(5, 15));
  EXPECT_EQ(range.intersect(IndexRange(15, 10)), IndexRange(15, 5));
  EXPECT_EQ(range.intersect(IndexRange(22, 2)), IndexRange(20, 0));
}

TEST(index_range, SliceRange)
{
  IndexRange range = IndexRange(5, 15);
  IndexRange slice = range.slice(IndexRange(3, 5));
  EXPECT_EQ(slice.size(), 5);
  EXPECT_EQ(slice.first(), 8);
  EXPECT_EQ(slice.last(), 12);
}

TEST(index_range, DropBack)
{
  IndexRange a(4, 4);
  auto slice = a.drop_back(2);
  EXPECT_EQ(slice.size(), 2);
  EXPECT_EQ(slice.start(), 4);
  EXPECT_EQ(slice[1], 5);
}

TEST(index_range, DropBackAll)
{
  IndexRange a(4, 4);
  auto slice = a.drop_back(a.size());
  EXPECT_TRUE(slice.is_empty());
}

TEST(index_range, DropFront)
{
  IndexRange a(4, 4);
  auto slice = a.drop_front(1);
  EXPECT_EQ(slice.size(), 3);
  EXPECT_EQ(slice[0], 5);
  EXPECT_EQ(slice[1], 6);
  EXPECT_EQ(slice.last(), 7);
}

TEST(index_range, DropFrontLargeN)
{
  IndexRange a(1, 5);
  IndexRange slice = a.drop_front(100);
  EXPECT_TRUE(slice.is_empty());
}

TEST(index_range, DropFrontAll)
{
  IndexRange a(50);
  IndexRange slice = a.drop_front(a.size());
  EXPECT_TRUE(slice.is_empty());
}

TEST(index_range, TakeFront)
{
  IndexRange a(4, 4);
  IndexRange slice = a.take_front(2);
  EXPECT_EQ(slice.size(), 2);
  EXPECT_EQ(slice[0], 4);
  EXPECT_EQ(slice[1], 5);
}

TEST(index_range, TakeFrontLargeN)
{
  IndexRange a(4, 4);
  IndexRange slice = a.take_front(100);
  EXPECT_EQ(slice.size(), 4);
}

TEST(index_range, TakeBack)
{
  IndexRange a(4, 4);
  auto slice = a.take_back(2);
  EXPECT_EQ(slice.size(), 2);
  EXPECT_EQ(slice[0], 6);
  EXPECT_EQ(slice[1], 7);
}

TEST(index_range, TakeBackLargeN)
{
  IndexRange a(3, 4);
  IndexRange slice = a.take_back(100);
  EXPECT_EQ(slice.size(), 4);
  EXPECT_EQ(slice.size(), 4);
}

TEST(index_range, constexpr_)
{
  constexpr IndexRange range = IndexRange(1, 1);
  std::array<int, range[0]> compiles = {1};
  BLI_STATIC_ASSERT(range.size() == 1, "");
  EXPECT_EQ(compiles[0], 1);
}

TEST(index_range, GenericAlgorithms)
{
  IndexRange range{4, 10};
  EXPECT_TRUE(std::any_of(range.begin(), range.end(), [](int v) { return v == 6; }));
  EXPECT_FALSE(std::any_of(range.begin(), range.end(), [](int v) { return v == 20; }));
  EXPECT_EQ(std::count_if(range.begin(), range.end(), [](int v) { return v < 7; }), 3);
}

TEST(index_range, SplitByAlignment)
{
  {
    AlignedIndexRanges ranges = split_index_range_by_alignment(IndexRange(0, 0), 16);
    EXPECT_EQ(ranges.prefix, IndexRange());
    EXPECT_EQ(ranges.aligned, IndexRange());
    EXPECT_EQ(ranges.suffix, IndexRange());
  }
  {
    AlignedIndexRanges ranges = split_index_range_by_alignment(IndexRange(0, 24), 8);
    EXPECT_EQ(ranges.prefix, IndexRange());
    EXPECT_EQ(ranges.aligned, IndexRange(0, 24));
    EXPECT_EQ(ranges.suffix, IndexRange());
  }
  {
    AlignedIndexRanges ranges = split_index_range_by_alignment(IndexRange(1, 2), 4);
    EXPECT_EQ(ranges.prefix, IndexRange(1, 2));
    EXPECT_EQ(ranges.aligned, IndexRange());
    EXPECT_EQ(ranges.suffix, IndexRange());
  }
  {
    AlignedIndexRanges ranges = split_index_range_by_alignment(IndexRange(3, 50), 8);
    EXPECT_EQ(ranges.prefix, IndexRange(3, 5));
    EXPECT_EQ(ranges.aligned, IndexRange(8, 40));
    EXPECT_EQ(ranges.suffix, IndexRange(48, 5));
  }
  {
    AlignedIndexRanges ranges = split_index_range_by_alignment(IndexRange(3, 50), 1);
    EXPECT_EQ(ranges.prefix, IndexRange());
    EXPECT_EQ(ranges.aligned, IndexRange(3, 50));
    EXPECT_EQ(ranges.suffix, IndexRange());
  }
  {
    AlignedIndexRanges ranges = split_index_range_by_alignment(IndexRange(64, 16), 16);
    EXPECT_EQ(ranges.prefix, IndexRange());
    EXPECT_EQ(ranges.aligned, IndexRange(64, 16));
    EXPECT_EQ(ranges.suffix, IndexRange());
  }
  {
    AlignedIndexRanges ranges = split_index_range_by_alignment(IndexRange(3, 5), 8);
    EXPECT_EQ(ranges.prefix, IndexRange(3, 5));
    EXPECT_EQ(ranges.aligned, IndexRange());
    EXPECT_EQ(ranges.suffix, IndexRange());
  }
  {
    AlignedIndexRanges ranges = split_index_range_by_alignment(IndexRange(64), 64);
    EXPECT_EQ(ranges.prefix, IndexRange());
    EXPECT_EQ(ranges.aligned, IndexRange(64));
    EXPECT_EQ(ranges.suffix, IndexRange());
  }
  {
    AlignedIndexRanges ranges = split_index_range_by_alignment(IndexRange(64, 64), 64);
    EXPECT_EQ(ranges.prefix, IndexRange());
    EXPECT_EQ(ranges.aligned, IndexRange(64, 64));
    EXPECT_EQ(ranges.suffix, IndexRange());
  }
  {
    AlignedIndexRanges ranges = split_index_range_by_alignment(IndexRange(4, 8), 64);
    EXPECT_EQ(ranges.prefix, IndexRange(4, 8));
    EXPECT_EQ(ranges.aligned, IndexRange());
    EXPECT_EQ(ranges.suffix, IndexRange());
  }
}

}  // namespace blender::tests
