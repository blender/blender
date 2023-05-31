/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_array.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_strict_flags.h"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"
#include "BLI_virtual_array.hh"
#include "testing/testing.h"

namespace blender::tests {

TEST(virtual_array, Span)
{
  std::array<int, 5> data = {3, 4, 5, 6, 7};
  VArray<int> varray = VArray<int>::ForSpan(data);
  EXPECT_EQ(varray.size(), 5);
  EXPECT_EQ(varray.get(0), 3);
  EXPECT_EQ(varray.get(4), 7);
  EXPECT_TRUE(varray.is_span());
  EXPECT_FALSE(varray.is_single());
  EXPECT_EQ(varray.get_internal_span().data(), data.data());
}

TEST(virtual_array, Single)
{
  VArray<int> varray = VArray<int>::ForSingle(10, 4);
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
    VArray<int> varray = VArray<int>::ForContainer(array);
    EXPECT_EQ(varray.size(), 5);
    EXPECT_EQ(varray[0], 1);
    EXPECT_EQ(varray[2], 3);
    EXPECT_EQ(varray[3], 5);
    EXPECT_TRUE(varray.is_span());
  }
  {
    VArray<int> varray = VArray<int>::ForContainer(std::move(array));
    EXPECT_EQ(varray.size(), 5);
    EXPECT_EQ(varray[0], 1);
    EXPECT_EQ(varray[2], 3);
    EXPECT_EQ(varray[3], 5);
    EXPECT_TRUE(varray.is_span());
  }
  {
    VArray<int> varray = VArray<int>::ForContainer(array); /* NOLINT: bugprone-use-after-move */
    EXPECT_TRUE(varray.is_empty());
  }
}

TEST(virtual_array, Vector)
{
  Vector<int> vector = {9, 8, 7, 6};
  VArray<int> varray = VArray<int>::ForContainer(std::move(vector));
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray[0], 9);
  EXPECT_EQ(varray[3], 6);
}

TEST(virtual_array, StdVector)
{
  std::vector<int> vector = {5, 6, 7, 8};
  VArray<int> varray = VArray<int>::ForContainer(std::move(vector));
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray[0], 5);
  EXPECT_EQ(varray[1], 6);
}

TEST(virtual_array, StdArray)
{
  std::array<int, 4> array = {2, 3, 4, 5};
  VArray<int> varray = VArray<int>::ForContainer(std::move(array));
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray[0], 2);
  EXPECT_EQ(varray[1], 3);
}

TEST(virtual_array, VectorSet)
{
  VectorSet<int> vector_set = {5, 3, 7, 3, 3, 5, 1};
  VArray<int> varray = VArray<int>::ForContainer(std::move(vector_set));
  EXPECT_TRUE(vector_set.is_empty()); /* NOLINT: bugprone-use-after-move. */
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray[0], 5);
  EXPECT_EQ(varray[1], 3);
  EXPECT_EQ(varray[2], 7);
  EXPECT_EQ(varray[3], 1);
}

TEST(virtual_array, Func)
{
  auto func = [](int64_t index) { return int(index * index); };
  VArray<int> varray = VArray<int>::ForFunc(10, func);
  EXPECT_EQ(varray.size(), 10);
  EXPECT_EQ(varray[0], 0);
  EXPECT_EQ(varray[3], 9);
  EXPECT_EQ(varray[9], 81);
}

TEST(virtual_array, AsSpan)
{
  auto func = [](int64_t index) { return int(10 * index); };
  VArray<int> func_varray = VArray<int>::ForFunc(10, func);
  VArraySpan span_varray{func_varray};
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
    VArray<int> varray = VArray<int>::ForDerivedSpan<std::array<int, 3>, get_x>(vector);
    EXPECT_EQ(varray.size(), 2);
    EXPECT_EQ(varray[0], 3);
    EXPECT_EQ(varray[1], 1);
  }
  {
    VMutableArray<int> varray =
        VMutableArray<int>::ForDerivedSpan<std::array<int, 3>, get_x, set_x>(vector);
    EXPECT_EQ(varray.size(), 2);
    EXPECT_EQ(varray[0], 3);
    EXPECT_EQ(varray[1], 1);
    varray.set(0, 10);
    varray.set(1, 20);
    EXPECT_EQ(vector[0][0], 10);
    EXPECT_EQ(vector[1][0], 20);
  }
}

TEST(virtual_array, MutableToImmutable)
{
  std::array<int, 4> array = {4, 2, 6, 4};
  {
    VMutableArray<int> mutable_varray = VMutableArray<int>::ForSpan(array);
    VArray<int> varray = mutable_varray;
    EXPECT_TRUE(varray.is_span());
    EXPECT_EQ(varray.size(), 4);
    EXPECT_EQ(varray[1], 2);
    EXPECT_EQ(mutable_varray.size(), 4);
  }
  {
    VMutableArray<int> mutable_varray = VMutableArray<int>::ForSpan(array);
    EXPECT_EQ(mutable_varray.size(), 4);
    VArray<int> varray = std::move(mutable_varray);
    EXPECT_TRUE(varray.is_span());
    EXPECT_EQ(varray.size(), 4);
    EXPECT_EQ(varray[1], 2);
    EXPECT_EQ(mutable_varray.size(), 0); /* NOLINT: bugprone-use-after-move */
  }
  {
    VArray<int> varray = VMutableArray<int>::ForSpan(array);
    EXPECT_TRUE(varray.is_span());
    EXPECT_EQ(varray.size(), 4);
    EXPECT_EQ(varray[1], 2);
  }
}

TEST(virtual_array, MaterializeCompressed)
{
  IndexMaskMemory memory;
  {
    std::array<int, 10> array = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90};
    VArray<int> varray = VArray<int>::ForSpan(array);
    std::array<int, 3> compressed_array;
    varray.materialize_compressed(IndexMask::from_indices<int>({3, 6, 7}, memory),
                                  compressed_array);
    EXPECT_EQ(compressed_array[0], 30);
    EXPECT_EQ(compressed_array[1], 60);
    EXPECT_EQ(compressed_array[2], 70);
    varray.materialize_compressed_to_uninitialized(IndexMask::from_indices<int>({2, 8, 9}, memory),
                                                   compressed_array);
    EXPECT_EQ(compressed_array[0], 20);
    EXPECT_EQ(compressed_array[1], 80);
    EXPECT_EQ(compressed_array[2], 90);
  }
  {
    VArray<int> varray = VArray<int>::ForSingle(4, 10);
    std::array<int, 3> compressed_array;
    varray.materialize_compressed(IndexMask::from_indices<int>({2, 6, 7}, memory),
                                  compressed_array);
    EXPECT_EQ(compressed_array[0], 4);
    EXPECT_EQ(compressed_array[1], 4);
    EXPECT_EQ(compressed_array[2], 4);
    compressed_array.fill(0);
    varray.materialize_compressed_to_uninitialized(IndexMask::from_indices<int>({0, 1, 2}, memory),
                                                   compressed_array);
    EXPECT_EQ(compressed_array[0], 4);
    EXPECT_EQ(compressed_array[1], 4);
    EXPECT_EQ(compressed_array[2], 4);
  }
  {
    VArray<int> varray = VArray<int>::ForFunc(10, [](const int64_t i) { return int(i * i); });
    std::array<int, 3> compressed_array;
    varray.materialize_compressed(IndexMask::from_indices<int>({5, 7, 8}, memory),
                                  compressed_array);
    EXPECT_EQ(compressed_array[0], 25);
    EXPECT_EQ(compressed_array[1], 49);
    EXPECT_EQ(compressed_array[2], 64);
    varray.materialize_compressed_to_uninitialized(IndexMask::from_indices<int>({1, 2, 3}, memory),
                                                   compressed_array);
    EXPECT_EQ(compressed_array[0], 1);
    EXPECT_EQ(compressed_array[1], 4);
    EXPECT_EQ(compressed_array[2], 9);
  }
}

TEST(virtual_array, EmptySpanWrapper)
{
  {
    VArray<int> varray;
    VArraySpan<int> span1 = varray;
    EXPECT_TRUE(span1.is_empty());
    VArraySpan<int> span2 = std::move(span1);
    EXPECT_TRUE(span2.is_empty());
  }
  {
    VMutableArray<int> varray;
    MutableVArraySpan<int> span1 = varray;
    EXPECT_TRUE(span1.is_empty());
    MutableVArraySpan<int> span2 = std::move(span1);
    EXPECT_TRUE(span2.is_empty());
  }
  {
    GVArray varray;
    GVArraySpan span1 = varray;
    EXPECT_TRUE(span1.is_empty());
    GVArraySpan span2 = std::move(span1);
    EXPECT_TRUE(span2.is_empty());
  }
  {
    GVMutableArray varray;
    GMutableVArraySpan span1 = varray;
    EXPECT_TRUE(span1.is_empty());
    GMutableVArraySpan span2 = std::move(span1);
    EXPECT_TRUE(span2.is_empty());
  }
}

}  // namespace blender::tests
