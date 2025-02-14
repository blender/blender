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

}  // namespace blender::offset_indices::tests
