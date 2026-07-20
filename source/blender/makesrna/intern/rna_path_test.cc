/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_gtest_base.hh"

#include "RNA_path.hh"

#include "testing/testing.h"

namespace blender::bke::tests {

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

}  // namespace blender::bke::tests
