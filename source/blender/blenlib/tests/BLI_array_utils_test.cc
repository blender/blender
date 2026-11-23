/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <limits>

#include "testing/testing.h"

#include "BLI_array_utils.hh"
#include "BLI_array_utils_c.hh"
#include "BLI_map.hh"
#include "BLI_utildefines.hh"
#include "BLI_utildefines_stack.hh"
#include "BLI_vector_set.hh"
#include "BLI_virtual_array.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/* tests */

/* BLI_array_reverse */
TEST(array_utils, ReverseStringEmpty)
{
  char data[] = "";
  BLI_array_reverse(data, ARRAY_SIZE(data) - 1);
  EXPECT_STREQ("", data);
}

TEST(array_utils, ReverseStringSingle)
{
  char data[] = "0";
  BLI_array_reverse(data, ARRAY_SIZE(data) - 1);
  EXPECT_STREQ("0", data);
}

TEST(array_utils, ReverseString4)
{
  char data[] = "0123";
  BLI_array_reverse(data, ARRAY_SIZE(data) - 1);
  EXPECT_STREQ("3210", data);
}

TEST(array_utils, ReverseInt4)
{
  const int data_cmp[] = {3, 2, 1, 0};
  int data[] = {0, 1, 2, 3};
  BLI_array_reverse(data, ARRAY_SIZE(data));
  EXPECT_EQ_ARRAY(data_cmp, data, ARRAY_SIZE(data));
}

/* BLI_array_findindex */
TEST(array_utils, FindIndexStringEmpty)
{
  char data[] = "", find = '0';
  EXPECT_EQ(BLI_array_findindex(data, ARRAY_SIZE(data) - 1, &find), -1);
  EXPECT_EQ(BLI_array_rfindindex(data, ARRAY_SIZE(data) - 1, &find), -1);
}

TEST(array_utils, FindIndexStringSingle)
{
  char data[] = "0", find = '0';
  EXPECT_EQ(BLI_array_findindex(data, ARRAY_SIZE(data) - 1, &find), 0);
  EXPECT_EQ(BLI_array_rfindindex(data, ARRAY_SIZE(data) - 1, &find), 0);
}

TEST(array_utils, FindIndexStringSingleMissing)
{
  char data[] = "1", find = '0';
  EXPECT_EQ(BLI_array_findindex(data, ARRAY_SIZE(data) - 1, &find), -1);
  EXPECT_EQ(BLI_array_rfindindex(data, ARRAY_SIZE(data) - 1, &find), -1);
}

TEST(array_utils, FindIndexString4)
{
  char data[] = "0123", find = '3';
  EXPECT_EQ(BLI_array_findindex(data, ARRAY_SIZE(data) - 1, &find), 3);
  EXPECT_EQ(BLI_array_rfindindex(data, ARRAY_SIZE(data) - 1, &find), 3);
}

TEST(array_utils, FindIndexInt4)
{
  int data[] = {0, 1, 2, 3}, find = 3;
  EXPECT_EQ(BLI_array_findindex(data, ARRAY_SIZE(data), &find), 3);
  EXPECT_EQ(BLI_array_rfindindex(data, ARRAY_SIZE(data), &find), 3);
}

TEST(array_utils, FindIndexInt4_DupeEnd)
{
  int data[] = {0, 1, 2, 0}, find = 0;
  EXPECT_EQ(BLI_array_findindex(data, ARRAY_SIZE(data), &find), 0);
  EXPECT_EQ(BLI_array_rfindindex(data, ARRAY_SIZE(data), &find), 3);
}

TEST(array_utils, FindIndexInt4_DupeMid)
{
  int data[] = {1, 0, 0, 3}, find = 0;
  EXPECT_EQ(BLI_array_findindex(data, ARRAY_SIZE(data), &find), 1);
  EXPECT_EQ(BLI_array_rfindindex(data, ARRAY_SIZE(data), &find), 2);
}

TEST(array_utils, FindIndexPointer)
{
  const char *data[4] = {nullptr};
  STACK_DECLARE(data);

  STACK_INIT(data, ARRAY_SIZE(data));

  const char *a = "a", *b = "b", *c = "c", *d = "d";

#define STACK_PUSH_AND_CHECK_FORWARD(v, i) \
  { \
    STACK_PUSH(data, v); \
    EXPECT_EQ(BLI_array_findindex(data, STACK_SIZE(data), &(v)), i); \
  } \
  ((void)0)

#define STACK_PUSH_AND_CHECK_BACKWARD(v, i) \
  { \
    STACK_PUSH(data, v); \
    EXPECT_EQ(BLI_array_rfindindex(data, STACK_SIZE(data), &(v)), i); \
  } \
  ((void)0)

#define STACK_PUSH_AND_CHECK_BOTH(v, i) \
  { \
    STACK_PUSH(data, v); \
    EXPECT_EQ(BLI_array_findindex(data, STACK_SIZE(data), &(v)), i); \
    EXPECT_EQ(BLI_array_rfindindex(data, STACK_SIZE(data), &(v)), i); \
  } \
  ((void)0)

  STACK_PUSH_AND_CHECK_BOTH(a, 0);
  STACK_PUSH_AND_CHECK_BOTH(b, 1);
  STACK_PUSH_AND_CHECK_BOTH(c, 2);
  STACK_PUSH_AND_CHECK_BOTH(d, 3);

  STACK_POP(data);
  STACK_PUSH_AND_CHECK_BACKWARD(a, 3);

  STACK_POP(data);
  STACK_PUSH_AND_CHECK_FORWARD(a, 0);

  STACK_POP(data);
  STACK_POP(data);

  STACK_PUSH_AND_CHECK_BACKWARD(b, 2);
  STACK_PUSH_AND_CHECK_BACKWARD(a, 3);

#undef STACK_PUSH_AND_CHECK_FORWARD
#undef STACK_PUSH_AND_CHECK_BACKWARD
#undef STACK_PUSH_AND_CHECK_BOTH
}

/* BLI_array_binary_and */
#define BINARY_AND_TEST(data_cmp, data_a, data_b, data_combine, length) \
  { \
    BLI_array_binary_and(data_combine, data_a, data_b, length); \
    EXPECT_EQ_ARRAY(data_cmp, data_combine, length); \
  } \
  ((void)0)

TEST(array_utils, BinaryAndInt4Zero)
{
  const int data_cmp[] = {0, 0, 0, 0};
  int data_a[] = {0, 1, 0, 1}, data_b[] = {1, 0, 1, 0};
  int data_combine[ARRAY_SIZE(data_cmp)];
  BINARY_AND_TEST(data_cmp, data_a, data_b, data_combine, ARRAY_SIZE(data_cmp));
}

TEST(array_utils, BinaryAndInt4Mix)
{
  const int data_cmp[] = {1, 0, 1, 0};
  int data_a[] = {1, 1, 1, 1}, data_b[] = {1, 0, 1, 0};
  int data_combine[ARRAY_SIZE(data_cmp)];
  BINARY_AND_TEST(data_cmp, data_a, data_b, data_combine, ARRAY_SIZE(data_cmp));
}
#undef BINARY_AND_TEST

/* BLI_array_binary_or */
#define BINARY_OR_TEST(data_cmp, data_a, data_b, data_combine, length) \
  { \
    BLI_array_binary_or(data_combine, data_a, data_b, length); \
    EXPECT_EQ_ARRAY(data_combine, data_cmp, length); \
  } \
  ((void)0)

TEST(array_utils, BinaryOrInt4Alternate)
{
  int data_a[] = {0, 1, 0, 1}, data_b[] = {1, 0, 1, 0}, data_cmp[] = {1, 1, 1, 1};
  int data_combine[ARRAY_SIZE(data_cmp)];
  BINARY_OR_TEST(data_cmp, data_a, data_b, data_combine, ARRAY_SIZE(data_cmp));
}

TEST(array_utils, BinaryOrInt4Mix)
{
  int data_a[] = {1, 1, 0, 0}, data_b[] = {0, 0, 1, 0}, data_cmp[] = {1, 1, 1, 0};
  int data_combine[ARRAY_SIZE(data_cmp)];
  BINARY_OR_TEST(data_cmp, data_a, data_b, data_combine, ARRAY_SIZE(data_cmp));
}
#undef BINARY_OR_TEST

static void find_all_ranges_test(const Span<bool> data, const Span<IndexRange> data_cmp)
{
  Vector<IndexRange> ranges = array_utils::find_all_ranges(data, true);
  EXPECT_EQ(ranges.size(), data_cmp.size());
  EXPECT_EQ_ARRAY(data_cmp.data(), ranges.as_span().data(), data_cmp.size());
}

TEST(array_utils, FindAllRanges1)
{
  const std::array data = {false};
  Vector<IndexRange> ranges = array_utils::find_all_ranges(Span(data.data(), data.size()), true);
  EXPECT_EQ(ranges.size(), 0);
}

TEST(array_utils, FindAllRanges2)
{
  const std::array data = {true, true, true};
  const std::array data_cmp = {IndexRange(0, 3)};
  find_all_ranges_test(data, data_cmp);
}

TEST(array_utils, FindAllRanges3)
{
  const std::array data = {true, false};
  const std::array data_cmp = {IndexRange(0, 1)};
  find_all_ranges_test(data, data_cmp);
}

TEST(array_utils, FindAllRanges4)
{
  const std::array data = {false, true};
  const std::array data_cmp = {IndexRange(1, 1)};
  find_all_ranges_test(data, data_cmp);
}

TEST(array_utils, FindAllRanges5)
{
  const std::array data = {true, false, false, true, true, false, true};
  const std::array data_cmp = {IndexRange(0, 1), IndexRange(3, 2), IndexRange(6, 1)};
  find_all_ranges_test(data, data_cmp);
}

static void find_max_element_test(const VArray<int> &values,
                                  const std::optional<int64_t> expect_index)
{
  const std::optional<int64_t> result_index = array_utils::max_element_index(values);
  EXPECT_TRUE(result_index.has_value() == expect_index.has_value());
  if (result_index.has_value() && expect_index.has_value()) {
    EXPECT_EQ(result_index.value(), expect_index.value());
  }
}

TEST(array_utils, FindMaxElement1)
{
  find_max_element_test(VArray<int>::from_span({}), std::nullopt);
}

TEST(array_utils, FindMaxElement2)
{
  const std::array data{1};
  find_max_element_test(VArray<int>::from_span(data), 0);
}

TEST(array_utils, FindMaxElement3)
{
  const std::array data{1, 2, 2, 1};
  find_max_element_test(VArray<int>::from_span(data), 1);
}

TEST(array_utils, FindMaxElement4)
{
  /* Inverted parabola with roots 0 and 9 and a peak at (x = 4.5, y = 20.25). */
  find_max_element_test(VArray<int>::from_func(10, [](const int x) { return -x * x + 9 * x; }), 4);
}

static void group_ids_to_indices_test(const Span<int> ids, const IndexMask &mask)
{
  /* Compare with the simpler #VectorSet implementation. */
  VectorSet<int> expected_ids;
  Vector<int> expected_first_indices;
  Vector<int> expected_group_indices;
  mask.foreach_index([&](const int i) {
    if (expected_ids.add(ids[i])) {
      expected_first_indices.append(i);
    }
    expected_group_indices.append(expected_ids.index_of(ids[i]));
  });

  Array<int> group_indices(mask.size());
  Vector<int> first_indices;
  const int groups_num = array_utils::group_ids_to_indices(
      ids, mask, group_indices, &first_indices);
  EXPECT_EQ(groups_num, expected_ids.size());
  EXPECT_EQ(first_indices.as_span(), expected_first_indices.as_span());
  Array<int> group_indices_only(mask.size());
  EXPECT_EQ(array_utils::group_ids_to_indices(ids, mask, group_indices_only), groups_num);
  EXPECT_EQ(group_indices_only.as_span(), expected_group_indices.as_span());
  EXPECT_EQ(group_indices.as_span(), expected_group_indices.as_span());
}

TEST(array_utils, GroupIdsToIndicesEmpty)
{
  group_ids_to_indices_test({}, IndexMask());
}

TEST(array_utils, GroupIdsToIndicesDense)
{
  const std::array ids{5, 3, 5, -2, 3, 3, 0, -2};
  group_ids_to_indices_test(ids, IndexMask(ids.size()));
}

TEST(array_utils, GroupIdsToIndicesSparse)
{
  const std::array ids{1000000, 3, 1000000, -2000000, 3, 7};
  group_ids_to_indices_test(ids, IndexMask(ids.size()));
}

TEST(array_utils, GroupIdsToIndicesMask)
{
  const std::array ids{4, 1, 4, 2, 1, 2, 4, 3};
  IndexMaskMemory memory;
  group_ids_to_indices_test(ids, IndexMask::from_indices<int>({1, 2, 5, 6, 7}, memory));
}

TEST(array_utils, GroupIdsToIndicesLarge)
{
  Array<int> dense_ids(100000);
  Array<int> sparse_ids(100000);
  for (const int i : dense_ids.index_range()) {
    dense_ids[i] = (i * 7919) % 20011;
    sparse_ids[i] = ((i * 7919) % 1009) * 1000003;
  }
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_predicate(
      dense_ids.index_range(), memory, [](const int i) { return i % 3 != 0; });
  group_ids_to_indices_test(dense_ids, IndexMask(dense_ids.size()));
  group_ids_to_indices_test(dense_ids, mask);
  group_ids_to_indices_test(sparse_ids, IndexMask(sparse_ids.size()));
  group_ids_to_indices_test(sparse_ids, mask);
}

static void map_ids_to_indices_test(const Span<int> src_ids, const Span<int> dst_ids)
{
  /* Compare with a simple #Map implementation. */
  Map<int, int> expected_map;
  for (const int i : src_ids.index_range()) {
    expected_map.add(src_ids[i], i);
  }
  Array<int> expected(dst_ids.size());
  for (const int i : dst_ids.index_range()) {
    expected[i] = expected_map.lookup_default(dst_ids[i], -1);
  }

  Array<int> result(dst_ids.size(), -999);
  array_utils::map_ids_to_indices(src_ids, dst_ids, result);
  EXPECT_EQ(result.as_span(), expected.as_span());
}

TEST(array_utils, MapIdsToIndicesEmpty)
{
  map_ids_to_indices_test({}, {});
  const std::array ids{0, 1, 2};
  map_ids_to_indices_test({}, ids);
  map_ids_to_indices_test(ids, {});
}

TEST(array_utils, MapIdsToIndicesDense)
{
  const std::array src_dup{0, 0, 1};
  const std::array dst_dup{0, 1, 2, -1};
  map_ids_to_indices_test(src_dup, dst_dup);

  const std::array src{5, 3, 5, -2, -1, 3, 0, -2};
  const std::array dst{-10, -2, -1, 0, 1, 2, 3, 4, 5, 10};
  map_ids_to_indices_test(src, dst);
}

TEST(array_utils, MapIdsToIndicesSparse)
{
  const int min_i = std::numeric_limits<int>::min();
  const int max_i = std::numeric_limits<int>::max();
  const std::array src{1000000, -1, 1000000, -2, min_i, max_i, -2, 7};
  const std::array dst{min_i, -2, -1, 0, 7, 42, 1000000, max_i};
  map_ids_to_indices_test(src, dst);
}

TEST(array_utils, MapIdsToIndicesExtremeDense)
{
  const int min_i = std::numeric_limits<int>::min();
  const int max_i = std::numeric_limits<int>::max();
  const std::array src_min{min_i + 2, min_i, min_i + 2, min_i + 1};
  const std::array dst_min{min_i, min_i + 1, min_i + 2, min_i + 3, max_i};
  map_ids_to_indices_test(src_min, dst_min);

  const std::array src_max{max_i, max_i - 2, max_i, max_i - 1};
  const std::array dst_max{min_i, max_i - 3, max_i - 2, max_i - 1, max_i};
  map_ids_to_indices_test(src_max, dst_max);
}

TEST(array_utils, MapIdsToIndicesLarge)
{
  /* Large enough to exercise the 4096 parallel grain size while staying fast in debug builds. */
  Array<int> dense_src(12000);
  Array<int> dense_dst(12000);
  Array<int> sparse_src(12000);
  Array<int> sparse_dst(12000);
  for (const int i : dense_src.index_range()) {
    dense_src[i] = (i * 7919) % 4001;
    dense_dst[i] = (i * 10007) % 5000 - 500;
    sparse_src[i] = ((i * 7919) % 4001) * 30011;
    sparse_dst[i] = ((i * 10007) % 5000) * 30011;
  }
  map_ids_to_indices_test(dense_src, dense_dst);
  map_ids_to_indices_test(sparse_src, sparse_dst);
}

}  // namespace blender
