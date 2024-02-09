/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "AS_asset_catalog.hh"
#include "AS_asset_library.hh"

#include "BKE_callbacks.hh"

#include "asset_library_service.hh"

#include "CLG_log.h"

#include "testing/testing.h"

namespace blender::asset_system::tests {

class AssetLibraryTest : public testing::Test {
 public:
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

  void TearDown() override
  {
    asset_system::AssetLibraryService::destroy();
  }
};

TEST_F(AssetLibraryTest, AS_asset_library_load)
{
  const std::string test_files_dir = blender::tests::flags_test_asset_dir();
  if (test_files_dir.empty()) {
    FAIL();
  }

  /* Load the asset library. */
  const std::string library_dirpath = test_files_dir + "/" + "asset_library";
  AssetLibrary *library = AS_asset_library_load(__func__, library_dirpath.data());
  ASSERT_NE(nullptr, library);

  /* Check that it can be cast to the C++ type and has a Catalog Service. */
  AssetCatalogService *service = library->catalog_service.get();
  ASSERT_NE(nullptr, service);

  /* Check that the catalogs defined in the library are actually loaded. This just tests one single
   * catalog, as that indicates the file has been loaded. Testing that loading went OK is for
   * the asset catalog service tests. */
  const bUUID uuid_poses_ellie("df60e1f6-2259-475b-93d9-69a1b4a8db78");
  AssetCatalog *poses_ellie = service->find_catalog(uuid_poses_ellie);
  ASSERT_NE(nullptr, poses_ellie) << "unable to find POSES_ELLIE catalog";
  EXPECT_EQ("character/Ellie/poselib", poses_ellie->path.str());
}

TEST_F(AssetLibraryTest, load_nonexistent_directory)
{
  const std::string test_files_dir = blender::tests::flags_test_asset_dir();
  if (test_files_dir.empty()) {
    FAIL();
  }

  /* Load the asset library. */
  const std::string library_dirpath = test_files_dir + "/" +
                                      "asset_library/this/subdir/does/not/exist";
  AssetLibrary *library = AS_asset_library_load(__func__, library_dirpath.data());
  ASSERT_NE(nullptr, library);

  /* Check that it can be cast to the C++ type and has a Catalog Service. */
  AssetCatalogService *service = library->catalog_service.get();
  ASSERT_NE(nullptr, service);

  /* Check that the catalog service doesn't have any catalogs. */
  EXPECT_TRUE(service->is_empty());
}

}  // namespace blender::asset_system::tests
