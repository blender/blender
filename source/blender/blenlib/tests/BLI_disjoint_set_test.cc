/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_disjoint_set.hh"
#include "BLI_strict_flags.h"

#include "testing/testing.h"

namespace blender::tests {

TEST(disjoint_set, Test)
{
  DisjointSet disjoint_set(6);
  EXPECT_FALSE(disjoint_set.in_same_set(1, 2));
  EXPECT_FALSE(disjoint_set.in_same_set(5, 3));
  EXPECT_TRUE(disjoint_set.in_same_set(2, 2));
  EXPECT_EQ(disjoint_set.find_root(3), 3);

  disjoint_set.join(1, 2);

  EXPECT_TRUE(disjoint_set.in_same_set(1, 2));
  EXPECT_FALSE(disjoint_set.in_same_set(0, 1));

  disjoint_set.join(3, 4);

  EXPECT_FALSE(disjoint_set.in_same_set(2, 3));
  EXPECT_TRUE(disjoint_set.in_same_set(3, 4));

  disjoint_set.join(1, 4);

  EXPECT_TRUE(disjoint_set.in_same_set(1, 4));
  EXPECT_TRUE(disjoint_set.in_same_set(1, 3));
  EXPECT_TRUE(disjoint_set.in_same_set(2, 4));
  EXPECT_FALSE(disjoint_set.in_same_set(0, 4));
}

}  // namespace blender::tests
