/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "AS_asset_catalog_path.hh"

#include "BLI_set.hh"
#include "BLI_vector.hh"

#include <set>
#include <sstream>

#include "testing/testing.h"

namespace blender::asset_system::tests {

TEST(AssetCatalogPathTest, construction)
{
  AssetCatalogPath default_constructed;
  /* Use `.str()` to use `std:string`'s comparison operators here, not our own (which are tested
   * later). */
  EXPECT_EQ(default_constructed.str(), "");

  /* C++ considers this construction special, it doesn't call the default constructor but does
   * recursive, member-wise value initialization. See https://stackoverflow.com/a/4982720. */
  AssetCatalogPath value_initialized = AssetCatalogPath();
  EXPECT_EQ(value_initialized.str(), "");

  AssetCatalogPath from_char_literal("the/path");

  const std::string str_const = "the/path";
  AssetCatalogPath from_string_constant(str_const);

  std::string str_variable = "the/path";
  AssetCatalogPath from_string_variable(str_variable);

  std::string long_string = "this is a long/string/with/a/path in the middle";
  StringRef long_string_ref(long_string);
  StringRef middle_bit = long_string_ref.substr(10, 23);
  AssetCatalogPath from_string_ref(middle_bit);
  EXPECT_EQ(from_string_ref, "long/string/with/a/path");
}

TEST(AssetCatalogPathTest, length)
{
  const AssetCatalogPath one("1");
  EXPECT_EQ(1, one.length());

  const AssetCatalogPath empty("");
  EXPECT_EQ(0, empty.length());

  const AssetCatalogPath utf8("some/родитель");
  EXPECT_EQ(21, utf8.length()) << "13 characters should be 21 bytes.";
}

TEST(AssetCatalogPathTest, name)
{
  EXPECT_EQ(StringRefNull(""), AssetCatalogPath("").name());
  EXPECT_EQ(StringRefNull("word"), AssetCatalogPath("word").name());
  EXPECT_EQ(StringRefNull("Пермь"), AssetCatalogPath("дорога/в/Пермь").name());
  EXPECT_EQ(StringRefNull("windows\\paths"),
            AssetCatalogPath("these/are/not/windows\\paths").name());
}

TEST(AssetCatalogPathTest, comparison_operators)
{
  const AssetCatalogPath empty("");
  const AssetCatalogPath the_path("the/path");
  const AssetCatalogPath the_path_child("the/path/child");
  const AssetCatalogPath unrelated_path("unrelated/path");
  const AssetCatalogPath other_instance_same_path("the/path");

  EXPECT_LT(empty, the_path);
  EXPECT_LT(the_path, the_path_child);
  EXPECT_LT(the_path, unrelated_path);

  EXPECT_EQ(empty, empty) << "Identical empty instances should compare equal.";
  EXPECT_EQ(empty, "") << "Comparison to empty string should be possible.";
  EXPECT_EQ(the_path, the_path) << "Identical non-empty instances should compare equal.";
  EXPECT_EQ(the_path, "the/path") << "Comparison to string should be possible.";
  EXPECT_EQ(the_path, other_instance_same_path)
      << "Different instances with equal path should compare equal.";

  EXPECT_NE(the_path, the_path_child);
  EXPECT_NE(the_path, unrelated_path);
  EXPECT_NE(the_path, empty);

  EXPECT_FALSE(empty);
  EXPECT_TRUE(the_path);
}

TEST(AssetCatalogPathTest, move_semantics)
{
  AssetCatalogPath source_path("source/path");
  EXPECT_TRUE(source_path);

  AssetCatalogPath dest_path = std::move(source_path);
  EXPECT_FALSE(source_path); /* NOLINT: bugprone-use-after-move */
  EXPECT_TRUE(dest_path);
}

TEST(AssetCatalogPathTest, concatenation)
{
  AssetCatalogPath some_parent("some/родитель");
  AssetCatalogPath child = some_parent / "ребенок";

  EXPECT_EQ(some_parent, "some/родитель")
      << "Appending a child path should not modify the parent.";
  EXPECT_EQ(child, "some/родитель/ребенок");

  AssetCatalogPath appended_compound_path = some_parent / "ребенок/внук";
  EXPECT_EQ(appended_compound_path, "some/родитель/ребенок/внук");

  AssetCatalogPath empty("");
  AssetCatalogPath child_of_the_void = empty / "child";
  EXPECT_EQ(child_of_the_void, "child")
      << "Appending to an empty path should not create an initial slash.";

  AssetCatalogPath parent_of_the_void = some_parent / empty;
  EXPECT_EQ(parent_of_the_void, "some/родитель")
      << "Prepending to an empty path should not create a trailing slash.";

  std::string subpath = "child";
  AssetCatalogPath concatenated_with_string = some_parent / subpath;
  EXPECT_EQ(concatenated_with_string, "some/родитель/child");
}

TEST(AssetCatalogPathTest, hashable)
{
  AssetCatalogPath path("heyyyyy");

  std::set<AssetCatalogPath> path_std_set;
  path_std_set.insert(path);

  blender::Set<AssetCatalogPath> path_blender_set;
  path_blender_set.add(path);
}

TEST(AssetCatalogPathTest, stream_operator)
{
  AssetCatalogPath path("путь/в/Пермь");
  std::stringstream sstream;
  sstream << path;
  EXPECT_EQ("путь/в/Пермь", sstream.str());
}

TEST(AssetCatalogPathTest, is_contained_in)
{
  const AssetCatalogPath catpath("simple/path/child");
  EXPECT_FALSE(catpath.is_contained_in("unrelated"));
  EXPECT_FALSE(catpath.is_contained_in("sim"));
  EXPECT_FALSE(catpath.is_contained_in("simple/pathx"));
  EXPECT_FALSE(catpath.is_contained_in("simple/path/c"));
  EXPECT_FALSE(catpath.is_contained_in("simple/path/child/grandchild"));
  EXPECT_FALSE(catpath.is_contained_in("simple/path/"))
      << "Non-normalized paths are not expected to work.";

  EXPECT_TRUE(catpath.is_contained_in(""));
  EXPECT_TRUE(catpath.is_contained_in("simple"));
  EXPECT_TRUE(catpath.is_contained_in("simple/path"));

  /* Test with some UTF8 non-ASCII characters. */
  AssetCatalogPath some_parent("some/родитель");
  AssetCatalogPath child = some_parent / "ребенок";

  EXPECT_TRUE(child.is_contained_in(some_parent));
  EXPECT_TRUE(child.is_contained_in("some"));

  AssetCatalogPath appended_compound_path = some_parent / "ребенок/внук";
  EXPECT_TRUE(appended_compound_path.is_contained_in(some_parent));
  EXPECT_TRUE(appended_compound_path.is_contained_in(child));

  /* Test "going up" directory-style. */
  AssetCatalogPath child_with_dotdot = some_parent / "../../other/hierarchy/part";
  EXPECT_TRUE(child_with_dotdot.is_contained_in(some_parent))
      << "dotdot path components should have no meaning";
}

TEST(AssetCatalogPathTest, cleanup)
{
  {
    AssetCatalogPath ugly_path("/  some /   родитель  / ");
    AssetCatalogPath clean_path = ugly_path.cleanup();
    EXPECT_EQ(AssetCatalogPath("/  some /   родитель  / "), ugly_path)
        << "cleanup should not modify the path instance itself";
    EXPECT_EQ(AssetCatalogPath("some/родитель"), clean_path);
  }
  {
    AssetCatalogPath double_slashed("some//родитель");
    EXPECT_EQ(AssetCatalogPath("some/родитель"), double_slashed.cleanup());
  }
  {
    AssetCatalogPath with_colons("some/key:subkey=value/path");
    EXPECT_EQ(AssetCatalogPath("some/key-subkey=value/path"), with_colons.cleanup());
  }
  {
    const AssetCatalogPath with_backslashes("windows\\for\\life");
    EXPECT_EQ(AssetCatalogPath("windows/for/life"), with_backslashes.cleanup());
  }
  {
    const AssetCatalogPath with_mixed("windows\\for/life");
    EXPECT_EQ(AssetCatalogPath("windows/for/life"), with_mixed.cleanup());
  }
  {
    const AssetCatalogPath with_punctuation("is!/this?/¿valid?");
    EXPECT_EQ(AssetCatalogPath("is!/this?/¿valid?"), with_punctuation.cleanup());
  }
}

TEST(AssetCatalogPathTest, iterate_components)
{
  AssetCatalogPath path("путь/в/Пермь");
  Vector<std::pair<std::string, bool>> seen_components;

  path.iterate_components([&seen_components](StringRef component_name, bool is_last_component) {
    std::pair<std::string, bool> parameter_pair = std::make_pair<std::string, bool>(
        component_name, bool(is_last_component));
    seen_components.append(parameter_pair);
  });

  ASSERT_EQ(3, seen_components.size());

  EXPECT_EQ("путь", seen_components[0].first);
  EXPECT_EQ("в", seen_components[1].first);
  EXPECT_EQ("Пермь", seen_components[2].first);

  EXPECT_FALSE(seen_components[0].second);
  EXPECT_FALSE(seen_components[1].second);
  EXPECT_TRUE(seen_components[2].second);
}

TEST(AssetCatalogPathTest, rebase)
{
  AssetCatalogPath path("some/path/to/some/catalog");
  EXPECT_EQ(path.rebase("some/path", "new/base"), "new/base/to/some/catalog");
  EXPECT_EQ(path.rebase("", "new/base"), "new/base/some/path/to/some/catalog");

  EXPECT_EQ(path.rebase("some/path/to/some/catalog", "some/path/to/some/catalog"),
            "some/path/to/some/catalog")
      << "Rebasing to itself should not change the path.";

  EXPECT_EQ(path.rebase("path/to", "new/base"), "")
      << "Non-matching base path should return empty string to indicate 'NO'.";

  /* Empty strings should be handled without crashing or other nasty side-effects. */
  AssetCatalogPath empty("");
  EXPECT_EQ(empty.rebase("path/to", "new/base"), "");
  EXPECT_EQ(empty.rebase("", "new/base"), "new/base");
  EXPECT_EQ(empty.rebase("", ""), "");
}

TEST(AssetCatalogPathTest, parent)
{
  const AssetCatalogPath ascii_path("path/with/missing/parents");
  EXPECT_EQ(ascii_path.parent(), "path/with/missing");

  const AssetCatalogPath path("путь/в/Пермь/долог/и/далек");
  EXPECT_EQ(path.parent(), "путь/в/Пермь/долог/и");
  EXPECT_EQ(path.parent().parent(), "путь/в/Пермь/долог");
  EXPECT_EQ(path.parent().parent().parent(), "путь/в/Пермь");

  const AssetCatalogPath one_level("one");
  EXPECT_EQ(one_level.parent(), "");

  const AssetCatalogPath empty("");
  EXPECT_EQ(empty.parent(), "");
}

}  // namespace blender::asset_system::tests
