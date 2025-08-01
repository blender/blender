/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "testing/testing.h"

#include "CLG_log.h"

#include "BKE_appdir.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_node.hh"
#include "BKE_scene.hh"

#include "IMB_imbuf.hh"

#include "RNA_define.hh"

#include "NOD_geometry_nodes_bundle.hh"

namespace blender::nodes::tests {

class BundleTest : public ::testing::Test {

 protected:
  static void SetUpTestSuite()
  {
    CLG_init();
    BKE_idtype_init();
    RNA_init();
    blender::bke::node_system_init();
    BKE_appdir_init();
    IMB_init();
    BKE_materials_init();
  }

  static void TearDownTestSuite()
  {
    BKE_materials_exit();
    bke::node_system_exit();
    RNA_exit();
    BKE_appdir_exit();
    IMB_exit();
    CLG_exit();
  }
};

TEST_F(BundleTest, DefaultBundle)
{
  BundlePtr bundle = Bundle::create();
  EXPECT_TRUE(bundle);
  EXPECT_TRUE(bundle->is_empty());
}

TEST_F(BundleTest, AddItems)
{
  BundlePtr bundle_ptr = Bundle::create();
  Bundle &bundle = const_cast<Bundle &>(*bundle_ptr);
  bundle.add("a", 3);
  EXPECT_EQ(bundle.size(), 1);
  EXPECT_TRUE(bundle.contains("a"));
  EXPECT_EQ(bundle.lookup<int>("a"), 3);
}

TEST_F(BundleTest, AddLookupPath)
{
  BundlePtr bundle_ptr = Bundle::create();
  Bundle &bundle = const_cast<Bundle &>(*bundle_ptr);
  bundle.add_path("a/b/c", 3);
  bundle.add_path("a/b/d", 4);
  EXPECT_EQ(bundle.size(), 1);
  EXPECT_EQ((*bundle.lookup_path<BundlePtr>("a"))->size(), 1);
  EXPECT_EQ((*bundle.lookup_path<BundlePtr>("a/b"))->size(), 2);
  EXPECT_EQ(bundle.lookup_path<int>("a/b/c"), 3);
  EXPECT_EQ(bundle.lookup_path<int>("a/b/d"), 4);
  EXPECT_EQ(bundle.lookup_path<BundlePtr>("a/b/c"), std::nullopt);
  EXPECT_EQ(bundle.lookup_path<BundlePtr>("a/b/x"), std::nullopt);
}

TEST_F(BundleTest, LookupConversion)
{
  BundlePtr bundle_ptr = Bundle::create();
  Bundle &bundle = const_cast<Bundle &>(*bundle_ptr);
  bundle.add_path("a/b", -3.4f);
  EXPECT_EQ(bundle.lookup_path<float>("a/b"), -3.4f);
  EXPECT_EQ(bundle.lookup_path<int>("a/b"), -3);
  EXPECT_EQ(bundle.lookup_path<bool>("a/b"), false);
  EXPECT_EQ(bundle.lookup_path<float3>("a/b"), float3(-3.4f));
  EXPECT_EQ(bundle.lookup_path<std::string>("a/b"), std::nullopt);
}

TEST_F(BundleTest, AddOverride)
{
  BundlePtr bundle_ptr = Bundle::create();
  Bundle &bundle = const_cast<Bundle &>(*bundle_ptr);
  bundle.add_path("a/b", 4);
  EXPECT_EQ(bundle.lookup_path<int>("a/b"), 4);
  bundle.add_path_override("a/b", 10);
  EXPECT_EQ(bundle.lookup_path<int>("a/b"), 10);
  bundle.add_path("a/b", 15);
  EXPECT_EQ(bundle.lookup_path<int>("a/b"), 10);
}

}  // namespace blender::nodes::tests
