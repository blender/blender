/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_string_utils.hh"

#include <string>

#include "testing/testing.h"

#include "BLI_vector.hh"

namespace blender {

TEST(BLI_string_utils, BLI_string_replace)
{
  {
    std::string s = "foo bar baz";
    BLI_string_replace(s, "bar", "hello");
    EXPECT_EQ(s, "foo hello baz");
  }

  {
    std::string s = "foo bar baz world bar";
    BLI_string_replace(s, "bar", "hello");
    EXPECT_EQ(s, "foo hello baz world hello");
  }
}

TEST(BLI_string_utils, BLI_uniquename_cb)
{
  const Vector<std::string> current_names{"Foo", "Bar", "Bar.003", "Baz.001", "Big.999"};

  {
    auto unique_check_func = [&](const StringRef check_name) {
      return current_names.contains(check_name);
    };

    {
      char name[64] = "";
      BLI_uniquename_cb(unique_check_func, "Default Name", '.', name, sizeof(name));
      EXPECT_STREQ(name, "Default Name");
    }

    {
      char name[64] = "Baz";
      BLI_uniquename_cb(unique_check_func, "Default Name", '.', name, sizeof(name));
      EXPECT_STREQ(name, "Baz");
    }

    {
      char name[64] = "Foo";
      BLI_uniquename_cb(unique_check_func, "Default Name", '.', name, sizeof(name));
      EXPECT_STREQ(name, "Foo.001");
    }

    {
      char name[64] = "Baz.001";
      BLI_uniquename_cb(unique_check_func, "Default Name", '.', name, sizeof(name));
      EXPECT_STREQ(name, "Baz.002");
    }

    {
      char name[64] = "Bar.003";
      BLI_uniquename_cb(unique_check_func, "Default Name", '.', name, sizeof(name));
      EXPECT_STREQ(name, "Bar.004");
    }

    {
      char name[64] = "Big.999";
      BLI_uniquename_cb(unique_check_func, "Default Name", '.', name, sizeof(name));
      EXPECT_STREQ(name, "Big.1000");
    }
  }

  {
    const auto unique_check = [&](const blender::StringRef name) -> bool {
      return current_names.contains(name);
    };

    EXPECT_EQ(BLI_uniquename_cb(unique_check, '.', ""), "");
    EXPECT_EQ(BLI_uniquename_cb(unique_check, '.', "Baz"), "Baz");
    EXPECT_EQ(BLI_uniquename_cb(unique_check, '.', "Foo"), "Foo.001");
    EXPECT_EQ(BLI_uniquename_cb(unique_check, '.', "Baz.001"), "Baz.002");
    EXPECT_EQ(BLI_uniquename_cb(unique_check, '.', "Bar.003"), "Bar.004");
    EXPECT_EQ(BLI_uniquename_cb(unique_check, '.', "Big.999"), "Big.1000");
  }
}

}  // namespace blender
