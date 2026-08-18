/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <algorithm>

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

static void test_build_groups_from_indices_mask(const int64_t groups_num,
                                                const int64_t indices_num)
{
  BLI_task_scheduler_init();

  Array<int> indices(indices_num);
  for (const int64_t i : indices.index_range()) {
    indices[i] = int((i * 4241) % groups_num);
  }

  /* A mask with gaps, so that the original indices differ from the positions. */
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_every_nth(3, indices_num, 1, memory);

  Array<int> offset_data;
  Array<int> index_data;
  const GroupedSpan<int> groups = build_groups_from_indices(
      indices, int(groups_num), offset_data, index_data);

  Array<int> mask_offset_data;
  Array<int> mask_index_data;
  const GroupedSpan<int> mask_groups = build_groups_from_indices(
      indices, int(groups_num), mask_offset_data, mask_index_data, mask);

  ASSERT_EQ(mask_groups.offsets.total_size(), groups.offsets.total_size());
  for (const int64_t i : index_data.index_range()) {
    ASSERT_EQ(mask_index_data[i], mask[index_data[i]]);
  }
}

TEST(offset_indices, build_groups_from_indices_mask)
{
  test_build_groups_from_indices_mask(64, 200000);
  test_build_groups_from_indices_mask(150000, 200000);
  test_build_groups_from_indices_mask(1000, 1000);
}

static void test_reverse_indices_in_value_groups(const int64_t groups_num,
                                                 const int64_t indices_num)
{
  BLI_task_scheduler_init();

  Array<int> indices(indices_num);
  Array<int> offset_data(groups_num + 1, 0);
  for (const int64_t i : indices.index_range()) {
    indices[i] = int((i * 4241) % groups_num);
    offset_data[indices[i]]++;
  }
  const OffsetIndices<int> offsets = accumulate_counts_to_offsets(offset_data);

  /* Build irregular "face"-like groups spanning the same index space as `indices`, each covering
   * a handful of consecutive positions, mimicking mesh faces referencing corners. */
  Array<int> value_group_offset_data;
  {
    Vector<int> sizes;
    int64_t remaining = indices_num;
    while (remaining > 0) {
      sizes.append(int(std::min<int64_t>(remaining, 3 + sizes.size() % 4)));
      remaining -= sizes.last();
    }
    value_group_offset_data.reinitialize(sizes.size() + 1);
    value_group_offset_data.as_mutable_span().drop_back(1).copy_from(sizes);
  }
  const OffsetIndices<int> value_groups = accumulate_counts_to_offsets(value_group_offset_data);
  ASSERT_EQ(value_groups.total_size(), indices_num);

  Array<int> positions(indices_num);
  reverse_indices_in_groups(indices, offsets, positions);

  Array<int> results(indices_num);
  reverse_indices_in_groups(indices, offsets, results, value_groups);

  const Span<int> value_group_offsets = value_group_offset_data;
  for (const int64_t i : results.index_range()) {
    const int64_t expected = std::ranges::upper_bound(value_group_offsets, positions[i]) -
                             value_group_offsets.begin() - 1;
    ASSERT_EQ(results[i], expected);
  }
}

TEST(offset_indices, reverse_indices_in_value_groups)
{
  test_reverse_indices_in_value_groups(20000, 150000);
  test_reverse_indices_in_value_groups(20000, 300000);
  test_reverse_indices_in_value_groups(1000, 1000);
}

static void test_reverse_indices_in_uniform_value_groups(const int64_t groups_num,
                                                         const int64_t pairs_num)
{
  BLI_task_scheduler_init();

  Array<int> indices(pairs_num * 2);
  Array<int> offset_data(groups_num + 1, 0);
  for (const int64_t i : indices.index_range()) {
    indices[i] = int((i * 4241) % groups_num);
    offset_data[indices[i]]++;
  }
  const OffsetIndices<int> offsets = accumulate_counts_to_offsets(offset_data);

  Array<int> positions(indices.size());
  reverse_indices_in_groups(indices, offsets, positions);

  Array<int> results(indices.size());
  reverse_indices_in_groups(indices, offsets, results, 2);

  for (const int64_t i : results.index_range()) {
    ASSERT_EQ(results[i], positions[i] / 2);
  }
}

TEST(offset_indices, reverse_indices_in_uniform_value_groups)
{
  test_reverse_indices_in_uniform_value_groups(20000, 75000);
  test_reverse_indices_in_uniform_value_groups(20000, 150000);
  test_reverse_indices_in_uniform_value_groups(1000, 500);
}

}  // namespace blender::offset_indices::tests
