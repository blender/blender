/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string>
#include <vector>

#include "BKE_blender_version.h"
#include "BKE_global.hh"
#include "BKE_gtest_base.hh"
#include "BKE_recents.hh"

#include "testing/testing.h"
#include "gmock/gmock.h"

namespace blender::bke::tests {

using namespace blender::recents;
using testing::ElementsAre;
using testing::FloatEq;

/* Runs with `G.background = true` so the module never touches the real
 * on-disk user config while the test suite is running. */
class RecentsTest : public BlenderGTestBase {
 protected:
  bool background_backup_ = false;

  void SetUp() override
  {
    background_backup_ = G.background;
    G.background = true;
    ensure_init();
  }

  void TearDown() override
  {
    G.background = background_backup_;
  }
};

TEST_F(RecentsTest, GetUnsetValueReturnsTypeDefault)
{
  Section section("unit_test.get_unset_value");
  EXPECT_EQ(section.get<std::string>("missing"), "");
  EXPECT_EQ(section.get<bool>("missing"), false);
  EXPECT_EQ(section.get<int32_t>("missing"), 0);
  EXPECT_EQ(section.get<float>("missing"), 0.0f);
}

TEST_F(RecentsTest, SetThenGetRoundTrip)
{
  Section section("unit_test.set_then_get");

  section.set<std::string>("str", "hello");
  EXPECT_EQ(section.get<std::string>("str"), "hello");

  section.set<bool>("flag", true);
  EXPECT_EQ(section.get<bool>("flag"), true);

  section.set<int32_t>("count", -42);
  EXPECT_EQ(section.get<int32_t>("count"), -42);

  section.set<float>("scale", 1.5f);
  EXPECT_FLOAT_EQ(section.get<float>("scale"), 1.5f);

  section.set<std::vector<int>>("indices", {1, 2, 3});
  EXPECT_THAT(section.get<std::vector<int>>("indices"), ElementsAre(1, 2, 3));

  section.remove_section();
}

TEST_F(RecentsTest, RootSectionStoresAtTopLevel)
{
  /* An empty section name reads/writes keys at the document root. */
  Section root("");
  root.set<int32_t>("unit_test_root_key", 7);
  EXPECT_EQ(root.get<int32_t>("unit_test_root_key"), 7);

  root.remove("unit_test_root_key");
  EXPECT_EQ(root.get<int32_t>("unit_test_root_key"), 0);
}

TEST_F(RecentsTest, BuiltinRootDefaultsAreReadable)
{
  Section root("");
  EXPECT_EQ(root.get<std::string>("title"), "Saved UI State Settings");
  EXPECT_EQ(root.get<std::string>("name"), "Blender");
}

TEST_F(RecentsTest, SectionsAreIsolated)
{
  Section a("unit_test.section_a");
  Section b("unit_test.section_b");

  a.set<int32_t>("value", 1);
  b.set<int32_t>("value", 2);

  EXPECT_EQ(a.get<int32_t>("value"), 1);
  EXPECT_EQ(b.get<int32_t>("value"), 2);

  a.remove_section();
  b.remove_section();
}

TEST_F(RecentsTest, RemoveItemFallsBackToDefault)
{
  Section section("unit_test.remove_item");
  section.set<int32_t>("value", 99);
  ASSERT_EQ(section.get<int32_t>("value"), 99);

  section.remove("value");
  EXPECT_EQ(section.get<int32_t>("value"), 0);
}

TEST_F(RecentsTest, RemoveSectionClearsAllItems)
{
  Section section("unit_test.remove_section");
  section.set<int32_t>("a", 1);
  section.set<int32_t>("b", 2);
  ASSERT_EQ(section.get<int32_t>("a"), 1);
  ASSERT_EQ(section.get<int32_t>("b"), 2);

  section.remove_section();

  EXPECT_EQ(section.get<int32_t>("a"), 0);
  EXPECT_EQ(section.get<int32_t>("b"), 0);
}

TEST_F(RecentsTest, BuiltinSectionLevelDefault)
{
  /* `panel.sortorder` and `panel.open` only define a section-level
   * `_default`, so any unknown key should fall back to it. */
  EXPECT_EQ(Section("panel.sortorder").get<int32_t>("unit_test_unknown_key"), -1);
  EXPECT_EQ(Section("panel.open").get<bool>("unit_test_unknown_key"), true);
}

TEST_F(RecentsTest, PerKeyDefaultOverridesSectionLevelDefault)
{
  Section dims("temp.window.dimensions");

  /* `PREFERENCES` has its own per-key default distinct from `_default`. */
  EXPECT_THAT(dims.get<std::vector<float>>("PREFERENCES"),
              ElementsAre(FloatEq(100.0f), FloatEq(940.0f), FloatEq(350.0f), FloatEq(900.0f)));

  /* An editor type with no specific default falls back to the section's `_default`. */
  EXPECT_THAT(dims.get<std::vector<float>>("UNIT_TEST_UNKNOWN_EDITOR"),
              ElementsAre(FloatEq(100.0f), FloatEq(900.0f), FloatEq(200.0f), FloatEq(800.0f)));
}

TEST_F(RecentsTest, UserValueOverridesBuiltinDefault)
{
  Section dims("temp.window.dimensions");
  const std::vector<float> builtin_default = dims.get<std::vector<float>>("PREFERENCES");

  const std::vector<float> user_value = {10.0f, 20.0f, 30.0f, 40.0f};
  dims.set<std::vector<float>>("PREFERENCES", user_value);
  EXPECT_THAT(dims.get<std::vector<float>>("PREFERENCES"),
              ElementsAre(FloatEq(10.0f), FloatEq(20.0f), FloatEq(30.0f), FloatEq(40.0f)));

  /* Removing the user override should reveal the builtin default again. */
  dims.remove("PREFERENCES");
  EXPECT_THAT(dims.get<std::vector<float>>("PREFERENCES"),
              ElementsAre(FloatEq(builtin_default[0]),
                          FloatEq(builtin_default[1]),
                          FloatEq(builtin_default[2]),
                          FloatEq(builtin_default[3])));
}

TEST_F(RecentsTest, UserSectionDefaultOverridesBuiltinSectionDefault)
{
  Section sortorder("panel.sortorder");
  ASSERT_EQ(sortorder.get<int32_t>("unit_test_unknown_key"), -1);

  sortorder.set<int32_t>("_default", 5);
  EXPECT_EQ(sortorder.get<int32_t>("unit_test_unknown_key"), 5);

  /* Restore shared builtin-section state for any other test/run in this process. */
  sortorder.remove("_default");
  EXPECT_EQ(sortorder.get<int32_t>("unit_test_unknown_key"), -1);
}

TEST_F(RecentsTest, SaveIsNoOpInBackgroundMode)
{
  /* The fixture forces background mode, so `save()` must never touch disk. */
  EXPECT_FALSE(save());
}

TEST_F(RecentsTest, SaveStampsBlenderVersion)
{
  /* `save()` still stamps the current Blender version onto the in-memory data
   * even in background mode, though the no-op path skips writing it to disk. */
  ASSERT_FALSE(save());
  EXPECT_EQ(Section("").get<int32_t>("blender_version"), BLENDER_VERSION);
}

TEST_F(RecentsTest, ExplicitInitIsSafeToCallAgain)
{
  /* In background mode `init()` only re-parses the builtin defaults and must
   * not disturb values already set by user code. */
  Section section("unit_test.explicit_init");
  section.set<int32_t>("value", 3);

  init();

  EXPECT_EQ(section.get<int32_t>("value"), 3);
  section.remove_section();
}

}  // namespace blender::bke::tests
