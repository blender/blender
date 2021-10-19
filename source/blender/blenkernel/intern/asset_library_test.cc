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
#include "BKE_asset_library.hh"

#include "asset_library_service.hh"

#include "CLG_log.h"

#include "testing/testing.h"

namespace blender::bke::tests {

class AssetLibraryTest : public testing::Test {
 public:
  static void SetUpTestSuite()
  {
    CLG_init();
  }
  static void TearDownTestSuite()
  {
    CLG_exit();
  }

  void TearDown() override
  {
    AssetLibraryService::destroy();
  }
};

TEST_F(AssetLibraryTest, bke_asset_library_load)
{
  const std::string test_files_dir = blender::tests::flags_test_asset_dir();
  if (test_files_dir.empty()) {
    FAIL();
  }

  /* Load the asset library. */
  const std::string library_path = test_files_dir + "/" + "asset_library";
  ::AssetLibrary *library_c_ptr = BKE_asset_library_load(library_path.data());
  ASSERT_NE(nullptr, library_c_ptr);

  /* Check that it can be cast to the C++ type and has a Catalog Service. */
  blender::bke::AssetLibrary *library_cpp_ptr = reinterpret_cast<blender::bke::AssetLibrary *>(
      library_c_ptr);
  AssetCatalogService *service = library_cpp_ptr->catalog_service.get();
  ASSERT_NE(nullptr, service);

  /* Check that the catalogs defined in the library are actually loaded. This just tests one single
   * catalog, as that indicates the file has been loaded. Testing that that loading went OK is for
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
  const std::string library_path = test_files_dir + "/" +
                                   "asset_library/this/subdir/does/not/exist";
  ::AssetLibrary *library_c_ptr = BKE_asset_library_load(library_path.data());
  ASSERT_NE(nullptr, library_c_ptr);

  /* Check that it can be cast to the C++ type and has a Catalog Service. */
  blender::bke::AssetLibrary *library_cpp_ptr = reinterpret_cast<blender::bke::AssetLibrary *>(
      library_c_ptr);
  AssetCatalogService *service = library_cpp_ptr->catalog_service.get();
  ASSERT_NE(nullptr, service);

  /* Check that the catalog service doesn't have any catalogs. */
  EXPECT_TRUE(service->is_empty());
}

}  // namespace blender::bke::tests
