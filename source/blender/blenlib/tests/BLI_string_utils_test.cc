/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_string_utils.hh"

#include <string>

#include "testing/testing.h"

#include "BLI_vector.hh"

namespace blender {

static bool unique_check_func(void *arg, const char *name)
{
  const Vector<std::string> *current_names = static_cast<const Vector<std::string> *>(arg);
  return current_names->contains(name);
}

TEST(BLI_string_utils, BLI_uniquename_cb)
{
  const Vector<std::string> current_names{"Foo", "Bar", "Bar.003", "Baz.001", "Big.999"};

  /* C version. */
  {
    void *arg = const_cast<void *>(static_cast<const void *>(&current_names));

    {
      char name[64] = "";
      BLI_uniquename_cb(unique_check_func, arg, "Default Name", '.', name, sizeof(name));
      EXPECT_STREQ(name, "Default Name");
    }

    {
      char name[64] = "Baz";
      BLI_uniquename_cb(unique_check_func, arg, "Default Name", '.', name, sizeof(name));
      EXPECT_STREQ(name, "Baz");
    }

    {
      char name[64] = "Foo";
      BLI_uniquename_cb(unique_check_func, arg, "Default Name", '.', name, sizeof(name));
      EXPECT_STREQ(name, "Foo.001");
    }

    {
      char name[64] = "Baz.001";
      BLI_uniquename_cb(unique_check_func, arg, "Default Name", '.', name, sizeof(name));
      EXPECT_STREQ(name, "Baz.002");
    }

    {
      char name[64] = "Bar.003";
      BLI_uniquename_cb(unique_check_func, arg, "Default Name", '.', name, sizeof(name));
      EXPECT_STREQ(name, "Bar.004");
    }

    {
      char name[64] = "Big.999";
      BLI_uniquename_cb(unique_check_func, arg, "Default Name", '.', name, sizeof(name));
      EXPECT_STREQ(name, "Big.1000");
    }
  }

  /* C++ version. */
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
