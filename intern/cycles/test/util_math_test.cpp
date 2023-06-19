/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "util/math.h"

CCL_NAMESPACE_BEGIN

TEST(math, next_power_of_two)
{
  EXPECT_EQ(next_power_of_two(0), 1);
  EXPECT_EQ(next_power_of_two(1), 2);
  EXPECT_EQ(next_power_of_two(2), 4);
  EXPECT_EQ(next_power_of_two(3), 4);
  EXPECT_EQ(next_power_of_two(4), 8);
}

TEST(math, prev_power_of_two)
{
  EXPECT_EQ(prev_power_of_two(0), 0);

  EXPECT_EQ(prev_power_of_two(1), 1);
  EXPECT_EQ(prev_power_of_two(2), 1);

  EXPECT_EQ(prev_power_of_two(3), 2);
  EXPECT_EQ(prev_power_of_two(4), 2);

  EXPECT_EQ(prev_power_of_two(5), 4);
  EXPECT_EQ(prev_power_of_two(6), 4);
  EXPECT_EQ(prev_power_of_two(7), 4);
  EXPECT_EQ(prev_power_of_two(8), 4);
}

TEST(math, reverse_integer_bits)
{
  EXPECT_EQ(reverse_integer_bits(0xFFFFFFFF), 0xFFFFFFFF);
  EXPECT_EQ(reverse_integer_bits(0x00000000), 0x00000000);
  EXPECT_EQ(reverse_integer_bits(0x1), 0x80000000);
  EXPECT_EQ(reverse_integer_bits(0x80000000), 0x1);
  EXPECT_EQ(reverse_integer_bits(0xFFFF0000), 0x0000FFFF);
  EXPECT_EQ(reverse_integer_bits(0x0000FFFF), 0xFFFF0000);
  EXPECT_EQ(reverse_integer_bits(0x00FF0000), 0x0000FF00);
  EXPECT_EQ(reverse_integer_bits(0x0000FF00), 0x00FF0000);
  EXPECT_EQ(reverse_integer_bits(0xAAAAAAAA), 0x55555555);
}

CCL_NAMESPACE_END
