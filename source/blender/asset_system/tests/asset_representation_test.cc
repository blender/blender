/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string.h"

#include "asset_library_service.hh"
#include "asset_library_test_common.hh"

#include "AS_asset_representation.hh"

#include "BKE_asset.hh"

#include "DNA_asset_types.h"

#include "../intern/utils.hh"

#include "testing/testing.h"

namespace blender::asset_system::tests {

/**
 * Sets up asset library loading so we have a library to load asset representations into (required
 * for some functionality to perform work).
 */
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
    return library.add_external_asset(
        relative_path, "Some asset name", 0, std::move(dummy_metadata));
  }
};

TEST_F(AssetRepresentationTest, weak_reference__current_file)
{
  AssetLibrary *library = get_builtin_library_from_type(ASSET_LIBRARY_LOCAL);
  AssetRepresentation &asset = add_dummy_asset(*library, "path/to/an/asset");

  {
    AssetWeakReference *weak_ref = asset.make_weak_reference();
    EXPECT_EQ(weak_ref->asset_library_type, ASSET_LIBRARY_LOCAL);
    EXPECT_EQ(weak_ref->asset_library_identifier, nullptr);
    EXPECT_STREQ(weak_ref->relative_asset_identifier, "path/to/an/asset");

    BKE_asset_weak_reference_free(&weak_ref);
  }
}

TEST_F(AssetRepresentationTest, weak_reference__custom_library)
{
  AssetLibraryService *service = AssetLibraryService::get();
  AssetLibrary *const library = service->get_asset_library_on_disk_custom("My custom lib",
                                                                          asset_library_root_);
  AssetRepresentation &asset = add_dummy_asset(*library, "path/to/an/asset");

  {
    AssetWeakReference *weak_ref = asset.make_weak_reference();
    EXPECT_EQ(weak_ref->asset_library_type, ASSET_LIBRARY_CUSTOM);
    EXPECT_STREQ(weak_ref->asset_library_identifier, "My custom lib");
    EXPECT_STREQ(weak_ref->relative_asset_identifier, "path/to/an/asset");
    BKE_asset_weak_reference_free(&weak_ref);
  }
}

TEST_F(AssetRepresentationTest, weak_reference__resolve_to_full_path__current_file)
{
  AssetLibraryService *service = AssetLibraryService::get();
  AssetLibrary *library = get_builtin_library_from_type(ASSET_LIBRARY_LOCAL);
  AssetRepresentation &asset = add_dummy_asset(*library, "path/to/an/asset");

  AssetWeakReference *weak_ref = asset.make_weak_reference();

  std::string resolved_path = service->resolve_asset_weak_reference_to_full_path(*weak_ref);
  EXPECT_EQ(resolved_path, "");
  BKE_asset_weak_reference_free(&weak_ref);
}

/* #AssetLibraryService::resolve_asset_weak_reference_to_full_path(). */
TEST_F(AssetRepresentationTest, weak_reference__resolve_to_full_path__custom_library)
{
  AssetLibraryService *service = AssetLibraryService::get();
  AssetLibrary *const library = service->get_asset_library_on_disk_custom("My custom lib",
                                                                          asset_library_root_);
  AssetRepresentation &asset = add_dummy_asset(*library, "path/to/an/asset");

  AssetWeakReference *weak_ref = asset.make_weak_reference();

  std::string expected_path = utils::normalize_path(asset_library_root_ + "/" + "path/") +
                              "to/an/asset";
  std::string resolved_path = service->resolve_asset_weak_reference_to_full_path(*weak_ref);

  EXPECT_EQ(BLI_path_cmp(resolved_path.c_str(), expected_path.c_str()), 0);
  BKE_asset_weak_reference_free(&weak_ref);
}

TEST_F(AssetRepresentationTest,
       weak_reference__resolve_to_full_path__custom_library__windows_pathsep)
{
  AssetLibraryService *service = AssetLibraryService::get();
  AssetLibrary *const library = service->get_asset_library_on_disk_custom("My custom lib",
                                                                          asset_library_root_);
  AssetRepresentation &asset = add_dummy_asset(*library, "path\\to\\an\\asset");

  AssetWeakReference *weak_ref = asset.make_weak_reference();

  std::string expected_path = utils::normalize_path(asset_library_root_ + "\\" + "path\\") +
                              "to\\an\\asset";
  std::string resolved_path = service->resolve_asset_weak_reference_to_full_path(*weak_ref);

  EXPECT_EQ(BLI_path_cmp(resolved_path.c_str(), expected_path.c_str()), 0);
  BKE_asset_weak_reference_free(&weak_ref);
}

/* #AssetLibraryService::resolve_asset_weak_reference_to_exploded_path(). */
TEST_F(AssetRepresentationTest, weak_reference__resolve_to_exploded_path__current_file)
{
  AssetLibraryService *service = AssetLibraryService::get();
  AssetLibrary *library = get_builtin_library_from_type(ASSET_LIBRARY_LOCAL);
  AssetRepresentation &asset = add_dummy_asset(*library, "path/to/an/asset");

  AssetWeakReference *weak_ref = asset.make_weak_reference();

  std::string expected_full_path = utils::normalize_path("path/to/an/asset", 5);
  std::optional<AssetLibraryService::ExplodedPath> resolved_path =
      service->resolve_asset_weak_reference_to_exploded_path(*weak_ref);

  EXPECT_EQ(*resolved_path->full_path, expected_full_path);
  EXPECT_EQ(resolved_path->dir_component, "");
  EXPECT_EQ(resolved_path->group_component, "path");
  /* ID names may contain slashes. */
  EXPECT_EQ(resolved_path->name_component, "to/an/asset");
  BKE_asset_weak_reference_free(&weak_ref);
}

/* #AssetLibraryService::resolve_asset_weak_reference_to_exploded_path(). */
TEST_F(AssetRepresentationTest, weak_reference__resolve_to_exploded_path__custom_library)
{
  AssetLibraryService *service = AssetLibraryService::get();
  AssetLibrary *const library = service->get_asset_library_on_disk_custom("My custom lib",
                                                                          asset_library_root_);
  AssetRepresentation &asset = add_dummy_asset(*library, "some.blend/Material/asset/name");

  AssetWeakReference *weak_ref = asset.make_weak_reference();

  std::string expected_full_path = utils::normalize_path(asset_library_root_ +
                                                         "/some.blend/Material/") +
                                   "asset/name";
  std::optional<AssetLibraryService::ExplodedPath> resolved_path =
      service->resolve_asset_weak_reference_to_exploded_path(*weak_ref);

  EXPECT_EQ(BLI_path_cmp(resolved_path->full_path->c_str(), expected_full_path.c_str()), 0);
  EXPECT_EQ(BLI_path_cmp_normalized(std::string(resolved_path->dir_component).c_str(),
                                    std::string(asset_library_root_ + "/some.blend").c_str()),
            0);
  EXPECT_EQ(resolved_path->group_component, "Material");
  /* ID names may contain slashes. */
  EXPECT_EQ(resolved_path->name_component, "asset/name");
  BKE_asset_weak_reference_free(&weak_ref);
}

/* #AssetLibraryService::resolve_asset_weak_reference_to_exploded_path(). */
TEST_F(AssetRepresentationTest,
       weak_reference__resolve_to_exploded_path__custom_library__windows_pathsep)
{
  AssetLibraryService *service = AssetLibraryService::get();
  AssetLibrary *const library = service->get_asset_library_on_disk_custom("My custom lib",
                                                                          asset_library_root_);
  AssetRepresentation &asset = add_dummy_asset(*library, "some.blend\\Material\\asset/name");

  AssetWeakReference *weak_ref = asset.make_weak_reference();

  std::string expected_full_path = utils::normalize_path(asset_library_root_ +
                                                         "\\some.blend\\Material\\") +
                                   "asset/name";
  std::optional<AssetLibraryService::ExplodedPath> resolved_path =
      service->resolve_asset_weak_reference_to_exploded_path(*weak_ref);

  EXPECT_EQ(BLI_path_cmp(resolved_path->full_path->c_str(), expected_full_path.c_str()), 0);
  EXPECT_EQ(BLI_path_cmp_normalized(std::string(resolved_path->dir_component).c_str(),
                                    std::string(asset_library_root_ + "\\some.blend").c_str()),
            0);
  EXPECT_EQ(resolved_path->group_component, "Material");
  /* ID names may contain slashes. */
  EXPECT_EQ(resolved_path->name_component, "asset/name");
  BKE_asset_weak_reference_free(&weak_ref);
}

}  // namespace blender::asset_system::tests
