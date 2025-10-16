/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 *
 * \brief Main runtime representation of an asset.
 *
 * Abstraction to reference an asset, with necessary data for display & interaction.
 * https://developer.blender.org/docs/features/asset_system/backend/#asset-representation
 */

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"

#include "DNA_ID_enums.h"
#include "DNA_asset_types.h"

struct AssetMetaData;
struct bContext;
struct ID;
struct PreviewImage;
struct ReportList;

namespace blender::asset_system {

class AssetLibrary;

class AssetRepresentation : NonCopyable, NonMovable {
  /** Pointer back to the asset library that owns this asset representation. */
  AssetLibrary &owner_asset_library_;
  /**
   * Uniquely identifies the asset within the asset library. Currently this is always a path (path
   * within the asset library).
   */
  std::string relative_identifier_;

  /** Information specific to online assets. */
  /* TODO move to #AS_remote_library.hh, use instead of passing individual members through API
   * functions? */
  struct OnlineAssetInfo {
    /** The path this file should be downloaded to. Usually relative, but isn't required to. The
     * downloader accepts both cases, see #download_asset() in Python. */
    std::string download_dst_filepath_;
    std::optional<std::string> preview_url_;
  };

  struct ExternalAsset {
    std::string name;
    int id_type = 0;
    std::unique_ptr<AssetMetaData> metadata_ = nullptr;
    PreviewImage *preview_ = nullptr;

    /** Set if this is an online asset only. */
    std::unique_ptr<OnlineAssetInfo> online_info_ = nullptr;
  };
  std::variant<ExternalAsset, ID *> asset_;

  friend class AssetLibrary;

 public:
  /**
   * Constructs an asset representation for an external ID stored on disk. The asset will not be
   * editable.
   *
   * For online assets, use the version with #download_dst_filepath below.
   */
  AssetRepresentation(StringRef relative_asset_path,
                      StringRef name,
                      int id_type,
                      std::unique_ptr<AssetMetaData> metadata,
                      AssetLibrary &owner_asset_library);
  /**
   * Constructs an asset representation for an external ID stored online (requiring download).
   *
   * \param download_dst_filepath: See #download_dst_filepath() getter.
   */
  AssetRepresentation(StringRef relative_asset_path,
                      StringRef name,
                      int id_type,
                      std::unique_ptr<AssetMetaData> metadata,
                      AssetLibrary &owner_asset_library,
                      StringRef download_dst_filepath,
                      std::optional<StringRef> preview_url);
  /**
   * Constructs an asset representation for an ID stored in the current file. This makes the asset
   * local and fully editable.
   */
  AssetRepresentation(StringRef relative_asset_path, ID &id, AssetLibrary &owner_asset_library);
  ~AssetRepresentation();

  /**
   * Create a weak reference for this asset that can be written to files, but can break under a
   * number of conditions.
   * A weak reference can only be created if an asset representation is owned by an asset library.
   */
  AssetWeakReference make_weak_reference() const;

  /**
   * Makes sure the asset ready to load a preview, if necessary.
   *
   * For local IDs it calls #BKE_previewimg_id_ensure(). For others, this sets loading information
   * to the preview but doesn't actually load it. To load it, attach its
   * #PreviewImageRuntime::icon_id to a UI button (UI loads it asynchronously then) or call
   * #BKE_previewimg_ensure() (not asynchronous).
   *
   * For online assets this triggers downloading.
   */
  void ensure_previewable(bContext &C, ReportList *reports = nullptr);
  /**
   * Get the preview of this asset.
   *
   * This will only return a preview for local ID assets or after #ensure_previewable() was
   * called.
   */
  PreviewImage *get_preview() const;

  StringRefNull get_name() const;
  ID_Type get_id_type() const;
  AssetMetaData &get_metadata() const;

  StringRefNull library_relative_identifier() const;
  std::string full_path() const;
  std::string full_library_path() const;

  /**
   * For online assets (see #is_online()), the path this file should be downloaded to when
   * requested. Usually relative, but isn't required to. The downloader accepts both cases, see
   * #download_asset() in Python.
   *
   * Will return an empty value if this is not an online asset.
   */
  std::optional<StringRefNull> download_dst_filepath() const;
  /**
   * For online assets (see #is_online()), the URL the asset's preview should be requested from.
   *
   * Will return an empty value if this is not an online asset.
   */
  std::optional<StringRefNull> online_asset_preview_url() const;
  /**
   * If the asset is marked as online, removes the online data and marking, turning it into a
   * regular on-disk asset.
   */
  void online_asset_mark_downloaded();

  /**
   * Get the import method to use for this asset. A different one may be used if
   * #may_override_import_method() returns true, otherwise, the returned value must be used. If
   * there is no import method predefined for this asset no value is returned.
   */
  std::optional<eAssetImportMethod> get_import_method() const;
  /**
   * Returns if this asset may be imported with an import method other than the one returned by
   * #get_import_method(). Also returns true if there is no predefined import method
   * (when #get_import_method() returns no value).
   */
  bool may_override_import_method() const;
  bool get_use_relative_path() const;
  /**
   * If this asset is stored inside this current file (#is_local_id() is true), this returns the
   * ID's pointer, otherwise null.
   */
  ID *local_id() const;
  /** Returns if this asset is stored inside this current file, and as such fully editable. */
  bool is_local_id() const;
  /** The asset is stored online, not on disk. */
  bool is_online() const;
  AssetLibrary &owner_asset_library() const;
};

}  // namespace blender::asset_system
