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
  AssetLibrary *get_builtin_library_from_type(eAssetLibraryType type)
  {
    AssetLibraryService *service = AssetLibraryService::get();

    AssetLibraryReference ref{};
    ref.type = type;
    return service->get_asset_library(nullptr, ref);
  }

  AssetRepresentation &add_dummy_asset(AssetLibrary &library, StringRef relative_path)
  {
    std::unique_ptr<AssetMetaData> dummy_metadata = std::make_unique<AssetMetaData>();
    return library.add_external_asset(relative_path, "Some asset name", std::move(dummy_metadata));
  }
};

TEST_F(AssetRepresentationTest, weak_reference__current_file)
{
  AssetLibrary *library = get_builtin_library_from_type(ASSET_LIBRARY_LOCAL);
  AssetRepresentation &asset = add_dummy_asset(*library, "path/to/an/asset");

  {
    std::unique_ptr<AssetWeakReference> weak_ref = asset.make_weak_reference();
    EXPECT_EQ(weak_ref->asset_library_type, ASSET_LIBRARY_LOCAL);
    EXPECT_EQ(weak_ref->asset_library_identifier, nullptr);
    EXPECT_STREQ(weak_ref->relative_asset_identifier, "path/to/an/asset");
  }

  {
    /* Also test the C-API, it moves memory, so worth testing. */
    AssetWeakReference *c_weak_ref = AS_asset_representation_weak_reference_create(
        reinterpret_cast<::AssetRepresentation *>(&asset));
    EXPECT_EQ(c_weak_ref->asset_library_type, ASSET_LIBRARY_LOCAL);
    EXPECT_EQ(c_weak_ref->asset_library_identifier, nullptr);
    EXPECT_STREQ(c_weak_ref->relative_asset_identifier, "path/to/an/asset");
    MEM_delete(c_weak_ref);
  }
}

TEST_F(AssetRepresentationTest, weak_reference__custom_library)
{
  AssetLibraryService *service = AssetLibraryService::get();
  AssetLibrary *const library = service->get_asset_library_on_disk_custom("My custom lib",
                                                                          asset_library_root_);
  AssetRepresentation &asset = add_dummy_asset(*library, "path/to/an/asset");

  {
    std::unique_ptr<AssetWeakReference> weak_ref = asset.make_weak_reference();
    EXPECT_EQ(weak_ref->asset_library_type, ASSET_LIBRARY_CUSTOM);
    EXPECT_STREQ(weak_ref->asset_library_identifier, "My custom lib");
    EXPECT_STREQ(weak_ref->relative_asset_identifier, "path/to/an/asset");
  }
}

}  // namespace blender::asset_system::tests
