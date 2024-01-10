/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "AS_asset_catalog.hh"
#include "AS_asset_catalog_tree.hh"

#include "BLI_path_util.h"

#include "testing/testing.h"

#include "asset_library_test_common.hh"

namespace blender::asset_system::tests {

class AssetCatalogTreeTest : public AssetLibraryTestBase, public AssetCatalogTreeTestFunctions {};

TEST_F(AssetCatalogTreeTest, insert_item_into_tree)
{
  {
    AssetCatalogTree tree;
    std::unique_ptr<AssetCatalog> catalog_empty_path = AssetCatalog::from_path("");
    tree.insert_item(*catalog_empty_path);

    expect_tree_items(&tree, {});
  }

  {
    AssetCatalogTree tree;

    std::unique_ptr<AssetCatalog> catalog = AssetCatalog::from_path("item");
    tree.insert_item(*catalog);
    expect_tree_items(&tree, {"item"});

    /* Insert child after parent already exists. */
    std::unique_ptr<AssetCatalog> child_catalog = AssetCatalog::from_path("item/child");
    tree.insert_item(*catalog);
    expect_tree_items(&tree, {"item", "item/child"});

    std::vector<AssetCatalogPath> expected_paths;

    /* Test inserting multi-component sub-path. */
    std::unique_ptr<AssetCatalog> grandgrandchild_catalog = AssetCatalog::from_path(
        "item/child/grandchild/grandgrandchild");
    tree.insert_item(*catalog);
    expected_paths = {
        "item", "item/child", "item/child/grandchild", "item/child/grandchild/grandgrandchild"};
    expect_tree_items(&tree, expected_paths);

    std::unique_ptr<AssetCatalog> root_level_catalog = AssetCatalog::from_path("root level");
    tree.insert_item(*catalog);
    expected_paths = {"item",
                      "item/child",
                      "item/child/grandchild",
                      "item/child/grandchild/grandgrandchild",
                      "root level"};
    expect_tree_items(&tree, expected_paths);
  }

  {
    AssetCatalogTree tree;

    std::unique_ptr<AssetCatalog> catalog = AssetCatalog::from_path("item/child");
    tree.insert_item(*catalog);
    expect_tree_items(&tree, {"item", "item/child"});
  }

  {
    AssetCatalogTree tree;

    std::unique_ptr<AssetCatalog> catalog = AssetCatalog::from_path("white space");
    tree.insert_item(*catalog);
    expect_tree_items(&tree, {"white space"});
  }

  {
    AssetCatalogTree tree;

    std::unique_ptr<AssetCatalog> catalog = AssetCatalog::from_path("/item/white space");
    tree.insert_item(*catalog);
    expect_tree_items(&tree, {"item", "item/white space"});
  }

  {
    AssetCatalogTree tree;

    std::unique_ptr<AssetCatalog> catalog_unicode_path = AssetCatalog::from_path("Ružena");
    tree.insert_item(*catalog_unicode_path);
    expect_tree_items(&tree, {"Ružena"});

    catalog_unicode_path = AssetCatalog::from_path("Ružena/Ružena");
    tree.insert_item(*catalog_unicode_path);
    expect_tree_items(&tree, {"Ružena", "Ružena/Ružena"});
  }
}

TEST_F(AssetCatalogTreeTest, load_single_file_into_tree)
{
  AssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + SEP_STR + "blender_assets.cats.txt");

  /* Contains not only paths from the CDF but also the missing parents (implicitly defined
   * catalogs). */
  std::vector<AssetCatalogPath> expected_paths{
      "character",
      "character/Ellie",
      "character/Ellie/backslashes",
      "character/Ellie/poselib",
      "character/Ellie/poselib/tailslash",
      "character/Ellie/poselib/white space",
      "character/Ružena",
      "character/Ružena/poselib",
      "character/Ružena/poselib/face",
      "character/Ružena/poselib/hand",
      "path",                    /* Implicit. */
      "path/without",            /* Implicit. */
      "path/without/simplename", /* From CDF. */
  };

  AssetCatalogTree *tree = service.get_catalog_tree();
  expect_tree_items(tree, expected_paths);
}

TEST_F(AssetCatalogTreeTest, foreach_in_tree)
{
  {
    AssetCatalogTree tree{};
    const std::vector<AssetCatalogPath> no_catalogs{};

    expect_tree_items(&tree, no_catalogs);
    expect_tree_root_items(&tree, no_catalogs);
    /* Need a root item to check child items. */
    std::unique_ptr<AssetCatalog> catalog = AssetCatalog::from_path("something");
    tree.insert_item(*catalog);
    tree.foreach_root_item([&no_catalogs](AssetCatalogTreeItem &item) {
      expect_tree_item_child_items(&item, no_catalogs);
    });
  }

  AssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + SEP_STR + "blender_assets.cats.txt");

  std::vector<AssetCatalogPath> expected_root_items{{"character", "path"}};
  AssetCatalogTree *tree = service.get_catalog_tree();
  expect_tree_root_items(tree, expected_root_items);

  /* Test if the direct children of the root item are what's expected. */
  std::vector<std::vector<AssetCatalogPath>> expected_root_child_items = {
      /* Children of the "character" root item. */
      {"character/Ellie", "character/Ružena"},
      /* Children of the "path" root item. */
      {"path/without"},
  };
  int i = 0;
  tree->foreach_root_item([&expected_root_child_items, &i](AssetCatalogTreeItem &item) {
    expect_tree_item_child_items(&item, expected_root_child_items[i]);
    i++;
  });
}

}  // namespace blender::asset_system::tests
