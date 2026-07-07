/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <array>

#include "BLI_bit_span.hh"
#include "BLI_bit_span_ops.hh"
#include "BLI_bit_span_to_index_ranges.hh"
#include "BLI_bit_vector.hh"
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

  {
    const Vector<int> expect{2, 3};
    Vector<int> result;
    for (const int bit_index : iter_1_indices(data)) {
      result.append(bit_index);
    }
    EXPECT_EQ_SPAN(expect.as_span(), result.as_span());
  }
  {
    uint64_t data2 = 0xFBu;
    const Vector<int> expect{0, 1, 3, 4, 5, 6, 7};
    Vector<int> result;
    for (const int bit_index : iter_1_indices(data2)) {
      result.append(bit_index);
    }
    EXPECT_EQ_SPAN(expect.as_span(), result.as_span());
  }
  {
    uint64_t data2 = ~uint64_t(0);
    int i = 0;
    for (const int bit_index : iter_1_indices(data2)) {
      EXPECT_EQ(i, bit_index);
      i++;
    }
    EXPECT_EQ(i, 64);
  }
  {
    uint64_t data2 = 0;
    int i = 0;
    for ([[maybe_unused]] const int bit_index : iter_1_indices(data2)) {
      i++;
    }
    EXPECT_EQ(i, 0);
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
  mutable_span[5].set(true);
  EXPECT_TRUE(mutable_span[5].test());
  EXPECT_TRUE(span[5].test());

  EXPECT_FALSE(mutable_span[120].test());
  EXPECT_FALSE(span[120].test());
  mutable_span[120].set(true);
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

TEST(bit_span, ForEach1Cancel)
{
  BitVector<> vec(100, false);
  vec[4].set();
  vec[10].set();
  vec[20].set();
  {
    Vector<int> indices;
    foreach_1_index(vec, [&](const int i) {
      indices.append(i);
      return i < 5;
    });
    EXPECT_EQ(indices.as_span(), Span({4, 10}));
  }
  {
    Vector<int> indices;
    foreach_1_index(vec, [&](const int i) {
      indices.append(i);
      return i < 15;
    });
    EXPECT_EQ(indices.as_span(), Span({4, 10, 20}));
  }
  {
    Vector<int> indices;
    foreach_1_index(vec, [&](const int i) {
      indices.append(i);
      return false;
    });
    EXPECT_EQ(indices.as_span(), Span({4}));
  }
  {
    Vector<int> indices;
    foreach_1_index(vec, [&](const int i) {
      indices.append(i);
      return true;
    });
    EXPECT_EQ(indices.as_span(), Span({4, 10, 20}));
  }
}

TEST(bit_span, FindFirst1Index)
{
  {
    BitVector<> vec(0);
    EXPECT_EQ(find_first_1_index(vec), std::nullopt);
  }
  {
    BitVector<> vec(10'000, false);
    EXPECT_EQ(find_first_1_index(vec), std::nullopt);
  }
  {
    BitVector<> vec(10'000, true);
    EXPECT_EQ(find_first_1_index(vec), 0);
  }
  {
    BitVector<> vec(10, false);
    vec[6].set();
    EXPECT_EQ(find_first_1_index(vec), 6);
  }
  {
    BitVector<> vec(10'000, false);
    vec[2'500].set();
    EXPECT_EQ(find_first_1_index(vec), 2'500);
    EXPECT_EQ(find_first_1_index(BitSpan(vec).drop_front(100)), 2'400);
  }
  {
    BitVector<> vec_a(10'000, false);
    BitVector<> vec_b(10'000, false);
    vec_a[2'000].set();
    vec_a[2'400].set();
    vec_a[2'500].set();
    vec_b[2'000].set();
    vec_b[2'400].set();
    vec_b[2'600].set();
    /* This finds the first index where the two vectors are different. */
    EXPECT_EQ(find_first_1_index_expr(
                  [](const BitInt a, const BitInt b) { return a ^ b; }, vec_a, vec_b),
              2'500);
  }
}

TEST(bit_span, FindFirst0Index)
{
  {
    BitVector<> vec(0);
    EXPECT_EQ(find_first_0_index(vec), std::nullopt);
  }
  {
    BitVector<> vec(10'000, true);
    EXPECT_EQ(find_first_0_index(vec), std::nullopt);
  }
  {
    BitVector<> vec(10'000, false);
    EXPECT_EQ(find_first_0_index(vec), 0);
  }
  {
    BitVector<> vec(10'000, true);
    vec[2'500].reset();
    EXPECT_EQ(find_first_0_index(vec), 2'500);
    EXPECT_EQ(find_first_0_index(BitSpan(vec).drop_front(100)), 2'400);
  }
}

TEST(bit_span, or_bools_into_bits)
{
  {
    Vector<bool> bools(5, false);
    bools[2] = true;
    BitVector<> bits(bools.size());
    bits[0].set();
    bits::or_bools_into_bits(bools, bits);
    EXPECT_TRUE(bits[0]);
    EXPECT_FALSE(bits[1]);
    EXPECT_TRUE(bits[2]);
    EXPECT_FALSE(bits[3]);
    EXPECT_FALSE(bits[4]);
  }
  {
    Vector<bool> bools(100, true);
    BitVector<> bits(1000, false);
    bits::or_bools_into_bits(bools,
                             MutableBitSpan(bits).slice(IndexRange::from_begin_size(100, 500)));
    EXPECT_FALSE(bits[99]);
    EXPECT_TRUE(bits[100]);
    EXPECT_TRUE(bits[101]);
    EXPECT_TRUE(bits[199]);
    EXPECT_FALSE(bits[200]);
  }
}

TEST(bit_span, to_index_ranges_small)
{
  BitVector<> bits(10, false);
  bits[2].set();
  bits[3].set();
  bits[4].set();
  bits[6].set();
  bits[7].set();

  IndexRangesBuilderBuffer<int, 10> builder_buffer;
  IndexRangesBuilder<int> builder(builder_buffer);
  bits_to_index_ranges(bits, builder);

  EXPECT_EQ(builder.size(), 2);
  EXPECT_EQ(builder[0], IndexRange::from_begin_end_inclusive(2, 4));
  EXPECT_EQ(builder[1], IndexRange::from_begin_end_inclusive(6, 7));
}

TEST(bit_span, to_index_ranges_all_ones)
{
  BitVector<> bits(10000, true);

  IndexRangesBuilderBuffer<int, 10> builder_buffer;
  IndexRangesBuilder<int> builder(builder_buffer);
  bits_to_index_ranges(BitSpan(bits).take_back(8765), builder);

  EXPECT_EQ(builder.size(), 1);
  EXPECT_EQ(builder[0], IndexRange(8765));
}

}  // namespace blender::bits::tests
