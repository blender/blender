/*
 * Copyright 2011-2021 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
