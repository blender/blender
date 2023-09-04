/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_uuid_types.h"

#ifdef __cplusplus
#  include <memory>

namespace blender::asset_system {
class AssetLibrary;
class AssetIdentifier;
}  // namespace blender::asset_system

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
#ifdef __cplusplus
  /** Enables use with `std::unique_ptr<AssetMetaData>`. */
  ~AssetMetaData();
#endif

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

  /** Optional copyright of this asset for display in the UI. Dynamic length. */
  char *copyright;

  /** Optional license of this asset for display in the UI. Dynamic length. */
  char *license;

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
  /** Display assets from the current session (current "Main"). */
  ASSET_LIBRARY_LOCAL = 1,
  ASSET_LIBRARY_ALL = 2,
  /** Display assets bundled with Blender by default. */
  ASSET_LIBRARY_ESSENTIALS = 3,

  /** Display assets from custom asset libraries, as defined in the preferences
   * (#bUserAssetLibrary). The name will be taken from #FileSelectParams.asset_library_ref.idname
   * then.
   * In RNA, we add the index of the custom library to this to identify it by index. So keep
   * this last! */
  ASSET_LIBRARY_CUSTOM = 100,
} eAssetLibraryType;

typedef enum eAssetImportMethod {
  /** Regular data-block linking. */
  ASSET_IMPORT_LINK = 0,
  /** Regular data-block appending (basically linking + "Make Local"). */
  ASSET_IMPORT_APPEND = 1,
  /** Append data-block with the #BLO_LIBLINK_APPEND_LOCAL_ID_REUSE flag enabled. Some typically
   * heavy data dependencies (e.g. the image data-blocks of a material, the mesh of an object) may
   * be reused from an earlier append. */
  ASSET_IMPORT_APPEND_REUSE = 2,
} eAssetImportMethod;

typedef enum eAssetLibrary_Flag {
  ASSET_LIBRARY_RELATIVE_PATH = (1 << 0),
} eAssetLibrary_Flag;

/**
 * Information to identify an asset library. May be either one of the predefined types (current
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
 * Information to refer to an asset (may be stored in files) on a "best effort" basis. It should
 * work well enough for many common cases, but can break. For example when the location of the
 * asset changes, the available asset libraries in the Preferences change, an asset library is
 * renamed, or when a file storing this is opened on a different system (with different
 * Preferences).
 *
 * #AssetWeakReference is similar to #AssetIdentifier, but is designed for file storage, not for
 * runtime references.
 *
 * It has two main components:
 * - A reference to the asset library: The #eAssetLibraryType and if that is not enough to identify
 *   the library, a library name (typically given by the user, but may change).
 * - An identifier for the asset within the library: A relative path currently, which can break if
 *   the asset is moved. Could also be a unique key for a database for example.
 *
 * \note Needs freeing through the destructor, so either use a smart pointer or #MEM_delete() for
 *       explicit freeing.
 */
typedef struct AssetWeakReference {
  char _pad[6];

  short asset_library_type; /* #eAssetLibraryType */
  /** If #asset_library_type is not enough to identify the asset library, this string can provide
   * further location info (allocated string). Null otherwise. */
  const char *asset_library_identifier;

  const char *relative_asset_identifier;

#ifdef __cplusplus
  AssetWeakReference();
  AssetWeakReference(AssetWeakReference &&);
  AssetWeakReference(const AssetWeakReference &) = delete;
  ~AssetWeakReference();

  /**
   * See AssetRepresentation::make_weak_reference(). Must be freed using
   * #BKE_asset_weak_reference_free().
   */
  static AssetWeakReference *make_reference(
      const blender::asset_system::AssetLibrary &library,
      const blender::asset_system::AssetIdentifier &asset_identifier);
#endif
} AssetWeakReference;

/**
 * To be replaced by #AssetRepresentation!
 *
 * Not part of the core design, we should try to get rid of it. Only needed to wrap FileDirEntry
 * into a type with PropertyGroup as base, so we can have an RNA collection of #AssetHandle's to
 * pass to the UI.
 *
 * \warning Never store this! When using #ED_assetlist_iterate(), only access it within the
 *          iterator function. The contained file data can be freed since the file cache has a
 *          maximum number of items.
 */
#
#
typedef struct AssetHandle {
  const struct FileDirEntry *file_data;
} AssetHandle;

typedef enum eUserExtensionRepo_Flag {
  /** Maintain disk cache. */
  USER_EXTENSION_FLAG_NO_CACHE = 1 << 0,
} eUserExtensionRepo_Flag;
