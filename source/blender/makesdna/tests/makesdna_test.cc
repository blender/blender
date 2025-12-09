/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include <array>

#include "DNA_genfile.h"
#include "DNA_sdna_types.h"

#include "BLI_string_ref.hh"

#include "dna/dna_test.h"

namespace blender::dna {
namespace {

class SDNATest : public ::testing::Test {
  virtual void SetUp() override
  {
    sdna = DNA_sdna_from_data(DNAstr, DNAlen, false, true, nullptr);
  }

  virtual void TearDown() override
  {
    if (sdna) {
      DNA_sdna_free(sdna);
    }
  }

 protected:
  SDNA *sdna = nullptr;
};

/**
 * Get the struct member with an exact name.
 * For example, expects the C array size in the name.
 * For example, pass "name[258]" to access the test struct name, not `name[]`.
 */
const SDNA_StructMember *get_struct_member(const SDNA *sdna,
                                           const StringRefNull struct_name,
                                           const StringRefNull member_name)
{
  const int struct_index = DNA_struct_find_index_without_alias(sdna, struct_name.c_str());
  if (struct_index == -1) {
    return nullptr;
  }

  const SDNA_Struct *struct_ptr = sdna->structs[struct_index];
  for (int i = 0; i < struct_ptr->members_num; ++i) {
    const SDNA_StructMember &struct_member = struct_ptr->members[i];
    if (sdna->members[struct_member.member_index] == member_name) {
      return &struct_member;
    }
  }

  return nullptr;
}

/**
 * Get struct member size with the exact name.
 * For example, expects the C array size in the name.
 * For example, pass `name[258]` to access test struct name, not `name[]`.
 */
int get_struct_member_size(const SDNA *sdna,
                           const StringRefNull struct_name,
                           const StringRefNull member_name)
{
  const SDNA_StructMember *struct_member = get_struct_member(sdna, struct_name, member_name);
  if (!struct_member) {
    return -1;
  }
  return DNA_struct_member_size(sdna, struct_member->type_index, struct_member->member_index);
}

}  // namespace

constexpr int kRawDataStructId = 0;  /* raw_data */
constexpr int kListBaseStructId = 1; /* ListBase */
constexpr int kTestStructId = 2;     /* ID */

TEST_F(SDNATest, basic)
{
  ASSERT_NE(sdna, nullptr);

  EXPECT_EQ(sdna->structs_num, 3);
}

TEST_F(SDNATest, index_without_alias)
{
  ASSERT_NE(sdna, nullptr);

  EXPECT_EQ(DNA_struct_find_index_without_alias(sdna, "raw_data"), kRawDataStructId);
  EXPECT_EQ(DNA_struct_find_index_without_alias(sdna, "ListBase"), kListBaseStructId);
  EXPECT_EQ(DNA_struct_find_index_without_alias(sdna, "TestStruct"), kTestStructId);
}

TEST_F(SDNATest, struct_size)
{
  ASSERT_NE(sdna, nullptr);

  EXPECT_EQ(DNA_struct_size(sdna, kListBaseStructId), sizeof(ListBase));
  EXPECT_EQ(DNA_struct_size(sdna, kTestStructId), sizeof(TestStruct));
}

TEST_F(SDNATest, struct_member_size)
{
  ASSERT_NE(sdna, nullptr);

  EXPECT_EQ(get_struct_member_size(sdna, "TestStruct", "*next"), sizeof(TestStruct::next));
  EXPECT_EQ(get_struct_member_size(sdna, "TestStruct", "*prev"), sizeof(TestStruct::prev));
  EXPECT_EQ(get_struct_member_size(sdna, "TestStruct", "name[258]"), sizeof(TestStruct::name));
  EXPECT_EQ(get_struct_member_size(sdna, "TestStruct", "flag"), sizeof(TestStruct::flag));
  EXPECT_EQ(get_struct_member_size(sdna, "TestStruct", "some_list"),
            sizeof(TestStruct::some_list));
  EXPECT_EQ(get_struct_member_size(sdna, "TestStruct", "tag"), sizeof(TestStruct::tag));
}

TEST_F(SDNATest, struct_member_offset_by_name_without_alias)
{
  ASSERT_NE(sdna, nullptr);

  EXPECT_EQ(DNA_struct_member_offset_by_name_without_alias(sdna, "TestStruct", "void", "*next"),
            offsetof(TestStruct, next));
  EXPECT_EQ(DNA_struct_member_offset_by_name_without_alias(sdna, "TestStruct", "void", "*prev"),
            offsetof(TestStruct, prev));
  EXPECT_EQ(DNA_struct_member_offset_by_name_without_alias(sdna, "TestStruct", "char", "name[]"),
            offsetof(TestStruct, name));
  EXPECT_EQ(DNA_struct_member_offset_by_name_without_alias(sdna, "TestStruct", "short", "flag"),
            offsetof(TestStruct, flag));
  EXPECT_EQ(
      DNA_struct_member_offset_by_name_without_alias(sdna, "TestStruct", "ListBase", "some_list"),
      offsetof(TestStruct, some_list));
  EXPECT_EQ(DNA_struct_member_offset_by_name_without_alias(sdna, "TestStruct", "int", "tag"),
            offsetof(TestStruct, tag));
}

}  // namespace blender::dna
