#include "testing/testing.h"
#include "BLI_set_vector.h"

using IntSetVector = BLI::SetVector<int>;

TEST(set_vector, DefaultConstructor)
{
  IntSetVector set;
  EXPECT_EQ(set.size(), 0);
}

TEST(set_vector, InitializerListConstructor_WithoutDuplicates)
{
  IntSetVector set = {1, 4, 5};
  EXPECT_EQ(set.size(), 3);
  EXPECT_EQ(set[0], 1);
  EXPECT_EQ(set[1], 4);
  EXPECT_EQ(set[2], 5);
}

TEST(set_vector, InitializerListConstructor_WithDuplicates)
{
  IntSetVector set = {1, 3, 3, 2, 1, 5};
  EXPECT_EQ(set.size(), 4);
  EXPECT_EQ(set[0], 1);
  EXPECT_EQ(set[1], 3);
  EXPECT_EQ(set[2], 2);
  EXPECT_EQ(set[3], 5);
}

TEST(set_vector, Copy)
{
  IntSetVector set1 = {1, 2, 3};
  IntSetVector set2 = set1;
  EXPECT_EQ(set1.size(), 3);
  EXPECT_EQ(set2.size(), 3);
  EXPECT_EQ(set1.index(2), 1);
  EXPECT_EQ(set2.index(2), 1);
}

TEST(set_vector, Move)
{
  IntSetVector set1 = {1, 2, 3};
  IntSetVector set2 = std::move(set1);
  EXPECT_EQ(set1.size(), 0);
  EXPECT_EQ(set2.size(), 3);
}

TEST(set_vector, AddNewIncreasesSize)
{
  IntSetVector set;
  EXPECT_EQ(set.size(), 0);
  set.add(5);
  EXPECT_EQ(set.size(), 1);
}

TEST(set_vector, AddExistingDoesNotIncreaseSize)
{
  IntSetVector set;
  EXPECT_EQ(set.size(), 0);
  set.add(5);
  EXPECT_EQ(set.size(), 1);
  set.add(5);
  EXPECT_EQ(set.size(), 1);
}

TEST(set_vector, Index)
{
  IntSetVector set = {3, 6, 4};
  EXPECT_EQ(set.index(6), 1);
  EXPECT_EQ(set.index(3), 0);
  EXPECT_EQ(set.index(4), 2);
}

TEST(set_vector, IndexTry)
{
  IntSetVector set = {3, 6, 4};
  EXPECT_EQ(set.index_try(5), -1);
  EXPECT_EQ(set.index_try(3), 0);
  EXPECT_EQ(set.index_try(6), 1);
  EXPECT_EQ(set.index_try(2), -1);
}

TEST(set_vector, Remove)
{
  IntSetVector set = {4, 5, 6, 7};
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
