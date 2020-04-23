#include "BLI_vector_set.hh"
#include "testing/testing.h"

using BLI::VectorSet;
using IntVectorSet = VectorSet<int>;

TEST(vector_set, DefaultConstructor)
{
  IntVectorSet set;
  EXPECT_EQ(set.size(), 0);
}

TEST(vector_set, InitializerListConstructor_WithoutDuplicates)
{
  IntVectorSet set = {1, 4, 5};
  EXPECT_EQ(set.size(), 3);
  EXPECT_EQ(set[0], 1);
  EXPECT_EQ(set[1], 4);
  EXPECT_EQ(set[2], 5);
}

TEST(vector_set, InitializerListConstructor_WithDuplicates)
{
  IntVectorSet set = {1, 3, 3, 2, 1, 5};
  EXPECT_EQ(set.size(), 4);
  EXPECT_EQ(set[0], 1);
  EXPECT_EQ(set[1], 3);
  EXPECT_EQ(set[2], 2);
  EXPECT_EQ(set[3], 5);
}

TEST(vector_set, Copy)
{
  IntVectorSet set1 = {1, 2, 3};
  IntVectorSet set2 = set1;
  EXPECT_EQ(set1.size(), 3);
  EXPECT_EQ(set2.size(), 3);
  EXPECT_EQ(set1.index(2), 1);
  EXPECT_EQ(set2.index(2), 1);
}

TEST(vector_set, CopyAssignment)
{
  IntVectorSet set1 = {1, 2, 3};
  IntVectorSet set2 = {};
  set2 = set1;
  EXPECT_EQ(set1.size(), 3);
  EXPECT_EQ(set2.size(), 3);
  EXPECT_EQ(set1.index(2), 1);
  EXPECT_EQ(set2.index(2), 1);
}

TEST(vector_set, Move)
{
  IntVectorSet set1 = {1, 2, 3};
  IntVectorSet set2 = std::move(set1);
  EXPECT_EQ(set1.size(), 0);
  EXPECT_EQ(set2.size(), 3);
}

TEST(vector_set, MoveAssignment)
{
  IntVectorSet set1 = {1, 2, 3};
  IntVectorSet set2 = {};
  set2 = std::move(set1);
  EXPECT_EQ(set1.size(), 0);
  EXPECT_EQ(set2.size(), 3);
}

TEST(vector_set, AddNewIncreasesSize)
{
  IntVectorSet set;
  EXPECT_EQ(set.size(), 0);
  set.add(5);
  EXPECT_EQ(set.size(), 1);
}

TEST(vector_set, AddExistingDoesNotIncreaseSize)
{
  IntVectorSet set;
  EXPECT_EQ(set.size(), 0);
  set.add(5);
  EXPECT_EQ(set.size(), 1);
  set.add(5);
  EXPECT_EQ(set.size(), 1);
}

TEST(vector_set, Index)
{
  IntVectorSet set = {3, 6, 4};
  EXPECT_EQ(set.index(6), 1);
  EXPECT_EQ(set.index(3), 0);
  EXPECT_EQ(set.index(4), 2);
}

TEST(vector_set, IndexTry)
{
  IntVectorSet set = {3, 6, 4};
  EXPECT_EQ(set.index_try(5), -1);
  EXPECT_EQ(set.index_try(3), 0);
  EXPECT_EQ(set.index_try(6), 1);
  EXPECT_EQ(set.index_try(2), -1);
}

TEST(vector_set, Remove)
{
  IntVectorSet set = {4, 5, 6, 7};
  EXPECT_EQ(set.size(), 4);
  set.remove(5);
  EXPECT_EQ(set.size(), 3);
  EXPECT_EQ(set[0], 4);
  EXPECT_EQ(set[1], 7);
  EXPECT_EQ(set[2], 6);
  set.remove(6);
  EXPECT_EQ(set.size(), 2);
  EXPECT_EQ(set[0], 4);
  EXPECT_EQ(set[1], 7);
  set.remove(4);
  EXPECT_EQ(set.size(), 1);
  EXPECT_EQ(set[0], 7);
  set.remove(7);
  EXPECT_EQ(set.size(), 0);
}

TEST(vector_set, UniquePtrValue)
{
  VectorSet<std::unique_ptr<int>> set;
  set.add_new(std::unique_ptr<int>(new int()));
  set.add(std::unique_ptr<int>(new int()));
  set.index_try(std::unique_ptr<int>(new int()));
  std::unique_ptr<int> value = set.pop();
  UNUSED_VARS(value);
}
