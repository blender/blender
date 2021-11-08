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
 */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_uuid_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief User defined tag.
 * Currently only used by assets, could be used more often at some point.
 * Maybe add a custom icon and color to these in future?
 */
typedef struct AssetTag {
  struct AssetTag *next, *prev;
  char name[64]; /* MAX_NAME */
} AssetTag;

#
#
typedef struct AssetFilterSettings {
  /** Tags to match against. These are newly allocated, and compared against the
   * #AssetMetaData.tags. */
  ListBase tags;     /* AssetTag */
  uint64_t id_types; /* rna_enum_id_type_filter_items */
} AssetFilterSettings;

/**
 * \brief The meta-data of an asset.
 * By creating and giving this for a data-block (#ID.asset_data), the data-block becomes an asset.
 *
 * \note This struct must be readable without having to read anything but blocks from the ID it is
 *       attached to! That way, asset information of a file can be read, without reading anything
 *       more than that from the file. So pointers to other IDs or ID data are strictly forbidden.
 */
typedef struct AssetMetaData {
  /** Runtime type, to reference event callbacks. Only valid for local assets. */
  struct AssetTypeInfo *local_type_info;

  /** Custom asset meta-data. Cannot store pointers to IDs (#STRUCT_NO_DATABLOCK_IDPROPERTIES)! */
  struct IDProperty *properties;

  /**
   * Asset Catalog identifier. Should not contain spaces.
   * Mapped to a path in the asset catalog hierarchy by an #AssetCatalogService.
   * Use #BKE_asset_metadata_catalog_id_set() to ensure a valid ID is set.
   */
  struct bUUID catalog_id;
  /**
   * Short name of the asset's catalog. This is for debugging purposes only, to allow (partial)
   * reconstruction of asset catalogs in the unfortunate case that the mapping from catalog UUID to
   * catalog path is lost. The catalog's simple name is copied to #catalog_simple_name whenever
   * #catalog_id is updated. */
  char catalog_simple_name[64]; /* MAX_NAME */

  /** Optional name of the author for display in the UI. Dynamic length. */
  char *author;

  /** Optional description of this asset for display in the UI. Dynamic length. */
  char *description;

  /** User defined tags for this asset. The asset manager uses these for filtering, but how they
   * function exactly (e.g. how they are registered to provide a list of searchable available tags)
   * is up to the asset-engine. */
  ListBase tags; /* AssetTag */
  short active_tag;
  /** Store the number of tags to avoid continuous counting. Could be turned into runtime data, we
   * can always reliably reconstruct it from the list. */
  short tot_tags;

  char _pad[4];
} AssetMetaData;

typedef enum eAssetLibraryType {
  /* For the future. Display assets bundled with Blender by default. */
  // ASSET_LIBRARY_BUNDLED = 0,
  /** Display assets from the current session (current "Main"). */
  ASSET_LIBRARY_LOCAL = 1,
  /* For the future. Display assets for the current project. */
  // ASSET_LIBRARY_PROJECT = 2,

  /** Display assets from custom asset libraries, as defined in the preferences
   * (#bUserAssetLibrary). The name will be taken from #FileSelectParams.asset_library_ref.idname
   * then.
   * In RNA, we add the index of the custom library to this to identify it by index. So keep
   * this last! */
  ASSET_LIBRARY_CUSTOM = 100,
} eAssetLibraryType;

/**
 * Information to identify a asset library. May be either one of the predefined types (current
 * 'Main', builtin library, project library), or a custom type as defined in the Preferences.
 *
 * If the type is set to #ASSET_LIBRARY_CUSTOM, `custom_library_index` must be set to identify the
 * custom library. Otherwise it is not used.
 */
typedef struct AssetLibraryReference {
  short type; /* eAssetLibraryType */
  char _pad1[2];
  /**
   * If showing a custom asset library (#ASSET_LIBRARY_CUSTOM), this is the index of the
   * #bUserAssetLibrary within #UserDef.asset_libraries.
   * Should be ignored otherwise (but better set to -1 then, for sanity and debugging).
   */
  int custom_library_index;
} AssetLibraryReference;

/**
 * Not part of the core design, we should try to get rid of it. Only needed to wrap FileDirEntry
 * into a type with PropertyGroup as base, so we can have an RNA collection of #AssetHandle's to
 * pass to the UI.
 */
#
#
typedef struct AssetHandle {
  const struct FileDirEntry *file_data;
} AssetHandle;

#ifdef __cplusplus
}
#endif
