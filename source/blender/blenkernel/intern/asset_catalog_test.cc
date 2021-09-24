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

#include "BLI_fileops.h"
#include "BLI_path_util.h"

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

/* UUIDs from lib/tests/asset_library/modified_assets.cats.txt */
const bUUID UUID_AGENT_47("c5744ba5-43f5-4f73-8e52-010ad4a61b34");

/* Subclass that adds accessors such that protected fields can be used in tests. */
class TestableAssetCatalogService : public AssetCatalogService {
 public:
  explicit TestableAssetCatalogService(const CatalogFilePath &asset_library_root)
      : AssetCatalogService(asset_library_root)
  {
  }

  AssetCatalogDefinitionFile *get_catalog_definition_file()
  {
    return catalog_definition_file_.get();
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

  struct CatalogPathInfo {
    StringRef name;
    int parent_count;
  };

  void assert_expected_tree_items(AssetCatalogTree *tree,
                                  const std::vector<CatalogPathInfo> &expected_paths)
  {
    int i = 0;
    tree->foreach_item([&](const AssetCatalogTreeItem &actual_item) {
      ASSERT_LT(i, expected_paths.size())
          << "More catalogs in tree than expected; did not expect " << actual_item.catalog_path();

      char expected_filename[FILE_MAXFILE];
      /* Is the catalog name as expected? "character", "Ellie", ... */
      BLI_split_file_part(
          expected_paths[i].name.data(), expected_filename, sizeof(expected_filename));
      EXPECT_EQ(expected_filename, actual_item.get_name());
      /* Does the computed number of parents match? */
      EXPECT_EQ(expected_paths[i].parent_count, actual_item.count_parents());
      EXPECT_EQ(expected_paths[i].name, actual_item.catalog_path());

      i++;
    });
  }

  void TearDown() override
  {
    if (!temp_library_path_.empty()) {
      BLI_delete(temp_library_path_.c_str(), true, true);
      temp_library_path_ = "";
    }
  }
};

TEST_F(AssetCatalogTest, load_single_file)
{
  AssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + "/" + "blender_assets.cats.txt");

  // Test getting a non-existant catalog ID.
  EXPECT_EQ(nullptr, service.find_catalog(BLI_uuid_generate_random()));

  // Test getting an invalid catalog (without path definition).
  AssetCatalog *cat_without_path = service.find_catalog(UUID_ID_WITHOUT_PATH);
  ASSERT_EQ(nullptr, cat_without_path);

  // Test getting a regular catalog.
  AssetCatalog *poses_ellie = service.find_catalog(UUID_POSES_ELLIE);
  ASSERT_NE(nullptr, poses_ellie);
  EXPECT_EQ(UUID_POSES_ELLIE, poses_ellie->catalog_id);
  EXPECT_EQ("character/Ellie/poselib", poses_ellie->path);
  EXPECT_EQ("POSES_ELLIE", poses_ellie->simple_name);

  // Test whitespace stripping and support in the path.
  AssetCatalog *poses_whitespace = service.find_catalog(UUID_POSES_ELLIE_WHITESPACE);
  ASSERT_NE(nullptr, poses_whitespace);
  EXPECT_EQ(UUID_POSES_ELLIE_WHITESPACE, poses_whitespace->catalog_id);
  EXPECT_EQ("character/Ellie/poselib/white space", poses_whitespace->path);
  EXPECT_EQ("POSES_ELLIE WHITESPACE", poses_whitespace->simple_name);

  // Test getting a UTF-8 catalog ID.
  AssetCatalog *poses_ruzena = service.find_catalog(UUID_POSES_RUZENA);
  ASSERT_NE(nullptr, poses_ruzena);
  EXPECT_EQ(UUID_POSES_RUZENA, poses_ruzena->catalog_id);
  EXPECT_EQ("character/Ružena/poselib", poses_ruzena->path);
  EXPECT_EQ("POSES_RUŽENA", poses_ruzena->simple_name);
}

TEST_F(AssetCatalogTest, load_single_file_into_tree)
{
  AssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + "/" + "blender_assets.cats.txt");

  /* Contains not only paths from the CDF but also the missing parents (implicitly defined
   * catalogs). */
  std::vector<CatalogPathInfo> expected_paths{
      {"character", 0},
      {"character/Ellie", 1},
      {"character/Ellie/poselib", 2},
      {"character/Ellie/poselib/tailslash", 3},
      {"character/Ellie/poselib/white space", 3},
      {"character/Ružena", 1},
      {"character/Ružena/poselib", 2},
      {"character/Ružena/poselib/face", 3},
      {"character/Ružena/poselib/hand", 3},
      {"path", 0},                     // Implicit.
      {"path/without", 1},             // Implicit.
      {"path/without/simplename", 2},  // From CDF.
  };

  AssetCatalogTree *tree = service.get_catalog_tree();
  assert_expected_tree_items(tree, expected_paths);
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

  // Test that the expected catalogs are there.
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE_WHITESPACE));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE_TRAILING_SLASH));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA_HAND));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA_FACE));

  // Test that the invalid catalog definition wasn't copied.
  EXPECT_EQ(nullptr, loaded_service.find_catalog(UUID_ID_WITHOUT_PATH));

  // TODO(@sybren): test ordering of catalogs in the file.
}

TEST_F(AssetCatalogTest, no_writing_empty_files)
{
  const CatalogFilePath temp_lib_root = create_temp_path();
  AssetCatalogService service(temp_lib_root);
  service.write_to_disk(temp_lib_root);

  const CatalogFilePath default_cdf_path = temp_lib_root +
                                           AssetCatalogService::DEFAULT_CATALOG_FILENAME;
  EXPECT_FALSE(BLI_exists(default_cdf_path.c_str()));
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
  service.write_to_disk(temp_lib_root);
  EXPECT_TRUE(BLI_is_dir(temp_lib_root.c_str()));

  const CatalogFilePath definition_file_path = temp_lib_root + "/" +
                                               AssetCatalogService::DEFAULT_CATALOG_FILENAME;
  EXPECT_TRUE(BLI_is_file(definition_file_path.c_str()));

  AssetCatalogService loaded_service(temp_lib_root);
  loaded_service.load_from_disk();

  // Test that the expected catalog is there.
  AssetCatalog *written_cat = loaded_service.find_catalog(cat->catalog_id);
  ASSERT_NE(nullptr, written_cat);
  EXPECT_EQ(written_cat->catalog_id, cat->catalog_id);
  EXPECT_EQ(written_cat->path, cat->path);
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
  BLI_copy(default_catalog_path.c_str(), writable_catalog_path.c_str());
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
  service.write_to_disk(temp_lib_root);
  AssetCatalogService reloaded_service(temp_lib_root);
  reloaded_service.load_from_disk();
  EXPECT_NE(nullptr, reloaded_service.find_catalog(UUID_POSES_ELLIE))
      << "expected pre-existing catalogs to be kept in the file";
  EXPECT_NE(nullptr, reloaded_service.find_catalog(new_catalog_id))
      << "expecting newly added catalog to exist in the file";
}

TEST_F(AssetCatalogTest, create_catalog_path_cleanup)
{
  const CatalogFilePath temp_lib_root = use_temp_path();
  AssetCatalogService service(temp_lib_root);
  AssetCatalog *cat = service.create_catalog(" /some/path  /  ");

  EXPECT_FALSE(BLI_uuid_is_nil(cat->catalog_id));
  EXPECT_EQ("some/path", cat->path);
  EXPECT_EQ("some-path", cat->simple_name);
}

TEST_F(AssetCatalogTest, create_catalog_simple_name)
{
  const CatalogFilePath temp_lib_root = use_temp_path();
  AssetCatalogService service(temp_lib_root);
  AssetCatalog *cat = service.create_catalog(
      "production/Spite Fright/Characters/Victora/Pose Library/Approved/Body Parts/Hands");

  EXPECT_FALSE(BLI_uuid_is_nil(cat->catalog_id));
  EXPECT_EQ("production/Spite Fright/Characters/Victora/Pose Library/Approved/Body Parts/Hands",
            cat->path);
  EXPECT_EQ("...ht-Characters-Victora-Pose Library-Approved-Body Parts-Hands", cat->simple_name);
}

TEST_F(AssetCatalogTest, delete_catalog_leaf)
{
  AssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + "/" + "blender_assets.cats.txt");

  /* Delete a leaf catalog, i.e. one that is not a parent of another catalog.
   * This keeps this particular test easy. */
  service.delete_catalog(UUID_POSES_RUZENA_HAND);
  EXPECT_EQ(nullptr, service.find_catalog(UUID_POSES_RUZENA_HAND));

  /* Contains not only paths from the CDF but also the missing parents (implicitly defined
   * catalogs). This is why a leaf catalog was deleted. */
  std::vector<CatalogPathInfo> expected_paths{
      {"character", 0},
      {"character/Ellie", 1},
      {"character/Ellie/poselib", 2},
      {"character/Ellie/poselib/tailslash", 3},
      {"character/Ellie/poselib/white space", 3},
      {"character/Ružena", 1},
      {"character/Ružena/poselib", 2},
      {"character/Ružena/poselib/face", 3},
      // {"character/Ružena/poselib/hand", 3}, // this is the deleted one
      {"path", 0},
      {"path/without", 1},
      {"path/without/simplename", 2},
  };

  AssetCatalogTree *tree = service.get_catalog_tree();
  assert_expected_tree_items(tree, expected_paths);
}

TEST_F(AssetCatalogTest, delete_catalog_write_to_disk)
{
  TestableAssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ + "/" +
                         AssetCatalogService::DEFAULT_CATALOG_FILENAME);

  service.delete_catalog(UUID_POSES_ELLIE);

  const CatalogFilePath save_to_path = use_temp_path();
  AssetCatalogDefinitionFile *cdf = service.get_catalog_definition_file();
  cdf->write_to_disk(save_to_path + "/" + AssetCatalogService::DEFAULT_CATALOG_FILENAME);

  AssetCatalogService loaded_service(save_to_path);
  loaded_service.load_from_disk();

  // Test that the expected catalogs are there, except the deleted one.
  EXPECT_EQ(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE_WHITESPACE));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE_TRAILING_SLASH));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA_HAND));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA_FACE));
}

TEST_F(AssetCatalogTest, merge_catalog_files)
{
  const CatalogFilePath cdf_dir = create_temp_path();
  const CatalogFilePath original_cdf_file = asset_library_root_ + "/blender_assets.cats.txt";
  const CatalogFilePath modified_cdf_file = asset_library_root_ + "/modified_assets.cats.txt";
  const CatalogFilePath temp_cdf_file = cdf_dir + "blender_assets.cats.txt";
  BLI_copy(original_cdf_file.c_str(), temp_cdf_file.c_str());

  // Load the unmodified, original CDF.
  TestableAssetCatalogService service(asset_library_root_);
  service.load_from_disk(cdf_dir);

  // Copy a modified file, to mimick a situation where someone changed the CDF after we loaded it.
  BLI_copy(modified_cdf_file.c_str(), temp_cdf_file.c_str());

  // Overwrite the modified file. This should merge the on-disk file with our catalogs.
  service.write_to_disk(cdf_dir);

  AssetCatalogService loaded_service(cdf_dir);
  loaded_service.load_from_disk();

  // Test that the expected catalogs are there.
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE_WHITESPACE));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_ELLIE_TRAILING_SLASH));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA_HAND));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_POSES_RUZENA_FACE));
  EXPECT_NE(nullptr, loaded_service.find_catalog(UUID_AGENT_47));  // New in the modified file.

  // When there are overlaps, the in-memory (i.e. last-saved) paths should win.
  const AssetCatalog *ruzena_face = loaded_service.find_catalog(UUID_POSES_RUZENA_FACE);
  EXPECT_EQ("character/Ružena/poselib/face", ruzena_face->path);
}

TEST_F(AssetCatalogTest, backups)
{
  const CatalogFilePath cdf_dir = create_temp_path();
  const CatalogFilePath original_cdf_file = asset_library_root_ + "/blender_assets.cats.txt";
  const CatalogFilePath writable_cdf_file = cdf_dir + "/blender_assets.cats.txt";
  BLI_copy(original_cdf_file.c_str(), writable_cdf_file.c_str());

  /* Read a CDF, modify, and write it. */
  AssetCatalogService service(cdf_dir);
  service.load_from_disk();
  service.delete_catalog(UUID_POSES_ELLIE);
  service.write_to_disk(cdf_dir);

  const CatalogFilePath backup_path = writable_cdf_file + "~";
  ASSERT_TRUE(BLI_is_file(backup_path.c_str()));

  AssetCatalogService loaded_service;
  loaded_service.load_from_disk(backup_path);

  // Test that the expected catalogs are there, including the deleted one.
  // This is the backup, after all.
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
  const bUUID cat4_uuid("11111111-b847-44d9-bdca-ff04db1c24f5");  // Sorts earlier than above.
  const AssetCatalog cat1(BLI_uuid_generate_random(), "simple/path/child", "");
  const AssetCatalog cat2(cat2_uuid, "simple/path", "");
  const AssetCatalog cat3(BLI_uuid_generate_random(), "complex/path/...or/is/it?", "");
  const AssetCatalog cat4(cat4_uuid, "simple/path", "different ID, same path");  // should be kept
  const AssetCatalog cat5(cat4_uuid, "simple/path", "same ID, same path");       // disappears

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

  EXPECT_EQ(cat3.catalog_id, (*(set_iter++))->catalog_id);  // complex/path
  EXPECT_EQ(cat4.catalog_id, (*(set_iter++))->catalog_id);  // simple/path with 111.. ID
  EXPECT_EQ(cat2.catalog_id, (*(set_iter++))->catalog_id);  // simple/path with 222.. ID
  EXPECT_EQ(cat1.catalog_id, (*(set_iter++))->catalog_id);  // simple/path/child

  if (set_iter != by_path.end()) {
    const AssetCatalog *next_cat = *set_iter;
    FAIL() << "Did not expect more items in the set, had at least " << next_cat->catalog_id << ":"
           << next_cat->path;
  }
}

}  // namespace blender::bke::tests
