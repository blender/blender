/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_bit_group_vector.hh"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

namespace blender::bits::tests {

TEST(bit_group_vector, DefaultConstruct)
{
  BitGroupVector<> groups;
  EXPECT_EQ(groups.size(), 0);
}

TEST(bit_group_vector, Construct)
{
  BitGroupVector<> groups(12, 5);

  EXPECT_EQ(groups.size(), 12);
  EXPECT_EQ(groups[0].size(), 5);
  EXPECT_EQ(groups[4].size(), 5);
}

TEST(bit_group_vector, CopyConstruct)
{
  BitGroupVector<> groups(12, 5);
  for (const int64_t i : groups.index_range()) {
    MutableBoundedBitSpan span = groups[i];
    for (const int64_t j : span.index_range()) {
      span[j].set(j % 2 == 0);
    }
  }

  BitGroupVector<> copy(groups);

  EXPECT_EQ(groups.size(), copy.size());
  EXPECT_EQ(groups.group_size(), copy.group_size());
  for (const int64_t i : groups.index_range()) {
    BoundedBitSpan span = groups[i];
    BoundedBitSpan copy_span = copy[i];
    for (const int64_t j : span.index_range()) {
      EXPECT_EQ(span[j].test(), copy_span[j].test());
    }
  }
}

TEST(bit_group_vector, foreachAnd)
{
  BitGroupVector<> groups(3, 4);
  groups.all_bits().set_all();

  BitVector<> mask{Span<bool>{false, false, true, false}};
  groups.foreach_and(mask);

  EXPECT_FALSE(groups[0][0].test());
  EXPECT_FALSE(groups[0][1].test());
  EXPECT_TRUE(groups[0][2].test());
  EXPECT_FALSE(groups[0][3].test());

  EXPECT_FALSE(groups[1][0].test());
  EXPECT_FALSE(groups[1][1].test());
  EXPECT_TRUE(groups[1][2].test());
  EXPECT_FALSE(groups[1][3].test());

  EXPECT_FALSE(groups[2][0].test());
  EXPECT_FALSE(groups[2][1].test());
  EXPECT_TRUE(groups[2][2].test());
  EXPECT_FALSE(groups[2][3].test());
}

}  // namespace blender::bits::tests
