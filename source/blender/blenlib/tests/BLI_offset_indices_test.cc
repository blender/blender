/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_index_mask.hh"
#include "BLI_offset_indices.hh"
#include "BLI_vector.hh"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

namespace blender::offset_indices::tests {

TEST(offset_indices, SumSizes)
{
  Vector<int> data = {3, 2, 1, 5, -1};
  const OffsetIndices<int> offsets = accumulate_counts_to_offsets(data);
  EXPECT_EQ(sum_group_sizes(offsets, {0, 1, 2, 3}), 11);
  EXPECT_EQ(sum_group_sizes(offsets, {3, 2, 1, 0}), 11);
  EXPECT_EQ(sum_group_sizes(offsets, {3, 0}), 8);
  EXPECT_EQ(sum_group_sizes(offsets, IndexRange(4)), 11);
  EXPECT_EQ(sum_group_sizes(offsets, IndexMask(4)), 11);
  EXPECT_EQ(sum_group_sizes(offsets, IndexMask(1)), 3);
}

TEST(offset_indices, build_groups_from_indices)
{
  Vector<int> data = {3, 2, 1, 3, 4, 1, 1, 6, 8, 1, 8, 0};
  const int groups_num = 10;

  Array<int> offset_data;
  Array<int> index_data;
  const GroupedSpan<int> groups = build_groups_from_indices(
      data, groups_num, offset_data, index_data);

  EXPECT_EQ(groups.size(), groups_num);
  EXPECT_EQ(groups.offsets.total_size(), data.size());
  EXPECT_EQ_SPAN(groups[1], {2, 5, 6, 9});
  EXPECT_TRUE(groups[5].is_empty());
}

}  // namespace blender::offset_indices::tests
