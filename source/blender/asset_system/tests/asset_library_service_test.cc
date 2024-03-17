/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "asset_library_service.hh"

#include "BLI_fileops.h" /* For PATH_MAX (at least on Windows). */
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BKE_appdir.hh"
#include "BKE_callbacks.hh"
#include "BKE_main.hh"

#include "DNA_asset_types.h"

#include "CLG_log.h"

#include "testing/testing.h"

namespace blender::asset_system::tests {

const bUUID UUID_POSES_ELLIE("df60e1f6-2259-475b-93d9-69a1b4a8db78");

class AssetLibraryServiceTest : public testing::Test {
 public:
  CatalogFilePath asset_library_root_;
  CatalogFilePath temp_library_path_;

  static void SetUpTestSuite()
  {
    CLG_init();
    BKE_callback_global_init();
  }
  static void TearDownTestSuite()
  {
    CLG_exit();
    BKE_callback_global_finalize();
  }

  void SetUp() override
  {
    const std::string test_files_dir = blender::tests::flags_test_asset_dir();
    if (test_files_dir.empty()) {
      FAIL();
    }
    asset_library_root_ = test_files_dir + SEP_STR + "asset_library";
    temp_library_path_ = "";
  }

  void TearDown() override
  {
    AssetLibraryService::destroy();

    if (!temp_library_path_.empty()) {
      BLI_delete(temp_library_path_.c_str(), true, true);
      temp_library_path_ = "";
    }
  }

  /* Register a temporary path, which will be removed at the end of the test.
   * The returned path ends in a slash. */
  CatalogFilePath use_temp_path()
  {
    BKE_tempdir_init(nullptr);
    const CatalogFilePath tempdir = BKE_tempdir_session();
    temp_library_path_ = tempdir + "test-temporary-path" + SEP_STR;
    return temp_library_path_;
  }

  CatalogFilePath create_temp_path()
  {
    CatalogFilePath path = use_temp_path();
    BLI_dir_create_recursive(path.c_str());
    return path;
  }
};

TEST_F(AssetLibraryServiceTest, get_destroy)
{
  AssetLibraryService *const service = AssetLibraryService::get();
  EXPECT_EQ(service, AssetLibraryService::get())
      << "Calling twice without destroying in between should return the same instance.";

  /* This should not crash. */
  AssetLibraryService::destroy();
  AssetLibraryService::destroy();

  /* NOTE: there used to be a test for the opposite here, that after a call to
   * AssetLibraryService::destroy() the above calls should return freshly allocated objects. This
   * cannot be reliably tested by just pointer comparison, though. */
}

TEST_F(AssetLibraryServiceTest, library_pointers)
{
  AssetLibraryService *service = AssetLibraryService::get();

  AssetLibrary *const lib = service->get_asset_library_on_disk_custom(__func__,
                                                                      asset_library_root_);
  AssetLibrary *const curfile_lib = service->get_asset_library_current_file();

  EXPECT_EQ(lib, service->get_asset_library_on_disk_custom(__func__, asset_library_root_))
      << "Calling twice without destroying in between should return the same instance.";
  EXPECT_EQ(curfile_lib, service->get_asset_library_current_file())
      << "Calling twice without destroying in between should return the same instance.";

  /* NOTE: there used to be a test for the opposite here, that after a call to
   * AssetLibraryService::destroy() the above calls should return freshly allocated objects. This
   * cannot be reliably tested by just pointer comparison, though. */
}

TEST_F(AssetLibraryServiceTest, library_from_reference)
{
  AssetLibraryService *service = AssetLibraryService::get();

  AssetLibrary *const curfile_lib = service->get_asset_library_current_file();

  AssetLibraryReference ref{};
  ref.type = ASSET_LIBRARY_LOCAL;
  EXPECT_EQ(curfile_lib, service->get_asset_library(nullptr, ref))
      << "Getting the local (current file) reference without a main saved on disk should return "
         "the current file library";

  {
    Main dummy_main{};
    std::string dummy_filepath = asset_library_root_ + SEP + "dummy.blend";
    STRNCPY(dummy_main.filepath, dummy_filepath.c_str());

    AssetLibrary *custom_lib = service->get_asset_library_on_disk_custom(__func__,
                                                                         asset_library_root_);
    AssetLibrary *tmp_curfile_lib = service->get_asset_library(&dummy_main, ref);

    /* Requested a current file library with a (fake) file saved in the same directory as a custom
     * asset library. The resulting library should never match the custom asset library, even
     * though the paths match. */

    EXPECT_NE(custom_lib, tmp_curfile_lib)
        << "Getting an asset library from a local (current file) library reference should never "
           "match any custom asset library";
    EXPECT_EQ(custom_lib->root_path(), tmp_curfile_lib->root_path());
  }
}

TEST_F(AssetLibraryServiceTest, library_path_trailing_slashes)
{
  AssetLibraryService *service = AssetLibraryService::get();

  char asset_lib_no_slash[PATH_MAX];
  char asset_lib_with_slash[PATH_MAX];
  STRNCPY(asset_lib_no_slash, asset_library_root_.c_str());
  STRNCPY(asset_lib_with_slash, asset_library_root_.c_str());

  /* Ensure #asset_lib_no_slash has no trailing slash, regardless of what was passed on the CLI to
   * the unit test. */
  while (strlen(asset_lib_no_slash) &&
         ELEM(asset_lib_no_slash[strlen(asset_lib_no_slash) - 1], SEP, ALTSEP))
  {
    asset_lib_no_slash[strlen(asset_lib_no_slash) - 1] = '\0';
  }

  BLI_path_slash_ensure(asset_lib_with_slash, PATH_MAX);

  AssetLibrary *const lib_no_slash = service->get_asset_library_on_disk_custom(__func__,
                                                                               asset_lib_no_slash);

  EXPECT_EQ(lib_no_slash,
            service->get_asset_library_on_disk_custom(__func__, asset_lib_with_slash))
      << "With or without trailing slash shouldn't matter.";
}

TEST_F(AssetLibraryServiceTest, catalogs_loaded)
{
  AssetLibraryService *const service = AssetLibraryService::get();
  AssetLibrary *const lib = service->get_asset_library_on_disk_custom(__func__,
                                                                      asset_library_root_);
  AssetCatalogService &cat_service = lib->catalog_service();

  const bUUID UUID_POSES_ELLIE("df60e1f6-2259-475b-93d9-69a1b4a8db78");
  EXPECT_NE(nullptr, cat_service.find_catalog(UUID_POSES_ELLIE))
      << "Catalogs should be loaded after getting an asset library from disk.";
}

TEST_F(AssetLibraryServiceTest, has_any_unsaved_catalogs)
{
  AssetLibraryService *const service = AssetLibraryService::get();
  EXPECT_FALSE(service->has_any_unsaved_catalogs())
      << "Empty AssetLibraryService should have no unsaved catalogs";

  AssetLibrary *const lib = service->get_asset_library_on_disk_custom(__func__,
                                                                      asset_library_root_);
  AssetCatalogService &cat_service = lib->catalog_service();
  EXPECT_FALSE(service->has_any_unsaved_catalogs())
      << "Unchanged AssetLibrary should have no unsaved catalogs";

  const bUUID UUID_POSES_ELLIE("df60e1f6-2259-475b-93d9-69a1b4a8db78");
  cat_service.prune_catalogs_by_id(UUID_POSES_ELLIE);
  EXPECT_FALSE(service->has_any_unsaved_catalogs())
      << "Deletion of catalogs via AssetCatalogService should not automatically tag as 'unsaved "
         "changes'.";

  const bUUID UUID_POSES_RUZENA("79a4f887-ab60-4bd4-94da-d572e27d6aed");
  AssetCatalog *cat = cat_service.find_catalog(UUID_POSES_RUZENA);
  ASSERT_NE(nullptr, cat) << "Catalog " << UUID_POSES_RUZENA << " should be known";

  cat_service.tag_has_unsaved_changes(cat);
  EXPECT_TRUE(service->has_any_unsaved_catalogs())
      << "Tagging as having unsaved changes of a single catalog service should result in unsaved "
         "changes being reported.";
  EXPECT_TRUE(cat->flags.has_unsaved_changes);
}

TEST_F(AssetLibraryServiceTest, has_any_unsaved_catalogs_after_write)
{
  const CatalogFilePath writable_dir = create_temp_path(); /* Has trailing slash. */
  const CatalogFilePath original_cdf_file = asset_library_root_ + SEP_STR +
                                            "blender_assets.cats.txt";
  CatalogFilePath writable_cdf_file = writable_dir + AssetCatalogService::DEFAULT_CATALOG_FILENAME;
  BLI_path_slash_native(writable_cdf_file.data());
  ASSERT_EQ(0, BLI_copy(original_cdf_file.c_str(), writable_cdf_file.c_str()));

  AssetLibraryService *const service = AssetLibraryService::get();
  AssetLibrary *const lib = service->get_asset_library_on_disk_custom(__func__, writable_dir);

  EXPECT_FALSE(service->has_any_unsaved_catalogs())
      << "Unchanged AssetLibrary should have no unsaved catalogs";

  AssetCatalogService &cat_service = lib->catalog_service();
  AssetCatalog *cat = cat_service.find_catalog(UUID_POSES_ELLIE);

  cat_service.tag_has_unsaved_changes(cat);

  EXPECT_TRUE(service->has_any_unsaved_catalogs())
      << "Tagging as having unsaved changes of a single catalog service should result in unsaved "
         "changes being reported.";
  EXPECT_TRUE(cat->flags.has_unsaved_changes);

  cat_service.write_to_disk(writable_dir + "dummy_path.blend");
  EXPECT_FALSE(service->has_any_unsaved_catalogs())
      << "Written AssetCatalogService should have no unsaved catalogs";
  EXPECT_FALSE(cat->flags.has_unsaved_changes);
}

}  // namespace blender::asset_system::tests
