/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
