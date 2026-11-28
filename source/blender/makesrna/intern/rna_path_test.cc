/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "BLI_string_ref.hh"

#include "BKE_gtest_base.hh"

#include "RNA_path.hh"

#include "testing/testing.h"

namespace blender::tests {

static void expect_member_item(const rna_path::Item &item, const StringRef expected)
{
  EXPECT_TRUE(std::holds_alternative<rna_path::Member>(item));
  const auto &member = std::get<rna_path::Member>(item);
  EXPECT_EQ(member.identifier.ref(), expected);
}

static void expect_lookup_index_item(const rna_path::Item &item, const int64_t expected)
{
  EXPECT_TRUE(std::holds_alternative<rna_path::LookupIndex>(item));
  const auto &lookup_index = std::get<rna_path::LookupIndex>(item);
  EXPECT_EQ(lookup_index.index, expected);
}

static void expect_lookup_key_item(const rna_path::Item &item, const StringRef expected)
{
  EXPECT_TRUE(std::holds_alternative<rna_path::LookupKey>(item));
  const auto &lookup_key = std::get<rna_path::LookupKey>(item);
  EXPECT_EQ(lookup_key.key.ref(), expected);
}

TEST(parse_rna_path, empty)
{
  const std::optional<ParsedRNAPath<>> path = ParsedRNAPath<>::from_string("");
  EXPECT_FALSE(path.has_value());
}

TEST(parse_rna_path, just_member)
{
  const std::optional<ParsedRNAPath<>> path = ParsedRNAPath<>::from_string("foo");
  EXPECT_EQ(path->to_string(), "foo");
  EXPECT_EQ(path->items.size(), 1);
  expect_member_item(path->items[0], "foo");
}

TEST(parse_rna_path, just_index)
{
  const std::optional<ParsedRNAPath<>> path = ParsedRNAPath<>::from_string("[42]");
  EXPECT_EQ(path->to_string(), "[42]");
  EXPECT_EQ(path->items.size(), 1);
  expect_lookup_index_item(path->items[0], 42);
}

TEST(parse_rna_path, just_key)
{
  const std::optional<ParsedRNAPath<>> path = ParsedRNAPath<>::from_string("[\"foo\"]");
  EXPECT_EQ(path->to_string(), "[\"foo\"]");
  EXPECT_EQ(path->items.size(), 1);
  expect_lookup_key_item(path->items[0], "foo");
}

TEST(parse_rna_path, multi)
{
  const std::optional<ParsedRNAPath<>> path = ParsedRNAPath<>::from_string(
      "foo[42].bar.bar2[\"b\\\"az\"]");
  EXPECT_EQ(path->to_string(), "foo[42].bar.bar2[\"b\\\"az\"]");
  EXPECT_EQ(path->items.size(), 5);
  expect_member_item(path->items[0], "foo");
  expect_lookup_index_item(path->items[1], 42);
  expect_member_item(path->items[2], "bar");
  expect_member_item(path->items[3], "bar2");
  expect_lookup_key_item(path->items[4], "b\"az");
}

TEST(parsed_rna_path, operator_eq_ne)
{
  /* To shorten the rest of the test code: */
  using ParsedPath = const ParsedRNAPath<>;
  const auto parse = [](const StringRefNull path) -> ParsedPath {
    return *ParsedPath::from_string(path);
  };

  ParsedPath empty = {};
  ParsedPath p_b_bone = parse("pose.bones[\"bone\"]");
  ParsedPath p_b_bone_2 = parse("pose.bones[\"bone\"]");
  ParsedPath p_b_bone_loc_0 = parse("pose.bones[\"bone\"].location[0]");
  ParsedPath p_b_root_loc_0 = parse("pose.bones[\"root\"].location[0]");
  ParsedPath p_b_root_loc_1 = parse("pose.bones[\"root\"].location[1]");
  ParsedPath p_b_root_mod_0_inf = parse("pose.bones[\"root\"].modifiers[0].influence");
  ParsedPath p_b_root_mod_1_inf = parse("pose.bones[\"root\"].modifiers[1].influence");

  EXPECT_EQ(empty, ParsedPath{}) << "Empty instances should be equal";
  EXPECT_EQ(p_b_bone, p_b_bone) << "Identical instances should be equal";
  EXPECT_EQ(p_b_bone, p_b_bone_2) << "Different but equal instances should be equal";
  EXPECT_NE(p_b_bone, p_b_bone_loc_0) << "Comparison of paths of different lengths should work";
}

/* Test sorting of RNA paths. */
TEST(parsed_rna_path, operator_lt)
{
  /* To shorten the rest of the test code: */
  using ParsedPath = const ParsedRNAPath<>;
  const auto parse = [](const StringRefNull path) -> ParsedPath {
    return *ParsedPath::from_string(path);
  };

  ParsedPath empty = {};
  ParsedPath location = parse("location");
  ParsedPath rotation_euler = parse("rotation_euler");
  ParsedPath p_b_bone = parse("pose.bones[\"bone\"]");
  ParsedPath p_b_bone_2 = parse("pose.bones[\"bone\"]");
  ParsedPath p_b_bone_loc_0 = parse("pose.bones[\"bone\"].location[0]");
  ParsedPath p_b_root_loc_0 = parse("pose.bones[\"root\"].location[0]");
  ParsedPath p_b_root_loc_1 = parse("pose.bones[\"root\"].location[1]");
  ParsedPath p_b_root_mod_0_inf = parse("pose.bones[\"root\"].modifiers[0].influence");
  ParsedPath p_b_root_mod_1_inf = parse("pose.bones[\"root\"].modifiers[1].influence");

  EXPECT_FALSE(empty < empty) << "Identical instances should not be less than";
  EXPECT_LT(empty, p_b_bone) << "Empty instances should be the smallest";
  EXPECT_LT(empty, p_b_root_mod_1_inf) << "Empty instances should be the smallest";
  EXPECT_LT(p_b_bone, p_b_bone_loc_0) << "Shorter should be less than longer";

  EXPECT_LT(location, rotation_euler) << "Member should be compared correctly";
  EXPECT_LT(p_b_bone_loc_0, p_b_root_loc_0) << "LookupKey should be compared correctly";
  EXPECT_LT(p_b_root_loc_0, p_b_root_loc_1) << "Final LookupIndex should be compared correctly";
  EXPECT_LT(p_b_root_mod_0_inf, p_b_root_mod_1_inf)
      << "Middle LookupIndex should be compared correctly";

  /* Test the bigger picture. Properties of the same struct should be sorted together. */
  Vector<ParsedRNAPath<>> parsed_paths{
      parse("bones[\"Zirconium\"].modifiers[0].influence"),
      parse("location"),
      parse("bones[\"Calcium\"].location"),
      parse("zbuffer"),
      parse("bones[\"Calcium\"]"),
      parse("bones[\"Zirconium\"]"),
      parse("constraints[\"LockDown\"]"),
      parse("bones[\"Zirconium\"].rotation"),
      parse("bones[\"Zirconium\"].location"),
      parse("bones[\"Calcium\"].modifiers[0].influence"),
      parse("bones[\"Calcium\"].modifiers[1].influence"),
  };
  const Vector<ParsedRNAPath<>> expected_paths{
      parse("bones[\"Calcium\"]"),
      parse("bones[\"Calcium\"].location"),
      parse("bones[\"Calcium\"].modifiers[0].influence"),
      parse("bones[\"Calcium\"].modifiers[1].influence"),
      parse("bones[\"Zirconium\"]"),
      parse("bones[\"Zirconium\"].location"),
      parse("bones[\"Zirconium\"].modifiers[0].influence"),
      parse("bones[\"Zirconium\"].rotation"),
      parse("constraints[\"LockDown\"]"),
      parse("location"),
      parse("zbuffer"),
  };
  std::sort(parsed_paths.begin(), parsed_paths.end());
  ASSERT_EQ(parsed_paths.size(), expected_paths.size())
      << "Sorting changed the length of the vector";

  /* Don't write this via EXPECT_EQ(), because it'll just log the UString binary values, not
   * anything human-readable. */
  constexpr int width = 41; /* Length of the longest path. */
  if (parsed_paths != expected_paths) {
    std::stringstream buf;
    buf << "Paths are different:" << std::endl
        << std::left << "| i  | " << std::setw(width) << "expected" << " | " << std::setw(width)
        << "actual" << " |" << std::endl
        << std::setfill('-') << "|----|" << std::setw(width + 2) << "" << "|"
        << std::setw(width + 2) << "" << "|" << std::endl
        << std::setfill(' ');
    for (const int i : parsed_paths.index_range()) {
      buf << "| " << std::setw(2) << i << " | " << std::setw(width)
          << expected_paths[i].to_string() << " | " << std::setw(width)
          << parsed_paths[i].to_string() << " |" << std::endl;
    }
    FAIL() << buf.str();
  }
}

class RNAPathTest : public bke::BlenderGTestBase {};

TEST_F(RNAPathTest, RNA_generate_keys_for_path_rename)
{
  { /* Simple infix replacement. Subscripts should be ignored. */
    auto &&[old_key, new_key] = RNA_generate_keys_for_path_rename(
        "old_name", "new_name", 1, 2, /*infix_is_name=*/false);
    EXPECT_EQ("old_name", old_key);
    EXPECT_EQ("new_name", new_key);
  }

  { /* Simple name replacement. Subscripts should be ignored. */
    auto &&[old_key, new_key] = RNA_generate_keys_for_path_rename(
        "old_name", "new_name", 1, 2, /*infix_is_name=*/true);
    EXPECT_EQ("[\"old_name\"]", old_key);
    EXPECT_EQ("[\"new_name\"]", new_key);
  }

  { /* Escaped name replacement. Subscripts should be ignored. */
    auto &&[old_key, new_key] = RNA_generate_keys_for_path_rename(
        "bone \"jaw\"", "\"jaw\" bone", 1, 2, /*infix_is_name=*/true);
    EXPECT_EQ("[\"bone \\\"jaw\\\"\"]", old_key);
    EXPECT_EQ("[\"\\\"jaw\\\" bone\"]", new_key);
  }

  {
    /* Subscript replacement, marked as 'not a name'. */
    auto &&[old_key,
            new_key] = RNA_generate_keys_for_path_rename("", "", 327, 47, /*infix_is_name=*/false);
    EXPECT_EQ("[327]", old_key);
    EXPECT_EQ("[47]", new_key);
  }

  {
    /* Subscript replacement, negative. */
    auto &&[old_key,
            new_key] = RNA_generate_keys_for_path_rename("", "", -1, 2, /*infix_is_name=*/false);
    EXPECT_EQ("[-1]", old_key);
    EXPECT_EQ("[2]", new_key);
  }

  {
    /* Subscript replacement, and marked as name. */
    auto &&[old_key,
            new_key] = RNA_generate_keys_for_path_rename("", "", 327, 47, /*infix_is_name=*/true);
    EXPECT_EQ("[327]", old_key);
    EXPECT_EQ("[47]", new_key);
  }
}

TEST_F(RNAPathTest, RNA_path_name_to_infix)
{
  {
    const std::string infix = RNA_path_name_to_infix("simple");
    EXPECT_EQ("[\"simple\"]", infix);
  }

  {
    const std::string infix = RNA_path_name_to_infix("\"quotes\"");
    /* Expected result without double escaping: ["\"quotes\""] */
    EXPECT_EQ("[\"\\\"quotes\\\"\"]", infix);
  }
}

TEST_F(RNAPathTest, RNA_path_number_to_infix)
{
  {
    const std::string infix = RNA_path_number_to_infix(1);
    EXPECT_EQ("[1]", infix);
  }

  {
    /* Negative numbers also work. */
    const std::string infix = RNA_path_number_to_infix(-1);
    EXPECT_EQ("[-1]", infix);
  }
}

}  // namespace blender::tests
