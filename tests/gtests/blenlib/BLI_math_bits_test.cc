#include "BLI_math_bits.h"
#include "testing/testing.h"
#include <iostream>

TEST(math_bits, BitscanReverseClearUint)
{
  uint a = 1234;
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 21);
  EXPECT_EQ(a, 210);
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 24);
  EXPECT_EQ(a, 82);
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 25);
  EXPECT_EQ(a, 18);
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 27);
  EXPECT_EQ(a, 2);
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 30);
  EXPECT_EQ(a, 0);

  a = 3563987529;
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 0);
  EXPECT_EQ(a, 1416503881);
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 1);
  EXPECT_EQ(a, 342762057);
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 3);
  EXPECT_EQ(a, 74326601);
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 5);
  EXPECT_EQ(a, 7217737);
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 9);
  EXPECT_EQ(a, 3023433);
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 10);
  EXPECT_EQ(a, 926281);
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 12);
  EXPECT_EQ(a, 401993);
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 13);
  EXPECT_EQ(a, 139849);
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 14);
  EXPECT_EQ(a, 8777);
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 18);
  EXPECT_EQ(a, 585);
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 22);
  EXPECT_EQ(a, 73);
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 25);
  EXPECT_EQ(a, 9);
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 28);
  EXPECT_EQ(a, 1);
  EXPECT_EQ(bitscan_reverse_clear_uint(&a), 31);
  EXPECT_EQ(a, 0);
}
