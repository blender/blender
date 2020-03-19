#include "BLI_vector.h"
#include "testing/testing.h"
#include <forward_list>

using BLI::Vector;
using IntVector = Vector<int>;

TEST(vector, DefaultConstructor)
{
  IntVector vec;
  EXPECT_EQ(vec.size(), 0);
}

TEST(vector, SizeConstructor)
{
  IntVector vec(3);
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec[0], 0);
  EXPECT_EQ(vec[1], 0);
  EXPECT_EQ(vec[2], 0);
}

TEST(vector, SizeValueConstructor)
{
  IntVector vec(4, 10);
  EXPECT_EQ(vec.size(), 4);
  EXPECT_EQ(vec[0], 10);
  EXPECT_EQ(vec[1], 10);
  EXPECT_EQ(vec[2], 10);
  EXPECT_EQ(vec[3], 10);
}

TEST(vector, InitializerListConstructor)
{
  IntVector vec = {1, 3, 4, 6};
  EXPECT_EQ(vec.size(), 4);
  EXPECT_EQ(vec[0], 1);
  EXPECT_EQ(vec[1], 3);
  EXPECT_EQ(vec[2], 4);
  EXPECT_EQ(vec[3], 6);
}

struct TestListValue {
  TestListValue *next, *prev;
  int value;
};

TEST(vector, IntrusiveListBaseConstructor)
{
  TestListValue *value1 = new TestListValue{0, 0, 4};
  TestListValue *value2 = new TestListValue{0, 0, 5};
  TestListValue *value3 = new TestListValue{0, 0, 6};

  ListBase list = {NULL, NULL};
  BLI_addtail(&list, value1);
  BLI_addtail(&list, value2);
  BLI_addtail(&list, value3);
  Vector<TestListValue *> vec(list, true);

  EXPECT_EQ(vec.size(), 3);
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

  IntVector vec = IntVector::FromContainer(list);
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec[0], 5);
  EXPECT_EQ(vec[1], 1);
  EXPECT_EQ(vec[2], 3);
}

TEST(vector, CopyConstructor)
{
  IntVector vec1 = {1, 2, 3};
  IntVector vec2(vec1);
  EXPECT_EQ(vec2.size(), 3);
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

  EXPECT_EQ(vec1.size(), 4);
  EXPECT_EQ(vec2.size(), 4);
  EXPECT_NE(vec1.begin(), vec2.begin());
  EXPECT_EQ(vec2[0], 1);
  EXPECT_EQ(vec2[1], 2);
  EXPECT_EQ(vec2[2], 3);
  EXPECT_EQ(vec2[3], 4);
}

TEST(vector, CopyConstructor3)
{
  Vector<int, 20> vec1 = {1, 2, 3, 4};
  Vector<int, 1> vec2(vec1);

  EXPECT_EQ(vec1.size(), 4);
  EXPECT_EQ(vec2.size(), 4);
  EXPECT_NE(vec1.begin(), vec2.begin());
  EXPECT_EQ(vec2[2], 3);
}

TEST(vector, CopyConstructor4)
{
  Vector<int, 5> vec1 = {1, 2, 3, 4};
  Vector<int, 6> vec2(vec1);

  EXPECT_EQ(vec1.size(), 4);
  EXPECT_EQ(vec2.size(), 4);
  EXPECT_NE(vec1.begin(), vec2.begin());
  EXPECT_EQ(vec2[3], 4);
}

TEST(vector, MoveConstructor)
{
  IntVector vec1 = {1, 2, 3, 4};
  IntVector vec2(std::move(vec1));

  EXPECT_EQ(vec1.size(), 0);
  EXPECT_EQ(vec2.size(), 4);
  EXPECT_EQ(vec2[0], 1);
  EXPECT_EQ(vec2[1], 2);
  EXPECT_EQ(vec2[2], 3);
  EXPECT_EQ(vec2[3], 4);
}

TEST(vector, MoveConstructor2)
{
  Vector<int, 2> vec1 = {1, 2, 3, 4};
  Vector<int, 3> vec2(std::move(vec1));

  EXPECT_EQ(vec1.size(), 0);
  EXPECT_EQ(vec2.size(), 4);
  EXPECT_EQ(vec2[0], 1);
  EXPECT_EQ(vec2[1], 2);
  EXPECT_EQ(vec2[2], 3);
  EXPECT_EQ(vec2[3], 4);
}

TEST(vector, MoveConstructor3)
{
  Vector<int, 20> vec1 = {1, 2, 3, 4};
  Vector<int, 1> vec2(std::move(vec1));

  EXPECT_EQ(vec1.size(), 0);
  EXPECT_EQ(vec2.size(), 4);
  EXPECT_EQ(vec2[2], 3);
}

TEST(vector, MoveConstructor4)
{
  Vector<int, 5> vec1 = {1, 2, 3, 4};
  Vector<int, 6> vec2(std::move(vec1));

  EXPECT_EQ(vec1.size(), 0);
  EXPECT_EQ(vec2.size(), 4);
  EXPECT_EQ(vec2[3], 4);
}

TEST(vector, MoveAssignment)
{
  IntVector vec = {1, 2};
  EXPECT_EQ(vec.size(), 2);
  EXPECT_EQ(vec[0], 1);
  EXPECT_EQ(vec[1], 2);

  vec = IntVector({5});
  EXPECT_EQ(vec.size(), 1);
  EXPECT_EQ(vec[0], 5);
}

TEST(vector, CopyAssignment)
{
  IntVector vec1 = {1, 2, 3};
  IntVector vec2 = {4, 5};
  EXPECT_EQ(vec1.size(), 3);
  EXPECT_EQ(vec2.size(), 2);

  vec2 = vec1;
  EXPECT_EQ(vec2.size(), 3);

  vec1[0] = 7;
  EXPECT_EQ(vec1[0], 7);
  EXPECT_EQ(vec2[0], 1);
}

TEST(vector, Append)
{
  IntVector vec;
  vec.append(3);
  vec.append(6);
  vec.append(7);
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec[0], 3);
  EXPECT_EQ(vec[1], 6);
  EXPECT_EQ(vec[2], 7);
}

TEST(vector, AppendAndGetIndex)
{
  IntVector vec;
  EXPECT_EQ(vec.append_and_get_index(10), 0);
  EXPECT_EQ(vec.append_and_get_index(10), 1);
  EXPECT_EQ(vec.append_and_get_index(10), 2);
  vec.append(10);
  EXPECT_EQ(vec.append_and_get_index(10), 4);
}

TEST(vector, AppendNonDuplicates)
{
  IntVector vec;
  vec.append_non_duplicates(4);
  EXPECT_EQ(vec.size(), 1);
  vec.append_non_duplicates(5);
  EXPECT_EQ(vec.size(), 2);
  vec.append_non_duplicates(4);
  EXPECT_EQ(vec.size(), 2);
}

TEST(vector, ExtendNonDuplicates)
{
  IntVector vec;
  vec.extend_non_duplicates({1, 2});
  EXPECT_EQ(vec.size(), 2);
  vec.extend_non_duplicates({3, 4});
  EXPECT_EQ(vec.size(), 4);
  vec.extend_non_duplicates({0, 1, 2, 3});
  EXPECT_EQ(vec.size(), 5);
}

TEST(vector, Fill)
{
  IntVector vec(5);
  vec.fill(3);
  EXPECT_EQ(vec.size(), 5);
  EXPECT_EQ(vec[0], 3);
  EXPECT_EQ(vec[1], 3);
  EXPECT_EQ(vec[2], 3);
  EXPECT_EQ(vec[3], 3);
  EXPECT_EQ(vec[4], 3);
}

TEST(vector, FillIndices)
{
  IntVector vec(5, 0);
  vec.fill_indices({1, 2}, 4);
  EXPECT_EQ(vec[0], 0);
  EXPECT_EQ(vec[1], 4);
  EXPECT_EQ(vec[2], 4);
  EXPECT_EQ(vec[3], 0);
  EXPECT_EQ(vec[4], 0);
}

TEST(vector, Iterator)
{
  IntVector vec({1, 4, 9, 16});
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
  EXPECT_EQ(vec.size(), 100);
  for (int i = 0; i < 100; i++) {
    EXPECT_EQ(vec[i], i * 5);
  }
}

static IntVector return_by_value_helper()
{
  return IntVector({3, 5, 1});
}

TEST(vector, ReturnByValue)
{
  IntVector vec = return_by_value_helper();
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec[0], 3);
  EXPECT_EQ(vec[1], 5);
  EXPECT_EQ(vec[2], 1);
}

TEST(vector, VectorOfVectors_Append)
{
  Vector<IntVector> vec;
  EXPECT_EQ(vec.size(), 0);

  IntVector v({1, 2});
  vec.append(v);
  vec.append({7, 8});
  EXPECT_EQ(vec.size(), 2);
  EXPECT_EQ(vec[0][0], 1);
  EXPECT_EQ(vec[0][1], 2);
  EXPECT_EQ(vec[1][0], 7);
  EXPECT_EQ(vec[1][1], 8);
}

TEST(vector, VectorOfVectors_Fill)
{
  Vector<IntVector> vec(3);
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
  IntVector vec = {5, 6};
  EXPECT_EQ(vec.size(), 2);
  vec.remove_last();
  EXPECT_EQ(vec.size(), 1);
  vec.remove_last();
  EXPECT_EQ(vec.size(), 0);
}

TEST(vector, Empty)
{
  IntVector vec;
  EXPECT_TRUE(vec.empty());
  vec.append(1);
  EXPECT_FALSE(vec.empty());
  vec.remove_last();
  EXPECT_TRUE(vec.empty());
}

TEST(vector, RemoveReorder)
{
  IntVector vec = {4, 5, 6, 7};
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
  EXPECT_TRUE(vec.empty());
}

TEST(vector, RemoveFirstOccurrenceAndReorder)
{
  IntVector vec = {4, 5, 6, 7};
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
  EXPECT_EQ(vec.size(), 0);
}

TEST(vector, AllEqual_False)
{
  IntVector a = {1, 2, 3};
  IntVector b = {1, 2, 4};
  bool result = IntVector::all_equal(a, b);
  EXPECT_FALSE(result);
}

TEST(vector, AllEqual_True)
{
  IntVector a = {4, 5, 6};
  IntVector b = {4, 5, 6};
  bool result = IntVector::all_equal(a, b);
  EXPECT_TRUE(result);
}

TEST(vector, ExtendSmallVector)
{
  IntVector a = {2, 3, 4};
  IntVector b = {11, 12};
  b.extend(a);
  EXPECT_EQ(b.size(), 5);
  EXPECT_EQ(b[0], 11);
  EXPECT_EQ(b[1], 12);
  EXPECT_EQ(b[2], 2);
  EXPECT_EQ(b[3], 3);
  EXPECT_EQ(b[4], 4);
}

TEST(vector, ExtendArray)
{
  int array[] = {3, 4, 5, 6};

  IntVector a;
  a.extend(array, 2);

  EXPECT_EQ(a.size(), 2);
  EXPECT_EQ(a[0], 3);
  EXPECT_EQ(a[1], 4);
}

TEST(vector, Last)
{
  IntVector a{3, 5, 7};
  EXPECT_EQ(a.last(), 7);
}

TEST(vector, AppendNTimes)
{
  IntVector a;
  a.append_n_times(5, 3);
  a.append_n_times(2, 2);
  EXPECT_EQ(a.size(), 5);
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

  std::unique_ptr<int> &a = vec.last();
  std::unique_ptr<int> b = vec.pop_last();
  vec.remove_and_reorder(0);

  UNUSED_VARS(a, b);
}
