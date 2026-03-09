/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_map.hh"
#include "BLI_ustring.hh"

namespace blender::tests {

TEST(ustring, MapUString)
{
  Map<UString, int> map;
  map.add("hello"_ustr, 1);
  map.add_as("world", 2);
  map.add_as("world", 3);
  EXPECT_EQ(map.lookup("hello"_ustr), 1);
  EXPECT_EQ(map.lookup_as("hello"), 1);
  EXPECT_EQ(map.lookup_as("world"), 2);
  EXPECT_EQ(map.lookup_as("world"_ustr), 2);
}

TEST(ustring, MapStringRef)
{
  Map<StringRef, int> map;
  map.add("hello", 1);
  map.add("world"_ustr.ref(), 2);
  map.add_as("world"_ustr.ref(), 3);
  EXPECT_EQ(map.lookup("hello"), 1);
  EXPECT_EQ(map.lookup("hello"_ustr.ref()), 1);
  EXPECT_EQ(map.lookup("world"), 2);
  EXPECT_EQ(map.lookup("world"_ustr.ref()), 2);
}

TEST(ustring, MapStdString)
{
  Map<std::string, int> map;
  map.add("hello", 1);
  map.add_as("world"_ustr.string(), 2);
  EXPECT_EQ(map.lookup("hello"), 1);
  EXPECT_EQ(map.lookup_as("hello"_ustr.ref()), 1);
  EXPECT_EQ(map.lookup("world"), 2);
  EXPECT_EQ(map.lookup_as("world"_ustr.ref()), 2);
}

TEST(ustring, Equality)
{
  /* This is mostly just checking if all these equality checks compile fine without ambiguities. */
  EXPECT_EQ("test"_ustr, "test"_ustr);
  EXPECT_EQ("test"_ustr, "test");
  EXPECT_EQ("test", "test"_ustr);
  EXPECT_EQ("test"_ustr, StringRef("test"));
  EXPECT_EQ(StringRef("test"), "test"_ustr);
  EXPECT_EQ("test"_ustr, std::string("test"));
  EXPECT_EQ(std::string("test"), "test"_ustr);
  EXPECT_EQ("test"_ustr, StringRefNull("test"));
  EXPECT_EQ(StringRefNull("test"), "test"_ustr);
  EXPECT_EQ("test"_ustr, std::string_view("test"));
  EXPECT_EQ(std::string_view("test"), "test"_ustr);
}

}  // namespace blender::tests
