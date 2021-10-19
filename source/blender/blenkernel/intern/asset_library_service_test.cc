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

#include "asset_library_service.hh"

#include "BLI_fileops.h"
#include "BLI_path_util.h"

#include "CLG_log.h"

#include "testing/testing.h"

namespace blender::bke::tests {

class AssetLibraryServiceTest : public testing::Test {
 public:
  CatalogFilePath asset_library_root_;

  static void SetUpTestSuite()
  {
    CLG_init();
  }
  static void TearDownTestSuite()
  {
    CLG_exit();
  }

  void SetUp() override
  {
    const std::string test_files_dir = blender::tests::flags_test_asset_dir();
    if (test_files_dir.empty()) {
      FAIL();
    }
    asset_library_root_ = test_files_dir + "/" + "asset_library";
  }

  void TearDown() override
  {
    AssetLibraryService::destroy();
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

  /* Note: there used to be a test for the opposite here, that after a call to
   * AssetLibraryService::destroy() the above calls should return freshly allocated objects. This
   * cannot be reliably tested by just pointer comparison, though. */
}

TEST_F(AssetLibraryServiceTest, library_pointers)
{
  AssetLibraryService *service = AssetLibraryService::get();
  AssetLibrary *const lib = service->get_asset_library_on_disk(asset_library_root_);
  AssetLibrary *const curfile_lib = service->get_asset_library_current_file();

  EXPECT_EQ(lib, service->get_asset_library_on_disk(asset_library_root_))
      << "Calling twice without destroying in between should return the same instance.";
  EXPECT_EQ(curfile_lib, service->get_asset_library_current_file())
      << "Calling twice without destroying in between should return the same instance.";

  /* Note: there used to be a test for the opposite here, that after a call to
   * AssetLibraryService::destroy() the above calls should return freshly allocated objects. This
   * cannot be reliably tested by just pointer comparison, though. */
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
         ELEM(asset_lib_no_slash[strlen(asset_lib_no_slash) - 1], SEP, ALTSEP)) {
    asset_lib_no_slash[strlen(asset_lib_no_slash) - 1] = '\0';
  }

  BLI_path_slash_ensure(asset_lib_with_slash);

  AssetLibrary *const lib_no_slash = service->get_asset_library_on_disk(asset_lib_no_slash);

  EXPECT_EQ(lib_no_slash, service->get_asset_library_on_disk(asset_lib_with_slash))
      << "With or without trailing slash shouldn't matter.";
}

TEST_F(AssetLibraryServiceTest, catalogs_loaded)
{
  AssetLibraryService *const service = AssetLibraryService::get();
  AssetLibrary *const lib = service->get_asset_library_on_disk(asset_library_root_);
  AssetCatalogService *const cat_service = lib->catalog_service.get();

  const bUUID UUID_POSES_ELLIE("df60e1f6-2259-475b-93d9-69a1b4a8db78");
  EXPECT_NE(nullptr, cat_service->find_catalog(UUID_POSES_ELLIE))
      << "Catalogs should be loaded after getting an asset library from disk.";
}

}  // namespace blender::bke::tests
