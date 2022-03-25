/* SPDX-License-Identifier: Apache-2.0 */

#include "BLI_index_mask.hh"
#include "testing/testing.h"

namespace blender::tests {

TEST(index_mask, DefaultConstructor)
{
  IndexMask mask;
  EXPECT_EQ(mask.min_array_size(), 0);
  EXPECT_EQ(mask.size(), 0);
}

TEST(index_mask, ArrayConstructor)
{
  [](IndexMask mask) {
    EXPECT_EQ(mask.size(), 4);
    EXPECT_EQ(mask.min_array_size(), 8);
    EXPECT_FALSE(mask.is_range());
    EXPECT_EQ(mask[0], 3);
    EXPECT_EQ(mask[1], 5);
    EXPECT_EQ(mask[2], 6);
    EXPECT_EQ(mask[3], 7);
  }({3, 5, 6, 7});
}

TEST(index_mask, RangeConstructor)
{
  IndexMask mask = IndexRange(3, 5);
  EXPECT_EQ(mask.size(), 5);
  EXPECT_EQ(mask.min_array_size(), 8);
  EXPECT_EQ(mask.last(), 7);
  EXPECT_TRUE(mask.is_range());
  EXPECT_EQ(mask.as_range().first(), 3);
  EXPECT_EQ(mask.as_range().last(), 7);
  Span<int64_t> indices = mask.indices();
  EXPECT_EQ(indices[0], 3);
  EXPECT_EQ(indices[1], 4);
  EXPECT_EQ(indices[2], 5);
}

TEST(index_mask, SliceAndOffset)
{
  Vector<int64_t> indices;
  {
    IndexMask mask{IndexRange(10)};
    IndexMask new_mask = mask.slice_and_offset(IndexRange(3, 5), indices);
    EXPECT_TRUE(new_mask.is_range());
    EXPECT_EQ(new_mask.size(), 5);
    EXPECT_EQ(new_mask[0], 0);
    EXPECT_EQ(new_mask[1], 1);
  }
  {
    Vector<int64_t> original_indices = {2, 3, 5, 7, 8, 9, 10};
    IndexMask mask{original_indices.as_span()};
    IndexMask new_mask = mask.slice_and_offset(IndexRange(1, 4), indices);
    EXPECT_FALSE(new_mask.is_range());
    EXPECT_EQ(new_mask.size(), 4);
    EXPECT_EQ(new_mask[0], 0);
    EXPECT_EQ(new_mask[1], 2);
    EXPECT_EQ(new_mask[2], 4);
    EXPECT_EQ(new_mask[3], 5);
  }
}

TEST(index_mask, ExtractRanges)
{
  {
    Vector<int64_t> indices = {1, 2, 3, 5, 7, 8};
    Vector<IndexRange> ranges = IndexMask(indices).extract_ranges();
    EXPECT_EQ(ranges.size(), 3);
    EXPECT_EQ(ranges[0], IndexRange(1, 3));
    EXPECT_EQ(ranges[1], IndexRange(5, 1));
    EXPECT_EQ(ranges[2], IndexRange(7, 2));
  }
  {
    Vector<int64_t> indices;
    Vector<IndexRange> ranges = IndexMask(indices).extract_ranges();
    EXPECT_EQ(ranges.size(), 0);
  }
  {
    Vector<int64_t> indices = {5, 6, 7, 8, 9, 10};
    Vector<IndexRange> ranges = IndexMask(indices).extract_ranges();
    EXPECT_EQ(ranges.size(), 1);
    EXPECT_EQ(ranges[0], IndexRange(5, 6));
  }
  {
    Vector<int64_t> indices = {1, 3, 6, 8};
    Vector<IndexRange> ranges = IndexMask(indices).extract_ranges();
    EXPECT_EQ(ranges.size(), 4);
    EXPECT_EQ(ranges[0], IndexRange(1, 1));
    EXPECT_EQ(ranges[1], IndexRange(3, 1));
    EXPECT_EQ(ranges[2], IndexRange(6, 1));
    EXPECT_EQ(ranges[3], IndexRange(8, 1));
  }
  {
    Vector<int64_t> indices;
    IndexRange range1{4, 10};
    IndexRange range2{20, 30};
    IndexRange range3{100, 1};
    IndexRange range4{150, 100};
    for (const IndexRange &range : {range1, range2, range3, range4}) {
      for (const int64_t i : range) {
        indices.append(i);
      }
    }
    Vector<IndexRange> ranges = IndexMask(indices).extract_ranges();
    EXPECT_EQ(ranges.size(), 4);
    EXPECT_EQ(ranges[0], range1);
    EXPECT_EQ(ranges[1], range2);
    EXPECT_EQ(ranges[2], range3);
    EXPECT_EQ(ranges[3], range4);
  }
  {
    const int64_t max_test_range_size = 50;
    Vector<int64_t> indices;
    int64_t offset = 0;
    for (const int64_t range_size : IndexRange(1, max_test_range_size)) {
      for (const int i : IndexRange(range_size)) {
        indices.append(offset + i);
      }
      offset += range_size + 1;
    }
    Vector<IndexRange> ranges = IndexMask(indices).extract_ranges();
    EXPECT_EQ(ranges.size(), max_test_range_size);
    for (const int64_t range_size : IndexRange(1, max_test_range_size)) {
      const IndexRange range = ranges[range_size - 1];
      EXPECT_EQ(range.size(), range_size);
    }
  }
}

TEST(index_mask, Invert)
{
  {
    Vector<int64_t> indices;
    Vector<int64_t> new_indices;
    IndexMask inverted_mask = IndexMask(indices).invert(IndexRange(10), new_indices);
    EXPECT_EQ(inverted_mask.size(), 10);
    EXPECT_TRUE(new_indices.is_empty());
  }
  {
    Vector<int64_t> indices = {3, 4, 5, 6};
    Vector<int64_t> new_indices;
    IndexMask inverted_mask = IndexMask(indices).invert(IndexRange(3, 4), new_indices);
    EXPECT_TRUE(inverted_mask.is_empty());
  }
  {
    Vector<int64_t> indices = {5};
    Vector<int64_t> new_indices;
    IndexMask inverted_mask = IndexMask(indices).invert(IndexRange(10), new_indices);
    EXPECT_EQ(inverted_mask.size(), 9);
    EXPECT_EQ(inverted_mask.indices(), Span<int64_t>({0, 1, 2, 3, 4, 6, 7, 8, 9}));
  }
  {
    Vector<int64_t> indices = {0, 1, 2, 6, 7, 9};
    Vector<int64_t> new_indices;
    IndexMask inverted_mask = IndexMask(indices).invert(IndexRange(10), new_indices);
    EXPECT_EQ(inverted_mask.size(), 4);
    EXPECT_EQ(inverted_mask.indices(), Span<int64_t>({3, 4, 5, 8}));
  }
}

TEST(index_mask, ExtractRangesInvert)
{
  {
    Vector<int64_t> indices;
    Vector<IndexRange> ranges = IndexMask(indices).extract_ranges_invert(IndexRange(10), nullptr);
    EXPECT_EQ(ranges.size(), 1);
    EXPECT_EQ(ranges[0], IndexRange(10));
  }
  {
    Vector<int64_t> indices = {1, 2, 3, 6, 7};
    Vector<int64_t> skip_amounts;
    Vector<IndexRange> ranges = IndexMask(indices).extract_ranges_invert(IndexRange(10),
                                                                         &skip_amounts);
    EXPECT_EQ(ranges.size(), 3);
    EXPECT_EQ(ranges[0], IndexRange(0, 1));
    EXPECT_EQ(ranges[1], IndexRange(4, 2));
    EXPECT_EQ(ranges[2], IndexRange(8, 2));
    EXPECT_EQ(skip_amounts[0], 0);
    EXPECT_EQ(skip_amounts[1], 3);
    EXPECT_EQ(skip_amounts[2], 5);
  }
  {
    Vector<int64_t> indices = {0, 1, 2, 3, 4};
    Vector<int64_t> skip_amounts;
    Vector<IndexRange> ranges = IndexMask(indices).extract_ranges_invert(IndexRange(5),
                                                                         &skip_amounts);
    EXPECT_TRUE(ranges.is_empty());
    EXPECT_TRUE(skip_amounts.is_empty());
  }
  {
    Vector<int64_t> indices = {5, 6, 7, 10, 11};
    Vector<int64_t> skip_amounts;
    Vector<IndexRange> ranges = IndexMask(indices).extract_ranges_invert(IndexRange(5, 20),
                                                                         &skip_amounts);
    EXPECT_EQ(ranges.size(), 2);
    EXPECT_EQ(ranges[0], IndexRange(8, 2));
    EXPECT_EQ(ranges[1], IndexRange(12, 13));
    EXPECT_EQ(skip_amounts[0], 3);
    EXPECT_EQ(skip_amounts[1], 5);
  }
}

TEST(index_mask, ContainedIn)
{
  EXPECT_TRUE(IndexMask({3, 4, 5}).contained_in(IndexRange(10)));
  EXPECT_TRUE(IndexMask().contained_in(IndexRange(5, 0)));
  EXPECT_FALSE(IndexMask({3}).contained_in(IndexRange(3)));
  EXPECT_FALSE(IndexMask({4, 5, 6}).contained_in(IndexRange(5, 10)));
  EXPECT_FALSE(IndexMask({5, 6}).contained_in(IndexRange()));
}

}  // namespace blender::tests
