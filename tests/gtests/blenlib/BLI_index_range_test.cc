/* Apache License, Version 2.0 */

#include "BLI_index_range.hh"
#include "BLI_strict_flags.h"
#include "BLI_vector.hh"
#include "testing/testing.h"

namespace blender {

TEST(index_range, DefaultConstructor)
{
  IndexRange range;
  EXPECT_EQ(range.size(), 0u);

  Vector<uint> vector;
  for (uint value : range) {
    vector.append(value);
  }
  EXPECT_EQ(vector.size(), 0u);
}

TEST(index_range, SingleElementRange)
{
  IndexRange range(4, 1);
  EXPECT_EQ(range.size(), 1u);
  EXPECT_EQ(*range.begin(), 4u);

  Vector<uint> vector;
  for (uint value : range) {
    vector.append(value);
  }

  EXPECT_EQ(vector.size(), 1u);
  EXPECT_EQ(vector[0], 4u);
}

TEST(index_range, MultipleElementRange)
{
  IndexRange range(6, 4);
  EXPECT_EQ(range.size(), 4u);

  Vector<uint> vector;
  for (uint value : range) {
    vector.append(value);
  }

  EXPECT_EQ(vector.size(), 4u);
  for (uint i = 0; i < 4; i++) {
    EXPECT_EQ(vector[i], i + 6);
  }
}

TEST(index_range, SubscriptOperator)
{
  IndexRange range(5, 5);
  EXPECT_EQ(range[0], 5u);
  EXPECT_EQ(range[1], 6u);
  EXPECT_EQ(range[2], 7u);
}

TEST(index_range, Before)
{
  IndexRange range = IndexRange(5, 5).before(3);
  EXPECT_EQ(range[0], 2u);
  EXPECT_EQ(range[1], 3u);
  EXPECT_EQ(range[2], 4u);
  EXPECT_EQ(range.size(), 3u);
}

TEST(index_range, After)
{
  IndexRange range = IndexRange(5, 5).after(4);
  EXPECT_EQ(range[0], 10u);
  EXPECT_EQ(range[1], 11u);
  EXPECT_EQ(range[2], 12u);
  EXPECT_EQ(range[3], 13u);
  EXPECT_EQ(range.size(), 4u);
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
  EXPECT_EQ(range.first(), 5u);
}

TEST(index_range, Last)
{
  IndexRange range = IndexRange(5, 3);
  EXPECT_EQ(range.last(), 7u);
}

TEST(index_range, OneAfterEnd)
{
  IndexRange range = IndexRange(5, 3);
  EXPECT_EQ(range.one_after_last(), 8u);
}

TEST(index_range, Start)
{
  IndexRange range = IndexRange(6, 2);
  EXPECT_EQ(range.start(), 6u);
}

TEST(index_range, Slice)
{
  IndexRange range = IndexRange(5, 15);
  IndexRange slice = range.slice(2, 6);
  EXPECT_EQ(slice.size(), 6u);
  EXPECT_EQ(slice.first(), 7u);
  EXPECT_EQ(slice.last(), 12u);
}

TEST(index_range, SliceRange)
{
  IndexRange range = IndexRange(5, 15);
  IndexRange slice = range.slice(IndexRange(3, 5));
  EXPECT_EQ(slice.size(), 5u);
  EXPECT_EQ(slice.first(), 8u);
  EXPECT_EQ(slice.last(), 12u);
}

TEST(index_range, AsSpan)
{
  IndexRange range = IndexRange(4, 6);
  Span<uint> span = range.as_span();
  EXPECT_EQ(span.size(), 6u);
  EXPECT_EQ(span[0], 4u);
  EXPECT_EQ(span[1], 5u);
  EXPECT_EQ(span[2], 6u);
  EXPECT_EQ(span[3], 7u);
}

}  // namespace blender
