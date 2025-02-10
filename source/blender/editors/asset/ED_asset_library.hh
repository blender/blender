/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include "BLI_function_ref.hh"
#include "BLI_string_ref.hh"
#include "DNA_asset_types.h"

struct bUserAssetLibrary;
struct bContext;
struct AssetLibraryReference;
struct EnumPropertyItem;
struct StringPropertySearchVisitParams;

namespace blender::asset_system {
class AssetCatalog;
class AssetCatalogPath;
class AssetRepresentation;
}  // namespace blender::asset_system

namespace blender::ed::asset {

/**
 * Return an index that can be used to uniquely identify \a library, assuming
 * that all relevant indices were created with this function.
 */
int library_reference_to_enum_value(const AssetLibraryReference *library);
/**
 * Return an asset library reference matching the index returned by
 * #library_reference_to_enum_value().
 */
AssetLibraryReference library_reference_from_enum_value(int value);
/**
 * Translate all available asset libraries to an RNA enum, whereby the enum values match the result
 * of #library_reference_to_enum_value() for any given library.
 *
 * Since this is meant for UI display, skips non-displayable libraries, that is, libraries with an
 * empty name or path.
 *
 * \param include_readonly: If set, the "All" and "Essentials" asset libraries will be added, which
 * cannot be written to.
 * \param include_current_file: If set, "Current File" asset library will be added.
 */
const EnumPropertyItem *library_reference_to_rna_enum_itemf(bool include_readonly,
                                                            bool include_current_file);
/**
 * Same as #library_reference_to_rna_enum_itemf(), but only includes custom asset libraries
 * (libraries on disk, configured in the Preferences).
 */
const EnumPropertyItem *custom_libraries_rna_enum_itemf();

/**
 * Find the catalog with the given path in the library. Creates it in case it doesn't exist.
 */
blender::asset_system::AssetCatalog &library_ensure_catalogs_in_path(
    blender::asset_system::AssetLibrary &library,
    const blender::asset_system::AssetCatalogPath &path);

AssetLibraryReference user_library_to_library_ref(const bUserAssetLibrary &user_library);

/**
 * Call after changes to an asset library have been made to reflect the changes in the UI.
 */
void refresh_asset_library(const bContext *C, const AssetLibraryReference &library_ref);
void refresh_asset_library(const bContext *C, const bUserAssetLibrary &user_library);
void refresh_asset_library_from_asset(const bContext *C,
                                      const blender::asset_system::AssetRepresentation &asset);

}  // namespace blender::ed::asset
