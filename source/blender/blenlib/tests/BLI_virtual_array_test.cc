/* Apache License, Version 2.0 */

#include "BLI_array.hh"
#include "BLI_strict_flags.h"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"
#include "BLI_virtual_array.hh"
#include "testing/testing.h"

namespace blender::tests {

TEST(virtual_array, Span)
{
  std::array<int, 5> data = {3, 4, 5, 6, 7};
  VArray_For_Span<int> varray{data};
  EXPECT_EQ(varray.size(), 5);
  EXPECT_EQ(varray.get(0), 3);
  EXPECT_EQ(varray.get(4), 7);
  EXPECT_TRUE(varray.is_span());
  EXPECT_FALSE(varray.is_single());
  EXPECT_EQ(varray.get_internal_span().data(), data.data());
}

TEST(virtual_array, Single)
{
  VArray_For_Single<int> varray{10, 4};
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray.get(0), 10);
  EXPECT_EQ(varray.get(3), 10);
  EXPECT_FALSE(varray.is_span());
  EXPECT_TRUE(varray.is_single());
}

TEST(virtual_array, Array)
{
  Array<int> array = {1, 2, 3, 5, 8};
  {
    VArray_For_ArrayContainer varray{array};
    EXPECT_EQ(varray.size(), 5);
    EXPECT_EQ(varray[0], 1);
    EXPECT_EQ(varray[2], 3);
    EXPECT_EQ(varray[3], 5);
    EXPECT_TRUE(varray.is_span());
  }
  {
    VArray_For_ArrayContainer varray{std::move(array)};
    EXPECT_EQ(varray.size(), 5);
    EXPECT_EQ(varray[0], 1);
    EXPECT_EQ(varray[2], 3);
    EXPECT_EQ(varray[3], 5);
    EXPECT_TRUE(varray.is_span());
  }
  {
    VArray_For_ArrayContainer varray{array}; /* NOLINT: bugprone-use-after-move */
    EXPECT_TRUE(varray.is_empty());
  }
}

TEST(virtual_array, Vector)
{
  Vector<int> vector = {9, 8, 7, 6};
  VArray_For_ArrayContainer varray{std::move(vector)};
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray[0], 9);
  EXPECT_EQ(varray[3], 6);
}

TEST(virtual_array, StdVector)
{
  std::vector<int> vector = {5, 6, 7, 8};
  VArray_For_ArrayContainer varray{std::move(vector)};
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray[0], 5);
  EXPECT_EQ(varray[1], 6);
}

TEST(virtual_array, StdArray)
{
  std::array<int, 4> array = {2, 3, 4, 5};
  VArray_For_ArrayContainer varray{array};
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray[0], 2);
  EXPECT_EQ(varray[1], 3);
}

TEST(virtual_array, VectorSet)
{
  VectorSet<int> vector_set = {5, 3, 7, 3, 3, 5, 1};
  VArray_For_ArrayContainer varray{std::move(vector_set)};
  EXPECT_TRUE(vector_set.is_empty()); /* NOLINT: bugprone-use-after-move. */
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray[0], 5);
  EXPECT_EQ(varray[1], 3);
  EXPECT_EQ(varray[2], 7);
  EXPECT_EQ(varray[3], 1);
}

TEST(virtual_array, Func)
{
  auto func = [](int64_t index) { return (int)(index * index); };
  VArray_For_Func<int, decltype(func)> varray{10, func};
  EXPECT_EQ(varray.size(), 10);
  EXPECT_EQ(varray[0], 0);
  EXPECT_EQ(varray[3], 9);
  EXPECT_EQ(varray[9], 81);
}

TEST(virtual_array, AsSpan)
{
  auto func = [](int64_t index) { return (int)(10 * index); };
  VArray_For_Func<int, decltype(func)> func_varray{10, func};
  VArray_Span span_varray{func_varray};
  EXPECT_EQ(span_varray.size(), 10);
  Span<int> span = span_varray;
  EXPECT_EQ(span.size(), 10);
  EXPECT_EQ(span[0], 0);
  EXPECT_EQ(span[3], 30);
  EXPECT_EQ(span[6], 60);
}

static int get_x(const std::array<int, 3> &item)
{
  return item[0];
}

static void set_x(std::array<int, 3> &item, int value)
{
  item[0] = value;
}

TEST(virtual_array, DerivedSpan)
{
  Vector<std::array<int, 3>> vector;
  vector.append({3, 4, 5});
  vector.append({1, 1, 1});
  {
    VArray_For_DerivedSpan<std::array<int, 3>, int, get_x> varray{vector};
    EXPECT_EQ(varray.size(), 2);
    EXPECT_EQ(varray[0], 3);
    EXPECT_EQ(varray[1], 1);
  }
  {
    VMutableArray_For_DerivedSpan<std::array<int, 3>, int, get_x, set_x> varray{vector};
    EXPECT_EQ(varray.size(), 2);
    EXPECT_EQ(varray[0], 3);
    EXPECT_EQ(varray[1], 1);
    varray.set(0, 10);
    varray.set(1, 20);
    EXPECT_EQ(vector[0][0], 10);
    EXPECT_EQ(vector[1][0], 20);
  }
}

}  // namespace blender::tests
