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

#include "AS_asset_file_status.hh"

namespace blender {

struct AssetMetaData;
struct bContext;
struct ID;
struct PreviewImage;
struct ReportList;

namespace asset_system {

class AssetLibrary;
struct OnlineAssetInfo;
struct OnlineAssetFile;
struct URLWithHash;

class AssetRepresentation : NonCopyable, NonMovable {
  /** Pointer back to the asset library that owns this asset representation. */
  AssetLibrary &owner_asset_library_;
  /**
   * Uniquely identifies the asset within the asset library. Currently this is always a path (path
   * within the asset library).
   */
  /* Mutable to allow lazy updating on name changes in #library_relative_identifier(). */
  mutable std::string relative_identifier_;

  struct ExternalAsset {
    std::string name;
    int id_type = 0;
    std::unique_ptr<AssetMetaData> metadata_ = nullptr;
    PreviewImage *preview_ = nullptr;

    /**
     * Status of this asset's file(s) compared to the remote listing.
     * Only meaningful for assets from a remote library that have been checked against the listing.
     * For online-only assets (#online_info_ is set), the status is stored there instead.
     *
     * \see #AssetRepresentation::remote_file_status()
     * \see #AssetRepresentation::remote_file_status_set()
     */
    RemoteAssetFileStatus remote_file_status_ = RemoteAssetFileStatus::UNSET;

    /**
     * Set if this is an online asset only.
     *
     * Note that this can also be set on online assets when their files have been downloaded
     * locally. To distinguish between 'pure online' (so no file) and other cases, use the
     * file_status_ field above.
     *
     * \see #AssetRepresentation::is_online_only()
     */
    std::unique_ptr<OnlineAssetInfo> online_info_;
  };
  std::variant<ExternalAsset, ID *> asset_;

  friend class AssetLibrary;

 public:
  /**
   * Constructs an asset representation for an external ID stored on disk. The asset will not be
   * editable.
   *
   * For online assets, use the version with #online_info below.
   */
  AssetRepresentation(StringRef relative_asset_path,
                      StringRef name,
                      int id_type,
                      std::unique_ptr<AssetMetaData> metadata,
                      AssetLibrary &owner_asset_library);
  /**
   * Constructs an asset representation for an external ID stored online (requiring download).
   */
  AssetRepresentation(StringRef relative_asset_path,
                      StringRef name,
                      int id_type,
                      std::unique_ptr<AssetMetaData> metadata,
                      AssetLibrary &owner_asset_library,
                      OnlineAssetInfo online_info);
  /**
   * Constructs an asset representation for an ID stored in the current file. This makes the asset
   * local and fully editable.
   */
  AssetRepresentation(ID &id, AssetLibrary &owner_asset_library);
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
   * For local IDs it calls #BKE_previewimg_id_get(). For others, this sets loading information
   * to the preview but doesn't actually load it. To load it, attach its
   * #PreviewImageRuntime::icon_id to a UI button (UI loads it asynchronously then) or call
   * #BKE_previewimg_ensure() (not asynchronous).
   *
   * For online assets this triggers downloading of the preview.
   */
  void ensure_previewable(const bContext &C, ReportList *reports = nullptr);
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

  /**
   * Return the absolute path of the blend file that contains this asset.
   *
   * Note that this performs a file-system check to see whether the blend file actually exists.
   * If it does not, an empty string is returned. This generally shouldn't be an issue, but can
   * happen, for example when the blend file is deleted and the asset browser not refreshed.
   *
   * This check is a necessity because data-blocks may have .blend and slashes in their name, and
   * directory names may also end in `.blend`, resulting in an identifier like
   * `directory.blend/Objects/filename.blend/Actions/hand/wave.blend/Actions/hi.blend`.
   * Here the file is `directory.blend/Objects/filename.blend` and the asset is an Action named
   * `hand/wave.blend/Actions/hi.blend`.
   */
  std::string full_library_path() const;

  /**
   * For online assets (see #is_online_only()), the files that make up this asset.
   *
   * Will return an empty span if this is not an online asset.
   */
  Span<OnlineAssetFile> online_asset_files() const;
  /**
   * Return the sum of sizes of all files associated with this asset, according to the listing.
   */
  std::optional<int64_t> online_asset_files_combined_size_in_bytes() const;
  /**
   * For online assets (see #is_online_only()), the URL the asset's preview should be requested
   * from.
   *
   * Will return an empty value if this is not an online asset.
   */
  std::optional<StringRefNull> online_asset_preview_url() const;
  /**
   * For online assets (see #is_online_only()), the hash of the asset's preview.
   *
   * Will return an empty value if this is not an online asset.
   */
  std::optional<StringRefNull> online_asset_preview_hash() const;

  /**
   * Turn the online asset into a normal asset. This removes the online data, and the "is online"
   * marking, turning it into a regular on-disk asset.
   *
   * No-op if this is not an online asset.
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
  /**
   * The asset is purely stored online, there is no local file on disk for this.
   *
   * Regardless of what this function returns, there may be 'online info' (information from a
   * remote asset listing) available, even when the file is on disk and this function returns
   * `false`.
   *
   * \see #remote_file_status()
   */
  bool is_online_only() const;
  /**
   * Returns whether the asset is stored in a probably-editable .asset.blend file.
   *
   * NOTE: This is suitable for poll functions (which should not open other files). The actual
   * operator should still check that `G_FILE_ASSET_EDIT_FILE` / `Main::is_asset_edit_file` is set
   * on the `.asset.blend` file (no utility function for this exists yet).
   *
   * NOTE: this function does cause _some_ disk I/O, as it checks one (or more) paths for
   * existence. See #AssetRepresentation::full_library_path() for more info.
   *
   * If the asset is already imported, this check can be done via
   * `bke::asset_edit_id_is_editable(asset_id)` and `bke::asset_edit_id_is_writable(asset_id)`.
   */
  bool is_potentially_editable_asset_blend() const;

  /**
   * Status of this asset's on-disk file(s) compared to the remote listing.
   * Returns #AssetFileStatus::UNSET if the asset has not been checked against a listing.
   * For on-disk assets this reflects the status stamped after listing comparison.
   * For online-only assets this reflects the status from #OnlineAssetInfo.
   */
  RemoteAssetFileStatus remote_file_status() const;
  /** Set the file status for on-disk assets. No-op for online-only assets. */
  void remote_file_status_set(RemoteAssetFileStatus status);
  /**
   * Store the remote listing's online info on an on-disk asset so it can be re-downloaded.
   * Replaces any previously set online info.
   */
  void online_info_set(OnlineAssetInfo info);

  /**
   * Return whether this asset requires (re-)downloading before it can be used.
   *
   * True for online-only assets (#is_online_only()) and for on-disk assets whose files no longer
   * match the remote listing (e.g. #AssetFileStatus::NO_MATCH).
   */
  bool needs_download() const;

  AssetLibrary &owner_asset_library() const;
};

}  // namespace asset_system

}  // namespace blender
