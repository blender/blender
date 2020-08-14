/* Apache License, Version 2.0 */

#include "BLI_array.hh"
#include "BLI_strict_flags.h"
#include "BLI_vector.hh"
#include "testing/testing.h"

namespace blender::tests {

TEST(array, DefaultConstructor)
{
  Array<int> array;
  EXPECT_EQ(array.size(), 0);
  EXPECT_TRUE(array.is_empty());
}

TEST(array, SizeConstructor)
{
  Array<int> array(5);
  EXPECT_EQ(array.size(), 5);
  EXPECT_FALSE(array.is_empty());
}

TEST(array, FillConstructor)
{
  Array<int> array(5, 8);
  EXPECT_EQ(array.size(), 5);
  EXPECT_EQ(array[0], 8);
  EXPECT_EQ(array[1], 8);
  EXPECT_EQ(array[2], 8);
  EXPECT_EQ(array[3], 8);
  EXPECT_EQ(array[4], 8);
}

TEST(array, InitializerListConstructor)
{
  Array<int> array = {4, 5, 6, 7};
  EXPECT_EQ(array.size(), 4);
  EXPECT_EQ(array[0], 4);
  EXPECT_EQ(array[1], 5);
  EXPECT_EQ(array[2], 6);
  EXPECT_EQ(array[3], 7);
}

TEST(array, SpanConstructor)
{
  int stackarray[4] = {6, 7, 8, 9};
  Span<int> span(stackarray, ARRAY_SIZE(stackarray));
  Array<int> array(span);
  EXPECT_EQ(array.size(), 4);
  EXPECT_EQ(array[0], 6);
  EXPECT_EQ(array[1], 7);
  EXPECT_EQ(array[2], 8);
  EXPECT_EQ(array[3], 9);
}

TEST(array, CopyConstructor)
{
  Array<int> array = {5, 6, 7, 8};
  Array<int> new_array(array);

  EXPECT_EQ(array.size(), 4);
  EXPECT_EQ(new_array.size(), 4);
  EXPECT_NE(array.data(), new_array.data());
  EXPECT_EQ(new_array[0], 5);
  EXPECT_EQ(new_array[1], 6);
  EXPECT_EQ(new_array[2], 7);
  EXPECT_EQ(new_array[3], 8);
}

TEST(array, MoveConstructor)
{
  Array<int> array = {5, 6, 7, 8};
  Array<int> new_array(std::move(array));

  EXPECT_EQ(array.size(), 0); /* NOLINT: bugprone-use-after-move */
  EXPECT_EQ(new_array.size(), 4);
  EXPECT_EQ(new_array[0], 5);
  EXPECT_EQ(new_array[1], 6);
  EXPECT_EQ(new_array[2], 7);
  EXPECT_EQ(new_array[3], 8);
}

TEST(array, CopyAssignment)
{
  Array<int> array = {1, 2, 3};
  Array<int> new_array = {4};
  EXPECT_EQ(new_array.size(), 1);
  new_array = array;
  EXPECT_EQ(new_array.size(), 3);
  EXPECT_EQ(array.size(), 3);
  EXPECT_NE(array.data(), new_array.data());
  EXPECT_EQ(new_array[0], 1);
  EXPECT_EQ(new_array[1], 2);
  EXPECT_EQ(new_array[2], 3);
}

TEST(array, MoveAssignment)
{
  Array<int> array = {1, 2, 3};
  Array<int> new_array = {4};
  EXPECT_EQ(new_array.size(), 1);
  new_array = std::move(array);
  EXPECT_EQ(new_array.size(), 3);
  EXPECT_EQ(array.size(), 0); /* NOLINT: bugprone-use-after-move */
  EXPECT_EQ(new_array[0], 1);
  EXPECT_EQ(new_array[1], 2);
  EXPECT_EQ(new_array[2], 3);
}

/**
 * Tests that the trivially constructible types are not zero-initialized. We do not want that for
 * performance reasons.
 */
TEST(array, TrivialTypeSizeConstructor)
{
  Array<char, 1> *array = new Array<char, 1>(1);
  char *ptr = &(*array)[0];
  array->~Array();

  const char magic = 42;
  *ptr = magic;
  EXPECT_EQ(*ptr, magic);

  new (array) Array<char, 1>(1);
  EXPECT_EQ((*array)[0], magic);
  EXPECT_EQ(*ptr, magic);
  delete array;
}

struct ConstructibleType {
  char value;

  ConstructibleType()
  {
    value = 42;
  }
};

TEST(array, NoInitializationSizeConstructor)
{
  using MyArray = Array<ConstructibleType>;

  TypedBuffer<MyArray> buffer;
  memset((void *)&buffer, 100, sizeof(MyArray));

  /* Doing this to avoid some compiler optimization. */
  for (int64_t i : IndexRange(sizeof(MyArray))) {
    EXPECT_EQ(((char *)buffer.ptr())[i], 100);
  }

  {
    MyArray &array = *new (buffer) MyArray(1, NoInitialization());
    EXPECT_EQ(array[0].value, 100);
    array.clear_without_destruct();
    array.~Array();
  }
  {
    MyArray &array = *new (buffer) MyArray(1);
    EXPECT_EQ(array[0].value, 42);
    array.~Array();
  }
}

TEST(array, Fill)
{
  Array<int> array(5);
  array.fill(3);
  EXPECT_EQ(array.size(), 5u);
  EXPECT_EQ(array[0], 3);
  EXPECT_EQ(array[1], 3);
  EXPECT_EQ(array[2], 3);
  EXPECT_EQ(array[3], 3);
  EXPECT_EQ(array[4], 3);
}

TEST(array, ReverseIterator)
{
  Array<int> array = {3, 4, 5, 6};
  Vector<int> reversed_vec;

  for (auto it = array.rbegin(); it != array.rend(); ++it) {
    reversed_vec.append(*it);
    *it += 10;
  }

  EXPECT_EQ_ARRAY(reversed_vec.data(), Span({6, 5, 4, 3}).data(), 4);
  EXPECT_EQ_ARRAY(array.data(), Span({13, 14, 15, 16}).data(), 4);
}

}  // namespace blender::tests
