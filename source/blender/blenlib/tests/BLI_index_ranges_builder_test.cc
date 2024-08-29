/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_index_ranges_builder.hh"

namespace blender::tests {

TEST(index_ranges_builder, Empty)
{
  IndexRangesBuilderBuffer<int, 10> builder_buffer;
  IndexRangesBuilder<int> builder{builder_buffer};
  EXPECT_EQ(builder.size(), 0);
  EXPECT_TRUE(builder.is_empty());
}

TEST(index_ranges_builder, Single)
{
  {
    IndexRangesBuilderBuffer<int, 10> builder_buffer;
    IndexRangesBuilder<int> builder{builder_buffer};
    builder.add(0);
    EXPECT_EQ(builder.size(), 1);
    EXPECT_EQ(builder[0], IndexRange::from_begin_size(0, 1));
  }
  {
    IndexRangesBuilderBuffer<int, 10> builder_buffer;
    IndexRangesBuilder<int> builder{builder_buffer};
    builder.add(10);
    EXPECT_EQ(builder.size(), 1);
    EXPECT_EQ(builder[0], IndexRange::from_begin_size(10, 1));
  }
}

TEST(index_ranges_builder, Multiple)
{
  IndexRangesBuilderBuffer<int, 10> builder_buffer;
  IndexRangesBuilder<int> builder{builder_buffer};
  builder.add(3);
  builder.add(4);
  builder.add(5);
  builder.add(8);
  builder.add(9);
  builder.add_range(20, 100);
  builder.add_range(100, 130);

  EXPECT_EQ(builder.size(), 3);
  EXPECT_EQ(builder[0], IndexRange::from_begin_end_inclusive(3, 5));
  EXPECT_EQ(builder[1], IndexRange::from_begin_end_inclusive(8, 9));
  EXPECT_EQ(builder[2], IndexRange::from_begin_end(20, 130));
}

TEST(index_ranges_builder, Full)
{
  {
    IndexRangesBuilderBuffer<int, 1> builder_buffer;
    IndexRangesBuilder<int> builder{builder_buffer};
    builder.add(10);
    builder.add(11);
    builder.add(12);

    EXPECT_EQ(builder.size(), 1);
    EXPECT_EQ(builder[0], IndexRange::from_begin_end_inclusive(10, 12));
  }
  {
    IndexRangesBuilderBuffer<int, 3> builder_buffer;
    IndexRangesBuilder<int> builder{builder_buffer};
    builder.add(100);
    builder.add(200);
    builder.add(300);
    EXPECT_EQ(builder.size(), 3);
    EXPECT_EQ(builder[0], IndexRange::from_begin_size(100, 1));
    EXPECT_EQ(builder[1], IndexRange::from_begin_size(200, 1));
    EXPECT_EQ(builder[2], IndexRange::from_begin_size(300, 1));
  }
}

}  // namespace blender::tests
