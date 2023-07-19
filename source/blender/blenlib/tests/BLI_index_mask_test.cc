/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_rand.hh"
#include "BLI_set.hh"
#include "BLI_strict_flags.h"
#include "BLI_timeit.hh"

#include "testing/testing.h"

namespace blender::index_mask::tests {

TEST(index_mask, IndicesToMask)
{
  IndexMaskMemory memory;
  Array<int> data = {
      5, 100, 16383, 16384, 16385, 20000, 20001, 50000, 50001, 50002, 100000, 101000};
  IndexMask mask = IndexMask::from_indices<int>(data, memory);

  EXPECT_EQ(mask.first(), 5);
  EXPECT_EQ(mask.last(), 101000);
  EXPECT_EQ(mask.min_array_size(), 101001);
}

TEST(index_mask, FromBits)
{
  IndexMaskMemory memory;
  const uint64_t bits =
      0b0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'0000'1111'0010'0000;
  const IndexMask mask = IndexMask::from_bits(BitSpan(&bits, IndexRange(2, 40)), memory);
  Array<int> indices(5);
  mask.to_indices<int>(indices);
  EXPECT_EQ(indices[0], 3);
  EXPECT_EQ(indices[1], 6);
  EXPECT_EQ(indices[2], 7);
  EXPECT_EQ(indices[3], 8);
  EXPECT_EQ(indices[4], 9);
}

TEST(index_mask, FromSize)
{
  {
    const IndexMask mask(5);
    Vector<IndexMaskSegment> segments;
    mask.foreach_segment([&](const IndexMaskSegment segment) { segments.append(segment); });
    EXPECT_EQ(segments.size(), 1);
    EXPECT_EQ(segments[0].size(), 5);
    EXPECT_EQ(mask.first(), 0);
    EXPECT_EQ(mask.last(), 4);
    EXPECT_EQ(mask.min_array_size(), 5);
  }
  {
    const IndexMask mask(max_segment_size);
    Vector<IndexMaskSegment> segments;
    mask.foreach_segment([&](const IndexMaskSegment segment) { segments.append(segment); });
    EXPECT_EQ(segments.size(), 1);
    EXPECT_EQ(segments[0].size(), max_segment_size);
    EXPECT_EQ(mask.first(), 0);
    EXPECT_EQ(mask.last(), max_segment_size - 1);
    EXPECT_EQ(mask.min_array_size(), max_segment_size);
  }
}

TEST(index_mask, FromUnion)
{
  {
    IndexMaskMemory memory;
    Array<int> data_a = {1, 2};
    IndexMask mask_a = IndexMask::from_indices<int>(data_a, memory);
    Array<int> data_b = {2, 20000, 20001};
    IndexMask mask_b = IndexMask::from_indices<int>(data_b, memory);

    IndexMask mask_union = IndexMask::from_union(mask_a, mask_b, memory);

    EXPECT_EQ(mask_union.size(), 4);
    EXPECT_EQ(mask_union[0], 1);
    EXPECT_EQ(mask_union[1], 2);
    EXPECT_EQ(mask_union[2], 20000);
    EXPECT_EQ(mask_union[3], 20001);
  }
  {
    IndexMaskMemory memory;
    Array<int> data_a = {1, 2, 3};
    IndexMask mask_a = IndexMask::from_indices<int>(data_a, memory);
    Array<int> data_b = {20000, 20001, 20002};
    IndexMask mask_b = IndexMask::from_indices<int>(data_b, memory);

    IndexMask mask_union = IndexMask::from_union(mask_a, mask_b, memory);

    EXPECT_EQ(mask_union.size(), 6);
    EXPECT_EQ(mask_union[0], 1);
    EXPECT_EQ(mask_union[1], 2);
    EXPECT_EQ(mask_union[2], 3);
    EXPECT_EQ(mask_union[3], 20000);
    EXPECT_EQ(mask_union[4], 20001);
    EXPECT_EQ(mask_union[5], 20002);
  }
}

TEST(index_mask, DefaultConstructor)
{
  IndexMask mask;
  EXPECT_EQ(mask.size(), 0);
  EXPECT_EQ(mask.min_array_size(), 0);
}

TEST(index_mask, ForeachRange)
{
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_indices<int>({2, 3, 4, 10, 40, 41}, memory);
  Vector<IndexRange> ranges;
  mask.foreach_range([&](const IndexRange range) { ranges.append(range); });

  EXPECT_EQ(ranges.size(), 3);
  EXPECT_EQ(ranges[0], IndexRange(2, 3));
  EXPECT_EQ(ranges[1], IndexRange(10, 1));
  EXPECT_EQ(ranges[2], IndexRange(40, 2));
}

TEST(index_mask, ToRange)
{
  IndexMaskMemory memory;
  {
    const IndexMask mask = IndexMask::from_indices<int>({4, 5, 6, 7}, memory);
    EXPECT_TRUE(mask.to_range().has_value());
    EXPECT_EQ(*mask.to_range(), IndexRange(4, 4));
  }
  {
    const IndexMask mask = IndexMask::from_indices<int>({}, memory);
    EXPECT_TRUE(mask.to_range().has_value());
    EXPECT_EQ(*mask.to_range(), IndexRange());
  }
  {
    const IndexMask mask = IndexMask::from_indices<int>({0, 1, 3, 4}, memory);
    EXPECT_FALSE(mask.to_range().has_value());
  }
  {
    const IndexRange range{16000, 40000};
    const IndexMask mask{range};
    EXPECT_TRUE(mask.to_range().has_value());
    EXPECT_EQ(*mask.to_range(), range);
  }
}

TEST(index_mask, FromRange)
{
  const auto test_range = [](const IndexRange range) {
    const IndexMask mask = range;
    EXPECT_EQ(mask.to_range(), range);
  };

  test_range({0, 0});
  test_range({0, 10});
  test_range({0, 16384});
  test_range({16320, 64});
  test_range({16384, 64});
  test_range({0, 100000});
  test_range({100000, 100000});
  test_range({688064, 64});
}

TEST(index_mask, FromPredicate)
{
  IndexMaskMemory memory;
  {
    const IndexRange range{20'000, 50'000};
    const IndexMask mask = IndexMask::from_predicate(
        IndexRange(100'000), GrainSize(1024), memory, [&](const int64_t i) {
          return range.contains(i);
        });
    EXPECT_EQ(mask.to_range(), range);
  }
  {
    const Vector<int64_t> indices = {0, 500, 20'000, 50'000};
    const IndexMask mask = IndexMask::from_predicate(
        IndexRange(100'000), GrainSize(1024), memory, [&](const int64_t i) {
          return indices.contains(i);
        });
    EXPECT_EQ(mask.size(), indices.size());
    Vector<int64_t> new_indices(mask.size());
    mask.to_indices<int64_t>(new_indices);
    EXPECT_EQ(indices, new_indices);
  }
}

TEST(index_mask, IndexIteratorConversionFuzzy)
{
  RandomNumberGenerator rng;

  Vector<int64_t> indices;
  indices.append(5);
  for ([[maybe_unused]] const int64_t i : IndexRange(1000)) {
    for ([[maybe_unused]] const int64_t j :
         IndexRange(indices.last() + 1 + rng.get_int32(1000), rng.get_int32(64)))
    {
      indices.append(j);
    }
  }

  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_indices<int64_t>(indices, memory);
  EXPECT_EQ(mask.size(), indices.size());

  for ([[maybe_unused]] const int64_t _ : IndexRange(100)) {
    const int64_t index = rng.get_int32(int(indices.size()));
    const RawMaskIterator it = mask.index_to_iterator(index);
    EXPECT_EQ(mask[it], indices[index]);
    const int64_t new_index = mask.iterator_to_index(it);
    EXPECT_EQ(index, new_index);
  }

  for ([[maybe_unused]] const int64_t _ : IndexRange(100)) {
    const int64_t start = rng.get_int32(int(indices.size() - 1));
    const int64_t size = 1 + rng.get_int32(int(indices.size() - start - 1));
    const IndexMask sub_mask = mask.slice(start, size);
    const int64_t index = rng.get_int32(int(sub_mask.size()));
    const RawMaskIterator it = sub_mask.index_to_iterator(index);
    EXPECT_EQ(sub_mask[it], indices[start + index]);
    const int64_t new_index = sub_mask.iterator_to_index(it);
    EXPECT_EQ(index, new_index);
  }

  for ([[maybe_unused]] const int64_t _ : IndexRange(100)) {
    const int64_t index = rng.get_int32(int(indices.size() - 1000));
    for (const int64_t offset : {0, 1, 2, 100, 500}) {
      const int64_t index_to_search = indices[index] + offset;
      const bool contained = std::binary_search(indices.begin(), indices.end(), index_to_search);
      const std::optional<RawMaskIterator> it = mask.find(index_to_search);
      EXPECT_EQ(contained, it.has_value());
      if (contained) {
        EXPECT_EQ(index_to_search, mask[*it]);
      }
    }
  }
}

TEST(index_mask, FromPredicateFuzzy)
{
  RandomNumberGenerator rng;
  Set<int> values;

  for ([[maybe_unused]] const int64_t _ : IndexRange(10000)) {
    values.add(rng.get_int32(100'000));
  }

  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_predicate(
      IndexRange(110'000), GrainSize(1024), memory, [&](const int64_t i) {
        return values.contains(int(i));
      });
  EXPECT_EQ(mask.size(), values.size());
  for (const int index : values) {
    EXPECT_TRUE(mask.contains(index));
  }
  mask.foreach_index([&](const int64_t index, const int64_t pos) {
    EXPECT_TRUE(values.contains(int(index)));
    EXPECT_EQ(index, mask[pos]);
  });
}

TEST(index_mask, Complement)
{
  IndexMaskMemory memory;
  {
    const IndexMask mask(0);
    const IndexMask complement = mask.complement(IndexRange(100), memory);
    EXPECT_EQ(100 - mask.size(), complement.size());
    complement.foreach_index([&](const int64_t i) { EXPECT_FALSE(mask.contains(i)); });
    mask.foreach_index([&](const int64_t i) { EXPECT_FALSE(complement.contains(i)); });
  }
  {
    const IndexMask mask(10000);
    const IndexMask complement = mask.complement(IndexRange(10000), memory);
    EXPECT_EQ(10000 - mask.size(), complement.size());
    complement.foreach_index([&](const int64_t i) { EXPECT_FALSE(mask.contains(i)); });
    mask.foreach_index([&](const int64_t i) { EXPECT_FALSE(complement.contains(i)); });
  }
  {
    const IndexMask mask(IndexRange(100, 900));
    const IndexMask complement = mask.complement(IndexRange(1000), memory);
    EXPECT_EQ(1000 - mask.size(), complement.size());
    complement.foreach_index([&](const int64_t i) { EXPECT_FALSE(mask.contains(i)); });
    mask.foreach_index([&](const int64_t i) { EXPECT_FALSE(complement.contains(i)); });
  }
  {
    const IndexMask mask(IndexRange(0, 900));
    const IndexMask complement = mask.complement(IndexRange(1000), memory);
    EXPECT_EQ(1000 - mask.size(), complement.size());
    complement.foreach_index([&](const int64_t i) { EXPECT_FALSE(mask.contains(i)); });
    mask.foreach_index([&](const int64_t i) { EXPECT_FALSE(complement.contains(i)); });
  }
}

TEST(index_mask, ComplementFuzzy)
{
  RandomNumberGenerator rng;

  const int64_t mask_size = 100;
  const int64_t iter_num = 100;
  const int64_t universe_size = 110;

  for (const int64_t iter : IndexRange(iter_num)) {
    Set<int> values;
    for ([[maybe_unused]] const int64_t _ : IndexRange(iter)) {
      values.add(rng.get_int32(mask_size));
    }
    IndexMaskMemory memory;
    const IndexMask mask = IndexMask::from_predicate(
        IndexRange(mask_size), GrainSize(1024), memory, [&](const int64_t i) {
          return values.contains(int(i));
        });

    const IndexMask complement = mask.complement(IndexRange(universe_size), memory);
    EXPECT_EQ(universe_size - mask.size(), complement.size());
    complement.foreach_index([&](const int64_t i) { EXPECT_FALSE(mask.contains(i)); });
    mask.foreach_index([&](const int64_t i) { EXPECT_FALSE(complement.contains(i)); });
  }
}

}  // namespace blender::index_mask::tests
