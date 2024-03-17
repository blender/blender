/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_array.hh"
#include "BLI_index_mask_expression.hh"
#include "BLI_rand.hh"
#include "BLI_set.hh"
#include "BLI_strict_flags.h"
#include "BLI_timeit.hh"

#include "testing/testing.h"

namespace blender::index_mask::tests {

TEST(index_mask_expression, Union)
{
  IndexMaskMemory memory;
  const IndexMask mask_a = IndexMask::from_initializers({5, IndexRange(50, 100), 100'000}, memory);
  const IndexMask mask_b = IndexMask::from_initializers({IndexRange(10, 10), 60, 200}, memory);

  ExprBuilder builder;
  const Expr &expr = builder.merge({&mask_a, &mask_b});
  const IndexMask union_mask = evaluate_expression(expr, memory);

  EXPECT_EQ(union_mask,
            IndexMask::from_initializers(
                {5, IndexRange(10, 10), IndexRange(50, 100), 200, 100'000}, memory));
}

TEST(index_mask_expression, UnionMulti)
{
  IndexMaskMemory memory;
  const IndexMask mask_a = IndexMask::from_initializers({3, 5, 6, 8, 9}, memory);
  const IndexMask mask_b = IndexMask::from_initializers({4, 6, 7, 12}, memory);
  const IndexMask mask_c = IndexMask::from_initializers({0, 5}, memory);
  const IndexMask mask_d = IndexMask::from_initializers({6, 7, 10}, memory);

  ExprBuilder builder;
  const Expr &expr = builder.merge({&mask_a, &mask_b, &mask_c, &mask_d});
  const IndexMask union_mask = evaluate_expression(expr, memory);

  EXPECT_EQ(union_mask, IndexMask::from_initializers({0, 3, 4, 5, 6, 7, 8, 9, 10, 12}, memory));
}

TEST(index_mask_expression, IntersectMulti)
{
  IndexMaskMemory memory;
  const IndexMask mask_a = IndexMask::from_initializers({3, 5, 6, 8, 9}, memory);
  const IndexMask mask_b = IndexMask::from_initializers({2, 5, 6, 10}, memory);
  const IndexMask mask_c = IndexMask::from_initializers({4, 5, 6}, memory);
  const IndexMask mask_d = IndexMask::from_initializers({1, 5, 10}, memory);

  ExprBuilder builder;
  const Expr &expr = builder.intersect({&mask_a, &mask_b, &mask_c, &mask_d});
  const IndexMask intersect_mask = evaluate_expression(expr, memory);

  EXPECT_EQ(intersect_mask, IndexMask::from_initializers({5}, memory));
}

TEST(index_mask_expression, DifferenceMulti)
{
  IndexMaskMemory memory;
  const IndexMask mask_a = IndexMask::from_initializers({1, 2, 3, 5, 6, 7, 9, 10}, memory);
  const IndexMask mask_b = IndexMask::from_initializers({2, 5, 6, 10}, memory);
  const IndexMask mask_c = IndexMask::from_initializers({4, 5, 6}, memory);
  const IndexMask mask_d = IndexMask::from_initializers({1, 5, 10}, memory);

  ExprBuilder builder;
  const Expr &expr = builder.subtract(&mask_a, {&mask_b, &mask_c, &mask_d});
  const IndexMask difference_mask = evaluate_expression(expr, memory);

  EXPECT_EQ(difference_mask, IndexMask::from_initializers({3, 7, 9}, memory));
}

TEST(index_mask_expression, Intersection)
{
  IndexMaskMemory memory;
  const IndexMask mask_a = IndexMask::from_initializers({5, IndexRange(50, 100), 100'000}, memory);
  const IndexMask mask_b = IndexMask::from_initializers(
      {5, 6, IndexRange(100, 100), 80000, 100'000}, memory);

  ExprBuilder builder;
  const Expr &expr = builder.intersect({&mask_a, &mask_b});
  const IndexMask intersection_mask = evaluate_expression(expr, memory);

  EXPECT_EQ(intersection_mask,
            IndexMask::from_initializers({5, IndexRange(100, 50), 100'000}, memory));
}

TEST(index_mask_expression, Difference)
{
  IndexMaskMemory memory;
  const IndexMask mask_a = IndexMask::from_initializers({5, IndexRange(50, 100), 100'000}, memory);
  const IndexMask mask_b = IndexMask::from_initializers({5, 60, IndexRange(100, 20)}, memory);

  ExprBuilder builder;
  const Expr &expr = builder.subtract(&mask_a, {&mask_b});
  const IndexMask difference_mask = evaluate_expression(expr, memory);

  EXPECT_EQ(difference_mask,
            IndexMask::from_initializers(
                {IndexRange(50, 10), IndexRange(61, 39), IndexRange(120, 30), 100'000}, memory));
}

TEST(index_mask_expression, FizzBuzz)
{
  IndexMaskMemory memory;
  const IndexMask mask_3 = IndexMask::from_every_nth(3, 11, 0, memory); /* 0 - 30 */
  const IndexMask mask_5 = IndexMask::from_every_nth(5, 11, 0, memory); /* 0 - 50 */

  {
    ExprBuilder builder;
    const Expr &expr = builder.merge({&mask_3, &mask_5});
    const IndexMask result = evaluate_expression(expr, memory);
    EXPECT_EQ(
        result,
        IndexMask::from_initializers(
            {0, 3, 5, 6, 9, 10, 12, 15, 18, 20, 21, 24, 25, 27, 30, 35, 40, 45, 50}, memory));
  }
  {
    ExprBuilder builder;
    const Expr &expr = builder.intersect({&mask_3, &mask_5});
    const IndexMask result = evaluate_expression(expr, memory);
    EXPECT_EQ(result, IndexMask::from_initializers({0, 15, 30}, memory));
  }
  {
    ExprBuilder builder;
    const Expr &expr = builder.subtract(&mask_3, {&mask_5});
    const IndexMask result = evaluate_expression(expr, memory);
    EXPECT_EQ(result, IndexMask::from_initializers({3, 6, 9, 12, 18, 21, 24, 27}, memory));
  }
  {
    ExprBuilder builder;
    const Expr &expr = builder.merge(
        {&builder.intersect({&mask_3, &mask_5}), &builder.subtract(&mask_3, {&mask_5})});
    const IndexMask &result = evaluate_expression(expr, memory);
    EXPECT_EQ(result,
              IndexMask::from_initializers({0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30}, memory));
  }
}

TEST(index_mask_expression, UnionToFullRange)
{
  IndexMaskMemory memory;
  const IndexMask mask_1 = IndexMask::from_initializers({2, 4, 5, 7}, memory);
  const IndexMask mask_2 = IndexMask::from_initializers({6, 8}, memory);
  const IndexMask mask_3 = IndexMask::from_initializers({1, 3}, memory);

  ExprBuilder builder;
  const Expr &expr = builder.merge({&mask_1, &mask_2, &mask_3});
  const IndexMask result = evaluate_expression(expr, memory);
  EXPECT_TRUE(result.to_range().has_value());
  EXPECT_EQ(*result.to_range(), IndexRange::from_begin_end_inclusive(1, 8));
  EXPECT_EQ(result.segments_num(), 1);
}

TEST(index_mask_expression, UnionIndividualIndices)
{
  IndexMaskMemory memory;
  const IndexMask mask_1 = IndexMask::from_initializers({3}, memory);
  const IndexMask mask_2 = IndexMask::from_initializers({6}, memory);
  const IndexMask mask_3 = IndexMask::from_initializers({5}, memory);

  ExprBuilder builder;
  const Expr &expr = builder.merge({&mask_1, &mask_2, &mask_3});
  const IndexMask result = evaluate_expression(expr, memory);
  EXPECT_EQ(result, IndexMask::from_initializers({3, 5, 6}, memory));
  EXPECT_EQ(result.segments_num(), 1);
}

TEST(index_mask_expression, UnionLargeRanges)
{
  IndexMaskMemory memory;
  const IndexMask mask_a(IndexRange(0, 1'000'000));
  const IndexMask mask_b(IndexRange(900'000, 1'100'000));

  ExprBuilder builder;
  const Expr &expr = builder.merge({&mask_a, &mask_b});
  const IndexMask result_mask = evaluate_expression(expr, memory);

  EXPECT_EQ(result_mask, IndexMask(IndexRange(0, 2'000'000)));
}

TEST(index_mask_expression, SubtractSmall)
{
  IndexMaskMemory memory;
  const IndexMask mask_a = IndexMask::from_initializers({3, 4, 5, 6, 7, 8, 9}, memory);
  const IndexMask mask_b = IndexMask::from_initializers({5, 7}, memory);
  const IndexMask mask_c = IndexMask::from_initializers({8}, memory);

  ExprBuilder builder;
  const Expr &expr = builder.subtract(&mask_a, {&mask_b, &mask_c});
  const IndexMask result = evaluate_expression(expr, memory);

  EXPECT_EQ(result, IndexMask::from_initializers({3, 4, 6, 9}, memory));
  EXPECT_EQ(result.segments_num(), 1);
}

TEST(index_mask_expression, RangeTerms)
{
  IndexMaskMemory memory;
  ExprBuilder builder;

  const IndexRange range_a = IndexRange::from_begin_end(30'000, 50'000);
  const IndexRange range_b = IndexRange::from_begin_end(40'000, 100'000);
  const IndexRange range_c = IndexRange::from_begin_end(45'000, 48'000);

  const Expr &expr = builder.subtract(&builder.merge({range_a, range_b}), {range_c});
  const IndexMask result_mask = evaluate_expression(expr, memory);

  EXPECT_EQ(result_mask,
            IndexMask::from_initializers({IndexRange::from_begin_end(30'000, 45'000),
                                          IndexRange::from_begin_end(48'000, 100'000)},
                                         memory));
}

TEST(index_mask_expression, SingleMask)
{
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_initializers({5, 6, 8, 9}, memory);

  ExprBuilder builder;
  const Expr &expr = builder.merge({&mask});
  const IndexMask result = evaluate_expression(expr, memory);

  EXPECT_EQ(result, mask);
}

TEST(index_mask_expression, SubtractSelf)
{
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask ::from_initializers({6, 8, 10, 100}, memory);

  ExprBuilder builder;
  const Expr &expr = builder.subtract(&mask, {&mask});
  const IndexMask result = evaluate_expression(expr, memory);

  EXPECT_TRUE(result.is_empty());
}

/* Disable benchmark by default. */
#if 0
TEST(index_mask_expression, Benchmark)
{
#  ifdef NDEBUG
  const int64_t iterations = 100;
#  else
  const int64_t iterations = 1;
#  endif

  for ([[maybe_unused]] const int64_t _1 : IndexRange(5)) {
    IndexMaskMemory m;
    const IndexMask a = IndexMask::from_every_nth(3, 1'000'000, 0, m);
    const IndexMask b = IndexMask::from_every_nth(100, 5'000, 0, m);
    ExprBuilder builder;
    const Expr &expr = builder.merge({&a, &b});

    SCOPED_TIMER("benchmark");
    for ([[maybe_unused]] const int64_t _2 : IndexRange(iterations)) {
      IndexMaskMemory memory;
      const IndexMask result = evaluate_expression(expr, memory);
      UNUSED_VARS(result);
    }
  }
}
#endif

}  // namespace blender::index_mask::tests
