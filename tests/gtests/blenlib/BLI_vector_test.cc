#include "BLI_strict_flags.h"
#include "BLI_vector.hh"
#include "testing/testing.h"
#include <forward_list>

using namespace blender;

TEST(vector, DefaultConstructor)
{
  Vector<int> vec;
  EXPECT_EQ(vec.size(), 0u);
}

TEST(vector, SizeConstructor)
{
  Vector<int> vec(3);
  EXPECT_EQ(vec.size(), 3u);
}

/**
 * Tests that the trivially constructible types are not zero-initialized. We do not want that for
 * performance reasons.
 */
TEST(vector, TrivialTypeSizeConstructor)
{
  Vector<char, 1> *vec = new Vector<char, 1>(1);
  char *ptr = &(*vec)[0];
  vec->~Vector();

  const char magic = 42;
  *ptr = magic;
  EXPECT_EQ(*ptr, magic);

  new (vec) Vector<char, 1>(1);
  EXPECT_EQ((*vec)[0], magic);
  EXPECT_EQ(*ptr, magic);
  delete vec;
}

TEST(vector, SizeValueConstructor)
{
  Vector<int> vec(4, 10);
  EXPECT_EQ(vec.size(), 4u);
  EXPECT_EQ(vec[0], 10);
  EXPECT_EQ(vec[1], 10);
  EXPECT_EQ(vec[2], 10);
  EXPECT_EQ(vec[3], 10);
}

TEST(vector, InitializerListConstructor)
{
  Vector<int> vec = {1, 3, 4, 6};
  EXPECT_EQ(vec.size(), 4u);
  EXPECT_EQ(vec[0], 1);
  EXPECT_EQ(vec[1], 3);
  EXPECT_EQ(vec[2], 4);
  EXPECT_EQ(vec[3], 6);
}

struct TestListValue {
  TestListValue *next, *prev;
  int value;
};

TEST(vector, ListBaseConstructor)
{
  TestListValue *value1 = new TestListValue{0, 0, 4};
  TestListValue *value2 = new TestListValue{0, 0, 5};
  TestListValue *value3 = new TestListValue{0, 0, 6};

  ListBase list = {NULL, NULL};
  BLI_addtail(&list, value1);
  BLI_addtail(&list, value2);
  BLI_addtail(&list, value3);
  Vector<TestListValue *> vec(list);

  EXPECT_EQ(vec.size(), 3u);
  EXPECT_EQ(vec[0]->value, 4);
  EXPECT_EQ(vec[1]->value, 5);
  EXPECT_EQ(vec[2]->value, 6);

  delete value1;
  delete value2;
  delete value3;
}

TEST(vector, ContainerConstructor)
{
  std::forward_list<int> list;
  list.push_front(3);
  list.push_front(1);
  list.push_front(5);

  Vector<int> vec = Vector<int>::FromContainer(list);
  EXPECT_EQ(vec.size(), 3u);
  EXPECT_EQ(vec[0], 5);
  EXPECT_EQ(vec[1], 1);
  EXPECT_EQ(vec[2], 3);
}

TEST(vector, CopyConstructor)
{
  Vector<int> vec1 = {1, 2, 3};
  Vector<int> vec2(vec1);
  EXPECT_EQ(vec2.size(), 3u);
  EXPECT_EQ(vec2[0], 1);
  EXPECT_EQ(vec2[1], 2);
  EXPECT_EQ(vec2[2], 3);

  vec1[1] = 5;
  EXPECT_EQ(vec1[1], 5);
  EXPECT_EQ(vec2[1], 2);
}

TEST(vector, CopyConstructor2)
{
  Vector<int, 2> vec1 = {1, 2, 3, 4};
  Vector<int, 3> vec2(vec1);

  EXPECT_EQ(vec1.size(), 4u);
  EXPECT_EQ(vec2.size(), 4u);
  EXPECT_NE(vec1.data(), vec2.data());
  EXPECT_EQ(vec2[0], 1);
  EXPECT_EQ(vec2[1], 2);
  EXPECT_EQ(vec2[2], 3);
  EXPECT_EQ(vec2[3], 4);
}

TEST(vector, CopyConstructor3)
{
  Vector<int, 20> vec1 = {1, 2, 3, 4};
  Vector<int, 1> vec2(vec1);

  EXPECT_EQ(vec1.size(), 4u);
  EXPECT_EQ(vec2.size(), 4u);
  EXPECT_NE(vec1.data(), vec2.data());
  EXPECT_EQ(vec2[2], 3);
}

TEST(vector, CopyConstructor4)
{
  Vector<int, 5> vec1 = {1, 2, 3, 4};
  Vector<int, 6> vec2(vec1);

  EXPECT_EQ(vec1.size(), 4u);
  EXPECT_EQ(vec2.size(), 4u);
  EXPECT_NE(vec1.data(), vec2.data());
  EXPECT_EQ(vec2[3], 4);
}

TEST(vector, MoveConstructor)
{
  Vector<int> vec1 = {1, 2, 3, 4};
  Vector<int> vec2(std::move(vec1));

  EXPECT_EQ(vec1.size(), 0u);
  EXPECT_EQ(vec2.size(), 4u);
  EXPECT_EQ(vec2[0], 1);
  EXPECT_EQ(vec2[1], 2);
  EXPECT_EQ(vec2[2], 3);
  EXPECT_EQ(vec2[3], 4);
}

TEST(vector, MoveConstructor2)
{
  Vector<int, 2> vec1 = {1, 2, 3, 4};
  Vector<int, 3> vec2(std::move(vec1));

  EXPECT_EQ(vec1.size(), 0u);
  EXPECT_EQ(vec2.size(), 4u);
  EXPECT_EQ(vec2[0], 1);
  EXPECT_EQ(vec2[1], 2);
  EXPECT_EQ(vec2[2], 3);
  EXPECT_EQ(vec2[3], 4);
}

TEST(vector, MoveConstructor3)
{
  Vector<int, 20> vec1 = {1, 2, 3, 4};
  Vector<int, 1> vec2(std::move(vec1));

  EXPECT_EQ(vec1.size(), 0u);
  EXPECT_EQ(vec2.size(), 4u);
  EXPECT_EQ(vec2[2], 3);
}

TEST(vector, MoveConstructor4)
{
  Vector<int, 5> vec1 = {1, 2, 3, 4};
  Vector<int, 6> vec2(std::move(vec1));

  EXPECT_EQ(vec1.size(), 0u);
  EXPECT_EQ(vec2.size(), 4u);
  EXPECT_EQ(vec2[3], 4);
}

TEST(vector, MoveAssignment)
{
  Vector<int> vec = {1, 2};
  EXPECT_EQ(vec.size(), 2u);
  EXPECT_EQ(vec[0], 1);
  EXPECT_EQ(vec[1], 2);

  vec = Vector<int>({5});
  EXPECT_EQ(vec.size(), 1u);
  EXPECT_EQ(vec[0], 5);
}

TEST(vector, CopyAssignment)
{
  Vector<int> vec1 = {1, 2, 3};
  Vector<int> vec2 = {4, 5};
  EXPECT_EQ(vec1.size(), 3u);
  EXPECT_EQ(vec2.size(), 2u);

  vec2 = vec1;
  EXPECT_EQ(vec2.size(), 3u);

  vec1[0] = 7;
  EXPECT_EQ(vec1[0], 7);
  EXPECT_EQ(vec2[0], 1);
}

TEST(vector, Append)
{
  Vector<int> vec;
  vec.append(3);
  vec.append(6);
  vec.append(7);
  EXPECT_EQ(vec.size(), 3u);
  EXPECT_EQ(vec[0], 3);
  EXPECT_EQ(vec[1], 6);
  EXPECT_EQ(vec[2], 7);
}

TEST(vector, AppendAndGetIndex)
{
  Vector<int> vec;
  EXPECT_EQ(vec.append_and_get_index(10), 0u);
  EXPECT_EQ(vec.append_and_get_index(10), 1u);
  EXPECT_EQ(vec.append_and_get_index(10), 2u);
  vec.append(10);
  EXPECT_EQ(vec.append_and_get_index(10), 4u);
}

TEST(vector, AppendNonDuplicates)
{
  Vector<int> vec;
  vec.append_non_duplicates(4);
  EXPECT_EQ(vec.size(), 1u);
  vec.append_non_duplicates(5);
  EXPECT_EQ(vec.size(), 2u);
  vec.append_non_duplicates(4);
  EXPECT_EQ(vec.size(), 2u);
}

TEST(vector, ExtendNonDuplicates)
{
  Vector<int> vec;
  vec.extend_non_duplicates({1, 2});
  EXPECT_EQ(vec.size(), 2u);
  vec.extend_non_duplicates({3, 4});
  EXPECT_EQ(vec.size(), 4u);
  vec.extend_non_duplicates({0, 1, 2, 3});
  EXPECT_EQ(vec.size(), 5u);
}

TEST(vector, Fill)
{
  Vector<int> vec(5);
  vec.fill(3);
  EXPECT_EQ(vec.size(), 5u);
  EXPECT_EQ(vec[0], 3);
  EXPECT_EQ(vec[1], 3);
  EXPECT_EQ(vec[2], 3);
  EXPECT_EQ(vec[3], 3);
  EXPECT_EQ(vec[4], 3);
}

TEST(vector, FillIndices)
{
  Vector<int> vec(5, 0);
  vec.fill_indices({1, 2}, 4);
  EXPECT_EQ(vec[0], 0);
  EXPECT_EQ(vec[1], 4);
  EXPECT_EQ(vec[2], 4);
  EXPECT_EQ(vec[3], 0);
  EXPECT_EQ(vec[4], 0);
}

TEST(vector, Iterator)
{
  Vector<int> vec({1, 4, 9, 16});
  int i = 1;
  for (int value : vec) {
    EXPECT_EQ(value, i * i);
    i++;
  }
}

TEST(vector, BecomeLarge)
{
  Vector<int, 4> vec;
  for (int i = 0; i < 100; i++) {
    vec.append(i * 5);
  }
  EXPECT_EQ(vec.size(), 100u);
  for (uint i = 0; i < 100; i++) {
    EXPECT_EQ(vec[i], static_cast<int>(i * 5));
  }
}

static Vector<int> return_by_value_helper()
{
  return Vector<int>({3, 5, 1});
}

TEST(vector, ReturnByValue)
{
  Vector<int> vec = return_by_value_helper();
  EXPECT_EQ(vec.size(), 3u);
  EXPECT_EQ(vec[0], 3);
  EXPECT_EQ(vec[1], 5);
  EXPECT_EQ(vec[2], 1);
}

TEST(vector, VectorOfVectors_Append)
{
  Vector<Vector<int>> vec;
  EXPECT_EQ(vec.size(), 0u);

  Vector<int> v({1, 2});
  vec.append(v);
  vec.append({7, 8});
  EXPECT_EQ(vec.size(), 2u);
  EXPECT_EQ(vec[0][0], 1);
  EXPECT_EQ(vec[0][1], 2);
  EXPECT_EQ(vec[1][0], 7);
  EXPECT_EQ(vec[1][1], 8);
}

TEST(vector, VectorOfVectors_Fill)
{
  Vector<Vector<int>> vec(3);
  vec.fill({4, 5});

  EXPECT_EQ(vec[0][0], 4);
  EXPECT_EQ(vec[0][1], 5);
  EXPECT_EQ(vec[1][0], 4);
  EXPECT_EQ(vec[1][1], 5);
  EXPECT_EQ(vec[2][0], 4);
  EXPECT_EQ(vec[2][1], 5);
}

TEST(vector, RemoveLast)
{
  Vector<int> vec = {5, 6};
  EXPECT_EQ(vec.size(), 2u);
  vec.remove_last();
  EXPECT_EQ(vec.size(), 1u);
  vec.remove_last();
  EXPECT_EQ(vec.size(), 0u);
}

TEST(vector, IsEmpty)
{
  Vector<int> vec;
  EXPECT_TRUE(vec.is_empty());
  vec.append(1);
  EXPECT_FALSE(vec.is_empty());
  vec.remove_last();
  EXPECT_TRUE(vec.is_empty());
}

TEST(vector, RemoveReorder)
{
  Vector<int> vec = {4, 5, 6, 7};
  vec.remove_and_reorder(1);
  EXPECT_EQ(vec[0], 4);
  EXPECT_EQ(vec[1], 7);
  EXPECT_EQ(vec[2], 6);
  vec.remove_and_reorder(2);
  EXPECT_EQ(vec[0], 4);
  EXPECT_EQ(vec[1], 7);
  vec.remove_and_reorder(0);
  EXPECT_EQ(vec[0], 7);
  vec.remove_and_reorder(0);
  EXPECT_TRUE(vec.is_empty());
}

TEST(vector, RemoveFirstOccurrenceAndReorder)
{
  Vector<int> vec = {4, 5, 6, 7};
  vec.remove_first_occurrence_and_reorder(5);
  EXPECT_EQ(vec[0], 4);
  EXPECT_EQ(vec[1], 7);
  EXPECT_EQ(vec[2], 6);
  vec.remove_first_occurrence_and_reorder(6);
  EXPECT_EQ(vec[0], 4);
  EXPECT_EQ(vec[1], 7);
  vec.remove_first_occurrence_and_reorder(4);
  EXPECT_EQ(vec[0], 7);
  vec.remove_first_occurrence_and_reorder(7);
  EXPECT_EQ(vec.size(), 0u);
}

TEST(vector, Remove)
{
  Vector<int> vec = {1, 2, 3, 4, 5, 6};
  vec.remove(3);
  EXPECT_TRUE(std::equal(vec.begin(), vec.end(), Span<int>({1, 2, 3, 5, 6}).begin()));
  vec.remove(0);
  EXPECT_TRUE(std::equal(vec.begin(), vec.end(), Span<int>({2, 3, 5, 6}).begin()));
  vec.remove(3);
  EXPECT_TRUE(std::equal(vec.begin(), vec.end(), Span<int>({2, 3, 5}).begin()));
  vec.remove(1);
  EXPECT_TRUE(std::equal(vec.begin(), vec.end(), Span<int>({2, 5}).begin()));
  vec.remove(1);
  EXPECT_TRUE(std::equal(vec.begin(), vec.end(), Span<int>({2}).begin()));
  vec.remove(0);
  EXPECT_TRUE(std::equal(vec.begin(), vec.end(), Span<int>({}).begin()));
}

TEST(vector, ExtendSmallVector)
{
  Vector<int> a = {2, 3, 4};
  Vector<int> b = {11, 12};
  b.extend(a);
  EXPECT_EQ(b.size(), 5u);
  EXPECT_EQ(b[0], 11);
  EXPECT_EQ(b[1], 12);
  EXPECT_EQ(b[2], 2);
  EXPECT_EQ(b[3], 3);
  EXPECT_EQ(b[4], 4);
}

TEST(vector, ExtendArray)
{
  int array[] = {3, 4, 5, 6};

  Vector<int> a;
  a.extend(array, 2);

  EXPECT_EQ(a.size(), 2u);
  EXPECT_EQ(a[0], 3);
  EXPECT_EQ(a[1], 4);
}

TEST(vector, Last)
{
  Vector<int> a{3, 5, 7};
  EXPECT_EQ(a.last(), 7);
}

TEST(vector, AppendNTimes)
{
  Vector<int> a;
  a.append_n_times(5, 3);
  a.append_n_times(2, 2);
  EXPECT_EQ(a.size(), 5u);
  EXPECT_EQ(a[0], 5);
  EXPECT_EQ(a[1], 5);
  EXPECT_EQ(a[2], 5);
  EXPECT_EQ(a[3], 2);
  EXPECT_EQ(a[4], 2);
}

TEST(vector, UniquePtrValue)
{
  Vector<std::unique_ptr<int>> vec;
  vec.append(std::unique_ptr<int>(new int()));
  vec.append(std::unique_ptr<int>(new int()));
  vec.append(std::unique_ptr<int>(new int()));
  vec.append(std::unique_ptr<int>(new int()));
  EXPECT_EQ(vec.size(), 4u);

  std::unique_ptr<int> &a = vec.last();
  std::unique_ptr<int> b = vec.pop_last();
  vec.remove_and_reorder(0);
  vec.remove(0);
  EXPECT_EQ(vec.size(), 1u);

  UNUSED_VARS(a, b);
}

class TypeConstructMock {
 public:
  bool default_constructed = false;
  bool copy_constructed = false;
  bool move_constructed = false;
  bool copy_assigned = false;
  bool move_assigned = false;

  TypeConstructMock() : default_constructed(true)
  {
  }

  TypeConstructMock(const TypeConstructMock &other) : copy_constructed(true)
  {
  }

  TypeConstructMock(TypeConstructMock &&other) : move_constructed(true)
  {
  }

  TypeConstructMock &operator=(const TypeConstructMock &other)
  {
    if (this == &other) {
      return *this;
    }

    copy_assigned = true;
    return *this;
  }

  TypeConstructMock &operator=(TypeConstructMock &&other)
  {
    if (this == &other) {
      return *this;
    }

    move_assigned = true;
    return *this;
  }
};

TEST(vector, SizeConstructorCallsDefaultConstructor)
{
  Vector<TypeConstructMock> vec(3);
  EXPECT_TRUE(vec[0].default_constructed);
  EXPECT_TRUE(vec[1].default_constructed);
  EXPECT_TRUE(vec[2].default_constructed);
}

TEST(vector, SizeValueConstructorCallsCopyConstructor)
{
  Vector<TypeConstructMock> vec(3, TypeConstructMock());
  EXPECT_TRUE(vec[0].copy_constructed);
  EXPECT_TRUE(vec[1].copy_constructed);
  EXPECT_TRUE(vec[2].copy_constructed);
}

TEST(vector, AppendCallsCopyConstructor)
{
  Vector<TypeConstructMock> vec;
  TypeConstructMock value;
  vec.append(value);
  EXPECT_TRUE(vec[0].copy_constructed);
}

TEST(vector, AppendCallsMoveConstructor)
{
  Vector<TypeConstructMock> vec;
  vec.append(TypeConstructMock());
  EXPECT_TRUE(vec[0].move_constructed);
}

TEST(vector, SmallVectorCopyCallsCopyConstructor)
{
  Vector<TypeConstructMock, 2> src(2);
  Vector<TypeConstructMock, 2> dst(src);
  EXPECT_TRUE(dst[0].copy_constructed);
  EXPECT_TRUE(dst[1].copy_constructed);
}

TEST(vector, LargeVectorCopyCallsCopyConstructor)
{
  Vector<TypeConstructMock, 2> src(5);
  Vector<TypeConstructMock, 2> dst(src);
  EXPECT_TRUE(dst[0].copy_constructed);
  EXPECT_TRUE(dst[1].copy_constructed);
}

TEST(vector, SmallVectorMoveCallsMoveConstructor)
{
  Vector<TypeConstructMock, 2> src(2);
  Vector<TypeConstructMock, 2> dst(std::move(src));
  EXPECT_TRUE(dst[0].move_constructed);
  EXPECT_TRUE(dst[1].move_constructed);
}

TEST(vector, LargeVectorMoveCallsNoConstructor)
{
  Vector<TypeConstructMock, 2> src(5);
  Vector<TypeConstructMock, 2> dst(std::move(src));

  EXPECT_TRUE(dst[0].default_constructed);
  EXPECT_FALSE(dst[0].move_constructed);
  EXPECT_FALSE(dst[0].copy_constructed);
}

TEST(vector, Resize)
{
  std::string long_string = "012345678901234567890123456789";
  Vector<std::string> vec;
  EXPECT_EQ(vec.size(), 0u);
  vec.resize(2);
  EXPECT_EQ(vec.size(), 2u);
  EXPECT_EQ(vec[0], "");
  EXPECT_EQ(vec[1], "");
  vec.resize(5, long_string);
  EXPECT_EQ(vec.size(), 5u);
  EXPECT_EQ(vec[0], "");
  EXPECT_EQ(vec[1], "");
  EXPECT_EQ(vec[2], long_string);
  EXPECT_EQ(vec[3], long_string);
  EXPECT_EQ(vec[4], long_string);
  vec.resize(1);
  EXPECT_EQ(vec.size(), 1u);
  EXPECT_EQ(vec[0], "");
}

TEST(vector, FirstIndexOf)
{
  Vector<int> vec = {2, 3, 5, 7, 5, 9};
  EXPECT_EQ(vec.first_index_of(2), 0u);
  EXPECT_EQ(vec.first_index_of(5), 2u);
  EXPECT_EQ(vec.first_index_of(9), 5u);
}

TEST(vector, FirstIndexTryOf)
{
  Vector<int> vec = {2, 3, 5, 7, 5, 9};
  EXPECT_EQ(vec.first_index_of_try(2), 0);
  EXPECT_EQ(vec.first_index_of_try(4), -1);
  EXPECT_EQ(vec.first_index_of_try(5), 2);
  EXPECT_EQ(vec.first_index_of_try(9), 5);
  EXPECT_EQ(vec.first_index_of_try(1), -1);
}

TEST(vector, OveralignedValues)
{
  Vector<AlignedBuffer<1, 512>, 2> vec;
  for (int i = 0; i < 100; i++) {
    vec.append({});
    EXPECT_EQ((uintptr_t)&vec.last() % 512, 0);
  }
}
