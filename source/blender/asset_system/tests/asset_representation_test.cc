/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "asset_library_service.hh"
#include "asset_library_test_common.hh"

#include "AS_asset_representation.hh"

#include "BKE_lib_id.hh"
#include "BKE_main.hh"

#include "DNA_asset_types.h"
#include "DNA_object_types.h"

#include "ED_asset_mark_clear.hh"

#include "BLI_string.h"

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
    return *library
                .add_external_asset(relative_path, "Some asset name", 0, std::move(dummy_metadata))
                .lock();
  }

  AssetRepresentation &add_dummy_id_asset(AssetLibrary &library, ID &id)
  {
    /* Ensure ID is marked as asset (no-op if already marked). */
    ed::asset::mark_id(&id);

    return *library.add_local_id_asset(id).lock();
  }
};

TEST_F(AssetRepresentationTest, library_relative_identifier__id_name_change)
{
  Main *bmain = BKE_main_new();
  Object *object = BKE_id_new<Object>(bmain, "Before rename");

  AssetLibrary *library = get_builtin_library_from_type(ASSET_LIBRARY_LOCAL);

  AssetRepresentation &asset = add_dummy_id_asset(*library, object->id);

  EXPECT_EQ(asset.library_relative_identifier(), "Object" SEP_STR "Before rename");

  BKE_id_rename(*bmain, object->id, "Renamed!");
  EXPECT_EQ(asset.library_relative_identifier(), "Object" SEP_STR "Renamed!");

  BKE_id_rename(*bmain, object->id, "Name/With\\Slashes/");
  EXPECT_EQ(asset.library_relative_identifier(), "Object" SEP_STR "Name/With\\Slashes/");

  BKE_main_free(bmain);
}

TEST_F(AssetRepresentationTest, weak_reference__current_file)
{
  AssetLibrary *library = get_builtin_library_from_type(ASSET_LIBRARY_LOCAL);
  AssetRepresentation &asset = add_dummy_asset(*library, "path/to/an/asset");

  {
    AssetWeakReference weak_ref = asset.make_weak_reference();
    EXPECT_EQ(weak_ref.asset_library_type, ASSET_LIBRARY_LOCAL);
    EXPECT_EQ(weak_ref.asset_library_identifier, nullptr);
    EXPECT_STREQ(weak_ref.relative_asset_identifier, "path/to/an/asset");
  }
}

TEST_F(AssetRepresentationTest, weak_reference__custom_library)
{
  AssetLibraryService *service = AssetLibraryService::get();
  AssetLibrary *const library = service->get_asset_library_on_disk_custom("My custom lib",
                                                                          asset_library_root_);
  AssetRepresentation &asset = add_dummy_asset(*library, "path/to/an/asset");

  {
    AssetWeakReference weak_ref = asset.make_weak_reference();
    EXPECT_EQ(weak_ref.asset_library_type, ASSET_LIBRARY_CUSTOM);
    EXPECT_STREQ(weak_ref.asset_library_identifier, "My custom lib");
    EXPECT_STREQ(weak_ref.relative_asset_identifier, "path/to/an/asset");
  }
}

/* Test if new weak references the ID name changes. */
TEST_F(AssetRepresentationTest, weak_reference__id_name_change)
{
  Main *bmain = BKE_main_new();
  Object *object = BKE_id_new<Object>(bmain, "Before rename");

  AssetLibrary *library = get_builtin_library_from_type(ASSET_LIBRARY_LOCAL);

  AssetRepresentation &asset = add_dummy_id_asset(*library, object->id);

  {
    AssetWeakReference weak_ref = asset.make_weak_reference();
    EXPECT_EQ(weak_ref.asset_library_type, ASSET_LIBRARY_LOCAL);
    EXPECT_STREQ(weak_ref.asset_library_identifier, nullptr);
    EXPECT_STREQ(weak_ref.relative_asset_identifier, "Object" SEP_STR "Before rename");
  }

  BKE_id_rename(*bmain, object->id, "Renamed!");
  {
    AssetWeakReference weak_ref = asset.make_weak_reference();
    EXPECT_EQ(weak_ref.asset_library_type, ASSET_LIBRARY_LOCAL);
    EXPECT_STREQ(weak_ref.asset_library_identifier, nullptr);
    EXPECT_STREQ(weak_ref.relative_asset_identifier, "Object" SEP_STR "Renamed!");
  }

  BKE_id_rename(*bmain, object->id, "Name/With\\Slashes/");
  {
    AssetWeakReference weak_ref = asset.make_weak_reference();
    EXPECT_EQ(weak_ref.asset_library_type, ASSET_LIBRARY_LOCAL);
    EXPECT_STREQ(weak_ref.asset_library_identifier, nullptr);
    EXPECT_STREQ(weak_ref.relative_asset_identifier, "Object" SEP_STR "Name/With\\Slashes/");
  }

  BKE_main_free(bmain);
}

TEST_F(AssetRepresentationTest, weak_reference__compare)
{
  {
    AssetWeakReference a;
    AssetWeakReference b;
    EXPECT_EQ(a, b);

    /* Arbitrary individual member changes to test how it affects the comparison. */
    b.asset_library_identifier = "My lib";
    EXPECT_NE(a, b);
    a.asset_library_identifier = "My lib";
    EXPECT_EQ(a, b);
    a.asset_library_type = ASSET_LIBRARY_ESSENTIALS;
    EXPECT_NE(a, b);
    b.asset_library_type = ASSET_LIBRARY_LOCAL;
    EXPECT_NE(a, b);
    b.asset_library_type = ASSET_LIBRARY_ESSENTIALS;
    EXPECT_EQ(a, b);
    a.relative_asset_identifier = "Foo";
    EXPECT_NE(a, b);
    b.relative_asset_identifier = "Bar";
    EXPECT_NE(a, b);
    a.relative_asset_identifier = "Bar";
    EXPECT_EQ(a, b);

    /* Make the destructor work. */
    a.asset_library_identifier = b.asset_library_identifier = nullptr;
    a.relative_asset_identifier = b.relative_asset_identifier = nullptr;
  }

  {
    AssetWeakReference a;
    a.asset_library_type = ASSET_LIBRARY_LOCAL;
    a.asset_library_identifier = "My custom lib";
    a.relative_asset_identifier = "path/to/an/asset";

    AssetWeakReference b;
    EXPECT_NE(a, b);

    b.asset_library_type = ASSET_LIBRARY_LOCAL;
    b.asset_library_identifier = "My custom lib";
    b.relative_asset_identifier = "path/to/an/asset";
    EXPECT_EQ(a, b);

    /* Make the destructor work. */
    a.asset_library_identifier = b.asset_library_identifier = nullptr;
    a.relative_asset_identifier = b.relative_asset_identifier = nullptr;
  }

  {
    AssetLibraryService *service = AssetLibraryService::get();
    AssetLibrary *const library = service->get_asset_library_on_disk_custom("My custom lib",
                                                                            asset_library_root_);
    AssetRepresentation &asset = add_dummy_asset(*library, "path/to/an/asset");

    AssetWeakReference weak_ref = asset.make_weak_reference();
    AssetWeakReference other;
    other.asset_library_type = ASSET_LIBRARY_CUSTOM;
    other.asset_library_identifier = "My custom lib";
    other.relative_asset_identifier = "path/to/an/asset";
    EXPECT_EQ(weak_ref, other);

    other.relative_asset_identifier = "";
    EXPECT_NE(weak_ref, other);
    other.relative_asset_identifier = nullptr;
    EXPECT_NE(weak_ref, other);

    /* Make the destructor work. */
    other.asset_library_identifier = nullptr;
    other.relative_asset_identifier = nullptr;
  }

  /* Same but comparing windows and unix style paths. */
  {
    AssetLibraryService *service = AssetLibraryService::get();
    AssetLibrary *const library = service->get_asset_library_on_disk_custom("My custom lib",
                                                                            asset_library_root_);
    AssetRepresentation &asset = add_dummy_asset(*library, "path/to/an/asset");

    AssetWeakReference weak_ref = asset.make_weak_reference();
    AssetWeakReference other;
    other.asset_library_type = ASSET_LIBRARY_CUSTOM;
    other.asset_library_identifier = "My custom lib";
    other.relative_asset_identifier = "path\\to\\an\\asset";
    EXPECT_EQ(weak_ref, other);

    other.relative_asset_identifier = "";
    EXPECT_NE(weak_ref, other);
    other.relative_asset_identifier = nullptr;
    EXPECT_NE(weak_ref, other);

    /* Make the destructor work. */
    other.asset_library_identifier = nullptr;
    other.relative_asset_identifier = nullptr;
  }
}

TEST_F(AssetRepresentationTest, weak_reference__resolve_to_full_path__current_file)
{
  AssetLibraryService *service = AssetLibraryService::get();
  AssetLibrary *library = get_builtin_library_from_type(ASSET_LIBRARY_LOCAL);
  AssetRepresentation &asset = add_dummy_asset(*library, "path/to/an/asset");

  AssetWeakReference weak_ref = asset.make_weak_reference();

  std::string resolved_path = service->resolve_asset_weak_reference_to_full_path(weak_ref);
  EXPECT_EQ(resolved_path, "");
}

/* #AssetLibraryService::resolve_asset_weak_reference_to_full_path(). */
TEST_F(AssetRepresentationTest, weak_reference__resolve_to_full_path__custom_library)
{
  AssetLibraryService *service = AssetLibraryService::get();
  AssetLibrary *const library = service->get_asset_library_on_disk_custom("My custom lib",
                                                                          asset_library_root_);
  AssetRepresentation &asset = add_dummy_asset(*library, "path/to/an/asset");

  AssetWeakReference weak_ref = asset.make_weak_reference();

  std::string expected_path = utils::normalize_path(asset_library_root_ + "/" + "path/") +
                              "to/an/asset";
  std::string resolved_path = service->resolve_asset_weak_reference_to_full_path(weak_ref);

  EXPECT_EQ(BLI_path_cmp(resolved_path.c_str(), expected_path.c_str()), 0);
}

TEST_F(AssetRepresentationTest,
       weak_reference__resolve_to_full_path__custom_library__windows_pathsep)
{
  AssetLibraryService *service = AssetLibraryService::get();
  AssetLibrary *const library = service->get_asset_library_on_disk_custom("My custom lib",
                                                                          asset_library_root_);
  AssetRepresentation &asset = add_dummy_asset(*library, "path\\to\\an\\asset");

  AssetWeakReference weak_ref = asset.make_weak_reference();

  std::string expected_path = utils::normalize_path(asset_library_root_ + "\\" + "path\\") +
                              "to\\an\\asset";
  std::string resolved_path = service->resolve_asset_weak_reference_to_full_path(weak_ref);

  EXPECT_EQ(BLI_path_cmp(resolved_path.c_str(), expected_path.c_str()), 0);
}

/* #AssetLibraryService::resolve_asset_weak_reference_to_exploded_path(). */
TEST_F(AssetRepresentationTest, weak_reference__resolve_to_exploded_path__current_file)
{
  AssetLibraryService *service = AssetLibraryService::get();
  AssetLibrary *library = get_builtin_library_from_type(ASSET_LIBRARY_LOCAL);
  AssetRepresentation &asset = add_dummy_asset(*library, "path/to/an/asset");

  AssetWeakReference weak_ref = asset.make_weak_reference();

  std::string expected_full_path = utils::normalize_path("path/to/an/asset", 5);
  std::optional<AssetLibraryService::ExplodedPath> resolved_path =
      service->resolve_asset_weak_reference_to_exploded_path(weak_ref);

  EXPECT_EQ(*resolved_path->full_path, expected_full_path);
  EXPECT_EQ(resolved_path->dir_component, "");
  EXPECT_EQ(resolved_path->group_component, "path");
  /* ID names may contain slashes. */
  EXPECT_EQ(resolved_path->name_component, "to/an/asset");
}

/* #AssetLibraryService::resolve_asset_weak_reference_to_exploded_path(). */
TEST_F(AssetRepresentationTest, weak_reference__resolve_to_exploded_path__custom_library)
{
  AssetLibraryService *service = AssetLibraryService::get();
  AssetLibrary *const library = service->get_asset_library_on_disk_custom("My custom lib",
                                                                          asset_library_root_);
  AssetRepresentation &asset = add_dummy_asset(*library, "some.blend/Material/asset/name");

  AssetWeakReference weak_ref = asset.make_weak_reference();

  std::string expected_full_path = utils::normalize_path(asset_library_root_ +
                                                         "/some.blend/Material/") +
                                   "asset/name";
  std::optional<AssetLibraryService::ExplodedPath> resolved_path =
      service->resolve_asset_weak_reference_to_exploded_path(weak_ref);

  EXPECT_EQ(BLI_path_cmp(resolved_path->full_path->c_str(), expected_full_path.c_str()), 0);
  EXPECT_EQ(BLI_path_cmp_normalized(std::string(resolved_path->dir_component).c_str(),
                                    std::string(asset_library_root_ + "/some.blend").c_str()),
            0);
  EXPECT_EQ(resolved_path->group_component, "Material");
  /* ID names may contain slashes. */
  EXPECT_EQ(resolved_path->name_component, "asset/name");
}

/* #AssetLibraryService::resolve_asset_weak_reference_to_exploded_path(). */
TEST_F(AssetRepresentationTest,
       weak_reference__resolve_to_exploded_path__custom_library__windows_pathsep)
{
  AssetLibraryService *service = AssetLibraryService::get();
  AssetLibrary *const library = service->get_asset_library_on_disk_custom("My custom lib",
                                                                          asset_library_root_);
  AssetRepresentation &asset = add_dummy_asset(*library, "some.blend\\Material\\asset/name");

  AssetWeakReference weak_ref = asset.make_weak_reference();

  std::string expected_full_path = utils::normalize_path(asset_library_root_ +
                                                         "\\some.blend\\Material\\") +
                                   "asset/name";
  std::optional<AssetLibraryService::ExplodedPath> resolved_path =
      service->resolve_asset_weak_reference_to_exploded_path(weak_ref);

  EXPECT_EQ(BLI_path_cmp(resolved_path->full_path->c_str(), expected_full_path.c_str()), 0);
  EXPECT_EQ(BLI_path_cmp_normalized(std::string(resolved_path->dir_component).c_str(),
                                    std::string(asset_library_root_ + "\\some.blend").c_str()),
            0);
  EXPECT_EQ(resolved_path->group_component, "Material");
  /* ID names may contain slashes. */
  EXPECT_EQ(resolved_path->name_component, "asset/name");
}

}  // namespace blender::asset_system::tests
