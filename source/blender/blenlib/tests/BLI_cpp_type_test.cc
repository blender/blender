/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_cpp_type.hh"
#include "BLI_cpp_type_make.hh"

namespace blender {

namespace tests {

static const int default_constructed_value = 1;
static const int copy_constructed_value = 2;
static const int move_constructed_value = 3;
static const int copy_constructed_from_value = 4;
static const int move_constructed_from_value = 5;
static const int copy_assigned_value = 6;
static const int copy_assigned_from_value = 7;
static const int move_assigned_value = 8;
static const int move_assigned_from_value = 9;
static const int destructed_value = 10;

struct TestType {
  mutable volatile int value;

  TestType()
  {
    value = default_constructed_value;
  }

  ~TestType()
  {
    value = destructed_value;
  }

  TestType(const TestType &other)
  {
    value = copy_constructed_value;
    other.value = copy_constructed_from_value;
  }

  TestType(TestType &&other) noexcept
  {
    value = move_constructed_value;
    other.value = move_constructed_from_value;
  }

  TestType &operator=(const TestType &other)
  {
    value = copy_assigned_value;
    other.value = copy_assigned_from_value;
    return *this;
  }

  TestType &operator=(TestType &&other) noexcept
  {
    value = move_assigned_value;
    other.value = move_assigned_from_value;
    return *this;
  }

  friend std::ostream &operator<<(std::ostream &stream, const TestType &value)
  {
    stream << value.value;
    return stream;
  }

  friend bool operator==(const TestType & /*a*/, const TestType & /*b*/)
  {
    return false;
  }

  uint64_t hash() const
  {
    return 0;
  }
};

}  // namespace tests

template<> void hash_unique_default(const tests::TestType &value, UniqueHashBytes &hash)
{
  const int int_value = value.value;
  hash.add(int_value);
}

namespace tests {

class CPPTypeTest : public testing::Test {
 public:
  const CPPType &CPPType_TestType;

  CPPTypeTest() : CPPType_TestType(CPPType::get<TestType>()) {}

  static void SetUpTestSuite()
  {
    register_cpp_types();
    BLI_CPP_TYPE_REGISTER(tests::TestType, CPPTypeFlags::BasicType);
  }
};

TEST_F(CPPTypeTest, Size)
{
  EXPECT_EQ(CPPType_TestType.size, sizeof(TestType));
}

TEST_F(CPPTypeTest, Alignment)
{
  EXPECT_EQ(CPPType_TestType.alignment, alignof(TestType));
}

TEST_F(CPPTypeTest, Is)
{
  EXPECT_TRUE(CPPType_TestType.is<TestType>());
  EXPECT_FALSE(CPPType_TestType.is<int>());
}

TEST_F(CPPTypeTest, DefaultConstruction)
{
  int buffer[10] = {0};
  CPPType_TestType.default_construct(static_cast<void *>(buffer));
  EXPECT_EQ(buffer[0], default_constructed_value);
  EXPECT_EQ(buffer[1], 0);
  CPPType_TestType.default_construct_n(static_cast<void *>(buffer), 3);
  EXPECT_EQ(buffer[0], default_constructed_value);
  EXPECT_EQ(buffer[1], default_constructed_value);
  EXPECT_EQ(buffer[2], default_constructed_value);
  EXPECT_EQ(buffer[3], 0);
  IndexMaskMemory memory;
  CPPType_TestType.default_construct_indices(static_cast<void *>(buffer),
                                             IndexMask::from_indices<int>({2, 5, 7}, memory));
  EXPECT_EQ(buffer[2], default_constructed_value);
  EXPECT_EQ(buffer[4], 0);
  EXPECT_EQ(buffer[5], default_constructed_value);
  EXPECT_EQ(buffer[6], 0);
  EXPECT_EQ(buffer[7], default_constructed_value);
  EXPECT_EQ(buffer[8], 0);
}

TEST_F(CPPTypeTest, DefaultConstructTrivial)
{
  int value = 5;
  CPPType::get<int>().default_construct(&value);
  EXPECT_EQ(value, 5);
}

TEST_F(CPPTypeTest, ValueInitialize)
{
  int buffer[10] = {0};
  CPPType_TestType.value_initialize(static_cast<void *>(buffer));
  EXPECT_EQ(buffer[0], default_constructed_value);
  EXPECT_EQ(buffer[1], 0);
  CPPType_TestType.value_initialize_n(static_cast<void *>(buffer), 3);
  EXPECT_EQ(buffer[0], default_constructed_value);
  EXPECT_EQ(buffer[1], default_constructed_value);
  EXPECT_EQ(buffer[2], default_constructed_value);
  EXPECT_EQ(buffer[3], 0);
  IndexMaskMemory memory;
  CPPType_TestType.value_initialize_indices(static_cast<void *>(buffer),
                                            IndexMask::from_indices<int>({2, 5, 7}, memory));
  EXPECT_EQ(buffer[2], default_constructed_value);
  EXPECT_EQ(buffer[4], 0);
  EXPECT_EQ(buffer[5], default_constructed_value);
  EXPECT_EQ(buffer[6], 0);
  EXPECT_EQ(buffer[7], default_constructed_value);
  EXPECT_EQ(buffer[8], 0);
}

TEST_F(CPPTypeTest, ValueInitializeTrivial)
{
  int value = 5;
  CPPType::get<int>().value_initialize(&value);
  EXPECT_EQ(value, 0);
}

TEST_F(CPPTypeTest, Destruct)
{
  int buffer[10] = {0};
  CPPType_TestType.destruct(static_cast<void *>(buffer));
  EXPECT_EQ(buffer[0], destructed_value);
  EXPECT_EQ(buffer[1], 0);
  CPPType_TestType.destruct_n(static_cast<void *>(buffer), 3);
  EXPECT_EQ(buffer[0], destructed_value);
  EXPECT_EQ(buffer[1], destructed_value);
  EXPECT_EQ(buffer[2], destructed_value);
  EXPECT_EQ(buffer[3], 0);
  IndexMaskMemory memory;
  CPPType_TestType.destruct_indices(static_cast<void *>(buffer),
                                    IndexMask::from_indices<int>({2, 5, 7}, memory));
  EXPECT_EQ(buffer[2], destructed_value);
  EXPECT_EQ(buffer[4], 0);
  EXPECT_EQ(buffer[5], destructed_value);
  EXPECT_EQ(buffer[6], 0);
  EXPECT_EQ(buffer[7], destructed_value);
  EXPECT_EQ(buffer[8], 0);
}

TEST_F(CPPTypeTest, CopyToUninitialized)
{
  int buffer1[10] = {0};
  int buffer2[10] = {0};
  CPPType_TestType.copy_construct(static_cast<void *>(buffer1), static_cast<void *>(buffer2));
  EXPECT_EQ(buffer1[0], copy_constructed_from_value);
  EXPECT_EQ(buffer2[0], copy_constructed_value);
  CPPType_TestType.copy_construct_n(static_cast<void *>(buffer1), static_cast<void *>(buffer2), 3);
  EXPECT_EQ(buffer1[0], copy_constructed_from_value);
  EXPECT_EQ(buffer2[0], copy_constructed_value);
  EXPECT_EQ(buffer1[1], copy_constructed_from_value);
  EXPECT_EQ(buffer2[1], copy_constructed_value);
  EXPECT_EQ(buffer1[2], copy_constructed_from_value);
  EXPECT_EQ(buffer2[2], copy_constructed_value);
  EXPECT_EQ(buffer1[3], 0);
  EXPECT_EQ(buffer2[3], 0);
  IndexMaskMemory memory;
  CPPType_TestType.copy_construct_indices(static_cast<void *>(buffer1),
                                          static_cast<void *>(buffer2),
                                          IndexMask::from_indices<int>({2, 5, 7}, memory));
  EXPECT_EQ(buffer1[2], copy_constructed_from_value);
  EXPECT_EQ(buffer2[2], copy_constructed_value);
  EXPECT_EQ(buffer1[4], 0);
  EXPECT_EQ(buffer2[4], 0);
  EXPECT_EQ(buffer1[5], copy_constructed_from_value);
  EXPECT_EQ(buffer2[5], copy_constructed_value);
  EXPECT_EQ(buffer1[6], 0);
  EXPECT_EQ(buffer2[6], 0);
  EXPECT_EQ(buffer1[7], copy_constructed_from_value);
  EXPECT_EQ(buffer2[7], copy_constructed_value);
  EXPECT_EQ(buffer1[8], 0);
  EXPECT_EQ(buffer2[8], 0);
}

TEST_F(CPPTypeTest, CopyToInitialized)
{
  int buffer1[10] = {0};
  int buffer2[10] = {0};
  CPPType_TestType.copy_assign(static_cast<void *>(buffer1), static_cast<void *>(buffer2));
  EXPECT_EQ(buffer1[0], copy_assigned_from_value);
  EXPECT_EQ(buffer2[0], copy_assigned_value);
  CPPType_TestType.copy_assign_n(static_cast<void *>(buffer1), static_cast<void *>(buffer2), 3);
  EXPECT_EQ(buffer1[0], copy_assigned_from_value);
  EXPECT_EQ(buffer2[0], copy_assigned_value);
  EXPECT_EQ(buffer1[1], copy_assigned_from_value);
  EXPECT_EQ(buffer2[1], copy_assigned_value);
  EXPECT_EQ(buffer1[2], copy_assigned_from_value);
  EXPECT_EQ(buffer2[2], copy_assigned_value);
  EXPECT_EQ(buffer1[3], 0);
  EXPECT_EQ(buffer2[3], 0);
  IndexMaskMemory memory;
  CPPType_TestType.copy_assign_indices(static_cast<void *>(buffer1),
                                       static_cast<void *>(buffer2),
                                       IndexMask::from_indices<int>({2, 5, 7}, memory));
  EXPECT_EQ(buffer1[2], copy_assigned_from_value);
  EXPECT_EQ(buffer2[2], copy_assigned_value);
  EXPECT_EQ(buffer1[4], 0);
  EXPECT_EQ(buffer2[4], 0);
  EXPECT_EQ(buffer1[5], copy_assigned_from_value);
  EXPECT_EQ(buffer2[5], copy_assigned_value);
  EXPECT_EQ(buffer1[6], 0);
  EXPECT_EQ(buffer2[6], 0);
  EXPECT_EQ(buffer1[7], copy_assigned_from_value);
  EXPECT_EQ(buffer2[7], copy_assigned_value);
  EXPECT_EQ(buffer1[8], 0);
  EXPECT_EQ(buffer2[8], 0);
}

TEST_F(CPPTypeTest, RelocateToUninitialized)
{
  int buffer1[10] = {0};
  int buffer2[10] = {0};
  CPPType_TestType.relocate_construct(static_cast<void *>(buffer1), static_cast<void *>(buffer2));
  EXPECT_EQ(buffer1[0], destructed_value);
  EXPECT_EQ(buffer2[0], move_constructed_value);
  CPPType_TestType.relocate_construct_n(
      static_cast<void *>(buffer1), static_cast<void *>(buffer2), 3);
  EXPECT_EQ(buffer1[0], destructed_value);
  EXPECT_EQ(buffer2[0], move_constructed_value);
  EXPECT_EQ(buffer1[1], destructed_value);
  EXPECT_EQ(buffer2[1], move_constructed_value);
  EXPECT_EQ(buffer1[2], destructed_value);
  EXPECT_EQ(buffer2[2], move_constructed_value);
  EXPECT_EQ(buffer1[3], 0);
  EXPECT_EQ(buffer2[3], 0);
  IndexMaskMemory memory;
  CPPType_TestType.relocate_construct_indices(static_cast<void *>(buffer1),
                                              static_cast<void *>(buffer2),
                                              IndexMask::from_indices<int>({2, 5, 7}, memory));
  EXPECT_EQ(buffer1[2], destructed_value);
  EXPECT_EQ(buffer2[2], move_constructed_value);
  EXPECT_EQ(buffer1[4], 0);
  EXPECT_EQ(buffer2[4], 0);
  EXPECT_EQ(buffer1[5], destructed_value);
  EXPECT_EQ(buffer2[5], move_constructed_value);
  EXPECT_EQ(buffer1[6], 0);
  EXPECT_EQ(buffer2[6], 0);
  EXPECT_EQ(buffer1[7], destructed_value);
  EXPECT_EQ(buffer2[7], move_constructed_value);
  EXPECT_EQ(buffer1[8], 0);
  EXPECT_EQ(buffer2[8], 0);
}

TEST_F(CPPTypeTest, RelocateToInitialized)
{
  int buffer1[10] = {0};
  int buffer2[10] = {0};
  CPPType_TestType.relocate_assign(static_cast<void *>(buffer1), static_cast<void *>(buffer2));
  EXPECT_EQ(buffer1[0], destructed_value);
  EXPECT_EQ(buffer2[0], move_assigned_value);
  CPPType_TestType.relocate_assign_n(
      static_cast<void *>(buffer1), static_cast<void *>(buffer2), 3);
  EXPECT_EQ(buffer1[0], destructed_value);
  EXPECT_EQ(buffer2[0], move_assigned_value);
  EXPECT_EQ(buffer1[1], destructed_value);
  EXPECT_EQ(buffer2[1], move_assigned_value);
  EXPECT_EQ(buffer1[2], destructed_value);
  EXPECT_EQ(buffer2[2], move_assigned_value);
  EXPECT_EQ(buffer1[3], 0);
  EXPECT_EQ(buffer2[3], 0);
  IndexMaskMemory memory;
  CPPType_TestType.relocate_assign_indices(static_cast<void *>(buffer1),
                                           static_cast<void *>(buffer2),
                                           IndexMask::from_indices<int>({2, 5, 7}, memory));
  EXPECT_EQ(buffer1[2], destructed_value);
  EXPECT_EQ(buffer2[2], move_assigned_value);
  EXPECT_EQ(buffer1[4], 0);
  EXPECT_EQ(buffer2[4], 0);
  EXPECT_EQ(buffer1[5], destructed_value);
  EXPECT_EQ(buffer2[5], move_assigned_value);
  EXPECT_EQ(buffer1[6], 0);
  EXPECT_EQ(buffer2[6], 0);
  EXPECT_EQ(buffer1[7], destructed_value);
  EXPECT_EQ(buffer2[7], move_assigned_value);
  EXPECT_EQ(buffer1[8], 0);
  EXPECT_EQ(buffer2[8], 0);
}

TEST_F(CPPTypeTest, FillInitialized)
{
  int buffer1 = 0;
  int buffer2[10] = {0};
  CPPType_TestType.fill_assign_n(static_cast<void *>(&buffer1), static_cast<void *>(buffer2), 3);
  EXPECT_EQ(buffer1, copy_assigned_from_value);
  EXPECT_EQ(buffer2[0], copy_assigned_value);
  EXPECT_EQ(buffer2[1], copy_assigned_value);
  EXPECT_EQ(buffer2[2], copy_assigned_value);
  EXPECT_EQ(buffer2[3], 0);

  buffer1 = 0;
  IndexMaskMemory memory;
  CPPType_TestType.fill_assign_indices(static_cast<void *>(&buffer1),
                                       static_cast<void *>(buffer2),
                                       IndexMask::from_indices<int>({1, 6, 8}, memory));
  EXPECT_EQ(buffer1, copy_assigned_from_value);
  EXPECT_EQ(buffer2[0], copy_assigned_value);
  EXPECT_EQ(buffer2[1], copy_assigned_value);
  EXPECT_EQ(buffer2[2], copy_assigned_value);
  EXPECT_EQ(buffer2[3], 0);
  EXPECT_EQ(buffer2[4], 0);
  EXPECT_EQ(buffer2[5], 0);
  EXPECT_EQ(buffer2[6], copy_assigned_value);
  EXPECT_EQ(buffer2[7], 0);
  EXPECT_EQ(buffer2[8], copy_assigned_value);
  EXPECT_EQ(buffer2[9], 0);
}

TEST_F(CPPTypeTest, FillUninitialized)
{
  int buffer1 = 0;
  int buffer2[10] = {0};
  CPPType_TestType.fill_construct_n(
      static_cast<void *>(&buffer1), static_cast<void *>(buffer2), 3);
  EXPECT_EQ(buffer1, copy_constructed_from_value);
  EXPECT_EQ(buffer2[0], copy_constructed_value);
  EXPECT_EQ(buffer2[1], copy_constructed_value);
  EXPECT_EQ(buffer2[2], copy_constructed_value);
  EXPECT_EQ(buffer2[3], 0);

  buffer1 = 0;
  IndexMaskMemory memory;
  CPPType_TestType.fill_construct_indices(static_cast<void *>(&buffer1),
                                          static_cast<void *>(buffer2),
                                          IndexMask::from_indices<int>({1, 6, 8}, memory));
  EXPECT_EQ(buffer1, copy_constructed_from_value);
  EXPECT_EQ(buffer2[0], copy_constructed_value);
  EXPECT_EQ(buffer2[1], copy_constructed_value);
  EXPECT_EQ(buffer2[2], copy_constructed_value);
  EXPECT_EQ(buffer2[3], 0);
  EXPECT_EQ(buffer2[4], 0);
  EXPECT_EQ(buffer2[5], 0);
  EXPECT_EQ(buffer2[6], copy_constructed_value);
  EXPECT_EQ(buffer2[7], 0);
  EXPECT_EQ(buffer2[8], copy_constructed_value);
  EXPECT_EQ(buffer2[9], 0);
}

TEST_F(CPPTypeTest, DebugPrint)
{
  int value = 42;
  std::stringstream ss;
  CPPType::get<int32_t>().print(static_cast<void *>(&value), ss);
  std::string text = ss.str();
  EXPECT_EQ(text, "42");
}

TEST_F(CPPTypeTest, ToStaticType)
{
  Vector<const CPPType *> types;
  auto fn = [&]<typename T>() { types.append(&CPPType::get<T>()); };
  EXPECT_TRUE((CPPType::get<std::string>().to_static_type_try<int, float, std::string>(fn)));
  EXPECT_TRUE((CPPType::get<float>().to_static_type_try<int, float, std::string>(fn)));
  EXPECT_FALSE((CPPType::get<int64_t>().to_static_type_try<int, float, std::string>(fn)));

  EXPECT_EQ(types.size(), 2);
  EXPECT_EQ(types[0], &CPPType::get<std::string>());
  EXPECT_EQ(types[1], &CPPType::get<float>());
}

TEST_F(CPPTypeTest, CopyAssignCompressed)
{
  std::array<std::string, 5> array = {"a", "b", "c", "d", "e"};
  std::array<std::string, 3> array_compressed;
  IndexMaskMemory memory;
  CPPType::get<std::string>().copy_assign_compressed(
      &array, &array_compressed, IndexMask::from_indices<int>({0, 2, 3}, memory));
  EXPECT_EQ(array_compressed[0], "a");
  EXPECT_EQ(array_compressed[1], "c");
  EXPECT_EQ(array_compressed[2], "d");
}

}  // namespace tests
}  // namespace blender
