/* SPDX-License-Identifier: Apache-2.0 */

#include <array>

#include "BLI_bit_span.hh"
#include "BLI_bit_span_ops.hh"
#include "BLI_timeit.hh"
#include "BLI_vector.hh"

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

TEST(bit_span, SetEmpty)
{
  MutableBitSpan().set_all(true);
  MutableBitSpan().set_all(false);
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

TEST(bit_span, IsBounded)
{
  std::array<uint64_t, 10> data;

  EXPECT_TRUE(is_bounded_span(BitSpan(data.data(), 0)));
  EXPECT_TRUE(is_bounded_span(BitSpan(data.data(), 1)));
  EXPECT_TRUE(is_bounded_span(BitSpan(data.data(), 50)));
  EXPECT_TRUE(is_bounded_span(BitSpan(data.data(), 63)));
  EXPECT_TRUE(is_bounded_span(BitSpan(data.data(), 64)));
  EXPECT_TRUE(is_bounded_span(BitSpan(data.data(), 65)));
  EXPECT_TRUE(is_bounded_span(BitSpan(data.data(), 100)));
  EXPECT_TRUE(is_bounded_span(BitSpan(data.data(), 400)));

  EXPECT_TRUE(is_bounded_span(BitSpan(data.data(), IndexRange(0, 3))));
  EXPECT_TRUE(is_bounded_span(BitSpan(data.data(), IndexRange(1, 3))));
  EXPECT_TRUE(is_bounded_span(BitSpan(data.data(), IndexRange(10, 20))));
  EXPECT_TRUE(is_bounded_span(BitSpan(data.data(), IndexRange(63, 1))));
  EXPECT_TRUE(is_bounded_span(BitSpan(data.data(), IndexRange(10, 54))));

  EXPECT_FALSE(is_bounded_span(BitSpan(data.data(), IndexRange(1, 64))));
  EXPECT_FALSE(is_bounded_span(BitSpan(data.data(), IndexRange(10, 64))));
  EXPECT_FALSE(is_bounded_span(BitSpan(data.data(), IndexRange(10, 200))));
  EXPECT_FALSE(is_bounded_span(BitSpan(data.data(), IndexRange(60, 5))));
  EXPECT_FALSE(is_bounded_span(BitSpan(data.data(), IndexRange(64, 0))));
  EXPECT_FALSE(is_bounded_span(BitSpan(data.data(), IndexRange(70, 5))));
}

TEST(bit_span, CopyFrom)
{
  std::array<uint64_t, 30> src_data;
  uint64_t i = 0;
  for (uint64_t &value : src_data) {
    value = i;
    i += 234589766883;
  }
  const BitSpan src(src_data.data(), src_data.size() * BitsPerInt);

  std::array<uint64_t, 4> dst_data;
  dst_data.fill(-1);
  MutableBitSpan dst(dst_data.data(), 100);
  dst.copy_from(src.slice({401, 100}));

  for (const int i : dst.index_range()) {
    EXPECT_TRUE(dst[i].test() == src[401 + i].test());
  }
}

TEST(bit_span, InPlaceOr)
{
  std::array<uint64_t, 100> data_1;
  MutableBitSpan span_1(data_1.data(), data_1.size() * BitsPerInt);
  for (const int i : span_1.index_range()) {
    span_1[i].set(i % 2 == 0);
  }

  std::array<uint64_t, 100> data_2;
  MutableBitSpan span_2(data_2.data(), data_2.size() * BitsPerInt);
  for (const int i : span_2.index_range()) {
    span_2[i].set(i % 2 != 0);
  }

  span_1 |= span_2;
  for (const int i : span_1.index_range()) {
    EXPECT_TRUE(span_1[i].test());
  }
}

TEST(bit_span, InPlaceAnd)
{
  std::array<uint64_t, 100> data_1{};
  MutableBitSpan span_1(data_1.data(), data_1.size() * BitsPerInt);
  for (const int i : span_1.index_range()) {
    span_1[i].set(i % 2 == 0);
  }

  std::array<uint64_t, 100> data_2{};
  MutableBitSpan span_2(data_2.data(), data_2.size() * BitsPerInt);
  for (const int i : span_2.index_range()) {
    span_2[i].set(i % 2 != 0);
  }

  span_1 &= span_2;
  for (const int i : span_1.index_range()) {
    EXPECT_FALSE(span_1[i].test());
  }
}

TEST(bit_span, ForEach1)
{
  std::array<uint64_t, 2> data{};
  MutableBitSpan span(data.data(), data.size() * BitsPerInt);
  for (const int i : {1, 28, 37, 86}) {
    span[i].set();
  }

  Vector<int> indices_test;
  foreach_1_index(span.slice({4, span.size() - 4}), [&](const int i) { indices_test.append(i); });

  EXPECT_EQ(indices_test.as_span(), Span({24, 33, 82}));
}

}  // namespace blender::bits::tests
