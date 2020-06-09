#include "BLI_index_range.hh"
#include "BLI_strict_flags.h"
#include "BLI_vector.hh"
#include "testing/testing.h"

using namespace blender;

TEST(index_range, DefaultConstructor)
{
  IndexRange range;
  EXPECT_EQ(range.size(), 0);

  Vector<uint> vector;
  for (uint value : range) {
    vector.append(value);
  }
  EXPECT_EQ(vector.size(), 0);
}

TEST(index_range, SingleElementRange)
{
  IndexRange range(4, 1);
  EXPECT_EQ(range.size(), 1);
  EXPECT_EQ(*range.begin(), 4);

  Vector<uint> vector;
  for (uint value : range) {
    vector.append(value);
  }

  EXPECT_EQ(vector.size(), 1);
  EXPECT_EQ(vector[0], 4);
}

TEST(index_range, MultipleElementRange)
{
  IndexRange range(6, 4);
  EXPECT_EQ(range.size(), 4);

  Vector<uint> vector;
  for (uint value : range) {
    vector.append(value);
  }

  EXPECT_EQ(vector.size(), 4);
  for (uint i = 0; i < 4; i++) {
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

TEST(index_range, SliceRange)
{
  IndexRange range = IndexRange(5, 15);
  IndexRange slice = range.slice(IndexRange(3, 5));
  EXPECT_EQ(slice.size(), 5);
  EXPECT_EQ(slice.first(), 8);
  EXPECT_EQ(slice.last(), 12);
}

TEST(index_range, AsArrayRef)
{
  IndexRange range = IndexRange(4, 6);
  ArrayRef<uint> ref = range.as_array_ref();
  EXPECT_EQ(ref.size(), 6);
  EXPECT_EQ(ref[0], 4);
  EXPECT_EQ(ref[1], 5);
  EXPECT_EQ(ref[2], 6);
  EXPECT_EQ(ref[3], 7);
}
