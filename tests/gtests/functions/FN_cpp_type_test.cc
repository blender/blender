/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "testing/testing.h"

#include "FN_cpp_type.hh"

namespace blender {
namespace FN {

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

  TestType(TestType &&other)
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

  TestType &operator=(TestType &&other)
  {
    value = move_assigned_value;
    other.value = move_assigned_from_value;
    return *this;
  }
};

MAKE_CPP_TYPE(TestType, TestType)

TEST(cpp_type, Size)
{
  EXPECT_EQ(CPPType_TestType.size(), sizeof(TestType));
}

TEST(cpp_type, Alignment)
{
  EXPECT_EQ(CPPType_TestType.alignment(), alignof(TestType));
}

TEST(cpp_type, DefaultConstruction)
{
  int buffer[10] = {0};
  CPPType_TestType.construct_default((void *)buffer);
  EXPECT_EQ(buffer[0], default_constructed_value);
  EXPECT_EQ(buffer[1], 0);
  CPPType_TestType.construct_default_n((void *)buffer, 3);
  EXPECT_EQ(buffer[0], default_constructed_value);
  EXPECT_EQ(buffer[1], default_constructed_value);
  EXPECT_EQ(buffer[2], default_constructed_value);
  EXPECT_EQ(buffer[3], 0);
  CPPType_TestType.construct_default_indices((void *)buffer, {2, 5, 7});
  EXPECT_EQ(buffer[2], default_constructed_value);
  EXPECT_EQ(buffer[4], 0);
  EXPECT_EQ(buffer[5], default_constructed_value);
  EXPECT_EQ(buffer[6], 0);
  EXPECT_EQ(buffer[7], default_constructed_value);
  EXPECT_EQ(buffer[8], 0);
}

TEST(cpp_type, Destruct)
{
  int buffer[10] = {0};
  CPPType_TestType.destruct((void *)buffer);
  EXPECT_EQ(buffer[0], destructed_value);
  EXPECT_EQ(buffer[1], 0);
  CPPType_TestType.destruct_n((void *)buffer, 3);
  EXPECT_EQ(buffer[0], destructed_value);
  EXPECT_EQ(buffer[1], destructed_value);
  EXPECT_EQ(buffer[2], destructed_value);
  EXPECT_EQ(buffer[3], 0);
  CPPType_TestType.destruct_indices((void *)buffer, {2, 5, 7});
  EXPECT_EQ(buffer[2], destructed_value);
  EXPECT_EQ(buffer[4], 0);
  EXPECT_EQ(buffer[5], destructed_value);
  EXPECT_EQ(buffer[6], 0);
  EXPECT_EQ(buffer[7], destructed_value);
  EXPECT_EQ(buffer[8], 0);
}

TEST(cpp_type, CopyToUninitialized)
{
  int buffer1[10] = {0};
  int buffer2[10] = {0};
  CPPType_TestType.copy_to_uninitialized((void *)buffer1, (void *)buffer2);
  EXPECT_EQ(buffer1[0], copy_constructed_from_value);
  EXPECT_EQ(buffer2[0], copy_constructed_value);
  CPPType_TestType.copy_to_uninitialized_n((void *)buffer1, (void *)buffer2, 3);
  EXPECT_EQ(buffer1[0], copy_constructed_from_value);
  EXPECT_EQ(buffer2[0], copy_constructed_value);
  EXPECT_EQ(buffer1[1], copy_constructed_from_value);
  EXPECT_EQ(buffer2[1], copy_constructed_value);
  EXPECT_EQ(buffer1[2], copy_constructed_from_value);
  EXPECT_EQ(buffer2[2], copy_constructed_value);
  EXPECT_EQ(buffer1[3], 0);
  EXPECT_EQ(buffer2[3], 0);
  CPPType_TestType.copy_to_uninitialized_indices((void *)buffer1, (void *)buffer2, {2, 5, 7});
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

TEST(cpp_type, CopyToInitialized)
{
  int buffer1[10] = {0};
  int buffer2[10] = {0};
  CPPType_TestType.copy_to_initialized((void *)buffer1, (void *)buffer2);
  EXPECT_EQ(buffer1[0], copy_assigned_from_value);
  EXPECT_EQ(buffer2[0], copy_assigned_value);
  CPPType_TestType.copy_to_initialized_n((void *)buffer1, (void *)buffer2, 3);
  EXPECT_EQ(buffer1[0], copy_assigned_from_value);
  EXPECT_EQ(buffer2[0], copy_assigned_value);
  EXPECT_EQ(buffer1[1], copy_assigned_from_value);
  EXPECT_EQ(buffer2[1], copy_assigned_value);
  EXPECT_EQ(buffer1[2], copy_assigned_from_value);
  EXPECT_EQ(buffer2[2], copy_assigned_value);
  EXPECT_EQ(buffer1[3], 0);
  EXPECT_EQ(buffer2[3], 0);
  CPPType_TestType.copy_to_initialized_indices((void *)buffer1, (void *)buffer2, {2, 5, 7});
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

TEST(cpp_type, RelocateToUninitialized)
{
  int buffer1[10] = {0};
  int buffer2[10] = {0};
  CPPType_TestType.relocate_to_uninitialized((void *)buffer1, (void *)buffer2);
  EXPECT_EQ(buffer1[0], destructed_value);
  EXPECT_EQ(buffer2[0], move_constructed_value);
  CPPType_TestType.relocate_to_uninitialized_n((void *)buffer1, (void *)buffer2, 3);
  EXPECT_EQ(buffer1[0], destructed_value);
  EXPECT_EQ(buffer2[0], move_constructed_value);
  EXPECT_EQ(buffer1[1], destructed_value);
  EXPECT_EQ(buffer2[1], move_constructed_value);
  EXPECT_EQ(buffer1[2], destructed_value);
  EXPECT_EQ(buffer2[2], move_constructed_value);
  EXPECT_EQ(buffer1[3], 0);
  EXPECT_EQ(buffer2[3], 0);
  CPPType_TestType.relocate_to_uninitialized_indices((void *)buffer1, (void *)buffer2, {2, 5, 7});
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

TEST(cpp_type, RelocateToInitialized)
{
  int buffer1[10] = {0};
  int buffer2[10] = {0};
  CPPType_TestType.relocate_to_initialized((void *)buffer1, (void *)buffer2);
  EXPECT_EQ(buffer1[0], destructed_value);
  EXPECT_EQ(buffer2[0], move_assigned_value);
  CPPType_TestType.relocate_to_initialized_n((void *)buffer1, (void *)buffer2, 3);
  EXPECT_EQ(buffer1[0], destructed_value);
  EXPECT_EQ(buffer2[0], move_assigned_value);
  EXPECT_EQ(buffer1[1], destructed_value);
  EXPECT_EQ(buffer2[1], move_assigned_value);
  EXPECT_EQ(buffer1[2], destructed_value);
  EXPECT_EQ(buffer2[2], move_assigned_value);
  EXPECT_EQ(buffer1[3], 0);
  EXPECT_EQ(buffer2[3], 0);
  CPPType_TestType.relocate_to_initialized_indices((void *)buffer1, (void *)buffer2, {2, 5, 7});
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

TEST(cpp_type, FillInitialized)
{
  int buffer1 = 0;
  int buffer2[10] = {0};
  CPPType_TestType.fill_initialized((void *)&buffer1, (void *)buffer2, 3);
  EXPECT_EQ(buffer1, copy_assigned_from_value);
  EXPECT_EQ(buffer2[0], copy_assigned_value);
  EXPECT_EQ(buffer2[1], copy_assigned_value);
  EXPECT_EQ(buffer2[2], copy_assigned_value);
  EXPECT_EQ(buffer2[3], 0);

  buffer1 = 0;
  CPPType_TestType.fill_initialized_indices((void *)&buffer1, (void *)buffer2, {1, 6, 8});
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

TEST(cpp_type, FillUninitialized)
{
  int buffer1 = 0;
  int buffer2[10] = {0};
  CPPType_TestType.fill_uninitialized((void *)&buffer1, (void *)buffer2, 3);
  EXPECT_EQ(buffer1, copy_constructed_from_value);
  EXPECT_EQ(buffer2[0], copy_constructed_value);
  EXPECT_EQ(buffer2[1], copy_constructed_value);
  EXPECT_EQ(buffer2[2], copy_constructed_value);
  EXPECT_EQ(buffer2[3], 0);

  buffer1 = 0;
  CPPType_TestType.fill_uninitialized_indices((void *)&buffer1, (void *)buffer2, {1, 6, 8});
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

}  // namespace FN
}  // namespace blender
