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

namespace blender {

struct AssetMetaData;
struct ID;
struct PreviewImage;

namespace asset_system {

class AssetLibrary;

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
  };
  std::variant<ExternalAsset, ID *> asset_;

  friend class AssetLibrary;

 public:
  /** Constructs an asset representation for an external ID. The asset will not be editable. */
  AssetRepresentation(StringRef relative_asset_path,
                      StringRef name,
                      int id_type,
                      std::unique_ptr<AssetMetaData> metadata,
                      AssetLibrary &owner_asset_library);
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
   */
  void ensure_previewable();
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
   * `hand/wave.blend/Actions/hi.blend`)
   */
  std::string full_library_path() const;

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

  AssetLibrary &owner_asset_library() const;
};

}  // namespace asset_system

}  // namespace blender
