#include "BLI_index_mask.hh"
#include "testing/testing.h"

using namespace blender;

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
  ArrayRef<uint> indices = mask.indices();
  EXPECT_EQ(indices[0], 3);
  EXPECT_EQ(indices[1], 4);
  EXPECT_EQ(indices[2], 5);
}
