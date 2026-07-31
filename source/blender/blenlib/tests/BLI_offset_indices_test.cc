/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_index_mask.hh"
#include "BLI_offset_indices.hh"
#include "BLI_rand.hh"
#include "BLI_task_c.hh"
#include "BLI_vector.hh"

#include "BLI_strict_flags.hh" /* IWYU pragma: keep. Keep last. */

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

/**
 * Check #build_groups_from_indices against a simple reference implementation, for enough
 * indices to use the parallel code paths. The number of groups selects between counting
 * per chunk and filling the groups with atomics.
 */
static void test_build_groups_from_indices(const int64_t groups_num,
                                           const int64_t indices_num,
                                           const bool clustered)
{
  BLI_task_scheduler_init();

  RandomNumberGenerator random;
  Array<int> indices(indices_num);
  for (const int64_t i : indices.index_range()) {
    if (clustered) {
      /* Approximately sorted, similar to real topology. */
      const int64_t center = i * groups_num / indices_num;
      const int64_t jitter = std::max<int64_t>(groups_num / 64, 1);
      indices[i] = int(std::clamp(
          center + random.get_int32(int32_t(2 * jitter)) - jitter, int64_t(0), groups_num - 1));
    }
    else {
      indices[i] = random.get_int32(int32_t(groups_num));
    }
  }

  Array<int> offset_data;
  Array<int> index_data;
  const GroupedSpan<int> groups = build_groups_from_indices(
      indices, int(groups_num), offset_data, index_data);

  ASSERT_EQ(groups.size(), groups_num);
  ASSERT_EQ(groups.offsets.total_size(), indices_num);

  /* Every index must appear in the group it points to, and groups must be sorted. */
  Array<int> expected_counts(groups_num, 0);
  for (const int group : indices) {
    expected_counts[group]++;
  }
  for (const int64_t group : IndexRange(groups_num)) {
    const Span<int> group_indices = groups[group];
    ASSERT_EQ(group_indices.size(), expected_counts[group]);
    for (const int64_t i : group_indices.index_range()) {
      ASSERT_EQ(indices[group_indices[i]], group);
      if (i > 0) {
        ASSERT_LT(group_indices[i - 1], group_indices[i]);
      }
    }
  }
}

TEST(offset_indices, build_groups_from_indices_few_groups)
{
  test_build_groups_from_indices(64, 200000, false);
  test_build_groups_from_indices(64, 200000, true);
}

TEST(offset_indices, build_groups_from_indices_many_groups)
{
  test_build_groups_from_indices(150000, 200000, false);
  test_build_groups_from_indices(150000, 200000, true);
}

TEST(offset_indices, build_groups_from_indices_atomic)
{
  test_build_groups_from_indices(2000000, 200000, false);
  test_build_groups_from_indices(2000000, 200000, true);
}

TEST(offset_indices, build_groups_from_indices_small)
{
  test_build_groups_from_indices(64, 1000, false);
  test_build_groups_from_indices(1000, 1000, false);
}

}  // namespace blender::offset_indices::tests
