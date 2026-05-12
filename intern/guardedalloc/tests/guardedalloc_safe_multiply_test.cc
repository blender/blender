/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "MEM_safe_multiply.h"

namespace {

template<typename T> void expect_ok(T a, T b, T expected)
{
  T result = T(123);
  EXPECT_TRUE(MEM_size_safe_multiply(a, b, &result));
  EXPECT_EQ(result, expected);
}

template<typename T> void expect_overflow(T a, T b)
{
  T result = T(123);
  EXPECT_FALSE(MEM_size_safe_multiply(a, b, &result));
  EXPECT_EQ(result, T(0));
}

}  // namespace

TEST(safe_multiply, SizeT)
{
  /* Value near overflow that should just fit. */
  const size_t near_overflow = (size_t(1) << (sizeof(size_t) * 8 / 2)) - 1;

  expect_ok<size_t>(0, 0, 0);
  expect_ok<size_t>(0, 42, 0);
  expect_ok<size_t>(42, 0, 0);
  expect_ok<size_t>(0, SIZE_MAX, 0);
  expect_ok<size_t>(SIZE_MAX, 0, 0);
  expect_ok<size_t>(1, 1, 1);
  expect_ok<size_t>(123, 456, 123 * 456);
  expect_ok<size_t>(1, SIZE_MAX, SIZE_MAX);
  expect_ok<size_t>(SIZE_MAX, 1, SIZE_MAX);
  expect_ok<size_t>(near_overflow, near_overflow, near_overflow * near_overflow);
  expect_ok<size_t>(near_overflow + 1, 2, (near_overflow + 1) * 2);
  expect_ok<size_t>(SIZE_MAX / 2, 2, (SIZE_MAX / 2) * 2);
  expect_ok<size_t>(SIZE_MAX / 1234567, 1234567, (SIZE_MAX / 1234567) * 1234567);

  expect_overflow<size_t>(SIZE_MAX, 2);
  expect_overflow<size_t>(2, SIZE_MAX);
  expect_overflow<size_t>(SIZE_MAX, SIZE_MAX);
  expect_overflow<size_t>(SIZE_MAX / 2 + 1, 2);
  expect_overflow<size_t>(SIZE_MAX, 12345567);
}

TEST(safe_multiply, Int64)
{
  expect_ok<int64_t>(0, 0, 0);
  expect_ok<int64_t>(0, 42, 0);
  expect_ok<int64_t>(42, 0, 0);
  expect_ok<int64_t>(1, 1, 1);
  expect_ok<int64_t>(123, 456, 123 * 456);
  expect_ok<int64_t>(2147483647, 2147483647, int64_t(2147483647) * 2147483647);

  if constexpr (sizeof(size_t) >= sizeof(int64_t)) {
    /* OK on 64 bit. */
    expect_ok<int64_t>(1, INT64_MAX, INT64_MAX);
    expect_ok<int64_t>(int64_t(1) << 31, 2, int64_t(1) << 32);
    expect_ok<int64_t>(INT64_MAX / 2, 2, (INT64_MAX / 2) * 2);
  }
  else {
    /* Overflow on 32 bit. */
    expect_overflow<int64_t>(1, INT64_MAX);
    expect_overflow<int64_t>(int64_t(1) << 31, 2);
    expect_overflow<int64_t>(INT64_MAX / 2, 2);
  }

  expect_overflow<int64_t>(-1, 1);
  expect_overflow<int64_t>(1, -1);
  expect_overflow<int64_t>(-5, -5);
  expect_overflow<int64_t>(INT64_MIN, 1);
  expect_overflow<int64_t>(-1000000, 1000000);
  expect_overflow<int64_t>(INT64_MAX, 2);
  expect_overflow<int64_t>(2, INT64_MAX);
  expect_overflow<int64_t>(INT64_MAX, INT64_MAX);
  expect_overflow<int64_t>(INT64_MAX / 2 + 1, 2);
}
