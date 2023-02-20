/* SPDX-License-Identifier: Apache-2.0 */

#include <array>

#include "BLI_bit_ref.hh"

#include "testing/testing.h"

namespace blender::bits::tests {

TEST(bit_ref, MaskFirstNBits)
{
  EXPECT_EQ(mask_first_n_bits(0), 0);
  EXPECT_EQ(mask_first_n_bits(1),
            0b0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0001);
  EXPECT_EQ(mask_first_n_bits(5),
            0b0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0001'1111);
  EXPECT_EQ(mask_first_n_bits(63),
            0b0111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111);
  EXPECT_EQ(mask_first_n_bits(64),
            0b1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111);
}

TEST(bit_ref, MaskLastNBits)
{
  EXPECT_EQ(mask_last_n_bits(0),
            0b0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000);
  EXPECT_EQ(mask_last_n_bits(1),
            0b1000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000);
  EXPECT_EQ(mask_last_n_bits(5),
            0b1111'1000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000);
  EXPECT_EQ(mask_last_n_bits(63),
            0b1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1110);
  EXPECT_EQ(mask_last_n_bits(64),
            0b1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111);
}

TEST(bit_ref, MaskSingleBit)
{
  EXPECT_EQ(mask_single_bit(0), 1);
  EXPECT_EQ(mask_single_bit(1),
            0b0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0010);
  EXPECT_EQ(mask_single_bit(5),
            0b0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0010'0000);
  EXPECT_EQ(mask_single_bit(63),
            0b1000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000);
}

TEST(bit_ref, IntContainingBit)
{
  std::array<uint64_t, 5> array;
  uint64_t *data = array.data();
  EXPECT_EQ(int_containing_bit(data, 0), data);
  EXPECT_EQ(int_containing_bit(data, 1), data);
  EXPECT_EQ(int_containing_bit(data, 63), data);
  EXPECT_EQ(int_containing_bit(data, 64), data + 1);
  EXPECT_EQ(int_containing_bit(data, 65), data + 1);
  EXPECT_EQ(int_containing_bit(data, 100), data + 1);
  EXPECT_EQ(int_containing_bit(data, 127), data + 1);
  EXPECT_EQ(int_containing_bit(data, 128), data + 2);
  const uint64_t *data_const = data;
  EXPECT_EQ(int_containing_bit(data_const, 0), data_const);
  EXPECT_EQ(int_containing_bit(data_const, 1), data_const);
  EXPECT_EQ(int_containing_bit(data_const, 63), data_const);
  EXPECT_EQ(int_containing_bit(data_const, 64), data_const + 1);
  EXPECT_EQ(int_containing_bit(data_const, 65), data_const + 1);
  EXPECT_EQ(int_containing_bit(data_const, 100), data_const + 1);
  EXPECT_EQ(int_containing_bit(data_const, 127), data_const + 1);
  EXPECT_EQ(int_containing_bit(data_const, 128), data_const + 2);
}

TEST(bit_ref, Test)
{
  uint64_t data = (1 << 3) | (1 << 7);
  EXPECT_FALSE(BitRef(&data, 0).test());
  EXPECT_FALSE(BitRef(&data, 1).test());
  EXPECT_FALSE(BitRef(&data, 2).test());
  EXPECT_TRUE(BitRef(&data, 3).test());
  EXPECT_FALSE(BitRef(&data, 4));
  EXPECT_FALSE(BitRef(&data, 5));
  EXPECT_FALSE(BitRef(&data, 6));
  EXPECT_TRUE(BitRef(&data, 7));

  EXPECT_FALSE(MutableBitRef(&data, 0).test());
  EXPECT_FALSE(MutableBitRef(&data, 1).test());
  EXPECT_FALSE(MutableBitRef(&data, 2).test());
  EXPECT_TRUE(MutableBitRef(&data, 3).test());
  EXPECT_FALSE(MutableBitRef(&data, 4));
  EXPECT_FALSE(MutableBitRef(&data, 5));
  EXPECT_FALSE(MutableBitRef(&data, 6));
  EXPECT_TRUE(MutableBitRef(&data, 7));
}

TEST(bit_ref, Set)
{
  uint64_t data = 0;
  MutableBitRef(&data, 0).set();
  MutableBitRef(&data, 1).set();
  MutableBitRef(&data, 1).set();
  MutableBitRef(&data, 4).set();
  EXPECT_EQ(data, (1 << 0) | (1 << 1) | (1 << 4));
  MutableBitRef(&data, 5).set(true);
  MutableBitRef(&data, 1).set(false);
  EXPECT_EQ(data, (1 << 0) | (1 << 4) | (1 << 5));
}

TEST(bit_ref, Reset)
{
  uint64_t data = -1;
  MutableBitRef(&data, 0).reset();
  MutableBitRef(&data, 2).reset();
  EXPECT_EQ(data, uint64_t(-1) & ~(1 << 0) & ~(1 << 2));
}

TEST(bit_ref, SetBranchless)
{
  uint64_t data = 0;
  MutableBitRef(&data, 0).set_branchless(true);
  EXPECT_EQ(data, 1);
  MutableBitRef(&data, 0).set_branchless(false);
  EXPECT_EQ(data, 0);
  MutableBitRef(&data, 3).set_branchless(false);
  MutableBitRef(&data, 4).set_branchless(true);
  EXPECT_EQ(data, 16);
  MutableBitRef(&data, 3).set_branchless(true);
  MutableBitRef(&data, 4).set_branchless(true);
  EXPECT_EQ(data, 24);
}

TEST(bit_ref, Cast)
{
  uint64_t data = 0;
  MutableBitRef mutable_ref(&data, 3);
  BitRef ref = mutable_ref;
  EXPECT_FALSE(ref);
  mutable_ref.set();
  EXPECT_TRUE(ref);
}

TEST(bit_ref, MaskRangeBits)
{
  EXPECT_EQ(mask_range_bits(0, 0),
            0b0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000);
  EXPECT_EQ(mask_range_bits(0, 1),
            0b0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0001);
  EXPECT_EQ(mask_range_bits(0, 5),
            0b0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0001'1111);
  EXPECT_EQ(mask_range_bits(64, 0),
            0b0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000);
  EXPECT_EQ(mask_range_bits(63, 1),
            0b1000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000);
  EXPECT_EQ(mask_range_bits(59, 5),
            0b1111'1000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000);
  EXPECT_EQ(mask_range_bits(8, 3),
            0b0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0111'0000'0000);
  EXPECT_EQ(mask_range_bits(0, 64),
            0b1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111);
}

}  // namespace blender::bits::tests
