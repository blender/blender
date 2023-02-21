/* SPDX-License-Identifier: Apache-2.0 */

#include <array>

#include "BLI_bit_span.hh"

#include "testing/testing.h"

namespace blender::bits::tests {

TEST(bit_span, DefaultConstructor)
{
  {
    char buffer[sizeof(BitSpan)];
    memset(buffer, 0xff, sizeof(BitSpan));
    BitSpan &span = *new (buffer) BitSpan();
    EXPECT_TRUE(span.is_empty());
    EXPECT_EQ(span.size(), 0);
  }
  {
    char buffer[sizeof(MutableBitSpan)];
    memset(buffer, 0xff, sizeof(MutableBitSpan));
    MutableBitSpan &span = *new (buffer) MutableBitSpan();
    EXPECT_TRUE(span.is_empty());
    EXPECT_EQ(span.size(), 0);
  }
}

TEST(bit_span, Iteration)
{
  uint64_t data = (1 << 2) | (1 << 3);
  const BitSpan span(&data, 30);
  EXPECT_EQ(span.size(), 30);
  int index = 0;
  for (const BitRef bit : span) {
    EXPECT_EQ(bit.test(), ELEM(index, 2, 3));
    index++;
  }
}

TEST(bit_span, MutableIteration)
{
  uint64_t data = 0;
  MutableBitSpan span(&data, 40);
  EXPECT_EQ(span.size(), 40);
  int index = 0;
  for (MutableBitRef bit : span) {
    bit.set(index % 4 == 0);
    index++;
  }
  EXPECT_EQ(data,
            0b0000'0000'0000'0000'0000'0000'0001'0001'0001'0001'0001'0001'0001'0001'0001'0001);
}

TEST(bit_span, SubscriptOperator)
{
  uint64_t data[2] = {0, 0};
  MutableBitSpan mutable_span(data, 128);
  BitSpan span = mutable_span;

  EXPECT_EQ(mutable_span.data(), data);
  EXPECT_EQ(mutable_span.bit_range(), IndexRange(128));
  EXPECT_EQ(span.data(), data);
  EXPECT_EQ(span.bit_range(), IndexRange(128));

  EXPECT_FALSE(mutable_span[5].test());
  EXPECT_FALSE(span[5].test());
  mutable_span[5].set(5);
  EXPECT_TRUE(mutable_span[5].test());
  EXPECT_TRUE(span[5].test());

  EXPECT_FALSE(mutable_span[120].test());
  EXPECT_FALSE(span[120].test());
  mutable_span[120].set(120);
  EXPECT_TRUE(mutable_span[120].test());
  EXPECT_TRUE(span[120].test());

  EXPECT_EQ(data[0],
            0b0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0010'0000);
  EXPECT_EQ(data[1],
            0b0000'0001'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000);
}

TEST(bit_span, RangeConstructor)
{
  uint64_t data = 0;
  MutableBitSpan mutable_span(&data, IndexRange(4, 3));
  BitSpan span = mutable_span;

  EXPECT_FALSE(mutable_span[1].test());
  EXPECT_FALSE(span[1].test());
  mutable_span[0].set(true);
  mutable_span[1].set(true);
  mutable_span[2].set(true);
  mutable_span[0].set(false);
  mutable_span[2].set(false);
  EXPECT_TRUE(mutable_span[1].test());
  EXPECT_TRUE(span[1].test());

  EXPECT_EQ(data,
            0b0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0010'0000);
}

TEST(bit_span, Set)
{
  uint64_t data = 0;
  MutableBitSpan(&data, 64).set_all(true);
  EXPECT_EQ(data, uint64_t(-1));
  MutableBitSpan(&data, 64).set_all(false);
  EXPECT_EQ(data, uint64_t(0));

  MutableBitSpan(&data, IndexRange(4, 8)).set_all(true);
  EXPECT_EQ(data,
            0b0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'1111'1111'0000);
  MutableBitSpan(&data, IndexRange(8, 30)).set_all(false);

  EXPECT_EQ(data,
            0b0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'1111'0000);
}

TEST(bit_span, SetSliced)
{
  std::array<uint64_t, 10> data;
  memset(data.data(), 0, sizeof(data));
  MutableBitSpan span{data.data(), 640};
  span.slice(IndexRange(5, 500)).set_all(true);

  for (const int64_t i : IndexRange(640)) {
    EXPECT_EQ(span[i], i >= 5 && i < 505);
  }

  span.slice(IndexRange(10, 190)).set_all(false);

  for (const int64_t i : IndexRange(640)) {
    EXPECT_EQ(span[i], (i >= 5 && i < 10) || (i >= 200 && i < 505));
  }
}

}  // namespace blender::bits::tests
