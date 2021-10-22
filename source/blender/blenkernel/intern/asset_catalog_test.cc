/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation
 * All rights reserved.
 */

#include "BKE_appdir.h"
#include "BKE_asset_catalog.hh"
#include "BKE_preferences.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"

#include "DNA_userdef_types.h"

#include "testing/testing.h"

namespace blender::bke::tests {

/* UUIDs from lib/tests/asset_library/blender_assets.cats.txt */
const bUUID UUID_ID_WITHOUT_PATH("e34dd2c5-5d2e-4668-9794-1db5de2a4f71");
const bUUID UUID_POSES_ELLIE("df60e1f6-2259-475b-93d9-69a1b4a8db78");
const bUUID UUID_POSES_ELLIE_WHITESPACE("b06132f6-5687-4751-a6dd-392740eb3c46");
const bUUID UUID_POSES_ELLIE_TRAILING_SLASH("3376b94b-a28d-4d05-86c1-bf30b937130d");
const bUUID UUID_POSES_RUZENA("79a4f887-ab60-4bd4-94da-d572e27d6aed");
const bUUID UUID_POSES_RUZENA_HAND("81811c31-1a88-4bd7-bb34-c6fc2607a12e");
const bUUID UUID_POSES_RUZENA_FACE("82162c1f-06cc-4d91-a9bf-4f72c104e348");
const bUUID UUID_WITHOUT_SIMPLENAME("d7916a31-6ca9-4909-955f-182ca2b81fa3");
const bUUID UUID_ANOTHER_RUZENA("00000000-d9fa-4b91-b704-e6af1f1339ef");

/* UUIDs from lib/tests/asset_library/modified_assets.cats.txt */
const bUUID UUID_AGENT_47("c5744ba5-43f5-4f73-8e52-010ad4a61b34");

/* Subclass that adds accessors such that protected fields can be used in tests. */
class TestableAssetCatalogService : public AssetCatalogService {
 public:
  TestableAssetCatalogService() = default;

  explicit TestableAssetCatalogService(const CatalogFilePath &asset_library_root)
      : AssetCatalogService(asset_library_root)
  {
  }

  AssetCatalogDefinitionFile *get_catalog_definition_file()
  {
    return AssetCatalogService::get_catalog_definition_file();
  }

  void create_missing_catalogs()
  {
    AssetCatalogService::create_missing_catalogs();
  }

  int64_t count_catalogs_with_path(const CatalogFilePath &path)
  {
    int64_t count = 0;
    for (auto &catalog_uptr : get_catalogs().values()) {
      if (catalog_uptr->path == path) {
        count++;
      }
    }
    return count;
  }
};

class AssetCatalogTest : public testing::Test {
 protected:
  CatalogFilePath asset_library_root_;
  CatalogFilePath temp_library_path_;

  void SetUp() override
  {
    const std::string test_files_dir = blender::tests::flags_test_asset_dir();
    if (test_files_dir.empty()) {
      FAIL();
    }

    asset_library_root_ = test_files_dir + "/" + "asset_library";
    temp_library_path_ = "";
  }

  void TearDown() override
  {
    if (!temp_library_path_.empty()) {
      BLI_delete(temp_library_path_.c_str(), true, true);
      temp_library_path_ = "";
    }
  }

  /* Register a temporary path, which will be removed at the end of the test.
   * The returned path ends in a slash. */
  CatalogFilePath use_temp_path()
  {
    BKE_tempdir_init("");
    const CatalogFilePath tempdir = BKE_tempdir_session();
    temp_library_path_ = tempdir + "test-temporary-path/";
    return temp_library_path_;
  }

  CatalogFilePath create_temp_path()
  {
    CatalogFilePath path = use_temp_path();
    BLI_dir_create_recursive(path.c_str());
    return path;
  }

  void assert_expected_item(const AssetCatalogPath &expected_path,
                            const AssetCatalogTreeItem &actual_item)
  {
    if (expected_path != actual_item.catalog_path().str()) {
      /* This will fail, but with a nicer error message than just calling FAIL(). */
      EXPECT_EQ(expected_path, actual_item.catalog_path());
      return;
    }

    /* Is the catalog name as expected? "character", "Ellie", ... */
    EXPECT_EQ(expected_path.name(), actual_item.get_name());

    /* Does the computed number of parents match? */
    const std::string expected_path_str = expected_path.str();
    const size_t expected_parent_count = std::count(
        expected_path_str.begin(), expected_path_str.end(), AssetCatalogPath::SEPARATOR);
    EXPECT_EQ(expected_parent_count, actual_item.count_parents());
  }

  /**
   * Recursively iterate over all tree items using #AssetCatalogTree::foreach_item() and check if
   * the items map exactly to \a expected_paths.
   */
  void assert_expected_tree_items(AssetCatalogTree *tree,
                                  const std::vector<AssetCatalogPath> &expected_paths)
  {
    int i = 0;
    tree->foreach_item([&](const AssetCatalogTreeItem &actual_item) {
      ASSERT_LT(i, expected_paths.size())
          << "More catalogs in tree than expected; did not expect " << actual_item.catalog_path();
      assert_expected_item(expected_paths[i], actual_item);
      i++;
    });
  }

  /**
   * Iterate over the root items of \a tree and check if the items map exactly to \a
   * expected_paths. Similar to #assert_expected_tree_items() but calls
   * #AssetCatalogTree::foreach_root_item() instead of #AssetCatalogTree::foreach_item().
   */
  void assert_expected_tree_root_items(AssetCatalogTree *tree,
                                       const std::vector<AssetCatalogPath> &expected_paths)
  {
    int i = 0;
    tree->foreach_root_item([&](const AssetCatalogTreeItem &actual_item) {
      ASSERT_LT(i, expected_paths.size())
          << "More catalogs in tree root than expected; did not expect "
          << actual_item.catalog_path();
      assert_expected_item(expected_paths[i], actual_item);
      i++;
    });
  }

  /**
   * Iterate over the child items of \a parent_item and check if the items map exactly to \a
   * expected_paths. Similar to #assert_expected_tree_items() but calls
   * #AssetCatalogTreeItem::foreach_child() instead of #AssetCatalogTree::foreach_item().
   */
  void assert_expected_tree_item_child_items(AssetCatalogTreeItem *parent_item,
                                             const std::vector<AssetCatalogPath> &expected_paths)
  {
    int i = 0;
    parent_item->foreach_child([&](const AssetCatalogTreeItem &actual_item) {
      ASSERT_LT(i, expected_paths.size())
          << "More catalogs in tree item than expected; did not expect "
          << actual_item.catalog_path();
      assert_expected_item(expected_paths[i], actual_item);
      i++;
    });
  }

  /* Used by on_blendfile_save__from_memory_into_existing_asset_lib* test functions. */
  void save_from_memory_into_existing_asset_lib(const bool should_top_level_cdf_exist)
  {
    const CatalogFilePath target_dir = create_temp_path(); /* Has trailing slash. */
    const CatalogFilePath original_cdf_file = asset_library_root_ + "/blender_assets.cats.txt";
    const CatalogFilePath registered_asset_lib = target_dir + "my_asset_library/";
    const CatalogFilePath asset_lib_subdir = registered_asset_lib + "subdir/";
    CatalogFilePath cdf_toplevel = registered_asset_lib +
                                   AssetCatalogService::DEFAULT_CATALOG_FILENAME;
    CatalogFilePath cdf_in_subdir = asset_lib_subdir +
                                    AssetCatalogService::DEFAULT_CATALOG_FILENAME;
    BLI_path_slash_native(cdf_toplevel.data());
    BLI_path_slash_native(cdf_in_subdir.data());

    /* Set up a temporary asset library for testing. */
    bUserAssetLibrary *asset_lib_pref = BKE_preferences_asset_library_add(
        &U, "Test", registered_asset_lib.c_str());
    ASSERT_NE(nullptr, asset_lib_pref);
    ASSERT_TRUE(BLI_dir_create_recursive(asset_lib_subdir.c_str()));

    if (should_top_level_cdf_exist) {
      ASSERT_EQ(0, BLI_copy(original_cdf_file.c_str(), cdf_toplevel.c_str()));
    }

    /* Create an empty CDF to add complexity. It should not save to this, but to the top-level
     * one.*/
    ASSERT_TRUE(BLI_file_touch(cdf_in_subdir.c_str()));
    ASSERT_EQ(0, BLI_file_size(cdf_in_subdir.c_str()));

    /* Create the catalog service without loading the already-existing CDF. */
    TestableAssetCatalogService service;
    const CatalogFilePath blendfilename = asset_lib_subdir + "some_file.blend";
    const AssetCatalog *cat = service.create_catalog("some/catalog/path");

    /* Mock that the blend file is written to the directory already containing a CDF. */
    ASSERT_TRUE(service.write_to_disk(blendfilename));

    /* Test that the CDF still exists in the expected location. */
    EXPECT_TRUE(BLI_exists(cdf_toplevel.c_str()));
    const CatalogFilePath backup_filename = cdf_toplevel + "~";
    const bool backup_exists = BLI_exists(backup_filename.c_str());
    EXPECT_EQ(should_top_level_cdf_exist, backup_exists)
        << "Overwritten CDF should have been backed up.";

    /* Test that the in-memory CDF has the expected file path. */
    AssetCatalogDefinitionFile *cdf = service.get_catalog_definition_file();
    BLI_path_slash_native(cdf->file_path.data());
    EXPECT_EQ(cdf_toplevel, cdf->file_path);

    /* Test that the in-memory catalogs have been merged with the on-disk one. */
    AssetCatalogService loaded_service(cdf_toplevel);
    loaded_service.load_from_disk();
    EXPECT_NE(nullptr, loaded_service.find_catalog(cat->catalog_id));

    /* This catalog comes from a pre-existing CDF that should have been merged.
     * However, if the file doesn't exist, so does the catalog. */
    AssetCatalog *poses_ellie_catalog = loaded_service.find_catalog(UUID_POSES_ELLIE);
    if (should_top_level_cdf_exist) {
      EXPECT_NE(nullptr, poses_ellie_catalog);
    }
    else {
      EXPECT_EQ(nullptr, poses_ellie_catalog);
    }

    /* Test that the "red herring" CDF has not been touched. */
    EXPECT_EQ(0, BLI_file_size(cdf_in_subdir.c_str()));

    BKE_preferences_asset_library_remove(&U, asset_lib_pref);
  }
};

TEST_F(AssetCatalogTest, load_single_file)
{
  AssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + "/" + "blender_assets.cats.txt");

  /* Test getting a non-existent catalog ID. */
  EXPECT_EQ(nullptr, service.find_catalog(BLI_uuid_generate_random()));

  /* Test getting an invalid catalog (without path definition). */
  AssetCatalog *cat_without_path = service.find_catalog(UUID_ID_WITHOUT_PATH);
  ASSERT_EQ(nullptr, cat_without_path);

  /* Test getting a regular catalog. */
  AssetCatalog *poses_ellie = service.find_catalog(UUID_POSES_ELLIE);
  ASSERT_NE(nullptr, poses_ellie);
  EXPECT_EQ(UUID_POSES_ELLIE, poses_ellie->catalog_id);
  EXPECT_EQ("character/Ellie/poselib", poses_ellie->path.str());
  EXPECT_EQ("POSES_ELLIE", poses_ellie->simple_name);

  /* Test white-space stripping and support in the path. */
  AssetCatalog *poses_whitespace = service.find_catalog(UUID_POSES_ELLIE_WHITESPACE);
  ASSERT_NE(nullptr, poses_whitespace);
  EXPECT_EQ(UUID_POSES_ELLIE_WHITESPACE, poses_whitespace->catalog_id);
  EXPECT_EQ("character/Ellie/poselib/white space", poses_whitespace->path.str());
  EXPECT_EQ("POSES_ELLIE WHITESPACE", poses_whitespace->simple_name);

  /* Test getting a UTF-8 catalog ID. */
  AssetCatalog *poses_ruzena = service.find_catalog(UUID_POSES_RUZENA);
  ASSERT_NE(nullptr, poses_ruzena);
  EXPECT_EQ(UUID_POSES_RUZENA, poses_ruzena->catalog_id);
  EXPECT_EQ("character/Ružena/poselib", poses_ruzena->path.str());
  EXPECT_EQ("POSES_RUŽENA", poses_ruzena->simple_name);

  /* Test getting a catalog that aliases an earlier-defined catalog. */
  AssetCatalog *another_ruzena = service.find_catalog(UUID_ANOTHER_RUZENA);
  ASSERT_NE(nullptr, another_ruzena);
  EXPECT_EQ(UUID_ANOTHER_RUZENA, another_ruzena->catalog_id);
  EXPECT_EQ("character/Ružena/poselib", another_ruzena->path.str());
  EXPECT_EQ("Another Ružena", another_ruzena->simple_name);
}

TEST_F(AssetCatalogTest, load_catalog_path_backslashes)
{
  AssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + "/" + "blender_assets.cats.txt");

  const bUUID ELLIE_BACKSLASHES_UUID("a51e17ae-34fc-47d5-ba0f-64c2c9b771f7");
  const AssetCatalog *found_by_id = service.find_catalog(ELLIE_BACKSLASHES_UUID);
  ASSERT_NE(nullptr, found_by_id);
  EXPECT_EQ(AssetCatalogPath("character/Ellie/backslashes"), found_by_id->path)
      << "Backslashes should be normalised when loading from disk.";
  EXPECT_EQ(StringRefNull("Windows For Life!"), found_by_id->simple_name);

  const AssetCatalog *found_by_path = service.find_catalog_by_path("character/Ellie/backslashes");
  EXPECT_EQ(found_by_id, found_by_path)
      << "Catalog with backslashed path should be findable by the normalized path.";

  EXPECT_EQ(nullptr, service.find_catalog_by_path("character\\Ellie\\backslashes"))
      << "Nothing should be found when searching for backslashes.";
}

TEST_F(AssetCatalogTest, is_first_loaded_flag)
{
  AssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + "/" + "blender_assets.cats.txt");

  AssetCatalog *new_cat = service.create_catalog("never/before/seen/path");
  EXPECT_FALSE(new_cat->flags.is_first_loaded)
      << "Adding a catalog at runtime should never mark it as 'first loaded'; "
         "only loading from disk is allowed to do that.";

  AssetCatalog *alias_cat = service.create_catalog("character/Ružena/poselib");
  EXPECT_FALSE(alias_cat->flags.is_first_loaded)
      << "Adding a new catalog with an already-loaded path should not mark it as 'first loaded'";

  EXPECT_TRUE(service.find_catalog(UUID_POSES_ELLIE)->flags.is_first_loaded);
  EXPECT_TRUE(service.find_catalog(UUID_POSES_ELLIE_WHITESPACE)->flags.is_first_loaded);
  EXPECT_TRUE(service.find_catalog(UUID_POSES_RUZENA)->flags.is_first_loaded);
  EXPECT_FALSE(service.find_catalog(UUID_ANOTHER_RUZENA)->flags.is_first_loaded);

  AssetCatalog *ruzena = service.find_catalog_by_path("character/Ružena/poselib");
  EXPECT_EQ(UUID_POSES_RUZENA, ruzena->catalog_id)
      << "The first-seen definition of a catalog should be returned";
}

TEST_F(AssetCatalogTest, insert_item_into_tree)
{
  {
    AssetCatalogTree tree;
    std::unique_ptr<AssetCatalog> catalog_empty_path = AssetCatalog::from_path("");
    tree.insert_item(*catalog_empty_path);

    assert_expected_tree_items(&tree, {});
  }

  {
    AssetCatalogTree tree;

    std::unique_ptr<AssetCatalog> catalog = AssetCatalog::from_path("item");
    tree.insert_item(*catalog);
    assert_expected_tree_items(&tree, {"item"});

    /* Insert child after parent already exists. */
    std::unique_ptr<AssetCatalog> child_catalog = AssetCatalog::from_path("item/child");
    tree.insert_item(*catalog);
    assert_expected_tree_items(&tree, {"item", "item/child"});

    std::vector<AssetCatalogPath> expected_paths;

    /* Test inserting multi-component sub-path. */
    std::unique_ptr<AssetCatalog> grandgrandchild_catalog = AssetCatalog::from_path(
        "item/child/grandchild/grandgrandchild");
    tree.insert_item(*catalog);
    expected_paths = {
        "item", "item/child", "item/child/grandchild", "item/child/grandchild/grandgrandchild"};
    assert_expected_tree_items(&tree, expected_paths);

    std::unique_ptr<AssetCatalog> root_level_catalog = AssetCatalog::from_path("root level");
    tree.insert_item(*catalog);
    expected_paths = {"item",
                      "item/child",
                      "item/child/grandchild",
                      "item/child/grandchild/grandgrandchild",
                      "root level"};
    assert_expected_tree_items(&tree, expected_paths);
  }

  {
    AssetCatalogTree tree;

    std::unique_ptr<AssetCatalog> catalog = AssetCatalog::from_path("item/child");
    tree.insert_item(*catalog);
    assert_expected_tree_items(&tree, {"item", "item/child"});
  }

  {
    AssetCatalogTree tree;

    std::unique_ptr<AssetCatalog> catalog = AssetCatalog::from_path("white space");
    tree.insert_item(*catalog);
    assert_expected_tree_items(&tree, {"white space"});
  }

  {
    AssetCatalogTree tree;

    std::unique_ptr<AssetCatalog> catalog = AssetCatalog::from_path("/item/white space");
    tree.insert_item(*catalog);
    assert_expected_tree_items(&tree, {"item", "item/white space"});
  }

  {
    AssetCatalogTree tree;

    std::unique_ptr<AssetCatalog> catalog_unicode_path = AssetCatalog::from_path("Ružena");
    tree.insert_item(*catalog_unicode_path);
    assert_expected_tree_items(&tree, {"Ružena"});

    catalog_unicode_path = AssetCatalog::from_path("Ružena/Ružena");
    tree.insert_item(*catalog_unicode_path);
    assert_expected_tree_items(&tree, {"Ružena", "Ružena/Ružena"});
  }
}

TEST_F(AssetCatalogTest, load_single_file_into_tree)
{
  AssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + "/" + "blender_assets.cats.txt");

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
  assert_expected_tree_items(tree, expected_paths);
}

TEST_F(AssetCatalogTest, foreach_in_tree)
{
  {
    AssetCatalogTree tree{};
    const std::vector<AssetCatalogPath> no_catalogs{};

    assert_expected_tree_items(&tree, no_catalogs);
    assert_expected_tree_root_items(&tree, no_catalogs);
    /* Need a root item to check child items. */
    std::unique_ptr<AssetCatalog> catalog = AssetCatalog::from_path("something");
    tree.insert_item(*catalog);
    tree.foreach_root_item([&no_catalogs, this](AssetCatalogTreeItem &item) {
      assert_expected_tree_item_child_items(&item, no_catalogs);
    });
  }

  AssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + "/" + "blender_assets.cats.txt");

  std::vector<AssetCatalogPath> expected_root_items{{"character", "path"}};
  AssetCatalogTree *tree = service.get_catalog_tree();
  assert_expected_tree_root_items(tree, expected_root_items);

  /* Test if the direct children of the root item are what's expected. */
  std::vector<std::vector<AssetCatalogPath>> expected_root_child_items = {
      /* Children of the "character" root item. */
      {"character/Ellie", "character/Ružena"},
      /* Children of the "path" root item. */
      {"path/without"},
  };
  int i = 0;
  tree->foreach_root_item([&expected_root_child_items, &i, this](AssetCatalogTreeItem &item) {
    assert_expected_tree_item_child_items(&item, expected_root_child_items[i]);
    i++;
  });
}

TEST_F(AssetCatalogTest, find_catalog_by_path)
{
  TestableAssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + "/" +
                         AssetCatalogService::DEFAULT_CATALOG_FILENAME);

  AssetCatalog *catalog;

  EXPECT_EQ(nullptr, service.find_catalog_by_path(""));
  catalog = service.find_catalog_by_path("character/Ellie/poselib/white space");
  EXPECT_NE(nullptr, catalog);
  EXPECT_EQ(UUID_POSES_ELLIE_WHITESPACE, catalog->catalog_id);
  catalog = service.find_catalog_by_path("character/Ružena/poselib");
  EXPECT_NE(nullptr, catalog);
  EXPECT_EQ(UUID_POSES_RUZENA, catalog->catalog_id);

  /* "character/Ellie/poselib" is used by two catalogs. Check if it's using the first one. */
  catalog = service.find_catalog_by_path("character/Ellie/poselib");
  EXPECT_NE(nullptr, catalog);
  EXPECT_EQ(UUID_POSES_ELLIE, catalog->catalog_id);
  EXPECT_NE(UUID_POSES_ELLIE_TRAILING_SLASH, catalog->catalog_id);
}

TEST_F(AssetCatalogTest, write_single_file)
{
  TestableAssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + "/" +
                         AssetCatalogService::DEFAULT_CATALOG_FILENAME);

  const CatalogFilePath save_to_path = use_temp_path() +
                                       AssetCatalogService::DEFAULT_CATALOG_FILENAME;
  AssetCatalogDefinitionFile *cdf = service.get_catalog_definition_file();
  cdf->write_to_disk(save_to_path);

  AssetCatalogService loaded_service(save_to_path);
  loaded_service.load_from_disk();

  /* Test that the expected catalogs are there. */
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE_WHITESPACE));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE_TRAILING_SLASH));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA_HAND));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA_FACE));

  /* Test that the invalid catalog definition wasn't copied. */
  EXPECT_EQ(nullptr, loaded_service.find_catalog(UUID_ID_WITHOUT_PATH));

  /* TODO(@sybren): test ordering of catalogs in the file. */
}

TEST_F(AssetCatalogTest, no_writing_empty_files)
{
  const CatalogFilePath temp_lib_root = create_temp_path();
  AssetCatalogService service(temp_lib_root);
  service.write_to_disk(temp_lib_root + "phony.blend");

  const CatalogFilePath default_cdf_path = temp_lib_root +
                                           AssetCatalogService::DEFAULT_CATALOG_FILENAME;
  EXPECT_FALSE(BLI_exists(default_cdf_path.c_str()));
}

/* Already loaded a CDF, saving to some unrelated directory. */
TEST_F(AssetCatalogTest, on_blendfile_save__with_existing_cdf)
{
  const CatalogFilePath top_level_dir = create_temp_path(); /* Has trailing slash. */

  /* Create a copy of the CDF in SVN, so we can safely write to it. */
  const CatalogFilePath original_cdf_file = asset_library_root_ + "/blender_assets.cats.txt";
  const CatalogFilePath cdf_dirname = top_level_dir + "other_dir/";
  const CatalogFilePath cdf_filename = cdf_dirname + AssetCatalogService::DEFAULT_CATALOG_FILENAME;
  ASSERT_TRUE(BLI_dir_create_recursive(cdf_dirname.c_str()));
  ASSERT_EQ(0, BLI_copy(original_cdf_file.c_str(), cdf_filename.c_str()))
      << "Unable to copy " << original_cdf_file << " to " << cdf_filename;

  /* Load the CDF, add a catalog, and trigger a write. This should write to the loaded CDF. */
  TestableAssetCatalogService service(cdf_filename);
  service.load_from_disk();
  const AssetCatalog *cat = service.create_catalog("some/catalog/path");

  const CatalogFilePath blendfilename = top_level_dir + "subdir/some_file.blend";
  ASSERT_TRUE(service.write_to_disk(blendfilename));
  EXPECT_EQ(cdf_filename, service.get_catalog_definition_file()->file_path);

  /* Test that the CDF was created in the expected location. */
  const CatalogFilePath backup_filename = cdf_filename + "~";
  EXPECT_TRUE(BLI_exists(cdf_filename.c_str()));
  EXPECT_TRUE(BLI_exists(backup_filename.c_str()))
      << "Overwritten CDF should have been backed up.";

  /* Test that the on-disk CDF contains the expected catalogs. */
  AssetCatalogService loaded_service(cdf_filename);
  loaded_service.load_from_disk();
  EXPECT_NE(nullptr, loaded_service.find_catalog(cat->catalog_id))
      << "Expected to see the newly-created catalog.";
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE))
      << "Expected to see the already-existing catalog.";
}

/* Create some catalogs in memory, save to directory that doesn't contain anything else. */
TEST_F(AssetCatalogTest, on_blendfile_save__from_memory_into_empty_directory)
{
  const CatalogFilePath target_dir = create_temp_path(); /* Has trailing slash. */

  TestableAssetCatalogService service;
  const AssetCatalog *cat = service.create_catalog("some/catalog/path");

  const CatalogFilePath blendfilename = target_dir + "some_file.blend";
  ASSERT_TRUE(service.write_to_disk(blendfilename));

  /* Test that the CDF was created in the expected location. */
  const CatalogFilePath expected_cdf_path = target_dir +
                                            AssetCatalogService::DEFAULT_CATALOG_FILENAME;
  EXPECT_TRUE(BLI_exists(expected_cdf_path.c_str()));

  /* Test that the in-memory CDF has been created, and contains the expected catalog. */
  AssetCatalogDefinitionFile *cdf = service.get_catalog_definition_file();
  ASSERT_NE(nullptr, cdf);
  EXPECT_TRUE(cdf->contains(cat->catalog_id));

  /* Test that the on-disk CDF contains the expected catalog. */
  AssetCatalogService loaded_service(expected_cdf_path);
  loaded_service.load_from_disk();
  EXPECT_NE(nullptr, loaded_service.find_catalog(cat->catalog_id));
}

/* Create some catalogs in memory, save to directory that contains a default CDF. */
TEST_F(AssetCatalogTest, on_blendfile_save__from_memory_into_existing_cdf_and_merge)
{
  const CatalogFilePath target_dir = create_temp_path(); /* Has trailing slash. */
  const CatalogFilePath original_cdf_file = asset_library_root_ + "/blender_assets.cats.txt";
  CatalogFilePath writable_cdf_file = target_dir + AssetCatalogService::DEFAULT_CATALOG_FILENAME;
  BLI_path_slash_native(writable_cdf_file.data());
  ASSERT_EQ(0, BLI_copy(original_cdf_file.c_str(), writable_cdf_file.c_str()));

  /* Create the catalog service without loading the already-existing CDF. */
  TestableAssetCatalogService service;
  const AssetCatalog *cat = service.create_catalog("some/catalog/path");

  /* Mock that the blend file is written to a subdirectory of the asset library. */
  const CatalogFilePath blendfilename = target_dir + "some_file.blend";
  ASSERT_TRUE(service.write_to_disk(blendfilename));

  /* Test that the CDF still exists in the expected location. */
  const CatalogFilePath backup_filename = writable_cdf_file + "~";
  EXPECT_TRUE(BLI_exists(writable_cdf_file.c_str()));
  EXPECT_TRUE(BLI_exists(backup_filename.c_str()))
      << "Overwritten CDF should have been backed up.";

  /* Test that the in-memory CDF has the expected file path. */
  AssetCatalogDefinitionFile *cdf = service.get_catalog_definition_file();
  ASSERT_NE(nullptr, cdf);
  EXPECT_EQ(writable_cdf_file, cdf->file_path);

  /* Test that the in-memory catalogs have been merged with the on-disk one. */
  AssetCatalogService loaded_service(writable_cdf_file);
  loaded_service.load_from_disk();
  EXPECT_NE(nullptr, loaded_service.find_catalog(cat->catalog_id));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE));
}

/* Create some catalogs in memory, save to subdirectory of a registered asset library, where the
 * subdirectory also contains a CDF. This should still write to the top-level dir of the asset
 * library. */
TEST_F(AssetCatalogTest,
       on_blendfile_save__from_memory_into_existing_asset_lib_without_top_level_cdf)
{
  save_from_memory_into_existing_asset_lib(true);
}

/* Create some catalogs in memory, save to subdirectory of a registered asset library, where the
 * subdirectory contains a CDF, but the top-level directory does not. This should still write to
 * the top-level dir of the asset library. */
TEST_F(AssetCatalogTest, on_blendfile_save__from_memory_into_existing_asset_lib)
{
  save_from_memory_into_existing_asset_lib(false);
}

TEST_F(AssetCatalogTest, create_first_catalog_from_scratch)
{
  /* Even from scratch a root directory should be known. */
  const CatalogFilePath temp_lib_root = use_temp_path();
  AssetCatalogService service;

  /* Just creating the service should NOT create the path. */
  EXPECT_FALSE(BLI_exists(temp_lib_root.c_str()));

  AssetCatalog *cat = service.create_catalog("some/catalog/path");
  ASSERT_NE(nullptr, cat);
  EXPECT_EQ(cat->path, "some/catalog/path");
  EXPECT_EQ(cat->simple_name, "some-catalog-path");

  /* Creating a new catalog should not save anything to disk yet. */
  EXPECT_FALSE(BLI_exists(temp_lib_root.c_str()));

  /* Writing to disk should create the directory + the default file. */
  service.write_to_disk(temp_lib_root + "phony.blend");
  EXPECT_TRUE(BLI_is_dir(temp_lib_root.c_str()));

  const CatalogFilePath definition_file_path = temp_lib_root + "/" +
                                               AssetCatalogService::DEFAULT_CATALOG_FILENAME;
  EXPECT_TRUE(BLI_is_file(definition_file_path.c_str()));

  AssetCatalogService loaded_service(temp_lib_root);
  loaded_service.load_from_disk();

  /* Test that the expected catalog is there. */
  AssetCatalog *written_cat = loaded_service.find_catalog(cat->catalog_id);
  ASSERT_NE(nullptr, written_cat);
  EXPECT_EQ(written_cat->catalog_id, cat->catalog_id);
  EXPECT_EQ(written_cat->path, cat->path.str());
}

TEST_F(AssetCatalogTest, create_catalog_after_loading_file)
{
  const CatalogFilePath temp_lib_root = create_temp_path();

  /* Copy the asset catalog definition files to a separate location, so that we can test without
   * overwriting the test file in SVN. */
  const CatalogFilePath default_catalog_path = asset_library_root_ + "/" +
                                               AssetCatalogService::DEFAULT_CATALOG_FILENAME;
  const CatalogFilePath writable_catalog_path = temp_lib_root +
                                                AssetCatalogService::DEFAULT_CATALOG_FILENAME;
  ASSERT_EQ(0, BLI_copy(default_catalog_path.c_str(), writable_catalog_path.c_str()));
  EXPECT_TRUE(BLI_is_dir(temp_lib_root.c_str()));
  EXPECT_TRUE(BLI_is_file(writable_catalog_path.c_str()));

  TestableAssetCatalogService service(temp_lib_root);
  service.load_from_disk();
  EXPECT_EQ(writable_catalog_path, service.get_catalog_definition_file()->file_path);
  EXPECT_NE(nullptr, service.find_catalog(UUID_POSES_ELLIE)) << "expected catalogs to be loaded";

  /* This should create a new catalog but not write to disk. */
  const AssetCatalog *new_catalog = service.create_catalog("new/catalog");
  const bUUID new_catalog_id = new_catalog->catalog_id;

  /* Reload the on-disk catalog file. */
  TestableAssetCatalogService loaded_service(temp_lib_root);
  loaded_service.load_from_disk();
  EXPECT_EQ(writable_catalog_path, loaded_service.get_catalog_definition_file()->file_path);

  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE))
      << "expected pre-existing catalogs to be kept in the file";
  EXPECT_EQ(nullptr, loaded_service.find_catalog(new_catalog_id))
      << "expecting newly added catalog to not yet be saved to " << temp_lib_root;

  /* Write and reload the catalog file. */
  service.write_to_disk(temp_lib_root + "phony.blend");
  AssetCatalogService reloaded_service(temp_lib_root);
  reloaded_service.load_from_disk();
  EXPECT_NE(nullptr, reloaded_service.find_catalog(UUID_POSES_ELLIE))
      << "expected pre-existing catalogs to be kept in the file";
  EXPECT_NE(nullptr, reloaded_service.find_catalog(new_catalog_id))
      << "expecting newly added catalog to exist in the file";
}

TEST_F(AssetCatalogTest, create_catalog_path_cleanup)
{
  AssetCatalogService service;
  AssetCatalog *cat = service.create_catalog(" /some/path  /  ");

  EXPECT_FALSE(BLI_uuid_is_nil(cat->catalog_id));
  EXPECT_EQ("some/path", cat->path.str());
  EXPECT_EQ("some-path", cat->simple_name);
}

TEST_F(AssetCatalogTest, create_catalog_simple_name)
{
  AssetCatalogService service;
  AssetCatalog *cat = service.create_catalog(
      "production/Spite Fright/Characters/Victora/Pose Library/Approved/Body Parts/Hands");

  EXPECT_FALSE(BLI_uuid_is_nil(cat->catalog_id));
  EXPECT_EQ("production/Spite Fright/Characters/Victora/Pose Library/Approved/Body Parts/Hands",
            cat->path.str());
  EXPECT_EQ("...ht-Characters-Victora-Pose Library-Approved-Body Parts-Hands", cat->simple_name);
}

TEST_F(AssetCatalogTest, delete_catalog_leaf)
{
  AssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + "/" + "blender_assets.cats.txt");

  /* Delete a leaf catalog, i.e. one that is not a parent of another catalog.
   * This keeps this particular test easy. */
  service.prune_catalogs_by_id(UUID_POSES_RUZENA_HAND);
  EXPECT_EQ(nullptr, service.find_catalog(UUID_POSES_RUZENA_HAND));

  /* Contains not only paths from the CDF but also the missing parents (implicitly defined
   * catalogs). This is why a leaf catalog was deleted. */
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
      // "character/Ružena/poselib/hand", /* This is the deleted one. */
      "path",
      "path/without",
      "path/without/simplename",
  };

  AssetCatalogTree *tree = service.get_catalog_tree();
  assert_expected_tree_items(tree, expected_paths);
}

TEST_F(AssetCatalogTest, delete_catalog_parent_by_id)
{
  AssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + "/" + "blender_assets.cats.txt");

  /* Delete a parent catalog. */
  service.delete_catalog_by_id(UUID_POSES_RUZENA);

  /* The catalog should have been deleted, but its children should still be there. */
  EXPECT_EQ(nullptr, service.find_catalog(UUID_POSES_RUZENA));
  EXPECT_NE(nullptr, service.find_catalog(UUID_POSES_RUZENA_FACE));
  EXPECT_NE(nullptr, service.find_catalog(UUID_POSES_RUZENA_HAND));
}

TEST_F(AssetCatalogTest, delete_catalog_parent_by_path)
{
  AssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + "/" + "blender_assets.cats.txt");

  /* Create an extra catalog with the to-be-deleted path, and one with a child of that.
   * This creates some duplicates that are bound to occur in production asset libraries as well.
   */
  const bUUID cat1_uuid = service.create_catalog("character/Ružena/poselib")->catalog_id;
  const bUUID cat2_uuid = service.create_catalog("character/Ružena/poselib/body")->catalog_id;

  /* Delete a parent catalog. */
  service.prune_catalogs_by_path("character/Ružena/poselib");

  /* The catalogs and their children should have been deleted. */
  EXPECT_EQ(nullptr, service.find_catalog(UUID_POSES_RUZENA));
  EXPECT_EQ(nullptr, service.find_catalog(UUID_POSES_RUZENA_FACE));
  EXPECT_EQ(nullptr, service.find_catalog(UUID_POSES_RUZENA_HAND));
  EXPECT_EQ(nullptr, service.find_catalog(cat1_uuid));
  EXPECT_EQ(nullptr, service.find_catalog(cat2_uuid));

  /* Contains not only paths from the CDF but also the missing parents (implicitly defined
   * catalogs). This is why a leaf catalog was deleted. */
  std::vector<AssetCatalogPath> expected_paths{
      "character",
      "character/Ellie",
      "character/Ellie/backslashes",
      "character/Ellie/poselib",
      "character/Ellie/poselib/tailslash",
      "character/Ellie/poselib/white space",
      "character/Ružena",
      "path",
      "path/without",
      "path/without/simplename",
  };

  AssetCatalogTree *tree = service.get_catalog_tree();
  assert_expected_tree_items(tree, expected_paths);
}

TEST_F(AssetCatalogTest, delete_catalog_write_to_disk)
{
  TestableAssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + "/" +
                         AssetCatalogService::DEFAULT_CATALOG_FILENAME);

  service.delete_catalog_by_id(UUID_POSES_ELLIE);

  const CatalogFilePath save_to_path = use_temp_path();
  AssetCatalogDefinitionFile *cdf = service.get_catalog_definition_file();
  cdf->write_to_disk(save_to_path + "/" + AssetCatalogService::DEFAULT_CATALOG_FILENAME);

  AssetCatalogService loaded_service(save_to_path);
  loaded_service.load_from_disk();

  /* Test that the expected catalogs are there, except the deleted one. */
  EXPECT_EQ(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE_WHITESPACE));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE_TRAILING_SLASH));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA_HAND));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA_FACE));
}

TEST_F(AssetCatalogTest, update_catalog_path)
{
  AssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + "/" +
                         AssetCatalogService::DEFAULT_CATALOG_FILENAME);

  const AssetCatalog *orig_cat = service.find_catalog(UUID_POSES_RUZENA);
  const AssetCatalogPath orig_path = orig_cat->path;

  service.update_catalog_path(UUID_POSES_RUZENA, "charlib/Ružena");

  EXPECT_EQ(nullptr, service.find_catalog_by_path(orig_path))
      << "The original (pre-rename) path should not be associated with a catalog any more.";

  const AssetCatalog *renamed_cat = service.find_catalog(UUID_POSES_RUZENA);
  ASSERT_NE(nullptr, renamed_cat);
  ASSERT_EQ(orig_cat, renamed_cat) << "Changing the path should not reallocate the catalog.";
  EXPECT_EQ(orig_cat->simple_name, renamed_cat->simple_name)
      << "Changing the path should not change the simple name.";
  EXPECT_EQ(orig_cat->catalog_id, renamed_cat->catalog_id)
      << "Changing the path should not change the catalog ID.";

  EXPECT_EQ("charlib/Ružena", renamed_cat->path.str())
      << "Changing the path should change the path. Surprise.";

  EXPECT_EQ("charlib/Ružena/hand", service.find_catalog(UUID_POSES_RUZENA_HAND)->path.str())
      << "Changing the path should update children.";
  EXPECT_EQ("charlib/Ružena/face", service.find_catalog(UUID_POSES_RUZENA_FACE)->path.str())
      << "Changing the path should update children.";
}

TEST_F(AssetCatalogTest, update_catalog_path_simple_name)
{
  AssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + "/" +
                         AssetCatalogService::DEFAULT_CATALOG_FILENAME);
  service.update_catalog_path(UUID_POSES_RUZENA, "charlib/Ružena");

  /* This may not be valid forever; maybe at some point we'll expose the simple name to users &
   * let them change it from the UI. Until then, automatically updating it is better, because
   * otherwise all simple names would be "Catalog". */
  EXPECT_EQ("charlib-Ružena", service.find_catalog(UUID_POSES_RUZENA)->simple_name)
      << "Changing the path should update the simplename.";
  EXPECT_EQ("charlib-Ružena-face", service.find_catalog(UUID_POSES_RUZENA_FACE)->simple_name)
      << "Changing the path should update the simplename of children.";
}

TEST_F(AssetCatalogTest, merge_catalog_files)
{
  const CatalogFilePath cdf_dir = create_temp_path();
  const CatalogFilePath original_cdf_file = asset_library_root_ + "/blender_assets.cats.txt";
  const CatalogFilePath modified_cdf_file = asset_library_root_ + "/modified_assets.cats.txt";
  const CatalogFilePath temp_cdf_file = cdf_dir + "blender_assets.cats.txt";
  ASSERT_EQ(0, BLI_copy(original_cdf_file.c_str(), temp_cdf_file.c_str()));

  /* Load the unmodified, original CDF. */
  TestableAssetCatalogService service(asset_library_root_);
  service.load_from_disk(cdf_dir);

  /* Copy a modified file, to mimic a situation where someone changed the
   * CDF after we loaded it. */
  ASSERT_EQ(0, BLI_copy(modified_cdf_file.c_str(), temp_cdf_file.c_str()));

  /* Overwrite the modified file. This should merge the on-disk file with our catalogs. */
  service.write_to_disk(cdf_dir + "phony.blend");

  AssetCatalogService loaded_service(cdf_dir);
  loaded_service.load_from_disk();

  /* Test that the expected catalogs are there. */
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE_WHITESPACE));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE_TRAILING_SLASH));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA_HAND));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA_FACE));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_AGENT_47)); /* New in the modified file. */

  /* When there are overlaps, the in-memory (i.e. last-saved) paths should win. */
  const AssetCatalog *ruzena_face = loaded_service.find_catalog(UUID_POSES_RUZENA_FACE);
  EXPECT_EQ("character/Ružena/poselib/face", ruzena_face->path.str());
}

TEST_F(AssetCatalogTest, backups)
{
  const CatalogFilePath cdf_dir = create_temp_path();
  const CatalogFilePath original_cdf_file = asset_library_root_ + "/blender_assets.cats.txt";
  const CatalogFilePath writable_cdf_file = cdf_dir + "/blender_assets.cats.txt";
  ASSERT_EQ(0, BLI_copy(original_cdf_file.c_str(), writable_cdf_file.c_str()));

  /* Read a CDF, modify, and write it. */
  AssetCatalogService service(cdf_dir);
  service.load_from_disk();
  service.delete_catalog_by_id(UUID_POSES_ELLIE);
  service.write_to_disk(cdf_dir + "phony.blend");

  const CatalogFilePath backup_path = writable_cdf_file + "~";
  ASSERT_TRUE(BLI_is_file(backup_path.c_str()));

  AssetCatalogService loaded_service;
  loaded_service.load_from_disk(backup_path);

  /* Test that the expected catalogs are there, including the deleted one.
   * This is the backup, after all. */
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE_WHITESPACE));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE_TRAILING_SLASH));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA_HAND));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA_FACE));
}

TEST_F(AssetCatalogTest, order_by_path)
{
  const bUUID cat2_uuid("22222222-b847-44d9-bdca-ff04db1c24f5");
  const bUUID cat4_uuid("11111111-b847-44d9-bdca-ff04db1c24f5"); /* Sorts earlier than above. */
  const AssetCatalog cat1(BLI_uuid_generate_random(), "simple/path/child", "");
  const AssetCatalog cat2(cat2_uuid, "simple/path", "");
  const AssetCatalog cat3(BLI_uuid_generate_random(), "complex/path/...or/is/it?", "");
  const AssetCatalog cat4(
      cat4_uuid, "simple/path", "different ID, same path");                /* should be kept */
  const AssetCatalog cat5(cat4_uuid, "simple/path", "same ID, same path"); /* disappears */

  AssetCatalogOrderedSet by_path;
  by_path.insert(&cat1);
  by_path.insert(&cat2);
  by_path.insert(&cat3);
  by_path.insert(&cat4);
  by_path.insert(&cat5);

  AssetCatalogOrderedSet::const_iterator set_iter = by_path.begin();

  EXPECT_EQ(1, by_path.count(&cat1));
  EXPECT_EQ(1, by_path.count(&cat2));
  EXPECT_EQ(1, by_path.count(&cat3));
  EXPECT_EQ(1, by_path.count(&cat4));
  ASSERT_EQ(4, by_path.size()) << "Expecting cat5 to not be stored in the set, as it duplicates "
                                  "an already-existing path + UUID";

  EXPECT_EQ(cat3.catalog_id, (*(set_iter++))->catalog_id); /* complex/path */
  EXPECT_EQ(cat4.catalog_id, (*(set_iter++))->catalog_id); /* simple/path with 111.. ID */
  EXPECT_EQ(cat2.catalog_id, (*(set_iter++))->catalog_id); /* simple/path with 222.. ID */
  EXPECT_EQ(cat1.catalog_id, (*(set_iter++))->catalog_id); /* simple/path/child */

  if (set_iter != by_path.end()) {
    const AssetCatalog *next_cat = *set_iter;
    FAIL() << "Did not expect more items in the set, had at least " << next_cat->catalog_id << ":"
           << next_cat->path;
  }
}

TEST_F(AssetCatalogTest, order_by_path_and_first_seen)
{
  AssetCatalogService service;
  service.load_from_disk(asset_library_root_);

  const bUUID first_seen_uuid("3d451c87-27d1-40fd-87fc-f4c9e829c848");
  const bUUID first_sorted_uuid("00000000-0000-0000-0000-000000000001");
  const bUUID last_sorted_uuid("ffffffff-ffff-ffff-ffff-ffffffffffff");

  AssetCatalog first_seen_cat(first_seen_uuid, "simple/path/child", "");
  const AssetCatalog first_sorted_cat(first_sorted_uuid, "simple/path/child", "");
  const AssetCatalog last_sorted_cat(last_sorted_uuid, "simple/path/child", "");

  /* Mimic that this catalog was first-seen when loading from disk. */
  first_seen_cat.flags.is_first_loaded = true;

  /* Just an assertion of the defaults; this is more to avoid confusing errors later on than an
   * actual test of these defaults. */
  ASSERT_FALSE(first_sorted_cat.flags.is_first_loaded);
  ASSERT_FALSE(last_sorted_cat.flags.is_first_loaded);

  AssetCatalogOrderedSet by_path;
  by_path.insert(&first_seen_cat);
  by_path.insert(&first_sorted_cat);
  by_path.insert(&last_sorted_cat);

  AssetCatalogOrderedSet::const_iterator set_iter = by_path.begin();

  EXPECT_EQ(1, by_path.count(&first_seen_cat));
  EXPECT_EQ(1, by_path.count(&first_sorted_cat));
  EXPECT_EQ(1, by_path.count(&last_sorted_cat));
  ASSERT_EQ(3, by_path.size());

  EXPECT_EQ(first_seen_uuid, (*(set_iter++))->catalog_id);
  EXPECT_EQ(first_sorted_uuid, (*(set_iter++))->catalog_id);
  EXPECT_EQ(last_sorted_uuid, (*(set_iter++))->catalog_id);

  if (set_iter != by_path.end()) {
    const AssetCatalog *next_cat = *set_iter;
    FAIL() << "Did not expect more items in the set, had at least " << next_cat->catalog_id << ":"
           << next_cat->path;
  }
}

TEST_F(AssetCatalogTest, create_missing_catalogs)
{
  TestableAssetCatalogService new_service;
  new_service.create_catalog("path/with/missing/parents");

  EXPECT_EQ(nullptr, new_service.find_catalog_by_path("path/with/missing"))
      << "Missing parents should not be immediately created.";
  EXPECT_EQ(nullptr, new_service.find_catalog_by_path("")) << "Empty path should never be valid";

  new_service.create_missing_catalogs();

  EXPECT_NE(nullptr, new_service.find_catalog_by_path("path/with/missing"));
  EXPECT_NE(nullptr, new_service.find_catalog_by_path("path/with"));
  EXPECT_NE(nullptr, new_service.find_catalog_by_path("path"));
  EXPECT_EQ(nullptr, new_service.find_catalog_by_path(""))
      << "Empty path should never be valid, even when after missing catalogs";
}

TEST_F(AssetCatalogTest, create_missing_catalogs_after_loading)
{
  TestableAssetCatalogService loaded_service(asset_library_root_);
  loaded_service.load_from_disk();

  const AssetCatalog *cat_char = loaded_service.find_catalog_by_path("character");
  const AssetCatalog *cat_ellie = loaded_service.find_catalog_by_path("character/Ellie");
  const AssetCatalog *cat_ruzena = loaded_service.find_catalog_by_path("character/Ružena");
  ASSERT_NE(nullptr, cat_char) << "Missing parents should be created immediately after loading.";
  ASSERT_NE(nullptr, cat_ellie) << "Missing parents should be created immediately after loading.";
  ASSERT_NE(nullptr, cat_ruzena) << "Missing parents should be created immediately after loading.";

  AssetCatalogDefinitionFile *cdf = loaded_service.get_catalog_definition_file();
  ASSERT_NE(nullptr, cdf);
  EXPECT_TRUE(cdf->contains(cat_char->catalog_id)) << "Missing parents should be saved to a CDF.";
  EXPECT_TRUE(cdf->contains(cat_ellie->catalog_id)) << "Missing parents should be saved to a CDF.";
  EXPECT_TRUE(cdf->contains(cat_ruzena->catalog_id))
      << "Missing parents should be saved to a CDF.";

  /* Check that each missing parent is only created once. The CDF contains multiple paths that
   * could trigger the creation of missing parents, so this test makes sense. */
  EXPECT_EQ(1, loaded_service.count_catalogs_with_path("character"));
  EXPECT_EQ(1, loaded_service.count_catalogs_with_path("character/Ellie"));
  EXPECT_EQ(1, loaded_service.count_catalogs_with_path("character/Ružena"));
}

TEST_F(AssetCatalogTest, create_catalog_filter)
{
  AssetCatalogService service(asset_library_root_);
  service.load_from_disk();

  /* Alias for the same catalog as the main one. */
  AssetCatalog *alias_ruzena = service.create_catalog("character/Ružena/poselib");
  /* Alias for a sub-catalog. */
  AssetCatalog *alias_ruzena_hand = service.create_catalog("character/Ružena/poselib/hand");

  AssetCatalogFilter filter = service.create_catalog_filter(UUID_POSES_RUZENA);

  /* Positive test for loaded-from-disk catalogs. */
  EXPECT_TRUE(filter.contains(UUID_POSES_RUZENA))
      << "Main catalog should be included in the filter.";
  EXPECT_TRUE(filter.contains(UUID_POSES_RUZENA_HAND))
      << "Sub-catalog should be included in the filter.";
  EXPECT_TRUE(filter.contains(UUID_POSES_RUZENA_FACE))
      << "Sub-catalog should be included in the filter.";

  /* Positive test for newly-created catalogs. */
  EXPECT_TRUE(filter.contains(alias_ruzena->catalog_id))
      << "Alias of main catalog should be included in the filter.";
  EXPECT_TRUE(filter.contains(alias_ruzena_hand->catalog_id))
      << "Alias of sub-catalog should be included in the filter.";

  /* Negative test for unrelated catalogs. */
  EXPECT_FALSE(filter.contains(BLI_uuid_nil())) << "Nil catalog should not be included.";
  EXPECT_FALSE(filter.contains(UUID_ID_WITHOUT_PATH));
  EXPECT_FALSE(filter.contains(UUID_POSES_ELLIE));
  EXPECT_FALSE(filter.contains(UUID_POSES_ELLIE_WHITESPACE));
  EXPECT_FALSE(filter.contains(UUID_POSES_ELLIE_TRAILING_SLASH));
  EXPECT_FALSE(filter.contains(UUID_WITHOUT_SIMPLENAME));
}

TEST_F(AssetCatalogTest, create_catalog_filter_for_unknown_uuid)
{
  AssetCatalogService service;
  const bUUID unknown_uuid = BLI_uuid_generate_random();

  AssetCatalogFilter filter = service.create_catalog_filter(unknown_uuid);
  EXPECT_TRUE(filter.contains(unknown_uuid));

  EXPECT_FALSE(filter.contains(BLI_uuid_nil())) << "Nil catalog should not be included.";
  EXPECT_FALSE(filter.contains(UUID_POSES_ELLIE));
}

TEST_F(AssetCatalogTest, create_catalog_filter_for_unassigned_assets)
{
  AssetCatalogService service;

  AssetCatalogFilter filter = service.create_catalog_filter(BLI_uuid_nil());
  EXPECT_TRUE(filter.contains(BLI_uuid_nil()));
  EXPECT_FALSE(filter.contains(UUID_POSES_ELLIE));
}

TEST_F(AssetCatalogTest, cat_collection_deep_copy__empty)
{
  const AssetCatalogCollection empty;
  auto copy = empty.deep_copy();
  EXPECT_NE(&empty, copy.get());
}

class TestableAssetCatalogCollection : public AssetCatalogCollection {
 public:
  OwningAssetCatalogMap &get_catalogs()
  {
    return catalogs_;
  }
  OwningAssetCatalogMap &get_deleted_catalogs()
  {
    return deleted_catalogs_;
  }
  AssetCatalogDefinitionFile *get_catalog_definition_file()
  {
    return catalog_definition_file_.get();
  }
  AssetCatalogDefinitionFile *allocate_catalog_definition_file()
  {
    catalog_definition_file_ = std::make_unique<AssetCatalogDefinitionFile>();
    return get_catalog_definition_file();
  }
};

TEST_F(AssetCatalogTest, cat_collection_deep_copy__nonempty_nocdf)
{
  TestableAssetCatalogCollection catcoll;
  auto cat1 = std::make_unique<AssetCatalog>(UUID_POSES_RUZENA, "poses/Henrik", "");
  auto cat2 = std::make_unique<AssetCatalog>(UUID_POSES_RUZENA_FACE, "poses/Henrik/face", "");
  auto cat3 = std::make_unique<AssetCatalog>(UUID_POSES_RUZENA_HAND, "poses/Henrik/hands", "");
  cat3->flags.is_deleted = true;

  AssetCatalog *cat1_ptr = cat1.get();
  AssetCatalog *cat3_ptr = cat3.get();

  catcoll.get_catalogs().add_new(cat1->catalog_id, std::move(cat1));
  catcoll.get_catalogs().add_new(cat2->catalog_id, std::move(cat2));
  catcoll.get_deleted_catalogs().add_new(cat3->catalog_id, std::move(cat3));

  auto copy = catcoll.deep_copy();
  EXPECT_NE(&catcoll, copy.get());

  TestableAssetCatalogCollection *testcopy = reinterpret_cast<TestableAssetCatalogCollection *>(
      copy.get());

  /* Test catalogs & deleted catalogs. */
  EXPECT_EQ(2, testcopy->get_catalogs().size());
  EXPECT_EQ(1, testcopy->get_deleted_catalogs().size());

  ASSERT_TRUE(testcopy->get_catalogs().contains(UUID_POSES_RUZENA));
  ASSERT_TRUE(testcopy->get_catalogs().contains(UUID_POSES_RUZENA_FACE));
  ASSERT_TRUE(testcopy->get_deleted_catalogs().contains(UUID_POSES_RUZENA_HAND));

  EXPECT_NE(nullptr, testcopy->get_catalogs().lookup(UUID_POSES_RUZENA));
  EXPECT_NE(cat1_ptr, testcopy->get_catalogs().lookup(UUID_POSES_RUZENA).get())
      << "AssetCatalogs should be actual copies.";

  EXPECT_NE(nullptr, testcopy->get_deleted_catalogs().lookup(UUID_POSES_RUZENA_HAND));
  EXPECT_NE(cat3_ptr, testcopy->get_deleted_catalogs().lookup(UUID_POSES_RUZENA_HAND).get())
      << "AssetCatalogs should be actual copies.";
}

class TestableAssetCatalogDefinitionFile : public AssetCatalogDefinitionFile {
 public:
  Map<CatalogID, AssetCatalog *> get_catalogs()
  {
    return catalogs_;
  }
};

TEST_F(AssetCatalogTest, cat_collection_deep_copy__nonempty_cdf)
{
  TestableAssetCatalogCollection catcoll;
  auto cat1 = std::make_unique<AssetCatalog>(UUID_POSES_RUZENA, "poses/Henrik", "");
  auto cat2 = std::make_unique<AssetCatalog>(UUID_POSES_RUZENA_FACE, "poses/Henrik/face", "");
  auto cat3 = std::make_unique<AssetCatalog>(UUID_POSES_RUZENA_HAND, "poses/Henrik/hands", "");
  cat3->flags.is_deleted = true;

  AssetCatalog *cat1_ptr = cat1.get();
  AssetCatalog *cat2_ptr = cat2.get();
  AssetCatalog *cat3_ptr = cat3.get();

  catcoll.get_catalogs().add_new(cat1->catalog_id, std::move(cat1));
  catcoll.get_catalogs().add_new(cat2->catalog_id, std::move(cat2));
  catcoll.get_deleted_catalogs().add_new(cat3->catalog_id, std::move(cat3));

  AssetCatalogDefinitionFile *cdf = catcoll.allocate_catalog_definition_file();
  cdf->file_path = "path/to/somewhere.cats.txt";
  cdf->add_new(cat1_ptr);
  cdf->add_new(cat2_ptr);
  cdf->add_new(cat3_ptr);

  /* Test CDF remapping. */
  auto copy = catcoll.deep_copy();
  TestableAssetCatalogCollection *testable_copy = static_cast<TestableAssetCatalogCollection *>(
      copy.get());

  TestableAssetCatalogDefinitionFile *cdf_copy = static_cast<TestableAssetCatalogDefinitionFile *>(
      testable_copy->get_catalog_definition_file());
  EXPECT_EQ(testable_copy->get_catalogs().lookup(UUID_POSES_RUZENA).get(),
            cdf_copy->get_catalogs().lookup(UUID_POSES_RUZENA))
      << "AssetCatalog pointers should have been remapped to the copy.";

  EXPECT_EQ(testable_copy->get_deleted_catalogs().lookup(UUID_POSES_RUZENA_HAND).get(),
            cdf_copy->get_catalogs().lookup(UUID_POSES_RUZENA_HAND))
      << "Deleted AssetCatalog pointers should have been remapped to the copy.";
}

TEST_F(AssetCatalogTest, undo_redo_one_step)
{
  TestableAssetCatalogService service(asset_library_root_);
  service.load_from_disk();

  EXPECT_FALSE(service.is_undo_possbile());
  EXPECT_FALSE(service.is_redo_possbile());

  service.create_catalog("some/catalog/path");
  EXPECT_FALSE(service.is_undo_possbile())
      << "Undo steps should be created explicitly, and not after creating any catalog.";

  service.undo_push();
  const bUUID other_catalog_id = service.create_catalog("other/catalog/path")->catalog_id;
  EXPECT_TRUE(service.is_undo_possbile())
      << "Undo should be possible after creating an undo snapshot.";

  /* Undo the creation of the catalog. */
  service.undo();
  EXPECT_FALSE(service.is_undo_possbile())
      << "Undoing the only stored step should make it impossible to undo further.";
  EXPECT_TRUE(service.is_redo_possbile()) << "Undoing a step should make redo possible.";
  EXPECT_EQ(nullptr, service.find_catalog_by_path("other/catalog/path"))
      << "Undone catalog should not exist after undo.";
  EXPECT_NE(nullptr, service.find_catalog_by_path("some/catalog/path"))
      << "First catalog should still exist after undo.";
  EXPECT_FALSE(service.get_catalog_definition_file()->contains(other_catalog_id))
      << "The CDF should also not contain the undone catalog.";

  /* Redo the creation of the catalog. */
  service.redo();
  EXPECT_TRUE(service.is_undo_possbile())
      << "Undoing and then redoing a step should make it possible to undo again.";
  EXPECT_FALSE(service.is_redo_possbile())
      << "Undoing and then redoing a step should make redo impossible.";
  EXPECT_NE(nullptr, service.find_catalog_by_path("other/catalog/path"))
      << "Redone catalog should exist after redo.";
  EXPECT_NE(nullptr, service.find_catalog_by_path("some/catalog/path"))
      << "First catalog should still exist after redo.";
  EXPECT_TRUE(service.get_catalog_definition_file()->contains(other_catalog_id))
      << "The CDF should contain the redone catalog.";
}

TEST_F(AssetCatalogTest, undo_redo_more_complex)
{
  TestableAssetCatalogService service(asset_library_root_);
  service.load_from_disk();

  service.undo_push();
  service.find_catalog(UUID_POSES_ELLIE_WHITESPACE)->simple_name = "Edited simple name";

  service.undo_push();
  service.find_catalog(UUID_POSES_ELLIE)->path = "poselib/EllieWithEditedPath";

  service.undo();
  service.undo();

  service.undo_push();
  service.find_catalog(UUID_POSES_ELLIE)->simple_name = "Ellie Simple";

  EXPECT_FALSE(service.is_redo_possbile())
      << "After storing an undo snapshot, the redo buffer should be empty.";
  EXPECT_TRUE(service.is_undo_possbile())
      << "After storing an undo snapshot, undoing should be possible";

  EXPECT_EQ(service.find_catalog(UUID_POSES_ELLIE)->simple_name, "Ellie Simple"); /* Not undone. */
  EXPECT_EQ(service.find_catalog(UUID_POSES_ELLIE_WHITESPACE)->simple_name,
            "POSES_ELLIE WHITESPACE");                                                /* Undone. */
  EXPECT_EQ(service.find_catalog(UUID_POSES_ELLIE)->path, "character/Ellie/poselib"); /* Undone. */
}

}  // namespace blender::bke::tests
