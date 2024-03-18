/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_bit_group_vector.hh"
#include "BLI_strict_flags.h"

#include "testing/testing.h"

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

}  // namespace blender::bits::tests
