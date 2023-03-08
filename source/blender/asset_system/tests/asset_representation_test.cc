/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "asset_library_service.hh"
#include "asset_library_test_common.hh"

#include "AS_asset_representation.h"
#include "AS_asset_representation.hh"

#include "DNA_asset_types.h"

#include "testing/testing.h"

namespace blender::asset_system::tests {

/** Sets up asset library loading so we have a library to load asset representations into (required
 * for some functionality to perform work). */
class AssetRepresentationTest : public AssetLibraryTestBase {
 public:
  using AssetLibraryTestBase::SetUpTestSuite;
  using AssetLibraryTestBase::TearDownTestSuite;
};

TEST_F(AssetRepresentationTest, weak_reference)
{
  AssetLibraryService *service = AssetLibraryService::get();

  AssetLibraryReference ref{};
  ref.type = ASSET_LIBRARY_LOCAL;

  AssetLibrary *library = service->get_asset_library(nullptr, ref);
  std::unique_ptr<AssetMetaData> dummy_metadata = std::make_unique<AssetMetaData>();
  AssetRepresentation &asset = library->add_external_asset(
      "path/to/an/asset", "Some Asset Name", std::move(dummy_metadata));

  std::unique_ptr<AssetWeakReference> weak_ref = asset.make_weak_reference();
  EXPECT_EQ(weak_ref->asset_library_type, ASSET_LIBRARY_LOCAL);
  EXPECT_STREQ(weak_ref->asset_library_identifier, "Current File");
  EXPECT_STREQ(weak_ref->relative_asset_identifier, "path/to/an/asset");

  /* Repeat the test with the C-API, which moves data into a guarded allocated block, so worth
   * testing. */
  AssetWeakReference *c_weak_ref = AS_asset_representation_weak_reference_create(
      reinterpret_cast<::AssetRepresentation *>(&asset));
  EXPECT_EQ(c_weak_ref->asset_library_type, ASSET_LIBRARY_LOCAL);
  EXPECT_STREQ(c_weak_ref->asset_library_identifier, "Current File");
  EXPECT_STREQ(c_weak_ref->relative_asset_identifier, "path/to/an/asset");

  MEM_delete(c_weak_ref);
}

}  // namespace blender::asset_system::tests
