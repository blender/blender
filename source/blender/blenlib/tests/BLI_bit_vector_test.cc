/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_bit_vector.hh"
#include "BLI_exception_safety_test_utils.hh"
#include "BLI_strict_flags.h"

#include "testing/testing.h"

namespace blender::bits::tests {

TEST(bit_vector, DefaultConstructor)
{
  BitVector vec;
  EXPECT_EQ(vec.size(), 0);
}

TEST(bit_vector, CopyConstructorInline)
{
  BitVector<> vec({false, false, true, true, false});
  BitVector<> vec2 = vec;

  EXPECT_EQ(vec.size(), 5);
  EXPECT_EQ(vec2.size(), 5);

  vec2[1].set();
  EXPECT_FALSE(vec[1]);

  EXPECT_FALSE(vec2[0]);
  EXPECT_TRUE(vec2[1]);
  EXPECT_TRUE(vec2[2]);
  EXPECT_TRUE(vec2[3]);
  EXPECT_FALSE(vec2[4]);
}

TEST(bit_vector, CopyConstructorLarge)
{
  BitVector<> vec(500, false);
  vec[1].set();

  BitVector<> vec2 = vec;

  EXPECT_EQ(vec.size(), 500);
  EXPECT_EQ(vec2.size(), 500);

  vec2[2].set();
  EXPECT_FALSE(vec[2]);

  EXPECT_FALSE(vec2[0]);
  EXPECT_TRUE(vec2[1]);
  EXPECT_TRUE(vec2[2]);
}

TEST(bit_vector, MoveConstructorInline)
{
  BitVector<> vec({false, false, true, true, false});
  BitVector<> vec2 = std::move(vec);

  EXPECT_EQ(vec.size(), 0);
  EXPECT_EQ(vec2.size(), 5);

  EXPECT_FALSE(vec2[0]);
  EXPECT_FALSE(vec2[1]);
  EXPECT_TRUE(vec2[2]);
  EXPECT_TRUE(vec2[3]);
  EXPECT_FALSE(vec2[4]);
}

TEST(bit_vector, MoveConstructorLarge)
{
  BitVector<> vec(500, false);
  vec[3].set();

  BitVector<> vec2 = std::move(vec);

  EXPECT_EQ(vec.size(), 0);
  EXPECT_EQ(vec2.size(), 500);

  EXPECT_FALSE(vec2[0]);
  EXPECT_FALSE(vec2[1]);
  EXPECT_FALSE(vec2[2]);
  EXPECT_TRUE(vec2[3]);
  EXPECT_FALSE(vec2[4]);
}

TEST(bit_vector, SizeConstructor)
{
  {
    BitVector<> vec(0);
    EXPECT_EQ(vec.size(), 0);
  }
  {
    BitVector<> vec(5);
    EXPECT_EQ(vec.size(), 5);
    for (BitRef bit : vec) {
      EXPECT_FALSE(bit);
    }
  }
  {
    BitVector<> vec(123);
    EXPECT_EQ(vec.size(), 123);
    for (BitRef bit : vec) {
      EXPECT_FALSE(bit);
    }
  }
}

TEST(bit_vector, SizeFillConstructor)
{
  {
    BitVector<> vec(5, false);
    for (const int64_t i : IndexRange(5)) {
      EXPECT_FALSE(vec[i]);
    }
  }
  {
    BitVector<> vec(123, true);
    for (const int64_t i : IndexRange(123)) {
      EXPECT_TRUE(vec[i]);
    }
  }
}

TEST(bit_vector, IndexAccess)
{
  BitVector<> vec(100, false);
  vec[55].set();
  EXPECT_FALSE(vec[50]);
  EXPECT_FALSE(vec[51]);
  EXPECT_FALSE(vec[52]);
  EXPECT_FALSE(vec[53]);
  EXPECT_FALSE(vec[54]);
  EXPECT_TRUE(vec[55]);
  EXPECT_FALSE(vec[56]);
  EXPECT_FALSE(vec[57]);
  EXPECT_FALSE(vec[58]);
}

TEST(bit_vector, Iterator)
{
  BitVector<> vec(100, false);
  {
    int64_t index = 0;
    for (MutableBitRef bit : vec) {
      bit.set(ELEM(index, 0, 4, 7, 10, 11));
      index++;
    }
  }
  {
    int64_t index = 0;
    for (BitRef bit : const_cast<const BitVector<> &>(vec)) {
      EXPECT_EQ(bit, ELEM(index, 0, 4, 7, 10, 11));
      index++;
    }
  }
}

TEST(bit_vector, Append)
{
  BitVector<> vec;
  vec.append(false);
  vec.append(true);
  vec.append(true);
  vec.append(false);

  EXPECT_EQ(vec.size(), 4);
  EXPECT_FALSE(vec[0]);
  EXPECT_TRUE(vec[1]);
  EXPECT_TRUE(vec[2]);
  EXPECT_FALSE(vec[3]);
}

TEST(bit_vector, AppendMany)
{
  BitVector<> vec;
  for (const int64_t i : IndexRange(1000)) {
    vec.append(i % 2);
  }
  EXPECT_FALSE(vec[0]);
  EXPECT_TRUE(vec[1]);
  EXPECT_FALSE(vec[2]);
  EXPECT_TRUE(vec[3]);
  EXPECT_FALSE(vec[4]);
  EXPECT_TRUE(vec[5]);
}

}  // namespace blender::bits::tests
