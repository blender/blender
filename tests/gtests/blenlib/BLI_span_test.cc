#include "BLI_span.hh"
#include "BLI_strict_flags.h"
#include "BLI_vector.hh"
#include "testing/testing.h"

using namespace blender;

TEST(span, FromSmallVector)
{
  Vector<int> a = {1, 2, 3};
  Span<int> a_span = a;
  EXPECT_EQ(a_span.size(), 3);
  EXPECT_EQ(a_span[0], 1);
  EXPECT_EQ(a_span[1], 2);
  EXPECT_EQ(a_span[2], 3);
}

TEST(span, AddConstToPointer)
{
  int a = 0;
  std::vector<int *> vec = {&a};
  Span<int *> span = vec;
  Span<const int *> const_span = span;
  EXPECT_EQ(const_span.size(), 1);
}

TEST(span, IsReferencing)
{
  int array[] = {3, 5, 8};
  MutableSpan<int> span(array, ARRAY_SIZE(array));
  EXPECT_EQ(span.size(), 3);
  EXPECT_EQ(span[1], 5);
  array[1] = 10;
  EXPECT_EQ(span[1], 10);
}

TEST(span, DropBack)
{
  Vector<int> a = {4, 5, 6, 7};
  auto slice = Span<int>(a).drop_back(2);
  EXPECT_EQ(slice.size(), 2);
  EXPECT_EQ(slice[0], 4);
  EXPECT_EQ(slice[1], 5);
}

TEST(span, DropBackAll)
{
  Vector<int> a = {4, 5, 6, 7};
  auto slice = Span<int>(a).drop_back(a.size());
  EXPECT_EQ(slice.size(), 0);
}

TEST(span, DropFront)
{
  Vector<int> a = {4, 5, 6, 7};
  auto slice = Span<int>(a).drop_front(1);
  EXPECT_EQ(slice.size(), 3);
  EXPECT_EQ(slice[0], 5);
  EXPECT_EQ(slice[1], 6);
  EXPECT_EQ(slice[2], 7);
}

TEST(span, DropFrontAll)
{
  Vector<int> a = {4, 5, 6, 7};
  auto slice = Span<int>(a).drop_front(a.size());
  EXPECT_EQ(slice.size(), 0);
}

TEST(span, TakeFront)
{
  Vector<int> a = {4, 5, 6, 7};
  auto slice = Span<int>(a).take_front(2);
  EXPECT_EQ(slice.size(), 2);
  EXPECT_EQ(slice[0], 4);
  EXPECT_EQ(slice[1], 5);
}

TEST(span, TakeBack)
{
  Vector<int> a = {5, 6, 7, 8};
  auto slice = Span<int>(a).take_back(2);
  EXPECT_EQ(slice.size(), 2);
  EXPECT_EQ(slice[0], 7);
  EXPECT_EQ(slice[1], 8);
}

TEST(span, Slice)
{
  Vector<int> a = {4, 5, 6, 7};
  auto slice = Span<int>(a).slice(1, 2);
  EXPECT_EQ(slice.size(), 2);
  EXPECT_EQ(slice[0], 5);
  EXPECT_EQ(slice[1], 6);
}

TEST(span, SliceEmpty)
{
  Vector<int> a = {4, 5, 6, 7};
  auto slice = Span<int>(a).slice(2, 0);
  EXPECT_EQ(slice.size(), 0);
}

TEST(span, SliceRange)
{
  Vector<int> a = {1, 2, 3, 4, 5};
  auto slice = Span<int>(a).slice(IndexRange(2, 2));
  EXPECT_EQ(slice.size(), 2);
  EXPECT_EQ(slice[0], 3);
  EXPECT_EQ(slice[1], 4);
}

TEST(span, Contains)
{
  Vector<int> a = {4, 5, 6, 7};
  Span<int> a_span = a;
  EXPECT_TRUE(a_span.contains(4));
  EXPECT_TRUE(a_span.contains(5));
  EXPECT_TRUE(a_span.contains(6));
  EXPECT_TRUE(a_span.contains(7));
  EXPECT_FALSE(a_span.contains(3));
  EXPECT_FALSE(a_span.contains(8));
}

TEST(span, Count)
{
  Vector<int> a = {2, 3, 4, 3, 3, 2, 2, 2, 2};
  Span<int> a_span = a;
  EXPECT_EQ(a_span.count(1), 0);
  EXPECT_EQ(a_span.count(2), 5);
  EXPECT_EQ(a_span.count(3), 3);
  EXPECT_EQ(a_span.count(4), 1);
  EXPECT_EQ(a_span.count(5), 0);
}

static void test_ref_from_initializer_list(Span<int> span)
{
  EXPECT_EQ(span.size(), 4);
  EXPECT_EQ(span[0], 3);
  EXPECT_EQ(span[1], 6);
  EXPECT_EQ(span[2], 8);
  EXPECT_EQ(span[3], 9);
}

TEST(span, FromInitializerList)
{
  test_ref_from_initializer_list({3, 6, 8, 9});
}

TEST(span, FromVector)
{
  std::vector<int> a = {1, 2, 3, 4};
  Span<int> a_span(a);
  EXPECT_EQ(a_span.size(), 4);
  EXPECT_EQ(a_span[0], 1);
  EXPECT_EQ(a_span[1], 2);
  EXPECT_EQ(a_span[2], 3);
  EXPECT_EQ(a_span[3], 4);
}

TEST(span, FromArray)
{
  std::array<int, 2> a = {5, 6};
  Span<int> a_span(a);
  EXPECT_EQ(a_span.size(), 2);
  EXPECT_EQ(a_span[0], 5);
  EXPECT_EQ(a_span[1], 6);
}

TEST(span, Fill)
{
  std::array<int, 5> a = {4, 5, 6, 7, 8};
  MutableSpan<int> a_span(a);
  a_span.fill(1);
  EXPECT_EQ(a[0], 1);
  EXPECT_EQ(a[1], 1);
  EXPECT_EQ(a[2], 1);
  EXPECT_EQ(a[3], 1);
  EXPECT_EQ(a[4], 1);
}

TEST(span, FillIndices)
{
  std::array<int, 5> a = {0, 0, 0, 0, 0};
  MutableSpan<int> a_span(a);
  a_span.fill_indices({0, 2, 3}, 1);
  EXPECT_EQ(a[0], 1);
  EXPECT_EQ(a[1], 0);
  EXPECT_EQ(a[2], 1);
  EXPECT_EQ(a[3], 1);
  EXPECT_EQ(a[4], 0);
}

TEST(span, SizeInBytes)
{
  std::array<int, 10> a;
  Span<int> a_span(a);
  EXPECT_EQ(a_span.size_in_bytes(), sizeof(a));
  EXPECT_EQ(a_span.size_in_bytes(), 40);
}

TEST(span, FirstLast)
{
  std::array<int, 4> a = {6, 7, 8, 9};
  Span<int> a_span(a);
  EXPECT_EQ(a_span.first(), 6);
  EXPECT_EQ(a_span.last(), 9);
}

TEST(span, FirstLast_OneElement)
{
  int a = 3;
  Span<int> a_span(&a, 1);
  EXPECT_EQ(a_span.first(), 3);
  EXPECT_EQ(a_span.last(), 3);
}

TEST(span, Get)
{
  std::array<int, 3> a = {5, 6, 7};
  Span<int> a_span(a);
  EXPECT_EQ(a_span.get(0, 42), 5);
  EXPECT_EQ(a_span.get(1, 42), 6);
  EXPECT_EQ(a_span.get(2, 42), 7);
  EXPECT_EQ(a_span.get(3, 42), 42);
  EXPECT_EQ(a_span.get(4, 42), 42);
}

TEST(span, ContainsPtr)
{
  std::array<int, 3> a = {5, 6, 7};
  int other = 10;
  Span<int> a_span(a);
  EXPECT_TRUE(a_span.contains_ptr(&a[0] + 0));
  EXPECT_TRUE(a_span.contains_ptr(&a[0] + 1));
  EXPECT_TRUE(a_span.contains_ptr(&a[0] + 2));
  EXPECT_FALSE(a_span.contains_ptr(&a[0] + 3));
  EXPECT_FALSE(a_span.contains_ptr(&a[0] - 1));
  EXPECT_FALSE(a_span.contains_ptr(&other));
}

TEST(span, FirstIndex)
{
  std::array<int, 5> a = {4, 5, 4, 2, 5};
  Span<int> a_span(a);

  EXPECT_EQ(a_span.first_index(4), 0);
  EXPECT_EQ(a_span.first_index(5), 1);
  EXPECT_EQ(a_span.first_index(2), 3);
}

TEST(span, CastSameSize)
{
  int value = 0;
  std::array<int *, 4> a = {&value, nullptr, nullptr, nullptr};
  Span<int *> a_span = a;
  Span<float *> new_a_span = a_span.cast<float *>();

  EXPECT_EQ(a_span.size(), 4);
  EXPECT_EQ(new_a_span.size(), 4);

  EXPECT_EQ(a_span[0], &value);
  EXPECT_EQ(new_a_span[0], (float *)&value);
}

TEST(span, CastSmallerSize)
{
  std::array<uint32_t, 4> a = {3, 4, 5, 6};
  Span<uint32_t> a_span = a;
  Span<uint16_t> new_a_span = a_span.cast<uint16_t>();

  EXPECT_EQ(a_span.size(), 4);
  EXPECT_EQ(new_a_span.size(), 8);
}

TEST(span, CastLargerSize)
{
  std::array<uint16_t, 4> a = {4, 5, 6, 7};
  Span<uint16_t> a_span = a;
  Span<uint32_t> new_a_span = a_span.cast<uint32_t>();

  EXPECT_EQ(a_span.size(), 4);
  EXPECT_EQ(new_a_span.size(), 2);
}
