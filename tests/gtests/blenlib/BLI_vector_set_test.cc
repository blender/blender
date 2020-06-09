#include "BLI_strict_flags.h"
#include "BLI_vector_set.hh"
#include "testing/testing.h"

using BLI::VectorSet;

TEST(vector_set, DefaultConstructor)
{
  VectorSet<int> set;
  EXPECT_EQ(set.size(), 0);
  EXPECT_TRUE(set.is_empty());
}

TEST(vector_set, InitializerListConstructor_WithoutDuplicates)
{
  VectorSet<int> set = {1, 4, 5};
  EXPECT_EQ(set.size(), 3);
  EXPECT_EQ(set[0], 1);
  EXPECT_EQ(set[1], 4);
  EXPECT_EQ(set[2], 5);
}

TEST(vector_set, InitializerListConstructor_WithDuplicates)
{
  VectorSet<int> set = {1, 3, 3, 2, 1, 5};
  EXPECT_EQ(set.size(), 4);
  EXPECT_EQ(set[0], 1);
  EXPECT_EQ(set[1], 3);
  EXPECT_EQ(set[2], 2);
  EXPECT_EQ(set[3], 5);
}

TEST(vector_set, Copy)
{
  VectorSet<int> set1 = {1, 2, 3};
  VectorSet<int> set2 = set1;
  EXPECT_EQ(set1.size(), 3);
  EXPECT_EQ(set2.size(), 3);
  EXPECT_EQ(set1.index_of(2), 1);
  EXPECT_EQ(set2.index_of(2), 1);
}

TEST(vector_set, CopyAssignment)
{
  VectorSet<int> set1 = {1, 2, 3};
  VectorSet<int> set2 = {};
  set2 = set1;
  EXPECT_EQ(set1.size(), 3);
  EXPECT_EQ(set2.size(), 3);
  EXPECT_EQ(set1.index_of(2), 1);
  EXPECT_EQ(set2.index_of(2), 1);
}

TEST(vector_set, Move)
{
  VectorSet<int> set1 = {1, 2, 3};
  VectorSet<int> set2 = std::move(set1);
  EXPECT_EQ(set1.size(), 0);
  EXPECT_EQ(set2.size(), 3);
}

TEST(vector_set, MoveAssignment)
{
  VectorSet<int> set1 = {1, 2, 3};
  VectorSet<int> set2 = {};
  set2 = std::move(set1);
  EXPECT_EQ(set1.size(), 0);
  EXPECT_EQ(set2.size(), 3);
}

TEST(vector_set, AddNewIncreasesSize)
{
  VectorSet<int> set;
  EXPECT_TRUE(set.is_empty());
  EXPECT_EQ(set.size(), 0);
  set.add(5);
  EXPECT_FALSE(set.is_empty());
  EXPECT_EQ(set.size(), 1);
}

TEST(vector_set, AddExistingDoesNotIncreaseSize)
{
  VectorSet<int> set;
  EXPECT_EQ(set.size(), 0);
  EXPECT_TRUE(set.add(5));
  EXPECT_EQ(set.size(), 1);
  EXPECT_FALSE(set.add(5));
  EXPECT_EQ(set.size(), 1);
}

TEST(vector_set, Index)
{
  VectorSet<int> set = {3, 6, 4};
  EXPECT_EQ(set.index_of(6), 1);
  EXPECT_EQ(set.index_of(3), 0);
  EXPECT_EQ(set.index_of(4), 2);
}

TEST(vector_set, IndexTry)
{
  VectorSet<int> set = {3, 6, 4};
  EXPECT_EQ(set.index_of_try(5), -1);
  EXPECT_EQ(set.index_of_try(3), 0);
  EXPECT_EQ(set.index_of_try(6), 1);
  EXPECT_EQ(set.index_of_try(2), -1);
}

TEST(vector_set, RemoveContained)
{
  VectorSet<int> set = {4, 5, 6, 7};
  EXPECT_EQ(set.size(), 4);
  set.remove_contained(5);
  EXPECT_EQ(set.size(), 3);
  EXPECT_EQ(set[0], 4);
  EXPECT_EQ(set[1], 7);
  EXPECT_EQ(set[2], 6);
  set.remove_contained(6);
  EXPECT_EQ(set.size(), 2);
  EXPECT_EQ(set[0], 4);
  EXPECT_EQ(set[1], 7);
  set.remove_contained(4);
  EXPECT_EQ(set.size(), 1);
  EXPECT_EQ(set[0], 7);
  set.remove_contained(7);
  EXPECT_EQ(set.size(), 0);
}

TEST(vector_set, AddMultipleTimes)
{
  VectorSet<int> set;
  for (int i = 0; i < 100; i++) {
    EXPECT_FALSE(set.contains(i * 13));
    set.add(i * 12);
    set.add(i * 13);
    EXPECT_TRUE(set.contains(i * 13));
  }
}

TEST(vector_set, UniquePtrValue)
{
  VectorSet<std::unique_ptr<int>> set;
  set.add_new(std::unique_ptr<int>(new int()));
  set.add(std::unique_ptr<int>(new int()));
  set.index_of_try(std::unique_ptr<int>(new int()));
  std::unique_ptr<int> value = set.pop();
  UNUSED_VARS(value);
}

TEST(vector_set, Remove)
{
  VectorSet<int> set;
  EXPECT_TRUE(set.add(5));
  EXPECT_TRUE(set.contains(5));
  EXPECT_FALSE(set.remove(6));
  EXPECT_TRUE(set.contains(5));
  EXPECT_TRUE(set.remove(5));
  EXPECT_FALSE(set.contains(5));
  EXPECT_FALSE(set.remove(5));
  EXPECT_FALSE(set.contains(5));
}
