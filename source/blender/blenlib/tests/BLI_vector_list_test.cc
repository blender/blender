/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_exception_safety_test_utils.hh"
#include "BLI_vector_list.hh"
#include "testing/testing.h"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

namespace blender::tests {

TEST(vectorlist, DefaultConstructor)
{
  VectorList<int> vec;
  EXPECT_EQ(vec.size(), 0);
}

TEST(vectorlist, MoveConstructor)
{
  VectorList<int> vec1;
  vec1.append(1);
  vec1.append(2);
  vec1.append(3);
  vec1.append(4);
  VectorList<int> vec2(std::move(vec1));

  EXPECT_EQ(vec1.size(), 0); /* NOLINT: bugprone-use-after-move */
  EXPECT_EQ(vec2.size(), 4);
  EXPECT_EQ(vec2[0], 1);
  EXPECT_EQ(vec2[1], 2);
  EXPECT_EQ(vec2[2], 3);
  EXPECT_EQ(vec2[3], 4);
}

TEST(vectorlist, MoveOperator)
{
  VectorList<int> vec1;
  vec1.append(1);
  vec1.append(2);
  vec1.append(3);
  vec1.append(4);
  VectorList<int> vec2;
  vec2 = std::move(vec1);

  EXPECT_EQ(vec1.size(), 0); /* NOLINT: bugprone-use-after-move */
  EXPECT_EQ(vec2.size(), 4);
  EXPECT_EQ(vec2[0], 1);
  EXPECT_EQ(vec2[1], 2);
  EXPECT_EQ(vec2[2], 3);
  EXPECT_EQ(vec2[3], 4);
}

TEST(vectorlist, Append)
{
  VectorList<int> vec;
  vec.append(3);
  vec.append(6);
  vec.append(7);
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec[0], 3);
  EXPECT_EQ(vec[1], 6);
  EXPECT_EQ(vec[2], 7);
}

TEST(vectorlist, Iterator)
{
  VectorList<int> vec;
  vec.append(1);
  vec.append(4);
  vec.append(9);
  vec.append(16);
  int i = 1;
  for (int value : vec) {
    EXPECT_EQ(value, i * i);
    i++;
  }
}

TEST(vectorlist, ConstIterator)
{
  VectorList<int> vec;
  vec.append(1);
  vec.append(4);
  vec.append(9);
  vec.append(16);
  const VectorList<int> &const_ref = vec;
  int i = 0;
  for (int value : const_ref) {
    i++;
    EXPECT_EQ(value, i * i);
  }
  EXPECT_EQ(i, 4);
}

TEST(vectorlist, LimitIterator)
{
  VectorList<int, 8, 128> vec;
  for (int64_t i : IndexRange(1024)) {
    vec.append(int(i));
  }
  int i = 0;
  for (int value : vec) {
    EXPECT_EQ(value, i);
    i++;
  }
  EXPECT_EQ(i, 1024);
}

TEST(vectorlist, IteratorAfterClear)
{
  VectorList<int, 8, 128> vec;
  for (int64_t i : IndexRange(1024)) {
    vec.append(int(i));
  }
  vec.clear();
  for (int64_t i : IndexRange(512)) {
    vec.append(int(-i));
  }
  int i = 0;
  for (int value : vec) {
    EXPECT_EQ(value, -i);
    i++;
  }
  EXPECT_EQ(i, 512);
}

TEST(vectorlist, LimitIndexing)
{
  VectorList<int, 8, 128> vec;
  for (int64_t i : IndexRange(1024)) {
    vec.append(int(i));
  }
  for (int64_t i : IndexRange(1024)) {
    EXPECT_EQ(vec[i], i);
  }
}

TEST(vectorlist, ConstLimitIndexing)
{
  VectorList<int, 8, 128> vec;
  for (int64_t i : IndexRange(1024)) {
    vec.append(int(i));
  }
  const VectorList<int, 8, 128> &const_ref = vec;
  for (int64_t i : IndexRange(1024)) {
    EXPECT_EQ(const_ref[i], i);
  }
}

static VectorList<int> return_by_value_helper()
{
  VectorList<int> vec;
  vec.append(3);
  vec.append(5);
  vec.append(1);
  return vec;
}

TEST(vectorlist, ReturnByValue)
{
  VectorList<int> vec = return_by_value_helper();
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec[0], 3);
  EXPECT_EQ(vec[1], 5);
  EXPECT_EQ(vec[2], 1);
}

TEST(vectorlist, IsEmpty)
{
  VectorList<int> vec;
  EXPECT_TRUE(vec.is_empty());
  vec.append(1);
  EXPECT_FALSE(vec.is_empty());
  vec.clear();
  EXPECT_TRUE(vec.is_empty());
}

TEST(vectorlist, First)
{
  VectorList<int> vec;
  vec.append(3);
  vec.append(5);
  vec.append(7);
  EXPECT_EQ(vec.first(), 3);
}

TEST(vectorlist, Last)
{
  VectorList<int> vec;
  vec.append(3);
  vec.append(5);
  vec.append(7);
  EXPECT_EQ(vec.last(), 7);
}

class TypeConstructMock {
 public:
  bool default_constructed = false;
  bool copy_constructed = false;
  bool move_constructed = false;
  bool copy_assigned = false;
  bool move_assigned = false;

  TypeConstructMock() : default_constructed(true) {}

  TypeConstructMock(const TypeConstructMock & /*other*/) : copy_constructed(true) {}

  TypeConstructMock(TypeConstructMock && /*other*/) noexcept : move_constructed(true) {}

  TypeConstructMock &operator=(const TypeConstructMock &other)
  {
    if (this == &other) {
      return *this;
    }

    copy_assigned = true;
    return *this;
  }

  TypeConstructMock &operator=(TypeConstructMock &&other) noexcept
  {
    if (this == &other) {
      return *this;
    }

    move_assigned = true;
    return *this;
  }
};

TEST(vectorlist, AppendCallsCopyConstructor)
{
  VectorList<TypeConstructMock> vec;
  TypeConstructMock value;
  vec.append(value);
  EXPECT_TRUE(vec[0].copy_constructed);
}

TEST(vectorlist, AppendCallsMoveConstructor)
{
  VectorList<TypeConstructMock> vec;
  vec.append(TypeConstructMock());
  EXPECT_TRUE(vec[0].move_constructed);
}

TEST(vectorlist, OveralignedValues)
{
  VectorList<AlignedBuffer<1, 512>> vec;
  for (int i = 0; i < 100; i++) {
    vec.append({});
    EXPECT_EQ(uintptr_t(&vec.last()) % 512, 0);
  }
}

TEST(vectorlist, AppendExceptions)
{
  VectorList<ExceptionThrower> vec;
  vec.append({});
  vec.append({});
  ExceptionThrower *ptr1 = &vec.last();
  ExceptionThrower value;
  value.throw_during_copy = true;
  EXPECT_ANY_THROW({ vec.append(value); });
  EXPECT_EQ(vec.size(), 2);
  ExceptionThrower *ptr2 = &vec.last();
  EXPECT_EQ(ptr1, ptr2);
}

}  // namespace blender::tests
